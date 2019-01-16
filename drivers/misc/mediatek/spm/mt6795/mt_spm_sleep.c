#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/aee.h>
#include <linux/i2c.h>

#include <mach/irqs.h>
#include <mach/mt_cirq.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_cpuidle.h>
#include <mach/wd_api.h>
#include <mach/eint.h>
#include <mach/mtk_ccci_helper.h>
#include <mach/mt_cpufreq.h>
#include <mach/upmu_common.h>
#include <mach/mt_dramc.h>
#include <mach/mt_boot.h>
#include <mt_i2c.h>

#include "mt_spm_internal.h"

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_PCMWDT_EN           0
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_PWAKE_EN            1
#define SPM_PCMWDT_EN           1
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define CA7_BUS_CONFIG          (CA7MCUCFG_BASE + 0x1C) //0x1020011c

// FIXME: wait for dual-vcore suspend test finish.
#if 0 
#define AP_PLL_CON7 (APMIXED_BASE+0x001C)
#endif

#define I2C_CHANNEL 1

int spm_dormant_sta = MT_CPU_DORMANT_RESET;

int spm_ap_mdsrc_req_cnt = 0;

struct wake_status suspend_info[20];
u32 log_wakesta_cnt = 0;
u32 log_wakesta_index = 0;
u8 spm_snapshot_golden_setting = 0;

/**********************************************************
 * PCM code for suspend
 **********************************************************/
static const u32 suspend_binary_ca15[] = {
	0x81f58407, 0x81f68407, 0x803a0400, 0x803a8400, 0x1b80001f, 0x20000000,
	0x80300400, 0x80318400, 0x80328400, 0xa1d28407, 0x81f20407, 0x81409801,
	0xd8000245, 0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c03260, 0x1200041f,
	0x80310400, 0x1b80001f, 0x2000000a, 0xa0110400, 0x18c0001f, 0x100062c8,
	0xe0e00010, 0xe0e00030, 0xe0e00070, 0xe0e000f0, 0x1b80001f, 0x2000001a,
	0xe0e00ff0, 0xe8208000, 0x10006354, 0xfffe7fff, 0xe8208000, 0x10006834,
	0x00000010, 0x81f00407, 0xa1dd0407, 0x81fd0407, 0xc2803840, 0x1290041f,
	0x8880000c, 0x2f7be75f, 0xd8200642, 0x17c07c1f, 0x1b00001f, 0x7fffe7ff,
	0xd0000680, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f,
	0x80880001, 0xd8000762, 0x17c07c1f, 0xd00027a0, 0x1200041f, 0xe8208000,
	0x10006834, 0x00000000, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004,
	0xd820092c, 0x17c07c1f, 0xe8208000, 0x10006834, 0x00000010, 0xd00011a0,
	0x17c07c1f, 0x18c0001f, 0x10006608, 0x1910001f, 0x10006608, 0x813b0404,
	0xe0c00004, 0x1880001f, 0x10006320, 0xc0c03740, 0xe080000f, 0xd8200b23,
	0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd00011a0, 0x17c07c1f, 0xe080001f,
	0xe8208000, 0x10006354, 0xffffffff, 0x18c0001f, 0x100062c8, 0xe0e000f0,
	0xe0e00030, 0xe0e00000, 0x81409801, 0xd8000fe5, 0x17c07c1f, 0x18c0001f,
	0x10004094, 0x1910001f, 0x1020e374, 0xe0c00004, 0x18c0001f, 0x10004098,
	0x1910001f, 0x1020e378, 0xe0c00004, 0x18c0001f, 0x10011094, 0x1910001f,
	0x10213374, 0xe0c00004, 0x18c0001f, 0x10011098, 0x1910001f, 0x10213378,
	0xe0c00004, 0x1910001f, 0x10213378, 0x18c0001f, 0x10006234, 0xc0c03420,
	0x17c07c1f, 0xc2803840, 0x1290841f, 0xa1d20407, 0x81f28407, 0xa1d68407,
	0xa0128400, 0xa0118400, 0xa0100400, 0xa01a8400, 0xa01a0400, 0x19c0001f,
	0x001c239f, 0x1b00001f, 0x3fffefff, 0xf0000000, 0x17c07c1f, 0x808d8001,
	0xd8201422, 0x17c07c1f, 0x803d8400, 0x1b80001f, 0x2000001a, 0x80340400,
	0x17c07c1f, 0x17c07c1f, 0x80310400, 0x81fa0407, 0x81f18407, 0x81f08407,
	0xa1dc0407, 0x1b80001f, 0x200000b6, 0xd00020e0, 0x17c07c1f, 0x1880001f,
	0x20000208, 0x81411801, 0xd8001605, 0x17c07c1f, 0xe8208000, 0x1000f600,
	0xd2000000, 0x1380081f, 0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e,
	0xe0e0000e, 0xe0e0000f, 0x80368400, 0x1380081f, 0x80370400, 0x1380081f,
	0x80360400, 0x803e0400, 0x1380081f, 0x80380400, 0x803b0400, 0xa01d8400,
	0x1b80001f, 0x20000034, 0x803d8400, 0x1b80001f, 0x20000152, 0x803d0400,
	0x1380081f, 0x18c0001f, 0x1000f5c8, 0x1910001f, 0x1000f5c8, 0xa1000404,
	0xe0c00004, 0x18c0001f, 0x100125c8, 0x1910001f, 0x100125c8, 0xa1000404,
	0xe0c00004, 0x1910001f, 0x100125c8, 0x80340400, 0x17c07c1f, 0x17c07c1f,
	0x80310400, 0xe8208000, 0x10000044, 0x00000100, 0x1b80001f, 0x20000068,
	0x1b80001f, 0x2000000a, 0x18c0001f, 0x10006240, 0xe0e0000d, 0xd8001e65,
	0x17c07c1f, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4, 0xa11c8404,
	0xe0c00004, 0x1b80001f, 0x2000000a, 0x813c8404, 0xe0c00004, 0x18c0001f,
	0x100110f4, 0x1910001f, 0x100110f4, 0xa11c8404, 0xe0c00004, 0x1b80001f,
	0x2000000a, 0x813c8404, 0xe0c00004, 0x1b80001f, 0x20000100, 0x81fa0407,
	0x81f18407, 0x81f08407, 0xe8208000, 0x10006354, 0xfffe7b47, 0x18c0001f,
	0x65930003, 0xc0c03080, 0x17c07c1f, 0xa1d80407, 0xa1dc0407, 0x18c0001f,
	0x10006608, 0x1910001f, 0x10006608, 0xa11b0404, 0xe0c00004, 0xc2803840,
	0x1291041f, 0x8880000c, 0x2f7be75f, 0xd8202222, 0x17c07c1f, 0x1b00001f,
	0x3fffe7ff, 0xd0002260, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xf0000000,
	0x17c07c1f, 0x1890001f, 0x10006608, 0x808b0801, 0xd8202502, 0x17c07c1f,
	0x1880001f, 0x10006320, 0xc0c034c0, 0xe080000f, 0xd8002663, 0x17c07c1f,
	0xe080001f, 0xa1da0407, 0x81fc0407, 0xa0110400, 0xa0140400, 0xa01d8400,
	0xd0002fc0, 0x17c07c1f, 0x1b80001f, 0x20000fdf, 0x1890001f, 0x10006608,
	0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002, 0x8080080d, 0xd82027a2,
	0x12007c1f, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004, 0xd800304c,
	0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0003040, 0x17c07c1f, 0x81f80407,
	0x81fc0407, 0x18c0001f, 0x65930006, 0xc0c03080, 0x17c07c1f, 0x18c0001f,
	0x65930007, 0xc0c03080, 0x17c07c1f, 0x1880001f, 0x10006320, 0xc0c034c0,
	0xe080000f, 0xd8002663, 0x17c07c1f, 0xe080001f, 0x18c0001f, 0x65930005,
	0xc0c03080, 0x17c07c1f, 0xa1da0407, 0xe8208000, 0x10000048, 0x00000100,
	0x1b80001f, 0x20000068, 0xa0110400, 0xa0140400, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0xa01d0400, 0xa01b0400, 0xa0180400, 0x803d8400, 0xa01e0400, 0xa0160400,
	0xa0170400, 0xa0168400, 0x1b80001f, 0x20000104, 0x81411801, 0xd8002f85,
	0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c03420, 0x17c07c1f, 0xe8208000,
	0x1000f600, 0xd2000001, 0xd8000768, 0x17c07c1f, 0xc2803840, 0x1291841f,
	0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830,
	0xe1000003, 0x18c0001f, 0x10006834, 0xe0e00000, 0xe0e00001, 0x18d0001f,
	0x10006830, 0x68e00003, 0x0000beef, 0xd8203163, 0x17c07c1f, 0xf0000000,
	0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f, 0xe0f07f0e,
	0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d, 0xe0f07c0d,
	0xe0f0780d, 0xf0000000, 0xe0f0700d, 0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e,
	0xf0000000, 0xe0f07f12, 0x11407c1f, 0x81f08407, 0x81f18407, 0x1b80001f,
	0x20000001, 0xa1d08407, 0xa1d18407, 0x1392841f, 0x812ab401, 0x80ebb401,
	0xa0c00c04, 0xd82036c3, 0x17c07c1f, 0x80c01403, 0xd82034e3, 0x01400405,
	0x1900001f, 0x10006814, 0xf0000000, 0xe1000003, 0xa1d00407, 0x1b80001f,
	0x20000208, 0x80ea3401, 0x1a00001f, 0x10006814, 0xf0000000, 0xe2000003,
	0x18c0001f, 0x10006b6c, 0x1910001f, 0x10006b6c, 0xa1002804, 0xf0000000,
	0xe0c00004, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0x1a50001f, 0x10006610, 0x8246a401, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2f7be75f, 0x1b80001f, 0xd00f0000, 0x8880000c,
	0x2f7be75f, 0xd8006322, 0x17c07c1f, 0xe8208000, 0x10006354, 0xfffe7b47,
	0xc0c06ee0, 0x81401801, 0xd8004885, 0x17c07c1f, 0x81f60407, 0x18c0001f,
	0x100062a0, 0xc0c063e0, 0x12807c1f, 0x18c0001f, 0x100062b4, 0x1910001f,
	0x100062b4, 0xa9000004, 0x00000001, 0xe0c00004, 0xa9000004, 0x00000011,
	0xe0c00004, 0x18c0001f, 0x100062a0, 0xc0c063e0, 0x1280041f, 0x18c0001f,
	0x100062b0, 0xc0c063e0, 0x12807c1f, 0xe8208000, 0x100062b8, 0x00000011,
	0x1b80001f, 0x20000080, 0xe8208000, 0x100062b8, 0x00000015, 0xc0c063e0,
	0x1280041f, 0x18c0001f, 0x10006290, 0xc0c063e0, 0x1280041f, 0xe8208000,
	0x10006404, 0x00003101, 0xc2803840, 0x1292041f, 0xc0c06a60, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0f07fff, 0xe0e00fff, 0xe0e000ff, 0x81449801,
	0xd8004c05, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200001, 0x81459801,
	0xd8004b45, 0x17c07c1f, 0x18c0001f, 0x10005468, 0x1111041f, 0xe0c00004,
	0xd0004bc0, 0x17c07c1f, 0x18c0001f, 0x10005468, 0x1113841f, 0xe0c00004,
	0xc0c06b20, 0x17c07c1f, 0xa1d38407, 0xa1d98407, 0x1800001f, 0x00000012,
	0x1800001f, 0x00000e12, 0x1800001f, 0x03800e12, 0x1800001f, 0x038e0e12,
	0x18c0001f, 0x10209200, 0x1910001f, 0x10209200, 0x81200404, 0xe0c00004,
	0x18c0001f, 0x1020920c, 0x1910001f, 0x1020920c, 0xa1108404, 0xe0c00004,
	0x81200404, 0xe0c00004, 0xe8208000, 0x10006310, 0x0b1600f8, 0x1b00001f,
	0xbfffe7ff, 0x1b80001f, 0x90100000, 0x80c00400, 0xd82050e3, 0xa1d58407,
	0xa1dd8407, 0x1b00001f, 0x3fffefff, 0xd0004fa0, 0x17c07c1f, 0x1890001f,
	0x100063e8, 0x88c0000c, 0x2f7be75f, 0xd8005303, 0x17c07c1f, 0x80c40001,
	0xd8005283, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd00052c0, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xd0004fa0, 0x17c07c1f, 0x80c40001, 0xd8205403,
	0x17c07c1f, 0xa1de0407, 0x1b00001f, 0x7fffe7ff, 0xd0004fa0, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc, 0xe0e007f8, 0xe0e00ff0,
	0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00, 0x81449801, 0xd8005825,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200000, 0x81459801, 0xd8005725,
	0x17c07c1f, 0x18c0001f, 0x10005464, 0x1111041f, 0xe0c00004, 0xd00057a0,
	0x17c07c1f, 0x18c0001f, 0x10005464, 0x1113841f, 0xe0c00004, 0xc0c06b20,
	0x17c07c1f, 0x1b80001f, 0x200016a8, 0x1800001f, 0x03800e12, 0x18c0001f,
	0x1020920c, 0x1910001f, 0x1020920c, 0xa1000404, 0xe0c00004, 0x1b80001f,
	0x20000300, 0x1800001f, 0x00000e12, 0x81308404, 0xe0c00004, 0x18c0001f,
	0x10209200, 0x1910001f, 0x10209200, 0xa1000404, 0xe0c00004, 0x1b80001f,
	0x20000300, 0x1800001f, 0x00000012, 0x1b80001f, 0x20000104, 0x10007c1f,
	0x81f38407, 0x81f98407, 0x81f90407, 0x81f40407, 0x1b80001f, 0x200016a8,
	0x81401801, 0xd8006325, 0x17c07c1f, 0xe8208000, 0x10006404, 0x00001101,
	0x18c0001f, 0x10006290, 0x1212841f, 0xc0c06560, 0x12807c1f, 0xc0c06560,
	0x1280041f, 0x18c0001f, 0x100062b0, 0x1212841f, 0xc0c06560, 0x12807c1f,
	0xe8208000, 0x100062b8, 0x00000011, 0xe8208000, 0x100062b8, 0x00000010,
	0x1b80001f, 0x20000080, 0xc0c06560, 0x1280041f, 0xe8208000, 0x10200268,
	0x000ffffe, 0xe8208000, 0x10200208, 0x00000000, 0x18c0001f, 0x100062a0,
	0x1212841f, 0xc0c06560, 0x12807c1f, 0x18c0001f, 0x100062b4, 0x1910001f,
	0x100062b4, 0x89000004, 0xffffffef, 0xe0c00004, 0x89000004, 0xffffffee,
	0xe0c00004, 0x1b80001f, 0x20000a50, 0x18c0001f, 0x100062a0, 0xc0c06560,
	0x1280041f, 0x19c0001f, 0x01411820, 0x1ac0001f, 0x55aa55aa, 0x10007c1f,
	0xf0000000, 0xd800646a, 0x17c07c1f, 0xe2e0006d, 0xe2e0002d, 0xd820652a,
	0x17c07c1f, 0xe2e0003d, 0xe2e0003f, 0xe2e0003e, 0xe2e00032, 0xf0000000,
	0x17c07c1f, 0xd800668a, 0x17c07c1f, 0xe2e00036, 0x1380201f, 0xe2e0003e,
	0x1380201f, 0xe2e0002e, 0x1380201f, 0xe2e0002c, 0xd820676a, 0x17c07c1f,
	0xe2e0006c, 0xe2e0004c, 0x1b80001f, 0x20000020, 0xe2e0004d, 0xf0000000,
	0x17c07c1f, 0xd82068a9, 0x17c07c1f, 0xe2e0000d, 0xe2e0000c, 0xe2e0001c,
	0xe2e0001e, 0xe2e00016, 0xe2e00012, 0xf0000000, 0x17c07c1f, 0xd8206a29,
	0x17c07c1f, 0xe2e00016, 0x1380201f, 0xe2e0001e, 0x1380201f, 0xe2e0001c,
	0x1380201f, 0xe2e0000c, 0xe2e0000d, 0xf0000000, 0x17c07c1f, 0xa1d40407,
	0x1391841f, 0xa1d90407, 0x1393041f, 0xf0000000, 0x17c07c1f, 0x18d0001f,
	0x10006604, 0x10cf8c1f, 0xd8206b23, 0x17c07c1f, 0xf0000000, 0x17c07c1f,
	0xe8208000, 0x11008014, 0x00000002, 0xe8208000, 0x11008020, 0x00000101,
	0xe8208000, 0x11008004, 0x000000d0, 0x1a00001f, 0x11008000, 0xd8006dea,
	0xe220005d, 0xd8206e0a, 0xe2200000, 0xe2200001, 0xe8208000, 0x11008024,
	0x00000001, 0x1b80001f, 0x20000424, 0xf0000000, 0x17c07c1f, 0xa1d10407,
	0x1b80001f, 0x20000020, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc suspend_pcm_ca15 = {
	.version	= "pcm_suspend_v32.19_20140930_CA15",
	.base		= suspend_binary_ca15,
	.size		= 892,
	.sess		= 2,
	.replace	= 0,
	.vec0		= EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1		= EVENT_VEC(12, 1, 0, 54),	/* FUNC_26M_SLEEP */
	.vec2		= EVENT_VEC(30, 1, 0, 143),	/* FUNC_APSRC_WAKEUP */
	.vec3		= EVENT_VEC(31, 1, 0, 277),	/* FUNC_APSRC_SLEEP */
};

static const u32 suspend_binary_ca7[] = {
	0x81f58407, 0x81f68407, 0x803a0400, 0x803a8400, 0x1b80001f, 0x20000000,
	0x80300400, 0x80318400, 0x80328400, 0xa1d28407, 0x81f20407, 0x81409801,
	0xd8000245, 0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c03260, 0x1200041f,
	0x80310400, 0x1b80001f, 0x2000000a, 0xa0110400, 0x18c0001f, 0x100062c8,
	0xe0e00010, 0xe0e00030, 0xe0e00070, 0xe0e000f0, 0x1b80001f, 0x2000001a,
	0xe0e00ff0, 0xe8208000, 0x10006354, 0xfffe7fff, 0xe8208000, 0x10006834,
	0x00000010, 0x81f00407, 0xa1dd0407, 0x81fd0407, 0xc2803840, 0x1290041f,
	0x8880000c, 0x2f7be75f, 0xd8200642, 0x17c07c1f, 0x1b00001f, 0x7fffe7ff,
	0xd0000680, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f,
	0x80880001, 0xd8000762, 0x17c07c1f, 0xd00027a0, 0x1200041f, 0xe8208000,
	0x10006834, 0x00000000, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004,
	0xd820092c, 0x17c07c1f, 0xe8208000, 0x10006834, 0x00000010, 0xd00011a0,
	0x17c07c1f, 0x18c0001f, 0x10006608, 0x1910001f, 0x10006608, 0x813b0404,
	0xe0c00004, 0x1880001f, 0x10006320, 0xc0c03740, 0xe080000f, 0xd8200b23,
	0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd00011a0, 0x17c07c1f, 0xe080001f,
	0xe8208000, 0x10006354, 0xffffffff, 0x18c0001f, 0x100062c8, 0xe0e000f0,
	0xe0e00030, 0xe0e00000, 0x81409801, 0xd8000fe5, 0x17c07c1f, 0x18c0001f,
	0x10004094, 0x1910001f, 0x1020e374, 0xe0c00004, 0x18c0001f, 0x10004098,
	0x1910001f, 0x1020e378, 0xe0c00004, 0x18c0001f, 0x10011094, 0x1910001f,
	0x10213374, 0xe0c00004, 0x18c0001f, 0x10011098, 0x1910001f, 0x10213378,
	0xe0c00004, 0x1910001f, 0x10213378, 0x18c0001f, 0x10006234, 0xc0c03420,
	0x17c07c1f, 0xc2803840, 0x1290841f, 0xa1d20407, 0x81f28407, 0xa1d68407,
	0xa0128400, 0xa0118400, 0xa0100400, 0xa01a8400, 0xa01a0400, 0x19c0001f,
	0x001c239f, 0x1b00001f, 0x3fffefff, 0xf0000000, 0x17c07c1f, 0x808d8001,
	0xd8201422, 0x17c07c1f, 0x803d8400, 0x1b80001f, 0x2000001a, 0x80340400,
	0x17c07c1f, 0x17c07c1f, 0x80310400, 0x81fa0407, 0x81f18407, 0x81f08407,
	0xa1dc0407, 0x1b80001f, 0x200000b6, 0xd00020e0, 0x17c07c1f, 0x1880001f,
	0x20000208, 0x81411801, 0xd8001605, 0x17c07c1f, 0xe8208000, 0x1000f600,
	0xd2000000, 0x1380081f, 0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e,
	0xe0e0000e, 0xe0e0000f, 0x80368400, 0x1380081f, 0x80370400, 0x1380081f,
	0x80360400, 0x803e0400, 0x1380081f, 0x80380400, 0x803b0400, 0xa01d8400,
	0x1b80001f, 0x20000034, 0x803d8400, 0x1b80001f, 0x20000152, 0x803d0400,
	0x1380081f, 0x18c0001f, 0x1000f5c8, 0x1910001f, 0x1000f5c8, 0xa1000404,
	0xe0c00004, 0x18c0001f, 0x100125c8, 0x1910001f, 0x100125c8, 0xa1000404,
	0xe0c00004, 0x1910001f, 0x100125c8, 0x80340400, 0x17c07c1f, 0x17c07c1f,
	0x80310400, 0xe8208000, 0x10000044, 0x00000100, 0x1b80001f, 0x20000068,
	0x1b80001f, 0x2000000a, 0x18c0001f, 0x10006240, 0xe0e0000d, 0xd8001e65,
	0x17c07c1f, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4, 0xa11c8404,
	0xe0c00004, 0x1b80001f, 0x2000000a, 0x813c8404, 0xe0c00004, 0x18c0001f,
	0x100110f4, 0x1910001f, 0x100110f4, 0xa11c8404, 0xe0c00004, 0x1b80001f,
	0x2000000a, 0x813c8404, 0xe0c00004, 0x1b80001f, 0x20000100, 0x81fa0407,
	0x81f18407, 0x81f08407, 0xe8208000, 0x10006354, 0xfffe7b47, 0x18c0001f,
	0x65930003, 0xc0c03080, 0x17c07c1f, 0xa1d80407, 0xa1dc0407, 0x18c0001f,
	0x10006608, 0x1910001f, 0x10006608, 0xa11b0404, 0xe0c00004, 0xc2803840,
	0x1291041f, 0x8880000c, 0x2f7be75f, 0xd8202222, 0x17c07c1f, 0x1b00001f,
	0x3fffe7ff, 0xd0002260, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xf0000000,
	0x17c07c1f, 0x1890001f, 0x10006608, 0x808b0801, 0xd8202502, 0x17c07c1f,
	0x1880001f, 0x10006320, 0xc0c034c0, 0xe080000f, 0xd8002663, 0x17c07c1f,
	0xe080001f, 0xa1da0407, 0x81fc0407, 0xa0110400, 0xa0140400, 0xa01d8400,
	0xd0002fc0, 0x17c07c1f, 0x1b80001f, 0x20000fdf, 0x1890001f, 0x10006608,
	0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002, 0x8080080d, 0xd82027a2,
	0x12007c1f, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004, 0xd800304c,
	0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0003040, 0x17c07c1f, 0x81f80407,
	0x81fc0407, 0x18c0001f, 0x65930006, 0xc0c03080, 0x17c07c1f, 0x18c0001f,
	0x65930007, 0xc0c03080, 0x17c07c1f, 0x1880001f, 0x10006320, 0xc0c034c0,
	0xe080000f, 0xd8002663, 0x17c07c1f, 0xe080001f, 0x18c0001f, 0x65930005,
	0xc0c03080, 0x17c07c1f, 0xa1da0407, 0xe8208000, 0x10000048, 0x00000100,
	0x1b80001f, 0x20000068, 0xa0110400, 0xa0140400, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0xa01d0400, 0xa01b0400, 0xa0180400, 0x803d8400, 0xa01e0400, 0xa0160400,
	0xa0170400, 0xa0168400, 0x1b80001f, 0x20000104, 0x81411801, 0xd8002f85,
	0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c03420, 0x17c07c1f, 0xe8208000,
	0x1000f600, 0xd2000001, 0xd8000768, 0x17c07c1f, 0xc2803840, 0x1291841f,
	0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830,
	0xe1000003, 0x18c0001f, 0x10006834, 0xe0e00000, 0xe0e00001, 0x18d0001f,
	0x10006830, 0x68e00003, 0x0000beef, 0xd8203163, 0x17c07c1f, 0xf0000000,
	0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f, 0xe0f07f0e,
	0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d, 0xe0f07c0d,
	0xe0f0780d, 0xf0000000, 0xe0f0700d, 0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e,
	0xf0000000, 0xe0f07f12, 0x11407c1f, 0x81f08407, 0x81f18407, 0x1b80001f,
	0x20000001, 0xa1d08407, 0xa1d18407, 0x1392841f, 0x812ab401, 0x80ebb401,
	0xa0c00c04, 0xd82036c3, 0x17c07c1f, 0x80c01403, 0xd82034e3, 0x01400405,
	0x1900001f, 0x10006814, 0xf0000000, 0xe1000003, 0xa1d00407, 0x1b80001f,
	0x20000208, 0x80ea3401, 0x1a00001f, 0x10006814, 0xf0000000, 0xe2000003,
	0x18c0001f, 0x10006b6c, 0x1910001f, 0x10006b6c, 0xa1002804, 0xf0000000,
	0xe0c00004, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0x1a50001f, 0x10006610, 0x8246a401, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2f7be75f, 0x1b80001f, 0xd00f0000, 0x8880000c,
	0x2f7be75f, 0xd8005b22, 0x17c07c1f, 0xe8208000, 0x10006354, 0xfffe7b47,
	0xc0c066e0, 0x81401801, 0xd80047c5, 0x17c07c1f, 0x81f60407, 0x18c0001f,
	0x10006200, 0xc0c05be0, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c05be0, 0x1280041f, 0x18c0001f, 0x10006204,
	0xc0c05fa0, 0x1280041f, 0x18c0001f, 0x10006208, 0xc0c05be0, 0x12807c1f,
	0xe8208000, 0x10006244, 0x00000001, 0x1b80001f, 0x20000080, 0xc0c05be0,
	0x1280041f, 0x18c0001f, 0x10006290, 0xc0c05be0, 0x1280041f, 0xe8208000,
	0x10006404, 0x00003101, 0xc2803840, 0x1292041f, 0xc0c06260, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0f07fff, 0xe0e00fff, 0xe0e000ff, 0x81449801,
	0xd8004a05, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200003, 0xc0c06320,
	0x17c07c1f, 0xe2200005, 0xc0c06320, 0x17c07c1f, 0xa1d38407, 0xa1d98407,
	0x1800001f, 0x00000012, 0x1800001f, 0x00000e12, 0x1800001f, 0x03800e12,
	0x1800001f, 0x038e0e12, 0xe8208000, 0x10006310, 0x0b1600f8, 0x1b00001f,
	0xbfffe7ff, 0x1b80001f, 0x90100000, 0x80c00400, 0xd8204d23, 0xa1d58407,
	0xa1dd8407, 0x1b00001f, 0x3fffefff, 0xd0004be0, 0x17c07c1f, 0x1890001f,
	0x100063e8, 0x88c0000c, 0x2f7be75f, 0xd8004f43, 0x17c07c1f, 0x80c40001,
	0xd8004ec3, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0004f00, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xd0004be0, 0x17c07c1f, 0x80c40001, 0xd8205043,
	0x17c07c1f, 0xa1de0407, 0x1b00001f, 0x7fffe7ff, 0xd0004be0, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc, 0xe0e007f8, 0xe0e00ff0,
	0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00, 0x81449801, 0xd8005325,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200002, 0xc0c06320, 0x17c07c1f,
	0xe2200004, 0xc0c06320, 0x17c07c1f, 0x1b80001f, 0x200016a8, 0x1800001f,
	0x03800e12, 0x1b80001f, 0x20000300, 0x1800001f, 0x00000e12, 0x1b80001f,
	0x20000300, 0x1800001f, 0x00000012, 0x1b80001f, 0x20000104, 0x10007c1f,
	0x81f38407, 0x81f98407, 0x81f90407, 0x81f40407, 0x1b80001f, 0x200016a8,
	0x81401801, 0xd8005b25, 0x17c07c1f, 0xe8208000, 0x10006404, 0x00002101,
	0x18c0001f, 0x10006290, 0x1212841f, 0xc0c05d60, 0x12807c1f, 0xc0c05d60,
	0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f, 0xc0c05d60, 0x12807c1f,
	0xe8208000, 0x10006244, 0x00000000, 0x1b80001f, 0x20000080, 0xc0c05d60,
	0x1280041f, 0xe8208000, 0x10200268, 0x000ffffe, 0x18c0001f, 0x10006204,
	0x1212841f, 0xc0c060e0, 0x1280041f, 0x18c0001f, 0x10006200, 0x1212841f,
	0xc0c05d60, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c05d60, 0x1280041f, 0x19c0001f, 0x01411820, 0x1ac0001f,
	0x55aa55aa, 0x10007c1f, 0xf0000000, 0xd8005c6a, 0x17c07c1f, 0xe2e0006d,
	0xe2e0002d, 0xd8205d2a, 0x17c07c1f, 0xe2e0003d, 0xe2e0003f, 0xe2e0003e,
	0xe2e00032, 0xf0000000, 0x17c07c1f, 0xd8005e8a, 0x17c07c1f, 0xe2e00036,
	0x1380201f, 0xe2e0003e, 0x1380201f, 0xe2e0002e, 0x1380201f, 0xe2e0002c,
	0xd8205f6a, 0x17c07c1f, 0xe2e0006c, 0xe2e0004c, 0x1b80001f, 0x20000020,
	0xe2e0004d, 0xf0000000, 0x17c07c1f, 0xd82060a9, 0x17c07c1f, 0xe2e0000d,
	0xe2e0000c, 0xe2e0001c, 0xe2e0001e, 0xe2e00016, 0xe2e00012, 0xf0000000,
	0x17c07c1f, 0xd8206229, 0x17c07c1f, 0xe2e00016, 0x1380201f, 0xe2e0001e,
	0x1380201f, 0xe2e0001c, 0x1380201f, 0xe2e0000c, 0xe2e0000d, 0xf0000000,
	0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0x1393041f, 0xf0000000,
	0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd8206323, 0x17c07c1f,
	0xf0000000, 0x17c07c1f, 0xe8208000, 0x11008014, 0x00000002, 0xe8208000,
	0x11008020, 0x00000101, 0xe8208000, 0x11008004, 0x000000d0, 0x1a00001f,
	0x11008000, 0xd80065ea, 0xe220005d, 0xd820660a, 0xe2200000, 0xe2200001,
	0xe8208000, 0x11008024, 0x00000001, 0x1b80001f, 0x20000424, 0xf0000000,
	0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc suspend_pcm_ca7 = {
	.version	= "pcm_suspend_v32.19_20140930_CA7",
	.base		= suspend_binary_ca7,
	.size		= 828,
	.sess		= 2,
	.replace	= 0,
	.vec0		= EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1		= EVENT_VEC(12, 1, 0, 54),	/* FUNC_26M_SLEEP */
	.vec2		= EVENT_VEC(30, 1, 0, 143),	/* FUNC_APSRC_WAKEUP */
	.vec3		= EVENT_VEC(31, 1, 0, 277),	/* FUNC_APSRC_SLEEP */
};

/**************************************
 * SW code for suspend
 **************************************/
#define SPM_SYSCLK_SETTLE       99      /* 3ms */

#define WAIT_UART_ACK_TIMES     10      /* 10 * 10us */

#define SPM_WAKE_PERIOD         600     /* sec */

#define WAKE_SRC_FOR_SUSPEND                                          \
    (WAKE_SRC_KP | WAKE_SRC_EINT | WAKE_SRC_CCIF_MD | WAKE_SRC_MD32 | \
     WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_THERM |            \
     WAKE_SRC_SYSPWREQ | WAKE_SRC_MD_WDT | WAKE_SRC_CLDMA_MD |        \
     WAKE_SRC_SEJ | WAKE_SRC_ALL_MD32)

#define WAKE_SRC_FOR_MD32  0                                          \
    //(WAKE_SRC_AUD_MD32)

#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

extern int get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level);

extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);

extern int request_uart_to_sleep(void);
extern int request_uart_to_wakeup(void);
extern void mtk_uart_restore(void);
extern void dump_uart_reg(void);

static struct pwr_ctrl suspend_ctrl = {
    .wake_src           = WAKE_SRC_FOR_SUSPEND,
    .wake_src_md32      = WAKE_SRC_FOR_MD32,
    .r0_ctrl_en         = 1,
    .r7_ctrl_en         = 1,
    .infra_dcm_lock     = 1,
    .wfi_op             = WFI_OP_AND,

    .ca7top_idle_mask   = 0,
    .ca15top_idle_mask  = 0,
    .mcusys_idle_mask   = 0,
    .disp_req_mask      = 0,
    .mfg_req_mask       = 0,
    .md1_req_mask       = 0,
    .md2_req_mask       = 0,
    .md32_req_mask      = 0,
    .md_apsrc_sel       = SEL_MD_DDR_EN,

    .dsi0_ddr_en_mask   = 1,
    .dsi1_ddr_en_mask   = 1,
    .dpi_ddr_en_mask    = 1,
    .isp0_ddr_en_mask   = 1,
    .isp1_ddr_en_mask   = 1,
      
    .ca7_wfi0_en        = 1,
    .ca7_wfi1_en        = 1,
    .ca7_wfi2_en        = 1,
    .ca7_wfi3_en        = 1,    
    .ca15_wfi0_en       = 1,
    .ca15_wfi1_en       = 1,
    .ca15_wfi2_en       = 1,
    .ca15_wfi3_en       = 1,

#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask      = 1,
#endif
};

struct spm_lp_scen __spm_suspend = {
	.pcmdesc	= &suspend_pcm_ca15,
	.pwrctrl	= &suspend_ctrl,
    .wakestatus = &suspend_info[0],
};

static void spm_set_suspend_pcm_ver(void)
{
    if (CHIP_SW_VER_02 <= mt_get_chip_sw_ver())
    {
    	/*E2 Suspend FW*/
        __spm_suspend.pcmdesc = &suspend_pcm_ca7;
    }
    else
    {
#if SPM_CTRL_BIG_CPU
        __spm_suspend.pcmdesc = &suspend_pcm_ca15;
#else
        __spm_suspend.pcmdesc = &suspend_pcm_ca7;
#endif
    }	
}

static void spm_i2c_control(u32 channel, bool onoff)
{
    static int pdn = 0;
    static bool i2c_onoff = 0;
#ifdef CONFIG_OF
    void __iomem *base;
#else
    u32 base;
#endif
    u32 i2c_clk;

    switch(channel)
    {
        case 0:
#ifdef CONFIG_OF
            base = SPM_I2C0_BASE;
#else
            base = I2C0_BASE;
#endif
            i2c_clk = MT_CG_PERI_I2C0;
            break;
        case 1:
#ifdef CONFIG_OF
            base = SPM_I2C1_BASE;
#else
            base = I2C1_BASE;
#endif
            i2c_clk = MT_CG_PERI_I2C1;
            break;
        case 2:
#ifdef CONFIG_OF
            base = SPM_I2C2_BASE;
#else
            base = I2C2_BASE;
#endif
            i2c_clk = MT_CG_PERI_I2C2;
            break;
        case 3:
#ifdef CONFIG_OF
            base = SPM_I2C3_BASE;
#else
            base = I2C3_BASE;
#endif
            i2c_clk = MT_CG_PERI_I2C3;
	          break;
//FIXME: I2C4 is defined in 6595 dts but not in 6795 dts. 
#if 0 
        case 4:
            base = I2C4_BASE;
            i2c_clk = MT_CG_PERI_I2C4;
	          break;
#endif
        default:
            break;
    }

    if ((1 == onoff) && (0 == i2c_onoff))
    {
       i2c_onoff = 1;
#if 1
        pdn = spm_read(PERI_PDN0_STA) & (1U << i2c_clk);
        spm_write(PERI_PDN0_CLR, pdn);                /* power on I2C */
#else
        pdn = clock_is_on(i2c_clk);
        if (!pdn)
            enable_clock(i2c_clk, "spm_i2c");
#endif
        spm_write(base + OFFSET_CONTROL,     0x0);    /* init I2C_CONTROL */
        spm_write(base + OFFSET_TRANSAC_LEN, 0x1);    /* init I2C_TRANSAC_LEN */
        spm_write(base + OFFSET_EXT_CONF,    0x1800); /* init I2C_EXT_CONF */
        spm_write(base + OFFSET_IO_CONFIG,   0x3);    /* init I2C_IO_CONFIG */
        spm_write(base + OFFSET_HS,          0x102);  /* init I2C_HS */
    }
    else
    if ((0 == onoff) && (1 == i2c_onoff))
    {
        i2c_onoff = 0;
#if 1
        spm_write(PERI_PDN0_SET, pdn);                /* restore I2C power */
#else
        if (!pdn)
            disable_clock(i2c_clk, "spm_i2c");      
#endif
    }
    else
        ASSERT(1);
}

static void spm_suspend_pre_process(struct pwr_ctrl *pwrctrl)
{
    /* set PMIC WRAP table for suspend power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_SUSPEND);
    
    // FIXME: wait for dual-vcore suspend test finish.
    #if 1 
    if (is_dualvcore_pdn(pwrctrl->pcm_flags))
    {
        /* set LTE pd mode to avoid LTE power on after dual-vcore resume */
        //spm_write(AP_PLL_CON7, spm_read(AP_PLL_CON7) | 0xF);  // set before dual-vcore suspend
        mt_cpufreq_apply_pmic_cmd(IDX_SP_VCORE_PDN_EN_HW_MODE); // if dual-vcore suspend enable, set VCORE_PND_EN to HW mode
        //mt6331_upmu_set_rg_int_en_chrdet(0); // disable charger detection to avoid abnormal EINT.
        //mt6331_upmu_set_rg_int_en_rtc(0); // mask rtc to avoid abnormal EINT.
    }
    else
    #endif
        mt_cpufreq_apply_pmic_cmd(IDX_SP_VCORE_PDN_EN_SW_MODE); // if dual-vcore suspend disable, set VCORE_PND_EN to SW mode

    spm_i2c_control(I2C_CHANNEL, 1);
}

static void spm_suspend_post_process(struct pwr_ctrl *pwrctrl)
{
    // FIXME: wait for dual-vcore suspend test finish.
    #if 1 
    if (is_dualvcore_pdn(pwrctrl->pcm_flags))
    {
        /* restore LTE pd mode after dual-vcore resume */
        //spm_write(AP_PLL_CON7, spm_read(AP_PLL_CON7) & ~0xF); // set after dual-vcore resume

        /* enable charger detection */
        //mt6331_upmu_set_rg_int_en_chrdet(1);
        /* enable rtc */
        //mt6331_upmu_set_rg_int_en_rtc(1);

        /* set VCORE_PND_EN to SW mode */
        mt_cpufreq_apply_pmic_cmd(IDX_SP_VCORE_PDN_EN_SW_MODE);
    }
    #endif

    /* set PMIC WRAP table for normal power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);

    spm_i2c_control(I2C_CHANNEL, 0);
}

static void spm_set_sysclk_settle(void)
{
    u32 md_settle, settle;

    /* get MD SYSCLK settle */
    spm_write(SPM_CLK_CON, spm_read(SPM_CLK_CON) | CC_SYSSETTLE_SEL);
    spm_write(SPM_CLK_SETTLE, 0);
    md_settle = spm_read(SPM_CLK_SETTLE);

    /* SYSCLK settle = MD SYSCLK settle but set it again for MD PDN */
    spm_write(SPM_CLK_SETTLE, SPM_SYSCLK_SETTLE - md_settle);
    settle = spm_read(SPM_CLK_SETTLE);

    spm_crit2("md_settle = %u, settle = %u\n", md_settle, settle);
}

static void spm_kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
    /* enable PCM WDT (normal mode) to start count if needed */
#if SPM_PCMWDT_EN
    {
        u32 con1;
        con1 = spm_read(SPM_PCM_CON1) & ~(CON1_PCM_WDT_WAKE_MODE | CON1_PCM_WDT_EN);
// PCM WDT WAKE MODE for lastPC
//	con1 = spm_read(SPM_PCM_CON1) & ~( CON1_PCM_WDT_EN) | CON1_PCM_WDT_WAKE_MODE;
        spm_write(SPM_PCM_CON1, CON1_CFG_KEY | con1);

        if (spm_read(SPM_PCM_TIMER_VAL) > PCM_TIMER_MAX)
            spm_write(SPM_PCM_TIMER_VAL, PCM_TIMER_MAX);
        spm_write(SPM_PCM_WDT_TIMER_VAL, spm_read(SPM_PCM_TIMER_VAL) + PCM_WDT_TIMEOUT);
        spm_write(SPM_PCM_CON1, con1 | CON1_CFG_KEY | CON1_PCM_WDT_EN);
    }
#endif

    /* init PCM_PASR_DPD_0 for DPD */
    spm_write(SPM_PCM_PASR_DPD_0, 0);

    /* make MD32 work in suspend: fscp_ck = CLK26M */
    clkmux_sel(MT_MUX_SCP, 0, "SPM-Sleep");

    __spm_kick_pcm_to_run(pwrctrl);
}

static void spm_trigger_wfi_for_sleep(struct pwr_ctrl *pwrctrl)
{
//    sync_hw_gating_value();     /* for Vcore DVFS */

    if (is_cpu_pdn(pwrctrl->pcm_flags)) {
        spm_dormant_sta = mt_cpu_dormant(CPU_SHUTDOWN_MODE/* | DORMANT_SKIP_WFI*/);
        switch (spm_dormant_sta)
        {
            case MT_CPU_DORMANT_RESET:
                break;
            case MT_CPU_DORMANT_ABORT:
                break;
            case MT_CPU_DORMANT_BREAK:
                break;
            case MT_CPU_DORMANT_BYPASS:
                break;
        }
    } else {
        spm_dormant_sta = -1;
        spm_write(CA7_BUS_CONFIG, spm_read(CA7_BUS_CONFIG) | 0x10);
        wfi_with_sync();
        spm_write(CA7_BUS_CONFIG, spm_read(CA7_BUS_CONFIG) & ~0x10);
    }

    if (is_infra_pdn(pwrctrl->pcm_flags))
        mtk_uart_restore();
}

static void spm_clean_after_wakeup(void)
{
    /* disable PCM WDT to stop count if needed */
#if SPM_PCMWDT_EN
    spm_write(SPM_PCM_CON1, CON1_CFG_KEY | (spm_read(SPM_PCM_CON1) & ~CON1_PCM_WDT_EN));
#endif

    __spm_clean_after_wakeup();

    /* restore clock mux: fscp_ck = SYSPLL1_D2 */
    clkmux_sel(MT_MUX_SCP, 1, "SPM-Sleep");
}

static wake_reason_t spm_output_wake_reason(struct wake_status *wakesta, struct pcm_desc *pcmdesc)
{
	wake_reason_t wr;

	wr = __spm_output_wake_reason(wakesta, pcmdesc, true);

#if 1
    memcpy(&suspend_info[log_wakesta_cnt], wakesta, sizeof(struct wake_status));
    suspend_info[log_wakesta_cnt].log_index = log_wakesta_index;

    log_wakesta_cnt++;
    log_wakesta_index++;

    if (10 <= log_wakesta_cnt)
    {
        log_wakesta_cnt = 0;
        spm_snapshot_golden_setting = 0;
    }
#if 0
    else
    {
        if (2 != spm_snapshot_golden_setting)
        {
            if ((0x90100000 == wakesta->event_reg) && (0x140001f == wakesta->debug_flag))
                spm_snapshot_golden_setting = 1;
        }
    }
#endif
    
    
    if (0xFFFFFFF0 <= log_wakesta_index)
        log_wakesta_index = 0;
#endif

    spm_crit2("big core = %d, suspend dormant state = %d, chip = %d\n", SPM_CTRL_BIG_CPU, spm_dormant_sta, mt_get_chip_sw_ver());
    if (0 != spm_ap_mdsrc_req_cnt)
        spm_crit2("warning: spm_ap_mdsrc_req_cnt = %d, r7[ap_mdsrc_req] = 0x%x\n", spm_ap_mdsrc_req_cnt, spm_read(SPM_POWER_ON_VAL1) & (1<<17));

    if (wakesta->r12 & WAKE_SRC_EINT)
        mt_eint_print_status();

    if (wakesta->r12 & WAKE_SRC_CLDMA_MD)
        exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);

	return wr;
}

#if SPM_PWAKE_EN
static u32 spm_get_wake_period(int pwake_time, wake_reason_t last_wr)
{
    int period = SPM_WAKE_PERIOD;

    if (pwake_time < 0) {
        /* use FG to get the period of 1% battery decrease */
        period = get_dynamic_period(last_wr != WR_PCM_TIMER ? 1 : 0, SPM_WAKE_PERIOD, 1);
        if (period <= 0) {
            spm_warn("CANNOT GET PERIOD FROM FUEL GAUGE\n");
            period = SPM_WAKE_PERIOD;
        }
    } else {
        period = pwake_time;
        spm_crit2("pwake = %d\n", pwake_time);
    }

    if (period > 36 * 3600)     /* max period is 36.4 hours */
        period = 36 * 3600;

    return period;
}
#endif

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace)
{
    unsigned long flags;

    if (spm_is_wakesrc_invalid(wakesrc))
        return -EINVAL;

    spin_lock_irqsave(&__spm_lock, flags);
    if (enable) {
        if (replace)
            __spm_suspend.pwrctrl->wake_src = wakesrc;
        else
            __spm_suspend.pwrctrl->wake_src |= wakesrc;
    } else {
        if (replace)
            __spm_suspend.pwrctrl->wake_src = 0;
        else
            __spm_suspend.pwrctrl->wake_src &= ~wakesrc;
    }
    spin_unlock_irqrestore(&__spm_lock, flags);

    return 0;
}

/*
 * wakesrc: WAKE_SRC_XXX
 */
u32 spm_get_sleep_wakesrc(void)
{
    return __spm_suspend.pwrctrl->wake_src;
}

extern int snapshot_golden_setting(const char *func, const unsigned int line);
wake_reason_t spm_go_to_sleep(u32 spm_flags, u32 spm_data)
{
    u32 sec = 0;
    int wd_ret;
    struct wake_status wakesta;
    unsigned long flags;
    struct mtk_irq_mask mask;
    struct wd_api *wd_api;
    static wake_reason_t last_wr = WR_NONE;
    struct pcm_desc *pcmdesc = __spm_suspend.pcmdesc;
    struct pwr_ctrl *pwrctrl = __spm_suspend.pwrctrl;
    struct spm_lp_scen *lpscen;

    lpscen = spm_check_talking_get_lpscen(&__spm_suspend, &spm_flags);
    pcmdesc = lpscen->pcmdesc;
    pwrctrl = lpscen->pwrctrl;

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
    set_pwrctrl_pcm_data(pwrctrl, spm_data);

#if SPM_PWAKE_EN
    sec = spm_get_wake_period(-1 /* FIXME */, last_wr);
#endif
    pwrctrl->timer_val = sec * 32768;

    wd_ret = get_wd_api(&wd_api);
    if (!wd_ret)
        wd_api->wd_suspend_notify();

    spm_suspend_pre_process(pwrctrl);

    spin_lock_irqsave(&__spm_lock, flags);
    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(MT_SPM_IRQ_ID);
    mt_cirq_clone_gic();
    mt_cirq_enable();

    spm_set_sysclk_settle();

    spm_crit2("sec = %u, wakesrc = 0x%x (%u)(%u)\n",
              sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags), is_infra_pdn(pwrctrl->pcm_flags));

    if (request_uart_to_sleep()) {
        last_wr = WR_UART_BUSY;
        goto RESTORE_IRQ;
    }

    __spm_reset_and_init_pcm(pcmdesc);

    __spm_kick_im_to_fetch(pcmdesc);

    __spm_init_pcm_register();

    __spm_init_event_vector(pcmdesc);

    __spm_set_power_control(pwrctrl);

    __spm_set_wakeup_event(pwrctrl);

    spm_kick_pcm_to_run(pwrctrl);

#if 0
    if (1 == spm_snapshot_golden_setting)
    {
        snapshot_golden_setting(__FUNCTION__, __LINE__);
        spm_snapshot_golden_setting = 2;
    }
#endif

    spm_trigger_wfi_for_sleep(pwrctrl);

    __spm_get_wakeup_status(&wakesta);

    spm_clean_after_wakeup();

    request_uart_to_wakeup();

    last_wr = spm_output_wake_reason(&wakesta, pcmdesc);

RESTORE_IRQ:
    mt_cirq_flush();
    mt_cirq_disable();
    mt_irq_mask_restore(&mask);
    spin_unlock_irqrestore(&__spm_lock, flags);

    spm_suspend_post_process(pwrctrl);

    if (!wd_ret)
        wd_api->wd_resume_notify();

    return last_wr;
}

bool spm_is_md_sleep(void)
{
    return !( (spm_read(SPM_PCM_REG13_DATA) & R13_MD1_SRCLKENA) | (spm_read(SPM_PCM_REG13_DATA) & R13_MD2_SRCLKENA));
}

#if 0 // No connsys
bool spm_is_conn_sleep(void)
{
    /* need to check */
}
#endif

void spm_set_wakeup_src_check(void)
{
    /* clean wakeup event raw status */
    spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, 0xFFFFFFFF);

    /* set wakeup event */
    spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~WAKE_SRC_FOR_SUSPEND);
}

bool spm_check_wakeup_src(void)
{
    u32 wakeup_src;

    /* check wanek event raw status */
    wakeup_src = spm_read(SPM_SLEEP_ISR_RAW_STA);
    
    if (wakeup_src)
    {
        spm_crit2("WARNING: spm_check_wakeup_src = 0x%x", wakeup_src);
        return 1;
    }
    else
        return 0;
}

void spm_poweron_config_set(void)
{
    unsigned long flags;

    spin_lock_irqsave(&__spm_lock, flags);
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_md32_sram_con(u32 value)
{
    unsigned long flags;

    spin_lock_irqsave(&__spm_lock, flags);
    /* enable register control */
    spm_write(SPM_MD32_SRAM_CON, value);
    spin_unlock_irqrestore(&__spm_lock, flags);
}

#define hw_spin_lock_for_ddrdfs()           \
do {                                        \
    spm_write(0xF0050090, 0x8000);          \
} while (!(spm_read(0xF0050090) & 0x8000))

#define hw_spin_unlock_for_ddrdfs()         \
    spm_write(0xF0050090, 0x8000)

void spm_ap_mdsrc_req(u8 set)
{
    unsigned long flags;
    u32 i = 0;
    u32 md_sleep = 0;

    if (set)
    {   
        spin_lock_irqsave(&__spm_lock, flags);

        if (spm_ap_mdsrc_req_cnt < 0)
        {
            spm_crit2("warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n", set, spm_ap_mdsrc_req_cnt); 
            //goto AP_MDSRC_REC_CNT_ERR;
            spin_unlock_irqrestore(&__spm_lock, flags);
        }
        else
        {
            spm_ap_mdsrc_req_cnt++;

            hw_spin_lock_for_ddrdfs();
            spm_write(SPM_POWER_ON_VAL1, spm_read(SPM_POWER_ON_VAL1) | (1 << 17));
            hw_spin_unlock_for_ddrdfs();

            spin_unlock_irqrestore(&__spm_lock, flags);
    
            /* if md_apsrc_req = 1'b0, wait 26M settling time (3ms) */
            if (0 == (spm_read(SPM_PCM_REG13_DATA) & R13_MD1_APSRC_REQ))
            {
                md_sleep = 1;
                mdelay(3);
            }

            /* Check ap_mdsrc_ack = 1'b1 */
            while(0 == (spm_read(SPM_PCM_REG13_DATA) & R13_AP_MD1SRC_ACK))
            {
                if (10 > i++)
                {
                    mdelay(1);
                }
                else
                {
                    spm_crit2("WARNING: MD SLEEP = %d, spm_ap_mdsrc_req CAN NOT polling AP_MD1SRC_ACK\n", md_sleep);
                    //goto AP_MDSRC_REC_CNT_ERR;
                    break;
                }
            }
        }        
    }
    else
    {
        spin_lock_irqsave(&__spm_lock, flags);

        spm_ap_mdsrc_req_cnt--;

        if (spm_ap_mdsrc_req_cnt < 0)
        {
            spm_crit2("warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n", set, spm_ap_mdsrc_req_cnt); 
            //goto AP_MDSRC_REC_CNT_ERR;
        }
        else
        {
            if (0 == spm_ap_mdsrc_req_cnt)
            {
                hw_spin_lock_for_ddrdfs();
                spm_write(SPM_POWER_ON_VAL1, spm_read(SPM_POWER_ON_VAL1) & ~(1 << 17));
                hw_spin_unlock_for_ddrdfs();
            }
        }
        
        spin_unlock_irqrestore(&__spm_lock, flags);
    }

//AP_MDSRC_REC_CNT_ERR:
//    spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_force_lte_onoff(u8 onoff)
{
    if (onoff)
        spm_write(AP_PLL_CON7, spm_read(AP_PLL_CON7) & ~0xF);
    else
        spm_write(AP_PLL_CON7, spm_read(AP_PLL_CON7) | 0xF);
}

void spm_output_sleep_option(void)
{
    spm_notice("PWAKE_EN:%d, PCMWDT_EN:%d, BYPASS_SYSPWREQ:%d, I2C_CHANNEL:%d\n",
               SPM_PWAKE_EN, SPM_PCMWDT_EN, SPM_BYPASS_SYSPWREQ, I2C_CHANNEL);
}

void spm_suspend_init(void)
{
    spm_set_suspend_pcm_ver();
    // disable lastpc
    //spm_write(SPM_SLEEP_MD32_WAKEUP_EVENT_MASK, 0xffffdff7);
}


/**************************************
 * LAST PC  API
 **************************************/
#define last_pc_size  48
#define last_pcm_sram_addr 1023
#define PCM_IM_HOST_EN		(1U << 31)

u32 pcm_im_read_data(u32 im_addr)
{
	spm_write(SPM_PCM_IM_HOST_RW_PTR, (im_addr | PCM_IM_HOST_EN));
	return spm_read(SPM_PCM_IM_HOST_RW_DAT);
}

u32 read_pwr_statu(void)
{
	return (spm_read(SPM_PWR_STATUS) & spm_read(SPM_PWR_STATUS_2ND)) ;
}

void read_pcm_data(int *sram_data, int length)

{
	int data_addr;
	if (length <= last_pc_size)	{
		for (data_addr =0 ; data_addr < length; data_addr++)
			sram_data[data_addr] = pcm_im_read_data(last_pcm_sram_addr -data_addr);
	}		
	else	{	
		spm_crit2("over size: %d\n", last_pc_size);
	}
}
MODULE_DESCRIPTION("SPM-Sleep Driver v0.1");
