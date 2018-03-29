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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>
#include <linux/lockdep.h>
/* TODO: wait irq/cirq driver ready */
/* #include <irq.h> */
#include <mt-plat/mt_cirq.h>

#include <mach/upmu_sw.h>
#include <mach/wd_api.h>
/* #include <mach/eint.h> */

#include "mt_spm_internal.h"
#include "mt_spm_sleep.h"
#include "mt_cpuidle.h"

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

#if defined(CONFIG_AMAZON_METRICS_LOG)
/* forced trigger system_resume:off_mode metrics log */
int force_gpt = 0;
module_param(force_gpt, int, 0644);
#endif

static struct mt_wake_event spm_wake_event = {
	.domain = "SPM",
};

static struct mt_wake_event *mt_wake_event;
static struct mt_wake_event_map *mt_wake_event_tbl;

#define MAX_REGISTER_WAKE_EVENT 8
static struct mt_wake_event_map *runtime_wake_event_tbl[MAX_REGISTER_WAKE_EVENT];
static int we_reg_count;

#undef MCUCFG_BASE
#ifdef CONFIG_OF
#define MCUCFG_BASE          spm_mcucfg
#else
#define MCUCFG_BASE          (0xF0200000)
#endif
#define MP0_AXI_CONFIG          (MCUCFG_BASE + 0x2C)
#define MP1_AXI_CONFIG          (MCUCFG_BASE + 0x22C)
#define ACINACTM                (1<<4)

static int spm_dormant_sta = MT_CPU_DORMANT_RESET;
/**********************************************************
 * PCM sequence for cpu suspend
 **********************************************************/
static const u32 suspend_binary_ca7[] = {
	0x81f58407, 0x81f68407, 0x803a0400, 0x803a8400, 0x1b80001f, 0x20000000,
	0x80300400, 0x80318400, 0x80328400, 0xa1d28407, 0x81f20407, 0x81409801,
	0xd8000245, 0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c031a0, 0x1200041f,
	0x80310400, 0x1b80001f, 0x2000000a, 0xa0110400, 0x18c0001f, 0x100062c8,
	0xe0e00010, 0xe0e00030, 0xe0e00070, 0xe0e000f0, 0x1b80001f, 0x2000001a,
	0xe0e00ff0, 0xe8208000, 0x10006354, 0xfffe7fff, 0xe8208000, 0x10006834,
	0x00000010, 0x81f00407, 0xa1dd0407, 0x81fd0407, 0xc2803780, 0x1290041f,
	0x8880000c, 0x2f7be75f, 0xd8200642, 0x17c07c1f, 0x1b00001f, 0x7fffe7ff,
	0xd0000680, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f,
	0x80880001, 0xd8000762, 0x17c07c1f, 0xd00027a0, 0x1200041f, 0xe8208000,
	0x10006834, 0x00000000, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004,
	0xd820092c, 0x17c07c1f, 0xe8208000, 0x10006834, 0x00000010, 0xd00011a0,
	0x17c07c1f, 0x18c0001f, 0x10006608, 0x1910001f, 0x10006608, 0x813b0404,
	0xe0c00004, 0x1880001f, 0x10006320, 0xc0c03680, 0xe080000f, 0xd8200b23,
	0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd00011a0, 0x17c07c1f, 0xe080001f,
	0xe8208000, 0x10006354, 0xffffffff, 0x18c0001f, 0x100062c8, 0xe0e000f0,
	0xe0e00030, 0xe0e00000, 0x81409801, 0xd8000fe5, 0x17c07c1f, 0x18c0001f,
	0x10004094, 0x1910001f, 0x1020e374, 0xe0c00004, 0x18c0001f, 0x10004098,
	0x1910001f, 0x1020e378, 0xe0c00004, 0x18c0001f, 0x10011094, 0x1910001f,
	0x10213374, 0xe0c00004, 0x18c0001f, 0x10011098, 0x1910001f, 0x10213378,
	0xe0c00004, 0x1910001f, 0x10213378, 0x18c0001f, 0x10006234, 0xc0c03360,
	0x17c07c1f, 0xc2803780, 0x1290841f, 0xa1d20407, 0x81f28407, 0xa1d68407,
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
	0x10006608, 0x1910001f, 0x10006608, 0xa11b0404, 0xe0c00004, 0xc2803780,
	0x1291041f, 0x8880000c, 0x2f7be75f, 0xd8202222, 0x17c07c1f, 0x1b00001f,
	0x3fffe7ff, 0xd0002260, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xf0000000,
	0x17c07c1f, 0x1890001f, 0x10006608, 0x808b0801, 0xd8202502, 0x17c07c1f,
	0x1880001f, 0x10006320, 0xc0c03400, 0xe080000f, 0xd8002663, 0x17c07c1f,
	0xe080001f, 0xa1da0407, 0x81fc0407, 0xa0110400, 0xa0140400, 0xa01d8400,
	0xd0002fc0, 0x17c07c1f, 0x1b80001f, 0x20000fdf, 0x1890001f, 0x10006608,
	0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002, 0x8080080d, 0xd82027a2,
	0x12007c1f, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004, 0xd800304c,
	0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0003040, 0x17c07c1f, 0x81f80407,
	0x81fc0407, 0x18c0001f, 0x65930006, 0xc0c03080, 0x17c07c1f, 0x18c0001f,
	0x65930007, 0xc0c03080, 0x17c07c1f, 0x1880001f, 0x10006320, 0xc0c03400,
	0xe080000f, 0xd8002663, 0x17c07c1f, 0xe080001f, 0x18c0001f, 0x65930005,
	0xc0c03080, 0x17c07c1f, 0xa1da0407, 0xe8208000, 0x10000048, 0x00000100,
	0x1b80001f, 0x20000068, 0xa0110400, 0xa0140400, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0xa01d0400, 0xa01b0400, 0xa0180400, 0x803d8400, 0xa01e0400, 0xa0160400,
	0xa0170400, 0xa0168400, 0x1b80001f, 0x20000104, 0x81411801, 0xd8002f85,
	0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c03360, 0x17c07c1f, 0xe8208000,
	0x1000f600, 0xd2000001, 0xd8000768, 0x17c07c1f, 0xc2803780, 0x1291841f,
	0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830,
	0xe1000003, 0x18c0001f, 0x10006834, 0xe0e00000, 0xe0e00001, 0xf0000000,
	0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f, 0xe0f07f0e,
	0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d, 0xe0f07c0d,
	0xe0f0780d, 0xf0000000, 0xe0f0700d, 0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e,
	0xf0000000, 0xe0f07f12, 0x11407c1f, 0x81f08407, 0x81f18407, 0x1b80001f,
	0x20000001, 0xa1d08407, 0xa1d18407, 0x1392841f, 0x812ab401, 0x80ebb401,
	0xa0c00c04, 0xd8203603, 0x17c07c1f, 0x80c01403, 0xd8203423, 0x01400405,
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
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0x1a50001f, 0x10006610, 0x8246a401, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2f7be75f, 0x81469801, 0xd8004305, 0x17c07c1f,
	0x1b80001f, 0xd00f0000, 0x8880000c, 0x2f7be75f, 0xd8005da2, 0x17c07c1f,
	0xd0004340, 0x17c07c1f, 0x1b80001f, 0x500f0000, 0xe8208000, 0x10006354,
	0xfffe7b47, 0xc0c06940, 0x81401801, 0xd80048e5, 0x17c07c1f, 0x81f60407,
	0x18c0001f, 0x10006200, 0xc0c05e60, 0x12807c1f, 0xe8208000, 0x1000625c,
	0x00000001, 0x1b80001f, 0x20000080, 0xc0c05e60, 0x1280041f, 0x18c0001f,
	0x10006204, 0xc0c06200, 0x1280041f, 0x18c0001f, 0x10006208, 0xc0c05e60,
	0x12807c1f, 0xe8208000, 0x10006244, 0x00000001, 0x1b80001f, 0x20000080,
	0xc0c05e60, 0x1280041f, 0x18d0001f, 0x10200200, 0x18c0001f, 0x10006290,
	0xc0c05e60, 0x1280041f, 0xe8208000, 0x10006404, 0x00003101, 0xc2803780,
	0x1292041f, 0x81469801, 0xd8204a45, 0x17c07c1f, 0x1b00001f, 0x2f7be75f,
	0x1b80001f, 0x30000004, 0x8880000c, 0x2f7be75f, 0xd8005802, 0x17c07c1f,
	0xc0c064c0, 0x17c07c1f, 0x18c0001f, 0x10006294, 0xe0f07fff, 0xe0e00fff,
	0xe0e000ff, 0x81449801, 0xd8004c85, 0x17c07c1f, 0x1a00001f, 0x10006604,
	0xe2200003, 0xc0c06580, 0x17c07c1f, 0xe2200005, 0xc0c06580, 0x17c07c1f,
	0xa1d38407, 0xa1d98407, 0x1800001f, 0x00000012, 0x1800001f, 0x00000e12,
	0x1800001f, 0x03800e12, 0x1800001f, 0x038e0e12, 0xe8208000, 0x10006310,
	0x0b1600f8, 0x1b00001f, 0xbfffe7ff, 0x1b80001f, 0x90100000, 0x80c00400,
	0xd8204fa3, 0xa1d58407, 0xa1dd8407, 0x1b00001f, 0x3fffefff, 0xd0004e60,
	0x17c07c1f, 0x1890001f, 0x100063e8, 0x88c0000c, 0x2f7be75f, 0xd80051c3,
	0x17c07c1f, 0x80c40001, 0xd8005143, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff,
	0xd0005180, 0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd0004e60, 0x17c07c1f,
	0x80c40001, 0xd82052c3, 0x17c07c1f, 0xa1de0407, 0x1b00001f, 0x7fffe7ff,
	0xd0004e60, 0x17c07c1f, 0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc,
	0xe0e007f8, 0xe0e00ff0, 0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00,
	0x81449801, 0xd80055a5, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200002,
	0xc0c06580, 0x17c07c1f, 0xe2200004, 0xc0c06580, 0x17c07c1f, 0x1b80001f,
	0x200016a8, 0x1800001f, 0x03800e12, 0x1b80001f, 0x20000300, 0x1800001f,
	0x00000e12, 0x1b80001f, 0x20000300, 0x1800001f, 0x00000012, 0x1b80001f,
	0x20000104, 0x10007c1f, 0x81f38407, 0x81f98407, 0x81f90407, 0x81f40407,
	0x1b80001f, 0x200016a8, 0x81401801, 0xd8005da5, 0x17c07c1f, 0xe8208000,
	0x10006404, 0x00002101, 0x18c0001f, 0x10006290, 0x1212841f, 0xc0c05fe0,
	0x12807c1f, 0xc0c05fe0, 0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f,
	0xc0c05fe0, 0x12807c1f, 0xe8208000, 0x10006244, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c05fe0, 0x1280041f, 0xe8208000, 0x10200268, 0x000ffffe,
	0x18c0001f, 0x10006204, 0x1212841f, 0xc0c06340, 0x1280041f, 0x18c0001f,
	0x10006200, 0x1212841f, 0xc0c05fe0, 0x12807c1f, 0xe8208000, 0x1000625c,
	0x00000000, 0x1b80001f, 0x20000080, 0xc0c05fe0, 0x1280041f, 0x19c0001f,
	0x01411820, 0x1ac0001f, 0x55aa55aa, 0x10007c1f, 0xf0000000, 0xd8005f0a,
	0x17c07c1f, 0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xd8205faa, 0x17c07c1f,
	0xe2e0002e, 0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0xd80060aa,
	0x17c07c1f, 0xe2e00036, 0xe2e0003e, 0x1380201f, 0xe2e0003c, 0xd82061ca,
	0x17c07c1f, 0x1380201f, 0xe2e0007c, 0x1b80001f, 0x20000003, 0xe2e0005c,
	0xe2e0004c, 0xe2e0004d, 0xf0000000, 0x17c07c1f, 0xd8206309, 0x17c07c1f,
	0xe2e0000d, 0xe2e0000c, 0xe2e0001c, 0xe2e0001e, 0xe2e00016, 0xe2e00012,
	0xf0000000, 0x17c07c1f, 0xd8206489, 0x17c07c1f, 0xe2e00016, 0x1380201f,
	0xe2e0001e, 0x1380201f, 0xe2e0001c, 0x1380201f, 0xe2e0000c, 0xe2e0000d,
	0xf0000000, 0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0x1393041f,
	0xf0000000, 0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd8206583,
	0x17c07c1f, 0xf0000000, 0x17c07c1f, 0xe8208000, 0x11008014, 0x00000002,
	0xe8208000, 0x11008020, 0x00000101, 0xe8208000, 0x11008004, 0x000000d0,
	0x1a00001f, 0x11008000, 0xd800684a, 0xe220005d, 0xd820686a, 0xe2200000,
	0xe2200001, 0xe8208000, 0x11008024, 0x00000001, 0x1b80001f, 0x20000424,
	0xf0000000, 0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000,
	0x17c07c1f
};

static struct pcm_desc suspend_pcm_ca7 = {
	.version = "pcm_suspend_20150805_V1",
	.base = suspend_binary_ca7,
	.size = 847,
	.sess = 2,
	.replace = 0,
	.vec0 = EVENT_VEC(11, 1, 0, 0),
	.vec1 = EVENT_VEC(12, 1, 0, 54),
	.vec2 = EVENT_VEC(30, 1, 0, 143),
	.vec3 = EVENT_VEC(31, 1, 0, 277),
};

/**************************************
 * SW code for suspend
 **************************************/
#define SPM_SYSCLK_SETTLE       128	/* 3.9ms */

#define WAIT_UART_ACK_TIMES     80	/* 80 * 10us */

#define SPM_WAKE_PERIOD         600	/* sec */

#define WAKE_SRC_FOR_SUSPEND \
		(WAKE_SRC_KP | WAKE_SRC_EINT | WAKE_SRC_MD32 | \
		 WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_THERM | \
		 WAKE_SRC_SYSPWREQ | WAKE_SRC_ALL_MD32 | \
		 WAKE_SRC_HDMI_CEC | WAKE_SRC_IRRX)

#define WAKE_SRC_FOR_MD32  0

#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

static struct pwr_ctrl suspend_ctrl = {
	.wake_src = WAKE_SRC_FOR_SUSPEND,
	.wake_src_md32 = WAKE_SRC_FOR_MD32,
	.r0_ctrl_en = 1,
	.r7_ctrl_en = 1,
	.infra_dcm_lock = 1,
	.wfi_op = WFI_OP_AND,
	.pcm_apsrc_req = 0,
	.ca7top_idle_mask = 0,
	.ca15top_idle_mask = 0,
	.mcusys_idle_mask = 0,
	.disp_req_mask = 0,
	.mfg_req_mask = 0,
	.md32_req_mask = 1,
	.srclkenai_mask = 1,
	.dsi0_ddr_en_mask = 1,
	.dsi1_ddr_en_mask = 1,
	.dpi_ddr_en_mask = 1,
	.isp0_ddr_en_mask = 1,
	.isp1_ddr_en_mask = 1,
	.ca7_wfi0_en = 1,
	.ca7_wfi1_en = 1,
	.ca7_wfi2_en = 1,
	.ca7_wfi3_en = 1,
	.ca15_wfi0_en = 1,
	.ca15_wfi1_en = 1,
	.ca15_wfi2_en = 1,
	.ca15_wfi3_en = 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask = 1,
#endif
};

struct spm_lp_scen __spm_suspend = {
	.pcmdesc = &suspend_pcm_ca7,
	.pwrctrl = &suspend_ctrl,
};

static void spm_set_suspend_pcm_ver(void)
{
	__spm_suspend.pcmdesc = &suspend_pcm_ca7;
}

static void spm_set_sysclk_settle(void)
{
	u32 settle;

	/* SYSCLK settle = VTCXO settle time */
	spm_write(SPM_CLK_SETTLE, SPM_SYSCLK_SETTLE);
	settle = spm_read(SPM_CLK_SETTLE);

	spm_crit2("settle = %u\n", settle);
}

static void spm_kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
	/* enable PCM WDT (normal mode) to start count if needed */
#if SPM_PCMWDT_EN
	{
		u32 con1;

		con1 = spm_read(SPM_PCM_CON1) & ~(CON1_PCM_WDT_WAKE_MODE | CON1_PCM_WDT_EN);
		spm_write(SPM_PCM_CON1, CON1_CFG_KEY | con1);

		if (spm_read(SPM_PCM_TIMER_VAL) > PCM_TIMER_MAX)
			spm_write(SPM_PCM_TIMER_VAL, PCM_TIMER_MAX);
		spm_write(SPM_PCM_WDT_TIMER_VAL, spm_read(SPM_PCM_TIMER_VAL) + PCM_WDT_TIMEOUT);
		spm_write(SPM_PCM_CON1, con1 | CON1_CFG_KEY | CON1_PCM_WDT_EN);
	}
#endif

	/* init PCM_PASR_DPD_0 for DPD */
	spm_write(SPM_PCM_PASR_DPD_0, 0);

/* FIXME: for 8173 fpga early porting */
#if 0
	/* make MD32 work in suspend: fscp_ck = CLK26M */
	clkmux_sel(MT_MUX_SCP, 0, "SPM-Sleep");
#endif

	__spm_kick_pcm_to_run(pwrctrl);
}

static void spm_trigger_wfi_for_sleep(struct pwr_ctrl *pwrctrl)
{
	if (is_cpu_pdn(pwrctrl->pcm_flags)) {
		spm_dormant_sta = mt_cpu_dormant(CPU_SHUTDOWN_MODE);
		switch (spm_dormant_sta) {
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
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) | ACINACTM);
		wfi_with_sync();
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) & ~ACINACTM);
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

/* FIXME: for 8173 fpga early porting */
#if 0
	/* restore clock mux: fscp_ck = SYSPLL1_D2 */
	clkmux_sel(MT_MUX_SCP, 1, "SPM-Sleep");
#endif
}

static wake_reason_t spm_output_wake_reason(struct wake_status *wakesta, struct pcm_desc *pcmdesc)
{
	wake_reason_t wr;

	wr = __spm_output_wake_reason(wakesta, pcmdesc, true);

	spm_crit2("big core = %d, suspend dormant state = %d\n", SPM_CTRL_BIG_CPU, spm_dormant_sta);
/* TODO: eint may need to provide new APIs
	if (wakesta->r12 & WAKE_SRC_EINT)
		mt_eint_print_status();
*/
	return wr;
}

#if SPM_PWAKE_EN
static u32 spm_get_wake_period(int pwake_time, wake_reason_t last_wr)
{
	int period = SPM_WAKE_PERIOD;

	if (pwake_time < 0) {
		/* use FG to get the period of 1% battery decrease */
/* TODO: wait battery ready
		period = get_dynamic_period(last_wr != WR_PCM_TIMER ? 1 : 0, SPM_WAKE_PERIOD, 1);
*/
		period = 5400;
		if (period <= 0) {
			spm_warn("CANNOT GET PERIOD FROM FUEL GAUGE\n");
			period = SPM_WAKE_PERIOD;
		}
	} else {
		period = pwake_time;
		spm_crit2("pwake = %d\n", pwake_time);
	}

	if (period > 36 * 3600)	/* max period is 36.4 hours */
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

void spm_set_wakeup_event_map(struct mt_wake_event_map *tbl)
{
	mt_wake_event_tbl = tbl;
}

void spm_register_wakeup_event(struct mt_wake_event_map *map)
{
	if (we_reg_count >= MAX_REGISTER_WAKE_EVENT) {
		pr_err("can't register wakeup event. over limit: %d\n", MAX_REGISTER_WAKE_EVENT);
		return;
	}

	runtime_wake_event_tbl[we_reg_count++] = map;
}

static struct mt_wake_event_map *spm_map_wakeup_event(struct mt_wake_event *mt_we)
{
	int i;

	/* map proprietary mt_wake_event to wake_source_t */
	struct mt_wake_event_map *tbl = mt_wake_event_tbl;

	if (tbl && mt_we) {
		for (; tbl->domain; tbl++) {
			if (!strcmp(tbl->domain, mt_we->domain) && tbl->code == mt_we->code)
				return tbl;
		}
	}

	/* check if it belongs to runtime registered ones */
	for (i = 0; i < we_reg_count && mt_we; i++) {
		tbl = runtime_wake_event_tbl[i];
		if (!strcmp(tbl->domain, mt_we->domain) && tbl->code == mt_we->code)
			return tbl;
	}

	return NULL;
}

wakeup_event_t spm_read_wakeup_event_and_irq(int *pirq)
{
	struct mt_wake_event_map *tbl = spm_map_wakeup_event(spm_get_wakeup_event());

	if (!tbl)
		return WEV_NONE;

	if (pirq)
		*pirq = tbl->irq;
	return tbl->we;
}
EXPORT_SYMBOL(spm_read_wakeup_event_and_irq);

void spm_report_wakeup_event(struct mt_wake_event *we, int code)
{
	unsigned long flags;
	struct mt_wake_event *head;
	struct mt_wake_event_map *evt;

	static const char const *ev_desc[] = {
		"RTC", "WIFI", "WAN", "USB",
		"PWR", "HALL", "BT", "CHARGER",
	};

	spin_lock_irqsave(&__spm_lock, flags);
	head = mt_wake_event;
	mt_wake_event = we;
	we->parent = head;
	we->code = code;
	mt_wake_event = we;
	spin_unlock_irqrestore(&__spm_lock, flags);
	pr_err("%s: WAKE EVT: %s#%d (parent %s#%d)\n",
	       __func__, we->domain, we->code,
	       head ? head->domain : "NONE", head ? head->code : -1);
	evt = spm_map_wakeup_event(we);
	if (evt && evt->we != WEV_NONE) {
		const char *name = (evt->we >= 0
			      && evt->we < ARRAY_SIZE(ev_desc)) ? ev_desc[evt->we] : "UNKNOWN";
		pm_report_resume_irq(evt->irq);
		pr_err("%s: WAKEUP from source %d [%s]\n", __func__, evt->we, name);
	}
}
EXPORT_SYMBOL(spm_report_wakeup_event);

void spm_clear_wakeup_event(void)
{
	mt_wake_event = NULL;
}
EXPORT_SYMBOL(spm_clear_wakeup_event);

wakeup_event_t irq_to_wakeup_ev(int irq)
{
	struct mt_wake_event_map *tbl = mt_wake_event_tbl;

	if (!tbl)
		return WEV_NONE;

	for (; tbl->domain; tbl++) {
		if (tbl->irq == irq)
			return tbl->we;
	}

	return WEV_MAX;
}
EXPORT_SYMBOL(irq_to_wakeup_ev);

struct mt_wake_event *spm_get_wakeup_event(void)
{
	return mt_wake_event;
}
EXPORT_SYMBOL(spm_get_wakeup_event);

static void spm_report_wake_source(u32 event_mask)
{
	int event = -1;
	u32 mask = event_mask;
	int event_count = 0;

	while (mask) {
		event = __ffs(mask);
		event_count++;
		mask &= ~(1 << event);
	}
	if (WARN_ON(event_count != 1))
		pr_err("%s: multiple wakeup events detected: %08X\n", __func__, event_mask);

	if (event >= 0)
		spm_report_wakeup_event(&spm_wake_event, event);
}

wake_reason_t spm_go_to_sleep(u32 spm_flags, u32 spm_data)
{
	u32 sec = 0;
	int wd_ret;
	struct wake_status wakesta;
	unsigned long flags;
/* TODO: wait irq driver ready
	struct mtk_irq_mask mask;
*/
	struct wd_api *wd_api;
	static wake_reason_t last_wr = WR_NONE;
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl;

	pcmdesc = __spm_suspend.pcmdesc;
	pwrctrl = __spm_suspend.pwrctrl;

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
	set_pwrctrl_pcm_data(pwrctrl, spm_data);

#if SPM_PWAKE_EN
	sec = spm_get_wake_period(-1 /* FIXME */ , last_wr);
#endif
	pwrctrl->timer_val = sec * 32768;

	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret)
		wd_api->wd_suspend_notify();

	spin_lock_irqsave(&__spm_lock, flags);

/* TODO: wait irq & cirq driver ready
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(MT_SPM_IRQ_ID);
	mt_cirq_clone_gic();
	mt_cirq_enable();
*/
	spm_set_sysclk_settle();

#if defined(CONFIG_AMAZON_METRICS_LOG)
	/* forced trigger system_resume:off_mode metrics log */
	if (force_gpt == 1) {
		gpt_set_cmp(GPT4, 1);
		start_gpt(GPT4);
		/* wait HW GPT trigger */
		udelay(200);
		pwrctrl->wake_src |= WAKE_SRC_GPT;
	}
#endif

	spm_crit2("sec = %u, wakesrc = 0x%x (%u)(%u)\n",
		  sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags),
		  is_infra_pdn(pwrctrl->pcm_flags));

	if (request_uart_to_sleep()) {
		last_wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	if (!is_cpu_pdn(pwrctrl->pcm_flags)) {
		pmic_config_interface(0x21E, 0x0, 0x3, 0);
		pmic_config_interface(0x244, 0x0, 0x3, 0);
		pmic_config_interface(0x270, 0x0, 0x1, 1);
	}

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	spm_kick_pcm_to_run(pwrctrl);

	spm_trigger_wfi_for_sleep(pwrctrl);

	__spm_get_wakeup_status(&wakesta);

	spm_clean_after_wakeup();

	request_uart_to_wakeup();

	last_wr = spm_output_wake_reason(&wakesta, pcmdesc);

#if defined(CONFIG_AMAZON_METRICS_LOG)
	/* forced trigger system_resume:off_mode metrics log */
	if (force_gpt == 1) {
		if (gpt_check_and_ack_irq(GPT4))
			spm_crit2("GPT4 triggered for off_mode metrics test\n");
		pwrctrl->wake_src &= ~WAKE_SRC_GPT;
	}
#endif

 RESTORE_IRQ:

/* TODO: wait irq & cirq driver ready
	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
*/
	spin_unlock_irqrestore(&__spm_lock, flags);

	if ((last_wr == WR_WAKE_SRC) || (last_wr == WR_PCM_ABORT))
		spm_report_wake_source(wakesta.r12);

	if (!wd_ret)
		wd_api->wd_resume_notify();

	return last_wr;
}

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

	if (wakeup_src) {
		spm_crit2("WARNING: spm_check_wakeup_src = 0x%x", wakeup_src);
		return 1;
	} else
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

void spm_output_sleep_option(void)
{
	spm_notice("PWAKE_EN:%d, PCMWDT_EN:%d, BYPASS_SYSPWREQ:%d\n",
		   SPM_PWAKE_EN, SPM_PCMWDT_EN, SPM_BYPASS_SYSPWREQ);
}

void spm_suspend_init(void)
{
	spm_set_suspend_pcm_ver();
}

MODULE_DESCRIPTION("SPM-Sleep Driver v1.0");
