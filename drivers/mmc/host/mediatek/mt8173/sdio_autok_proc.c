/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <../drivers/mmc/core/core.h>
#include "mt_sd.h"
#include "sdio_autok.h"
/* #include "autok.h" */
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/debugfs.h>
#include <linux/mm.h>		/* mmap related stuff */
#include <linux/slab.h>		/* kmalloc/kfree/kzalloc */

#define AUTOK_THREAD
#define MAX_ARGS_BUF    2048
#define DEVNAME_SIZE 80
/* #define UT_TEST */

static struct kobject *autok_kobj;
/* stage1 kobjects */
static struct kobject *stage1_kobj;
static struct kobject **s1_host_kobj;
static struct attribute **stage1_attrs;
static struct kobj_attribute **stage1_kattr;
/* stage2 kobjects */
static struct kobject *stage2_kobj;
struct attribute **stage2_attrs;
struct kobj_attribute **stage2_kattr;
/* host kobjects */
struct attribute **host_attrs;
struct kobj_attribute **host_kattr;

struct dentry *autok_log_entry;
char *autok_log_info = NULL;


enum STAGE1_NODES {
	VOLTAGE,
	PARAMS,
	DONE,
	LOG,
	TOTAL_STAGE1_NODE_COUNT
};
const char stage1_nodes[][DEVNAME_SIZE] = { "VOLTAGE", "PARAMS", "DONE", "LOG" };

enum HOST_NODES {
	READY,
	DEBUG,
	PARAM_COUNT,
	SS_CORNER,
	SUGGEST_VOL,
	TOTAL_HOST_NODE_COUNT
};

const char host_nodes[][DEVNAME_SIZE] = {
	"ready", "debug", "param_count", "ss_corner", "suggest_vol"
};

#ifdef UT_TEST
u8 is_first_stage1 = 1;		/* UT usage */
#endif

#ifndef MTK_SDIO30_ONLINE_TUNING_SUPPORT
#define DMA_ON 0
#define DMA_OFF 1
#endif

struct autok_predata *p_autok_predata;
struct autok_predata *p_single_autok;

static int sdio_host_debug = 1;
static int is_full_log;
static unsigned int *suggest_vol;
static unsigned int suggest_vol_count;

#ifdef AUTOK_THREAD
/* #define msdc_dma_status() ((sdr_read32(MSDC_CFG) & MSDC_CFG_PIO) >> 3) */
#define msdc_dma_on()        sdr_clr_bits(MSDC_CFG, MSDC_CFG_PIO)
#define msdc_dma_off()       sdr_set_bits(MSDC_CFG, MSDC_CFG_PIO)
#define msdc_dma_status()    ((sdr_read32(MSDC_CFG) & MSDC_CFG_PIO) >> 3)
/* extern void mt_cpufreq_disable(unsigned int type, bool disabled); */

/* struct mmc_host *mmc; */
struct sdio_autok_thread_data *p_autok_thread_data;
struct task_struct *task;
u32 *cur_voltage;

static int fake_sdio_device(void *data);
struct task_struct *task2;

void autok_claim_host(struct msdc_host *host)
{
	mmc_claim_host(host->mmc);
	pr_debug("[%s] msdc%d host claimed\n", __func__, host->id);
}

void autok_release_host(struct msdc_host *host)
{
	mmc_release_host(host->mmc);
	pr_debug("[%s] msdc%d host released\n", __func__, host->id);
}

/* keep track of how many times it is mmapped */
void autok_mmap_open(struct vm_area_struct *vma)
{
	struct log_mmap_info *info = (struct log_mmap_info *)vma->vm_private_data;

	info->reference++;
}

void autok_mmap_close(struct vm_area_struct *vma)
{
	struct log_mmap_info *info = (struct log_mmap_info *)vma->vm_private_data;

	info->reference--;
}

/* nopage is called the first time a memory area is accessed which is not in memory,
 * it does the actual mapping between kernel and user space memory
 */
int autok_mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	unsigned long offset;
	struct log_mmap_info *dev = vma->vm_private_data;
	int result = VM_FAULT_SIGBUS;
	struct page *page;
	/* void *pageptr = NULL; default to "missing" */
	pgoff_t pgoff = vmf->pgoff;

	offset = (pgoff << PAGE_SHIFT) + (vma->vm_pgoff << PAGE_SHIFT);
	if (!dev->data) {
		pr_warn("no data\n");
		return 0;
	}
	if (offset >= dev->size)
		return 0;
	/* offset >>= PAGE_SHIFT; offset is a number of pages */
	/* got it, now convert pointer to a struct page and increment the count */
	page = virt_to_page(&dev->data[offset]);
	get_page(page);
	vmf->page = page;
	result = 0;
	return result;

}

struct vm_operations_struct autok_mmap_vm_ops = {
	.open = autok_mmap_open,
	.close = autok_mmap_close,
	.fault = autok_mmap_fault,
};


int autok_log_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &autok_mmap_vm_ops;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	/* assign the file private data to the vm private data */
	vma->vm_private_data = filp->private_data;
	autok_mmap_open(vma);
	return 0;
}

int autok_log_close(struct inode *inode, struct file *filp)
{
	struct log_mmap_info *info = filp->private_data;
	/* obtain new memory */
	kfree(autok_log_info);
	kfree(info);
	filp->private_data = NULL;
	autok_log_info = NULL;
	return 0;
}

int autok_log_open(struct inode *inode, struct file *filp)
{
	struct log_mmap_info *info = kmalloc(sizeof(struct log_mmap_info), GFP_KERNEL);
	/* obtain new memory */

	/* assign this info struct to the file */
	info->size = LOG_SIZE;
	info->data = autok_log_info;
	filp->private_data = info;
	return 0;
}

static const struct file_operations autok_log_fops = {
	.open = autok_log_open,
	.release = autok_log_close,
	.mmap = autok_log_mmap,
};

static int autok_calibration_done(int id, struct sdio_autok_thread_data *autok_thread_data)
{
	/* int err=0; */
	/* char cali_done[80] = "CALI_DONE"; */
	if (autok_thread_data->is_autok_done[id] == 0)
		autok_thread_data->is_autok_done[id] = 1;
	/*err = send_autok_uevent(cali_done, autok_thread_data->host);
	   if(err < 0)
	   return err;
	 */
	return 0;
}

#include <linux/time.h>

static int autok_thread_func(void *data)
{
	struct sdio_autok_thread_data *autok_thread_data;
	/* struct sched_param param = { .sched_priority = 99 }; */
	unsigned int vcore_uv = 0;
	struct msdc_host *host;
	struct mmc_host *mmc;
	char stage = 0;
	int i, j;
	int res = 0;
	int doStg2 = 0;
	void __iomem *base;
	u32 dma;
	struct timeval t0, t1;
	int time_in_s, time_in_ms;
	unsigned int hz;
/* unsigned long flags; */

	autok_thread_data = (struct sdio_autok_thread_data *)data;
	/* sched_setscheduler(current, SCHED_FIFO, &param);    */
/* preempt_disable(); */

	host = autok_thread_data->host;
	mmc = host->mmc;
	stage = autok_thread_data->stage;
	base = host->base;
	dma = msdc_dma_status();

	/* Inform msdc_set_mclk() auto-K is going to process */
	sdio_autok_processed = 1;

	/* Set clock to card max clock */
	hz = mmc->ios.clock;
	mmc_set_clock(mmc, 50000000);
	mmc_set_clock(mmc, hz);
	msdc_sdio_set_long_timing_delay_by_freq(host, mmc->ios.clock);

	msdc_ungate_clock(host);

	/* Set PIO mode */
	msdc_dma_off();

	vcore_uv = autok_get_current_vcore_offset();
	/* End of initialize */
	do_gettimeofday(&t0);
	/* if (autok_thread_data->log != NULL)
		log_info = autok_thread_data->log;
	 */
	if (stage == 1) {
		/* call stage 1 auto-K callback function */
		autok_thread_data->is_autok_done[host->id] = 0;
		res = msdc_autok_stg1_cal(host, vcore_uv, autok_thread_data->p_autok_predata);
		if (res) {
			pr_err
			    ("[%s] Auto-K stage 1 fail, res = %d, set msdc parameter settings stored in nvram to 0\n",
			     __func__, res);
			memset(autok_thread_data->p_autok_predata->ai_data[0], 0,
			       autok_thread_data->p_autok_predata->param_count *
			       sizeof(unsigned int));
			autok_thread_data->is_autok_done[host->id] = 2;
		}
	} else if (stage == 2) {
		/* call stage 2 auto-K callback function */

		/* check if msdc params of different volt are all 0, if so, that means auto-K stg1 failed */
		for (i = 0; i < autok_thread_data->p_autok_predata->vol_count; i++) {
			for (j = 0; j < autok_thread_data->p_autok_predata->param_count; j++) {
				if (autok_thread_data->p_autok_predata->ai_data[i][j].data.sel != 0) {
					doStg2 = 1;
					break;
				}
			}
			if (doStg2)
				break;
		}

		if (doStg2) {
			res =
			    msdc_autok_stg2_cal(host, autok_thread_data->p_autok_predata, vcore_uv);
			if (res) {
				pr_err
				    ("[%s] Auto-K stage 2 fail, res = %d, downgrade SDIO freq to 50MHz\n",
				     __func__, res);
				mmc->ios.clock = 50 * 1000 * 1000;
				mmc_set_clock(mmc, mmc->ios.clock);
				msdc_sdio_set_long_timing_delay_by_freq(host, mmc->ios.clock);
				sdio_autok_processed = 0;
				for (i = 0; i < HOST_MAX_NUM; i++) {
					if (autok_thread_data->p_autok_progress[i].host_id == -1) {
						break;
					} else if (autok_thread_data->p_autok_progress[i].host_id ==
						   host->id) {
						autok_thread_data->p_autok_progress[i].fail = 1;
					}
				}
			}
		} else {	/* Auto-K stg1 failed */
			pr_err("[%s] Auto-K stage 1 fail, downgrade SDIO freq to 50MHz\n",
			       __func__);
			mmc->ios.clock = 50 * 1000 * 1000;
			mmc_set_clock(mmc, mmc->ios.clock);
			msdc_sdio_set_long_timing_delay_by_freq(host, mmc->ios.clock);
			sdio_autok_processed = 0;
			for (i = 0; i < HOST_MAX_NUM; i++) {
				if (autok_thread_data->p_autok_progress[i].host_id == -1) {
					break;
				} else if (autok_thread_data->p_autok_progress[i].host_id ==
					   host->id) {
					autok_thread_data->p_autok_progress[i].fail = 1;
				}
			}
		}

		/* log_info = NULL; */
	} else {
		pr_err("[%s] stage %d doesn't support in auto-K\n", __func__, stage);
		/* return -EFAULT; */
	}
	do_gettimeofday(&t1);
	if (dma == DMA_ON)
		msdc_dma_on();

	msdc_gate_clock(host, 1);

	/* [FIXDONE] Tell native module that auto-K has finished */
	if (stage == 1)
		autok_calibration_done(host->id, autok_thread_data);
	else if (stage == 2) {
		for (i = 0; i < HOST_MAX_NUM; i++) {
			if (autok_thread_data->p_autok_progress[i].host_id == -1) {
				break;
			} else if (autok_thread_data->p_autok_progress[i].host_id == host->id) {
				autok_thread_data->p_autok_progress[i].done = 1;
				if (autok_thread_data->p_autok_progress[i].done > 0)
					complete(&autok_thread_data->autok_completion[i]);
			}
		}
	}
	time_in_s = (t1.tv_sec - t0.tv_sec);
	time_in_ms = (t1.tv_usec - t0.tv_usec) >> 10;
	pr_info("\n[AUTOKK][Stage%d] Timediff is %d.%d(s)\n", (int)stage, time_in_s, time_in_ms);

/* preempt_enable(); */
	return 0;
}
#endif

int send_autok_uevent(char *text, struct msdc_host *host)
{
	int err = 0;
	char *envp[3];
	char *host_buf;
	char *what_buf;
	/* struct msdc_host *host = mtk_msdc_host[id]; */
	host_buf = kzalloc(sizeof(char) * 128, GFP_KERNEL);
	what_buf = kzalloc(sizeof(char) * 128, GFP_KERNEL);

	snprintf(host_buf, MAX_ARGS_BUF - 1, "HOST=%d", host->id);
	snprintf(what_buf, MAX_ARGS_BUF - 1, "WHAT=%s", text);
	envp[0] = host_buf;
	envp[1] = what_buf;
	envp[2] = NULL;

	if (host != NULL)
		err = kobject_uevent_env(&host->mmc->class_dev.kobj, KOBJ_CHANGE, envp);
	kfree(host_buf);
	kfree(what_buf);
	if (err < 0)
		pr_err("[%s] kobject_uevent_env error = %d\n", __func__, err);

	return err;
}

int wait_sdio_autok_ready(void *data)
{
	int i;
	int ret = 0;
	enum boot_mode_t btmod;
	struct mmc_host *mmc = (struct mmc_host *)data;
	struct msdc_host *host = NULL;
	int id;
	unsigned int vcore_uv = 0;

	btmod = get_boot_mode();

	pr_info("btmod = %d\n", btmod);

	if ((btmod != NORMAL_BOOT) && (btmod != META_BOOT)) {
		pr_info("Not META or normal boot, return directly\n");
		return 0;
	}

	if (1 /*(btmod!=META_BOOT) && (btmod!=FACTORY_BOOT) && (btmod!=ATE_FACTORY_BOOT) */) {
		sdio_host_debug = 0;
		/* host = mtk_msdc_host[id]; */
		host = mmc_priv(mmc);
		id = host->id;

#ifndef UT_TEST
		/* claim host */
#ifdef CONFIG_SDIOAUTOK_SUPPORT
#if 0
		pr_warn("wait_sdio_autok_ready(): is_vcorefs_can_work= %d\n",
			is_vcorefs_can_work());
		if (vcorefs_request_dvfs_opp(KIR_SDIO, OPPI_PERF) != 0) {	/* performance mode, return 0 pass */
			/* BUG_ON("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n"); */
			pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");
		}
#endif
		g_autok_vcore_sel[0] = 1150000;
		pr_warn("wait_sdio_autok_ready(): vcorefs_get_curr_voltage= %d\n",
			g_autok_vcore_sel[0]);

		/* mt_cpufreq_disable(0, true); */
		/* FIXME@CCJ mt_vcore_dvfs_disable_by_sdio(0, true); */
		/* vcorefs_sdio_lock_dvfs(0); //ccyeh */
#endif
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
		/* ccyeh atomic_set(&host->ot_work.ot_disable, 1); */
#endif				/* MTK_SDIO30_ONLINE_TUNING_SUPPORT */
		autok_claim_host(host);
#endif

#ifdef AUTOK_THREAD
		for (i = 0; i < HOST_MAX_NUM; i++) {
			if (p_autok_thread_data->p_autok_progress[i].host_id == id &&
			    p_autok_thread_data->p_autok_progress[i].done == 1 &&
			    p_autok_thread_data->p_autok_progress[i].fail == 0) {
				vcore_uv = autok_get_current_vcore_offset();
				msdc_autok_stg2_cal(host, &p_autok_predata[id], vcore_uv);
				break;
			}
		}

		if (i != HOST_MAX_NUM)
			goto EXIT_WAIT_AUTOK_READY;

#endif

		for (i = 0; i < HOST_MAX_NUM; i++) {
			if (p_autok_thread_data->p_autok_progress[i].host_id == -1
			    || p_autok_thread_data->p_autok_progress[i].host_id == id) {
				send_autok_uevent("s2_ready", host);
				init_completion(&p_autok_thread_data->autok_completion[i]);
				p_autok_thread_data->p_autok_progress[i].done = 0;
				p_autok_thread_data->p_autok_progress[i].fail = 0;
				p_autok_thread_data->p_autok_progress[i].host_id = id;
				wait_for_completion_interruptible
				    (&p_autok_thread_data->autok_completion[i]);
				for (i = 0; i < HOST_MAX_NUM; i++) {
					if (p_autok_thread_data->p_autok_progress[i].host_id == id
					    && p_autok_thread_data->p_autok_progress[i].fail == 1) {
						ret = -1;
						break;
					}
				}
				send_autok_uevent("s2_done", host);

				break;
			}
		}

		/* reset_autok_cursor(0); */
EXIT_WAIT_AUTOK_READY:
#ifndef UT_TEST
		/* release host */
		autok_release_host(host);
#ifdef CONFIG_SDIOAUTOK_SUPPORT
		/* mt_cpufreq_disable(0, false); */
		/* FIXME@CCJ mt_vcore_dvfs_disable_by_sdio(0, false); */
		/* vcorefs_sdio_unlock_dvfs(0);//ccyeh */

#if 0
		if (vcorefs_request_dvfs_opp(KIR_SDIO, OPPI_UNREQ) != 0) {	/* un-request, return 0 pass */
			/* BUG_ON("vcorefs_request_dvfs_opp@OPPI_UNREQ fail!\n"); */
			pr_err("vcorefs_request_dvfs_opp@OPPI_UNREQ fail!\n");
		}
#endif
#endif
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
		atomic_set(&host->ot_work.autok_done, 1);
		atomic_set(&host->ot_work.ot_disable, 0);
#endif				/* MTK_SDIO30_ONLINE_TUNING_SUPPORT */
#endif
	}

	return ret;
}
EXPORT_SYMBOL(wait_sdio_autok_ready);

void init_tune_sdio(struct msdc_host *host)
{
	int i;

	for (i = 0; i < HOST_MAX_NUM; i++) {
		if (p_autok_thread_data->p_autok_progress[i].host_id == host->id) {
			if (p_autok_thread_data->p_autok_progress[i].done == 1 &&
			    p_autok_thread_data->p_autok_progress[i].fail == 0) {
				u32 vcore_uv = 0;

				vcore_uv = autok_get_current_vcore_offset();
				pr_warn
				    ("init_tune_sdio@msdc_autok_apply_param, id:%d vcore_uv:%d\n",
				     host->id, vcore_uv);
				msdc_autok_apply_param(host, vcore_uv);
				/* wait_sdio_autok_ready(host->mmc); */
			}
			break;
		}
	}
}

static int print_autok(struct autok_predata *t_predata, char *buf)
{
	int offset;
	int i, j;
	int vol_count;
	int param_count;
	int param_size;
	int width;
	/* struct autok_predata *t_predata; */
	char data_buf[1024] = "";
	/* char *temp_ch; */
	offset = 0;
	/* t_predata = &p_autok_predata[id]; */
	vol_count = t_predata->vol_count;
	param_count = t_predata->param_count;
	param_size = param_count * sizeof(U_AUTOK_INTERFACE_DATA);
	/* len = snprintf(data_buf + count, 32, "%02x", vol_count); */
	/* count += len; */
	/* len = snprintf(data_buf + count, 32, "%02x", param_count); */
	/* count += len; */
	data_buf[0] = vol_count;
	data_buf[1] = param_count;
	offset += 2;
	/* temp_ch = (char*)&t_predata->vol_list[0]; */
	width = sizeof(unsigned int) / sizeof(char);
	for (i = 0; i < vol_count; i++) {
		/* len = snprintf(data_buf + count, 32, "%02x", temp_ch[i]); */
		/* count += len; */
		memcpy(data_buf + offset + i * width, &t_predata->vol_list[i], width);
	}
	offset += vol_count * width;
	width = sizeof(U_AUTOK_INTERFACE_DATA) / sizeof(char);
	for (i = 0; i < vol_count; i++) {
		/* temp_ch = (char*)&t_predata->ai_data[i][0]; */
		for (j = 0; j < param_count; j++) {
			/* len = snprintf(data_buf + offset, 32, "%02x", temp_ch[j]); */
			/* offset += len; */
			/* data_buf[offset+j] = temp_ch[j]; */
			memcpy(data_buf + offset + j * width, &t_predata->ai_data[i][j], width);
		}
		offset += param_count * width;
	}

	/* len = snprintf(buf, 2048, "%s", data_buf); */
	memcpy(buf, data_buf, offset);

	return offset;
}

static int store_autok(struct autok_predata *t_predata, const char *buf, int count)
{
	U_AUTOK_INTERFACE_DATA **ai_data;
	unsigned int *vollist;
	int param_size, offset, i;
	int width;
	int vol_count;
	int param_count;

	offset = 0;
	vol_count = buf[0];
	param_count = buf[1];

	offset += 2;
	vollist = kcalloc(vol_count, sizeof(unsigned int), GFP_KERNEL);
	width = sizeof(unsigned int) / sizeof(char);
	param_size = param_count * sizeof(U_AUTOK_INTERFACE_DATA);
	for (i = 0; i < vol_count; i++) {
		vollist[i] = *((unsigned int *)(&buf[offset]));
		offset += width;
	}
	/* if(vol_count*param_size + offset > count) */
	/* return count; */
	ai_data = kcalloc(vol_count, sizeof(U_AUTOK_INTERFACE_DATA *), GFP_KERNEL);
	for (i = 0; i < vol_count; i++) {
		ai_data[i] = kzalloc(param_size, GFP_KERNEL);
		memcpy(ai_data[i], &buf[offset], param_size);
		offset += param_size;
	}
	t_predata->vol_count = vol_count;
	t_predata->param_count = param_count;
	t_predata->ai_data = ai_data;
	t_predata->vol_list = vollist;

	return 0;
}

static ssize_t stage1_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	char cur_name[DEVNAME_SIZE] = "";
	int test_len, cur_len;
	int i, j;
	int id;
	int select;
	int ret;
	struct msdc_host *host;
	/* char *p_log; */
	/* id = 3; */
	select = -1;
	/* sscanf(kobj->name, "%d", &id); */
	ret = kstrtoint(kobj->name, 0, &id);

	if (id >= HOST_MAX_NUM) {
		pr_err("[%s] id<%d> is bigger than HOST_MAX_NUM<%d>\n", __func__, id, HOST_MAX_NUM);
		return count;
	}

	host = mtk_msdc_host[id];
	/* sscanf(attr->attr.name, "%s", cur_name); */
	strncpy(cur_name, attr->attr.name, DEVNAME_SIZE);

	for (i = 0; i < TOTAL_STAGE1_NODE_COUNT; i++) {
		test_len = strlen(stage1_nodes[i]);
		cur_len = strlen(cur_name);
		if ((test_len == cur_len) && (strncmp(stage1_nodes[i], cur_name, cur_len) == 0)) {
			select = i;
			break;
		}
	}

	switch (select) {
	case VOLTAGE:
		/* sscanf(buf, "%u", &cur_voltage[id]); */
		ret = kstrtou32(buf, 0, &cur_voltage[id]);
		break;
	case PARAMS:
		memset(cur_name, 0, DEVNAME_SIZE);
		cur_name[0] = 1;
		cur_name[1] = E_AUTOK_PARM_MAX;
		memcpy(&cur_name[2], &cur_voltage[id], sizeof(unsigned int));
		store_autok(&p_single_autok[id], cur_name, count);

		pr_err("[AUTOKD] Enter Store Autok");
		pr_err("[AUTOKD] p_single_autok[%d].vol_count=%d", id,
		       p_single_autok[id].vol_count);
		pr_err("[AUTOKD] p_single_autok[%d].param_count=%d", id,
		       p_single_autok[id].param_count);
		for (i = 0; i < p_single_autok[id].vol_count; i++) {
			pr_err("[AUTOKD] p_single_autok[%d].vol_list[%d]=%d", id, i,
			       p_single_autok[id].vol_list[i]);
		}
		for (i = 0; i < p_single_autok[id].vol_count; i++) {
			for (j = 0; j < p_single_autok[id].param_count; j++)
				pr_err("[AUTOKD] p_single_autok[%d].ai_data[%d][%d]=%d", id, i, j,
				       p_single_autok[id].ai_data[i][j].data.sel);
		}
		/* [FIXDONE] Start to do autok alforithm; data is in p_single_autok */
#ifdef UT_TEST
#error "remove me"
		if (is_first_stage1 == 1) {
			/* claim host */
#ifdef CONFIG_SDIOAUTOK_SUPPORT
			/* mt_cpufreq_disable(0, true); */
			/* FIXME@CCJ mt_vcore_dvfs_disable_by_sdio(0, true); */
#endif
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
			atomic_set(&host->ot_work.ot_disable, 1);
#endif				/* MTK_SDIO30_ONLINE_TUNING_SUPPORT */
			autok_claim_host(host);

			is_first_stage1 = 0;
		}
#endif
#ifdef AUTOK_THREAD
		p_autok_thread_data->host = host;
		p_autok_thread_data->stage = 1;
		p_autok_thread_data->p_autok_predata = &p_single_autok[id];
		p_autok_thread_data->log = autok_log_info;
		task = kthread_run(&autok_thread_func, (void *)(p_autok_thread_data), "autokp");
#endif
		break;
	case DONE:
		/* sscanf(buf, "%d", &i); */
		ret = kstrtoint(buf, 0, &i);
		p_autok_thread_data->is_autok_done[id] = (u8) i;
		break;
	case LOG:
		/* sscanf(buf, "%d", &i); */
		ret = kstrtoint(buf, 0, &i);
		if (is_full_log != i) {
			is_full_log = i;
			if (i == 0) {
				debugfs_remove(autok_log_entry);
				/* kfree(autok_log_info); */
			} else {
				autok_log_entry =
				    debugfs_create_file("autok_log", 0660, NULL, NULL,
							&autok_log_fops);
				i_gid_write(autok_log_entry->d_inode, 1000);
				autok_log_info = kzalloc(LOG_SIZE, GFP_KERNEL);
				/* total_msg_size = 0;	*/
				/* rewind the counter to 0 when allocate new buffer */
			}
		}
		break;
	default:
		break;
	}
	return count;
}

static ssize_t stage1_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char cur_name[DEVNAME_SIZE] = "";
	char data_buf[1024] = "";
	int test_len, cur_len;
	int i, j;
	int id;
	int len, count = 0;
	int select;
	int ret;
	struct msdc_host *host;
	/* char *p_log; */
	/* id = 3; */

	/* sscanf(kobj->name, "%d", &id); */
	ret = kstrtoint(kobj->name, 0, &id);

	if (id >= HOST_MAX_NUM) {
		pr_err("[%s] id<%d> is bigger than HOST_MAX_NUM<%d>\n", __func__, id, HOST_MAX_NUM);
		return count;
	}

	host = mtk_msdc_host[id];
	/* sscanf(attr->attr.name, "%s", cur_name); */
	strncpy(cur_name, attr->attr.name, DEVNAME_SIZE);
	select = 0;
	for (i = 0; i < TOTAL_STAGE1_NODE_COUNT; i++) {
		test_len = strlen(stage1_nodes[i]);
		cur_len = strlen(cur_name);
		if ((test_len == cur_len) && (strncmp(stage1_nodes[i], cur_name, cur_len) == 0)) {
			select = i;
			break;
		}
	}
	len = 0;
	count = 0;
	switch (select) {
	case VOLTAGE:
		len = snprintf(buf, 32, "%d\n", cur_voltage[id]);
		break;
	case PARAMS:
		if (!sdio_host_debug) {
			len = print_autok(&p_single_autok[id], (char *)buf);
		} else {
			len = snprintf(data_buf + count, 32, "\nDetail Information\n");
			count += len;
			len =
			    snprintf(data_buf + count, 32, "vol_count:%d\n",
				     p_single_autok[id].vol_count);
			count += len;
			len =
			    snprintf(data_buf + count, 32, "param_count:%d\n",
				     p_single_autok[id].param_count);
			count += len;

			for (i = 0; i < p_single_autok[id].vol_count; i++) {
				len =
				    snprintf(data_buf + count, 32, "vol[%d]:%d\n", i,
					     p_single_autok[id].vol_list[i]);
				count += len;
			}
			/* goto EXIT_STAGE2; */
			for (i = 0; i < p_single_autok[id].vol_count; i++) {
				for (j = 0; j < p_single_autok[id].param_count; j++) {
					len =
					    snprintf(data_buf + count, 32, "param[%d][%d]:%d\n", i,
						     j, p_single_autok[id].ai_data[i][j].data.sel);
					count += len;
				}
			}
			len = snprintf(buf, 1024, "%s\n", data_buf);
		}
		break;
	case DONE:
		len = snprintf(buf, 32, "%d\n", p_autok_thread_data->is_autok_done[id]);
		break;
	case LOG:
		/* if(autok_log_info != NULL) */
		len = snprintf(buf, 32, "%d", is_full_log);
		break;
	}

	return len;
}

static int create_stage1_nodes(int size, struct kobject *parent_kobj)
{
#define NODE_COUNT (TOTAL_STAGE1_NODE_COUNT)
	int i, j;
	int retval = 0;
	char *name;

	stage1_attrs = kzalloc(NODE_COUNT * size * sizeof(struct attribute *), GFP_KERNEL);
	stage1_kattr = kzalloc(NODE_COUNT * size * sizeof(struct kobj_attribute *), GFP_KERNEL);

	s1_host_kobj = kcalloc(size, sizeof(struct kobject *), GFP_KERNEL);
	for (i = 0; i < size; i++) {
		name = kzalloc(DEVNAME_SIZE, GFP_KERNEL);
		sprintf(name, "%d", i);
		s1_host_kobj[i] = kobject_create_and_add(name, parent_kobj);
		if (s1_host_kobj[i] == NULL)
			return -ENOMEM;
		for (j = 0; j < NODE_COUNT; j++) {
			stage1_attrs[i * NODE_COUNT + j] =
			    kzalloc(sizeof(struct attribute), GFP_KERNEL);
			stage1_kattr[i * NODE_COUNT + j] =
			    kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
			/* name = kzalloc(DEVNAME_SIZE, GFP_KERNEL); */
			/* sprintf(name, "%s", args_type[j]); */
			stage1_attrs[i * NODE_COUNT + j]->name = stage1_nodes[j];
			stage1_attrs[i * NODE_COUNT + j]->mode = 0660;
			stage1_kattr[i * NODE_COUNT + j]->attr = *stage1_attrs[i * NODE_COUNT + j];
			stage1_kattr[i * NODE_COUNT + j]->show = stage1_show;
			stage1_kattr[i * NODE_COUNT + j]->store = stage1_store;
			sysfs_attr_init(&stage1_kattr[i * NODE_COUNT + j]->attr);
			retval =
			    sysfs_create_file(s1_host_kobj[i],
					      &stage1_kattr[i * NODE_COUNT + j]->attr);
			if (retval)
				kobject_put(s1_host_kobj[i]);
		}
	}
	p_autok_thread_data->is_autok_done = kcalloc(size, sizeof(u8), GFP_KERNEL);
	cur_voltage = kcalloc(size, sizeof(u32), GFP_KERNEL);
	p_single_autok = kcalloc(size, sizeof(struct autok_predata), GFP_KERNEL);
	return 0;
}

static ssize_t stage2_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int id;
	int i, j;
	int count;
	char data_buf[1024] = "";
	int len = 0;
	int ret;

	id = 2;

	count = 0;
	/* sscanf(attr->attr.name, "%d", &id); */
	ret = kstrtoint(attr->attr.name, 0, &id);

/* =========================================================================// */
	if (!sdio_host_debug) {
		len = print_autok(&p_autok_predata[id], (char *)buf);
		count += len;
	} else {
		len = snprintf(data_buf + count, 32, "\nDetail Information\n");
		count += len;
		len =
		    snprintf(data_buf + count, 32, "vol_count:%d\n", p_autok_predata[id].vol_count);
		count += len;
		len =
		    snprintf(data_buf + count, 32, "param_count:%d\n",
			     p_autok_predata[id].param_count);
		count += len;

		for (i = 0; i < p_autok_predata[id].vol_count; i++) {
			len =
			    snprintf(data_buf + count, 32, "vol[%d]:%d\n", i,
				     p_autok_predata[id].vol_list[i]);
			count += len;
		}
		/* goto EXIT_STAGE2; */
		for (i = 0; i < p_autok_predata[id].vol_count; i++) {
			for (j = 0; j < p_autok_predata[id].param_count; j++) {
				len =
				    snprintf(data_buf + count, 32, "param[%d][%d]:%d\n", i, j,
					     p_autok_predata[id].ai_data[i][j].data.sel);
				count += len;
			}
		}
		len = snprintf(buf, 1024, "%s\n", data_buf);
	}

	return len;
}

static ssize_t stage2_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	int id;
	struct msdc_host *host;
	int ret;
	/* id = 3; */
	/* sscanf(attr->attr.name, "%d", &id); */
	ret = kstrtoint(attr->attr.name, 0, &id);

	if (id >= HOST_MAX_NUM) {
		pr_err("[%s] id<%d> is bigger than HOST_MAX_NUM<%d>\n", __func__, id, HOST_MAX_NUM);
		return count;
	}

	host = mtk_msdc_host[id];

	if (count > 2) {

		store_autok(&p_autok_predata[id], buf, count);

		/* [FIXDONE] Hook the function to apply parameter to respective controller */
#ifdef AUTOK_THREAD
		p_autok_thread_data->host = host;
		p_autok_thread_data->stage = 2;
		p_autok_thread_data->p_autok_predata = &p_autok_predata[id];
		p_autok_thread_data->log = autok_log_info;
		task = kthread_run(&autok_thread_func, (void *)(p_autok_thread_data), "autokp");
		/* autok_thread_func((void *)(p_autok_thread_data)); */
		/* wake_up_process(task); */
#endif

#ifdef UT_TEST
#error "remove me"
		if (is_first_stage1 == 0) {
			/* release host */
			autok_release_host(host);
#ifdef CONFIG_SDIOAUTOK_SUPPORT
			/* mt_cpufreq_disable(0, false); */
			/* FIXME@CCJ mt_vcore_dvfs_disable_by_sdio(0, false); */
#endif
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
			atomic_set(&host->ot_work.autok_done, 1);
			atomic_set(&host->ot_work.ot_disable, 0);
#endif				/* MTK_SDIO30_ONLINE_TUNING_SUPPORT */

			is_first_stage1 = 1;
		}
#endif
	}


	return count;
}

static int create_stage2_node(int size, struct kobject *parent_kobj)
{
	int i;
	int retval = 0;
	char *name;

	stage2_attrs = kcalloc(size, sizeof(struct attribute *), GFP_KERNEL);
	stage2_kattr = kcalloc(size, sizeof(struct kobj_attribute *), GFP_KERNEL);

	for (i = 0; i < size; i++) {
		name = kzalloc(DEVNAME_SIZE, GFP_KERNEL);
		sprintf(name, "%d", i);
		stage2_attrs[i] = kzalloc(sizeof(struct attribute), GFP_KERNEL);
		stage2_kattr[i] = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
		stage2_attrs[i]->name = name;
		stage2_attrs[i]->mode = 0660;
		stage2_kattr[i]->attr = *stage2_attrs[i];
		stage2_kattr[i]->show = stage2_show;
		stage2_kattr[i]->store = stage2_store;
		sysfs_attr_init(&stage2_kattr[i]->attr);
		retval = sysfs_create_file(parent_kobj, &stage2_kattr[i]->attr);
		if (retval)
			kobject_put(parent_kobj);
	}
	p_autok_predata = kcalloc(size, sizeof(struct autok_predata), GFP_KERNEL);
	return 0;
}

static ssize_t host_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len, count;
	int i;
	int select;
	int test_len, cur_len;
	char cur_name[80];
	char data_buf[1024] = "";

	select = 0;
	/* sscanf(attr->attr.name, "%s", cur_name); */
	strncpy(cur_name, attr->attr.name, 80);
	for (i = 0; i < TOTAL_HOST_NODE_COUNT; i++) {
		test_len = strlen(host_nodes[i]);
		cur_len = strlen(cur_name);
		if ((test_len == cur_len) && (strncmp(host_nodes[i], cur_name, cur_len) == 0)) {
			select = i;
			break;
		}
	}
	len = 0;
	count = 0;
	switch (select) {
	case READY:
		for (i = 0; i < HOST_MAX_NUM; i++) {
			if (p_autok_thread_data->p_autok_progress[i].host_id == -1)
				break;
			len =
			    snprintf(data_buf + count, 32, "%d:%d\t[msdc_id:progress]\n",
				     p_autok_thread_data->p_autok_progress[i].host_id,
				     p_autok_thread_data->p_autok_progress[i].done);
			count += len;

		}
		break;
	case PARAM_COUNT:
		len = snprintf(data_buf, 32, "%d", E_AUTOK_PARM_MAX);
		break;
	case DEBUG:
		len = snprintf(data_buf, 32, "%d", sdio_host_debug);
		break;
	case SS_CORNER:
		len = snprintf(data_buf, 32, "%d", 0);
		break;
	case SUGGEST_VOL:
		if (suggest_vol != NULL) {
			cur_len = 0;
			memset(data_buf, 0, 1024);
			for (i = 0; i < suggest_vol_count; i++) {
				test_len = snprintf(data_buf + cur_len, 32, "%u:", suggest_vol[i]);
				cur_len += test_len;
			}
		}
		len = snprintf(buf, PAGE_SIZE, "%s", data_buf);
		break;
	}

	len = snprintf(buf, 1024, "%s", data_buf);
	return len;
}

static volatile int id_data;
static ssize_t host_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	int id;
	int i;
	int select;
	int test_len, cur_len;
	char cur_name[80];
	int ret;

	select = 0;
	/* struct msdc_host *host; */

	/* sscanf(attr->attr.name, "%s", cur_name); */
	strncpy(cur_name, attr->attr.name, 80);
	for (i = 0; i < TOTAL_HOST_NODE_COUNT; i++) {
		test_len = strlen(host_nodes[i]);
		cur_len = strlen(cur_name);
		if ((test_len == cur_len) && (strncmp(host_nodes[i], cur_name, cur_len) == 0)) {
			select = i;
			break;
		}
	}

	switch (select) {
	case READY:
		/* sscanf(buf, "%d", &id); */
		ret = kstrtoint(buf, 0, &id);
		for (i = 0; i < HOST_MAX_NUM; i++) {
			if (p_autok_thread_data->p_autok_progress[i].host_id == -1) {
				/* p_autok_thread_data->p_autok_progress[i] = id; */
				task2 = kthread_run(&fake_sdio_device, (void *)&id, "fake_lte");
				break;
			} else if (p_autok_thread_data->p_autok_progress[i].host_id == id) {
				p_autok_thread_data->p_autok_progress[i].done ^= 1;
				if (p_autok_thread_data->p_autok_progress[i].done == 1) {
					if (p_autok_thread_data->autok_completion[i].done > 0)
						complete(&p_autok_thread_data->autok_completion[i]);
				} else {
					task2 =
					    kthread_run(&fake_sdio_device, (void *)&id, "fake_lte");
				}
				break;
			}
		}
		break;
	case DEBUG:
		/* sscanf(buf, "%d", &i); */
		ret = kstrtoint(buf, 0, &i);
		sdio_host_debug = i;
		break;
	case SS_CORNER:
	case PARAM_COUNT:
	case SUGGEST_VOL:
	default:
		break;

	}

	return count;
}

static int create_host_node(struct kobject *parent_kobj)
{
	int i;
	int retval = 0;

	host_attrs = kzalloc(sizeof(struct attribute *), GFP_KERNEL);
	host_kattr = kzalloc(sizeof(struct kobj_attribute *), GFP_KERNEL);
	/* name = kzalloc(DEVNAME_SIZE, GFP_KERNEL); */

	for (i = 0; i < (sizeof(host_nodes) / sizeof(host_nodes[0])); i++) {
		host_attrs[i] = kzalloc(sizeof(struct attribute), GFP_KERNEL);
		host_kattr[i] = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
		host_attrs[i]->name = host_nodes[i];
		host_attrs[i]->mode = 0660;
		host_kattr[i]->attr = *host_attrs[i];
		host_kattr[i]->show = host_show;
		host_kattr[i]->store = host_store;
		sysfs_attr_init(&host_kattr[i]->attr);
		retval = sysfs_create_file(parent_kobj, &host_kattr[i]->attr);
	}

	p_autok_thread_data->p_autok_progress =
	    kzalloc(sizeof(struct autok_progress) * HOST_MAX_NUM, GFP_KERNEL);
	p_autok_thread_data->autok_completion =
	    kzalloc(sizeof(struct completion) * HOST_MAX_NUM, GFP_KERNEL);
	for (i = 0; i < HOST_MAX_NUM; i++) {
		p_autok_thread_data->p_autok_progress[i].host_id = -1;
		p_autok_thread_data->p_autok_progress[i].done = 0;
		p_autok_thread_data->p_autok_progress[i].fail = 0;
		init_completion(&p_autok_thread_data->autok_completion[i]);
	}
	return retval;
}

struct task_struct *task2;
static int fake_sdio_device(void *data)
{
	/* struct msdc_host *host = mtk_msdc_host[3]; */
	struct msdc_host *host = mtk_msdc_host[2];	/* Denali use MSDC2 to connect 6630. */

	wait_sdio_autok_ready(host->mmc);
	return 0;
}

static int __init autok_init(void)
{
	int retval = 0;

	autok_log_entry = debugfs_create_file("autok_log", 0660, NULL, NULL, &autok_log_fops);
	i_gid_write(autok_log_entry->d_inode, 1000);
	autok_log_info = kzalloc(LOG_SIZE, GFP_KERNEL);
	is_full_log = 1;
	/* create node /sys/mtk_sdio */
	autok_kobj = kobject_create_and_add("autok", NULL);
	if (autok_kobj == NULL)
		return -ENOMEM;
	stage1_kobj = kobject_create_and_add("stage1", autok_kobj);
	if (stage1_kobj == NULL)
		return -ENOMEM;

	stage2_kobj = kobject_create_and_add("stage2", autok_kobj);
	if (stage2_kobj == NULL)
		return -ENOMEM;
#ifdef AUTOK_THREAD
	p_autok_thread_data = kzalloc(sizeof(struct sdio_autok_thread_data), GFP_KERNEL);
	p_autok_thread_data->log = autok_log_info;
	/* msdc_autok_apply_param_2 = msdc_autok_apply_param; */
#endif

	retval = create_host_node(autok_kobj);
	if (retval != 0)
		return retval;

	retval = create_stage1_nodes(HOST_MAX_NUM, stage1_kobj);
	if (retval != 0)
		return retval;

	retval = create_stage2_node(HOST_MAX_NUM, stage2_kobj);
	if (retval != 0)
		return retval;

	suggest_vol_count = msdc_autok_get_suggetst_vcore(&suggest_vol);

	/* ccyeh suggest_vol_count = 1;//ccyeh FIXME */

	return retval;
}

static void __exit autok_exit(void)
{
	int i, j;
	/* kobject_put(args_kobj); */
	kobject_put(stage2_kobj);
	kobject_put(stage1_kobj);
	kobject_put(autok_kobj);
	for (i = 0; i < HOST_MAX_NUM * TOTAL_STAGE1_NODE_COUNT; i++) {
		struct attribute *attr = *(stage1_attrs + i);
		/* kfree(attr->name); */
		kfree(attr);
		kfree(stage1_kattr[i]);
	}
	kfree(stage1_attrs);
	kfree(stage1_kattr);

	for (i = 0; i < HOST_MAX_NUM; i++)
		kobject_put(s1_host_kobj[i]);
	kfree(s1_host_kobj);

	for (i = 0; i < HOST_MAX_NUM; i++) {
		struct attribute *attr = *(stage2_attrs + i);

		kfree(attr->name);
		kfree(attr);
		kfree(stage2_kattr[i]);
	}
	kfree(stage2_attrs);
	kfree(stage2_kattr);

	for (i = 0; i < sizeof(host_nodes) / sizeof(host_nodes[0]); i++) {
		struct attribute *attr = *(host_attrs + i);

		kfree(attr);
		kfree(host_kattr[i]);
	}

	kfree(host_attrs);
	kfree(host_kattr);

	for (i = 0; i < HOST_MAX_NUM; i++) {
		if (p_autok_predata[i].vol_count == 0 || p_autok_predata[i].vol_list == NULL)
			continue;
		kfree(p_autok_predata[i].vol_list);
		for (j = 0; j < p_autok_predata[i].vol_count; j++)
			kfree(p_autok_predata[i].ai_data[j]);
	}
	kfree(p_autok_predata);


	for (i = 0; i < HOST_MAX_NUM; i++) {
		if (p_single_autok[i].vol_count == 0 || p_single_autok[i].vol_list == NULL)
			continue;
		kfree(p_single_autok[i].vol_list);
		for (j = 0; j < p_single_autok[i].vol_count; j++)
			kfree(p_single_autok[i].ai_data[j]);
	}
	kfree(cur_voltage);
	kfree(p_single_autok);
	kfree(p_autok_thread_data->is_autok_done);
	kfree(p_autok_thread_data->p_autok_progress);
	kfree(p_autok_thread_data->autok_completion);
	/* kthread_stop(task); */
#ifdef AUTOK_THREAD
	kfree(p_autok_thread_data);
#endif
	/* debugfs_remove(autok_log_entry); */
}
module_init(autok_init);
module_exit(autok_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek SDIO Auto-K Proc");
MODULE_LICENSE("GPL");
