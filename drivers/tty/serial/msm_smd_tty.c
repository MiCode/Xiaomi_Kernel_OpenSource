/* arch/arm/mach-msm/smd_tty.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ipc_logging.h>
#include <linux/of.h>
#include <linux/suspend.h>

#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <soc/qcom/smd.h>
#include <soc/qcom/smsm.h>
#include <soc/qcom/subsystem_restart.h>

#define MODULE_NAME "msm_smdtty"
#define MAX_SMD_TTYS 37
#define MAX_TTY_BUF_SIZE 2048
#define TTY_PUSH_WS_DELAY 500
#define TTY_PUSH_WS_POST_SUSPEND_DELAY 100
#define MAX_RA_WAKE_LOCK_NAME_LEN 32
#define SMD_TTY_PROBE_WAIT_TIMEOUT 3000
#define SMD_TTY_LOG_PAGES 2

#define SMD_TTY_INFO(buf...) \
do { \
	if (smd_tty_log_ctx) { \
		ipc_log_string(smd_tty_log_ctx, buf); \
	} \
} while (0)

#define SMD_TTY_ERR(buf...) \
do { \
	if (smd_tty_log_ctx) \
		ipc_log_string(smd_tty_log_ctx, buf); \
	pr_err(buf); \
} while (0)

static void *smd_tty_log_ctx;

static struct delayed_work smd_tty_probe_work;
static int smd_tty_probe_done;

static bool smd_tty_in_suspend;
static bool smd_tty_read_in_suspend;
static struct wakeup_source read_in_suspend_ws;

/**
 * struct smd_tty_info - context for an individual SMD TTY device
 *
 * @ch:  SMD channel handle
 * @port:  TTY port context structure
 * @device_ptr:  TTY device pointer
 * @pending_ws:  pending-data wakeup source
 * @tty_tsklt:  read tasklet
 * @buf_req_timer:  RX buffer retry timer
 * @ch_allocated:  completion set when SMD channel is allocated
 * @pil:  Peripheral Image Loader handle
 * @edge:  SMD edge associated with port
 * @ch_name:  SMD channel name associated with port
 * @dev_name:  SMD platform device name associated with port
 *
 * @open_lock_lha1: open/close lock - used to serialize open/close operations
 * @open_wait:  Timeout in seconds to wait for SMD port to be created / opened
 *
 * @reset_lock_lha2: lock for reset and open state
 * @in_reset:  True if SMD channel is closed / in SSR
 * @in_reset_updated:  reset state changed
 * @is_open:  True if SMD port is open
 * @ch_opened_wait_queue:  SMD port open/close wait queue
 *
 * @ra_lock_lha3:  Read-available lock - used to synchronize reads from SMD
 * @ra_wakeup_source_name: Name of the read-available wakeup source
 * @ra_wakeup_source:  Read-available wakeup source
 */
struct smd_tty_info {
	smd_channel_t *ch;
	struct tty_port port;
	struct device *device_ptr;
	struct wakeup_source pending_ws;
	struct tasklet_struct tty_tsklt;
	struct timer_list buf_req_timer;
	struct completion ch_allocated;
	void *pil;
	uint32_t edge;
	char ch_name[SMD_MAX_CH_NAME_LEN];
	char dev_name[SMD_MAX_CH_NAME_LEN];

	struct mutex open_lock_lha1;
	unsigned int open_wait;

	spinlock_t reset_lock_lha2;
	int in_reset;
	int in_reset_updated;
	int is_open;
	wait_queue_head_t ch_opened_wait_queue;

	spinlock_t ra_lock_lha3;
	char ra_wakeup_source_name[MAX_RA_WAKE_LOCK_NAME_LEN];
	struct wakeup_source ra_wakeup_source;
};

/**
 * struct smd_tty_pfdriver - SMD tty channel platform driver structure
 *
 * @list:  Adds this structure into smd_tty_platform_driver_list::list.
 * @ref_cnt:  reference count for this structure.
 * @driver:  SMD channel platform driver context structure
 */
struct smd_tty_pfdriver {
	struct list_head list;
	int ref_cnt;
	struct platform_driver driver;
};

/**
 * SMD port configuration.
 *
 * @tty_dev_index   Index into smd_tty[]
 * @port_name       Name of the SMD port
 * @dev_name        Name of the TTY Device (if NULL, @port_name is used)
 * @edge            SMD edge
 */
struct smd_config {
	uint32_t tty_dev_index;
	const char *port_name;
	const char *dev_name;
	uint32_t edge;
};

/**
 * struct smd_config smd_configs[]: Legacy configuration
 *
 * An array of all SMD tty channel supported in legacy targets.
 * Future targets use either platform device or device tree configuration.
 */
static struct smd_config smd_configs[] = {
	{0, "DS", NULL, SMD_APPS_MODEM},
	{1, "APPS_FM", NULL, SMD_APPS_WCNSS},
	{2, "APPS_RIVA_BT_ACL", NULL, SMD_APPS_WCNSS},
	{3, "APPS_RIVA_BT_CMD", NULL, SMD_APPS_WCNSS},
	{4, "MBALBRIDGE", NULL, SMD_APPS_MODEM},
	{5, "APPS_RIVA_ANT_CMD", NULL, SMD_APPS_WCNSS},
	{6, "APPS_RIVA_ANT_DATA", NULL, SMD_APPS_WCNSS},
	{7, "DATA1", NULL, SMD_APPS_MODEM},
	{8, "DATA4", NULL, SMD_APPS_MODEM},
	{11, "DATA11", NULL, SMD_APPS_MODEM},
	{21, "DATA21", NULL, SMD_APPS_MODEM},
	{27, "GPSNMEA", NULL, SMD_APPS_MODEM},
	{36, "LOOPBACK", "LOOPBACK_TTY", SMD_APPS_MODEM},
};
#define DS_IDX 0
#define LOOPBACK_IDX 36

static struct delayed_work loopback_work;
static struct smd_tty_info smd_tty[MAX_SMD_TTYS];

static DEFINE_MUTEX(smd_tty_pfdriver_lock_lha1);
static LIST_HEAD(smd_tty_pfdriver_list);

static int is_in_reset(struct smd_tty_info *info)
{
	return info->in_reset;
}

static void buf_req_retry(unsigned long param)
{
	struct smd_tty_info *info = (struct smd_tty_info *)param;
	unsigned long flags;

	spin_lock_irqsave(&info->reset_lock_lha2, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock_lha2, flags);
		tasklet_hi_schedule(&info->tty_tsklt);
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock_lha2, flags);
}

static ssize_t open_timeout_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t n)
{
	unsigned int num_dev;
	unsigned long wait;
	if (dev == NULL) {
		SMD_TTY_INFO("%s: Invalid Device passed", __func__);
		return -EINVAL;
	}
	for (num_dev = 0; num_dev < MAX_SMD_TTYS; num_dev++) {
		if (dev == smd_tty[num_dev].device_ptr)
			break;
	}
	if (num_dev >= MAX_SMD_TTYS) {
		SMD_TTY_ERR("[%s]: Device Not found", __func__);
		return -EINVAL;
	}
	if (!kstrtoul(buf, 10, &wait)) {
		mutex_lock(&smd_tty[num_dev].open_lock_lha1);
		smd_tty[num_dev].open_wait = wait;
		mutex_unlock(&smd_tty[num_dev].open_lock_lha1);
		return n;
	} else {
		SMD_TTY_INFO("[%s]: Unable to convert %s to an int",
			__func__, buf);
		return -EINVAL;
	}
}

static ssize_t open_timeout_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned int num_dev;
	unsigned int open_wait;

	if (dev == NULL) {
		SMD_TTY_INFO("%s: Invalid Device passed", __func__);
		return -EINVAL;
	}
	for (num_dev = 0; num_dev < MAX_SMD_TTYS; num_dev++) {
		if (dev == smd_tty[num_dev].device_ptr)
			break;
	}
	if (num_dev >= MAX_SMD_TTYS) {
		SMD_TTY_ERR("[%s]: Device Not Found", __func__);
		return -EINVAL;
	}

	mutex_lock(&smd_tty[num_dev].open_lock_lha1);
	open_wait = smd_tty[num_dev].open_wait;
	mutex_unlock(&smd_tty[num_dev].open_lock_lha1);

	return snprintf(buf, PAGE_SIZE, "%d\n", open_wait);
}

static DEVICE_ATTR
	(open_timeout, 0664, open_timeout_show, open_timeout_store);

static void smd_tty_read(unsigned long param)
{
	unsigned char *ptr;
	int avail;
	struct smd_tty_info *info = (struct smd_tty_info *)param;
	struct tty_struct *tty = tty_port_tty_get(&info->port);
	unsigned long flags;

	if (!tty)
		return;

	for (;;) {
		if (is_in_reset(info)) {
			/* signal TTY clients using TTY_BREAK */
			tty_insert_flip_char(tty->port, 0x00, TTY_BREAK);
			tty_flip_buffer_push(tty->port);
			break;
		}

		if (test_bit(TTY_THROTTLED, &tty->flags))
			break;
		spin_lock_irqsave(&info->ra_lock_lha3, flags);
		avail = smd_read_avail(info->ch);
		if (avail == 0) {
			__pm_relax(&info->ra_wakeup_source);
			spin_unlock_irqrestore(&info->ra_lock_lha3, flags);
			break;
		}
		spin_unlock_irqrestore(&info->ra_lock_lha3, flags);

		if (avail > MAX_TTY_BUF_SIZE)
			avail = MAX_TTY_BUF_SIZE;

		avail = tty_prepare_flip_string(tty->port, &ptr, avail);
		if (avail <= 0) {
			mod_timer(&info->buf_req_timer,
					jiffies + msecs_to_jiffies(30));
			tty_kref_put(tty);
			return;
		}

		if (smd_read(info->ch, ptr, avail) != avail) {
			/* shouldn't be possible since we're in interrupt
			** context here and nobody else could 'steal' our
			** characters.
			*/
			SMD_TTY_ERR(
				"%s - Possible smd_tty_buffer mismatch for %s",
				__func__, info->ch_name);
		}

		/*
		 * Keep system awake long enough to allow the TTY
		 * framework to pass the flip buffer to any waiting
		 * userspace clients.
		 */
		__pm_wakeup_event(&info->pending_ws, TTY_PUSH_WS_DELAY);

		if (smd_tty_in_suspend)
			smd_tty_read_in_suspend = true;

		tty_flip_buffer_push(tty->port);
	}

	/* XXX only when writable and necessary */
	tty_wakeup(tty);
	tty_kref_put(tty);
}

static void smd_tty_notify(void *priv, unsigned event)
{
	struct smd_tty_info *info = priv;
	struct tty_struct *tty;
	unsigned long flags;

	switch (event) {
	case SMD_EVENT_DATA:
		spin_lock_irqsave(&info->reset_lock_lha2, flags);
		if (!info->is_open) {
			spin_unlock_irqrestore(&info->reset_lock_lha2, flags);
			break;
		}
		spin_unlock_irqrestore(&info->reset_lock_lha2, flags);
		/* There may be clients (tty framework) that are blocked
		 * waiting for space to write data, so if a possible read
		 * interrupt came in wake anyone waiting and disable the
		 * interrupts
		 */
		if (smd_write_avail(info->ch)) {
			smd_disable_read_intr(info->ch);
			tty = tty_port_tty_get(&info->port);
			if (tty)
				wake_up_interruptible(&tty->write_wait);
			tty_kref_put(tty);
		}
		spin_lock_irqsave(&info->ra_lock_lha3, flags);
		if (smd_read_avail(info->ch)) {
			__pm_stay_awake(&info->ra_wakeup_source);
			tasklet_hi_schedule(&info->tty_tsklt);
		}
		spin_unlock_irqrestore(&info->ra_lock_lha3, flags);
		break;

	case SMD_EVENT_OPEN:
		tty = tty_port_tty_get(&info->port);
		spin_lock_irqsave(&info->reset_lock_lha2, flags);
		if (tty)
			clear_bit(TTY_OTHER_CLOSED, &tty->flags);
		info->in_reset = 0;
		info->in_reset_updated = 1;
		info->is_open = 1;
		wake_up_interruptible(&info->ch_opened_wait_queue);
		spin_unlock_irqrestore(&info->reset_lock_lha2, flags);
		tty_kref_put(tty);
		break;

	case SMD_EVENT_CLOSE:
		spin_lock_irqsave(&info->reset_lock_lha2, flags);
		info->in_reset = 1;
		info->in_reset_updated = 1;
		info->is_open = 0;
		wake_up_interruptible(&info->ch_opened_wait_queue);
		spin_unlock_irqrestore(&info->reset_lock_lha2, flags);

		tty = tty_port_tty_get(&info->port);
		if (tty) {
			/* send TTY_BREAK through read tasklet */
			set_bit(TTY_OTHER_CLOSED, &tty->flags);
			tasklet_hi_schedule(&info->tty_tsklt);

			if (tty->index == LOOPBACK_IDX)
				schedule_delayed_work(&loopback_work,
						msecs_to_jiffies(1000));
		}
		tty_kref_put(tty);
		break;
	}
}

static uint32_t is_modem_smsm_inited(void)
{
	uint32_t modem_state;
	uint32_t ready_state = (SMSM_INIT | SMSM_SMDINIT);

	modem_state = smsm_get_state(SMSM_MODEM_STATE);
	return (modem_state & ready_state) == ready_state;
}

static int smd_tty_dummy_probe(struct platform_device *pdev)
{
	int n;

	for (n = 0; n < MAX_SMD_TTYS; ++n) {
		if (!smd_tty[n].dev_name)
			continue;

		if (pdev->id == smd_tty[n].edge &&
			!strcmp(pdev->name, smd_tty[n].dev_name)) {
			complete_all(&smd_tty[n].ch_allocated);
			return 0;
		}
	}
	SMD_TTY_ERR("[ERR]%s: unknown device '%s'\n", __func__, pdev->name);

	return -ENODEV;
}

/**
 * smd_tty_add_driver() - Add platform drivers for smd tty device
 *
 * @info: context for an individual SMD TTY device
 *
 * @returns: 0 for success, standard Linux error code otherwise
 *
 * This function is used to register platform driver once for all
 * smd tty devices which have same names and increment the reference
 * count for 2nd to nth devices.
 */
static int smd_tty_add_driver(struct smd_tty_info *info)
{
	int r = 0;
	struct smd_tty_pfdriver *smd_tty_pfdriverp;
	struct smd_tty_pfdriver *item;

	if (!info) {
		pr_err("%s on a NULL device structure\n", __func__);
		return -EINVAL;
	}

	SMD_TTY_INFO("Begin %s on smd_tty[%s]\n", __func__,
					info->ch_name);

	mutex_lock(&smd_tty_pfdriver_lock_lha1);
	list_for_each_entry(item, &smd_tty_pfdriver_list, list) {
		if (!strcmp(item->driver.driver.name, info->dev_name)) {
			SMD_TTY_INFO("%s:%s Driver Already reg. cnt:%d\n",
				__func__, info->ch_name, item->ref_cnt);
			++item->ref_cnt;
			goto exit;
		}
	}

	smd_tty_pfdriverp = kzalloc(sizeof(*smd_tty_pfdriverp), GFP_KERNEL);
	if (IS_ERR_OR_NULL(smd_tty_pfdriverp)) {
		pr_err("%s: kzalloc() failed for smd_tty_pfdriver[%s]\n",
			__func__, info->ch_name);
		r = -ENOMEM;
		goto exit;
	}

	smd_tty_pfdriverp->driver.probe = smd_tty_dummy_probe;
	smd_tty_pfdriverp->driver.driver.name = info->dev_name;
	smd_tty_pfdriverp->driver.driver.owner = THIS_MODULE;
	r = platform_driver_register(&smd_tty_pfdriverp->driver);
	if (r) {
		pr_err("%s: %s Platform driver reg. failed\n",
			__func__, info->ch_name);
		kfree(smd_tty_pfdriverp);
		goto exit;
	}
	++smd_tty_pfdriverp->ref_cnt;
	list_add(&smd_tty_pfdriverp->list, &smd_tty_pfdriver_list);

exit:
	SMD_TTY_INFO("End %s on smd_tty_ch[%s]\n", __func__, info->ch_name);
	mutex_unlock(&smd_tty_pfdriver_lock_lha1);
	return r;
}

/**
 * smd_tty_remove_driver() - Remove the platform drivers for smd tty device
 *
 * @info: context for an individual SMD TTY device
 *
 * This function is used to decrement the reference count on
 * platform drivers for smd pkt devices and removes the drivers
 * when the reference count becomes zero.
 */
static void smd_tty_remove_driver(struct smd_tty_info *info)
{
	struct smd_tty_pfdriver *smd_tty_pfdriverp;
	bool found_item = false;

	if (!info) {
		pr_err("%s on a NULL device\n", __func__);
		return;
	}

	SMD_TTY_INFO("Begin %s on smd_tty_ch[%s]\n", __func__,
					info->ch_name);
	mutex_lock(&smd_tty_pfdriver_lock_lha1);
	list_for_each_entry(smd_tty_pfdriverp, &smd_tty_pfdriver_list, list) {
		if (!strcmp(smd_tty_pfdriverp->driver.driver.name,
					info->dev_name)) {
			found_item = true;
			SMD_TTY_INFO("%s:%s Platform driver cnt:%d\n",
				__func__, info->ch_name,
				smd_tty_pfdriverp->ref_cnt);
			if (smd_tty_pfdriverp->ref_cnt > 0)
				--smd_tty_pfdriverp->ref_cnt;
			else
				pr_warn("%s reference count <= 0\n", __func__);
			break;
		}
	}
	if (!found_item)
		SMD_TTY_ERR("%s:%s No item found in list.\n",
			__func__, info->ch_name);

	if (found_item && smd_tty_pfdriverp->ref_cnt == 0) {
		platform_driver_unregister(&smd_tty_pfdriverp->driver);
		smd_tty_pfdriverp->driver.probe = NULL;
		list_del(&smd_tty_pfdriverp->list);
		kfree(smd_tty_pfdriverp);
	}
	mutex_unlock(&smd_tty_pfdriver_lock_lha1);
	SMD_TTY_INFO("End %s on smd_tty_ch[%s]\n", __func__, info->ch_name);
}

static int smd_tty_port_activate(struct tty_port *tport,
				 struct tty_struct *tty)
{
	int res = 0;
	unsigned int n = tty->index;
	struct smd_tty_info *info;
	const char *peripheral = NULL;

	if (n >= MAX_SMD_TTYS || !smd_tty[n].ch_name)
		return -ENODEV;

	info = smd_tty + n;

	mutex_lock(&info->open_lock_lha1);
	tty->driver_data = info;

	res = smd_tty_add_driver(info);
	if (res) {
		SMD_TTY_ERR("%s:%d Idx smd_tty_driver register failed %d\n",
							__func__, n, res);
		goto out;
	}

	peripheral = smd_edge_to_pil_str(smd_tty[n].edge);
	if (!IS_ERR_OR_NULL(peripheral)) {
		info->pil = subsystem_get(peripheral);
		if (IS_ERR(info->pil)) {
			SMD_TTY_INFO(
				"%s failed on smd_tty device :%s subsystem_get failed for %s",
				__func__, info->ch_name,
				peripheral);

			/*
			 * Sleep, inorder to reduce the frequency of
			 * retry by user-space modules and to avoid
			 * possible watchdog bite.
			 */
			msleep((smd_tty[n].open_wait * 1000));
			res = PTR_ERR(info->pil);
			goto platform_unregister;
		}
	}

	/* Wait for the modem SMSM to be inited for the SMD
	 * Loopback channel to be allocated at the modem. Since
	 * the wait need to be done atmost once, using msleep
	 * doesn't degrade the performance.
	 */
	if (n == LOOPBACK_IDX) {
		if (!is_modem_smsm_inited())
			msleep(5000);
		smsm_change_state(SMSM_APPS_STATE,
				  0, SMSM_SMD_LOOPBACK);
		msleep(100);
	}

	/*
	 * Wait for a channel to be allocated so we know
	 * the modem is ready enough.
	 */
	if (smd_tty[n].open_wait) {
		res = wait_for_completion_interruptible_timeout(
				&info->ch_allocated,
				msecs_to_jiffies(smd_tty[n].open_wait *
								1000));

		if (res == 0) {
			SMD_TTY_INFO(
				"Timed out waiting for SMD channel %s",
				info->ch_name);
			res = -ETIMEDOUT;
			goto release_pil;
		} else if (res < 0) {
			SMD_TTY_INFO(
				"Error waiting for SMD channel %s : %d\n",
				info->ch_name, res);
			goto release_pil;
		}
	}

	tasklet_init(&info->tty_tsklt, smd_tty_read, (unsigned long)info);
	wakeup_source_init(&info->pending_ws, info->ch_name);
	scnprintf(info->ra_wakeup_source_name, MAX_RA_WAKE_LOCK_NAME_LEN,
		  "SMD_TTY_%s_RA", info->ch_name);
	wakeup_source_init(&info->ra_wakeup_source,
			info->ra_wakeup_source_name);

	res = smd_named_open_on_edge(info->ch_name,
				     smd_tty[n].edge, &info->ch, info,
				     smd_tty_notify);
	if (res < 0) {
		SMD_TTY_INFO("%s: %s open failed %d\n",
			      __func__, info->ch_name, res);
		goto release_wl_tl;
	}

	res = wait_event_interruptible_timeout(info->ch_opened_wait_queue,
					       info->is_open, (2 * HZ));
	if (res == 0)
		res = -ETIMEDOUT;
	if (res < 0) {
		SMD_TTY_INFO("%s: wait for %s smd_open failed %d\n",
			      __func__, info->ch_name, res);
		goto close_ch;
	}
	SMD_TTY_INFO("%s with PID %u opened port %s",
		      current->comm, current->pid, info->ch_name);
	smd_disable_read_intr(info->ch);
	mutex_unlock(&info->open_lock_lha1);
	return 0;

close_ch:
	smd_close(info->ch);
	info->ch = NULL;

release_wl_tl:
	tasklet_kill(&info->tty_tsklt);
	wakeup_source_trash(&info->pending_ws);
	wakeup_source_trash(&info->ra_wakeup_source);

release_pil:
	subsystem_put(info->pil);

platform_unregister:
	smd_tty_remove_driver(info);

out:
	mutex_unlock(&info->open_lock_lha1);

	return res;
}

static void smd_tty_port_shutdown(struct tty_port *tport)
{
	struct smd_tty_info *info;
	struct tty_struct *tty = tty_port_tty_get(tport);
	unsigned long flags;

	info = tty->driver_data;
	if (info == 0) {
		tty_kref_put(tty);
		return;
	}

	mutex_lock(&info->open_lock_lha1);

	spin_lock_irqsave(&info->reset_lock_lha2, flags);
	info->is_open = 0;
	spin_unlock_irqrestore(&info->reset_lock_lha2, flags);

	tasklet_kill(&info->tty_tsklt);
	wakeup_source_trash(&info->pending_ws);
	wakeup_source_trash(&info->ra_wakeup_source);

	SMD_TTY_INFO("%s with PID %u closed port %s",
			current->comm, current->pid,
			info->ch_name);
	tty->driver_data = NULL;
	del_timer(&info->buf_req_timer);

	smd_close(info->ch);
	info->ch = NULL;
	subsystem_put(info->pil);
	smd_tty_remove_driver(info);

	mutex_unlock(&info->open_lock_lha1);
	tty_kref_put(tty);
}

static int smd_tty_open(struct tty_struct *tty, struct file *f)
{
	struct smd_tty_info *info = smd_tty + tty->index;

	return tty_port_open(&info->port, tty, f);
}

static void smd_tty_close(struct tty_struct *tty, struct file *f)
{
	struct smd_tty_info *info = smd_tty + tty->index;

	tty_port_close(&info->port, tty, f);
}

static int smd_tty_write(struct tty_struct *tty, const unsigned char *buf,
									int len)
{
	struct smd_tty_info *info = tty->driver_data;
	int avail;

	/* if we're writing to a packet channel we will
	** never be able to write more data than there
	** is currently space for
	*/
	if (is_in_reset(info))
		return -ENETRESET;

	avail = smd_write_avail(info->ch);
	/* if no space, we'll have to setup a notification later to wake up the
	 * tty framework when space becomes avaliable
	 */
	if (!avail) {
		smd_enable_read_intr(info->ch);
		return 0;
	}
	if (len > avail)
		len = avail;
	SMD_TTY_INFO("[WRITE]: PID %u -> port %s %x bytes",
			current->pid, info->ch_name, len);

	return smd_write(info->ch, buf, len);
}

static int smd_tty_write_room(struct tty_struct *tty)
{
	struct smd_tty_info *info = tty->driver_data;
	return smd_write_avail(info->ch);
}

static int smd_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct smd_tty_info *info = tty->driver_data;
	return smd_read_avail(info->ch);
}

static void smd_tty_unthrottle(struct tty_struct *tty)
{
	struct smd_tty_info *info = tty->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&info->reset_lock_lha2, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock_lha2, flags);
		tasklet_hi_schedule(&info->tty_tsklt);
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock_lha2, flags);
}

/*
 * Returns the current TIOCM status bits including:
 *      SMD Signals (DTR/DSR, CTS/RTS, CD, RI)
 *      TIOCM_OUT1 - reset state (1=in reset)
 *      TIOCM_OUT2 - reset state updated (1=updated)
 */
static int smd_tty_tiocmget(struct tty_struct *tty)
{
	struct smd_tty_info *info = tty->driver_data;
	unsigned long flags;
	int tiocm;

	tiocm = smd_tiocmget(info->ch);

	spin_lock_irqsave(&info->reset_lock_lha2, flags);
	tiocm |= (info->in_reset ? TIOCM_OUT1 : 0);
	if (info->in_reset_updated) {
		tiocm |= TIOCM_OUT2;
		info->in_reset_updated = 0;
	}
	SMD_TTY_INFO("PID %u --> %s TIOCM is %x ",
			current->pid, __func__, tiocm);
	spin_unlock_irqrestore(&info->reset_lock_lha2, flags);

	return tiocm;
}

static int smd_tty_tiocmset(struct tty_struct *tty,
				unsigned int set, unsigned int clear)
{
	struct smd_tty_info *info = tty->driver_data;

	if (info->in_reset)
		return -ENETRESET;

	SMD_TTY_INFO("PID %u --> %s Set: %x Clear: %x",
			current->pid, __func__, set, clear);
	return smd_tiocmset(info->ch, set, clear);
}

static void loopback_probe_worker(struct work_struct *work)
{
	/* wait for modem to restart before requesting loopback server */
	if (!is_modem_smsm_inited())
		schedule_delayed_work(&loopback_work, msecs_to_jiffies(1000));
	else
		smsm_change_state(SMSM_APPS_STATE,
			  0, SMSM_SMD_LOOPBACK);
}

static const struct tty_port_operations smd_tty_port_ops = {
	.shutdown = smd_tty_port_shutdown,
	.activate = smd_tty_port_activate,
};

static const struct tty_operations smd_tty_ops = {
	.open = smd_tty_open,
	.close = smd_tty_close,
	.write = smd_tty_write,
	.write_room = smd_tty_write_room,
	.chars_in_buffer = smd_tty_chars_in_buffer,
	.unthrottle = smd_tty_unthrottle,
	.tiocmget = smd_tty_tiocmget,
	.tiocmset = smd_tty_tiocmset,
};

static int smd_tty_pm_notifier(struct notifier_block *nb,
				unsigned long event, void *unused)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		smd_tty_read_in_suspend = false;
		smd_tty_in_suspend = true;
		break;

	case PM_POST_SUSPEND:
		smd_tty_in_suspend = false;
		if (smd_tty_read_in_suspend) {
			smd_tty_read_in_suspend = false;
			__pm_wakeup_event(&read_in_suspend_ws,
					TTY_PUSH_WS_POST_SUSPEND_DELAY);
		}
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block smd_tty_pm_nb = {
	.notifier_call = smd_tty_pm_notifier,
	.priority = 0,
};

/**
 * smd_tty_log_init()- Init function for IPC logging
 *
 * Initialize the buffer that is used to provide the log information
 * pertaining to the smd_tty module.
 */
static void smd_tty_log_init(void)
{
	smd_tty_log_ctx = ipc_log_context_create(SMD_TTY_LOG_PAGES,
						"smd_tty");
	if (!smd_tty_log_ctx)
		pr_err("%s: Unable to create IPC log", __func__);
}

static struct tty_driver *smd_tty_driver;

static int smd_tty_register_driver(void)
{
	int ret;

	smd_tty_driver = alloc_tty_driver(MAX_SMD_TTYS);
	if (smd_tty_driver == 0) {
		SMD_TTY_ERR("%s - Driver allocation failed", __func__);
		return -ENOMEM;
	}

	smd_tty_driver->owner = THIS_MODULE;
	smd_tty_driver->driver_name = "smd_tty_driver";
	smd_tty_driver->name = "smd";
	smd_tty_driver->major = 0;
	smd_tty_driver->minor_start = 0;
	smd_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	smd_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	smd_tty_driver->init_termios = tty_std_termios;
	smd_tty_driver->init_termios.c_iflag = 0;
	smd_tty_driver->init_termios.c_oflag = 0;
	smd_tty_driver->init_termios.c_cflag = B38400 | CS8 | CREAD;
	smd_tty_driver->init_termios.c_lflag = 0;
	smd_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS |
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(smd_tty_driver, &smd_tty_ops);

	ret = tty_register_driver(smd_tty_driver);
	if (ret) {
		put_tty_driver(smd_tty_driver);
		SMD_TTY_ERR("%s: driver registration failed %d", __func__, ret);
	}

	return ret;
}

static void smd_tty_device_init(int idx)
{
	struct tty_port *port;

	port = &smd_tty[idx].port;
	tty_port_init(port);
	port->ops = &smd_tty_port_ops;
	smd_tty[idx].device_ptr = tty_port_register_device(port, smd_tty_driver,
							   idx, NULL);
	init_completion(&smd_tty[idx].ch_allocated);
	mutex_init(&smd_tty[idx].open_lock_lha1);
	spin_lock_init(&smd_tty[idx].reset_lock_lha2);
	spin_lock_init(&smd_tty[idx].ra_lock_lha3);
	smd_tty[idx].is_open = 0;
	setup_timer(&smd_tty[idx].buf_req_timer, buf_req_retry,
			(unsigned long)&smd_tty[idx]);
	init_waitqueue_head(&smd_tty[idx].ch_opened_wait_queue);

	if (device_create_file(smd_tty[idx].device_ptr, &dev_attr_open_timeout))
		SMD_TTY_ERR("%s: Unable to create device attributes for %s",
			__func__, smd_configs[idx].port_name);
}

static int smd_tty_core_init(void)
{
	int ret;
	int n;
	int idx;

	ret = smd_tty_register_driver();
	if (ret) {
		pr_err("%s: driver registration failed %d\n", __func__, ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(smd_configs); ++n) {
		idx = smd_configs[n].tty_dev_index;
		smd_tty[idx].edge = smd_configs[n].edge;

		strlcpy(smd_tty[idx].ch_name, smd_configs[n].port_name,
							SMD_MAX_CH_NAME_LEN);
		if (smd_configs[n].dev_name == NULL) {
			strlcpy(smd_tty[idx].dev_name, smd_tty[idx].ch_name,
							SMD_MAX_CH_NAME_LEN);
		} else {
			strlcpy(smd_tty[idx].dev_name, smd_configs[n].dev_name,
							SMD_MAX_CH_NAME_LEN);
		}

		smd_tty_device_init(idx);
	}
	INIT_DELAYED_WORK(&loopback_work, loopback_probe_worker);

	ret = register_pm_notifier(&smd_tty_pm_nb);
	if (ret)
		pr_err("%s: power state notif error %d\n", __func__, ret);

	return 0;
}

static int smd_tty_devicetree_init(struct platform_device *pdev)
{
	int ret;
	int idx;
	int edge;
	char *key = NULL;
	const char *ch_name;
	const char *dev_name;
	const char *remote_ss;
	struct device_node *node;

	ret = smd_tty_register_driver();
	if (ret) {
		SMD_TTY_ERR("%s: driver registration failed %d\n",
						__func__, ret);
		return ret;
	}

	for_each_child_of_node(pdev->dev.of_node, node) {

		ret = of_alias_get_id(node, "smd");
		SMD_TTY_INFO("%s:adding smd%d\n", __func__, ret);

		if (ret < 0 || ret >= MAX_SMD_TTYS)
			goto error;
		idx = ret;

		key = "qcom,smdtty-remote";
		remote_ss = of_get_property(node, key, NULL);
		if (!remote_ss)
			goto error;

		edge = smd_remote_ss_to_edge(remote_ss);
		if (edge < 0)
			goto error;
		smd_tty[idx].edge = edge;

		key = "qcom,smdtty-port-name";
		ch_name = of_get_property(node, key, NULL);
		if (!ch_name)
			goto error;
		strlcpy(smd_tty[idx].ch_name, ch_name,
					SMD_MAX_CH_NAME_LEN);

		key = "qcom,smdtty-dev-name";
		dev_name = of_get_property(node, key, NULL);
		if (!dev_name) {
			strlcpy(smd_tty[idx].dev_name, smd_tty[idx].ch_name,
							SMD_MAX_CH_NAME_LEN);
		} else {
			strlcpy(smd_tty[idx].dev_name, dev_name,
						SMD_MAX_CH_NAME_LEN);
		}

		smd_tty_device_init(idx);
	}
	INIT_DELAYED_WORK(&loopback_work, loopback_probe_worker);

	ret = register_pm_notifier(&smd_tty_pm_nb);
	if (ret)
		pr_err("%s: power state notif error %d\n", __func__, ret);

	return 0;

error:
	SMD_TTY_ERR("%s:Initialization error, key[%s]\n", __func__, key);
	/* Unregister tty platform devices */
	for_each_child_of_node(pdev->dev.of_node, node) {

		key = "qcom,smdtty-dev-idx";
		ret = of_property_read_u32(node, key, &idx);
		if (ret || idx >= MAX_SMD_TTYS)
			goto out;
		if (smd_tty[idx].device_ptr)
			tty_unregister_device(smd_tty_driver, idx);
	}
out:
	tty_unregister_driver(smd_tty_driver);
	put_tty_driver(smd_tty_driver);
	return ret;
}

static int msm_smd_tty_probe(struct platform_device *pdev)
{
	int ret;

	if (pdev) {
		if (pdev->dev.of_node) {
			ret = smd_tty_devicetree_init(pdev);
			if (ret) {
				SMD_TTY_ERR("%s: device tree init failed\n",
								__func__);
				return ret;
			}
		}
	}

	smd_tty_probe_done = 1;
	return 0;
}

static void smd_tty_probe_worker(struct work_struct *work)
{
	int ret;
	if (!smd_tty_probe_done) {
		ret = smd_tty_core_init();
		if (ret < 0)
			SMD_TTY_ERR("smd_tty_core_init failed ret = %d\n", ret);
	}

}

static struct of_device_id msm_smd_tty_match_table[] = {
	{ .compatible = "qcom,smdtty" },
	{},
};

static struct platform_driver msm_smd_tty_driver = {
	.probe = msm_smd_tty_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_smd_tty_match_table,
	 },
};


static int __init smd_tty_init(void)
{
	int rc;

	smd_tty_log_init();
	rc = platform_driver_register(&msm_smd_tty_driver);
	if (rc) {
		SMD_TTY_ERR("%s: msm_smd_tty_driver register failed %d\n",
								__func__, rc);
		return rc;
	}

	INIT_DELAYED_WORK(&smd_tty_probe_work, smd_tty_probe_worker);
	schedule_delayed_work(&smd_tty_probe_work,
				msecs_to_jiffies(SMD_TTY_PROBE_WAIT_TIMEOUT));

	wakeup_source_init(&read_in_suspend_ws, "SMDTTY_READ_IN_SUSPEND");
	return 0;
}

module_init(smd_tty_init);
