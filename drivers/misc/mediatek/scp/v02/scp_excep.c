/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/vmalloc.h>         /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <mt-plat/aee.h>
#include <mt-plat/sync_write.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include <linux/delay.h>
#include "scp_ipi_pin.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_feature_define.h"
#include "scp_l1c.h"

struct scp_status_reg {
	unsigned int pc;
	unsigned int lr;
	unsigned int psp;
	unsigned int sp;
	unsigned int m2h;
	unsigned int h2m;
};

struct scp_dump_st {
	uint8_t *detail_buff;
	uint8_t *ramdump;
	uint32_t ramdump_alloc_size;
	uint32_t ramdump_length;
};

//static unsigned char *scp_A_dump_buffer;
struct scp_dump_st scp_dump;

//static unsigned int scp_A_dump_length;
static unsigned int scp_A_task_context_addr;
static struct scp_status_reg scp_A_aee_status;
static struct mutex scp_excep_mutex;
int scp_ee_enable;

/*
 * return last lr for debugging
 */
uint32_t scp_dump_lr(void)
{
	if (is_scp_ready(SCP_A_ID))
		return readl(SCP_A_DEBUG_LR_REG);
	else
		return 0xFFFFFFFF;
}

/*
 * return last pc for debugging
 */
uint32_t scp_dump_pc(void)
{
	if (is_scp_ready(SCP_A_ID))
		return readl(SCP_A_DEBUG_PC_REG);
	else
		return 0xFFFFFFFF;
}

/*
 * dump scp register for debugging
 */
void scp_A_dump_regs(void)
{
	uint32_t tmp;

	if (is_scp_ready(SCP_A_ID)) {
		pr_debug("[SCP]ready PC:0x%x,LR:0x%x,PSP:0x%x,SP:0x%x\n"
		, readl(SCP_A_DEBUG_PC_REG), readl(SCP_A_DEBUG_LR_REG)
		, readl(SCP_A_DEBUG_PSP_REG), readl(SCP_A_DEBUG_SP_REG));
	} else {
		pr_debug("[SCP]not ready PC:0x%x,LR:0x%x,PSP:0x%x,SP:0x%x\n"
		, readl(SCP_A_DEBUG_PC_REG), readl(SCP_A_DEBUG_LR_REG)
		, readl(SCP_A_DEBUG_PSP_REG), readl(SCP_A_DEBUG_SP_REG));
	}

	pr_debug("[SCP]GIPC     0x%x\n", readl(SCP_GIPC_IN_REG));
	pr_debug("[SCP]BUS_CTRL 0x%x\n", readl(SCP_BUS_CTRL));
	pr_debug("[SCP]SLEEP_STATUS 0x%x\n", readl(SCP_CPU_SLEEP_STATUS));
	pr_debug("[SCP]INFRA_STATUS 0x%x\n", readl(INFRA_CTRL_STATUS));
	pr_debug("[SCP]IRQ_STATUS 0x%x\n", readl(SCP_INTC_IRQ_STATUS));
	pr_debug("[SCP]IRQ_ENABLE 0x%x\n", readl(SCP_INTC_IRQ_ENABLE));
	pr_debug("[SCP]IRQ_SLEEP 0x%x\n", readl(SCP_INTC_IRQ_SLEEP));
	pr_debug("[SCP]IRQ_STATUS_MSB 0x%x\n", readl(SCP_INTC_IRQ_STATUS_MSB));
	pr_debug("[SCP]IRQ_ENABLE_MSB 0x%x\n", readl(SCP_INTC_IRQ_ENABLE_MSB));
	pr_debug("[SCP]IRQ_SLEEP_MSB 0x%x\n", readl(SCP_INTC_IRQ_SLEEP_MSB));
	pr_debug("[SCP]CLK_CTRL_SEL 0x%x\n", readl(SCP_CLK_SW_SEL));
	pr_debug("[SCP]CLK_ENABLE  0x%x\n", readl(SCP_CLK_ENABLE));
	pr_debug("[SCP]SLEEP_DEBUG 0x%x\n", readl(SCP_A_SLEEP_DEBUG_REG));

	tmp = readl(SCP_BUS_CTRL)&(~dbg_irq_info_sel_mask);
	writel(tmp | (0 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:INFRA LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
	writel(tmp | (1 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:DCACHE LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
	writel(tmp | (2 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:ICACHE LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
	writel(tmp | (3 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:PC LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
}

/*
 * save scp register when scp crash
 * these data will be used to generate EE
 */
void scp_aee_last_reg(void)
{
	pr_debug("[SCP] %s begins\n", __func__);

	scp_A_aee_status.pc = readl(SCP_A_DEBUG_PC_REG);
	scp_A_aee_status.lr = readl(SCP_A_DEBUG_LR_REG);
	scp_A_aee_status.psp = readl(SCP_A_DEBUG_PSP_REG);
	scp_A_aee_status.sp = readl(SCP_A_DEBUG_SP_REG);
	scp_A_aee_status.m2h = readl(SCP_A_TO_HOST_REG);
	scp_A_aee_status.h2m = readl(SCP_GIPC_IN_REG);

	pr_debug("[SCP] %s ends\n", __func__);
}

/*
 * this function need SCP to keeping awaken
 * scp_crash_dump: dump scp tcm info.
 * @param MemoryDump:   scp dump struct
 * @param scp_core_id:  core id
 * @return:             scp dump size
 */
static unsigned int scp_crash_dump(struct MemoryDump *pMemoryDump,
		enum scp_core_id id)
{
	unsigned int scp_dump_size;
	unsigned int scp_awake_fail_flag;

	/*flag use to indicate scp awake success or not*/
	scp_awake_fail_flag = 0;
	/*check SRAM lock ,awake scp*/
	if (scp_awake_lock(id) == -1) {
		pr_err("[SCP] %s: awake scp fail, scp id=%u\n", __func__, id);
		scp_awake_fail_flag = 1;
	}

	memcpy_from_scp((void *)&(pMemoryDump->l2tcm),
		(void *)(SCP_TCM),
		(SCP_A_TCM_SIZE));
	scp_dump_size = (SCP_A_TCM_SIZE);
	dsb(SY);
	/*check SRAM unlock*/
	if (scp_awake_fail_flag != 1) {
		if (scp_awake_unlock(id) == -1)
			pr_debug("[SCP]%s awake unlock fail, scp id=%u\n",
				__func__, id);
	}

	return scp_dump_size;
}

/*
 * generate aee argument with scp register dump
 * @param aed_str:  exception description
 * @param id:       identify scp core id
 */
static void scp_prepare_aed_dump(char *aed_str,
		enum scp_core_id id)
{
	u32 memory_dump_size;
	struct MemoryDump *md = (struct MemoryDump *) scp_dump.ramdump;
	char *scp_A_log = NULL;

	pr_debug("[SCP] %s begins:%s\n", __func__, aed_str);
	scp_aee_last_reg();	//fix me

	scp_A_log = scp_get_last_log(SCP_A_ID);

	if (scp_dump.detail_buff == NULL) {
		pr_err("[SCP AEE]detail buf is null\n");
	} else {
		/* prepare scp aee detail information*/
		memset(scp_dump.detail_buff, 0, SCP_AED_STR_LEN);

		snprintf(scp_dump.detail_buff, SCP_AED_STR_LEN,
		"%s\nscp_A pc=0x%08x, lr=0x%08x, psp=0x%08x, sp=0x%08x\n"
		"last log:\n%s",
		aed_str, scp_A_aee_status.pc,
		scp_A_aee_status.lr,
		scp_A_aee_status.psp,
		scp_A_aee_status.sp,
		scp_A_log);

		scp_dump.detail_buff[SCP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare scp A db file*/
	memory_dump_size = 0;


	pr_debug("[SCP AEE]scp A dump ptr:%p\n", md);

	memset(md, 0x0, sizeof(*md));
	memory_dump_size = scp_crash_dump(md, SCP_A_ID);

	pr_debug("[SCP] %s ends\n", __func__);
}

/*
 * generate an exception according to exception type
 * NOTE: this function may be blocked and
 * should not be called in interrupt context
 * @param type: exception type
 */
void scp_aed(enum SCP_RESET_TYPE type, enum scp_core_id id)
{
	char *scp_aed_title;

	if (!scp_ee_enable) {
		pr_debug("[SCP]ee disable value=%d\n", scp_ee_enable);
		return;
	}

	mutex_lock(&scp_excep_mutex);

	/* get scp title and exception type*/
	switch (type) {
	case RESET_TYPE_WDT:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A wdt reset";
		else
			scp_aed_title = "SCP_B wdt reset";
		break;
	case RESET_TYPE_AWAKE:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A awake reset";
		else
			scp_aed_title = "SCP_B awake reset";
		break;
	case RESET_TYPE_CMD:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A cmd reset";
		else
			scp_aed_title = "SCP_B cmd reset";
		break;
	case RESET_TYPE_TIMEOUT:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A timeout reset";
		else
			scp_aed_title = "SCP_B timeout reset";
		break;
	}
	scp_get_log(id);
	/*print scp message*/
	pr_debug("scp_aed_title=%s\n", scp_aed_title);

	scp_prepare_aed_dump(scp_aed_title, id);

	/* scp aed api, only detail information available*/
	aed_common_exception_api("scp", NULL, 0, NULL, 0,
			scp_dump.detail_buff, DB_OPT_DEFAULT);

	pr_debug("[SCP] scp exception dump is done\n");

	mutex_unlock(&scp_excep_mutex);
}



static ssize_t scp_A_dump_show(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	mutex_lock(&scp_excep_mutex);

	if (offset >= 0 && offset < scp_dump.ramdump_length) {
		if ((offset + size) > scp_dump.ramdump_length)
			size = scp_dump.ramdump_length - offset;

		memcpy(buf, scp_dump.ramdump + offset, size);
		length = size;
	}

	mutex_unlock(&scp_excep_mutex);

	return length;
}


struct bin_attribute bin_attr_scp_dump = {
	.attr = {
		.name = "scp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = scp_A_dump_show,
};



/*
 * init a work struct
 */
int scp_excep_init(void)
{
	int dram_size = 0;

	mutex_init(&scp_excep_mutex);

	/* alloc dump memory */
	scp_dump.detail_buff = vmalloc(SCP_AED_STR_LEN);
	if (!scp_dump.detail_buff)
		return -1;

	/* support L1C or not? */
	if ((int)(scp_region_info->ap_dram_size) > 0)
		dram_size = scp_region_info->ap_dram_size;

	scp_dump.ramdump = vmalloc(sizeof(struct MemoryDump) +
		roundup(dram_size, 4));
	if (!scp_dump.ramdump)
		return -1;

	/* init global values */
	scp_dump.ramdump_length = 0;
	/* 1: ee on, 0: ee disable */
	scp_ee_enable = 1;

	return 0;
}


/******************************************************************************
 * This function is called in the interrupt context. Note that scp_region_info
 * was initialized in scp_region_info_init() which must be called before this
 * function is called.
 *****************************************************************************/
void scp_ram_dump_init(void)
{
	scp_A_task_context_addr = scp_region_info->TaskContext_ptr;
	pr_debug("[SCP] get scp_A_task_context_addr: 0x%x\n",
		scp_A_task_context_addr);
}


/*
 * cleanup scp exception
 */
void scp_excep_cleanup(void)
{
	vfree(scp_dump.detail_buff);
	vfree(scp_dump.ramdump);

	scp_A_task_context_addr = 0;

	pr_debug("[SCP] %s ends\n", __func__);
}

