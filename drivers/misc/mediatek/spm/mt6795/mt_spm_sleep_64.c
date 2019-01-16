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
#include <linux/of_address.h>
#include "mt_spm_internal.h"
#include <mach/md32_helper.h>

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

#ifdef CONFIG_OF
#define SPM_MCUCFG_BASE		spm_mcucfg
#define SPM_MD32_BASE_ADDR	spm_md32_base
#define SPM_EINT_BASE	spm_eint_base
#else
#define SPM_MCUCFG_BASE		(0xF0200000)      //0x1020_0000
#define SPM_MD32_BASE_ADDR	(0xF0050000)
#endif
#define MP0_AXI_CONFIG          (SPM_MCUCFG_BASE + 0x2C) 
#define MP1_AXI_CONFIG          (SPM_MCUCFG_BASE + 0x22C) 
#define I2C_CHANNEL 1
#define MD32_SEMAPHORE_BASE	(SPM_MD32_BASE_ADDR + 0x90)

int spm_dormant_sta = MT_CPU_DORMANT_RESET;

int spm_ap_mdsrc_req_cnt = 0;

struct wake_status suspend_info[20];
u32 log_wakesta_cnt = 0;
u32 log_wakesta_index = 0;
u8 spm_snapshot_golden_setting = 0;
bool wake_eint_status[EINT_AP_MAXNUMBER];
bool eint_wake =0;

/**********************************************************
 * PCM code for suspend
 **********************************************************/
static const u32 suspend_binary[] = {
	0xa1d58407, 0x81f68407, 0x803a0400, 0x803a8400, 0x1b80001f, 0x20000000,
	0x80300400, 0x80318400, 0x80328400, 0xa1d28407, 0x81f20407, 0x81409801,
	0xd8000245, 0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c03540, 0x1200041f,
	0x80310400, 0x1b80001f, 0x2000000a, 0xa0110400, 0x18c0001f, 0x100062c8,
	0xe0e00010, 0xe0e00030, 0xe0e00070, 0xe0e000f0, 0x1b80001f, 0x2000001a,
	0xe0e00ff0, 0xe8208000, 0x10006354, 0xfffe7fbf, 0xe8208000, 0x10006834,
	0x00000010, 0x81f00407, 0xa1dd0407, 0x81fd0407, 0xc2803b20, 0x1290041f,
	0x8880000c, 0x2f7be75f, 0xd8200642, 0x17c07c1f, 0x1b00001f, 0x7fffe7ff,
	0xd0000680, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f,
	0x80880001, 0xd8000762, 0x17c07c1f, 0xd0002a80, 0x1200041f, 0xe8208000,
	0x10006834, 0x00000000, 0x18c0001f, 0x10006608, 0x1910001f, 0x10006608,
	0x813b0404, 0xe0c00004, 0x1880001f, 0x10006320, 0xc0c03a20, 0xe080000f,
	0xd82009c3, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd0001040, 0x17c07c1f,
	0xe080001f, 0xe8208000, 0x10006354, 0xffffffbf, 0x18c0001f, 0x100062c8,
	0xe0e000f0, 0xe0e00030, 0xe0e00000, 0x81409801, 0xd8000e85, 0x17c07c1f,
	0x18c0001f, 0x10004094, 0x1910001f, 0x1020e374, 0xe0c00004, 0x18c0001f,
	0x10004098, 0x1910001f, 0x1020e378, 0xe0c00004, 0x18c0001f, 0x10011094,
	0x1910001f, 0x10213374, 0xe0c00004, 0x18c0001f, 0x10011098, 0x1910001f,
	0x10213378, 0xe0c00004, 0x1910001f, 0x10213378, 0x18c0001f, 0x10006234,
	0xc0c03700, 0x17c07c1f, 0xc2803b20, 0x1290841f, 0xa1d20407, 0x81f28407,
	0xa1d68407, 0xa0128400, 0xa0118400, 0xa0100400, 0xa01a8400, 0xa01a0400,
	0x81f70407, 0x81f58407, 0x1b00001f, 0x3fffefff, 0xf0000000, 0x17c07c1f,
	0x808f9801, 0xd8201402, 0x81481801, 0x80c89801, 0xd8201243, 0x17c07c1f,
	0xd80011a5, 0x17c07c1f, 0x803d8400, 0x18c0001f, 0x10006204, 0xe0e00000,
	0x1b80001f, 0x2000002f, 0x80340400, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x80310400, 0x81fa0407, 0x81f18407, 0x81f08407, 0xa1dc0407,
	0x1b80001f, 0x200000b6, 0xd0002280, 0x17c07c1f, 0x1880001f, 0x20000208,
	0x81411801, 0xd80015e5, 0x17c07c1f, 0xe8208000, 0x1000f600, 0xd2000000,
	0x1380081f, 0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e, 0xe0e0000e,
	0xe0e0000f, 0x81fe8407, 0x80368400, 0x1380081f, 0x80370400, 0x1380081f,
	0x80360400, 0x803e0400, 0x1b80001f, 0x20000034, 0x80380400, 0x803b0400,
	0x803d0400, 0x18c0001f, 0x10006204, 0xe0e00007, 0xa01d8400, 0x1b80001f,
	0x20000034, 0xe0e00000, 0x803d8400, 0x1b80001f, 0x20000152, 0x18c0001f,
	0x1000f5c8, 0x1910001f, 0x1000f5c8, 0xa1000404, 0xe0c00004, 0x18c0001f,
	0x100125c8, 0x1910001f, 0x100125c8, 0xa1000404, 0xe0c00004, 0x1910001f,
	0x100125c8, 0x81481801, 0xd8201b45, 0x17c07c1f, 0xa01d8400, 0xa1de8407,
	0xd0001b60, 0x17c07c1f, 0xa01d0400, 0x80340400, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x80310400, 0xe8208000, 0x10000044, 0x00000100,
	0x1b80001f, 0x20000073, 0x18c0001f, 0x10006240, 0xe0e0000d, 0x81411801,
	0xd8002005, 0x17c07c1f, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4,
	0xa11c8404, 0xe0c00004, 0x1b80001f, 0x2000000a, 0x813c8404, 0xe0c00004,
	0x18c0001f, 0x100110f4, 0x1910001f, 0x100110f4, 0xa11c8404, 0xe0c00004,
	0x1b80001f, 0x2000000a, 0x813c8404, 0xe0c00004, 0x1b80001f, 0x20000100,
	0x81fa0407, 0x81f18407, 0x81f08407, 0xe8208000, 0x10006354, 0xfffe7b07,
	0x18c0001f, 0x65930003, 0xc0c03360, 0x17c07c1f, 0xa1d80407, 0xa1dc0407,
	0x18c0001f, 0x10006608, 0x1910001f, 0x10006608, 0xa11b0404, 0xe0c00004,
	0xc2803b20, 0x1291041f, 0x8880000c, 0x2f7be75f, 0xd82023c2, 0x17c07c1f,
	0x1b00001f, 0x3fffe7ff, 0xd0002400, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff,
	0xf0000000, 0x17c07c1f, 0x1890001f, 0x10006608, 0x808b0801, 0xd8202762,
	0x17c07c1f, 0x1880001f, 0x10006320, 0xc0c037a0, 0xe080000f, 0xd80028c3,
	0x17c07c1f, 0xe080001f, 0xa1da0407, 0x81fc0407, 0xa0110400, 0xa0140400,
	0x80c89801, 0xd8202723, 0x17c07c1f, 0xa01d8400, 0x18c0001f, 0x10006204,
	0xe0e00001, 0xd00032a0, 0xa19f8406, 0x1b80001f, 0x20000fdf, 0x1890001f,
	0x10006608, 0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002, 0x8080080d,
	0xd8202a82, 0x12007c1f, 0x81f08407, 0x81f18407, 0xa1d80407, 0xa1dc0407,
	0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004, 0xd800332c, 0x17c07c1f,
	0x1b00001f, 0xbfffe7ff, 0xd0003320, 0x17c07c1f, 0x81f80407, 0x81fc0407,
	0x18c0001f, 0x65930006, 0xc0c03360, 0x17c07c1f, 0x18c0001f, 0x65930007,
	0xc0c03360, 0x17c07c1f, 0x1880001f, 0x10006320, 0xc0c037a0, 0xe080000f,
	0xd80028c3, 0x17c07c1f, 0xe080001f, 0x18c0001f, 0x65930005, 0xc0c03360,
	0x17c07c1f, 0xa1da0407, 0xe8208000, 0x10000048, 0x00000100, 0x1b80001f,
	0x20000068, 0xa0110400, 0xa0140400, 0x18c0001f, 0x1000f5c8, 0x1910001f,
	0x1000f5c8, 0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8, 0x1910001f,
	0x100125c8, 0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8, 0xa01d0400,
	0xa01b0400, 0xa0180400, 0x803d8400, 0xa01e0400, 0xa0160400, 0xa0170400,
	0xa0168400, 0x1b80001f, 0x20000104, 0x81411801, 0xd8003265, 0x17c07c1f,
	0x18c0001f, 0x10006240, 0xc0c03700, 0x17c07c1f, 0xe8208000, 0x1000f600,
	0xd2000001, 0xd8000768, 0x81bf8406, 0xc2803b20, 0x1291841f, 0x1b00001f,
	0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830, 0xe1000003,
	0x18c0001f, 0x10006834, 0xe0e00000, 0xe0e00001, 0x18d0001f, 0x10006830,
	0x68e00003, 0x0000beef, 0xd8203443, 0x17c07c1f, 0xf0000000, 0x17c07c1f,
	0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f, 0xe0f07f0e, 0x1b80001f,
	0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d, 0xe0f07c0d, 0xe0f0780d,
	0xf0000000, 0xe0f0700d, 0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e, 0xf0000000,
	0xe0f07f12, 0x11407c1f, 0x81f08407, 0x81f18407, 0x1b80001f, 0x20000001,
	0xa1d08407, 0xa1d18407, 0x1392841f, 0x812ab401, 0x80ebb401, 0xa0c00c04,
	0xd82039a3, 0x17c07c1f, 0x80c01403, 0xd82037c3, 0x01400405, 0x1900001f,
	0x10006814, 0xf0000000, 0xe1000003, 0xa1d00407, 0x1b80001f, 0x20000208,
	0x80ea3401, 0x1a00001f, 0x10006814, 0xf0000000, 0xe2000003, 0x18c0001f,
	0x10006b6c, 0x1910001f, 0x10006b6c, 0xa1002804, 0xf0000000, 0xe0c00004,
	0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd8203c03, 0x17c07c1f, 0xf0000000,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0xe8208000, 0x10006b6c, 0x00000000, 0x1b00001f, 0x2f7be75f,
	0x1b80001f, 0x500f0000, 0xe8208000, 0x10006354, 0xfffe7b07, 0xc0c06920,
	0x81401801, 0xd8004665, 0x17c07c1f, 0x81f60407, 0x18c0001f, 0x10006200,
	0xc0c06120, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001, 0x1b80001f,
	0x20000080, 0xc0c06120, 0x1280041f, 0x18c0001f, 0x10006208, 0xc0c06120,
	0x12807c1f, 0xe8208000, 0x10006244, 0x00000001, 0x1b80001f, 0x20000080,
	0xc0c06120, 0x1280041f, 0x18c0001f, 0x10006290, 0xc0c06120, 0x1280041f,
	0xe8208000, 0x10006404, 0x00003101, 0xc2803b20, 0x1292041f, 0x1b00001f,
	0x2f7be75f, 0x1b80001f, 0x30000004, 0x8880000c, 0x2f7be75f, 0xd8005b02,
	0x17c07c1f, 0xc0c064a0, 0x17c07c1f, 0x18c0001f, 0x10006294, 0xe0f07fff,
	0xe0e00fff, 0xe0e000ff, 0x81449801, 0xd8004985, 0x17c07c1f, 0x1a00001f,
	0x10006604, 0xe2200005, 0xc0c06640, 0x12807c1f, 0xc0c06560, 0x17c07c1f,
	0x18c0001f, 0x10209f4c, 0x1910001f, 0x10209f4c, 0xa1120404, 0xe0c00004,
	0xa1d38407, 0xa1d98407, 0xa0108400, 0xa0120400, 0xa0148400, 0xa0150400,
	0xa0158400, 0xa01b8400, 0xa01c0400, 0xa01c8400, 0xa0188400, 0xa0190400,
	0xa0198400, 0x18c0001f, 0x10209200, 0x1910001f, 0x10209200, 0x81200404,
	0xe0c00004, 0x18c0001f, 0x1020920c, 0x1910001f, 0x1020920c, 0xa1108404,
	0xe0c00004, 0x81200404, 0xe0c00004, 0xe8208000, 0x10006310, 0x0b1600f8,
	0x1b00001f, 0xbfffe7ff, 0x1b80001f, 0x90100000, 0x80c00400, 0xd8204f83,
	0xa1d58407, 0xa1dd8407, 0x1b00001f, 0x3fffefff, 0xd0004e40, 0x17c07c1f,
	0x1890001f, 0x100063e8, 0x88c0000c, 0x2f7be75f, 0xd80051a3, 0x17c07c1f,
	0x80c40001, 0xd8005123, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0005160,
	0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd0004e40, 0x17c07c1f, 0x80c40001,
	0xd82052a3, 0x17c07c1f, 0xa1de0407, 0x1b00001f, 0x7fffe7ff, 0xd0004e40,
	0x17c07c1f, 0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc, 0xe0e007f8,
	0xe0e00ff0, 0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00, 0x81449801,
	0xd8005565, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200004, 0xc0c06640,
	0x1280041f, 0xc0c06560, 0x17c07c1f, 0x1b80001f, 0x200016a8, 0x80388400,
	0x80390400, 0x80398400, 0x18c0001f, 0x1020920c, 0x1910001f, 0x1020920c,
	0xa1000404, 0xe0c00004, 0x1b80001f, 0x20000300, 0x803b8400, 0x803c0400,
	0x803c8400, 0x81308404, 0xe0c00004, 0x18c0001f, 0x10209200, 0x1910001f,
	0x10209200, 0xa1000404, 0xe0c00004, 0x1910001f, 0x10209200, 0x1b80001f,
	0x20000300, 0x80348400, 0x80350400, 0x80358400, 0x1b80001f, 0x20000104,
	0x80308400, 0x80320400, 0x81f38407, 0x81f98407, 0x18c0001f, 0x10209f4c,
	0x1910001f, 0x10209f4c, 0x81320404, 0xe0c00004, 0x81f90407, 0x81f40407,
	0x1b80001f, 0x200016a8, 0x81401801, 0xd8005fa5, 0x17c07c1f, 0xe8208000,
	0x10006404, 0x00002101, 0x18c0001f, 0x10006290, 0x1212841f, 0xc0c062a0,
	0x12807c1f, 0xc0c062a0, 0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f,
	0xc0c062a0, 0x12807c1f, 0xe8208000, 0x10006244, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c062a0, 0x1280041f, 0x18c0001f, 0x10006200, 0x1212841f,
	0xc0c062a0, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c062a0, 0x1280041f, 0xe8208000, 0x10006824, 0x000f0000,
	0x81f48407, 0xa1d60407, 0x81f10407, 0xa1db0407, 0x81fd8407, 0x81fe0407,
	0x1ac0001f, 0x55aa55aa, 0xf0000000, 0xd80061ca, 0x17c07c1f, 0xe2e0004f,
	0xe2e0006f, 0xe2e0002f, 0xd820626a, 0x17c07c1f, 0xe2e0002e, 0xe2e0003e,
	0xe2e00032, 0xf0000000, 0x17c07c1f, 0xd800636a, 0x17c07c1f, 0xe2e00036,
	0xe2e0003e, 0x1380201f, 0xe2e0003c, 0xd820646a, 0x17c07c1f, 0xe2e0007c,
	0x1b80001f, 0x20000003, 0xe2e0005c, 0xe2e0004c, 0xe2e0004d, 0xf0000000,
	0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0x1393041f, 0xf0000000,
	0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd8206563, 0x17c07c1f,
	0xf0000000, 0x17c07c1f, 0xe8208000, 0x11008014, 0x00000002, 0xe8208000,
	0x11008020, 0x00000101, 0xe8208000, 0x11008004, 0x000000d0, 0x1a00001f,
	0x11008000, 0xd800682a, 0xe220005d, 0xd820684a, 0xe2200040, 0xe2200041,
	0xe8208000, 0x11008024, 0x00000001, 0x1b80001f, 0x20000424, 0xf0000000,
	0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000, 0x17c07c1f
};

static struct pcm_desc suspend_pcm = {
	.version	= "pcm_suspend_v10.10_20150326",
	.base		= suspend_binary,
	.size		= 846,
	.sess		= 2,
	.replace	= 0,
	.vec0		= EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1		= EVENT_VEC(12, 1, 0, 54),	/* FUNC_26M_SLEEP */
	.vec2		= EVENT_VEC(30, 1, 0, 132),	/* FUNC_APSRC_WAKEUP */
	.vec3		= EVENT_VEC(31, 1, 0, 290),	/* FUNC_APSRC_SLEEP */
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

#define WAKE_SRC_FOR_MD32  0x00002008                                          \
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
	.pcmdesc	= &suspend_pcm,
	.pwrctrl	= &suspend_ctrl,
    .wakestatus = &suspend_info[0],
};


static void spm_set_suspend_pcm_ver(void)
{
	__spm_suspend.pcmdesc = &suspend_pcm;
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

    spm_i2c_control(I2C_CHANNEL, 1);
}

static void spm_suspend_post_process(struct pwr_ctrl *pwrctrl)
{
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
// PCM WDT WAKE MODE for lastPC
//        con1 = spm_read(SPM_PCM_CON1) & ~(CON1_PCM_WDT_WAKE_MODE | CON1_PCM_WDT_EN);
	con1 = (spm_read(SPM_PCM_CON1) & ~(CON1_PCM_WDT_EN)) | (CON1_PCM_WDT_WAKE_MODE);
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
        spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) | 0x10);
	spm_write(MP1_AXI_CONFIG, spm_read(MP1_AXI_CONFIG) | 0x10);
        wfi_with_sync();
        spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) & ~0x10);
        spm_write(MP1_AXI_CONFIG, spm_read(MP1_AXI_CONFIG) & ~0x10);
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

static void spm_get_wake_eint_status(void)
{
	unsigned int status, index;
	unsigned int offset, reg_base;

	for (reg_base = 0; reg_base < EINT_AP_MAXNUMBER; reg_base += 32) {
		status = spm_read((reg_base / 32) * 4 + SPM_EINT_BASE);
		for (offset = 0; offset < 32; offset++) {
			index = reg_base + offset;
			if (index >= EINT_AP_MAXNUMBER)
				break;
			wake_eint_status[index] = (status >> offset) & 0x1;
			if (wake_eint_status[index])
				spm_crit2("wake up by EINT:%d \n", index);
		}
	}
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

    spm_crit2("big core = %d, suspend dormant state = %d\n", SPM_CTRL_BIG_CPU, spm_dormant_sta);
    if (0 != spm_ap_mdsrc_req_cnt)
        spm_crit2("warning: spm_ap_mdsrc_req_cnt = %d, r7[ap_mdsrc_req] = 0x%x\n", spm_ap_mdsrc_req_cnt, spm_read(SPM_POWER_ON_VAL1) & (1<<17));

    if (wakesta->r12 & WAKE_SRC_EINT)
	{
		eint_wake = 1;
		mt_eint_print_status();
		spm_get_wake_eint_status();
	}
	else
		eint_wake = 0;
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
    if (wd_api->wd_spmwdt_mode_config)
        wd_api->wd_spmwdt_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);
    if (!wd_ret)
        wd_api->wd_suspend_notify();
    spm_suspend_pre_process(pwrctrl);

    spin_lock_irqsave(&__spm_lock, flags);
    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(spm_irq_0);
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
    if (wd_api->wd_spmwdt_mode_config)
        wd_api->wd_spmwdt_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);

    return last_wr;
}

bool spm_is_md_sleep(void)
{
    return !((spm_read(SPM_PCM_REG13_DATA) & R13_MD1_SRCLKENA) | (spm_read(SPM_PCM_REG13_DATA) & R13_MD2_SRCLKENA) | (spm_read(SPM_PCM_REG13_DATA) & R13_MD1_APSRC_REQ));
}

bool spm_is_md1_sleep(void)
{
    return !((spm_read(SPM_PCM_REG13_DATA) & R13_MD1_SRCLKENA) | (spm_read(SPM_PCM_REG13_DATA) & R13_MD2_SRCLKENA) | (spm_read(SPM_PCM_REG13_DATA) & R13_MD1_APSRC_REQ));
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
    spm_write(MD32_SEMAPHORE_BASE, 0x8000);      \
} while (!(spm_read(MD32_SEMAPHORE_BASE) & 0x8000))

#define hw_spin_unlock_for_ddrdfs()         \
    spm_write(MD32_SEMAPHORE_BASE, 0x8000)


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
    spm_write(SPM_SLEEP_MD32_WAKEUP_EVENT_MASK, 0xffffdff7);
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
		spm_write(SPM_PCM_IM_HOST_RW_PTR, 0);
	}		
	else	{	
		spm_crit2("over size: %d\n", last_pc_size);
	}
}

/* eint wake up status API */
bool spm_read_eint_status (unsigned int eint_num)
{
	if(eint_wake)
		return wake_eint_status[eint_num];
	return 0;
}

MODULE_DESCRIPTION("SPM-Sleep Driver v0.1");
