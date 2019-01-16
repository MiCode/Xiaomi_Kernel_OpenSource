#ifndef _EXT_EMD_CTL_HAL_H_
#define _EXT_EMD_CTL_HAL_H_
extern void cm_gpio_setup(void);
extern void cm_hold_wakeup_md_signal(void);
extern void cm_release_wakeup_md_signal(void);
extern void cm_enable_ext_md_wakeup_irq(void);
extern int cm_enter_md_download_mode(void);
extern int  cm_do_md_power_on(int bootmode);
extern int  cm_do_md_power_off(void);
extern int  cm_do_md_go(void);
extern int  cm_get_assertlog_status(void);
extern void cm_hold_rst_signal(void);
extern void cm_disable_ext_md_wdt_irq(void);
extern void cm_disable_ext_md_wakeup_irq(void);
extern void cm_disable_ext_md_exp_irq(void);
extern int cm_register_irq_cb(int type, void(*irq_cb)(void));
extern int is_traffic_on(int type);
extern int switch_sim_mode(int id, char *buf, unsigned int len);
extern unsigned int get_sim_switch_type(void);
#endif //_EXT_EMD_CTL_HAL_H_