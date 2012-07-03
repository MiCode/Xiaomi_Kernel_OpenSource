/* arch/arm/mach-msm/smd_tty.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/sched.h>

#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <mach/msm_smd.h>
#include <mach/peripheral-loader.h>
#include <mach/socinfo.h>

#include "smd_private.h"

#define MAX_SMD_TTYS 37
#define MAX_TTY_BUF_SIZE 2048

static DEFINE_MUTEX(smd_tty_lock);

static uint smd_tty_modem_wait;
module_param_named(modem_wait, smd_tty_modem_wait,
			uint, S_IRUGO | S_IWUSR | S_IWGRP);

struct smd_tty_info {
	smd_channel_t *ch;
	struct tty_struct *tty;
	struct wake_lock wake_lock;
	int open_count;
	struct tasklet_struct tty_tsklt;
	struct timer_list buf_req_timer;
	struct completion ch_allocated;
	struct platform_driver driver;
	void *pil;
	int in_reset;
	int in_reset_updated;
	int is_open;
	wait_queue_head_t ch_opened_wait_queue;
	spinlock_t reset_lock;
	struct smd_config *smd;
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

static struct smd_config smd_configs[] = {
	{0, "DS", NULL, SMD_APPS_MODEM},
	{1, "APPS_FM", NULL, SMD_APPS_WCNSS},
	{2, "APPS_RIVA_BT_ACL", NULL, SMD_APPS_WCNSS},
	{3, "APPS_RIVA_BT_CMD", NULL, SMD_APPS_WCNSS},
	{4, "MBALBRIDGE", NULL, SMD_APPS_MODEM},
	{5, "APPS_RIVA_ANT_CMD", NULL, SMD_APPS_WCNSS},
	{6, "APPS_RIVA_ANT_DATA", NULL, SMD_APPS_WCNSS},
	{7, "DATA1", NULL, SMD_APPS_MODEM},
	{11, "DATA11", NULL, SMD_APPS_MODEM},
	{21, "DATA21", NULL, SMD_APPS_MODEM},
	{27, "GPSNMEA", NULL, SMD_APPS_MODEM},
	{36, "LOOPBACK", "LOOPBACK_TTY", SMD_APPS_MODEM},
};
#define DS_IDX 0
#define LOOPBACK_IDX 36

static struct delayed_work loopback_work;
static struct smd_tty_info smd_tty[MAX_SMD_TTYS];

static int is_in_reset(struct smd_tty_info *info)
{
	return info->in_reset;
}

static void buf_req_retry(unsigned long param)
{
	struct smd_tty_info *info = (struct smd_tty_info *)param;
	unsigned long flags;

	spin_lock_irqsave(&info->reset_lock, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock, flags);
		tasklet_hi_schedule(&info->tty_tsklt);
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock, flags);
}

static void smd_tty_read(unsigned long param)
{
	unsigned char *ptr;
	int avail;
	struct smd_tty_info *info = (struct smd_tty_info *)param;
	struct tty_struct *tty = info->tty;

	if (!tty)
		return;

	for (;;) {
		if (is_in_reset(info)) {
			/* signal TTY clients using TTY_BREAK */
			tty_insert_flip_char(tty, 0x00, TTY_BREAK);
			tty_flip_buffer_push(tty);
			break;
		}

		if (test_bit(TTY_THROTTLED, &tty->flags)) break;
		avail = smd_read_avail(info->ch);
		if (avail == 0)
			break;

		if (avail > MAX_TTY_BUF_SIZE)
			avail = MAX_TTY_BUF_SIZE;

		avail = tty_prepare_flip_string(tty, &ptr, avail);
		if (avail <= 0) {
			mod_timer(&info->buf_req_timer,
					jiffies + msecs_to_jiffies(30));
			return;
		}

		if (smd_read(info->ch, ptr, avail) != avail) {
			/* shouldn't be possible since we're in interrupt
			** context here and nobody else could 'steal' our
			** characters.
			*/
			printk(KERN_ERR "OOPS - smd_tty_buffer mismatch?!");
		}

		wake_lock_timeout(&info->wake_lock, HZ / 2);
		tty_flip_buffer_push(tty);
	}

	/* XXX only when writable and necessary */
	tty_wakeup(tty);
}

static void smd_tty_notify(void *priv, unsigned event)
{
	struct smd_tty_info *info = priv;
	unsigned long flags;

	switch (event) {
	case SMD_EVENT_DATA:
		spin_lock_irqsave(&info->reset_lock, flags);
		if (!info->is_open) {
			spin_unlock_irqrestore(&info->reset_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&info->reset_lock, flags);
		/* There may be clients (tty framework) that are blocked
		 * waiting for space to write data, so if a possible read
		 * interrupt came in wake anyone waiting and disable the
		 * interrupts
		 */
		if (smd_write_avail(info->ch)) {
			smd_disable_read_intr(info->ch);
			if (info->tty)
				wake_up_interruptible(&info->tty->write_wait);
		}
		tasklet_hi_schedule(&info->tty_tsklt);
		break;

	case SMD_EVENT_OPEN:
		spin_lock_irqsave(&info->reset_lock, flags);
		info->in_reset = 0;
		info->in_reset_updated = 1;
		info->is_open = 1;
		wake_up_interruptible(&info->ch_opened_wait_queue);
		spin_unlock_irqrestore(&info->reset_lock, flags);
		break;

	case SMD_EVENT_CLOSE:
		spin_lock_irqsave(&info->reset_lock, flags);
		info->in_reset = 1;
		info->in_reset_updated = 1;
		info->is_open = 0;
		wake_up_interruptible(&info->ch_opened_wait_queue);
		spin_unlock_irqrestore(&info->reset_lock, flags);
		/* schedule task to send TTY_BREAK */
		tasklet_hi_schedule(&info->tty_tsklt);

		if (info->tty->index == LOOPBACK_IDX)
			schedule_delayed_work(&loopback_work,
					msecs_to_jiffies(1000));
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

static int smd_tty_open(struct tty_struct *tty, struct file *f)
{
	int res = 0;
	unsigned int n = tty->index;
	struct smd_tty_info *info;
	const char *peripheral = NULL;


	if (n >= MAX_SMD_TTYS || !smd_tty[n].smd)
		return -ENODEV;

	info = smd_tty + n;

	mutex_lock(&smd_tty_lock);
	tty->driver_data = info;

	if (info->open_count++ == 0) {
		peripheral = smd_edge_to_subsystem(smd_tty[n].smd->edge);
		if (peripheral) {
			info->pil = pil_get(peripheral);
			if (IS_ERR(info->pil)) {
				res = PTR_ERR(info->pil);
				goto out;
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
			if (smd_tty_modem_wait) {
				res = wait_for_completion_interruptible_timeout(
					&info->ch_allocated,
					msecs_to_jiffies(smd_tty_modem_wait *
									1000));

				if (res == 0) {
					pr_err("Timed out waiting for SMD"
								" channel\n");
					res = -ETIMEDOUT;
					goto release_pil;
				} else if (res < 0) {
					pr_err("Error waiting for SMD channel:"
									" %d\n",
						res);
					goto release_pil;
				}

				res = 0;
			}
		}


		info->tty = tty;
		tasklet_init(&info->tty_tsklt, smd_tty_read,
			     (unsigned long)info);
		wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND,
				smd_tty[n].smd->port_name);
		if (!info->ch) {
			res = smd_named_open_on_edge(smd_tty[n].smd->port_name,
							smd_tty[n].smd->edge,
							&info->ch, info,
							smd_tty_notify);
			if (res < 0) {
				pr_err("%s: %s open failed %d\n", __func__,
					smd_tty[n].smd->port_name, res);
				goto release_pil;
			}

			res = wait_event_interruptible_timeout(
				info->ch_opened_wait_queue,
				info->is_open, (2 * HZ));
			if (res == 0)
				res = -ETIMEDOUT;
			if (res < 0) {
				pr_err("%s: wait for %s smd_open failed %d\n",
					__func__, smd_tty[n].smd->port_name,
					res);
				goto release_pil;
			}
			res = 0;
		}
	}

release_pil:
	if (res < 0)
		pil_put(info->pil);
	else
		smd_disable_read_intr(info->ch);
out:
	mutex_unlock(&smd_tty_lock);

	return res;
}

static void smd_tty_close(struct tty_struct *tty, struct file *f)
{
	struct smd_tty_info *info = tty->driver_data;
	unsigned long flags;

	if (info == 0)
		return;

	mutex_lock(&smd_tty_lock);
	if (--info->open_count == 0) {
		spin_lock_irqsave(&info->reset_lock, flags);
		info->is_open = 0;
		spin_unlock_irqrestore(&info->reset_lock, flags);
		if (info->tty) {
			tasklet_kill(&info->tty_tsklt);
			wake_lock_destroy(&info->wake_lock);
			info->tty = 0;
		}
		tty->driver_data = 0;
		del_timer(&info->buf_req_timer);
		if (info->ch) {
			smd_close(info->ch);
			info->ch = 0;
			pil_put(info->pil);
		}
	}
	mutex_unlock(&smd_tty_lock);
}

static int smd_tty_write(struct tty_struct *tty, const unsigned char *buf, int len)
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

	spin_lock_irqsave(&info->reset_lock, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock, flags);
		tasklet_hi_schedule(&info->tty_tsklt);
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock, flags);
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

	spin_lock_irqsave(&info->reset_lock, flags);
	tiocm |= (info->in_reset ? TIOCM_OUT1 : 0);
	if (info->in_reset_updated) {
		tiocm |= TIOCM_OUT2;
		info->in_reset_updated = 0;
	}
	spin_unlock_irqrestore(&info->reset_lock, flags);

	return tiocm;
}

static int smd_tty_tiocmset(struct tty_struct *tty,
				unsigned int set, unsigned int clear)
{
	struct smd_tty_info *info = tty->driver_data;

	if (info->in_reset)
		return -ENETRESET;

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

static struct tty_operations smd_tty_ops = {
	.open = smd_tty_open,
	.close = smd_tty_close,
	.write = smd_tty_write,
	.write_room = smd_tty_write_room,
	.chars_in_buffer = smd_tty_chars_in_buffer,
	.unthrottle = smd_tty_unthrottle,
	.tiocmget = smd_tty_tiocmget,
	.tiocmset = smd_tty_tiocmset,
};

static int smd_tty_dummy_probe(struct platform_device *pdev)
{
	int n;
	int idx;

	for (n = 0; n < ARRAY_SIZE(smd_configs); ++n) {
		idx = smd_configs[n].tty_dev_index;

		if (!smd_configs[n].dev_name)
			continue;

		if (pdev->id == smd_configs[n].edge &&
			!strncmp(pdev->name, smd_configs[n].dev_name,
					SMD_MAX_CH_NAME_LEN)) {
			complete_all(&smd_tty[idx].ch_allocated);
			return 0;
		}
	}
	pr_err("%s: unknown device '%s'\n", __func__, pdev->name);

	return -ENODEV;
}

static struct tty_driver *smd_tty_driver;

static int __init smd_tty_init(void)
{
	int ret;
	int n;
	int idx;

	smd_tty_driver = alloc_tty_driver(MAX_SMD_TTYS);
	if (smd_tty_driver == 0)
		return -ENOMEM;

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
		pr_err("%s: driver registration failed %d\n", __func__, ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(smd_configs); ++n) {
		idx = smd_configs[n].tty_dev_index;

		if (smd_configs[n].dev_name == NULL)
			smd_configs[n].dev_name = smd_configs[n].port_name;

		if (idx == DS_IDX) {
			/*
			 * DS port uses the kernel API starting with
			 * 8660 Fusion.  Only register the userspace
			 * platform device for older targets.
			 */
			int legacy_ds = 0;

			legacy_ds |= cpu_is_msm7x01() || cpu_is_msm7x25();
			legacy_ds |= cpu_is_msm7x27() || cpu_is_msm7x30();
			legacy_ds |= cpu_is_qsd8x50() || cpu_is_msm8x55();
			/*
			 * use legacy mode for 8660 Standalone (subtype 0)
			 */
			legacy_ds |= cpu_is_msm8x60() &&
					(socinfo_get_platform_subtype() == 0x0);

			if (!legacy_ds)
				continue;
		}

		tty_register_device(smd_tty_driver, idx, 0);
		init_completion(&smd_tty[idx].ch_allocated);

		/* register platform device */
		smd_tty[idx].driver.probe = smd_tty_dummy_probe;
		smd_tty[idx].driver.driver.name = smd_configs[n].dev_name;
		smd_tty[idx].driver.driver.owner = THIS_MODULE;
		spin_lock_init(&smd_tty[idx].reset_lock);
		smd_tty[idx].is_open = 0;
		setup_timer(&smd_tty[idx].buf_req_timer, buf_req_retry,
				(unsigned long)&smd_tty[idx]);
		init_waitqueue_head(&smd_tty[idx].ch_opened_wait_queue);
		ret = platform_driver_register(&smd_tty[idx].driver);

		if (ret) {
			pr_err("%s: init failed %d (%d)\n", __func__, idx, ret);
			smd_tty[idx].driver.probe = NULL;
			goto out;
		}
		smd_tty[idx].smd = &smd_configs[n];
	}
	INIT_DELAYED_WORK(&loopback_work, loopback_probe_worker);
	return 0;

out:
	/* unregister platform devices */
	for (n = 0; n < ARRAY_SIZE(smd_configs); ++n) {
		idx = smd_configs[n].tty_dev_index;

		if (smd_tty[idx].driver.probe) {
			platform_driver_unregister(&smd_tty[idx].driver);
			tty_unregister_device(smd_tty_driver, idx);
		}
	}

	tty_unregister_driver(smd_tty_driver);
	put_tty_driver(smd_tty_driver);
	return ret;
}

module_init(smd_tty_init);
