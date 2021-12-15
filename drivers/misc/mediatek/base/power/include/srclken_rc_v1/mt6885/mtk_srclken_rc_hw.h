/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
*/

/**
 * @file    mtk_srclken_rc_hw.h
 * @brief   Driver for subys request resource control
 *
 */
#ifndef __MTK_SRCLKEN_RC_HW_H__
#define __MTK_SRCLKEN_RC_HW_H__

#define SRCLKEN_DBG				0
#define RC_GPIO_DBG_ENABLE			0

#define SRCLKEN_RC_EN_SHFT			0

#define FORCE_SRCVOLTEN_OFF_MSK			0x1
#define FORCE_SRCVOLTEN_OFF_SHFT		10
#define FORCE_SRCVOLTEN_ON_MSK			0x1
#define FORCE_SRCVOLTEN_ON_SHFT			11
#define ULPOSC_CTRL_M_MSK			0xf
#define ULPOSC_CTRL_M_SHFT			12
#define FORCE_VCORE_RDY_MSK			0x1
#define FORCE_VCORE_RDY_SHFT			16
#define FORCE_ULPOSC2ON_MSK			0x1
#define FORCE_ULPOSC2ON_SHFT			17
#define FORCE_ULPOSC_CLK_EN_MSK		0x1
#define FORCE_ULPOSC_CLK_EN_SHFT		18
#define FORCE_ULPOSC_ON_MSK			0x1
#define FORCE_ULPOSC_ON_SHFT			19
#define DIS_ULPOSC_RDY_CHK_MSK			0x1
#define DIS_ULPOSC_RDY_CHK_SHFT			20
#define PWRAP_SLP_CTRL_M_MSK			0xf
#define PWRAP_SLP_CTRL_M_SHFT			21
#define PWRAP_SLP_MUX_SEL_MSK			0x1
#define PWRAP_SLP_MUX_SEL_SHFT			25
#define FORCE_PWRAP_ON_MSK			0x1
#define FORCE_PWRAP_ON_SHFT			26
#define FORCE_PWRAP_AWK_MSK			0x1
#define FORCE_PWRAP_AWK_SHFT			27
#define NON_DCXO_REQ_FORCEON_MSK		0x1
#define NON_DCXO_REQ_FORCEON_SHFT		28
#define NON_DCXO_REQ_FORCEOFF_MSK		0x1
#define NON_DCXO_REQ_FORCEOFF_SHFT		29
#define DCXO_REQ_FORCEON_MSK			0x1
#define DCXO_REQ_FORCEON_SHFT			30
#define DCXO_REQ_FORCEOFF_MSK			0x1
#define DCXO_REQ_FORCEOFF_SHFT			31

#define SW_SRCLKEN_RC_SHFT			3
#define SW_SRCLKEN_FPM_SHFT			4
#define SW_SRCLKEN_BBLPM_SHFT			5
#define SW_SRCLKEN_RC_MSK			0x7

#define ANY_REQ_32K				0x1
#define ANY_BYS_REQ				0x2
#define ANY_ND_REQ				0x4
#define ANY_DCXO_REQ				0x8
#define VCORE_ULPOSC_STA_MSK			0x1F
#define VCORE_ULPOSC_STA_SHFT			4
#define WRAP_CMD_STA_MSK			0x7FF
#define WRAP_CMD_STA_SHFT			9
#define ULPOSC_STA_MSK				0x7
#define ULPOSC_STA_SHFT				20
#define WRAP_SLP_STA_MSK			0x1F
#define WRAP_SLP_STA_SHFT			23
#define SPI_STA_MSK				0xF
#define SPI_STA_SHFT				28

#define PMIC_PMRC_EN_MSK			0x1FFF
#define PMIC_PMRC_EN_SHFT			0
#define PMIC_DCXO_MSK				0x7
#define PMIC_DCXO_SHFT				13
#define TAR_CMD_ARB_MSK			0xFFFF
#define TAR_CMD_ARB_SHFT			16

#define TAR_XO_REQ_MSK				0x1FFF
#define TAR_XO_REQ_SHFT				0
#define LATCH_REQ_M0				(0x1 << 13)
#define LATCH_REQ_M1				(0x1 << 14)
#define LATCH_REQ_M2				(0x1 << 15)
#define TAR_DCXO_REQ_MSK			0x7
#define TAR_DCXO_REQ_SHFT			26
#define LATCH_REQ_M9				(0x1 << 29)

#define SPI_CMD_REQ				0x1
#define SPI_CMD_REQ_ACK				0x2
#define SPI_CMD_ADDR_MSK			0x1FFF
#define SPI_CMD_ADDR_SHFT			2
#define SPI_CMD_DATA_MSK			0xFFFF
#define SPI_CMD_DATA_SHFT			16

#define SPM_O0_ACK				(0x1 << 0)
#define SPM_O0					(0x1 << 1)
#define SPM_VREQ_MASK_B			(0x1 << 2)
#define SCP_VREQ_WRAP				(0x1 << 3)
#define SCP_VREQ_ACK				(0x1 << 4)
#define SCP_VREQ					(0x1 << 5)
#define SCP_WRAP_SLP_ACK			(0x1 << 6)
#define SCP_WRAP_SLP				(0x1 << 7)
#define SCP_ULPOSC_CK				(0x1 << 8)
#define SCP_ULPOSC_RST				(0x1 << 9)
#define SCP_ULPOSC_EN				(0x1 << 10)
#define RC_VREQ					(0x1 << 11)
#define RC_O0					(0x1 << 12)
#define RC_VREQ_WRAP				(0x1 << 13)
#define RC_WRAP_SLP_ACK				(0x1 << 14)
#define RC_WRAP_SLP				(0x1 << 15)
#define SCP_ULPOSC_CK_ACK			(0x1 << 16)
#define SCP_ULPOSC_RST_ACK			(0x1 << 17)
#define SCP_ULPOSC_EN_ACK			(0x1 << 18)
#define AP_26M_RDY				(0x1 << 19)
#define ERR_TRIG					(0x1 << 20)
#define WRAP_WK_TO				(0x1 << 21)
#define WRAP_SLP_TO				(0x1 << 22)
#define LATCH_REQ_M4				(0x1 << 23)
#define LATCH_REQ_M5				(0x1 << 24)
#define LATCH_REQ_M6				(0x1 << 25)
#define LATCH_REQ_M7				(0x1 << 26)
#define LATCH_REQ_M8				(0x1 << 27)
#define LATCH_REQ_M10				(0x1 << 28)
#define LATCH_REQ_M11				(0x1 << 29)

#define FPM_MSK					0x1
#define FPM_SHFT				0
#define FPM_ACK_MSK				0x1
#define FPM_ACK_SHFT				1
#define BBLPM_MSK				0x1
#define BBLPM_SHFT				2
#define BBLPM_ACK_MSK				0x1
#define BBLPM_ACK_SHFT				3
#define INQUEUE_32K_MSK				0x1
#define INQUEUE_32K_SHFT				4
#define ALLOW_REQ_MSK				0x1
#define ALLOW_REQ_SHFT				5
#define CUR_RC_MSK				0x1
#define CUR_RC_SHFT				6
#define CUR_DCXO_MSK				0x7
#define CUR_DCXO_SHFT				7
#define TAR_DCXO_MSK				0x7
#define TAR_DCXO_SHFT				10
#define REQ_ONGO_MSK				0x1
#define REQ_ONGO_SHFT				13
#define TAR_BBLPM_MSK				0x1
#define TAR_BBLPM_SHFT				14
#define TAR_FPM_MSK				0x1
#define TAR_FPM_SHFT				15
#define DCXO_EQ_MSK				0x1
#define DCXO_EQ_SHFT				16
#define DCXO_CHG_MSK				0x1
#define DCXO_CHG_SHFT				17
#define REQ_FILT_MSK				0x7
#define REQ_FILT_SHFT				18
#define CUR_REQ_STA_MSK				0x1FF
#define CUR_REQ_STA_SHFT			21
#define CMD_OK_MSK				0x3
#define CMD_OK_SHFT				30

#define SPM_AP_26M_RDY				(0x1 << 0)
#define KEEP_RC_SPI_ACTIVE			(0x1 << 1)

#define DBG_INFO_MSK				0x7
#define DBG_INFO_SHFT				3
#define DBG_SPI_ADDR_MSK			0x1FF
#define DBG_SPI_ADDR_SHFT			7
#define DBG_SPI_DATA_MSK			0xFFFF
#define DBG_SPI_DATA_SHFT			16

enum sys_id {
	SYS_SUSPEND = 0,
	SYS_RF = 1,
	SYS_DPIDLE = 2,
	SYS_MD = 3,
	SYS_GPS = 4,
	SYS_BT = 5,
	SYS_WIFI = 6,
	SYS_MCU = 7,
	SYS_COANT = 8,
	SYS_NFC = 9,
	SYS_UFS = 10,
	SYS_SCP = 11,
	SYS_RSV = 12,
	MAX_SYS_NUM,
};

enum dcxo_m {
	RC_DCXO_FPM = 0,
	RC_DCXO_BBLPM = 1,
	RC_DCXO_LPM = 2,
	RC_DCXO_M_NUM,
};

enum rc_ctrl_m {
	HW_MODE = 0 << SW_SRCLKEN_RC_SHFT,
	SW_MODE = 1 << SW_SRCLKEN_RC_SHFT,
};

enum rc_ctrl_r {
	OFF_REQ = 0,
	NO_REQ = 1,
	FPM_REQ = 1 << SW_SRCLKEN_FPM_SHFT,
	BBLPM_REQ = 1 << SW_SRCLKEN_BBLPM_SHFT,
	MAX_REQ_TYPE = BBLPM_REQ + 1,
};

#endif /* __MTK_SRCLKEN_RC_HW_H__ */

