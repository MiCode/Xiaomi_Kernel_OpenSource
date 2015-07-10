/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

/* M_Generic */

enum bke_sw {
	BKE_OFF = 0,
	BKE_ON = 1,
};

#define M_REG_BASE(b)		((b) + 0x00008000)

#define M_MODE_ADDR(b, n) \
		(M_REG_BASE(b) + (0x4000 * (n)) + 0x00000210)
enum bimc_m_mode {
	M_MODE_RMSK				= 0xf0000011,
	M_MODE_WR_GATHER_BEATS_BMSK		= 0xf0000000,
	M_MODE_WR_GATHER_BEATS_SHFT		= 0x1c,
	M_MODE_NARROW_WR_BMSK			= 0x10,
	M_MODE_NARROW_WR_SHFT			= 0x4,
	M_MODE_ORDERING_MODEL_BMSK		= 0x1,
	M_MODE_ORDERING_MODEL_SHFT		= 0x0,
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

#define NUM_HEALTH_LEVEL	(4)
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

static void set_bke_en(void __iomem *addr, uint32_t index,
		bool req)
{
	uint32_t old_val, new_val;

	old_val = readl_relaxed(M_BKE_EN_ADDR(addr, index));
	new_val = req << M_BKE_EN_EN_SHFT;
	if ((old_val & M_BKE_EN_RMSK) == (new_val))
		return;
	writel_relaxed(((old_val & ~(M_BKE_EN_EN_BMSK)) | (new_val &
				M_BKE_EN_EN_BMSK)), M_BKE_EN_ADDR(addr, index));
	/* Ensure that BKE register is programmed set before returning */
	wmb();
}

static void set_health_reg(void __iomem *addr, uint32_t rmsk,
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
	/*
	 * Ensure that priority for regulator/limiter modes are
	 * set before returning
	 */
	wmb();
}

static void msm_bus_bimc_set_qos_prio(void __iomem *base,
	uint32_t mas_index, uint8_t qmode_sel,
	struct msm_bus_bimc_qos_mode *qmode)
{

	switch (qmode_sel) {
	case BIMC_QOS_MODE_FIXED:
	case BIMC_QOS_MODE_REGULATOR:
	case BIMC_QOS_MODE_LIMITER:
		set_health_reg(M_BKE_HEALTH_3_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_3_CONFIG_RMSK, 3, qmode);
		set_health_reg(M_BKE_HEALTH_2_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_2_CONFIG_RMSK, 2, qmode);
		set_health_reg(M_BKE_HEALTH_1_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_1_CONFIG_RMSK, 1, qmode);
		set_health_reg(M_BKE_HEALTH_0_CONFIG_ADDR(base,
			mas_index), M_BKE_HEALTH_0_CONFIG_RMSK, 0 , qmode);
		set_bke_en(base, mas_index, true);
		break;
	case BIMC_QOS_MODE_BYPASS:
		set_bke_en(base, mas_index, false);
		break;
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

static int msm_bus_bimc_limit_mport(struct msm_bus_node_device_type *info,
				void __iomem *qos_base, uint32_t qos_off,
				uint32_t qos_delta, uint32_t qos_freq,
				int enable_lim, u64 lim_bw)
{
	int mode;
	int i;
	struct msm_bus_bimc_qos_mode qmode = {0};

	if (ZERO_OR_NULL_PTR(info->node_info->qport)) {
		MSM_BUS_DBG("No QoS Ports to limit\n");
		return 0;
	}

	if ((enable_lim == THROTTLE_ON) && lim_bw) {
		mode =  BIMC_QOS_MODE_LIMITER;

		qmode.rl.qhealth[0].limit_commands = 1;
		qmode.rl.qhealth[1].limit_commands = 0;
		qmode.rl.qhealth[2].limit_commands = 0;
		qmode.rl.qhealth[3].limit_commands = 0;
		for (i = 0; i < NUM_HEALTH_LEVEL; i++) {
			qmode.rl.qhealth[i].prio_level =
					info->node_info->qos_params.prio_lvl;
			qmode.rl.qhealth[i].areq_prio =
					info->node_info->qos_params.prio_rd;
		}

		for (i = 0; i < info->node_info->num_qports; i++) {
			struct msm_bus_bimc_qos_bw qbw;
			/* If not in fixed mode, update bandwidth */
			if (info->node_info->lim_bw != lim_bw) {
				qbw.ws = info->node_info->qos_params.ws;
				qbw.bw = lim_bw;
				qbw.gp = info->node_info->qos_params.gp;
				qbw.thmp = info->node_info->qos_params.thmp;
				bimc_set_static_qos_bw(qos_base, qos_freq,
					info->node_info->qport[i], &qbw);
			}
		}
		info->node_info->lim_bw = lim_bw;
	} else {
		mode = info->node_info->qos_params.mode;
		if (mode != BIMC_QOS_MODE_BYPASS) {
			for (i = 0; i < NUM_HEALTH_LEVEL; i++) {
				qmode.rl.qhealth[i].prio_level =
					info->node_info->qos_params.prio_lvl;
				qmode.rl.qhealth[i].areq_prio =
					info->node_info->qos_params.prio_rd;
			}
		}
	}

	for (i = 0; i < info->node_info->num_qports; i++)
		msm_bus_bimc_set_qos_prio(qos_base, info->node_info->qport[i],
				mode, &qmode);
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

static int msm_bus_bimc_qos_init(struct msm_bus_node_device_type *info,
				void __iomem *qos_base,
				uint32_t qos_off, uint32_t qos_delta,
				uint32_t qos_freq)
{
	int i;
	struct msm_bus_bimc_qos_mode qmode = {0};

	if (ZERO_OR_NULL_PTR(info->node_info->qport)) {
		MSM_BUS_DBG("No QoS Ports to init\n");
		return 0;
	}

	switch (info->node_info->qos_params.mode) {
		/* For now Fixed and regulator are handled the same way. */
	case BIMC_QOS_MODE_FIXED:
	case BIMC_QOS_MODE_REGULATOR:
		for (i = 0; i < NUM_HEALTH_LEVEL; i++) {
			qmode.rl.qhealth[i].prio_level =
				info->node_info->qos_params.prio_lvl;
			qmode.rl.qhealth[i].areq_prio =
				info->node_info->qos_params.prio_rd;
		}
		break;
	case BIMC_QOS_MODE_LIMITER:
		qmode.rl.qhealth[0].limit_commands = 1;
		qmode.rl.qhealth[1].limit_commands = 0;
		qmode.rl.qhealth[2].limit_commands = 0;
		qmode.rl.qhealth[3].limit_commands = 0;
		for (i = 0; i < NUM_HEALTH_LEVEL; i++) {
			qmode.rl.qhealth[i].prio_level =
				info->node_info->qos_params.prio_lvl;
			qmode.rl.qhealth[i].areq_prio =
				info->node_info->qos_params.prio_rd;
		}
		break;
	default:
		break;
	}


	for (i = 0; i < info->node_info->num_qports; i++)
		msm_bus_bimc_set_qos_prio(qos_base, info->node_info->qport[i],
			info->node_info->qos_params.mode, &qmode);

	return 0;
}

static int msm_bus_bimc_set_bw(struct msm_bus_node_device_type *dev,
				void __iomem *qos_base, uint32_t qos_off,
				uint32_t qos_delta, uint32_t qos_freq)
{
	struct msm_bus_bimc_qos_bw qbw;
	struct msm_bus_bimc_qos_mode qmode = {0};
	int i;
	int64_t bw = 0;
	int ret = 0;
	struct msm_bus_node_info_type *info = dev->node_info;
	int mode;

	if (info && info->num_qports &&
		((info->qos_params.mode == BIMC_QOS_MODE_LIMITER))) {
		bw = msm_bus_div64(info->num_qports,
				dev->node_bw[ACTIVE_CTX].sum_ab);

		MSM_BUS_DBG("BIMC: Update mas_bw for ID: %d -> %llu\n",
				info->id, bw);

		if (!info->qport) {
			MSM_BUS_DBG("No qos ports to update!\n");
			goto exit_set_bw;
		}

		qbw.bw = bw + info->qos_params.bw_buffer;
		trace_bus_bimc_config_limiter(info->id, bw);

		/* Default to gp of 5us */
		qbw.gp = (info->qos_params.gp ?
				info->qos_params.gp : 5000);
		/* Default to thmp of 50% */
		qbw.thmp = (info->qos_params.thmp ?
				info->qos_params.thmp : 50);
		/*
		 * If the BW vote is 0 then set the QoS mode to
		 * Fixed/0/0.
		 */
		if (bw) {
			qmode.rl.qhealth[0].limit_commands = 1;
			qmode.rl.qhealth[1].limit_commands = 0;
			qmode.rl.qhealth[2].limit_commands = 0;
			qmode.rl.qhealth[3].limit_commands = 0;
			mode = info->qos_params.mode;
		} else {
			mode =	BIMC_QOS_MODE_FIXED;
		}

		for (i = 0; i < info->num_qports; i++) {
			msm_bus_bimc_set_qos_prio(qos_base,
				info->qport[i], mode, &qmode);
			if (bw)
				bimc_set_static_qos_bw(qos_base, qos_freq,
					info->qport[i], &qbw);
		}
	}
exit_set_bw:
	return ret;
}

int msm_bus_bimc_set_ops(struct msm_bus_node_device_type *bus_dev)
{
	if (!bus_dev)
		return -ENODEV;
	bus_dev->fabdev->noc_ops.qos_init = msm_bus_bimc_qos_init;
	bus_dev->fabdev->noc_ops.set_bw = msm_bus_bimc_set_bw;
	bus_dev->fabdev->noc_ops.limit_mport = msm_bus_bimc_limit_mport;
	bus_dev->fabdev->noc_ops.update_bw_reg =
					msm_bus_bimc_update_bw_reg;
	return 0;
}
EXPORT_SYMBOL(msm_bus_bimc_set_ops);
