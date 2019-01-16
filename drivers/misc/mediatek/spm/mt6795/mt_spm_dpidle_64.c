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
#include <mach/wd_api.h>
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
#define SPM_PWAKE_EN            0
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_PWAKE_EN            1
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define WAKE_SRC_FOR_DPIDLE                                                                      \
    (WAKE_SRC_KP | WAKE_SRC_GPT | WAKE_SRC_EINT | WAKE_SRC_CCIF_MD | WAKE_SRC_MD32 |             \
     WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_AFE | \
     WAKE_SRC_SYSPWREQ | WAKE_SRC_MD_WDT | WAKE_SRC_CLDMA_MD |            \
     WAKE_SRC_SEJ)

#define WAKE_SRC_FOR_MD32  0x00002008                                          \
    //(WAKE_SRC_AUD_MD32)


#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

#ifdef CONFIG_MTK_RAM_CONSOLE
#define SPM_AEE_RR_REC 1
#else
#define SPM_AEE_RR_REC 0
#endif

#if SPM_AEE_RR_REC
enum spm_deepidle_step
{
	SPM_DEEPIDLE_ENTER=0,
	SPM_DEEPIDLE_ENTER_UART_SLEEP,
	SPM_DEEPIDLE_ENTER_WFI,
	SPM_DEEPIDLE_LEAVE_WFI,
	SPM_DEEPIDLE_ENTER_UART_AWAKE,
	SPM_DEEPIDLE_LEAVE
};
#endif

/**********************************************************
 * PCM code for deep idle
 **********************************************************/ 
static const u32 dpidle_binary[] = {
	0x80328400, 0x81419801, 0xd80001c5, 0x17c07c1f, 0x1a00001f, 0x10006604,
	0xe2200004, 0xc0c030e0, 0x17c07c1f, 0xe2200006, 0xc0c030e0, 0x17c07c1f,
	0x1b80001f, 0x20000208, 0xe8208000, 0x10006354, 0xfffe7fbf, 0xc2803000,
	0x1290041f, 0x8880000c, 0x2f7be75f, 0xd8200362, 0x17c07c1f, 0x1b00001f,
	0x7fffe7ff, 0xd00003a0, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xf0000000,
	0x17c07c1f, 0x80880001, 0xd8000482, 0x17c07c1f, 0xd00021a0, 0x1200041f,
	0x18c0001f, 0x10006608, 0x1910001f, 0x10006608, 0x813b0404, 0xe0c00004,
	0xe8208000, 0x10006354, 0xffffffbf, 0x81419801, 0xd8000745, 0x17c07c1f,
	0x1a00001f, 0x10006604, 0xe2200005, 0xc0c030e0, 0x17c07c1f, 0xe2200007,
	0xc0c030e0, 0x17c07c1f, 0x1b80001f, 0x2000008c, 0xc2803000, 0x1290841f,
	0xa0128400, 0x1b00001f, 0x3fffefff, 0xf0000000, 0x17c07c1f, 0x808f9801,
	0xd8200ba2, 0x81481801, 0x80c89801, 0xd82009e3, 0x17c07c1f, 0xd8000945,
	0x17c07c1f, 0x803d8400, 0x18c0001f, 0x10006204, 0xe0e00000, 0x1b80001f,
	0x2000002f, 0x80340400, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x80310400, 0x81fa0407, 0x81f18407, 0x81f08407, 0xa1dc0407, 0x1b80001f,
	0x200000b6, 0xd00019a0, 0x17c07c1f, 0x1880001f, 0x20000208, 0x81411801,
	0xd8000d85, 0x17c07c1f, 0xe8208000, 0x1000f600, 0xd2000000, 0x1380081f,
	0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e, 0xe0e0000e, 0xe0e0000f,
	0x81fe8407, 0x80368400, 0x1380081f, 0x80370400, 0x1380081f, 0x80360400,
	0x803e0400, 0x1b80001f, 0x20000034, 0x80380400, 0x803b0400, 0x803d0400,
	0x18c0001f, 0x10006204, 0xe0e00007, 0xa01d8400, 0x1b80001f, 0x20000034,
	0xe0e00000, 0x803d8400, 0x1b80001f, 0x20000152, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0xa1000404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0xa1000404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0x81481801, 0xd82012e5, 0x17c07c1f, 0xa01d8400, 0xa1de8407, 0xd0001300,
	0x17c07c1f, 0xa01d0400, 0x80340400, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x80310400, 0xe8208000, 0x10000044, 0x00000100, 0x1b80001f,
	0x20000073, 0x18c0001f, 0x10006240, 0xe0e0000d, 0x81411801, 0xd80017a5,
	0x17c07c1f, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4, 0xa11c8404,
	0xe0c00004, 0x1b80001f, 0x2000000a, 0x813c8404, 0xe0c00004, 0x18c0001f,
	0x100110f4, 0x1910001f, 0x100110f4, 0xa11c8404, 0xe0c00004, 0x1b80001f,
	0x2000000a, 0x813c8404, 0xe0c00004, 0x1b80001f, 0x20000100, 0x81fa0407,
	0x81f18407, 0x81f08407, 0xe8208000, 0x10006354, 0xfffe7b07, 0xa1d80407,
	0xa1dc0407, 0x18c0001f, 0x10006608, 0x1910001f, 0x10006608, 0xa11b0404,
	0xe0c00004, 0xc2803000, 0x1291041f, 0x8880000c, 0x2f7be75f, 0xd8201ae2,
	0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0xd0001b20, 0x17c07c1f, 0x1b00001f,
	0xbfffe7ff, 0xf0000000, 0x17c07c1f, 0x1890001f, 0x10006608, 0x808b0801,
	0xd8201e82, 0x17c07c1f, 0x1880001f, 0x10006320, 0xc0c02c80, 0xe080000f,
	0xd8001fe3, 0x17c07c1f, 0xe080001f, 0xa1da0407, 0x81fc0407, 0xa0110400,
	0xa0140400, 0x80c89801, 0xd8201e43, 0x17c07c1f, 0xa01d8400, 0x18c0001f,
	0x10006204, 0xe0e00001, 0xd0002840, 0xa19f8406, 0x1b80001f, 0x20000fdf,
	0x1890001f, 0x10006608, 0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002,
	0x8080080d, 0xd82021a2, 0x12007c1f, 0x81f08407, 0x81f18407, 0xa1d80407,
	0xa1dc0407, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004, 0xd80028cc,
	0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd00028c0, 0x17c07c1f, 0x81f80407,
	0x81fc0407, 0x1880001f, 0x10006320, 0xc0c02c80, 0xe080000f, 0xd8001fe3,
	0x17c07c1f, 0xe080001f, 0xa1da0407, 0xe8208000, 0x10000048, 0x00000100,
	0x1b80001f, 0x20000068, 0xa0110400, 0xa0140400, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0xa01d0400, 0xa01b0400, 0xa0180400, 0x803d8400, 0xa01e0400, 0xa0160400,
	0xa0170400, 0xa0168400, 0x1b80001f, 0x20000104, 0x81411801, 0xd8002805,
	0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c02be0, 0x17c07c1f, 0xe8208000,
	0x1000f600, 0xd2000001, 0xd8000488, 0x81bf8406, 0xc2803000, 0x1291841f,
	0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830,
	0xe1000003, 0x18c0001f, 0x10006834, 0xe0e00000, 0xe0e00001, 0xf0000000,
	0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f, 0xe0f07f0e,
	0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d, 0xe0f07c0d,
	0xe0f0780d, 0xf0000000, 0xe0f0700d, 0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e,
	0xf0000000, 0xe0f07f12, 0x11407c1f, 0x81f08407, 0x81f18407, 0x1b80001f,
	0x20000001, 0xa1d08407, 0xa1d18407, 0x1392841f, 0x812ab401, 0x80ebb401,
	0xa0c00c04, 0xd8202e83, 0x17c07c1f, 0x80c01403, 0xd8202ca3, 0x01400405,
	0x1900001f, 0x10006814, 0xf0000000, 0xe1000003, 0xa1d00407, 0x1b80001f,
	0x20000208, 0x80ea3401, 0x1a00001f, 0x10006814, 0xf0000000, 0xe2000003,
	0x18c0001f, 0x10006b6c, 0x1910001f, 0x10006b6c, 0xa1002804, 0xf0000000,
	0xe0c00004, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd82030e3, 0x17c07c1f,
	0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
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
	0x1b80001f, 0x500f0000, 0xe8208000, 0x10006354, 0xfffe7b07, 0xc0c06b80,
	0x81401801, 0xd8004625, 0x17c07c1f, 0x81f60407, 0x18c0001f, 0x10006200,
	0xc0c06380, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001, 0x1b80001f,
	0x20000080, 0xc0c06380, 0x1280041f, 0x18c0001f, 0x10006208, 0xc0c06380,
	0x12807c1f, 0x1b80001f, 0x20000003, 0xe8208000, 0x10006248, 0x00000000,
	0x1b80001f, 0x20000080, 0xc0c06380, 0x1280041f, 0xe8208000, 0x10006404,
	0x00003101, 0xc2803000, 0x1292041f, 0x1b00001f, 0x2f7be75f, 0x1b80001f,
	0x30000004, 0x8880000c, 0x2f7be75f, 0xd8005e42, 0x17c07c1f, 0xc0c06700,
	0x17c07c1f, 0x18c0001f, 0x10006294, 0xe0f07fff, 0xe0e00fff, 0xe0e000ff,
	0x81449801, 0xd8004b45, 0x17c07c1f, 0x81459801, 0xd8004985, 0x17c07c1f,
	0x18c0001f, 0x10005468, 0x1111041f, 0xe0c00004, 0xd0004a00, 0x17c07c1f,
	0x18c0001f, 0x10005468, 0x1113841f, 0xe0c00004, 0x1b80001f, 0x20000104,
	0x1a00001f, 0x10006604, 0xe2200003, 0xc0c067c0, 0x17c07c1f, 0xe2200001,
	0xc0c067c0, 0x17c07c1f, 0x18c0001f, 0x10209f4c, 0x1910001f, 0x10209f4c,
	0xa1120404, 0xe0c00004, 0xa1d38407, 0xa1d98407, 0xa0108400, 0xa0120400,
	0xa0148400, 0xa0150400, 0xa0158400, 0xa01b8400, 0xa01c0400, 0xa01c8400,
	0xa0188400, 0xa0190400, 0xa0198400, 0x18c0001f, 0x10209200, 0x1910001f,
	0x10209200, 0x81200404, 0xe0c00004, 0x18c0001f, 0x1020920c, 0x1910001f,
	0x1020920c, 0xa1108404, 0xe0c00004, 0x81200404, 0xe0c00004, 0xe8208000,
	0x10006310, 0x0b1600f8, 0x1b00001f, 0xbfffe7ff, 0x1b80001f, 0x90100000,
	0x80c28001, 0xd8205143, 0x17c07c1f, 0xa1dd8407, 0x1b00001f, 0x3fffefff,
	0xd0005000, 0x17c07c1f, 0x1890001f, 0x100063e8, 0x88c0000c, 0x2f7be75f,
	0xd8005363, 0x17c07c1f, 0x80c40001, 0xd80052e3, 0x17c07c1f, 0x1b00001f,
	0xbfffe7ff, 0xd0005320, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd0005000,
	0x17c07c1f, 0x80c40001, 0xd8205463, 0x17c07c1f, 0xa1de0407, 0x1b00001f,
	0x7fffe7ff, 0xd0005000, 0x17c07c1f, 0x18c0001f, 0x10006294, 0xe0e001fe,
	0xe0e003fc, 0xe0e007f8, 0xe0e00ff0, 0x1b80001f, 0x20000020, 0xe0f07ff0,
	0xe0f07f00, 0x81449801, 0xd80058e5, 0x17c07c1f, 0x1a00001f, 0x10006604,
	0xe2200000, 0xc0c067c0, 0x17c07c1f, 0xe2200002, 0xc0c067c0, 0x17c07c1f,
	0x1b80001f, 0x20000104, 0x81459801, 0xd8005865, 0x17c07c1f, 0x18c0001f,
	0x10005464, 0x1111041f, 0xe0c00004, 0xd00058e0, 0x17c07c1f, 0x18c0001f,
	0x10005464, 0x1113841f, 0xe0c00004, 0x80388400, 0x80390400, 0x80398400,
	0x18c0001f, 0x1020920c, 0x1910001f, 0x1020920c, 0xa1000404, 0xe0c00004,
	0x1b80001f, 0x20000300, 0x803b8400, 0x803c0400, 0x803c8400, 0x81308404,
	0xe0c00004, 0x18c0001f, 0x10209200, 0x1910001f, 0x10209200, 0xa1000404,
	0xe0c00004, 0x1910001f, 0x10209200, 0x1b80001f, 0x20000300, 0x80348400,
	0x80350400, 0x80358400, 0x1b80001f, 0x20000104, 0x80308400, 0x80320400,
	0x81f38407, 0x81f98407, 0x18c0001f, 0x10209f4c, 0x1910001f, 0x10209f4c,
	0x81320404, 0xe0c00004, 0x81f90407, 0x81f40407, 0x81401801, 0xd8006205,
	0x17c07c1f, 0xe8208000, 0x10006404, 0x00002101, 0x18c0001f, 0x10006208,
	0x1212841f, 0xc0c06500, 0x12807c1f, 0xe8208000, 0x10006248, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c06500, 0x1280041f, 0x18c0001f, 0x10006200,
	0x1212841f, 0xc0c06500, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000,
	0x1b80001f, 0x20000080, 0xc0c06500, 0x1280041f, 0xe8208000, 0x10006824,
	0x000f0000, 0x81f48407, 0xa1d60407, 0x81f10407, 0xa1db0407, 0x81fd8407,
	0x81fe0407, 0x1ac0001f, 0x55aa55aa, 0xf0000000, 0xd800642a, 0x17c07c1f,
	0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xd82064ca, 0x17c07c1f, 0xe2e0002e,
	0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0xd80065ca, 0x17c07c1f,
	0xe2e00036, 0xe2e0003e, 0x1380201f, 0xe2e0003c, 0xd82066ca, 0x17c07c1f,
	0xe2e0007c, 0x1b80001f, 0x20000003, 0xe2e0005c, 0xe2e0004c, 0xe2e0004d,
	0xf0000000, 0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0x1393041f,
	0xf0000000, 0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd82067c3,
	0x17c07c1f, 0xf0000000, 0x17c07c1f, 0xe8208000, 0x11008014, 0x00000002,
	0xe8208000, 0x11008020, 0x00000101, 0xe8208000, 0x11008004, 0x000000d0,
	0x1a00001f, 0x11008000, 0xd8006a8a, 0xe220005d, 0xd8206aaa, 0xe2200040,
	0xe2200041, 0xe8208000, 0x11008024, 0x00000001, 0x1b80001f, 0x20000424,
	0xf0000000, 0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000,
	0x17c07c1f
};

static struct pcm_desc dpidle_pcm = {
	.version	= "pcm_deepidle_v8.7_20150326",
	.base		= dpidle_binary,
	.size		= 865,
	.sess		= 2,
	.replace	= 0,
	.vec0		= EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1		= EVENT_VEC(12, 1, 0, 31),	/* FUNC_26M_SLEEP */
	.vec2		= EVENT_VEC(30, 1, 0, 65),	/* FUNC_APSRC_WAKEUP */
	.vec3		= EVENT_VEC(31, 1, 0, 219),	/* FUNC_APSRC_SLEEP */
};

static struct pwr_ctrl dpidle_ctrl = {
	.wake_src		= WAKE_SRC_FOR_DPIDLE,
	.wake_src_md32		= WAKE_SRC_FOR_MD32,
	.r0_ctrl_en		= 1,
	.r7_ctrl_en		= 1,
	.infra_dcm_lock		= 1,
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
	.disp_req_mask		= 1,
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
	.disp_req_mask		= 1,
	.mfg_req_mask		= 1,
	.md32_req_mask  = 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask		= 1,
#endif
#endif
};

struct spm_lp_scen __spm_dpidle = {
	.pcmdesc	= &dpidle_pcm,
	.pwrctrl	= &dpidle_ctrl,
};


extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);
extern int request_uart_to_sleep(void);
extern int request_uart_to_wakeup(void);

#if SPM_AEE_RR_REC
extern void aee_rr_rec_deepidle_val(u32 val);
extern u32 aee_rr_curr_deepidle_val(void);
#endif

static void spm_trigger_wfi_for_dpidle(struct pwr_ctrl *pwrctrl)
{
    //sync_hw_gating_value();     /* for Vcore DVFS */

#if 0	//deepidle no need, vproc(ext buck) can't set to 0v, because SRAM perpheral control from vproc
    spm_i2c_control(mt6333_BUSNUM, 1);
#endif

    if (is_cpu_pdn(pwrctrl->pcm_flags)) {
        mt_cpu_dormant(CPU_DEEPIDLE_MODE);
    } else {
        wfi_with_sync();
    }

#if 0	//deepidle no need, vproc(ext buck) can't set to 0v, because SRAM perpheral control from vproc
    spm_i2c_control(mt6333_BUSNUM, 0);     /* restore I2C1 power */
#endif
}

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_dpidle_wakesrc(u32 wakesrc, bool enable, bool replace)
{
    unsigned long flags;

    if (spm_is_wakesrc_invalid(wakesrc))
        return -EINVAL;

    spin_lock_irqsave(&__spm_lock, flags);
    if (enable) {
        if (replace)
            __spm_dpidle.pwrctrl->wake_src = wakesrc;
        else
            __spm_dpidle.pwrctrl->wake_src |= wakesrc;
    } else {
        if (replace)
            __spm_dpidle.pwrctrl->wake_src = 0;
        else
            __spm_dpidle.pwrctrl->wake_src &= ~wakesrc;
    }
    spin_unlock_irqrestore(&__spm_lock, flags);

    return 0;
}

wake_reason_t spm_go_to_dpidle(u32 spm_flags, u32 spm_data)
{
    struct wake_status wakesta;
    unsigned long flags;
    struct mtk_irq_mask mask;
    wake_reason_t wr = WR_NONE;
    struct pcm_desc *pcmdesc = __spm_dpidle.pcmdesc;
    struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;
    struct spm_lp_scen *lpscen;
    
#if SPM_AEE_RR_REC
    aee_rr_rec_deepidle_val(1<<SPM_DEEPIDLE_ENTER);
#endif    

    lpscen = spm_check_talking_get_lpscen(&__spm_dpidle, &spm_flags);
    pcmdesc = lpscen->pcmdesc;
    pwrctrl = lpscen->pwrctrl;

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
	
    /* set PMIC WRAP table for deepidle power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);

    lockdep_off();
    spin_lock_irqsave(&__spm_lock, flags);

    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
    mt_cirq_clone_gic();
    mt_cirq_enable();
    
#if SPM_AEE_RR_REC
    aee_rr_rec_deepidle_val(aee_rr_curr_deepidle_val()|(1<<SPM_DEEPIDLE_ENTER_UART_SLEEP));
#endif     

    if (request_uart_to_sleep()) {
        wr = WR_UART_BUSY;
        goto RESTORE_IRQ;
    }
	
    __spm_reset_and_init_pcm(pcmdesc);

    __spm_kick_im_to_fetch(pcmdesc);
	
    __spm_init_pcm_register();

    __spm_init_event_vector(pcmdesc);

    __spm_set_power_control(pwrctrl);

    __spm_set_wakeup_event(pwrctrl);

    __spm_kick_pcm_to_run(pwrctrl);

    spm_dpidle_before_wfi();
    
#if SPM_AEE_RR_REC
    aee_rr_rec_deepidle_val(aee_rr_curr_deepidle_val()|(1<<SPM_DEEPIDLE_ENTER_WFI));
#endif

    spm_trigger_wfi_for_dpidle(pwrctrl);
    
#if SPM_AEE_RR_REC
    aee_rr_rec_deepidle_val(aee_rr_curr_deepidle_val()|(1<<SPM_DEEPIDLE_LEAVE_WFI));
#endif    

    spm_dpidle_after_wfi();

    __spm_get_wakeup_status(&wakesta);

    __spm_clean_after_wakeup();
    
#if SPM_AEE_RR_REC
    aee_rr_rec_deepidle_val(aee_rr_curr_deepidle_val()|(1<<SPM_DEEPIDLE_ENTER_UART_AWAKE));
#endif 
    request_uart_to_wakeup();

    wr = __spm_output_wake_reason(&wakesta, pcmdesc, false);

RESTORE_IRQ:
    mt_cirq_flush();
    mt_cirq_disable();
    mt_irq_mask_restore(&mask);

    spin_unlock_irqrestore(&__spm_lock, flags);
    lockdep_on();
    /* set PMIC WRAP table for normal power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
#if SPM_AEE_RR_REC
    aee_rr_rec_deepidle_val(0);
#endif 
    return wr;
}

/*
 * cpu_pdn:
 *    true  = CPU dormant
 *    false = CPU standby
 * pwrlevel:
 *    0 = AXI is off
 *    1 = AXI is 26M
 * pwake_time:
 *    >= 0  = specific wakeup period
 */
wake_reason_t spm_go_to_sleep_dpidle(u32 spm_flags, u32 spm_data)
{
    u32 sec = 0;
    int wd_ret;
    struct wake_status wakesta;
    unsigned long flags;
    struct mtk_irq_mask mask;
    struct wd_api *wd_api;
    static wake_reason_t last_wr = WR_NONE;
    struct pcm_desc *pcmdesc = __spm_dpidle.pcmdesc;
    struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

#if SPM_PWAKE_EN
    sec = spm_get_wake_period(-1 /* FIXME */, last_wr);
#endif
    pwrctrl->timer_val = sec * 32768;

    pwrctrl->wake_src = spm_get_sleep_wakesrc();

    wd_ret = get_wd_api(&wd_api);
    if (!wd_ret)
        wd_api->wd_suspend_notify();
    spin_lock_irqsave(&__spm_lock, flags);
    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
    mt_cirq_clone_gic();
    mt_cirq_enable();

    /* set PMIC WRAP table for deepidle power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);


    spm_crit2("sleep_deepidle, sec = %u, wakesrc = 0x%x [%u]\n",
              sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags));

    __spm_reset_and_init_pcm(pcmdesc);

    __spm_kick_im_to_fetch(pcmdesc);

    if (request_uart_to_sleep()) {
        last_wr = WR_UART_BUSY;
        goto RESTORE_IRQ;
    }

    __spm_init_pcm_register();

    __spm_init_event_vector(pcmdesc);

    __spm_set_power_control(pwrctrl);

    __spm_set_wakeup_event(pwrctrl);

    __spm_kick_pcm_to_run(pwrctrl);

    spm_trigger_wfi_for_dpidle(pwrctrl);

    __spm_get_wakeup_status(&wakesta);

    __spm_clean_after_wakeup();

	request_uart_to_wakeup();

    last_wr = __spm_output_wake_reason(&wakesta, pcmdesc, true);

RESTORE_IRQ:
	
    /* set PMIC WRAP table for normal power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);	
    mt_cirq_flush();
    mt_cirq_disable();
    mt_irq_mask_restore(&mask);
    spin_unlock_irqrestore(&__spm_lock, flags);
    if (!wd_ret)
        wd_api->wd_resume_notify();
    return last_wr;  
}


#if SPM_AEE_RR_REC
static void spm_dpidle_aee_init(void)
{
    aee_rr_rec_deepidle_val(0);
}
#endif

void spm_dpidle_init(void)
{
#if SPM_AEE_RR_REC
    spm_dpidle_aee_init();
#endif
}


MODULE_DESCRIPTION("SPM-DPIdle Driver v0.1");
