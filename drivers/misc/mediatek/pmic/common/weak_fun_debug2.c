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
#include <linux/dcache.h>

#include <mach/upmu_hw.h>
#include <mach/upmu_sw.h>

#include <mt-plat/upmu_common.h>
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_debugfs.h"
#include "include/pmic_api_buck.h"
#include "include/pmic_bif.h"
#include "include/pmic_auxadc.h"


#if 0
/* /proj/mtk22477/casws_mtk22477/lc1/kernel-4.19/drivers/misc/mediatek/include/mt-plat */
/*
-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include \
-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach \
 */

/*
 * PMIC EXTERN FUNCTIONS
 */
/*----- BATTERY_OC_PROTECT -----*/
 void __attribute__ ((weak)) exec_battery_oc_callback(BATTERY_OC_LEVEL battery_oc_level){  }
 void __attribute__ ((weak)) bat_oc_h_en_setting(int en_val){  }
 void __attribute__ ((weak)) bat_oc_l_en_setting(int en_val){  }
/*----- CHRDET_PROTECT -----*/
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
 unsigned int __attribute__ ((weak)) upmu_get_rgs_chrdet(void){ return 0; }
#endif
/*----- PMIC thread -----*/
 void __attribute__ ((weak)) pmic_enable_charger_detection_int(int x){  }
/*----- LDO OC  -----*/
 void __attribute__ ((weak)) msdc_sd_power_off(void){  }
/*----- REGULATOR -----*/
 int __attribute__ ((weak)) mtk_regulator_init(struct platform_device *dev){ return 0; }
 unsigned int __attribute__ ((weak)) pmic_config_interface_buck_vsleep_check(unsigned int RegNum,
	unsigned int val, unsigned int MASK, unsigned int SHIFT){ return 0; }
 void __attribute__ ((weak)) pmic_regulator_debug_init(
	struct platform_device *dev, struct dentry *debug_dir){  }
 void __attribute__ ((weak)) pmic_regulator_suspend(void){  }
 void __attribute__ ((weak)) pmic_regulator_resume(void){  }
/*----- EFUSE -----*/
 int __attribute__ ((weak)) pmic_read_VMC_efuse(void){ return 0; }
/*----- Others -----*/
 int __attribute__ ((weak)) PMIC_POWER_HOLD(unsigned int hold){ return 0; }
 void __attribute__ ((weak)) mt_power_off(void){  }
 unsigned int __attribute__ ((weak)) bat_get_ui_percentage(void){ return 0; }
 signed int __attribute__ ((weak)) fgauge_read_IM_current(void *data){ return 0; }
 void __attribute__ ((weak)) pmic_auxadc_lock(void){  }
 void __attribute__ ((weak)) pmic_auxadc_unlock(void){  }
 signed int __attribute__ ((weak)) fgauge_read_v_by_d(int d_val){ return 0; }
 signed int __attribute__ ((weak)) fgauge_read_r_bat_by_v(signed int voltage){ return 0; }
 void __attribute__ ((weak)) kpd_pwrkey_pmic_handler(unsigned long pressed){  }
 void __attribute__ ((weak)) kpd_pmic_rstkey_handler(unsigned long pressed){  }
 int __attribute__ ((weak)) is_mt6311_sw_ready(void){ return 0; }
 int __attribute__ ((weak)) is_mt6311_exist(void){ return 0; }
 int __attribute__ ((weak)) get_mt6311_i2c_ch_num(void){ return 0; }
#if !defined CONFIG_MTK_LEGACY
 void __attribute__ ((weak)) pmu_drv_tool_customization_init(void){  }
#endif
 int __attribute__ ((weak)) batt_init_cust_data(void){ return 0; }

 unsigned int __attribute__ ((weak)) mt_gpio_to_irq(unsigned int gpio){return 0;  }
 int __attribute__ ((weak)) mt_gpio_set_debounce(unsigned int gpio, unsigned int debounce){ return 0; }
#ifdef CONFIG_MTK_PMIC_COMMON
 int __attribute__ ((weak)) PMIC_check_battery(void){ return 0; }
 int __attribute__ ((weak)) PMIC_check_wdt_status(void){ return 0; }
 int __attribute__ ((weak)) PMIC_check_pwrhold_status(void){ return 0; }
 void __attribute__ ((weak)) PMIC_LP_INIT_SETTING(void){  }
 int __attribute__ ((weak)) PMIC_MD_INIT_SETTING_V1(void){ return 0; }
 void __attribute__ ((weak)) PMIC_PWROFF_SEQ_SETTING(void){  }
 int __attribute__ ((weak)) pmic_tracking_init(void){ return 0; }
#endif
 unsigned int __attribute__ ((weak)) PMIC_CHIP_VER(void){ return 0; }
 void __attribute__ ((weak)) record_md_vosel(void){  }
 
 void __attribute__ ((weak)) register_low_battery_notify(
	void (*low_battery_callback)(enum LOW_BATTERY_LEVEL_TAG),
	enum LOW_BATTERY_PRIO_TAG prio_val)
{
}

void __attribute__ ((weak)) register_low_battery_notify_ext(
	void (*low_battery_callback)(enum LOW_BATTERY_LEVEL_TAG),
	enum LOW_BATTERY_PRIO_TAG prio_val)
{
}

int __attribute__ ((weak)) dlpt_check_power_off(void)
{
	return 0;
}

unsigned int __attribute__ ((weak)) PMIC_LP_CHIP_VER(void) { return 0; }
unsigned int __attribute__ ((weak)) is_pmic_mrv(void) { return 0; }
void __attribute__ ((weak)) PMIC_INIT_SETTING_V1(void) {  }
int __attribute__ ((weak)) do_ptim_ex(bool isSuspend, unsigned int *bat, signed int *cur) { return 0; }

#if defined(CONFIG_MACH_MT6768) || defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6781)
int __attribute__ ((weak)) pmic_buck_vproc_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_buck_vcore_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_buck_vmodem_lp(enum BUCK_LDO_EN_USER user,
			       unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_buck_vs1_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_buck_vpa_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vsram_proc_lp(enum BUCK_LDO_EN_USER user,
				  unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vsram_others_lp(enum BUCK_LDO_EN_USER user,
				    unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vfe28_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vxo22_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vrf18_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vrf12_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vefuse_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vcn33_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vcn28_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vcn18_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vcama_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vcamd_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vcamio_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vldo28_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vaux18_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vaud28_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vio28_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vdram_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vmc_lp(enum BUCK_LDO_EN_USER user,
			   unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vmch_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vemc_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vsim1_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vsim2_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vibr_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vusb33_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_tref_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg){ return 0; }
int __attribute__ ((weak)) pmic_ldo_vio18_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg){ return 0; }
#endif

/*kernel-4.14/drivers/misc/mediatek/pmic/mt6359p/v1/pmic_lp_api.c*/
#if defined(CONFIG_MACH_MT6877)
int __attribute__ ((weak)) pmic_ldo_vio18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg) { return 0; }
#endif

void __attribute__ ((weak)) register_battery_oc_notify(
  			void (*battery_oc_callback)(BATTERY_OC_LEVEL tag),
  			BATTERY_OC_PRIO prio_val) {}
#endif

#include <mt-plat/mtk_boot_common.h>
void __attribute__ ((weak)) mt_usb_connect_v1(void){}
void __attribute__ ((weak)) mt_usb_disconnect_v1(void){}
enum boot_mode_t __attribute__ ((weak)) get_boot_mode(void){ return 0;}
