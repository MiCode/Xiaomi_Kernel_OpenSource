/*
 * Copyright 2017 NXP
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef _NXP_PHY_H
#define _NXP_PHY_H

 /* PHY IDs */
#define NXP_PHY_ID_TJA1100        (0x0180DC40U)
#define NXP_PHY_ID_TJA1102P0      (0x0180DC80U)
#define NXP_PHY_ID_TJA1102P1      (0x00000000U)
#define NXP_PHY_ID_TJA1102S       (0x0180DC90U) /* only P0 is available */

/* masks out the revision number */
#define NXP_PHY_ID_MASK           (0xFFFFFFF0U)

#endif /* _NXP_PHY_H */

/* NXP specific registers */

/* extended_control register (TJA1100 and TJA1102) */
#define MII_ECTRL                 (0x11U)
/* configuration_1 register (TJA1100 and TJA1102) */
#define MII_CFG1                  (0x12U)
/* configuration_2 register (TJA1100 and TJA1102) */
#define MII_CFG2                  (0x13U)
/* symbol_error_counter register (TJA1100 and TJA1102) */
#define MII_SYMERRCNT             (0x14U)
/* interrupt_source_flag register (TJA1100 and TJA1102) */
#define MII_INTSRC                (0x15U)
/* interrupt_mask register (TJA1100 and TJA1102) */
#define MII_INTMASK               (0x16U)
/* communication_status register (TJA1100 and TJA1102) */
#define MII_COMMSTAT              (0x17U)
/* general_status register (TJA1100 and TJA1102) */
#define MII_GENSTAT               (0x18U)
/* external_status register (TJA1100 and TJA1102) */
#define MII_EXTERNAL_STATUS       (0x19U)
/* link_fail_counter register (TJA1100 and TJA1102) */
#define MII_LINK_FAIL_COUNTER     (0x1AU)
/* common_configuration register (only TJA1102) */
#define MII_COMMCFG               (0x1BU)
/* configuration_3 register (only TJA1102) */
#define MII_CONFIGURATION_3       (0x1CU)

/* extended_control register */
#define ECTRL_LINK_CONTROL        BIT(15)
#define ECTRL_POWER_MODE          (0x00007800U)
#define ECTRL_SLAVE_JITTER_TEST   BIT(10)
#define ECTRL_TRAINING_RESTART    BIT(9)
#define ECTRL_TEST_MODE           (0x000001C0U)
#define ECTRL_CABLE_TEST          BIT(5)
#define ECTRL_LOOPBACK_MODE       (0x00000018U)
#define ECTRL_CONFIG_EN           BIT(2)
#define ECTRL_WAKE_REQUEST        BIT(0)
#define CABLE_TEST_TIMEOUT        1U /* cable test takes <= 1*100us */

/* register values of the different power modes */
#define POWER_MODE_NOCHANGE       (0x00000000U)
#define POWER_MODE_NORMAL         (0x00001800U)
#define POWER_MODE_SLEEPREQUEST   (0x00005800U)
#define POWER_MODE_STANDBY        (0x00006000U)
#define POWER_MODE_SILENT         (0x00004800U)
#define POWER_MODE_SLEEP          (0x00005000U)

/* timeouts for different power mode transitions */
#define POWER_MODE_TIMEOUT        200U
#define SLEEP_REQUEST_TO          160U /* 16ms = 160*100us */

/* duration necessary for a reliable wake up of the link partner (in us) */
#define TJA100_WAKE_REQUEST_TIMEOUT_US     5000U
#define TJA102_WAKE_REQUEST_TIMEOUT_US     1300U

/* configuration_1 register */
#define CFG1_MASTER_SLAVE         BIT(15)
#define TJA1100_CFG1_AUTO_OP      BIT(14)
#define TJA1102_CFG1_FWDPHYLOC    BIT(14)
#define CFG1_LINK_LENGTH          (0x00003000U)
#define CFG1_REMWUPHY             BIT(11)
#define CFG1_LOCWUPHY             BIT(10)
#define CFG1_MII_MODE             (0x00000300U)
#define CFG1_MII_DRIVER           BIT(7)
#define CFG1_SLEEP_CONFIRM        BIT(6)
#define TJA1100_CFG1_LED_MODE     (0x00000030U)
#define TJA1100_CFG1_LED_EN       BIT(3)
#define CFG1_FWDPHYREM            BIT(2)
#define CFG1_AUTO_PWD             BIT(1)
#define CFG1_LPS_ACTIVE           BIT(0)

/* configuration_2 register */
#define CFG2_PHYAD                (0x0000F800U)
#define CFG2_SNR_AVERAGING        (0x00000600U)
#define CFG2_SNR_WLIMIT           (0x000001C0U)
#define CFG2_SNR_FAILLIMIT        (0x00000038U)
#define CFG2_JUMBO_ENABLE         BIT(2)
#define CFG2_SLEEP_REQUEST_TO     (0x00000003U)

#define SLEEP_REQUEST_TO_16MS     (0x00000003U)

/* symbol_error_counter register */
#define SYMERRCNT_SYM_ERR_CNT     (0xFFFFFFFFU)

/* interrupt_source_flag register */
#define INTERRUPT_PWON                    BIT(15)
#define INTERRUPT_WAKEUP                  BIT(14)
#define INTERRUPT_WUR_RECEIVED            BIT(13)
#define INTERRUPT_LPS_RECEIVED            BIT(12)
#define INTERRUPT_PHY_INIT_FAIL           BIT(11)
#define INTERRUPT_LINK_STATUS_FAIL        BIT(10)
#define INTERRUPT_LINK_STATUS_UP          BIT(9)
#define INTERRUPT_SYM_ERR                 BIT(8)
#define INTERRUPT_TRAINING_FAILED         BIT(7)
#define INTERRUPT_SNR_WARNING             BIT(6)
#define INTERRUPT_CONTROL_ERROR           BIT(5)
#define INTERRUPT_TXEN_CLAMPED            BIT(4)
#define INTERRUPT_UV_ERR                  BIT(3)
#define INTERRUPT_UV_RECOVERY             BIT(2)
#define INTERRUPT_TEMP_ERROR              BIT(1)
#define INTERRUPT_SLEEP_ABORT             BIT(0)
#define INTERRUPT_NONE                    (0x00000000U)
#define INTERRUPT_ALL                     (0x0000FFFFU)

/* communication_status register */
#define COMMSTAT_LINK_UP                BIT(15)
#define COMMSTAT_TX_MODE                (0x00006000U)
#define COMMSTAT_LOC_RCVR_STATUS        BIT(12)
#define COMMSTAT_REM_RCVR_STATUS        BIT(11)
#define COMMSTAT_SCR_LOCKED             BIT(10)
#define COMMSTAT_SSD_ERROR              BIT(9)
#define COMMSTAT_ESD_ERROR              BIT(8)
#define COMMSTAT_SNR                    (0x000000E0U)
#define COMMSTAT_RECEIVE_ERROR          BIT(4)
#define COMMSTAT_TRANSMIT_ERROR         BIT(3)
#define COMMSTAT_PHY_STATE              (0x00000007U)

/* register category general_status */
#define GENSTAT_INT_STATUS        BIT(15)
#define GENSTAT_PLL_LOCKED        BIT(14)
#define GENSTAT_LOCAL_WU          BIT(13)
#define GENSTAT_REMOTE_WU         BIT(12)
#define GENSTAT_DATA_DET_WU       BIT(11)
#define GENSTAT_EN_STATUS         BIT(10)
#define GENSTAT_RESET_STATUS      BIT(9)
#define GENSTAT_LINKFAIL_CNT      (0x000000F8U)

/* common_configuration register */
#define COMMCFG_AUTO_OP           BIT(15)

/* External status register */
#define EXTSTAT_OPEN_DETECT       BIT(7)
#define EXTSTAT_SHORT_DETECT      BIT(8)

/*
 * Indicator for BRR support in ESTATUS register
 * and in phydev->supported member.
 * Not yet present in include/uapi/linux/mii.h and
 * include/uapi/linux/ethtool.h
 */
#define ESTATUS_100T1_FULL        BIT(7)
#define SUPPORTED_100BASET1_FULL  BIT(27)
#define ADVERTISED_100BASET1_FULL BIT(27)

/* length of delay during one loop iteration in
 * wait_on_condition (in us)
 */
#define DELAY_LENGTH              100U

/* length of delay during two pollings (in ms) */
#define POLL_PAUSE              50U

/* possible test modes of the PHY */
enum test_mode {
	NO_TMODE = 1,
	TMODE1,
	TMODE2,
	TMODE3,
	TMODE4,
	TMODE5,
	TMODE6
};

 /* register values of the different test modes */
 #define ECTRL_NO_TMODE          (0x000000U) /* no test mode */
 #define ECTRL_TMODE1            (0x000040U)
 #define ECTRL_TMODE2            (0x000080U)
 #define ECTRL_TMODE3            (0x0000C0U)
 #define ECTRL_TMODE4            (0x000100U)
 #define ECTRL_TMODE5            (0x000140U)
 /* scrambler, descrambler bypassed */
 #define ECTRL_TMODE6            (0x000180U)

/* possible loopback modes of the PHY */
enum loopback_mode {
	NO_LMODE = 1,
	INTERNAL_LMODE,
	EXTERNAL_LMODE,
	REMOTE_LMODE
};

/* register values of the different loopback modes */
#define ECTRL_INTERNAL_LMODE    (0x000000U)
#define ECTRL_EXTERNAL_LMODE    (0x000008U)
#define ECTRL_REMOTE_LMODE      (0x000018U)

/* possible led modes of the PHY */
enum led_mode {
	NO_LED_MODE = 1,
	LINKUP_LED_MODE,
	FRAMEREC_LED_MODE,
	SYMERR_LED_MODE,
	CRSSIG_LED_MODE
};

/* register values of the different led modes */
#define CFG1_LED_LINKUP    (0x00000000U)
#define CFG1_LED_FRAMEREC  (0x00000010U)
#define CFG1_LED_SYMERR    (0x00000020U)
#define CFG1_LED_CRSSIG    (0x00000030U)

/* values written to sysfs nodes */
#define SYSFS_FWDPHYLOC    BIT(0)
#define SYSFS_REMWUPHY     BIT(1)
#define SYSFS_LOCWUPHY     BIT(2)
#define SYSFS_FWDPHYREM    BIT(3)

/* nxp specific data */
struct nxp_specific_data {
	int is_master;
	int poll_setup;
};

/* Helper Function prototypes */
static int set_master_cfg(struct phy_device *phydev, int setMaster);
static int get_master_cfg(struct phy_device *phydev);
static struct phy_device *search_phy_by_id(int phy_id);
static struct phy_device *search_phy_by_addr(int phy_id);
static int wait_on_condition(struct phy_device *phydev, int reg_addr,
			     int reg_mask, int cond, int timeout);
static void set_link_control(struct phy_device *phydev,
			     int enable_link_control);
static inline int phy_configure_bit(struct phy_device *phydev,
				    int reg_name, int bit_mask,
				    int bit_value);
static inline int phy_configure_bits(struct phy_device *phydev,
				     int reg_name, int bit_mask,
				     int bit_value);
static int nxp_resume(struct phy_device *phydev);
static int nxp_ack_interrupt(struct phy_device *phydev);
static void poll(struct work_struct *work);

static struct attribute *nxp_sysfs_entries[];
static struct attribute_group nxp_attribute_group;
