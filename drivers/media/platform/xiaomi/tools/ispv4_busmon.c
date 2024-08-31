// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 *
 *
 */
#include "ispv4_busmon.h"
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <media/ispv4_defs.h>

extern struct dentry *ispv4_debugfs;
static struct debugfs_com debugfs_param;
struct busmon_perf_param_s perf_param_buf;

struct busmon_inst_info_s {
	uintptr_t base; /* base address of the registers */
};

struct busmon_chn_info_s {
	const struct busmon_inst_info_s *inst_info;
	uint8_t axi_sel; /* AXI group select */
	uint8_t axi_width; /* AXI bus width */
	uint32_t axi_freq; /* AXI clock frequency */
};

static const enum busmon_inst g_chn_2_inst[] = {
	[APB_BUSMON_PCIE] = APB_BUSMON_INST0,
	[APB_BUSMON_MIPI] = APB_BUSMON_INST0,
	[APB_BUSMON_CPU] = APB_BUSMON_INST1,
	[APB_BUSMON_DMA] = APB_BUSMON_INST1,
	[APB_BUSMON_OCM] = APB_BUSMON_INST2,
	[APB_BUSMON_NPU] = APB_BUSMON_INST2,

	[DDR_BUSMON_ISP_NIC0] = DDR_BUSMON_INST0,
	[DDR_BUSMON_ISP_NIC1] = DDR_BUSMON_INST1,
	[DDR_BUSMON_OCM_NIC] = DDR_BUSMON_INST2,
	[DDR_BUSMON_CPU_NIC] = DDR_BUSMON_INST3,
	[DDR_BUSMON_MAIN_NIC] = DDR_BUSMON_INST4,

	[ISP_BUSMON_FE0] = ISP_BUSMON_INST0,
	[ISP_BUSMON_FE1] = ISP_BUSMON_INST0,
	[ISP_BUSMON_CVP] = ISP_BUSMON_INST1,
	[ISP_BUSMON_ROUTER] = ISP_BUSMON_INST1,
	[ISP_BUSMON_BEF] = ISP_BUSMON_INST2,
	[ISP_BUSMON_BEB] = ISP_BUSMON_INST2,
	[ISP_BUSMON_NIC] = ISP_BUSMON_INST3,
	[ISP_BUSMON_CMDDMA] = ISP_BUSMON_INST3,
};

static const struct busmon_inst_info_s g_busmon_inst_info[] = {
	[APB_BUSMON_INST0] = {
			.base = ISPV4_BUSMON_0_ADDR_OFFSET,
		},
	[APB_BUSMON_INST1] = {
			.base = ISPV4_BUSMON_1_ADDR_OFFSET,
		},
	[APB_BUSMON_INST2] = {
			.base = ISPV4_BUSMON_2_ADDR_OFFSET,
		},
	[DDR_BUSMON_INST0] = {
			.base = ISPV4_DDR_BUSMON0_ADDR_OFFSET,
		},
	[DDR_BUSMON_INST1] = {
			.base = ISPV4_DDR_BUSMON1_ADDR_OFFSET,
		},
	[DDR_BUSMON_INST2] = {
			.base = ISPV4_DDR_BUSMON2_ADDR_OFFSET,
		},
	[DDR_BUSMON_INST3] = {
			.base = ISPV4_DDR_BUSMON3_ADDR_OFFSET,
		},
	[DDR_BUSMON_INST4] = {
			.base = ISPV4_DDR_BUSMON4_ADDR_OFFSET,
		},

	[ISP_BUSMON_INST0] = {
			.base = ISP_AXI_BUSMON0_OFFSET,
		},
	[ISP_BUSMON_INST1] = {
			.base = ISP_AXI_BUSMON1_OFFSET,
		},
	[ISP_BUSMON_INST2] = {
			.base = ISP_AXI_BUSMON2_OFFSET,
		},
	[ISP_BUSMON_INST3] = {
			.base = ISP_AXI_BUSMON3_OFFSET,
		},
};

static const struct busmon_chn_info_s g_busmon_chn_info[] = {
	[APB_BUSMON_PCIE] = {
			.inst_info = &g_busmon_inst_info[APB_BUSMON_INST0],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_PCIE,
			.axi_freq = BUSMON_AXI_CLK_PCIE,
		},
	[APB_BUSMON_MIPI] = {
			.inst_info = &g_busmon_inst_info[APB_BUSMON_INST0],
			.axi_sel = 1,
			.axi_width = BUSMON_AXI_WIDTH_MIPI,
			.axi_freq = BUSMON_AXI_CLK_MIPI,
		},
	[APB_BUSMON_CPU] = {
			.inst_info = &g_busmon_inst_info[APB_BUSMON_INST1],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_CPU,
			.axi_freq = BUSMON_AXI_CLK_CPU,
		},
	[APB_BUSMON_DMA] = {
			.inst_info = &g_busmon_inst_info[APB_BUSMON_INST1],
			.axi_sel = 1,
			.axi_width = BUSMON_AXI_WIDTH_DMA,
			.axi_freq = BUSMON_AXI_CLK_DMA,
		},
	[APB_BUSMON_OCM] = {
			.inst_info = &g_busmon_inst_info[APB_BUSMON_INST2],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_OCM,
			.axi_freq = BUSMON_AXI_CLK_OCM,
		},
	[APB_BUSMON_NPU] = {
			.inst_info = &g_busmon_inst_info[APB_BUSMON_INST2],
			.axi_sel = 1,
			.axi_width = BUSMON_AXI_WIDTH_NPU,
			.axi_freq = BUSMON_AXI_CLK_NPU,
		},
	[DDR_BUSMON_ISP_NIC0] = {
			.inst_info = &g_busmon_inst_info[DDR_BUSMON_INST0],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_ISP_NIC0,
			.axi_freq = BUSMON_AXI_CLK_ISP_NIC0,
		},
	[DDR_BUSMON_ISP_NIC1] = {
			.inst_info = &g_busmon_inst_info[DDR_BUSMON_INST1],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_ISP_NIC1,
			.axi_freq = BUSMON_AXI_CLK_ISP_NIC1,
		},
	[DDR_BUSMON_OCM_NIC] = {
			.inst_info = &g_busmon_inst_info[DDR_BUSMON_INST2],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_OCM_NIC,
			.axi_freq = BUSMON_AXI_CLK_OCM_NIC,
		},
	[DDR_BUSMON_CPU_NIC] = {
			.inst_info = &g_busmon_inst_info[DDR_BUSMON_INST3],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_CPU_NIC,
			.axi_freq = BUSMON_AXI_CLK_CPU_NIC,
		},
	[DDR_BUSMON_MAIN_NIC] = {
			.inst_info = &g_busmon_inst_info[DDR_BUSMON_INST4],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_MAIN_NIC,
			.axi_freq = BUSMON_AXI_CLK_MAIN_NIC,
		},
	[ISP_BUSMON_FE0] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST0],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_ISP_FE0,
			.axi_freq = BUSMON_AXI_CLK_ISP_FE0,
		},
	[ISP_BUSMON_FE1] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST0],
			.axi_sel = 1,
			.axi_width = BUSMON_AXI_WIDTH_ISP_FE1,
			.axi_freq = BUSMON_AXI_CLK_ISP_FE1,
		},
	[ISP_BUSMON_CVP] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST1],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_ISP_CVP,
			.axi_freq = BUSMON_AXI_CLK_ISP_CVP,
		},
	[ISP_BUSMON_ROUTER] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST1],
			.axi_sel = 1,
			.axi_width = BUSMON_AXI_WIDTH_ISP_ROUTER,
			.axi_freq = BUSMON_AXI_CLK_ISP_ROUTER,
		},
	[ISP_BUSMON_BEF] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST2],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_ISP_BEF,
			.axi_freq = BUSMON_AXI_CLK_ISP_BEF,
		},
	[ISP_BUSMON_BEB] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST2],
			.axi_sel = 1,
			.axi_width = BUSMON_AXI_WIDTH_ISP_BEB,
			.axi_freq = BUSMON_AXI_CLK_ISP_BEB,
		},
	[ISP_BUSMON_NIC] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST3],
			.axi_sel = 0,
			.axi_width = BUSMON_AXI_WIDTH_ISP_NIC,
			.axi_freq = BUSMON_AXI_CLK_ISP_NIC,
		},
	[ISP_BUSMON_CMDDMA] = {
			.inst_info = &g_busmon_inst_info[ISP_BUSMON_INST3],
			.axi_sel = 1,
			.axi_width = BUSMON_AXI_WIDTH_ISP_CMDDMA,
			.axi_freq = BUSMON_AXI_CLK_ISP_CMDDMA,
		},
};

struct busmon_inst_stat_s {
	bool match_busy;
	bool perf_busy;
};

struct busmon_inst_stat_s g_busmon_inst_stat[BUSMON_INST_CNT];

struct match_mgr_s {
	bool busy;
	struct mutex match_mytex;
	const struct busmon_chn_info_s *chn_info;
};

struct busmon_workqueue {
	int arg;
	struct work_struct busmon_work;
};

struct match_dev_s {
	bool inited;
	struct match_mgr_s mgr[BUSMON_CHN_CNT];
};

struct perf_mgr_s {
	int chn;
	bool busy;
	const struct busmon_chn_info_s *chn_info;
	struct work_struct work;
	struct mutex perfm_mutex;

	int option;
	size_t mem_size;

	struct busmon_perf_cnt_result_s *mem_addr_start;
	int cur_wr_offset;
	size_t new_wr_size;
	uint64_t int_time_us;
	bool mem_full;

	struct dentry *debugfs_blob;
	struct debugfs_blob_wrapper blob_buf;
};

struct perf_dev_s {
	bool inited;
	struct perf_mgr_s mgr[BUSMON_CHN_CNT];
};

struct busmon_workqueue match_work_s = {
	.arg = 0,
};
struct match_dev_s g_match_dev = {
	.inited = false,
};

struct perf_dev_s g_perf_dev = {
	.inited = false,
};

/*top\ddr\isp irq manage struct*/
struct irq_set_manage {
	bool bus_monitor_start;	/* if start monitor */
	atomic_t ref_top_en;	/* num of start top monitor */
	atomic_t ref_ddr_en;	/* num of start ddr monitor */
	atomic_t ref_isp_en;	/* num of start isp monitor */
};

struct irq_set_manage g_irq_manage = {
	.bus_monitor_start = false,
	.ref_top_en = ATOMIC_INIT(0),
	.ref_ddr_en = ATOMIC_INIT(0),
	.ref_isp_en = ATOMIC_INIT(0),
};

//DDR intc
struct merge_int_mgr_s;

typedef void (*merge_int_enable_t)(struct merge_int_mgr_s *int_mgr, int index);
typedef void (*merge_int_clear_t)(struct merge_int_mgr_s *int_mgr, uint32_t int_stat);

struct merge_int_mgr_s {
	/* static values should be init when compile */
	int irq; /* irq num for this merge interrupt */
	uintptr_t base; /* base address of the merge int ip */
	uint32_t stat_offset; /* offset of the interrupt status register */
	int max_int_cnt; /* max interrupt count of this merge interrupt */

	/* functions */
	merge_int_enable_t int_enable_func;
	merge_int_clear_t int_clear_func;

	uint32_t int_stat; /* interrupt status */
};

#define DDR_BUSMON_INT_PERFM_CNT_SHIFT(inst) ((inst)*3 + 1)
#define DDR_BUSMON_INT_MATCH_SHIFT(inst)     ((inst)*3 + 2)

static void isp_busmon_en(void)
{
	write_field(ISP_SYS_TOP_OFFSET, ISP_SYS_CFG12, ISP_SYS_CFG12_OCLA_EN, 1);
}
/*enable or disable irq*/
static int gpio_busmon_irq_set(void)
{
	int ret;
	uint32_t rounter_mask;
	uint32_t mask;
	static bool top_irq_en = false;
	static bool ddr_irq_en = false;
	static bool isp_irq_en = false;

	/* enable top irq mask gpio g2r2 */
	ret = ispv4_regops_read(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R2_INT_MASK,
				&rounter_mask);
	if (ret) {
		pr_err("ispv4 busmon read irq_mask registers error\n");
		return ret;
	}
	mask = (AP_REQ_MATCH_BIT | AP_REQ_MATCHT_BIT | AP_REQ_MATCHTT_BIT | AP_REQ_PERFM_CNT_BIT |
		AP_REQ_PERFM_CNTT_BIT | AP_REQ_PERFM_CNTTT_BIT);

	if (atomic_read(&g_irq_manage.ref_top_en) > 0 && top_irq_en == false) {
		ret = ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R2_INT_MASK,
					 rounter_mask & ~mask);
		if (ret) {
			pr_err("ispv4 busmon enable irq fail \n");
			return ret;
		}
		top_irq_en = true;
		pr_info("ispv4 busmon enable top irq");
	} else if (atomic_read(&g_irq_manage.ref_top_en) == 0 && top_irq_en == true) {
		ret = ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R2_INT_MASK,
					 rounter_mask | mask); // disable interrupt
		if (ret) {
			pr_err("ispv4 busmon enable irq fail \n");
			return ret;
		}
		top_irq_en = false;
		pr_info("ispv4 busmon disable top irq");
	}

	/* enable isp irq mask */
	ret = ispv4_regops_read(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R2_INT_MASK,
				&rounter_mask);
	if (ret) {
		pr_err("ispv4 busmon read irq_mask registers error\n");
		return ret;
	}
	mask = AP_ISP_BUSMON_BIT;

	if (atomic_read(&g_irq_manage.ref_isp_en) > 0 && isp_irq_en == false) {
		ret = ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R2_INT_MASK,
					 rounter_mask & ~mask);
		if (ret) {
			pr_err("ispv4 busmon enable irq fail \n");
			return ret;
		}
		isp_irq_en = true;
		pr_info("ispv4 busmon enable isp irq");
	} else if (atomic_read(&g_irq_manage.ref_isp_en) == 0 && isp_irq_en == true) {
		ret = ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R2_INT_MASK,
					 rounter_mask | mask);
		if (ret) {
			pr_err("ispv4 busmon enable irq fail \n");
			return ret;
		}
		isp_irq_en = false;
		pr_info("ispv4 busmon disable isp irq");
	}

	/* enable ddr irq mask gpio g2r1 */
	ret = ispv4_regops_read(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R1_INT_MASK,
				&rounter_mask);
	if (ret) {
		pr_err("ispv4 busmon read g2r1 registers error\n");
		return ret;
	}
	mask = AP_DDR_BUSMON_BIT;

	if (atomic_read(&g_irq_manage.ref_ddr_en) > 0 && ddr_irq_en == false) {
		ret = ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R1_INT_MASK,
					 rounter_mask & ~mask);
		if (ret) {
			pr_err("ispv4 busmon write g2r1 registers error\n");
			return ret;
		}
		ddr_irq_en = true;
		pr_info("ispv4 busmon enable ddr irq");
	} else if (atomic_read(&g_irq_manage.ref_ddr_en) == 0 && ddr_irq_en == true) {
		ret = ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR + AP_INTC_G2R1_INT_MASK,
					 rounter_mask | mask);
		if (ret) {
			pr_err("ispv4 busmon write g2r1 registers error\n");
			return ret;
		}
		ddr_irq_en = false;
		pr_info("ispv4 busmon disable ddr irq");
	}

	return 0;
}

static void _intc_int_enable(struct merge_int_mgr_s *int_mgr, int index)
{
	/* clear interrupt */
	pr_info("ispv4 busmon intc base address: %lx\n", int_mgr->base);
	write_all(int_mgr->base, INTC_REG_INT_CLEAR, 1 << index); /* W1C 0xf133018*/

	/* set interrupt type: high level */
	write_all_g(int_mgr->base, index, INTC_REG_INT0_TYPE, INTC_INT_TYPE_HIGH);

	/* set interrupt enable: enable IRQ */
	if (index < 16)
		write_field_bg(int_mgr->base, INTC_REG_INT_EN0, index, INTC_REG_INT_EN0_INT0,
			       INTC_INT_EN_IRQ);
	else
		write_field_bg(int_mgr->base, INTC_REG_INT_EN1, index - 16, INTC_REG_INT_EN1_INT16,
			       INTC_INT_EN_IRQ);

	/* set interrupt mask ,0 for unmask*/
	// write_field_bg(int_mgr->base, INTC_REG_INT_MASK, index, INTC_REG_INT_MASK_INT0, 0);
	/* 0xffff9249: bit 1,2 4,5 7,8 10,11 13,14 ; 0 can into irq*/
	write_all(int_mgr->base, INTC_REG_INT_MASK, 0xffff9249);
}

static void _intc_int_clear(struct merge_int_mgr_s *int_mgr, uint32_t int_stat)
{
	write_all_pcie(int_mgr->base, INTC_REG_INT_CLEAR, int_stat);
	// pr_info("ispv4 busmon ddr intc cleared");
}

static struct merge_int_mgr_s g_ddr_int_mgr = {
	.irq = ISPV4_IRQ_DDR_BUSMON,
	.base = ISPV4_DDR_INTC_ADDR_OFFSET,
	.stat_offset = INTC_REG_INT_MASKEDSTATUS_OFFSET,
	.max_int_cnt = INTC_MAX_INT_CNT,
	.int_enable_func = _intc_int_enable,
	.int_clear_func = _intc_int_clear,
};

static struct merge_int_mgr_s g_isp_int_mgr = {
	.irq = ISPV4_IRQ_ISP_BUSMON,
	.base = ISP_SYS_TOP_OFFSET,
	.stat_offset = ISP_SYS_TOP_STA2_OFFSET,
	.max_int_cnt = ISP_BUSMON_MAX_INT_CNT,
	.int_enable_func = NULL,
	.int_clear_func = NULL,
};

static void _match_dump_param(enum busmon_chn chn,
			      struct busmon_match_param_s *param)
{
	pr_info("ispv4 busmon match param: chn[%d],  addr=0x%llx ~ 0x%llx,  id=0x%llx ~ 0x%llx,  "
		"addr_exc=%s,  id_exc=%s\n",
		chn, param->filter.addr_min, param->filter.addr_max,
		param->filter.id_min, param->filter.id_max,
		param->filter.addr_exc ? "true" : "false",
		param->filter.id_exc ? "true" : "false");
}

static int _busmon_match_callback(struct busmon_match_info_s *info)
{
	int i;

	pr_info("idpv4 busmon match callback start\n");
	pr_info("ispv4 busmon WCMD valid count:%d\n", info->wcmd_valid_cnt);
	for (i = 0; i < info->wcmd_valid_cnt; i++) {
		pr_info("ispv4 busmon WCMD[%d]: id=0x%lx,  addr=0x%llx,  cache=%d,  len=%d,  "
			"size=%d,  burst=%d",
			i, info->wr_trans_info[i].id,
			info->wr_trans_info[i].addr,
			info->wr_trans_info[i].cache,
			info->wr_trans_info[i].len, info->wr_trans_info[i].size,
			info->wr_trans_info[i].burst);
	}

	pr_info("ispv4 busmon RCMD valid count:%d\n", info->rcmd_valid_cnt);
	for (i = 0; i < info->rcmd_valid_cnt; i++) {
		pr_info("ispv4 busmon RCMD[%d]: id=0x%lx,  addr=0x%llx,  cache=%d,  len=%d,  "
			"size=%d,  burst=%d",
			i, info->rd_trans_info[i].id,
			info->rd_trans_info[i].addr,
			info->rd_trans_info[i].cache,
			info->rd_trans_info[i].len, info->rd_trans_info[i].size,
			info->rd_trans_info[i].burst);
	}
	pr_info("ispv4 busmon match callback end\n");
	return 0;
}

static void _match_get_result(uintptr_t base_in)
{
	int i;
	uintptr_t base = base_in;
	struct busmon_match_info_s info;

	info.wcmd_valid_cnt =
		read_field_pcie(base, CNT_VLD_4CMD_MATCH, CNT_VLD_4WCMD_MATCH);
	info.rcmd_valid_cnt =
		read_field_pcie(base, CNT_VLD_4CMD_MATCH, CNT_VLD_4RCMD_MATCH);

	for (i = 0; i < info.wcmd_valid_cnt; i++) {
		info.wr_trans_info[i].cache =
			read_field_g_pcie(base, i, WR_CMD0_MATCH, AWCACHE0_MATCH);
		info.wr_trans_info[i].len =
			read_field_g_pcie(base, i, WR_CMD0_MATCH, AWLEN0_MATCH);
		info.wr_trans_info[i].size =
			read_field_g_pcie(base, i, WR_CMD0_MATCH, AWSIZE0_MATCH);
		info.wr_trans_info[i].burst =
			read_field_g_pcie(base, i, WR_CMD0_MATCH, AWBURST0_MATCH);

		info.wr_trans_info[i].addr =
			((uint64_t)read_all_g_pcie(base, i, WR_ADDR0_H_MATCH) << 32) +
			read_all_g_pcie(base, i, WR_ADDR0_L_MATCH);
		info.wr_trans_info[i].id = read_all_g_pcie(base, i, WR_ID0_MATCH);
	}

	for (i = 0; i < info.rcmd_valid_cnt; i++) {
		info.rd_trans_info[i].cache =
			read_field_g_pcie(base, i, RD_CMD0_MATCH, ARCACHE0_MATCH);
		info.rd_trans_info[i].len =
			read_field_g_pcie(base, i, RD_CMD0_MATCH, ARLEN0_MATCH);
		info.rd_trans_info[i].size =
			read_field_g_pcie(base, i, RD_CMD0_MATCH, ARSIZE0_MATCH);
		info.rd_trans_info[i].burst =
			read_field_g_pcie(base, i, RD_CMD0_MATCH, ARBURST0_MATCH);

		info.rd_trans_info[i].addr =
			((uint64_t)read_all_g_pcie(base, i, RD_ADDR0_H_MATCH) << 32) +
			read_all_g_pcie(base, i, RD_ADDR0_L_MATCH);
		info.rd_trans_info[i].id = read_all_g_pcie(base, i, RD_ID0_MATCH);
	}
	// show the results in the dmesg
	_busmon_match_callback(&info);
}
static void _match_worker(struct work_struct *work)
{
	int inst;
	uintptr_t base;
	struct busmon_workqueue *temp_mwork =
		container_of(work, struct busmon_workqueue, busmon_work);
	inst = temp_mwork->arg;
	base = g_busmon_inst_info[inst].base;
	_match_get_result(base);
}

static void _match_hw_cfg(const struct busmon_chn_info_s *chn_info,
			  struct busmon_match_param_s *param)
{
	uintptr_t base = chn_info->inst_info->base;

	write_field(base, BUSMON_COM_CFG, AXI_GRP_SEL, chn_info->axi_sel);

	/* clear interrupt */
	write_all(base, BUSMON_INT_CLR, 1 << INT_CLR_MATCH_SHIFT);
	write_all(base, BUSMON_INT_CLR, 0);
	/* enable interrupt */
	write_field(base, BUSMON_INT_MASK, INT_MASK_MATCH, 0);

	/* config addr & id */
	write_all(base, MATCH_MIN_ADDR_H, param->filter.addr_min >> 32);
	write_all(base, MATCH_MIN_ADDR_L, param->filter.addr_min & 0xffffffff);
	write_all(base, MATCH_MAX_ADDR_H, param->filter.addr_max >> 32);
	write_all(base, MATCH_MAX_ADDR_L, param->filter.addr_max & 0xffffffff);
	write_all(base, MATCH_MIN_ID, param->filter.id_min);
	write_all(base, MATCH_MAX_ID, param->filter.id_max);
	write_field(base, MATCH_WIN_CFG, MATCH_ADDR_EXC_EN, param->filter.addr_exc ? 1 : 0);
	write_field(base, MATCH_WIN_CFG, MATCH_ID_EXC_EN, param->filter.id_exc ? 1 : 0);

	write_field(base, MATCH_WIN_CFG, MATCH_INT_MODE, 0);
	write_field(base, BUSMON_COM_CFG, MATCH_EN, 1);

	pr_info("ispv4 busmon match chn[%d]register init finished!\n",
		param->chn);
}

int _match_dev_init(struct match_dev_s *dev)
{
	int i;

	if (dev->inited) {
		pr_info("ispv4 busmon match_dev is already initialized!\n");
		return 0;
	}

	INIT_WORK(&match_work_s.busmon_work, _match_worker);

	for (i = 0; i < BUSMON_CHN_CNT; i++) {
		dev->mgr[i].busy = false;
		dev->mgr[i].chn_info = &g_busmon_chn_info[i];
		mutex_init(&dev->mgr[i].match_mytex);
	}
	dev->inited = true;

	pr_info("ispv4 busmon match_dev param init finish!\n");
	return 0;
}

static void _match_start_mon(const struct busmon_chn_info_s *chn_info)
{
	write_field(chn_info->inst_info->base, MATCH_WIN_CFG, MATCH_WIN_EN, 1);
}
static void _match_stop_mon(const struct busmon_chn_info_s *chn_info)
{
	write_field(chn_info->inst_info->base, MATCH_WIN_CFG, MATCH_WIN_EN, 0);
}

int busmon_match_start(struct busmon_match_param_s *param)
{
	struct match_mgr_s *mgr;
	struct match_dev_s *match_dev;
	const struct busmon_chn_info_s *chn_info;
	int inst;
	int chn;

	chn = param->chn;
	if (chn >= BUSMON_CHN_CNT || chn < APB_BUSMON_CHN_START) {
		pr_info("ispv4 busmon match start: invalid chn %d\n", chn);
		return -EINVAL;
	}
	match_dev = &g_match_dev;

	_match_dump_param(chn, param);

	if (!match_dev->inited)
		_match_dev_init(match_dev);

	mgr = &match_dev->mgr[chn];
	chn_info = mgr->chn_info;
	inst = g_chn_2_inst[chn];

	if (mgr->busy || g_busmon_inst_stat[inst].perf_busy) {
		pr_err("ispv4 busmon instance busy\n");
		return -EBUSY;
	}

	pr_info("ispv4 busmon chn[%d]hw_info: base=0x%lx,  axi_sel=%d\n", chn,
		chn_info->inst_info->base, chn_info->axi_sel);
	mutex_lock(&mgr->match_mytex);

	_match_hw_cfg(chn_info, param);

	/*ddr_INTC_irq_set*/
	if ((inst >= DDR_BUSMON_INST_START) &&
	    (inst < (DDR_BUSMON_INST_START + DDR_BUSMON_INST_CNT))) {
		_intc_int_enable(&g_ddr_int_mgr, DDR_BUSMON_INT_MATCH_SHIFT(inst - DDR_BUSMON_INST_START));
	}

	if (chn >= DDR_BUSMON_ISP_NIC0 && chn <= DDR_BUSMON_MAIN_NIC)
		atomic_inc(&g_irq_manage.ref_ddr_en);
	else if (chn >= ISP_BUSMON_FE0 && chn <= ISP_BUSMON_CMDDMA)
		atomic_inc(&g_irq_manage.ref_isp_en);
	else {
		atomic_inc(&g_irq_manage.ref_top_en);
	}
	gpio_busmon_irq_set();

	_match_start_mon(chn_info);

	mgr->busy = true;
	g_busmon_inst_stat[inst].match_busy = true;
	mutex_unlock(&mgr->match_mytex);
	pr_info("ispv4 busmon chn[%d] match_start finish!\n", chn);
	return 0;
}

void busmon_match_stop(enum busmon_chn chn)
{
	struct match_dev_s *match_dev = &g_match_dev;
	struct match_mgr_s *mgr = &match_dev->mgr[chn];
	const struct busmon_chn_info_s *chn_info = mgr->chn_info;
	enum busmon_inst inst = g_chn_2_inst[chn];

	if (chn >= BUSMON_CHN_CNT) {
		pr_info("ispv4 busmon_match_stop: invalid chn[%d]!\n", chn);
		return;
	}

	if (!match_dev->inited) {
		_match_dev_init(match_dev);
		return;
	}
	if (mgr->busy && g_busmon_inst_stat[inst].match_busy) {
		mutex_lock(&mgr->match_mytex);
		_match_stop_mon(chn_info);
		mgr->busy = false;
		g_busmon_inst_stat[inst].match_busy = false;

		if (chn >= DDR_BUSMON_ISP_NIC0 && chn <= DDR_BUSMON_MAIN_NIC)
			atomic_dec(&g_irq_manage.ref_ddr_en);
		else if (chn >= ISP_BUSMON_FE0 && chn <= ISP_BUSMON_CMDDMA)
			atomic_dec(&g_irq_manage.ref_isp_en);
		else {
			atomic_dec(&g_irq_manage.ref_top_en);
		}
		gpio_busmon_irq_set();

		mutex_unlock(&mgr->match_mytex);
	}

	pr_info("ispv4 busmon chn[%d] match_stop finish!\n", chn);
}

void _perf_stop_mon(const struct busmon_chn_info_s *chn_info)
{
	uintptr_t base = chn_info->inst_info->base;

	/* cfg_en: pull low */
	write_field_pcie(base, PERFM_WIN_CFG, PERFM_CFG_EN, 0);
	write_field_pcie(base, PERFM_WIN_CFG, PERFM_WIN_SW_TRIG_EN, 0);
	/* cfg_en: pull high */
	write_field_pcie(base, PERFM_WIN_CFG, PERFM_CFG_EN, 1);
}

__maybe_unused  static void _perf_get_result(struct perf_mgr_s *mgr,
			     struct busmon_perf_cnt_result_s *result)
{
	int i;
	uint32_t flag;
	uint32_t win_len_clk;
	const struct busmon_chn_info_s *chn_info = mgr->chn_info;
	uint32_t bytes_per_cycle = chn_info->axi_width / 8;
	uintptr_t base = chn_info->inst_info->base;

	result->magic = PERF_RESULT_MAGIC;

	result->time_us = mgr->int_time_us;

	/* Get window length */
	win_len_clk = read_all_pcie(base, PERFM_WIN_LEN);

	/* Get latency result */
	flag = read_field_pcie(base, CNT_OVFW_FLAG2_PERFM,
			  CNT_WR_LATCY_TOTAL_UDFW_PERFM);
	result->flags.bits.latcy_wr_uf = flag & 0x1;

	flag = read_field_pcie(base, CNT_OVFW_FLAG2_PERFM,
			  CNT_WR_LATCY_TOTAL_OVFW_PERFM);
	result->flags.bits.latcy_wr_of = flag & 0x1;

	flag = read_field_pcie(base, CNT_OVFW_FLAG2_PERFM,
			  CNT_RD_LATCY_TOTAL_UDFW_PERFM);
	result->flags.bits.latcy_rd_uf = flag & 0x1;

	flag = read_field_pcie(base, CNT_OVFW_FLAG2_PERFM,
			  CNT_RD_LATCY_TOTAL_OVFW_PERFM);
	result->flags.bits.latcy_rd_of = flag & 0x1;

	result->latcy_total_wr = read_all_pcie(base, CNT_WR_LATCY_TOTAL_PERFM);
	result->latcy_total_rd = read_all_pcie(base, CNT_RD_LATCY_TOTAL_PERFM);

	/* Get bandwidth result */
	result->bw_enable = 0;
	result->bw_overflow = 0;
	for (i = 0; i < ISPV4_BUSMON_PERF_CNT_PER_CHN; i++) {
		flag = read_field_bg_pcie(base, PERFM_CNT_GRP_EN, i,
				     PERFM_CNT_GRP0_EN);
		result->bw_enable |= ((flag & 0x1) << i);
		if (!flag)
			continue;

		flag = read_field_bg_pcie(base, CNT_OVFW_FLAG1_PERFM, i,
				     CNT0_WR_BW_OVFW_PERFM);
		result->bw_overflow |=
			((flag & 0x1) << (PERF_BW_OVERFLOW_WR_SHIFT + i));
		if (!flag) {
			result->bw_wr[i] =
				read_all_g_pcie(base, i, CNT0_WR_BW_PERFM);
			result->bw_wr[i] = (uint64_t)result->bw_wr[i] *
					   bytes_per_cycle *
					   chn_info->axi_freq / win_len_clk /
					   PERF_BW_CALC_UNIT;
		}

		flag = read_field_bg_pcie(base, CNT_OVFW_FLAG1_PERFM, i,
				     CNT0_RD_BW_OVFW_PERFM);
		result->bw_overflow |=
			((flag & 0x1) << (PERF_BW_OVERFLOW_RD_SHIFT + i));
		if (!flag) {
			result->bw_rd[i] =
				read_all_g_pcie(base, i, CNT0_RD_BW_PERFM);
			result->bw_rd[i] = (uint64_t)result->bw_rd[i] *
					   bytes_per_cycle *
					   chn_info->axi_freq / win_len_clk /
					   PERF_BW_CALC_UNIT;
		}
	}

	/* Calculate average latency */
	flag = read_field_bg_pcie(base, CNT_OVFW_FLAG1_PERFM, 0,
			     CNT0_WR_CMD_OVFW_PERFM);
	result->flags.bits.cmd_wr_of = flag & 0x1;
	if (!flag) {
		result->cmd_total_wr = read_all_g_pcie(base, 0, CNT0_WR_CMD_PERFM);
		result->latcy_ave_wr =
			result->latcy_total_wr / (int64_t)result->cmd_total_wr;
	}

	flag = read_field_bg_pcie(base, CNT_OVFW_FLAG1_PERFM, 0,
			     CNT0_RD_CMD_OVFW_PERFM);
	result->flags.bits.cmd_rd_of = flag & 0x1;
	if (!flag) {
		result->cmd_total_rd = read_all_g_pcie(base, 0, CNT0_RD_CMD_PERFM);
		result->latcy_ave_rd =
			result->latcy_total_rd / (int64_t)result->cmd_total_rd;
	}
}

void busmon_perf_print_result(struct busmon_perf_cnt_result_s *result)
{
	int i;

	/* Not print for idle window */
	if ((result->flags.raw == 0) && (result->latcy_total_wr == 0) &&
	    (result->latcy_total_rd == 0))
		return;

	pr_info("ispv4 busmon perf print start\n");

	pr_info("ispv4 busmon time: %ld us\n", result->time_us);

	if (result->flags.bits.latcy_wr_uf)
		pr_info("ispv4 busmon latcy_wr: underflow!\n");
	else if (result->flags.bits.latcy_wr_of)
		pr_info("ispv4 busmon latcy_wr: overflow!\n");
	else if (result->flags.bits.cmd_wr_of)
		pr_info("ispv4 busmon latcy_wr: cmd cnt overflow!\n");
	else
		pr_info("ispv4 busmon latcy_wr: total=%ld, ave=%ld\n",
			result->latcy_total_wr, result->latcy_ave_wr);

	if (result->flags.bits.latcy_rd_uf)
		pr_info("ispv4 busmon latcy_rd: underflow!\n");
	else if (result->flags.bits.latcy_rd_of)
		pr_info("ispv4 busmon latcy_rd: overflow!\n");
	else if (result->flags.bits.cmd_rd_of)
		pr_info("ispv4 busmon latcy_rd: cmd cnt overflow!\n");
	else
		pr_info("ispv4 busmon latcy_rd: total=%ld, ave=%ld\n",
			result->latcy_total_rd, result->latcy_ave_rd);

	for (i = 0; i < ISPV4_BUSMON_PERF_CNT_PER_CHN; i++) {
		if (!(result->bw_enable & (1 << i)))
			continue;

		if (i == 0)
			pr_info("ispv4 busmon total bw:");
		else
			pr_info("ispv4 busmon bw[%d]:", i - 1);

		if (result->bw_overflow &
		    (1 << (PERF_BW_OVERFLOW_WR_SHIFT + i)))
			pr_info("ispv4 busmon wr-overflow,");
		else
			pr_info("ispv4 busmon wr-%ldKB/s,", result->bw_wr[i]);

		if (result->bw_overflow &
		    (1 << (PERF_BW_OVERFLOW_RD_SHIFT + i)))
			pr_info("ispv4 busmon rd-overflow\n");
		else
			pr_info("ispv4 busmon rd-%ldKB/s\n", result->bw_rd[i]);
	}

	pr_info("ispv4 busmon perf print end\n");
}

static void _perf_write_result(struct perf_mgr_s *mgr,
			       struct busmon_perf_cnt_result_s *result)
{
	size_t wr_size = sizeof(struct busmon_perf_cnt_result_s);
	struct busmon_perf_cnt_result_s *wr_point = NULL;
	char blob_name[128];

	if (mgr->mem_full == false) {
		if (mgr->new_wr_size < mgr->mem_size) {

			wr_point = (struct busmon_perf_cnt_result_s
					    *)(mgr->mem_addr_start +
					       mgr->cur_wr_offset);
			memcpy(wr_point, result, wr_size);
			mgr->cur_wr_offset++;
			mgr->new_wr_size += wr_size;
		} else {
			snprintf(blob_name, sizeof(blob_name),
				 "perfm_chn%d_outfile", mgr->chn);

			mgr->blob_buf.data = mgr->mem_addr_start;
			mgr->blob_buf.size = mgr->mem_size;
			mgr->debugfs_blob = debugfs_create_blob(
				blob_name, 0222, debugfs_param.busmon_debugfs,
				&mgr->blob_buf);

			mgr->mem_full = true;
			_perf_stop_mon(mgr->chn_info);
			pr_info("ispv4 busmon wr_size = %ld ; mem_size = %ld\n",
				mgr->new_wr_size, mgr->mem_size);
			pr_err("ispv4 busmon perfm storage space is full!\n");
		}
	}
}
//perfm_worker
void _perfm_cnt_worker(struct work_struct *work)
{
	struct busmon_perf_cnt_result_s result;
	struct perf_mgr_s *mgr = container_of(work, struct perf_mgr_s, work);
	pr_info("ispv4 busmon irq into worker  chn: %d , base: %lx", mgr->chn ,mgr->chn_info->inst_info->base);
	memset(&result, 0, sizeof(result));
	_perf_get_result(mgr, &result);

	if (mgr->option == BUSMON_PERF_OUTPUT_PRINT)
		busmon_perf_print_result(&result);
	else
		_perf_write_result(mgr, &result);
}

static void _perf_dump_param(enum busmon_chn chn,
			     struct busmon_perf_param_s *param)
{
	int i;

	pr_info("ispv4 perf param: chn[%d], win_len_us=%ld, out_option=%d\n",
		chn, param->win_len_us, param->output_cfg.option);

	for (i = 0; i < ISPV4_BUSMON_PERF_CNT_PER_CHN; i++) {
		if (!param->count_param[i].enable)
			continue;

		pr_info("ispv4 perfm chn[%d]: filter[%d], mode=%d, addr=0x%lx ~ 0x%lx, "
			"id=0x%x ~ 0x%x, "
			"addr_exc=%s, id_exc=%s\n",
			chn, i, param->count_param[i].mode, param->count_param[i].filter.addr_min,
			param->count_param[i].filter.addr_max, param->count_param[i].filter.id_min,
			param->count_param[i].filter.id_max,
			param->count_param[i].filter.addr_exc ? "true" : "false",
			param->count_param[i].filter.id_exc ? "true" : "false");
	}
}

int _perf_dev_init(struct perf_dev_s *dev)
{
	int i;
	if (dev->inited) {
		pr_info("ispv4 busmon perf_dev is already initialized!\n");
		return 0;
	}

	for (i = 0; i < BUSMON_CHN_CNT; i++) {
		dev->mgr[i].busy = false;
		dev->mgr[i].chn_info = &g_busmon_chn_info[i];
		dev->mgr[i].mem_addr_start = NULL;
		dev->mgr[i].mem_full = false;
		dev->mgr[i].cur_wr_offset = 0;
		dev->mgr[i].new_wr_size = 0;
		dev->mgr[i].debugfs_blob = NULL;
		mutex_init(&dev->mgr[i].perfm_mutex);
		INIT_WORK(&dev->mgr[i].work, _perfm_cnt_worker);
	}
	dev->inited = true;
	pr_info("ispv4 busmon perf_dev param init finish!\n");
	return 0;
}

static void _perf_hw_cfg(const struct busmon_chn_info_s *chn_info,
			 struct busmon_perf_param_s *param)
{
	int i;
	uintptr_t base = chn_info->inst_info->base;

	/* cfg_en: pull low */
	write_field(base, PERFM_WIN_CFG, PERFM_CFG_EN, 0);
	write_field(base, BUSMON_COM_CFG, AXI_GRP_SEL, chn_info->axi_sel);

	/* clear interrupt */
	write_all(base, BUSMON_INT_CLR,
		  (1 << INT_CLR_PERFM_CNT_SHIFT) |
			  (1 << INT_CLR_LATCY_BAD_SHIFT));
	write_all(base, BUSMON_INT_CLR, 0);

	/* enable perf_cnt interrupt */
	write_field(base, BUSMON_INT_MASK, INT_MASK_PERFM_CNT, 0);

	/* disable latcy_bad alarm */
	write_field(base, BUSMON_INT_MASK, INT_MASK_LATCY_BAD, 1);
	write_all(base, PERFM_LATCY_TH, 0);
	write_all(base, PERFM_TIMEOUT_TH, 0);

	/* window length */
	write_all(base, PERFM_WIN_LEN,
		  US_TO_CLK_CYC(param->win_len_us, chn_info->axi_freq));

	/* use group0 for total bw & cmd_cnt */
	write_field_bg(base, PERFM_CNT_GRP_EN, 0, PERFM_CNT_GRP0_EN, 1);
	write_field_bg(base, PERFM_CNT_MODE, 0, PERFM_CNT0_MODE, 0);

	/* config addr & id */
	for (i = 1; i < ISPV4_BUSMON_PERF_CNT_PER_CHN; i++) {
		int j = i - 1; /* index for param array */
		if (!param->count_param[j].enable) {
			write_field_bg(base, PERFM_CNT_GRP_EN, i,
				       PERFM_CNT_GRP0_EN, 0);
			continue;
		}

		write_field_bg(base, PERFM_CNT_GRP_EN, i, PERFM_CNT_GRP0_EN, 1);

		if ((param->count_param[j].mode == BUSMON_PERF_CNT_ID) ||
		    (param->count_param[j].mode == BUSMON_PERF_CNT_ID_ADDR)) {
			write_all_g(base, i, PERFM_MIN_ID0,
				    param->count_param[j].filter.id_min);
			write_all_g(base, i, PERFM_MAX_ID0,
				    param->count_param[j].filter.id_max);
			write_field_bg(
				base, PERFM_CNT_MODE, i, PERFM_ID_EXC_EN0,
				param->count_param[j].filter.id_exc ? 1 : 0);
		}

		if ((param->count_param[j].mode == BUSMON_PERF_CNT_ADDR) ||
		    (param->count_param[j].mode == BUSMON_PERF_CNT_ID_ADDR)) {
			write_all_g(base, i, PERFM_MIN_ADDR0_H,
				    param->count_param[j].filter.addr_min >>
					    32);
			write_all_g(base, i, PERFM_MIN_ADDR0_L,
				    param->count_param[j].filter.addr_min &
					    0xffffffff);
			write_all_g(base, i, PERFM_MAX_ADDR0_H,
				    param->count_param[j].filter.addr_max >>
					    32);
			write_all_g(base, i, PERFM_MAX_ADDR0_L,
				    param->count_param[j].filter.addr_max &
					    0xffffffff);
			write_field_bg(
				base, PERFM_CNT_MODE, i, PERFM_ADDR_EXC_EN0,
				param->count_param[j].filter.addr_exc ? 1 : 0);
		}

		write_field_bg(base, PERFM_CNT_MODE, i, PERFM_CNT0_MODE,
			       param->count_param[j].mode);
	}
	/* sw control & loop mode
	 *      [5]: perfm_win_sw_mode = 0
	 *      [4]: perfm_win_loop_mode = 1
	 *      [3]: perfm_win_loop_en = 1
	 *      [2]: perfm_win_trig_mode = 0
	 */
	write_field(base, PERFM_WIN_CFG, PERFM_WIN_SW_MODE, 0);
	write_field(base, PERFM_WIN_CFG, PERFM_WIN_LOOP_MODE, 1);
	write_field(base, PERFM_WIN_CFG, PERFM_WIN_LOOP_EN, 1);
	write_field(base, PERFM_WIN_CFG, PERFM_WIN_TRIG_MODE, 0);

	/* cfg_en: pull high */
	write_field(base, PERFM_WIN_CFG, PERFM_CFG_EN, 1);
	write_field(base, BUSMON_COM_CFG, PERFM_EN, 1);
	pr_info("ispv4 busmon perfm chn[%d]register init finished!\n",
		param->chn);
}

static void _perf_start_mon(const struct busmon_chn_info_s *chn_info)
{
	uintptr_t base = chn_info->inst_info->base;

	/* cfg_en: pull low */
	write_field(base, PERFM_WIN_CFG, PERFM_CFG_EN, 0);
	write_field(base, PERFM_WIN_CFG, PERFM_WIN_SW_TRIG_EN, 1);
	/* cfg_en: pull high */
	write_field(base, PERFM_WIN_CFG, PERFM_CFG_EN, 1);
}

int busmon_perf_start(struct busmon_perf_param_s *param)
{
	struct perf_dev_s *perf_dev;
	struct perf_mgr_s *mgr;
	const struct busmon_chn_info_s *chn_info;
	int inst;
	int merge_int_shift = 0;
	int chn = param->chn;

	if (chn >= BUSMON_CHN_CNT || chn < APB_BUSMON_CHN_START) {
		pr_err("ispv4 busmon perfm input: invalid chn %d\n", chn);
		return -EINVAL;
	}

	if ((param->output_cfg.mem_size % BUSMON_PERF_MEM_SIZE_MULTIPLE) != 0) {
		pr_err("ispv4 busmon mem_size should be multiple of 64KB\n");
		return -EINVAL;
	}

	perf_dev = &g_perf_dev;
	/*print input parameters*/
	_perf_dump_param(chn, param);

	if (!perf_dev->inited)
		_perf_dev_init(perf_dev);

	mgr = &perf_dev->mgr[chn];
	chn_info = mgr->chn_info;
	inst = g_chn_2_inst[chn];
	mgr->chn = chn;

	if (mgr->busy || g_busmon_inst_stat[inst].match_busy) {
		pr_err("ispv4 busmon instance busy\n");
		return -EBUSY;
	}

	pr_info("ispv4 busmon perfm hw_info: base=0x%lx,  axi_sel=%d\n",
		chn_info->inst_info->base, chn_info->axi_sel);

	mutex_lock(&mgr->perfm_mutex);
	_perf_hw_cfg(chn_info, param);
	mgr->mem_addr_start = NULL;
	mgr->mem_size = param->output_cfg.mem_size;
	mgr->option = param->output_cfg.option;
	mgr->cur_wr_offset = 0;
	mgr->new_wr_size = 0;
	mgr->mem_full = false;
	mgr->debugfs_blob = NULL;

	if (mgr->option == BUSMON_PERF_OUTPUT_WR_MEM) {
		// allocating memory space
		pr_info("ispv4 busmon into memory option %d; size %d\n", mgr->option,
			mgr->mem_size);
		mgr->mem_addr_start = (struct busmon_perf_cnt_result_s *)vmalloc(mgr->mem_size);
		if (mgr->mem_addr_start == NULL) {
			pr_err("ispv4 busmon malloc mem failed!\n");
			return -EINVAL;
		}
		pr_info("ispv4 busmon chn[%d] mem malloc succeed!\n", chn);
	}

	//ddr_INTC_irq_set
	if ((inst >= DDR_BUSMON_INST_START) &&
	    (inst < (DDR_BUSMON_INST_START + DDR_BUSMON_INST_CNT))) {
		merge_int_shift = DDR_BUSMON_INT_PERFM_CNT_SHIFT(inst - DDR_BUSMON_INST_START);
		_intc_int_enable(&g_ddr_int_mgr, merge_int_shift);
	}

	g_busmon_inst_stat[inst].perf_busy = true;
	mgr->busy = true;
	if (chn >= DDR_BUSMON_ISP_NIC0 && chn <= DDR_BUSMON_MAIN_NIC)
		atomic_inc(&g_irq_manage.ref_ddr_en);
	else if (chn >= ISP_BUSMON_FE0 && chn <= ISP_BUSMON_CMDDMA)
		atomic_inc(&g_irq_manage.ref_isp_en);
	else {
		atomic_inc(&g_irq_manage.ref_top_en);
	}
	gpio_busmon_irq_set();

	_perf_start_mon(chn_info);

	mutex_unlock(&mgr->perfm_mutex);

	pr_info("ispv4 busmon chn[%d] perf_start finish!\n", chn);
	return 0;
}

void busmon_perf_stop(enum busmon_chn chn)
{
	struct perf_dev_s *perf_dev;
	struct perf_mgr_s *mgr;
	const struct busmon_chn_info_s *chn_info;
	int inst;
	uintptr_t base;

	if (chn >= BUSMON_CHN_CNT) {
		pr_info("ispv4 busmon perfm stop: invalid chn %d\n", chn);
		return;
	}

	perf_dev = &g_perf_dev;

	if (!perf_dev->inited) {
		_perf_dev_init(perf_dev);
		return;
	}

	mgr = &perf_dev->mgr[chn];
	chn_info = mgr->chn_info;
	inst = g_chn_2_inst[chn];

	if (mgr->busy && g_busmon_inst_stat[inst].perf_busy) {
		mutex_lock(&mgr->perfm_mutex);
		_perf_stop_mon(chn_info);
		//* reset irq */
		base = chn_info->inst_info->base;
		write_all_pcie(base, BUSMON_INT_CLR, 1 << INT_CLR_PERFM_CNT_SHIFT);

		mgr->busy = false;
		g_busmon_inst_stat[inst].perf_busy = false;

		debugfs_remove(mgr->debugfs_blob);
		if (mgr->mem_addr_start != NULL) {
			vfree(mgr->mem_addr_start);
			pr_info("ispv4 busmon chn[%d] mem malloc free!", chn);
			mgr->mem_addr_start = NULL;
		}

		if (chn >= DDR_BUSMON_ISP_NIC0 && chn <= DDR_BUSMON_MAIN_NIC)
			atomic_dec(&g_irq_manage.ref_ddr_en);
		else if (chn >= ISP_BUSMON_FE0 && chn <= ISP_BUSMON_CMDDMA)
			atomic_dec(&g_irq_manage.ref_isp_en);
		else {
			atomic_dec(&g_irq_manage.ref_top_en);
		}
		gpio_busmon_irq_set();

		mutex_unlock(&mgr->perfm_mutex);
	}

	pr_info("ispv4 busmon chn[%d] perf_stop finish!\n", chn);
}

static void ispv4_busmon_irq_perfm(uint32_t int_status)
{
	uintptr_t base;
	uint32_t perfm_int_status = int_status;
	int index = 0;
	struct perf_mgr_s *mgr;
	//inst0=bit9, inst1=bit1, inst2=bit5
	static int perfm_arr[] = { AP_REQ_PERFM_CNTTT_BIT, AP_REQ_PERFM_CNT_BIT,
				   AP_REQ_PERFM_CNTT_BIT };
	static int perfm_inst[] = { APB_BUSMON_INST0, APB_BUSMON_INST1, APB_BUSMON_INST2 };
	static int perfm_chn[] = { APB_BUSMON_PCIE, APB_BUSMON_MIPI, APB_BUSMON_CPU,
				   APB_BUSMON_DMA,  APB_BUSMON_OCM,  APB_BUSMON_NPU };

	for (index = 0; index < 3; index++) {
		if (perfm_arr[index] & perfm_int_status) {
			base = g_busmon_inst_info[perfm_inst[index]].base;
			if (g_busmon_inst_stat[perfm_inst[index]].perf_busy) {
				if (g_perf_dev.mgr[perfm_chn[2 * index]].busy) {
					mgr = &g_perf_dev.mgr[perfm_chn[2 * index]];
				} else if (g_perf_dev.mgr[perfm_chn[2 * index + 1]].busy) {
					mgr = &g_perf_dev.mgr[perfm_chn[2 * index + 1]];
				}else {
					goto clean;
				}
				if (mgr->mem_full == false) {
					queue_work(system_highpri_wq, &mgr->work);
					mgr->int_time_us = (ktime_get_ns() / 1000);
				}
				// _perfm_cnt_worker(&mgr->work);
			}
		clean:
			// clear raw_perfm_cnt
			write_all_pcie(base, BUSMON_INT_SET, 0);
			write_all_pcie(base, BUSMON_INT_CLR, 1 << INT_CLR_PERFM_CNT_SHIFT);
			write_all_pcie(base, BUSMON_INT_CLR, 0);
		}
	}
}

static void ispv4_busmon_irq_match(uint32_t int_status)
{
	uintptr_t base;
	uint32_t match_int_status = int_status;
	int index = 0;
	//inst0=bit8, inst1=bit0, inst2=bit4
	static int match_arr[] = { AP_REQ_MATCHTT_BIT, AP_REQ_MATCH_BIT,
				   AP_REQ_MATCHT_BIT };
	static int match_inst[] = { APB_BUSMON_INST0, APB_BUSMON_INST1,
				    APB_BUSMON_INST2 };

	for (index = 0; index < 3; index++) {
		if (match_arr[index] & match_int_status) {
			base = g_busmon_inst_info[match_inst[index]].base;
			if (g_busmon_inst_stat[match_inst[index]].match_busy) {
				match_work_s.arg = match_inst[index];
				queue_work(system_highpri_wq,
					   &match_work_s.busmon_work);
			}
			// clear interrupt
			write_all_pcie(base, BUSMON_INT_SET, 0);
			write_all_pcie(base, BUSMON_INT_CLR, 1 << INT_CLR_MATCH_SHIFT);
			write_all_pcie(base, BUSMON_INT_CLR, 0);
		}
	}
}
static void ispv4_busmon_irq_isp(void)
{
	int index;
	uintptr_t base;
	int inst;
	struct perf_mgr_s *mgr;
	int chn;

	/*get isp_sys irq_raw*/
	g_isp_int_mgr.int_stat = pcie_read_reg(g_isp_int_mgr.base + g_isp_int_mgr.stat_offset);
	// pr_info("ispv4 busmon isp irq raw  %lx = %lx ",
	// 	g_isp_int_mgr.base + g_isp_int_mgr.stat_offset, g_isp_int_mgr.int_stat);

	if (g_isp_int_mgr.int_stat) {
		for (index = 0; index < ISP_BUSMON_ARR_LEN; index++) {
			if (0 == (g_isp_int_mgr.int_stat & (1 << isp_busmon_bit[index])))
				continue;
			// pr_info("ispv4 busmon isp irq index %d", index);
			inst = ISP_BUSMON_INST0 + index / 2;
			base = g_busmon_inst_info[inst].base;

			/*match_mode*/
			if (index % 2 == ISP_BUSMON_MATCH_MODE) {
				pr_info("ispv4 busmon isp irq into match");
				match_work_s.arg = inst;
				queue_work(system_highpri_wq, &match_work_s.busmon_work);
				/* Clear interrupt */
				write_all_pcie(base, BUSMON_INT_SET, 0);
				write_all_pcie(base, BUSMON_INT_CLR, 1 << INT_CLR_MATCH_SHIFT);
				write_all_pcie(base, BUSMON_INT_CLR, 0);
			}
			/*perfm_mode*/
			else {
				for (chn = ISP_BUSMON_FE0; chn < BUSMON_CHN_CNT; chn++) {
					if (inst != g_chn_2_inst[chn])
						continue;
					pr_info("ispv4 busmon isp irq into perfm");
					mgr = &g_perf_dev.mgr[chn];
					pr_info("ispv4 busmon isp irq chn %d, busy %d", chn, mgr->busy);
					if (mgr->busy) {
						if (mgr->mem_full == false) {
							mgr->int_time_us = (ktime_get_ns() / 1000);
							queue_work(system_highpri_wq, &mgr->work);
						}
						// _perfm_cnt_worker(&mgr->work);
						/* Clear interrupt */
						write_all_pcie(base, BUSMON_INT_SET, 0);
						write_all_pcie(base, BUSMON_INT_CLR, 1 << INT_CLR_PERFM_CNT_SHIFT);
						write_all_pcie(base, BUSMON_INT_CLR, 0);
					}
				}
			}
		}
	}
}

__maybe_unused static irqreturn_t ispv4_busmon_irq_ddr(void)
{
	int index;
	uintptr_t base;
	int inst;
	struct perf_mgr_s *mgr;
	int chn;
	uint32_t intc_irq_static;

	g_ddr_int_mgr.int_stat = pcie_read_reg(g_ddr_int_mgr.base + g_ddr_int_mgr.stat_offset);	//get intc mask value
	pr_info("ispv4 busmon interrupt ddr intc mask value 0x%lx ", g_ddr_int_mgr.int_stat);
	intc_irq_static = pcie_read_reg(g_ddr_int_mgr.base + INTC_REG_INT_RAWSTATUS_OFFSET);		//get intc raw
	pr_info("ispv4 busmon interrupt ddr intc irq value 0x%lx ", intc_irq_static);
	if (intc_irq_static & 0x6db6) {
		/* 0x6db6: bit 1,2 4,5 7,8 10,11 13,14 */
		for (index = 0; index < DDR_BUSMON_ARR_LEN; index++) {
			if (0 == (g_ddr_int_mgr.int_stat & (1 << ddr_busmon_bit[index])) ||
			    0 == (intc_irq_static & (1 << ddr_busmon_bit[index])))
				continue;
			inst = DDR_BUSMON_INST0 + index / 2;
			base = g_busmon_inst_info[inst].base;
			pr_info(" ispv4 busmon into irq ddr , inst : %d , index %d", inst, index);

			/*match_mode*/
			if (index % 2 == DDR_BUSMON_MATCH_MODE) {
				pr_info(" ispv4 busmon into ddr match");
				match_work_s.arg = inst;
				queue_work(system_highpri_wq, &match_work_s.busmon_work);
				/* Clear interrupt */
				write_all_pcie(base, BUSMON_INT_SET, 0);
				write_all_pcie(base, BUSMON_INT_CLR, 1 << INT_CLR_MATCH_SHIFT);
				write_all_pcie(base, BUSMON_INT_CLR, 0);

			} else { /*perfm_mode*/
				chn = DDR_BUSMON_ISP_NIC0 + inst - DDR_BUSMON_INST0;
				pr_info(" ispv4 busmon into ddr perfm chn %d", chn);
				mgr = &g_perf_dev.mgr[chn];
				if (mgr->busy) {
					if (mgr->mem_full == false) {
						mgr->int_time_us = ktime_get_ns();
						queue_work(system_highpri_wq, &mgr->work);
					}

					// _perfm_cnt_worker(&mgr->work);
					/* Clear interrupt */
					pr_info(" ispv4 busmon into ddr perfm clean irq");
					write_all_pcie(base, BUSMON_INT_SET, 0);
					write_all_pcie(base, BUSMON_INT_CLR, 1 << INT_CLR_PERFM_CNT_SHIFT);
					write_all_pcie(base, BUSMON_INT_CLR, 0);
				}
			}
		}
		g_ddr_int_mgr.int_clear_func(&g_ddr_int_mgr, g_ddr_int_mgr.int_stat);	// clear INTC
	}
	return 0;
}

//main function
static irqreturn_t ispv4_busmon_irq(int irq, void *dev_id)
{
	uint32_t int_status;
	uint32_t int_status_ddr;
	bool irq_response = false;

	if (g_irq_manage.bus_monitor_start == false) {
		pr_info("ispv4 busmon interrupt pmu");
		ispv4_regops_clear_and_set(AP_INTC_G2R1_INT_MASK_REG_ADDR,
				AP_PMU_INT_BIT, AP_PMU_INT_BIT);
		return IRQ_NONE;
	}

	int_status = pcie_read_reg(AP_INTC_G0R0_INT_MASK_REG_ADDR + INTC_G2R2_INT_STATUS);
	if (atomic_read(&g_irq_manage.ref_top_en) > 0) {
		/*top irq*/
		if (int_status & AP_REQ_MATCH) {
			/*top match irq*/
			// pr_info("ispv4 busmon interrupt top");
			ispv4_busmon_irq_match(int_status);
			irq_response = true;
		} else if (int_status & AP_REQ_PERFM_CNT) {
			/*top perfm irq*/
			// pr_info("ispv4 busmon interrupt top");
			ispv4_busmon_irq_perfm(int_status);
			irq_response = true;
		}
	}

	/*isp irq*/
	if ((atomic_read(&g_irq_manage.ref_isp_en) > 0) && (int_status & AP_ISP_BUSMON_BIT)) {
		// pr_info("ispv4 busmon interrupt isp");
		ispv4_busmon_irq_isp();
		irq_response=true;
	}

	/*ddr irq*/
	if (atomic_read(&g_irq_manage.ref_ddr_en) > 0) {
		int_status_ddr = pcie_read_reg(AP_INTC_G0R0_INT_MASK_REG_ADDR + INTC_G2R1_INT_STATUS);
		if (int_status_ddr & AP_DDR_BUSMON_BIT) {
			// pr_info("ispv4 busmon interrupt ddr");
			ispv4_busmon_irq_ddr();
			irq_response = true;
		}
	}

	if(irq_response)
		return IRQ_HANDLED;
	return IRQ_NONE;
}

/* IOCTL FOPS now not in use */
static long ispv4_busmon_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct busmon_match_param_s match_param;
	struct busmon_perf_param_s perfm_param;

	switch (cmd) {
	case ISPV4_BUSMON_MATCH_START:
		if (copy_from_user(&match_param, (char __user *)arg, sizeof(match_param))) {
			return -EINVAL;
		}
		ret = busmon_match_start(&match_param);
		break;
	case ISPV4_BUSMON_MATCH_STOP:
		busmon_match_stop(arg);
		ret = 0;
		break;
	case ISPV4_BUSMON_PREFM_START:
		if (copy_from_user(&perfm_param, (char __user *)arg, sizeof(perfm_param))) {
			return -EINVAL;
		}
		ret = busmon_perf_start(&perfm_param);
		break;
	case ISPV4_BUSMON_PREFM_STOP:
		busmon_perf_stop(arg);
		ret = 0;
		break;
	default:
		printk("ispv4 busmon ioctl error\n");
		return -EINVAL;
		break;
	}
	return ret;
}

static int gpio_busmon_irq_assign(struct device *dev, unsigned int irq)
{
	int ret;
	unsigned long irqflags;

	irqflags = IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED;
	ret = devm_request_threaded_irq(dev, irq, NULL, ispv4_busmon_irq, irqflags,
					"ispv4_busmon", dev);
	g_irq_manage.bus_monitor_start = false;
	pr_info("ispv4 busmon irq num:%d", irq);
	if (ret) {
		pr_err("ispv4 busmon irq threaded assign error \n");
		return ret;
	}
	return 0;
}

static ssize_t match_mode_set(struct file *file, const char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	int mode;
	struct busmon_match_param_s param;
	uint32_t reg_read;
	param.chn = debugfs_param.debug_chn;
	param.filter.addr_max = debugfs_param.debug_max_addr;
	param.filter.addr_min = debugfs_param.debug_min_addr;
	param.filter.id_max = debugfs_param.debug_max_id;
	param.filter.id_min = debugfs_param.debug_min_id;
	param.filter.addr_exc = debugfs_param.addr_exc;
	param.filter.id_exc = debugfs_param.id_exc;

	(void)kstrtouint_from_user(user_buf, count, 10, &mode);

	if (mode == 0) {
		/* reset iatu base0 */
		pcie_iatu_reset(ISPV4_BUSMON_0_ADDR_OFFSET);
	} else if (mode == 1) {
		isp_busmon_en();
		g_irq_manage.bus_monitor_start = true;
		busmon_match_start(&param);
	} else if (mode == 2) {
		busmon_match_stop(debugfs_param.debug_chn);
		g_irq_manage.bus_monitor_start = false;
	} else if (mode == 3) {
		_match_get_result(g_chn_2_inst[debugfs_param.debug_chn]);
	} else {
		reg_read = pcie_read_reg(mode);
		pr_err("ispv4 busmon debugfs read reg0x%x: 0x%x", mode, reg_read);
		pr_err("ispv4 busmon debugfs input param error!");
	}
	return count;
}

static ssize_t perfm_mode_set(struct file *file, const char __user *user_buf, size_t count,
			      loff_t *ppos)
{
	int mode;

	(void)kstrtouint_from_user(user_buf, count, 10, &mode);
	if (mode == 0) {
		/* clear perf_param_buf and reset iatu base0 */
		pcie_iatu_reset(ISPV4_BUSMON_0_ADDR_OFFSET);
		memset(&perf_param_buf, 0, sizeof(perf_param_buf));
	} else if (mode == 1) {
		/* update set param */
		if (debugfs_param.perf_param_cnt < 7) {
			perf_param_buf.count_param[debugfs_param.perf_param_cnt].enable =
				debugfs_param.perf_enable;
			perf_param_buf.count_param[debugfs_param.perf_param_cnt].mode =
				debugfs_param.perf_count_mode;

			perf_param_buf.count_param[debugfs_param.perf_param_cnt].filter.addr_max =
				debugfs_param.perf_max_addr;
			perf_param_buf.count_param[debugfs_param.perf_param_cnt].filter.addr_min =
				debugfs_param.perf_min_addr;
			perf_param_buf.count_param[debugfs_param.perf_param_cnt].filter.id_max =
				debugfs_param.perf_max_id;
			perf_param_buf.count_param[debugfs_param.perf_param_cnt].filter.id_min =
				debugfs_param.perf_min_id;
			perf_param_buf.count_param[debugfs_param.perf_param_cnt].filter.addr_exc =
				debugfs_param.perf_addr_exc;
			perf_param_buf.count_param[debugfs_param.perf_param_cnt].filter.id_exc =
				debugfs_param.perf_id_exc;

			perf_param_buf.chn = debugfs_param.perf_chn;

			if (debugfs_param.len_us < 5000) {
				pr_err("ispv4 busmon len_us needs to be at least 5ms!");
				goto out;
			}

			perf_param_buf.win_len_us = debugfs_param.len_us;
			perf_param_buf.output_cfg.option = debugfs_param.output_option;
			perf_param_buf.output_cfg.mem_size = debugfs_param.perf_mem_size;
		}
		_perf_dump_param(debugfs_param.perf_chn, &perf_param_buf);
	} else if (mode == 2) {
		/*perfm monitor start*/
		g_irq_manage.bus_monitor_start = true;
		isp_busmon_en();
		busmon_perf_start(&perf_param_buf);
	} else if (mode == 3) {
		/*perfm monitor stop*/
		busmon_perf_stop(debugfs_param.perf_chn);
		g_irq_manage.bus_monitor_start = false;
	} else {
		pr_err("ispv4 busmon debugfs input param error!");
	}
out:
	return count;
}

static const struct file_operations ispv4_busmon_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ispv4_busmon_unlocked_ioctl,
};

static struct miscdevice ispv4_busmon_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ispv4_busmon",
	.fops = &ispv4_busmon_fops,
};

static struct file_operations match_mode_fops = {
	.open = simple_open,
	.write = match_mode_set,
};

static struct file_operations perfm_mode_fops = {
	.open = simple_open,
	.write = perfm_mode_set,
};

static int ispv4_busmon_debugfs_create(void)
{
	struct dentry *debugfs_file;

	debugfs_param.busmon_debugfs = debugfs_create_dir("ispv4_busmon", ispv4_debugfs);

	if (!IS_ERR_OR_NULL(debugfs_param.busmon_debugfs)) {
		/*match*/
		debugfs_param.match_debugfs_menu = debugfs_create_dir(
			"match", debugfs_param.busmon_debugfs);
		debugfs_create_u32("min_addr", 0666,
				   debugfs_param.match_debugfs_menu,
				   &debugfs_param.debug_min_addr);
		debugfs_create_u32("max_addr", 0666,
				   debugfs_param.match_debugfs_menu,
				   &debugfs_param.debug_max_addr);
		debugfs_create_u32("min_id", 0666,
				   debugfs_param.match_debugfs_menu,
				   &debugfs_param.debug_min_id);
		debugfs_create_u32("max_id", 0666,
				   debugfs_param.match_debugfs_menu,
				   &debugfs_param.debug_max_id);
		debugfs_create_u32("chn", 0666,
				   debugfs_param.match_debugfs_menu,
				   &debugfs_param.debug_chn);
		debugfs_create_u32("addr_exc", 0666,
				   debugfs_param.match_debugfs_menu,
				   &debugfs_param.addr_exc);
		debugfs_create_u32("id_exc", 0666,
				   debugfs_param.match_debugfs_menu,
				   &debugfs_param.id_exc);

		debugfs_file =
			debugfs_create_file("match_mode", 0222,
					    debugfs_param.match_debugfs_menu,
					    NULL, &match_mode_fops);

		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("mode debugfs init failed %d\n", PTR_ERR(debugfs_file));
		}

		/*perfm*/
		debugfs_param.perfm_debugfs_menu = debugfs_create_dir(
			"perfm", debugfs_param.busmon_debugfs);
		debugfs_create_u32("len_us", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.len_us);
		debugfs_create_u32("perf_out_op", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.output_option);
		debugfs_create_u32("perf_mem_size", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_mem_size);
		debugfs_create_u32("perf_cnt", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_param_cnt);
		debugfs_create_u32("perf_chn", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_chn);

		debugfs_create_u32("min_addr", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_min_addr);
		debugfs_create_u32("max_addr", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_max_addr);
		debugfs_create_u32("min_id", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_min_id);
		debugfs_create_u32("max_id", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_max_id);
		debugfs_create_u32("addr_exc", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_addr_exc);
		debugfs_create_u32("id_exc", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_id_exc);
		debugfs_create_u32("perf_count_mode", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_count_mode);
		debugfs_create_u32("perf_enable", 0666,
				   debugfs_param.perfm_debugfs_menu,
				   &debugfs_param.perf_enable);

		debugfs_file =
			debugfs_create_file("perfm_mode", 0222,
					    debugfs_param.perfm_debugfs_menu,
					    NULL, &perfm_mode_fops);

		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("mode debugfs init failed %d\n", PTR_ERR(debugfs_file));
		}
	} else
		return -EINVAL;
	return 0;
}

static int xm_ispv4_busmon_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res_irq;

	ret = misc_register(&ispv4_busmon_miscdev);
	if (ret != 0) {
		pr_err("ispv4 busmon misc register failed %d!\n", ret);
		return ret;
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res_irq->start) {
		ret = gpio_busmon_irq_assign(&pdev->dev, res_irq->start);
		if (ret) {
			pr_err("ispv4 busmon irq assign error\n");
			goto err;
		}
	}
	ret = ispv4_busmon_debugfs_create();
	if (ret) {
		pr_err("ispv4 busmon debugfs create error\n");
		goto err;
	}
	pr_info("ispv4 busmon probe finish!\n");
	return 0;

err:
	misc_deregister(&ispv4_busmon_miscdev);
	return ret;
}

static int xm_ispv4_busmon_remove(struct platform_device *pdev)
{
	debugfs_remove(debugfs_param.busmon_debugfs);
	misc_deregister(&ispv4_busmon_miscdev);
	pr_info("ispv4 busmon remove finish!\n");
	return 0;
}

static const struct platform_device_id busmon_id_table[] = {
	{
		.name = "xm-ispv4-busmon",
		.driver_data = 0,
	},
	{}
};
MODULE_DEVICE_TABLE(platform, busmon_id_table);

static struct platform_driver xm_ispv4_busmon_driver = {
	.driver = {
			.name = "xm-ispv4-busmon",
			.probe_type = PROBE_FORCE_SYNCHRONOUS,
		},
	.probe = xm_ispv4_busmon_probe,
	.remove = xm_ispv4_busmon_remove,
	.id_table = busmon_id_table,
	.prevent_deferred_probe = true,
};
module_platform_driver(xm_ispv4_busmon_driver);

MODULE_AUTHOR("lizexin <lizexinxiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ispv4 busmon driver");
MODULE_LICENSE("GPL v2");
