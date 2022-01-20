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
#include <linux/of.h> /* probe dts */
#include "scp.h"
#include "scp_ipi_pin.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_feature_define.h"
#include "scp_l1c.h"

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
#include <linux/of_reserved_mem.h>
#include "scp_reservedmem_define.h"
#endif

#define SCP_SECURE_DUMP_MEASURE 0
#define POLLING_RETRY 200
#define SCP_SECURE_DUMP_DEBUG 1
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM) && SCP_SECURE_DUMP_MEASURE
	struct cal {
		uint64_t start, end;
		int type;	//1:all 2:dump 3:polling
	} scpdump_cal[POLLING_RETRY+1];
#endif

struct scp_dump_st {
	uint8_t *detail_buff;
	uint8_t *ramdump;
	uint32_t ramdump_length;
	/* use prefix to get size or offset in O(1) to save memory */
	uint32_t prefix[MDUMP_TOTAL];
};

uint32_t scp_reg_base_phy;

struct reg_save_st {
	uint32_t addr;
	uint32_t size;
};

struct reg_save_st reg_save_list[] = {
	/* size must 16 byte alignment */
	{0x00021000, 0x120},
	{0x00024000, 0x170},
	{0x00030000, 0x180},
	{0x00032000, 0x260},
	{0x00033000, 0x120},
	{0x00040000, 0x180},
	{0x00042000, 0x260},
	{0x00043000, 0x120},
	{0x00050000, 0x330},
	{0x00051000, 0x10},
	{0x00051400, 0x70},
	{0x00052000, 0x400},
	{0x000A5000, 0x110},
	{0x10001B14, 0x10},
};

//static unsigned char *scp_A_dump_buffer;
struct scp_dump_st scp_dump;

//static unsigned int scp_A_dump_length;
static unsigned int scp_A_task_context_addr;

struct scp_status_reg *c0_m = NULL;
struct scp_status_reg *c0_t1_m = NULL;
struct scp_status_reg *c1_m = NULL;
struct scp_status_reg *c1_t1_m = NULL;
void (*scp_do_tbufdump)(uint32_t*, uint32_t*) = NULL;

int scp_ee_enable;
int scp_reset_counts = 100000;
static atomic_t coredumping = ATOMIC_INIT(0);
static DECLARE_COMPLETION(scp_coredump_comp);
static uint32_t get_MDUMP_size(MDUMP_t type)
{
	return scp_dump.prefix[type] - scp_dump.prefix[type - 1];
}

static uint32_t get_MDUMP_size_accumulate(MDUMP_t type)
{
	return scp_dump.prefix[type];
}

static uint8_t* get_MDUMP_addr(MDUMP_t type)
{
	return (uint8_t*)(scp_dump.ramdump + scp_dump.prefix[type - 1]);
}

uint32_t memorydump_size_probe(struct platform_device *pdev)
{
	uint32_t i, ret;
	for (i = MDUMP_L2TCM; i < MDUMP_TOTAL; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
			"memorydump", i - 1, &scp_dump.prefix[i]);
		if (ret) {
			pr_notice("[SCP] %s:Cannot get memorydump size(%d)\n", __func__, i - 1);
			return -1;
		}
		scp_dump.prefix[i] += scp_dump.prefix[i - 1];
	}
	return 0;
}

void scp_dump_last_regs(void)
{
	c0_m->status = readl(R_CORE0_STATUS);
	c0_m->pc = readl(R_CORE0_MON_PC);
	c0_m->lr = readl(R_CORE0_MON_LR);
	c0_m->sp = readl(R_CORE0_MON_SP);
	c0_m->pc_latch = readl(R_CORE0_MON_PC_LATCH);
	c0_m->lr_latch = readl(R_CORE0_MON_LR_LATCH);
	c0_m->sp_latch = readl(R_CORE0_MON_SP_LATCH);
	if (scpreg.twohart) {
		c0_t1_m->pc = readl(R_CORE0_T1_MON_PC);
		c0_t1_m->lr = readl(R_CORE0_T1_MON_LR);
		c0_t1_m->sp = readl(R_CORE0_T1_MON_SP);
		c0_t1_m->pc_latch = readl(R_CORE0_T1_MON_PC_LATCH);
		c0_t1_m->lr_latch = readl(R_CORE0_T1_MON_LR_LATCH);
		c0_t1_m->sp_latch = readl(R_CORE0_T1_MON_SP_LATCH);
	}
	if (scpreg.core_nums == 2) {
		c1_m->status = readl(R_CORE1_STATUS);
		c1_m->pc = readl(R_CORE1_MON_PC);
		c1_m->lr = readl(R_CORE1_MON_LR);
		c1_m->sp = readl(R_CORE1_MON_SP);
		c1_m->pc_latch = readl(R_CORE1_MON_PC_LATCH);
		c1_m->lr_latch = readl(R_CORE1_MON_LR_LATCH);
		c1_m->sp_latch = readl(R_CORE1_MON_SP_LATCH);
	}

	if (scpreg.core_nums == 2 && scpreg.twohart) {
		c1_t1_m->pc = readl(R_CORE1_T1_MON_PC);
		c1_t1_m->lr = readl(R_CORE1_T1_MON_LR);
		c1_t1_m->sp = readl(R_CORE1_T1_MON_SP);
		c1_t1_m->pc_latch = readl(R_CORE1_T1_MON_PC_LATCH);
		c1_t1_m->lr_latch = readl(R_CORE1_T1_MON_LR_LATCH);
		c1_t1_m->sp_latch = readl(R_CORE1_T1_MON_SP_LATCH);
	}
	scp_dump_bus_tracker_status();
}

void scp_show_last_regs(void)
{
	pr_notice("[SCP] c0h0_status = %08x\n", c0_m->status);
	pr_notice("[SCP] c0h0_pc = %08x\n", c0_m->pc);
	pr_notice("[SCP] c0h0_lr = %08x\n", c0_m->lr);
	pr_notice("[SCP] c0h0_sp = %08x\n", c0_m->sp);
	pr_notice("[SCP] c0h0_pc_latch = %08x\n", c0_m->pc_latch);
	pr_notice("[SCP] c0h0_lr_latch = %08x\n", c0_m->lr_latch);
	pr_notice("[SCP] c0h0_sp_latch = %08x\n", c0_m->sp_latch);
	if (scpreg.twohart) {
		pr_notice("[SCP] c0h1_pc = %08x\n", c0_t1_m->pc);
		pr_notice("[SCP] c0h1_lr = %08x\n", c0_t1_m->lr);
		pr_notice("[SCP] c0h1_sp = %08x\n", c0_t1_m->sp);
		pr_notice("[SCP] c0h1_pc_latch = %08x\n", c0_t1_m->pc_latch);
		pr_notice("[SCP] c0h1_lr_latch = %08x\n", c0_t1_m->lr_latch);
		pr_notice("[SCP] c0h1_sp_latch = %08x\n", c0_t1_m->sp_latch);
	}
	if (scpreg.core_nums == 2) {
		pr_notice("[SCP] c1h0_status = %08x\n", c1_m->status);
		pr_notice("[SCP] c1h0_pc = %08x\n", c1_m->pc);
		pr_notice("[SCP] c1h0_lr = %08x\n", c1_m->lr);
		pr_notice("[SCP] c1h0_sp = %08x\n", c1_m->sp);
		pr_notice("[SCP] c1h0_pc_latch = %08x\n", c1_m->pc_latch);
		pr_notice("[SCP] c1h0_lr_latch = %08x\n", c1_m->lr_latch);
		pr_notice("[SCP] c1h0_sp_latch = %08x\n", c1_m->sp_latch);
	}
	if (scpreg.core_nums == 2 && scpreg.twohart) {
		pr_notice("[SCP] c1h1_pc = %08x\n", c1_t1_m->pc);
		pr_notice("[SCP] c1h1_lr = %08x\n", c1_t1_m->lr);
		pr_notice("[SCP] c1h1_sp = %08x\n", c1_t1_m->sp);
		pr_notice("[SCP] c1h1_pc_latch = %08x\n", c1_t1_m->pc_latch);
		pr_notice("[SCP] c1h1_lr_latch = %08x\n", c1_t1_m->lr_latch);
		pr_notice("[SCP] c1h1_sp_latch = %08x\n", c1_t1_m->sp_latch);
	}
	scp_show_bus_tracker_status();
}

void scp_dump_bus_tracker_status(void)
{
	uint32_t offset;
	uint64_t __iomem *bus_dbg_read_l;
	uint64_t __iomem *bus_dbg_write_l;
	int i, j;
	struct scp_bus_tracker_status *bus_tracker;

	bus_tracker = &scpreg.tracker_status;
	bus_tracker->dbg_con = readl(SCP_BUS_DBG_CON);
	for (i = 3; i >= 0; --i) {
		offset = i << 3;
		bus_dbg_read_l = ((uint64_t *)SCP_BUS_DBG_AR_TRACK0_L) + offset;
		bus_dbg_write_l = ((uint64_t *)SCP_BUS_DBG_AW_TRACK0_L) + offset;
		if (!readl(bus_dbg_read_l + 7) && !readl(bus_dbg_write_l + 7))
			continue;

		for (j = 7; j >= 0; --j) {
			bus_tracker->dbg_r[offset + j] =
				readl(bus_dbg_read_l + j);
			bus_tracker->dbg_w[offset + j] =
				readl(bus_dbg_write_l + j);
		}
	}
}

void scp_show_bus_tracker_status(void)
{
	uint32_t offset;
	int i;
	struct scp_bus_tracker_status *bus_tracker;

	bus_tracker = &scpreg.tracker_status;
	pr_notice("BUS DBG CON: %x\n", bus_tracker->dbg_con);
	for (i = 3; i >= 0; --i) {
		offset = i << 3;
		if (!bus_tracker->dbg_r[offset + 7] && !bus_tracker->dbg_r[offset + 7])
			continue;
		pr_notice("R[%u-%u] %08x %08x %08x %08x %08x %08x %08x %08x\n",
				offset, offset + 7,
				bus_tracker->dbg_r[offset],
				bus_tracker->dbg_r[offset + 1],
				bus_tracker->dbg_r[offset + 2],
				bus_tracker->dbg_r[offset + 3],
				bus_tracker->dbg_r[offset + 4],
				bus_tracker->dbg_r[offset + 5],
				bus_tracker->dbg_r[offset + 6],
				bus_tracker->dbg_r[offset + 7]);
		pr_notice("W[%u-%u] %08x %08x %08x %08x %08x %08x %08x %08x\n",
				offset, offset + 7,
				bus_tracker->dbg_w[offset],
				bus_tracker->dbg_w[offset + 1],
				bus_tracker->dbg_w[offset + 2],
				bus_tracker->dbg_w[offset + 3],
				bus_tracker->dbg_w[offset + 4],
				bus_tracker->dbg_w[offset + 5],
				bus_tracker->dbg_w[offset + 6],
				bus_tracker->dbg_w[offset + 7]);
		}
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
	uint32_t l1c_size = get_MDUMP_size(MDUMP_L1C);

	tmp = readl(R_SEC_CTRL);
	/* enable cache debug */
	writel(tmp | B_CORE0_CACHE_DBG_EN | B_CORE1_CACHE_DBG_EN, R_SEC_CTRL);
	if ((void *)buf + l1c_size > (void *)out_end) {
		pr_notice("[SCP] %s overflow\n", __func__);
		return;
	}
	memcpy_from_scp(buf, R_CORE0_CACHE_RAM, l1c_size);
	/* disable cache debug */
	writel(tmp, R_SEC_CTRL);
}

void scp_do_tbufdump_RV33(uint32_t *out, uint32_t *out_end)
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

void scp_do_tbufdump_RV55(uint32_t *out, uint32_t *out_end)
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
		pr_notice("[SCP] C0:H0:%02d:0x%08x::0x%08x\n",
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
		pr_notice("[SCP] C0:H1:%02d:0x%08x::0x%08x\n",
			i, *(out + 64 + i * 2), *(out + 64 + i * 2 + 1));
	}

	if (scpreg.core_nums == 1)
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
		pr_notice("[SCP] C1:H0:%02d:0x%08x::0x%08x\n",
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
		pr_notice("[SCP] C1:H1:%02d:0x%08x::0x%08x\n",
			i, *(out + 64 + i * 2), *(out + 64 + i * 2 + 1));
	}
}

/*
 * this function need SCP to keeping awaken
 * scp_crash_dump: dump scp tcm info.
 * @param scp_core_id:  core id
 * @return:             scp dump size
 */
static unsigned int scp_crash_dump(enum scp_core_id id)
{
	unsigned int scp_dump_size;
	unsigned int scp_awake_fail_flag;
	uint32_t dram_start = 0;
	uint32_t dram_size = 0;
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM) && SCP_SECURE_DUMP_MEASURE
	int idx;
#endif
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	uint64_t dump_start, dump_end;
#endif

	/*flag use to indicate scp awake success or not*/
	scp_awake_fail_flag = 0;

	/*check SRAM lock ,awake scp*/
	if (scp_awake_lock((void *)id) == -1) {
		pr_notice("[SCP] %s: awake scp fail, scp id=%u\n", __func__, id);
		scp_awake_fail_flag = 1;
	}


#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
#if SCP_SECURE_DUMP_MEASURE
		memset(scpdump_cal, 0x0, sizeof(scpdump_cal));
		idx = 0;
		scpdump_cal[0].type = 1;
		scpdump_cal[0].start = ktime_get_boot_ns();
#endif

		dump_start = ktime_get_boot_ns();
		{
			int polling = 1;
			int retry = POLLING_RETRY;
#if SCP_SECURE_DUMP_MEASURE
			idx++;
			scpdump_cal[idx].type = 2;
			scpdump_cal[idx].start = ktime_get_boot_ns();
#endif

			scp_do_dump();

#if SCP_SECURE_DUMP_MEASURE
			scpdump_cal[idx].end = ktime_get_boot_ns();
#endif

			while (polling != 0 && retry > 0) {
#if SCP_SECURE_DUMP_MEASURE
				if (idx >= POLLING_RETRY)
					break;
				idx++;
				scpdump_cal[idx].type = 3;
				scpdump_cal[idx].start = ktime_get_boot_ns();
#endif

				polling = scp_do_polling();

#if SCP_SECURE_DUMP_MEASURE
				scpdump_cal[idx].end = ktime_get_boot_ns();
#endif

				if (!polling)
					break;
				mdelay(polling);
				retry--;
			}
			if (retry == 0)	{
				pr_notice("[SCP] Dump timed out:%d\n", POLLING_RETRY);
#if SCP_SECURE_DUMP_DEBUG
				pr_notice("[SCP] Dump timeout dump once again.\n");
				print_clk_registers();
#endif
			}
		}
		dump_end = ktime_get_boot_ns();
		pr_notice("[SCP] Dump: %lld ns\n", (dump_end - dump_start));

#if SCP_SECURE_DUMP_MEASURE
		scpdump_cal[0].end = ktime_get_boot_ns();
		for (idx = 0; idx < POLLING_RETRY; idx++) {
			if (scpdump_cal[idx].type == 0)
				break;
			pr_notice("[SCP] scpdump:%d Type:%d %lldns\n", idx, scpdump_cal[idx].type,
						(scpdump_cal[idx].end - scpdump_cal[idx].start));
		}
#endif

		/* tbuf is different between rv33/rv55 */
		if (scpreg.twohart) {
			/* RV55 */
			uint32_t *out = (uint32_t *)get_MDUMP_addr(MDUMP_TBUF);
			int i;

			for (i = 0; i < 32; i++) {
				pr_notice("[SCP] C0:H0:%02d:0x%08x::0x%08x\n",
					i, *(out + i * 2), *(out + i * 2 + 1));
			}


			for (i = 0; i < 32; i++) {
				pr_notice("[SCP] C0:H1:%02d:0x%08x::0x%08x\n",
					i, *(out + 64 + i * 2), *(out + 64 + i * 2 + 1));
			}
			if (scpreg.core_nums > 1) {
				for (i = 0; i < 32; i++) {
					pr_notice("[SCP] C1:H0:%02d:0x%08x::0x%08x\n",
						i, *(out + 128 + i * 2), *(out + 128 + i * 2 + 1));
				}
				for (i = 0; i < 32; i++) {
					pr_notice("[SCP] C1:H1:%02d:0x%08x::0x%08x\n",
						i, *(out + 192 + i * 2), *(out + 192 + i * 2 + 1));
				}
			}
		} else {
			/* RV33 */
			uint32_t *out = (uint32_t *)get_MDUMP_addr(MDUMP_TBUF);
			int i;

			for (i = 0; i < 16; i++) {
				pr_notice("[SCP] C0:%02d:0x%08x::0x%08x\n",
					i, *(out + i * 2), *(out + i * 2 + 1));
			}
			for (i = 0; i < 16; i++) {
				pr_notice("[SCP] C1:%02d:0x%08x::0x%08x\n",
					i, *(out + i * 2 + 16), *(out + i * 2 + 17));
			}
		}

		scp_dump_size = get_MDUMP_size_accumulate(MDUMP_TBUF);

		/* dram support? */
		if ((int)(scp_region_info_copy.ap_dram_size) <= 0) {
			pr_notice("[scp] ap_dram_size <=0\n");
		} else {
			dram_start = scp_region_info_copy.ap_dram_start;
			dram_size = scp_region_info_copy.ap_dram_size;
			scp_dump_size += roundup(dram_size, 4);
		}

	} else {
#else
	{
#endif
	memcpy_from_scp((void *)get_MDUMP_addr(MDUMP_L2TCM),
		(void *)(SCP_TCM),
		(SCP_A_TCM_SIZE));
	scp_do_l1cdump((void *)get_MDUMP_addr(MDUMP_L1C),
		(void *)get_MDUMP_addr(MDUMP_REGDUMP));
	/* dump sys registers */
	scp_do_regdump((void *)get_MDUMP_addr(MDUMP_REGDUMP),
		(void *)get_MDUMP_addr(MDUMP_TBUF));
	scp_do_tbufdump((void *)get_MDUMP_addr(MDUMP_TBUF),
		(void *)get_MDUMP_addr(MDUMP_DRAM));
	scp_dump_size = get_MDUMP_size_accumulate(MDUMP_TBUF);

	/* dram support? */
	if ((int)(scp_region_info_copy.ap_dram_size) <= 0) {
		pr_notice("[scp] ap_dram_size <=0\n");
	} else {
		dram_start = scp_region_info_copy.ap_dram_start;
		dram_size = scp_region_info_copy.ap_dram_size;
		/* copy dram data*/
		memcpy((void *)get_MDUMP_addr(MDUMP_DRAM),
			scp_ap_dram_virt, dram_size);
		scp_dump_size += roundup(dram_size, 4);
	}
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
	char *scp_A_log = NULL;
	size_t offset = 0;

	pr_debug("[SCP] %s begins:%s\n", __func__, aed_str);
	scp_dump_last_regs();
	scp_show_last_regs();

	scp_A_log = scp_pickup_log_for_aee();

	if (scp_dump.detail_buff == NULL) {
		pr_notice("[SCP AEE]detail buf is null\n");
	} else {
		/* prepare scp aee detail information*/
		memset(scp_dump.detail_buff, 0, SCP_AED_STR_LEN);

		offset += SCP_CHECK_AED_STR_LEN(snprintf(scp_dump.detail_buff + offset,
		SCP_AED_STR_LEN - offset, "%s\n", aed_str), offset);


		offset += SCP_CHECK_AED_STR_LEN(snprintf(scp_dump.detail_buff + offset,
		SCP_AED_STR_LEN - offset,
		"core0 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c0_m->pc, c0_m->lr, c0_m->sp), offset);

		if (!scpreg.twohart)
			goto core1;

		offset += SCP_CHECK_AED_STR_LEN(snprintf(scp_dump.detail_buff + offset,
		SCP_AED_STR_LEN - offset,
		"hart1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c0_t1_m->pc, c0_t1_m->lr, c0_t1_m->sp), offset);
core1:
		if (scpreg.core_nums == 1)
			goto end;

		offset += SCP_CHECK_AED_STR_LEN(snprintf(scp_dump.detail_buff + offset,
		SCP_AED_STR_LEN - offset,
		"core1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c1_m->pc, c1_m->lr, c1_m->sp), offset);

		if (!scpreg.twohart)
			goto end;

		offset += SCP_CHECK_AED_STR_LEN(snprintf(scp_dump.detail_buff + offset,
		SCP_AED_STR_LEN - offset,
		"hart1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c1_t1_m->pc, c1_t1_m->lr, c1_t1_m->sp), offset);
end:
		offset += SCP_CHECK_AED_STR_LEN(snprintf(scp_dump.detail_buff + offset,
			SCP_AED_STR_LEN - offset, "last log:\n%s", scp_A_log), offset);

		scp_dump.detail_buff[SCP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare scp A db file*/
	scp_dump.ramdump_length = 0;
	scp_dump.ramdump_length = scp_crash_dump(SCP_A_ID);

	pr_notice("[SCP] %s ends, @%p, size = %x\n", __func__,
		scp_dump.ramdump, scp_dump.ramdump_length);
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
	size_t timeout = msecs_to_jiffies(SCP_COREDUMP_TIMEOUT_MS);
	size_t expire = jiffies + timeout;
	int ret;

	if (!scp_ee_enable) {
		pr_debug("[SCP]ee disable value=%d\n", scp_ee_enable);
		return;
	}

	/* wait for previous coredump complete */
	while (1) {
		ret = wait_for_completion_interruptible_timeout(
			&scp_coredump_comp, timeout);
		if (ret == 0) {
			pr_notice("[SCP] %s:TIMEOUT, skip\n",
				__func__);
			break;
		}
		if (ret > 0)
			break;
		if ((ret == -ERESTARTSYS) && time_before(jiffies, expire)) {
			pr_debug("[SCP] %s: continue waiting for completion\n",
				__func__);
			timeout = expire - jiffies;
			continue;
		}
	}
	if (atomic_read(&coredumping) == true)
		pr_notice("[SCP] coredump overwrite happen\n");
	atomic_set(&coredumping, true);

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

#if IS_ENABLED(CONFIG_MTK_AEE_AED)
	/* scp aed api, only detail information available*/
	aed_common_exception_api("scp", NULL, 0, NULL, 0,
			scp_dump.detail_buff, DB_OPT_DEFAULT);
#endif

	pr_debug("[SCP] scp exception dump is done\n");

}



static ssize_t scp_A_dump_show(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;


	if (offset >= 0 && offset < scp_dump.ramdump_length) {
		if ((offset + size) >= scp_dump.ramdump_length)
			size = scp_dump.ramdump_length - offset;

		memcpy(buf, scp_dump.ramdump + offset, size);
		length = size;

		/* clean the buff after readed */
		memset(scp_dump.ramdump + offset, 0x0, size);
		/* log for the first and latest cleanup */
		if (offset == 0 || size == (scp_dump.ramdump_length - offset))
			pr_notice("[SCP] %s ramdump cleaned of:0x%llx sz:0x%lx\n", __func__,
				offset, size);

		/* the last time read scp_dump buffer has done
		 * so the next coredump flow can be continued
		 */
		if (size == scp_dump.ramdump_length - offset) {
			atomic_set(&coredumping, false);
			pr_notice("[SCP] coredumping:%d, coredump complete\n",
				atomic_read(&coredumping));
			complete(&scp_coredump_comp);
		}
	}


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
	int i;
	int size_limit = sizeof(reg_save_list) / sizeof(struct reg_save_st);


	/* last addr is infra */
	for (i = 0; i < size_limit - 1; i++)
		reg_save_list[i].addr |= scp_reg_base_phy;

	/* alloc dump memory */
	scp_dump.detail_buff = vmalloc(SCP_AED_STR_LEN);
	if (!scp_dump.detail_buff)
		return -1;

	/* support L1C or not? */
	if ((int)(scp_region_info->ap_dram_size) > 0)
		dram_size = scp_region_info->ap_dram_size;

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
		scp_dump.ramdump =
				(uint8_t *)(void *)scp_get_reserve_mem_virt(SCP_A_SECDUMP_MEM_ID);
		if (!scp_dump.ramdump)
			return -1;
	} else {
#else
	{
#endif
	scp_dump.ramdump = vmalloc(get_MDUMP_size_accumulate(MDUMP_DRAM));
	if (!scp_dump.ramdump)
		return -1;
	}
	memset(scp_dump.ramdump, 0x0, get_MDUMP_size_accumulate(MDUMP_DRAM));
	pr_notice("[SCP] %s cleaned ramdump\n", __func__);

	/* scp_status_reg init */
	c0_m = vmalloc(sizeof(struct scp_status_reg));
	if (!c0_m)
		return -1;
	if (scpreg.twohart) {
		c0_t1_m = vmalloc(sizeof(struct scp_status_reg));
		if (!c0_t1_m)
			return -1;
	}
	if (scpreg.core_nums == 2) {
		c1_m = vmalloc(sizeof(struct scp_status_reg));
		if (!c1_m)
			return -1;
	}
	if (scpreg.core_nums == 2 && scpreg.twohart) {
		c1_t1_m = vmalloc(sizeof(struct scp_status_reg));
		if (!c1_t1_m)
			return -1;
	}
	/* scp_do_tbufdump init, because tbuf is different between rv33/rv55 */
	if (scpreg.twohart)
		scp_do_tbufdump = scp_do_tbufdump_RV55;
	else
		scp_do_tbufdump = scp_do_tbufdump_RV33;

	/* init global values */
	scp_dump.ramdump_length = 0;
	/* 1: ee on, 0: ee disable */
	scp_ee_enable = 1;
	/* all coredump need element is prepare done */
	complete(&scp_coredump_comp);

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
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
		scp_dump.ramdump = NULL;
	} else {
#else
	{
#endif
	vfree(scp_dump.ramdump);
	}

	scp_A_task_context_addr = 0;

	pr_debug("[SCP] %s ends\n", __func__);
}

