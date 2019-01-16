#ifndef _MT_SLEEP_
#define _MT_SLEEP_

#include <linux/kernel.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_sleep.h>

/* defined in mt_spm.h */
#if 0
#define WAKE_SRC_TS                 (1U << 1)
#define WAKE_SRC_KP                 (1U << 2)
#define WAKE_SRC_WDT                (1U << 3)
#define WAKE_SRC_GPT                (1U << 4)
#define WAKE_SRC_EINT               (1U << 5)
#define WAKE_SRC_CONN_WDT           (1U << 6)
#define WAKE_SRC_CCIF_MD            (1U << 8)
#define WAKE_SRC_LOW_BAT            (1U << 9)
#define WAKE_SRC_CONN               (1U << 10)
#define WAKE_SRC_USB_CD             (1U << 14)
#define WAKE_SRC_USB_PDN            (1U << 16)
#define WAKE_SRC_DBGSYS             (1U << 18)
#define WAKE_SRC_UART0              (1U << 19)
#define WAKE_SRC_AFE                (1U << 20)
#define WAKE_SRC_THERM              (1U << 21)
#define WAKE_SRC_CIRQ               (1U << 22)
#define WAKE_SRC_SYSPWREQ           (1U << 24)
#define WAKE_SRC_MD_WDT             (1U << 25)
#define WAKE_SRC_CPU0_IRQ           (1U << 26)
#define WAKE_SRC_CPU1_IRQ           (1U << 27)
#define WAKE_SRC_CPU2_IRQ           (1U << 28)
#define WAKE_SRC_CPU3_IRQ           (1U << 29)
#endif

#define WAKE_SRC_CFG_KEY            (1U << 31)

extern int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on);

extern wake_reason_t slp_get_wake_reason(void);
extern bool slp_will_infra_pdn(void);
extern void slp_dualvcore_en(bool en);
extern bool slp_get_dualvcore(void);
extern bool slp_dualvcore_ready(void);
extern void slp_pasr_en(bool en, u32 value);
extern void slp_dpd_en(bool en);

extern void slp_set_auto_suspend_wakelock(bool lock);
extern void slp_start_auto_suspend_resume_timer(u32 sec);
extern void slp_create_auto_suspend_resume_thread(void);

extern void slp_module_init(void);

#endif
