/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_REG_H__
#define __MTK_CM_MGR_PLATFORM_REG_H__

/*
 * BIT Operation
 */
#define CM_BIT(_bit_) \
	((unsigned int)(1U << (_bit_)))
#define CM_BITMASK(_bits_) \
	(((unsigned int) -1 >> (31 - ((1) ? _bits_))) & \
	 ~((1U << ((0) ? _bits_)) - 1))
#define CM_BITS(_bits_, _val_) \
	(CM_BITMASK(_bits_) & \
	 ((_val_) << ((0) ? _bits_)))
#define CM_GET_BITS_VAL_0(_bits_, _val_) \
	(((_val_) & (CM_BITMASK(_bits_))) >> ((0) ? _bits_))
#define CM_NAME_VAL(_name_, _val_) \
	(_val_ << _name_##_SHIFT)
#define CM_GET_NAME_VAL(_reg_val_, _name_) \
	((_reg_val_) & (_name_##_MASK))
#define CM_GET_NAME_VAL_0(_reg_val_, _name_) \
	(((_reg_val_) & (_name_##_MASK)) >> _name_##_SHIFT)
#define CM_SET_NAME_VAL(_reg_val_, _name_, _val_) \
	(((_reg_val_) & ~(_name_##_MASK)) | \
	 ((_val_ << _name_##_SHIFT) & (_name_##_MASK)))

#undef CM_MGR_BASE
#define CM_MGR_BASE cm_mgr_base


#define STALL_INFO_CONF(x) (CM_MGR_BASE + 0x800 * (x) + 0x238)

#define CPU_AVG_STALL_RATIO(x) (CM_MGR_BASE + 0x800 * (x) + 0x700)
#define CPU_AVG_STALL_DEBUG_CTRL(x) (CM_MGR_BASE + 0x800 * (x) + 0x704)
#define CPU_AVG_STALL_FMETER_STATUS(x) (CM_MGR_BASE + 0x800 * (x) + 0x708)
#define CPU_AVG_STALL_RATIO_CTRL(x) (CM_MGR_BASE + 0x800 * (x) + 0x710)
#define CPU_AVG_STALL_COUNTER(x) (CM_MGR_BASE + 0x800 * (x) + 0x714)

#define MP0_CPU_NONWFX_CTRL(x) (CM_MGR_BASE + 0x8 * (x) + 0x8500)
#define MP0_CPU_NONWFX_CNT(x) (CM_MGR_BASE + 0x8 * (x) + 0x8504)

/* STALL_INFO_CONF */
#define CPU_STALL_IDLE_CNT_LSB CM_BIT(0)
#define CPU_STALL_IDLE_CNT_SHIFT 0
#define CPU_STALL_IDLE_CNT_MASK CM_BITMASK(5:0)
#define CPU_STALL_CACHE_OPT_LSB CM_BIT(6)
#define CPU_STALL_CACHE_OPT_SHIFT 6
#define CPU_STALL_CACHE_OPT_MASK CM_BITMASK(10:6)
/* CPU_AVG_STALL_RATIO */
#define RG_CPU_AVG_STALL_RATIO CM_BIT(0)
/* CPU_AVG_STALL_DEBUG_CTRL */
#define RG_FMETER_APB_FREQUENCY CM_BIT(0)
#define RG_STALL_INFO_SEL CM_BIT(12)
#define RG_SAMPLE_RATIO CM_BIT(16)
/* CPU_AVG_STALL_FMETER_STATUS */
#define RO_FMETER_OUTPUT CM_BIT(0)
#define RO_FMETER_LOW CM_BIT(12)
/* CPU_AVG_STALL_RATIO_CTRL */
#define RG_CPU_AVG_STALL_RATIO_EN CM_BIT(0)
#define RG_CPU_AVG_STALL_RATIO_RESTART CM_BIT(1)
#define RG_CPU_STALL_COUNTER_EN CM_BIT(4)
#define REMOVED_RG_CPU_NON_WFX_COUNTER_EN CM_BIT(5)
#define RG_CPU_STALL_COUNTER_RESET CM_BIT(6)
#define REMOVED_RG_CPU_NON_WFX_COUNTER_RESET CM_BIT(7)
#define RG_MP0_AVG_STALL_PERIOD CM_BIT(8)
#define RG_FMETER_MIN_FREQUENCY CM_BIT(12)
#define RG_FMETER_EN CM_BIT(24)
/* CPU_AVG_STALL_COUNTER */
#define RO_CPU_STALL_COUNTER CM_BIT(0)
/* MP0_CPU0_NONWFX_CTRL */
#define RG_CPU0_NON_WFX_COUNTER_EN CM_BIT(0)
#define RG_CPU0_NON_WFX_COUNTER_RESET CM_BIT(4)

#define RG_MP0_AVG_STALL_PERIOD_1MS CM_BITS(11:8, 0x8)
#define RG_MP0_AVG_STALL_PERIOD_2MS CM_BITS(11:8, 0x9)
#define RG_MP0_AVG_STALL_PERIOD_4MS CM_BITS(11:8, 0xa)
#define RG_MP0_AVG_STALL_PERIOD_8MS CM_BITS(11:8, 0xb)
#define RG_MP0_AVG_STALL_PERIOD_10MS CM_BITS(11:8, 0xc)
#define RG_MP0_AVG_STALL_PERIOD_20MS CM_BITS(11:8, 0xd)
#define RG_MP0_AVG_STALL_PERIOD_40MS CM_BITS(11:8, 0xe)
#define RG_MP0_AVG_STALL_PERIOD_80MS CM_BITS(11:8, 0xf)

#define cm_mgr_read(addr)	__raw_readl((void __force __iomem *)(addr))
#define cm_mgr_write(addr, val)	mt_reg_sync_writel(val, addr)

#endif /* __MTK_CM_MGR_PLATFORM_REG_H__*/
