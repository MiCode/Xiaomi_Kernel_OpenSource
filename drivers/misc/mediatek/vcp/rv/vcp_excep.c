// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/vmalloc.h>         /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <mt-plat/aee.h>
//#include <mt-plat/sync_write.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include <linux/delay.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include "vcp.h"
#include "vcp_ipi_pin.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp_feature_define.h"
#include "vcp_l1c.h"
#include "vcp_reservedmem_define.h"
#include "vcp_status.h"

struct vcp_dump_st {
	uint8_t *detail_buff;
	uint8_t *ramdump;
	uint32_t ramdump_length;
	/* use prefix to get size or offset in O(1) to save memory */
	uint32_t prefix[MDUMP_TOTAL];
};

uint32_t vcp_reg_base_phy;

struct reg_save_st {
	uint32_t addr;
	uint32_t size;
};

struct reg_save_st reg_save_list[] = {
	/* size must 16 byte alignment */
	{0x1EC24000, 0x170},
	{0x1EC30000, 0x180},
	{0x1EC32000, 0x260},
	{0x1EC33000, 0x120},
	{0x1EC40000, 0x180},
	{0x1EC42000, 0x260},
	{0x1EC43000, 0x120},
	{0x1EC50000, 0x330},
	{0x1EC51000, 0x10},
	{0x1EC51400, 0x70},
	{0x1EC52000, 0x340},
	{0x1ECA5000, 0x110},
};

//static unsigned char *vcp_A_dump_buffer;
struct vcp_dump_st vcp_dump;

//static unsigned int vcp_A_dump_length;
static unsigned int vcp_A_task_context_addr;

struct vcp_status_reg *c0_m = NULL;
struct vcp_status_reg *c0_t1_m = NULL;
struct vcp_status_reg *c1_m = NULL;
struct vcp_status_reg *c1_t1_m = NULL;
void (*vcp_do_tbufdump)(uint32_t*, uint32_t*) = NULL;

static struct mutex vcp_excep_mutex;
int vcp_ee_enable;
unsigned int vcp_reset_counts = 0xFFFFFFFF;

#ifdef VCP_DEBUG_REMOVED
static atomic_t coredumping = ATOMIC_INIT(0);
static DECLARE_COMPLETION(vcp_coredump_comp);
static uint32_t get_MDUMP_size(MDUMP_t type)
{
	return vcp_dump.prefix[type] - vcp_dump.prefix[type - 1];
}

static uint32_t get_MDUMP_size_accumulate(MDUMP_t type)
{
	return vcp_dump.prefix[type];
}

static uint8_t *get_MDUMP_addr(MDUMP_t type)
{
	return (uint8_t *)(vcp_dump.ramdump + vcp_dump.prefix[type - 1]);
}

uint32_t memorydump_size_probe(struct platform_device *pdev)
{
	uint32_t i, ret;

	for (i = MDUMP_L2TCM; i < MDUMP_TOTAL; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
			"memorydump", i - 1, &vcp_dump.prefix[i]);
		if (ret) {
			pr_notice("[VCP] %s:Cannot get memorydump size(%d)\n", __func__, i - 1);
			return -1;
		}
		vcp_dump.prefix[i] += vcp_dump.prefix[i - 1];
	}
	return 0;
}
#endif

void vcp_dump_last_regs(int mmup_enable)
{
	uint32_t *out, *out_end;

	if (mmup_enable == 0) {
		pr_notice("[VCP] power off, do not vcp_dump_last_regs\n");
		return;
	}

	c0_m->status = readl(R_CORE0_STATUS);
	c0_m->pc = readl(R_CORE0_MON_PC);
	c0_m->lr = readl(R_CORE0_MON_LR);
	c0_m->sp = readl(R_CORE0_MON_SP);
	c0_m->pc_latch = readl(R_CORE0_MON_PC_LATCH);
	c0_m->lr_latch = readl(R_CORE0_MON_LR_LATCH);
	c0_m->sp_latch = readl(R_CORE0_MON_SP_LATCH);
	if (vcpreg.twohart) {
		c0_t1_m->pc = readl(R_CORE0_T1_MON_PC);
		c0_t1_m->lr = readl(R_CORE0_T1_MON_LR);
		c0_t1_m->sp = readl(R_CORE0_T1_MON_SP);
		c0_t1_m->pc_latch = readl(R_CORE0_T1_MON_PC_LATCH);
		c0_t1_m->lr_latch = readl(R_CORE0_T1_MON_LR_LATCH);
		c0_t1_m->sp_latch = readl(R_CORE0_T1_MON_SP_LATCH);
	}
	if (vcpreg.core_nums == 2) {
		c1_m->status = readl(R_CORE1_STATUS);
		c1_m->pc = readl(R_CORE1_MON_PC);
		c1_m->lr = readl(R_CORE1_MON_LR);
		c1_m->sp = readl(R_CORE1_MON_SP);
		c1_m->pc_latch = readl(R_CORE1_MON_PC_LATCH);
		c1_m->lr_latch = readl(R_CORE1_MON_LR_LATCH);
		c1_m->sp_latch = readl(R_CORE1_MON_SP_LATCH);
	}

	if (vcpreg.core_nums == 2 && vcpreg.twohart) {
		c1_t1_m->pc = readl(R_CORE1_T1_MON_PC);
		c1_t1_m->lr = readl(R_CORE1_T1_MON_LR);
		c1_t1_m->sp = readl(R_CORE1_T1_MON_SP);
		c1_t1_m->pc_latch = readl(R_CORE1_T1_MON_PC_LATCH);
		c1_t1_m->lr_latch = readl(R_CORE1_T1_MON_LR_LATCH);
		c1_t1_m->sp_latch = readl(R_CORE1_T1_MON_SP_LATCH);
	}

	pr_notice("[VCP] c0_status = %08x\n", c0_m->status);
	pr_notice("[VCP] c0_pc = %08x\n", c0_m->pc);
	pr_notice("[VCP] c0_pc2 = %08x\n", readl(R_CORE0_MON_PC));
	pr_notice("[VCP] c0_lr = %08x\n", c0_m->lr);
	pr_notice("[VCP] c0_sp = %08x\n", c0_m->sp);
	pr_notice("[VCP] c0_pc_latch = %08x\n", c0_m->pc_latch);
	pr_notice("[VCP] c0_lr_latch = %08x\n", c0_m->lr_latch);
	pr_notice("[VCP] c0_sp_latch = %08x\n", c0_m->sp_latch);
	if (vcpreg.twohart) {
		pr_notice("[VCP] c0_t1_pc = %08x\n", c0_t1_m->pc);
		pr_notice("[VCP] c0_t1_pc2 = %08x\n", readl(R_CORE0_T1_MON_PC));
		pr_notice("[VCP] c0_t1_lr = %08x\n", c0_t1_m->lr);
		pr_notice("[VCP] c0_t1_sp = %08x\n", c0_t1_m->sp);
		pr_notice("[VCP] c0_t1_pc_latch = %08x\n", c0_t1_m->pc_latch);
		pr_notice("[VCP] c0_t1_lr_latch = %08x\n", c0_t1_m->lr_latch);
		pr_notice("[VCP] c0_t1_sp_latch = %08x\n", c0_t1_m->sp_latch);
	}
	if (vcpreg.core_nums == 2) {
		pr_notice("[VCP] c1_status = %08x\n", c1_m->status);
		pr_notice("[VCP] c1_pc = %08x\n", c1_m->pc);
		pr_notice("[VCP] c1_pc2 = %08x\n", readl(R_CORE1_MON_PC));
		pr_notice("[VCP] c1_lr = %08x\n", c1_m->lr);
		pr_notice("[VCP] c1_sp = %08x\n", c1_m->sp);
		pr_notice("[VCP] c1_pc_latch = %08x\n", c1_m->pc_latch);
		pr_notice("[VCP] c1_lr_latch = %08x\n", c1_m->lr_latch);
		pr_notice("[VCP] c1_sp_latch = %08x\n", c1_m->sp_latch);
	}
	if (vcpreg.core_nums == 2 && vcpreg.twohart) {
		pr_notice("[VCP] c1_t1_pc = %08x\n", c1_t1_m->pc);
		pr_notice("[VCP] c1_t1_pc2 = %08x\n", readl(R_CORE1_T1_MON_PC));
		pr_notice("[VCP] c1_t1_lr = %08x\n", c1_t1_m->lr);
		pr_notice("[VCP] c1_t1_sp = %08x\n", c1_t1_m->sp);
		pr_notice("[VCP] c1_t1_pc_latch = %08x\n", c1_t1_m->pc_latch);
		pr_notice("[VCP] c1_t1_lr_latch = %08x\n", c1_t1_m->lr_latch);
		pr_notice("[VCP] c1_t1_sp_latch = %08x\n", c1_t1_m->sp_latch);
	}

	/* bus tracker reg dump */
	pr_notice("BUS DBG CON: %x\n", readl(VCP_BUS_DBG_CON));
	pr_notice("R %08x %08x %08x %08x %08x %08x %08x %08x\n",
			readl(VCP_BUS_DBG_AR_TRACK0_L),
			readl(VCP_BUS_DBG_AR_TRACK1_L),
			readl(VCP_BUS_DBG_AR_TRACK2_L),
			readl(VCP_BUS_DBG_AR_TRACK3_L),
			readl(VCP_BUS_DBG_AR_TRACK4_L),
			readl(VCP_BUS_DBG_AR_TRACK5_L),
			readl(VCP_BUS_DBG_AR_TRACK6_L),
			readl(VCP_BUS_DBG_AR_TRACK7_L)
		   );
	pr_notice("W %08x %08x %08x %08x %08x %08x %08x %08x\n",
			readl(VCP_BUS_DBG_AW_TRACK0_L),
			readl(VCP_BUS_DBG_AW_TRACK1_L),
			readl(VCP_BUS_DBG_AW_TRACK2_L),
			readl(VCP_BUS_DBG_AW_TRACK3_L),
			readl(VCP_BUS_DBG_AW_TRACK4_L),
			readl(VCP_BUS_DBG_AW_TRACK5_L),
			readl(VCP_BUS_DBG_AW_TRACK6_L),
			readl(VCP_BUS_DBG_AW_TRACK7_L)
		   );

	out = kmalloc(0x400 * sizeof(uint32_t), GFP_DMA|GFP_ATOMIC);
	if (!out)
		return;

	out_end = out + 0x400;
	pr_notice("%s at %d out:%p, out_end:%p\n", __func__, __LINE__, out, out_end);
	vcp_do_tbufdump(out, out_end);
	kfree(out);
}

void vcp_do_regdump(uint32_t *out, uint32_t *out_end)
{
#if VCP_RECOVERY_SUPPORT
	int i = 0;
	void *from;
	uint32_t *buf = out;
	int size_limit = sizeof(reg_save_list) / sizeof(struct reg_save_st);

	for (i = 0; i < size_limit; i++) {
		if (((void *)buf + reg_save_list[i].size
			+ sizeof(struct reg_save_st)) > (void *)out_end) {
			pr_notice("[VCP] %s overflow\n", __func__);
			break;
		}
		*buf = reg_save_list[i].addr;
		buf++;
		*buf = reg_save_list[i].size;
		buf++;
		from = vcp_regdump_virt + (reg_save_list[i].addr & 0xfffff);
		memcpy_from_vcp(buf, from, reg_save_list[i].size);
		buf += (reg_save_list[i].size / sizeof(uint32_t));
	}
#endif
}

#ifdef VCP_DEBUG_REMOVED
void vcp_do_l1cdump(uint32_t *out, uint32_t *out_end)
{
	uint32_t *buf = out;
	uint32_t tmp;
	uint32_t l1c_size = get_MDUMP_size(MDUMP_L1C);

	tmp = readl(R_SEC_CTRL);
	/* enable cache debug */
	writel(tmp | B_CORE0_CACHE_DBG_EN | B_CORE1_CACHE_DBG_EN, R_SEC_CTRL);
	if ((void *)buf + l1c_size > (void *)out_end) {
		pr_notice("[VCP] %s overflow\n", __func__);
		return;
	}
	memcpy_from_vcp(buf, R_CORE0_CACHE_RAM, l1c_size);
	/* disable cache debug */
	writel(tmp, R_SEC_CTRL);
}
#endif

void vcp_do_tbufdump_RV33(uint32_t *out, uint32_t *out_end)
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
		pr_notice("[VCP] C0:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2), *(out + i * 2 + 1));
	}
	for (i = 0; i < 16; i++) {
		pr_notice("[VCP] C1:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2 + 16), *(out + i * 2 + 17));
	}
}

void vcp_do_tbufdump_RV55(uint32_t *out, uint32_t *out_end)
{
	uint32_t *buf = out;
	uint32_t tmp, tmp1, index, offset, wbuf_ptr, wbuf1_ptr;
	int i;

	wbuf1_ptr = readl(R_CORE0_TBUF_WPTR);
	wbuf_ptr = wbuf1_ptr & 0x1f;
	wbuf1_ptr = wbuf1_ptr >> 8;
	tmp = readl(R_CORE0_DBG_CTRL) & M_CORE_TBUF_DBG_SEL_RV55;
	for (i = 0; i < 32; i++) {
		index = ((wbuf_ptr + i) / 4) & 0x7;
		offset = ((wbuf_ptr + i) % 4) * 0x8;
		tmp1 = (index << S_CORE_TBUF_S) | (index << S_CORE_TBUF1_S);
		writel(tmp | tmp1, R_CORE0_DBG_CTRL);
		*(buf) = readl(R_CORE0_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE0_TBUF_DATA63_32 + offset);
		buf += 2;
	}
	for (i = 0; i < 32; i++) {
		pr_notice("[VCP] C0:H0:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2), *(out + i * 2 + 1));
	}
	for (i = 0; i < 32; i++) {
		index = (((wbuf1_ptr + i) / 4) & 0x7) | 0x8;
		offset = ((wbuf1_ptr + i) % 4) * 0x8;
		tmp1 = (index << S_CORE_TBUF_S) | (index << S_CORE_TBUF1_S);
		writel(tmp | tmp1, R_CORE0_DBG_CTRL);
		*(buf) = readl(R_CORE0_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE0_TBUF_DATA63_32 + offset);
		buf += 2;
	}
	for (i = 0; i < 32; i++) {
		pr_notice("[VCP] C0:H1:%02d:0x%08x::0x%08x\n",
			i, *(out + 64 + i * 2), *(out + 64 + i * 2 + 1));
	}

	if (vcpreg.core_nums == 1)
		return;

	wbuf1_ptr = readl(R_CORE1_TBUF_WPTR);
	wbuf_ptr = wbuf1_ptr & 0x1f;
	wbuf1_ptr = wbuf1_ptr >> 8;
	tmp = readl(R_CORE1_DBG_CTRL) & M_CORE_TBUF_DBG_SEL_RV55;
	for (i = 0; i < 32; i++) {
		index = ((wbuf_ptr + i) / 4) & 0x7;
		offset = ((wbuf_ptr + i) % 4) * 0x8;
		tmp1 = (index << S_CORE_TBUF_S) | (index << S_CORE_TBUF1_S);
		writel(tmp | tmp1, R_CORE1_DBG_CTRL);
		*(buf) = readl(R_CORE1_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE1_TBUF_DATA63_32 + offset);
		buf += 2;
	}
	for (i = 0; i < 32; i++) {
		pr_notice("[VCP] C1:H0:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2), *(out + i * 2 + 1));
	}
	for (i = 0; i < 32; i++) {
		index = (((wbuf1_ptr + i) / 4) & 0x7) | 0x8;
		offset = ((wbuf1_ptr + i) % 4) * 0x8;
		tmp1 = (index << S_CORE_TBUF_S) | (index << S_CORE_TBUF1_S);
		writel(tmp | tmp1, R_CORE1_DBG_CTRL);
		*(buf) = readl(R_CORE1_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE1_TBUF_DATA63_32 + offset);
		buf += 2;
	}
	for (i = 0; i < 32; i++) {
		pr_notice("[VCP] C1:H1:%02d:0x%08x::0x%08x\n",
			i, *(out + 64 + i * 2), *(out + 64 + i * 2 + 1));
	}
}

/*
 * this function need VCP to keeping awaken
 * vcp_crash_dump: dump vcp tcm info.
 * @param MemoryDump:   vcp dump struct
 * @param vcp_core_id:  core id
 * @return:             vcp dump size
 */
static unsigned int vcp_crash_dump(enum vcp_core_id id)
{
	unsigned int vcp_dump_size = 0;
	unsigned int vcp_awake_fail_flag;
#ifdef VCP_DEBUG_REMOVED
	uint32_t dram_start = 0;
	uint32_t dram_size = 0;
#endif

	/*flag use to indicate vcp awake success or not*/
	vcp_awake_fail_flag = 0;

	/*check SRAM lock ,awake vcp*/
	if (vcp_awake_lock((void *)id) == -1) {
		pr_notice("[VCP] %s: awake vcp fail, vcp id=%u\n", __func__, id);
		vcp_awake_fail_flag = 1;
	}
#ifdef VCP_DEBUG_REMOVED
	//TO DO: SMC aee dump
	memcpy_from_vcp((void *)get_MDUMP_addr(MDUMP_L2TCM),
		(void *)(VCP_TCM),
		(VCP_A_TCM_SIZE));
	vcp_do_l1cdump((void *)get_MDUMP_addr(MDUMP_L1C),
		(void *)get_MDUMP_addr(MDUMP_REGDUMP));
	/* dump sys registers */
	vcp_do_regdump((void *)&get_MDUMP_addr(MDUMP_REGDUMP),
		(void *)get_MDUMP_addr(MDUMP_TBUF));
	vcp_do_tbufdump((void *)get_MDUMP_addr(MDUMP_TBUF),
		(void *)get_MDUMP_addr(MDUMP_DRAM));
	vcp_dump_size = get_MDUMP_size_accumulate(MDUMP_TBUF);

	/* dram support? */
	if ((int)(vcp_region_info_copy.ap_dram_size) <= 0) {
		pr_notice("[vcp] ap_dram_size <=0\n");
	} else {
		dram_start = vcp_region_info_copy.ap_dram_start;
		dram_size = vcp_region_info_copy.ap_dram_size;
		/* copy dram data*/
		memcpy((void *)&(pMemoryDump->dram),
			vcp_ap_dram_virt, dram_size);
		vcp_dump_size += roundup(dram_size, 4);
	}
#endif

	dsb(SY); /* may take lot of time */

	/*check SRAM unlock*/
	if (vcp_awake_fail_flag != 1) {
		if (vcp_awake_unlock((void *)id) == -1)
			pr_debug("[VCP]%s awake unlock fail, vcp id=%u\n",
				__func__, id);
	}

	return vcp_dump_size;
}

/*
 * generate aee argument with vcp register dump
 * @param aed_str:  exception description
 * @param id:       identify vcp core id
 */
static void vcp_prepare_aed_dump(char *aed_str, enum vcp_core_id id)
{
	char *vcp_A_log = NULL;
	int offset = 0;

	pr_debug("[VCP] %s begins:%s\n", __func__, aed_str);
	vcp_dump_last_regs(mmup_enable_count());

	vcp_A_log = vcp_pickup_log_for_aee();

	if (vcp_dump.detail_buff == NULL) {
		pr_notice("[VCP AEE]detail buf is null\n");
	} else {
		/* prepare vcp aee detail information*/
		memset(vcp_dump.detail_buff, 0, VCP_AED_STR_LEN);

		offset = snprintf(vcp_dump.detail_buff + offset,
			VCP_AED_STR_LEN - offset, "%s\n", aed_str);
		if (offset < 0)
			pr_notice("%s line %d error\n", __func__, __LINE__);
		offset = snprintf(vcp_dump.detail_buff + offset,
			VCP_AED_STR_LEN - offset,
			"core0 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
			c0_m->pc, c0_m->lr, c0_m->sp);
		if (offset < 0)
			pr_notice("%s line %d error\n", __func__, __LINE__);

		if (!vcpreg.twohart)
			goto core1;

		offset = snprintf(vcp_dump.detail_buff + offset,
			VCP_AED_STR_LEN - offset,
			"hart1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
			c0_t1_m->pc, c0_t1_m->lr, c0_t1_m->sp);
		if (offset < 0)
			pr_notice("%s line %d error\n", __func__, __LINE__);
core1:
		if (vcpreg.core_nums == 1)
			goto end;

		offset = snprintf(vcp_dump.detail_buff + offset,
			VCP_AED_STR_LEN - offset,
			"core1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
			c1_m->pc, c1_m->lr, c1_m->sp);
		if (offset < 0)
			pr_notice("%s line %d error\n", __func__, __LINE__);

		if (!vcpreg.twohart)
			goto end;

		offset = snprintf(vcp_dump.detail_buff + offset,
			VCP_AED_STR_LEN - offset,
			"hart1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
			c1_t1_m->pc, c1_t1_m->lr, c1_t1_m->sp);
		if (offset < 0)
			pr_notice("%s line %d error\n", __func__, __LINE__);
end:
		if (snprintf(vcp_dump.detail_buff + offset,
				VCP_AED_STR_LEN - offset, "last log:\n%s", vcp_A_log) < 0)
			pr_notice("%s line %d error\n", __func__, __LINE__);

		vcp_dump.detail_buff[VCP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare vcp A db file*/
	vcp_dump.ramdump_length = 0;
	vcp_dump.ramdump_length = vcp_crash_dump(VCP_A_ID);

	pr_notice("[VCP] %s ends, @%p, size = %x\n", __func__,
		vcp_dump.ramdump, vcp_dump.ramdump_length);
}

/*
 * generate an exception according to exception type
 * NOTE: this function may be blocked and
 * should not be called in interrupt context
 * @param type: exception type
 */
void vcp_aed(enum VCP_RESET_TYPE type, enum vcp_core_id id)
{
	char *vcp_aed_title = NULL;

	if (!vcp_ee_enable) {
		pr_debug("[VCP]ee disable value=%d\n", vcp_ee_enable);
		return;
	}

	mutex_lock(&vcp_excep_mutex);

	/* get vcp title and exception type*/
	switch (type) {
	case RESET_TYPE_WDT:
		if (id == VCP_A_ID)
			vcp_aed_title = "VCP_A wdt reset";
		else
			vcp_aed_title = "VCP_B wdt reset";
		break;
	case RESET_TYPE_AWAKE:
		if (id == VCP_A_ID)
			vcp_aed_title = "VCP_A awake reset";
		else
			vcp_aed_title = "VCP_B awake reset";
		break;
	case RESET_TYPE_CMD:
		if (id == VCP_A_ID)
			vcp_aed_title = "VCP_A cmd reset";
		else
			vcp_aed_title = "VCP_B cmd reset";
		break;
	case RESET_TYPE_TIMEOUT:
		if (id == VCP_A_ID)
			vcp_aed_title = "VCP_A timeout reset";
		else
			vcp_aed_title = "VCP_B timeout reset";
		break;
	}
	vcp_get_log(id);
	/*print vcp message*/
	pr_debug("vcp_aed_title=%s\n", vcp_aed_title);

	vcp_prepare_aed_dump(vcp_aed_title, id);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	/* vcp aed api, only detail information available*/
	aed_common_exception_api("vcp", NULL, 0, NULL, 0,
			vcp_dump.detail_buff, DB_OPT_DEFAULT);
#endif

	pr_debug("[VCP] vcp exception dump is done\n");

	mutex_unlock(&vcp_excep_mutex);
}



static ssize_t vcp_A_dump_show(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	mutex_lock(&vcp_excep_mutex);

	if (offset >= 0 && offset < vcp_dump.ramdump_length) {
		if ((offset + size) > vcp_dump.ramdump_length)
			size = vcp_dump.ramdump_length - offset;

		memcpy(buf, vcp_dump.ramdump + offset, size);
		length = size;

		/* clean the buff after readed */
		memset(vcp_dump.ramdump + offset, 0x0, size);
		/* log for the first and latest cleanup */
		if (offset == 0 || size == (vcp_dump.ramdump_length - offset))
			pr_notice("[VCP] %s ramdump cleaned of:0x%llx sz:0x%zx\n", __func__,
				offset, size);
#ifdef VCP_DEBUG_REMOVED
		/* the last time read vcp_dump buffer has done
		 * so the next coredump flow can be continued
		 */
		if (size == vcp_dump.ramdump_length - offset) {
			atomic_set(&coredumping, false);
			pr_notice("[VCP] coredumping:%d, coredump complete\n",
				atomic_read(&coredumping));
			complete(&vcp_coredump_comp);
		}
#endif
	}

	mutex_unlock(&vcp_excep_mutex);

	return length;
}


struct bin_attribute bin_attr_vcp_dump = {
	.attr = {
		.name = "vcp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = vcp_A_dump_show,
};



/*
 * init a work struct
 */
int vcp_excep_init(void)
{
	mutex_init(&vcp_excep_mutex);

	/* alloc dump memory */
	vcp_dump.detail_buff = vmalloc(VCP_AED_STR_LEN);
	if (!vcp_dump.detail_buff)
		return -1;

	/* vcp_status_reg init */
	c0_m = vmalloc(sizeof(struct vcp_status_reg));
	if (!c0_m)
		return -1;
	if (vcpreg.twohart) {
		c0_t1_m = vmalloc(sizeof(struct vcp_status_reg));
		if (!c0_t1_m)
			return -1;
	}
	if (vcpreg.core_nums == 2) {
		c1_m = vmalloc(sizeof(struct vcp_status_reg));
		if (!c1_m)
			return -1;
	}
	if (vcpreg.core_nums == 2 && vcpreg.twohart) {
		c1_t1_m = vmalloc(sizeof(struct vcp_status_reg));
		if (!c1_t1_m)
			return -1;
	}
	/* vcp_do_tbufdump init, because tbuf is different between rv33/rv55 */
	if (vcpreg.twohart)
		vcp_do_tbufdump = vcp_do_tbufdump_RV55;
	else
		vcp_do_tbufdump = vcp_do_tbufdump_RV33;

	/* init global values */
	vcp_dump.ramdump_length = 0;
	/* 1: ee on, 0: ee disable */
	vcp_ee_enable = 0;

	return 0;
}


/******************************************************************************
 * This function is called in the interrupt context. Note that vcp_region_info
 * was initialized in vcp_region_info_init() which must be called before this
 * function is called.
 *****************************************************************************/
void vcp_ram_dump_init(void)
{
#ifdef VCP_DEBUG_REMOVED
	vcp_A_task_context_addr = vcp_region_info->TaskContext_ptr;
	pr_debug("[VCP] get vcp_A_task_context_addr: 0x%x\n",
		vcp_A_task_context_addr);
#endif
}


/*
 * cleanup vcp exception
 */
void vcp_excep_cleanup(void)
{
	vfree(vcp_dump.detail_buff);
	vcp_A_task_context_addr = 0;

	pr_debug("[VCP] %s ends\n", __func__);
}

