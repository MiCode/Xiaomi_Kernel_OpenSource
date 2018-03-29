/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MT_WD_API_H_
#define _MT_WD_API_H_
/* WD MODE MARK BIT */
#define MTK_WDT_REQ_DEBUG_EN_MARK		(0x80000)
#define MTK_WDT_REQ_SPM_THERMAL_MARK		(0x0001)
#define MTK_WDT_REQ_SPM_SCPSYS_MARK		(0x0002)
#define MTK_WDT_REQ_EINT_MARK			(1<<2)
#define MTK_WDT_REQ_SYSRST_MARK			(1<<3)
#define MTK_WDT_REQ_THERMAL_MARK		(1<<18)

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE  (1)
#endif

typedef enum ext_wdt_mode {
	WDT_IRQ_ONLY_MODE,
	WDT_HW_REBOOT_ONLY_MODE,
	WDT_DUAL_MODE,
} WD_MODE;

typedef enum wk_wdt_en {
	WK_WDT_DIS,
	WK_WDT_EN,
} WD_CTL;


typedef enum wd_restart_type {
	WD_TYPE_NORMAL,
	WD_TYPE_NOLOCK,
} WD_RES_TYPE;

typedef enum wk_req_en {
	WD_REQ_DIS,
	WD_REQ_EN,
} WD_REQ_CTL;

typedef enum wk_req_mode {
	WD_REQ_IRQ_MODE,
	WD_REQ_RST_MODE,
} WD_REQ_MODE;

struct wd_api {
	long ready;
	int (*wd_restart)(enum wd_restart_type type);
	int (*wd_cpu_hot_plug_on_notify)(int);
	int (*wd_cpu_hot_plug_off_notify)(int);
	int (*wd_sw_reset)(int);
	int (*wd_config)(enum ext_wdt_mode, int timeout_val);
	int (*wd_disable_ext)(void);
	int (*wd_disable_local)(void);
	int (*wd_disable_all)(void);
	int (*wd_set_mode)(enum ext_wdt_mode);
	int (*wd_aee_confirm_hwreboot)(void);
	void (*wd_suspend_notify)(void);
	void (*wd_resume_notify)(void);
	unsigned int (*wd_get_check_bit)(void);
	unsigned int (*wd_get_kick_bit)(void);
	int (*wd_spmwdt_mode_config)(WD_REQ_CTL en, WD_REQ_MODE mode);
	int (*wd_thermal_mode_config)(WD_REQ_CTL en, WD_REQ_MODE mode);
	int (*wd_dram_reserved_mode)(bool enabled);
	int (*wd_thermal_direct_mode_config)(WD_REQ_CTL en, WD_REQ_MODE mode);
	int (*wd_debug_key_eint_config)(WD_REQ_CTL en, WD_REQ_MODE mode);
	int (*wd_debug_key_sysrst_config)(WD_REQ_CTL en, WD_REQ_MODE mode);
};

int wd_api_init(void);
int get_wd_api(struct wd_api **obj);
void wk_cpu_update_bit_flag(int cpu, int plug_status);
unsigned int get_check_bit(void);
unsigned int get_kick_bit(void);
extern void aee_wdt_printf(const char *fmt, ...);

#endif				/* _MT_WD_API_H_ */
