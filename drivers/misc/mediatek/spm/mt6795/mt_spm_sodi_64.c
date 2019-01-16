#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>  
#include <linux/lockdep.h>

#include <mach/irqs.h>
#include <mach/mt_cirq.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_cpuidle.h>
#include <mach/mt_gpt.h>
#include <mach/mt_cpufreq.h>
#include <mach/mt_dramc.h>
#include <mach/mt_boot.h>

#include "mt_spm_internal.h"


/**************************************
 * only for internal debug
 **************************************/
//FIXME: for FPGA early porting
#define  CONFIG_MTK_LDVT

#ifdef CONFIG_MTK_LDVT
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define WAKE_SRC_FOR_SODI \
    (WAKE_SRC_KP | WAKE_SRC_GPT | WAKE_SRC_EINT | WAKE_SRC_CCIF_MD |        \
     WAKE_SRC_MD32 | WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | \
     WAKE_SRC_AFE | WAKE_SRC_CIRQ | WAKE_SRC_SYSPWREQ |         \
     WAKE_SRC_MD_WDT | WAKE_SRC_CLDMA_MD | WAKE_SRC_SEJ | WAKE_SRC_CPU_IRQ)

#define WAKE_SRC_FOR_MD32  0x00002008                                          \
    //(WAKE_SRC_AUD_MD32)

#ifdef CONFIG_MTK_RAM_CONSOLE
#define SPM_AEE_RR_REC 1
#else
#define SPM_AEE_RR_REC 0
#endif

#if SPM_AEE_RR_REC
enum spm_sodi_step
{
	SPM_SODI_ENTER=0,
	SPM_SODI_ENTER_SPM_FLOW,
	SPM_SODI_ENTER_WFI,
	SPM_SODI_LEAVE_WFI,
	SPM_SODI_LEAVE_SPM_FLOW,
	SPM_SODI_LEAVE
};
#endif

#define CA7_BUS_CONFIG          (CA7MCUCFG_BASE + 0x1C) //0x1020011c

// ==========================================
// PCM code for SODI (Screen On Deep Idle)
//
// core 0 : GPT 4
// ==========================================
static const u32 sodi_binary[] = {
	0x814e0001, 0xd82000a5, 0x17c07c1f, 0xd0000260, 0x17c07c1f, 0x80340400,
	0x80310400, 0xe8208000, 0x10006354, 0xfffe7b01, 0x81fa0407, 0x81f18407,
	0x81f08407, 0xa1dc0407, 0xa1d80407, 0x1b80001f, 0x200000d0, 0xd0000e20,
	0x17c07c1f, 0x1880001f, 0x20000208, 0x81411801, 0xd8000445, 0x17c07c1f,
	0xe8208000, 0x1000f600, 0xd2000000, 0x1380081f, 0x18c0001f, 0x10006240,
	0xe0e00016, 0xe0e0001e, 0xe0e0000e, 0xe0e0000f, 0x81fe8407, 0x80368400,
	0x1380081f, 0x80370400, 0x1380081f, 0x80360400, 0x803e0400, 0x1b80001f,
	0x20000034, 0x80380400, 0x803b0400, 0x803d0400, 0x18c0001f, 0x10006204,
	0xe0e00007, 0xa01d8400, 0x1b80001f, 0x20000034, 0xe0e00000, 0x803d8400,
	0x1b80001f, 0x20000152, 0x18c0001f, 0x1000f5c8, 0x1910001f, 0x1000f5c8,
	0xa1000404, 0xe0c00004, 0x18c0001f, 0x100125c8, 0x1910001f, 0x100125c8,
	0xa1000404, 0xe0c00004, 0x1910001f, 0x100125c8, 0xa01d0400, 0x80340400,
	0x17c07c1f, 0x17c07c1f, 0x80310400, 0x1b80001f, 0x2000000a, 0x18c0001f,
	0x10006240, 0xe0e0000d, 0x81411801, 0xd8000ce5, 0x17c07c1f, 0x18c0001f,
	0x100040f4, 0x1910001f, 0x100040f4, 0xa11c8404, 0xe0c00004, 0x1b80001f,
	0x2000000a, 0x813c8404, 0xe0c00004, 0x18c0001f, 0x100110f4, 0x1910001f,
	0x100110f4, 0xa11c8404, 0xe0c00004, 0x1b80001f, 0x2000000a, 0x813c8404,
	0xe0c00004, 0x1b80001f, 0x20000100, 0x81fa0407, 0x81f18407, 0x81f08407,
	0xe8208000, 0x10006354, 0xfffe7b01, 0xa1d80407, 0xa1dc0407, 0x8880000c,
	0x2f7be75f, 0xd8200f22, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0xd0000f60,
	0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xf0000000, 0x17c07c1f, 0x1950001f,
	0x10006b04, 0x81439401, 0xd8001145, 0x17c07c1f, 0x81491801, 0xd8001145,
	0x17c07c1f, 0x81481801, 0xd8001145, 0x17c07c1f, 0xd0001360, 0x17c07c1f,
	0x81fc0407, 0x81f80407, 0xe8208000, 0x10006354, 0xfffe7b07, 0x1880001f,
	0x10006320, 0xc0c01ee0, 0xe080000f, 0xd80014e3, 0x17c07c1f, 0xe080001f,
	0xa1da0407, 0xa0110400, 0xa0140400, 0xd0001da0, 0x17c07c1f, 0x1b80001f,
	0x20000fdf, 0x1890001f, 0x10006608, 0x80c98801, 0x810a8801, 0x10918c1f,
	0xa0939002, 0xa0950402, 0x8080080d, 0xd82016a2, 0x17c07c1f, 0x81f18407,
	0x81f08407, 0xa1dc0407, 0xa1d80407, 0x1b00001f, 0x3fffe7ff, 0x1b80001f,
	0x20000004, 0xd8001e0c, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0001e00,
	0x17c07c1f, 0x81f80407, 0x81fc0407, 0xe8208000, 0x10006354, 0xfffe7b07,
	0x1880001f, 0x10006320, 0xc0c01ee0, 0xe080000f, 0xd80014e3, 0x17c07c1f,
	0xe080001f, 0xa1da0407, 0xa0110400, 0xa0140400, 0x1950001f, 0x10006b04,
	0x81439401, 0xd8201965, 0x17c07c1f, 0xd0001da0, 0x17c07c1f, 0x18c0001f,
	0x1000f5c8, 0x1910001f, 0x1000f5c8, 0x81200404, 0xe0c00004, 0x18c0001f,
	0x100125c8, 0x1910001f, 0x100125c8, 0x81200404, 0xe0c00004, 0x1910001f,
	0x100125c8, 0xa01d0400, 0xa01b0400, 0xa0180400, 0x803d8400, 0xa01e0400,
	0xa0160400, 0xa0170400, 0xa0168400, 0x1b80001f, 0x20000104, 0x81411801,
	0xd8001da5, 0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c01e40, 0x17c07c1f,
	0xe8208000, 0x1000f600, 0xd2000001, 0x02000408, 0x1b00001f, 0x7fffe7ff,
	0xf0000000, 0x17c07c1f, 0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e, 0xf0000000,
	0xe0f07f12, 0x11407c1f, 0x81f08407, 0x81f18407, 0x1b80001f, 0x20000001,
	0xa1d08407, 0xa1d18407, 0x1392841f, 0x812ab401, 0x80ebb401, 0xa0c00c04,
	0xd82020e3, 0x17c07c1f, 0x80c01403, 0xd8201f03, 0x01400405, 0x1900001f,
	0x10006814, 0xf0000000, 0xe1000003, 0x18d0001f, 0x10006604, 0x10cf8c1f,
	0xd8202163, 0x17c07c1f, 0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0xe8208000, 0x10006b6c, 0x00000000, 0x1b00001f, 0x2f7be75f,
	0x1b80001f, 0x500f0000, 0xe8208000, 0x10006354, 0xfffe7b01, 0xc0c064c0,
	0x81401801, 0xd80045e5, 0x17c07c1f, 0x81f60407, 0x18c0001f, 0x10006200,
	0xc0c05cc0, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001, 0x1b80001f,
	0x20000080, 0xc0c05cc0, 0x1280041f, 0x18c0001f, 0x10006208, 0xc0c05cc0,
	0x12807c1f, 0x1b80001f, 0x20000003, 0xe8208000, 0x10006248, 0x00000000,
	0x1b80001f, 0x20000080, 0xc0c05cc0, 0x1280041f, 0xe8208000, 0x10006404,
	0x00003101, 0x1b00001f, 0x2f7be75f, 0x1b80001f, 0x30000004, 0x8880000c,
	0x2f7be75f, 0xd8005782, 0x17c07c1f, 0x81449801, 0xd8004a25, 0x17c07c1f,
	0x81459801, 0xd8004865, 0x17c07c1f, 0x18c0001f, 0x10005468, 0x1111041f,
	0xe0c00004, 0xd00048e0, 0x17c07c1f, 0x18c0001f, 0x10005468, 0x1113841f,
	0xe0c00004, 0x1b80001f, 0x20000104, 0x1a00001f, 0x10006604, 0xe2200003,
	0xc0c06100, 0x17c07c1f, 0xe2200001, 0xc0c06100, 0x17c07c1f, 0x18c0001f,
	0x10209f4c, 0x1910001f, 0x10209f4c, 0xa1120404, 0xe0c00004, 0xa1d38407,
	0xa0108400, 0xa0148400, 0xa01b8400, 0xa0188400, 0x18c0001f, 0x10209200,
	0x1910001f, 0x10209200, 0x81200404, 0xe0c00004, 0x18c0001f, 0x1020920c,
	0x1910001f, 0x1020920c, 0xa1108404, 0xe0c00004, 0x81200404, 0xe0c00004,
	0xe8208000, 0x10006310, 0x0b160008, 0x81431801, 0xd8004ec5, 0x17c07c1f,
	0xe8208000, 0x10006310, 0x0b1600c8, 0x12007c1f, 0x1b00001f, 0xbfffe7ff,
	0x1b80001f, 0x90100000, 0x1ac0001f, 0x10006b6c, 0xe2c00008, 0xe8208000,
	0x10006310, 0x0b160008, 0x80c09c01, 0xc8c00003, 0x17c07c1f, 0x81449801,
	0xd8005365, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200000, 0xc0c06100,
	0x17c07c1f, 0xe2200002, 0xc0c06100, 0x17c07c1f, 0x1b80001f, 0x20000104,
	0x81459801, 0xd80052e5, 0x17c07c1f, 0x18c0001f, 0x10005464, 0x1111041f,
	0xe0c00004, 0xd0005360, 0x17c07c1f, 0x18c0001f, 0x10005464, 0x1113841f,
	0xe0c00004, 0x80388400, 0x18c0001f, 0x1020920c, 0x1910001f, 0x1020920c,
	0xa1000404, 0xe0c00004, 0x1b80001f, 0x20000300, 0x803b8400, 0x81308404,
	0xe0c00004, 0x18c0001f, 0x10209200, 0x1910001f, 0x10209200, 0xa1000404,
	0xe0c00004, 0x1910001f, 0x10209200, 0x1b80001f, 0x20000300, 0x80348400,
	0x1b80001f, 0x20000104, 0x80308400, 0x81f38407, 0x18c0001f, 0x10209f4c,
	0x1910001f, 0x10209f4c, 0x81320404, 0xe0c00004, 0x81401801, 0xd8005b45,
	0x17c07c1f, 0xe8208000, 0x10006404, 0x00002101, 0x18c0001f, 0x10006208,
	0x1212841f, 0xc0c05e40, 0x12807c1f, 0xe8208000, 0x10006248, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c05e40, 0x1280041f, 0x18c0001f, 0x10006200,
	0x1212841f, 0xc0c05e40, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000,
	0x1b80001f, 0x20000080, 0xc0c05e40, 0x1280041f, 0x81f48407, 0xa1d60407,
	0x81f10407, 0xa1db0407, 0x81fd8407, 0x81fe0407, 0x1ac0001f, 0x55aa55aa,
	0xe8208000, 0x100063e0, 0x00000001, 0xf0000000, 0xd8005d6a, 0x17c07c1f,
	0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xd8205e0a, 0x17c07c1f, 0xe2e0002e,
	0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0xd8005f0a, 0x17c07c1f,
	0xe2e00036, 0xe2e0003e, 0x1380201f, 0xe2e0003c, 0xd820600a, 0x17c07c1f,
	0xe2e0007c, 0x1b80001f, 0x20000003, 0xe2e0005c, 0xe2e0004c, 0xe2e0004d,
	0xf0000000, 0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0x1393041f,
	0xf0000000, 0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd8206103,
	0x17c07c1f, 0xf0000000, 0x17c07c1f, 0xe8208000, 0x11008014, 0x00000002,
	0xe8208000, 0x11008020, 0x00000101, 0xe8208000, 0x11008004, 0x000000d0,
	0x1a00001f, 0x11008000, 0xd80063ca, 0xe220005d, 0xd82063ea, 0xe2200000,
	0xe2200001, 0xe8208000, 0x11008024, 0x00000001, 0x1b80001f, 0x20000424,
	0xf0000000, 0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000,
	0x17c07c1f
};

static struct pcm_desc sodi_pcm = {
	.version	= "pcm_sodi_v2.11_20150326",
	.base		= sodi_binary,
	.size		= 811,
	.sess		= 2,
	.replace	= 0,
	.vec2		= EVENT_VEC(30, 1, 0, 0),	/* FUNC_APSRC_WAKEUP */
	.vec3		= EVENT_VEC(31, 1, 0, 125),	/* FUNC_APSRC_SLEEP */
};

static struct pwr_ctrl sodi_ctrl = {
	.wake_src		= WAKE_SRC_FOR_SODI,
	.wake_src_md32		= WAKE_SRC_FOR_MD32,
	.r0_ctrl_en		= 1,
	.r7_ctrl_en		= 1,
	.wfi_op			= WFI_OP_AND,
#if 1
	.ca15_wfi0_en		= 1,
	.ca15_wfi1_en		= 1,
	.ca15_wfi2_en		= 1,
	.ca15_wfi3_en		= 1,
	.ca7_wfi0_en		= 1,
	.ca7_wfi1_en		= 1,
	.ca7_wfi2_en		= 1,
	.ca7_wfi3_en		= 1,
	.md2_req_mask		= 1,
	.mfg_req_mask		= 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask		= 1,
#endif
#else
	.ca15_wfi0_en		= 1,
	.ca15_wfi1_en		= 1,
	.ca15_wfi2_en		= 1,
	.ca15_wfi3_en		= 1,
	.ca7_wfi0_en		= 1,
	.ca7_wfi1_en		= 1,
	.ca7_wfi2_en		= 1,
	.ca7_wfi3_en		= 1,
	.md2_req_mask		= 1,
	.md1_req_mask		= 0,
	.disp_req_mask		= 0,
	.mfg_req_mask		= 1,
	.md32_req_mask		= 0,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask		= 1,
#endif

#endif
};

struct spm_lp_scen __spm_sodi = {
	.pcmdesc	= &sodi_pcm,
	.pwrctrl	= &sodi_ctrl,
};

static bool gSpm_SODI_mempll_pwr_mode = 0;
static bool gSpm_sodi_en=0;

extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);

extern void soidle_before_wfi(int cpu);
extern void soidle_after_wfi(int cpu);

#if SPM_AEE_RR_REC
extern void aee_rr_rec_sodi_val(u32 val);
extern u32 aee_rr_curr_sodi_val(void);
#endif

void __attribute__((weak)) soidle_before_wfi(int cpu)
{
}

void __attribute__((weak)) soidle_after_wfi(int cpu)
{
}
static void spm_trigger_wfi_for_sodi(struct pwr_ctrl *pwrctrl)
{
    //sync_hw_gating_value();     /* for Vcore DVFS */

    if (is_cpu_pdn(pwrctrl->pcm_flags)) {
        mt_cpu_dormant(CPU_SODI_MODE);
    } else {
        
//        spm_write(CA7_BUS_CONFIG, spm_read(CA7_BUS_CONFIG) | 0x10);
        wfi_with_sync();
//        spm_write(CA7_BUS_CONFIG, spm_read(CA7_BUS_CONFIG) & ~0x10);

    }
}

/*
  PCM_FLAGS bit18:
    Selection of MEMPLL CG mode or shutdown mode for CPU
    1’b0: shutdown mode
    1’b1: 6795:CG mode (6795M reset mode) 

*/
#define SPM_MEMPLL_CPU	(1U << 18)

void spm_go_to_sodi(u32 spm_flags, u32 spm_data)
{
    struct wake_status wakesta;
    unsigned long flags;
    struct mtk_irq_mask mask;
    wake_reason_t wr = WR_NONE;
    struct pcm_desc *pcmdesc = __spm_sodi.pcmdesc;
    struct pwr_ctrl *pwrctrl = __spm_sodi.pwrctrl;

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(1<<SPM_SODI_ENTER);
#endif 

	if (gSpm_SODI_mempll_pwr_mode) {
		/* CG mode */
		spm_flags |= SPM_MEMPLL_CPU;
	} else {
		/* PD mode */
		spm_flags &= ~SPM_MEMPLL_CPU;
	}

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

    /* set PMIC WRAP table for deepidle power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_SODI);	

    soidle_before_wfi(0);
    lockdep_off(); 
    spin_lock_irqsave(&__spm_lock, flags);

    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(SPM_IRQ0_ID/*MT_SPM_IRQ_ID*/);
    mt_cirq_clone_gic();
    mt_cirq_enable();

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_ENTER_SPM_FLOW));
#endif   

    __spm_reset_and_init_pcm(pcmdesc);
#if 0
    /* 0: mempll shutdown mode; 1: cg mode */
    gSpm_SODI_mempll_pwr_mode ? (pwrctrl->pcm_flags |= SPM_MEMPLL_CPU) :
				(pwrctrl->pcm_flags &= ~SPM_MEMPLL_CPU);
#endif

    __spm_kick_im_to_fetch(pcmdesc);

    __spm_init_pcm_register();

    __spm_init_event_vector(pcmdesc);

	/* set pcm_apsrc_req to be 1 if 10006b0c[0] is 1 */
	if (spm_read(SPM_PCM_SRC_REQ) & 1) {
		/* request apsrc */
		pwrctrl->pcm_apsrc_req = 1;
	} else {
		/* release apsrc */
		pwrctrl->pcm_apsrc_req = 0;
	}

    __spm_set_power_control(pwrctrl);

    __spm_set_wakeup_event(pwrctrl);

    /* set pcm_flags[18] to be 1 if 10006b08[7] is 1 */
    if ((spm_read(SPM_PCM_FLAGS) & SPM_MEMPLL_RESET) ||
        gSpm_SODI_mempll_pwr_mode ||
        (pwrctrl->pcm_flags_cust & SPM_MEMPLL_CPU))
        pwrctrl->pcm_flags |= SPM_MEMPLL_CPU;
    else
        pwrctrl->pcm_flags &= ~SPM_MEMPLL_CPU;

    __spm_kick_pcm_to_run(pwrctrl);

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_ENTER_WFI));
#endif

    spm_trigger_wfi_for_sodi(pwrctrl);

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_LEAVE_WFI));
#endif  

    __spm_get_wakeup_status(&wakesta);

    __spm_clean_after_wakeup();	

    wr = __spm_output_wake_reason(&wakesta, pcmdesc, false);
    /* for test */
    /* wr = __spm_output_wake_reason(&wakesta, pcmdesc, true); */

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_LEAVE_SPM_FLOW));
#endif  

    mt_cirq_flush();
    mt_cirq_disable();
    mt_irq_mask_restore(&mask);

    spin_unlock_irqrestore(&__spm_lock, flags);
    lockdep_on();
    soidle_after_wfi(0);
	
     /* set PMIC WRAP table for normal power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);  

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(0);
#endif 
    //return wr;

}
void spm_sodi_mempll_pwr_mode(bool pwr_mode)
{
    gSpm_SODI_mempll_pwr_mode = pwr_mode;
}

void spm_enable_sodi(bool en)
{
    gSpm_sodi_en=en;
}

bool spm_get_sodi_en(void)
{
    return gSpm_sodi_en;
}

#if SPM_AEE_RR_REC
static void spm_sodi_aee_init(void)
{
    aee_rr_rec_sodi_val(0);
}
#endif

void spm_sodi_init(void)
{
#if SPM_AEE_RR_REC
    spm_sodi_aee_init();
#endif
}


#if 0
void spm_sodi_lcm_video_mode(bool IsLcmVideoMode)
{
    gSpm_IsLcmVideoMode = IsLcmVideoMode;

    spm_idle_ver("spm_sodi_lcm_video_mode() : gSpm_IsLcmVideoMode = %x\n", gSpm_IsLcmVideoMode);    
    
}
#endif
MODULE_DESCRIPTION("SPM-SODI Driver v0.1");
