#ifndef _MT_SPM_SLEEP_
#define _MT_SPM_SLEEP_

#include <linux/kernel.h>
#include <mach/mt_spm.h>
/*
 * for suspend
 */
extern int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace);
extern u32 spm_get_sleep_wakesrc(void);
extern wake_reason_t spm_go_to_sleep(u32 spm_flags, u32 spm_data);

extern bool spm_is_md_sleep(void);
extern bool spm_is_md1_sleep(void);
#if 0 // No connsys
extern bool spm_is_conn_sleep(void);
#endif
extern void spm_set_wakeup_src_check(void);
extern bool spm_check_wakeup_src(void);
extern void spm_poweron_config_set(void);
extern void spm_md32_sram_con(u32 value);
extern void spm_ap_mdsrc_req(u8 set);
extern void spm_force_lte_onoff(u8 onoff);

extern void spm_output_sleep_option(void);
extern void spm_suspend_init(void);
/**************************************
 * LAST PC  API
 **************************************/
extern void read_pcm_data(int *sram_data, int length);
#ifdef CONFIG_ARM64
extern bool spm_read_eint_status (unsigned int eint_num);
#endif
#endif
