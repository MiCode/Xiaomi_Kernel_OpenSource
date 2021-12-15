/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/smp.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#include <mt-plat/mtk_auxadc_intf.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_hw.h>
#include <mach/upmu_sw.h>


/* /proj/mtk22477/casws_mtk22477/lc1/kernel-4.19/drivers/misc/mediatek/include/mt-plat */

void __attribute__ ((weak)) pmic_auxadc_init(void) {}
void __attribute__ ((weak)) pmic_auxadc_dump_regs(char *buf){}
int __attribute__ ((weak)) pmic_get_auxadc_channel_max(void){ return 0; }
int __attribute__ ((weak)) mt_get_auxadc_value(u8 channel){ return 0; }
void __attribute__ ((weak)) pmic_auxadc_lock(void){}
void __attribute__ ((weak)) pmic_auxadc_unlock(void){}
int __attribute__ ((weak)) mic_get_auxadc_value(int list) { return 0; }
int __attribute__ ((weak)) pmic_get_auxadc_value(int list) { return 0; }
/*
 * PMIC Exported Function for power service
 */
//int __attribute__ ((weak)) g_I_SENSE_offset{   }

/*
 * PMIC extern functions
 */
unsigned int __attribute__ ((weak)) pmic_read_interface(unsigned int RegNum,
					unsigned int *val,
					unsigned int MASK,
					unsigned int SHIFT) { return 0; }
unsigned int __attribute__ ((weak)) pmic_config_interface(unsigned int RegNum,
					  unsigned int val,
					  unsigned int MASK,
					  unsigned int SHIFT) { return 0; }
unsigned int __attribute__ ((weak)) pmic_read_interface_nolock(unsigned int RegNum,
	unsigned int *val,
	unsigned int MASK,
	unsigned int SHIFT) { return 0; }
unsigned int __attribute__ ((weak)) pmic_config_interface_nolock(unsigned int RegNum,
	unsigned int val,
	unsigned int MASK,
	unsigned int SHIFT) { return 0; }
unsigned int __attribute__ ((weak)) pmic_config_interface_nospinlock(unsigned int RegNum,
	unsigned int val,
	unsigned int MASK,
	unsigned int SHIFT) { return 0; }
#ifdef CONFIG_MTK_PMIC_COMMON
unsigned short __attribute__ ((weak)) pmic_set_register_value(PMU_FLAGS_LIST_ENUM flagname,
					      unsigned int val) { return 0; }
unsigned short __attribute__ ((weak)) pmic_get_register_value(PMU_FLAGS_LIST_ENUM flagname) { return 0; }
unsigned short __attribute__ ((weak)) pmic_set_register_value_nolock(
						PMU_FLAGS_LIST_ENUM flagname,
						unsigned int val) { return 0; }
unsigned short __attribute__ ((weak)) pmic_get_register_value_nolock(
						PMU_FLAGS_LIST_ENUM flagname) { return 0; }
unsigned short __attribute__ ((weak)) pmic_set_register_value_nospinlock(
						PMU_FLAGS_LIST_ENUM flagname,
						unsigned int val) { return 0; }
unsigned short __attribute__ ((weak)) bc11_set_register_value(PMU_FLAGS_LIST_ENUM flagname,
    unsigned int val) { return 0; }
unsigned short __attribute__ ((weak)) bc11_get_register_value(PMU_FLAGS_LIST_ENUM flagname) { return 0; }
#endif
void __attribute__ ((weak)) upmu_set_reg_value(unsigned int reg, unsigned int reg_val) {   }
unsigned int __attribute__ ((weak)) upmu_get_reg_value(unsigned int reg) { return 0; }
void __attribute__ ((weak)) pmic_lock(void) {   }
void __attribute__ ((weak)) pmic_unlock(void) {   }

void __attribute__ ((weak)) pmic_enable_interrupt(unsigned int intNo,
		  unsigned int en,
		  char *str) {   }
void __attribute__ ((weak)) pmic_mask_interrupt(unsigned int intNo, char *str) {   }
void __attribute__ ((weak)) pmic_unmask_interrupt(unsigned int intNo, char *str) {   }
//extern void pmic_register_interrupt_callback(unsigned int intNo
//				, void (EINT_FUNC_PTR)(void)) {   }

unsigned short __attribute__ ((weak)) is_battery_remove_pmic(void) { return 0; }

void __attribute__ ((weak)) lockadcch3(void) {   }
void __attribute__ ((weak)) unlockadcch3(void) {   }

unsigned int __attribute__ ((weak)) pmic_Read_Efuse_HPOffset(int i) { return 0; }
void __attribute__ ((weak)) Charger_Detect_Init(void) {   }
void __attribute__ ((weak)) Charger_Detect_Release(void) {   }

int __attribute__ ((weak)) get_dlpt_imix_spm(void) { return 0; }
int __attribute__ ((weak)) get_dlpt_imix(void) { return 0; }
int __attribute__ ((weak)) dlpt_check_power_off(void) { return 0; }
unsigned int __attribute__ ((weak)) pmic_read_vbif28_volt(unsigned int *val) { return 0; }
unsigned int __attribute__ ((weak)) pmic_get_vbif28_volt(void) { return 0; }
void __attribute__ ((weak)) pmic_auxadc_debug(int index) {  }
int __attribute__ ((weak)) PMIC_IMM_GetOneChannelValue(unsigned int dwChannel,
				       int deCount,
				       int trimd) { return 0; }

int __attribute__ ((weak)) get_battery_plug_out_status(void) { return 0; }

void __attribute__ ((weak)) pmic_turn_on_clock(unsigned int enable) {   }
int __attribute__ ((weak)) do_ptim_ex(bool isSuspend, unsigned int *bat, signed int *cur) { return 0; }
void __attribute__ ((weak)) get_ptim_value(bool isSuspend, unsigned int *bat, signed int *cur) {   }

int __attribute__ ((weak)) pmic_pre_wdt_reset(void) { return 0; }
int __attribute__ ((weak)) pmic_pre_condition1(void) { return 0; }
int __attribute__ ((weak)) pmic_pre_condition2(void) { return 0; }
int __attribute__ ((weak)) pmic_pre_condition3(void) { return 0; }
int __attribute__ ((weak)) pmic_post_condition1(void) { return 0; }
int __attribute__ ((weak)) pmic_post_condition2(void) { return 0; }
int __attribute__ ((weak)) pmic_post_condition3(void) { return 0; }
int __attribute__ ((weak)) pmic_dump_all_reg(void) { return 0; }
	__attribute__ ((weak)) 
int __attribute__ ((weak)) pmic_force_vcore_pwm(bool enable) { return 0; }
	__attribute__ ((weak)) 
int __attribute__ ((weak)) is_ext_buck_gpio_exist(void) { return 0; }
int __attribute__ ((weak)) is_ext_buck_exist(void) { return 0; }
int __attribute__ ((weak)) is_ext_buck2_exist(void) { return 0; }
int __attribute__ ((weak)) is_ext_vbat_boost_exist(void) { return 0; }
int __attribute__ ((weak)) is_ext_swchr_exist(void) { return 0; }

/*----- Smart Reset -----*/
void __attribute__ ((weak)) pmic_enable_smart_reset(unsigned char smart_en,
				    unsigned char smart_sdn_en) {   }
/*----- BAT_TEMP detection -----*/
void __attribute__ ((weak)) enable_bat_temp_det(bool en) {   }

int __attribute__ ((weak)) vproc_pmic_set_mode(unsigned char mode) { return 0; }
int __attribute__ ((weak)) vcore_pmic_set_mode(unsigned char mode) { return 0; }
void __attribute__ ((weak)) wk_auxadc_bgd_ctrl(unsigned char en) {}
void __attribute__ ((weak)) register_dlpt_notify(void (*dlpt_callback)(unsigned int dlpt_val),
                                                enum DLPT_PRIO_TAG prio_val) {}
unsigned int __attribute__ ((weak)) ppm_get_cluster_cpi(unsigned int cluster) {return 0; }
