#ifndef KPD_HAL_H
#define KPD_HAL_H
		  
#include <mtk_kpd.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_boot.h>
#include <mach/irqs.h>
#include <mach/eint.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_sleep.h>
#include <mach/sync_write.h>

/*function define*/
void kpd_slide_qwerty_init(void);
void kpd_ldvt_test_init(void);
void long_press_reboot_function_setting(void);
void kpd_auto_test_for_factorymode(void);
void kpd_wakeup_src_setting(int enable);
void kpd_get_keymap_state(u16 state[]);
void kpd_set_debounce(u16 val);
void kpd_init_keymap(u16 keymap[]);
void kpd_init_keymap_state(u16 keymap_state[]);
void kpd_pmic_rstkey_hal(unsigned long pressed);
void kpd_pmic_pwrkey_hal(unsigned long pressed);
void kpd_pwrkey_handler_hal(unsigned long data);
void mt_eint_register(void);

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
void sb_kpd_enable(void);
void sb_kpd_disable(void);
#else
void sb_kpd_enable(void);
void sb_kpd_disable(void);
#endif

#define KPD_NUM_MEMS	5
#define KPD_MEM5_BITS	8

#define KPD_NUM_KEYS	72	/* 4 * 16 + KPD_MEM5_BITS */

#endif
