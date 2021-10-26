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

struct scp_dump_st {
	uint8_t *detail_buff;
	uint8_t *ramdump;
	uint32_t ramdump_length;
};

struct reg_save_st {
	uint32_t addr;
	uint32_t size;
};

struct reg_save_st reg_save_list[] = {
	/* size must 16 byte alignment */
	{0x10721000, 0x120},
	{0x10724000, 0x170},
	{0x10730000, 0x120},
	{0x10732000, 0x260},
	{0x10733000, 0x120},
	{0x10740000, 0x120},
	{0x10742000, 0x260},
	{0x10743000, 0x120},
	{0x10750000, 0x330},
	{0x10751000, 0x10},
	{0x10751400, 0x70},
	{0x10752000, 0x340},
	{0x107A5000, 0x110},
	{0x10001B14, 0x10},
};

//static unsigned char *scp_A_dump_buffer;
struct scp_dump_st scp_dump;

//static unsigned int scp_A_dump_length;
static unsigned int scp_A_task_context_addr;

struct scp_status_reg c0_m;
struct scp_status_reg c1_m;

static struct mutex scp_excep_mutex;
int scp_ee_enable;
int scp_reset_counts = 100000;

void scp_dump_last_regs(void)
{
	c0_m.status = readl(R_CORE0_STATUS);
	c0_m.pc = readl(R_CORE0_MON_PC);
	c0_m.lr = readl(R_CORE0_MON_LR);
	c0_m.sp = readl(R_CORE0_MON_SP);
	c0_m.pc_latch = readl(R_CORE0_MON_PC_LATCH);
	c0_m.lr_latch = readl(R_CORE0_MON_LR_LATCH);
	c0_m.sp_latch = readl(R_CORE0_MON_SP_LATCH);
	c1_m.status = readl(R_CORE1_STATUS);
	c1_m.pc = readl(R_CORE1_MON_PC);
	c1_m.lr = readl(R_CORE1_MON_LR);
	c1_m.sp = readl(R_CORE1_MON_SP);
	c1_m.pc_latch = readl(R_CORE1_MON_PC_LATCH);
	c1_m.lr_latch = readl(R_CORE1_MON_LR_LATCH);
	c1_m.sp_latch = readl(R_CORE1_MON_SP_LATCH);

	pr_debug("[SCP] c0_status = %08x\n", c0_m.status);
	pr_debug("[SCP] c0_pc = %08x\n", c0_m.pc);
	pr_debug("[SCP] c0_lr = %08x\n", c0_m.lr);
	pr_debug("[SCP] c0_sp = %08x\n", c0_m.sp);
	pr_debug("[SCP] c0_pc_latch = %08x\n", c0_m.pc_latch);
	pr_debug("[SCP] c0_lr_latch = %08x\n", c0_m.lr_latch);
	pr_debug("[SCP] c0_sp_latch = %08x\n", c0_m.sp_latch);
	pr_debug("[SCP] c1_status = %08x\n", c1_m.status);
	pr_debug("[SCP] c1_pc = %08x\n", c1_m.pc);
	pr_debug("[SCP] c1_lr = %08x\n", c1_m.lr);
	pr_debug("[SCP] c1_sp = %08x\n", c1_m.sp);
	pr_debug("[SCP] c1_pc_latch = %08x\n", c1_m.pc_latch);
	pr_debug("[SCP] c1_lr_latch = %08x\n", c1_m.lr_latch);
	pr_debug("[SCP] c1_sp_latch = %08x\n", c1_m.sp_latch);

	/* bus tracker reg dump */
	pr_debug("BUS DBG CON: %x\n", readl(SCP_BUS_DBG_CON));
	pr_debug("R %08x %08x %08x %08x %08x %08x %08x %08x\n",
			readl(SCP_BUS_DBG_AR_TRACK0_L),
			readl(SCP_BUS_DBG_AR_TRACK1_L),
			readl(SCP_BUS_DBG_AR_TRACK2_L),
			readl(SCP_BUS_DBG_AR_TRACK3_L),
			readl(SCP_BUS_DBG_AR_TRACK4_L),
			readl(SCP_BUS_DBG_AR_TRACK5_L),
			readl(SCP_BUS_DBG_AR_TRACK6_L),
			readl(SCP_BUS_DBG_AR_TRACK7_L)
		   );
	pr_debug("W %08x %08x %08x %08x %08x %08x %08x %08x\n",
			readl(SCP_BUS_DBG_AW_TRACK0_L),
			readl(SCP_BUS_DBG_AW_TRACK1_L),
			readl(SCP_BUS_DBG_AW_TRACK2_L),
			readl(SCP_BUS_DBG_AW_TRACK3_L),
			readl(SCP_BUS_DBG_AW_TRACK4_L),
			readl(SCP_BUS_DBG_AW_TRACK5_L),
			readl(SCP_BUS_DBG_AW_TRACK6_L),
			readl(SCP_BUS_DBG_AW_TRACK7_L)
		   );
}

void scp_do_regdump(uint32_t *out, uint32_t *out_end)
{
	int i = 0;
	void *from;
	uint32_t *buf = out;
	int size_limit = sizeof(reg_save_list) / sizeof(struct reg_save_st);

	for (i = 0; i < size_limit; i++) {
		if (((void *)buf + reg_save_list[i].size
			+ sizeof(struct reg_save_st)) > (void *)out_end) {
			pr_notice("[SCP] %s overflow\n", __func__);
			break;
		}
		*buf = reg_save_list[i].addr;
		buf++;
		*buf = reg_save_list[i].size;
		buf++;
		from = scp_regdump_virt + (reg_save_list[i].addr & 0xfffff);
		if ((reg_save_list[i].addr & 0xfff00000) < 0x10700000)
			from = scpreg.scpsys + (reg_save_list[i].addr & 0xfff);
		memcpy_from_scp(buf, from, reg_save_list[i].size);
		buf += (reg_save_list[i].size / sizeof(uint32_t));
	}
}

void scp_do_l1cdump(uint32_t *out, uint32_t *out_end)
{
	uint32_t *buf = out;
	uint32_t tmp;

	tmp = readl(R_SEC_CTRL);
	/* enable cache debug */
	writel(tmp | B_CORE0_CACHE_DBG_EN | B_CORE1_CACHE_DBG_EN, R_SEC_CTRL);
	if ((void *)buf + MDUMP_L1C_SIZE > (void *)out_end) {
		pr_notice("[SCP] %s overflow\n", __func__);
		return;
	}
	memcpy_from_scp(buf, R_CORE0_CACHE_RAM, MDUMP_L1C_SIZE);
	/* disable cache debug */
	writel(tmp, R_SEC_CTRL);
}

void scp_do_tbufdump(uint32_t *out, uint32_t *out_end)
{
	uint32_t *buf = out;
	uint32_t tmp, index, offset, wbuf_ptr;
	int i;

	wbuf_ptr = readl(R_CORE0_TBUF_WPTR);
	tmp = readl(R_CORE0_DBG_CTRL) & (~M_CORE_TBUF_DBG_SEL);
	for (i = 0; i < 16; i++) {
		index = (wbuf_ptr + i) / 2;
		offset = ((wbuf_ptr + i) % 2) * 0x8;
		writel(tmp | (index << S_CORE_TBUF_DBG_SEL), R_CORE0_DBG_CTRL);
		*(buf) = readl(R_CORE0_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE0_TBUF_DATA63_32 + offset);
		buf += 2;
	}

	wbuf_ptr = readl(R_CORE1_TBUF_WPTR);
	tmp = readl(R_CORE1_DBG_CTRL) & (~M_CORE_TBUF_DBG_SEL);
	for (i = 0; i < 16; i++) {
		index = (wbuf_ptr + i) / 2;
		offset = ((wbuf_ptr + i) % 2) * 0x8;
		writel(tmp | (index << S_CORE_TBUF_DBG_SEL), R_CORE1_DBG_CTRL);
		*(buf) = readl(R_CORE1_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE1_TBUF_DATA63_32 + offset);
		buf += 2;
	}

	for (i = 0; i < 16; i++) {
		pr_notice("[SCP] C0:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2), *(out + i * 2 + 1));
	}
	for (i = 0; i < 16; i++) {
		pr_notice("[SCP] C1:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2 + 16), *(out + i * 2 + 17));
	}
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
	uint32_t dram_start = 0;
	uint32_t dram_size = 0;

	/*flag use to indicate scp awake success or not*/
	scp_awake_fail_flag = 0;

	/*check SRAM lock ,awake scp*/
	if (scp_awake_lock((void *)id) == -1) {
		pr_err("[SCP] %s: awake scp fail, scp id=%u\n", __func__, id);
		scp_awake_fail_flag = 1;
	}

	memcpy_from_scp((void *)&(pMemoryDump->l2tcm),
		(void *)(SCP_TCM),
		(SCP_A_TCM_SIZE));
	scp_do_l1cdump((void *)&(pMemoryDump->l1c),
		(void *)&(pMemoryDump->regdump));

	/* dump sys registers */
	scp_do_regdump((void *)&(pMemoryDump->regdump),
		(void *)&(pMemoryDump->tbuf));
	scp_do_tbufdump((void *)&(pMemoryDump->tbuf),
		(void *)&(pMemoryDump->dram));

	scp_dump_size = MDUMP_L2TCM_SIZE + MDUMP_L1C_SIZE
		+ MDUMP_REGDUMP_SIZE + MDUMP_TBUF_SIZE;

	/* dram support? */
	if ((int)(scp_region_info->ap_dram_size) <= 0) {
		pr_notice("[scp] ap_dram_size <=0\n");
	} else {
		dram_start = scp_region_info_copy.ap_dram_start;
		dram_size = scp_region_info_copy.ap_dram_size;
		/* copy dram data*/
		memcpy((void *)&(pMemoryDump->dram),
			scp_ap_dram_virt, dram_size);
		scp_dump_size += roundup(dram_size, 4);
	}

	dsb(SY); /* may take lot of time */

	/*check SRAM unlock*/
	if (scp_awake_fail_flag != 1) {
		if (scp_awake_unlock((void *)id) == -1)
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
	struct MemoryDump *md = (struct MemoryDump *) scp_dump.ramdump;
	char *scp_A_log = NULL;

	pr_debug("[SCP] %s begins:%s\n", __func__, aed_str);
	scp_dump_last_regs();

	scp_A_log = scp_pickup_log_for_aee();

	if (scp_dump.detail_buff == NULL) {
		pr_err("[SCP AEE]detail buf is null\n");
	} else {
		/* prepare scp aee detail information*/
		memset(scp_dump.detail_buff, 0, SCP_AED_STR_LEN);

		snprintf(scp_dump.detail_buff, SCP_AED_STR_LEN,
		"%s\ncore0 pc=0x%08x, lr=0x%08x, sp=0x%08x\n"
		"core1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n"
		"last log:\n%s",
		aed_str, c0_m.pc, c0_m.lr, c0_m.sp,
		c1_m.pc, c1_m.lr, c1_m.sp, scp_A_log);

		scp_dump.detail_buff[SCP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare scp A db file*/
	scp_dump.ramdump_length = 0;
	memset(md, 0x0, sizeof(*md));
	scp_dump.ramdump_length = scp_crash_dump(md, SCP_A_ID);

	pr_notice("[SCP] %s ends, @%px, size = %x\n", __func__,
		md, scp_dump.ramdump_length);
}

/*
 * generate an exception according to exception type
 * NOTE: this function may be blocked and
 * should not be called in interrupt context
 * @param type: exception type
 */
void scp_aed(enum SCP_RESET_TYPE type, enum scp_core_id id)
{
	char *scp_aed_title = NULL;

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

	scp_dump.ramdump = vmalloc(sizeof(struct MemoryDump));
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

