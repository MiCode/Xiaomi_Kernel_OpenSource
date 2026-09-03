// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_tcpm.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/property.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/usb/sprd_tcpm.h>
#include <linux/workqueue.h>
#include <uapi/linux/sched/types.h>

struct sprd_pd_rx_event {
	struct kthread_work work;
	struct sprd_tcpm_port *port;
	struct sprd_pd_message msg;
};

#define sprd_tcpm_cc_is_sink(cc) \
	((cc) == SPRD_TYPEC_CC_RP_DEF || (cc) == SPRD_TYPEC_CC_RP_1_5 || \
	 (cc) == SPRD_TYPEC_CC_RP_3_0)

#define sprd_tcpm_port_is_sink(port) \
	((sprd_tcpm_cc_is_sink((port)->cc1) && !sprd_tcpm_cc_is_sink((port)->cc2)) || \
	 (sprd_tcpm_cc_is_sink((port)->cc2) && !sprd_tcpm_cc_is_sink((port)->cc1)))

#define sprd_tcpm_cc_is_source(cc) ((cc) == SPRD_TYPEC_CC_RD)
#define sprd_tcpm_cc_is_audio(cc) ((cc) == SPRD_TYPEC_CC_RA)
#define sprd_tcpm_cc_is_open(cc) ((cc) == SPRD_TYPEC_CC_OPEN)

#define sprd_tcpm_port_is_source(port) \
	((sprd_tcpm_cc_is_source((port)->cc1) && \
	 !sprd_tcpm_cc_is_source((port)->cc2)) || \
	 (sprd_tcpm_cc_is_source((port)->cc2) && \
	  !sprd_tcpm_cc_is_source((port)->cc1)))

#define sprd_tcpm_port_is_debug(port) \
	(sprd_tcpm_cc_is_source((port)->cc1) && sprd_tcpm_cc_is_source((port)->cc2))

#define sprd_tcpm_port_is_audio(port) \
	(sprd_tcpm_cc_is_audio((port)->cc1) && sprd_tcpm_cc_is_audio((port)->cc2))

#define sprd_tcpm_port_is_audio_detached(port) \
	((sprd_tcpm_cc_is_audio((port)->cc1) && sprd_tcpm_cc_is_open((port)->cc2)) || \
	 (sprd_tcpm_cc_is_audio((port)->cc2) && sprd_tcpm_cc_is_open((port)->cc1)))

#define sprd_tcpm_try_snk(port) \
	((port)->try_snk_count == 0 && (port)->try_role == TYPEC_SINK && \
	(port)->port_type == TYPEC_PORT_DRP)

#define sprd_tcpm_try_src(port) \
	((port)->try_src_count == 0 && (port)->try_role == TYPEC_SOURCE && \
	(port)->port_type == TYPEC_PORT_DRP)

static int last_index = -1;
static void sprd_tcpm_unregister_altmodes(struct sprd_tcpm_port *port);
static void sprd_tcpm_log_force(struct sprd_tcpm_port *port, const char *fmt, ...);
static inline enum sprd_tcpm_state sprd_hard_reset_state(struct sprd_tcpm_port *port);

static struct sprd_typec_device_ops *g_sprd_typec_device_ops;
int sprd_tcpm_typec_device_ops_register(struct sprd_typec_device_ops *ops)
{
	if (ops != NULL) {
		g_sprd_typec_device_ops = ops;
		pr_info("%s:line%d typec device ops registered\n", __func__, __LINE__);
	} else {
		pr_err("%s:line%d typec device ops register failed\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_tcpm_typec_device_ops_register);

static struct sprd_charger_ops *g_sprd_charger_ops;
int sprd_tcpm_charger_ops_register(struct sprd_charger_ops *ops)
{
	if (ops != NULL) {
		g_sprd_charger_ops = ops;
		pr_info("%s:line%d charger ops registered\n", __func__, __LINE__);
	} else {
		pr_err("%s:line%d charger ops register failed\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_tcpm_charger_ops_register);

void sprd_tcpm_set_support_accessory_mode(struct sprd_tcpm_port *port)
{
	if (port->can_power_data_role_swap && g_sprd_typec_device_ops &&
	    g_sprd_typec_device_ops->set_support_accessory_mode)
		g_sprd_typec_device_ops->set_support_accessory_mode(port->typec_port);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_set_support_accessory_mode);

static void sprd_tcpm_set_pr_swap_flag(enum sprd_tcpm_typec_pd_swap flag)
{
	if (g_sprd_typec_device_ops &&
	    g_sprd_typec_device_ops->typec_set_pr_swap_flag)
		g_sprd_typec_device_ops->typec_set_pr_swap_flag(flag);
}

static void sprd_tcpm_set_pd_swap_event(enum sprd_tcpm_typec_pd_swap event)
{
	if (g_sprd_typec_device_ops &&
	    g_sprd_typec_device_ops->typec_set_pd_swap_event)
		g_sprd_typec_device_ops->typec_set_pd_swap_event(event);
}

static void sprd_tcpm_set_dr_swap_flag(enum sprd_tcpm_typec_pd_swap flag)
{
	if (g_sprd_typec_device_ops &&
	    g_sprd_typec_device_ops->typec_set_pd_dr_swap_flag)
		g_sprd_typec_device_ops->typec_set_pd_dr_swap_flag(flag);
}

static void sprd_tcpm_set_typec_err_recover_enter(void)
{
	if (g_sprd_typec_device_ops &&
	    g_sprd_typec_device_ops->set_typec_err_recovery_enter)
		g_sprd_typec_device_ops->set_typec_err_recovery_enter();
}

static void sprd_tcpm_set_typec_rp_level(struct sprd_tcpm_port *port, enum sprd_typec_cc_status cc)
{
	static enum sprd_typec_cc_status cc_last = SPRD_TYPEC_CC_RP_DEF;

	if (cc == cc_last)
		return;

	cc_last = cc;
	sprd_tcpm_log_force(port, "%s, set cc rp = %d", __func__, cc);
	if (g_sprd_typec_device_ops &&
	    g_sprd_typec_device_ops->set_typec_rp_level)
		g_sprd_typec_device_ops->set_typec_rp_level(cc);
}

static void sprd_tcpm_set_cm_ic_limit_current(enum sprd_pd_pdo_type pdo_type,
					      int req_vol_uv,
					      int req_cur_ua,
					      bool enable_limit)
{
	if (g_sprd_charger_ops && g_sprd_charger_ops->negotiated_limit_current)
		g_sprd_charger_ops->negotiated_limit_current(pdo_type,
							     req_vol_uv,
							     req_cur_ua,
							     enable_limit);
}

static enum sprd_tcpm_state sprd_tcpm_default_state(struct sprd_tcpm_port *port)
{
	if (port->port_type == TYPEC_PORT_DRP) {
		if (port->try_role == TYPEC_SINK)
			return SNK_UNATTACHED;
		else if (port->try_role == TYPEC_SOURCE)
			return SRC_UNATTACHED;
		else if (port->tcpc->config &&
			 port->tcpc->config->default_role == TYPEC_SINK)
			return SNK_UNATTACHED;
		/* Fall through to return SRC_UNATTACHED */
	} else if (port->port_type == TYPEC_PORT_SNK) {
		return SNK_UNATTACHED;
	}
	return SRC_UNATTACHED;
}

static bool sprd_tcpm_port_is_disconnected(struct sprd_tcpm_port *port)
{
	return (!port->attached && port->cc1 == SPRD_TYPEC_CC_OPEN &&
		port->cc2 == SPRD_TYPEC_CC_OPEN) ||
	       (port->attached && ((port->polarity == SPRD_TYPEC_POLARITY_CC1 &&
				    port->cc1 == SPRD_TYPEC_CC_OPEN) ||
				   (port->polarity == SPRD_TYPEC_POLARITY_CC2 &&
				    port->cc2 == SPRD_TYPEC_CC_OPEN)));
}

static const char *sprd_tcpm_log_tag = "[sprd_tcpm_log]";

#define SPRD_LOG_PRINT_WORK_PERIOD_30_S	30000
#define SPRD_LOG_PRINT_WORK_PERIOD_60_S	60000
#define SPRD_LOG_POLLING_MS		20
#define SPRD_LOG_BUFFER_IDLE_COUNT	30

static bool sprd_tcpm_log_full(struct sprd_tcpm_port *port)
{
	return port->logbuffer_tail ==
		(port->logbuffer_head + 1) % SPRD_LOG_BUFFER_ENTRIES;
}

static void _sprd_tcpm_log(struct sprd_tcpm_port *port, const char *fmt, va_list args, const char *dev_tag)
{
	char tmpbuffer[SPRD_LOG_BUFFER_ENTRY_SIZE];
	u64 ts_nsec;
	unsigned long rem_nsec;

	ts_nsec = local_clock();

	mutex_lock(&port->logbuffer_lock);
	if (port->logbuffer_head < 0 ||
	    port->logbuffer_head >= SPRD_LOG_BUFFER_ENTRIES) {
		dev_warn(port->dev,
			 "Bad log buffer index %d\n", port->logbuffer_head);
		goto abort;
	}

	if (!port->logbuffer[port->logbuffer_head]) {
		port->logbuffer[port->logbuffer_head] =
				kzalloc(SPRD_LOG_BUFFER_ENTRY_SIZE, GFP_KERNEL);
		if (!port->logbuffer[port->logbuffer_head]) {
			mutex_unlock(&port->logbuffer_lock);
			return;
		}
	}

	vsnprintf(tmpbuffer, sizeof(tmpbuffer), fmt, args);

	if (!port->logbuffer[port->logbuffer_head]) {
		dev_warn(port->dev,
			 "Log buffer index %d is NULL\n", port->logbuffer_head);
		goto abort;
	}

	memset(port->logbuffer[port->logbuffer_head], 0, SPRD_LOG_BUFFER_ENTRY_SIZE);

	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(port->logbuffer[port->logbuffer_head],
		  SPRD_LOG_BUFFER_ENTRY_SIZE, "[%5lu.%06lu] %s %s",
		  (unsigned long)ts_nsec, rem_nsec / 1000, dev_tag,
		  tmpbuffer);
	if (sprd_tcpm_log_full(port)) {
		port->logbuffer_full = true;
		port->logbuffer_show_full = true;
	}
	port->logbuffer_head = (port->logbuffer_head + 1) % SPRD_LOG_BUFFER_ENTRIES;
	if (!port->log_output_running) {
		port->log_output_running = true;
		kthread_mod_delayed_work(&port->log_kworker, &port->log_kwork, 0);
	}

abort:
	mutex_unlock(&port->logbuffer_lock);
}

static void sprd_tcpm_log(struct sprd_tcpm_port *port, const char *fmt, ...)
{
	va_list args;

	if (!port->enable_tcpm_log)
		return;

	/* Do not log while disconnected and unattached */
	if (sprd_tcpm_port_is_disconnected(port) &&
	    (port->state == SRC_UNATTACHED || port->state == SNK_UNATTACHED ||
	     port->state == TOGGLING))
		return;

	va_start(args, fmt);
	_sprd_tcpm_log(port, fmt, args, sprd_tcpm_log_tag);
	va_end(args);
}

static void sprd_tcpm_log_force(struct sprd_tcpm_port *port, const char *fmt, ...)
{
	va_list args;

	if (!port->enable_tcpm_log)
		return;

	va_start(args, fmt);
	_sprd_tcpm_log(port, fmt, args, sprd_tcpm_log_tag);
	va_end(args);
}

void sprd_tcpm_log_do_outside(struct sprd_tcpm_port *port, const char *dev_tag,
			      const char *fmt, va_list args)
{
	if (!port)
		return;

	if (!port->enable_tcpm_log)
		return;

	_sprd_tcpm_log(port, fmt, args, dev_tag);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_log_do_outside);

static void sprd_tcpm_log_buffer_release(struct sprd_tcpm_port *port)
{
	int i;

	mutex_lock(&port->logbuffer_lock);

	for (i = (SPRD_LOG_BUFFER_ENTRIES / 4); i < SPRD_LOG_BUFFER_ENTRIES; i++) {
		if (port->logbuffer[i]) {
			kfree(port->logbuffer[i]);
			port->logbuffer[i] = NULL;
		}
	}

	pr_info("%s: free done\n", __func__);

	mutex_unlock(&port->logbuffer_lock);
}

static void sprd_tcpm_log_print_kwork(struct kthread_work *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
						   log_kwork.work);
	int tail, head;
	u32 work_time_ms = SPRD_LOG_PRINT_WORK_PERIOD_30_S;
	bool re_check;

	if (!port) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	mutex_lock(&port->logbuffer_lock);
	port->log_output_running = true;
	mutex_unlock(&port->logbuffer_lock);

recheck:
	mutex_lock(&port->logprintk_lock);
	re_check = false;
	tail = port->logbuffer_last;
	if (tail < 0 || tail >= SPRD_LOG_BUFFER_ENTRIES) {
		pr_err("[%s:%d]tail:%d out of range\n", __func__, __LINE__, tail);
		goto unlock;
	}

	if (likely(!port->logbuffer_full))
		head = port->logbuffer_head;
	else
		head = SPRD_LOG_BUFFER_ENTRIES;

	if (!port->logbuffer_full && (head < 0 || head >= SPRD_LOG_BUFFER_ENTRIES)) {
		pr_err("[%s:%d]head:%d out of range\n", __func__, __LINE__, head);
		goto unlock;
	}

	if (port->logbuffer_idle_count && (head != tail))
		port->logbuffer_idle_count = 0;

	if (head == tail) {
		work_time_ms = SPRD_LOG_PRINT_WORK_PERIOD_60_S;

		if (port->logbuffer_idle_count > SPRD_LOG_BUFFER_IDLE_COUNT)
			goto unlock;

		port->logbuffer_idle_count++;
		if (port->logbuffer_idle_count > SPRD_LOG_BUFFER_IDLE_COUNT)
			sprd_tcpm_log_buffer_release(port);
	}

	while (tail < head) {
		if (port->logbuffer[tail])
			pr_info("[%d / %d] %s\n", tail, head, port->logbuffer[tail]);
		tail++;
	}

	if (likely(!port->logbuffer_full)) {
		if (port->logbuffer_last != head)
			port->logbuffer_last = head;

		if (port->logbuffer_head != head)
			re_check = true;
	} else {
		port->logbuffer_last = 0;
		port->logbuffer_full = false;
		re_check = true;
	}

unlock:
	mutex_unlock(&port->logprintk_lock);

	if (re_check) {
		msleep(SPRD_LOG_POLLING_MS);
		goto recheck;
	}

	mutex_lock(&port->logbuffer_lock);
	port->log_output_running = false;
	mutex_unlock(&port->logbuffer_lock);

	kthread_queue_delayed_work(&port->log_kworker, &port->log_kwork,
				   msecs_to_jiffies(work_time_ms));
}

static void sprd_tcpm_source_acquire_wake_lock(struct sprd_tcpm_port *port)
{
	mutex_lock(&port->keep_source_awake_mtx);
	if (!port->keep_source_awake) {
		__pm_stay_awake(port->pd_source_ws);
		port->keep_source_awake = true;
		sprd_tcpm_log(port, "acquire pd_source_wakelock when pd as source");
		pr_info("acquire pd_source_wakelock when pd as source\n");
	}
	mutex_unlock(&port->keep_source_awake_mtx);
}

static void sprd_tcpm_source_release_wake_lock(struct sprd_tcpm_port *port)
{
	mutex_lock(&port->keep_source_awake_mtx);
	if (port->keep_source_awake) {
		__pm_relax(port->pd_source_ws);
		port->keep_source_awake = false;
		sprd_tcpm_log(port, "release pd_source_wakelock");
		pr_info("release pd_source_wakelock\n");
	}
	mutex_unlock(&port->keep_source_awake_mtx);
}

static void sprd_tcpm_log_source_caps(struct sprd_tcpm_port *port)
{
	int i;

	if (!port->enable_tcpm_log)
		return;

	for (i = 0; i < port->nr_source_caps; i++) {
		u32 pdo = port->source_caps[i];
		enum sprd_pd_pdo_type type = sprd_pdo_type(pdo);
		char msg[64];

		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
			scnprintf(msg, sizeof(msg),
				  "%u mV, %u mA [%s%s%s%s%s%s]",
				  sprd_pdo_fixed_voltage(pdo),
				  sprd_pdo_max_current(pdo),
				  (pdo & SPRD_PDO_FIXED_DUAL_ROLE) ?
							"R" : "",
				  (pdo & SPRD_PDO_FIXED_SUSPEND) ?
							"S" : "",
				  (pdo & SPRD_PDO_FIXED_HIGHER_CAP) ?
							"H" : "",
				  (pdo & SPRD_PDO_FIXED_USB_COMM) ?
							"U" : "",
				  (pdo & SPRD_PDO_FIXED_DATA_SWAP) ?
							"D" : "",
				  (pdo & SPRD_PDO_FIXED_EXTPOWER) ?
							"E" : "");
			break;
		case SPRD_PDO_TYPE_VAR:
			scnprintf(msg, sizeof(msg),
				  "%u-%u mV, %u mA",
				  sprd_pdo_min_voltage(pdo),
				  sprd_pdo_max_voltage(pdo),
				  sprd_pdo_max_current(pdo));
			break;
		case SPRD_PDO_TYPE_BATT:
			scnprintf(msg, sizeof(msg),
				  "%u-%u mV, %u mW",
				  sprd_pdo_min_voltage(pdo),
				  sprd_pdo_max_voltage(pdo),
				  sprd_pdo_max_power(pdo));
			break;
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) == SPRD_APDO_TYPE_PPS)
				scnprintf(msg, sizeof(msg),
					  "%u-%u mV, %u mA",
					  sprd_pdo_pps_apdo_min_voltage(pdo),
					  sprd_pdo_pps_apdo_max_voltage(pdo),
					  sprd_pdo_pps_apdo_max_current(pdo));
			else
				strcpy(msg, "undefined APDO");
			break;
		default:
			strcpy(msg, "undefined");
			break;
		}
		sprd_tcpm_log(port, " PDO %d: type %d, %s",
			 i, type, msg);
	}
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int sprd_tcpm_debug_seq_log_check(struct sprd_tcpm_port *port, struct seq_file *s)
{
	int tail, head;

	if (!port->logbuffer_show_full) {
		tail = port->logbuffer_show_last;
		if (tail < 0 || tail >= SPRD_LOG_BUFFER_ENTRIES) {
			pr_err("[%s:%d]tail:%d out of range\n", __func__, __LINE__, tail);
			return -EINVAL;
		}
		head = port->logbuffer_head;
		if (head < 0 || head >= SPRD_LOG_BUFFER_ENTRIES) {
			pr_err("[%s:%d]head:%d out of range\n", __func__, __LINE__, head);
			return -EINVAL;
		}
		pr_info("[%s:%d][tail:%d / head:%d]\n", __func__, __LINE__, tail, head);
		while (tail < head) {
			if (port->logbuffer[tail])
				seq_printf(s, "[%d / %d] %s\n", tail, head, port->logbuffer[tail]);
			tail++;
		}

		if (!seq_has_overflowed(s)) {
			pr_info("[%s:%d]logbuffer_show_last = %d\n", __func__, __LINE__, head);
			port->logbuffer_show_last = head;
		} else {
			pr_info("[%s:%d]logbuffer_show_last not change\n", __func__, __LINE__);
		}
	} else {
		tail = port->logbuffer_show_last;
		if (tail < 0 || tail >= SPRD_LOG_BUFFER_ENTRIES) {
			pr_err("[%s:%d]tail:%d out of range\n", __func__, __LINE__, tail);
			return -EINVAL;
		}
		head = SPRD_LOG_BUFFER_ENTRIES;
		pr_info("[%s:%d][full tail:%d / head:%d]\n", __func__, __LINE__, tail, head);
		while (tail < head) {
			if (port->logbuffer[tail])
				seq_printf(s, "[%d / %d] %s\n", tail, head, port->logbuffer[tail]);
			tail++;
		}
		tail = 0;
		head = port->logbuffer_head;
		if (head < 0 || head >= SPRD_LOG_BUFFER_ENTRIES) {
			pr_err("[%s:%d]head:%d out of range\n", __func__, __LINE__, head);
			return -EINVAL;
		}
		pr_info("[%s:%d][full tail:%d / head:%d]\n", __func__, __LINE__, tail, head);
		while (tail < head) {
			if (port->logbuffer[tail])
				seq_printf(s, "[%d / %d] %s\n", tail, head, port->logbuffer[tail]);
			tail++;
		}

		port->logbuffer_show_full = false;
		if (!seq_has_overflowed(s)) {
			pr_info("[%s:%d]full logbuffer_show_last = %d\n", __func__, __LINE__, tail);
			port->logbuffer_show_last = tail;
		}
	}

	return 0;
}

static int _sprd_tcpm_debug_show(struct sprd_tcpm_port *port, struct seq_file *s)
{
	if (!port) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return 0;
	}

	pr_info("[%s]line%d\n", __func__, __LINE__);

	if (port->logbuffer_idle_count > SPRD_LOG_BUFFER_IDLE_COUNT) {
		pr_info("[%s:%d]log buffer free\n", __func__, __LINE__);
		goto out;
	}

	kthread_mod_delayed_work(&port->log_kworker, &port->log_kwork, 0);
	kthread_flush_work(&port->log_kwork.work);

	sprd_tcpm_debug_seq_log_check(port, s);

out:
	return 0;
}

static int sprd_tcpm_debug_show(struct seq_file *s, void *v)
{
	struct sprd_tcpm_port *port = (struct sprd_tcpm_port *)s->private;

	_sprd_tcpm_debug_show(port, s);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(sprd_tcpm_debug);

static struct dentry *rootdir;

static void sprd_tcpm_debugfs_init(struct sprd_tcpm_port *port)
{
	if (!port->enable_tcpm_log)
		return;
	/* /sys/kernel/debug/tcpm/usbcX */
	if (!rootdir)
		rootdir = debugfs_create_dir("sprd_tcpm", NULL);

	port->dentry = debugfs_create_file(dev_name(port->dev),
					   S_IFREG | 0444, rootdir,
					   port, &sprd_tcpm_debug_fops);
}

static void sprd_tcpm_debugfs_exit(struct sprd_tcpm_port *port)
{
	int i;

	if (!port->enable_tcpm_log)
		return;

	mutex_lock(&port->logbuffer_lock);
	for (i = 0; i < SPRD_LOG_BUFFER_ENTRIES; i++) {
		kfree(port->logbuffer[i]);
		port->logbuffer[i] = NULL;
	}
	mutex_unlock(&port->logbuffer_lock);

	debugfs_remove(port->dentry);
}

#else
static void sprd_tcpm_debugfs_init(const struct sprd_tcpm_port *port) { }
static void sprd_tcpm_debugfs_exit(const struct sprd_tcpm_port *port) { }

#endif

static ssize_t sprd_tcpm_log_ctl_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		 container_of(attr, struct sprd_tcpm_sysfs, attr_log_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;

	if (!port)
		return snprintf(buf, PAGE_SIZE, "%s tcpm_sysfs->port is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "1(enable), 0(disable), enable_tcpm_log = %d\n",
			port->enable_tcpm_log);
}

static ssize_t sprd_tcpm_log_ctl_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		container_of(attr, struct sprd_tcpm_sysfs, attr_log_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;
	int ret;
	bool enbale_log_ctl;

	if (!port) {
		pr_err("%s tcpm_sysfs->port is null\n", __func__);
		return count;
	}

	ret = kstrtobool(buf, &enbale_log_ctl);
	if (ret) {
		pr_err("%s: store log ctl fail\n", __func__);
		return count;
	}

	if (enbale_log_ctl && !port->enable_tcpm_log) {
		port->enable_tcpm_log = true;
		kthread_queue_delayed_work(&port->log_kworker, &port->log_kwork,
					   msecs_to_jiffies(15000));
	} else if (!enbale_log_ctl && port->enable_tcpm_log) {
		port->enable_tcpm_log = false;
	}

	pr_info("%s store enbale_log_ctl = %d success\n", __func__, enbale_log_ctl);
	return count;
}

static ssize_t sprd_tcpm_vbus_wait_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		 container_of(attr, struct sprd_tcpm_sysfs, attr_vbus_wait_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;

	if (!port)
		return snprintf(buf, PAGE_SIZE, "%s tcpm_sysfs->port is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", port->vbus_wait);
}

static ssize_t sprd_tcpm_vbus_wait_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		container_of(attr, struct sprd_tcpm_sysfs, attr_vbus_wait_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;
	int value = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		dev_err(dev, "input err:%d\n", ret);
		return count;
	}

	pr_info("vbus wait input: %d, current %d\n", value, port->vbus_wait);

	port->vbus_wait = value;
	return count;
}

static ssize_t sprd_tcpm_tcc_debounce_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		 container_of(attr, struct sprd_tcpm_sysfs, attr_tcc_debounce_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;

	if (!port)
		return snprintf(buf, PAGE_SIZE, "%s tcpm_sysfs->port is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", port->tcc_debounce);
}

static ssize_t sprd_tcpm_tcc_debounce_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		container_of(attr, struct sprd_tcpm_sysfs, attr_tcc_debounce_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;
	int value = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		dev_err(dev, "input err:%d\n", ret);
		return count;
	}

	pr_info("tcc debounce input: %d, current %d\n", value, port->tcc_debounce);

	port->tcc_debounce = value;
	return count;
}

static ssize_t sprd_tcpm_first_pd_cap_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		 container_of(attr, struct sprd_tcpm_sysfs, attr_first_pd_cap_delay_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;

	if (!port)
		return snprintf(buf, PAGE_SIZE, "%s tcpm_sysfs->port is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", port->first_pd_cap_delay);
}

static ssize_t sprd_tcpm_first_pd_cap_delay_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs =
		container_of(attr, struct sprd_tcpm_sysfs, attr_first_pd_cap_delay_ctl);
	struct sprd_tcpm_port *port = tcpm_sysfs->port;
	int value = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		dev_err(dev, "input err:%d\n", ret);
		return count;
	}

	pr_info("pd cap delay input: %d, current %d\n", value, port->first_pd_cap_delay);

	port->first_pd_cap_delay = value;
	return count;
}

static int sprd_tcpm_debug_log_register_sysfs(struct sprd_tcpm_port *port)
{
	struct sprd_tcpm_sysfs *tcpm_sysfs;
	int ret;

	tcpm_sysfs = devm_kzalloc(port->dev, sizeof(*tcpm_sysfs), GFP_KERNEL);
	if (!tcpm_sysfs)
		return -ENOMEM;

	port->sysfs = tcpm_sysfs;
	tcpm_sysfs->name = "sprd_tcpm_sysfs";
	tcpm_sysfs->port = port;
	tcpm_sysfs->attrs[0] = &tcpm_sysfs->attr_log_ctl.attr;
	tcpm_sysfs->attrs[1] = &tcpm_sysfs->attr_vbus_wait_ctl.attr;
	tcpm_sysfs->attrs[2] = &tcpm_sysfs->attr_tcc_debounce_ctl.attr;
	tcpm_sysfs->attrs[3] = &tcpm_sysfs->attr_first_pd_cap_delay_ctl.attr;
	tcpm_sysfs->attrs[4] = NULL;
	tcpm_sysfs->attr_g.name = "debug";
	tcpm_sysfs->attr_g.attrs = tcpm_sysfs->attrs;

	sysfs_attr_init(&tcpm_sysfs->attr_log_ctl.attr);
	tcpm_sysfs->attr_log_ctl.attr.name = "log_ctl";
	tcpm_sysfs->attr_log_ctl.attr.mode = 0644;
	tcpm_sysfs->attr_log_ctl.show = sprd_tcpm_log_ctl_show;
	tcpm_sysfs->attr_log_ctl.store = sprd_tcpm_log_ctl_store;

	sysfs_attr_init(&tcpm_sysfs->attr_vbus_wait_ctl.attr);
	tcpm_sysfs->attr_vbus_wait_ctl.attr.name = "vbus_wait";
	tcpm_sysfs->attr_vbus_wait_ctl.attr.mode = 0644;
	tcpm_sysfs->attr_vbus_wait_ctl.show = sprd_tcpm_vbus_wait_show;
	tcpm_sysfs->attr_vbus_wait_ctl.store = sprd_tcpm_vbus_wait_store;

	sysfs_attr_init(&tcpm_sysfs->attr_tcc_debounce_ctl.attr);
	tcpm_sysfs->attr_tcc_debounce_ctl.attr.name = "tcc_debounce";
	tcpm_sysfs->attr_tcc_debounce_ctl.attr.mode = 0644;
	tcpm_sysfs->attr_tcc_debounce_ctl.show = sprd_tcpm_tcc_debounce_show;
	tcpm_sysfs->attr_tcc_debounce_ctl.store = sprd_tcpm_tcc_debounce_store;

	sysfs_attr_init(&tcpm_sysfs->attr_first_pd_cap_delay_ctl.attr);
	tcpm_sysfs->attr_first_pd_cap_delay_ctl.attr.name = "first_pd_cap_delay";
	tcpm_sysfs->attr_first_pd_cap_delay_ctl.attr.mode = 0644;
	tcpm_sysfs->attr_first_pd_cap_delay_ctl.show = sprd_tcpm_first_pd_cap_delay_show;
	tcpm_sysfs->attr_first_pd_cap_delay_ctl.store = sprd_tcpm_first_pd_cap_delay_store;


	/* file node: /sys/class/power_supply/sprd-tcpm-source-psy-sc27xx-pd/debug */
	ret = sysfs_create_group(&port->psy->dev.kobj, &tcpm_sysfs->attr_g);
	if (ret < 0)
		pr_err("%s:Cannot create sysfs , ret = %d\n", __func__, ret);

	return ret;
}

static void sprd_tcpm_debug_log_init(struct sprd_tcpm_port *port)
{
	mutex_init(&port->logbuffer_lock);
	mutex_init(&port->logprintk_lock);
	kthread_init_worker(&port->log_kworker);
	kthread_init_delayed_work(&port->log_kwork, sprd_tcpm_log_print_kwork);
}

static int sprd_tcpm_debug_log_switch(struct sprd_tcpm_port *port)
{
	int ret;

	if (!port) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return 0;
	}

	/*
	 * the userdebug version pd log enable by default,
	 * you can define userdebug-pd-log-disable in dts
	 * if you don't use this function.
	 * the user version pd log disable by default,
	 * you can define user-pd-log-enable in dts
	 * if you want to use this function.
	 */

#if IS_ENABLED(CONFIG_DEBUG_FS)
	port->enable_tcpm_log = true;
#else
	port->enable_tcpm_log = false;
#endif

	if (!port->dev || !port->dev->of_node) {
		pr_info("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return 0;
	}

#if IS_ENABLED(CONFIG_DEBUG_FS)
	ret = of_property_read_bool(port->dev->of_node, "userdebug-pd-log-disable");
	if (ret) {
		port->enable_tcpm_log = false;
		pr_info("[%s]userdebug-pd-log-disable\n", __func__);
	}
#else
	ret = of_property_read_bool(port->dev->of_node, "user-pd-log-enable");
	if (ret) {
		port->enable_tcpm_log = true;
		pr_info("[%s]user-pd-log-enable\n", __func__);
	}
#endif

	return 0;
}

static void sprd_tcpm_typec_pr_swap_no_chk_detach(struct sprd_tcpm_port *port, bool on)
{
	sprd_tcpm_log(port, "%s, cur state: %s, on: %d",
		      __func__, sprd_tcpm_states[port->state], on);
	if (g_sprd_typec_device_ops &&
	    g_sprd_typec_device_ops->typec_pr_swap_no_chk_detach)
		g_sprd_typec_device_ops->typec_pr_swap_no_chk_detach(on);
}

static int sprd_tcpm_pd_transmit(struct sprd_tcpm_port *port,
				 enum sprd_tcpm_transmit_type type,
				 const struct sprd_pd_message *msg)
{
	unsigned long timeout;
	int ret, wait_time = SPRD_PD_T_TCPC_TX_TIMEOUT;

	if (msg)
		sprd_tcpm_log(port, "PD TX, header: %#x", msg->header);
	else
		sprd_tcpm_log(port, "PD TX, type: %#x", type);

	reinit_completion(&port->tx_complete);
	ret = port->tcpc->pd_transmit(port->tcpc, type, msg);
	port->tx_complete_curr_time = ktime_to_ms(ktime_get_boottime());
	sprd_tcpm_log(port, "%s: tx_complete_curr_time = %lld ms",
		      __func__, port->tx_complete_curr_time);
	if (ret < 0)
		return ret;

	if (type == SPRD_TCPC_TX_HARD_RESET)
		wait_time = SPRD_PD_T_TCPC_TX_MIN_TIMEOUT;

	mutex_unlock(&port->lock);
	timeout = wait_for_completion_timeout(&port->tx_complete,
					      msecs_to_jiffies(wait_time));
	mutex_lock(&port->lock);
	if (!timeout) {
		sprd_tcpm_log(port, "PD TX time out");
		return -ETIMEDOUT;
	}

	switch (port->tx_status) {
	case SPRD_TCPC_TX_SUCCESS:
		port->message_id = (port->message_id + 1) & SPRD_PD_HEADER_ID_MASK;
		return 0;
	case SPRD_TCPC_TX_DISCARDED:
		return -EAGAIN;
	case SPRD_TCPC_TX_FAILED:
	default:
		return -EIO;
	}
}

void sprd_tcpm_pd_transmit_complete(struct sprd_tcpm_port *port,
				    enum sprd_tcpm_transmit_status status)
{
	sprd_tcpm_log(port, "PD TX complete, status: %u", status);
	port->tx_status = status;
	complete(&port->tx_complete);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_pd_transmit_complete);

static int sprd_tcpm_mux_set(struct sprd_tcpm_port *port, int state,
			     enum usb_role usb_role,
			     enum typec_orientation orientation)
{
	int ret;

	sprd_tcpm_log(port, "Requesting mux state %d, usb-role %d, orientation %d",
		      state, usb_role, orientation);

	if (port->orientation != orientation) {
		ret = typec_set_orientation(port->typec_port, orientation);
		if (ret)
			return ret;

		sprd_tcpm_log(port, "Requesting new orientation %d, old orientation %d",
			      orientation, port->orientation);
		port->orientation = orientation;
	}

	if (port->role_sw) {
		ret = usb_role_switch_set_role(port->role_sw, usb_role);
		if (ret)
			return ret;
	}

	return typec_set_mode(port->typec_port, state);
}

static int sprd_tcpm_set_polarity(struct sprd_tcpm_port *port,
				  enum sprd_typec_cc_polarity polarity)
{
	int ret;

	sprd_tcpm_log(port, "polarity %d", polarity);

	ret = port->tcpc->set_polarity(port->tcpc, polarity);
	if (ret < 0)
		return ret;

	port->polarity = polarity;

	return 0;
}

static int sprd_tcpm_set_vconn(struct sprd_tcpm_port *port, bool enable)
{
	int ret;

	sprd_tcpm_log(port, "vconn:=%d", enable);

	ret = port->tcpc->set_vconn(port->tcpc, enable);
	if (!ret) {
		port->vconn_role = enable ? TYPEC_SOURCE : TYPEC_SINK;
		typec_set_vconn_role(port->typec_port, port->vconn_role);
	}

	return ret;
}

static u32 sprd_tcpm_get_current_limit(struct sprd_tcpm_port *port)
{
	enum sprd_typec_cc_status cc;
	u32 limit;

	cc = port->polarity ? port->cc2 : port->cc1;
	switch (cc) {
	case SPRD_TYPEC_CC_RP_1_5:
		limit = 1500;
		break;
	case SPRD_TYPEC_CC_RP_3_0:
		limit = 3000;
		break;
	case SPRD_TYPEC_CC_RP_DEF:
	default:
		if (port->tcpc->get_current_limit)
			limit = port->tcpc->get_current_limit(port->tcpc);
		else
			limit = 100;
		break;
	}

	sprd_tcpm_log(port, "get_rp_limit %u mA", limit);

	return limit;
}

static void sprd_tcpm_set_rp_limit_current(int req_cur_ua)
{
	if (g_sprd_charger_ops && g_sprd_charger_ops->set_rp_limit_current)
		g_sprd_charger_ops->set_rp_limit_current(req_cur_ua);
}


static int sprd_tcpm_set_current_limit(struct sprd_tcpm_port *port, u32 max_ma, u32 mv)
{
	int ret = -EOPNOTSUPP;

	sprd_tcpm_log(port, "Setting voltage/current limit %u mV %u mA", mv, max_ma);

	port->supply_voltage = mv;
	port->current_limit = max_ma;

	if (port->tcpc->set_current_limit)
		ret = port->tcpc->set_current_limit(port->tcpc, max_ma, mv);

	if (port->set_rp_limint_en) {
		sprd_tcpm_set_rp_limit_current(max_ma);
		port->negotiated_limit_cur = max_ma;
		port->rp_limit = max_ma;
		port->set_rp_limint_en = false;
	}

	return ret;
}

static void sprd_mod_tcpm_rp_delayed_work(struct sprd_tcpm_port *port, unsigned int delay_ms)
{
	if (unlikely(!port->registered)) {
		pr_info("%s:line%d: port unregister\n", __func__, __LINE__);
		return;
	}

	if (delay_ms) {
		hrtimer_start(&port->rp_state_change_timer, ms_to_ktime(delay_ms),
			      HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&port->rp_state_change_timer);
		kthread_queue_work(&port->tcpm_kworker, &port->rp_state_work);
	}
}

/*
 * Determine RP value to set based on maximum current supported
 * by a port if configured as source.
 * Returns CC value to report to link partner.
 */
static enum sprd_typec_cc_status sprd_tcpm_rp_cc(struct sprd_tcpm_port *port)
{
	const u32 *src_pdo = port->src_pdo;
	int nr_pdo = port->nr_src_pdo;
	int i;

	/*
	 * Search for first entry with matching voltage.
	 * It should report the maximum supported current.
	 */
	for (i = 0; i < nr_pdo; i++) {
		const u32 pdo = src_pdo[i];

		if (sprd_pdo_type(pdo) == SPRD_PDO_TYPE_FIXED &&
		    sprd_pdo_fixed_voltage(pdo) == 5000) {
			unsigned int curr = sprd_pdo_max_current(pdo);

			if (curr >= 3000)
				return SPRD_TYPEC_CC_RP_3_0;
			else if (curr >= 1500)
				return SPRD_TYPEC_CC_RP_1_5;
			return SPRD_TYPEC_CC_RP_DEF;
		}
	}

	return SPRD_TYPEC_CC_RP_DEF;
}

static int sprd_tcpm_set_attached_state(struct sprd_tcpm_port *port, bool attached)
{
	return port->tcpc->set_roles(port->tcpc, attached, port->pwr_role,
				     port->data_role);
}

static int sprd_tcpm_set_roles(struct sprd_tcpm_port *port, bool attached,
			       enum typec_role role, enum typec_data_role data)
{
	enum typec_orientation orientation;
	enum usb_role usb_role;
	int ret;

	sprd_tcpm_log(port, "%s:line%d set roles [%s:%s]", __func__, __LINE__,
		      role ? "source" : "sink", data ? "host" : "device");

	if (port->polarity == SPRD_TYPEC_POLARITY_CC1)
		orientation = TYPEC_ORIENTATION_NORMAL;
	else
		orientation = TYPEC_ORIENTATION_REVERSE;

	if (data == TYPEC_HOST)
		usb_role = USB_ROLE_HOST;
	else
		usb_role = USB_ROLE_DEVICE;

	ret = sprd_tcpm_mux_set(port, TYPEC_STATE_USB, usb_role, orientation);
	if (ret < 0)
		return ret;

	ret = port->tcpc->set_roles(port->tcpc, attached, role, data);
	if (ret < 0)
		return ret;

	port->pwr_role = role;
	port->data_role = data;
	typec_set_data_role(port->typec_port, data);
	typec_set_pwr_role(port->typec_port, role);

	return 0;
}

static int sprd_tcpm_set_pwr_role(struct sprd_tcpm_port *port, enum typec_role role)
{
	int ret;

	ret = port->tcpc->set_roles(port->tcpc, true, role,
				    port->data_role);
	if (ret < 0)
		return ret;

	port->pwr_role = role;
	typec_set_pwr_role(port->typec_port, role);

	return 0;
}

/*
 * Transform the PDO to be compliant to PD rev2.0.
 * Return 0 if the PDO type is not defined in PD rev2.0.
 * Otherwise, return the converted PDO.
 */
static u32 sprd_tcpm_forge_legacy_pdo(struct sprd_tcpm_port *port, u32 pdo, enum typec_role role)
{
	switch (sprd_pdo_type(pdo)) {
	case SPRD_PDO_TYPE_FIXED:
		if (role == TYPEC_SINK)
			return pdo & ~SPRD_PDO_FIXED_FRS_CURR_MASK;
		else
			return pdo & ~SPRD_PDO_FIXED_UNCHUNK_EXT;
	case SPRD_PDO_TYPE_VAR:
	case SPRD_PDO_TYPE_BATT:
		return pdo;
	case SPRD_PDO_TYPE_APDO:
	default:
		return 0;
	}
}

static int sprd_tcpm_pd_send_source_caps(struct sprd_tcpm_port *port)
{
	struct power_supply *batt_psy = NULL;
	union power_supply_propval val;
	struct sprd_pd_message msg;
	int i = 0, ret;
	int temp;
	int capacity;

	memset(&msg, 0, sizeof(msg));
	if (!port->nr_src_pdo) {
		/* No source capabilities defined, sink only */
		msg.header = SPRD_PD_HEADER(SPRD_PD_CTRL_REJECT,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id, 0, 0);
	} else {
		if (!port->update_ext_src_caps) {
			msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_SOURCE_CAP,
						       port->pwr_role,
						       port->data_role,
						       port->negotiated_rev,
						       port->message_id,
						       port->nr_src_pdo, 0);
		} else {
			msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_SOURCE_CAP,
						       port->pwr_role,
						       port->data_role,
						       port->negotiated_rev,
						       port->message_id,
						       port->nr_src_pdo_ext, 0);
		}
	}
	if (!port->update_ext_src_caps) {
		port->src_pdo[0] = port->xvddsrc_pdo[1];
		batt_psy = power_supply_get_by_name("battery");
		if (!batt_psy)
			goto err;
		ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
		temp = val.intval;
		ret |= power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		capacity = val.intval;
		if (ret)
			goto err;
		if ((temp <= 0 && capacity <= 20) || temp <= -100){
			port->src_pdo[0] = port->xvddsrc_pdo[0];
			pr_err("%s, port->src_pdo[0] = %d\n", __func__,port->src_pdo[0]);
		}
		if (temp > 0 && capacity > 20 && capacity <= 40){
			port->src_pdo[0] = port->xvddsrc_pdo[2]; 
			pr_err("%s, port->src_pdo[0] = %d\n", __func__,port->src_pdo[0]);
		}
		if (temp > 0 && capacity > 40){
			port->src_pdo[0] = port->xvddsrc_pdo[3];
			pr_err("%s, port->src_pdo[0] = %d\n", __func__,port->src_pdo[0]);
		}
err:
		for (i = 0; i < port->nr_src_pdo; i++)
			msg.payload[i] = port->src_pdo[i];
	} else {
		for (i = 0; i < port->nr_src_pdo_ext; i++)
			msg.payload[i] = port->src_pdo_ext[i];
	}

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_pd_send_sink_caps(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	u32 pdo;
	unsigned int i, nr_pdo = 0;
	u32 snk_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_snk_pdo;

	nr_snk_pdo = port->nr_snk_default_pdo;
	for (i = 0; i < port->nr_snk_default_pdo; i++)
		snk_pdo[i] = port->snk_default_pdo[i];

	memset(&msg, 0, sizeof(msg));

	for (i = 0; i < nr_snk_pdo; i++) {
		if (port->negotiated_rev >= SPRD_PD_REV30) {
			msg.payload[nr_pdo++] = snk_pdo[i];
		} else {
			pdo = sprd_tcpm_forge_legacy_pdo(port, snk_pdo[i], TYPEC_SINK);
			if (pdo)
				msg.payload[nr_pdo++] = pdo;
		}
	}

	if (!nr_pdo) {
		/* No sink capabilities defined, source only */
		msg.header = SPRD_PD_HEADER(SPRD_PD_CTRL_REJECT,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id, 0, 0);
	} else {
		msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_SINK_CAP,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id,
					       nr_pdo, 0);
	}

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_pd_send_revision(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	u32 revision_msg = 0;
	u32 rmajor = 3;
	u32 rminor = 1;
	u32 vmajor = 1;
	u32 vminor = 8;

	revision_msg = (rmajor << 28  | rminor << 24 | vmajor << 20 | vminor << 16);

	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_REVISION,
				       port->pwr_role,
				       port->data_role,
				       port->negotiated_rev,
				       port->message_id, 1, 0);
	msg.payload[0] = revision_msg;

	sprd_tcpm_log(port, "send revision msg");

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_pd_send_ext_msg(struct sprd_tcpm_port *port, u8 msg_type,
				     const u8 *data, u32 data_len)
{
	struct sprd_pd_message msg;
	int ret;
	u8 num_objs;
	u32 len_remain, chunk_len, len_max = SPRD_PD_EXT_MAX_CHUNK_DATA;
	bool chunked = 1;//must be 1,use chunk,hardware not support unchunked
	bool req_chunk = 0;// response set to 0,request set to 1
	u32 chunk_num = 0, offset_data = 0;
	unsigned long timeout;

	if (data_len > SPRD_PD_EXT_MAX_MSG_LEN) {
		sprd_tcpm_log(port, "ext msg len exceeds max data_len = %d", data_len);
		data_len = SPRD_PD_EXT_MAX_MSG_LEN;
	}

	len_remain = data_len;
	sprd_tcpm_log(port, "ext chunk msg, len_remain = %d", len_remain);
	do {
		chunk_len = min(len_remain, len_max);
		num_objs = DIV_ROUND_UP(chunk_len + sizeof(u16), sizeof(u32));

		memset(&msg, 0, sizeof(msg));
		msg.header = SPRD_PD_HEADER_EXT(msg_type, port->pwr_role,
						   port->data_role, port->negotiated_rev,
						   port->message_id, num_objs);

		msg.ext_msg.header = SPRD_PD_EXT_HDR(data_len, req_chunk,
							chunk_num++, chunked);

		memcpy(msg.ext_msg.data, data + offset_data, chunk_len);
		len_remain -= chunk_len;
		offset_data += chunk_len;

		sprd_tcpm_log(port, "chunk msg, len_remain = %d, offset_data = %d",
			      len_remain, offset_data);

		reinit_completion(&port->tx_chunk_request);
		ret = sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
		if (ret) {
			sprd_tcpm_log(port, "failed to send ext msg, ret = %d", ret);
			return ret;
		}

		/* wait for request chunk */
		if (len_remain) {
			mutex_unlock(&port->lock);
			port->tx_chunk_msg = true;
			sprd_tcpm_log(port, "unlock, len_remain = %d", len_remain);
			timeout = wait_for_completion_timeout(&port->tx_chunk_request,
						msecs_to_jiffies(SPRD_PD_T_SENDER_RESPONSE));
			port->tx_chunk_msg = false;
			sprd_tcpm_log(port, "lock, len_remain = %d", len_remain);
			mutex_lock(&port->lock);
			if (!timeout) {
				sprd_tcpm_log(port, "time out waiting for chunk rwquest");
				return -ETIMEDOUT;
			}
		}
	} while (len_remain);

	return 0;
}

static int sprd_tcpm_pd_send_sink_cap_ext(struct sprd_tcpm_port *port)
{
	int ret;
	struct {
		u16 vid;
		u16 pid;
		u32 xid;
		u8 fw_version;
		u8 hw_version;
		u8 skedb_version;
		u8 load_step;
		u16 sink_load_characteristics;
		u8 compliance;
		u8 touch_temp;
		u8 battery_info;
		u8 sink_modes;
		u8 sink_minimum_pdp;
		u8 sink_operational_pdp;
		u8 sink_maximum_pdp;
		u8 epr_sink_minimum_pdp;
		u8 epr_sink_operational_pdp;
		u8 epr_sink_maximum_pdp;
	} __packed cap = {0};

	cap.vid = 1782;//according to your product
	cap.pid = 6360;//according to your product
	cap.skedb_version = 1;
	cap.battery_info = 1;
	ret = sprd_tcpm_pd_send_ext_msg(port, SPRD_PD_EXT_SINK_CAPABILITIES_EXTENDED,
					(u8 *)&cap, sizeof(cap));
	if (ret)
		return ret;

	return 0;
}

static int sprd_tcpm_pd_send_battery_cap_ext(struct sprd_tcpm_port *port, u8 ext_msg_data)
{
	int ret;
	u8 bat_num;
	struct {
		u16 vid;
		u16 pid;
		u16 battery_design_capacity;
		u16 battery_last_full_charge_capacity;
		u8 battery_type;
	} __packed bcdb = {1782, 6360, 0xffff, 0xffff, 0};

	bat_num = ext_msg_data;

	if (bat_num)
		bcdb.battery_type = BIT(0);

	if (bat_num > 7  && bat_num <= 255) {
		bcdb.vid = 0xffff;
		bcdb.pid = 0;
	}

	ret = sprd_tcpm_pd_send_ext_msg(port, SPRD_PD_EXT_BATT_CAP,
					(u8 *)&bcdb, sizeof(bcdb));
	if (ret)
		return ret;

	return 0;
}

static int sprd_tcpm_pd_send_battery_status(struct sprd_tcpm_port *port, u8 ext_msg_data)
{
	int ret;
	int cap;
	union power_supply_propval val = {0};
	u8 bat_num;
	u32 bsdo = 0xffff0000;
	struct power_supply *psy_battery;
	struct sprd_pd_message msg;

	bat_num = ext_msg_data;
	psy_battery = power_supply_get_by_name("battery");
	if (!psy_battery) {
		sprd_tcpm_log(port, "%s, psy_battery is NULL\n", __func__);
		bsdo |= BIT(8);
		goto send;
	}

	if (bat_num) {
		sprd_tcpm_log(port, "battery %d don't exit", bat_num);
		bsdo |= BIT(8);
		goto send;
	}

	ret = power_supply_get_property(psy_battery, POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret || !val.intval) {
		sprd_tcpm_log(port, "%s, failed to get present, ret=%d\n", __func__, ret);
		goto send;
	}

	bsdo |= BIT(9);

	ret = power_supply_get_property(psy_battery, POWER_SUPPLY_PROP_STATUS, &val);
	if (!ret) {
		switch (val.intval) {
		case POWER_SUPPLY_STATUS_CHARGING:
			break;
		case POWER_SUPPLY_STATUS_DISCHARGING:
			bsdo |= (1 << 10);
			break;
		default:
			bsdo |= (2 << 10);
			break;
		}
	}

	ret = power_supply_get_property(psy_battery, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret) {
		sprd_tcpm_log(port, "%s, failed to get capacity, ret=%d\n", __func__, ret);
		goto send;
	}

	cap = val.intval;

	bsdo &= 0xffff;
	bsdo |= cap << 16;//to do Wh

send:
	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_BATT_STATUS,
					   port->pwr_role,
					   port->data_role,
					   port->negotiated_rev,
					   port->message_id, 1, 0);
	msg.payload[0] = bsdo;

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static void sprd_mod_tcpm_delayed_work(struct sprd_tcpm_port *port, unsigned int delay_ms)
{
	if (unlikely(!port->registered)) {
		pr_info("%s:line%d: port unregister\n", __func__, __LINE__);
		return;
	}

	if (delay_ms) {
		hrtimer_start(&port->state_machine_timer, ms_to_ktime(delay_ms), HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&port->state_machine_timer);
		kthread_queue_work(&port->tcpm_kworker, &port->state_machine);
	}
}

static void sprd_mod_vdm_delayed_work(struct sprd_tcpm_port *port, unsigned int delay_ms)
{
	if (unlikely(!port->registered)) {
		pr_info("%s:line%d: port unregister\n", __func__, __LINE__);
		return;
	}

	if (delay_ms) {
		hrtimer_start(&port->vdm_state_machine_timer, ms_to_ktime(delay_ms),
			      HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&port->vdm_state_machine_timer);
		kthread_queue_work(&port->tcpm_kworker, &port->vdm_state_machine);
	}
}

static void sprd_tcpm_set_state(struct sprd_tcpm_port *port, enum sprd_tcpm_state state,
				unsigned int delay_ms)
{
	if (delay_ms) {
		sprd_tcpm_log(port, "pending state change %s -> %s @ %u ms",
			      sprd_tcpm_states[port->state], sprd_tcpm_states[state],
			      delay_ms);
		port->delayed_state = state;
		sprd_mod_tcpm_delayed_work(port, delay_ms);
		port->delayed_runtime = ktime_add(ktime_get(), ms_to_ktime(delay_ms));
		port->delay_ms = delay_ms;
	} else {
		sprd_tcpm_log(port, "state change %s -> %s",
			      sprd_tcpm_states[port->state], sprd_tcpm_states[state]);
		port->delayed_state = INVALID_STATE;
		port->prev_state = port->state;
		port->state = state;
		/*
		 * Don't re-queue the state machine work item if we're currently
		 * in the state machine and we're immediately changing states.
		 * sprd_tcpm_state_machine_work() will continue running the state
		 * machine.
		 */
		if (!port->state_machine_running)
			sprd_mod_tcpm_delayed_work(port, 0);
	}
}

static void sprd_tcpm_set_state_cond(struct sprd_tcpm_port *port, enum sprd_tcpm_state state,
				     unsigned int delay_ms)
{
	if (port->enter_state == port->state)
		sprd_tcpm_set_state(port, state, delay_ms);
	else
		sprd_tcpm_log(port,
			      "skipped %sstate change %s -> %s [%u ms], context state %s",
			      delay_ms ? "delayed " : "",
			      sprd_tcpm_states[port->state], sprd_tcpm_states[state],
			      delay_ms, sprd_tcpm_states[port->enter_state]);
}

static void sprd_tcpm_dp_vdm_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_tcpm_port *port = container_of(dwork, struct sprd_tcpm_port, dp_work);

	if (port->tcpc->dp_altmode_notify) {
		sprd_tcpm_log(port, "%s:line%d status update", __func__, __LINE__);
		port->tcpc->dp_altmode_notify(port->tcpc, port->dp_status);
		port->dp_status = 0;
	}
}

static void sprd_tcpm_queue_message(struct sprd_tcpm_port *port,
				    enum sprd_pd_msg_request message)
{
	port->queued_message = message;
	sprd_mod_tcpm_delayed_work(port, 0);
}

static void sprd_tcpm_queue_chunk_message(struct sprd_tcpm_port *port,
					  enum sprd_pd_chunk_msg_request message)
{
	port->queued_chunk_message = message;
	queue_delayed_work(system_unbound_wq, &port->chunk_msg_work, 0);
}

/*
 * VDM/VDO handling functions
 */
static void sprd_tcpm_queue_vdm(struct sprd_tcpm_port *port, const u32 header,
				const u32 *data, int cnt)
{
	port->vdo_count = cnt + 1;
	port->vdo_data[0] = header;
	memcpy(&port->vdo_data[1], data, sizeof(u32) * cnt);
	/* Set ready, vdm state machine will actually send */
	port->vdm_retries = 0;
	port->vdm_state = VDM_STATE_READY;
}

static void sprd_svdm_consume_identity(struct sprd_tcpm_port *port,
				       const __le32 *payload, int cnt)
{
	u32 vdo = payload[SPRD_VDO_INDEX_IDH];
	u32 product = payload[SPRD_VDO_INDEX_PRODUCT];
	u32 product_type = 0;

	memset(&port->mode_data, 0, sizeof(port->mode_data));

	product_type = SPRD_PD_IDH_PTYPE(vdo);
	sprd_tcpm_log(port, "product_type %d", product_type);
	if (product_type == SPRD_IDH_PTYPE_HUB || product_type == SPRD_IDH_PTYPE_AMA) {
		sprd_tcpm_log(port, "product_type IDH_PTYPE_HUB or IDH_PTYPE_AMA");
		if (g_sprd_charger_ops && g_sprd_charger_ops->update_ac_usb_online)
			g_sprd_charger_ops->update_ac_usb_online(true);
		sprd_tcpm_log(port, "product_type IDH_PTYPE_HUB or IDH_PTYPE_AMA done");
	}

	port->partner_ident.id_header = vdo;
	port->partner_ident.cert_stat = payload[SPRD_VDO_INDEX_CSTAT];
	port->partner_ident.product = product;

	typec_partner_set_identity(port->partner);

	sprd_tcpm_log(port, "Identity: %04x:%04x.%04x",
		      SPRD_PD_IDH_VID(vdo),
		      SPRD_PD_PRODUCT_PID(product), product & 0xffff);
}

static bool sprd_svdm_consume_svids(struct sprd_tcpm_port *port,
				    const __le32 *payload, int cnt)
{
	struct sprd_pd_mode_data *pmdata = &port->mode_data;
	int i;

	for (i = 1; i < cnt; i++) {
		u32 p = payload[i];
		u16 svid;

		svid = (p >> 16) & 0xffff;
		if (!svid)
			return false;

		if (pmdata->nsvids >= SPRD_SVID_DISCOVERY_MAX)
			goto abort;

		pmdata->svids[pmdata->nsvids++] = svid;
		sprd_tcpm_log(port, "SVID %d: 0x%x", pmdata->nsvids, svid);

		svid = p & 0xffff;
		if (!svid)
			return false;

		if (pmdata->nsvids >= SPRD_SVID_DISCOVERY_MAX)
			goto abort;

		pmdata->svids[pmdata->nsvids++] = svid;
		sprd_tcpm_log(port, "SVID %d: 0x%x", pmdata->nsvids, svid);
	}
	return true;
abort:
	sprd_tcpm_log(port, "SPRD_SVID_DISCOVERY_MAX(%d) too low!", SPRD_SVID_DISCOVERY_MAX);
	return false;
}

static void sprd_svdm_consume_modes(struct sprd_tcpm_port *port,
				    const __le32 *payload, int cnt)
{
	struct sprd_pd_mode_data *pmdata = &port->mode_data;
	struct typec_altmode_desc *paltmode;
	int i;

	if (pmdata->altmodes >= ARRAY_SIZE(port->partner_altmode)) {
		/* Already logged in sprd_svdm_consume_svids() */
		return;
	}

	for (i = 1; i < cnt; i++) {
		paltmode = &pmdata->altmode_desc[pmdata->altmodes];
		memset(paltmode, 0, sizeof(*paltmode));

		paltmode->svid = pmdata->svids[pmdata->svid_index];
		paltmode->mode = i;
		paltmode->vdo = payload[i];

		sprd_tcpm_log(port, " Alternate mode %d: SVID 0x%04x, VDO %d: 0x%08x",
			      pmdata->altmodes, paltmode->svid,
			      paltmode->mode, paltmode->vdo);

		pmdata->altmodes++;
	}
}

static void sprd_tcpm_register_partner_altmodes(struct sprd_tcpm_port *port)
{
	struct sprd_pd_mode_data *modep = &port->mode_data;
	struct typec_altmode *altmode;
	int i;

	for (i = 0; i < modep->altmodes; i++) {
		altmode = typec_partner_register_altmode(port->partner, &modep->altmode_desc[i]);
		if (IS_ERR(altmode)) {
			sprd_tcpm_log(port, "Failed to register partner SVID 0x%04x",
				      modep->altmode_desc[i].svid);
			altmode = NULL;
		}
		port->partner_altmode[i] = altmode;
	}
}

static void sprd_tcpm_typec_altmode_attention(struct sprd_tcpm_port *port,
					      struct typec_altmode *adev, u32 vdo)
{
	const struct typec_altmode *pdev;

	pdev = typec_altmode_get_partner(adev);
	if (!pdev) {
		sprd_tcpm_log(port, "%s:line%d: NULL pointer!!!", __func__, __LINE__);
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	typec_altmode_attention(adev, vdo);
}

#define supports_modal(port)	SPRD_PD_IDH_MODAL_SUPP((port)->partner_ident.id_header)

static int sprd_tcpm_pd_svdm(struct sprd_tcpm_port *port,
			     const __le32 *payload, int cnt, u32 *response)
{
	struct typec_port *typec = port->typec_port;
	struct typec_altmode *adev;
	struct typec_altmode *pdev;
	struct sprd_pd_mode_data *modep;
	u32 p[SPRD_PD_MAX_PAYLOAD];
	int svdm_version;
	int svdm_version_minor;
	int rlen = 0;
	int cmd_type;
	int cmd;
	int i;

	for (i = 0; i < cnt; i++)
		p[i] = payload[i];

	cmd_type = SPRD_PD_VDO_CMDT(p[0]);
	cmd = SPRD_PD_VDO_CMD(p[0]);

	sprd_tcpm_log(port, "Rx VDM cmd 0x%x type %d cmd %d len %d", p[0], cmd_type, cmd, cnt);

	modep = &port->mode_data;

	adev = typec_match_altmode(port->port_altmode, SPRD_ALTMODE_DISCOVERY_MAX,
				   SPRD_PD_VDO_VID(p[0]), SPRD_PD_VDO_OPOS(p[0]));

	pdev = typec_match_altmode(port->partner_altmode, SPRD_ALTMODE_DISCOVERY_MAX,
				   SPRD_PD_VDO_VID(p[0]), SPRD_PD_VDO_OPOS(p[0]));

	svdm_version = typec_get_negotiated_svdm_version(typec);
	if (svdm_version < 0) {
		sprd_tcpm_log_force(port, "%s, negotiated svdm_version(%d) is error!!!",
				    __func__, svdm_version);
		return 0;
	}

	svdm_version_minor = port->negotiated_svdm_ver_minor;

	switch (cmd_type) {
	case SPRD_CMDT_INIT:
		switch (cmd) {
		case SPRD_CMD_DISCOVER_IDENT:
			if (SPRD_PD_VDO_VID(p[0]) != SPRD_USB_SID_PD)
				break;

			if (IS_ERR_OR_NULL(port->partner))
				break;

			if (SPRD_PD_VDO_SVDM_VER(p[0]) < svdm_version) {
				typec_partner_set_svdm_version(port->partner,
							       SPRD_PD_VDO_SVDM_VER(p[0]));
				svdm_version = SPRD_PD_VDO_SVDM_VER(p[0]);
			}

			if (SPRD_PD_VDO_SVDM_VER_MINOR(p[0]) < svdm_version_minor) {
				svdm_version_minor = SPRD_PD_VDO_SVDM_VER_MINOR(p[0]);
				port->negotiated_svdm_ver_minor = svdm_version_minor;
			}

			/* 6.4.4.3.1: Only respond as UFP (device) */
			if (port->data_role == TYPEC_DEVICE &&
			    port->nr_snk_vdo) {
				for (i = 0; i <  port->nr_snk_vdo; i++)
					response[i + 1] = port->snk_vdo[i];
				rlen = port->nr_snk_vdo + 1;
			}
			if (port->data_role == TYPEC_HOST) {
				sprd_tcpm_log(port, "Rx VDM from ufp");
				port->vdm_discovery_id_retry = 1;
			}
			break;
		case SPRD_CMD_DISCOVER_SVID:
			break;
		case SPRD_CMD_DISCOVER_MODES:
			break;
		case SPRD_CMD_ENTER_MODE:
			break;
		case SPRD_CMD_EXIT_MODE:
			break;
		case SPRD_CMD_ATTENTION:
			/* Attention command does not have response */
			if (adev) {
				sprd_tcpm_source_release_wake_lock(port);
				sprd_tcpm_typec_altmode_attention(port, adev, p[1]);
				cancel_delayed_work(&port->dp_work);
				if (port->tcpc->dp_altmode_notify) {
					sprd_tcpm_log(port, "%s:line%d CMD_ATTENTION", __func__, __LINE__);
					port->tcpc->dp_altmode_notify(port->tcpc, p[1]);
				}
			}
			return 0;
		default:
			break;
		}

		if (rlen >= 1) {
			response[0] = p[0] | SPRD_VDO_CMDT(SPRD_CMDT_RSP_ACK);
		} else if (rlen == 0) {
			response[0] = p[0] | SPRD_VDO_CMDT(SPRD_CMDT_RSP_NAK);
			rlen = 1;
		} else {
			response[0] = p[0] | SPRD_VDO_CMDT(SPRD_CMDT_RSP_BUSY);
			rlen = 1;
		}

		response[0] = (response[0] &
			       ~(SPRD_VDO_SVDM_VERS_MASK | SPRD_VDO_SVDM_VERS_MINOR_MASK)) |
			      (SPRD_VDO_SVDM_VERS(typec_get_negotiated_svdm_version(typec)) |
			       SPRD_VDO_SVDM_VERS_MINOR(svdm_version_minor));
		break;
	case SPRD_CMDT_RSP_ACK:
		/* silently drop message if we are not connected */
		if (IS_ERR_OR_NULL(port->partner))
			break;

		switch (cmd) {
		case SPRD_CMD_DISCOVER_IDENT:
			if (SPRD_PD_VDO_SVDM_VER(p[0]) < svdm_version)
				typec_partner_set_svdm_version(port->partner,
							       SPRD_PD_VDO_SVDM_VER(p[0]));

			if (SPRD_PD_VDO_SVDM_VER_MINOR(p[0]) < svdm_version_minor) {
				svdm_version_minor = SPRD_PD_VDO_SVDM_VER_MINOR(p[0]);
				port->negotiated_svdm_ver_minor = svdm_version_minor;
			}

			/* 6.4.4.3.1 */
			sprd_svdm_consume_identity(port, payload, cnt);
			response[0] = SPRD_VDO(SPRD_USB_SID_PD, 1,
					       typec_get_negotiated_svdm_version(typec),
					       svdm_version_minor,
					       SPRD_CMD_DISCOVER_SVID);
			rlen = 1;
			break;
		case SPRD_CMD_DISCOVER_SVID:
			/* 6.4.4.3.2 */
			if (sprd_svdm_consume_svids(port, payload, cnt)) {
				response[0] = SPRD_VDO(SPRD_USB_SID_PD, 1, svdm_version,
						       svdm_version_minor, SPRD_CMD_DISCOVER_SVID);
				rlen = 1;
			} else if (modep->nsvids && supports_modal(port)) {
				response[0] = SPRD_VDO(modep->svids[0], 1, svdm_version,
						       svdm_version_minor, SPRD_CMD_DISCOVER_MODES);
				rlen = 1;
			}
			break;
		case SPRD_CMD_DISCOVER_MODES:
			/* 6.4.4.3.3 */
			sprd_svdm_consume_modes(port, payload, cnt);
			modep->svid_index++;
			if (modep->svid_index < modep->nsvids) {
				u32 svid = modep->svids[modep->svid_index];
				if (svid == SPRD_USB_SID_PD || svid == SPRD_USB_SID_DISPLAYPORT ||
				    svid == SPRD_USB_SID_MHL) {
					response[0] = SPRD_VDO(svid, 1, svdm_version,
							       svdm_version_minor,
							       SPRD_CMD_DISCOVER_MODES);
					rlen = 1;
				} else {
					sprd_tcpm_register_partner_altmodes(port);
				}
			} else {
				sprd_tcpm_register_partner_altmodes(port);
			}
			break;
		case SPRD_CMD_ENTER_MODE:
			if (adev && pdev) {
				typec_altmode_update_active(pdev, true);

				if (typec_altmode_vdm(adev, p[0], &p[1], cnt)) {
					response[0] = SPRD_VDO(adev->svid, 1, svdm_version,
							       svdm_version_minor,
							       SPRD_CMD_EXIT_MODE);
					response[0] |= SPRD_VDO_OPOS(adev->mode);
					return 1;
				}
			}
			return 0;
		case SPRD_CMD_EXIT_MODE:
			if (adev && pdev) {
				typec_altmode_update_active(pdev, false);

				/* Back to USB Operation */
				WARN_ON(typec_altmode_notify(adev,
							     TYPEC_STATE_USB,
							     NULL));
			}
			break;
		case SPRD_CMD_DP_STATUS_UPDATE:
			port->dp_status = p[1];
			sprd_tcpm_log(port, "DP_STATUS_UPDATE status 0x%x", p[1]);
			break;
		case SPRD_CMD_DP_CONFIGURE:
			sprd_tcpm_log(port, "DP_CONFIGURE status = 0x%x", port->dp_status);
			if (port->dp_status & 0x80)
				schedule_delayed_work(&port->dp_work, msecs_to_jiffies(200));
			break;
		default:
			break;
		}
		break;
	case SPRD_CMDT_RSP_NAK:
		switch (cmd) {
		case SPRD_CMD_ENTER_MODE:
			/* Back to USB Operation */
			if (adev)
				WARN_ON(typec_altmode_notify(adev,
							     TYPEC_STATE_USB,
							     NULL));
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	/* Informing the alternate mode drivers about everything */
	if (adev)
		typec_altmode_vdm(adev, p[0], &p[1], cnt);

	return rlen;
}

static void sprd_tcpm_handle_vdm_request(struct sprd_tcpm_port *port,
					 const __le32 *payload, int cnt)
{
	int rlen = 0;
	u32 response[8] = { };
	u32 p0 = payload[0];

	if (port->vdm_state == VDM_STATE_BUSY) {
		/* If UFP responded busy retry after timeout */
		if (SPRD_PD_VDO_CMDT(p0) == SPRD_CMDT_RSP_BUSY) {
			port->vdm_state = VDM_STATE_WAIT_RSP_BUSY;
			port->vdo_retry = (p0 & ~SPRD_VDO_CMDT_MASK) | SPRD_CMDT_INIT;
			sprd_mod_vdm_delayed_work(port, SPRD_PD_T_VDM_BUSY);
			return;
		}
		port->vdm_state = VDM_STATE_DONE;
	}

	if (SPRD_PD_VDO_SVDM(p0)) {
		rlen = sprd_tcpm_pd_svdm(port, payload, cnt, response);
	} else {
		if (port->negotiated_rev >= SPRD_PD_REV30)
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
	}

	if (rlen > 0) {
		sprd_tcpm_queue_vdm(port, response[0], &response[1], rlen - 1);
		sprd_mod_vdm_delayed_work(port, 0);
	}
}

static void sprd_tcpm_send_vdm(struct sprd_tcpm_port *port,
			       u32 vid, int cmd, const u32 *data, int count)
{
	int svdm_version = typec_get_negotiated_svdm_version(port->typec_port);
	int svdm_version_minor;
	u32 header;
	u32 timeout = 100;

	if (svdm_version < 0) {
		sprd_tcpm_log_force(port, "%s, negotiated svdm_version(%d) is error!!!",
				    __func__, svdm_version);
		return;
	}

	svdm_version_minor = port->negotiated_svdm_ver_minor;

	if (WARN_ON(count > SPRD_VDO_MAX_SIZE - 1))
		count = SPRD_VDO_MAX_SIZE - 1;

	/* set VDM header with VID & CMD */
	header = SPRD_VDO(vid,
			  ((vid & SPRD_USB_SID_PD) == SPRD_USB_SID_PD) ?
			   1 : (SPRD_PD_VDO_CMD(cmd) <= SPRD_CMD_ATTENTION),
			  svdm_version,
			  svdm_version_minor,
			  cmd);
	sprd_tcpm_queue_vdm(port, header, data, count);

	if (port->vdm_discovery_id_retry == 1)
		timeout = 20;
	else
		timeout = 100;

	port->vdm_queue = true;
	sprd_mod_vdm_delayed_work(port, timeout);
}

static unsigned int sprd_vdm_ready_timeout(u32 vdm_hdr)
{
	unsigned int timeout;
	int cmd = SPRD_PD_VDO_CMD(vdm_hdr);

	/* its not a structured VDM command */
	if (!SPRD_PD_VDO_SVDM(vdm_hdr))
		return SPRD_PD_T_VDM_UNSTRUCTURED;

	switch (SPRD_PD_VDO_CMDT(vdm_hdr)) {
	case SPRD_CMDT_INIT:
		if (cmd == SPRD_CMD_ENTER_MODE || cmd == SPRD_CMD_EXIT_MODE)
			timeout = SPRD_PD_T_VDM_WAIT_MODE_E;
		else
			timeout = SPRD_PD_T_VDM_SNDR_RSP;
		break;
	default:
		if (cmd == SPRD_CMD_ENTER_MODE || cmd == SPRD_CMD_EXIT_MODE)
			timeout = SPRD_PD_T_VDM_E_MODE;
		else
			timeout = SPRD_PD_T_VDM_RCVR_RSP;
		break;
	}
	return timeout;
}

static void sprd_tcpm_cancel_vdm(struct sprd_tcpm_port *port)
{
	if (port->vdm_state != VDM_STATE_DONE || port->send_discover) {
		sprd_tcpm_log(port, "AMS, cancel vdm handle");
		complete(&port->tx_complete);
		port->vdm_state = VDM_STATE_BUSY;
		port->send_discover = false;
		port->vdm_retries = 3;
		kthread_cancel_work_sync(&port->vdm_state_machine);
		port->vdm_state = VDM_STATE_DONE;
	}
}

static void sprd_vdm_run_state_machine(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int i, res;

	switch (port->vdm_state) {
	case VDM_STATE_READY:
		/* Only transmit VDM if attached */
		if (!port->attached) {
			port->vdm_state = VDM_STATE_ERR_BUSY;
			break;
		}

		/*
		 * if there's traffic or we're not in PDO ready state don't send
		 * a VDM.
		 */
		if (port->state != SRC_READY && port->state != SNK_READY)
			break;

		/* Prepare and send VDM */
		memset(&msg, 0, sizeof(msg));
		msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_VENDOR_DEF,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id, port->vdo_count, 0);
		for (i = 0; i < port->vdo_count; i++)
			msg.payload[i] = port->vdo_data[i];
		res = sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
		if (res < 0) {
			port->vdm_state = VDM_STATE_ERR_SEND;
		} else if (port->vdm_discovery_id_retry == 1) {
			u32 temp = 0;

			sprd_tcpm_log(port, "ufp, retry send discovery ident");
			sprd_tcpm_unregister_altmodes(port);
			sprd_tcpm_send_vdm(port, SPRD_USB_SID_PD, SPRD_CMD_DISCOVER_IDENT,
					   &temp, 0);
			port->vdm_discovery_id_retry = 0;
		} else {
			unsigned long timeout;

			port->vdm_retries = 0;
			port->vdm_state = VDM_STATE_BUSY;
			port->vdm_sent = true;
			timeout = sprd_vdm_ready_timeout(port->vdo_data[0]);
			sprd_mod_vdm_delayed_work(port, timeout);
		}
		break;
	case VDM_STATE_WAIT_RSP_BUSY:
		port->vdo_data[0] = port->vdo_retry;
		port->vdo_count = 1;
		port->vdm_state = VDM_STATE_READY;
		break;
	case VDM_STATE_BUSY:
		port->vdm_state = VDM_STATE_ERR_TMOUT;
		break;
	case VDM_STATE_ERR_SEND:
		/*
		 * A partner which does not support USB PD will not reply,
		 * so this is not a fatal error. At the same time, some
		 * devices may not return GoodCRC under some circumstances,
		 * so we need to retry.
		 */
		if (port->vdm_retries < 3) {
			sprd_tcpm_log(port, "VDM Tx error, retry");
			port->vdm_retries++;
			port->vdm_state = VDM_STATE_READY;
		}
		break;
	default:
		break;
	}
}

static void sprd_vdm_state_machine_work(struct kthread_work *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
						   vdm_state_machine);
	enum sprd_vdm_states prev_state;

	mutex_lock(&port->lock);

	/*
	 * Continue running as long as the port is not busy and there was
	 * a state change.
	 */
	do {
		prev_state = port->vdm_state;
		sprd_vdm_run_state_machine(port);
	} while (port->vdm_state != prev_state &&
		 port->vdm_state != VDM_STATE_BUSY);

	mutex_unlock(&port->lock);
}

static void adjust_ctc_current_work(struct work_struct *work)
{
	int index = 1;
	int temp = 0, ret;
	int capacity = 0;
	struct power_supply *batt_psy = NULL;
	union power_supply_propval val;
	struct sprd_tcpm_port *port =
		container_of(work, struct sprd_tcpm_port, adjust_ctc_current.work);

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
		temp = val.intval;
		ret |= power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		capacity = val.intval;
		if (!ret) {
			if (temp <= 0 && capacity <= 20)
				index = 0;
			if (temp > 0 && capacity > 20 && capacity <= 40)
				index = 2;
			if (temp > 0 && capacity > 40)
				index = 3;

			if (last_index >= 0 && last_index != index) {
				sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, 0);
				pr_info("set state to SRC_SEND_CAPABILITIES\n");
			}
			last_index = index;
		}
	}
	pr_debug("%s, soc = %d, temp = %d\n", __func__, capacity, temp);
	schedule_delayed_work(&port->adjust_ctc_current, msecs_to_jiffies(5000));
}

enum sprd_pdo_err {
	PDO_NO_ERR,
	PDO_ERR_NO_VSAFE5V,
	PDO_ERR_VSAFE5V_NOT_FIRST,
	PDO_ERR_PDO_TYPE_NOT_IN_ORDER,
	PDO_ERR_FIXED_NOT_SORTED,
	PDO_ERR_VARIABLE_BATT_NOT_SORTED,
	PDO_ERR_DUPE_PDO,
	PDO_ERR_PPS_APDO_NOT_SORTED,
	PDO_ERR_DUPE_PPS_APDO,
};

static const char * const sprd_pdo_err_msg[] = {
	[PDO_ERR_NO_VSAFE5V] =
	" err: source/sink caps should atleast have vSafe5V",
	[PDO_ERR_VSAFE5V_NOT_FIRST] =
	" err: vSafe5V Fixed Supply Object Shall always be the first object",
	[PDO_ERR_PDO_TYPE_NOT_IN_ORDER] =
	" err: PDOs should be in the following order: Fixed; Battery; Variable",
	[PDO_ERR_FIXED_NOT_SORTED] =
	" err: Fixed supply pdos should be in increasing order of their fixed voltage",
	[PDO_ERR_VARIABLE_BATT_NOT_SORTED] =
	" err: Variable/Battery supply pdos should be in increasing order of their minimum voltage",
	[PDO_ERR_DUPE_PDO] =
	" err: Variable/Batt supply pdos cannot have same min/max voltage",
	[PDO_ERR_PPS_APDO_NOT_SORTED] =
	" err: Programmable power supply apdos should be in increasing order of their maximum voltage",
	[PDO_ERR_DUPE_PPS_APDO] =
	" err: Programmable power supply apdos cannot have same min/max voltage and max current",
};

static enum sprd_pdo_err sprd_tcpm_caps_err(struct sprd_tcpm_port *port, const u32 *pdo,
					    unsigned int nr_pdo)
{
	unsigned int i;

	/* Should at least contain vSafe5v */
	if (nr_pdo < 1)
		return PDO_ERR_NO_VSAFE5V;

	/* The vSafe5V Fixed Supply Object Shall always be the first object */
	if (sprd_pdo_type(pdo[0]) != SPRD_PDO_TYPE_FIXED ||
	    sprd_pdo_fixed_voltage(pdo[0]) != SPRD_VSAFE5V)
		return PDO_ERR_VSAFE5V_NOT_FIRST;

	for (i = 1; i < nr_pdo; i++) {
		if (sprd_pdo_type(pdo[i]) < sprd_pdo_type(pdo[i - 1])) {
			return PDO_ERR_PDO_TYPE_NOT_IN_ORDER;
		} else if (sprd_pdo_type(pdo[i]) == sprd_pdo_type(pdo[i - 1])) {
			enum sprd_pd_pdo_type type = sprd_pdo_type(pdo[i]);

			switch (type) {
			/*
			 * The remaining Fixed Supply Objects, if
			 * present, shall be sent in voltage order;
			 * lowest to highest.
			 */
			case SPRD_PDO_TYPE_FIXED:
				if (sprd_pdo_fixed_voltage(pdo[i]) <
				    sprd_pdo_fixed_voltage(pdo[i - 1]))
					return PDO_ERR_FIXED_NOT_SORTED;
				break;
			/*
			 * The Battery Supply Objects and Variable
			 * supply, if present shall be sent in Minimum
			 * Voltage order; lowest to highest.
			 */
			case SPRD_PDO_TYPE_VAR:
			case SPRD_PDO_TYPE_BATT:
				if (sprd_pdo_min_voltage(pdo[i]) <
				    sprd_pdo_min_voltage(pdo[i - 1]))
					return PDO_ERR_VARIABLE_BATT_NOT_SORTED;
				else if ((sprd_pdo_min_voltage(pdo[i]) ==
					  sprd_pdo_min_voltage(pdo[i - 1])) &&
					 (sprd_pdo_max_voltage(pdo[i]) ==
					  sprd_pdo_max_voltage(pdo[i - 1])))
					return PDO_ERR_DUPE_PDO;
				break;
			/*
			 * The Programmable Power Supply APDOs, if present,
			 * shall be sent in Maximum Voltage order;
			 * lowest to highest.
			 */
			case SPRD_PDO_TYPE_APDO:
				if (sprd_pdo_apdo_type(pdo[i]) != SPRD_APDO_TYPE_PPS)
					break;

				if (sprd_pdo_pps_apdo_max_voltage(pdo[i]) <
				    sprd_pdo_pps_apdo_max_voltage(pdo[i - 1]))
					return PDO_ERR_PPS_APDO_NOT_SORTED;
				else if (sprd_pdo_pps_apdo_min_voltage(pdo[i]) ==
					  sprd_pdo_pps_apdo_min_voltage(pdo[i - 1]) &&
					 sprd_pdo_pps_apdo_max_voltage(pdo[i]) ==
					  sprd_pdo_pps_apdo_max_voltage(pdo[i - 1]) &&
					 sprd_pdo_pps_apdo_max_current(pdo[i]) ==
					  sprd_pdo_pps_apdo_max_current(pdo[i - 1]))
					return PDO_ERR_DUPE_PPS_APDO;
				break;
			default:
				sprd_tcpm_log_force(port, " Unknown pdo type");
			}
		}
	}

	return PDO_NO_ERR;
}

static int sprd_tcpm_validate_caps(struct sprd_tcpm_port *port, const u32 *pdo,
				   unsigned int nr_pdo)
{
	enum sprd_pdo_err err_index = sprd_tcpm_caps_err(port, pdo, nr_pdo);

	if (err_index != PDO_NO_ERR) {
		sprd_tcpm_log_force(port, " %s", sprd_pdo_err_msg[err_index]);
		return -EINVAL;
	}

	return 0;
}

/*
 * Note: k515 adds *vdo parameter, which is not currently processed, and may
 *       need to be debugged later.
*/
static int sprd_tcpm_altmode_enter(struct typec_altmode *altmode, u32 *vdo)
{
	struct sprd_tcpm_port *port = typec_altmode_get_drvdata(altmode);
	int svdm_version;
	int svdm_version_minor;
	u32 header;

	svdm_version = typec_get_negotiated_svdm_version(port->typec_port);
	if (svdm_version < 0) {
		sprd_tcpm_log_force(port, "%s, negotiated svdm_version(%d) is error!!!",
				    __func__, svdm_version);
		return svdm_version;
	}

	svdm_version_minor = port->negotiated_svdm_ver_minor;

	mutex_lock(&port->lock);
	header = SPRD_VDO(altmode->svid, 1, svdm_version, svdm_version_minor, SPRD_CMD_ENTER_MODE);
	header |= SPRD_VDO_OPOS(altmode->mode);

	sprd_tcpm_queue_vdm(port, header, NULL, 0);
	sprd_mod_vdm_delayed_work(port, 0);
	mutex_unlock(&port->lock);

	return 0;
}

static int sprd_tcpm_altmode_exit(struct typec_altmode *altmode)
{
	struct sprd_tcpm_port *port = typec_altmode_get_drvdata(altmode);
	int svdm_version;
	int svdm_version_minor;
	u32 header;

	svdm_version = typec_get_negotiated_svdm_version(port->typec_port);
	if (svdm_version < 0) {
		sprd_tcpm_log_force(port, "%s, negotiated svdm_version(%d) is error!!!",
				    __func__, svdm_version);
		return svdm_version;
	}

	svdm_version_minor = port->negotiated_svdm_ver_minor;

	mutex_lock(&port->lock);
	header = SPRD_VDO(altmode->svid, 1, svdm_version, svdm_version_minor, SPRD_CMD_EXIT_MODE);
	header |= SPRD_VDO_OPOS(altmode->mode);

	sprd_tcpm_queue_vdm(port, header, NULL, 0);
	sprd_mod_vdm_delayed_work(port, 0);
	mutex_unlock(&port->lock);

	return 0;
}

static int sprd_tcpm_altmode_vdm(struct typec_altmode *altmode,
			    u32 header, const u32 *data, int count)
{
	struct sprd_tcpm_port *port = typec_altmode_get_drvdata(altmode);

	mutex_lock(&port->lock);
	sprd_tcpm_queue_vdm(port, header, data, count - 1);
	sprd_mod_vdm_delayed_work(port, 0);
	mutex_unlock(&port->lock);

	return 0;
}

static const struct typec_altmode_ops sprd_tcpm_altmode_ops = {
	.enter = sprd_tcpm_altmode_enter,
	.exit = sprd_tcpm_altmode_exit,
	.vdm = sprd_tcpm_altmode_vdm,
};

/*
 * PD (data, control) command handling functions
 */
static inline enum sprd_tcpm_state sprd_ready_state(struct sprd_tcpm_port *port)
{
	if (port->pwr_role == TYPEC_SOURCE)
		return SRC_READY;
	else
		return SNK_READY;
}

static int sprd_tcpm_pd_send_control(struct sprd_tcpm_port *port,
				     enum sprd_pd_ctrl_msg_type type);

static void sprd_tcpm_handle_alert(struct sprd_tcpm_port *port,
				   const __le32 *payload, int cnt)
{
	u32 p0 = payload[0];
	unsigned int type = sprd_usb_pd_ado_type(p0);

	if (!type) {
		sprd_tcpm_log(port, "Alert message received with no type");
		return;
	}

	/* Just handling non-battery alerts for now */
	if (!(type & SPRD_USB_PD_ADO_TYPE_BATT_STATUS_CHANGE)) {
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_set_state(port, GET_STATUS_SEND, 0);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
	}
}

static void sprd_tcpm_pd_data_request(struct sprd_tcpm_port *port,
				      const struct sprd_pd_message *msg)
{
	enum sprd_pd_data_msg_type type = sprd_pd_header_type(msg->header);
	unsigned int cnt = sprd_pd_header_cnt(msg->header);
	unsigned int rev = sprd_pd_header_rev(msg->header);
	unsigned int i;
	int ret = 0;

	if (port->received_bad_good_crc) {
		sprd_tcpm_log_force(port, "sprd: %s, received_bad_good_crc: true --> false",
				    __func__);
		port->tcpc->check_tx_goodcrc(port->tcpc, true);
		port->tcpc->enable_tx_auto_retry(port->tcpc, true);
		port->received_bad_good_crc = false;
		ret = port->tcpc->set_pd_tx_id(port->tcpc, port->message_id);
		if (ret)
			sprd_tcpm_log(port, "sprd: %s, failed to set tx_id: 0x%x",
				      __func__, port->message_id);
	}

	if (port->received_get_snk_cap_cnt) {
		sprd_tcpm_log_force(port, "sprd: %s, received_get_snk_cap_cnt: %d --> 0",
				    __func__, port->received_get_snk_cap_cnt);
		port->received_get_snk_cap_cnt = 0;
	}

	switch (type) {
	case SPRD_PD_DATA_SOURCE_CAP:
		if (port->pwr_role != TYPEC_SINK)
			break;

		for (i = 0; i < cnt; i++)
			port->source_caps[i] = msg->payload[i];

		port->nr_source_caps = cnt;

		sprd_tcpm_log_source_caps(port);

		sprd_tcpm_validate_caps(port, port->source_caps, port->nr_source_caps);

		/*
		 * Adjust revision in subsequent message headers, as required,
		 * to comply with 6.2.1.1.5 of the USB PD 3.0 spec. We don't
		 * support Rev 1.0 so just do nothing in that scenario.
		 */
		if (rev == SPRD_PD_REV10)
			break;

		if (rev < SPRD_PD_MAX_REV)
			port->negotiated_rev = rev;

		/*
		 * This message may be received even if VBUS is not
		 * present. This is quite unexpected; see USB PD
		 * specification, sections 8.3.3.6.3.1 and 8.3.3.6.3.2.
		 * However, at the same time, we must be ready to
		 * receive this message and respond to it 15ms after
		 * receiving PS_RDY during power swap operations, no matter
		 * if VBUS is available or not (USB PD specification,
		 * section 6.5.9.2).
		 * So we need to accept the message either way,
		 * but be prepared to keep waiting for VBUS after it was
		 * handled.
		 */
		sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
		break;
	case SPRD_PD_DATA_REQUEST:
		if (port->pwr_role != TYPEC_SOURCE ||
		    cnt != 1) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}

		/*
		 * Adjust revision in subsequent message headers, as required,
		 * to comply with 6.2.1.1.5 of the USB PD 3.0 spec. We don't
		 * support Rev 1.0 so just reject in that scenario.
		 */
		if (rev == SPRD_PD_REV10) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}

		if (rev < SPRD_PD_MAX_REV)
			port->negotiated_rev = rev;

		port->sink_request = msg->payload[0];
		sprd_tcpm_set_state(port, SRC_NEGOTIATE_CAPABILITIES, 0);
		break;
	case SPRD_PD_DATA_SINK_CAP:
		/* We don't do anything with this at the moment... */
		for (i = 0; i < cnt; i++)
			port->sink_caps[i] = msg->payload[i];
		port->nr_sink_caps = cnt;
		break;
	case SPRD_PD_DATA_VENDOR_DEF:
		if (port->data_role_swap) {
			sprd_tcpm_log(port, "data role swapping, cancel vdm");
			break;
		}
		sprd_tcpm_handle_vdm_request(port, msg->payload, cnt);
		break;
	case SPRD_PD_DATA_BIST:
		if (port->state == SRC_READY || port->state == SNK_READY) {
			port->bist_request = msg->payload[0];
			sprd_tcpm_set_state(port, BIST_RX, 0);
		}
		break;
	case SPRD_PD_DATA_ALERT:
		sprd_tcpm_handle_alert(port, msg->payload, cnt);
		break;
	case SPRD_PD_DATA_BATT_STATUS:
	case SPRD_PD_DATA_GET_COUNTRY_INFO:
		/* Currently unsupported */
		sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		break;
	default:
		if (port->negotiated_rev < SPRD_PD_REV30)
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
		else
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		sprd_tcpm_log(port, "Unhandled data message type %#x", type);
		break;
	}
}

static void sprd_tcpm_fixed_pd_complete(struct sprd_tcpm_port *port)
{
	if (port->fixed_pd_pending) {
		port->fixed_pd_pending = false;
		complete(&port->fixed_pd_complete);
	}
}

static void sprd_tcpm_pps_complete(struct sprd_tcpm_port *port, int result)
{
	if (port->pps_pending) {
		port->pps_status = result;
		port->pps_pending = false;
		complete(&port->pps_complete);
	}
}

static void sprd_tcpm_check_vdm_send_conflict(struct sprd_tcpm_port *port)
{
	if (port->data_role == TYPEC_HOST && port->vdm_queue && !port->vdm_sent) {
		sprd_tcpm_log(port, "%s: delay send vdm", __func__);
		sprd_mod_vdm_delayed_work(port, 100);
	}
}

static void sprd_tcpm_pd_ctrl_request(struct sprd_tcpm_port *port,
				      const struct sprd_pd_message *msg)
{
	enum sprd_pd_ctrl_msg_type type = sprd_pd_header_type(msg->header);
	enum sprd_tcpm_state next_state;
	int ret = 0;

	if (port->received_bad_good_crc &&
	    (type != SPRD_PD_CTRL_GOOD_CRC && type != SPRD_PD_CTRL_GET_SOURCE_CAP)) {
		sprd_tcpm_log_force(port, "sprd: %s[%d], received_bad_good_crc: true --> false",
				    __func__, __LINE__);
		port->tcpc->check_tx_goodcrc(port->tcpc, true);
		port->tcpc->enable_tx_auto_retry(port->tcpc, true);
		port->received_bad_good_crc = false;
		sprd_tcpm_log_force(port, "sprd: %s[%d], received_get_snk_cap_cnt: %d --> 0",
				    __func__, __LINE__, port->received_get_snk_cap_cnt);
		port->received_get_snk_cap_cnt = 0;
		ret = port->tcpc->set_pd_tx_id(port->tcpc, port->message_id);
		if (ret)
			sprd_tcpm_log(port, "sprd: %s, failed to set tx_id: 0x%x",
				      __func__, port->message_id);
	}

	if ((type != SPRD_PD_CTRL_GOOD_CRC && type != SPRD_PD_CTRL_GET_SINK_CAP) &&
	    port->received_get_snk_cap_cnt) {
		sprd_tcpm_log_force(port, "sprd: %s[%d], received_get_snk_cap_cnt: %d --> 0",
				    __func__, __LINE__, port->received_get_snk_cap_cnt);
		port->received_get_snk_cap_cnt = 0;
	}

	sprd_tcpm_check_vdm_send_conflict(port);
	switch (type) {
	case SPRD_PD_CTRL_GOOD_CRC:
	case SPRD_PD_CTRL_PING:
		break;
	case SPRD_PD_CTRL_GET_SOURCE_CAP:
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			if (port->received_bad_good_crc) {
				sprd_tcpm_log(port, "sprd: %s, handler bad good crc: soft reset",
					      __func__);
				port->tcpc->check_tx_goodcrc(port->tcpc, true);
				port->tcpc->enable_tx_auto_retry(port->tcpc, true);
				port->received_bad_good_crc = false;
				sprd_tcpm_set_state(port, SOFT_RESET_SEND, 0);
				break;
			}

			sprd_tcpm_queue_message(port, PD_MSG_DATA_SOURCE_CAP);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		break;
	case SPRD_PD_CTRL_GET_SINK_CAP:
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			if (port->support_pd_cts)
				port->received_get_snk_cap_cnt++;

			if (port->received_get_snk_cap_cnt == 3) {
				sprd_tcpm_log(port, "sprd: %s, waiting for bad good crc",
					      __func__);
				port->tcpc->enable_tx_auto_retry(port->tcpc, false);
				port->tcpc->check_tx_goodcrc(port->tcpc, false);
				port->received_bad_good_crc = true;
			}

			sprd_tcpm_queue_message(port, PD_MSG_DATA_SINK_CAP);
			break;
		case SNK_TRANSITION_SINK:
			sprd_tcpm_set_state(port, sprd_hard_reset_state(port), 0);
			break;
		case SNK_NEGOTIATE_CAPABILITIES:
		case SRC_SEND_CAPABILITIES:
		case SRC_SEND_CAPABILITIES_TIMEOUT:
			sprd_tcpm_set_state(port, SOFT_RESET_SEND,
					    SPRD_PD_T_PRO_ERR_SOFTRESET);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		break;
	case SPRD_PD_CTRL_GOTO_MIN:
		break;
	case SPRD_PD_CTRL_PS_RDY:
		switch (port->state) {
		case SNK_TRANSITION_SINK:
			if (port->vbus_present) {
				if (!port->xts_limit_cur && port->fixed_pd_voltage > 0 &&
				    port->req_current_limit >= port->negotiated_limit_cur) {
					sprd_tcpm_set_cm_ic_limit_current(SPRD_PDO_TYPE_FIXED,
								port->req_supply_voltage * 1000,
								port->req_current_limit * 1000,
								false);
					port->negotiated_limit_cur = port->req_current_limit;
					port->negotiated_limit_ic_current = false;
				}
				sprd_tcpm_set_current_limit(port,
							    port->req_current_limit,
							    port->req_supply_voltage);
				port->explicit_contract = true;
				sprd_tcpm_set_state(port, SNK_READY, 0);
			} else {
				/*
				 * Seen after power swap. Keep waiting for VBUS
				 * in a transitional state.
				 */
				sprd_tcpm_set_state(port, SNK_TRANSITION_SINK_VBUS, 0);
			}
			break;
		case PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED:
			sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SINK_ON, 0);
			break;
		case PR_SWAP_SNK_SRC_SINK_OFF:
			sprd_tcpm_set_state(port, PR_SWAP_SNK_SRC_SOURCE_ON, 0);
			break;
		case VCONN_SWAP_WAIT_FOR_VCONN:
			sprd_tcpm_set_state(port, VCONN_SWAP_TURN_OFF_VCONN, 0);
			break;
		default:
			break;
		}
		break;
	case SPRD_PD_CTRL_REJECT:
	case SPRD_PD_CTRL_WAIT:
	case SPRD_PD_CTRL_NOT_SUPP:
		switch (port->state) {
		case SNK_NEGOTIATE_CAPABILITIES:
			/* USB PD specification, Figure 8-43 */
			if (port->explicit_contract)
				next_state = SNK_READY;
			else
				next_state = SNK_WAIT_CAPABILITIES;
			sprd_tcpm_set_state(port, next_state, 0);
			break;
		case SNK_NEGOTIATE_PPS_CAPABILITIES:
			/* Revert data back from any requested PPS updates */
			port->pps_data.req_out_volt = port->supply_voltage;
			port->pps_data.req_op_curr = port->current_limit;
			port->pps_status = (type == SPRD_PD_CTRL_WAIT ?
					    -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, SNK_READY, 0);
			break;
		case DR_SWAP_SEND:
			port->swap_status = (type == SPRD_PD_CTRL_WAIT ?
					     -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, DR_SWAP_CANCEL, 0);
			break;
		case PR_SWAP_SEND:
			port->swap_status = (type == SPRD_PD_CTRL_WAIT ?
					     -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, PR_SWAP_CANCEL, 0);
			break;
		case VCONN_SWAP_SEND:
			port->swap_status = (type == SPRD_PD_CTRL_WAIT ?
					     -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, VCONN_SWAP_CANCEL, 0);
			break;
		default:
			break;
		}
		break;
	case SPRD_PD_CTRL_ACCEPT:
		switch (port->state) {
		case SNK_NEGOTIATE_CAPABILITIES:
			port->pps_data.active = false;
			sprd_tcpm_set_state(port, SNK_TRANSITION_SINK, 0);
			break;
		case SNK_NEGOTIATE_PPS_CAPABILITIES:
			port->pps_data.active = true;
			port->pps_data.min_volt = port->pps_data.req_min_volt;
			port->pps_data.max_volt = port->pps_data.req_max_volt;
			port->pps_data.max_curr = port->pps_data.req_max_curr;
			port->req_supply_voltage = port->pps_data.req_out_volt;
			port->req_current_limit = port->pps_data.req_op_curr;
			sprd_tcpm_set_state(port, SNK_TRANSITION_SINK, 0);
			break;
		case SOFT_RESET_SEND:
			port->message_id = 0;
			port->rx_msgid = -1;
			if (port->pwr_role == TYPEC_SOURCE)
				next_state = SRC_SEND_CAPABILITIES;
			else
				next_state = SNK_WAIT_CAPABILITIES;
			sprd_tcpm_set_state(port, next_state, 0);
			break;
		case DR_SWAP_SEND:
			sprd_tcpm_cancel_vdm(port);
			sprd_tcpm_set_state(port, DR_SWAP_CHANGE_DR, 0);
			break;
		case PR_SWAP_SEND:
			sprd_tcpm_set_state(port, PR_SWAP_START, 0);
			break;
		case VCONN_SWAP_SEND:
			sprd_tcpm_set_state(port, VCONN_SWAP_START, 0);
			break;
		case SNK_READY:
		case SRC_READY:
			sprd_tcpm_set_state(port, SOFT_RESET_SEND,
					    SPRD_PD_T_PRO_ERR_SOFTRESET);
		default:
			break;
		}
		break;
	case SPRD_PD_CTRL_SOFT_RESET:
		sprd_tcpm_set_state(port, SOFT_RESET, 0);
		break;
	case SPRD_PD_CTRL_DR_SWAP:
		if (port->port_type != TYPEC_PORT_DRP) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		/*
		 * XXX
		 * 6.3.9: If an alternate mode is active, a request to swap
		 * alternate modes shall trigger a port reset.
		 */
		sprd_tcpm_cancel_vdm(port);
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_set_state(port, DR_SWAP_ACCEPT, 0);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	case SPRD_PD_CTRL_PR_SWAP:
		if (port->port_type != TYPEC_PORT_DRP) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_set_state(port, PR_SWAP_ACCEPT, 0);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	case SPRD_PD_CTRL_VCONN_SWAP:
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			/* Currently not supported */
			if (port->negotiated_rev < SPRD_PD_REV30)
				sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			else
				sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	case SPRD_PD_CTRL_GET_SOURCE_CAP_EXT:
	case SPRD_PD_CTRL_GET_STATUS:
	case SPRD_PD_CTRL_FR_SWAP:
	case SPRD_PD_CTRL_GET_PPS_STATUS:
	case SPRD_PD_CTRL_GET_COUNTRY_CODES:
	case SPRD_PD_CTRL_DATA_RESET:
	case SPRD_PD_CTRL_GET_SOURCE_INFO:
		/* Currently not supported */
		sprd_tcpm_queue_message(port,
					port->negotiated_rev < SPRD_PD_REV30 ?
					PD_MSG_CTRL_REJECT : PD_MSG_CTRL_NOT_SUPP);
		break;
	case SPRD_PD_CTRL_GET_REVISION:
		sprd_tcpm_log(port, "SPRD_PD_CTRL_GET_REVISION");
		/* Currently not supported */
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_log(port, "CTRL_GET_REVISION message type %#x", type);
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_GET_REVISION);
			break;
		default:
			sprd_tcpm_log(port, "CTRL_WAIT message type %#x", type);
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	case SPRD_PD_CTRL_GET_SINK_CAP_EXT:
		switch (port->state) {
		case SNK_READY:
			sprd_tcpm_queue_chunk_message(port, PD_CHUNK_MSG_CTRL_GET_SINK_CAP_EXT);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	default:
		if (port->negotiated_rev < SPRD_PD_REV30)
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
		else
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		sprd_tcpm_log(port, "Unhandled ctrl message type %#x", type);
		break;
	}
}

static void sprd_tcpm_pd_ext_msg_request(struct sprd_tcpm_port *port,
					 const struct sprd_pd_message *msg)
{
	enum sprd_pd_ext_msg_type type = sprd_pd_header_type(msg->header);
	unsigned int data_size = sprd_pd_ext_header_data_size(msg->ext_msg.header);
	unsigned int request_chunk = sprd_pd_ext_header_request_chunk(msg->ext_msg.header);
	int ret = 0;

	if (port->received_bad_good_crc) {
		sprd_tcpm_log_force(port, "sprd: %s, received_bad_good_crc: true --> false",
				    __func__);
		port->tcpc->check_tx_goodcrc(port->tcpc, true);
		port->tcpc->enable_tx_auto_retry(port->tcpc, true);
		port->received_bad_good_crc = false;
		ret = port->tcpc->set_pd_tx_id(port->tcpc, port->message_id);
		if (ret)
			sprd_tcpm_log(port, "sprd: %s, failed to set tx_id: 0x%x",
				      __func__, port->message_id);
	}

	if (port->received_get_snk_cap_cnt) {
		sprd_tcpm_log_force(port, "sprd: %s, received_get_snk_cap_cnt: %d --> 0",
				    __func__, port->received_get_snk_cap_cnt);
		port->received_get_snk_cap_cnt = 0;
	}

	if (!((msg->ext_msg.header) & SPRD_PD_EXT_HDR_CHUNKED)) {
		sprd_tcpm_log(port, "Unchunked extended messages unsupported");
		return;
	}

	if (data_size > SPRD_PD_EXT_MAX_CHUNK_DATA) {
		sprd_tcpm_set_state(port, CHUNK_NOT_SUPP, SPRD_PD_T_CHUNK_NOT_SUPP);
		sprd_tcpm_log(port, "Chunk handling not yet supported, data_size = %d", data_size);
		return;
	}

	if (request_chunk) {
		sprd_tcpm_log(port, "request chunk");

		if (port->tx_chunk_msg) {
			port->tx_chunk_msg = false;
			if (!completion_done(&port->tx_chunk_request))
				complete(&port->tx_chunk_request);
			return;
		}
	}

	switch (type) {
	case SPRD_PD_EXT_STATUS:
		/*
		 * If PPS related events raised then get PPS status to clear
		 * (see USB PD 3.0 Spec, 6.5.2.4)
		 */
		if (msg->ext_msg.data[SPRD_USB_PD_EXT_SDB_EVENT_FLAGS] &
		    SPRD_USB_PD_EXT_SDB_PPS_EVENTS)
			sprd_tcpm_set_state(port, GET_PPS_STATUS_SEND, 0);
		else
			sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case SPRD_PD_EXT_PPS_STATUS:
		/*
		 * For now the PPS status message is used to clear events
		 * and nothing more.
		 */
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case SPRD_PD_EXT_SOURCE_CAP_EXT:
	case SPRD_PD_EXT_GET_BATT_CAP:
		port->data[0] = msg->ext_msg.data[0];
		sprd_tcpm_queue_chunk_message(port, PD_CHUNK_MSG_EXT_GET_BATTERY_CAP_EXT);
		break;
	case SPRD_PD_EXT_GET_BATT_STATUS:
		sprd_tcpm_pd_send_battery_status(port, msg->ext_msg.data[0]);
		break;
	case SPRD_PD_EXT_BATT_CAP:
	case SPRD_PD_EXT_GET_MANUFACTURER_INFO:
	case SPRD_PD_EXT_MANUFACTURER_INFO:
	case SPRD_PD_EXT_SECURITY_REQUEST:
	case SPRD_PD_EXT_SECURITY_RESPONSE:
	case SPRD_PD_EXT_FW_UPDATE_REQUEST:
	case SPRD_PD_EXT_FW_UPDATE_RESPONSE:
	case SPRD_PD_EXT_COUNTRY_INFO:
	case SPRD_PD_EXT_COUNTRY_CODES:
		sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		break;
	default:
		sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		sprd_tcpm_log(port, "Unhandled extended message type %#x", type);
		break;
	}
}

static void sprd_tcpm_pd_rx_handler(struct kthread_work *work)
{
	struct sprd_pd_rx_event *event = container_of(work,
						      struct sprd_pd_rx_event, work);
	const struct sprd_pd_message *msg = &event->msg;
	unsigned int cnt = sprd_pd_header_cnt(msg->header);
	struct sprd_tcpm_port *port = event->port;
	int i;

	mutex_lock(&port->lock);

	sprd_tcpm_log(port, "PD RX, header: %#x [%d][%s]", msg->header,
		      port->attached, port->data_role ? "host" : "device");

	for (i = 0; i < cnt; i++) {
		if (msg->payload[i])
			sprd_tcpm_log(port, "PD RX, data[%d]=0x%x", i, msg->payload[i]);
	}

	if (port->bist_test_data) {
		sprd_tcpm_log(port, "%s: bist test data", __func__);
		goto done;
	}

	if (port->attached) {
		enum sprd_pd_ctrl_msg_type type = sprd_pd_header_type(msg->header);
		unsigned int msgid = sprd_pd_header_msgid(msg->header);

		/*
		 * USB PD standard, 6.6.1.2:
		 * "... if MessageID value in a received Message is the
		 * same as the stored value, the receiver shall return a
		 * GoodCRC Message with that MessageID value and drop
		 * the Message (this is a retry of an already received
		 * Message). Note: this shall not apply to the Soft_Reset
		 * Message which always has a MessageID value of zero."
		 */
		if (msgid == port->rx_msgid && type != SPRD_PD_CTRL_SOFT_RESET) {
			sprd_tcpm_log(port, "%s:line%d msgid same", __func__, __LINE__);
			goto done;
		}
		port->rx_msgid = msgid;

		/*
		 * If both ends believe to be DFP/host, we have a data role
		 * mismatch.
		 */
		if (!!((msg->header) & SPRD_PD_HEADER_DATA_ROLE) ==
		    (port->data_role == TYPEC_HOST)) {
			if (port->data_role_swap) {
				sprd_tcpm_log(port, "Data role mismatch, data role swap, ignore");
				goto done;
			}
			sprd_tcpm_log(port, "Data role mismatch, initiating error recovery");
			sprd_tcpm_set_state(port, sprd_hard_reset_state(port), 0);
		} else {
			if (msg->header & SPRD_PD_HEADER_EXT_HDR)
				sprd_tcpm_pd_ext_msg_request(port, msg);
			else if (cnt)
				sprd_tcpm_pd_data_request(port, msg);
			else
				sprd_tcpm_pd_ctrl_request(port, msg);
		}
	}

done:
	mutex_unlock(&port->lock);
	kfree(event);
}

void sprd_tcpm_pd_receive(struct sprd_tcpm_port *port, const struct sprd_pd_message *msg)
{
	struct sprd_pd_rx_event *event;

	event = kzalloc(sizeof(*event), GFP_ATOMIC);
	if (!event)
		return;

	kthread_init_work(&event->work, sprd_tcpm_pd_rx_handler);
	event->port = port;
	memcpy(&event->msg, msg, sizeof(*msg));
	kthread_queue_work(&port->tcpm_kworker, &event->work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_pd_receive);

static int sprd_tcpm_pd_send_control(struct sprd_tcpm_port *port,
				     enum sprd_pd_ctrl_msg_type type)
{
	struct sprd_pd_message msg;

	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER(type, port->pwr_role,
				       port->data_role,
				       port->negotiated_rev,
				       port->message_id, 0, 0);

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

/*
 * Send queued message without affecting state.
 * Return true if state machine should go back to sleep,
 * false otherwise.
 */
static bool sprd_tcpm_send_queued_message(struct sprd_tcpm_port *port)
{
	enum sprd_pd_msg_request queued_message;
	int ret = 0;

	do {
		queued_message = port->queued_message;
		port->queued_message = PD_MSG_NONE;

		switch (queued_message) {
		case PD_MSG_CTRL_WAIT:
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_WAIT);
			break;
		case PD_MSG_CTRL_REJECT:
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_REJECT);
			break;
		case PD_MSG_CTRL_NOT_SUPP:
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_NOT_SUPP);
			break;
		case PD_MSG_DATA_SINK_CAP:
			ret = sprd_tcpm_pd_send_sink_caps(port);
			if (ret < 0) {
				sprd_tcpm_log(port, "Unable to send snk caps, ret=%d", ret);
				sprd_tcpm_set_state(port, SOFT_RESET_SEND, 0);
			}
			break;
		case PD_MSG_DATA_SOURCE_CAP:
			ret = sprd_tcpm_pd_send_source_caps(port);
			if (ret < 0) {
				sprd_tcpm_log(port, "%s, Unable to send src caps, ret = %d",
					      __func__, ret);
				sprd_tcpm_set_state(port, SOFT_RESET_SEND, 0);
			} else if (port->pwr_role == TYPEC_SOURCE) {
				sprd_tcpm_set_state_cond(port,
							 sprd_hard_reset_state(port),
							 port->negotiated_rev < SPRD_PD_REV30 ?
							 SPRD_PD_T_SENDER_RESPONSE_PD2 :
							 SPRD_PD_T_SENDER_RESPONSE_PD3);
			}
			break;
		case PD_MSG_CTRL_GET_REVISION:
			sprd_tcpm_pd_send_revision(port);
			break;
		default:
			break;
		}
	} while (port->queued_message != PD_MSG_NONE);

	if (port->delayed_state != INVALID_STATE) {
		if (ktime_after(port->delayed_runtime, ktime_get())) {
			sprd_mod_tcpm_delayed_work(port,
						   ktime_to_ms(ktime_sub(port->delayed_runtime,
									 ktime_get())));
			return true;
		}
		port->delayed_state = INVALID_STATE;
	}
	return false;
}

static int sprd_tcpm_pd_check_request(struct sprd_tcpm_port *port)
{
	u32 pdo, rdo = port->sink_request;
	unsigned int max, op, pdo_max, index;
	enum sprd_pd_pdo_type type;

	index = sprd_rdo_index(rdo);
	if (!index || index > port->nr_src_pdo)
		return -EINVAL;
	pdo = port->src_pdo[index - 1];

	type = sprd_pdo_type(pdo);
	switch (type) {
	case SPRD_PDO_TYPE_FIXED:
	case SPRD_PDO_TYPE_VAR:
		max = sprd_rdo_max_current(rdo);
		op = sprd_rdo_op_current(rdo);
		pdo_max = sprd_pdo_max_current(pdo);

		if (op > pdo_max)
			return -EINVAL;
		if (max > pdo_max && !(rdo & SPRD_RDO_CAP_MISMATCH))
			return -EINVAL;

		if (type == SPRD_PDO_TYPE_FIXED) {
			port->partner_support_usb_suspend = !(rdo & SPRD_RDO_NO_SUSPEND);
			sprd_tcpm_log(port,
				      "Requested %u mV, %u mA for %u / %u mA",
				      sprd_pdo_fixed_voltage(pdo), pdo_max, op, max);
		} else {
			sprd_tcpm_log(port,
				      "Requested %u -> %u mV, %u mA for %u / %u mA",
				      sprd_pdo_min_voltage(pdo), sprd_pdo_max_voltage(pdo),
				      pdo_max, op, max);
		}
		break;
	case SPRD_PDO_TYPE_BATT:
		max = sprd_rdo_max_power(rdo);
		op = sprd_rdo_op_power(rdo);
		pdo_max = sprd_pdo_max_power(pdo);

		if (op > pdo_max)
			return -EINVAL;
		if (max > pdo_max && !(rdo & SPRD_RDO_CAP_MISMATCH))
			return -EINVAL;
		sprd_tcpm_log(port,
			      "Requested %u -> %u mV, %u mW for %u / %u mW",
			      sprd_pdo_min_voltage(pdo), sprd_pdo_max_voltage(pdo),
			      pdo_max, op, max);
		break;
	default:
		return -EINVAL;
	}

	port->op_vsafe5v = index == 1;

	return 0;
}

#define min_power(x, y) min(sprd_pdo_max_power(x), sprd_pdo_max_power(y))
#define min_current(x, y) min(sprd_pdo_max_current(x), sprd_pdo_max_current(y))

static int sprd_tcpm_pd_select_pdo(struct sprd_tcpm_port *port, int *sink_pdo, int *src_pdo)
{
	unsigned int i, j, max_src_mv = 0, min_src_mv = 0, max_mw = 0,
		     max_mv = 0, src_mw = 0, src_ma = 0, max_snk_mv = 0,
		     min_snk_mv = 0;
	int ret = -EINVAL;
	bool dual_role_power = false, usb_comm = false;

	port->pps_data.supported = false;
	port->usb_type = POWER_SUPPLY_USB_TYPE_PD;

	/*
	 * Select the source PDO providing the most power which has a
	 * matchig sink cap.
	 */
	for (i = 0; i < port->nr_source_caps; i++) {
		u32 pdo = port->source_caps[i];
		enum sprd_pd_pdo_type type = sprd_pdo_type(pdo);
		if (i == 0) {
			dual_role_power = !!(pdo & SPRD_PDO_FIXED_DUAL_ROLE);
			usb_comm = !!(pdo & SPRD_PDO_FIXED_USB_COMM);
			port->partner_support_usb_suspend = !!(pdo & SPRD_PDO_FIXED_SUSPEND);
		}

		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
			max_src_mv = sprd_pdo_fixed_voltage(pdo);
			min_src_mv = max_src_mv;
			break;
		case SPRD_PDO_TYPE_BATT:
		case SPRD_PDO_TYPE_VAR:
			max_src_mv = sprd_pdo_max_voltage(pdo);
			min_src_mv = sprd_pdo_min_voltage(pdo);
			break;
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) == SPRD_APDO_TYPE_PPS &&
			    !usb_comm && !dual_role_power) {
				port->pps_data.supported = true;
				port->usb_type =
					POWER_SUPPLY_USB_TYPE_PD_PPS;
			}
			continue;
		default:
			sprd_tcpm_log(port, "Invalid source PDO type, ignoring");
			continue;
		}

		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
		case SPRD_PDO_TYPE_VAR:
			src_ma = sprd_pdo_max_current(pdo);
			src_mw = src_ma * min_src_mv / 1000;
			break;
		case SPRD_PDO_TYPE_BATT:
			src_mw = sprd_pdo_max_power(pdo);
			break;
		case SPRD_PDO_TYPE_APDO:
			continue;
		default:
			sprd_tcpm_log(port, "Invalid source PDO type, ignoring");
			continue;
		}

		for (j = 0; j < port->nr_snk_pdo; j++) {
			pdo = port->snk_pdo[j];

			switch (sprd_pdo_type(pdo)) {
			case SPRD_PDO_TYPE_FIXED:
				max_snk_mv = sprd_pdo_fixed_voltage(pdo);
				min_snk_mv = max_snk_mv;
				break;
			case SPRD_PDO_TYPE_BATT:
			case SPRD_PDO_TYPE_VAR:
				max_snk_mv = sprd_pdo_max_voltage(pdo);
				min_snk_mv = sprd_pdo_min_voltage(pdo);
				break;
			case SPRD_PDO_TYPE_APDO:
				continue;
			default:
				sprd_tcpm_log(port, "Invalid sink PDO type, ignoring");
				continue;
			}

			if (max_src_mv <= max_snk_mv &&
				min_src_mv >= min_snk_mv) {
				/* Prefer higher voltages if available */
				if ((src_mw == max_mw && min_src_mv > max_mv) ||
							src_mw > max_mw) {
					*src_pdo = i;
					*sink_pdo = j;
					max_mw = src_mw;
					max_mv = min_src_mv;
					ret = 0;
				}
			}
		}
	}

	return ret;
}

#define sprd_min_pps_apdo_current(x, y)	\
	min(sprd_pdo_pps_apdo_max_current(x), sprd_pdo_pps_apdo_max_current(y))

static unsigned int sprd_tcpm_pd_select_pps_apdo(struct sprd_tcpm_port *port)
{
	unsigned int i, j, max_mw = 0, max_mv = 0;
	unsigned int min_src_mv, max_src_mv, src_ma, src_mw;
	unsigned int min_snk_mv, max_snk_mv;
	unsigned int max_op_mv;
	u32 pdo, src, snk;
	unsigned int src_pdo = 0, snk_pdo = 0;

	/*
	 * Select the source PPS APDO providing the most power while staying
	 * within the board's limits. We skip the first PDO as this is always
	 * 5V 3A.
	 */
	for (i = 1; i < port->nr_source_caps; ++i) {
		pdo = port->source_caps[i];

		switch (sprd_pdo_type(pdo)) {
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) != SPRD_APDO_TYPE_PPS) {
				sprd_tcpm_log(port, "Not PPS APDO (source), ignoring");
				continue;
			}

			min_src_mv = sprd_pdo_pps_apdo_min_voltage(pdo);
			max_src_mv = sprd_pdo_pps_apdo_max_voltage(pdo);
			src_ma = sprd_pdo_pps_apdo_max_current(pdo);
			src_mw = (src_ma * max_src_mv) / 1000;

			/*
			 * Now search through the sink PDOs to find a matching
			 * PPS APDO. Again skip the first sink PDO as this will
			 * always be 5V 3A.
			 */
			for (j = 1; j < port->nr_snk_pdo; j++) {
				pdo = port->snk_pdo[j];

				switch (sprd_pdo_type(pdo)) {
				case SPRD_PDO_TYPE_APDO:
					if (sprd_pdo_apdo_type(pdo) != SPRD_APDO_TYPE_PPS) {
						sprd_tcpm_log(port,
							      "Not PPS APDO (sink), ignoring");
						continue;
					}

					min_snk_mv =
						sprd_pdo_pps_apdo_min_voltage(pdo);
					max_snk_mv =
						sprd_pdo_pps_apdo_max_voltage(pdo);
					break;
				default:
					sprd_tcpm_log(port, "Not APDO type (sink), ignoring");
					continue;
				}

				if (min_src_mv <= max_snk_mv &&
				    max_src_mv >= min_snk_mv) {
					max_op_mv = min(max_src_mv, max_snk_mv);
					src_mw = (max_op_mv * src_ma) / 1000;
					/* Prefer higher voltages if available */
					if (src_mw > max_mw ||
					    (src_mw == max_mw && max_op_mv > max_mv) ||
					    (src_mw < max_mw && max_mv <= SPRD_PPS_5V_PROG_MAX &&
					     max_op_mv >= SPRD_PPS_5V_PROG_MAX)) {
						src_pdo = i;
						snk_pdo = j;
						max_mw = src_mw;
						max_mv = max_op_mv;
					}
				}
			}

			break;
		default:
			sprd_tcpm_log(port, "Not APDO type (source), ignoring");
			continue;
		}
	}

	if (src_pdo) {
		src = port->source_caps[src_pdo];
		snk = port->snk_pdo[snk_pdo];

		port->pps_data.req_min_volt = max(sprd_pdo_pps_apdo_min_voltage(src),
						  sprd_pdo_pps_apdo_min_voltage(snk));
		port->pps_data.req_max_volt = min(sprd_pdo_pps_apdo_max_voltage(src),
						  sprd_pdo_pps_apdo_max_voltage(snk));
		port->pps_data.req_max_curr = sprd_min_pps_apdo_current(src, snk);
		port->pps_data.req_out_volt = min(port->pps_data.req_max_volt,
						  max(port->pps_data.req_min_volt,
						      port->pps_data.req_out_volt));
		port->pps_data.req_op_curr = min(port->pps_data.req_max_curr,
						 port->pps_data.req_op_curr);
	}

	return src_pdo;
}

static int sprd_tcpm_pd_build_request(struct sprd_tcpm_port *port, u32 *rdo)
{
	unsigned int mv, ma, mw, flags;
	unsigned int max_ma, max_mw;
	enum sprd_pd_pdo_type type;
	u32 pdo, matching_snk_pdo;
	int src_pdo_index = 0;
	int snk_pdo_index = 0;
	int ret;

	ret = sprd_tcpm_pd_select_pdo(port, &snk_pdo_index, &src_pdo_index);
	if (ret < 0)
		return ret;

	pdo = port->source_caps[src_pdo_index];
	matching_snk_pdo = port->snk_pdo[snk_pdo_index];
	type = sprd_pdo_type(pdo);

	switch (type) {
	case SPRD_PDO_TYPE_FIXED:
		mv = sprd_pdo_fixed_voltage(pdo);
		break;
	case SPRD_PDO_TYPE_BATT:
	case SPRD_PDO_TYPE_VAR:
		mv = sprd_pdo_min_voltage(pdo);
		break;
	default:
		sprd_tcpm_log(port, "Invalid PDO selected!");
		return -EINVAL;
	}

	/* Select maximum available current within the sink pdo's limit */
	if (type == SPRD_PDO_TYPE_BATT) {
		mw = min_power(pdo, matching_snk_pdo);
		ma = 1000 * mw / mv;
	} else {
		ma = min_current(pdo, matching_snk_pdo);
		mw = ma * mv / 1000;
	}

	flags = SPRD_RDO_USB_COMM;
	if (!port->support_usb_suspend || !port->partner_support_usb_suspend)
		flags |= SPRD_RDO_NO_SUSPEND;

	/* Set mismatch bit if offered power is less than operating power */
	max_ma = ma;
	max_mw = mw;
	if (mw < port->operating_snk_mw) {
		flags |= SPRD_RDO_CAP_MISMATCH;
		if (type == SPRD_PDO_TYPE_BATT &&
		    (sprd_pdo_max_power(matching_snk_pdo) > sprd_pdo_max_power(pdo)))
			max_mw = sprd_pdo_max_power(matching_snk_pdo);
		else if (sprd_pdo_max_current(matching_snk_pdo) >
			 sprd_pdo_max_current(pdo))
			max_ma = sprd_pdo_max_current(matching_snk_pdo);
	}

	sprd_tcpm_log(port, "cc=%d cc1=%d cc2=%d vbus=%d vconn=%s polarity=%d",
		      port->cc_req, port->cc1, port->cc2, port->vbus_source,
		      port->vconn_role == TYPEC_SOURCE ? "source" : "sink",
		      port->polarity);

	if (type == SPRD_PDO_TYPE_BATT) {
		*rdo = SPRD_RDO_BATT(src_pdo_index + 1, mw, max_mw, flags);

		sprd_tcpm_log(port, "Requesting PDO %d: %u mV, %u mW%s",
			      src_pdo_index, mv, mw,
			      flags & SPRD_RDO_CAP_MISMATCH ? " [mismatch]" : "");
	} else {
		*rdo = SPRD_RDO_FIXED(src_pdo_index + 1, ma, max_ma, flags);

		sprd_tcpm_log(port, "Requesting PDO %d: %u mV, %u mA%s",
			      src_pdo_index, mv, ma,
			      flags & SPRD_RDO_CAP_MISMATCH ? " [mismatch]" : "");
	}

	port->req_current_limit = ma;
	port->req_supply_voltage = mv;
	port->fixed_pd_voltage = mv;
	if (!port->xts_limit_cur && port->negotiated_limit_ic_current && mv > 5900) {
		sprd_tcpm_set_cm_ic_limit_current(SPRD_PDO_TYPE_FIXED, mv * 1000, ma * 1000, false);
		port->negotiated_limit_cur = ma;
		port->negotiated_limit_ic_current = false;
	} else if (!port->xts_limit_cur && ma < port->negotiated_limit_cur) {
		sprd_tcpm_set_cm_ic_limit_current(SPRD_PDO_TYPE_FIXED, mv * 1000, ma * 1000, true);
		port->negotiated_limit_cur = ma;
		port->negotiated_limit_ic_current = true;
	}

	return 0;
}

static int sprd_tcpm_pd_send_request(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int ret;
	u32 rdo;

	ret = sprd_tcpm_pd_build_request(port, &rdo);
	if (ret < 0)
		return ret;

	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_REQUEST,
				       port->pwr_role,
				       port->data_role,
				       port->negotiated_rev,
				       port->message_id, 1, 0);
	msg.payload[0] = rdo;

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_pd_build_pps_request(struct sprd_tcpm_port *port, u32 *rdo)
{
	unsigned int out_mv, op_ma, op_mw, max_mv, max_ma, flags;
	enum sprd_pd_pdo_type type;
	unsigned int src_pdo_index;
	u32 pdo;

	src_pdo_index = sprd_tcpm_pd_select_pps_apdo(port);
	if (!src_pdo_index)
		return -EOPNOTSUPP;

	pdo = port->source_caps[src_pdo_index];
	type = sprd_pdo_type(pdo);

	switch (type) {
	case SPRD_PDO_TYPE_APDO:
		if (sprd_pdo_apdo_type(pdo) != SPRD_APDO_TYPE_PPS) {
			sprd_tcpm_log(port, "Invalid APDO selected!");
			return -EINVAL;
		}
		max_mv = port->pps_data.req_max_volt;
		max_ma = port->pps_data.req_max_curr;
		out_mv = port->pps_data.req_out_volt;
		op_ma = port->pps_data.req_op_curr;
		break;
	default:
		sprd_tcpm_log(port, "Invalid PDO selected!");
		return -EINVAL;
	}

	flags = SPRD_RDO_USB_COMM;
	if (!port->support_usb_suspend || !port->partner_support_usb_suspend)
		flags |= SPRD_RDO_NO_SUSPEND;

	op_mw = (op_ma * out_mv) / 1000;
	if (op_mw < port->operating_snk_mw) {
		/*
		 * Try raising current to meet power needs. If that's not enough
		 * then try upping the voltage. If that's still not enough
		 * then we've obviously chosen a PPS APDO which really isn't
		 * suitable so abandon ship.
		 */
		op_ma = (port->operating_snk_mw * 1000) / out_mv;
		if ((port->operating_snk_mw * 1000) % out_mv)
			++op_ma;
		op_ma += SPRD_RDO_PROG_CURR_MA_STEP - (op_ma % SPRD_RDO_PROG_CURR_MA_STEP);

		if (op_ma > max_ma) {
			op_ma = max_ma;
			out_mv = (port->operating_snk_mw * 1000) / op_ma;
			if ((port->operating_snk_mw * 1000) % op_ma)
				++out_mv;
			out_mv += SPRD_RDO_PROG_VOLT_MV_STEP -
				  (out_mv % SPRD_RDO_PROG_VOLT_MV_STEP);

			if (out_mv > max_mv) {
				sprd_tcpm_log(port, "Invalid PPS APDO selected!");
				return -EINVAL;
			}
		}
	}

	sprd_tcpm_log(port, "cc=%d cc1=%d cc2=%d vbus=%d vconn=%s polarity=%d",
		      port->cc_req, port->cc1, port->cc2, port->vbus_source,
		      port->vconn_role == TYPEC_SOURCE ? "source" : "sink",
		      port->polarity);

	*rdo = SPRD_RDO_PROG(src_pdo_index + 1, out_mv, op_ma, flags);

	sprd_tcpm_log(port, "Requesting APDO %d: %u mV, %u mA",
		      src_pdo_index, out_mv, op_ma);

	port->pps_data.req_op_curr = op_ma;
	port->pps_data.req_out_volt = out_mv;
	port->fixed_pd_voltage = 0;

	return 0;
}

static int sprd_tcpm_pd_send_pps_request(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int ret;
	u32 rdo;

	ret = sprd_tcpm_pd_build_pps_request(port, &rdo);
	if (ret < 0)
		return ret;

	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER(SPRD_PD_DATA_REQUEST,
				       port->pwr_role,
				       port->data_role,
				       port->negotiated_rev,
				       port->message_id, 1, 0);
	msg.payload[0] = rdo;

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_set_vbus(struct sprd_tcpm_port *port, bool enable)
{
	int ret;

	if (enable && port->vbus_charge)
		return -EINVAL;

	sprd_tcpm_log(port, "vbus:=%d charge=%d", enable, port->vbus_charge);

	ret = port->tcpc->set_vbus(port->tcpc, enable, port->vbus_charge);
	if (ret < 0)
		return ret;

	sprd_tcpm_log(port, "[%s:line%d] set vbus done", __func__, __LINE__);

	port->vbus_source = enable;
	return 0;
}

static int sprd_tcpm_set_charge(struct sprd_tcpm_port *port, bool charge)
{
	int ret;

	if (charge && port->vbus_source)
		return -EINVAL;

	if (charge != port->vbus_charge) {
		sprd_tcpm_log(port, "vbus=%d charge:=%d", port->vbus_source, charge);
		ret = port->tcpc->set_vbus(port->tcpc, port->vbus_source,
					   charge);
		if (ret < 0)
			return ret;
	}
	port->vbus_charge = charge;
	return 0;
}

static bool sprd_tcpm_start_toggling(struct sprd_tcpm_port *port, enum sprd_typec_cc_status cc)
{
	int ret;

	if (!port->tcpc->start_toggling)
		return false;

	sprd_tcpm_log_force(port, "Start toggling");
	ret = port->tcpc->start_toggling(port->tcpc, port->port_type, cc);
	return ret == 0;
}

static void sprd_tcpm_set_cc(struct sprd_tcpm_port *port, enum sprd_typec_cc_status cc)
{
	sprd_tcpm_log(port, "cc:=%d", cc);
	port->cc_req = cc;
	port->tcpc->set_cc(port->tcpc, cc);
}

static void sprd_tcpm_set_typec_roles(struct sprd_tcpm_port *port,
				      enum typec_port_type role,
				      enum typec_data_role data)
{
	sprd_tcpm_log(port, "typec try role en:=%d", role);
	port->tcpc->set_typec_role(port->tcpc, role, data);
}

static void sprd_tcpm_force_switch_rp_rd(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log(port, "%s, power role: %s",
		      __func__, port->pwr_role == TYPEC_SINK ? "source" : "sink");
	port->tcpc->force_swich_rp_rd(port->tcpc, port->pwr_role);
}

static int sprd_tcpm_init_vbus(struct sprd_tcpm_port *port)
{
	int ret;

	ret = port->tcpc->set_vbus(port->tcpc, false, false);
	port->vbus_source = false;
	port->vbus_charge = false;
	return ret;
}

static int sprd_tcpm_init_vconn(struct sprd_tcpm_port *port)
{
	int ret;

	ret = port->tcpc->set_vconn(port->tcpc, false);
	port->vconn_role = TYPEC_SINK;
	return ret;
}

static void sprd_tcpm_typec_connect(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log(port, "%s:line%d connected = %d", __func__, __LINE__, port->connected);
	if (!port->connected) {
		/* Make sure we don't report stale identity information */
		memset(&port->partner_ident, 0, sizeof(port->partner_ident));
		port->partner_desc.usb_pd = port->pd_capable;
		sprd_tcpm_log(port, "%s:line%d pd_capable = %d",
			      __func__, __LINE__, port->pd_capable);
		if (sprd_tcpm_port_is_debug(port))
			port->partner_desc.accessory = TYPEC_ACCESSORY_DEBUG;
		else if (sprd_tcpm_port_is_audio(port))
			port->partner_desc.accessory = TYPEC_ACCESSORY_AUDIO;
		else
			port->partner_desc.accessory = TYPEC_ACCESSORY_NONE;
		port->partner = typec_register_partner(port->typec_port,
						       &port->partner_desc);
		port->connected = true;
	}
}

static int sprd_tcpm_src_attach(struct sprd_tcpm_port *port)
{
	enum sprd_typec_cc_polarity polarity =
					port->cc2 == SPRD_TYPEC_CC_RD ? SPRD_TYPEC_POLARITY_CC2
							 : SPRD_TYPEC_POLARITY_CC1;
	int ret;

	if (port->attached)
		return 0;

	ret = sprd_tcpm_set_polarity(port, polarity);
	if (ret < 0)
		return ret;

	ret = sprd_tcpm_set_roles(port, true, TYPEC_SOURCE, TYPEC_HOST);
	if (ret < 0)
		return ret;

	ret = port->tcpc->set_pd_rx(port->tcpc, true);
	if (ret < 0)
		goto out_disable_mux;

	/*
	 * USB Type-C specification, version 1.2,
	 * chapter 4.5.2.2.8.1 (Attached.SRC Requirements)
	 * Enable VCONN only if the non-RD port is set to RA.
	 */
	if ((polarity == SPRD_TYPEC_POLARITY_CC1 && port->cc2 == SPRD_TYPEC_CC_RA) ||
	    (polarity == SPRD_TYPEC_POLARITY_CC2 && port->cc1 == SPRD_TYPEC_CC_RA)) {
		ret = sprd_tcpm_set_vconn(port, true);
		if (ret < 0)
			goto out_disable_pd;
	}

	ret = sprd_tcpm_set_vbus(port, true);
	if (ret < 0)
		goto out_disable_vconn;

	port->pd_capable = false;
	sprd_tcpm_log(port, "%s:line%d pd_capable flase", __func__, __LINE__);

	port->partner = NULL;
	sprd_tcpm_typec_connect(port);

	port->attached = true;
	port->send_discover = true;

	return 0;

out_disable_vconn:
	sprd_tcpm_set_vconn(port, false);
out_disable_pd:
	port->tcpc->set_pd_rx(port->tcpc, false);
out_disable_mux:
	sprd_tcpm_mux_set(port, TYPEC_STATE_SAFE, USB_ROLE_NONE,
		     TYPEC_ORIENTATION_NONE);
	return ret;
}

static void sprd_tcpm_typec_disconnect(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log(port, "%s:line%d connected = %d", __func__, __LINE__, port->connected);
	if (port->connected) {
		sprd_tcpm_log(port, "%s:line%d unregister_partner", __func__, __LINE__);
		typec_unregister_partner(port->partner);
		port->partner = NULL;
		port->connected = false;
	}
}

static void sprd_tcpm_unregister_altmodes(struct sprd_tcpm_port *port)
{
	struct sprd_pd_mode_data *modep = &port->mode_data;
	int i;

	for (i = 0; i < modep->altmodes; i++) {
		sprd_tcpm_log_force(port, "%s:line%d:, i = %d", __func__, __LINE__, i);
		typec_unregister_altmode(port->partner_altmode[i]);
		port->partner_altmode[i] = NULL;
	}

	memset(modep, 0, sizeof(*modep));
}

static void sprd_tcpm_reset_port(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log_force(port, "%s:line%d", __func__, __LINE__);
	sprd_tcpm_unregister_altmodes(port);
	sprd_tcpm_typec_disconnect(port);
	cancel_delayed_work(&port->dp_work);
	port->dp_status = 0;
	port->data_role_swap = false;
	port->drs_not_vdm = false;
	port->attached = false;
	port->pd_capable = false;
	sprd_tcpm_log(port, "%s:line%d pd_capable flase", __func__, __LINE__);
	port->pps_data.supported = false;
	port->update_ext_src_caps = false;
	port->xts_limit_cur = false;
	port->vdm_discovery_id_retry = 0;
	port->vdm_sent = false;
	port->vdm_queue = false;
	port->bist_test_data = false;
	port->sink_in_hard_reset = false;
	port->tx_chunk_msg = false;

	//sprd: modify
	port->negotiated_svdm_ver_minor = port->default_svdm_ver_minor;
	port->partner_support_usb_suspend = false;

	sprd_tcpm_set_cm_ic_limit_current(SPRD_PDO_TYPE_FIXED, 0, 0, false);
	port->negotiated_limit_cur = 0;
	port->negotiated_limit_ic_current = false;
	port->rp_limit = -EINVAL;

	port->received_get_snk_cap_cnt = 0;
	if (port->received_bad_good_crc) {
		sprd_tcpm_log(port, "%s, received_bad_good_crc: true --> false",
			      __func__);
		port->tcpc->check_tx_goodcrc(port->tcpc, true);
		port->tcpc->enable_tx_auto_retry(port->tcpc, true);
		port->received_bad_good_crc = false;
	}

	/*
	 * First Rx ID should be 0; set this to a sentinel of -1 so that
	 * we can check sprd_tcpm_pd_rx_handler() if we had seen it before.
	 */
	port->rx_msgid = -1;

	port->tcpc->set_pd_rx(port->tcpc, false);
	sprd_tcpm_init_vbus(port);	/* also disables charging */
	sprd_tcpm_init_vconn(port);
	sprd_tcpm_set_current_limit(port, 0, 0);
	sprd_tcpm_set_polarity(port, SPRD_TYPEC_POLARITY_CC1);
	sprd_tcpm_mux_set(port, TYPEC_STATE_SAFE, USB_ROLE_NONE,
		     TYPEC_ORIENTATION_NONE);
	sprd_tcpm_set_attached_state(port, false);
	port->try_src_count = 0;
	port->try_snk_count = 0;
	port->usb_type = POWER_SUPPLY_USB_TYPE_C;
	port->last_usb_type = POWER_SUPPLY_USB_TYPE_C;

	power_supply_changed(port->psy);
	sprd_tcpm_source_release_wake_lock(port);
}

static void sprd_tcpm_detach(struct sprd_tcpm_port *port)
{
	if (sprd_tcpm_port_is_disconnected(port))
		port->hard_reset_count = 0;

	sprd_tcpm_source_release_wake_lock(port);
	sprd_tcpm_set_typec_rp_level(port, SPRD_TYPEC_CC_RP_DEF);

	if (!port->attached)
		return;

	sprd_tcpm_reset_port(port);
}

static void sprd_tcpm_src_detach(struct sprd_tcpm_port *port)
{
	sprd_tcpm_detach(port);
}

static int sprd_tcpm_snk_attach(struct sprd_tcpm_port *port)
{
	int ret;

	if (port->attached)
		return 0;

	ret = sprd_tcpm_set_polarity(port, port->cc2 != SPRD_TYPEC_CC_OPEN ?
				     SPRD_TYPEC_POLARITY_CC2 : SPRD_TYPEC_POLARITY_CC1);
	if (ret < 0)
		return ret;

	ret = sprd_tcpm_set_roles(port, true, TYPEC_SINK, TYPEC_DEVICE);
	if (ret < 0)
		return ret;

	port->pd_capable = false;
	sprd_tcpm_log(port, "%s:line%d pd_capable flase", __func__, __LINE__);

	port->partner = NULL;
	sprd_tcpm_typec_connect(port);

	port->attached = true;
	port->send_discover = true;

	return 0;
}

static void sprd_tcpm_snk_detach(struct sprd_tcpm_port *port)
{
	sprd_tcpm_detach(port);
}

static int sprd_tcpm_acc_attach(struct sprd_tcpm_port *port)
{
	int ret;

	if (port->attached)
		return 0;

	ret = sprd_tcpm_set_roles(port, true, TYPEC_SOURCE, TYPEC_HOST);
	if (ret < 0)
		return ret;

	port->partner = NULL;

	sprd_tcpm_typec_connect(port);

	port->attached = true;

	return 0;
}

static void sprd_tcpm_acc_detach(struct sprd_tcpm_port *port)
{
	sprd_tcpm_detach(port);
}

static inline enum sprd_tcpm_state sprd_hard_reset_state(struct sprd_tcpm_port *port)
{
	if (port->hard_reset_count < SPRD_PD_N_HARD_RESET_COUNT)
		return HARD_RESET_SEND;
	if (port->pd_capable)
		return ERROR_RECOVERY;
	if (port->pwr_role == TYPEC_SOURCE)
		return SRC_UNATTACHED;
	if (port->state == SNK_WAIT_CAPABILITIES)
		return SNK_READY;
	return SNK_UNATTACHED;
}

static inline enum sprd_tcpm_state sprd_unattached_state(struct sprd_tcpm_port *port)
{
	if (port->port_type == TYPEC_PORT_DRP) {
		if (port->pwr_role == TYPEC_SOURCE)
			return SRC_UNATTACHED;
		else
			return SNK_UNATTACHED;
	} else if (port->port_type == TYPEC_PORT_SRC) {
		return SRC_UNATTACHED;
	}

	return SNK_UNATTACHED;
}

static void sprd_tcpm_check_retry_send_vdm(struct sprd_tcpm_port *port)
{
	if (port->power_role_swap && port->data_role == TYPEC_HOST && port->pd_capable &&
	    !port->send_discover && !port->vdm_sent) {
		sprd_tcpm_log(port, "retry send vdm");
		port->send_discover = true;
	}
}

static void sprd_tcpm_check_send_discover(struct sprd_tcpm_port *port)
{
	if (port->data_role == TYPEC_HOST && port->send_discover &&
	    port->pd_capable) {
		sprd_tcpm_send_vdm(port, SPRD_USB_SID_PD, SPRD_CMD_DISCOVER_IDENT, NULL, 0);
		port->send_discover = false;
	}
}

static void sprd_tcpm_swap_complete(struct sprd_tcpm_port *port, int result)
{
	if (port->swap_pending) {
		port->swap_status = result;
		port->swap_pending = false;
		port->non_pd_role_swap = false;
		port->role_swap_flag = false;
		sprd_tcpm_log(port, "source sink role swap complete");
		complete(&port->swap_complete);
	}
}

static enum typec_pwr_opmode sprd_tcpm_get_pwr_opmode(enum sprd_typec_cc_status cc)
{
	switch (cc) {
	case SPRD_TYPEC_CC_RP_1_5:
		return TYPEC_PWR_MODE_1_5A;
	case SPRD_TYPEC_CC_RP_3_0:
		return TYPEC_PWR_MODE_3_0A;
	case SPRD_TYPEC_CC_RP_DEF:
	default:
		return TYPEC_PWR_MODE_USB;
	}
}

static void sprd_tcpm_update_limit_current(struct sprd_tcpm_port *port)
{
	if (port->nr_source_caps == 1) {
		u32 pdo = port->source_caps[0];
		int max_mv, ma;
		enum sprd_pd_pdo_type type = sprd_pdo_type(pdo);

		if (type == SPRD_PDO_TYPE_FIXED) {
			max_mv = sprd_pdo_fixed_voltage(pdo);
			ma = sprd_pdo_max_current(pdo);
			if (max_mv == 5000 && ma == SPRD_LIMIT_POWER_TRANSFER_MA && ma > 0) {
				port->xts_limit_cur = true;
				power_supply_changed(port->psy);
			} else if (port->xts_limit_cur && max_mv == 5000 &&
				   ma != SPRD_LIMIT_POWER_TRANSFER_MA) {
				port->xts_limit_cur = false;
				power_supply_changed(port->psy);
			}
		}
	}
}

static void sprd_tcpm_set_initial_svdm_version(struct sprd_tcpm_port *port)
{
	if (!port->partner) {
		sprd_tcpm_log_force(port, "%s, partner is NULL!!!", __func__);
		return;
	}

	switch (port->negotiated_rev) {
	case SPRD_PD_REV30:
		break;
	/*
	 * 6.4.4.2.3 Structured VDM Version
	 * 2.0 states "At this time, there is only one version (1.0) defined.
	 * This field Shall be set to zero to indicate Version 1.0."
	 * 3.0 states "This field Shall be set to 01b to indicate Version 2.0."
	 * To ensure that we follow the Power Delivery revision we are currently
	 * operating on, downgrade the SVDM version to the highest one supported
	 * by the Power Delivery revision.
	 */
	case SPRD_PD_REV20:
		port->negotiated_svdm_ver_minor = SVDM_VER_MIMOR_0;
		typec_partner_set_svdm_version(port->partner, SVDM_VER_1_0);
		break;
	default:
		port->negotiated_svdm_ver_minor = SVDM_VER_MIMOR_0;
		typec_partner_set_svdm_version(port->partner, SVDM_VER_1_0);
		break;
	}
}

static void sprd_run_state_machine(struct sprd_tcpm_port *port)
{
	int ret;
	enum typec_pwr_opmode opmode;
	unsigned int msecs;
	u64 curr_time1, curr_time2, duration;

	port->enter_state = port->state;
	switch (port->state) {
	case TOGGLING:
		break;
	/* SRC states */
	case SRC_UNATTACHED:
		if (port->power_role_swap) {
			port->power_role_swap = false;
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_DRP, TYPEC_DEVICE);
			sprd_tcpm_log(port, "SRC_UNATTACHED clear power role swap flag");
		}
		if (!port->non_pd_role_swap)
			sprd_tcpm_swap_complete(port, -ENOTCONN);
		sprd_tcpm_src_detach(port);
		if (sprd_tcpm_start_toggling(port, sprd_tcpm_rp_cc(port))) {
			sprd_tcpm_set_state(port, TOGGLING, 0);
			break;
		}
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		if (port->port_type == TYPEC_PORT_DRP)
			sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_DRP_SNK);
		break;
	case SRC_ATTACH_WAIT:
		if (sprd_tcpm_port_is_debug(port))
			sprd_tcpm_set_state(port, DEBUG_ACC_ATTACHED,
					    SPRD_PD_T_CC_DEBOUNCE);
		else if (sprd_tcpm_port_is_audio(port))
			sprd_tcpm_set_state(port, AUDIO_ACC_ATTACHED,
					    SPRD_PD_T_CC_DEBOUNCE);
		else if (sprd_tcpm_port_is_source(port)) {
			if (!port->vbus_source &&
				port->get_vbus_ok &&
				port->get_vbus_ok(port->driver_data)) {
				pr_info("delay enable vbus %d ms\n", port->vbus_wait);
				sprd_tcpm_set_state(port, SRC_ATTACHED,
						    port->vbus_wait);
			} else {
				pr_info("tcc debounce %d ms\n", port->tcc_debounce);
				sprd_tcpm_set_state(port, SRC_ATTACHED,
						    port->tcc_debounce);
			}
		}
		break;

	case SNK_TRY:
		port->try_snk_count++;
		/*
		 * Requirements:
		 * - Do not drive vconn or vbus
		 * - Terminate CC pins (both) to Rd
		 * Action:
		 * - Wait for tDRPTry (SPRD_PD_T_DRP_TRY).
		 *   Until then, ignore any state changes.
		 */
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		sprd_tcpm_set_state(port, SNK_TRY_WAIT, SPRD_PD_T_DRP_TRY);
		break;
	case SNK_TRY_WAIT:
		if (sprd_tcpm_port_is_sink(port)) {
			sprd_tcpm_set_state(port, SNK_TRY_WAIT_DEBOUNCE, 0);
		} else {
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
			port->max_wait = 0;
		}
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS,
				    SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS:
		if (port->vbus_present && sprd_tcpm_port_is_sink(port)) {
			sprd_tcpm_set_state(port, SNK_ATTACHED, 0);
		} else {
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
			port->max_wait = 0;
		}
		break;
	case SRC_TRYWAIT:
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		if (port->max_wait == 0) {
			port->max_wait = jiffies +
					 msecs_to_jiffies(SPRD_PD_T_DRP_TRY);
			sprd_tcpm_set_state(port, SRC_TRYWAIT_UNATTACHED, SPRD_PD_T_DRP_TRY);
		} else {
			if (time_is_after_jiffies(port->max_wait))
				sprd_tcpm_set_state(port, SRC_TRYWAIT_UNATTACHED,
					       jiffies_to_msecs(port->max_wait -
								jiffies));
			else
				sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		}
		break;
	case SRC_TRYWAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_ATTACHED, SPRD_PD_T_CC_DEBOUNCE);
		break;
	case SRC_TRYWAIT_UNATTACHED:
		sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		break;

	case SRC_ATTACHED:
		ret = sprd_tcpm_src_attach(port);
		sprd_tcpm_set_state(port, SRC_STARTUP,
				    ret < 0 ? 0 : SPRD_PD_T_PS_SOURCE_ON);
		break;
	case SRC_STARTUP:
		opmode =  sprd_tcpm_get_pwr_opmode(sprd_tcpm_rp_cc(port));
		typec_set_pwr_opmode(port->typec_port, opmode);
		port->pwr_opmode = TYPEC_PWR_MODE_USB;
		port->caps_count = 0;
		port->negotiated_rev = SPRD_PD_MAX_REV;
		if (!port->power_role_swap)
			port->message_id = 0;

		if (port->power_role_swap) {
			port->message_id = 0;
			if (port->tcpc->set_pd_tx_id) {
				sprd_tcpm_log(port, "SRC_STARTUP, set tx id");
				ret = port->tcpc->set_pd_tx_id(port->tcpc, 0);
				if (ret)
					sprd_tcpm_log(port, "SRC_STARTUP, failed to set tx id");
			}
			if (port->tcpc->reset_pd_rx_id) {
				sprd_tcpm_log(port, "SRC_STARTUP, clear rx id");
				ret = port->tcpc->reset_pd_rx_id(port->tcpc);
				if (ret)
					sprd_tcpm_log(port, "SRC_STARTUP, failed to clear rx id");
			}
		}

		port->rx_msgid = -1;
		port->explicit_contract = false;
		sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, port->first_pd_cap_delay);
		break;
	case SRC_SEND_CAPABILITIES:
		port->caps_count++;
		if (port->caps_count > SPRD_PD_N_CAPS_COUNT) {
			sprd_tcpm_set_state(port, SRC_READY, 0);
			break;
		}
		curr_time1 = ktime_to_ms(ktime_get_boottime());
		sprd_tcpm_log(port, "%s:line%d curr_time1 = %lld ms",
			      __func__, __LINE__, curr_time1);
		ret = sprd_tcpm_pd_send_source_caps(port);
		curr_time2 = ktime_to_ms(ktime_get_boottime());
		sprd_tcpm_log(port, "%s:line%d curr_time2 = %lld ms",
			      __func__, __LINE__, curr_time2);
		duration = curr_time2 - port->tx_complete_curr_time;
		sprd_tcpm_log(port, "%s:line%d duration = %lld ms", __func__, __LINE__, duration);
		if (duration > 2  && duration < 15)
			duration -= 2;
		else
			duration = 0;
		if (ret < 0) {
			sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES,
					    SPRD_PD_T_SEND_SOURCE_CAP);
		} else {
			/*
			 * Per standard, we should clear the reset counter here.
			 * However, that can result in state machine hang-ups.
			 * Reset it only in READY state to improve stability.
			 */
			/* port->hard_reset_count = 0; */
			port->caps_count = 0;
			port->pd_capable = true;
			sprd_tcpm_log(port, "%s:line%d pd_capable true", __func__, __LINE__);
			if (!port->power_role_swap)
				sprd_tcpm_set_state_cond(port,
					SRC_SEND_CAPABILITIES_TIMEOUT,
					port->negotiated_rev < SPRD_PD_REV30 ?
					(SPRD_PD_T_SENDER_RESPONSE_PD2 - duration) :
					(SPRD_PD_T_SENDER_RESPONSE_PD3 - duration));
			else
				sprd_tcpm_set_state_cond(port, SRC_SEND_CAPABILITIES_TIMEOUT,
							 SPRD_PD_T_SEND_SOURCE_CAP_RESET);
		}
		break;
	case SRC_SEND_CAPABILITIES_TIMEOUT:
		/*
		 * Error recovery for a PD_DATA_SOURCE_CAP reply timeout.
		 *
		 * PD 2.0 sinks are supposed to accept src-capabilities with a
		 * 3.0 header and simply ignore any src PDOs which the sink does
		 * not understand such as PPS but some 2.0 sinks instead ignore
		 * the entire PD_DATA_SOURCE_CAP message, causing contract
		 * negotiation to fail.
		 *
		 * After SPRD_PD_N_HARD_RESET_COUNT hard-reset attempts, we try
		 * sending src-capabilities with a lower PD revision to
		 * make these broken sinks work.
		 */
		if (port->hard_reset_count < SPRD_PD_N_HARD_RESET_COUNT) {
			sprd_tcpm_set_state(port, HARD_RESET_SEND, 0);
		} else if (port->negotiated_rev > SPRD_PD_REV20) {
			port->negotiated_rev--;
			port->hard_reset_count = 0;
			sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, 0);
		} else {
			sprd_tcpm_set_state(port, sprd_hard_reset_state(port), 0);
		}
		break;
	case SRC_NEGOTIATE_CAPABILITIES:
		ret = sprd_tcpm_pd_check_request(port);
		if (ret < 0) {
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_REJECT);
			if (!port->explicit_contract) {
				sprd_tcpm_set_state(port,
					       SRC_WAIT_NEW_CAPABILITIES, 0);
			} else {
				sprd_tcpm_set_state(port, SRC_READY, 0);
			}
		} else {
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
			sprd_tcpm_set_state(port, SRC_TRANSITION_SUPPLY, SPRD_PD_T_SRC_TRANSITION);
		}
		break;
	case SRC_TRANSITION_SUPPLY:
		/* XXX: regulator_set_voltage(vbus, ...) */
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY);
		port->explicit_contract = true;
		typec_set_pwr_opmode(port->typec_port, TYPEC_PWR_MODE_PD);
		port->pwr_opmode = TYPEC_PWR_MODE_PD;
		sprd_tcpm_source_acquire_wake_lock(port);
		sprd_tcpm_set_typec_rp_level(port, SPRD_TYPEC_CC_RP_3_0);
		sprd_tcpm_set_state_cond(port, SRC_READY, 0);
		break;
	case SRC_READY:
#if 1
		port->hard_reset_count = 0;
#endif
		port->try_src_count = 0;
		if (port->update_ext_src_caps)
			port->update_ext_src_caps = false;

		schedule_delayed_work(&port->adjust_ctc_current, msecs_to_jiffies(0));
		if (port->power_role_swap) {
			port->power_role_swap = false;
			port->power_role_swap_hard_reset = false;
			sprd_tcpm_log(port, "SRC_READY swap_notify_typec");
			sprd_tcpm_set_pr_swap_flag(TCPM_TYPEC_SINK_TO_SOURCE);
			sprd_tcpm_log(port, "notify TYPEC_SINK_TO_SOURCE %s %d",
				      __func__, __LINE__);
			sprd_tcpm_set_pd_swap_event(TCPM_TYPEC_SINK_TO_SOURCE);
			sprd_tcpm_log(port, "notify TYPEC_SINK_TO_SOURCE %s %d",
				      __func__, __LINE__);
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_DRP, TYPEC_DEVICE);
			sprd_tcpm_log(port, "SRC_READY clear power role swap flag");
		}

		sprd_tcpm_typec_pr_swap_no_chk_detach(port, false);
		sprd_tcpm_swap_complete(port, 0);
		if (port->explicit_contract)
			sprd_tcpm_set_initial_svdm_version(port);

		sprd_tcpm_check_send_discover(port);
		/*
		 * 6.3.5
		 * Sending ping messages is not necessary if
		 * - the source operates at vSafe5V
		 * or
		 * - The system is not operating in PD mode
		 * or
		 * - Both partners are connected using a Type-C connector
		 *
		 * There is no actual need to send PD messages since the local
		 * port type-c and the spec does not clearly say whether PD is
		 * possible when type-c is connected to Type-A/B
		 */
		break;
	case SRC_WAIT_NEW_CAPABILITIES:
		/* Nothing to do... */
		break;

	/* SNK states */
	case SNK_UNATTACHED:
		if (port->power_role_swap) {
			port->power_role_swap = false;
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_DRP, TYPEC_DEVICE);
			sprd_tcpm_log(port, "SNK_UNATTACHED clear power role swap flag");
		}
		if (!port->non_pd_role_swap)
			sprd_tcpm_swap_complete(port, -ENOTCONN);
		sprd_tcpm_fixed_pd_complete(port);
		sprd_tcpm_pps_complete(port, -ENOTCONN);
		sprd_tcpm_snk_detach(port);
		if (sprd_tcpm_start_toggling(port, SPRD_TYPEC_CC_RD)) {
			sprd_tcpm_set_state(port, TOGGLING, 0);
			break;
		}
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		if (port->port_type == TYPEC_PORT_DRP)
			sprd_tcpm_set_state(port, SRC_UNATTACHED, SPRD_PD_T_DRP_SRC);
		break;
	case SNK_ATTACH_WAIT:
		if ((port->cc1 == SPRD_TYPEC_CC_OPEN &&
		     port->cc2 != SPRD_TYPEC_CC_OPEN) ||
		    (port->cc1 != SPRD_TYPEC_CC_OPEN &&
		     port->cc2 == SPRD_TYPEC_CC_OPEN))
			sprd_tcpm_set_state(port, SNK_DEBOUNCED, SPRD_PD_T_CC_DEBOUNCE);
		else if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_DEBOUNCED:
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		else if (port->vbus_present)
			sprd_tcpm_set_state(port,
					    sprd_tcpm_try_src(port) ? SRC_TRY : SNK_ATTACHED,
					    0);
		else
			/* Wait for VBUS, but not forever */
			if (!port->power_role_swap)
				sprd_tcpm_set_state(port, PORT_RESET, SPRD_PD_T_PS_SOURCE_ON);
			else
				sprd_tcpm_set_state(port, PORT_RESET, SPRD_PD_T_PS_SOURCE_ON_SWAP);
		break;

	case SRC_TRY:
		port->try_src_count++;
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		port->max_wait = 0;
		sprd_tcpm_set_state(port, SRC_TRY_WAIT, 0);
		break;
	case SRC_TRY_WAIT:
		if (port->max_wait == 0) {
			port->max_wait = jiffies +
					 msecs_to_jiffies(SPRD_PD_T_DRP_TRY);
			msecs = SPRD_PD_T_DRP_TRY;
		} else {
			if (time_is_after_jiffies(port->max_wait))
				msecs = jiffies_to_msecs(port->max_wait -
							 jiffies);
			else
				msecs = 0;
		}
		sprd_tcpm_set_state(port, SNK_TRYWAIT, msecs);
		break;
	case SRC_TRY_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_ATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_TRYWAIT:
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		sprd_tcpm_set_state(port, SNK_TRYWAIT_VBUS, SPRD_PD_T_CC_DEBOUNCE);
		break;
	case SNK_TRYWAIT_VBUS:
		/*
		 * TCPM stays in this state indefinitely until VBUS
		 * is detected as long as Rp is not detected for
		 * more than a time period of tPDDebounce.
		 */
		if (port->vbus_present && sprd_tcpm_port_is_sink(port)) {
			sprd_tcpm_set_state(port, SNK_ATTACHED, 0);
			break;
		}
		if (!sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_TRYWAIT_DEBOUNCE, 0);
		break;
	case SNK_TRYWAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_ATTACHED:
		ret = sprd_tcpm_snk_attach(port);
		if (ret < 0)
			sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		else
			sprd_tcpm_set_state(port, SNK_STARTUP, 0);
		break;
	case SNK_STARTUP:
		opmode =  sprd_tcpm_get_pwr_opmode(port->polarity ? port->cc2 : port->cc1);
		typec_set_pwr_opmode(port->typec_port, opmode);
		port->pwr_opmode = TYPEC_PWR_MODE_USB;
		port->negotiated_rev = SPRD_PD_MAX_REV;
		if (!port->power_role_swap)
			port->message_id = 0;

		if (port->power_role_swap) {
			port->message_id = 0;
			if (port->tcpc->set_pd_tx_id) {
				sprd_tcpm_log(port, "SNK_STARTUP, set tx id");
				ret = port->tcpc->set_pd_tx_id(port->tcpc, 0);
				if (ret)
					sprd_tcpm_log(port, "SNK_STARTUP, failed to set tx id");
			}
			if (port->tcpc->reset_pd_rx_id) {
				sprd_tcpm_log(port, "SNK_STARTUP, clear rx id");
				ret = port->tcpc->reset_pd_rx_id(port->tcpc);
				if (ret)
					sprd_tcpm_log(port, "SNK_STARTUP, failed to clear rx id");
			}
		}

		port->rx_msgid = -1;
		port->explicit_contract = false;
		sprd_tcpm_set_state(port, SNK_DISCOVERY, 0);
		break;
	case SNK_DISCOVERY:
		if (port->vbus_present) {
			port->set_rp_limint_en = true;
			sprd_tcpm_set_current_limit(port,
						    sprd_tcpm_get_current_limit(port),
						    5000);
			sprd_tcpm_set_charge(port, true);
			sprd_tcpm_set_state(port, SNK_WAIT_CAPABILITIES, 0);
			break;
		}
		/*
		 * For DRP, timeouts differ. Also, handling is supposed to be
		 * different and much more complex (dead battery detection;
		 * see USB power delivery specification, section 8.3.3.6.1.5.1).
		 */
		sprd_tcpm_set_state(port, sprd_hard_reset_state(port),
				    port->port_type == TYPEC_PORT_DRP ?
				    SPRD_PD_T_DB_DETECT : SPRD_PD_T_NO_RESPONSE);
		break;
	case SNK_DISCOVERY_DEBOUNCE:
		sprd_tcpm_set_state(port, SNK_DISCOVERY_DEBOUNCE_DONE, SPRD_PD_T_CC_DEBOUNCE);
		break;
	case SNK_DISCOVERY_DEBOUNCE_DONE:
		if (!sprd_tcpm_port_is_disconnected(port) &&
		    sprd_tcpm_port_is_sink(port) &&
		    ktime_after(port->delayed_runtime, ktime_get())) {
			sprd_tcpm_set_state(port, SNK_DISCOVERY,
					    ktime_to_ms(ktime_sub(port->delayed_runtime,
								  ktime_get())));
			break;
		}
		sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
		break;
	case SNK_WAIT_CAPABILITIES:
		if (!port->power_role_swap) {
			ret = port->tcpc->set_pd_rx(port->tcpc, true);
			if (ret < 0) {
				sprd_tcpm_set_state(port, SNK_READY, 0);
				break;
			}
		}
		/*
		 * If VBUS has never been low, and we time out waiting
		 * for source cap, try a soft reset first, in case we
		 * were already in a stable contract before this boot.
		 * Do this only once.
		 */
		if (port->vbus_never_low) {
			port->vbus_never_low = false;
			if (!port->power_role_swap)
				sprd_tcpm_set_state(port, SOFT_RESET_SEND, SPRD_PD_T_SINK_WAIT_CAP);
			else
				sprd_tcpm_set_state(port, SOFT_RESET_SEND,
						    SPRD_PD_T_SINK_WAIT_CAP_PR);
		} else {
			if (!port->power_role_swap)
				sprd_tcpm_set_state(port,
						    sprd_hard_reset_state(port),
						    SPRD_PD_T_SINK_WAIT_CAP);
			else
				sprd_tcpm_set_state(port,
						    sprd_hard_reset_state(port),
						    SPRD_PD_T_SINK_WAIT_CAP_PR);
		}
		break;
	case SNK_NEGOTIATE_CAPABILITIES:
		port->pd_capable = true;
		curr_time1 = ktime_to_ms(ktime_get_boottime());
		sprd_tcpm_log(port, "%s:line%d curr_time1 = %lld ms",
			      __func__, __LINE__, curr_time1);
		sprd_tcpm_log(port, "%s:line%d pd_capable true", __func__, __LINE__);
		port->hard_reset_count = 0;
		ret = sprd_tcpm_pd_send_request(port);
		curr_time2 = ktime_to_ms(ktime_get_boottime());
		sprd_tcpm_log(port, "%s:line%d curr_time2 = %lld ms",
			      __func__, __LINE__, curr_time2);
		duration = curr_time2 - port->tx_complete_curr_time;
		sprd_tcpm_log(port, "%s:line%d duration = %lld ms", __func__, __LINE__, duration);
		if (duration > 2 && duration < 15)
			duration -= 2;
		else
			duration = 0;
		if (ret < 0) {
			/* Let the Source send capabilities again. */
			sprd_tcpm_set_state(port, SNK_WAIT_CAPABILITIES, 0);
		} else {
			if (!port->power_role_swap)
				sprd_tcpm_set_state_cond(port,
						sprd_hard_reset_state(port),
						port->negotiated_rev < SPRD_PD_REV30 ?
						(SPRD_PD_T_SENDER_RESPONSE_PD2 - duration) :
						(SPRD_PD_T_SENDER_RESPONSE_PD3 - duration));
			else
				sprd_tcpm_set_state_cond(port, sprd_hard_reset_state(port),
							 SPRD_PD_T_SENDER_RESPONSE_RESET);
		}
		break;
	case SNK_NEGOTIATE_PPS_CAPABILITIES:
		ret = sprd_tcpm_pd_send_pps_request(port);
		if (ret < 0) {
			port->pps_status = ret;
			/*
			 * If this was called due to updates to sink
			 * capabilities, and pps is no longer valid, we should
			 * safely fall back to a standard PDO.
			 */
			if (port->update_sink_caps)
				sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
			else
				sprd_tcpm_set_state(port, SNK_READY, 0);
		} else {
			sprd_tcpm_set_state_cond(port,
						 sprd_hard_reset_state(port),
						 port->negotiated_rev < SPRD_PD_REV30 ?
						 SPRD_PD_T_SENDER_RESPONSE_PD2 :
						 SPRD_PD_T_SENDER_RESPONSE_PD3);
		}
		break;
	case SNK_TRANSITION_SINK:
	case SNK_TRANSITION_SINK_VBUS:
		if (port->power_role_swap)
			port->vbus_present = true;

		sprd_tcpm_set_state(port, sprd_hard_reset_state(port), SPRD_PD_T_PS_TRANSITION);
		break;
	case SNK_READY:
		if (port->power_role_swap) {
			port->power_role_swap_hard_reset = false;
			sprd_tcpm_log(port, "SNK_READY swap_notify_typec");
			sprd_tcpm_set_pr_swap_flag(TCPM_TYPEC_SOURCE_TO_SINK);
			sprd_tcpm_log(port, "notify TYPEC_SOURCE_TO_SINK %s %d",
				      __func__, __LINE__);
			sprd_tcpm_set_pd_swap_event(TCPM_TYPEC_SOURCE_TO_SINK);
			sprd_tcpm_log(port, "notify TYPEC_SOURCE_TO_SINK %s %d",
				      __func__, __LINE__);
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_DRP, TYPEC_DEVICE);
			sprd_tcpm_log(port, "SNK_READY clear power role swap flag");
		}

		if (port->explicit_contract &&
			g_sprd_typec_device_ops &&
			g_sprd_typec_device_ops->typec_notify_sink_ready_state)
			g_sprd_typec_device_ops->typec_notify_sink_ready_state();

		port->try_snk_count = 0;
		port->update_sink_caps = false;
		if (port->explicit_contract) {
			typec_set_pwr_opmode(port->typec_port, TYPEC_PWR_MODE_PD);
			port->pwr_opmode = TYPEC_PWR_MODE_PD;
		}

		port->sink_in_hard_reset = false;
		sprd_tcpm_typec_pr_swap_no_chk_detach(port, false);
		sprd_tcpm_update_limit_current(port);
		sprd_tcpm_swap_complete(port, 0);
		if (port->explicit_contract)
			sprd_tcpm_set_initial_svdm_version(port);

		sprd_tcpm_check_retry_send_vdm(port);
		sprd_tcpm_check_send_discover(port);
		sprd_tcpm_fixed_pd_complete(port);
		sprd_tcpm_pps_complete(port, port->pps_status);

		/*
		 * When avoiding PPS charging, the upper layer is notified
		 * repeatedly if the USB type is changed.
		*/
		if (port->usb_type != port->last_usb_type || port->power_role_swap) {
			port->last_usb_type = port->usb_type;
			port->power_role_swap = false;
			power_supply_changed(port->psy);
		}
		sprd_tcpm_source_release_wake_lock(port);
		break;

	/* Accessory states */
	case ACC_UNATTACHED:
		sprd_tcpm_acc_detach(port);
		sprd_tcpm_set_state(port, SRC_UNATTACHED, 0);
		break;
	case DEBUG_ACC_ATTACHED:
	case AUDIO_ACC_ATTACHED:
		ret = sprd_tcpm_acc_attach(port);
		if (ret < 0)
			sprd_tcpm_set_state(port, ACC_UNATTACHED, 0);
		break;
	case AUDIO_ACC_DEBOUNCE:
		sprd_tcpm_set_state(port, ACC_UNATTACHED, SPRD_PD_T_CC_DEBOUNCE);
		break;

	/* Hard_Reset states */
	case HARD_RESET_SEND:
		sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_HARD_RESET, NULL);
		sprd_tcpm_set_state(port, HARD_RESET_START, 0);
		break;
	case HARD_RESET_START:
		if (port->power_role_swap) {
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_DRP, TYPEC_DEVICE);
			port->power_role_swap = false;
			port->message_id = 0;
			port->power_role_swap_hard_reset = true;
		}

		port->received_get_snk_cap_cnt = 0;
		if (port->received_bad_good_crc) {
			sprd_tcpm_log(port, "%s, received_bad_good_crc: true --> false",
				      __func__);
			port->tcpc->check_tx_goodcrc(port->tcpc, true);
			port->tcpc->enable_tx_auto_retry(port->tcpc, true);
			port->received_bad_good_crc = false;
		}

		port->negotiated_limit_cur = 0;
		sprd_tcpm_typec_pr_swap_no_chk_detach(port, false);
		port->bist_test_data = false;
		port->hard_reset_count++;
		port->tcpc->set_pd_rx(port->tcpc, false);
		sprd_tcpm_unregister_altmodes(port);
		port->send_discover = true;
		port->last_usb_type = POWER_SUPPLY_USB_TYPE_C;
		if (port->pwr_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, SRC_HARD_RESET_VBUS_OFF,
					    SPRD_PD_T_PS_HARD_RESET);
		else
			sprd_tcpm_set_state(port, SNK_HARD_RESET_SINK_OFF, 0);
		break;
	case SRC_HARD_RESET_VBUS_OFF:
		sprd_tcpm_set_vconn(port, true);
		sprd_tcpm_set_vbus(port, false);
		if (!port->power_role_swap_hard_reset) {
			sprd_tcpm_set_roles(port, port->self_powered, TYPEC_SOURCE,
					    port->data_role);
		} else {
			port->pwr_role = TYPEC_SOURCE;
			port->tcpc->set_roles(port->tcpc, port->self_powered,
					      TYPEC_SOURCE, port->data_role);
		}
		sprd_tcpm_set_state(port, SRC_HARD_RESET_VBUS_ON, SPRD_PD_T_SRC_RECOVER);
		break;
	case SRC_HARD_RESET_VBUS_ON:
		sprd_tcpm_set_vbus(port, true);
		port->tcpc->set_pd_rx(port->tcpc, true);
		sprd_tcpm_set_attached_state(port, true);
		if (!port->power_role_swap)
			sprd_tcpm_set_state(port, SRC_STARTUP, SPRD_PD_T_PS_SOURCE_ON);
		else
			sprd_tcpm_set_state(port, SRC_STARTUP, SPRD_PD_T_PS_SOURCE_ON_RESET);
		break;
	case SNK_HARD_RESET_SINK_OFF:
		port->sink_in_hard_reset = true;
		memset(&port->pps_data, 0, sizeof(port->pps_data));
		sprd_tcpm_set_vconn(port, false);
		if (port->pd_capable)
			sprd_tcpm_set_charge(port, false);
		if (!port->power_role_swap_hard_reset) {
			sprd_tcpm_set_roles(port, port->self_powered, TYPEC_SINK,
					    port->data_role);
		} else {
			port->pwr_role = TYPEC_SINK;
			port->tcpc->set_roles(port->tcpc, port->self_powered,
					      TYPEC_SINK, port->data_role);
		}
		/*
		 * VBUS may or may not toggle, depending on the adapter.
		 * If it doesn't toggle, transition to SNK_HARD_RESET_SINK_ON
		 * directly after timeout.
		 */
		sprd_tcpm_set_state(port, SNK_HARD_RESET_SINK_ON, SPRD_PD_T_SAFE_0V +
				    SPRD_PD_T_SRC_RECOVER_MAX + SPRD_PD_T_SRC_TURN_ON);
		break;
	case SNK_HARD_RESET_WAIT_VBUS:
		/* Assume we're disconnected if VBUS doesn't come back. */
		sprd_tcpm_set_state(port, SNK_UNATTACHED,
				    SPRD_PD_T_SRC_RECOVER_MAX + SPRD_PD_T_SRC_TURN_ON +
				    SPRD_PD_T_VBUS_ON_COMPENSATE + 200); 
		break;
	case SNK_HARD_RESET_SINK_ON:
		/* Note: There is no guarantee that VBUS is on in this state */
		/*
		 * XXX:
		 * The specification suggests that dual mode ports in sink
		 * mode should transition to state PE_SRC_Transition_to_default.
		 * See USB power delivery specification chapter 8.3.3.6.1.3.
		 * This would mean to to
		 * - turn off VCONN, reset power supply
		 * - request hardware reset
		 * - turn on VCONN
		 * - Transition to state PE_Src_Startup
		 * SNK only ports shall transition to state Snk_Startup
		 * (see chapter 8.3.3.3.8).
		 * Similar, dual-mode ports in source mode should transition
		 * to PE_SNK_Transition_to_default.
		 */
		if (port->pd_capable) {
			port->set_rp_limint_en = true;
			sprd_tcpm_set_current_limit(port,
						    sprd_tcpm_get_current_limit(port),
						    5000);
			sprd_tcpm_set_charge(port, true);
		}
		sprd_tcpm_set_attached_state(port, true);
		sprd_tcpm_set_state(port, SNK_STARTUP, 0);
		break;

	/* Soft_Reset states */
	case SOFT_RESET:
		port->message_id = 0;
		port->rx_msgid = -1;

		if (port->tcpc->set_pd_tx_id) {
			sprd_tcpm_log(port, "soft reset send, clear tx id");
			ret = port->tcpc->set_pd_tx_id(port->tcpc, 0x0);
			if (ret)
				sprd_tcpm_log(port, "failed to clear tx id");
		}

		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		if (port->pwr_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, 0);
		else
			sprd_tcpm_set_state(port, SNK_WAIT_CAPABILITIES, 0);
		break;
	case SOFT_RESET_SEND:
		port->message_id = 0;
		port->rx_msgid = -1;

		if (port->tcpc->set_pd_tx_id) {
			sprd_tcpm_log(port, "soft reset send, clear tx id");
			ret = port->tcpc->set_pd_tx_id(port->tcpc, 0x0);
			if (ret)
				sprd_tcpm_log(port, "failed to clear tx id");
		}

		if (port->tcpc->reset_pd_rx_id) {//avoid receive the same message id
			sprd_tcpm_log(port, "soft reset send, clear rx id");
			ret = port->tcpc->reset_pd_rx_id(port->tcpc);
			if (ret)
				sprd_tcpm_log(port, "failed to clear rx id");
		}

		if (sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_SOFT_RESET))
			sprd_tcpm_set_state_cond(port, sprd_hard_reset_state(port), 0);
		else
			sprd_tcpm_set_state_cond(port,
						 sprd_hard_reset_state(port),
						 port->negotiated_rev < SPRD_PD_REV30 ?
						 SPRD_PD_T_SENDER_RESPONSE_PD2 :
						 SPRD_PD_T_SENDER_RESPONSE_PD3);
		break;

	/* DR_Swap states */
	case DR_SWAP_SEND:
		sprd_tcpm_cancel_vdm(port);
		port->data_role_swap = true;
		ret = sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_DR_SWAP);
		if (ret < 0 && port->data_role_send_count < 5) {
			sprd_tcpm_log(port, "DR_SWAP_SEND retry to send dr swap, ret = %d", ret);
			port->data_role_send_count++;
			sprd_tcpm_set_state_cond(port, DR_SWAP_SEND, 10);
			break;
		}
		port->data_role_send_count = 0;
		sprd_tcpm_set_state_cond(port, DR_SWAP_SEND_TIMEOUT, SPRD_PD_T_SENDER_RESPONSE_DR);
		break;
	case DR_SWAP_ACCEPT:
		ret = sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		if (ret < 0 && port->data_role_send_count < 5) {
			sprd_tcpm_log(port, "DR_SWAP_ACCEPT retry to send dr swap, ret = %d", ret);
			port->data_role_send_count++;
			sprd_tcpm_set_state_cond(port, DR_SWAP_ACCEPT, 10);
			break;
		}
		port->data_role_send_count = 0;
		sprd_tcpm_set_state_cond(port, DR_SWAP_CHANGE_DR, 0);
		break;
	case DR_SWAP_SEND_TIMEOUT:
		port->data_role_swap = false;
		sprd_tcpm_swap_complete(port, -ETIMEDOUT);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case DR_SWAP_CHANGE_DR:
		sprd_tcpm_log(port, "%s:line%d current roles [%s]", __func__,
			      __LINE__, port->data_role ? "host" : "device");
		if (port->data_role == TYPEC_HOST) {
			sprd_tcpm_unregister_altmodes(port);
			sprd_tcpm_set_roles(port, true, port->pwr_role, TYPEC_DEVICE);
			sprd_tcpm_set_dr_swap_flag(TCPM_TYPEC_HOST_TO_DEVICE);
			sprd_tcpm_set_pd_swap_event(TCPM_TYPEC_HOST_TO_DEVICE);
		} else {
			sprd_tcpm_set_roles(port, true, port->pwr_role, TYPEC_HOST);
			if (!port->drs_not_vdm) {
				port->send_discover = true;
				port->drs_not_vdm = true;
			}
			sprd_tcpm_set_dr_swap_flag(TCPM_TYPEC_DEVICE_TO_HOST);
			sprd_tcpm_set_pd_swap_event(TCPM_TYPEC_DEVICE_TO_HOST);
		}
		port->data_role_swap = false;
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;

	/* PR_Swap states */
	case PR_SWAP_ACCEPT:
		port->role_swap_flag = false;
		if (!port->try_pr_swap)
			sprd_tcpm_typec_pr_swap_no_chk_detach(port, true);

		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		sprd_tcpm_set_state(port, PR_SWAP_START, 0);
		break;
	case PR_SWAP_SEND:
		port->role_swap_flag = true;
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PR_SWAP);
		sprd_tcpm_set_state_cond(port,
					 PR_SWAP_SEND_TIMEOUT,
					 port->negotiated_rev < SPRD_PD_REV30 ?
					 SPRD_PD_T_SENDER_RESPONSE_PD2 :
					 SPRD_PD_T_SENDER_RESPONSE_PD3);
		break;
	case PR_SWAP_SEND_TIMEOUT:
		sprd_tcpm_swap_complete(port, -ETIMEDOUT);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case PR_SWAP_START:
		if (port->try_pr_swap)
			sprd_tcpm_typec_pr_swap_no_chk_detach(port, true);

		if (port->pwr_role == TYPEC_SOURCE) {
			sprd_tcpm_log(port, "source swap sink role start");
			port->power_role_swap = true;
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_SNK, TYPEC_DEVICE);
			sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_TRANSITION_OFF,
					    SPRD_PD_T_SRC_TRANSITION);
		} else {
			sprd_tcpm_log(port, "sink swap source role start");
			port->power_role_swap = true;
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_SRC, TYPEC_HOST);
			sprd_tcpm_set_state(port, PR_SWAP_SNK_SRC_SINK_OFF, 0);
		}
		break;
	case PR_SWAP_SRC_SNK_TRANSITION_OFF:
		sprd_tcpm_set_vbus(port, false);
		port->explicit_contract = false;
		/* allow time for Vbus discharge, must be < tSrcSwapStdby */
		sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SOURCE_OFF, SPRD_PD_T_SRCSWAPSTDBY);
		break;
	case PR_SWAP_SRC_SNK_SOURCE_OFF:
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		sprd_tcpm_force_switch_rp_rd(port);
		/* allow CC debounce */
		sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED,
				    SPRD_PD_T_CC_DEBOUNCE_SWAP);
		break;
	case PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED:
		/*
		 * USB-PD standard, 6.2.1.4, Port Power Role:
		 * "During the Power Role Swap Sequence, for the initial Source
		 * Port, the Port Power Role field shall be set to Sink in the
		 * PS_RDY Message indicating that the initial Source’s power
		 * supply is turned off"
		 */
		sprd_tcpm_set_pwr_role(port, TYPEC_SINK);
		if (sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY)) {
			sprd_tcpm_log(port, "OFF_CC_DEBOUNCED send ps rdy failed");
			sprd_tcpm_set_state(port, ERROR_RECOVERY, 0);
			break;
		}

		sprd_tcpm_set_state_cond(port, ERROR_RECOVERY,
					 SPRD_PD_T_PS_SOURCE_ON_SWAP);
		break;
	case PR_SWAP_SRC_SNK_SINK_ON:
		sprd_tcpm_typec_pr_swap_no_chk_detach(port, false);
		ret = port->tcpc->reset_pd_rx_id(port->tcpc);
		if (ret)
			sprd_tcpm_log(port, "failed to clear rx id");

		sprd_tcpm_set_state(port, SNK_STARTUP, 0);
		break;
	case PR_SWAP_SNK_SRC_SINK_OFF:
		sprd_tcpm_set_charge(port, false);
		sprd_tcpm_set_state(port, ERROR_RECOVERY, SPRD_PD_T_PS_SOURCE_OFF);
		break;
	case PR_SWAP_SNK_SRC_SOURCE_ON:
		sprd_tcpm_log(port, "[%s:line%d] mslssp start", __func__, __LINE__);
		sprd_tcpm_set_typec_rp_level(port, SPRD_TYPEC_CC_RP_3_0);
		msleep(50);
		sprd_tcpm_log(port, "[%s:line%d] msleep 50 end", __func__, __LINE__);
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		sprd_tcpm_force_switch_rp_rd(port);
		sprd_tcpm_set_vbus(port, true);
		/*
		 * allow time VBUS ramp-up, must be < tNewSrc
		 * Also, this window overlaps with CC debounce as well.
		 * So, Wait for the max of two which is SPRD_PD_T_NEWSRC
		 */
		sprd_tcpm_log(port, "[%s:line%d] PR_SWAP_SNK_SRC_SOURCE_ON", __func__, __LINE__);
		sprd_tcpm_set_state(port, PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP,
				    SPRD_PD_T_NEWSRC_SWAP);
		break;
	case PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP:
		/*
		 * USB PD standard, 6.2.1.4:
		 * "Subsequent Messages initiated by the Policy Engine,
		 * such as the PS_RDY Message sent to indicate that Vbus
		 * is ready, will have the Port Power Role field set to
		 * Source."
		 */
		sprd_tcpm_set_pwr_role(port, TYPEC_SOURCE);
		sprd_tcpm_typec_pr_swap_no_chk_detach(port, false);

source_pr_send_psrdy_retry:
		ret = sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY);
		if (ret < 0 && port->power_role_send_psrdy_count < 10) {
			sprd_tcpm_log(port, "VBUS_RAMPED_UP retry to send ps ready, ret = %d", ret);
			port->power_role_send_psrdy_count++;
			msleep(20);
			goto source_pr_send_psrdy_retry;
		}

		port->power_role_send_psrdy_count = 0;
		sprd_tcpm_set_state(port, SRC_STARTUP, SPRD_PD_T_SWAP_SRC_START);
		break;

	case VCONN_SWAP_ACCEPT:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		sprd_tcpm_set_state(port, VCONN_SWAP_START, 0);
		break;
	case VCONN_SWAP_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_VCONN_SWAP);
		sprd_tcpm_set_state(port,
				    VCONN_SWAP_SEND_TIMEOUT,
				    port->negotiated_rev < SPRD_PD_REV30 ?
				    SPRD_PD_T_SENDER_RESPONSE_PD2 : SPRD_PD_T_SENDER_RESPONSE_PD3);
		break;
	case VCONN_SWAP_SEND_TIMEOUT:
		sprd_tcpm_swap_complete(port, -ETIMEDOUT);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case VCONN_SWAP_START:
		if (port->vconn_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, VCONN_SWAP_WAIT_FOR_VCONN, 0);
		else
			sprd_tcpm_set_state(port, VCONN_SWAP_TURN_ON_VCONN, 0);
		break;
	case VCONN_SWAP_WAIT_FOR_VCONN:
		sprd_tcpm_set_state(port, sprd_hard_reset_state(port), SPRD_PD_T_VCONN_SOURCE_ON);
		break;
	case VCONN_SWAP_TURN_ON_VCONN:
		sprd_tcpm_set_vconn(port, true);
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case VCONN_SWAP_TURN_OFF_VCONN:
		sprd_tcpm_set_vconn(port, false);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;

	case DR_SWAP_CANCEL:
	case PR_SWAP_CANCEL:
	case VCONN_SWAP_CANCEL:
		sprd_tcpm_swap_complete(port, port->swap_status);
		if (port->pwr_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, SRC_READY, 0);
		else
			sprd_tcpm_set_state(port, SNK_READY, 0);
		break;

	case BIST_RX:
		switch (SPRD_BDO_MODE_MASK(port->bist_request)) {
		case SPRD_BDO_MODE_CARRIER2:
			sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_BIST_MODE_2, NULL);
			sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
			break;
		case SPRD_BDO_MODE_TESTDATA:
			sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
			port->bist_test_data = true;
			break;
		default:
			break;
		}
		break;
	case GET_STATUS_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_GET_STATUS);
		sprd_tcpm_set_state(port,
				    GET_STATUS_SEND_TIMEOUT,
				    port->negotiated_rev < SPRD_PD_REV30 ?
				    SPRD_PD_T_SENDER_RESPONSE_PD2 : SPRD_PD_T_SENDER_RESPONSE_PD3);
		break;
	case GET_STATUS_SEND_TIMEOUT:
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case GET_PPS_STATUS_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_GET_PPS_STATUS);
		sprd_tcpm_set_state(port,
				    GET_PPS_STATUS_SEND_TIMEOUT,
				    port->negotiated_rev < SPRD_PD_REV30 ?
				    SPRD_PD_T_SENDER_RESPONSE_PD2 : SPRD_PD_T_SENDER_RESPONSE_PD3);
		break;
	case GET_PPS_STATUS_SEND_TIMEOUT:
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case ERROR_RECOVERY:
		sprd_tcpm_typec_pr_swap_no_chk_detach(port, false);
		if (port->power_role_swap) {
			port->power_role_swap = false;
			sprd_tcpm_set_typec_roles(port, TYPEC_PORT_DRP, TYPEC_DEVICE);
			sprd_tcpm_log(port, "ERROR_RECOVERY clear power role swap flag");
		}
		sprd_tcpm_swap_complete(port, -EPROTO);
		sprd_tcpm_fixed_pd_complete(port);
		sprd_tcpm_pps_complete(port, -EPROTO);
		sprd_tcpm_set_state(port, PORT_RESET, 0);
		break;
	case PORT_RESET:
		sprd_tcpm_reset_port(port);
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_OPEN);
		sprd_tcpm_set_typec_err_recover_enter();
		sprd_tcpm_set_state(port, PORT_RESET_WAIT_OFF, SPRD_PD_T_ERROR_RECOVERY);
		break;
	case PORT_RESET_WAIT_OFF:
		sprd_tcpm_set_state(port,
				    sprd_tcpm_default_state(port),
				    port->vbus_present ? SPRD_PD_T_PS_SOURCE_OFF : 0);
		break;
	/* Chunk state */
	case CHUNK_NOT_SUPP:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_NOT_SUPP);
		sprd_tcpm_set_state(port,
				    port->pwr_role == TYPEC_SOURCE ? SRC_READY : SNK_READY, 0);
		break;
	default:
		WARN(1, "Unexpected port state %d\n", port->state);
		break;
	}
}

static void sprd_tcpm_state_machine_work(struct kthread_work *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
						   state_machine);
	enum sprd_tcpm_state prev_state;

	mutex_lock(&port->lock);
	port->state_machine_running = true;

	if (port->queued_message && sprd_tcpm_send_queued_message(port))
		goto done;

	/* If we were queued due to a delayed state change, update it now */
	if (port->delayed_state) {
		sprd_tcpm_log(port, "state change %s -> %s [delayed %ld ms]",
			      sprd_tcpm_states[port->state],
			      sprd_tcpm_states[port->delayed_state], port->delay_ms);
		port->prev_state = port->state;
		port->state = port->delayed_state;
		port->delayed_state = INVALID_STATE;
	}

	/*
	 * Continue running as long as we have (non-delayed) state changes
	 * to make.
	 */
	do {
		prev_state = port->state;
		sprd_run_state_machine(port);
		if (port->queued_message)
			sprd_tcpm_send_queued_message(port);
	} while (port->state != prev_state && !port->delayed_state);

done:
	port->state_machine_running = false;
	mutex_unlock(&port->lock);
}

static void sprd_tcpm_chunk_msg_work(struct work_struct *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
						   chunk_msg_work.work);
	enum sprd_pd_chunk_msg_request queued_chunk_message;

	if (!port) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	sprd_tcpm_log_force(port, "%s:line%d", __func__, __LINE__);

	/*
	 * note: careful lock
	 */
	mutex_lock(&port->lock);

	do {
		queued_chunk_message = port->queued_chunk_message;
		port->queued_chunk_message = PD_CHUNK_MSG_NONE;

		switch (queued_chunk_message) {
		case PD_CHUNK_MSG_CTRL_GET_SINK_CAP_EXT:
			sprd_tcpm_pd_send_sink_cap_ext(port);
			break;
		case PD_CHUNK_MSG_EXT_GET_BATTERY_CAP_EXT:
			sprd_tcpm_pd_send_battery_cap_ext(port, port->data[0]);
			break;
		default:
			break;
		}
	} while (port->queued_chunk_message != PD_CHUNK_MSG_NONE);

	mutex_unlock(&port->lock);
}

static void sprd_tcpm_rp_state_work(struct kthread_work *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
						   rp_state_work);
	u32 max_ma;

	max_ma = sprd_tcpm_get_current_limit(port);
	sprd_tcpm_set_rp_limit_current(max_ma);
	port->rp_limit = max_ma;
}

static void _sprd_tcpm_cc_change(struct sprd_tcpm_port *port,
				 enum sprd_typec_cc_status cc1,
				 enum sprd_typec_cc_status cc2)
{
	enum sprd_typec_cc_status old_cc1, old_cc2;
	enum sprd_tcpm_state new_state;
	u32 max_ma;

	old_cc1 = port->cc1;
	old_cc2 = port->cc2;
	port->cc1 = cc1;
	port->cc2 = cc2;

	sprd_tcpm_log_force(port,
			    "CC1: %u -> %u, CC2: %u -> %u [state %s, polarity %d, %s]",
			    old_cc1, cc1, old_cc2, cc2, sprd_tcpm_states[port->state],
			    port->polarity,
			    sprd_tcpm_port_is_disconnected(port) ? "disconnected" : "connected");

	if (sprd_tcpm_port_is_disconnected(port)) {
		pr_info("cc disconnected, cancel adjust work\n");
		cancel_delayed_work(&port->adjust_ctc_current);
		last_index = -1;
	}

	if (!port->explicit_contract && port->vbus_present) {
		max_ma = sprd_tcpm_get_current_limit(port);
		sprd_mod_tcpm_rp_delayed_work(port, max_ma > port->rp_limit ? 15 : 0);
	}

	switch (port->state) {
	case TOGGLING:
		if (sprd_tcpm_port_is_debug(port) || sprd_tcpm_port_is_audio(port) ||
		    sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_ATTACH_WAIT, 0);
		else if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_ATTACH_WAIT, 0);
		break;
	case SRC_UNATTACHED:
	case ACC_UNATTACHED:
		if (sprd_tcpm_port_is_debug(port) || sprd_tcpm_port_is_audio(port) ||
		    sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_ATTACH_WAIT, 0);
		break;
	case SRC_ATTACH_WAIT:
		if (sprd_tcpm_port_is_disconnected(port) ||
		    sprd_tcpm_port_is_audio_detached(port))
			sprd_tcpm_set_state(port, SRC_UNATTACHED, 0);
		else if (cc1 != old_cc1 || cc2 != old_cc2)
			sprd_tcpm_set_state(port, SRC_ATTACH_WAIT, 0);
		break;
	case SRC_ATTACHED:
	case SRC_SEND_CAPABILITIES:
	case SRC_READY:
		if (sprd_tcpm_port_is_disconnected(port) ||
		    !sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_UNATTACHED, 0);
		break;
	case SNK_UNATTACHED:
		if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_ATTACH_WAIT, 0);
		break;
	case SNK_ATTACH_WAIT:
		if ((port->cc1 == SPRD_TYPEC_CC_OPEN &&
		     port->cc2 != SPRD_TYPEC_CC_OPEN) ||
		    (port->cc1 != SPRD_TYPEC_CC_OPEN &&
		     port->cc2 == SPRD_TYPEC_CC_OPEN))
			new_state = SNK_DEBOUNCED;
		else if (sprd_tcpm_port_is_disconnected(port))
			new_state = SNK_UNATTACHED;
		else
			break;
		if (new_state != port->delayed_state)
			sprd_tcpm_set_state(port, SNK_ATTACH_WAIT, 0);
		break;
	case SNK_DEBOUNCED:
		if (sprd_tcpm_port_is_disconnected(port))
			new_state = SNK_UNATTACHED;
		else if (port->vbus_present)
			new_state = sprd_tcpm_try_src(port) ? SRC_TRY : SNK_ATTACHED;
		else
			new_state = SNK_UNATTACHED;
		if (new_state != port->delayed_state)
			sprd_tcpm_set_state(port, SNK_DEBOUNCED, 0);
		break;
	case SNK_READY:
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
		else if (!port->pd_capable &&
			 (cc1 != old_cc1 || cc2 != old_cc2)) {
			port->set_rp_limint_en = true;
			sprd_tcpm_set_current_limit(port,
						    sprd_tcpm_get_current_limit(port),
						    5000);
			}
		break;

	case AUDIO_ACC_ATTACHED:
		if (cc1 == SPRD_TYPEC_CC_OPEN || cc2 == SPRD_TYPEC_CC_OPEN)
			sprd_tcpm_set_state(port, AUDIO_ACC_DEBOUNCE, 0);
		break;
	case AUDIO_ACC_DEBOUNCE:
		if (sprd_tcpm_port_is_audio(port))
			sprd_tcpm_set_state(port, AUDIO_ACC_ATTACHED, 0);
		break;

	case DEBUG_ACC_ATTACHED:
		if (cc1 == SPRD_TYPEC_CC_OPEN || cc2 == SPRD_TYPEC_CC_OPEN)
			sprd_tcpm_set_state(port, ACC_UNATTACHED, 0);
		break;

	case SNK_TRY:
		/* Do nothing, waiting for timeout */
		break;

	case SNK_DISCOVERY:
		/* CC line is unstable, wait for debounce */
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, SNK_DISCOVERY_DEBOUNCE, 0);
		break;
	case SNK_DISCOVERY_DEBOUNCE:
		break;

	case SRC_TRYWAIT:
		/* Hand over to state machine if needed */
		if (!port->vbus_present && sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRYWAIT_DEBOUNCE, 0);
		break;
	case SRC_TRYWAIT_DEBOUNCE:
		if (port->vbus_present || !sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		if (!sprd_tcpm_port_is_sink(port)) {
			port->max_wait = 0;
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
		}
		break;
	case SRC_TRY_WAIT:
		if (sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRY_DEBOUNCE, 0);
		break;
	case SRC_TRY_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_TRY_WAIT, 0);
		break;
	case SNK_TRYWAIT_DEBOUNCE:
		if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_TRYWAIT_VBUS, 0);
		break;
	case SNK_TRYWAIT_VBUS:
		if (!sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_TRYWAIT_DEBOUNCE, 0);
		break;
	case SNK_TRYWAIT:
		/* Do nothing, waiting for tCCDebounce */
		break;
	case PR_SWAP_SNK_SRC_SINK_OFF:
	case PR_SWAP_SRC_SNK_TRANSITION_OFF:
	case PR_SWAP_SRC_SNK_SOURCE_OFF:
	case PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED:
	case PR_SWAP_SNK_SRC_SOURCE_ON:
		/*
		 * CC state change is expected in PR_SWAP
		 * Ignore it.
		 */
		break;

	case PORT_RESET:
	case PORT_RESET_WAIT_OFF:
		/*
		 * State set back to default mode once the timer completes.
		 * Ignore CC changes here.
		 */
		break;

	default:
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
		break;
	}
}

static void _sprd_tcpm_pd_vbus_on(struct sprd_tcpm_port *port)
{
	int i;

	port->nr_snk_pdo = port->nr_snk_default_pdo;
	for (i = 0; i < port->nr_snk_default_pdo; i++)
		port->snk_pdo[i] = port->snk_default_pdo[i];

	port->operating_snk_mw = port->operating_snk_default_mw;
	port->fixed_pd_voltage = 0;

	sprd_tcpm_log_force(port, "VBUS on");
	port->vbus_present = true;
	switch (port->state) {
	case SNK_TRANSITION_SINK_VBUS:
		port->explicit_contract = true;
		sprd_tcpm_set_state(port, SNK_READY, 0);
		break;
	case SNK_DISCOVERY:
		sprd_tcpm_set_state(port, SNK_DISCOVERY, 0);
		break;

	case SNK_DEBOUNCED:
		sprd_tcpm_set_state(port,
				    sprd_tcpm_try_src(port) ? SRC_TRY : SNK_ATTACHED,
				    0);
		break;
	case SNK_HARD_RESET_WAIT_VBUS:
		sprd_tcpm_set_state(port, SNK_HARD_RESET_SINK_ON, 0);
		break;
	case SRC_ATTACHED:
		sprd_tcpm_set_state(port, SRC_STARTUP, 0);
		break;
	case SRC_HARD_RESET_VBUS_ON:
		sprd_tcpm_set_state(port, SRC_STARTUP, 0);
		break;

	case SNK_TRY:
		/* Do nothing, waiting for timeout */
		break;
	case SRC_TRYWAIT:
		/* Do nothing, Waiting for Rd to be detected */
		break;
	case SRC_TRYWAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		/* Do nothing, waiting for PD_DEBOUNCE to do be done */
		break;
	case SNK_TRYWAIT:
		/* Do nothing, waiting for tCCDebounce */
		break;
	case SNK_TRYWAIT_VBUS:
		if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_ATTACHED, 0);
		break;
	case SNK_TRYWAIT_DEBOUNCE:
		/* Do nothing, waiting for Rp */
		break;
	case SRC_TRY_WAIT:
	case SRC_TRY_DEBOUNCE:
		/* Do nothing, waiting for sink detection */
		break;

	case PORT_RESET:
	case PORT_RESET_WAIT_OFF:
		/*
		 * State set back to default mode once the timer completes.
		 * Ignore vbus changes here.
		 */
		break;

	default:
		break;
	}
}

static void _sprd_tcpm_pd_vbus_off(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log_force(port, "VBUS off");
	port->vbus_present = false;
	port->vbus_never_low = false;
	switch (port->state) {
	case SNK_HARD_RESET_SINK_OFF:
		sprd_tcpm_set_state(port, SNK_HARD_RESET_WAIT_VBUS, 0);
		break;
	case SRC_HARD_RESET_VBUS_OFF:
		sprd_tcpm_set_state(port, SRC_HARD_RESET_VBUS_ON, 0);
		break;
	case HARD_RESET_SEND:
		break;

	case SNK_TRY:
		/* Do nothing, waiting for timeout */
		break;
	case SRC_TRYWAIT:
		/* Hand over to state machine if needed */
		if (sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRYWAIT_DEBOUNCE, 0);
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		/* Do nothing, waiting for PD_DEBOUNCE to do be done */
		break;
	case SNK_TRYWAIT:
	case SNK_TRYWAIT_VBUS:
	case SNK_TRYWAIT_DEBOUNCE:
		break;
	case SNK_ATTACH_WAIT:
		sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		break;

	case SNK_NEGOTIATE_CAPABILITIES:
		break;

	case PR_SWAP_SRC_SNK_TRANSITION_OFF:
		sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SOURCE_OFF, 0);
		break;

	case PR_SWAP_SNK_SRC_SINK_OFF:
		/* Do nothing, expected */
		break;

	case PORT_RESET_WAIT_OFF:
		sprd_tcpm_set_state(port, sprd_tcpm_default_state(port), 0);
		break;

	case SRC_TRY_WAIT:
	case SRC_TRY_DEBOUNCE:
		/* Do nothing, waiting for sink detection */
		break;

	case PORT_RESET:
		/*
		 * State set back to default mode once the timer completes.
		 * Ignore vbus changes here.
		 */
		break;

	default:
		if (port->pwr_role == TYPEC_SINK &&
		    port->attached)
			sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		break;
	}
}

static void _sprd_tcpm_pd_hard_reset(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log_force(port, "Received hard reset");
	/*
	 * If we keep receiving hard reset requests, executing the hard reset
	 * must have failed. Revert to error recovery if that happens.
	 */
	sprd_tcpm_set_state(port,
			    port->hard_reset_count < SPRD_PD_N_HARD_RESET_COUNT ?
			    HARD_RESET_START : ERROR_RECOVERY,
			    0);
}

static void sprd_tcpm_pd_event_handler(struct kthread_work *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
					      event_work);
	u32 events;

	mutex_lock(&port->lock);

	spin_lock(&port->pd_event_lock);
	while (port->pd_events) {
		events = port->pd_events;
		port->pd_events = 0;
		spin_unlock(&port->pd_event_lock);
		if (events & SPRD_TCPM_RESET_EVENT)
			_sprd_tcpm_pd_hard_reset(port);
		if (events & SPRD_TCPM_VBUS_EVENT) {
			bool vbus;

			vbus = port->tcpc->get_vbus(port->tcpc);
			if (vbus)
				_sprd_tcpm_pd_vbus_on(port);
			else
				_sprd_tcpm_pd_vbus_off(port);
		}
		if (events & SPRD_TCPM_CC_EVENT) {
			enum sprd_typec_cc_status cc1, cc2;

			if (port->tcpc->get_cc(port->tcpc, &cc1, &cc2) == 0)
				_sprd_tcpm_cc_change(port, cc1, cc2);
		}

		if (events & SPRD_TCPM_SOFT_RESET_EVENT) {
			sprd_tcpm_log_force(port, "%s, received soft reset event", __func__);
			if (port->received_bad_good_crc) {
				sprd_tcpm_log_force(port,
					"sprd: %s[%d], received_bad_good_crc: true --> false",
					__func__, __LINE__);
				port->tcpc->check_tx_goodcrc(port->tcpc, true);
				port->tcpc->enable_tx_auto_retry(port->tcpc, true);
				port->received_bad_good_crc = false;
				sprd_tcpm_log_force(port,
					"sprd: %s[%d], received_get_snk_cap_cnt: %d --> 0",
					__func__, __LINE__, port->received_get_snk_cap_cnt);
				port->received_get_snk_cap_cnt = 0;
			}

			sprd_tcpm_set_state(port, SOFT_RESET, 0);
		}

		spin_lock(&port->pd_event_lock);
	}
	spin_unlock(&port->pd_event_lock);
	mutex_unlock(&port->lock);
}

void sprd_tcpm_cc_change(struct sprd_tcpm_port *port)
{
	spin_lock(&port->pd_event_lock);
	port->pd_events |= SPRD_TCPM_CC_EVENT;
	spin_unlock(&port->pd_event_lock);
	kthread_queue_work(&port->tcpm_kworker, &port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_cc_change);

void sprd_tcpm_vbus_change(struct sprd_tcpm_port *port)
{
	spin_lock(&port->pd_event_lock);
	port->pd_events |= SPRD_TCPM_VBUS_EVENT;
	spin_unlock(&port->pd_event_lock);
	kthread_queue_work(&port->tcpm_kworker, &port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_vbus_change);

void sprd_tcpm_pd_hard_reset(struct sprd_tcpm_port *port)
{
	spin_lock(&port->pd_event_lock);
	port->pd_events = SPRD_TCPM_RESET_EVENT;
	spin_unlock(&port->pd_event_lock);
	kthread_queue_work(&port->tcpm_kworker, &port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_pd_hard_reset);

void sprd_tcpm_pd_soft_reset(struct sprd_tcpm_port *port)
{
	spin_lock(&port->pd_event_lock);
	port->pd_events = SPRD_TCPM_SOFT_RESET_EVENT;
	spin_unlock(&port->pd_event_lock);
	kthread_queue_work(&port->tcpm_kworker, &port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_pd_soft_reset);

static int sprd_tcpm_dr_set(struct typec_port *p, enum typec_data_role data)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	int ret;

	if (!port->can_power_data_role_swap) {
		sprd_tcpm_log_force(port, "[%s] can't data role swap", __func__);
		return 0;
	}

	sprd_tcpm_log_force(port, "[%s:line%d] wait lock [%s] ",
			    __func__, __LINE__, data ? "host" : "device");

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	sprd_tcpm_log_force(port, "[%s:line%d] get lock", __func__, __LINE__);
	if (port->port_type != TYPEC_PORT_DRP) {
		ret = -EINVAL;
		goto port_unlock;
	}

	sprd_tcpm_log_force(port, "[%s:line%d] current state  %s ",
			    __func__, __LINE__, sprd_tcpm_states[port->state]);
	if (port->state != SRC_READY && port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	sprd_tcpm_log_force(port, "[%s:line%d] state ready [%s]",
			    __func__, __LINE__, port->data_role ? "host" : "device");
	if (port->data_role == data) {
		ret = 0;
		goto port_unlock;
	}

	sprd_tcpm_log_force(port, "[%s:line%d] data role different", __func__, __LINE__);

	/*
	 * XXX
	 * 6.3.9: If an alternate mode is active, a request to swap
	 * alternate modes shall trigger a port reset.
	 * Reject data role swap request in this case.
	 */

	if (!port->pd_capable) {
		/*
		 * If the partner is not PD capable, reset the port to
		 * trigger a role change. This can only work if a preferred
		 * role is configured, and if it matches the requested role.
		 */
		if (port->try_role == TYPEC_NO_PREFERRED_ROLE ||
		    port->try_role == port->pwr_role) {
			ret = -EINVAL;
			goto port_unlock;
		}
		port->non_pd_role_swap = true;
		sprd_tcpm_set_state(port, PORT_RESET, 0);
	} else {
		sprd_tcpm_log_force(port, "[%s:line%d] DR_SWAP_SEND", __func__, __LINE__);
		sprd_tcpm_set_state(port, DR_SWAP_SEND, 0);
	}

	port->swap_status = 0;
	port->swap_pending = true;
	reinit_completion(&port->swap_complete);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->swap_complete,
					 msecs_to_jiffies(SPRD_PD_ROLE_SWAP_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->swap_status;

	port->non_pd_role_swap = false;
	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);
	return ret;
}

static int sprd_tcpm_pr_set(struct typec_port *p, enum typec_role role)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	int ret;

	if (!port->can_power_data_role_swap) {
		sprd_tcpm_log_force(port, "[%s] can't power role swap", __func__);
		return 0;
	}
	sprd_tcpm_log_force(port, "[%s:line%d] wait lock", __func__, __LINE__);

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	sprd_tcpm_log_force(port, "[%s:line%d] get lock", __func__, __LINE__);
	if (port->port_type != TYPEC_PORT_DRP) {
		ret = -EINVAL;
		goto port_unlock;
	}

	sprd_tcpm_log_force(port, "[%s:line%d] current state  %s ",
			    __func__, __LINE__, sprd_tcpm_states[port->state]);
	if (port->state != SRC_READY && port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	sprd_tcpm_log_force(port, "[%s:line%d] state ready", __func__, __LINE__);

	if (role == port->pwr_role) {
		ret = 0;
		goto port_unlock;
	}

	sprd_tcpm_log_force(port, "[%s:line%d] role different", __func__, __LINE__);

	port->swap_status = 0;
	port->swap_pending = true;
	port->try_pr_swap = true;
	reinit_completion(&port->swap_complete);
	sprd_tcpm_set_state(port, PR_SWAP_SEND, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->swap_complete,
					 msecs_to_jiffies(SPRD_PD_ROLE_SWAP_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->swap_status;

	port->try_pr_swap = false;
	sprd_tcpm_log(port, "%s, power role swap: %s",
		      __func__, (ret == -ETIMEDOUT) ? "fail" : "success");
	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);
	return ret;
}

static int sprd_tcpm_vconn_set(struct typec_port *p, enum typec_role role)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (port->state != SRC_READY && port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (role == port->vconn_role) {
		ret = 0;
		goto port_unlock;
	}

	port->swap_status = 0;
	port->swap_pending = true;
	reinit_completion(&port->swap_complete);
	sprd_tcpm_set_state(port, VCONN_SWAP_SEND, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->swap_complete,
					 msecs_to_jiffies(SPRD_PD_ROLE_SWAP_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->swap_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);
	return ret;
}

static int sprd_tcpm_try_role(struct typec_port *p, int role)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	struct tcpc_dev	*tcpc = port->tcpc;
	int ret = 0;

	mutex_lock(&port->lock);
	if (tcpc->try_role)
		ret = tcpc->try_role(tcpc, role);
	if (!ret && (!tcpc->config || !tcpc->config->try_role_hw))
		port->try_role = role;
	port->try_src_count = 0;
	port->try_snk_count = 0;
	mutex_unlock(&port->lock);

	return ret;
}

static int sprd_tcpm_pps_set_op_curr(struct sprd_tcpm_port *port, u16 req_op_curr)
{
	unsigned int target_mw;
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (!port->pps_data.active) {
		ret = -EOPNOTSUPP;
		goto port_unlock;
	}

	if (port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (req_op_curr > port->pps_data.max_curr) {
		ret = -EINVAL;
		goto port_unlock;
	}

	target_mw = (req_op_curr * port->supply_voltage) / 1000;
	if (target_mw < port->operating_snk_mw) {
		ret = -EINVAL;
		goto port_unlock;
	}

	/* Round down operating current to align with PPS valid steps */
	req_op_curr = req_op_curr - (req_op_curr % SPRD_RDO_PROG_CURR_MA_STEP);

	reinit_completion(&port->pps_complete);
	port->pps_data.req_op_curr = req_op_curr;
	port->pps_status = 0;
	port->pps_pending = true;
	sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->pps_complete,
					  msecs_to_jiffies(SPRD_PD_PPS_CTRL_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->pps_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);

	return ret;
}

static int sprd_tcpm_pps_set_out_volt(struct sprd_tcpm_port *port, u16 req_out_volt)
{
	unsigned int target_mw;
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (!port->pps_data.active) {
		ret = -EOPNOTSUPP;
		goto port_unlock;
	}

	if (port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (req_out_volt < port->pps_data.min_volt ||
	    req_out_volt > port->pps_data.max_volt) {
		ret = -EINVAL;
		goto port_unlock;
	}

	target_mw = (port->current_limit * req_out_volt) / 1000;
	if (target_mw < port->operating_snk_mw) {
		ret = -EINVAL;
		goto port_unlock;
	}

	/* Round down output voltage to align with PPS valid steps */
	req_out_volt = req_out_volt - (req_out_volt % SPRD_RDO_PROG_VOLT_MV_STEP);

	reinit_completion(&port->pps_complete);
	port->pps_data.req_out_volt = req_out_volt;
	port->pps_status = 0;
	port->pps_pending = true;
	sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->pps_complete,
					 msecs_to_jiffies(SPRD_PD_PPS_CTRL_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->pps_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);

	return ret;
}

static int sprd_tcpm_pps_activate(struct sprd_tcpm_port *port, bool activate)
{
	int ret = 0;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (!port->pps_data.supported) {
		ret = -EOPNOTSUPP;
		goto port_unlock;
	}

	/* Trying to deactivate PPS when already deactivated so just bail */
	if (!port->pps_data.active && !activate)
		goto port_unlock;

	if (port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	reinit_completion(&port->pps_complete);
	port->pps_status = 0;
	port->pps_pending = true;

	/* Trigger PPS request or move back to standard PDO contract */
	if (activate) {
		port->pps_data.req_out_volt = port->supply_voltage;
		port->pps_data.req_op_curr = port->current_limit;
		sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
	} else {
		sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
	}
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->pps_complete,
					 msecs_to_jiffies(SPRD_PD_PPS_CTRL_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->pps_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);

	return ret;
}

static void sprd_tcpm_init(struct sprd_tcpm_port *port)
{
	enum sprd_typec_cc_status cc1, cc2;

	port->tcpc->init(port->tcpc);

	sprd_tcpm_reset_port(port);

	/*
	 * XXX
	 * Should possibly wait for VBUS to settle if it was enabled locally
	 * since sprd_tcpm_reset_port() will disable VBUS.
	 */
	port->vbus_present = port->tcpc->get_vbus(port->tcpc);
	if (port->vbus_present)
		port->vbus_never_low = true;

	sprd_tcpm_set_state(port, sprd_tcpm_default_state(port), 0);

	if (port->tcpc->get_cc(port->tcpc, &cc1, &cc2) == 0)
		_sprd_tcpm_cc_change(port, cc1, cc2);

	/*
	 * Some adapters need a clean slate at startup, and won't recover
	 * otherwise. So do not try to be fancy and force a clean disconnect.
	 */
//	sprd_tcpm_set_state(port, PORT_RESET, 0);
}

static int sprd_tcpm_port_type_set(struct typec_port *p, enum typec_port_type type)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);

	sprd_tcpm_log_force(port, "[%s:line%d] port type:%d", __func__, __LINE__, type);

	mutex_lock(&port->lock);
	if (type == port->port_type)
		goto port_unlock;

	port->port_type = type;

	if (!port->connected) {
		sprd_tcpm_set_state(port, PORT_RESET, 0);
	} else if (type == TYPEC_PORT_SNK) {
		if (!(port->pwr_role == TYPEC_SINK &&
		      port->data_role == TYPEC_DEVICE))
			sprd_tcpm_set_state(port, PORT_RESET, 0);
	} else if (type == TYPEC_PORT_SRC) {
		if (!(port->pwr_role == TYPEC_SOURCE &&
		      port->data_role == TYPEC_HOST))
			sprd_tcpm_set_state(port, PORT_RESET, 0);
	}

port_unlock:
	mutex_unlock(&port->lock);
	return 0;
}

static const struct typec_operations sprd_tcpm_ops = {
	.try_role = sprd_tcpm_try_role,
	.dr_set = sprd_tcpm_dr_set,
	.pr_set = sprd_tcpm_pr_set,
	.vconn_set = sprd_tcpm_vconn_set,
	.port_type_set = sprd_tcpm_port_type_set
};

void sprd_tcpm_tcpc_reset(struct sprd_tcpm_port *port)
{
	mutex_lock(&port->lock);
	/* XXX: Maintain PD connection if possible? */
	sprd_tcpm_init(port);
	mutex_unlock(&port->lock);
}

static int sprd_tcpm_copy_pdos(u32 *dest_pdo, const u32 *src_pdo, unsigned int nr_pdo)
{
	unsigned int i;

	if (nr_pdo > SPRD_PDO_MAX_OBJECTS)
		nr_pdo = SPRD_PDO_MAX_OBJECTS;

	for (i = 0; i < nr_pdo; i++)
		dest_pdo[i] = src_pdo[i];

	return nr_pdo;
}

static int sprd_tcpm_copy_vdos(u32 *dest_vdo, const u32 *src_vdo, unsigned int nr_vdo)
{
	unsigned int i;

	if (nr_vdo > SPRD_VDO_MAX_OBJECTS)
		nr_vdo = SPRD_VDO_MAX_OBJECTS;

	for (i = 0; i < nr_vdo; i++)
		dest_vdo[i] = src_vdo[i];

	return nr_vdo;
}

static int sprd_tcpm_parse_dt(struct sprd_tcpm_port *port, struct fwnode_handle *fwnode)
{
	if (!fwnode)
		return -EINVAL;

	port->support_pd_cts = fwnode_property_read_bool(fwnode, "sprd,support-pd-cts");

	return 0;
}

static int sprd_tcpm_fw_get_caps(struct sprd_tcpm_port *port, struct fwnode_handle *fwnode)
{
	const char *cap_str;
	int ret, i;
	u32 uw;

	if (!fwnode)
		return -EINVAL;

	/* USB data support is optional */
	ret = fwnode_property_read_string(fwnode, "data-role", &cap_str);
	if (ret == 0) {
		ret = typec_find_port_data_role(cap_str);
		if (ret < 0)
			return ret;
		port->typec_caps.data = ret;
	}

	ret = fwnode_property_read_string(fwnode, "power-role", &cap_str);
	if (ret < 0)
		return ret;

	ret = typec_find_port_power_role(cap_str);
	if (ret < 0)
		return ret;
	port->typec_caps.type = ret;
	port->port_type = port->typec_caps.type;

	if (port->port_type == TYPEC_PORT_SNK)
		goto sink;

	/* Get source pdos */
	ret = fwnode_property_count_u32(fwnode, "source-pdos");
	if (ret <= 0)
		return -EINVAL;

	port->nr_src_pdo = min(ret, SPRD_PDO_MAX_OBJECTS);
	ret = fwnode_property_read_u32_array(fwnode, "source-pdos",
					     port->src_pdo, port->nr_src_pdo);
	if ((ret < 0) || sprd_tcpm_validate_caps(port, port->src_pdo, port->nr_src_pdo))
		return -EINVAL;

	for (i = 0; i < port->nr_src_pdo; i++) {
		if (sprd_pdo_type(port->src_pdo[i]) == SPRD_PDO_TYPE_FIXED &&
		    sprd_pdo_fixed_voltage(port->src_pdo[i]) == 5000)
			port->support_usb_suspend = !!(port->src_pdo[i] & SPRD_PDO_FIXED_SUSPEND);
	}
	/* Get xvdd pdos */
	ret = fwnode_property_count_u32(fwnode, "xvddsrc-pdos");
	if (ret <= 0)
		return -EINVAL;

	port->nr_xvddsrc_pdo = min(ret, SPRD_PDO_MAX_OBJECTS);
	ret = fwnode_property_read_u32_array(fwnode, "xvddsrc-pdos",
					     port->xvddsrc_pdo, port->nr_xvddsrc_pdo);
	if ((ret < 0) || sprd_tcpm_validate_caps(port, port->xvddsrc_pdo, port->nr_xvddsrc_pdo))
		return -EINVAL;

	if (port->port_type == TYPEC_PORT_SRC)
		return 0;

	/* Get the preferred power role for DRP */
	ret = fwnode_property_read_string(fwnode, "try-power-role", &cap_str);
	if (ret < 0)
		return ret;

	port->typec_caps.prefer_role = typec_find_power_role(cap_str);
	if (port->typec_caps.prefer_role < 0)
		return -EINVAL;
sink:
	/* Get sink pdos */
	ret = fwnode_property_count_u32(fwnode, "sink-pdos");
	if (ret <= 0)
		return -EINVAL;

	port->nr_snk_pdo = min(ret, SPRD_PDO_MAX_OBJECTS);
	ret = fwnode_property_read_u32_array(fwnode, "sink-pdos",
					     port->snk_pdo, port->nr_snk_pdo);
	if ((ret < 0) || sprd_tcpm_validate_caps(port, port->snk_pdo,
					    port->nr_snk_pdo))
		return -EINVAL;

	port->nr_snk_default_pdo = port->nr_snk_pdo;
	for (i = 0; i < port->nr_snk_default_pdo; i++)
		port->snk_default_pdo[i] = port->snk_pdo[i];

	if (fwnode_property_read_u32(fwnode, "op-sink-microwatt", &uw) < 0)
		return -EINVAL;
	port->operating_snk_default_mw = uw / 1000;
	port->operating_snk_mw = port->operating_snk_default_mw;

	port->self_powered = fwnode_property_read_bool(fwnode, "self-powered");

	return 0;
}

int sprd_tcpm_update_sink_capabilities(struct sprd_tcpm_port *port, const u32 *pdo,
				       unsigned int nr_pdo,
				       unsigned int operating_snk_mw)
{
	unsigned int delay_ms = 0;

	if (sprd_tcpm_validate_caps(port, pdo, nr_pdo))
		return -EINVAL;

	mutex_lock(&port->lock);
	port->nr_snk_pdo = sprd_tcpm_copy_pdos(port->snk_pdo, pdo, nr_pdo);
	port->operating_snk_mw = operating_snk_mw;
	port->update_sink_caps = true;
	if (port->drs_not_vdm) {
		delay_ms = 50;
		sprd_tcpm_log(port, "drs vdm, delay 50ms to request pdo");
	}

	switch (port->state) {
	case SNK_NEGOTIATE_CAPABILITIES:
	case SNK_NEGOTIATE_PPS_CAPABILITIES:
	case SNK_READY:
	case SNK_TRANSITION_SINK:
	case SNK_TRANSITION_SINK_VBUS:
		if (port->pps_data.active)
			sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
		else
			sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, delay_ms);
		break;
	default:
		break;
	}
	mutex_unlock(&port->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(sprd_tcpm_update_sink_capabilities);

int sprd_tcpm_update_ext_source_capabilities(struct sprd_tcpm_port *port,
					     const u32 *pdo,
					     unsigned int nr_pdo)
{
	int ret = 0;

	mutex_lock(&port->lock);
	port->nr_src_pdo_ext = sprd_tcpm_copy_pdos(port->src_pdo_ext, pdo, nr_pdo);
	sprd_tcpm_log_force(port, "%s:state %s", __func__, sprd_tcpm_states[port->state]);
	switch (port->state) {
	case SRC_READY:
		port->update_ext_src_caps = true;
		sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&port->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(sprd_tcpm_update_ext_source_capabilities);

void sprd_tcpm_get_source_capabilities(struct sprd_tcpm_port *port,
				       struct adapter_power_cap *pd_source_cap)
{
	int i;

	if (!port) {
		pd_source_cap->nr_source_caps = 0;
		pr_warn("port Null!!!\n");
		return;
	}

	/* Clears SRC_CAP in the disconnected state */
	if (sprd_tcpm_port_is_disconnected(port) &&
	    (port->state == SRC_UNATTACHED || port->state == SNK_UNATTACHED ||
	     port->state == TOGGLING)) {
		for (i = 0; i < pd_source_cap->nr_source_caps; i++) {
			pd_source_cap->max_mv[i] = 0;
			pd_source_cap->min_mv[i] = 0;
			pd_source_cap->ma[i] = 0;
			pd_source_cap->pwr_mw_limit[i] = 0;
		}
		pd_source_cap->nr_source_caps = 0;
		return;
	}

	pd_source_cap->nr_source_caps = port->nr_source_caps;
	for (i = 0; i < port->nr_source_caps; i++) {
		u32 pdo = port->source_caps[i];
		enum sprd_pd_pdo_type type = sprd_pdo_type(pdo);

		pd_source_cap->type[i] = type;
		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
			pd_source_cap->max_mv[i] = sprd_pdo_fixed_voltage(pdo);
			pd_source_cap->min_mv[i] = pd_source_cap->max_mv[i];
			pd_source_cap->ma[i] = sprd_pdo_max_current(pdo);
			break;
		case SPRD_PDO_TYPE_VAR:
			pd_source_cap->max_mv[i] = sprd_pdo_max_voltage(pdo);
			pd_source_cap->min_mv[i] = sprd_pdo_min_voltage(pdo);
			pd_source_cap->ma[i] = sprd_pdo_max_current(pdo);
			break;
		case SPRD_PDO_TYPE_BATT:
			pd_source_cap->max_mv[i] = sprd_pdo_max_voltage(pdo);
			pd_source_cap->min_mv[i] = sprd_pdo_min_voltage(pdo);
			pd_source_cap->pwr_mw_limit[i] = sprd_pdo_max_power(pdo);
			break;
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) == SPRD_APDO_TYPE_PPS) {
				pd_source_cap->max_mv[i] = sprd_pdo_pps_apdo_max_voltage(pdo);
				pd_source_cap->min_mv[i] = sprd_pdo_pps_apdo_min_voltage(pdo);
				pd_source_cap->ma[i] = sprd_pdo_pps_apdo_max_current(pdo);
			} else {
				pd_source_cap->nr_source_caps = pd_source_cap->nr_source_caps - 1;
			}
			break;
		default:
			pd_source_cap->nr_source_caps = pd_source_cap->nr_source_caps - 1;
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(sprd_tcpm_get_source_capabilities);

/* Power Supply access to expose source power information */
enum sprd_tcpm_psy_online_states {
	TCPM_PSY_OFFLINE = 0,
	TCPM_PSY_FIXED_ONLINE,
	TCPM_PSY_PROG_ONLINE,
};

static enum power_supply_property sprd_tcpm_psy_props[] = {
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int sprd_tcpm_psy_get_online(struct sprd_tcpm_port *port,
				    union power_supply_propval *val)
{
	if (port->vbus_charge) {
		if (port->pps_data.active)
			val->intval = TCPM_PSY_PROG_ONLINE;
		else
			val->intval = TCPM_PSY_FIXED_ONLINE;
	} else {
		val->intval = TCPM_PSY_OFFLINE;
	}

	return 0;
}

static int sprd_tcpm_psy_get_voltage_min(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	if (port->pps_data.active)
		val->intval = port->pps_data.min_volt * 1000;
	else
		val->intval = port->supply_voltage * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_voltage_max(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	if (port->pps_data.active)
		val->intval = port->pps_data.max_volt * 1000;
	else
		val->intval = port->supply_voltage * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_voltage_now(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	val->intval = port->supply_voltage * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_current_max(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	if (port->pps_data.active)
		val->intval = port->pps_data.max_curr * 1000;
	else
		val->intval = port->current_limit * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_current_now(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	val->intval = port->current_limit * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_prop(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct sprd_tcpm_port *port = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = port->usb_type;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sprd_tcpm_psy_get_online(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		ret = sprd_tcpm_psy_get_voltage_min(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = sprd_tcpm_psy_get_voltage_max(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sprd_tcpm_psy_get_voltage_now(port, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = sprd_tcpm_psy_get_current_max(port, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sprd_tcpm_psy_get_current_now(port, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_tcpm_psy_set_online(struct sprd_tcpm_port *port,
				    const union power_supply_propval *val)
{
	int ret;

	switch (val->intval) {
	case TCPM_PSY_FIXED_ONLINE:
		ret = sprd_tcpm_pps_activate(port, false);
		break;
	case TCPM_PSY_PROG_ONLINE:
		ret = sprd_tcpm_pps_activate(port, true);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_tcpm_psy_set_prop(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct sprd_tcpm_port *port = power_supply_get_drvdata(psy);
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sprd_tcpm_psy_set_online(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (val->intval < port->pps_data.min_volt * 1000 ||
		    val->intval > port->pps_data.max_volt * 1000)
			ret = -EINVAL;
		else
			ret = sprd_tcpm_pps_set_out_volt(port, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval > port->pps_data.max_curr * 1000)
			ret = -EINVAL;
		else
			ret = sprd_tcpm_pps_set_op_curr(port, val->intval / 1000);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_tcpm_psy_prop_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_usb_type sprd_tcpm_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
};

static const char *tcpm_psy_name_prefix = "sprd-tcpm-source-psy-";

static int devm_sprd_tcpm_psy_register(struct sprd_tcpm_port *port)
{
	struct power_supply_config psy_cfg = {};
	const char *port_dev_name = "sc27xx-pd";
	size_t psy_name_len = strlen(tcpm_psy_name_prefix) +
				     strlen(port_dev_name) + 1;
	char *psy_name;

	psy_cfg.drv_data = port;
	psy_cfg.fwnode = dev_fwnode(port->dev);
	psy_name = devm_kzalloc(port->dev, psy_name_len, GFP_KERNEL);
	if (!psy_name)
		return -ENOMEM;

	snprintf(psy_name, psy_name_len, "%s%s", tcpm_psy_name_prefix,
		 port_dev_name);
	port->psy_desc.name = psy_name;
	port->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	port->psy_desc.usb_types = sprd_tcpm_psy_usb_types;
	port->psy_desc.num_usb_types = ARRAY_SIZE(sprd_tcpm_psy_usb_types);
	port->psy_desc.properties = sprd_tcpm_psy_props;
	port->psy_desc.num_properties = ARRAY_SIZE(sprd_tcpm_psy_props);
	port->psy_desc.get_property = sprd_tcpm_psy_get_prop;
	port->psy_desc.set_property = sprd_tcpm_psy_set_prop;
	port->psy_desc.property_is_writeable = sprd_tcpm_psy_prop_writeable;

	port->usb_type = POWER_SUPPLY_USB_TYPE_C;
	port->last_usb_type = POWER_SUPPLY_USB_TYPE_C;

	port->psy = devm_power_supply_register(port->dev, &port->psy_desc,
					       &psy_cfg);

	return PTR_ERR_OR_ZERO(port->psy);
}

static enum hrtimer_restart sprd_tcpm_state_machine_timer_handler(struct hrtimer *timer)
{
	struct sprd_tcpm_port *port = container_of(timer, struct sprd_tcpm_port,
						   state_machine_timer);

	if (port->registered)
		kthread_queue_work(&port->tcpm_kworker, &port->state_machine);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart sprd_tcpm_vdm_state_machine_timer_handler(struct hrtimer *timer)
{
	struct sprd_tcpm_port *port = container_of(timer, struct sprd_tcpm_port,
						   vdm_state_machine_timer);

	if (port->registered)
		kthread_queue_work(&port->tcpm_kworker, &port->vdm_state_machine);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart sprd_tcpm_rp_state_change_timer_handler(struct hrtimer *timer)
{
	struct sprd_tcpm_port *port = container_of(timer, struct sprd_tcpm_port,
						   rp_state_change_timer);

	if (port->registered)
		kthread_queue_work(&port->tcpm_kworker, &port->rp_state_work);

	return HRTIMER_NORESTART;
}

static int sprd_tcpm_copy_caps(struct sprd_tcpm_port *port, const struct tcpc_config *tcfg)
{
	if (sprd_tcpm_validate_caps(port, tcfg->src_pdo, tcfg->nr_src_pdo) ||
	    sprd_tcpm_validate_caps(port, tcfg->snk_pdo, tcfg->nr_snk_pdo))
		return -EINVAL;

	port->nr_src_pdo = sprd_tcpm_copy_pdos(port->src_pdo, tcfg->src_pdo, tcfg->nr_src_pdo);
	port->nr_snk_pdo = sprd_tcpm_copy_pdos(port->snk_pdo, tcfg->snk_pdo, tcfg->nr_snk_pdo);

	port->nr_snk_vdo = sprd_tcpm_copy_vdos(port->snk_vdo, tcfg->snk_vdo, tcfg->nr_snk_vdo);

	port->operating_snk_mw = tcfg->operating_snk_mw;

	port->typec_caps.prefer_role = tcfg->default_role;
	port->typec_caps.type = tcfg->type;
	port->typec_caps.data = tcfg->data;
	port->self_powered = tcfg->self_powered;

	return 0;
}

static int sprd_tcpm_fixed_pd_deactivate(struct sprd_tcpm_port *port)
{
	int i, ret = 0;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	port->nr_snk_pdo = port->nr_snk_default_pdo;
	for (i = 0; i < port->nr_snk_default_pdo; i++)
		port->snk_pdo[i] = port->snk_default_pdo[i];

	port->operating_snk_mw = port->operating_snk_default_mw;

	/* Prevent 5V fixed voltage gear from not being configured in dts */
	if (port->nr_snk_pdo <= 0 ||
	    sprd_pdo_type(port->snk_pdo[0]) != SPRD_PDO_TYPE_FIXED ||
	    sprd_pdo_fixed_voltage(port->snk_pdo[0]) != 5000) {
		port->nr_snk_pdo = 1;
		port->snk_pdo[0] = SPRD_PDO_FIXED(5000, 3000, 0);
		port->operating_snk_mw = 15000;
	}

	port->fixed_pd_pending = true;
	reinit_completion(&port->fixed_pd_complete);

	sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);

	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->fixed_pd_complete,
					 msecs_to_jiffies(SPRD_PD_CTRL_TIMEOUT)))
		ret = -ETIMEDOUT;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);

	return ret;
}

void sprd_tcpm_shutdown(struct sprd_tcpm_port *port)
{
	int ret;

	if (port->usb_type == POWER_SUPPLY_USB_TYPE_PD ||
	    port->usb_type == POWER_SUPPLY_USB_TYPE_PD_PPS)
		pr_info("%s, pps_data.active=%d, pd_type=%d, fixed_pd_vol=%d\n",
			__func__, port->pps_data.active, port->usb_type, port->fixed_pd_voltage);

	if (port->pps_data.active) {
		ret = sprd_tcpm_pps_activate(port, false);
		if (ret)
			pr_err("%s, failed to disable pps, ret = %d\n", __func__, ret);
	} else if ((port->usb_type == POWER_SUPPLY_USB_TYPE_PD ||
		    port->usb_type == POWER_SUPPLY_USB_TYPE_PD_PPS) &&
		   port->fixed_pd_voltage > 5000) {
		ret = sprd_tcpm_fixed_pd_deactivate(port);
		if (ret)
			pr_err("%s, failed to force PD charger voltage to 5V, ret = %d\n",
			       __func__, ret);
	}

	port->registered = false;
	kthread_cancel_work_sync(&port->state_machine);
	kthread_cancel_work_sync(&port->vdm_state_machine);
	kthread_cancel_work_sync(&port->event_work);
	kthread_cancel_work_sync(&port->rp_state_work);
	hrtimer_cancel(&port->vdm_state_machine_timer);
	hrtimer_cancel(&port->state_machine_timer);
	hrtimer_cancel(&port->rp_state_change_timer);
	kthread_cancel_delayed_work_sync(&port->log_kwork);
	cancel_delayed_work(&port->adjust_ctc_current);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_shutdown);

struct sprd_tcpm_port *sprd_tcpm_register_port(struct device *dev, struct tcpc_dev *tcpc)
{
	struct sprd_tcpm_port *port;
	int i, err;

	if (!dev || !tcpc ||
	    !tcpc->get_vbus || !tcpc->set_cc || !tcpc->get_cc ||
	    !tcpc->set_polarity || !tcpc->set_vconn || !tcpc->set_vbus ||
	    !tcpc->set_pd_rx || !tcpc->set_roles || !tcpc->pd_transmit)
		return ERR_PTR(-EINVAL);

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return ERR_PTR(-ENOMEM);

	port->dev = dev;
	port->tcpc = tcpc;

	mutex_init(&port->lock);
	mutex_init(&port->swap_lock);
	sprd_tcpm_debug_log_init(port);
	sprd_tcpm_debug_log_switch(port);
	mutex_init(&port->keep_source_awake_mtx);

	port->log_task = kthread_run(kthread_worker_fn, &port->log_kworker,
			  "sprd_tcpm_log_worker");
	if (IS_ERR(port->log_task)) {
		pr_err("failed to run sprd tcpm log worker\n");
		return ERR_CAST(port->log_task);
	}

	kthread_init_worker(&port->tcpm_kworker);
	port->tcpm_event_task = kthread_run(kthread_worker_fn, &port->tcpm_kworker,
					    "sprd_tcpm_event_worker");
	if (IS_ERR(port->tcpm_event_task)) {
		pr_err("failed to run sprd tcpm event worker\n");
		return ERR_CAST(port->tcpm_event_task);
	}
	sched_set_fifo(port->tcpm_event_task);

	kthread_init_work(&port->state_machine, sprd_tcpm_state_machine_work);
	kthread_init_work(&port->vdm_state_machine, sprd_vdm_state_machine_work);
	kthread_init_work(&port->event_work, sprd_tcpm_pd_event_handler);
	kthread_init_work(&port->rp_state_work, sprd_tcpm_rp_state_work);
	hrtimer_init(&port->state_machine_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	port->state_machine_timer.function = sprd_tcpm_state_machine_timer_handler;
	hrtimer_init(&port->vdm_state_machine_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	port->vdm_state_machine_timer.function = sprd_tcpm_vdm_state_machine_timer_handler;
	INIT_DELAYED_WORK(&port->dp_work, sprd_tcpm_dp_vdm_work);
	INIT_DELAYED_WORK(&port->chunk_msg_work, sprd_tcpm_chunk_msg_work);
	hrtimer_init(&port->rp_state_change_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	port->rp_state_change_timer.function = sprd_tcpm_rp_state_change_timer_handler;
	INIT_DELAYED_WORK(&port->adjust_ctc_current, adjust_ctc_current_work);

	spin_lock_init(&port->pd_event_lock);

	init_completion(&port->tx_complete);
	init_completion(&port->swap_complete);
	init_completion(&port->fixed_pd_complete);
	init_completion(&port->pps_complete);
	sprd_tcpm_debugfs_init(port);

	port->pd_source_ws = wakeup_source_create("pd_source_wakelock");
	wakeup_source_add(port->pd_source_ws);

	port->fixed_pd_voltage = 0;

	if (sprd_tcpm_parse_dt(port, tcpc->fwnode))
		pr_err("%s, failed to parse device tree\n", __func__);

	err = sprd_tcpm_fw_get_caps(port, tcpc->fwnode);
	if ((err < 0) && tcpc->config)
		err = sprd_tcpm_copy_caps(port, tcpc->config);
	if (err < 0)
		goto out_destroy_wq;

	if (!tcpc->config || !tcpc->config->try_role_hw)
		port->try_role = port->typec_caps.prefer_role;
	else
		port->try_role = TYPEC_NO_PREFERRED_ROLE;

	port->typec_caps.fwnode = tcpc->fwnode;
	port->typec_caps.revision = 0x0120;	/* Type-C spec release 1.2 */
	port->typec_caps.pd_revision = 0x0300;	/* USB-PD spec release 3.0 */
	port->typec_caps.svdm_version = SVDM_VER_2_0;
	port->typec_caps.driver_data = port;
	port->typec_caps.ops = &sprd_tcpm_ops;
	port->typec_caps.data = TYPEC_PORT_DRD;

	port->partner_desc.identity = &port->partner_ident;
	port->port_type = port->typec_caps.type;

	port->role_sw = usb_role_switch_get(port->dev);
	if (IS_ERR(port->role_sw)) {
		err = PTR_ERR(port->role_sw);
		goto out_destroy_wq;
	}

	err = devm_sprd_tcpm_psy_register(port);
	if (err)
		goto out_role_sw_put;

	port->typec_port = typec_register_port(port->dev, &port->typec_caps);
	if (IS_ERR(port->typec_port)) {
		err = PTR_ERR(port->typec_port);
		goto out_role_sw_put;
	}

	if (tcpc->config && tcpc->config->alt_modes) {
		const struct typec_altmode_desc *paltmode = tcpc->config->alt_modes;

		i = 0;
		while (paltmode->svid && i < ARRAY_SIZE(port->port_altmode)) {
			struct typec_altmode *alt;

			alt = typec_port_register_altmode(port->typec_port,
							  paltmode);
			if (IS_ERR(alt)) {
				sprd_tcpm_log(port,
					      "%s: failed to register port alternate mode 0x%x",
					      dev_name(dev), paltmode->svid);
				break;
			}
			typec_altmode_set_drvdata(alt, port);
			alt->ops = &sprd_tcpm_altmode_ops;
			port->port_altmode[i] = alt;
			i++;
			if (i == tcpc->config->nr_alt_modes)
				break;
			paltmode++;
		}
	}
	port->registered = true;
	port->default_svdm_ver_minor = SVDM_VER_MIMOR_1;
	port->negotiated_svdm_ver_minor = port->default_svdm_ver_minor;

	init_completion(&port->tx_chunk_request);
	sprd_tcpm_debug_log_register_sysfs(port);

	port->vbus_wait = 500;
	port->tcc_debounce = 0;
	port->first_pd_cap_delay = 100;

	mutex_lock(&port->lock);
	sprd_tcpm_init(port);
	mutex_unlock(&port->lock);

	sprd_tcpm_log(port, "%s: registered, support_pd_cts: %d",
		      dev_name(dev), port->support_pd_cts);
	return port;

out_role_sw_put:
	usb_role_switch_put(port->role_sw);
out_destroy_wq:
	sprd_tcpm_debugfs_exit(port);
	kthread_flush_worker(&port->tcpm_kworker);
	kthread_stop(port->tcpm_event_task);
	kthread_flush_worker(&port->log_kworker);
	kthread_stop(port->log_task);
	wakeup_source_remove(port->pd_source_ws);
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_register_port);

void sprd_tcpm_unregister_port(struct sprd_tcpm_port *port)
{
	int i;

	port->registered = false;
	kthread_flush_worker(&port->tcpm_kworker);
	kthread_stop(port->tcpm_event_task);

	hrtimer_cancel(&port->vdm_state_machine_timer);
	hrtimer_cancel(&port->state_machine_timer);
	hrtimer_cancel(&port->rp_state_change_timer);
	cancel_delayed_work(&port->adjust_ctc_current);

	sprd_tcpm_reset_port(port);
	for (i = 0; i < ARRAY_SIZE(port->port_altmode); i++)
		typec_unregister_altmode(port->port_altmode[i]);
	typec_unregister_port(port->typec_port);
	usb_role_switch_put(port->role_sw);
	sprd_tcpm_debugfs_exit(port);
	kthread_flush_worker(&port->log_kworker);
	kthread_stop(port->log_task);
	wakeup_source_remove(port->pd_source_ws);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_unregister_port);

MODULE_LICENSE("GPL");
