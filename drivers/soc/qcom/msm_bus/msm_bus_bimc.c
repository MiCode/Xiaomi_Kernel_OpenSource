/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "AXI: BIMC: %s(): " fmt, __func__

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/msm-bus-board.h>
#include "msm_bus_core.h"
#include "msm_bus_bimc.h"
#include "msm_bus_adhoc.h"
#include <trace/events/trace_msm_bus.h>

enum bke_sw {
	BKE_OFF = 0,
	BKE_ON = 1,
};

/* M_Generic */

#define M_REG_BASE(b)		((b) + 0x00008000)

#define M_CLK_CTRL_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000200)
enum bimc_m_clk_ctrl {
	M_CLK_CTRL_RMSK				= 0x3,
	M_CLK_CTRL_MAS_CLK_GATING_EN_BMSK	= 0x2,
	M_CLK_CTRL_MAS_CLK_GATING_EN_SHFT	= 0x1,
	M_CLK_CTRL_CORE_CLK_GATING_EN_BMSK	= 0x1,
	M_CLK_CTRL_CORE_CLK_GATING_EN_SHFT	= 0x0,
};

#define M_PRIOLVL_OVERRIDE_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000230)
enum bimc_m_priolvl_override {
	M_PRIOLVL_OVERRIDE_RMSK			= 0x301,
	M_PRIOLVL_OVERRIDE_BMSK			= 0x300,
	M_PRIOLVL_OVERRIDE_SHFT			= 0x8,
	M_PRIOLVL_OVERRIDE_OVERRIDE_PRIOLVL_BMSK	= 0x1,
	M_PRIOLVL_OVERRIDE_OVERRIDE_PRIOLVL_SHFT	= 0x0,
};

#define M_RD_CMD_OVERRIDE_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000240)
enum bimc_m_read_command_override {
	M_RD_CMD_OVERRIDE_RMSK			= 0x3071f7f,
	M_RD_CMD_OVERRIDE_AREQPRIO_BMSK		= 0x3000000,
	M_RD_CMD_OVERRIDE_AREQPRIO_SHFT		= 0x18,
	M_RD_CMD_OVERRIDE_AMEMTYPE_BMSK		= 0x70000,
	M_RD_CMD_OVERRIDE_AMEMTYPE_SHFT		= 0x10,
	M_RD_CMD_OVERRIDE_ATRANSIENT_BMSK		= 0x1000,
	M_RD_CMD_OVERRIDE_ATRANSIENT_SHFT		= 0xc,
	M_RD_CMD_OVERRIDE_ASHARED_BMSK		= 0x800,
	M_RD_CMD_OVERRIDE_ASHARED_SHFT		= 0xb,
	M_RD_CMD_OVERRIDE_AREDIRECT_BMSK		= 0x400,
	M_RD_CMD_OVERRIDE_AREDIRECT_SHFT		= 0xa,
	M_RD_CMD_OVERRIDE_AOOO_BMSK			= 0x200,
	M_RD_CMD_OVERRIDE_AOOO_SHFT			= 0x9,
	M_RD_CMD_OVERRIDE_AINNERSHARED_BMSK		= 0x100,
	M_RD_CMD_OVERRIDE_AINNERSHARED_SHFT		= 0x8,
	M_RD_CMD_OVERRIDE_OVERRIDE_AREQPRIO_BMSK	= 0x40,
	M_RD_CMD_OVERRIDE_OVERRIDE_AREQPRIO_SHFT	= 0x6,
	M_RD_CMD_OVERRIDE_OVERRIDE_ATRANSIENT_BMSK	= 0x20,
	M_RD_CMD_OVERRIDE_OVERRIDE_ATRANSIENT_SHFT	= 0x5,
	M_RD_CMD_OVERRIDE_OVERRIDE_AMEMTYPE_BMSK	= 0x10,
	M_RD_CMD_OVERRIDE_OVERRIDE_AMEMTYPE_SHFT	= 0x4,
	M_RD_CMD_OVERRIDE_OVERRIDE_ASHARED_BMSK	= 0x8,
	M_RD_CMD_OVERRIDE_OVERRIDE_ASHARED_SHFT	= 0x3,
	M_RD_CMD_OVERRIDE_OVERRIDE_AREDIRECT_BMSK	= 0x4,
	M_RD_CMD_OVERRIDE_OVERRIDE_AREDIRECT_SHFT	= 0x2,
	M_RD_CMD_OVERRIDE_OVERRIDE_AOOO_BMSK		= 0x2,
	M_RD_CMD_OVERRIDE_OVERRIDE_AOOO_SHFT		= 0x1,
	M_RD_CMD_OVERRIDE_OVERRIDE_AINNERSHARED_BMSK	= 0x1,
	M_RD_CMD_OVERRIDE_OVERRIDE_AINNERSHARED_SHFT	= 0x0,
};

#define M_WR_CMD_OVERRIDE_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000250)
enum bimc_m_write_command_override {
	M_WR_CMD_OVERRIDE_RMSK			= 0x3071f7f,
	M_WR_CMD_OVERRIDE_AREQPRIO_BMSK		= 0x3000000,
	M_WR_CMD_OVERRIDE_AREQPRIO_SHFT		= 0x18,
	M_WR_CMD_OVERRIDE_AMEMTYPE_BMSK		= 0x70000,
	M_WR_CMD_OVERRIDE_AMEMTYPE_SHFT		= 0x10,
	M_WR_CMD_OVERRIDE_ATRANSIENT_BMSK	= 0x1000,
	M_WR_CMD_OVERRIDE_ATRANSIENT_SHFT	= 0xc,
	M_WR_CMD_OVERRIDE_ASHARED_BMSK		= 0x800,
	M_WR_CMD_OVERRIDE_ASHARED_SHFT		= 0xb,
	M_WR_CMD_OVERRIDE_AREDIRECT_BMSK		= 0x400,
	M_WR_CMD_OVERRIDE_AREDIRECT_SHFT		= 0xa,
	M_WR_CMD_OVERRIDE_AOOO_BMSK			= 0x200,
	M_WR_CMD_OVERRIDE_AOOO_SHFT			= 0x9,
	M_WR_CMD_OVERRIDE_AINNERSHARED_BMSK		= 0x100,
	M_WR_CMD_OVERRIDE_AINNERSHARED_SHFT		= 0x8,
	M_WR_CMD_OVERRIDE_OVERRIDE_AREQPRIO_BMSK	= 0x40,
	M_WR_CMD_OVERRIDE_OVERRIDE_AREQPRIO_SHFT	= 0x6,
	M_WR_CMD_OVERRIDE_OVERRIDE_ATRANSIENT_BMSK	= 0x20,
	M_WR_CMD_OVERRIDE_OVERRIDE_ATRANSIENT_SHFT	= 0x5,
	M_WR_CMD_OVERRIDE_OVERRIDE_AMEMTYPE_BMSK	= 0x10,
	M_WR_CMD_OVERRIDE_OVERRIDE_AMEMTYPE_SHFT	= 0x4,
	M_WR_CMD_OVERRIDE_OVERRIDE_ASHARED_BMSK	= 0x8,
	M_WR_CMD_OVERRIDE_OVERRIDE_ASHARED_SHFT	= 0x3,
	M_WR_CMD_OVERRIDE_OVERRIDE_AREDIRECT_BMSK	= 0x4,
	M_WR_CMD_OVERRIDE_OVERRIDE_AREDIRECT_SHFT	= 0x2,
	M_WR_CMD_OVERRIDE_OVERRIDE_AOOO_BMSK	= 0x2,
	M_WR_CMD_OVERRIDE_OVERRIDE_AOOO_SHFT	= 0x1,
	M_WR_CMD_OVERRIDE_OVERRIDE_AINNERSHARED_BMSK	= 0x1,
	M_WR_CMD_OVERRIDE_OVERRIDE_AINNERSHARED_SHFT	= 0x0,
};

#define M_BKE_EN_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000300)
enum bimc_m_bke_en {
	M_BKE_EN_RMSK			= 0x1,
	M_BKE_EN_EN_BMSK		= 0x1,
	M_BKE_EN_EN_SHFT		= 0x0,
};

/* Grant Period registers */
#define M_BKE_GP_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000304)
enum bimc_m_bke_grant_period {
	M_BKE_GP_RMSK		= 0x3ff,
	M_BKE_GP_GP_BMSK	= 0x3ff,
	M_BKE_GP_GP_SHFT	= 0x0,
};

/* Grant count register.
 * The Grant count register represents a signed 16 bit
 * value, range 0-0x7fff
 */
#define M_BKE_GC_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000308)
enum bimc_m_bke_grant_count {
	M_BKE_GC_RMSK			= 0xffff,
	M_BKE_GC_GC_BMSK		= 0xffff,
	M_BKE_GC_GC_SHFT		= 0x0,
};

/* Threshold High Registers */
#define M_BKE_THH_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000320)
enum bimc_m_bke_thresh_high {
	M_BKE_THH_RMSK		= 0xffff,
	M_BKE_THH_THRESH_BMSK	= 0xffff,
	M_BKE_THH_THRESH_SHFT	= 0x0,
};

/* Threshold Medium Registers */
#define M_BKE_THM_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000324)
enum bimc_m_bke_thresh_medium {
	M_BKE_THM_RMSK		= 0xffff,
	M_BKE_THM_THRESH_BMSK	= 0xffff,
	M_BKE_THM_THRESH_SHFT	= 0x0,
};

/* Threshold Low Registers */
#define M_BKE_THL_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000328)
enum bimc_m_bke_thresh_low {
	M_BKE_THL_RMSK			= 0xffff,
	M_BKE_THL_THRESH_BMSK		= 0xffff,
	M_BKE_THL_THRESH_SHFT		= 0x0,
};

#define M_BKE_HEALTH_0_CONFIG_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000340)
enum bimc_m_bke_health_0 {
	M_BKE_HEALTH_0_CONFIG_RMSK			= 0x80000303,
	M_BKE_HEALTH_0_CONFIG_LIMIT_CMDS_BMSK		= 0x80000000,
	M_BKE_HEALTH_0_CONFIG_LIMIT_CMDS_SHFT		= 0x1f,
	M_BKE_HEALTH_0_CONFIG_AREQPRIO_BMSK		= 0x300,
	M_BKE_HEALTH_0_CONFIG_AREQPRIO_SHFT		= 0x8,
	M_BKE_HEALTH_0_CONFIG_PRIOLVL_BMSK		= 0x3,
	M_BKE_HEALTH_0_CONFIG_PRIOLVL_SHFT		= 0x0,
};

#define M_BKE_HEALTH_1_CONFIG_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000344)
enum bimc_m_bke_health_1 {
	M_BKE_HEALTH_1_CONFIG_RMSK			= 0x80000303,
	M_BKE_HEALTH_1_CONFIG_LIMIT_CMDS_BMSK		= 0x80000000,
	M_BKE_HEALTH_1_CONFIG_LIMIT_CMDS_SHFT		= 0x1f,
	M_BKE_HEALTH_1_CONFIG_AREQPRIO_BMSK		= 0x300,
	M_BKE_HEALTH_1_CONFIG_AREQPRIO_SHFT		= 0x8,
	M_BKE_HEALTH_1_CONFIG_PRIOLVL_BMSK		= 0x3,
	M_BKE_HEALTH_1_CONFIG_PRIOLVL_SHFT		= 0x0,
};

#define M_BKE_HEALTH_2_CONFIG_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000348)
enum bimc_m_bke_health_2 {
	M_BKE_HEALTH_2_CONFIG_RMSK			= 0x80000303,
	M_BKE_HEALTH_2_CONFIG_LIMIT_CMDS_BMSK		= 0x80000000,
	M_BKE_HEALTH_2_CONFIG_LIMIT_CMDS_SHFT		= 0x1f,
	M_BKE_HEALTH_2_CONFIG_AREQPRIO_BMSK		= 0x300,
	M_BKE_HEALTH_2_CONFIG_AREQPRIO_SHFT		= 0x8,
	M_BKE_HEALTH_2_CONFIG_PRIOLVL_BMSK		= 0x3,
	M_BKE_HEALTH_2_CONFIG_PRIOLVL_SHFT		= 0x0,
};

#define M_BKE_HEALTH_3_CONFIG_ADDR(b, n) \
	(M_REG_BASE(b) + (0x4000 * (n)) + 0x0000034c)
enum bimc_m_bke_health_3 {
	M_BKE_HEALTH_3_CONFIG_RMSK			= 0x303,
	M_BKE_HEALTH_3_CONFIG_AREQPRIO_BMSK	= 0x300,
	M_BKE_HEALTH_3_CONFIG_AREQPRIO_SHFT	= 0x8,
	M_BKE_HEALTH_3_CONFIG_PRIOLVL_BMSK		= 0x3,
	M_BKE_HEALTH_3_CONFIG_PRIOLVL_SHFT		= 0x0,
};

/* S_ARB_GENERIC */

#define S_ARB_REG_BASE(b)	((b) + 0x00049000)

#define S_ARB_CONFIG_INFO_0_ADDR(b, n) \
	(S_ARB_REG_BASE(b) + (0x8000 * (n)) + 0x00000020)
#define S_ARB_MODE_ADDR(b, n) \
	(S_ARB_REG_BASE(b) + (0x8000 * (n)) + 0x00000210)
enum bimc_s_arb_mode {
	S_ARB_MODE_RMSK				= 0xf0000001,
	S_ARB_MODE_WR_GRANTS_AHEAD_BMSK		= 0xf0000000,
	S_ARB_MODE_WR_GRANTS_AHEAD_SHFT		= 0x1c,
	S_ARB_MODE_PRIO_RR_EN_BMSK		= 0x1,
	S_ARB_MODE_PRIO_RR_EN_SHFT		= 0x0,
};

#define BKE_HEALTH_MASK \
	(M_BKE_HEALTH_0_CONFIG_LIMIT_CMDS_BMSK |\
	M_BKE_HEALTH_0_CONFIG_AREQPRIO_BMSK |\
	M_BKE_HEALTH_0_CONFIG_PRIOLVL_BMSK)

#define BKE_HEALTH_VAL(limit, areq, plvl) \
	((((limit) << M_BKE_HEALTH_0_CONFIG_LIMIT_CMDS_SHFT) & \
	M_BKE_HEALTH_0_CONFIG_LIMIT_CMDS_BMSK) | \
	(((areq) << M_BKE_HEALTH_0_CONFIG_AREQPRIO_SHFT) & \
	M_BKE_HEALTH_0_CONFIG_AREQPRIO_BMSK) | \
	(((plvl) << M_BKE_HEALTH_0_CONFIG_PRIOLVL_SHFT) & \
	M_BKE_HEALTH_0_CONFIG_PRIOLVL_BMSK))

#define MAX_GRANT_PERIOD \
	(M_BKE_GP_GP_BMSK >> \
	M_BKE_GP_GP_SHFT)

#define MAX_GC \
	(M_BKE_GC_GC_BMSK >> \
	(M_BKE_GC_GC_SHFT + 1))

static int bimc_div(int64_t *a, uint32_t b)
{
	if ((*a > 0) && (*a < b)) {
		*a = 0;
		return 1;
	} else {
		return do_div(*a, b);
	}
}

#define ENABLE(val) ((val) == 1 ? 1 : 0)
void msm_bus_bimc_set_mas_clk_gate(struct msm_bus_bimc_info *binfo,
	uint32_t mas_index, struct msm_bus_bimc_clk_gate *bgate)
{
	uint32_t val, mask, reg_val;
	void __iomem *addr;

	reg_val = readl_relaxed(M_CLK_CTRL_ADDR(binfo->base,
			mas_index)) & M_CLK_CTRL_RMSK;
	addr = M_CLK_CTRL_ADDR(binfo->base, mas_index);
	mask = (M_CLK_CTRL_MAS_CLK_GATING_EN_BMSK |
		M_CLK_CTRL_CORE_CLK_GATING_EN_BMSK);
	val = (bgate->core_clk_gate_en <<
		M_CLK_CTRL_MAS_CLK_GATING_EN_SHFT) |
		bgate->port_clk_gate_en;
	writel_relaxed(((reg_val & (~mask)) | (val & mask)), addr);
	/* Ensure clock gating enable mask is set before exiting */
	wmb();
}

void msm_bus_bimc_arb_en(struct msm_bus_bimc_info *binfo,
	uint32_t slv_index, bool en)
{
	uint32_t reg_val, reg_mask_val, enable, val;

	reg_mask_val = (readl_relaxed(S_ARB_CONFIG_INFO_0_ADDR(binfo->
		base, slv_index)) & S_ARB_CONFIG_INFO_0_FUNC_BMSK)
		>> S_ARB_CONFIG_INFO_0_FUNC_SHFT;
	enable = ENABLE(en);
	val = enable << S_ARB_MODE_PRIO_RR_EN_SHFT;
	if (reg_mask_val == BIMC_ARB_MODE_PRIORITY_RR) {
		reg_val = readl_relaxed(S_ARB_CONFIG_INFO_0_ADDR(binfo->
			base, slv_index)) & S_ARB_MODE_RMSK;
		writel_relaxed(((reg_val & (~(S_ARB_MODE_PRIO_RR_EN_BMSK))) |
			(val & S_ARB_MODE_PRIO_RR_EN_BMSK)),
			S_ARB_MODE_ADDR(binfo->base, slv_index));
		/* Ensure arbitration mode is set before returning */
		wmb();
	}
}

static void set_qos_mode(void __iomem *baddr, uint32_t index, uint32_t val0,
	uint32_t val1, uint32_t val2)
{
	uint32_t reg_val, val;

	reg_val = readl_relaxed(M_PRIOLVL_OVERRIDE_ADDR(baddr,
		index)) & M_PRIOLVL_OVERRIDE_RMSK;
	val = val0 << M_PRIOLVL_OVERRIDE_OVERRIDE_PRIOLVL_SHFT;
	writel_relaxed(((reg_val & ~(M_PRIOLVL_OVERRIDE_OVERRIDE_PRIOLVL_BMSK))
		| (val & M_PRIOLVL_OVERRIDE_OVERRIDE_PRIOLVL_BMSK)),
		M_PRIOLVL_OVERRIDE_ADDR(baddr, index));
	reg_val = readl_relaxed(M_RD_CMD_OVERRIDE_ADDR(baddr, index)) &
		M_RD_CMD_OVERRIDE_RMSK;
	val = val1 << M_RD_CMD_OVERRIDE_OVERRIDE_AREQPRIO_SHFT;
	writel_relaxed(((reg_val & ~(M_RD_CMD_OVERRIDE_OVERRIDE_AREQPRIO_BMSK
		)) | (val & M_RD_CMD_OVERRIDE_OVERRIDE_AREQPRIO_BMSK)),
		M_RD_CMD_OVERRIDE_ADDR(baddr, index));
	reg_val = readl_relaxed(M_WR_CMD_OVERRIDE_ADDR(baddr, index)) &
		M_WR_CMD_OVERRIDE_RMSK;
	val = val2 << M_WR_CMD_OVERRIDE_OVERRIDE_AREQPRIO_SHFT;
	writel_relaxed(((reg_val & ~(M_WR_CMD_OVERRIDE_OVERRIDE_AREQPRIO_BMSK
		)) | (val & M_WR_CMD_OVERRIDE_OVERRIDE_AREQPRIO_BMSK)),
		M_WR_CMD_OVERRIDE_ADDR(baddr, index));
	/* Ensure the priority register writes go through */
	wmb();
}

static void msm_bus_bimc_set_qos_mode(void __iomem *base,
	uint32_t mas_index, uint8_t qmode_sel)
{
	uint32_t reg_val, val;

	switch (qmode_sel) {
	case BIMC_QOS_MODE_FIXED:
		reg_val = readl_relaxed(M_BKE_EN_ADDR(base,
			mas_index));
		writel_relaxed((reg_val & (~M_BKE_EN_EN_BMSK)),
			M_BKE_EN_ADDR(base, mas_index));
		/* Ensure that the book-keeping register writes
		 * go through before setting QoS mode.
		 * QoS mode registers might write beyond 1K
		 * boundary in future
		 */
		wmb();
		set_qos_mode(base, mas_index, 1, 1, 1);
		break;

	case BIMC_QOS_MODE_BYPASS:
		reg_val = readl_relaxed(M_BKE_EN_ADDR(base,
			mas_index));
		writel_relaxed((reg_val & (~M_BKE_EN_EN_BMSK)),
			M_BKE_EN_ADDR(base, mas_index));
		/* Ensure that the book-keeping register writes
		 * go through before setting QoS mode.
		 * QoS mode registers might write beyond 1K
		 * boundary in future
		 */
		wmb();
		set_qos_mode(base, mas_index, 0, 0, 0);
		break;

	case BIMC_QOS_MODE_REGULATOR:
	case BIMC_QOS_MODE_LIMITER:
		set_qos_mode(base, mas_index, 0, 0, 0);
		reg_val = readl_relaxed(M_BKE_EN_ADDR(base,
			mas_index));
		val = 1 << M_BKE_EN_EN_SHFT;
		/* Ensure that the book-keeping register writes
		 * go through before setting QoS mode.
		 * QoS mode registers might write beyond 1K
		 * boundary in future
		 */
		wmb();
		writel_relaxed(((reg_val & (~M_BKE_EN_EN_BMSK)) | (val &
			M_BKE_EN_EN_BMSK)), M_BKE_EN_ADDR(base,
			mas_index));
		break;
	default:
		break;
	}
}

static void set_qos_prio_rl(void __iomem *addr, uint32_t rmsk,
	uint8_t index, struct msm_bus_bimc_qos_mode *qmode)
{
	uint32_t reg_val, val0, val;

	/* Note, addr is already passed with right mas_index */
	reg_val = readl_relaxed(addr) & rmsk;
	val0 = BKE_HEALTH_VAL(qmode->rl.qhealth[index].limit_commands,
		qmode->rl.qhealth[index].areq_prio,
		qmode->rl.qhealth[index].prio_level);
	val = ((reg_val & (~(BKE_HEALTH_MASK))) | (val0 & BKE_HEALTH_MASK));
	writel_relaxed(val, addr);
	/* Ensure that priority for regulator/limiter modes are
	 * set before returning
	 */
	wmb();

}

static void msm_bus_bimc_set_qos_prio(void __iomem *base,
	uint32_t mas_index, uint8_t qmode_sel,
	struct msm_bus_bimc_qos_mode *qmode)
{
	uint32_t reg_val, val;

	switch (qmode_sel) {
	case BIMC_QOS_MODE_FIXED:
		reg_val = readl_relaxed(M_PRIOLVL_OVERRIDE_ADDR(
			base, mas_index)) & M_PRIOLVL_OVERRIDE_RMSK;
		val =  qmode->fixed.prio_level <<
			M_PRIOLVL_OVERRIDE_SHFT;
		writel_relaxed(((reg_val &
			~(M_PRIOLVL_OVERRIDE_BMSK)) | (val
			& M_PRIOLVL_OVERRIDE_BMSK)),
			M_PRIOLVL_OVERRIDE_ADDR(base, mas_index));

		reg_val = readl_relaxed(M_RD_CMD_OVERRIDE_ADDR(
			base, mas_index)) & M_RD_CMD_OVERRIDE_RMSK;
		val =  qmode->fixed.areq_prio_rd <<
			M_RD_CMD_OVERRIDE_AREQPRIO_SHFT;
		writel_relaxed(((reg_val & ~(M_RD_CMD_OVERRIDE_AREQPRIO_BMSK))
			| (val & M_RD_CMD_OVERRIDE_AREQPRIO_BMSK)),
			M_RD_CMD_OVERRIDE_ADDR(base, mas_index));

		reg_val = readl_relaxed(M_WR_CMD_OVERRIDE_ADDR(
			base, mas_index)) & M_WR_CMD_OVERRIDE_RMSK;
		val =  qmode->fixed.areq_prio_wr <<
			M_WR_CMD_OVERRIDE_AREQPRIO_SHFT;
		writel_relaxed(((reg_val & ~(M_WR_CMD_OVERRIDE_AREQPRIO_BMSK))
			| (val & M_WR_CMD_OVERRIDE_AREQPRIO_BMSK)),
			M_WR_CMD_OVERRIDE_ADDR(base, mas_index));
		/* Ensure that fixed mode register writes go through
		 * before returning
		 */
		wmb();
		break;

	case BIMC_QOS_MODE_REGULATOR:
	case BIMC_QOS_MODE_LIMITER:
		set_qos_prio_rl(M_BKE_HEALTH_3_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_3_CONFIG_RMSK, 3, qmode);
		set_qos_prio_rl(M_BKE_HEALTH_2_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_2_CONFIG_RMSK, 2, qmode);
		set_qos_prio_rl(M_BKE_HEALTH_1_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_1_CONFIG_RMSK, 1, qmode);
		set_qos_prio_rl(M_BKE_HEALTH_0_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_0_CONFIG_RMSK, 0 , qmode);
		break;
	case BIMC_QOS_MODE_BYPASS:
	default:
		break;
	}
}

static void set_qos_bw_regs(void __iomem *baddr, uint32_t mas_index,
	int32_t th, int32_t tm, int32_t tl, uint32_t gp,
	uint32_t gc)
{
	int32_t reg_val, val;
	int32_t bke_reg_val;
	int16_t val2;

	/* Disable BKE before writing to registers as per spec */
	bke_reg_val = readl_relaxed(M_BKE_EN_ADDR(baddr, mas_index));
	writel_relaxed((bke_reg_val & ~(M_BKE_EN_EN_BMSK)),
		M_BKE_EN_ADDR(baddr, mas_index));

	/* Write values of registers calculated */
	reg_val = readl_relaxed(M_BKE_GP_ADDR(baddr, mas_index))
		& M_BKE_GP_RMSK;
	val =  gp << M_BKE_GP_GP_SHFT;
	writel_relaxed(((reg_val & ~(M_BKE_GP_GP_BMSK)) | (val &
		M_BKE_GP_GP_BMSK)), M_BKE_GP_ADDR(baddr, mas_index));

	reg_val = readl_relaxed(M_BKE_GC_ADDR(baddr, mas_index)) &
		M_BKE_GC_RMSK;
	val =  gc << M_BKE_GC_GC_SHFT;
	writel_relaxed(((reg_val & ~(M_BKE_GC_GC_BMSK)) | (val &
		M_BKE_GC_GC_BMSK)), M_BKE_GC_ADDR(baddr, mas_index));

	reg_val = readl_relaxed(M_BKE_THH_ADDR(baddr, mas_index)) &
		M_BKE_THH_RMSK;
	val =  th << M_BKE_THH_THRESH_SHFT;
	writel_relaxed(((reg_val & ~(M_BKE_THH_THRESH_BMSK)) | (val &
		M_BKE_THH_THRESH_BMSK)), M_BKE_THH_ADDR(baddr, mas_index));

	reg_val = readl_relaxed(M_BKE_THM_ADDR(baddr, mas_index)) &
		M_BKE_THM_RMSK;
	val2 =	tm << M_BKE_THM_THRESH_SHFT;
	writel_relaxed(((reg_val & ~(M_BKE_THM_THRESH_BMSK)) | (val2 &
		M_BKE_THM_THRESH_BMSK)), M_BKE_THM_ADDR(baddr, mas_index));

	reg_val = readl_relaxed(M_BKE_THL_ADDR(baddr, mas_index)) &
		M_BKE_THL_RMSK;
	val2 =	tl << M_BKE_THL_THRESH_SHFT;
	writel_relaxed(((reg_val & ~(M_BKE_THL_THRESH_BMSK)) |
		(val2 & M_BKE_THL_THRESH_BMSK)), M_BKE_THL_ADDR(baddr,
		mas_index));

	/* Ensure that all bandwidth register writes have completed
	 * before returning
	 */
	wmb();
}

static void msm_bus_bimc_set_qos_bw(void __iomem *base, uint32_t qos_freq,
	uint32_t mas_index, struct msm_bus_bimc_qos_bw *qbw)
{
	uint32_t bke_en;

	/* Validate QOS Frequency */
	if (qos_freq == 0) {
		MSM_BUS_DBG("Zero frequency\n");
		return;
	}

	/* Get enable bit for BKE before programming the period */
	bke_en = (readl_relaxed(M_BKE_EN_ADDR(base, mas_index)) &
		M_BKE_EN_EN_BMSK) >> M_BKE_EN_EN_SHFT;

	/* Only calculate if there's a requested bandwidth and window */
	if (qbw->bw && qbw->ws) {
		int64_t th, tm, tl;
		uint32_t gp, gc;
		int64_t gp_nominal, gp_required, gp_calc, data, temp;
		int64_t win = qbw->ws * qos_freq;
		temp = win;
		/*
		 * Calculate nominal grant period defined by requested
		 * window size.
		 * Ceil this value to max grant period.
		 */
		bimc_div(&temp, 1000000);
		gp_nominal = min_t(uint64_t, MAX_GRANT_PERIOD, temp);
		/*
		 * Calculate max window size, defined by bw request.
		 * Units: (KHz, MB/s)
		 */
		gp_calc = MAX_GC * qos_freq * 1000;
		gp_required = gp_calc;
		bimc_div(&gp_required, qbw->bw);

		/* User min of two grant periods */
		gp = min_t(int64_t, gp_nominal, gp_required);

		/* Calculate bandwith in grants and ceil. */
		temp = qbw->bw * gp;
		data = qos_freq * 1000;
		bimc_div(&temp, data);
		gc = min_t(int64_t, MAX_GC, temp);

		/* Calculate thresholds */
		th = qbw->bw - qbw->thh;
		tm = qbw->bw - qbw->thm;
		tl = qbw->bw - qbw->thl;

		th = th * gp;
		bimc_div(&th, data);
		tm = tm * gp;
		bimc_div(&tm, data);
		tl = tl * gp;
		bimc_div(&tl, data);

		MSM_BUS_DBG("BIMC: BW: mas_index: %d, th: %llu tm: %llu\n",
			mas_index, th, tm);
		MSM_BUS_DBG("BIMC: tl: %llu gp:%u gc: %u bke_en: %u\n",
			tl, gp, gc, bke_en);
		set_qos_bw_regs(base, mas_index, th, tm, tl, gp, gc);
	} else
		/* Clear bandwidth registers */
		set_qos_bw_regs(base, mas_index, 0, 0, 0, 0, 0);
}

static int msm_bus_bimc_allocate_commit_data(struct msm_bus_fabric_registration
	*fab_pdata, void **cdata, int ctx)
{
	struct msm_bus_bimc_commit **cd = (struct msm_bus_bimc_commit **)cdata;
	struct msm_bus_bimc_info *binfo =
		(struct msm_bus_bimc_info *)fab_pdata->hw_data;

	MSM_BUS_DBG("Allocating BIMC commit data\n");
	*cd = kzalloc(sizeof(struct msm_bus_bimc_commit), GFP_KERNEL);
	if (!*cd) {
		MSM_BUS_DBG("Couldn't alloc mem for cdata\n");
		return -ENOMEM;
	}

	(*cd)->mas = binfo->cdata[ctx].mas;
	(*cd)->slv = binfo->cdata[ctx].slv;

	return 0;
}

static void *msm_bus_bimc_allocate_bimc_data(struct platform_device *pdev,
	struct msm_bus_fabric_registration *fab_pdata)
{
	struct resource *bimc_mem;
	struct resource *bimc_io;
	struct msm_bus_bimc_info *binfo;
	int i;

	MSM_BUS_DBG("Allocating BIMC data\n");
	binfo = kzalloc(sizeof(struct msm_bus_bimc_info), GFP_KERNEL);
	if (!binfo) {
		WARN(!binfo, "Couldn't alloc mem for bimc_info\n");
		return NULL;
	}

	binfo->qos_freq = fab_pdata->qos_freq;

	binfo->params.nmasters = fab_pdata->nmasters;
	binfo->params.nslaves = fab_pdata->nslaves;
	binfo->params.bus_id = fab_pdata->id;

	for (i = 0; i < NUM_CTX; i++) {
		binfo->cdata[i].mas = kzalloc(sizeof(struct
			msm_bus_node_hw_info) * fab_pdata->nmasters * 2,
			GFP_KERNEL);
		if (!binfo->cdata[i].mas) {
			MSM_BUS_ERR("Couldn't alloc mem for bimc master hw\n");
			kfree(binfo);
			return NULL;
		}

		binfo->cdata[i].slv = kzalloc(sizeof(struct
			msm_bus_node_hw_info) * fab_pdata->nslaves * 2,
			GFP_KERNEL);
		if (!binfo->cdata[i].slv) {
			MSM_BUS_DBG("Couldn't alloc mem for bimc slave hw\n");
			kfree(binfo->cdata[i].mas);
			kfree(binfo);
			return NULL;
		}
	}

	if (fab_pdata->virt) {
		MSM_BUS_DBG("Don't get memory regions for virtual fabric\n");
		goto skip_mem;
	}

	bimc_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!bimc_mem) {
		MSM_BUS_ERR("Cannot get BIMC Base address\n");
		kfree(binfo);
		return NULL;
	}

	bimc_io = request_mem_region(bimc_mem->start,
			resource_size(bimc_mem), pdev->name);
	if (!bimc_io) {
		MSM_BUS_ERR("BIMC memory unavailable\n");
		kfree(binfo);
		return NULL;
	}

	binfo->base = ioremap(bimc_mem->start, resource_size(bimc_mem));
	if (!binfo->base) {
		MSM_BUS_ERR("IOremap failed for BIMC!\n");
		release_mem_region(bimc_mem->start, resource_size(bimc_mem));
		kfree(binfo);
		return NULL;
	}

skip_mem:
	fab_pdata->hw_data = (void *)binfo;
	return (void *)binfo;
}

static void free_commit_data(void *cdata)
{
	struct msm_bus_bimc_commit *cd = (struct msm_bus_bimc_commit *)cdata;

	kfree(cd->mas);
	kfree(cd->slv);
	kfree(cd);
}

static void bke_switch(
	void __iomem *baddr, uint32_t mas_index, bool req, int mode)
{
	uint32_t reg_val, val, cur_val;

	val = req << M_BKE_EN_EN_SHFT;
	reg_val = readl_relaxed(M_BKE_EN_ADDR(baddr, mas_index));
	cur_val = reg_val & M_BKE_EN_RMSK;
	if (val == cur_val)
		return;

	if (!req && mode == BIMC_QOS_MODE_FIXED)
		set_qos_mode(baddr, mas_index, 1, 1, 1);

	writel_relaxed(((reg_val & ~(M_BKE_EN_EN_BMSK)) | (val &
		M_BKE_EN_EN_BMSK)), M_BKE_EN_ADDR(baddr, mas_index));
	/* Make sure BKE on/off goes through before changing priorities */
	wmb();

	if (req)
		set_qos_mode(baddr, mas_index, 0, 0, 0);
}

static void bimc_set_static_qos_bw(void __iomem *base, unsigned int qos_freq,
	int mport, struct msm_bus_bimc_qos_bw *qbw)
{
	int32_t bw_mbps, thh = 0, thm, thl, gc;
	int32_t gp;
	u64 temp;

	if (qos_freq == 0) {
		MSM_BUS_DBG("No QoS Frequency.\n");
		return;
	}

	if (!(qbw->bw && qbw->gp)) {
		MSM_BUS_DBG("No QoS Bandwidth or Window size\n");
		return;
	}

	/* Convert bandwidth to MBPS */
	temp = qbw->bw;
	bimc_div(&temp, 1000000);
	bw_mbps = temp;

	/* Grant period in clock cycles
	 * Grant period from bandwidth structure
	 * is in nano seconds, QoS freq is in KHz.
	 * Divide by 1000 to get clock cycles.
	 */
	gp = (qos_freq * qbw->gp) / (1000 * NSEC_PER_USEC);

	/* Grant count = BW in MBps * Grant period
	 * in micro seconds
	 */
	gc = bw_mbps * (qbw->gp / NSEC_PER_USEC);
	gc = min(gc, MAX_GC);

	/* Medium threshold = -((Medium Threshold percentage *
	 * Grant count) / 100)
	 */
	thm = -((qbw->thmp * gc) / 100);
	qbw->thm = thm;

	/* Low threshold = -(Grant count) */
	thl = -gc;
	qbw->thl = thl;

	MSM_BUS_DBG("%s: BKE parameters: gp %d, gc %d, thm %d thl %d thh %d",
			__func__, gp, gc, thm, thl, thh);

	trace_bus_bke_params(gc, gp, thl, thm, thl);
	set_qos_bw_regs(base, mport, thh, thm, thl, gp, gc);
}

static void msm_bus_bimc_config_master(
	struct msm_bus_fabric_registration *fab_pdata,
	struct msm_bus_inode_info *info,
	uint64_t req_clk, uint64_t req_bw)
{
	int mode, i, ports;
	struct msm_bus_bimc_info *binfo;
	uint64_t bw = 0;

	binfo = (struct msm_bus_bimc_info *)fab_pdata->hw_data;
	ports = info->node_info->num_mports;

	/**
	 * Here check the details of dual configuration.
	 * Take actions based on different modes.
	 * Check for threshold if limiter mode, etc.
	*/

	if (req_clk <= info->node_info->th[0]) {
		mode = info->node_info->mode;
		bw = info->node_info->bimc_bw[0];
	} else if ((info->node_info->num_thresh > 1) &&
			(req_clk <= info->node_info->th[1])) {
		mode = info->node_info->mode;
		bw = info->node_info->bimc_bw[1];
	} else
		mode = info->node_info->mode_thresh;

	switch (mode) {
	case BIMC_QOS_MODE_BYPASS:
	case BIMC_QOS_MODE_FIXED:
		for (i = 0; i < ports; i++)
			bke_switch(binfo->base, info->node_info->qport[i],
				BKE_OFF, mode);
		break;
	case BIMC_QOS_MODE_REGULATOR:
	case BIMC_QOS_MODE_LIMITER:
		for (i = 0; i < ports; i++) {
			/* If not in fixed mode, update bandwidth */
			if ((info->node_info->cur_lim_bw != bw)
					&& (mode != BIMC_QOS_MODE_FIXED)) {
				struct msm_bus_bimc_qos_bw qbw;
				qbw.ws = info->node_info->ws;
				qbw.bw = bw;
				qbw.gp = info->node_info->bimc_gp;
				qbw.thmp = info->node_info->bimc_thmp;
				bimc_set_static_qos_bw(binfo->base,
					binfo->qos_freq,
					info->node_info->qport[i], &qbw);
				info->node_info->cur_lim_bw = bw;
				MSM_BUS_DBG("%s: Qos is %d reqclk %llu bw %llu",
						__func__, mode, req_clk, bw);
			}
			bke_switch(binfo->base, info->node_info->qport[i],
				BKE_ON, mode);
		}
		break;
	default:
		break;
	}
}

static void msm_bus_bimc_update_bw(struct msm_bus_inode_info *hop,
	struct msm_bus_inode_info *info,
	struct msm_bus_fabric_registration *fab_pdata,
	void *sel_cdata, int *master_tiers,
	int64_t add_bw)
{
	struct msm_bus_bimc_info *binfo;
	struct msm_bus_bimc_qos_bw qbw;
	int i;
	int64_t bw;
	int ports = info->node_info->num_mports;
	struct msm_bus_bimc_commit *sel_cd =
		(struct msm_bus_bimc_commit *)sel_cdata;

	MSM_BUS_DBG("BIMC: Update bw for ID %d, with IID: %d: %lld\n",
		info->node_info->id, info->node_info->priv_id, add_bw);

	binfo = (struct msm_bus_bimc_info *)fab_pdata->hw_data;

	if (info->node_info->num_mports == 0) {
		MSM_BUS_DBG("BIMC: Skip Master BW\n");
		goto skip_mas_bw;
	}

	ports = info->node_info->num_mports;
	bw = INTERLEAVED_BW(fab_pdata, add_bw, ports);

	for (i = 0; i < ports; i++) {
		sel_cd->mas[info->node_info->masterp[i]].bw += bw;
		sel_cd->mas[info->node_info->masterp[i]].hw_id =
			info->node_info->mas_hw_id;
		MSM_BUS_DBG("BIMC: Update mas_bw for ID: %d -> %llu\n",
			info->node_info->priv_id,
			sel_cd->mas[info->node_info->masterp[i]].bw);
		if (info->node_info->hw_sel == MSM_BUS_RPM)
			sel_cd->mas[info->node_info->masterp[i]].dirty = 1;
		else {
			if (!info->node_info->qport) {
				MSM_BUS_DBG("No qos ports to update!\n");
				break;
			}
			if (!(info->node_info->mode == BIMC_QOS_MODE_REGULATOR)
					|| (info->node_info->mode ==
						BIMC_QOS_MODE_LIMITER)) {
				MSM_BUS_DBG("Skip QoS reg programming\n");
				break;
			}

			MSM_BUS_DBG("qport: %d\n", info->node_info->qport[i]);
			qbw.bw = sel_cd->mas[info->node_info->masterp[i]].bw;
			qbw.ws = info->node_info->ws;
			/* Threshold low = 90% of bw */
			qbw.thl = div_s64((90 * bw), 100);
			/* Threshold medium = bw */
			qbw.thm = bw;
			/* Threshold high = 10% more than bw */
			qbw.thh = div_s64((110 * bw), 100);
			/* Check if info is a shared master.
			 * If it is, mark it dirty
			 * If it isn't, then set QOS Bandwidth.
			 * Also if dual-conf is set, don't program bw regs.
			 **/
			if (!info->node_info->dual_conf &&
			((info->node_info->mode == BIMC_QOS_MODE_LIMITER) ||
			(info->node_info->mode == BIMC_QOS_MODE_REGULATOR)))
				msm_bus_bimc_set_qos_bw(binfo->base,
					binfo->qos_freq,
					info->node_info->qport[i], &qbw);
		}
	}

skip_mas_bw:
	ports = hop->node_info->num_sports;
	MSM_BUS_DBG("BIMC: ID: %d, Sports: %d\n", hop->node_info->priv_id,
		ports);

	for (i = 0; i < ports; i++) {
		sel_cd->slv[hop->node_info->slavep[i]].bw += add_bw;
		sel_cd->slv[hop->node_info->slavep[i]].hw_id =
			hop->node_info->slv_hw_id;
		MSM_BUS_DBG("BIMC: Update slave_bw: ID: %d -> %llu\n",
			hop->node_info->priv_id,
			sel_cd->slv[hop->node_info->slavep[i]].bw);
		MSM_BUS_DBG("BIMC: Update slave_bw: index: %d\n",
			hop->node_info->slavep[i]);
		/* Check if hop is a shared slave.
		 * If it is, mark it dirty
		 * If it isn't, then nothing to be done as the
		 * slaves are in bypass mode.
		 **/
		if (hop->node_info->hw_sel == MSM_BUS_RPM) {
			MSM_BUS_DBG("Slave dirty: %d, slavep: %d\n",
				hop->node_info->priv_id,
				hop->node_info->slavep[i]);
			sel_cd->slv[hop->node_info->slavep[i]].dirty = 1;
		}
	}
}

static int msm_bus_bimc_commit(struct msm_bus_fabric_registration
	*fab_pdata, void *hw_data, void **cdata)
{
	MSM_BUS_DBG("\nReached BIMC Commit\n");
	msm_bus_remote_hw_commit(fab_pdata, hw_data, cdata);
	return 0;
}

static void msm_bus_bimc_config_limiter(
	struct msm_bus_fabric_registration *fab_pdata,
	struct msm_bus_inode_info *info)
{
	struct msm_bus_bimc_info *binfo;
	int mode, i, ports;

	binfo = (struct msm_bus_bimc_info *)fab_pdata->hw_data;
	ports = info->node_info->num_mports;

	if (!info->node_info->qport) {
		MSM_BUS_DBG("No QoS Ports to init\n");
		return;
	}

	if (info->cur_lim_bw)
		mode = BIMC_QOS_MODE_LIMITER;
	else
		mode = info->node_info->mode;

	switch (mode) {
	case BIMC_QOS_MODE_BYPASS:
	case BIMC_QOS_MODE_FIXED:
		for (i = 0; i < ports; i++)
			bke_switch(binfo->base, info->node_info->qport[i],
				BKE_OFF, mode);
		break;
	case BIMC_QOS_MODE_REGULATOR:
	case BIMC_QOS_MODE_LIMITER:
		if (info->cur_lim_bw != info->cur_prg_bw) {
			MSM_BUS_DBG("Enabled BKE throttling node %d to %llu\n",
				info->node_info->id, info->cur_lim_bw);
			trace_bus_bimc_config_limiter(info->node_info->id,
				info->cur_lim_bw);
			for (i = 0; i < ports; i++) {
				/* If not in fixed mode, update bandwidth */
				struct msm_bus_bimc_qos_bw qbw;

				qbw.ws = info->node_info->ws;
				qbw.bw = info->cur_lim_bw;
				qbw.gp = info->node_info->bimc_gp;
				qbw.thmp = info->node_info->bimc_thmp;
				bimc_set_static_qos_bw(binfo->base,
					binfo->qos_freq,
					info->node_info->qport[i], &qbw);
				bke_switch(binfo->base,
					info->node_info->qport[i],
					BKE_ON, mode);
				info->cur_prg_bw = qbw.bw;
			}
		}
		break;
	default:
		break;
	}
}

static void bimc_init_mas_reg(struct msm_bus_bimc_info *binfo,
	struct msm_bus_inode_info *info,
	struct msm_bus_bimc_qos_mode *qmode, int mode)
{
	int i;

	switch (mode) {
	case BIMC_QOS_MODE_FIXED:
		qmode->fixed.prio_level = info->node_info->prio_lvl;
		qmode->fixed.areq_prio_rd = info->node_info->prio_rd;
		qmode->fixed.areq_prio_wr = info->node_info->prio_wr;
		break;
	case BIMC_QOS_MODE_LIMITER:
		qmode->rl.qhealth[0].limit_commands = 1;
		qmode->rl.qhealth[1].limit_commands = 0;
		qmode->rl.qhealth[2].limit_commands = 0;
		qmode->rl.qhealth[3].limit_commands = 0;
		break;
	default:
		break;
	}

	if (!info->node_info->qport) {
		MSM_BUS_DBG("No QoS Ports to init\n");
		return;
	}

	for (i = 0; i < info->node_info->num_mports; i++) {
		/* If not in bypass mode, update priority */
		if (mode != BIMC_QOS_MODE_BYPASS) {
			msm_bus_bimc_set_qos_prio(binfo->base,
				info->node_info->
				qport[i], mode, qmode);

			/* If not in fixed mode, update bandwidth */
			if (mode != BIMC_QOS_MODE_FIXED) {
				struct msm_bus_bimc_qos_bw qbw;
				qbw.ws = info->node_info->ws;
				qbw.bw = info->node_info->bimc_bw[0];
				qbw.gp = info->node_info->bimc_gp;
				qbw.thmp = info->node_info->bimc_thmp;
				bimc_set_static_qos_bw(binfo->base,
					binfo->qos_freq,
					info->node_info->qport[i], &qbw);
			}
		}

		/* set mode */
		msm_bus_bimc_set_qos_mode(binfo->base,
					info->node_info->qport[i],
					mode);
	}
}

static void init_health_regs(struct msm_bus_bimc_info *binfo,
				struct msm_bus_inode_info *info,
				struct msm_bus_bimc_qos_mode *qmode,
				int mode)
{
	int i;

	if (mode == BIMC_QOS_MODE_LIMITER) {
		qmode->rl.qhealth[0].limit_commands = 1;
		qmode->rl.qhealth[1].limit_commands = 0;
		qmode->rl.qhealth[2].limit_commands = 0;
		qmode->rl.qhealth[3].limit_commands = 0;

		if (!info->node_info->qport) {
			MSM_BUS_DBG("No QoS Ports to init\n");
			return;
		}

		for (i = 0; i < info->node_info->num_mports; i++) {
			/* If not in bypass mode, update priority */
			if (mode != BIMC_QOS_MODE_BYPASS)
				msm_bus_bimc_set_qos_prio(binfo->base,
				info->node_info->qport[i], mode, qmode);
		}
	}
}


static int msm_bus_bimc_mas_init(struct msm_bus_bimc_info *binfo,
	struct msm_bus_inode_info *info)
{
	struct msm_bus_bimc_qos_mode *qmode;
	qmode = kzalloc(sizeof(struct msm_bus_bimc_qos_mode),
		GFP_KERNEL);
	if (!qmode) {
		MSM_BUS_WARN("Couldn't alloc prio data for node: %d\n",
			info->node_info->id);
		return -ENOMEM;
	}

	info->hw_data = (void *)qmode;

	/**
	 * If the master supports dual configuration,
	 * configure registers for both modes
	 */
	if (info->node_info->dual_conf)
		bimc_init_mas_reg(binfo, info, qmode,
			info->node_info->mode_thresh);
	else if (info->node_info->nr_lim)
		init_health_regs(binfo, info, qmode, BIMC_QOS_MODE_LIMITER);

	bimc_init_mas_reg(binfo, info, qmode, info->node_info->mode);
	return 0;
}

static void msm_bus_bimc_node_init(void *hw_data,
	struct msm_bus_inode_info *info)
{
	struct msm_bus_bimc_info *binfo =
		(struct msm_bus_bimc_info *)hw_data;

	if (!IS_SLAVE(info->node_info->priv_id) &&
		(info->node_info->hw_sel != MSM_BUS_RPM))
		msm_bus_bimc_mas_init(binfo, info);
}

static int msm_bus_bimc_port_halt(uint32_t haltid, uint8_t mport)
{
	return 0;
}

static int msm_bus_bimc_port_unhalt(uint32_t haltid, uint8_t mport)
{
	return 0;
}

static bool msm_bus_bimc_update_bw_reg(int mode)
{
	bool ret = false;

	if ((mode == BIMC_QOS_MODE_LIMITER)
		|| (mode == BIMC_QOS_MODE_REGULATOR))
		ret = true;

	return ret;
}

int msm_bus_bimc_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo)
{
	/* Set interleaving to true by default */
	MSM_BUS_DBG("\nInitializing BIMC...\n");
	pdata->il_flag = true;
	hw_algo->allocate_commit_data = msm_bus_bimc_allocate_commit_data;
	hw_algo->allocate_hw_data = msm_bus_bimc_allocate_bimc_data;
	hw_algo->node_init = msm_bus_bimc_node_init;
	hw_algo->free_commit_data = free_commit_data;
	hw_algo->update_bw = msm_bus_bimc_update_bw;
	hw_algo->commit = msm_bus_bimc_commit;
	hw_algo->port_halt = msm_bus_bimc_port_halt;
	hw_algo->port_unhalt = msm_bus_bimc_port_unhalt;
	hw_algo->config_master = msm_bus_bimc_config_master;
	hw_algo->config_limiter = msm_bus_bimc_config_limiter;
	hw_algo->update_bw_reg = msm_bus_bimc_update_bw_reg;
	/* BIMC slaves are shared. Slave registers are set through RPM */
	if (!pdata->ahb)
		pdata->rpm_enabled = 1;
	return 0;
}

