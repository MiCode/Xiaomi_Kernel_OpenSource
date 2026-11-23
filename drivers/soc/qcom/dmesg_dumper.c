// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kmsg_dump.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/skbuff.h>
#include <linux/suspend.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/gunyah/gh_dbl.h>
#include <linux/gunyah/gh_panic_notifier.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_vm.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <soc/qcom/secure_buffer.h>

#include "dmesg_dumper_private.h"

#define DDUMP_DBL_MASK				0x1
#define DDUMP_PROFS_NAME			"vmkmsg"
#define DDUMP_WAIT_WAKEIRQ_TIMEOUT	msecs_to_jiffies(1000)
#define TIME_PREFIX_LEN				14

static void qcom_ddump_to_shm(struct kmsg_dumper *dumper,
			  struct kmsg_dump_detail *detail)
{
	struct qcom_dmesg_dumper *qdd = container_of(dumper,
					struct qcom_dmesg_dumper, dump);
	size_t len;

	dev_warn(qdd->dev, "detail->reason = %d\n", detail->reason);
	kmsg_dump_rewind(&qdd->iter);
	memset(qdd->base, 0, qdd->size);
	kmsg_dump_get_buffer(&qdd->iter, false, qdd->base, qdd->size, &len);
	dev_warn(qdd->dev, "size of dmesg logbuf logged = %zu\n", len);
}

static int qcom_ddump_gh_panic_handler(struct notifier_block *nb,
				 unsigned long cmd, void *data)
{
	struct qcom_dmesg_dumper *qdd;
	struct kmsg_dump_detail detail = {
		.reason = KMSG_DUMP_PANIC,
		.description = NULL};

	qdd = container_of(nb, struct qcom_dmesg_dumper, gh_panic_nb);
	qcom_ddump_to_shm(&qdd->dump, &detail);

	return NOTIFY_DONE;
}

static struct device_node *qcom_ddump_svm_of_parse(struct qcom_dmesg_dumper *qdd)
{
	const char *compat = "qcom,ddump-gunyah-gen";
	struct device_node *np = NULL;
	u32 label;
	int ret;

	while ((np = of_find_compatible_node(np, NULL, compat))) {
		ret = of_property_read_u32(np, "qcom,label", &label);
		if (ret) {
			of_node_put(np);
			continue;
		}
		if (label == qdd->label)
			break;

		of_node_put(np);
	}

	return np;
}

static int qcom_ddump_map_memory(struct qcom_dmesg_dumper *qdd)
{
	struct device *dev = qdd->dev;
	struct device_node *shm_np;
	struct device_node *np;
	u32 size = 0;
	int ret;

	np = dev->of_node;
	if (!qdd->primary_vm) {
		np = qcom_ddump_svm_of_parse(qdd);
		if (!np) {
			dev_err(dev, "Unable to parse shared mem node\n");
			return -EINVAL;
		}
	}

	shm_np = of_parse_phandle(np, "memory-region", 0);
	if (!shm_np)
		return -EINVAL;

	ret = of_address_to_resource(shm_np, 0, &qdd->res);
	of_node_put(shm_np);
	if (!qdd->primary_vm)
		of_node_put(np);

	if (!ret) {
		qdd->is_static = true;
		qdd->size = resource_size(&qdd->res);
		return ret;
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(dev, "%s: dma_set_mask_and_coherent failed\n", __func__);
		return ret;
	}

	ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);
	if (ret) {
		dev_err(dev, "%s: Failed to initialize CMA mem, ret %d\n",
			__func__, ret);
		return ret;
	}

	ret = of_property_read_u32(qdd->dev->of_node, "shared-buffer-size", &size);
	if (ret) {
		dev_err(dev, "%s: Failed to get shared memory size, ret %d\n",
					__func__, ret);
		return ret;
	}

	qdd->size = size;
	qdd->is_static = false;
	return 0;
}

static int qcom_ddump_share_mem(struct qcom_dmesg_dumper *qdd, gh_vmid_t self,
			   gh_vmid_t peer)
{
	struct qcom_scm_vmperm dst_vmlist[] = {{self, PERM_READ | PERM_WRITE},
						{peer, PERM_READ | PERM_WRITE}};
	struct qcom_scm_vmperm src_vmlist[] = {{self,
					   PERM_READ | PERM_WRITE | PERM_EXEC}};
	u64 dst_vmid = BIT(dst_vmlist[0].vmid) | BIT(dst_vmlist[1].vmid);
	u64 src_vmid = BIT(src_vmlist[0].vmid);
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;
	int ret, assign_mem_ret;

	ret = qcom_scm_assign_mem(qdd->res.start, resource_size(&qdd->res),
			     &src_vmid, dst_vmlist, ARRAY_SIZE(dst_vmlist));
	if (ret) {
		dev_err(qdd->dev, "qcom_scm_assign_mem addr=%llx size=%llu failed: %d\n",
		       qdd->res.start, qdd->size, ret);
		return ret;
	}

	acl = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]), GFP_KERNEL);
	if (!acl)
		return -ENOMEM;
	sgl = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[1]), GFP_KERNEL);
	if (!sgl) {
		kfree(acl);
		return -ENOMEM;
	}
	acl->n_acl_entries = 2;
	acl->acl_entries[0].vmid = (u16)self;
	acl->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;
	acl->acl_entries[1].vmid = (u16)peer;
	acl->acl_entries[1].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	sgl->n_sgl_entries = 1;
	sgl->sgl_entries[0].ipa_base = qdd->res.start;
	sgl->sgl_entries[0].size = resource_size(&qdd->res);

	ret = ghd_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, qdd->label,
			      acl, sgl, NULL, &qdd->memparcel);
	if (ret) {
		dev_err(qdd->dev, "Gunyah mem share addr=%llx size=%llu failed: %d\n",
		       qdd->res.start, qdd->size, ret);
		/* Attempt to give resource back to HLOS */
		assign_mem_ret = qcom_scm_assign_mem(qdd->res.start, resource_size(&qdd->res),
				&dst_vmid, src_vmlist, ARRAY_SIZE(src_vmlist));
		if (assign_mem_ret) {
			dev_err(qdd->dev, "qcom_scm_assign_mem addr=%llx size=%llu failed: %d\n",
				qdd->res.start, qdd->size, ret);
		}
	}

	kfree(acl);
	kfree(sgl);

	return ret;
}

static int qcom_ddump_unshare_mem(struct qcom_dmesg_dumper *qdd, gh_vmid_t self,
			      gh_vmid_t peer)
{
	struct qcom_scm_vmperm dst_vmlist[] = {{self,
					       PERM_READ | PERM_WRITE | PERM_EXEC}};
	u64 src_vmid = BIT(self) | BIT(peer);
	int ret;

	ret = ghd_rm_mem_reclaim(qdd->memparcel, 0);
	if (ret)
		dev_err(qdd->dev, "Gunyah mem reclaim failed: %d\n", ret);

	ret = qcom_scm_assign_mem(qdd->res.start, resource_size(&qdd->res),
			&src_vmid, dst_vmlist, ARRAY_SIZE(dst_vmlist));
	if (ret)
		dev_err(qdd->dev, "unshare mem assign call failed with %d\n",
			ret);

	return ret;
}

static int qcom_ddump_vm_cb(struct notifier_block *nb, unsigned long cmd,
			     void *data)
{
	struct qcom_dmesg_dumper *qdd = container_of(nb, struct qcom_dmesg_dumper, vm_nb);
	dma_addr_t dma_handle;
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;
	gh_vmid_t vmid;
	int ret;

	if (!data)
		return NOTIFY_DONE;
	vmid = *((gh_vmid_t *)data);

	if (ghd_rm_get_vmid(qdd->peer_name, &peer_vmid))
		return NOTIFY_DONE;
	if (ghd_rm_get_vmid(GH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;
	if (peer_vmid != vmid)
		return NOTIFY_DONE;

	switch (cmd) {
	case GH_VM_BEFORE_POWERUP:
		if (qdd->is_ready)
			break;

		if (!qdd->is_static) {
			qdd->base = dma_alloc_coherent(qdd->dev, qdd->size,
							&dma_handle, GFP_KERNEL);
			if (!qdd->base)
				return NOTIFY_DONE;

			qdd->res.start = dma_to_phys(qdd->dev, dma_handle);
			qdd->res.end = qdd->res.start + qdd->size - 1;
		}

		strscpy(qdd->md_entry.name, "VM_LOG", sizeof(qdd->md_entry.name));
		qdd->md_entry.virt_addr = (uintptr_t)qdd->base;
		qdd->md_entry.phys_addr = qdd->res.start;
		qdd->md_entry.size = qdd->size;
		ret = msm_minidump_add_region(&qdd->md_entry);
		if (ret < 0)
			dev_err(qdd->dev, "Failed to add vm log entry in minidump table %d\n", ret);

		if (qcom_ddump_share_mem(qdd, self_vmid, peer_vmid)) {
			dev_err(qdd->dev, "Failed to share memory\n");
			if (!qdd->is_static)
				dma_free_coherent(qdd->dev, qdd->size, qdd->base,
					phys_to_dma(qdd->dev, qdd->res.start));

			return NOTIFY_DONE;
		}

		qdd->is_ready = true;
		break;
	case GH_VM_POWERUP_FAIL:
		fallthrough;
	case GH_VM_EARLY_POWEROFF:
		if (qdd->is_ready) {
			qdd->is_ready = false;
			memset(qdd->rec_time, 0,
					sizeof(qdd->rec_time[0]) * REC_TIME_NUM);
			msm_minidump_remove_region(&qdd->md_entry);
			if (!qcom_ddump_unshare_mem(qdd, self_vmid, peer_vmid)) {
				if (!qdd->is_static)
					dma_free_coherent(qdd->dev, qdd->size, qdd->base,
						phys_to_dma(qdd->dev, qdd->res.start));
			}
		}

		break;
	}

	return NOTIFY_DONE;
}

static inline int qcom_ddump_gh_kick(struct qcom_dmesg_dumper *qdd)
{
	gh_dbl_flags_t dbl_mask = DDUMP_DBL_MASK;
	int ret;

	ret = gh_dbl_send(qdd->tx_dbl, &dbl_mask, GH_DBL_NONBLOCK);
	if (ret)
		dev_err(qdd->dev, "failed to raise virq to the sender %d\n", ret);

	return ret;
}

static void qcom_ddump_gh_cb(int irq, void *data)
{
	gh_dbl_flags_t dbl_mask = DDUMP_DBL_MASK;
	struct qcom_dmesg_dumper *qdd;
	struct ddump_shm_hdr *hdr;
	int ret;

	qdd = data;
	hdr = qdd->base;
	gh_dbl_read_and_clean(qdd->rx_dbl, &dbl_mask, GH_DBL_NONBLOCK);

	if (qdd->primary_vm) {
		complete(&qdd->ddump_completion);
	} else {
		/* avoid system enter suspend */
		pm_wakeup_ws_event(qdd->wakeup_source, 2000, true);
		ret = qcom_ddump_alive_log_to_shm(qdd, hdr->user_buf_len);
		if (ret)
			dev_err(qdd->dev, "dump alive log error %d\n", ret);

		qcom_ddump_gh_kick(qdd);
		if (hdr->svm_dump_len == 0) {
			kmsg_dump_rewind(&qdd->iter);
			pm_wakeup_ws_event(qdd->wakeup_source, 0, true);
		}
	}
}

static u64 get_pvm_svm_offset(struct record_time *record_time, u64 ktime_ns)
{
	int i;

	for (i = 1; i < REC_TIME_NUM; i++) {
		if (ktime_ns < record_time[i].ns)
			break;
	}

	return record_time[i - 1].pvm_svm_ofs;
}

static void get_time_prefix(struct record_time *record_time, unsigned long sec,
		unsigned long usec, char *time_prefix)
{
	unsigned long rem_nsec, rem_usec;
	u64 offset1, offset2;
	u64 ktime_ns;

	ktime_ns = sec * NSEC_PER_SEC + usec * NSEC_PER_USEC;
	if (ktime_ns < record_time[0].ns) {
		memcpy(time_prefix, "[Invalid Time]", TIME_PREFIX_LEN);
	} else {
		offset1 = get_pvm_svm_offset(record_time, ktime_ns);
		rem_nsec = do_div(offset1, NSEC_PER_SEC);
		offset2 = (rem_nsec / NSEC_PER_USEC) + usec;
		rem_usec = do_div(offset2, USEC_PER_SEC);
		snprintf(time_prefix, TIME_PREFIX_LEN + 1, "[%5lu.%06lu]",
					(unsigned long)(offset1 + sec + offset2), rem_usec);
	}
}

static ssize_t vmkmsg_with_pvm_ktime_prefix(struct qcom_dmesg_dumper *qdd,
			char __user *user_buf, size_t count)
{
	u32 line_len = 0, total_len = 0, pre_total_len = 0;
	unsigned long sec, usec, buf_size;
	struct ddump_shm_hdr *hdr;
	char time_prefix[16];
	char *line_head, *p;
	ssize_t ret;
	void *buf;
	int i;

	hdr = qdd->base;

	/**
	 * svm only use the minimum between share memory size
	 * and user buffer size to storage log. So svm dmesg log
	 * with pvm ktime prefix will always less than the
	 * minimum * 2.
	 */
	buf_size = min(qdd->size, hdr->user_buf_len);
	buf = vmalloc(buf_size * 2);
	if (!buf)
		return -ENOMEM;

	line_head = (char *)&hdr->data;
	for (i = 0, p = line_head; i < hdr->svm_dump_len; i++, p++) {
		if (*p != '\n') {
			line_len++;
			continue;
		} else {
			if (sscanf(line_head, "[%5lu.%06lu]", &sec, &usec) != 2) {
				ret = -EINVAL;
				goto vfree_buf;
			}

			pre_total_len = total_len + TIME_PREFIX_LEN + line_len + 1;
			if (unlikely(buf_size < pre_total_len)) {
				ret = -EINVAL;
				goto vfree_buf;
			}

			get_time_prefix(qdd->rec_time, sec, usec, time_prefix);
			memcpy(buf + total_len, time_prefix, TIME_PREFIX_LEN);
			memcpy(buf + total_len + TIME_PREFIX_LEN, line_head, line_len + 1);
			total_len = pre_total_len;
			line_len = 0;
			line_head = p + 1;
		}
	}

	if (unlikely(total_len > count)) {
		ret = -EINVAL;
		goto vfree_buf;
	}

	if (hdr->svm_dump_len &&
		copy_to_user(user_buf, buf, total_len)) {
		pr_err("copy_to_user fail\n");
		ret = -EFAULT;
		goto vfree_buf;
	}

	ret = total_len;

vfree_buf:
	vfree(buf);
	return ret;
}

static void reverse_rec_time(struct record_time *rec_time)
{
	int i;

	for (i = REC_TIME_NUM - 1; i > 0; i--) {
		rec_time[i].ns = rec_time[i - 1].ns;
		rec_time[i].pvm_svm_ofs = rec_time[i - 1].pvm_svm_ofs;
	}
}

static void update_rec_time(struct record_time *rec_time)
{
	int i;

	for (i = 1; i < REC_TIME_NUM; i++) {
		rec_time[i-1].ns = rec_time[i].ns;
		rec_time[i-1].pvm_svm_ofs = rec_time[i].pvm_svm_ofs;
	}
}

static void pvm_update_record_time(struct record_time *rec_time,
		struct gh_virt_time_offset *time_ofs)
{
	struct gh_virt_time_offset cur_time_ofs;
	u64 ofs, ns, ns_bu, ofs_bu;
	int i, retry_cnt = 0;

	gh_get_virt_time_offset(&cur_time_ofs);
	for (i = 0; i < REC_TIME_NUM && retry_cnt < DDUMP_MAX_RETRY; i++) {
		/**
		 * rec_time[REC_TIME_NUM - 1].ns is always maximum
		 * need update rec time arrays in two case
		 * 1. when time_ofs[i].ns is greater than maximum in rec_time
		 *    arrays
		 * 2. when time_ofs[i].offset != 0 and the maximun in rec_time
		 *    is 0, means the first update not be called and need do
		 *    update for the first update
		 */
		if (time_ofs[i].ns > rec_time[REC_TIME_NUM - 1].ns ||
			(!rec_time[REC_TIME_NUM - 1].pvm_svm_ofs &&
			time_ofs[i].offset)) {

			/* record the value of time_ofs[i].ns before do update */
			ns = time_ofs[i].ns;

			/* back up the minimum ns and pvm_svm_ofs in rec_time arrays */
			ns_bu = rec_time[0].ns;
			ofs_bu = rec_time[0].pvm_svm_ofs;

			/* update the rec_time arrays, sort by ns from smallest to largest */
			update_rec_time(rec_time);
			rec_time[REC_TIME_NUM - 1].ns = time_ofs[i].ns;
			ofs = time_ofs[i].offset - cur_time_ofs.offset;
			rec_time[REC_TIME_NUM - 1].pvm_svm_ofs = ofs;

			/**
			 * compare the value of time_ofs[i].ns before do update
			 * and after do update. if has been changed, means svm
			 * has updated time_ofs arrays at the same time. so need
			 * do reverse and retry update to keep the data in
			 * time_ofs arrays not be changed when do update rec_time
			 * arrays
			 */
			if (unlikely(ns != time_ofs[i].ns)) {
				reverse_rec_time(rec_time);
				rec_time[0].ns = ns_bu;
				rec_time[0].pvm_svm_ofs = ofs_bu;
				retry_cnt++;
				i = -1;
				continue;
			}

			pr_info("svm ktime=%llu, pvm_svm_offset=%llu\n",
					rec_time[REC_TIME_NUM - 1].ns, ofs);
		}
	}
}

static ssize_t qcom_ddump_vmkmsg_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct qcom_dmesg_dumper *qdd = pde_data(file_inode(file));
	struct ddump_shm_hdr *hdr;
	int ret;
	static bool fresh_read = true;
	static bool last_dump = true;
	char dump_status[512] = {0};
	int len = 0;

	if (!qdd->is_ready)
		return -ENODEV;

	if (count < LOG_LINE_MAX) {
		dev_err(qdd->dev, "user buffer size should greater than %d\n", LOG_LINE_MAX);
		return -EINVAL;
	}
	/* fresh_read::false means last read has some wrong, so return directly */
	if (!fresh_read) {
		fresh_read = true;
		last_dump = true;
		return 0;
	}
	hdr = qdd->base;
	if (last_dump) {
		last_dump = false;
		len += sprintf(dump_status + len, "[last read vmkmsg info]\n");
		len += sprintf(dump_status + len, "\tpvm timing: %llu.%llu s\n",
				hdr->pvm_read_timing/1000000000ULL, hdr->pvm_read_timing%1000000000ULL);
		len += sprintf(dump_status + len, "\tread len: %llu, svm_suspend: %d\n", hdr->pvm_read_len, hdr->pvm_read_suspend);
		len += sprintf(dump_status + len, "[svm info]\n");
		len += sprintf(dump_status + len, "\tsvm suspend timing: %llu.%llu s\n\n",
				hdr->svm_suspend_timing/1000000000ULL, hdr->svm_suspend_timing%1000000000ULL);
		(void)copy_to_user(buf, dump_status, len);
		hdr->pvm_read_timing = sched_clock();
		hdr->pvm_read_len = 0;
		hdr->pvm_read_suspend = hdr->svm_is_suspend;
		return len;
	}

	/**
	 * If SVM is in suspend mode and the log size more than 1k byte,
	 * we think SVM has log need to be read. Otherwise, we think the
	 * log is only suspend log that we need skip the unnecessary log.
	 */
	if (hdr->svm_is_suspend && hdr->svm_dump_len < 1024) {
		if (fresh_read) {
			fresh_read = false;
			len += sprintf(dump_status + len, "\n\n[current read vmkmsg info]\n");
			len += sprintf(dump_status + len, "[skip read as svm is in suspend and log size less than 1k]\n");
			(void)copy_to_user(buf, dump_status, len);
			hdr->pvm_read_suspend = true;
			hdr->pvm_read_len = hdr->svm_dump_len;
			return len;
		}
		return 0;
	}

	if (hdr->log_is_encrypted)
		hdr->user_buf_len = count;
	else
		/**
		 * If log is not encrypted, we will add pvm ktime prefix for
		 * each line of svm dmesg. For the worst case, the each svm
		 * dmesg print null except kernel log timestamp. So avoid the
		 * overflow, we set usr_buf_len as count / 2.
		 */
		hdr->user_buf_len = count / 2;

	ret = qcom_ddump_gh_kick(qdd);
	if (ret) {
		if (fresh_read) {
			fresh_read = false;
			len += sprintf(dump_status + len, "\n\n[current read vmkmsg info]\n");
			len += sprintf(dump_status + len, " [gh dbl kick failed with %d, svm might abnormal\n", ret);
			(void)copy_to_user(buf, dump_status, len);
			return len;
		}
		return ret;
	}

	ret = wait_for_completion_timeout(&qdd->ddump_completion, DDUMP_WAIT_WAKEIRQ_TIMEOUT);
	if (!ret) {
		if (fresh_read) {
			fresh_read = false;
			len += sprintf(dump_status + len, "\n\n[current read vmkmsg info]\n");
			len += sprintf(dump_status + len, " [gh dbl wait timeout with %d, svm might abnormal\n", ret);
			(void)copy_to_user(buf, dump_status, len);
			return len;
		}
		dev_err(qdd->dev, "wait for completion timeout\n");
		return -ETIMEDOUT;
	}

	if (hdr->svm_dump_len > count) {
		dev_err(qdd->dev, "can not read the correct length of svm kmsg\n");
		return -EINVAL;
	}

	if (!hdr->log_is_encrypted) {
		pvm_update_record_time(qdd->rec_time, hdr->time_ofs);
		return vmkmsg_with_pvm_ktime_prefix(qdd, buf, count);
	}

	hdr->pvm_read_len += hdr->svm_dump_len;
	if (hdr->svm_dump_len &&
		copy_to_user(buf, &hdr->data, hdr->svm_dump_len)) {
		dev_err(qdd->dev, "copy_to_user fail\n");
		if (fresh_read) {
			fresh_read = false;
			len += sprintf(dump_status + len, "\n\n[current read vmkmsg info]\n");
			len += sprintf(dump_status + len, " [copy_to_user failed]\n");
			(void)copy_to_user(buf, dump_status, len);
			return len;
		}
		return -EFAULT;
	}
	if (hdr->svm_dump_len == 0) {
		/* read empty, update the read info */
		if (fresh_read) {
			fresh_read = false;
			len += sprintf(dump_status + len, "\n\n[current vmkmsg info]\n");
			len += sprintf(dump_status + len, " svm_dump_len: %llu\n", hdr->pvm_read_len);
			(void)copy_to_user(buf, dump_status, len);
			return len;
		}
	}
	return hdr->svm_dump_len;
}

static const struct proc_ops ddump_proc_ops = {
	.proc_flags	= PROC_ENTRY_PERMANENT,
	.proc_read	= qcom_ddump_vmkmsg_read,
};

static int qcom_ddump_alive_log_probe(struct qcom_dmesg_dumper *qdd)
{
	struct device_node *node = qdd->dev->of_node;
	struct gh_virt_time_offset cur_time_ofs;
	struct device *dev = qdd->dev;
	struct proc_dir_entry *dent;
	struct ddump_shm_hdr *hdr;
	enum gh_dbl_label dbl_label;
	struct resource *res;
	size_t shm_min_size;
	int ret;

	shm_min_size = LOG_LINE_MAX + DDUMP_GET_SHM_HDR;
	if (qdd->size < shm_min_size) {
		dev_err(dev, "Shared memory size should greater than %zu\n", shm_min_size);
		return -EINVAL;
	}

	if (qdd->primary_vm) {
		if (qdd->is_static) {
			res = devm_request_mem_region(dev, qdd->res.start,
					qdd->size, dev_name(dev));
			if (!res) {
				dev_err(dev, "request mem region fail\n");
				return -ENXIO;
			}

			qdd->base = devm_ioremap_wc(dev, qdd->res.start, qdd->size);
			if (!qdd->base) {
				dev_err(dev, "ioremap fail\n");
				return -ENOMEM;
			}
		}

		qdd->rec_time = devm_kzalloc(dev,
				sizeof(qdd->rec_time[0]) * REC_TIME_NUM, GFP_KERNEL);
		if (!qdd->rec_time)
			return -ENOMEM;

		init_completion(&qdd->ddump_completion);
		dent = proc_create_data(DDUMP_PROFS_NAME, 0444, NULL, &ddump_proc_ops, qdd);
		if (!dent) {
			dev_err(dev, "proc_create_data fail\n");
			return -ENOMEM;
		}
	} else {
		/* init shared memory header */
		hdr = qdd->base;
		hdr->svm_is_suspend = false;
		hdr->log_is_encrypted = false;
		hdr->svm_suspend_timing = 0;
		hdr->pvm_read_timing = 0;
		hdr->pvm_read_len = 0;
		hdr->pvm_read_suspend = false;

		gh_get_virt_time_offset(&cur_time_ofs);
		memset(hdr->time_ofs, 0,
				sizeof(hdr->time_ofs[0]) * REC_TIME_NUM);
		/**
		 * need update time_ofs arrays when vm boot up to
		 * handle the vm kmsg timestamp which less than
		 * the ktime of the first call resume callback
		 */
		hdr->time_ofs[REC_TIME_NUM - 1].offset = cur_time_ofs.offset;

		ret = qcom_ddump_encrypt_init(node, hdr);
		if (ret)
			return ret;

		qdd->wakeup_source = wakeup_source_register(dev, dev_name(dev));
		if (!qdd->wakeup_source)
			return -ENOMEM;

		qdd->gh_panic_nb.notifier_call = qcom_ddump_gh_panic_handler;
		qdd->vm_nb.priority = INT_MAX - 1;
		ret = gh_panic_notifier_register(&qdd->gh_panic_nb);
		if (ret)
			goto err_panic_notifier_register;
	}

	dbl_label = qdd->label;
	qdd->tx_dbl = gh_dbl_tx_register(dbl_label);
	if (IS_ERR_OR_NULL(qdd->tx_dbl)) {
		ret = PTR_ERR(qdd->tx_dbl);
		dev_err(dev, "%s:Failed to get gunyah tx dbl %d\n", __func__, ret);
		goto err_dbl_tx_register;
	}

	qdd->rx_dbl = gh_dbl_rx_register(dbl_label, qcom_ddump_gh_cb, qdd);
	if (IS_ERR_OR_NULL(qdd->rx_dbl)) {
		ret = PTR_ERR(qdd->rx_dbl);
		dev_err(dev, "%s:Failed to get gunyah rx dbl %d\n", __func__, ret);
		goto err_dbl_rx_register;
	}

	return 0;

err_dbl_rx_register:
	gh_dbl_tx_unregister(qdd->tx_dbl);
err_dbl_tx_register:
	if (qdd->primary_vm)
		remove_proc_entry(DDUMP_PROFS_NAME, NULL);
	else
		gh_panic_notifier_unregister(&qdd->gh_panic_nb);
err_panic_notifier_register:
	if (!qdd->primary_vm)
		wakeup_source_unregister(qdd->wakeup_source);

	return ret;
}

static int qcom_ddump_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct qcom_dmesg_dumper *qdd;
	struct device *dev;
	int ret;
	struct resource *res;

	qdd = devm_kzalloc(&pdev->dev, sizeof(*qdd), GFP_KERNEL);
	if (!qdd)
		return -ENOMEM;

	qdd->dev = &pdev->dev;
	platform_set_drvdata(pdev, qdd);

	dev = qdd->dev;
	ret = of_property_read_u32(node, "gunyah-label", &qdd->label);
	if (ret) {
		dev_err(dev, "Failed to read label %d\n", ret);
		return ret;
	}

	qdd->primary_vm = of_property_read_bool(node, "qcom,primary-vm");

	ret = qcom_ddump_map_memory(qdd);
	if (ret)
		return ret;

	if (qdd->primary_vm) {
		ret = of_property_read_u32(node, "peer-name", &qdd->peer_name);
		if (ret)
			qdd->peer_name = GH_SELF_VM;

		qdd->vm_nb.notifier_call = qcom_ddump_vm_cb;
		qdd->vm_nb.priority = INT_MAX;
		gh_register_vm_notifier(&qdd->vm_nb);
	} else {
		res = devm_request_mem_region(dev, qdd->res.start, qdd->size, dev_name(dev));
		if (!res) {
			dev_err(dev, "request mem region fail\n");
			return -ENXIO;
		}

		qdd->base = devm_ioremap_wc(dev, qdd->res.start, qdd->size);
		if (!qdd->base) {
			dev_err(dev, "ioremap fail\n");
			return -ENOMEM;
		}

		kmsg_dump_rewind(&qdd->iter);
		qdd->dump.dump = qcom_ddump_to_shm;
		ret = kmsg_dump_register(&qdd->dump);
		if (ret)
			return ret;
	}

	if (IS_ENABLED(CONFIG_QCOM_VM_ALIVE_LOG_DUMPER)) {
		ret = qcom_ddump_alive_log_probe(qdd);
		if (ret) {
			if (qdd->primary_vm)
				gh_unregister_vm_notifier(&qdd->vm_nb);
			else
				kmsg_dump_unregister(&qdd->dump);
			return ret;
		}
	}

	return 0;
}

static void qcom_ddump_remove(struct platform_device *pdev)
{
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;
	int ret;
	struct qcom_dmesg_dumper *qdd = platform_get_drvdata(pdev);

	if (IS_ENABLED(CONFIG_QCOM_VM_ALIVE_LOG_DUMPER)) {
		gh_dbl_tx_unregister(qdd->tx_dbl);
		gh_dbl_rx_unregister(qdd->rx_dbl);
		if (qdd->primary_vm) {
			remove_proc_entry(DDUMP_PROFS_NAME, NULL);
		} else {
			gh_panic_notifier_unregister(&qdd->gh_panic_nb);
			wakeup_source_unregister(qdd->wakeup_source);
			qcom_ddump_encrypt_exit();
		}
	}

	if (qdd->primary_vm) {
		gh_unregister_vm_notifier(&qdd->vm_nb);
		ret = ghd_rm_get_vmid(qdd->peer_name, &peer_vmid);
		if (ret)
			return;

		ret = ghd_rm_get_vmid(GH_PRIMARY_VM, &self_vmid);
		if (ret)
			return;

		ret = qcom_ddump_unshare_mem(qdd, self_vmid, peer_vmid);
		if (ret)
			return;

		if (!qdd->is_static)
			dma_free_coherent(qdd->dev, qdd->size, qdd->base,
				phys_to_dma(qdd->dev, qdd->res.start));
	} else {
		ret = kmsg_dump_unregister(&qdd->dump);
		if (ret)
			return;
	}
}

#if IS_ENABLED(CONFIG_PM_SLEEP) && \
	IS_ENABLED(CONFIG_QCOM_VM_ALIVE_LOG_DUMPER)
#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
static int qcom_ddump_suspend(struct device *pdev)
{
	struct qcom_dmesg_dumper *qdd = dev_get_drvdata(pdev);
	struct ddump_shm_hdr *hdr = qdd->base;
	u64 seq_backup;
	int ret;

	hdr->svm_is_suspend = true;
	hdr->svm_suspend_timing = sched_clock();
	mb();
	seq_backup = qdd->iter.cur_seq;
	ret = qcom_ddump_alive_log_to_shm(qdd, qdd->size);
	if (ret)
		dev_err(qdd->dev, "dump alive log error %d\n", ret);

	qdd->iter.cur_seq = seq_backup;
	mb();
	return 0;
}

static void svm_update_time_offset(struct gh_virt_time_offset *time_ofs)
{
	int i;

	for (i = 1; i < REC_TIME_NUM; i++) {
		time_ofs[i-1].ns = time_ofs[i].ns;
		time_ofs[i-1].offset = time_ofs[i].offset;
	}
}

static int qcom_ddump_resume(struct device *pdev)
{
	struct qcom_dmesg_dumper *qdd = dev_get_drvdata(pdev);
	struct gh_virt_time_offset cur_time_ofs, *time_ofs;
	struct ddump_shm_hdr *hdr = qdd->base;

	hdr->svm_is_suspend = false;
	time_ofs = hdr->time_ofs;
	gh_get_virt_time_offset(&cur_time_ofs);

	/* update the time_ofs arrays, sort by ns from smallest to largest */
	if (cur_time_ofs.ns > time_ofs[REC_TIME_NUM - 1].ns) {
		svm_update_time_offset(time_ofs);
		time_ofs[REC_TIME_NUM - 1].ns = cur_time_ofs.ns;
		time_ofs[REC_TIME_NUM - 1].offset = cur_time_ofs.offset;
		pr_info("svm ktime=%llu\n", cur_time_ofs.ns);
	}

	return 0;
}
#else
static int qcom_ddump_suspend(struct device *pdev)
{
	struct qcom_dmesg_dumper *qdd = dev_get_drvdata(pdev);
	struct ddump_shm_hdr *hdr = qdd->base;

	if (qdd->is_ready)
		pvm_update_record_time(qdd->rec_time, hdr->time_ofs);

	return 0;
}

static int qcom_ddump_resume(struct device *pdev)
{
	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(ddump_pm_ops, qcom_ddump_suspend, qcom_ddump_resume);
#endif

static const struct of_device_id ddump_match_table[] = {
	{ .compatible = "qcom,dmesg-dump" },
	{}
};

static struct platform_driver ddump_driver = {
	.driver = {
		.name = "qcom_dmesg_dumper",
#if IS_ENABLED(CONFIG_PM_SLEEP) && \
	IS_ENABLED(CONFIG_QCOM_VM_ALIVE_LOG_DUMPER)
		.pm = &ddump_pm_ops,
#endif
		.of_match_table = ddump_match_table,
	 },
	.probe = qcom_ddump_probe,
	.remove = qcom_ddump_remove,
};

static int __init qcom_ddump_init(void)
{
	return platform_driver_register(&ddump_driver);
}

#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
arch_initcall(qcom_ddump_init);
#else
module_init(qcom_ddump_init);
#endif

static __exit void qcom_ddump_exit(void)
{
	platform_driver_unregister(&ddump_driver);
}
module_exit(qcom_ddump_exit);

MODULE_DESCRIPTION("QTI Virtual Machine dmesg log buffer dumper");
MODULE_LICENSE("GPL");
