/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*! \file
 * \brief  Declaration of library functions
 *
 * Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#ifndef _MTK_WCN_CONSYS_HW_H_
#define _MTK_WCN_CONSYS_HW_H_

/*#include <mt_reg_base.h>*/
#include "wmt_plat.h"

/*device tree mode*/
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#endif
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
#include <linux/regmap.h>
#endif

#define ENABLE MTK_WCN_BOOL_TRUE
#define DISABLE MTK_WCN_BOOL_FALSE

#define KBYTE (1024*sizeof(char))
#define CONSYS_PAGED_DUMP_SIZE (32*KBYTE)
#define CONSYS_EMI_MEM_SIZE (96*KBYTE) /*coredump space , 96K is enough */

#define CONSYS_SET_BIT(REG, BITVAL) (*((volatile UINT32 *)(REG)) |= ((UINT32)(BITVAL)))
#define CONSYS_CLR_BIT(REG, BITVAL) ((*(volatile UINT32 *)(REG)) &= ~((UINT32)(BITVAL)))
#define CONSYS_CLR_BIT_WITH_KEY(REG, BITVAL, KEY) {\
	UINT32 val = (*(volatile UINT32 *)(REG)); \
	val &= ~((UINT32)(BITVAL)); \
	val |= ((UINT32)(KEY)); \
	(*(volatile UINT32 *)(REG)) = val;\
}
#define CONSYS_REG_READ(addr) (*((volatile UINT32 *)(addr)))
#define CONSYS_REG_WRITE(addr, data) do {\
	writel(data, (volatile void *)addr); \
	mb(); \
} while (0)
#define CONSYS_REG_WRITE_RANGE(reg, data, end, begin) {\
	UINT32 val = CONSYS_REG_READ(reg); \
	SET_BIT_RANGE(&val, data, end, begin); \
	CONSYS_REG_WRITE(reg, val); \
}
#define CONSYS_REG_WRITE_MASK(reg, data, mask) {\
	UINT32 val = CONSYS_REG_READ(reg); \
	SET_BIT_MASK(&val, data, mask); \
	CONSYS_REG_WRITE(reg, val); \
}

/*
 * Write value with value_offset bits of right shift and size bits,
 * to the reg_offset-th bit of address reg
 * value  -----------XXXXXXXXXXXX-------------------
 *                   |<--size-->|<--value_offset-->|
 * reg    -------------OOOOOOOOOOOO-----------------
 *                     |<--size-->|<--reg_offset-->|
 * result -------------XXXXXXXXXXXX-----------------
 */
#define CONSYS_REG_WRITE_OFFSET_RANGE(reg, value, reg_offset, value_offset, size) ({\
	UINT32 data = (value) >> (value_offset); \
	data = GET_BIT_RANGE(data, size, 0); \
	data = data << (reg_offset); \
	CONSYS_REG_WRITE_RANGE(reg, data, ((reg_offset) + ((size) - 1)), reg_offset); \
})

#define CONSYS_REG_WRITE_BIT(reg, offset, val) CONSYS_REG_WRITE_OFFSET_RANGE(reg, ((val) & 1), offset, 0, 1)

/*force fw assert pattern*/
#define EXP_APMEM_HOST_OUTBAND_ASSERT_MAGIC_W1   (0x19b30bb1)

#define DYNAMIC_DUMP_GROUP_NUM 5


#define OPT_POWER_ON_DLM_TABLE			(0x1)
#define OPT_SET_MCUCLK_TABLE_1_2		(0x1 << 1)
#define OPT_SET_MCUCLK_TABLE_3_4		(0x1 << 2)
#define OPT_QUERY_ADIE				(0x1 << 3)
#define OPT_SET_WIFI_EXT_COMPONENT		(0x1 << 4)
#define OPT_WIFI_LTE_COEX			(0x1 << 5)
#define OPT_COEX_TDM_REQ_ANTSEL_NUM		(0x1 << 6)
#define OPT_BT_TSSI_FROM_WIFI_CONFIG_NEW_OPID	(0x1 << 7)
#define OPT_INIT_COEX_BEFORE_RF_CALIBRATION	(0x1 << 8)
#define OPT_INIT_COEX_AFTER_RF_CALIBRATION	(0x1 << 9)
#define OPT_COEX_CONFIG_ADJUST			(0x1 << 10)
#define OPT_COEX_CONFIG_ADJUST_NEW_FLAG		(0x1 << 11)
#define OPT_SET_OSC_TYPE			(0x1 << 12)
#define OPT_SET_COREDUMP_LEVEL			(0x1 << 13)
#define OPT_WIFI_5G_PALDO			(0x1 << 14)
#define OPT_GPS_SYNC				(0x1 << 15)
#define OPT_WIFI_LTE_COEX_TABLE_0		(0x1 << 16)
#define OPT_WIFI_LTE_COEX_TABLE_1		(0x1 << 17)
#define OPT_WIFI_LTE_COEX_TABLE_2		(0x1 << 18)
#define OPT_WIFI_LTE_COEX_TABLE_3		(0x1 << 19)
#define OPT_COEX_EXT_ELNA_GAIN_P1_SUPPORT	(0x1 << 20)
#define OPT_NORMAL_PATCH_DWN_0			(0x1 << 21)
#define OPT_NORMAL_PATCH_DWN_1			(0x1 << 22)
#define OPT_NORMAL_PATCH_DWN_2			(0x1 << 23)
#define OPT_NORMAL_PATCH_DWN_3			(0x1 << 24)
#define OPT_PATCH_CHECKSUM			(0x1 << 25)
#define OPT_DISABLE_ROM_PATCH_DWN		(0x1 << 26)

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

struct CONSYS_BASE_ADDRESS {
	SIZE_T mcu_base;
	SIZE_T ap_rgu_base;
	SIZE_T topckgen_base;
	SIZE_T spm_base;
	SIZE_T mcu_conn_hif_on_base;
	SIZE_T mcu_top_misc_off_base;
	SIZE_T mcu_cfg_on_base;
	SIZE_T mcu_cirq_base;
	SIZE_T da_xobuf_base;
	SIZE_T mcu_top_misc_on_base;
	SIZE_T mcu_conn_hif_pdma_base;
	SIZE_T ap_pccif4_base;
	SIZE_T infra_ao_pericfg_base;
	SIZE_T infracfg_reg_base;
};

enum CONSYS_BASE_ADDRESS_INDEX {
	MCU_BASE_INDEX = 0,
	TOP_RGU_BASE_INDEX,
	INFRACFG_AO_BASE_INDEX,
	SPM_BASE_INDEX,
	MCU_CONN_HIF_ON_BASE_INDEX,
	MCU_TOP_MISC_OFF_BASE_INDEX,
	MCU_CFG_ON_BASE_INDEX,
	MCU_CIRQ_BASE_INDEX,
	MCU_TOP_MISC_ON_BASE_INDEX,
	MCU_CONN_HIF_PDMA_BASE_INDEX,
	AP_PCCIF4_BASE_INDEX,
	INFRA_AO_PERICFG_BASE_INDEX,
	INFRACFG_REG_BASE_INDEX,
};

typedef enum _ENUM_EMI_CTRL_STATE_OFFSET_ {
	EXP_APMEM_CTRL_STATE = 0x0,
	EXP_APMEM_CTRL_HOST_SYNC_STATE = 0x4,
	EXP_APMEM_CTRL_HOST_SYNC_NUM = 0x8,
	EXP_APMEM_CTRL_CHIP_SYNC_STATE = 0xc,
	EXP_APMEM_CTRL_CHIP_SYNC_NUM = 0x10,
	EXP_APMEM_CTRL_CHIP_SYNC_ADDR = 0x14,
	EXP_APMEM_CTRL_CHIP_SYNC_LEN = 0x18,
	EXP_APMEM_CTRL_CHIP_PRINT_BUFF_START = 0x1c,
	EXP_APMEM_CTRL_CHIP_PRINT_BUFF_LEN = 0x20,
	EXP_APMEM_CTRL_CHIP_PRINT_BUFF_IDX = 0x24,
	EXP_APMEM_CTRL_CHIP_INT_STATUS = 0x28,
	EXP_APMEM_CTRL_CHIP_PAGED_DUMP_END = 0x2c,
	EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1 = 0x30,
	EXP_APMEM_CTRL_CHIP_PAGE_DUMP_NUM = 0x44,
	EXP_APMEM_CTRL_CHIP_FW_DBGLOG_MODE = 0x40,
	EXP_APMEM_CTRL_CHIP_DYNAMIC_DUMP = 0x48,
	EXP_APMEM_CTRL_CHIP_CHECK_SLEEP = 0x4c,
	EXP_APMEM_CTRL_ASSERT_FLAG = 0x100,
	EXP_APMEM_CTRL_MAX
} ENUM_EMI_CTRL_STATE_OFFSET, *P_ENUM_EMI_CTRL_STATE_OFFSET;

typedef enum _CONSYS_GPS_CO_CLOCK_TYPE_ {
	GPS_TCXO_TYPE = 0,
	GPS_CO_TSX_TYPE = 1,
	GPS_CO_DCXO_TYPE = 2,
	GPS_CO_VCTCXO_TYPE = 3,
	GPS_CO_CLOCK_TYPE_MAX
} CONSYS_GPS_CO_CLOCK_TYPE, *P_CONSYS_GPS_CO_CLOCK_TYPE;

typedef INT32(*CONSYS_IC_CLOCK_BUFFER_CTRL) (MTK_WCN_BOOL enable);
typedef VOID(*CONSYS_IC_HW_RESET_BIT_SET) (MTK_WCN_BOOL enable);
typedef VOID(*CONSYS_IC_HW_SPM_CLK_GATING_ENABLE) (VOID);
typedef INT32(*CONSYS_IC_HW_POWER_CTRL) (MTK_WCN_BOOL enable);
typedef INT32(*CONSYS_IC_AHB_CLOCK_CTRL) (MTK_WCN_BOOL enable);
typedef INT32(*POLLING_CONSYS_IC_CHIPID) (VOID);
typedef VOID(*UPDATE_CONSYS_ROM_DESEL_VALUE) (VOID);
typedef VOID(*CONSYS_HANG_DEBUG)(VOID);
typedef VOID(*CONSYS_IC_ARC_REG_SETTING) (VOID);
typedef VOID(*CONSYS_IC_AFE_REG_SETTING) (VOID);
typedef INT32(*CONSYS_IC_HW_VCN18_CTRL) (MTK_WCN_BOOL enable);
typedef VOID(*CONSYS_IC_VCN28_HW_MODE_CTRL) (UINT32 enable);
typedef INT32(*CONSYS_IC_HW_VCN28_CTRL) (UINT32 enable);
typedef INT32(*CONSYS_IC_HW_WIFI_VCN33_CTRL) (UINT32 enable);
typedef INT32(*CONSYS_IC_HW_BT_VCN33_CTRL) (UINT32 enable);
typedef INT32(*CONSYS_IC_HW_VCN_CTRL_AFTER_IDLE) (VOID);
typedef UINT32(*CONSYS_IC_SOC_CHIPID_GET) (VOID);
typedef INT32(*CONSYS_IC_ADIE_CHIPID_DETECT) (VOID);
typedef INT32(*CONSYS_IC_EMI_MPU_SET_REGION_PROTECTION) (VOID);
typedef UINT32(*CONSYS_IC_EMI_SET_REMAPPING_REG) (VOID);
typedef INT32(*IC_BT_WIFI_SHARE_V33_SPIN_LOCK_INIT) (VOID);
typedef INT32(*CONSYS_IC_CLK_GET_FROM_DTS) (struct platform_device *pdev);
typedef INT32(*CONSYS_IC_PMIC_GET_FROM_DTS) (struct platform_device *pdev);
typedef INT32(*CONSYS_IC_READ_IRQ_INFO_FROM_DTS) (struct platform_device *pdev, PINT32 irq_num, PUINT32 irq_flag);
typedef INT32(*CONSYS_IC_READ_REG_FROM_DTS) (struct platform_device *pdev);
typedef UINT32(*CONSYS_IC_READ_CPUPCR) (VOID);
typedef VOID(*IC_FORCE_TRIGGER_ASSERT_DEBUG_PIN) (VOID);
typedef INT32(*CONSYS_IC_CO_CLOCK_TYPE) (VOID);
typedef P_CONSYS_EMI_ADDR_INFO(*CONSYS_IC_SOC_GET_EMI_PHY_ADD) (VOID);
typedef MTK_WCN_BOOL(*CONSYS_IC_NEED_STORE_PDEV) (VOID);
typedef UINT32(*CONSYS_IC_STORE_PDEV) (struct platform_device *pdev);
typedef UINT32(*CONSYS_IC_STORE_RESET_CONTROL) (struct platform_device *pdev);
typedef MTK_WCN_BOOL(*CONSYS_IC_NEED_GPS) (VOID);
typedef VOID(*CONSYS_IC_SET_IF_PINMUX) (MTK_WCN_BOOL enable);
typedef VOID(*CONSYS_IC_SET_DL_ROM_PATCH_FLAG) (INT32 flag);
typedef INT32(*CONSYS_IC_DEDICATED_LOG_PATH_INIT) (struct platform_device *pdev);
typedef VOID(*CONSYS_IC_DEDICATED_LOG_PATH_DEINIT) (VOID);
typedef INT32(*CONSYS_IC_CHECK_REG_READABLE) (VOID);
typedef INT32(*CONSYS_IC_EMI_COREDUMP_REMAPPING) (UINT8 __iomem **addr, UINT32 enable);
typedef INT32(*CONSYS_IC_RESET_EMI_COREDUMP) (UINT8 __iomem *addr);
typedef VOID(*CONSYS_IC_CLOCK_FAIL_DUMP) (VOID);
typedef INT32(*CONSYS_IC_IS_CONNSYS_REG) (UINT32 addr);
typedef INT32(*CONSYS_IC_IS_HOST_CSR) (SIZE_T addr);
typedef INT32(*CONSYS_IC_DUMP_OSC_STATE) (P_CONSYS_STATE state);
typedef VOID(*CONSYS_IC_SET_PDMA_AXI_RREADY_FORCE_HIGH) (UINT32 enable);
typedef VOID(*CONSYS_IC_SET_MCIF_EMI_MPU_PROTECTION)(MTK_WCN_BOOL enable);
typedef INT32(*CONSYS_IC_CALIBRATION_BACKUP_RESTORE) (VOID);
typedef VOID(*CONSYS_IC_REGISTER_DEVAPC_CB) (VOID);
typedef VOID(*CONSYS_IC_INFRA_REG_DUMP)(VOID);
typedef INT32(*CONSYS_IC_IS_ANT_SWAP_ENABLE_BY_HWID) (INT32 pin_num);
typedef VOID(*CONSYS_IC_GET_ANT_SEL_CR_ADDR) (PUINT32 default_invert_cr, PUINT32 default_invert_bit);
typedef VOID(*CONSYS_IC_EMI_ENTRY_ADDRESS) (VOID);
typedef VOID(*CONSYS_IC_SET_XO_OSC_CTRL) (VOID);
typedef VOID(*CONSYS_IC_IDENTIFY_ADIE) (VOID);
typedef VOID(*CONSYS_IC_WIFI_CTRL_SETTING) (VOID);
typedef VOID(*CONSYS_IC_BUS_TIMEOUT_CONFIG) (VOID);
typedef VOID(*CONSYS_IC_SET_ACCESS_EMI_HW_MODE) (VOID);
typedef INT32(*CONSYS_IC_DUMP_GATING_STATE) (P_CONSYS_STATE state);
typedef INT32(*CONSYS_IC_SLEEP_INFO_ENABLE_CTRL) (UINT32 enable);
typedef INT32(*CONSYS_IC_SLEEP_INFO_READ_CTRL) (WMT_SLEEP_COUNT_TYPE type, PUINT64 sleep_counter, PUINT64 sleep_timer);
typedef INT32(*CONSYS_IC_SLEEP_INFO_CLEAR) (VOID);
typedef UINT64(*CONSYS_IC_GET_OPTIONS) (VOID);

typedef struct _WMT_CONSYS_IC_OPS_ {
	CONSYS_IC_CLOCK_BUFFER_CTRL consys_ic_clock_buffer_ctrl;
	CONSYS_IC_HW_RESET_BIT_SET consys_ic_hw_reset_bit_set;
	CONSYS_IC_HW_SPM_CLK_GATING_ENABLE consys_ic_hw_spm_clk_gating_enable;
	CONSYS_IC_HW_POWER_CTRL consys_ic_hw_power_ctrl;
	CONSYS_IC_AHB_CLOCK_CTRL consys_ic_ahb_clock_ctrl;
	POLLING_CONSYS_IC_CHIPID polling_consys_ic_chipid;
	UPDATE_CONSYS_ROM_DESEL_VALUE update_consys_rom_desel_value;
	CONSYS_HANG_DEBUG consys_hang_debug;
	CONSYS_IC_ARC_REG_SETTING consys_ic_acr_reg_setting;
	CONSYS_IC_AFE_REG_SETTING consys_ic_afe_reg_setting;
	CONSYS_IC_HW_VCN18_CTRL consys_ic_hw_vcn18_ctrl;
	CONSYS_IC_VCN28_HW_MODE_CTRL consys_ic_vcn28_hw_mode_ctrl;
	CONSYS_IC_HW_VCN28_CTRL consys_ic_hw_vcn28_ctrl;
	CONSYS_IC_HW_WIFI_VCN33_CTRL consys_ic_hw_wifi_vcn33_ctrl;
	CONSYS_IC_HW_BT_VCN33_CTRL consys_ic_hw_bt_vcn33_ctrl;
	CONSYS_IC_HW_VCN_CTRL_AFTER_IDLE consys_ic_hw_vcn_ctrl_after_idle;
	CONSYS_IC_SOC_CHIPID_GET consys_ic_soc_chipid_get;
	CONSYS_IC_ADIE_CHIPID_DETECT consys_ic_adie_chipid_detect;
	CONSYS_IC_EMI_MPU_SET_REGION_PROTECTION consys_ic_emi_mpu_set_region_protection;
	CONSYS_IC_EMI_SET_REMAPPING_REG consys_ic_emi_set_remapping_reg;
	IC_BT_WIFI_SHARE_V33_SPIN_LOCK_INIT ic_bt_wifi_share_v33_spin_lock_init;
	CONSYS_IC_CLK_GET_FROM_DTS consys_ic_clk_get_from_dts;
	CONSYS_IC_PMIC_GET_FROM_DTS consys_ic_pmic_get_from_dts;
	CONSYS_IC_READ_IRQ_INFO_FROM_DTS consys_ic_read_irq_info_from_dts;
	CONSYS_IC_READ_REG_FROM_DTS consys_ic_read_reg_from_dts;
	CONSYS_IC_READ_CPUPCR consys_ic_read_cpupcr;
	IC_FORCE_TRIGGER_ASSERT_DEBUG_PIN ic_force_trigger_assert_debug_pin;
	CONSYS_IC_CO_CLOCK_TYPE consys_ic_co_clock_type;
	CONSYS_IC_SOC_GET_EMI_PHY_ADD consys_ic_soc_get_emi_phy_add;
	CONSYS_IC_NEED_STORE_PDEV consys_ic_need_store_pdev;
	CONSYS_IC_STORE_PDEV consys_ic_store_pdev;
	CONSYS_IC_STORE_RESET_CONTROL consys_ic_store_reset_control;
	CONSYS_IC_NEED_GPS consys_ic_need_gps;
	CONSYS_IC_SET_IF_PINMUX consys_ic_set_if_pinmux;
	CONSYS_IC_SET_DL_ROM_PATCH_FLAG consys_ic_set_dl_rom_patch_flag;
	CONSYS_IC_DEDICATED_LOG_PATH_INIT consys_ic_dedicated_log_path_init;
	CONSYS_IC_DEDICATED_LOG_PATH_DEINIT consys_ic_dedicated_log_path_deinit;
	CONSYS_IC_CHECK_REG_READABLE consys_ic_check_reg_readable;
	CONSYS_IC_EMI_COREDUMP_REMAPPING consys_ic_emi_coredump_remapping;
	CONSYS_IC_RESET_EMI_COREDUMP consys_ic_reset_emi_coredump;
	CONSYS_IC_CLOCK_FAIL_DUMP consys_ic_clock_fail_dump;
	CONSYS_IC_IS_CONNSYS_REG consys_ic_is_connsys_reg;
	CONSYS_IC_IS_HOST_CSR consys_ic_is_host_csr;
	CONSYS_IC_DUMP_OSC_STATE consys_ic_dump_osc_state;
	CONSYS_IC_SET_PDMA_AXI_RREADY_FORCE_HIGH consys_ic_set_pdma_axi_rready_force_high;
	CONSYS_IC_SET_MCIF_EMI_MPU_PROTECTION consys_ic_set_mcif_emi_mpu_protection;
	CONSYS_IC_CALIBRATION_BACKUP_RESTORE consys_ic_calibration_backup_restore;
	CONSYS_IC_REGISTER_DEVAPC_CB consys_ic_register_devapc_cb;
	CONSYS_IC_INFRA_REG_DUMP consys_ic_infra_reg_dump;
	CONSYS_IC_IS_ANT_SWAP_ENABLE_BY_HWID consys_ic_is_ant_swap_enable_by_hwid;
	CONSYS_IC_GET_ANT_SEL_CR_ADDR consys_ic_get_ant_sel_cr_addr;
	CONSYS_IC_EMI_ENTRY_ADDRESS consys_ic_emi_entry_address;
	CONSYS_IC_SET_XO_OSC_CTRL consys_ic_set_xo_osc_ctrl;
	CONSYS_IC_IDENTIFY_ADIE consys_ic_identify_adie;
	CONSYS_IC_WIFI_CTRL_SETTING consys_ic_wifi_ctrl_setting;
	CONSYS_IC_BUS_TIMEOUT_CONFIG consys_ic_bus_timeout_config;
	CONSYS_IC_SET_ACCESS_EMI_HW_MODE consys_ic_set_access_emi_hw_mode;
	CONSYS_IC_DUMP_GATING_STATE consys_ic_dump_gating_state;
	CONSYS_IC_SLEEP_INFO_ENABLE_CTRL consys_ic_sleep_info_enable_ctrl;
	CONSYS_IC_SLEEP_INFO_READ_CTRL consys_ic_sleep_info_read_ctrl;
	CONSYS_IC_SLEEP_INFO_CLEAR consys_ic_sleep_info_clear;
	CONSYS_IC_GET_OPTIONS consys_ic_get_options;
} WMT_CONSYS_IC_OPS, *P_WMT_CONSYS_IC_OPS;
/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
#if CFG_WMT_DUMP_INT_STATUS
extern void mt_irq_dump_status(int irq);
#endif
extern struct CONSYS_BASE_ADDRESS conn_reg;
extern UINT32 gCoClockFlag;
extern EMI_CTRL_STATE_OFFSET mtk_wcn_emi_state_off;
extern CONSYS_EMI_ADDR_INFO mtk_wcn_emi_addr_info;

extern UINT64 gConEmiSize;
extern phys_addr_t gConEmiPhyBase;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
extern struct regmap *g_regmap;
#endif


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
INT32 mtk_wcn_consys_hw_init(VOID);
INT32 mtk_wcn_consys_hw_deinit(VOID);
INT32 mtk_wcn_consys_hw_pwr_off(UINT32 co_clock_type);
INT32 mtk_wcn_consys_hw_pwr_on(UINT32 co_clock_type);
INT32 mtk_wcn_consys_hw_rst(UINT32 co_clock_type);
INT32 mtk_wcn_consys_hw_bt_paldo_ctrl(UINT32 enable);
INT32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT32 enable);
INT32 mtk_wcn_consys_hw_efuse_paldo_ctrl(UINT32 enable, UINT32 co_clock_type);
INT32 mtk_wcn_consys_hw_vcn28_ctrl(UINT32 enable);
INT32 mtk_wcn_consys_hw_state_show(VOID);
PUINT8 mtk_wcn_consys_emi_virt_addr_get(UINT32 ctrl_state_offset);
P_CONSYS_EMI_ADDR_INFO mtk_wcn_consys_soc_get_emi_phy_add(VOID);
UINT32 mtk_wcn_consys_read_cpupcr(VOID);

VOID mtk_wcn_force_trigger_assert_debug_pin(VOID);
INT32 mtk_wcn_consys_read_irq_info_from_dts(PINT32 irq_num, PUINT32 irq_flag);
INT32 mtk_wcn_consys_reg_ctrl(UINT32 is_write, enum CONSYS_BASE_ADDRESS_INDEX index, UINT32 offset,
		PUINT32 value);

P_WMT_CONSYS_IC_OPS mtk_wcn_get_consys_ic_ops(VOID);
INT32 mtk_wcn_consys_jtag_set_for_mcu(VOID);
#if CONSYS_ENALBE_SET_JTAG
UINT32 mtk_wcn_consys_jtag_flag_ctrl(UINT32 en);
#endif
#ifdef CONSYS_WMT_REG_SUSPEND_CB_ENABLE
UINT32 mtk_wcn_consys_hw_osc_en_ctrl(UINT32 en);
#endif
UINT32 mtk_wcn_consys_soc_chipid(VOID);
UINT32 mtk_wcn_consys_get_adie_chipid(VOID);
INT32 mtk_wcn_consys_detect_adie_chipid(UINT32 co_clock_type);
#if !defined(CONFIG_MTK_GPIO_LEGACY)
struct pinctrl *mtk_wcn_consys_get_pinctrl(VOID);
#endif
INT32 mtk_wcn_consys_co_clock_type(VOID);
INT32 mtk_wcn_consys_set_dbg_mode(UINT32 flag);
INT32 mtk_wcn_consys_set_dynamic_dump(PUINT32 buf);
INT32 mtk_wdt_swsysret_config(INT32 bit, INT32 set_value);
VOID mtk_wcn_consys_hang_debug(VOID);
UINT32 mtk_consys_get_gps_lna_pin_num(VOID);
INT32 mtk_consys_check_reg_readable(VOID);
INT32 mtk_consys_check_reg_readable_by_addr(SIZE_T addr);
VOID mtk_wcn_consys_clock_fail_dump(VOID);
INT32 mtk_consys_is_connsys_reg(UINT32 addr);
VOID mtk_consys_set_mcif_mpu_protection(MTK_WCN_BOOL enable);
INT32 mtk_consys_is_calibration_backup_restore_support(VOID);
VOID mtk_consys_set_chip_reset_status(INT32 status);
INT32 mtk_consys_chip_reset_status(VOID);
INT32 mtk_consys_is_ant_swap_enable_by_hwid(VOID);
INT32 mtk_consys_dump_osc_state(P_CONSYS_STATE state);
VOID mtk_wcn_consys_ic_get_ant_sel_cr_addr(PUINT32 default_invert_cr, PUINT32 default_invert_bit);
INT32 mtk_wcn_consys_dump_gating_state(P_CONSYS_STATE state);
INT32 mtk_wcn_consys_sleep_info_read_all_ctrl(P_CONSYS_STATE state);
INT32 mtk_wcn_consys_sleep_info_clear(VOID);
INT32 mtk_wcn_consys_sleep_info_restore(VOID);
UINT64 mtk_wcn_consys_get_options(VOID);
#endif /* _MTK_WCN_CONSYS_HW_H_ */

