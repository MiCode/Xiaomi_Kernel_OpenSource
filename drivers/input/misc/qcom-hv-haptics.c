// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
//#define DEBUG
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/qpnp/qpnp-pbs.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/soc/qcom/qcom_hv_haptics.h>

#define CREATE_TRACE_POINTS
#include <trace/events/qcom_haptics.h>

/* status register definitions in HAPTICS_CFG module */
#define HAP_CFG_REVISION1_REG			0x00
#define HAP_CFG_REVISION2_REG			0x01
#define HAP_CFG_V1				0x1
#define HAP_CFG_V2				0x2
#define HAP_CFG_V3				0x3
#define HAP_CFG_V4				0x4
#define HAP_CFG_V5				0x5
#define MAJOR_REV(rev)				((rev) >> 8)

#define HAP_CFG_REV_LSB_MASK			GENMASK(7, 0)
#define HAP_CFG_STATUS_DATA_MSB_REG		0x09
/* STATUS_DATA_MSB definitions while MOD_STATUS_SEL is 0 */
#define AUTO_RES_CAL_DONE_BIT			BIT(5)
#define CAL_TLRA_CL_STS_MSB_MASK		GENMASK(4, 0)
/* STATUS_DATA_MSB definition while MOD_STATUS_SEL is 3 */
#define LAST_GOOD_TLRA_CL_MSB_MASK		GENMASK(4, 0)
/* STATUS_DATA_MSB definition while MOD_STATUS_SEL is 4 */
#define TLRA_CL_ERR_MSB_MASK			GENMASK(4, 0)
/* STATUS_DATA_MSB when MOD_STATUS_SEL is 5 and MOD_STATUS_XT.SEL is 1 */
#define FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V2	GENMASK(1, 0)
#define FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V3	GENMASK(2, 0)
#define FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V4	GENMASK(3, 0)
#define FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V5	GENMASK(4, 0)
#define FIFO_EMPTY_FLAG_BIT			BIT(6)
#define FIFO_FULL_FLAG_BIT			BIT(5)

#define HAP_CFG_STATUS_DATA_LSB_REG		0x0A
/* STATUS_DATA_LSB definition while MOD_STATUS_SEL is 0 */
#define CAL_TLRA_CL_STS_LSB_MASK		GENMASK(7, 0)
/* STATUS_DATA_LSB when MOD_STATUS_SEL is 5 and MOD_STATUS_XT.SEL is 1 */
#define FIFO_REAL_TIME_FILL_STATUS_LSB_MASK	GENMASK(7, 0)
/* STATUS_DATA_LSB when MOD_STATUS_SEL is 5 and MOD_STATUS_XT.SEL is 0 */
#define HAP_DRV_PATTERN_SRC_STATUS_MASK		GENMASK(2, 0)

#define HAP_CFG_FAULT_STATUS_REG		0x0C
#define SC_FLAG_BIT				BIT(2)
#define AUTO_RES_ERROR_BIT			BIT(1)
#define HPRW_RDY_FAULT_BIT			BIT(0)

/* For HAP525_HV and later */
#define HAP_CFG_HPWR_INTF_REG                   0x0B
#define HPWR_INTF_STATUS_MASK                   GENMASK(1, 0)
#define HPWR_DISABLED                           0
#define HPWR_READY                              3

#define HAP_CFG_REAL_TIME_LRA_IMPEDANCE_REG	0x0E
#define LRA_IMPEDANCE_MOHMS_LSB			250
#define HAP530_LRA_IMPEDANCE_MOHMS_LSB		200

#define HAP_CFG_INT_RT_STS_REG			0x10
#define FIFO_EMPTY_BIT				BIT(1)

/* config register definitions in HAPTICS_CFG module */
#define HAP_CFG_EN_CTL_REG			0x46
#define HAPTICS_EN_BIT				BIT(7)

#define HAP_CFG_DRV_CTRL_REG			0x47
#define PSTG_DLY_MASK				GENMASK(7, 6)
#define DRV_SLEW_RATE_MASK			GENMASK(2, 0)

#define HAP_CFG_VMAX_REG			0x48
#define VMAX_HV_STEP_MV				50
#define VMAX_MV_STEP_MV				32
#define MAX_VMAX_MV				11000
#define MAX_HV_VMAX_MV				10000
#define MAX_MV_VMAX_MV				6000
#define CLAMPED_VMAX_MV				5000
#define DEFAULT_VMAX_MV				5000

#define HAP_CFG_DRV_WF_SEL_REG			0x49
#define DRV_WF_FMT_BIT				BIT(4)
#define DRV_WF_SEL_MASK				GENMASK(1, 0)

#define HAP_CFG_AUTO_SHUTDOWN_CFG_REG		0x4A
#define EN_WFHOLD_FIFO_BIT			BIT(2) /* For haptics HAP525_HV and later */

#define HAP_CFG_TRIG_PRIORITY_REG		0x4B
#define SWR_IGNORE_BIT				BIT(4)

#define HAP_CFG_SPMI_PLAY_REG			0x4C
#define PLAY_EN_BIT				BIT(7)
#define PATX_MEM_SEL_MASK			GENMASK(5, 4) /* This is only for HAP525_HV */
#define BRAKE_EN_BIT				BIT(3)
#define PAT_SRC_MASK				GENMASK(2, 0)

#define HAP_CFG_EXT_TRIG_REG			0x4D

#define HAP_CFG_SWR_ACCESS_REG			0x4E
#define SWR_PAT_CFG_EN_BIT			BIT(7)
#define SWR_PAT_INPUT_EN_BIT			BIT(6)
#define SWR_PAT_RES_N_BIT			BIT(5)

#define HAP_CFG_BRAKE_MODE_CFG_REG		0x50
#define BRAKE_MODE_MASK				GENMASK(7, 6)
#define BRAKE_MODE_SHIFT			6
#define BRAKE_SINE_GAIN_MASK			GENMASK(3, 2)
#define BRAKE_SINE_GAIN_SHIFT			2
#define BRAKE_WF_SEL_MASK			GENMASK(1, 0)

#define HAP_CFG_CL_BRAKE_CFG_REG		0x51
#define HAP_CFG_CL_BRAKE_CAL_PARAM_REG		0x52
#define HAP_CFG_CL_BRAKE_RSET_REG		0x53
#define HAP_CFG_PWM_CFG_REG			0x5A

/* Only for HAP530_HV */

#define HAP_CFG_CL_BRAKE_RCAL_REG		0x52
#define CL_BRAKE_RCAL_MASK			GENMASK(5, 0)

/* For HAP_CFG_CL_BRAKE_RSET_REG */
#define FORCE_RNAT_RCAL_PARAM_MASK		BIT(0)

#define HAP_CFG_CL_BRAKE_RNAT_REG		0x54
#define CL_BRAKE_RNAT_MASK			GENMASK(5, 0)

/* End HAP530_HV */

#define HAP_CFG_TLRA_OL_HIGH_REG		0x5C
#define TLRA_OL_MSB_MASK			GENMASK(3, 0)
#define HAP_CFG_TLRA_OL_LOW_REG			0x5D
#define TLRA_OL_LSB_MASK			GENMASK(7, 0)
#define TLRA_STEP_US				5
#define TLRA_MAX_US				20475

#define HAP_CFG_RC_CLK_CAL_COUNT_MSB_REG	0x5E
#define RC_CLK_CAL_COUNT_MSB_MASK		GENMASK(1, 0)
#define HAP_CFG_RC_CLK_CAL_COUNT_LSB_REG	0x5F
#define RC_CLK_CAL_COUNT_LSB_MASK		GENMASK(7, 0)

#define HAP_CFG_DRV_DUTY_CFG_REG		0x60
#define ADT_DRV_DUTY_EN_BIT			BIT(7)
#define ADT_BRK_DUTY_EN_BIT			BIT(6)
#define DRV_DUTY_MASK				GENMASK(5, 3)
#define DRV_DUTY_62P5_PCT			2
#define DRV_DUTY_SHIFT				3
#define BRK_DUTY_MASK				GENMASK(2, 0)
#define BRK_DUTY_75_PCT			6

#define HAP_CFG_ADT_DRV_DUTY_CFG_REG		0x61
#define HAP_CFG_ZX_WIND_CFG_REG			0x62

#define HAP_CFG_AUTORES_CFG_REG			0x63
#define AUTORES_EN_BIT				BIT(7)
#define AUTORES_EN_DLY_MASK			GENMASK(5, 2)
#define AUTORES_EN_DLY(cycles)			((cycles) * 2)
#define AUTORES_EN_DLY_6_CYCLES			AUTORES_EN_DLY(6)
#define AUTORES_EN_DLY_7_CYCLES			AUTORES_EN_DLY(7)
#define AUTORES_EN_DLY_SHIFT			2
#define AUTORES_ERR_WINDOW_MASK			GENMASK(1, 0)
#define AUTORES_ERR_WINDOW_12P5_PERCENT		0x0
#define AUTORES_ERR_WINDOW_25_PERCENT		0x1
#define AUTORES_ERR_WINDOW_50_PERCENT		0x2
#define AUTORES_ERR_WINDOW_100_PERCENT		0x3

#define HAP_CFG_AUTORES_ERR_RECOVERY_REG	0x64
#define EN_HW_RECOVERY_BIT			BIT(1)
#define SW_ERR_DRV_FREQ_BIT			BIT(0)

#define HAP_CFG_ISC_CFG_REG			0x65
#define ILIM_CC_EN_BIT				BIT(7)
/* Following bits are only for HAP525_HV */
#define EN_SC_DET_P_HAP525_HV_BIT		BIT(6)
#define EN_SC_DET_N_HAP525_HV_BIT		BIT(5)
#define EN_IMP_DET_HAP525_HV_BIT		BIT(4)
#define ILIM_PULSE_DENSITY_MASK			GENMASK(3, 2)
#define ILIM_DENSITY_8_OVER_64_CYCLES		0

#define HAP_CFG_FAULT_CLR_REG			0x66
#define HAP_ZX_TO_FAULT_CLR_BIT			BIT(4) /* only for HAP530_HV */
#define SC_CLR_BIT				BIT(2)
#define AUTO_RES_ERR_CLR_BIT			BIT(1)
#define HPWR_RDY_FAULT_CLR_BIT			BIT(0)

#define HAP_CFG_VMAX_HDRM_REG			0x67
#define VMAX_HDRM_MASK				GENMASK(6, 0)
#define VMAX_HDRM_STEP_MV			50
#define VMAX_HDRM_MAX_MV			6350

#define HAP_CFG_VSET_CFG_REG			0x68
#define FORCE_VSET_ACK_BIT			BIT(1) /* This is only for HAP525_HV */
#define FORCE_VREG_RDY_BIT			BIT(0)

#define HAP_CFG_ZX_SYNC_CFG			0x6F /* only for HAP530_HV */
#define EN_PDN_ZX_TO_FAULT_BIT			BIT(2)

#define HAP_CFG_MOD_STATUS_SEL_REG		0x70
#define HAP_CFG_MOD_STATUS_XT_REG		0x71

#define HAP_CFG_CAL_EN_REG			0x72
#define CAL_RC_CLK_MASK				GENMASK(3, 2)
#define CAL_RC_CLK_SHIFT			2
#define CAL_RC_CLK_DISABLED_VAL			0
#define CAL_RC_CLK_AUTO_VAL			1
#define CAL_RC_CLK_MANUAL_VAL			2
#define BRAKE_CAL_MASK				GENMASK(1, 0)
#define BRAKE_CAL_NONE				0
#define BRAKE_CAL_CL_NATURAL			1
#define BRAKE_CAL_CL_FORCED			2
#define BRAKE_CAL_CL_RECAL			3

/* For HAP520_MV and HAP525_HV */
#define HAP_CFG_ISC_CFG2_REG			0x77
#define EN_SC_DET_P_HAP520_MV_BIT		BIT(6)
#define EN_SC_DET_N_HAP520_MV_BIT		BIT(5)
#define ISC_THRESH_HAP520_MV_MASK		GENMASK(2, 0)
#define ISC_THRESH_HAP520_MV_140MA		0x01
#define ISC_THRESH_HAP525_HV_MASK		GENMASK(4, 0)
#define ISC_THRESH_HAP525_HV_125MA		0x11
#define ISC_THRESH_HAP525_HV_250MA		0x12

/* These registers are only applicable for HAP520_MV */
#define HAP_CFG_HW_CONFIG_REG			0x0D
#define HV_HAP_DRIVER_BIT			BIT(1)

#define HAP_CFG_HPWR_INTF_CTL_REG		0x80
#define INTF_CTL_MASK				GENMASK(1, 0)
#define INTF_CTL_BOB				1
#define INTF_CTL_BHARGER			2

#define HAP_CFG_VHPWR_REG			0x84
#define VHPWR_STEP_MV				32

/* Only for HAP525_HV */
#define HAP_CFG_RT_LRA_IMPD_MEAS_CFG_REG	0xA4
#define LRA_IMPEDANCE_MEAS_EN_BIT		BIT(7)
#define LRA_IMPEDANCE_MEAS_CURRENT_SEL_BIT	BIT(0)
#define CURRENT_SEL_VAL_125MA			0
#define CURRENT_SEL_VAL_250MA			1

/* Only for HAP530 */
#define HAP_CFG_DIG_SPARE_REG			0xB8
#define EN_FE_PU_CHECK_BIT			BIT(3)

#define HAP_CFG_ICOMP_AVG_CTL1_REG		0xC0
#define ICOMP_THRESH_MASK			GENMASK(6, 0)
#define ICOMP_THRESH_STEP_NA			7812500

#define HAP_CFG_ICOMP_AVG_CTL2_REG		0xC1

#define HAP_CFG_RSENSE_MEAS_LSB_REG		0xC4
#define LRA_IMPEDANCE_VISENSE_MOHMS		25
#define HAP_CFG_RSENSE_MEAS_MSB_MASK		GENMASK(3, 0)

#define HAP_CFG_VISENSE_PEAK_DET_CTL_REG	0xC7
#define VISENSE_PEAK_DET_EN_BIT			BIT(7)
#define VISENSE_PEAK_DET_MODE			BIT(6)
#define VISENSE_INST_DET_EN_BIT			BIT(4)

#define HAP_CFG_VSENSE_PEAK_LSB_REG		0xC8
#define HAP_CFG_VSENSE_PEAK_MSB_MASK		GENMASK(5, 0)

#define HAP_CFG_ISENSE_PEAK_LSB_REG		0xCA
#define HAP_CFG_ISENSE_PEAK_MSB_MASK		GENMASK(4, 0)

/* version register definitions for HAPTICS_PATTERN module */
#define HAP_PTN_REVISION1_REG			0x00
#define HAP_PTN_REVISION2_REG			0x01
#define HAP_PTN_V1				0x1
#define HAP_PTN_V2				0x2
#define HAP_PTN_V3				0x3
#define HAP_PTN_V4				0x4
#define HAP_PTN_V5				0x5
#define HAP_PTN_PATX_PLAY_CFG_SUPPORT_MIN_REV	0x501

/* status register definition for HAPTICS_PATTERN module */
#define HAP_PTN_FIFO_READY_STS_REG		0x08
#define FIFO_READY_BIT				BIT(0)

#define HAP_PTN_NUM_PAT_REG			0x09

/* config register definition for HAPTICS_PATTERN module */
#define HAP_PTN_FIFO_DIN_0_REG			0x20
#define HAP_PTN_FIFO_DIN_NUM			4

#define HAP_PTN_FIFO_PLAY_RATE_REG		0x24
#define PAT_MEM_PLAY_RATE_MASK			GENMASK(7, 4)
#define FIFO_PLAY_RATE_MASK			GENMASK(3, 0)

#define HAP_PTN_AUTORES_CAL_CFG_REG		0x28
#define AUTORES_CAL_TRIG_BIT			BIT(7)
#define AUTORES_CAL_TIMER_4_HALF_CYCLES		4
#define AUTORES_CAL_TIMER_6_HALF_CYCLES		6

#define HAP_PTN_FIFO_EMPTY_CFG_REG		0x2A
#define HAP520_EMPTY_THRESH_MASK		GENMASK(3, 0)
#define HAP520_MV_EMPTY_THRESH_MASK		GENMASK(4, 0)
#define HAP525_EMPTY_THRESH_MASK		GENMASK(5, 0)
#define HAP530_EMPTY_THRESH_MASK		GENMASK(6, 0)
#define HAP_PTN_FIFO_THRESH_LSB			40

#define HAP_PTN_FIFO_DEPTH_CFG_REG		0x2B
#define HAP_PTN_FIFO_DIN_1B_REG			0x2C

#define HAP_PTN_DIRECT_PLAY_REG			0x26
#define DIRECT_PLAY_MAX_AMPLITUDE		0xFF

#define HAP_PTN_AUTORES_CAL_CFG_REG		0x28

#define HAP_PTN_PTRN1_TLRA_MSB_REG		0x30
#define HAP_PTN_PTRN1_TLRA_LSB_REG		0x31
#define HAP_PTN_PTRN2_TLRA_MSB_REG		0x32
#define HAP_PTN_PTRN2_TLRA_LSB_REG		0x33

#define HAP_PTN_PTRN1_CFG_REG			0x34
#define PTRN_FLRA2X_SHIFT			7
#define PTRN_SAMPLE_PER_MASK			GENMASK(2, 0)

#define PTRN_AMP_MSB_MASK			BIT(0)
#define PTRN_AMP_LSB_MASK			GENMASK(7, 0)

/* For HAP530_HV */
#define HAP_PTN_VISENSE_ADC_CFG			0x47
#define EN_VISENSE_PD_PROT_BIT			BIT(3)

#define HAP_PTN_CL_VDRIVE_CTL_REG		0x4A
#define EN_CL_VDRIVE_BIT			BIT(7)
/* End for HAP530_HV */

#define HAP_PTN_PTRN2_CFG_REG			0x50

#define HAP_PTN_BRAKE_AMP_REG			0x70

/* HAPTICS_PATTERN registers only present in HAP525_HV */
#define HAP_PTN_MEM_OP_ACCESS_REG		0x2D
#define MEM_PAT_ACCESS_BIT			BIT(7)
#define MEM_PAT_RW_SEL_MASK			GENMASK(5, 4)
#define MEM_FLUSH_RELOAD_BIT			BIT(0)

#define HAP_PTN_MMAP_FIFO_REG			0xA0
#define MMAP_FIFO_EXIST_BIT			BIT(7)
#define MMAP_FIFO_LEN_MASK			GENMASK(3, 0)
#define HAP530_MMAP_FIFO_LEN_MASK		GENMASK(6, 0)
#define MMAP_FIFO_LEN_PER_LSB			128
#define HAP530_MMAP_FIFO_LEN_PER_LSB		64

#define HAP_PTN_MMAP_PAT1_REG			0xA1
#define MMAP_PAT_LEN_PER_LSB			32
#define HAP530_MMAP_PAT_LEN_PER_LSB		4
#define MMAP_PAT1_LEN_MASK			GENMASK(6, 0)
#define MMAP_PAT2_LEN_MASK			GENMASK(5, 0)
#define MMAP_PAT3_PAT4_LEN_MASK			GENMASK(4, 0)

#define HAP_PTN_PATX_PLAY_CFG			0xA2
#define HAP530_V2_MMAP_PAT_LEN_PER_LSB		1

/* HAPTICS_PATTERN registers only present in HAP530_HV */
#define HAP_PTN_FORCE_TWIND_CAL_MSB		0x30
#define HAP_PTN_TWIND_CAL_MSB_MASK		GENMASK(3, 0)

#define HAP_PTN_FORCE_TWIND_CAL_LSB		0x31

#define HAP_PTN_VISENSE_EN_REG			0x46
#define HAP_PTN_VISENSE_EN_BIT			BIT(7)

#define HAP_PTN_PATX_MEM_WR_START_ADDR_REG	0x50
#define HAP_PTN_BRAKE_AMP_0_REG			0x70
#define HAP_PTN_SPMI_PATX_MEM_START_ADDR_REG	0xA4
#define HAP_PTN_SPMI_PATX_MEM_LEN_LSB_REG	0xA6
#define HAP_PTN_PATX_MEM_LEN_HI_MASK		GENMASK(2, 0)
#define HAP530_V2_PTN_PATX_MEM_LEN_HI_MASK	GENMASK(4, 0)
#define HAP_PTN_PATX_MEM_LEN_LO_MASK		GENMASK(7, 0)
#define HAP_PTN_PATX_MEM_LEN_HI_SHIFT		8

#define HAP_PTN_VSENSE_ADC_CTL_REG		0x68
#define HAP_PTN_ISENSE_ADC_CTL_REG		0x69
#define HAP_PTN_VISENSE_ADC_CTL_MASK		GENMASK(7, 0)

#define HAP_PTN_VGAIN_ERR_COMP_LSB		0xF1
#define HAP_PTN_IGAIN_ERR_COMP_LSB		0xF3

/* register in HBOOST module */
#define HAP_BOOST_REVISION1			0x00
#define HAP_BOOST_REVISION2			0x01
#define HAP_BOOST_V0P0				0x0000
#define HAP_BOOST_V0P1				0x0001
#define HAP_BOOST_V0P2				0x0002

#define HAP_BOOST_STATUS4_REG			0x0B
#define BOOST_DTEST1_STATUS_BIT			BIT(0)

#define HAP_BOOST_HW_CTRL_FOLLOW_REG		0x41
#define FOLLOW_HW_EN_BIT			BIT(7)
#define FOLLOW_HW_CCM_BIT			BIT(6)
#define FOLLOW_HW_VSET_BIT			BIT(5)

#define HAP_BOOST_VREG_EN_REG			0x46
#define VREG_EN_BIT				BIT(7)

#define HAP_BOOST_CLAMP_REG			0x70
#define CLAMP_5V_BIT				BIT(0)

/* haptics SDAM registers offset definition */
#define HAP_STATUS_DATA_MSB_SDAM_OFFSET		0x46
#define HAP_AUTO_BRAKE_CAL_VMAX_OFFSET		0x4B
#define HAP_AUTO_BRAKE_CAL_PTRN1_CFG0_OFFSET	0x4C
#define HAP_AUTO_BRAKE_CAL_PTRN1_LSB0_OFFSET	0x52
#define HAP_LRA_PTRN1_TLRA_LSB_OFFSET		0x64
#define HAP_LRA_PTRN1_TLRA_MSB_OFFSET		0x65
#define HAP_LRA_NOMINAL_OHM_SDAM_OFFSET		0x75
#define HAP_LRA_DETECTED_OHM_SDAM_OFFSET	0x76
#define HAP_AUTO_BRAKE_CAL_DONE_OFFSET		0x7E

#define AUTO_BRAKE_CAL_DONE			0x80

#define PBS_ARG_REG				0x42
#define PBS_TRIG_SET_REG			0xE5
#define PBS_TRIG_CLR_REG			0xE6
#define PBS_TRIG_SET_VAL			0x1
#define PBS_TRIG_CLR_VAL			0x1

/*
 * haptics SDAM register offset definitions for HAP530 that overrides some
 * definitions above.
 */

#define HAP_AUTO_BRAKE_CAL_CL_BRAKE_RSET_OFFSET	0x52
#define HAP_AUTO_BRAKE_CAL_BRAKE_AMP0_OFFSET	0x53
#define HAP_AUTO_BRAKE_RNAT_DEFAULT_OFFSET	0x61
#define HAP_AUTO_BRAKE_RCAL_DEFAULT_OFFSET	0x62
#define HAP_AUTO_BRAKE_T_WIND_MSB_DEF_OFFSET	0x63
#define HAP_AUTO_BRAKE_T_WIND_LSB_DEF_OFFSET	0x64
#define HAP_AUTO_BRAKE_T_WIND_STS_MSB_OFFSET	0x6E
#define HAP_AUTO_BRAKE_T_WIND_STS_LSB_OFFSET	0x6F
#define HAP_AUTO_BRAKE_RNAT_OFFSET		0x78
#define HAP_AUTO_BRAKE_RCAL_OFFSET		0x79

/* constant parameters */
#define SAMPLES_PER_PATTERN			8
#define BRAKE_SAMPLE_COUNT			8
#define DEFAULT_ERM_PLAY_RATE_US		5000
#define MAX_EFFECT_COUNT			64
#define FIFO_READY_TIMEOUT_MS			1000
#define CHAR_PER_PATTERN_S			48
#define CHAR_PER_SAMPLE				8
#define CHAR_MSG_HEADER				16
#define CHAR_BRAKE_MODE				24
#define HW_BRAKE_MAX_CYCLES			16
#define F_LRA_VARIATION_HZ			5
#define NON_HBOOST_MAX_VMAX_MV			4000
#define FIFO_EMPTY_THRESH_RATIO_NUM		45
#define FIFO_EMPTY_THRESH_RATIO_DEN		100
/* below definitions are only for HAP525_HV */
#define MMAP_NUM_BYTES				2048
#define MMAP_FIFO_MIN_SIZE			640
#define FIFO_PRGM_INIT_SIZE			320

/* below definitions are only for HAP530_HV */
#define HAP530_MMAP_NUM_BYTES			8192
#define REV_T_LRA_US				0
#define VMAX_HDRM_MV_DEFAULT			1500

#define is_between(val, min, max)	\
	(((min) <= (max)) && ((min) <= (val)) && ((val) <= (max)))

#define F0_CALI_INIT 0
#define F0_CALI_FINISHED 1
#define F0_CALI_VALID 2
#define F0_CALI_ERROR_1 -1
#define F0_CALI_ERROR_2 -2

enum hap_status_sel {
	CAL_TLRA_CL_STS = 0x00,
	T_WIND_STS,
	T_WIND_STS_PREV,
	LAST_GOOD_TLRA_CL_STS,
	TLRA_CL_ERR_STS,
	HAP_DRV_STS,
	RNAT_RCAL_INT,
	BRAKE_CAL_SCALAR = 0x07,
	CLAMPED_DUTY_CYCLE_STS = 0x8003,
	FIFO_REAL_TIME_STS = 0x8005,
	AUTO_BRAKE_CAL_STS = 0x8006,
};

enum hap_pbs_type {
	HAP_VREG_ON_PBS = 0x1,
	HAP_VREG_OFF_PBS = 0x2,
	HAP_AUTO_BRAKE_CAL_PBS = 0x3,
	HAP_HBST_OVP_TRIM_PBS = 0x10,
};

enum drv_sig_shape {
	WF_SQUARE,
	WF_SINE,
	WF_NO_MODULATION,
	WF_RESERVED,
};

enum brake_mode {
	OL_BRAKE,
	CL_BRAKE,
	PREDICT_BRAKE,
	AUTO_BRAKE,
};

enum brake_sine_gain {
	BRAKE_SINE_GAIN_X1,
	BRAKE_SINE_GAIN_X2,
	BRAKE_SINE_GAIN_X4,
	BRAKE_SINE_GAIN_X8,
};

enum pat_mem_sel {
	PAT1_MEM,
	PAT2_MEM,
	PAT3_MEM,
	PAT4_MEM,
	PAT_MEM_MAX,
};

enum pattern_src {
	FIFO,
	DIRECT_PLAY,
	PATTERN1,
	PATTERN2,
	SWR,
	PATTERN_MEM,
	SRC_RESERVED,
};

enum s_period {
	T_LRA = 0,
	T_LRA_DIV_2,
	T_LRA_DIV_4,
	T_LRA_DIV_8,
	T_LRA_X_2,
	T_LRA_X_4,
	T_LRA_X_8,
	T_RESERVED,
	/* F_xKHZ definitions are for FIFO only */
	F_8KHZ,
	F_16KHZ,
	F_24KHZ,
	F_32KHZ,
	F_44P1KHZ,
	F_48KHZ,
	F_RESERVED,
};

enum custom_effect_param {
	CUSTOM_DATA_EFFECT_IDX,
	CUSTOM_DATA_TIMEOUT_SEC_IDX,
	CUSTOM_DATA_TIMEOUT_MSEC_IDX,
	CUSTOM_DATA_LEN,
};

/*
 * HW type of the haptics module, the type value follows the
 * revision value of the HAPTICS_CFG/HAPTICS_PATTERN modules
 */
enum haptics_hw_type {
	HAP520 = 0x2,  /* PM8350B */
	HAP520_MV = 0x3,  /* PM5100 */
	HAP525_HV = 0x4,  /* PM8550B */
	HAP530_HV = 0x5,  /* PMIH010X */
};

enum wa_flags {
	TOGGLE_CAL_RC_CLK = BIT(0),
	SW_CTRL_HBST = BIT(1),
	SLEEP_CLK_32K_SCALE = BIT(2),
	TOGGLE_EN_TO_FLUSH_FIFO = BIT(3),
	RECOVER_SWR_SLAVE = BIT(4),
	TOGGLE_MODULE_EN = BIT(5),
	EN_RUNTIME_PM = BIT(6),
	VISENSE_RECOVERY_EN = BIT(7),
	IGNORE_SWR_IN_SPMI_PLAY = BIT(8),
	DISCHARGE_VNDRV_LDO = BIT(9),
	RESTORE_VMAX_HDRM_1P5V = BIT(10),
};

static const char * const src_str[] = {
	"FIFO",
	"DIRECT_PLAY",
	"PATTERN1",
	"PATTERN2",
	"SWR",
	"PATTERN_MEM",
	"reserved",
};

static const char * const brake_str[] = {
	"open-loop-brake",
	"close-loop-brake",
	"predictive-brake",
	"auto-brake",
};

static const char * const period_str[] = {
	"T_LRA",
	"T_LRA_DIV_2",
	"T_LRA_DIV_4",
	"T_LRA_DIV_8",
	"T_LRA_X_2",
	"T_LRA_X_4",
	"T_LRA_X_8",
	"reserved_1",
	"F_8KHZ",
	"F_16HKZ",
	"F_24KHZ",
	"F_32KHZ",
	"F_44P1KHZ",
	"F_48KHZ",
	"reserved_2",
};

struct pattern_s {
	u16			amplitude;
	enum s_period		period;
	bool			f_lra_x2;
};

struct pattern_cfg {
	struct pattern_s	samples[SAMPLES_PER_PATTERN];
	u32			play_rate_us;
	u32			play_length_us;
	bool			preload;
};

struct fifo_cfg {
	u8			*samples;
	u32			num_s;
	enum s_period		period_per_s;
	u32			play_length_us;
	bool			preload;
};

struct brake_cfg {
	u8			samples[BRAKE_SAMPLE_COUNT];
	enum brake_mode		mode;
	enum drv_sig_shape	brake_wf;
	enum brake_sine_gain	sine_gain;
	u32			play_length_us;
	bool			disabled;
};

struct haptics_effect {
	struct pattern_cfg	*pattern;
	struct fifo_cfg		*fifo;
	struct brake_cfg	*brake;
	struct list_head	node;
	u32			id;
	u32			vmax_mv;
	u32			t_lra_us;
	enum pattern_src	src;
	u32			pat_sel;
	bool			auto_res_disable;
};

struct mmap_partition {
	u32	max_size;
	u32	length;
	u32	start_addr;
	bool	in_use;
};

struct mmap_hw_info {
	u32	memory_size;
	u32	pat_mem_step;
	u32	pat_mem_play_step;
	u8	pat_mem_play_len_msb;
	u32	fifo_mem_step;
	u8	fifo_mem_mask;
};

struct haptics_mmap {
	struct mmap_partition	fifo_mmap;
	struct mmap_partition	*pat_sel_mmap;
	struct mmap_hw_info	hw_info;
	enum s_period		pat_play_rate;
};

/**
 * struct fifo_play_status - Data used for recording the FIFO playing status
 *
 * @samples_written:	The number of the samples that has been written into
 *			FIFO memory.
 * @written_done:	The flag to indicate if all of the FIFO samples has
 *			been written to the FIFO memory.
 * @is_busy:		The flag to indicate if it's in the middle of FIFO
 *			playing.
 * @cancelled:		The flag to indicate if FIFO playing is cancelled due
 *			to a stopping command arrived in the middle of playing.
 */
struct fifo_play_status {
	u32			samples_written;
	atomic_t		written_done;
	atomic_t		is_busy;
	atomic_t		cancelled;
};

struct haptics_play_info {
	struct haptics_effect	*effect;
	struct brake_cfg	*brake;
	struct fifo_play_status	fifo_status;
	struct mutex		lock;
	atomic_t		gain;
	u32			vmax_mv;
	u32			length_us;
	enum pattern_src	pattern_src;
	bool			in_calibration;
};

struct haptics_hw_config {
	struct brake_cfg	brake;
	u32			vmax_mv;
	u32			custom_vmax_mv;
	u32			f0_vmax_mv;
	u32			t_lra_us;
	u32			cl_t_lra_us;
	u32			rev_t_lra_us;
	u32			lra_min_mohms;
	u32			lra_max_mohms;
	u32			lra_open_mohms;
	u32			lra_measured_mohms;
	u32			preload_effect;
	u32			fifo_empty_thresh;
	u16			rc_clk_cal_count;
	enum drv_sig_shape	drv_wf;
	bool			is_erm;
	bool			measure_lra_impedance;
	bool			lra_calibrated;
	bool			sw_cmd_freq_det;
	u32			f0_check;
	bool			hbst_ovp_trim;
};

struct custom_fifo_data {
	u32	idx;
	u32	length;
	u32	play_rate_hz;
	u8	*data;
};

struct fifo_hw_info {
	u32	max_fifo_samples;
	u32	fifo_empty_threshold;
	u32	fifo_threshold_per_bit;
	u8	fifo_empty_threshold_mask;
};

struct haptics_chip {
	struct device			*dev;
	struct regmap			*regmap;
	struct input_dev		*input_dev;
	struct haptics_hw_config	config;
	struct haptics_effect		*effects;
	struct haptics_effect		*primitives;
	struct haptics_effect		*custom_effect;
	struct haptics_play_info	play;
	struct haptics_mmap		mmap;
	const struct fifo_hw_info	*fifo_info;
	struct dentry			*debugfs_dir;
	struct delayed_work		stop_work;
	struct regulator_dev		*swr_slave_rdev;
	struct nvmem_cell		*cl_brake_nvmem;
	struct nvmem_device		*hap_cfg_nvmem;
	struct device_node		*pbs_node;
	struct class			hap_class;
	struct regulator		*hpwr_vreg;
	struct hrtimer			hbst_off_timer;
	struct notifier_block		hboost_nb;
	struct notifier_block		lpass_ssr_nb;
	struct mutex			vmax_lock;
	struct work_struct		set_gain_work;
	struct list_head		mmap_effect_list;
	int				fifo_empty_irq;
	u32				hpwr_voltage_mv;
	u32				effects_count;
	u32				primitives_count;
	u32				mmap_effect_count;
	u32				cfg_addr_base;
	u32				ptn_addr_base;
	u32				hbst_addr_base;
	u32				clamped_vmax_mv;
	u32				wa_flags;
	u32				primitive_duration;
	u16				cfg_revision;
	u16				ptn_revision;
	u8				hpwr_intf_ctl;
	u16				hbst_revision;
	u16				max_vmax_mv;
	enum haptics_hw_type		hw_type;
	bool				fifo_empty_irq_en;
	bool				swr_slave_enabled;
	bool				clamp_at_5v;
	bool				hpwr_vreg_enabled;
	bool				is_hv_haptics;
	bool				hboost_enabled;
	bool				visense_enabled;
	bool				visense_hs_disabled;
	ktime_t				pattern_start_time;
	void				*lpass_ssr_handle;
};

struct haptics_reg_info {
	u8 addr;
	u8 val;
};

static const struct fifo_hw_info hap520_fifo = {
	.max_fifo_samples	= 640,
	.fifo_empty_threshold	= 280,
	.fifo_threshold_per_bit = 40,
	.fifo_empty_threshold_mask = HAP520_EMPTY_THRESH_MASK,
};

static const struct fifo_hw_info hap520_mv_fifo = {
	.max_fifo_samples	= 1024,
	.fifo_empty_threshold	= 288,
	.fifo_threshold_per_bit = 32,
	.fifo_empty_threshold_mask = HAP520_MV_EMPTY_THRESH_MASK,
};

static const struct fifo_hw_info hap525_fifo = {
	.max_fifo_samples	= 2048,
	.fifo_empty_threshold	= 288,
	.fifo_threshold_per_bit	= 32,
	.fifo_empty_threshold_mask = HAP525_EMPTY_THRESH_MASK,
};

static const struct fifo_hw_info hap530_fifo = {
	.max_fifo_samples	= 8912,
	.fifo_empty_threshold	= 320,
	.fifo_threshold_per_bit	= 64,
	.fifo_empty_threshold_mask = HAP530_EMPTY_THRESH_MASK,
};

static struct haptics_chip *phapchip;

bool qcom_haptics_vi_sense_is_enabled(void)
{
	if (!phapchip)
		return false;

	return phapchip->visense_enabled;
}
EXPORT_SYMBOL_GPL(qcom_haptics_vi_sense_is_enabled);

static inline int get_max_fifo_samples(struct haptics_chip *chip)
{
	return chip->mmap.fifo_mmap.length ?
		chip->mmap.fifo_mmap.length : chip->fifo_info->max_fifo_samples;
}

static bool is_haptics_external_powered(struct haptics_chip *chip)
{
	/* SW based explicit vote */
	if (chip->hpwr_vreg)
		return true;

	/* Implicit voting by HW */
	if (chip->hw_type == HAP520_MV)
		return true;

	/* Powered by HBOOST */
	return false;
}

static int haptics_read(struct haptics_chip *chip,
		u16 base, u8 offset, u8 *val, u32 length)
{
	int rc = 0;
	u16 addr = base + offset;

	rc = regmap_bulk_read(chip->regmap, addr, val, length);
	if (rc < 0)
		dev_err(chip->dev, "read addr %#x failed, rc=%d\n", addr, rc);

	return rc;
}

static int haptics_write(struct haptics_chip *chip,
		u16 base, u8 offset, u8 *val, u32 length)
{
	int rc = 0;
	u16 addr = base + offset;

	rc = regmap_bulk_write(chip->regmap, addr, val, length);
	if (rc < 0)
		dev_err(chip->dev, "write addr %#x failed, rc=%d\n", addr, rc);

	return rc;
}

static int haptics_masked_write(struct haptics_chip *chip,
		u16 base, u8 offset, u8 mask, u8 val)
{
	int rc = 0;
	u16 addr = base + offset;

	regmap_update_bits(chip->regmap, addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "update addr %#x failed, rc=%d\n", addr, rc);

	return rc;
}

static void __dump_effects(struct haptics_chip *chip,
				struct haptics_effect *effects, int effects_count)
{
	struct haptics_effect *effect;
	struct pattern_s *sample;
	char *str;
	u32 size, pos;
	int i, j;

	for (i = 0; i < effects_count; i++) {
		effect = &effects[i];
		if (!effect)
			return;

		dev_dbg(chip->dev, "effect %d\n", effect->id);
		dev_dbg(chip->dev, "vmax_mv = %d\n", effect->vmax_mv);
		if (effect->pattern) {
			for (j = 0; j < SAMPLES_PER_PATTERN; j++) {
				sample = &effect->pattern->samples[j];
				dev_dbg(chip->dev, "pattern = %d, period = %s, f_lra_x2 = %d\n",
						sample->amplitude,
						period_str[sample->period],
						sample->f_lra_x2);
			}

			dev_dbg(chip->dev, "pattern play_rate_us = %d\n",
					effect->pattern->play_rate_us);
			dev_dbg(chip->dev, "pattern play_length_us = %d\n",
					effect->pattern->play_length_us);
			dev_dbg(chip->dev, "pattern preload = %d\n",
					effect->pattern->preload);
		}

		if (effect->fifo) {
			size = effect->fifo->num_s * CHAR_PER_SAMPLE
							+ CHAR_MSG_HEADER;
			str = kzalloc(size, GFP_KERNEL);
			if (str == NULL)
				return;

			pos = 0;
			pos += scnprintf(str, size, "%s", "FIFO data: ");
			for (j = 0; j < effect->fifo->num_s; j++)
				pos += scnprintf(str + pos, size - pos, "%d ",
						(s8)effect->fifo->samples[j]);

			dev_dbg(chip->dev, "%s\n", str);
			kfree(str);
			dev_dbg(chip->dev, "FIFO data preload: %d, play rate: %s, play length: %uus\n",
					effect->fifo->preload,
					period_str[effect->fifo->period_per_s],
					effect->fifo->play_length_us);
		}

		if (effect->brake && !effect->brake->disabled) {
			size = BRAKE_SAMPLE_COUNT * CHAR_PER_SAMPLE
						+ CHAR_MSG_HEADER;
			str = kzalloc(size, GFP_KERNEL);
			if (str == NULL)
				return;

			pos = 0;
			pos += scnprintf(str, size, "%s", "brake pattern: ");
			for (j = 0; j < BRAKE_SAMPLE_COUNT; j++)
				pos += scnprintf(str + pos, size - pos, "%#x ",
						effect->brake->samples[j]);

			dev_dbg(chip->dev, "%s\n", str);
			kfree(str);
			dev_dbg(chip->dev, "brake mode: %s\n",
					brake_str[effect->brake->mode]);
			dev_dbg(chip->dev, "brake play length: %dus\n",
					effect->brake->play_length_us);
		}

		dev_dbg(chip->dev, "pattern src: %s\n", src_str[effect->src]);
		dev_dbg(chip->dev, "auto resonance %s\n",
				effect->auto_res_disable ?
				"disabled" : "enabled");
	}
}

static void verify_brake_samples(struct brake_cfg *brake)
{
	int i;

	if (brake->mode == PREDICT_BRAKE || brake->mode == AUTO_BRAKE)
		return;

	for (i = BRAKE_SAMPLE_COUNT - 1; i > 0; i--) {
		if (brake->samples[i] != 0) {
			brake->disabled = false;
			return;
		}
	}

	brake->disabled = true;
}

static int get_pattern_play_length_us(struct pattern_cfg *pattern)
{
	int i = SAMPLES_PER_PATTERN - 1, j;
	u32 us_per_sample, total_length_us = 0;

	if (!pattern)
		return -EINVAL;

	for (; i >= 0; i--)
		if (pattern->samples[i].amplitude != 0)
			break;

	for (j = 0; j <= i; j++) {
		us_per_sample = pattern->play_rate_us;
		switch (pattern->samples[j].period) {
		case T_LRA:
			break;
		case T_LRA_DIV_2:
			us_per_sample /= 2;
			break;
		case T_LRA_DIV_4:
			us_per_sample /= 4;
			break;
		case T_LRA_DIV_8:
			us_per_sample /= 8;
			break;
		case T_LRA_X_2:
			us_per_sample *= 2;
			break;
		case T_LRA_X_4:
			us_per_sample *= 4;
			break;
		case T_LRA_X_8:
			us_per_sample *= 8;
			break;
		default:
			return -EINVAL;
		}

		if (pattern->samples[j].f_lra_x2)
			us_per_sample /= 2;

		total_length_us += us_per_sample;
	}

	return total_length_us;
}

static int get_fifo_play_length_us(struct fifo_cfg *fifo, u32 t_lra_us)
{
	u32 length_us;
	int i;

	if (!fifo)
		return -EINVAL;

	for (i = fifo->num_s - 1; i > 0; i--)
		if (fifo->samples[i] != 0)
			break;

	length_us = (i + 1) * t_lra_us;
	switch (fifo->period_per_s) {
	case T_LRA:
		break;
	case T_LRA_DIV_2:
		length_us /= 2;
		break;
	case T_LRA_DIV_4:
		length_us /= 4;
		break;
	case T_LRA_DIV_8:
		length_us /= 8;
		break;
	case T_LRA_X_2:
		length_us *= 2;
		break;
	case T_LRA_X_4:
		length_us *= 4;
		break;
	case T_LRA_X_8:
		length_us *= 8;
		break;
	case F_8KHZ:
		length_us = 1000 * fifo->num_s / 8;
		break;
	case F_16KHZ:
		length_us = 1000 * fifo->num_s / 16;
		break;
	case F_24KHZ:
		length_us = 1000 * fifo->num_s / 24;
		break;
	case F_32KHZ:
		length_us = 1000 * fifo->num_s / 32;
		break;
	case F_44P1KHZ:
		length_us = 10000 * fifo->num_s / 441;
		break;
	case F_48KHZ:
		length_us = 1000 * fifo->num_s / 48;
		break;
	default:
		length_us = -EINVAL;
		break;
	}

	return length_us;
}

static int get_brake_play_length_us(struct brake_cfg *brake, u32 t_lra_us)
{
	int i = BRAKE_SAMPLE_COUNT - 1;

	if (!brake || brake->disabled)
		return 0;

	if (brake->mode == PREDICT_BRAKE || brake->mode == AUTO_BRAKE)
		return HW_BRAKE_MAX_CYCLES * t_lra_us / 2;

	for (; i >= 0; i--)
		if (brake->samples[i] != 0)
			break;

	return t_lra_us * (i + 1) / 2;
}

static int haptics_get_status_data(struct haptics_chip *chip,
			enum hap_status_sel sel, u8 data[])
{
	int rc;
	u8 mod_sel_val[2];
	const char *hap_status_name[BRAKE_CAL_SCALAR + 1] = {
		"CAL_TLRA_CL_STS",
		"T_WIND_STS",
		"T_WIND_STS_PREV",
		"LAST_GOOD_TLRA_CL_STS",
		"TLRA_CL_ERR_STS",
		"HAP_DRV_STS",
		"RNAT_RCAL_INT",
		"BRAKE_CAL_SCALAR",
	};
	const char *name;

	mod_sel_val[0] = sel & 0xff;
	mod_sel_val[1] = (sel >> 8) & 0xff;
	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_MOD_STATUS_XT_REG, &mod_sel_val[1], 1);
	if (rc < 0)
		return rc;

	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_MOD_STATUS_SEL_REG, mod_sel_val, 1);
	if (rc < 0)
		return rc;

	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_STATUS_DATA_MSB_REG, data, 2);
	if (rc < 0)
		return rc;

	if (sel <= BRAKE_CAL_SCALAR)
		name = hap_status_name[sel];
	else if (sel == CLAMPED_DUTY_CYCLE_STS)
		name = "CLAMPED_DUTY_CYCLE_STS";
	else if (sel == FIFO_REAL_TIME_STS)
		name = "FIFO_REAL_TIME_STS";
	else if (sel == AUTO_BRAKE_CAL_STS)
		name = "AUTO_BRAKE_CAL_STS";

	dev_dbg(chip->dev, "Get status data[%s] = (%#x, %#x)\n", name, data[0], data[1]);
	trace_qcom_haptics_status(name, data[0], data[1]);
	return 0;
}

#define AUTO_CAL_CLK_SCALE_DEN		1000
#define AUTO_CAL_CLK_SCALE_NUM		1024
static int haptics_adjust_lra_period(struct haptics_chip *chip, u32 *t_lra_us)
{
	int rc;
	u8 val;

	if (chip->wa_flags & SLEEP_CLK_32K_SCALE) {
		rc = haptics_read(chip, chip->cfg_addr_base,
				HAP_CFG_CAL_EN_REG, &val, 1);
		if (rc < 0)
			return rc;

		if ((val & CAL_RC_CLK_MASK) ==
				(CAL_RC_CLK_AUTO_VAL << CAL_RC_CLK_SHIFT))
			*t_lra_us = div_u64((u64)(*t_lra_us) * AUTO_CAL_CLK_SCALE_NUM,
					AUTO_CAL_CLK_SCALE_DEN);
	}

	return 0;
}

/* constant definitions for calculating TLRA */
#define TLRA_AUTO_RES_ERR_NO_CAL_STEP_PSEC	1667000
#define TLRA_AUTO_RES_NO_CAL_STEP_PSEC		3333000
#define TLRA_AUTO_RES_ERR_AUTO_CAL_STEP_PSEC	\
	((chip->wa_flags & SLEEP_CLK_32K_SCALE) ? 1627700 : 1666667)
#define TLRA_AUTO_RES_AUTO_CAL_STEP_PSEC	\
	((chip->wa_flags & SLEEP_CLK_32K_SCALE) ? 813850 : 833333)
#define SLEEP_CLK_CAL_DIVIDER			\
	((chip->wa_flags & SLEEP_CLK_32K_SCALE) ? 600 : 586)
#define CL_TLRA_ERROR_RANGE_PCT			20
static int haptics_get_closeloop_lra_period(
		struct haptics_chip *chip, bool in_boot)
{
	struct haptics_hw_config *config = &chip->config;
	u16 cal_tlra_cl_sts, tlra_cl_err_sts, tlra_ol, last_good_tlra_cl_sts;
	u8 val[2], rc_clk_cal;
	bool auto_res_done;
	u64 tmp, tmp_us;
	int rc;
	int f0_mix, f0_max, f0_default, f0_cnt;
	int rc1, rc2, rc3, rc4;
	struct device_node *node = chip->dev->of_node;
	if (in_boot) {
		config->f0_check = F0_CALI_INIT;
	}
	/* read RC_CLK_CAL enabling mode */
	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_CAL_EN_REG, val, 1);
	if (rc < 0)
		return rc;

	rc_clk_cal = ((val[0] & CAL_RC_CLK_MASK) >> CAL_RC_CLK_SHIFT);
	/* read auto resonance calibration result */
	if (in_boot && ((chip->hw_type == HAP520) || (chip->hw_type == HAP520_MV))) {
		if (chip->hap_cfg_nvmem == NULL) {
			dev_dbg(chip->dev, "nvmem device for hap_cfg is not defined\n");
			return -EINVAL;
		}

		rc = nvmem_device_read(chip->hap_cfg_nvmem,
				HAP_STATUS_DATA_MSB_SDAM_OFFSET, 2, val);
		if (rc < 0) {
			dev_err(chip->dev, "read SDAM %#x failed, rc=%d\n",
					HAP_STATUS_DATA_MSB_SDAM_OFFSET, rc);
			return rc;
		}
	} else {
		rc = haptics_get_status_data(chip, CAL_TLRA_CL_STS, val);
		if (rc < 0)
			return rc;
	}

	auto_res_done = !!(val[0] & AUTO_RES_CAL_DONE_BIT);
	cal_tlra_cl_sts =
		((val[0] & CAL_TLRA_CL_STS_MSB_MASK) << 8) | val[1];

	/* read auto resonance calibration error status */
	rc = haptics_get_status_data(chip, TLRA_CL_ERR_STS, val);
	if (rc < 0)
		return rc;

	tlra_cl_err_sts =
		((val[0] & TLRA_CL_ERR_MSB_MASK) << 8) | val[1];

	dev_dbg(chip->dev, "rc_clk_cal = %u, auto_res_done = %d\n",
			rc_clk_cal, auto_res_done);

	chip->config.lra_calibrated = auto_res_done;

	if (rc_clk_cal == CAL_RC_CLK_DISABLED_VAL && !auto_res_done) {
		/* TLRA_CL_ERR(us) = TLRA_CL_ERR_STS * 1.667 us */
		tmp = tlra_cl_err_sts * TLRA_AUTO_RES_ERR_NO_CAL_STEP_PSEC;
		dev_dbg(chip->dev, "tlra_cl_err_sts = %#x\n", tlra_cl_err_sts);
		config->cl_t_lra_us = div_u64(tmp, 1000000);
	} else if (rc_clk_cal == CAL_RC_CLK_DISABLED_VAL && auto_res_done) {
		/*
		 * CAL_TLRA_CL_STS_NO_CAL = CAL_TLRA_CL_STS
		 * TLRA_AUTO_RES(us) = CAL_TLRA_CL_STS_NO_CAL * 3.333 us
		 */
		tmp = cal_tlra_cl_sts * TLRA_AUTO_RES_NO_CAL_STEP_PSEC;
		dev_dbg(chip->dev, "cal_tlra_cl_sts = %#x\n", cal_tlra_cl_sts);
		config->cl_t_lra_us = div_u64(tmp, 1000000);
	} else if (rc_clk_cal == CAL_RC_CLK_AUTO_VAL && !auto_res_done) {
		/*
		 * CAL_TLRA_OL = CAL_TLRA_CL_STS;
		 * TLRA_CL_ERR(us) = TLRA_CL_ERR_STS *
		 *     (TLRA_OL / CAL_TLRA_OL) * TLRA_AUTO_RES_ERR_AUTO_CAL_STEP_US
		 */

		/* read the TLRA_OL setting */
		rc = haptics_read(chip, chip->cfg_addr_base,
				HAP_CFG_TLRA_OL_HIGH_REG, val, 2);
		if (rc < 0)
			return rc;

		tlra_ol = (val[0] & TLRA_OL_MSB_MASK) << 8 | val[1];
		dev_dbg(chip->dev, "tlra_ol = %#x, tlra_cl_err_sts = %#x, cal_tlra_cl_sts = %#x\n",
				tlra_ol, tlra_cl_err_sts, cal_tlra_cl_sts);

		tmp = tlra_cl_err_sts * tlra_ol;
		tmp *= TLRA_AUTO_RES_ERR_AUTO_CAL_STEP_PSEC;
		tmp = div_u64(tmp, cal_tlra_cl_sts);
		config->cl_t_lra_us = div_u64(tmp, 1000000);

		/* calculate RC_CLK_CAL_COUNT */
		if (!config->t_lra_us || !config->cl_t_lra_us)
			return -EINVAL;
		/*
		 * RC_CLK_CAL_COUNT = SLEEP_CLK_CAL_DIVIDER * (CAL_TLRA_OL / TLRA_OL)
		 *		* (SLEEP_CLK_CAL_DIVIDER / 586) * (CL_T_TLRA_US / OL_T_LRA_US)
		 */
		tmp = SLEEP_CLK_CAL_DIVIDER * SLEEP_CLK_CAL_DIVIDER;
		tmp *= cal_tlra_cl_sts * config->cl_t_lra_us;
		tmp = div_u64(tmp, tlra_ol);
		tmp = div_u64(tmp, 586);
		config->rc_clk_cal_count = div_u64(tmp, config->t_lra_us);
	} else if (rc_clk_cal == CAL_RC_CLK_AUTO_VAL && auto_res_done) {
		/*
		 * CAL_TLRA_CL_STS_W_CAL = CAL_TLRA_CL_STS;
		 * TLRA_AUTO_RES(us) = LAST_GOOD_TLRA_CL_STS *
		 *      TLRA_AUTO_RES_AUTO_CAL_STEP_US *
		 *     (LAST_GOOD_TLRA_CL_STS / CAL_TLRA_CL_STS_AUTO_CAL)
		 */

		/* read LAST_GOOD_TLRA_CL_STS */
		rc = haptics_get_status_data(chip, LAST_GOOD_TLRA_CL_STS, val);
		if (rc < 0)
			return rc;

		last_good_tlra_cl_sts =
			((val[0] & LAST_GOOD_TLRA_CL_MSB_MASK) << 8) | val[1];

		dev_dbg(chip->dev, "last_good_tlra_cl_sts = %#x, cal_tlra_cl_sts = %#x\n",
				last_good_tlra_cl_sts, cal_tlra_cl_sts);

		tmp = last_good_tlra_cl_sts * last_good_tlra_cl_sts;
		tmp *= TLRA_AUTO_RES_AUTO_CAL_STEP_PSEC;
		tmp = div_u64(tmp, cal_tlra_cl_sts);
		config->cl_t_lra_us = div_u64(tmp, 1000000);
		tmp_us = config->cl_t_lra_us;
		config->cl_t_lra_us = tmp_us + config->rev_t_lra_us;

		/* calculate RC_CLK_CAL_COUNT */
		if (!config->t_lra_us || !config->cl_t_lra_us)
			return -EINVAL;

		/*
		 * RC_CLK_CAL_COUNT =
		 *	SLEEP_CLK_CAL_DIVIDER * (CAL_TLRA_CL_STS_AUTO_CAL / LAST_GOOD_TLRA_CL_STS)
		 *		* (SLEEP_CLK_CAL_DIVIDER / 293) * (CL_T_TLRA_US / OL_T_LRA_US)
		 */
		tmp = SLEEP_CLK_CAL_DIVIDER * SLEEP_CLK_CAL_DIVIDER;
		tmp *= cal_tlra_cl_sts * tmp_us;
		tmp = div_u64(tmp, last_good_tlra_cl_sts);
		tmp = div_u64(tmp, 293);
		config->rc_clk_cal_count = div_u64(tmp, config->t_lra_us);
	} else {
		dev_err(chip->dev, "Can't get close-loop LRA period in rc_clk_cal mode %u\n",
				rc_clk_cal);
		return -EINVAL;
	}
  
	if (in_boot) {
		config->f0_check = F0_CALI_FINISHED;
	}

	if(in_boot){
		u32  xbl_f0 = USEC_PER_SEC / config->cl_t_lra_us;
		dev_err(chip->dev, "xbl f0  =%d \n", xbl_f0);
		rc1 = of_property_read_u32(node, "qcom,lra-f0-min", &f0_mix);
		if (rc1 < 0) {
			dev_err(chip->dev, "lra-f0-min failed, rc=%d\n", rc);
		}
		rc2 = of_property_read_u32(node, "qcom,lra-f0-max", &f0_max);
		if (rc2 < 0) {
			dev_err(chip->dev, "lra-f0-max failed, rc=%d\n", rc);
                }
		rc3 = of_property_read_u32(node, "qcom,lra-f0-default", &f0_default);
		if (rc3 < 0) {
			dev_err(chip->dev, "lra-f0-default failed, rc=%d\n", rc);
		}
		rc4 = of_property_read_u32(node, "qcom,lra-f0-cal-count", &f0_cnt);
		if (rc4 < 0) {
			dev_err(chip->dev, "lra-f0-cal-count failed, rc=%d\n", rc);
		}
		if (rc1 >= 0 && rc2 >= 0 && rc3 >= 0 && rc4 >= 0) {
			if (xbl_f0 > f0_max || xbl_f0 < f0_mix) {
				dev_err(chip->dev, "xbl f0 abnormal: %d ~ 0x%x use default: %d ~ 0x%x f0:%d - %d after boot\n",xbl_f0, config->rc_clk_cal_count, f0_default, f0_cnt, f0_mix, f0_max);
				config->cl_t_lra_us = USEC_PER_SEC /f0_default;
				config->rc_clk_cal_count = f0_cnt;
                                if (!auto_res_done) {
					config->f0_check = F0_CALI_ERROR_1;
				} else {
					config->f0_check = F0_CALI_ERROR_2;
				}
			} else {
				config->f0_check = F0_CALI_VALID;
			}
		} else {
			dev_err(chip->dev, "lra-f0: default min max count must set together in dtsi\n");
		}
	}
	/*
	if ((abs(config->t_lra_us - config->cl_t_lra_us) * 100 / config->t_lra_us) >
			CL_TLRA_ERROR_RANGE_PCT) {
		dev_warn(chip->dev, "The calibrated period (%d us) has large variation, use open-loop LRA period (%d us) instead\n",
				config->cl_t_lra_us, config->t_lra_us);
		config->cl_t_lra_us = config->t_lra_us;
		chip->config.rc_clk_cal_count = 0;
	}*/

	dev_dbg(chip->dev, "OL_TLRA %u us, CL_TLRA %u us, RC_CLK_CAL_COUNT %#x\n",
		chip->config.t_lra_us, chip->config.cl_t_lra_us,
		chip->config.rc_clk_cal_count);
	return 0;
}

static int haptics_module_enable(struct haptics_chip *chip, bool enable)
{
	u8 val;
	int rc;
	unsigned int delay_us = 100;

	/*
	 * Increase the delay to 500us for HAP530_HV to remove the potential
	 * overshoot on VNDRV when there is a rapid haptics module enable and
	 * disable sequence.
	 */
	if (chip->hw_type >= HAP530_HV)
		delay_us = 500;

	/*
	 * Delay for a while before enabling haptics module to avoid
	 * it's mistakenly blocked by HW debounce logic.
	 */
	if (enable)
		usleep_range(delay_us, delay_us + 1);

	val = enable ? HAPTICS_EN_BIT : 0;
	rc = haptics_write(chip, chip->cfg_addr_base,
		HAP_CFG_EN_CTL_REG, &val, 1);
	if (rc < 0)
		return rc;

	dev_dbg(chip->dev, "haptics module %s\n",
		enable ? "enabled" : "disabled");
	return 0;
}

static int haptics_toggle_module_enable(struct haptics_chip *chip)
{
	int rc;

	/*
	 * Updating HAPTICS_EN would vote hBoost enable status. Add 100us
	 * delay before updating HAPTICS_EN for hBoost to have enough time
	 * to handle its power transition.
	 */
	usleep_range(100, 101);
	rc = haptics_module_enable(chip, false);
	if (rc < 0)
		return rc;

	return haptics_module_enable(chip, true);
}

#define VMAX_SETTLE_COUNT	10
static int haptics_check_hpwr_status(struct haptics_chip *chip)
{
	int i, rc = 0;
	u8 val;

	/* Apply the HPWR_READY check for haptics module revision HAP525_HV and later. */
	if (chip->hw_type < HAP525_HV)
		return 0;

	for (i = 0; i < VMAX_SETTLE_COUNT; i++) {
		rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_HPWR_INTF_REG, &val, 1);
		if (rc < 0)
			break;

		val &= HPWR_INTF_STATUS_MASK;

		if (chip->wa_flags & DISCHARGE_VNDRV_LDO) {
			/*
			 * Haptics VNDRV LDO has already been disabled when HPWR_DISABLED
			 * status is set, delay 500us here to discharge the VNDRV voltage.
			 */
			if (val == HPWR_DISABLED) {
				usleep_range(500, 501);
				break;
			}
		} else {
			if ((val == HPWR_DISABLED) || (val == HPWR_READY))
				break;
		}

		usleep_range(1000, 1001);
	}

	if (!rc && i == VMAX_SETTLE_COUNT) {
		haptics_toggle_module_enable(chip);
		dev_err(chip->dev, "set Vmax failed, toggle HAPTICS_EN to restore HW status\n");
		rc = -EBUSY;
	}

	return rc;
}

static int haptics_get_lra_nominal_impedance(struct haptics_chip *chip, u32 *nominal_ohm)
{
	u8 val;
	int rc;

	if (!chip->hap_cfg_nvmem) {
		dev_dbg(chip->dev, "nvmem is not defined, couldn't get defined LRA nominal impedance\n");
		return -EINVAL;
	}

	rc = nvmem_device_read(chip->hap_cfg_nvmem,
			HAP_LRA_NOMINAL_OHM_SDAM_OFFSET, 1, &val);
	if (rc > 0) {
		*nominal_ohm = (val * LRA_IMPEDANCE_MOHMS_LSB) / 1000;
		dev_dbg(chip->dev, "LRA nominal impedance is %d ohm\n", *nominal_ohm);
	}

	return rc > 0 ? 0 : rc;
}

static int haptics_clear_fault(struct haptics_chip *chip)
{
	u8 val;

	val = SC_CLR_BIT | AUTO_RES_ERR_CLR_BIT |
		HPWR_RDY_FAULT_CLR_BIT;

	if (chip->hw_type >= HAP530_HV)
		val |= HAP_ZX_TO_FAULT_CLR_BIT;

	return haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_FAULT_CLR_REG, &val, 1);
}

static int haptics_set_vmax_headroom_mv(struct haptics_chip *chip, u32 hdrm_mv)
{
	int rc = 0;
	u8 val;

	if (hdrm_mv > VMAX_HDRM_MAX_MV) {
		dev_err(chip->dev, "headroom (%d) exceed the max value: %d\n",
					hdrm_mv, VMAX_HDRM_MAX_MV);
		return -EINVAL;
	}

	val = hdrm_mv / VMAX_HDRM_STEP_MV;
	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_VMAX_HDRM_REG, &val, 1);
	if (rc < 0)
		dev_err(chip->dev, "config VMAX_HDRM failed, rc=%d\n", rc);

	return rc;
}

static int haptics_get_vmax_headroom_mv(struct haptics_chip *chip, u32 *hdrm_mv)
{
	int rc;
	u8 val;

	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_VMAX_HDRM_REG, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Get Vmax HDRM failed, rc=%d\n",
				rc);
		return rc;
	}

	*hdrm_mv = (val & VMAX_HDRM_MASK) * VMAX_HDRM_STEP_MV;
	return 0;
}

static int __haptics_set_vmax_mv(struct haptics_chip *chip, u32 vmax_mv)
{
	int rc = 0;
	u8 val, vmax_step;

	if (vmax_mv > chip->max_vmax_mv) {
		dev_dbg(chip->dev, "vmax (%d) exceed the max value: %d\n",
					vmax_mv, chip->max_vmax_mv);
		vmax_mv = chip->max_vmax_mv;
	}

	vmax_step = (chip->is_hv_haptics) ? VMAX_HV_STEP_MV : VMAX_MV_STEP_MV;
	val = vmax_mv / vmax_step;
	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_VMAX_REG, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "config VMAX failed, rc=%d\n", rc);
		return rc;
	}

	dev_dbg(chip->dev, "Set Vmax to %u mV\n", vmax_mv);
	rc = haptics_check_hpwr_status(chip);
	if (rc < 0)
		dev_err(chip->dev, "check hpwr_status failed, rc=%d\n", rc);

	return rc;
}

static int haptics_ignore_swr_play(struct haptics_chip *chip, bool enable)
{
	int rc;
	u8 val = enable ? SWR_IGNORE_BIT : 0;

	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_TRIG_PRIORITY_REG, SWR_IGNORE_BIT, val);
	if (rc < 0)
		dev_err(chip->dev, "update SWR_IGNORE_BIT failed, rc=%d\n", rc);

	return rc;
}

static int haptics_set_vmax_mv(struct haptics_chip *chip, u32 vmax_mv)
{
	int rc = 0;
	u32 nominal_ohm, vmax_hdrm_mv;
	u8 cl_vdrv_ctl;

	mutex_lock(&chip->vmax_lock);
	if (vmax_mv > chip->clamped_vmax_mv)
		vmax_mv = chip->clamped_vmax_mv;

	if (chip->clamp_at_5v && (vmax_mv > CLAMPED_VMAX_MV))
		vmax_mv = CLAMPED_VMAX_MV;

	rc = haptics_get_lra_nominal_impedance(chip, &nominal_ohm);
	if (!rc && (chip->hw_type >= HAP525_HV)) {
		if ((nominal_ohm >= 15) && (vmax_mv > 4500))
			vmax_hdrm_mv = 1000;
		else if ((nominal_ohm < 15) && (vmax_mv > 7000))
			vmax_hdrm_mv = 1500;
		else if ((nominal_ohm < 15) && (vmax_mv > 3500))
			vmax_hdrm_mv = 1000;
		else
			vmax_hdrm_mv = 500;

		if ((vmax_mv + vmax_hdrm_mv) > MAX_VMAX_MV)
			vmax_mv = MAX_VMAX_MV - vmax_hdrm_mv;

		rc = haptics_set_vmax_headroom_mv(chip, vmax_hdrm_mv);
		if (rc < 0)
			goto unlock;

		dev_dbg(chip->dev, "update VMAX_HDRM to %d mv\n", vmax_hdrm_mv);
	}

	if (chip->hw_type == HAP530_HV) {
		rc = haptics_masked_write(chip, chip->cfg_addr_base,
				HAP_CFG_ZX_SYNC_CFG,
				EN_PDN_ZX_TO_FAULT_BIT,
				EN_PDN_ZX_TO_FAULT_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "enable PDM_ZX_TO_FAULT failed, rc=%d\n", rc);
			goto unlock;
		}

		rc = haptics_read(chip, chip->ptn_addr_base,
				HAP_PTN_CL_VDRIVE_CTL_REG, &cl_vdrv_ctl, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Read CL_VDRIVE_CTL failed, rc=%d\n", rc);
			goto unlock;
		}

		rc = haptics_masked_write(chip, chip->ptn_addr_base,
				HAP_PTN_CL_VDRIVE_CTL_REG,
				EN_CL_VDRIVE_BIT, 0);
		if (rc < 0) {
			dev_err(chip->dev, "disable CL_VDRIVE_CTL failed, rc=%d\n", rc);
			goto unlock;
		}

		rc = haptics_clear_fault(chip);
		if (rc < 0)
			goto unlock;
	}

	rc = __haptics_set_vmax_mv(chip, vmax_mv);
	if (rc < 0)
		goto unlock;

	if (chip->hw_type == HAP530_HV) {
		rc = haptics_masked_write(chip, chip->cfg_addr_base,
				HAP_CFG_ZX_SYNC_CFG,
				EN_PDN_ZX_TO_FAULT_BIT, 0);
		if (rc < 0) {
			dev_err(chip->dev, "disable PDM_ZX_TO_FAULT failed, rc=%d\n", rc);
			goto unlock;
		}

		rc = haptics_write(chip, chip->ptn_addr_base,
				HAP_PTN_CL_VDRIVE_CTL_REG, &cl_vdrv_ctl, 1);
		if (rc < 0)
			dev_err(chip->dev, "restore CL_VDRIVE_CTL failed, rc=%d\n", rc);
	}

unlock:
	mutex_unlock(&chip->vmax_lock);
	return rc;
}

static int haptics_enable_autores(struct haptics_chip *chip, bool en)
{
	int rc;

	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_AUTORES_CFG_REG, AUTORES_EN_BIT,
			en ? AUTORES_EN_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "%s auto resonance failed, rc=%d\n",
				en ? "enable" : "disable", rc);

	return rc;
}

static int haptics_set_direct_play(struct haptics_chip *chip, u8 amplitude)
{
	int rc;

	rc = haptics_write(chip, chip->ptn_addr_base,
			HAP_PTN_DIRECT_PLAY_REG, &amplitude, 1);
	if (rc < 0)
		dev_err(chip->dev, "config DIRECT_PLAY failed, rc=%d\n", rc);

	return rc;
}

static int haptics_trigger_pbs(struct haptics_chip *chip, enum hap_pbs_type type)
{
	int rc;
	u8 val;

	val = (u8)type;
	rc = nvmem_device_write(chip->hap_cfg_nvmem, PBS_ARG_REG, 1, &val);
	if (rc < 0) {
		dev_err(chip->dev, "set PBS_ARG for auto brake cal failed, rc=%d\n",
			rc);
		return rc;
	}

	val = PBS_TRIG_CLR_VAL;
	rc = nvmem_device_write(chip->hap_cfg_nvmem, PBS_TRIG_CLR_REG, 1, &val);
	if (rc < 0) {
		dev_err(chip->dev, "clear PBS_TRIG for auto brake cal failed, rc=%d\n",
			rc);
		return rc;
	}

	val = PBS_TRIG_SET_VAL;
	rc = nvmem_device_write(chip->hap_cfg_nvmem, PBS_TRIG_SET_REG, 1, &val);
	if (rc < 0)
		dev_err(chip->dev, "set PBS_TRIG for auto brake cal failed, rc=%d\n",
			rc);

	return rc;
}

static bool is_boost_vreg_enabled_in_open_loop(struct haptics_chip *chip)
{
	int rc;
	u8 val;

	if (is_haptics_external_powered(chip))
		return false;

	rc = haptics_read(chip, chip->hbst_addr_base, HAP_BOOST_VREG_EN_REG, &val, 1);
	if (rc < 0)
		return false;

	chip->hboost_enabled = (val & VREG_EN_BIT);
	rc = haptics_read(chip, chip->hbst_addr_base, HAP_BOOST_HW_CTRL_FOLLOW_REG, &val, 1);
	if (rc < 0)
		return false;

	if (!(val & FOLLOW_HW_EN_BIT) && chip->hboost_enabled) {
		dev_dbg(chip->dev, "HBoost is enabled in open loop condition\n");
		return true;
	}

	return false;
}

static int haptics_boost_vreg_enable(struct haptics_chip *chip, bool en)
{
	int rc;
	enum hap_pbs_type type;

	if (is_haptics_external_powered(chip))
		return 0;

	if (!(chip->wa_flags & SW_CTRL_HBST))
		return 0;

	if (chip->hap_cfg_nvmem == NULL) {
		dev_dbg(chip->dev, "nvmem device for hap_cfg is not defined\n");
		return 0;
	}

	if (is_boost_vreg_enabled_in_open_loop(chip)) {
		dev_dbg(chip->dev, "Ignore %s hBoost while it's enabled in open-loop mode\n",
				en ? "enabling" : "disabling");
		return 0;
	}

	if (chip->hboost_enabled == en)
		return 0;

	type = en ? HAP_VREG_ON_PBS : HAP_VREG_OFF_PBS;
	rc = haptics_trigger_pbs(chip, type);
	if (rc < 0) {
		dev_err(chip->dev, "trigger HAP_VREG_ON/OFF PBS failed, rc=%d\n", rc);
		return rc;
	}

	chip->hboost_enabled = en;
	return 0;
}

static bool is_swr_play_enabled(struct haptics_chip *chip)
{
	int rc;
	u8 val[2];

	rc = haptics_get_status_data(chip, HAP_DRV_STS, val);
	if (rc < 0)
		return false;

	if ((val[1] & HAP_DRV_PATTERN_SRC_STATUS_MASK) == SWR)
		return true;

	return false;
}

#define HBOOST_WAIT_READY_COUNT		100
#define HBOOST_WAIT_READY_INTERVAL_US	200
static int haptics_wait_hboost_ready(struct haptics_chip *chip)
{
	int i, rc;
	u8 val;

	if (is_haptics_external_powered(chip))
		return 0;

	if (!(chip->wa_flags & SW_CTRL_HBST))
		return 0;

	if ((hrtimer_get_remaining(&chip->hbst_off_timer) > 0) ||
			hrtimer_active(&chip->hbst_off_timer)) {
		hrtimer_cancel(&chip->hbst_off_timer);
		dev_dbg(chip->dev, "hboost is still on, ignore\n");
		return 0;
	}

	/*
	 * Wait ~20ms until hBoost is ready, otherwise
	 * bail out and return -EBUSY
	 */
	for (i = 0; i < HBOOST_WAIT_READY_COUNT; i++) {
		/* HBoost is always ready when working in open loop mode */
		if (is_boost_vreg_enabled_in_open_loop(chip))
			return 0;

		/*
		 * If there is already a SWR play in the background, then HBoost
		 * will be kept as on hence no need to wait its ready.
		 */
		mutex_lock(&chip->play.lock);
		if (is_swr_play_enabled(chip)) {
			dev_dbg(chip->dev, "Ignore waiting hBoost when SWR play is in progress\n");
			mutex_unlock(&chip->play.lock);
			return 0;
		}
		mutex_unlock(&chip->play.lock);

		/* Check if HBoost is in standby (disabled) state */
		rc = haptics_read(chip, chip->hbst_addr_base,
				HAP_BOOST_VREG_EN_REG, &val, 1);
		if (!rc && !(val & VREG_EN_BIT)) {
			rc = haptics_read(chip, chip->hbst_addr_base,
					HAP_BOOST_STATUS4_REG, &val, 1);
			if (!rc && !(val & BOOST_DTEST1_STATUS_BIT))
				return 0;
		}

		dev_dbg(chip->dev, "hBoost is busy, wait %d\n", i);
		usleep_range(HBOOST_WAIT_READY_INTERVAL_US,
				HBOOST_WAIT_READY_INTERVAL_US + 1);
	}

	if (i == HBOOST_WAIT_READY_COUNT) {
		dev_err(chip->dev, "hboost is not ready for haptics play\n");
		return -EBUSY;
	}

	return 0;
}

static int haptics_enable_hpwr_vreg(struct haptics_chip *chip, bool en)
{
	int rc;

	if (chip->hpwr_vreg == NULL || chip->hpwr_vreg_enabled == en)
		return 0;

	if (en) {
		rc = regulator_set_voltage(chip->hpwr_vreg,
				chip->hpwr_voltage_mv * 1000, INT_MAX);
		if (rc < 0) {
			dev_err(chip->dev, "Set hpwr voltage failed, rc=%d\n",
					rc);
			return rc;
		}

		rc = regulator_enable(chip->hpwr_vreg);
		if (rc < 0) {
			dev_err(chip->dev, "Enable hpwr failed, rc=%d\n",
					rc);
			regulator_set_voltage(chip->hpwr_vreg, 0, INT_MAX);
			return rc;
		}
	} else {
		rc = regulator_disable(chip->hpwr_vreg);
		if (rc < 0) {
			dev_err(chip->dev, "Disable hpwr failed, rc=%d\n",
					rc);
			return rc;
		}

		rc = regulator_set_voltage(chip->hpwr_vreg, 0, INT_MAX);
		if (rc < 0) {
			dev_err(chip->dev, "Set hpwr voltage failed, rc=%d\n",
					rc);
			return rc;
		}
	}

	dev_dbg(chip->dev, "%s hpwr vreg\n", en ? "enabled" : "disabled");
	chip->hpwr_vreg_enabled = en;
	return 0;
}

static int haptics_force_vreg_ready(struct haptics_chip *chip, bool ready)
{
	u8 mask;

	mask = FORCE_VREG_RDY_BIT;
	if (chip->hw_type >= HAP525_HV)
		mask |= FORCE_VSET_ACK_BIT;

	return haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_VSET_CFG_REG, mask, ready ? mask : 0);
}

static int haptics_open_loop_drive_config(struct haptics_chip *chip, bool en)
{
	int rc = 0;
	u8 val;

	if ((is_boost_vreg_enabled_in_open_loop(chip) ||
			chip->hboost_enabled || is_haptics_external_powered(chip)) && en) {
		/*
		 * Only set force-VREG-ready here if hBoost is not used by charger firmware.
		 * In the case of charger firmware enabling hBoost, force-VREG-ready will be
		 * set/reset based on the VMAX_CLAMP notification.
		 */
		if (chip->clamped_vmax_mv == MAX_HV_VMAX_MV) {
			rc = haptics_force_vreg_ready(chip, true);
			if (rc < 0)
				return rc;
		}

		/* Toggle RC_CLK_CAL_EN if it's in auto mode */
		rc = haptics_read(chip, chip->cfg_addr_base,
				HAP_CFG_CAL_EN_REG, &val, 1);
		if (rc < 0)
			return rc;

		if ((chip->wa_flags & TOGGLE_CAL_RC_CLK) &&
		    ((val & CAL_RC_CLK_MASK) ==
		      CAL_RC_CLK_AUTO_VAL << CAL_RC_CLK_SHIFT)) {
			val = CAL_RC_CLK_DISABLED_VAL << CAL_RC_CLK_SHIFT;
			rc = haptics_masked_write(chip, chip->cfg_addr_base,
					HAP_CFG_CAL_EN_REG, CAL_RC_CLK_MASK,
					val);
			if (rc < 0)
				return rc;

			val = CAL_RC_CLK_AUTO_VAL << CAL_RC_CLK_SHIFT;
			rc = haptics_masked_write(chip, chip->cfg_addr_base,
					HAP_CFG_CAL_EN_REG, CAL_RC_CLK_MASK,
					val);
			if (rc < 0)
				return rc;

			dev_dbg(chip->dev, "Toggle CAL_EN in open-loop-VREG playing\n");
		}
	} else if (!is_haptics_external_powered(chip) &&
			(chip->clamped_vmax_mv == MAX_HV_VMAX_MV)) {
		/*
		 * Reset force-VREG-ready only when hBoost is not
		 * used by charger firmware.
		 */
		rc = haptics_force_vreg_ready(chip, false);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int haptics_visense_handshake_enable(struct haptics_chip *chip, bool enable)
{
	u8 mask, val;
	int rc;

	if (chip->visense_hs_disabled != enable)
		return 0;

	mask = EN_FE_PU_CHECK_BIT;
	val = enable ? mask : 0;
	rc = haptics_masked_write(chip, chip->cfg_addr_base, HAP_CFG_DIG_SPARE_REG, mask, val);
	if (rc < 0)
		return rc;

	mask = EN_VISENSE_PD_PROT_BIT;
	val = enable ? mask : 0;
	rc = haptics_masked_write(chip, chip->ptn_addr_base, HAP_PTN_VISENSE_ADC_CFG, mask, val);
	if (rc < 0)
		return rc;

	mask = EN_CL_VDRIVE_BIT;
	val = enable ? mask : 0;
	rc = haptics_masked_write(chip, chip->ptn_addr_base, HAP_PTN_CL_VDRIVE_CTL_REG, mask, val);
	if (rc < 0)
		return rc;

	chip->visense_hs_disabled = !enable;
	return 0;
}

static int haptics_wait_brake_complete(struct haptics_chip *chip)
{
	struct haptics_play_info *play = &chip->play;
	u32 brake_length_us, t_lra_us, timeout, delay_us;
	int rc;
	u8 val;

	/*
	 * Apply the brake synchronization delay for haptics module
	 * revision HAP525_HV and later.
	 */
	if (chip->hw_type < HAP525_HV)
		return 0;

	t_lra_us = (chip->config.cl_t_lra_us) ?
		chip->config.cl_t_lra_us : chip->config.t_lra_us;

	brake_length_us = get_brake_play_length_us(play->brake, t_lra_us);

	/* add a cycle to give some margin for brake synchronization */
	brake_length_us += t_lra_us;
	delay_us = t_lra_us / 2;
	timeout = brake_length_us / delay_us + 1;
	dev_dbg(chip->dev, "wait %d us for brake pattern to complete\n", brake_length_us);

	/* poll HPWR_DISABLED to guarantee the brake pattern has been played completely */
	do {
		usleep_range(delay_us, delay_us + 1);
		rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_HPWR_INTF_REG, &val, 1);
		if (rc < 0) {
			dev_err(chip->dev, "read HPWR_INTF failed, rc=%d\n", rc);
			return rc;
		}

		if ((val & HPWR_INTF_STATUS_MASK) == HPWR_DISABLED) {
			/* Delay 500us to discharge the VNDRV voltage after play is stopped */
			if (chip->wa_flags & DISCHARGE_VNDRV_LDO)
				usleep_range(500, 501);

			dev_dbg(chip->dev, "stopped play completely");
			break;
		}

		dev_dbg(chip->dev, "polling HPWR_INTF timeout %d, value = %d\n",
				timeout, val);
	} while (--timeout);

	if (timeout == 0) {
		/*
		 * If there is a SWR play in the background, HPWR_DISABLED
		 * bit won't be set and toggling HAPTICS_EN will re-initiate
		 * the voltage requested from hBoost, it would cause potential
		 * glitches on hBoost output which is unexpected, hence ignore
		 * doing that in such case.
		 */
		if (is_swr_play_enabled(chip))
			return 0;

		dev_warn(chip->dev, "poll HPWR_DISABLED failed after stopped play\n");
		return haptics_toggle_module_enable(chip);
	}

	return 0;
}

#define BOOST_VREG_OFF_DELAY_SECONDS	2
static int haptics_enable_play(struct haptics_chip *chip, bool en)
{
	struct haptics_play_info *play = &chip->play;
	int rc;
	u8 val;

	if (en) {
		rc = haptics_clear_fault(chip);
		if (rc < 0)
			goto restore;
	}

	rc = haptics_open_loop_drive_config(chip, en);
	if (rc < 0)
		goto restore;

	val = play->pattern_src;
	if (chip->hw_type == HAP525_HV && play->pattern_src == PATTERN_MEM)
		val |= FIELD_PREP(PATX_MEM_SEL_MASK, play->effect->pat_sel);

	if (en)
		val |= PLAY_EN_BIT;

	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_SPMI_PLAY_REG, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Write SPMI_PLAY failed, rc=%d\n", rc);
		goto restore;
	}

	if (!en)
		haptics_wait_brake_complete(chip);

	if (chip->wa_flags & SW_CTRL_HBST) {
		if (en) {
			rc = haptics_boost_vreg_enable(chip, true);
			if (rc < 0) {
				dev_err(chip->dev, "Keep boost vreg on failed, rc=%d\n",
						rc);
				goto restore;
			}
		} else {
			hrtimer_start(&chip->hbst_off_timer,
					ktime_set(BOOST_VREG_OFF_DELAY_SECONDS, 0),
					HRTIMER_MODE_REL);
		}
	}

	if ((chip->wa_flags & TOGGLE_MODULE_EN) && !en)
		rc = haptics_toggle_module_enable(chip);

	trace_qcom_haptics_play(en);
restore:
	/* Restore SWR play mode after SPMI play mode is done or any faults */
	if ((!en || rc) && (chip->wa_flags & IGNORE_SWR_IN_SPMI_PLAY))
		haptics_ignore_swr_play(chip, false);

	return rc;
}

static int haptics_set_brake(struct haptics_chip *chip, struct brake_cfg *brake)
{
	int rc = 0;
	u8 val;

	if (brake->disabled)
		return 0;

	val = brake->mode << BRAKE_MODE_SHIFT |
		brake->sine_gain << BRAKE_SINE_GAIN_SHIFT;
	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_BRAKE_MODE_CFG_REG,
			BRAKE_MODE_MASK | BRAKE_SINE_GAIN_MASK, val);
	if (rc < 0) {
		dev_err(chip->dev, "set brake CFG failed, rc=%d\n", rc);
		return rc;
	}

	rc = haptics_write(chip, chip->ptn_addr_base, HAP_PTN_BRAKE_AMP_REG,
			brake->samples, BRAKE_SAMPLE_COUNT);
	if (rc < 0) {
		dev_err(chip->dev, "set brake pattern failed, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int haptics_set_pattern(struct haptics_chip *chip,
		struct pattern_cfg *pattern,
		enum pattern_src src)
{
	struct pattern_s *sample;
	u8 values[SAMPLES_PER_PATTERN * 3] = { 0 };
	u8 ptn_tlra_addr, ptn_cfg_addr;
	int i, rc, tmp;
	u32 play_rate_us;

	if (chip->hw_type >= HAP530_HV) {
		dev_dbg(chip->dev, "haptics module with revision %d no longer supports PATTERNx mode, ignore parsing pattern effect DT\n",
				chip->hw_type);
		return 0;
	}

	if (src != PATTERN1 && src != PATTERN2) {
		dev_err(chip->dev, "no pattern src specified!\n");
		return -EINVAL;
	}

	ptn_tlra_addr = HAP_PTN_PTRN1_TLRA_MSB_REG;
	ptn_cfg_addr = HAP_PTN_PTRN1_CFG_REG;
	if (src == PATTERN2) {
		ptn_tlra_addr = HAP_PTN_PTRN2_TLRA_MSB_REG;
		ptn_cfg_addr = HAP_PTN_PTRN2_CFG_REG;
	}

	/* Adjust T_LRA before programming it into HW */
	play_rate_us = pattern->play_rate_us;
	rc = haptics_adjust_lra_period(chip, &play_rate_us);
	if (rc < 0)
		return rc;

	/* Configure T_LRA for this pattern */
	tmp = play_rate_us / TLRA_STEP_US;
	values[0] = (tmp >> 8) & TLRA_OL_MSB_MASK;
	values[1] = tmp & TLRA_OL_LSB_MASK;
	rc = haptics_write(chip, chip->ptn_addr_base, ptn_tlra_addr,
			values, 2);
	if (rc < 0) {
		dev_err(chip->dev, "update pattern TLRA failed, rc=%d\n", rc);
		return rc;
	}

	/* Configure pattern registers */
	for (i = 0; i < SAMPLES_PER_PATTERN; i++) {
		sample = &pattern->samples[i];
		values[i * 3] = sample->f_lra_x2 << PTRN_FLRA2X_SHIFT;
		values[i * 3] |= sample->period & PTRN_SAMPLE_PER_MASK;
		values[i * 3 + 1] =
			(sample->amplitude >> 8) & PTRN_AMP_MSB_MASK;
		values[i * 3 + 2] = sample->amplitude & PTRN_AMP_LSB_MASK;
	}

	rc = haptics_write(chip, chip->ptn_addr_base, ptn_cfg_addr,
			values, SAMPLES_PER_PATTERN * 3);
	if (rc < 0) {
		dev_err(chip->dev, "write pattern data failed, rc=%d\n", rc);
		return rc;
	}

	return 0;

}

static int haptics_update_memory_data(struct haptics_chip *chip,
					u8 *data, u32 length)
{
	int rc, count, i;
	u32 left;
	u8 tmp[HAP_PTN_FIFO_DIN_NUM] = {0};

	if (!length)
		return 0;

	count = length / HAP_PTN_FIFO_DIN_NUM;
	for (i = 0; i < count; i++) {
		rc = haptics_write(chip, chip->ptn_addr_base,
				HAP_PTN_FIFO_DIN_0_REG, data,
				HAP_PTN_FIFO_DIN_NUM);
		if (rc < 0) {
			dev_err(chip->dev, "bulk write FIFO_DIN failed, rc=%d\n",
					rc);
			return rc;
		}

		data += HAP_PTN_FIFO_DIN_NUM;
	}

	left = length % HAP_PTN_FIFO_DIN_NUM;
	if (left) {
		/*
		 * In HAP520 module, when 1-byte FIFO write clashes
		 * with the HW FIFO read operation, the HW will only read
		 * 1 valid byte in every 4 bytes FIFO samples. So avoid
		 * this by keeping the samples 4-byte aligned and always
		 * use 4-byte write for HAP520 module.
		 */
		if (chip->hw_type == HAP520) {
			memcpy(tmp, data, left);
			rc = haptics_write(chip, chip->ptn_addr_base,
					HAP_PTN_FIFO_DIN_0_REG, tmp,
					HAP_PTN_FIFO_DIN_NUM);
			if (rc < 0)
				dev_err(chip->dev, "write FIFO_DIN failed, rc=%d\n", rc);

			return rc;
		}

		for (i = 0; i < left; i++) {
			rc = haptics_write(chip, chip->ptn_addr_base,
					HAP_PTN_FIFO_DIN_1B_REG,
					(data + i), 1);
			if (rc < 0) {
				dev_err(chip->dev, "write FIFO_DIN_1B failed, rc=%d\n",
						rc);
				return rc;
			}
		}
	}

	return 0;
}

static int haptics_update_fifo_samples(struct haptics_chip *chip,
					u8 *samples, u32 length, bool refill)
{
	int rc;
	u8 val;

	if (!samples) {
		dev_err(chip->dev, "no data available to update FIFO\n");
		return -EINVAL;
	}

	if ((chip->hw_type >= HAP525_HV) && !refill) {
		val = MEM_FLUSH_RELOAD_BIT;
		rc = haptics_write(chip, chip->ptn_addr_base,
				HAP_PTN_MEM_OP_ACCESS_REG, &val, 1);
		if (rc < 0)
			return rc;

		val = 0;
		rc = haptics_write(chip, chip->ptn_addr_base,
				HAP_PTN_MEM_OP_ACCESS_REG, &val, 1);
		if (rc < 0)
			return rc;
	}

	return haptics_update_memory_data(chip, samples, length);
}

static int haptics_update_pat_mem_samples(struct haptics_chip *chip,
		u32 pat_sel, u8 *samples, u32 length)
{
	u8 val[4] = {0}, zero_samples[HAP_PTN_FIFO_DIN_NUM] = {0};
	u32 addr, size;
	int zero_padding, rc;

	if (!samples) {
		dev_err(chip->dev, "no data available to update PAT_MEM\n");
		return -EINVAL;
	}

	/* Haptics module revision HAP525_HV and above support PATx_MEM pattern source */
	if (chip->hw_type < HAP525_HV) {
		dev_dbg(chip->dev, "HW type %d doesn't support PATx_MEM pattern source\n",
				chip->hw_type);
		return 0;
	}

	/*
	 * For HAP530_HV module, start address and length for each partition
	 * need to be set before programming pattern memory
	 */
	if (chip->hw_type == HAP530_HV) {
		size = chip->mmap.pat_sel_mmap[pat_sel].length / HAP530_MMAP_PAT_LEN_PER_LSB;
		addr = chip->mmap.pat_sel_mmap[pat_sel].start_addr / HAP530_MMAP_PAT_LEN_PER_LSB;
		val[0] = addr & HAP_PTN_PATX_MEM_LEN_LO_MASK;
		val[1] = (addr >> HAP_PTN_PATX_MEM_LEN_HI_SHIFT) & HAP_PTN_PATX_MEM_LEN_HI_MASK;
		val[2] = size & HAP_PTN_PATX_MEM_LEN_LO_MASK;
		val[3] = (size >> HAP_PTN_PATX_MEM_LEN_HI_SHIFT) & HAP_PTN_PATX_MEM_LEN_HI_MASK;

		rc = haptics_write(chip, chip->ptn_addr_base,
				HAP_PTN_PATX_MEM_WR_START_ADDR_REG, val, 4);
		if (rc < 0)
			return rc;
	}

	val[0] = 0;
	if (chip->hw_type == HAP525_HV)
		val[0] = FIELD_PREP(MEM_PAT_RW_SEL_MASK, pat_sel);

	val[0] |= MEM_PAT_ACCESS_BIT | MEM_FLUSH_RELOAD_BIT;
	rc = haptics_write(chip, chip->ptn_addr_base,
			HAP_PTN_MEM_OP_ACCESS_REG, val, 1);
	if (rc < 0)
		return rc;

	val[0] &= ~MEM_FLUSH_RELOAD_BIT;
	rc = haptics_write(chip, chip->ptn_addr_base,
			HAP_PTN_MEM_OP_ACCESS_REG, val, 1);
	if (rc < 0)
		return rc;

	rc = haptics_update_memory_data(chip, samples, length);
	if (rc < 0)
		return rc;

	zero_padding = chip->mmap.pat_sel_mmap[pat_sel].length - length;
	while (zero_padding > 0) {
		rc = haptics_update_memory_data(chip, zero_samples,
				zero_padding > HAP_PTN_FIFO_DIN_NUM ?
				HAP_PTN_FIFO_DIN_NUM : zero_padding);
		if (rc < 0)
			return rc;

		zero_padding -= HAP_PTN_FIFO_DIN_NUM;
	}

	return haptics_masked_write(chip, chip->ptn_addr_base,
			HAP_PTN_MEM_OP_ACCESS_REG, MEM_PAT_ACCESS_BIT, 0);
}

static int haptics_get_fifo_fill_status(struct haptics_chip *chip, u32 *fill)
{
	int rc;
	u8 val[2], fill_status_mask;
	u32 filled, available;
	bool empty = false, full = false;

	rc = haptics_get_status_data(chip, FIFO_REAL_TIME_STS, val);
	if (rc < 0)
		return rc;

	switch (chip->hw_type) {
	case HAP520:
		fill_status_mask = FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V2;
		break;
	case HAP520_MV:
		fill_status_mask = FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V3;
		break;
	case HAP525_HV:
		fill_status_mask = FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V4;
		break;
	case HAP530_HV:
		fill_status_mask = FIFO_REAL_TIME_FILL_STATUS_MSB_MASK_V5;
		break;
	default:
		dev_err(chip->dev, "HW type %d is not supported\n",
				chip->hw_type);
		return -EINVAL;
	}

	filled = ((val[0] & fill_status_mask) << 8) | val[1];
	empty = !!(val[0] & FIFO_EMPTY_FLAG_BIT);
	full = !!(val[0] & FIFO_FULL_FLAG_BIT);
	available = get_max_fifo_samples(chip) - filled;

	dev_dbg(chip->dev, "filled=%u, available=%u, full=%d, empty=%d\n",
			filled, available, full, empty);
	trace_qcom_haptics_fifo_hw_status(filled, available, full, empty);
	*fill = filled;
	return 0;
}

static int haptics_get_available_fifo_memory(struct haptics_chip *chip)
{
	int rc;
	u32 fill, available;

	rc = haptics_get_fifo_fill_status(chip, &fill);
	if (rc < 0)
		return rc;

	if (fill > get_max_fifo_samples(chip)) {
		dev_err(chip->dev, "Filled FIFO number %d exceed the max %d\n",
				fill, get_max_fifo_samples(chip));
		return -EINVAL;
	} else if (fill == get_max_fifo_samples(chip)) {
		dev_err(chip->dev, "no FIFO space available\n");
		return -EBUSY;
	}

	available = get_max_fifo_samples(chip) - fill;
	return available;
}

static int haptics_set_manual_rc_clk_cal(struct haptics_chip *chip)
{
	int rc;
	u16 cal_count = chip->config.rc_clk_cal_count;
	u8 val[2];

	if (cal_count == 0) {
		dev_dbg(chip->dev, "Ignore setting RC_CLK_CAL_COUNT\n");
		return 0;
	}

	val[0] = (cal_count >> 8) & RC_CLK_CAL_COUNT_MSB_MASK;
	val[1] = cal_count & RC_CLK_CAL_COUNT_LSB_MASK;
	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_RC_CLK_CAL_COUNT_MSB_REG, val, 2);
	if (rc < 0)
		return rc;

	val[0] = CAL_RC_CLK_MANUAL_VAL << CAL_RC_CLK_SHIFT;
	return haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_CAL_EN_REG, CAL_RC_CLK_MASK,
			val[0]);
}

static int haptics_set_fifo_playrate(struct haptics_chip *chip,
				enum s_period period_per_s)
{
	int rc;

	rc = haptics_masked_write(chip, chip->ptn_addr_base,
			HAP_PTN_FIFO_PLAY_RATE_REG,
			FIFO_PLAY_RATE_MASK, period_per_s);
	if (rc < 0)
		dev_err(chip->dev, "Set FIFO play rate failed, rc=%d\n", rc);

	return rc;
}

static int haptics_set_fifo_empty_threshold(struct haptics_chip *chip,
							u32 thresh)
{
	u8 thresh_per_bit, thresh_mask;
	int rc;

	thresh_per_bit = chip->fifo_info->fifo_threshold_per_bit;
	thresh_mask = chip->fifo_info->fifo_empty_threshold_mask;
	rc = haptics_masked_write(chip, chip->ptn_addr_base,
			HAP_PTN_FIFO_EMPTY_CFG_REG,
			thresh_mask, (thresh / thresh_per_bit));
	if (rc < 0)
		dev_err(chip->dev, "Set FIFO empty threshold failed, rc=%d\n",
				rc);

	return rc;
}

static void haptics_fifo_empty_irq_config(struct haptics_chip *chip,
						bool enable)
{
	if (!chip->fifo_empty_irq_en && enable) {
		enable_irq(chip->fifo_empty_irq);
		chip->fifo_empty_irq_en = true;
	} else if (chip->fifo_empty_irq_en && !enable) {
		disable_irq_nosync(chip->fifo_empty_irq);
		chip->fifo_empty_irq_en = false;
	}
}

static int haptics_set_fifo(struct haptics_chip *chip, struct fifo_cfg *fifo)
{
	struct fifo_play_status *status = &chip->play.fifo_status;
	u32 num, fifo_thresh;
	int rc, available;

	if (atomic_read(&status->is_busy) == 1) {
		dev_err(chip->dev, "FIFO is busy\n");
		return -EBUSY;
	}

	/* Configure FIFO play rate */
	rc = haptics_set_fifo_playrate(chip, fifo->period_per_s);
	if (rc < 0)
		return rc;

	if (fifo->period_per_s >= F_8KHZ) {
		/* Set manual RC CLK CAL when playing FIFO */
		rc = haptics_set_manual_rc_clk_cal(chip);
		if (rc < 0)
			return rc;
	}

	atomic_set(&status->written_done, 0);
	atomic_set(&status->cancelled, 0);
	status->samples_written = 0;

	/*
	 * Write the 1st set of the data into FIFO if there are
	 * more than MAX_FIFO_SAMPLES samples, the rest will be
	 * written if any FIFO memory is available after playing.
	 */
	num = min_t(u32, fifo->num_s, get_max_fifo_samples(chip));
	available = haptics_get_available_fifo_memory(chip);
	if (available < 0)
		return available;

	num = min_t(u32, available, num);
	num = min_t(u32, num, FIFO_PRGM_INIT_SIZE);
	/* Keep the FIFO programming 4-byte aligned if FIFO refilling is needed */
	if ((num < fifo->num_s) && (num % HAP_PTN_FIFO_DIN_NUM))
		num = round_down(num, HAP_PTN_FIFO_DIN_NUM);

	rc = haptics_update_fifo_samples(chip, fifo->samples, num, false);
	if (rc < 0) {
		dev_err(chip->dev, "write FIFO samples failed, rc=%d\n", rc);
		return rc;
	}

	atomic_set(&status->is_busy, 1);
	status->samples_written = num;
	trace_qcom_haptics_fifo_prgm_status(fifo->num_s, status->samples_written, num);
	if (num == fifo->num_s) {
		fifo_thresh = 0;
		atomic_set(&status->written_done, 1);
	} else {
		fifo_thresh = chip->config.fifo_empty_thresh;
	}

	/*
	 * Set FIFO empty threshold here. FIFO empty IRQ will
	 * be enabled after playing FIFO samples so that more
	 * FIFO samples can be written (if available) when
	 * FIFO empty IRQ is triggered.
	 */
	rc = haptics_set_fifo_empty_threshold(chip, fifo_thresh);
	if (rc < 0)
		return rc;

	haptics_fifo_empty_irq_config(chip, true);

	return 0;
}

static int haptics_load_constant_effect(struct haptics_chip *chip, u8 amplitude)
{
	struct haptics_play_info *play = &chip->play;
	u32 hdrm_mv, vmax_mv = chip->config.vmax_mv;
	int rc = 0;

	mutex_lock(&chip->play.lock);
	if (chip->play.in_calibration) {
		dev_err(chip->dev, "calibration in progress, ignore playing constant effect\n");
		rc = -EBUSY;
		goto unlock;
	}

	/* No effect data when playing constant waveform */
	play->effect = NULL;

	/* Fix Vmax to (hpwr_vreg_mv - hdrm_mv) in non-HBOOST regulator case */
	if (is_haptics_external_powered(chip)) {
		rc = haptics_get_vmax_headroom_mv(chip, &hdrm_mv);
		if (rc < 0)
			goto unlock;

		vmax_mv = chip->hpwr_voltage_mv - hdrm_mv;
	}

	/* configure VMAX in case it was changed in previous effect playing */
	rc = haptics_set_vmax_mv(chip, vmax_mv);
	if (rc < 0)
		goto unlock;

	/* Config brake settings if it's necessary */
	play->brake = &chip->config.brake;
	if (play->brake) {
		rc = haptics_set_brake(chip, play->brake);
		if (rc < 0)
			goto unlock;
	}

	rc = haptics_set_direct_play(chip, amplitude);
	if (rc < 0)
		goto unlock;

	/* Always enable LRA auto resonance for DIRECT_PLAY */
	rc = haptics_enable_autores(chip, false);
	if (rc < 0)
		goto unlock;

	play->pattern_src = DIRECT_PLAY;
unlock:
	mutex_unlock(&chip->play.lock);
	return rc;
}

static int haptics_load_predefined_effect(struct haptics_chip *chip,
					struct haptics_effect *effect)
{
	struct haptics_play_info *play = &chip->play;
	struct mmap_partition *pat_sel_mmap;
	u32 addr, length;
	u8 val[4] = {0};
	int rc;

	if (effect == NULL)
		return -EINVAL;

	play->effect = effect;
	/* Clamp VMAX for different vibration strength */
	rc = haptics_set_vmax_mv(chip, play->vmax_mv);
	if (rc < 0)
		return rc;

	rc = haptics_enable_autores(chip, !play->effect->auto_res_disable);
	if (rc < 0)
		return rc;

	play->brake = play->effect->brake;
	/* Config brake settings if it's necessary */
	if (play->brake) {
		rc = haptics_set_brake(chip, play->brake);
		if (rc < 0)
			return rc;
	}

	play->pattern_src = play->effect->src;
	switch (play->pattern_src) {
	case PATTERN1:
	case PATTERN2:
		if (chip->hw_type >= HAP530_HV) {
			dev_dbg(chip->dev, "haptics module with revision %d no longer supports PATTERNx mode, ignore parsing pattern effect DT\n",
					chip->hw_type);
			return 0;
		}

		if (play->effect->pattern->preload) {
			dev_dbg(chip->dev, "Ignore preloaded PATTERNx effect: %d\n",
					play->effect->id);
			break;
		}

		rc = haptics_set_pattern(chip, play->effect->pattern,
						play->pattern_src);
		if (rc < 0)
			return rc;
		break;
	case FIFO:
		if (chip->wa_flags & TOGGLE_EN_TO_FLUSH_FIFO) {
			/* Toggle HAPTICS_EN for a clear start point of FIFO playing */
			rc = haptics_toggle_module_enable(chip);
			if (rc < 0)
				return rc;

			usleep_range(100, 101);
		}

		rc = haptics_set_fifo(chip, play->effect->fifo);
		if (rc < 0)
			return rc;
		break;
	case PATTERN_MEM:
		if (!chip->mmap.pat_sel_mmap)
			return 0;

		if (!play->effect->fifo->preload) {
			dev_err(chip->dev, "effect %d has PATTERN_MEM src but not preloaded\n",
					play->effect->id);
			return -EINVAL;
		}

		if (chip->hw_type == HAP530_HV) {
			/*
			 * Update length to 0 to avoid the end address
			 * calculation getting messed up if the previous
			 * pattern is still under playing.
			 */
			rc = haptics_write(chip, chip->ptn_addr_base,
					HAP_PTN_SPMI_PATX_MEM_LEN_LSB_REG, val, 2);
			if (rc < 0)
				return rc;

			pat_sel_mmap = &chip->mmap.pat_sel_mmap[effect->pat_sel];
			length = pat_sel_mmap->length /
					chip->mmap.hw_info.pat_mem_play_step;
			addr = pat_sel_mmap->start_addr / HAP530_MMAP_PAT_LEN_PER_LSB;
			val[0] = addr & HAP_PTN_PATX_MEM_LEN_LO_MASK;
			val[1] = (addr >> HAP_PTN_PATX_MEM_LEN_HI_SHIFT)
					& HAP_PTN_PATX_MEM_LEN_HI_MASK;
			val[2] = length & HAP_PTN_PATX_MEM_LEN_LO_MASK;
			val[3] = (length >> HAP_PTN_PATX_MEM_LEN_HI_SHIFT)
					& chip->mmap.hw_info.pat_mem_play_len_msb;

			rc = haptics_write(chip, chip->ptn_addr_base,
					HAP_PTN_SPMI_PATX_MEM_START_ADDR_REG, val, 4);
			if (rc < 0)
				return rc;
		}

		/* disable auto resonance for PATx_MEM mode */
		rc = haptics_enable_autores(chip, false);
		if (rc < 0)
			return rc;

		dev_dbg(chip->dev, "Ignore loading data for preload FIFO effect: %d\n",
				play->effect->id);
		break;
	case DIRECT_PLAY:
	case SWR:
	default:
		dev_err(chip->dev, "pattern src %d can't be used for predefined effect\n",
				play->pattern_src);
		return -EINVAL;
	}

	return 0;
}

static int haptics_init_custom_effect(struct haptics_chip *chip)
{
	chip->custom_effect = devm_kzalloc(chip->dev,
			sizeof(*chip->custom_effect), GFP_KERNEL);
	if (!chip->custom_effect)
		return -ENOMEM;

	chip->custom_effect->fifo = devm_kzalloc(chip->dev,
			sizeof(*chip->custom_effect->fifo), GFP_KERNEL);
	if (!chip->custom_effect->fifo)
		return -ENOMEM;

	/* custom effect will be played in FIFO mode without brake */
	chip->custom_effect->pattern = NULL;
	chip->custom_effect->brake = NULL;
	chip->custom_effect->id = UINT_MAX;
	chip->custom_effect->vmax_mv = chip->config.custom_vmax_mv;
	chip->custom_effect->t_lra_us = chip->config.t_lra_us;
	chip->custom_effect->src = FIFO;
	chip->custom_effect->auto_res_disable = true;

	return 0;
}

static int haptics_convert_sample_period(struct haptics_chip *chip,
						u32 play_rate_hz)
{
	enum s_period period;
	u32 f_lra, f_lra_min, f_lra_max;

	if (chip->config.t_lra_us == 0)
		return -EINVAL;

	f_lra = USEC_PER_SEC / chip->config.t_lra_us;
	if (f_lra == 0 || f_lra < F_LRA_VARIATION_HZ)
		return -EINVAL;

	f_lra_min = f_lra - F_LRA_VARIATION_HZ;
	f_lra_max = f_lra + F_LRA_VARIATION_HZ;

	if (play_rate_hz == 8000)
		period = F_8KHZ;
	else if (play_rate_hz == 16000)
		period = F_16KHZ;
	else if (play_rate_hz == 24000)
		period = F_24KHZ;
	else if (play_rate_hz == 32000)
		period = F_32KHZ;
	else if (play_rate_hz == 44100)
		period = F_44P1KHZ;
	else if (play_rate_hz == 48000)
		period = F_48KHZ;
	else if (is_between(play_rate_hz, f_lra_min, f_lra_max))
		period = T_LRA;
	else if (is_between(play_rate_hz / 2, f_lra_min, f_lra_max))
		period = T_LRA_DIV_2;
	else if (is_between(play_rate_hz / 4, f_lra_min, f_lra_max))
		period = T_LRA_DIV_4;
	else if (is_between(play_rate_hz / 8, f_lra_min, f_lra_max))
		period = T_LRA_DIV_8;
	else if (is_between(play_rate_hz * 2, f_lra_min, f_lra_max))
		period = T_LRA_X_2;
	else if (is_between(play_rate_hz * 4, f_lra_min, f_lra_max))
		period = T_LRA_X_4;
	else if (is_between(play_rate_hz * 8, f_lra_min, f_lra_max))
		period = T_LRA_X_8;
	else
		return -EINVAL;

	return period;
}

static int haptics_load_custom_effect(struct haptics_chip *chip,
			s16 __user *data, u32 length, s16 magnitude)
{
	struct haptics_play_info *play = &chip->play;
	struct custom_fifo_data custom_data = {};
	struct fifo_cfg *fifo;
	int rc;

	if (!chip->custom_effect || !chip->custom_effect->fifo)
		return -ENOMEM;

	fifo = chip->custom_effect->fifo;
	if (copy_from_user(&custom_data, data, sizeof(custom_data)))
		return -EFAULT;

	dev_dbg(chip->dev, "custom data length %d with play-rate %d Hz\n",
			custom_data.length, custom_data.play_rate_hz);
	rc = haptics_convert_sample_period(chip, custom_data.play_rate_hz);
	if (rc < 0) {
		dev_err(chip->dev, "Can't support play rate: %d Hz\n",
				custom_data.play_rate_hz);
		return rc;
	}

	mutex_lock(&chip->play.lock);
	fifo->period_per_s = rc;
	/*
	 * Before allocating samples buffer, free the old sample
	 * buffer first if it's not been freed.
	 */
	kvfree(fifo->samples);
	fifo->samples = kcalloc(custom_data.length, sizeof(u8), GFP_KERNEL);
	if (!fifo->samples) {
		fifo->samples = vmalloc(custom_data.length);
		if (!fifo->samples) {
			rc = -ENOMEM;
			goto unlock;
		}
	}

	if (copy_from_user(fifo->samples,
				(u8 __user *)custom_data.data,
				custom_data.length)) {
		rc = -EFAULT;
		goto cleanup;
	}

	dev_dbg(chip->dev, "Copy custom FIFO samples successfully\n");
	fifo->num_s = custom_data.length;
	fifo->play_length_us = get_fifo_play_length_us(fifo,
			chip->custom_effect->t_lra_us);

	if (chip->play.in_calibration) {
		dev_err(chip->dev, "calibration in progress, ignore playing custom effect\n");
		rc = -EBUSY;
		goto cleanup;
	}

	play->effect = chip->custom_effect;
	play->brake = NULL;
	play->vmax_mv = (magnitude * chip->custom_effect->vmax_mv) / 0x7fff;
	rc = haptics_set_vmax_mv(chip, play->vmax_mv);
	if (rc < 0)
		goto cleanup;

	/* Toggle HAPTICS_EN for a clear start point of FIFO playing */
	if (chip->wa_flags & TOGGLE_EN_TO_FLUSH_FIFO) {
		rc = haptics_toggle_module_enable(chip);
		if (rc < 0)
			goto cleanup;
	}

	rc = haptics_enable_autores(chip, !play->effect->auto_res_disable);
	if (rc < 0)
		goto cleanup;

	play->pattern_src = FIFO;
	rc = haptics_set_fifo(chip, play->effect->fifo);
	if (rc < 0)
		goto cleanup;

	mutex_unlock(&chip->play.lock);
	return 0;
cleanup:
	kvfree(fifo->samples);
	fifo->samples = NULL;
unlock:
	mutex_unlock(&chip->play.lock);
	return rc;
}

static u32 get_play_length_effect_us(struct haptics_effect *effect)
{
	u32 length_us = 0;

	if (effect->brake)
		length_us = effect->brake->play_length_us;

	if ((effect->src == PATTERN1 || effect->src == PATTERN2)
			&& effect->pattern)
		length_us += effect->pattern->play_length_us;
	else if ((effect->src == FIFO || effect->src == PATTERN_MEM) && effect->fifo)
		length_us += effect->fifo->play_length_us;

	return length_us;
}

static inline u32 get_play_length_us(struct haptics_play_info *play)
{
	return get_play_length_effect_us(play->effect);
}

#define PRIMITIVE_EFFECT_ID_BIT		BIT(15)
#define PRIMITIVE_EFFECT_ID_MASK	GENMASK(14, 0)
static int haptics_load_periodic_effect(struct haptics_chip *chip,
			s16 __user *data, u32 length, s16 magnitude)
{
	struct haptics_play_info *play = &chip->play;
	s16 custom_data[CUSTOM_DATA_LEN] = { 0 };
	struct haptics_effect *effects = NULL;
	s16 custom_id = 0;
	int effects_count = 0;
	int rc, i;
	bool primitive;

	if (copy_from_user(custom_data, data, sizeof(custom_data)))
		return -EFAULT;

	primitive = !!(custom_data[CUSTOM_DATA_EFFECT_IDX] & PRIMITIVE_EFFECT_ID_BIT);

	if (primitive) {
		if (chip->primitives_count == 0)
			return -EINVAL;

		custom_id = custom_data[CUSTOM_DATA_EFFECT_IDX] & PRIMITIVE_EFFECT_ID_MASK;
		effects = chip->primitives;
		effects_count = chip->primitives_count;
	} else {
		if (chip->effects_count == 0)
			return -EINVAL;

		custom_id = custom_data[CUSTOM_DATA_EFFECT_IDX];
		effects = chip->effects;
		effects_count = chip->effects_count;
	}

	for (i = 0; i < effects_count; i++)
		if (effects[i].id == custom_id)
			break;

	if (i == effects_count) {
		dev_err(chip->dev, "effect%d is not supported!\n",
				custom_data[CUSTOM_DATA_EFFECT_IDX]);
		return -EINVAL;
	}

	mutex_lock(&chip->play.lock);
	if (chip->play.in_calibration) {
		dev_err(chip->dev, "calibration in progress, ignore playing predefined effect\n");
		rc = -EBUSY;
		goto unlock;
	}

	play->vmax_mv = (magnitude * effects[i].vmax_mv) / 0x7fff;
	dev_dbg(chip->dev, "upload %s effect %d, vmax=%d\n", primitive ? "primitive" : "predefined",
			effects[i].id, play->vmax_mv);

	rc = haptics_load_predefined_effect(chip, &effects[i]);
	if (rc < 0) {
		dev_err(chip->dev, "Play predefined effect%d failed, rc=%d\n",
				effects[i].id, rc);
		goto unlock;
	}
	mutex_unlock(&chip->play.lock);

	play->length_us = get_play_length_us(play);
	custom_data[CUSTOM_DATA_TIMEOUT_SEC_IDX] =
		play->length_us / USEC_PER_SEC;
	custom_data[CUSTOM_DATA_TIMEOUT_MSEC_IDX] =
		(play->length_us % USEC_PER_SEC) / USEC_PER_MSEC;

	if (copy_to_user(data, custom_data, length))
		return -EFAULT;

	return 0;
unlock:
	mutex_unlock(&chip->play.lock);
	return rc;
}

static u8 get_direct_play_max_amplitude(struct haptics_chip *chip)
{
	u32 amplitude = DIRECT_PLAY_MAX_AMPLITUDE, hdrm_mv;
	int rc;

	if (is_haptics_external_powered(chip)) {
		rc = haptics_get_vmax_headroom_mv(chip, &hdrm_mv);
		if (rc < 0)
			return 0;

		amplitude *= chip->config.vmax_mv;
		amplitude /= (chip->hpwr_voltage_mv - hdrm_mv);
		if (amplitude > DIRECT_PLAY_MAX_AMPLITUDE)
			amplitude = DIRECT_PLAY_MAX_AMPLITUDE;
	}

	dev_dbg(chip->dev, "max amplitude for direct play: %#x\n", amplitude);
	return (u8)amplitude;
}

static int haptics_stop_fifo_play(struct haptics_chip *chip)
{
	int rc;
	u8 val;

	if (atomic_read(&chip->play.fifo_status.is_busy) == 0) {
		dev_dbg(chip->dev, "FIFO playing is not in progress\n");
		return 0;
	}

	rc = haptics_enable_play(chip, false);
	if (rc < 0)
		return rc;

	/* restore FIFO play rate back to T_LRA */
	rc = haptics_set_fifo_playrate(chip, T_LRA);
	if (rc < 0)
		return rc;

	haptics_fifo_empty_irq_config(chip, false);
	kvfree(chip->custom_effect->fifo->samples);
	chip->custom_effect->fifo->samples = NULL;

	atomic_set(&chip->play.fifo_status.is_busy, 0);

	/*
	 * All other playing modes would use AUTO mode RC
	 * calibration except FIFO streaming mode, so restore
	 * back to AUTO RC calibration after FIFO playing.
	 */
	val = CAL_RC_CLK_AUTO_VAL << CAL_RC_CLK_SHIFT;
	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_CAL_EN_REG, CAL_RC_CLK_MASK, val);
	if (rc < 0)
		return rc;

	dev_dbg(chip->dev, "stopped FIFO playing successfully\n");
	return 0;
}

static void haptics_stop_constant_effect_play(struct work_struct *work)
{
	struct haptics_chip *chip = container_of(work, struct haptics_chip, stop_work.work);
	int rc = 0;

	rc = haptics_enable_play(chip, false);
	if (rc < 0)
		dev_err(chip->dev, "stop constant effect play failed\n");

	rc = haptics_enable_hpwr_vreg(chip, false);
	if (rc < 0)
		dev_err(chip->dev, "disable hpwr_vreg failed\n");
}

static inline bool is_haptics_runtime_pm_enabled(struct haptics_chip *chip)
{
	return !!(chip->wa_flags & EN_RUNTIME_PM);
}

#define HAPTICS_AUTO_SUSPEND_MS			1000

static void haptics_runtime_disable_action(void *data)
{
	pm_runtime_dont_use_autosuspend(data);
	pm_runtime_disable(data);
}

static int haptics_runtime_pm_init(struct haptics_chip *chip)
{
	int rc;

	if (!is_haptics_runtime_pm_enabled(chip))
		return 0;

	pm_runtime_use_autosuspend(chip->dev);
	pm_runtime_set_autosuspend_delay(chip->dev, HAPTICS_AUTO_SUSPEND_MS);
	pm_runtime_set_active(chip->dev);
	pm_runtime_enable(chip->dev);

	rc = __devm_add_action(chip->dev, haptics_runtime_disable_action,
			chip->dev, "haptics_runtime_disable_action");
	if (rc < 0)
		haptics_runtime_disable_action(chip->dev);

	return rc;
}

static int haptics_runtime_resume_get(struct haptics_chip *chip)
{
	if (!is_haptics_runtime_pm_enabled(chip))
		return 0;

	return pm_runtime_resume_and_get(chip->dev);
}

static void haptics_runtime_autosuspend_put(struct haptics_chip *chip)
{
	if (!is_haptics_runtime_pm_enabled(chip))
		return;

	pm_runtime_mark_last_busy(chip->dev);
	pm_runtime_put_autosuspend(chip->dev);
}

static void haptics_runtime_autosuspend(struct haptics_chip *chip)
{
	if (!is_haptics_runtime_pm_enabled(chip))
		return;

	pm_runtime_mark_last_busy(chip->dev);
	pm_runtime_autosuspend(chip->dev);
}

static int haptics_upload_effect(struct input_dev *dev,
		struct ff_effect *effect, struct ff_effect *old)
{
	struct haptics_chip *chip = input_get_drvdata(dev);
	u32 length_ms, tmp;
	s16 level;
	u8 amplitude;
	int rc = 0;

	rc = haptics_runtime_resume_get(chip);
	if (rc < 0)
		return rc;

	/*
	 * When switching between SWR and SPMI play, the haptics module doesn't
	 * set HPWR_DISABLED status especially if triggering a SPMI play before
	 * SWR play is not de-asserted. This triggers haptics enable toggling
	 * workaround trying to recover the HW in multiple places and it is
	 * unexpected.
	 *
	 * To avoid this, set SWR_IGNORE before triggering a SPMI play, and
	 * unset SWR_IGNORE after the SPMI play is stopped.
	 */
	if (chip->wa_flags & IGNORE_SWR_IN_SPMI_PLAY) {
		rc = haptics_ignore_swr_play(chip, true);
		if (rc < 0)
			goto auto_suspend;
	}

	switch (effect->type) {
	case FF_CONSTANT:
		length_ms = effect->replay.length;
		level = effect->u.constant.level;
		tmp = get_direct_play_max_amplitude(chip);
		tmp *= level;
		amplitude = tmp / 0x7fff;
		dev_dbg(chip->dev, "upload constant effect, length = %dms, amplitude = %#x\n",
				length_ms, amplitude);
		schedule_delayed_work(&chip->stop_work, msecs_to_jiffies(length_ms));
		haptics_load_constant_effect(chip, amplitude);
		if (rc < 0) {
			dev_err(chip->dev, "set direct play failed, rc=%d\n",
					rc);
			goto restore;
		}

		break;
	case FF_PERIODIC:
		if (effect->u.periodic.waveform != FF_CUSTOM) {
			dev_err(chip->dev, "Only support custom waveforms\n");
			rc = -EINVAL;
			goto restore;
		}

		if (effect->u.periodic.custom_len ==
				sizeof(struct custom_fifo_data)) {
			rc = haptics_load_custom_effect(chip,
					effect->u.periodic.custom_data,
					effect->u.periodic.custom_len,
					effect->u.periodic.magnitude);
			if (rc < 0) {
				dev_err(chip->dev, "Upload custom FIFO data failed rc=%d\n",
						rc);
				goto restore;
			}
		} else if (effect->u.periodic.custom_len ==
				sizeof(s16) * CUSTOM_DATA_LEN) {
			rc = haptics_load_periodic_effect(chip,
					effect->u.periodic.custom_data,
					effect->u.periodic.custom_len,
					effect->u.periodic.magnitude);
			if (rc < 0) {
				dev_err(chip->dev, "Upload periodic effect failed rc=%d\n",
						rc);
				goto restore;
			}
		}

		break;
	default:
		dev_err(chip->dev, "%d effect is not supported\n",
				effect->type);
		rc = -EINVAL;
		goto restore;
	}

	rc = haptics_enable_hpwr_vreg(chip, true);
	if (rc < 0) {
		dev_err(chip->dev, "enable hpwr_vreg failed, rc=%d\n", rc);
		goto restore;
	}

	rc = haptics_wait_hboost_ready(chip);
	if (rc < 0 && chip->play.pattern_src == FIFO) {
		/*
		 * Call haptics_stop_fifo_play(chip) explicitly if hBoost is
		 * not ready for the FIFO play. This drops current FIFO play
		 * but it restores the SW back to initial status so that the
		 * following FIFO play requests can still be served.
		 */
		dev_dbg(chip->dev, "stop FIFO play explicitly to restore SW status\n");
		mutex_lock(&chip->play.lock);
		haptics_stop_fifo_play(chip);
		mutex_unlock(&chip->play.lock);
		goto restore;
	}

	return 0;

restore:
	if (chip->wa_flags & IGNORE_SWR_IN_SPMI_PLAY)
		haptics_ignore_swr_play(chip, false);
auto_suspend:
	haptics_runtime_autosuspend_put(chip);
	return rc;
}

static int haptics_playback(struct input_dev *dev, int effect_id, int val)
{
	struct haptics_chip *chip = input_get_drvdata(dev);

	dev_dbg(chip->dev, "playback val = %d\n", val);
	if (!!val) {
		if (chip->config.measure_lra_impedance && chip->visense_enabled)
			chip->pattern_start_time = ktime_get_boottime();

		return haptics_enable_play(chip, true);
	}

	return 0;
}

#define AVG_RSENSE_MEAS_PER_CYCLE 4
static int haptics_measure_visense_lra_impedance(struct haptics_chip *chip, int pattern_run_time)
{
	u32 avg_rsense_meas, icomp_thresh_na, r_typical_ohm, vmax_mv;
	u32 i_peak_ma, rsense_mohm, v_peak_mv;
	u32 play_rsense_meas;
	u8 val[2];
	int rc;

	rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_ICOMP_AVG_CTL1_REG, val, 1);
	if (rc < 0)
		return rc;
	icomp_thresh_na = (val[0] & ICOMP_THRESH_MASK) * ICOMP_THRESH_STEP_NA;

	rc = haptics_get_lra_nominal_impedance(chip, &r_typical_ohm);
	if (rc < 0)
		return rc;

	rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_VMAX_REG, val, 1);
	if (rc < 0)
		return rc;
	vmax_mv = val[0] * ((chip->is_hv_haptics) ? VMAX_HV_STEP_MV : VMAX_MV_STEP_MV);

	rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_ICOMP_AVG_CTL2_REG, val, 1);
	if (rc < 0)
		return rc;
	avg_rsense_meas = 1 << val[0];

	play_rsense_meas = (pattern_run_time * AVG_RSENSE_MEAS_PER_CYCLE) / chip->config.t_lra_us;

	/*
	 * If vmax is higher than ICOMP_THRESH * R_typical and play length is
	 * longer than #N of measurement time use rsense directly otherwise use
	 * v_peak and i_peak to calculate rsense
	 */
	if ((u64)vmax_mv * 1000000 > (u64)icomp_thresh_na * r_typical_ohm &&
	    play_rsense_meas >= avg_rsense_meas) {
		/* LRA Resistance = RSENSE_MEAS * 0.025 Ohms */
		rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_RSENSE_MEAS_LSB_REG, val, 2);
		if (rc < 0)
			return rc;
		rsense_mohm = ((val[1] & HAP_CFG_RSENSE_MEAS_MSB_MASK) << 8) | val[0];

		dev_dbg(chip->dev, "measured rsense: %#x mohm\n", rsense_mohm);

		chip->config.lra_measured_mohms = rsense_mohm * LRA_IMPEDANCE_VISENSE_MOHMS;
	} else {
		/* Measured Voltage Peak = VSENSE_MEAS * 50mV/2^6 */
		rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_VSENSE_PEAK_LSB_REG, val, 2);
		if (rc < 0)
			return rc;
		v_peak_mv = ((val[1] & HAP_CFG_VSENSE_PEAK_MSB_MASK) << 8) | val[0];
		v_peak_mv = (v_peak_mv * 50) / 64;

		/* Measured Current Peak = ISENSE_MEAS * 1A/2^12 */
		rc = haptics_read(chip, chip->cfg_addr_base, HAP_CFG_ISENSE_PEAK_LSB_REG, val, 2);
		if (rc < 0)
			return rc;
		i_peak_ma = ((val[1] & HAP_CFG_ISENSE_PEAK_MSB_MASK) << 8) | val[0];
		i_peak_ma = (i_peak_ma * 1000) / 4096;

		dev_dbg(chip->dev, "measured voltage peak: %#x mv, current peak = %#x ma\n",
			v_peak_mv, i_peak_ma);

		/* LRA Resistance = VPEAK / IPEAK */
		chip->config.lra_measured_mohms = (v_peak_mv * 1000) / i_peak_ma;
	}

	dev_dbg(chip->dev, "measured LRA impedance: %u mohm\n", chip->config.lra_measured_mohms);
	return 0;
}

static int haptics_erase(struct input_dev *dev, int effect_id)
{
	struct haptics_chip *chip = input_get_drvdata(dev);
	struct haptics_play_info *play = &chip->play;
	ktime_t pattern_run_time;
	int rc;

	dev_dbg(chip->dev, "erase effect, really stop play\n");
	cancel_work_sync(&chip->set_gain_work);
	mutex_lock(&play->lock);
	cancel_delayed_work_sync(&chip->stop_work);
	pattern_run_time = ktime_us_delta(ktime_get_boottime(), chip->pattern_start_time);
	if ((play->pattern_src == FIFO) &&
			atomic_read(&play->fifo_status.is_busy)) {
		if (atomic_read(&play->fifo_status.written_done) == 0) {
			dev_dbg(chip->dev, "cancelling FIFO playing\n");
			atomic_set(&play->fifo_status.cancelled, 1);
		}

		rc = haptics_stop_fifo_play(chip);
		if (rc < 0) {
			dev_err(chip->dev, "stop FIFO playing failed, rc=%d\n",
					rc);
			mutex_unlock(&play->lock);
			goto restore;
		}
	} else {
		rc = haptics_enable_play(chip, false);
		if (rc < 0) {
			dev_err(chip->dev, "stop play failed, rc=%d\n", rc);
			mutex_unlock(&play->lock);
			goto restore;
		}
	}
	mutex_unlock(&play->lock);

	rc = haptics_enable_hpwr_vreg(chip, false);
	if (rc < 0)
		dev_err(chip->dev, "disable hpwr_vreg failed, rc=%d\n", rc);

	if (chip->config.measure_lra_impedance && chip->visense_enabled) {
		rc = haptics_measure_visense_lra_impedance(chip, pattern_run_time);
		if (rc < 0)
			dev_err(chip->dev, "visense lra impedance measurement failed, rc=%d\n", rc);
	}

restore:
	/* Restore Vmax headroom to 1.5V after SPMI play is done */
	if (chip->wa_flags & RESTORE_VMAX_HDRM_1P5V) {
		rc = haptics_set_vmax_headroom_mv(chip, VMAX_HDRM_MV_DEFAULT);
		if (rc)
			return rc;
	}

	/* Restore SWR play mode after SPMI play is done or any faults */
	if (chip->wa_flags & IGNORE_SWR_IN_SPMI_PLAY)
		haptics_ignore_swr_play(chip, false);

	haptics_runtime_autosuspend_put(chip);
	return rc;
}

static void haptics_set_gain_work(struct work_struct *work)
{
	struct haptics_chip *chip =
		container_of(work, struct haptics_chip, set_gain_work);
	struct haptics_hw_config *config = &chip->config;
	struct haptics_play_info *play = &chip->play;
	u32 vmax_mv, amplitude;
	u16 gain;

	mutex_lock(&play->lock);
	gain = atomic_read(&play->gain);
	/* scale amplitude when playing in DIRECT_PLAY mode */
	if (chip->play.pattern_src == DIRECT_PLAY) {
		amplitude = get_direct_play_max_amplitude(chip);
		amplitude *= gain;
		amplitude /= 0x7fff;

		dev_dbg(chip->dev, "Set amplitude: %#x\n", amplitude);
		haptics_set_direct_play(chip, (u8)amplitude);
		mutex_unlock(&play->lock);
		return;
	}

	/* scale Vmax when playing in other modes */
	vmax_mv = config->vmax_mv;
	if (play->effect)
		vmax_mv = play->effect->vmax_mv;

	if (chip->clamp_at_5v && (vmax_mv > CLAMPED_VMAX_MV))
		vmax_mv = CLAMPED_VMAX_MV;

	play->vmax_mv = ((u32)(gain * vmax_mv)) / 0x7fff;
	haptics_set_vmax_mv(chip, play->vmax_mv);
	mutex_unlock(&play->lock);
}

static void haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct haptics_chip *chip = input_get_drvdata(dev);
	struct haptics_play_info *play = &chip->play;

	if (gain == 0)
		return;

	if (gain > 0x7fff)
		gain = 0x7fff;

	atomic_set(&play->gain, gain);
	schedule_work(&chip->set_gain_work);
	dev_dbg(chip->dev, "Set gain: %#x\n", gain);
}

static int haptics_store_cl_brake_settings(struct haptics_chip *chip)
{
	int rc = 0;
	u8 val[2];

	if (!chip->cl_brake_nvmem)
		return 0;

	rc = haptics_get_status_data(chip, RNAT_RCAL_INT, val);
	if (rc < 0)
		return rc;

	rc = nvmem_cell_write(chip->cl_brake_nvmem, &val[1], 1);
	if (rc < 0)
		dev_err(chip->dev, "store RNAT/RCAL to SDAM failed, rc=%d\n", rc);

	return rc;
}

static int haptics_config_openloop_lra_period(struct haptics_chip *chip,
							u32 t_lra_us)
{
	u32 tmp;
	u8 val[2];
	int rc;

	rc = haptics_adjust_lra_period(chip, &t_lra_us);
	if (rc < 0)
		return rc;

	tmp = t_lra_us / TLRA_STEP_US;
	val[0] = (tmp >> 8) & TLRA_OL_MSB_MASK;
	val[1] = tmp & TLRA_OL_LSB_MASK;

	return haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_TLRA_OL_HIGH_REG, val, 2);
}

static int haptics_config_preload_fifo_effect(struct haptics_chip *chip,
				struct haptics_effect *effect)
{
	int rc;

	if (!effect->fifo->preload) {
		dev_err(chip->dev, "effect %d doesn't support preload\n",
				effect->id);
		return -EINVAL;
	}

	if (effect->src != PATTERN_MEM) {
		dev_err(chip->dev, "effect %d pattern_src is not PATTERN_MEM\n",
				effect->id);
		return -EINVAL;
	}

	rc = haptics_update_pat_mem_samples(chip, effect->pat_sel,
			effect->fifo->samples, effect->fifo->num_s);
	if (!rc)
		dev_dbg(chip->dev, "effect %d is preloaded in PAT_MEM %d\n",
				effect->id, effect->pat_sel);
	return rc;
}

static int haptics_mmap_config(struct haptics_chip *chip)
{
	int rc, i, left;
	u32 empty_thresh_allowed;
	u8 val[4];

	/* config PATx_PLAY_RATE */
	rc = haptics_masked_write(chip, chip->ptn_addr_base,
			HAP_PTN_FIFO_PLAY_RATE_REG, PAT_MEM_PLAY_RATE_MASK,
			FIELD_PREP(PAT_MEM_PLAY_RATE_MASK, chip->mmap.pat_play_rate));
	if (rc < 0) {
		dev_err(chip->dev, "Set pat_mem play rate failed, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Make the FIFO memory with step size aligned, and append
	 * the leftover memory bytes to PAT1_MEM.
	 */
	left = chip->mmap.fifo_mmap.length % chip->mmap.hw_info.fifo_mem_step;
	if (left) {
		chip->mmap.fifo_mmap.length -= left;
		chip->mmap.pat_sel_mmap[0].length += left;
		chip->mmap.pat_sel_mmap[0].start_addr = chip->mmap.fifo_mmap.length;
		dev_dbg(chip->dev, "PAT_MEM partition 0 is updated: start_addr %#x, length %#x\n",
				chip->mmap.pat_sel_mmap[0].start_addr,
				chip->mmap.pat_sel_mmap[0].length);
	}

	empty_thresh_allowed = chip->mmap.fifo_mmap.length *
		FIFO_EMPTY_THRESH_RATIO_NUM / FIFO_EMPTY_THRESH_RATIO_DEN;
	if (chip->config.fifo_empty_thresh > empty_thresh_allowed) {
		chip->config.fifo_empty_thresh = empty_thresh_allowed;
		dev_dbg(chip->dev, "Update FIFO empty threshold after MMAP allocation\n");
	}

	dev_dbg(chip->dev, "FIFO memory length: %d, empty threshold: %d\n",
			chip->mmap.fifo_mmap.length, chip->config.fifo_empty_thresh);
	/* config MMAP_FIFO */
	val[0] = chip->mmap.fifo_mmap.length / chip->mmap.hw_info.fifo_mem_step;
	if (!val[0]--) {
		dev_err(chip->dev, "fifo length %d is less than %d\n",
				chip->mmap.fifo_mmap.length, MMAP_FIFO_LEN_PER_LSB);
		return -EINVAL;
	}

	val[0] &= chip->mmap.hw_info.fifo_mem_mask;
	val[0] |= MMAP_FIFO_EXIST_BIT;
	rc = haptics_write(chip, chip->ptn_addr_base,
			HAP_PTN_MMAP_FIFO_REG, val, 1);
	if (rc < 0)
		return rc;

	/*
	 * Explicit HW indexed PATx_MEM partition is only supported in HAP525_HV
	 * and it's deprecated in HAP530_HV.
	 */
	if (chip->hw_type == HAP525_HV) {
		/* config MMAP_PAT1/2/3/4 */
		for (i = PAT1_MEM; i <= PAT4_MEM; i++)
			val[i] = min(chip->mmap.pat_sel_mmap[i].length,
				   chip->mmap.pat_sel_mmap[i].max_size) / MMAP_PAT_LEN_PER_LSB;

		rc = haptics_write(chip, chip->ptn_addr_base,
				HAP_PTN_MMAP_PAT1_REG, val, 4);
		if (rc < 0)
			return rc;

		dev_dbg(chip->dev, "haptics memory map configured with FIFO: %d, PAT1:%d, PAT2: %d, PAT3: %d, PAT4: %d\n",
				chip->mmap.fifo_mmap.length,
				chip->mmap.pat_sel_mmap[PAT1_MEM].length,
				chip->mmap.pat_sel_mmap[PAT2_MEM].length,
				chip->mmap.pat_sel_mmap[PAT3_MEM].length,
				chip->mmap.pat_sel_mmap[PAT4_MEM].length);
	}

	return 0;
}

static int haptics_mmap_preload_fifo_effect(struct haptics_chip *chip,
				struct haptics_effect *effect)
{
	u32 length, fifo_length;
	u32 end_addr, max_size;
	int i, num_pat;

	if (!effect->fifo->preload) {
		dev_err(chip->dev, "effect %d doesn't support preload\n",
			effect->id);
		return -EINVAL;
	}

	if (!chip->mmap.pat_sel_mmap)
		return 0;

	if (chip->mmap.pat_play_rate == F_RESERVED) {
		chip->mmap.pat_play_rate = effect->fifo->period_per_s;
	} else if (chip->mmap.pat_play_rate != effect->fifo->period_per_s) {
		dev_warn(chip->dev, "PATx_MEM sources only support one play rate\n");
		goto mmap_failed;
	}

	length = roundup(effect->fifo->num_s, chip->mmap.hw_info.pat_mem_step);
	fifo_length = chip->mmap.fifo_mmap.length - length;
	if (fifo_length < MMAP_FIFO_MIN_SIZE) {
		dev_warn(chip->dev, "not enough space for MMAP effect ID: %d, length: %d\n",
				effect->id, length);
		goto mmap_failed;
	}

	if (chip->hw_type == HAP525_HV)
		num_pat = PAT_MEM_MAX - 1;
	else
		num_pat = chip->mmap_effect_count - 1;

	for (i = num_pat; i >= 0; i--) {
		if (!chip->mmap.pat_sel_mmap[i].in_use &&
				(length <= chip->mmap.pat_sel_mmap[i].max_size))
			break;
	}

	if (i < 0) {
		dev_warn(chip->dev, "no PAT_MEM source available for MMAP effect ID: %d, length: %d\n",
				effect->id, length);
		effect->fifo->preload = false;
		goto mmap_failed;
	}

	/*
	 * The end address of the last partition is the end address of the memory
	 * space. For other partitions, the end address is prior to the start
	 * address of the next partition.
	 */
	if (i == num_pat)
		end_addr = chip->mmap.hw_info.memory_size - 1;
	else
		end_addr = chip->mmap.pat_sel_mmap[i + 1].start_addr - 1;

	/* update the mmap configuration */
	chip->mmap.pat_sel_mmap[i].in_use = true;
	chip->mmap.pat_sel_mmap[i].length = length;
	chip->mmap.pat_sel_mmap[i].start_addr = end_addr - length + 1;
	chip->mmap.fifo_mmap.length = fifo_length;

	/* Update the effect pattern_src and pat_sel for the preload effect */
	effect->src = PATTERN_MEM;
	effect->pat_sel = i;

	if (chip->hw_type == HAP525_HV)
		return 0;

	/* Update max size for the preceding partition */
	if (i > 0) {
		max_size = chip->mmap.pat_sel_mmap[i].max_size - length;
		chip->mmap.pat_sel_mmap[i - 1].max_size = max_size;
	}

	dev_dbg(chip->dev, "PAT_MEM partition %d: start_addr %#x, length %#x\n", i,
			chip->mmap.pat_sel_mmap[i].start_addr,
			chip->mmap.pat_sel_mmap[i].length);
	return 0;

	/* if mmap is failed then the effect couldn't be preloaded */
mmap_failed:
	effect->fifo->preload = false;
	return -ENOSPC;
}

static void haptics_mmap_init(struct haptics_chip *chip)
{
	u32 max_pat_mem_size;

	if (!chip->mmap.pat_sel_mmap)
		return;

	max_pat_mem_size = chip->mmap.hw_info.memory_size - MMAP_FIFO_MIN_SIZE;
	/* Assume that all memory space is used for FIFO mode by default */
	chip->mmap.fifo_mmap.in_use = true;
	chip->mmap.fifo_mmap.length = chip->mmap.hw_info.memory_size;
	chip->mmap.fifo_mmap.max_size = chip->mmap.hw_info.memory_size;

	chip->mmap.pat_play_rate = F_RESERVED;
	if (chip->hw_type != HAP525_HV) {
		/*
		 * To keep the mmap allocation logic backward compatible, the
		 * allocation will be started from the last partition with highest
		 * address. Thus, update the max_size of the last partition to
		 * the total size. The max_size of the following partitions
		 * will be updated according to the bytes in the available space.
		 */
		chip->mmap.pat_sel_mmap[chip->mmap_effect_count - 1].max_size = max_pat_mem_size;
		return;
	}

	/* In HAP525_HV, different PATx_MEM partition has different maximum size */
	chip->mmap.pat_sel_mmap[PAT1_MEM].max_size = min(max_pat_mem_size,
			(u32)MMAP_PAT1_LEN_MASK * MMAP_PAT_LEN_PER_LSB);
	chip->mmap.pat_sel_mmap[PAT2_MEM].max_size = min(max_pat_mem_size,
			(u32)MMAP_PAT2_LEN_MASK * MMAP_PAT_LEN_PER_LSB);
	chip->mmap.pat_sel_mmap[PAT3_MEM].max_size = min(max_pat_mem_size,
			(u32)MMAP_PAT3_PAT4_LEN_MASK * MMAP_PAT_LEN_PER_LSB);
	chip->mmap.pat_sel_mmap[PAT4_MEM].max_size = min(max_pat_mem_size,
			(u32)MMAP_PAT3_PAT4_LEN_MASK * MMAP_PAT_LEN_PER_LSB);
}

static int haptics_init_fifo_memory(struct haptics_chip *chip)
{
	struct haptics_effect *effect;
	struct mmap_partition *partitions;
	int counts;
	int rc;
	u8 val;

	/* HAP525_HV haptics module and above support PATx_MEM pattern source */
	if (chip->hw_type < HAP525_HV) {
		dev_dbg(chip->dev, "HW type %d doesn't support mmap\n",
				chip->hw_type);
		return 0;
	}

	if (chip->hw_type == HAP525_HV) {
		counts = PAT_MEM_MAX;
		chip->mmap.hw_info.memory_size = MMAP_NUM_BYTES;
		chip->mmap.hw_info.pat_mem_step = MMAP_PAT_LEN_PER_LSB;
		chip->mmap.hw_info.fifo_mem_step = MMAP_FIFO_LEN_PER_LSB;
		chip->mmap.hw_info.fifo_mem_mask = MMAP_FIFO_LEN_MASK;
	} else {
		counts = chip->mmap_effect_count;
		chip->mmap.hw_info.memory_size = HAP530_MMAP_NUM_BYTES;
		chip->mmap.hw_info.pat_mem_step = HAP530_MMAP_PAT_LEN_PER_LSB;
		chip->mmap.hw_info.pat_mem_play_step = HAP530_MMAP_PAT_LEN_PER_LSB;
		chip->mmap.hw_info.pat_mem_play_len_msb = HAP_PTN_PATX_MEM_LEN_HI_MASK;
		chip->mmap.hw_info.fifo_mem_step = HAP530_MMAP_FIFO_LEN_PER_LSB;
		chip->mmap.hw_info.fifo_mem_mask = HAP530_MMAP_FIFO_LEN_MASK;

		if (chip->ptn_revision >= HAP_PTN_PATX_PLAY_CFG_SUPPORT_MIN_REV) {
			val = HAP530_V2_MMAP_PAT_LEN_PER_LSB;
			rc = haptics_write(chip, chip->ptn_addr_base,
					HAP_PTN_PATX_PLAY_CFG, &val, 1);
			if (rc < 0)
				return rc;
			chip->mmap.hw_info.pat_mem_play_step =
					HAP530_V2_MMAP_PAT_LEN_PER_LSB;
			chip->mmap.hw_info.pat_mem_play_len_msb =
					HAP530_V2_PTN_PATX_MEM_LEN_HI_MASK;
		}
	}

	if (!counts) {
		dev_dbg(chip->dev, "no PAT_MEM effect is defined, skip MMAP config\n");
		return 0;
	}

	partitions = devm_kcalloc(chip->dev, sizeof(*partitions), counts, GFP_KERNEL);
	if (!partitions)
		return -ENOMEM;

	chip->mmap.pat_sel_mmap = partitions;
	haptics_mmap_init(chip);

	list_for_each_entry(effect, &chip->mmap_effect_list, node) {
		rc = haptics_mmap_preload_fifo_effect(chip, effect);
		if (rc < 0 && rc != -ENOSPC)
			return rc;
	}

	rc = haptics_mmap_config(chip);
	if (rc < 0)
		return rc;

	list_for_each_entry(effect, &chip->mmap_effect_list, node) {
		rc = haptics_config_preload_fifo_effect(chip, effect);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int haptics_init_preload_pattern_effect(struct haptics_chip *chip)
{
	struct haptics_hw_config *config = &chip->config;
	struct haptics_effect *effect;
	int i;

	if (config->preload_effect == -EINVAL)
		return 0;

	for (i = 0; i < chip->effects_count; i++)
		if (chip->effects[i].id == config->preload_effect)
			break;

	if (i == chip->effects_count) {
		dev_err(chip->dev, "preload effect %d is not found\n",
				config->preload_effect);
		return -EINVAL;
	}

	effect = &chip->effects[i];
	return haptics_set_pattern(chip, effect->pattern, effect->src);
}

static int haptics_detect_lra_frequency(struct haptics_chip *chip, bool in_boot);

static int haptics_init_lra_period_config(struct haptics_chip *chip)
{
	int rc = 0;
	u8 val;
	// u32 t_lra_us;

	/* set AUTO_mode RC CLK calibration by default */
	val = FIELD_PREP(CAL_RC_CLK_MASK, CAL_RC_CLK_AUTO_VAL);
	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_CAL_EN_REG, CAL_RC_CLK_MASK, val);
	if (rc < 0)
		return rc;

	/* get calibrated close loop period */
	// t_lra_us = chip->config.t_lra_us;
	// rc = haptics_get_closeloop_lra_period(chip, true);
	// if (!rc && chip->config.cl_t_lra_us != 0)
	// 	t_lra_us = chip->config.cl_t_lra_us;
	// else
	// 	dev_warn(chip->dev, "get closeloop LRA period failed, rc=%d\n", rc);

	/* Config T_LRA */
	// return haptics_config_openloop_lra_period(chip, t_lra_us);
	dev_info(chip->dev, "BOOTUP CALI START!");
	return haptics_detect_lra_frequency(chip, true/*in_boot*/);
}

static int haptics_init_hpwr_config(struct haptics_chip *chip)
{
	int rc;
	u8 val;

	if ((chip->hw_type == HAP520_MV) && !chip->hpwr_vreg) {
		/* Indicates if HPWR is BOB or Bharger */
		rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_HPWR_INTF_CTL_REG, &val, 1);
		if (rc < 0)
			return rc;

		chip->hpwr_intf_ctl = val & INTF_CTL_MASK;

		/* Read HPWR voltage to adjust the VMAX */
		if (chip->hpwr_voltage_mv == 0 &&
				chip->hpwr_intf_ctl == INTF_CTL_BOB) {
			rc = haptics_read(chip, chip->cfg_addr_base,
				HAP_CFG_VHPWR_REG, &val, 1);
			if (rc < 0)
				return rc;

			chip->hpwr_voltage_mv = val * VHPWR_STEP_MV;
		}
	}

	/* Force VREG_RDY if non-HBoost is used for powering haptics */
	if (is_haptics_external_powered(chip))
		return haptics_masked_write(chip, chip->cfg_addr_base,
				HAP_CFG_VSET_CFG_REG, FORCE_VREG_RDY_BIT,
				FORCE_VREG_RDY_BIT);

	return 0;
}

static int haptics_init_drive_config(struct haptics_chip *chip)
{
	struct haptics_hw_config *config = &chip->config;
	int rc;
	u8 val;

	/* Config driver waveform shape and use 2's complement data format */
	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_DRV_WF_SEL_REG,
			DRV_WF_SEL_MASK | DRV_WF_FMT_BIT, config->drv_wf);
	if (rc < 0)
		return rc;

	/* Config brake mode and waveform shape */
	val = FIELD_PREP(BRAKE_MODE_MASK, config->brake.mode);
	val |= FIELD_PREP(BRAKE_SINE_GAIN_MASK, config->brake.sine_gain);
	val |= FIELD_PREP(BRAKE_WF_SEL_MASK, config->brake.brake_wf);

	return haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_BRAKE_MODE_CFG_REG,
			BRAKE_MODE_MASK | BRAKE_SINE_GAIN_MASK
			| BRAKE_WF_SEL_MASK, val);
}

static int haptics_init_vmax_config(struct haptics_chip *chip)
{
	int rc;
	u8 val;

	if (!is_haptics_external_powered(chip)) {
		rc = haptics_read(chip, chip->hbst_addr_base,
				HAP_BOOST_CLAMP_REG, &val, 1);
		if (rc < 0)
			return rc;

		chip->clamp_at_5v = val & CLAMP_5V_BIT;
	}

	chip->is_hv_haptics = true;
	chip->max_vmax_mv = MAX_VMAX_MV;
	if (chip->hw_type > HAP520_MV) {
		rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_HW_CONFIG_REG, &val, 1);
		if (rc < 0)
			return rc;

		chip->is_hv_haptics = val & HV_HAP_DRIVER_BIT;
		chip->max_vmax_mv = (chip->is_hv_haptics) ?
					MAX_HV_VMAX_MV : MAX_MV_VMAX_MV;
	}

	/* Set the initial clamped vmax value when hBoost is used by charger firmware */
	chip->clamped_vmax_mv = MAX_HV_VMAX_MV;
	/* Config VMAX */
	return haptics_set_vmax_mv(chip, chip->config.vmax_mv);
}

static int haptics_config_wa(struct haptics_chip *chip)
{
	switch (chip->hw_type) {
	case HAP520:
		chip->wa_flags |= TOGGLE_CAL_RC_CLK | SW_CTRL_HBST | SLEEP_CLK_32K_SCALE |
			TOGGLE_EN_TO_FLUSH_FIFO | RECOVER_SWR_SLAVE;
		break;
	case HAP520_MV:
		chip->wa_flags |= SLEEP_CLK_32K_SCALE;
		if (!(chip->cfg_revision & HAP_CFG_REV_LSB_MASK))
			chip->wa_flags |= TOGGLE_MODULE_EN;
		break;
	case HAP525_HV:
		if (chip->hbst_revision == HAP_BOOST_V0P1)
			chip->wa_flags |= SW_CTRL_HBST;
		break;
	case HAP530_HV:
		chip->wa_flags |= EN_RUNTIME_PM | VISENSE_RECOVERY_EN |
			IGNORE_SWR_IN_SPMI_PLAY | DISCHARGE_VNDRV_LDO |
			RESTORE_VMAX_HDRM_1P5V;
		break;
	default:
		dev_err(chip->dev, "HW type %d does not match\n",
			chip->hw_type);
		return -EINVAL;
	}

	return 0;
}

static int haptics_visense_enable(struct haptics_chip *chip, bool enable)
{
	u8 val = enable ? HAP_PTN_VISENSE_ADC_CTL_MASK : 0;
	int rc;

	rc = haptics_write(chip, chip->ptn_addr_base, HAP_PTN_VSENSE_ADC_CTL_REG, &val, 1);
	if (rc < 0)
		return rc;

	rc = haptics_write(chip, chip->ptn_addr_base, HAP_PTN_ISENSE_ADC_CTL_REG, &val, 1);
	if (rc < 0)
		return rc;

	return 0;
}

static int haptics_init_visense_config(struct haptics_chip *chip)
{
	int rc;
	u8 val, mask;

	if (chip->hw_type < HAP530_HV)
		return 0;

	rc = haptics_read(chip, chip->ptn_addr_base, HAP_PTN_VISENSE_EN_REG, &val, 1);
	if (!rc)
		chip->visense_enabled = val & HAP_PTN_VISENSE_EN_BIT;

	if (chip->visense_enabled) {
		rc = haptics_visense_enable(chip, true);
		if (rc < 0)
			return rc;

		if (chip->config.measure_lra_impedance) {
			mask = VISENSE_PEAK_DET_EN_BIT | VISENSE_PEAK_DET_MODE |
				VISENSE_INST_DET_EN_BIT;
			val = VISENSE_PEAK_DET_EN_BIT | VISENSE_PEAK_DET_MODE;
			rc = haptics_masked_write(chip, chip->cfg_addr_base,
				HAP_CFG_VISENSE_PEAK_DET_CTL_REG, mask, val);
		}
	}

	return rc;
}

static int haptics_init_fifo_config(struct haptics_chip *chip)
{
	/*
	 * WF_HOLD_FIFO keeps haptics output to the previous value when FIFO runs dry.
	 * It is helpful to keep haptics play continuously when we have a small FIFO
	 * space and slow SW response to refill the FIFO samples. However, it also
	 * brings a side effect that haptics would continue to output if FIFO play
	 * ends with a non-zero sample while the SPMI_PLAY bit is not de-asserted
	 * timely. Further, with the default minimal FIFO space of 640 bytes, we have
	 * never run into the situation when FIFO runs dry because of SW couldn't
	 * refill the FIFO samples. Hence, keep WF_HOLD_FIFO as disabled.
	 */
	if (chip->hw_type >= HAP525_HV)
		return haptics_masked_write(chip, chip->cfg_addr_base,
				HAP_CFG_AUTO_SHUTDOWN_CFG_REG, EN_WFHOLD_FIFO_BIT, 0);

	return 0;
}

static int haptics_auto_brake_manual_config(struct haptics_chip *chip);

static int haptics_hbst_ovp_trim_pbs_trigger(struct haptics_chip *chip)
{
	return haptics_trigger_pbs(chip, HAP_HBST_OVP_TRIM_PBS);
}

static int haptics_hw_init(struct haptics_chip *chip)
{
	int rc;

	rc = haptics_config_wa(chip);
	if (rc < 0)
		return rc;

	rc = haptics_store_cl_brake_settings(chip);
	if (rc < 0)
		return rc;

	rc = haptics_init_vmax_config(chip);
	if (rc < 0)
		return rc;

	if (chip->wa_flags & RESTORE_VMAX_HDRM_1P5V) {
		rc = haptics_set_vmax_headroom_mv(chip, VMAX_HDRM_MV_DEFAULT);
		if (rc)
			return rc;
	}

	rc = haptics_init_drive_config(chip);
	if (rc < 0)
		return rc;

	rc = haptics_init_hpwr_config(chip);
	if (rc < 0)
		return rc;

	if (chip->config.hbst_ovp_trim) {
		rc = haptics_hbst_ovp_trim_pbs_trigger(chip);
		if (rc < 0)
			return rc;
	}

	if (chip->config.is_erm)
		return 0;

	rc = haptics_init_lra_period_config(chip);
	if (rc < 0)
		return rc;

	rc = haptics_init_preload_pattern_effect(chip);
	if (rc < 0)
		return rc;

	rc = haptics_init_visense_config(chip);
	if (rc < 0)
		return rc;

	if (chip->visense_enabled) {
		rc = haptics_auto_brake_manual_config(chip);
		if (rc < 0)
			return rc;
	}

	rc = haptics_init_fifo_config(chip);
	if (rc < 0)
		return rc;

	return haptics_init_fifo_memory(chip);
}

static irqreturn_t fifo_empty_irq_handler(int irq, void *data)
{
	struct haptics_chip *chip = data;
	struct fifo_cfg *fifo;
	struct fifo_play_status *status;
	u32 samples_left;
	u8 *samples, val;
	int rc, num;

	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_INT_RT_STS_REG, &val, 1);
	if (rc < 0)
		return IRQ_HANDLED;

	if (!(val & FIFO_EMPTY_BIT))
		return IRQ_HANDLED;

	mutex_lock(&chip->play.lock);
	status = &chip->play.fifo_status;
	if (atomic_read(&status->written_done) == 1) {
		/*
		 * Check the FIFO real time fill status before stopping
		 * play to make sure that all FIFO samples can be played
		 * successfully. If there are still samples left in FIFO
		 * memory, defer the stop into erase() function.
		 */
		num = haptics_get_available_fifo_memory(chip);
		if (num != get_max_fifo_samples(chip)) {
			dev_dbg(chip->dev, "%d FIFO samples still in playing\n",
					get_max_fifo_samples(chip) - num);
			goto unlock;
		}

		rc = haptics_stop_fifo_play(chip);
		if (rc < 0)
			goto unlock;

		dev_dbg(chip->dev, "FIFO playing is done\n");
	} else {
		if (atomic_read(&status->cancelled) == 1) {
			dev_dbg(chip->dev, "FIFO programming got cancelled\n");
			goto unlock;
		}

		if (!chip->play.effect)
			goto unlock;

		fifo = chip->play.effect->fifo;
		if (!fifo || !fifo->samples) {
			dev_err(chip->dev, "no FIFO samples available\n");
			goto unlock;
		}

		samples_left = fifo->num_s - status->samples_written;
		num = haptics_get_available_fifo_memory(chip);
		if (num < 0)
			goto unlock;

		samples = fifo->samples + status->samples_written;

		/*
		 * Always use 4-byte burst write in the middle of FIFO programming to
		 * avoid HW padding zeros during 1-byte write which would cause the HW
		 * stop driving for the unexpected padding zeros.
		 */
		if (num < samples_left)
			num = round_down(num, HAP_PTN_FIFO_DIN_NUM);
		else
			num = samples_left;

		rc = haptics_update_fifo_samples(chip, samples, num, true);
		if (rc < 0) {
			dev_err(chip->dev, "Update FIFO samples failed, rc=%d\n",
					rc);
			goto unlock;
		}

		status->samples_written += num;
		trace_qcom_haptics_fifo_prgm_status(fifo->num_s, status->samples_written, num);
		if (status->samples_written == fifo->num_s) {
			dev_dbg(chip->dev, "FIFO programming is done\n");
			atomic_set(&chip->play.fifo_status.written_done, 1);
			haptics_set_fifo_empty_threshold(chip, 0);
		}
	}

unlock:
	mutex_unlock(&chip->play.lock);
	return IRQ_HANDLED;
}

static int haptics_parse_effect_pattern_data(struct haptics_chip *chip,
		struct device_node *node, struct haptics_effect *effect)
{
	struct haptics_hw_config *config = &chip->config;
	u32 data[SAMPLES_PER_PATTERN * 3];
	int rc, tmp, i;

	if (chip->hw_type >= HAP530_HV) {
		dev_dbg(chip->dev, "haptics module with revision %d no longer supports PATTERNx mode, ignore parsing pattern effect DT\n",
				chip->hw_type);
		return 0;
	}

	effect->t_lra_us = config->t_lra_us;
	tmp = of_property_count_elems_of_size(node,
			"qcom,wf-pattern-data", sizeof(u32));
	if (tmp <= 0) {
		dev_dbg(chip->dev, "qcom,wf-pattern-data is not defined properly for effect %d\n",
				effect->id);
		return 0;
	}

	if (tmp > SAMPLES_PER_PATTERN * 3) {
		dev_err(chip->dev, "Pattern src can only play 8 samples at max\n");
		return -EINVAL;
	}

	effect->pattern = devm_kzalloc(chip->dev,
			sizeof(*effect->pattern), GFP_KERNEL);
	if (!effect->pattern)
		return -ENOMEM;

	rc = of_property_read_u32_array(node,
			"qcom,wf-pattern-data", data, tmp);
	if (rc < 0) {
		dev_err(chip->dev, "Read wf-pattern-data failed, rc=%d\n",
				rc);
		return rc;
	}

	for (i = 0; i < tmp / 3; i++) {
		if (data[3 * i] > 0x1ff || data[3 * i + 1] > T_LRA_X_8
				|| data[3 * i + 2] > 1) {
			dev_err(chip->dev, "allowed tuples: [amplitude(<= 0x1ff) period(<=6(T_LRA_X_8)) f_lra_x2(0,1)]\n");
			return -EINVAL;
		}

		effect->pattern->samples[i].amplitude =
			(u16)data[3 * i];
		effect->pattern->samples[i].period =
			(enum s_period)data[3 * i + 1];
		effect->pattern->samples[i].f_lra_x2 =
			(bool)data[3 * i + 2];
	}

	effect->pattern->preload = of_property_read_bool(node,
			"qcom,wf-pattern-preload");
	/*
	 * Use PATTERN1 src by default, effect with preloaded
	 * pattern will use PATTERN2 by default and only the
	 * 1st preloaded pattern will be served.
	 */
	effect->src = PATTERN1;
	if (effect->pattern->preload) {
		if (config->preload_effect != -EINVAL) {
			dev_err(chip->dev, "effect %d has been defined as preloaded\n",
					config->preload_effect);
			effect->pattern->preload = false;
		} else {
			config->preload_effect = effect->id;
			effect->src = PATTERN2;
		}
	}

	if (config->is_erm)
		effect->pattern->play_rate_us = DEFAULT_ERM_PLAY_RATE_US;
	else
		effect->pattern->play_rate_us = config->t_lra_us;

	rc = of_property_read_u32(node, "qcom,wf-pattern-period-us", &tmp);
	if (rc < 0)
		dev_dbg(chip->dev, "Read qcom,wf-pattern-period-us failed, rc=%d\n",
				rc);
	else
		effect->pattern->play_rate_us = tmp;

	if (effect->pattern->play_rate_us > TLRA_MAX_US) {
		dev_err(chip->dev, "qcom,wf-pattern-period-us (%d) exceed the max value: %d\n",
				effect->pattern->play_rate_us,
				TLRA_MAX_US);
		return -EINVAL;
	}

	effect->pattern->play_length_us = get_pattern_play_length_us(effect->pattern);
	if (effect->pattern->play_length_us == -EINVAL) {
		dev_err(chip->dev, "get pattern play length failed\n");
		return -EINVAL;
	}

	return 0;
}

static int haptics_parse_effect_fifo_data(struct haptics_chip *chip,
		struct device_node *node, struct haptics_effect *effect)
{
	struct haptics_hw_config *config = &chip->config;
	int rc, tmp;

	if (effect->pattern) {
		dev_dbg(chip->dev, "ignore parsing FIFO effect when pattern effect is present\n");
		return 0;
	}

	INIT_LIST_HEAD(&effect->node);
	tmp = of_property_count_u8_elems(node, "qcom,wf-fifo-data");
	if (tmp <= 0) {
		dev_dbg(chip->dev, "qcom,wf-fifo-data is not defined properly for effect %d\n",
				effect->id);
		return 0;
	}

	effect->fifo = devm_kzalloc(chip->dev,
			sizeof(*effect->fifo), GFP_KERNEL);
	if (!effect->fifo)
		return -ENOMEM;

	effect->fifo->samples = devm_kcalloc(chip->dev,
			tmp, sizeof(u8), GFP_KERNEL);
	if (!effect->fifo->samples)
		return -ENOMEM;

	rc = of_property_read_u8_array(node, "qcom,wf-fifo-data",
			effect->fifo->samples, tmp);
	if (rc < 0) {
		dev_err(chip->dev, "Read wf-fifo-data failed, rc=%d\n",
				rc);
		return rc;
	}

	effect->fifo->num_s = tmp;
	effect->fifo->period_per_s = T_LRA;
	rc = of_property_read_u32(node, "qcom,wf-fifo-period", &tmp);
	if (rc < 0) {
		dev_err(chip->dev, "Get qcom,wf-fifo-period failed, rc=%d\n", rc);
		return rc;
	} else if (tmp > F_48KHZ) {
		dev_err(chip->dev, "FIFO playing period %d is not supported\n",
				tmp);
		return -EINVAL;
	}

	effect->fifo->period_per_s = tmp;
	effect->fifo->play_length_us =
		get_fifo_play_length_us(effect->fifo, config->t_lra_us);
	if (effect->fifo->play_length_us == -EINVAL) {
		dev_err(chip->dev, "get fifo play length failed\n");
		return -EINVAL;
	}

	effect->src = FIFO;
	effect->fifo->preload = of_property_read_bool(node,
					"qcom,wf-fifo-preload");
	if (effect->fifo->preload) {
		list_add_tail(&effect->node, &chip->mmap_effect_list);
		chip->mmap_effect_count++;
	}

	return 0;
}

static int haptics_parse_effect_brake_data(struct haptics_chip *chip,
		struct device_node *node, struct haptics_effect *effect)
{
	struct haptics_hw_config *config = &chip->config;
	int rc, tmp;

	effect->brake = devm_kzalloc(chip->dev,
			sizeof(*effect->brake), GFP_KERNEL);
	if (!effect->brake)
		return -ENOMEM;

	memcpy(effect->brake, &config->brake, sizeof(*effect->brake));

	of_property_read_u32(node, "qcom,wf-brake-mode", &effect->brake->mode);
	if (effect->brake->mode > AUTO_BRAKE) {
		dev_err(chip->dev, "can't support brake mode: %d\n",
				effect->brake->mode);
		return -EINVAL;
	}

	if (effect->brake->brake_wf == WF_SINE) {
		of_property_read_u32(node, "qcom,wf-brake-sine-gain",
				&effect->brake->sine_gain);
		if (effect->brake->sine_gain > BRAKE_SINE_GAIN_X8) {
			dev_err(chip->dev, "can't support brake sine gain: %d\n",
					effect->brake->sine_gain);
			return -EINVAL;
		}
	}

	effect->brake->disabled =
		of_property_read_bool(node, "qcom,wf-brake-disable");
	tmp = of_property_count_u8_elems(node, "qcom,wf-brake-pattern");
	if (tmp > BRAKE_SAMPLE_COUNT) {
		dev_err(chip->dev, "more than %d brake samples\n",
				BRAKE_SAMPLE_COUNT);
		return -EINVAL;
	}

	if (tmp > 0) {
		memset(effect->brake->samples, 0,
				sizeof(u8) * BRAKE_SAMPLE_COUNT);
		rc = of_property_read_u8_array(node, "qcom,wf-brake-pattern",
				effect->brake->samples, tmp);
		if (rc < 0) {
			dev_err(chip->dev, "Read wf-brake-pattern failed, rc=%d\n",
					rc);
			return rc;
		}
		verify_brake_samples(effect->brake);
	} else {
		if (effect->brake->mode == OL_BRAKE ||
				effect->brake->mode == CL_BRAKE)
			effect->brake->disabled = true;
	}

	effect->brake->play_length_us =
		get_brake_play_length_us(effect->brake, config->t_lra_us);

	return 0;
}

static int haptics_parse_per_effect_dt(struct haptics_chip *chip,
		struct device_node *node, struct haptics_effect *effect)
{
	struct haptics_hw_config *config = &chip->config;
	int rc, tmp;

	if (!effect)
		return -EINVAL;

	effect->vmax_mv = config->vmax_mv;
	rc = of_property_read_u32(node, "qcom,wf-vmax-mv", &tmp);
	if (rc < 0)
		dev_dbg(chip->dev, "read qcom,wf-vmax-mv failed, rc=%d\n",
				rc);
	else
		effect->vmax_mv = tmp;

	if (effect->vmax_mv > MAX_VMAX_MV) {
		dev_err(chip->dev, "qcom,wf-vmax-mv (%d) exceed the max value: %d\n",
				effect->vmax_mv, MAX_VMAX_MV);
		return -EINVAL;
	}

	rc = haptics_parse_effect_pattern_data(chip, node, effect);
	if (rc < 0) {
		dev_err(chip->dev, "parse effect PATTERN data failed, rc=%d\n", rc);
		return rc;
	}

	rc = haptics_parse_effect_fifo_data(chip, node, effect);
	if (rc < 0) {
		dev_err(chip->dev, "parse effect FIFO data failed, rc=%d\n", rc);
		return rc;
	}

	if (!effect->pattern && !effect->fifo) {
		dev_err(chip->dev, "no pattern specified for effect %d\n",
				effect->id);
		return -EINVAL;
	}

	rc = haptics_parse_effect_brake_data(chip, node, effect);
	if (rc < 0) {
		dev_err(chip->dev, "parse effect brake data failed, rc=%d\n", rc);
		return rc;
	}

	if (!config->is_erm)
		effect->auto_res_disable = of_property_read_bool(node,
				"qcom,wf-auto-res-disable");

	return 0;
}

static int haptics_parse_primitives_dt(struct haptics_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *child;
	int rc, i = 0, num = 0;

	for_each_available_child_of_node(node, child) {
		if (of_find_property(child, "qcom,primitive-id", NULL))
			num++;
	}
	if (!num)
		return 0;

	chip->primitives = devm_kcalloc(chip->dev, num,
			sizeof(*chip->effects), GFP_KERNEL);
	if (!chip->primitives)
		return -ENOMEM;

	for_each_available_child_of_node(node, child) {
		if (!of_find_property(child, "qcom,primitive-id", NULL))
			continue;

		rc = of_property_read_u32(child, "qcom,primitive-id", &(chip->primitives[i].id));
		if (rc < 0) {
			dev_err(chip->dev, "Read qcom,primitive-id failed, rc=%d\n",
					rc);
			of_node_put(child);
			return rc;
		}

		rc = haptics_parse_per_effect_dt(chip, child,
					&chip->primitives[i]);
		if (rc < 0) {
			dev_err(chip->dev, "parse primitive %d failed, rc=%d\n",
					i, rc);
			of_node_put(child);
			return rc;
		}
		i++;
	}

	chip->primitives_count = i;
	dev_dbg(chip->dev, "Dump primitive effect settings as following\n");
	__dump_effects(chip, chip->primitives, chip->primitives_count);

	return 0;
}

static int haptics_parse_effects_dt(struct haptics_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *child;
	int rc, i = 0, num = 0;

	for_each_available_child_of_node(node, child) {
		if (of_find_property(child, "qcom,effect-id", NULL))
			num++;
	}
	if (num == 0)
		return 0;

	chip->effects = devm_kcalloc(chip->dev, num,
			sizeof(*chip->effects), GFP_KERNEL);
	if (!chip->effects)
		return -ENOMEM;

	for_each_available_child_of_node(node, child) {
		if (!of_find_property(child, "qcom,effect-id", NULL))
			continue;

		rc = of_property_read_u32(child, "qcom,effect-id", &(chip->effects[i].id));
		if (rc < 0) {
			dev_err(chip->dev, "Read qcom,effect-id failed, rc=%d\n",
					rc);
			of_node_put(child);
			return rc;
		}

		rc = haptics_parse_per_effect_dt(chip, child,
					&chip->effects[i]);
		if (rc < 0) {
			dev_err(chip->dev, "parse effect %d failed, rc=%d\n",
					i, rc);
			of_node_put(child);
			return rc;
		}
		i++;
	}

	chip->effects_count = i;
	dev_dbg(chip->dev, "Dump predefined effect settings as following\n");
	__dump_effects(chip, chip->effects, chip->effects_count);

	return 0;
}

static int haptics_parse_lra_dt(struct haptics_chip *chip)
{
	struct haptics_hw_config *config = &chip->config;
	struct device_node *node = chip->dev->of_node;
	int rc;

	rc = of_property_read_u32(node, "qcom,lra-period-us",
					&config->t_lra_us);
	if (rc < 0) {
		dev_err(chip->dev, "Read T-LRA failed, rc=%d\n", rc);
		return rc;
	}

	if (config->t_lra_us > TLRA_MAX_US) {
		dev_err(chip->dev, "qcom,lra-period-us (%d) exceed the max value: %d\n",
				config->t_lra_us, TLRA_MAX_US);
		return -EINVAL;
	}

	config->drv_wf = WF_SINE;
	of_property_read_u32(node, "qcom,drv-sig-shape", &config->drv_wf);
	if (config->drv_wf >= WF_RESERVED) {
		dev_err(chip->dev, "Can't support drive shape: %d\n",
				config->drv_wf);
		return -EINVAL;
	}

	config->brake.brake_wf = WF_SINE;
	of_property_read_u32(node, "qcom,brake-sig-shape",
			&config->brake.brake_wf);
	if (config->brake.brake_wf >= WF_RESERVED) {
		dev_err(chip->dev, "Can't support brake shape: %d\n",
				config->brake.brake_wf);
		return -EINVAL;
	}

	if (config->brake.brake_wf == WF_SINE) {
		config->brake.sine_gain = BRAKE_SINE_GAIN_X1;
		of_property_read_u32(node, "qcom,brake-sine-gain",
				&config->brake.sine_gain);
		if (config->brake.sine_gain > BRAKE_SINE_GAIN_X8) {
			dev_err(chip->dev, "Can't support brake sine gain: %d\n",
					config->brake.sine_gain);
			return -EINVAL;
		}
	}

	if (chip->hw_type >= HAP525_HV)
		config->measure_lra_impedance = of_property_read_bool(node,
				"qcom,rt-imp-detect");

	config->sw_cmd_freq_det = of_property_read_bool(node, "qcom,sw-cmd-freq-detect");

	return 0;
}

static int haptics_get_revision(struct haptics_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	const __be32 *addr;
	int rc;
	u8 val[2];

	/* Get HAP_CFG module revision */
	addr = of_get_address(node, 0, NULL, NULL);
	if (!addr) {
		dev_err(chip->dev, "Read HAPTICS_CFG address failed\n");
		return -EINVAL;
	}

	chip->cfg_addr_base = be32_to_cpu(*addr);
	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_REVISION1_REG, val, 2);
	if (rc < 0)
		return rc;

	chip->cfg_revision = (val[1] << 8) | val[0];

	/* Get HAP_PTN module revision */
	addr = of_get_address(node, 1, NULL, NULL);
	if (!addr) {
		dev_err(chip->dev, "Read HAPTICS_PATTERN address failed\n");
		return -EINVAL;
	}

	chip->ptn_addr_base = be32_to_cpu(*addr);
	rc = haptics_read(chip, chip->ptn_addr_base,
			HAP_PTN_REVISION1_REG, val, 2);
	if (rc < 0)
		return rc;

	chip->ptn_revision = (val[1] << 8) | val[0];

	/* Get hw_type based on HAP_CFG and HAP_PTN revision combination */
	if ((MAJOR_REV(chip->cfg_revision) == HAP_CFG_V2) &&
			(MAJOR_REV(chip->ptn_revision) == HAP_PTN_V2)) {
		chip->hw_type = HAP520;
		chip->fifo_info = &hap520_fifo;
	} else if ((MAJOR_REV(chip->cfg_revision) == HAP_CFG_V3) &&
			(MAJOR_REV(chip->ptn_revision) == HAP_PTN_V3)) {
		chip->hw_type = HAP520_MV;
		chip->fifo_info = &hap520_mv_fifo;
	} else if ((MAJOR_REV(chip->cfg_revision) == HAP_CFG_V4) &&
			(MAJOR_REV(chip->ptn_revision) == HAP_PTN_V4)) {
		chip->hw_type = HAP525_HV;
		chip->fifo_info = &hap525_fifo;
	} else if ((MAJOR_REV(chip->cfg_revision) == HAP_CFG_V5) &&
			(MAJOR_REV(chip->ptn_revision) == HAP_PTN_V5)) {
		chip->hw_type = HAP530_HV;
		chip->fifo_info = &hap530_fifo;
	} else {
		dev_err(chip->dev, "haptics revision is not supported\n");
		return -EOPNOTSUPP;
	}

	if (is_haptics_external_powered(chip)) {
		dev_info(chip->dev, "haptics revision: HAP_CFG %#x, HAP_PTN %#x, hw_type %d\n",
			chip->cfg_revision, chip->ptn_revision, chip->hw_type);
		return 0;
	}

	/* Get HAP_HBST revision */
	addr = of_get_address(node, 2, NULL, NULL);
	if (!addr) {
		dev_err(chip->dev, "Read HAPTICS_HBOOST address failed\n");
		return -EINVAL;
	}

	chip->hbst_addr_base = be32_to_cpu(*addr);
	rc = haptics_read(chip, chip->hbst_addr_base,
			HAP_BOOST_REVISION1, val, 2);
	if (rc < 0)
		return rc;

	chip->hbst_revision = (val[1] << 8) | val[0];
	dev_info(chip->dev, "haptics revision: HAP_CFG %#x, HAP_PTN %#x, HAP_HBST %#x, hw_type %d\n",
		chip->cfg_revision, chip->ptn_revision, chip->hbst_revision, chip->hw_type);

	return 0;
}

static int haptics_parse_hpwr_vreg_dt(struct haptics_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc;

	if (!of_find_property(node, "qcom,hpwr-supply", NULL))
		return 0;

	chip->hpwr_vreg = devm_regulator_get(chip->dev, "qcom,hpwr");
	if (IS_ERR(chip->hpwr_vreg)) {
		rc = PTR_ERR(chip->hpwr_vreg);
		if (rc != -EPROBE_DEFER)
			dev_err(chip->dev, "Failed to get qcom,hpwr-supply, rc=%d\n",
					rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,hpwr-voltage-mv",
			&chip->hpwr_voltage_mv);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read qcom,hpwr-voltage-mv, rc=%d\n",
				rc);
		return rc;
	}

	if (chip->hpwr_voltage_mv == 0 ||
			chip->hpwr_voltage_mv > NON_HBOOST_MAX_VMAX_MV)
		return -EINVAL;

	return 0;
}

static int haptics_parse_dt(struct haptics_chip *chip)
{
	struct haptics_hw_config *config = &chip->config;
	struct device_node *node = chip->dev->of_node;
	struct platform_device *pdev = to_platform_device(chip->dev);
	int rc = 0, tmp;

	rc = haptics_parse_hpwr_vreg_dt(chip);
	if (rc < 0)
		return rc;

	if (of_find_property(node, "nvmem-cells", NULL)) {
		chip->cl_brake_nvmem = devm_nvmem_cell_get(chip->dev,
						"hap_cl_brake");
		if (IS_ERR(chip->cl_brake_nvmem)) {
			rc = PTR_ERR(chip->cl_brake_nvmem);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev, "Failed to get nvmem-cells, rc=%d\n",
							rc);

			return rc;
		}
	}

	if (of_find_property(node, "nvmem", NULL)) {
		chip->hap_cfg_nvmem =
			devm_nvmem_device_get(chip->dev, "hap_cfg_sdam");
		if (IS_ERR(chip->hap_cfg_nvmem)) {
			rc = PTR_ERR(chip->hap_cfg_nvmem);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev, "Failed to get hap_cfg nvmem device, rc=%d\n",
						rc);
			return rc;
		}
	}

	if (of_find_property(node, "qcom,pbs-client", NULL)) {
		chip->pbs_node = of_parse_phandle(node, "qcom,pbs-client", 0);
		if (!chip->pbs_node) {
			dev_err(chip->dev, "Failed to get PBS client\n");
			return -ENODEV;
		}
	}

	rc = haptics_get_revision(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Get revision failed, rc=%d\n", rc);
		goto free_pbs;
	}

	chip->fifo_empty_irq = platform_get_irq_byname(pdev, "fifo-empty");
	if (!chip->fifo_empty_irq) {
		dev_err(chip->dev, "Get fifo-empty IRQ failed\n");
		rc = -EINVAL;
		goto free_pbs;
	}

	config->vmax_mv = DEFAULT_VMAX_MV;
	of_property_read_u32(node, "qcom,vmax-mv", &config->vmax_mv);
	if (config->vmax_mv >= MAX_VMAX_MV) {
		dev_err(chip->dev, "qcom,vmax-mv (%d) exceed the max value: %d\n",
				config->vmax_mv, MAX_VMAX_MV);
		rc = -EINVAL;
		goto free_pbs;
	}
	config->custom_vmax_mv = DEFAULT_VMAX_MV;
	of_property_read_u32(node, "qcom,custom_vmax_mv", &config->custom_vmax_mv);

	config->f0_vmax_mv = DEFAULT_VMAX_MV;
	of_property_read_u32(node, "qcom,f0-vmax-mv", &config->f0_vmax_mv);

	config->rev_t_lra_us = REV_T_LRA_US;
	of_property_read_u32(node,"qcom,rev_t_lra_us",&config->rev_t_lra_us);

	config->fifo_empty_thresh = chip->fifo_info->fifo_empty_threshold;
	of_property_read_u32(node, "qcom,fifo-empty-threshold",
			&config->fifo_empty_thresh);
	if (config->fifo_empty_thresh >= get_max_fifo_samples(chip)) {
		dev_err(chip->dev, "FIFO empty threshold (%d) should be less than %d\n",
			config->fifo_empty_thresh, get_max_fifo_samples(chip));
		rc = -EINVAL;
		goto free_pbs;
	}

	config->brake.mode = AUTO_BRAKE;
	of_property_read_u32(node, "qcom,brake-mode", &config->brake.mode);
	if (config->brake.mode > AUTO_BRAKE) {
		dev_err(chip->dev, "Can't support brake mode: %d\n",
				config->brake.mode);
		rc = -EINVAL;
		goto free_pbs;
	}

	config->brake.disabled =
		of_property_read_bool(node, "qcom,brake-disable");
	tmp = of_property_count_u8_elems(node, "qcom,brake-pattern");
	if (tmp > BRAKE_SAMPLE_COUNT) {
		dev_err(chip->dev, "more than %d brake samples\n",
				BRAKE_SAMPLE_COUNT);
		rc = -EINVAL;
		goto free_pbs;
	}

	if (tmp > 0) {
		rc = of_property_read_u8_array(node, "qcom,brake-pattern",
				config->brake.samples, tmp);
		if (rc < 0) {
			dev_err(chip->dev, "Read brake-pattern failed, rc=%d\n",
					rc);
			goto free_pbs;
		}
		verify_brake_samples(&config->brake);
	} else {
		if (config->brake.mode == OL_BRAKE ||
				config->brake.mode == CL_BRAKE)
			config->brake.disabled = true;
	}

	config->is_erm = of_property_read_bool(node, "qcom,use-erm");
	if (config->is_erm) {
		config->drv_wf = WF_NO_MODULATION;
		config->brake.brake_wf = WF_NO_MODULATION;
	} else {
		rc = haptics_parse_lra_dt(chip);
		if (rc < 0) {
			dev_err(chip->dev, "Parse device-tree for LRA failed, rc=%d\n",
					rc);
			goto free_pbs;
		}
	}

	if (chip->hw_type >= HAP525_HV)
		config->hbst_ovp_trim = of_property_read_bool(node, "qcom,hbst-ovp-trim");

	config->preload_effect = -EINVAL;
	rc = haptics_parse_effects_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Parse device-tree for effects failed, rc=%d\n",
				 rc);
		goto free_pbs;
	}

	rc = haptics_parse_primitives_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Parse device-tree for primitives failed, rc=%d\n",
				rc);
		goto free_pbs;
	}

	return 0;
free_pbs:
	if (chip->pbs_node) {
		of_node_put(chip->pbs_node);
		chip->pbs_node = NULL;
	}

	return rc;
}

static int swr_slave_reg_enable(struct regulator_dev *rdev)
{
	struct haptics_chip *chip = rdev_get_drvdata(rdev);
	int rc;
	u8 mask = SWR_PAT_INPUT_EN_BIT | SWR_PAT_RES_N_BIT | SWR_PAT_CFG_EN_BIT;

	if (chip->swr_slave_enabled)
		return 0;

	rc = haptics_runtime_resume_get(chip);
	if (rc < 0)
		return rc;

	if (chip->wa_flags & VISENSE_RECOVERY_EN) {
		rc = haptics_visense_handshake_enable(chip, true);
		if (rc < 0)
			goto auto_suspend;
	}

	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_SWR_ACCESS_REG, mask, mask);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to enable SWR_PAT, rc=%d\n",
				rc);
		goto auto_suspend;
	}

	if (!(chip->wa_flags & RECOVER_SWR_SLAVE))
		goto done;

	/*
	 * If haptics has already been in SWR mode when enabling the SWR
	 * slave, it means that the haptics module was stuck in prevous
	 * SWR play. Then toggle HAPTICS_EN to reset haptics module and
	 * ignore SWR mode until next SWR slave enable request is coming.
	 */
	if (is_swr_play_enabled(chip)) {
		rc = haptics_ignore_swr_play(chip, true);
		if (rc < 0)
			goto auto_suspend;

		rc = haptics_toggle_module_enable(chip);
		if (rc < 0)
			goto auto_suspend;
	} else {
		rc = haptics_ignore_swr_play(chip, false);
		if (rc < 0)
			goto auto_suspend;
	}
done:
	chip->swr_slave_enabled = true;
	return 0;
auto_suspend:
	haptics_runtime_autosuspend_put(chip);
	return rc;
}

static int swr_slave_reg_disable(struct regulator_dev *rdev)
{
	struct haptics_chip *chip = rdev_get_drvdata(rdev);
	int rc = 0;

	if (!chip->swr_slave_enabled)
		return 0;

	/* Keep SWR slave always enabled for HAP530_HV */
	if (chip->hw_type == HAP530_HV)
		goto done;

	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_SWR_ACCESS_REG,
			SWR_PAT_INPUT_EN_BIT | SWR_PAT_RES_N_BIT |
			SWR_PAT_CFG_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to disable SWR_PAT, rc=%d\n", rc);
		goto auto_suspend;
	}
done:
	chip->swr_slave_enabled = false;
auto_suspend:
	haptics_runtime_autosuspend_put(chip);
	return rc;
}

static int swr_slave_reg_is_enabled(struct regulator_dev *rdev)
{
	struct haptics_chip *chip = rdev_get_drvdata(rdev);

	return chip->swr_slave_enabled;
}

static const struct regulator_ops swr_slave_reg_ops = {
	.enable		= swr_slave_reg_enable,
	.disable	= swr_slave_reg_disable,
	.is_enabled	= swr_slave_reg_is_enabled,
};

static struct regulator_desc swr_slave_reg_rdesc = {
	.owner		= THIS_MODULE,
	.of_match	= "qcom,hap-swr-slave-reg",
	.name		= "hap-swr-slave-reg",
	.type		= REGULATOR_VOLTAGE,
	.ops		= &swr_slave_reg_ops,
};

static int haptics_init_swr_slave_regulator(struct haptics_chip *chip)
{
	struct regulator_config cfg = {};
	u8 val, mask;
	int rc = 0;

	if (chip->hw_type != HAP530_HV) {
		rc = haptics_read(chip, chip->cfg_addr_base,
				HAP_CFG_SWR_ACCESS_REG, &val, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to read SWR_ACCESS, rc=%d\n",
					rc);
			return rc;
		}

		mask = SWR_PAT_INPUT_EN_BIT | SWR_PAT_RES_N_BIT | SWR_PAT_CFG_EN_BIT;
		val &= mask;
		chip->swr_slave_enabled = ((val == mask) ? true : false);
	}

	cfg.dev = chip->dev;
	cfg.driver_data = chip;
	chip->swr_slave_rdev = devm_regulator_register(chip->dev,
			&swr_slave_reg_rdesc, &cfg);
	if (IS_ERR(chip->swr_slave_rdev)) {
		rc = PTR_ERR(chip->swr_slave_rdev);
		chip->swr_slave_rdev = NULL;

		if (rc != -EPROBE_DEFER)
			dev_err(chip->dev, "register swr-slave-reg regulator failed, rc=%d\n",
					rc);
	}

	return rc;
}

static int haptics_pbs_trigger_isc_config(struct haptics_chip *chip)
{
	int rc;

	if (chip->pbs_node == NULL) {
		dev_err(chip->dev, "PBS device is not defined\n");
		return -ENODEV;
	}

	rc = qpnp_pbs_trigger_single_event(chip->pbs_node);
	if (rc < 0)
		dev_err(chip->dev, "Trigger PBS to config ISC failed, rc=%d\n",
				rc);

	return rc;
}

#define MAX_SWEEP_STEPS		5
#define MIN_DUTY_MILLI_PCT	0
#define MAX_DUTY_MILLI_PCT	100000
#define LRA_CONFIG_REGS		2
static u32 get_lra_impedance_capable_max(struct haptics_chip *chip)
{
	u32 mohms;
	u32 max_vmax_mv, min_isc_ma;

	switch (chip->hw_type) {
	case HAP520:
		min_isc_ma = 250;
		if (chip->clamp_at_5v)
			max_vmax_mv = 5000;
		else
			max_vmax_mv = 10000;
		break;
	case HAP520_MV:
		max_vmax_mv = 5000;
		min_isc_ma = 140;
		break;
	case HAP525_HV:
	case HAP530_HV:
		if (chip->clamp_at_5v) {
			max_vmax_mv = 5000;
			min_isc_ma = 125;
		} else {
			max_vmax_mv = 10000;
			min_isc_ma = 250;
		}
		break;
	default:
		return 0;
	}

	mohms = (max_vmax_mv * 1000) / min_isc_ma;
	if (is_haptics_external_powered(chip))
		mohms = (chip->hpwr_voltage_mv * 1000) / min_isc_ma;

	dev_dbg(chip->dev, "LRA impedance capable max: %u mohms\n", mohms);
	return mohms;
}

#define RT_IMPD_DET_VMAX_DEFAULT_MV		4500
static int haptics_measure_realtime_lra_impedance(struct haptics_chip *chip)
{
	u32 vmax_mv, nominal_ohm, current_ma, vmax_margin_mv, play_length_us, lsb_mohm;
	struct pattern_cfg pattern = {
		.samples = {
			    {0xff, T_LRA, false},
			    {0, 0, 0},
			    {0, 0, 0},
			    {0, 0, 0},
			    {0, 0, 0},
			    {0, 0, 0},
			    {0, 0, 0},
			    {0, 0, 0},
			   },
		.play_rate_us = chip->config.t_lra_us + 2000, /* drive off resonance of the LRA */
		.play_length_us = chip->config.t_lra_us + 2000, /* drive it at least 1 cycle */
		.preload = false,
	};
	u8 samples[4] = {0x7f, 0x7f, 0, 0};
	u32 t_lra_us = (chip->config.cl_t_lra_us * 3) / 2; /* drive off resonance in FIFO mode */
	struct fifo_cfg fifo = {
		.samples = samples,
		.num_s = ARRAY_SIZE(samples),
		.period_per_s = T_LRA_DIV_2, /* half cycle with T_LRA */
		.play_length_us = t_lra_us + 2000,
		.preload = false,
	};
	u8 val, current_sel;
	int rc;

	/* calculate Vmax according to nominal resistance */
	vmax_mv = RT_IMPD_DET_VMAX_DEFAULT_MV;
	current_sel = chip->clamp_at_5v ? CURRENT_SEL_VAL_125MA : CURRENT_SEL_VAL_250MA;
	if (chip->hap_cfg_nvmem != NULL) {
		rc = haptics_get_lra_nominal_impedance(chip, &nominal_ohm);
		if (!rc) {
			/*
			 * use 250mA current_sel when nominal impedance is lower than 15 Ohms
			 * and use 125mA detection when nominal impedance is higher than 40 Ohms
			 */
			if (nominal_ohm < 15)
				current_sel = CURRENT_SEL_VAL_250MA;
			else if (nominal_ohm > 40)
				current_sel = CURRENT_SEL_VAL_125MA;

			/*
			 * give 8 Ohms margin (1000mV / 125mA, or 2000mV / 250mA) for wider
			 * detectability
			 */
			if (current_sel == CURRENT_SEL_VAL_125MA) {
				current_ma = 125;
				vmax_margin_mv = 1000;
			} else {
				current_ma = 250;
				vmax_margin_mv = 2000;
			}

			vmax_mv = max(vmax_mv, nominal_ohm * current_ma + vmax_margin_mv);
		}
	}

	dev_dbg(chip->dev, "Set %u mV Vmax for impedance detection\n", vmax_mv);
	rc = haptics_set_vmax_mv(chip, vmax_mv);
	if (rc < 0)
		return rc;

	/* set current for imp_det comparator and enable it */
	val = LRA_IMPEDANCE_MEAS_EN_BIT | current_sel;
	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_RT_LRA_IMPD_MEAS_CFG_REG,
			LRA_IMPEDANCE_MEAS_EN_BIT |
			LRA_IMPEDANCE_MEAS_CURRENT_SEL_BIT, val);
	if (rc < 0)
		return rc;

	/* disconnect imp_det comparator to avoid it impact SC behavior */
	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_ISC_CFG_REG, EN_IMP_DET_HAP525_HV_BIT, 0);
	if (rc < 0)
		goto restore;

	if (chip->hw_type >= HAP530_HV) {
		/* drive off resonance of the LRA */
		rc = haptics_config_openloop_lra_period(chip, t_lra_us);
		if (rc < 0)
			goto restore;

		rc = haptics_enable_autores(chip, false);
		if (rc < 0)
			goto restore;

		chip->play.brake = NULL;
		rc = haptics_set_fifo(chip, &fifo);
		if (rc < 0)
			goto restore;

		chip->play.pattern_src = FIFO;
		play_length_us = fifo.play_length_us;
	} else {
		/* play 1 drive cycle using PATTERN1 source */
		rc = haptics_set_pattern(chip, &pattern, PATTERN1);
		if (rc < 0)
			goto restore;

		chip->play.pattern_src = PATTERN1;
		play_length_us = pattern.play_length_us;
	}

	rc = haptics_enable_play(chip, true);
	if (rc < 0)
		goto restore;

	usleep_range(play_length_us, play_length_us + 1);

	if (chip->hw_type >= HAP530_HV) {
		rc = haptics_stop_fifo_play(chip);
		if (rc < 0)
			goto restore;

		val = 0;
		rc = haptics_write(chip, chip->cfg_addr_base,
				HAP_CFG_MOD_STATUS_SEL_REG, &val, 1);
	} else {
		rc = haptics_enable_play(chip, false);
	}
	if (rc < 0)
		goto restore;

	/* read the measured LRA impedance */
	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_REAL_TIME_LRA_IMPEDANCE_REG, &val, 1);
	if (rc < 0)
		goto restore;

	lsb_mohm = LRA_IMPEDANCE_MOHMS_LSB;
	if (chip->hw_type >= HAP530_HV)
		lsb_mohm = HAP530_LRA_IMPEDANCE_MOHMS_LSB;

	chip->config.lra_measured_mohms = val * lsb_mohm;
	dev_dbg(chip->dev, "measured LRA impedance: %u mohm",
			chip->config.lra_measured_mohms);
	/* store the detected LRA impedance into SDAM for future usage */
	if (chip->hap_cfg_nvmem != NULL) {
		rc = nvmem_device_write(chip->hap_cfg_nvmem,
				HAP_LRA_DETECTED_OHM_SDAM_OFFSET, 1, &val);
		if (rc < 0)
			dev_err(chip->dev, "write measured impedance value into SDAM failed, rc=%d\n",
					rc);
	}
restore:
	if (rc < 0)
		dev_err(chip->dev, "measure LRA impedance failed, rc=%d\n", rc);

	/* restore T_LRA */
	if (chip->hw_type >= HAP530_HV) {
		rc = haptics_config_openloop_lra_period(chip, chip->config.cl_t_lra_us);
		if (rc)
			return rc;
	}

	return haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_RT_LRA_IMPD_MEAS_CFG_REG,
			LRA_IMPEDANCE_MEAS_EN_BIT, 0);
}

static int haptics_lra_impedance_sweeping(struct haptics_chip *chip, u32 *low, u32 *high)
{
	u32 amplitude, duty_milli_pct, low_milli_pct, high_milli_pct;
	int i, rc;
	u8 val;

	low_milli_pct = MIN_DUTY_MILLI_PCT;
	high_milli_pct = MAX_DUTY_MILLI_PCT;
	/* Sweep duty cycle using binary approach */
	for (i = 0; i < MAX_SWEEP_STEPS; i++) {
		/* Set direct play amplitude */
		duty_milli_pct = (low_milli_pct + high_milli_pct) / 2;
		amplitude = (duty_milli_pct * DIRECT_PLAY_MAX_AMPLITUDE)
						/ 100000;
		rc = haptics_set_direct_play(chip, (u8)amplitude);
		if (rc < 0)
			return rc;

		dev_dbg(chip->dev, "sweeping milli_pct %u, amplitude %#x\n",
				duty_milli_pct, amplitude);
		/* Enable play */
		chip->play.pattern_src = DIRECT_PLAY;
		rc = haptics_enable_play(chip, true);
		if (rc < 0)
			return rc;

		/* Play a cycle then read SC fault status */
		usleep_range(chip->config.t_lra_us,
				chip->config.t_lra_us + 1000);
		rc = haptics_read(chip, chip->cfg_addr_base,
				HAP_CFG_FAULT_STATUS_REG, &val, 1);
		if (rc < 0)
			return rc;

		if (val & SC_FLAG_BIT)
			high_milli_pct = duty_milli_pct;
		else
			low_milli_pct = duty_milli_pct;

		/* Disable play */
		rc = haptics_enable_play(chip, false);
		if (rc < 0)
			return rc;

		/* Sleep 4ms */
		usleep_range(4000, 5000);
	}

	*low = low_milli_pct;
	*high = high_milli_pct;
	return 0;
}

static int haptics_detect_lra_impedance(struct haptics_chip *chip)
{
	int rc, i;
	struct haptics_reg_info lra_config[LRA_CONFIG_REGS] = {
		{ HAP_CFG_DRV_WF_SEL_REG, 0x10 },
		{ HAP_CFG_VMAX_HDRM_REG, 0x00 },
	};
	struct haptics_reg_info backup[LRA_CONFIG_REGS];
	u8 cfg1, cfg2, reg1, reg2, mask1, mask2, val1, val2;
	u32 lra_min_mohms, lra_max_mohms, capability_mohms;
	u32 low_milli_pct, high_milli_pct;

	/* Backup default register values */
	memcpy(backup, lra_config, sizeof(backup));
	for (i = 0; i < LRA_CONFIG_REGS; i++) {
		rc = haptics_read(chip, chip->cfg_addr_base,
				backup[i].addr, &backup[i].val, 1);
		if (rc < 0)
			return rc;
	}

	if (chip->hw_type == HAP520) {
		/* Trigger PBS to config 250mA ISC setting */
		rc = haptics_pbs_trigger_isc_config(chip);
		if (rc < 0)
			return rc;
	} else {
		 /* Config ISC_CFG settings for LRA impedance_detection */
		switch (chip->hw_type) {
		case HAP520_MV:
			reg1 = HAP_CFG_ISC_CFG_REG;
			mask1 = ILIM_CC_EN_BIT;
			val1 = ILIM_CC_EN_BIT;
			reg2 = HAP_CFG_ISC_CFG2_REG;
			mask2 = EN_SC_DET_P_HAP520_MV_BIT |
				EN_SC_DET_N_HAP520_MV_BIT |
				ISC_THRESH_HAP520_MV_MASK;
			val2 = EN_SC_DET_P_HAP520_MV_BIT|
				EN_SC_DET_N_HAP520_MV_BIT|
				ISC_THRESH_HAP520_MV_140MA;
			break;
		case HAP525_HV:
		case HAP530_HV:
			reg1 = HAP_CFG_ISC_CFG_REG;
			mask1 = EN_SC_DET_P_HAP525_HV_BIT | EN_SC_DET_N_HAP525_HV_BIT |
				EN_IMP_DET_HAP525_HV_BIT | ILIM_PULSE_DENSITY_MASK;
			val1 = EN_IMP_DET_HAP525_HV_BIT |
				FIELD_PREP(ILIM_PULSE_DENSITY_MASK, ILIM_DENSITY_8_OVER_64_CYCLES);
			reg2 = HAP_CFG_RT_LRA_IMPD_MEAS_CFG_REG;
			mask2 = LRA_IMPEDANCE_MEAS_EN_BIT | LRA_IMPEDANCE_MEAS_CURRENT_SEL_BIT;
			val2 = chip->clamp_at_5v ? CURRENT_SEL_VAL_125MA : CURRENT_SEL_VAL_250MA;
			val2 |= LRA_IMPEDANCE_MEAS_EN_BIT;
			break;
		default:
			dev_err(chip->dev, "unsupported HW type: %d\n",
					chip->hw_type);
			return -EOPNOTSUPP;
		}

		 /* save ISC_CFG default settings */
		rc = haptics_read(chip, chip->cfg_addr_base, reg1, &cfg1, 1);
		if (rc < 0)
			return rc;

		rc = haptics_read(chip, chip->cfg_addr_base, reg2, &cfg2, 1);
		if (rc < 0)
			return rc;

		/* update ISC_CFG settings for the detection */
		rc = haptics_masked_write(chip, chip->cfg_addr_base, reg1, mask1, val1);
		if (rc < 0)
			return rc;

		rc = haptics_masked_write(chip, chip->cfg_addr_base, reg2, mask2, val2);
		if (rc < 0)
			goto restore;
	}

	/* Set square drive waveform, 10V Vmax, no HDRM */
	rc = __haptics_set_vmax_mv(chip, 10000);
	if (rc < 0)
		goto restore;

	for (i = 0; i < LRA_CONFIG_REGS; i++) {
		rc = haptics_write(chip, chip->cfg_addr_base,
				lra_config[i].addr, &lra_config[i].val, 1);
		if (rc < 0)
			goto restore;
	}

	rc = haptics_enable_hpwr_vreg(chip, true);
	if (rc < 0)
		goto restore;


	rc = haptics_lra_impedance_sweeping(chip, &low_milli_pct, &high_milli_pct);
	if (rc < 0)
		goto restore;

	capability_mohms = get_lra_impedance_capable_max(chip);
	lra_min_mohms = low_milli_pct * capability_mohms / 100000;
	lra_max_mohms = high_milli_pct * capability_mohms / 100000;
	if (lra_min_mohms == 0)
		dev_warn(chip->dev, "Short circuit detected!\n");
	else if (lra_max_mohms >= capability_mohms)
		dev_warn(chip->dev, "Open circuit detected!\n");
	else
		dev_dbg(chip->dev, "LRA impedance is between %u - %u mohms\n",
				lra_min_mohms, lra_max_mohms);

	chip->config.lra_min_mohms = lra_min_mohms;
	chip->config.lra_max_mohms = lra_max_mohms;
	chip->config.lra_open_mohms = capability_mohms;
restore:
	/* Disable play in case it's not been disabled */
	haptics_enable_play(chip, false);
	rc = haptics_enable_hpwr_vreg(chip, false);
	if (rc < 0)
		return rc;

	if (chip->hw_type == HAP520) {
		/* Trigger PBS to restore 1500mA ISC setting */
		rc = haptics_pbs_trigger_isc_config(chip);
		if (rc < 0)
			return rc;
	} else {
		/* restore ISC_CFG settings to default */
		rc = haptics_write(chip, chip->cfg_addr_base, reg1, &cfg1, 1);
		if (rc < 0)
			return rc;

		rc = haptics_write(chip, chip->cfg_addr_base, reg2, &cfg2, 1);
		if (rc < 0)
			return rc;
	}

	/* Restore driver waveform, Vmax, HDRM settings */
	for (i = 0; i < LRA_CONFIG_REGS; i++) {
		rc = haptics_write(chip, chip->cfg_addr_base,
				backup[i].addr, &backup[i].val, 1);
		if (rc < 0)
			break;
	}

	return rc;
}

static int haptics_enable_autores_cal(struct haptics_chip *chip, bool enable)
{
	u8 val = 0;
	int rc;

	if (enable)
		val = AUTORES_CAL_TRIG_BIT | AUTORES_CAL_TIMER_6_HALF_CYCLES;
	else
		val = AUTORES_CAL_TIMER_4_HALF_CYCLES;

	rc = haptics_write(chip, chip->ptn_addr_base,
			HAP_PTN_AUTORES_CAL_CFG_REG, &val, 1);

	return rc;
}

#define LRA_CALIBRATION_VMAX_HDRM_MV	500
static int haptics_detect_lra_frequency(struct haptics_chip *chip, bool in_boot)
{
	int rc;
	u8 autores_cfg, drv_duty_cfg, amplitude, mask, val = 0;
	u32 vmax_mv = chip->config.f0_vmax_mv;

	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_AUTORES_CFG_REG, &autores_cfg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Read AUTORES_CFG failed, rc=%d\n", rc);
		return rc;
	}

	rc = haptics_read(chip, chip->cfg_addr_base,
			HAP_CFG_DRV_DUTY_CFG_REG, &drv_duty_cfg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Read DRV_DUTY_CFG failed, rc=%d\n", rc);
		return rc;
	}

	if (!chip->config.sw_cmd_freq_det)
		val = AUTORES_EN_BIT;

	if (chip->hw_type >= HAP525_HV)
		val |= AUTORES_EN_DLY_7_CYCLES << AUTORES_EN_DLY_SHIFT |
			AUTORES_ERR_WINDOW_25_PERCENT;
	else
		val |= AUTORES_EN_DLY_6_CYCLES << AUTORES_EN_DLY_SHIFT |
			AUTORES_ERR_WINDOW_50_PERCENT;

	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_AUTORES_CFG_REG, AUTORES_EN_BIT |
			AUTORES_EN_DLY_MASK | AUTORES_ERR_WINDOW_MASK,
			val);
	if (rc < 0)
		return rc;

	if (chip->hw_type >= HAP525_HV) {
		mask = ADT_DRV_DUTY_EN_BIT | ADT_BRK_DUTY_EN_BIT |
			DRV_DUTY_MASK | BRK_DUTY_MASK;
		val = DRV_DUTY_62P5_PCT << DRV_DUTY_SHIFT | BRK_DUTY_75_PCT;
	} else {
		mask = ADT_DRV_DUTY_EN_BIT;
		val = 0;
	}

	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_DRV_DUTY_CFG_REG, mask, val);
	if (rc < 0)
		goto restore;

	rc = haptics_config_openloop_lra_period(chip, chip->config.t_lra_us);
	if (rc < 0)
		goto restore;

	rc = haptics_set_vmax_headroom_mv(chip, LRA_CALIBRATION_VMAX_HDRM_MV);
	if (rc < 0)
		goto restore;

	/* Fix Vmax to (hpwr_vreg_mv - hdrm_mv) in non-HBOOST regulator case */
	if (is_haptics_external_powered(chip))
		vmax_mv = chip->hpwr_voltage_mv - LRA_CALIBRATION_VMAX_HDRM_MV;

	rc = haptics_set_vmax_mv(chip, vmax_mv);
	if (rc < 0)
		goto restore;

	amplitude = get_direct_play_max_amplitude(chip);
	rc = haptics_set_direct_play(chip, amplitude);
	if (rc < 0)
		goto restore;

	rc = haptics_enable_hpwr_vreg(chip, true);
	if (rc < 0)
		goto restore;

	chip->play.pattern_src = DIRECT_PLAY;
	rc = haptics_enable_play(chip, true);
	if (rc < 0)
		goto restore;

	if (chip->config.sw_cmd_freq_det) {
		msleep(60);
		/* Enable SW based auto resonance */
		rc = haptics_enable_autores_cal(chip, true);
		if (rc < 0)
			goto restore;

		msleep(20);

		/* Get the 1st detected frequency */
		rc = haptics_get_closeloop_lra_period(chip, in_boot);
		if (rc < 0)
			goto restore;

		msleep(50);

		/* Reset SW based auto resonance */
		rc = haptics_enable_autores_cal(chip, false);
		if (rc < 0)
			goto restore;

		rc = haptics_enable_autores_cal(chip, true);
		if (rc < 0)
			goto restore;

		msleep(20);

		/* Get the 2nd detected frequency */
		rc = haptics_get_closeloop_lra_period(chip, in_boot);
		if (rc < 0)
			goto restore;

		usleep_range(4000, 4001);

	} else {
		/* Wait for ~150ms to get the LRA calibration result */
		msleep(150);
		rc = haptics_get_closeloop_lra_period(chip, in_boot);
		if (rc < 0)
			goto restore;
	}

	rc = haptics_enable_play(chip, false);
	if (rc < 0)
		goto restore;

	if (chip->config.sw_cmd_freq_det)
		haptics_enable_autores_cal(chip, false);

	haptics_config_openloop_lra_period(chip, chip->config.cl_t_lra_us);

restore:
	/* Disable play in case it's not been disabled */
	haptics_enable_play(chip, false);
	rc = haptics_enable_hpwr_vreg(chip, false);
	if (rc < 0)
		return rc;

	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_AUTORES_CFG_REG, &autores_cfg, 1);
	if (rc < 0)
		return rc;

	rc = haptics_write(chip, chip->cfg_addr_base,
			HAP_CFG_DRV_DUTY_CFG_REG, &drv_duty_cfg, 1);

	return rc;
}

static int haptics_auto_brake_manual_config(struct haptics_chip *chip)
{
	bool auto_brake_cal_done = false;
	unsigned int addr;
	u8 val[2];
	int rc;

	if (chip->config.brake.mode != AUTO_BRAKE)
		return 0;

	if (!chip->hap_cfg_nvmem)
		return -EOPNOTSUPP;

	rc = nvmem_device_read(chip->hap_cfg_nvmem,
				HAP_AUTO_BRAKE_CAL_DONE_OFFSET, 1, val);
	if (rc <= 0) {
		dev_err(chip->dev, "read AUTO_BRAKE_CAL_DONE failed, rc=%d\n",
			rc);
		return rc;
	}

	auto_brake_cal_done = val[0] & AUTO_BRAKE_CAL_DONE;
	dev_dbg(chip->dev, "auto_brake_cal_done: %u\n", auto_brake_cal_done);

	/* Read RNAT and RCAL values from SDAM and write to HAP_CFG */
	addr = auto_brake_cal_done ? HAP_AUTO_BRAKE_RNAT_OFFSET :
		HAP_AUTO_BRAKE_RNAT_DEFAULT_OFFSET;
	rc = nvmem_device_read(chip->hap_cfg_nvmem, addr, 2, val);
	if (rc < 0) {
		dev_err(chip->dev, "Error reading RNAT/RCAL values, rc=%d\n",
			rc);
		return rc;
	}

	val[0] &= CL_BRAKE_RNAT_MASK;
	rc = haptics_write(chip, chip->cfg_addr_base, HAP_CFG_CL_BRAKE_RNAT_REG,
			&val[0], 1);
	if (rc < 0)
		return rc;

	val[1] &= CL_BRAKE_RCAL_MASK;
	rc = haptics_write(chip, chip->cfg_addr_base, HAP_CFG_CL_BRAKE_RCAL_REG,
			&val[1], 1);
	if (rc < 0)
		return rc;

	/* Read T_WIND_STS values from SDAM and write to HAP_PTN */
	addr = auto_brake_cal_done ? HAP_AUTO_BRAKE_T_WIND_STS_MSB_OFFSET :
		HAP_AUTO_BRAKE_T_WIND_MSB_DEF_OFFSET;
	rc = nvmem_device_read(chip->hap_cfg_nvmem, addr, 2, val);
	if (rc < 0) {
		dev_err(chip->dev, "Error reading T_WIND_STS values, rc=%d\n",
			rc);
		return rc;
	}

	val[0] &= HAP_PTN_TWIND_CAL_MSB_MASK;
	rc = haptics_write(chip, chip->ptn_addr_base,
			HAP_PTN_FORCE_TWIND_CAL_MSB, val, 2);
	if (rc < 0)
		return rc;

	/* Read BRAKE_AMP_0 value from SDAM and write to HAP_PTN */
	rc = nvmem_device_read(chip->hap_cfg_nvmem,
			HAP_AUTO_BRAKE_CAL_BRAKE_AMP0_OFFSET, 1, val);
	if (rc < 0) {
		dev_err(chip->dev, "Error reading BRAKE_AMP0 value, rc=%d\n",
			rc);
		return rc;
	}

	rc = haptics_write(chip, chip->ptn_addr_base, HAP_PTN_BRAKE_AMP_0_REG,
			&val[0], 1);
	if (rc < 0)
		return rc;

	/* Read BRAKE_RSET value from SDAM and write to HAP_CFG */
	rc = nvmem_device_read(chip->hap_cfg_nvmem,
			HAP_AUTO_BRAKE_CAL_CL_BRAKE_RSET_OFFSET, 1, val);
	if (rc < 0) {
		dev_err(chip->dev, "Error reading BRAKE_RSET value, rc=%d\n",
			rc);
		return rc;
	}

	val[0] &= FORCE_RNAT_RCAL_PARAM_MASK;
	rc = haptics_masked_write(chip, chip->cfg_addr_base,
			HAP_CFG_CL_BRAKE_RSET_REG, FORCE_RNAT_RCAL_PARAM_MASK,
			val[0]);
	if (rc < 0)
		return rc;

	/* Run auto brake re-calibration */
	val[0] = BRAKE_CAL_CL_RECAL;
	rc = haptics_masked_write(chip, chip->cfg_addr_base, HAP_CFG_CAL_EN_REG,
			BRAKE_CAL_MASK, val[0]);
	if (rc < 0)
		return rc;

	/* As per HW recommendation, wait for 5 uS before clearing BRAKE_CAL */
	udelay(5);

	val[0] = BRAKE_CAL_NONE;
	rc = haptics_masked_write(chip, chip->cfg_addr_base, HAP_CFG_CAL_EN_REG,
			BRAKE_CAL_MASK, val[0]);

	/*
	 * Read back whether BRAKE_CAL_DONE is high and the calibration is
	 * successful or not.
	 */
	rc = haptics_get_status_data(chip, AUTO_BRAKE_CAL_STS, val);
	if (!rc)
		dev_dbg(chip->dev, "Manual AUTO_BRAKE_CAL_DONE: %#x %s\n",
			 val[1], val[1] & BIT(2) ? "Success" : "Fail");

	return rc;
}

#define AUTO_BRAKE_CAL_POLLING_COUNT	10
#define AUTO_BRAKE_CAL_POLLING_STEP_US	20000
#define AUTO_BRAKE_CAL_WAIT_MS		800

/*static int haptics_auto_brake_pbs_trigger(struct haptics_chip *chip)
{
	u32 retry_count = AUTO_BRAKE_CAL_POLLING_COUNT;
	int rc;
	u8 val;

	rc = haptics_clear_fault(chip);
	if (rc < 0)
		return rc;

	rc = haptics_trigger_pbs(chip, HAP_AUTO_BRAKE_CAL_PBS);
	if (rc < 0) {
		dev_err(chip->dev, "trigger AUTO_BRAKE_CAL PBS failed, rc=%d\n", rc);
		return rc;
	}


	 * wait for ~800ms and then poll auto brake cal done flag with ~200ms
	 * timeout

	msleep(AUTO_BRAKE_CAL_WAIT_MS);

	while (retry_count--) {
		rc = nvmem_device_read(chip->hap_cfg_nvmem,
				HAP_AUTO_BRAKE_CAL_DONE_OFFSET, 1, &val);
		if (rc < 0) {
			dev_err(chip->dev, "read auto brake cal done flag failed, rc=%d\n",
				rc);
			return rc;
		}

		if (val & AUTO_BRAKE_CAL_DONE) {
			dev_info(chip->dev, "auto brake calibration is done\n");
			break;
		}

		usleep_range(AUTO_BRAKE_CAL_POLLING_STEP_US,
				AUTO_BRAKE_CAL_POLLING_STEP_US + 1);
	}

	return rc;
}*/

#define AUTO_BRAKE_CAL_DRIVE_CYCLES	6

/*static int haptics_auto_brake_calibration_customize(struct haptics_chip *chip)
{
	struct haptics_reg_info lra_config[4] = {
		{ HAP_CFG_DRV_DUTY_CFG_REG, 0x55 },
		{ HAP_CFG_BRAKE_MODE_CFG_REG, 0xC1 },
		{ HAP_CFG_CL_BRAKE_CFG_REG, 0xE1 },
		{ HAP_CFG_ADT_DRV_DUTY_CFG_REG, 0x3B },
	};
	struct haptics_reg_info backup[4];
	u32 t_lra_us, tmp;
	u8 val[AUTO_BRAKE_CAL_DRIVE_CYCLES] = {};
	int rc, i;

	t_lra_us = chip->config.t_lra_us;
     Update T_LRA into SDAM 
	if (chip->config.cl_t_lra_us != 0)
		t_lra_us = chip->config.cl_t_lra_us;

	tmp = t_lra_us / TLRA_STEP_US;
	val[0] = tmp & TLRA_OL_LSB_MASK;
	val[1] = (tmp >> 8) & TLRA_OL_MSB_MASK;
	rc = nvmem_device_write(chip->hap_cfg_nvmem, HAP_LRA_PTRN1_TLRA_LSB_OFFSET, 2, val);
	if (rc < 0) {
		dev_err(chip->dev, "set T_LRA for auto brake cal failed,rc=%d\n", rc);
		return rc;
	}

    Set Vmax to 4.5V with 50mV per step
	val[0] = 0x5A;
	rc = nvmem_device_write(chip->hap_cfg_nvmem, HAP_AUTO_BRAKE_CAL_VMAX_OFFSET, 1, val);
	if (rc < 0) {
		dev_err(chip->dev, "set Vmax for auto brake cal failed,rc=%d\n", rc);
		return rc;
	}

    Set PTRN1_CFGx for each cycle in the 6-cycle drive calibration
	memset(val, 0, sizeof(val));
	rc = nvmem_device_write(chip->hap_cfg_nvmem, HAP_AUTO_BRAKE_CAL_PTRN1_CFG0_OFFSET,
			AUTO_BRAKE_CAL_DRIVE_CYCLES, val);
	if (rc < 0) {
		dev_err(chip->dev, "set PTRN1_CFGx for auto brake cal failed, rc=%d\n", rc);
		return rc;
	}


	 * Set amplitude for 6-cycle drive calibration
	 *   1 cycle over-drive with 4.5V VMAX + 0.5V VMAX_HDRM
	 *   2 cycles intermediate drive with VMAX close to 3.5V
	 *   3 cycles steady drive with VMAX close to Vrms

	val[0] = 0xFF;
	val[1] = val[2] = 0xC9;
	val[3] = val[4] = val[5] = chip->config.vmax_mv * 0xFF / 4500;
	rc = nvmem_device_write(chip->hap_cfg_nvmem, HAP_AUTO_BRAKE_CAL_PTRN1_LSB0_OFFSET,
			AUTO_BRAKE_CAL_DRIVE_CYCLES, val);
	if (rc < 0) {
		dev_err(chip->dev, "set PTRN1_LSBx for auto brake cal failed, rc=%d\n", rc);
		return rc;
	}

    cache several registers setting before configure them for auto brake calibration 
	memcpy(backup, lra_config, sizeof(backup));
	for (i = 0; i < 4; i++) {
		rc = haptics_read(chip, chip->cfg_addr_base,
				backup[i].addr, &backup[i].val, 1);
		if (rc < 0)
			return rc;
	}

    Set 62.5% BRK_DUTY, enable AUTO brake, etc 
	for (i = 0; i < 4; i++) {
		rc = haptics_write(chip, chip->cfg_addr_base,
				lra_config[i].addr, &lra_config[i].val, 1);
		if (rc < 0)
			goto restore;
	}

    Clear calibration done flag 1st 
	val[0] = 0;
	rc = nvmem_device_write(chip->hap_cfg_nvmem, HAP_AUTO_BRAKE_CAL_DONE_OFFSET, 1, val);
	if (rc < 0) {
		dev_err(chip->dev, "clear auto brake cal done flag failed, rc=%d\n", rc);
		goto restore;
	}

	rc = haptics_auto_brake_pbs_trigger(chip);

restore:
    restore haptics settings after auto brake calibration 
	for (i = 0; i < 4; i++)
		haptics_write(chip, chip->cfg_addr_base,
				backup[i].addr, &backup[i].val, 1);

	return rc;
}

static int haptics_start_auto_brake_calibration(struct haptics_chip *chip)
{
	Auto brake calibration is supported only if AUTO_BRAKE mode is specified
	if (chip->config.brake.mode != AUTO_BRAKE)
		return 0;

    Ignore calibration if nvmem is not assigned 
	if (!chip->hap_cfg_nvmem)
		return -EOPNOTSUPP;


	 * Auto brake calibration is supported only for HAP525_HV
	 * and HAP530_HV.

	if (chip->hw_type < HAP525_HV)
		return -EOPNOTSUPP;

	if (chip->hw_type == HAP525_HV)
		return haptics_auto_brake_calibration_customize(chip);

	return haptics_auto_brake_pbs_trigger(chip);
}*/

static int haptics_start_lra_calibrate(struct haptics_chip *chip)
{
	int rc;

	rc = haptics_runtime_resume_get(chip);
	if (rc < 0)
		return rc;

	mutex_lock(&chip->play.lock);
	/*
	 * Ignore calibration if it's in FIFO playing to avoid
	 * messing up the FIFO playing status
	 */
	if ((chip->play.pattern_src == FIFO) &&
			atomic_read(&chip->play.fifo_status.is_busy)) {
		dev_err(chip->dev, "In FIFO playing, ignore calibration\n");
		rc = -EBUSY;
		goto unlock;
	}

	/* Stop other mode playing if there is any */
	rc = haptics_enable_play(chip, false);
	if (rc < 0) {
		dev_err(chip->dev, "Stop playing failed, rc=%d\n", rc);
		goto unlock;
	}

	chip->play.in_calibration = true;
	rc = haptics_detect_lra_frequency(chip, false/*in_boot*/);
	if (rc < 0) {
		dev_err(chip->dev, "Detect LRA frequency failed, rc=%d\n", rc);
		goto unlock;
	}

	/* Sleep 50ms to stabilize the LRA from frequency detection */
	msleep(50);
	if (chip->config.measure_lra_impedance)
		rc = haptics_measure_realtime_lra_impedance(chip);
	else
		rc = haptics_detect_lra_impedance(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Detect LRA impedance failed, rc=%d\n", rc);
		goto unlock;
	}

	/* Sleep 50ms to stabilize the LRA from impedance detection */
	msleep(50);

/*	rc = haptics_start_auto_brake_calibration(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Run auto brake calibration failed, rc=%d\n", rc);
		goto unlock;
	}
*/

unlock:
	if (chip->wa_flags & RESTORE_VMAX_HDRM_1P5V)
		haptics_set_vmax_headroom_mv(chip, VMAX_HDRM_MV_DEFAULT);

	haptics_clear_fault(chip);
	chip->play.in_calibration = false;
	mutex_unlock(&chip->play.lock);
	haptics_runtime_autosuspend_put(chip);
	return rc;
}

static ssize_t lra_calibration_store(const struct class *c,
		const struct class_attribute *attr, const char *buf, size_t count)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);
	bool val;
	int rc;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (val) {
		rc = haptics_start_lra_calibrate(chip);
		if (rc < 0)
			return rc;
	}
	return count;
}
static CLASS_ATTR_WO(lra_calibration);

static ssize_t lra_frequency_hz_show(const struct class *c,
		const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);
	u32 cl_f_lra;

	if (chip->config.cl_t_lra_us == 0)
		return -EINVAL;

	cl_f_lra = USEC_PER_SEC * 10 / chip->config.cl_t_lra_us;
	return scnprintf(buf, PAGE_SIZE, "%d Hz\n", cl_f_lra);
}
static CLASS_ATTR_RO(lra_frequency_hz);

static ssize_t lra_impedance_show(const struct class *c,
		const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);

	if (chip->config.measure_lra_impedance)
		return scnprintf(buf, PAGE_SIZE, "%u mohms\n",
				chip->config.lra_measured_mohms);

	if (chip->config.lra_min_mohms == 0 && chip->config.lra_max_mohms == 0)
		return -EINVAL;
	else if (chip->config.lra_min_mohms == 0)
		return scnprintf(buf, PAGE_SIZE, "%s\n", "Short circuit");
	else if (chip->config.lra_max_mohms >= chip->config.lra_open_mohms)
		return scnprintf(buf, PAGE_SIZE, "%s\n", "Open circuit");
	else
		return scnprintf(buf, PAGE_SIZE, "%u ~ %u mohms\n",
				chip->config.lra_min_mohms,
				chip->config.lra_max_mohms);
}
static CLASS_ATTR_RO(lra_impedance);

static ssize_t lra_calibration_check_show(const struct class *c,
		const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);
	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->config.lra_calibrated);
}
static CLASS_ATTR_RO(lra_calibration_check);

static ssize_t primitive_duration_show(const struct class *c,
		const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->primitive_duration);
}

static ssize_t primitive_duration_store(const struct class *c,
		const struct class_attribute *attr, const char *buf, size_t count)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);
	u16 primitive_id = 0;
	int i = 0;

	if (kstrtou16(buf, 0, &primitive_id))
		return -EINVAL;

	for (i = 0; i < chip->primitives_count; i++) {
		if (chip->primitives[i].id == primitive_id)
			break;
	}

	if (i == chip->primitives_count) {
		pr_err("Primitive id specified is incorrect\n");
		return -EINVAL;
	}

	chip->primitive_duration = get_play_length_effect_us(&chip->primitives[i]);

	return count;
}

static CLASS_ATTR_RW(primitive_duration);

static ssize_t visense_enabled_show(const struct class *c,
		const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->visense_enabled);
}
static CLASS_ATTR_RO(visense_enabled);


static ssize_t xiaomi_f0_put(const struct class *c, const struct class_attribute *attr, const char *buf, size_t count)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);
	u32 val, oldVal;
	int rc;
	if (kstrtou32(buf, 0, &val))
		return -EINVAL;
	oldVal = chip->config.cl_t_lra_us;
	chip->config.cl_t_lra_us = USEC_PER_SEC * 10 / val;
	chip->config.t_lra_us = chip->config.cl_t_lra_us;
	rc = haptics_config_openloop_lra_period(chip, chip->config.cl_t_lra_us);
	dev_info(chip->dev, "xiaomi_f0_put: cl_t_lra_us(%d  -->  %d).\n", oldVal, chip->config.cl_t_lra_us);
	if (rc < 0) {
		dev_err(chip->dev, "xiaomi_f0: f0(%d) config failed.\n", val);
		return rc;
	}
	dev_info(chip->dev, "xiaomi_f0_put: f0(%d) config success.\n", val);
	return count;
}

static ssize_t xiaomi_f0_get(const struct class *c, const struct class_attribute *attr, char *buf)
{
	return lra_frequency_hz_show(c, attr, buf);
}

#define mi_f0_ctrl_show xiaomi_f0_get
#define mi_f0_ctrl_store xiaomi_f0_put
static CLASS_ATTR_RW(mi_f0_ctrl);

static ssize_t mi_vmax_ctrl_store(const struct class *c, const struct class_attribute *attr, const char *buf, size_t count)
{
	struct haptics_chip *chip = container_of(c, struct haptics_chip, hap_class);
	u32 val;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	chip->config.vmax_mv = val;

	dev_info(chip->dev, "mi_vmax_ctrl_store: vmax %d.\n", val);

	return count;
}

static ssize_t mi_vmax_ctrl_show(const struct class *c, const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c, struct haptics_chip, hap_class);
	return scnprintf(buf, PAGE_SIZE, "vmax: %d\n", chip->config.vmax_mv);
}

static CLASS_ATTR_RW(mi_vmax_ctrl);

static ssize_t mi_custom_vmax_ctrl_store(const struct class *c, const struct class_attribute *attr, const char *buf, size_t count)
{
	struct haptics_chip *chip = container_of(c, struct haptics_chip, hap_class);
	u32 val;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	chip->custom_effect->vmax_mv = val;

	dev_info(chip->dev, "mi_custom_vmax_ctrl_store: vmax %d.\n", val);

	return count;
}

static ssize_t mi_custom_vmax_ctrl_show(const struct class *c, const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c, struct haptics_chip, hap_class);
	return scnprintf(buf, PAGE_SIZE, "custom vmax: %d\n", chip->custom_effect->vmax_mv);
}

static CLASS_ATTR_RW(mi_custom_vmax_ctrl);

static ssize_t mi_f0_check_show(const struct class *c, const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c, struct haptics_chip, hap_class);
	return scnprintf(buf, PAGE_SIZE, "%d\n",  chip->config.f0_check);
}

static CLASS_ATTR_RO(mi_f0_check);

static ssize_t v_gain_error_show(const struct class *c,
		const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);
	u8 v_gain_error[2] = { 0 };
	int rc;

	rc = haptics_read(chip, chip->ptn_addr_base,
			HAP_PTN_VGAIN_ERR_COMP_LSB, v_gain_error, 2);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%#x\n", (v_gain_error[1] << 8 | v_gain_error[0]));
}
static const CLASS_ATTR_RO(v_gain_error);

static ssize_t i_gain_error_show(const struct class *c,
		const struct class_attribute *attr, char *buf)
{
	struct haptics_chip *chip = container_of(c,
			struct haptics_chip, hap_class);
	u8 i_gain_error[2] = { 0 };
	int rc;

	rc = haptics_read(chip, chip->ptn_addr_base,
			HAP_PTN_IGAIN_ERR_COMP_LSB, i_gain_error, 2);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%#x\n", (i_gain_error[1] << 8 | i_gain_error[0]));
}
static const CLASS_ATTR_RO(i_gain_error);

static struct attribute *hap_class_attrs[] = {
	&class_attr_lra_calibration.attr,
	&class_attr_lra_frequency_hz.attr,
	&class_attr_lra_impedance.attr,
	&class_attr_primitive_duration.attr,
	&class_attr_visense_enabled.attr,
	&class_attr_mi_f0_ctrl.attr,
	&class_attr_mi_vmax_ctrl.attr,
	&class_attr_mi_custom_vmax_ctrl.attr,
	&class_attr_lra_calibration_check.attr,
	&class_attr_mi_f0_check.attr,
	NULL,
};
ATTRIBUTE_GROUPS(hap_class);

static bool is_swr_supported(struct haptics_chip *chip)
{
	/* HAP520_MV does not support soundwire */
	if (chip->hw_type == HAP520_MV)
		return false;

	return true;
}

static enum hrtimer_restart haptics_disable_hbst_timer(struct hrtimer *timer)
{
	struct haptics_chip *chip = container_of(timer,
			struct haptics_chip, hbst_off_timer);
	int rc;

	rc = haptics_boost_vreg_enable(chip, false);
	if (rc < 0)
		dev_err(chip->dev, "disable boost vreg failed, rc=%d\n", rc);
	else
		dev_dbg(chip->dev, "boost vreg is disabled\n");

	return HRTIMER_NORESTART;
}

static int haptics_boost_notifier(struct notifier_block *nb, unsigned long event, void *val)
{
	struct haptics_chip *chip = container_of(nb, struct haptics_chip, hboost_nb);
	u32 vmax_mv;
	int rc;

	switch (event) {
	case VMAX_CLAMP:
		vmax_mv = *(u32 *)val;
		if (vmax_mv > MAX_HV_VMAX_MV) {
			dev_err(chip->dev, "voted Vmax (%u mV) is higher than maximum (%u mV)\n",
					vmax_mv, MAX_HV_VMAX_MV);
			return -EINVAL;
		}

		chip->clamped_vmax_mv = vmax_mv;
		dev_dbg(chip->dev, "Vmax is clamped at %u mV to support hBoost concurrency\n",
				vmax_mv);
		rc = haptics_force_vreg_ready(chip, vmax_mv != MAX_HV_VMAX_MV);
		if (rc < 0)
			return rc;
		break;
	default:
		break;
	}

	return 0;
}

#include "qcom-hv-haptics-debugfs.c"

static int haptics_lpass_ssr_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct haptics_chip *chip = container_of(nb, struct haptics_chip, lpass_ssr_nb);
	u8 val;
	int rc;

	if (!(chip->wa_flags & VISENSE_RECOVERY_EN))
		return NOTIFY_DONE;

	if (!is_swr_play_enabled(chip)) {
		dev_dbg(chip->dev, "ignore VISENSE recovery if not in SWR play\n");
		return NOTIFY_DONE;
	}

	switch (event) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		/*
		 * Enable HAPTICS_EN to keep haptics module clock on during ADSP
		 * SSR. It's needed for keeping haptics SPMI and SWR data port in
		 * correct state, and it could be disabled after ADSP SSR.
		 */
		rc = haptics_runtime_resume_get(chip);
		if (rc < 0)
			return rc;

		val = 0;
		rc = haptics_write(chip, chip->cfg_addr_base, HAP_CFG_SWR_ACCESS_REG,  &val, 1);
		if (rc < 0)
			return rc;

		rc = haptics_visense_handshake_enable(chip, false);
		if (rc < 0)
			return rc;

		dev_dbg(chip->dev, "QCOM_SSR_BEFORE_SHUTDOWN is handled\n");
		break;
	case QCOM_SSR_BEFORE_POWERUP:
		val = SWR_PAT_INPUT_EN_BIT | SWR_PAT_RES_N_BIT | SWR_PAT_CFG_EN_BIT;
		rc = haptics_write(chip, chip->cfg_addr_base, HAP_CFG_SWR_ACCESS_REG,  &val, 1);
		if (rc < 0)
			return rc;

		haptics_runtime_autosuspend_put(chip);
		dev_dbg(chip->dev, "QCOM_SSR_BEFORE_POWERUP is handled\n");
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int haptics_probe(struct platform_device *pdev)
{
	struct haptics_chip *chip;
	struct input_dev *input_dev;
	struct ff_device *ff_dev;
	int rc, count;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(&pdev->dev);
	if (!input_dev)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	dev_set_drvdata(chip->dev, chip);
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Get regmap failed\n");
		return -ENXIO;
	}

	INIT_LIST_HEAD(&chip->mmap_effect_list);
	rc = haptics_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Parse device-tree failed, rc = %d\n", rc);
		return rc;
	}

	rc = haptics_init_custom_effect(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Init custom effect failed, rc=%d\n", rc);
		return rc;
	}

	mutex_init(&chip->vmax_lock);

	rc = haptics_hw_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Initialize HW failed, rc = %d\n", rc);
		return rc;
	}

	rc = haptics_runtime_pm_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Runtime PM initialize failed, rc=%d\n", rc);
		return rc;
	}

	if (is_swr_supported(chip)) {
		rc = haptics_init_swr_slave_regulator(chip);
		if (rc < 0) {
			dev_err(chip->dev, "Initialize swr slave regulator failed, rc = %d\n",
					rc);
			return rc;
		}
	}

	rc = devm_request_threaded_irq(chip->dev, chip->fifo_empty_irq,
			NULL, fifo_empty_irq_handler,
			IRQF_ONESHOT, "fifo-empty", chip);
	if (rc < 0) {
		dev_err(chip->dev, "request fifo-empty IRQ failed, rc=%d\n",
				rc);
		return rc;
	}

	mutex_init(&chip->play.lock);
	disable_irq_nosync(chip->fifo_empty_irq);
	chip->fifo_empty_irq_en = false;
	hrtimer_init(&chip->hbst_off_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->hbst_off_timer.function = haptics_disable_hbst_timer;
	INIT_DELAYED_WORK(&chip->stop_work, haptics_stop_constant_effect_play);
	INIT_WORK(&chip->set_gain_work, haptics_set_gain_work);

	atomic_set(&chip->play.fifo_status.is_busy, 0);
	atomic_set(&chip->play.fifo_status.written_done, 0);
	atomic_set(&chip->play.fifo_status.cancelled, 0);
	input_dev->name = "qcom-hv-haptics";
	input_set_drvdata(input_dev, chip);
	chip->input_dev = input_dev;

	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	if ((chip->effects_count != 0) || (chip->primitives_count != 0)) {
		input_set_capability(input_dev, EV_FF, FF_PERIODIC);
		input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	}

	if (chip->effects_count + chip->primitives_count > MAX_EFFECT_COUNT)
		dev_err(chip->dev, "Effects count cannot be more than %d\n", MAX_EFFECT_COUNT);

	count = min_t(u32, chip->effects_count + chip->primitives_count + 1, MAX_EFFECT_COUNT);

	rc = input_ff_create(input_dev, count);
	if (rc < 0) {
		dev_err(chip->dev, "create input FF device failed, rc=%d\n",
				rc);
		return rc;
	}

	ff_dev = input_dev->ff;
	ff_dev->upload = haptics_upload_effect;
	ff_dev->playback = haptics_playback;
	ff_dev->erase = haptics_erase;
	ff_dev->set_gain = haptics_set_gain;
	rc = input_register_device(input_dev);
	if (rc < 0) {
		dev_err(chip->dev, "register input device failed, rc=%d\n",
				rc);
		goto destroy_ff;
	}

	chip->hap_class.name = "qcom-haptics";
	chip->hap_class.class_groups = hap_class_groups;
	rc = class_register(&chip->hap_class);
	if (rc < 0) {
		dev_err(chip->dev, "register hap_class failed, rc=%d\n", rc);
		goto destroy_ff;
	}

	if (chip->hw_type >= HAP530_HV && chip->visense_enabled) {
		rc = class_create_file(&chip->hap_class, &class_attr_i_gain_error);
		if (rc) {
			dev_err(chip->dev, "failed to add i_gain_error, rc=%d\n", rc);
			goto destroy_ff;
		}

		rc = class_create_file(&chip->hap_class, &class_attr_v_gain_error);
		if (rc) {
			dev_err(chip->dev, "failed to add v_gain_error, rc=%d\n", rc);
			goto remove_i_gain_error;
		}
	}

	chip->hboost_nb.notifier_call = haptics_boost_notifier;
	register_hboost_event_notifier(&chip->hboost_nb);

	if (chip->hw_type == HAP530_HV) {
		chip->lpass_ssr_nb.notifier_call = haptics_lpass_ssr_notifier;
		chip->lpass_ssr_handle =
			qcom_register_ssr_notifier("lpass", &chip->lpass_ssr_nb);
		if (IS_ERR(chip->lpass_ssr_handle)) {
			rc = PTR_ERR(chip->lpass_ssr_handle);
			dev_err(chip->dev, "register SSR notifier failed, rc=%d\n", rc);
			if (chip->visense_enabled)
				goto remove_v_gain_error;
			else
				goto destroy_ff;
		}
	}

	rc = haptics_create_debugfs(chip);
	if (rc < 0)
		dev_err(chip->dev, "Creating debugfs failed, rc=%d\n", rc);

	haptics_runtime_autosuspend(chip);
	phapchip = chip;
	return 0;
remove_v_gain_error:
	class_remove_file(&chip->hap_class, &class_attr_v_gain_error);
remove_i_gain_error:
	class_remove_file(&chip->hap_class, &class_attr_i_gain_error);
destroy_ff:
	input_ff_destroy(chip->input_dev);
	return rc;
}

static int haptics_remove(struct platform_device *pdev)
{
	struct haptics_chip *chip = dev_get_drvdata(&pdev->dev);

	phapchip = NULL;
	if (chip->pbs_node)
		of_node_put(chip->pbs_node);

	if (chip->hw_type == HAP530_HV)
		qcom_unregister_ssr_notifier(chip->lpass_ssr_handle, &chip->lpass_ssr_nb);

	unregister_hboost_event_notifier(&chip->hboost_nb);
	class_unregister(&chip->hap_class);
	haptics_remove_debugfs(chip);
	input_unregister_device(chip->input_dev);
	dev_set_drvdata(chip->dev, NULL);

	return 0;
}

static void haptics_ds_suspend_config(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);

	if (chip->fifo_empty_irq > 0)
		devm_free_irq(dev, chip->fifo_empty_irq, chip);
}

static int haptics_ds_resume_config(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	rc = haptics_hw_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Initialize HW failed, rc = %d\n", rc);
		return rc;
	}

	if (chip->fifo_empty_irq > 0) {
		rc = devm_request_threaded_irq(chip->dev, chip->fifo_empty_irq,
					NULL, fifo_empty_irq_handler,
					IRQF_ONESHOT, "fifo-empty", chip);
		if (rc < 0) {
			dev_err(chip->dev, "request fifo-empty IRQ failed, rc=%d\n",
					rc);
			return rc;
		}

		disable_irq_nosync(chip->fifo_empty_irq);
		chip->fifo_empty_irq_en = false;
	}

	return rc;
}

static int haptics_suspend_config(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);
	struct haptics_play_info *play = &chip->play;
	int rc;

	mutex_lock(&play->lock);
	if ((play->pattern_src == FIFO) &&
			atomic_read(&play->fifo_status.is_busy)) {
		if (atomic_read(&play->fifo_status.written_done) == 0) {
			dev_dbg(chip->dev, "cancelling FIFO playing\n");
			atomic_set(&play->fifo_status.cancelled, 1);
		}

		rc = haptics_stop_fifo_play(chip);
		if (rc < 0) {
			dev_err(chip->dev, "stop FIFO playing failed, rc=%d\n", rc);
			mutex_unlock(&play->lock);
			return rc;
		}
	} else {
		rc = haptics_enable_play(chip, false);
		if (rc < 0) {
			mutex_unlock(&play->lock);
			return rc;
		}
	}
	mutex_unlock(&play->lock);

	/*
	 * Cancel the hBoost turning off timer and disable
	 * hBoost if it's still enabled
	 */
	if (chip->wa_flags & SW_CTRL_HBST) {
		hrtimer_cancel(&chip->hbst_off_timer);
		haptics_boost_vreg_enable(chip, false);
	}

	rc = haptics_enable_hpwr_vreg(chip, false);
	if (rc < 0)
		return rc;

	return haptics_module_enable(chip, false);
}

static int __maybe_unused haptics_runtime_suspend(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);

	/*
	 * Don't check return values, disable haptics module
	 * to minimize the quiescent current.
	 */
	haptics_visense_enable(chip, false);
	haptics_module_enable(chip, false);

	return 0;
}

static int __maybe_unused haptics_runtime_resume(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	rc = haptics_module_enable(chip, true);
	if (rc < 0)
		return rc;

	return haptics_visense_enable(chip, true);
}

static int __maybe_unused haptics_suspend(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	if (MAJOR_REV(chip->cfg_revision) == HAP_CFG_V1)
		return 0;

	rc = haptics_suspend_config(dev);
	if (rc < 0)
		return rc;

#ifdef CONFIG_DEEPSLEEP
	if (mem_sleep_current == PM_SUSPEND_MEM)
		haptics_ds_suspend_config(dev);
#endif

	return 0;
}

static int __maybe_unused haptics_resume(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);

#ifdef CONFIG_DEEPSLEEP
	if (mem_sleep_current == PM_SUSPEND_MEM) {
		int rc = 0;

		rc = haptics_ds_resume_config(dev);
		if (rc < 0)
			return rc;
	}
#endif

	if (is_haptics_runtime_pm_enabled(chip))
		return 0;

	return haptics_module_enable(chip, true);
}

static int __maybe_unused haptics_freeze(struct device *dev)
{
	int rc = 0;

	rc = haptics_suspend_config(dev);
	if (rc < 0)
		return rc;

	haptics_ds_suspend_config(dev);

	return 0;
}

static int __maybe_unused haptics_restore(struct device *dev)
{
	struct haptics_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	rc = haptics_ds_resume_config(dev);
	if (rc < 0)
		return rc;

	if (is_haptics_runtime_pm_enabled(chip))
		return 0;

	return haptics_module_enable(chip, true);

}

static void haptics_shutdown(struct platform_device *pdev)
{
	struct haptics_chip *chip = platform_get_drvdata(pdev);

	haptics_suspend_config(chip->dev);

	haptics_ds_suspend_config(chip->dev);
}

static const struct dev_pm_ops __maybe_unused haptics_pm_ops = {
	.runtime_suspend = pm_ptr(haptics_runtime_suspend),
	.runtime_resume = pm_ptr(haptics_runtime_resume),
	.suspend = pm_ptr(haptics_suspend),
	.resume = pm_ptr(haptics_resume),
	.freeze = pm_ptr(haptics_freeze),
	.restore = pm_ptr(haptics_restore),
};

static const struct of_device_id haptics_match_table[] = {
	{
		.compatible = "qcom,hv-haptics",
	},
	{
		.compatible = "qcom,pm8350b-haptics",
	},
	{
		.compatible = "qcom,pm5100-haptics",
	},
	{},
};

static struct platform_driver haptics_driver = {
	.driver		= {
		.name = "qcom-hv-haptics",
		.of_match_table = haptics_match_table,
		.pm		= pm_ptr(&haptics_pm_ops),
	},
	.probe		= haptics_probe,
	.shutdown	= haptics_shutdown,
	.remove		= haptics_remove,
};
module_platform_driver(haptics_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. High-Voltage Haptics driver");
MODULE_LICENSE("GPL");
