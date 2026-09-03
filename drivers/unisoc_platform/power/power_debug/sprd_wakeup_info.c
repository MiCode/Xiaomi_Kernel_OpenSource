// SPDX-License-Identifier: GPL-2.0
//
// UNISOC APCPU POWER STAT driver
//
// Copyright (C) 2020 Unisoc, Inc.
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqnr.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_pdbg.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>
#include "sprd_pdbg_comm.h"
#include "sprd_wakeup_info.h"

#define IRQ_DOMAIN_RETRY_CNT        (10)
#define WAKEUP_INFO_MAX_SIZE        (20)
#define WAKEUP_INFO_BUF_PER_WS      (64)
#define SMSG_BUF_MAX                (16)
#define INVALID_DATA                (0xff)
#define WS_BUF_SIZE        (WAKEUP_INFO_MAX_SIZE * WAKEUP_INFO_BUF_PER_WS * SIPC_ID_NR)
#define TIME_FOMATE "(%d-%02d-%02d %02d:%02d:%02d to %d-%02d-%02d %02d:%02d:%02d UTC)\n"

static void wakeup_info_list_clear(struct wakeup_info_data *ws_data);
static void sprd_ws_info_update(int virq, u8 dst, u8 channel);

static struct wakeup_info_data *g_ws_data;
static inline struct wakeup_info_data *ws_inst_get(void)
{
	return g_ws_data;
}

static struct ws_irq_domain *ws_irq_domain_get(struct wakeup_info_data *ws_data, int domain_id)
{
	struct ws_irq_domain *pos, *tmp;

	read_lock(&ws_data->rw_lock);
	list_for_each_entry_safe(pos, tmp, &ws_data->ws_irq_domain_list, list) {
		if (pos->domain_id != domain_id)
			continue;
		read_unlock(&ws_data->rw_lock);
		return pos;
	}
	read_unlock(&ws_data->rw_lock);

	return NULL;
}

static void ws_get_info(struct ws_irq_domain *ws_domain, u32 hwirq, char *ws_info, int *buf_cnt)
{
	int virq;
	struct irq_desc *desc;
	struct irq_domain *irq_domain = (struct irq_domain *)(ws_domain->priv_data);

	virq = irq_find_mapping(irq_domain, hwirq);
	desc = irq_to_desc(virq);
	sprd_ws_info_update(virq, SIPC_ID_AP, -1);

	*buf_cnt += scnprintf(ws_info + *buf_cnt, WS_LOG_BUF_MAX - *buf_cnt, " | [%d]", virq);

	if (desc == NULL) {
		*buf_cnt += scnprintf(ws_info + *buf_cnt, WS_LOG_BUF_MAX - *buf_cnt, "| stray irq");
		return;
	}

	if (desc->action && desc->action->name)
		*buf_cnt += scnprintf(ws_info + *buf_cnt, WS_LOG_BUF_MAX - *buf_cnt,
				      " | action: %s", desc->action->name);

	if (desc->action && desc->action->handler)
		*buf_cnt += scnprintf(ws_info + *buf_cnt, WS_LOG_BUF_MAX - *buf_cnt,
				      " | handler: %ps", desc->action->handler);

	if (desc->action && desc->action->thread_fn)
		*buf_cnt += scnprintf(ws_info + *buf_cnt, WS_LOG_BUF_MAX - *buf_cnt,
				      " | thread_fn: %ps", desc->action->thread_fn);
}

static int ws_parse(struct wakeup_info_data *ws_data, u32 major, u32 domain_id, u32 hwirq,
		    char *ws_info)
{
	int buf_cnt = 0, intc_num, intc_bit;
	struct ws_irq_domain *ws_irq_domain;

	intc_num = (major >> 16) & 0xFFFF;
	intc_bit = major & 0xFFFF;

	buf_cnt += scnprintf(ws_info + buf_cnt, WS_LOG_BUF_MAX - buf_cnt, "[%d:%d:%d:%d]",
			     intc_num, intc_bit, domain_id, hwirq);

	if ((domain_id != DATA_INVALID) && (hwirq != DATA_INVALID)) {

		ws_irq_domain = ws_irq_domain_get(ws_data, domain_id);

		if (!ws_irq_domain) {
			SPRD_PDBG_ERR("ws irq_domain[%u] match error\n", domain_id);
			return 0;
		}

		ws_get_info(ws_irq_domain, hwirq, ws_info, &buf_cnt);
	}
	return 0;
}

static int irq_domain_add(struct wakeup_info_data *ws_data, int irq_domain_id, void *priv_data)
{
	struct ws_irq_domain *pos, *tmp;
	struct ws_irq_domain *pw;

	read_lock(&ws_data->rw_lock);
	list_for_each_entry_safe(pos, tmp, &ws_data->ws_irq_domain_list, list) {
		if (pos->domain_id != irq_domain_id)
			continue;
		SPRD_PDBG_ERR("%s: ws irq domain(%d) exist\n", __func__, irq_domain_id);
		read_unlock(&ws_data->rw_lock);
		return -EEXIST;
	}
	read_unlock(&ws_data->rw_lock);

	pw = kzalloc(sizeof(struct ws_irq_domain), GFP_KERNEL);
	if (!pw) {
		SPRD_PDBG_ERR("%s: ws irq domain alloc error\n", __func__);
		return -ENOMEM;
	}

	pw->domain_id = irq_domain_id;
	pw->priv_data = priv_data;

	write_lock(&ws_data->rw_lock);
	list_add_tail(&pw->list, &ws_data->ws_irq_domain_list);
	write_unlock(&ws_data->rw_lock);

	return 0;
}

static void dt_irq_domain_names_get(struct device *dev, struct wakeup_info_data *ws_data)
{
	int i;
	char *irq_domain_names[SPRD_PDBG_WS_DOMAIN_ID_MAX] = {
		"sprd,pdbg-irq-domain-gic",
		"sprd,pdbg-irq-domain-gpio",
		"sprd,pdbg-irq-domain-ana",
		"sprd,pdbg-irq-domain-ana-eic",
		"sprd,pdbg-irq-domain-ap-eic-dbnc",
		"sprd,pdbg-irq-domain-ap-eic-latch",
		"sprd,pdbg-irq-domain-ap-eic-async",
		"sprd,pdbg-irq-domain-ap-eic-sync"
	};
	struct device_node *node = dev->of_node;

	for (i = 0; i < SPRD_PDBG_WS_DOMAIN_ID_MAX; i++) {
		if (of_property_read_string(node, irq_domain_names[i],
					    &ws_data->irq_domain_names[i]))
			ws_data->irq_domain_names[i] = NULL;
		else
			SPRD_PDBG_INFO("dt found %s[%s]\n", irq_domain_names[i],
				       ws_data->irq_domain_names[i]);
	}
}

static void irq_domain_parse_work(struct work_struct *work)
{
	int irq, j;
	struct irq_desc *desc;
	bool match_done = false;
	struct wakeup_info_data *ws_data =
		container_of(work, struct wakeup_info_data, irq_domain_work.work);

	irq = ws_data->last_nr_irqs;
	ws_data->last_nr_irqs = nr_irqs;
	for (; irq < nr_irqs; irq++) {
		desc = irq_to_desc(irq);
		if (!desc || !desc->irq_data.chip)
			continue;

		match_done = true;
		for (j = 0; j < SPRD_PDBG_WS_DOMAIN_ID_MAX; j++) {
			if (!ws_data->irq_domain_names[j])
				continue;
			match_done = false;
			if (!strcmp(ws_data->irq_domain_names[j], desc->irq_data.chip->name)) {
				irq_domain_add(ws_data, j, desc->irq_data.domain);
				SPRD_PDBG_INFO("match irq domain[%s]\n",
					       ws_data->irq_domain_names[j]);
				ws_data->irq_domain_names[j] = NULL;
				break;
			}
		}
		if (match_done)
			break;
	}

	if (!match_done && ws_data->irq_domain_retry_cnt++ < IRQ_DOMAIN_RETRY_CNT)
		schedule_delayed_work(&ws_data->irq_domain_work, msecs_to_jiffies(2000));
}

static int wakeup_info_add(struct wakeup_info_data *ws_data, struct wakeup_info *ws_add)
{
	struct wakeup_info *pos, *tmp;
	struct wakeup_info *ws;
	u8 dst = ws_add->dst, channel = ws_add->channel;
	int virq = ws_add->virq;

	if (dst >= SIPC_ID_NR || !ws_data)
		return 0;

	mutex_lock(&ws_data->wakeup_info_mutex);
	list_for_each_entry_safe(pos, tmp, &ws_data->wakeup_info_lists[dst], list) {
		if (!((virq == pos->virq && virq > 0) ||
			(virq < 0 && channel == pos->channel)))
			continue;
		pos->wakeup_cnt++;
		SPRD_PDBG_DBG("ws_add_old: [%d %u %u %u]\n", virq, dst, channel, pos->wakeup_cnt);
		mutex_unlock(&ws_data->wakeup_info_mutex);
		return 0;
	}
	mutex_unlock(&ws_data->wakeup_info_mutex);

	ws = kzalloc(sizeof(struct wakeup_info), GFP_KERNEL);
	if (!ws) {
		SPRD_PDBG_ERR("%s: wakeup_info alloc error\n", __func__);
		return -ENOMEM;
	}

	ws->dst = dst;
	ws->virq = virq;
	ws->channel = channel;
	ws->wakeup_cnt++;
	SPRD_PDBG_DBG("ws_add_new: [%d %u %u %u]\n", ws->virq, ws->dst, channel, ws->wakeup_cnt);

	mutex_lock(&ws_data->wakeup_info_mutex);
	list_add_tail(&ws->list, &ws_data->wakeup_info_lists[dst]);
	mutex_unlock(&ws_data->wakeup_info_mutex);

	return 0;
}

static void wakeup_info_add_work(struct work_struct *work)
{
	struct wakeup_info ws_add;
	int ret;
	struct wakeup_info_data *ws_data =
		container_of(work, struct wakeup_info_data, ws_update_work.work);

	__pm_stay_awake(ws_data->ws_update);
	while (!kfifo_is_empty(&ws_data->ws_fifo)) {
		ret = kfifo_out(&ws_data->ws_fifo, &ws_add, sizeof(struct wakeup_info));
		if (ret)
			wakeup_info_add(ws_data, &ws_add);
	}
	__pm_relax(ws_data->ws_update);
}

static size_t wakeup_info_list_show(struct wakeup_info_data *ws_data, char *buf)
{
	u32 dst, num = 0;
	struct wakeup_info *pos, *tmp;
	struct list_head *dst_list;
	struct irq_desc *desc;
	char smsg[SMSG_BUF_MAX];
	struct rtc_time ws_record_end;

	if (!ws_data)
		return 0;

	sprd_pdbg_time_get(&ws_record_end);

	mutex_lock(&ws_data->wakeup_info_mutex);

	dst_list = &ws_data->wakeup_info_lists[SIPC_ID_AP];
	num += scnprintf(buf + num, WS_BUF_SIZE - num, "%s", "WAKEUP INFO: ");
	num += scnprintf(buf + num, WS_BUF_SIZE - num, TIME_FOMATE,
		ws_data->ws_record_start.tm_year + 1900, ws_data->ws_record_start.tm_mon + 1,
		ws_data->ws_record_start.tm_mday, ws_data->ws_record_start.tm_hour,
		ws_data->ws_record_start.tm_min, ws_data->ws_record_start.tm_sec,
		ws_record_end.tm_year + 1900, ws_record_end.tm_mon + 1,
		ws_record_end.tm_mday, ws_record_end.tm_hour,
		ws_record_end.tm_min, ws_record_end.tm_sec);

	num += scnprintf(buf + num, WS_BUF_SIZE - num,
		"%16s %32s %16s\n", "VIRQ(%d)", "ACT(%s)", "WS_CNT(%d)");
	list_for_each_entry_safe(pos, tmp, dst_list, list) {
		desc = irq_to_desc(pos->virq);
		num += scnprintf(buf + num, WS_BUF_SIZE - num, "%16u %32s %16u\n", pos->virq,
			(desc && desc->action) ? desc->action->name : "unknown", pos->wakeup_cnt);
	}

	num += scnprintf(buf + num, WS_BUF_SIZE - num, "%s\n", "MAILBOX WS DETAIL:");
	num += scnprintf(buf + num, WS_BUF_SIZE - num, "%16s %16s\n", "SMSG(%s)", "WS_CNT(%d)");
	for (dst = 1; dst < SIPC_ID_NR ; dst++) {
		dst_list = &ws_data->wakeup_info_lists[dst];
		if (list_empty(dst_list))
			continue;
		list_for_each_entry_safe(pos, tmp, dst_list, list) {
			scnprintf(smsg, SMSG_BUF_MAX, "smsg-%u-%u", dst, pos->channel);
			num += scnprintf(buf + num, WS_BUF_SIZE - num, "%16s %16u\n",
					 smsg, pos->wakeup_cnt);
		}
	}

	mutex_unlock(&ws_data->wakeup_info_mutex);

	return num;
}

static ssize_t ws_info_read(struct file *file, char __user *in_buf, size_t count, loff_t *ppos)
{
	char *out_buf;
	size_t len, out_len;
	struct wakeup_info_data *ws_data = PDE_DATA(file_inode(file));

	out_buf = kzalloc(sizeof(char) * WS_BUF_SIZE, GFP_KERNEL);
	if (!out_buf) {
		SPRD_PDBG_ERR("%s: out_buf alloc error\n", __func__);
		return -ENOMEM;
	}

	len = wakeup_info_list_show(ws_data, out_buf);
	out_len = simple_read_from_buffer(in_buf, count, ppos, out_buf, len);

	kfree(out_buf);

	return out_len;
}

static ssize_t ws_info_write(struct file *file, const char __user *user_buf, size_t count,
			     loff_t *ppos)
{
	int ret;
	u32 val;
	struct wakeup_info_data *ws_data = PDE_DATA(file_inode(file));

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtouint_from_user(user_buf, count, 10, &val);
	if (ret)
		return -EINVAL;

	if (!val) {
		sprd_pdbg_time_get(&ws_data->ws_record_start);
		wakeup_info_list_clear(ws_data);
	}

	return count;
}

static const struct proc_ops ws_info_fops = {
	.proc_open	= simple_open,
	.proc_read	= ws_info_read,
	.proc_write	= ws_info_write,
	.proc_lseek	= default_llseek,
};

static int ws_proc_init(struct wakeup_info_data *data, struct proc_dir_entry *dir)
{
	struct proc_dir_entry *fle;

	fle = proc_create_data("wakeup_info", 0644, dir, &ws_info_fops, data);
	if (!fle) {
		SPRD_PDBG_ERR("Proc ws_info_fops  file create failed\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_pdbg_ws_show(struct wakeup_info_data *data)
{
	u32 major, domain_id, hwirq;
	int ret = 0;

	if (!data) {
		SPRD_PDBG_ERR("%s: wakeup_info_data is NULL\n", __func__);
		return -EINVAL;
	}

	if (sprd_pdbg_ws_info_trans(&major, &domain_id, &hwirq)) {
		SPRD_PDBG_ERR("sprd_pdbg_ws_info_trans error\n");
		return -EINVAL;
	}

	/**
	 * The interface is called in the process of entering the suspend, and
	 * if the entry into the suspend fails, the wake-up source
	 * cannot be obtained.
	 */
	if (!major) {
		SPRD_PDBG_ERR("The system has not yet entered sleep mode\n");
		return 0;
	}

	ret = ws_parse(data, major, domain_id, hwirq, data->log_buf);
	SPRD_PDBG_INFO("%s\n", data->log_buf);

	return ret;
}

static void sprd_ws_info_update(int virq, u8 dst, u8 channel)
{
	struct wakeup_info_data *ws_data = ws_inst_get();
	struct wakeup_info ws_add;

	if (!ws_data)
		return;

	ws_add.virq = virq;
	ws_add.dst = dst;
	ws_add.channel = channel;
	kfifo_in_spinlocked(&ws_data->ws_fifo, &ws_add, sizeof(struct wakeup_info),
		&ws_data->kfifo_in_lock);
}

static void ws_mailbox_get(void)
{
	u32 smsg_info = 0xffff;
	u8 dst, channel;

	pdbg_notifier_call_chain(PDBG_NB_WS_UPDATE, &smsg_info);

	dst = (smsg_info & 0xff);
	channel = ((smsg_info >> 8) & 0xff);

	if (dst != INVALID_DATA && channel != INVALID_DATA)
		sprd_ws_info_update(-1, dst, channel);
}

static void ws_notify_handler(void *data, unsigned long cmd)
{
	ktime_t sleep_time, total_time, suspend_resume_time;
	u64 suspend_time_ms;
	struct wakeup_info_data *ws_data = data;

	switch (cmd) {
	case SPRD_CPU_PM_EXIT:
		sprd_pdbg_ws_show(ws_data);
		break;
	case SPRD_PM_ENTER:
		/* monotonic time since boot */
		ws_data->last_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		ws_data->last_stime = ktime_get_boottime();
		break;
	case SPRD_PM_EXIT:
		ws_mailbox_get();
		queue_delayed_work(system_highpri_wq, &ws_data->ws_update_work, 0);
		/* monotonic time since boot */
		ws_data->curr_monotime = ktime_get();
		 /* monotonic time since boot including the time spent in suspend */
		ws_data->curr_stime = ktime_get_boottime();
		total_time = ktime_sub(ws_data->curr_stime, ws_data->last_stime);
		suspend_resume_time = ktime_sub(ws_data->curr_monotime, ws_data->last_monotime);
		sleep_time = ktime_sub(total_time, suspend_resume_time);
		suspend_time_ms = ktime_to_ms(sleep_time);
		SPRD_PDBG_INFO("kernel suspend %llums\n", suspend_time_ms);
		break;
	case SPRD_PM_MONITOR:
		sprd_pdbg_kernel_active_ws_show();
		break;
	default:
		break;
	}
}

static void ws_info_list_init(struct wakeup_info_data *ws_data)
{
	int i;

	for (i = 0; i < SIPC_ID_NR; i++)
		INIT_LIST_HEAD(&ws_data->wakeup_info_lists[i]);
}

static void wakeup_info_list_clear(struct wakeup_info_data *ws_data)
{
	u32 dst;
	struct wakeup_info *pos, *tmp;
	struct list_head *dst_list;

	if (!ws_data)
		return;

	mutex_lock(&ws_data->wakeup_info_mutex);
	for (dst = 0; dst < SIPC_ID_NR ; dst++) {
		dst_list = &ws_data->wakeup_info_lists[dst];
		if (list_empty(dst_list))
			continue;
		list_for_each_entry_safe(pos, tmp, dst_list, list) {
			list_del(&pos->list);
			kfree(pos);
		}
	}
	mutex_unlock(&ws_data->wakeup_info_mutex);
}

static void info_work_devm_action0(void *_data)
{
	struct wakeup_info_data *ws_data = _data;

	wakeup_source_unregister(ws_data->ws_update);
}

static void info_work_devm_action1(void *_data)
{
	struct wakeup_info_data *ws_data = _data;

	cancel_delayed_work_sync(&ws_data->ws_update_work);
	wakeup_info_list_clear(ws_data);
	kfifo_free(&ws_data->ws_fifo);
	mutex_destroy(&ws_data->wakeup_info_mutex);
}

static int sprd_pdbg_info_work_init(struct device *dev, struct wakeup_info_data *ws_data)
{
	int ret;

	ws_data->ws_update = wakeup_source_register(NULL, "ws_update");
	if (!ws_data->ws_update) {
		SPRD_PDBG_ERR("wakeup_source_register err!");
		return -EBUSY;
	}

	ret = devm_add_action(dev, info_work_devm_action0, ws_data);
	if (ret) {
		info_work_devm_action0(ws_data);
		SPRD_PDBG_ERR("failed to add info_work_devm_action0\n");
		return ret;
	}

	spin_lock_init(&ws_data->kfifo_in_lock);
	ret = kfifo_alloc(&ws_data->ws_fifo, sizeof(struct wakeup_info) * 16, GFP_KERNEL);
	if (ret) {
		SPRD_PDBG_ERR("alloc kfifo fail\n");
		return ret;
	}

	mutex_init(&ws_data->wakeup_info_mutex);
	ws_info_list_init(ws_data);
	INIT_DELAYED_WORK(&ws_data->ws_update_work, wakeup_info_add_work);
	sprd_pdbg_time_get(&ws_data->ws_record_start);

	ret = devm_add_action(dev, info_work_devm_action1, ws_data);
	if (ret) {
		info_work_devm_action1(ws_data);
		SPRD_PDBG_ERR("failed to add info_work_devm_action1\n");
		return ret;
	}

	return 0;
}

static int irq_domain_release(struct wakeup_info_data *ws_data)
{
	struct ws_irq_domain *pos, *tmp;

	write_lock(&ws_data->rw_lock);
	list_for_each_entry_safe(pos, tmp, &ws_data->ws_irq_domain_list, list) {
		list_del(&pos->list);
		kfree(pos);
	}
	write_unlock(&ws_data->rw_lock);

	return 0;
}

static void irq_domain_devm_action(void *_data)
{
	struct wakeup_info_data *ws_data = _data;

	cancel_delayed_work_sync(&ws_data->irq_domain_work);
	irq_domain_release(ws_data);
}

static int sprd_pdbg_irq_domain_init(struct device *dev, struct wakeup_info_data *ws_data)
{
	int ret;

	INIT_LIST_HEAD(&ws_data->ws_irq_domain_list);
	rwlock_init(&ws_data->rw_lock);

	INIT_DELAYED_WORK(&ws_data->irq_domain_work, irq_domain_parse_work);
	dt_irq_domain_names_get(dev, ws_data);
	ws_data->last_nr_irqs = 0;
	ws_data->irq_domain_retry_cnt = 0;
	schedule_delayed_work(&ws_data->irq_domain_work, 0);

	ret = devm_add_action(dev, irq_domain_devm_action, ws_data);
	if (ret) {
		irq_domain_devm_action(ws_data);
		SPRD_PDBG_ERR("failed to add irq_domain_devm_action\n");
		return ret;
	}

	return 0;
}

int sprd_pdbg_ws_info_init(struct device *dev, struct proc_dir_entry *dir,
			   struct wakeup_info_data **data)
{
	struct wakeup_info_data *ws_data;
	int ret;

	ws_data = devm_kzalloc(dev, sizeof(struct wakeup_info_data), GFP_KERNEL);
	if (!ws_data) {
		SPRD_PDBG_ERR("%s: ws_data alloc error\n", __func__);
		return -ENOMEM;
	}
	g_ws_data = *data = ws_data;

	ret = ws_proc_init(ws_data, dir);
	if (ret) {
		SPRD_PDBG_ERR("%s: ws_proc_init error\n", __func__);
		return ret;
	}

	ws_data->notify_cb = ws_notify_handler;

	ret = sprd_pdbg_irq_domain_init(dev, ws_data);
	if (ret) {
		SPRD_PDBG_ERR("%s: irq_domain_init error\n", __func__);
		return ret;
	}

	ret = sprd_pdbg_info_work_init(dev, ws_data);
	if (ret) {
		SPRD_PDBG_ERR("%s: info_work_init error\n", __func__);
		return ret;
	}

	return 0;
}

