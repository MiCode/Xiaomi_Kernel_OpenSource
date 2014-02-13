/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef UFS_MSM_H_
#define UFS_MSM_H_

#define MAX_U32                 (~(u32)0)
#define MPHY_TX_FSM_STATE       0x41
#define TX_FSM_HIBERN8          0x1
#define HBRN8_POLL_TOUT_MS      100
#define DEFAULT_CLK_RATE_HZ     1000000
#define BUS_VECTOR_NAME_LEN     32

/* MSM UFS host controller vendor specific registers */
enum {
	REG_UFS_SYS1CLK_1US                 = 0xC0,
	REG_UFS_TX_SYMBOL_CLK_NS_US         = 0xC4,
	REG_UFS_LOCAL_PORT_ID_REG           = 0xC8,
	REG_UFS_PA_ERR_CODE                 = 0xCC,
	REG_UFS_RETRY_TIMER_REG             = 0xD0,
	REG_UFS_PA_LINK_STARTUP_TIMER       = 0xD8,
	REG_UFS_CFG1                        = 0xDC,
	REG_UFS_CFG2                        = 0xE0,
	REG_UFS_HW_VERSION                  = 0xE4,
};

/* bit offset */
enum {
	OFFSET_UFS_PHY_SOFT_RESET           = 1,
	OFFSET_CLK_NS_REG                   = 10,
};

/* bit masks */
enum {
	MASK_UFS_PHY_SOFT_RESET             = 0x2,
	MASK_TX_SYMBOL_CLK_1US_REG          = 0x3FF,
	MASK_CLK_NS_REG                     = 0xFFFC00,
};

static LIST_HEAD(phy_list);

struct msm_ufs_phy_calibration {
	u32 reg_offset;
	u32 cfg_value;
};

struct msm_ufs_stored_attributes {
	u32 att;
	u32 value;
};

enum msm_ufs_phy_init_type {
	UFS_PHY_INIT_FULL,
	UFS_PHY_INIT_CFG_RESTORE,
};

struct msm_ufs_phy_vreg {
	const char *name;
	struct regulator *reg;
	int max_uA;
	int min_uV;
	int max_uV;
	bool enabled;
};

struct msm_ufs_phy {
	struct list_head list;
	struct device *dev;
	void __iomem *mmio;
	struct clk *tx_iface_clk;
	struct clk *rx_iface_clk;
	bool is_iface_clk_enabled;
	struct clk *ref_clk_src;
	struct clk *ref_clk_parent;
	struct clk *ref_clk;
	bool is_ref_clk_enabled;
	struct msm_ufs_phy_vreg vdda_pll;
	struct msm_ufs_phy_vreg vdda_phy;
	unsigned int quirks;
	/*
	 * As part of UFS power management, UFS link would be put in hibernate
	 * and UFS device would be put in SLEEP mode as part of runtime/system
	 * suspend callback. But when system goes into suspend with VDD
	 * minimization, UFS PHY states are being reset which means UFS link
	 * hibernate exit command on system resume would fail.
	 * If this quirk is enabled then above issue is workaround by saving
	 * the UFS PHY state information before system goes into suspend and
	 * restoring the saved state information during system resume but
	 * before executing the hibern8 exit command.
	 * Note that this quirk will help restoring the PHY state if even when
	 * link in not kept in hibern8 during suspend.
	 *
	 * Here is the list of steps to save/restore the configuration:
	 * Before entering into system suspend:
	 *	1. Read Critical PCS SWI Registers  + less critical PHY CSR
	 *	2. Read RMMI Attributes
	 * Enter into system suspend
	 * After exiting from system suspend:
	 *	1. Set UFS_PHY_SOFT_RESET bit in UFS_CFG1 register of the UFS
	 *	   Controller
	 *	2. Write 0x01 to the UFS_PHY_POWER_DOWN_CONTROL register in the
	 *	   UFS PHY
	 *	3. Write back the values of the PHY SWI registers
	 *	4. Clear UFS_PHY_SOFT_RESET bit in UFS_CFG1 register of the UFS
	 *	   Controller
	 *	5. Write 0x01 to the UFS_PHY_PHY_START in the UFS PHY. This will
	 *	   start the PLL calibration and bring-up of the PHY.
	 *	6. Write back the values to the PHY RMMI Attributes
	 *	7. Wait for UFS_PHY_PCS_READY_STATUS[0] to be '1'
	 */
	#define MSM_UFS_PHY_QUIRK_CFG_RESTORE		(1 << 0)
};

struct msm_ufs_bus_vote {
	uint32_t client_handle;
	uint32_t curr_vote;
	int min_bw_vote;
	int max_bw_vote;
	int saved_vote;
	bool is_max_bw_needed;
	struct device_attribute max_bus_bw;
};

struct msm_ufs_host {
	struct msm_ufs_phy *phy;
	struct ufs_hba *hba;
	struct msm_ufs_bus_vote bus_vote;
	struct ufs_pa_layer_attr dev_req_params;
	struct clk *rx_l0_sync_clk;
	struct clk *tx_l0_sync_clk;
	struct clk *rx_l1_sync_clk;
	struct clk *tx_l1_sync_clk;
	bool is_lane_clks_enabled;
};

static int msm_ufs_update_bus_bw_vote(struct msm_ufs_host *host);

/* MSM UFS PHY control registers */

#define COM_OFF(x)     (0x000 + x)
#define PHY_OFF(x)     (0x700 + x)
#define TX_OFF(n, x)   (0x100 + (0x400 * n) + x)
#define RX_OFF(n, x)   (0x200 + (0x400 * n) + x)

/* UFS PHY PLL block registers */
#define QSERDES_COM_SYS_CLK_CTRL                            COM_OFF(0x00)
#define QSERDES_COM_PLL_VCOTAIL_EN                          COM_OFF(0x04)
#define QSERDES_COM_CMN_MODE                                COM_OFF(0x08)
#define QSERDES_COM_IE_TRIM                                 COM_OFF(0x0C)
#define QSERDES_COM_IP_TRIM                                 COM_OFF(0x10)
#define QSERDES_COM_PLL_CNTRL                               COM_OFF(0x14)
#define QSERDES_COM_PLL_IP_SETI                             COM_OFF(0x18)
#define QSERDES_COM_CORE_CLK_IN_SYNC_SEL                    COM_OFF(0x1C)
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN                     COM_OFF(0x20)
#define QSERDES_COM_PLL_CP_SETI                             COM_OFF(0x24)
#define QSERDES_COM_PLL_IP_SETP                             COM_OFF(0x28)
#define QSERDES_COM_PLL_CP_SETP                             COM_OFF(0x2C)
#define QSERDES_COM_ATB_SEL1                                COM_OFF(0x30)
#define QSERDES_COM_ATB_SEL2                                COM_OFF(0x34)
#define QSERDES_COM_SYSCLK_EN_SEL                           COM_OFF(0x38)
#define QSERDES_COM_RES_CODE_TXBAND                         COM_OFF(0x3C)
#define QSERDES_COM_RESETSM_CNTRL                           COM_OFF(0x40)
#define QSERDES_COM_PLLLOCK_CMP1                            COM_OFF(0x44)
#define QSERDES_COM_PLLLOCK_CMP2                            COM_OFF(0x48)
#define QSERDES_COM_PLLLOCK_CMP3                            COM_OFF(0x4C)
#define QSERDES_COM_PLLLOCK_CMP_EN                          COM_OFF(0x50)
#define QSERDES_COM_RES_TRIM_OFFSET                         COM_OFF(0x54)
#define QSERDES_COM_BGTC                                    COM_OFF(0x58)
#define QSERDES_COM_PLL_TEST_UPDN_RESTRIMSTEP               COM_OFF(0x5C)
#define QSERDES_COM_PLL_VCO_TUNE                            COM_OFF(0x60)
#define QSERDES_COM_DEC_START1                              COM_OFF(0x64)
#define QSERDES_COM_PLL_AMP_OS                              COM_OFF(0x68)
#define QSERDES_COM_SSC_EN_CENTER                           COM_OFF(0x6C)
#define QSERDES_COM_SSC_ADJ_PER1                            COM_OFF(0x70)
#define QSERDES_COM_SSC_ADJ_PER2                            COM_OFF(0x74)
#define QSERDES_COM_SSC_PER1                                COM_OFF(0x78)
#define QSERDES_COM_SSC_PER2                                COM_OFF(0x7C)
#define QSERDES_COM_SSC_STEP_SIZE1                          COM_OFF(0x80)
#define QSERDES_COM_SSC_STEP_SIZE2                          COM_OFF(0x84)
#define QSERDES_COM_RES_TRIM_SEARCH                         COM_OFF(0x88)
#define QSERDES_COM_RES_TRIM_FREEZE                         COM_OFF(0x8C)
#define QSERDES_COM_RES_TRIM_EN_VCOCALDONE                  COM_OFF(0x90)
#define QSERDES_COM_FAUX_EN                                 COM_OFF(0x94)
#define QSERDES_COM_DIV_FRAC_START1                         COM_OFF(0x98)
#define QSERDES_COM_DIV_FRAC_START2                         COM_OFF(0x9C)
#define QSERDES_COM_DIV_FRAC_START3                         COM_OFF(0xA0)
#define QSERDES_COM_DEC_START2                              COM_OFF(0xA4)
#define QSERDES_COM_PLL_RXTXEPCLK_EN                        COM_OFF(0xA8)
#define QSERDES_COM_PLL_CRCTRL                              COM_OFF(0xAC)
#define QSERDES_COM_PLL_CLKEPDIV                            COM_OFF(0xB0)
#define QSERDES_COM_PLL_FREQUPDATE                          COM_OFF(0xB4)
#define QSERDES_COM_PLL_VCO_HIGH                            COM_OFF(0xB8)
#define QSERDES_COM_RESET_SM                                COM_OFF(0xBC)

/* UFS PHY registers */
#define UFS_PHY_PHY_START                                   PHY_OFF(0x00)
#define UFS_PHY_POWER_DOWN_CONTROL                          PHY_OFF(0x04)
#define UFS_PHY_PWM_G1_CLK_DIVIDER                          PHY_OFF(0x08)
#define UFS_PHY_PWM_G2_CLK_DIVIDER                          PHY_OFF(0x0C)
#define UFS_PHY_PWM_G3_CLK_DIVIDER                          PHY_OFF(0x10)
#define UFS_PHY_PWM_G4_CLK_DIVIDER                          PHY_OFF(0x14)
#define UFS_PHY_TIMER_100US_SYSCLK_STEPS_MSB                PHY_OFF(0x18)
#define UFS_PHY_TIMER_100US_SYSCLK_STEPS_LSB                PHY_OFF(0x1C)
#define UFS_PHY_TIMER_20US_CORECLK_STEPS_MSB                PHY_OFF(0x20)
#define UFS_PHY_TIMER_20US_CORECLK_STEPS_LSB                PHY_OFF(0x24)
#define UFS_PHY_LINE_RESET_TIME                             PHY_OFF(0x28)
#define UFS_PHY_LINE_RESET_GRANULARITY                      PHY_OFF(0x2C)
#define UFS_PHY_CONTROLSYM_ONE_HOT_DISABLE                  PHY_OFF(0x30)
#define UFS_PHY_CORECLK_PWM_G1_CLK_DIVIDER                  PHY_OFF(0x34)
#define UFS_PHY_CORECLK_PWM_G2_CLK_DIVIDER                  PHY_OFF(0x38)
#define UFS_PHY_CORECLK_PWM_G3_CLK_DIVIDER                  PHY_OFF(0x3C)
#define UFS_PHY_CORECLK_PWM_G4_CLK_DIVIDER                  PHY_OFF(0x40)
#define UFS_PHY_TX_LANE_ENABLE                              PHY_OFF(0x44)
#define UFS_PHY_TSYNC_RSYNC_CNTL                            PHY_OFF(0x48)
#define UFS_PHY_RETIME_BUFFER_EN                            PHY_OFF(0x4C)
#define UFS_PHY_PLL_CNTL                                    PHY_OFF(0x50)
#define UFS_PHY_TX_LARGE_AMP_DRV_LVL                        PHY_OFF(0x54)
#define UFS_PHY_TX_LARGE_AMP_POST_EMP_LVL                   PHY_OFF(0x58)
#define UFS_PHY_TX_SMALL_AMP_DRV_LVL                        PHY_OFF(0x5C)
#define UFS_PHY_TX_SMALL_AMP_POST_EMP_LVL                   PHY_OFF(0x60)
#define UFS_PHY_CFG_CHANGE_CNT_VAL                          PHY_OFF(0x64)
#define UFS_PHY_OMC_STATUS_RDVAL                            PHY_OFF(0x68)
#define UFS_PHY_RX_SYNC_WAIT_TIME                           PHY_OFF(0x6C)
#define UFS_PHY_L0_BIST_CTRL                                PHY_OFF(0x70)
#define UFS_PHY_L1_BIST_CTRL                                PHY_OFF(0x74)
#define UFS_PHY_BIST_PRBS_POLY0                             PHY_OFF(0x78)
#define UFS_PHY_BIST_PRBS_POLY1                             PHY_OFF(0x7C)
#define UFS_PHY_BIST_PRBS_SEED0                             PHY_OFF(0x80)
#define UFS_PHY_BIST_PRBS_SEED1                             PHY_OFF(0x84)
#define UFS_PHY_BIST_FIXED_PAT_CTRL                         PHY_OFF(0x88)
#define UFS_PHY_BIST_FIXED_PAT0_DATA                        PHY_OFF(0x8C)
#define UFS_PHY_BIST_FIXED_PAT1_DATA                        PHY_OFF(0x90)
#define UFS_PHY_BIST_FIXED_PAT2_DATA                        PHY_OFF(0x94)
#define UFS_PHY_BIST_FIXED_PAT3_DATA                        PHY_OFF(0x98)
#define UFS_PHY_TX_HSGEAR_CAPABILITY                        PHY_OFF(0x9C)
#define UFS_PHY_TX_PWMGEAR_CAPABILITY                       PHY_OFF(0xA0)
#define UFS_PHY_TX_AMPLITUDE_CAPABILITY                     PHY_OFF(0xA4)
#define UFS_PHY_TX_EXTERNALSYNC_CAPABILITY                  PHY_OFF(0xA8)
#define UFS_PHY_TX_HS_UNTERMINATED_LINE_DRIVE_CAPABILITY    PHY_OFF(0xAC)
#define UFS_PHY_TX_LS_TERMINATED_LINE_DRIVE_CAPABILITY      PHY_OFF(0xB0)
#define UFS_PHY_TX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xB4)
#define UFS_PHY_TX_MIN_STALL_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xB8)
#define UFS_PHY_TX_MIN_SAVE_CONFIG_TIME_CAPABILITY          PHY_OFF(0xBC)
#define UFS_PHY_TX_REF_CLOCK_SHARED_CAPABILITY              PHY_OFF(0xC0)
#define UFS_PHY_TX_PHY_MAJORMINOR_RELEASE_CAPABILITY        PHY_OFF(0xC4)
#define UFS_PHY_TX_PHY_EDITORIAL_RELEASE_CAPABILITY         PHY_OFF(0xC8)
#define UFS_PHY_TX_HIBERN8TIME_CAPABILITY                   PHY_OFF(0xCC)
#define UFS_PHY_RX_HSGEAR_CAPABILITY                        PHY_OFF(0xD0)
#define UFS_PHY_RX_PWMGEAR_CAPABILITY                       PHY_OFF(0xD4)
#define UFS_PHY_RX_HS_UNTERMINATED_CAPABILITY               PHY_OFF(0xD8)
#define UFS_PHY_RX_LS_TERMINATED_CAPABILITY                 PHY_OFF(0xDC)
#define UFS_PHY_RX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xE0)
#define UFS_PHY_RX_MIN_STALL_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xE4)
#define UFS_PHY_RX_MIN_SAVE_CONFIG_TIME_CAPABILITY          PHY_OFF(0xE8)
#define UFS_PHY_RX_REF_CLOCK_SHARED_CAPABILITY              PHY_OFF(0xEC)
#define UFS_PHY_RX_HS_G1_SYNC_LENGTH_CAPABILITY             PHY_OFF(0xF0)
#define UFS_PHY_RX_HS_G1_PREPARE_LENGTH_CAPABILITY          PHY_OFF(0xF4)
#define UFS_PHY_RX_LS_PREPARE_LENGTH_CAPABILITY             PHY_OFF(0xF8)
#define UFS_PHY_RX_PWM_BURST_CLOSURE_LENGTH_CAPABILITY      PHY_OFF(0xFC)
#define UFS_PHY_RX_MIN_ACTIVATETIME_CAPABILITY              PHY_OFF(0x100)
#define UFS_PHY_RX_PHY_MAJORMINOR_RELEASE_CAPABILITY        PHY_OFF(0x104)
#define UFS_PHY_RX_PHY_EDITORIAL_RELEASE_CAPABILITY         PHY_OFF(0x108)
#define UFS_PHY_RX_HIBERN8TIME_CAPABILITY                   PHY_OFF(0x10C)
#define UFS_PHY_RX_HS_G2_SYNC_LENGTH_CAPABILITY             PHY_OFF(0x110)
#define UFS_PHY_RX_HS_G3_SYNC_LENGTH_CAPABILITY             PHY_OFF(0x114)
#define UFS_PHY_RX_HS_G2_PREPARE_LENGTH_CAPABILITY          PHY_OFF(0x118)
#define UFS_PHY_RX_HS_G3_PREPARE_LENGTH_CAPABILITY          PHY_OFF(0x11C)
#define UFS_PHY_DEBUG_BUS_SEL                               PHY_OFF(0x120)
#define UFS_PHY_DEBUG_BUS_0_STATUS_CHK                      PHY_OFF(0x124)
#define UFS_PHY_DEBUG_BUS_1_STATUS_CHK                      PHY_OFF(0x128)
#define UFS_PHY_DEBUG_BUS_2_STATUS_CHK                      PHY_OFF(0x12C)
#define UFS_PHY_DEBUG_BUS_3_STATUS_CHK                      PHY_OFF(0x130)
#define UFS_PHY_PCS_READY_STATUS                            PHY_OFF(0x134)
#define UFS_PHY_L0_BIST_CHK_ERR_CNT_L_STATUS                PHY_OFF(0x138)
#define UFS_PHY_L0_BIST_CHK_ERR_CNT_H_STATUS                PHY_OFF(0x13C)
#define UFS_PHY_L1_BIST_CHK_ERR_CNT_L_STATUS                PHY_OFF(0x140)
#define UFS_PHY_L1_BIST_CHK_ERR_CNT_H_STATUS                PHY_OFF(0x144)
#define UFS_PHY_L0_BIST_CHK_STATUS                          PHY_OFF(0x148)
#define UFS_PHY_L1_BIST_CHK_STATUS                          PHY_OFF(0x14C)
#define UFS_PHY_DEBUG_BUS_0_STATUS                          PHY_OFF(0x150)
#define UFS_PHY_DEBUG_BUS_1_STATUS                          PHY_OFF(0x154)
#define UFS_PHY_DEBUG_BUS_2_STATUS                          PHY_OFF(0x158)
#define UFS_PHY_DEBUG_BUS_3_STATUS                          PHY_OFF(0x15C)
#define UFS_PHY_RMMI_ATTR_CTRL				    PHY_OFF(0x16C)
#define UFS_PHY_RMMI_RX_CFGUPDT_L1	(1 << 7)
#define UFS_PHY_RMMI_TX_CFGUPDT_L1	(1 << 6)
#define UFS_PHY_RMMI_CFGWR_L1		(1 << 5)
#define UFS_PHY_RMMI_CFGRD_L1		(1 << 4)
#define UFS_PHY_RMMI_RX_CFGUPDT_L0	(1 << 3)
#define UFS_PHY_RMMI_TX_CFGUPDT_L0	(1 << 2)
#define UFS_PHY_RMMI_CFGWR_L0		(1 << 1)
#define UFS_PHY_RMMI_CFGRD_L0		(1 << 0)
#define UFS_PHY_RMMI_ATTRID				    PHY_OFF(0x170)
#define UFS_PHY_RMMI_ATTRWRVAL				    PHY_OFF(0x174)
#define UFS_PHY_RMMI_ATTRRDVAL_L0_STATUS		    PHY_OFF(0x178)
#define UFS_PHY_RMMI_ATTRRDVAL_L1_STATUS		    PHY_OFF(0x17C)

/* TX LANE n (0, 1) registers */
#define QSERDES_TX_BIST_MODE_LANENO(n)                      TX_OFF(n, 0x00)
#define QSERDES_TX_CLKBUF_ENABLE(n)                         TX_OFF(n, 0x04)
#define QSERDES_TX_TX_EMP_POST1_LVL(n)                      TX_OFF(n, 0x08)
#define QSERDES_TX_TX_DRV_LVL(n)                            TX_OFF(n, 0x0C)
#define QSERDES_TX_RESET_TSYNC_EN(n)                        TX_OFF(n, 0x10)
#define QSERDES_TX_LPB_EN(n)                                TX_OFF(n, 0x14)
#define QSERDES_TX_RES_CODE(n)                              TX_OFF(n, 0x18)
#define QSERDES_TX_PERL_LENGTH1(n)                          TX_OFF(n, 0x1C)
#define QSERDES_TX_PERL_LENGTH2(n)                          TX_OFF(n, 0x20)
#define QSERDES_TX_SERDES_BYP_EN_OUT(n)                     TX_OFF(n, 0x24)
#define QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_EN(n)           TX_OFF(n, 0x28)
#define QSERDES_TX_PARRATE_REC_DETECT_IDLE_EN(n)            TX_OFF(n, 0x2C)
#define QSERDES_TX_BIST_PATTERN1(n)                         TX_OFF(n, 0x30)
#define QSERDES_TX_BIST_PATTERN2(n)                         TX_OFF(n, 0x34)
#define QSERDES_TX_BIST_PATTERN3(n)                         TX_OFF(n, 0x38)
#define QSERDES_TX_BIST_PATTERN4(n)                         TX_OFF(n, 0x3C)
#define QSERDES_TX_BIST_PATTERN5(n)                         TX_OFF(n, 0x40)
#define QSERDES_TX_BIST_PATTERN6(n)                         TX_OFF(n, 0x44)
#define QSERDES_TX_BIST_PATTERN7(n)                         TX_OFF(n, 0x48)
#define QSERDES_TX_BIST_PATTERN8(n)                         TX_OFF(n, 0x4C)
#define QSERDES_TX_LANE_MODE(n)                             TX_OFF(n, 0x50)
#define QSERDES_TX_ATB_SEL(n)                               TX_OFF(n, 0x54)
#define QSERDES_TX_REC_DETECT_LVL(n)                        TX_OFF(n, 0x58)
#define QSERDES_TX_PRBS_SEED1(n)                            TX_OFF(n, 0x5C)
#define QSERDES_TX_PRBS_SEED2(n)                            TX_OFF(n, 0x60)
#define QSERDES_TX_PRBS_SEED3(n)                            TX_OFF(n, 0x64)
#define QSERDES_TX_PRBS_SEED4(n)                            TX_OFF(n, 0x68)
#define QSERDES_TX_RESET_GEN(n)                             TX_OFF(n, 0x6C)
#define QSERDES_TX_TRAN_DRVR_EMP_EN(n)                      TX_OFF(n, 0x70)
#define QSERDES_TX_TX_INTERFACE_MODE(n)                     TX_OFF(n, 0x74)
#define QSERDES_TX_BIST_STATUS(n)                           TX_OFF(n, 0x78)
#define QSERDES_TX_BIST_ERROR_COUNT1(n)                     TX_OFF(n, 0x7C)
#define QSERDES_TX_BIST_ERROR_COUNT2(n)                     TX_OFF(n, 0x80)

/* RX LANE n (0, 1) registers */
#define QSERDES_RX_CDR_CONTROL(n)                           RX_OFF(n, 0x00)
#define QSERDES_RX_AUX_CONTROL(n)                           RX_OFF(n, 0x04)
#define QSERDES_RX_AUX_DATA_TCODE(n)                        RX_OFF(n, 0x08)
#define QSERDES_RX_RCLK_AUXDATA_SEL(n)                      RX_OFF(n, 0x0C)
#define QSERDES_RX_EQ_CONTROL(n)                            RX_OFF(n, 0x10)
#define QSERDES_RX_RX_EQ_GAIN2(n)                           RX_OFF(n, 0x14)
#define QSERDES_RX_AC_JTAG_INIT(n)                          RX_OFF(n, 0x18)
#define QSERDES_RX_AC_JTAG_LVL_EN(n)                        RX_OFF(n, 0x1C)
#define QSERDES_RX_AC_JTAG_MODE(n)                          RX_OFF(n, 0x20)
#define QSERDES_RX_AC_JTAG_RESET(n)                         RX_OFF(n, 0x24)
#define QSERDES_RX_RX_IQ_RXDET_EN(n)                        RX_OFF(n, 0x28)
#define QSERDES_RX_RX_TERM_HIGHZ_CM_AC_COUPLE(n)            RX_OFF(n, 0x2C)
#define QSERDES_RX_RX_EQ_GAIN1(n)                           RX_OFF(n, 0x30)
#define QSERDES_RX_SIGDET_CNTRL(n)                          RX_OFF(n, 0x34)
#define QSERDES_RX_RX_BAND(n)                               RX_OFF(n, 0x38)
#define QSERDES_RX_CDR_FREEZE_UP_DN(n)                      RX_OFF(n, 0x3C)
#define QSERDES_RX_RX_INTERFACE_MODE(n)                     RX_OFF(n, 0x40)
#define QSERDES_RX_JITTER_GEN_MODE(n)                       RX_OFF(n, 0x44)
#define QSERDES_RX_BUJ_AMP(n)                               RX_OFF(n, 0x48)
#define QSERDES_RX_SJ_AMP1(n)                               RX_OFF(n, 0x4C)
#define QSERDES_RX_SJ_AMP2(n)                               RX_OFF(n, 0x50)
#define QSERDES_RX_SJ_PER1(n)                               RX_OFF(n, 0x54)
#define QSERDES_RX_SJ_PER2(n)                               RX_OFF(n, 0x58)
#define QSERDES_RX_BUJ_STEP_FREQ1(n)                        RX_OFF(n, 0x5C)
#define QSERDES_RX_BUJ_STEP_FREQ2(n)                        RX_OFF(n, 0x60)
#define QSERDES_RX_PPM_OFFSET1(n)                           RX_OFF(n, 0x64)
#define QSERDES_RX_PPM_OFFSET2(n)                           RX_OFF(n, 0x68)
#define QSERDES_RX_SIGN_PPM_PERIOD1(n)                      RX_OFF(n, 0x6C)
#define QSERDES_RX_SIGN_PPM_PERIOD2(n)                      RX_OFF(n, 0x70)
#define QSERDES_RX_SSC_CTRL(n)                              RX_OFF(n, 0x74)
#define QSERDES_RX_SSC_COUNT1(n)                            RX_OFF(n, 0x78)
#define QSERDES_RX_SSC_COUNT2(n)                            RX_OFF(n, 0x7C)
#define QSERDES_RX_PWM_CNTRL1(n)                            RX_OFF(n, 0x80)
#define QSERDES_RX_PWM_CNTRL2(n)                            RX_OFF(n, 0x84)
#define QSERDES_RX_PWM_NDIV(n)                              RX_OFF(n, 0x88)
#define QSERDES_RX_SIGDET_CNTRL2(n)                         RX_OFF(n, 0x8C)
#define QSERDES_RX_UFS_CNTRL(n)                             RX_OFF(n, 0x90)
#define QSERDES_RX_CDR_CONTROL3(n)                          RX_OFF(n, 0x94)
#define QSERDES_RX_CDR_CONTROL_HALF(n)                      RX_OFF(n, 0x98)
#define QSERDES_RX_CDR_CONTROL_QUARTER(n)                   RX_OFF(n, 0x9C)
#define QSERDES_RX_CDR_CONTROL_EIGHTH(n)                    RX_OFF(n, 0xA0)
#define QSERDES_RX_UCDR_FO_GAIN(n)                          RX_OFF(n, 0xA4)
#define QSERDES_RX_UCDR_SO_GAIN(n)                          RX_OFF(n, 0xA8)
#define QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE(n)         RX_OFF(n, 0xAC)
#define QSERDES_RX_UCDR_FO_TO_SO_DELAY(n)                   RX_OFF(n, 0xB0)
#define QSERDES_RX_PI_CTRL1(n)                              RX_OFF(n, 0xB4)
#define QSERDES_RX_PI_CTRL2(n)                              RX_OFF(n, 0xB8)
#define QSERDES_RX_PI_QUAD(n)                               RX_OFF(n, 0xBC)
#define QSERDES_RX_IDATA1(n)                                RX_OFF(n, 0xC0)
#define QSERDES_RX_IDATA2(n)                                RX_OFF(n, 0xC4)
#define QSERDES_RX_AUX_DATA1(n)                             RX_OFF(n, 0xC8)
#define QSERDES_RX_AUX_DATA2(n)                             RX_OFF(n, 0xCC)
#define QSERDES_RX_AC_JTAG_OUTP(n)                          RX_OFF(n, 0xD0)
#define QSERDES_RX_AC_JTAG_OUTN(n)                          RX_OFF(n, 0xD4)
#define QSERDES_RX_RX_SIGDET_PWMDECSTATUS(n)                RX_OFF(n, 0xD8)

enum {
	MASK_SERDES_START       = 0x1,
	MASK_PCS_READY          = 0x1,
};

enum {
	OFFSET_SERDES_START     = 0x0,
};

#define MAX_PROP_NAME              32
#define VDDA_PHY_MIN_UV            1000000
#define VDDA_PHY_MAX_UV            1000000
#define VDDA_PLL_MIN_UV            1800000
#define VDDA_PLL_MAX_UV            1800000

#endif /* UFS_MSM_H_ */
