/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>
#include <linux/lockdep.h>
#include <linux/irqchip/mt-gic.h>

#include <mach/wd_api.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mt_cirq.h>

#include "mt_spm_idle.h"
#include "mt_cpufreq.h"
#include "mt_cpuidle.h"

#include "mt_spm_internal.h"

/*
 * only for internal debug
 */
#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_PWAKE_EN            1
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define WAKE_SRC_FOR_DPIDLE \
		(WAKE_SRC_KP | WAKE_SRC_GPT | WAKE_SRC_EINT | WAKE_SRC_MD32 | \
		 WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_AFE | WAKE_SRC_THERM | \
		 WAKE_SRC_SYSPWREQ)

#define WAKE_SRC_FOR_MD32  0

#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

#undef MCUCFG_BASE
#ifdef CONFIG_OF
#define MCUCFG_BASE          spm_mcucfg
#else
#define MCUCFG_BASE          (0xF0200000)	/* 0x1020_0000 */
#endif
#define MP0_AXI_CONFIG          (MCUCFG_BASE + 0x2C)
#define MP1_AXI_CONFIG          (MCUCFG_BASE + 0x22C)
#define ACINACTM                (1<<4)

/*
 * PCM sequence for cpu deep idle
 */
static const u32 dpidle_binary[] = {
	0x1800001f, 0x17cf7f16, 0x1990001f, 0x10006b08, 0x81419801, 0xd8000125,
	0x17c07c1f, 0xc0c02600, 0x10c0041f, 0xe8208000, 0x10006b04, 0x00000008,
	0xe8208000, 0x10006b04, 0x00000010, 0xe8208000, 0x10006354, 0xfffe7fff,
	0xe8208000, 0x10006b04, 0x00000000, 0xc2802880, 0x1290041f, 0x8880000c,
	0x2f7be75f, 0xd82003e2, 0x17c07c1f, 0x1b00001f, 0x7fffe7ff, 0xd0000420,
	0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0xe8208000,
	0x10006354, 0xffffffff, 0x1940001f, 0x10004094, 0x1990001f, 0x1020e374,
	0xe1400006, 0x1940001f, 0x10004098, 0x1990001f, 0x1020e378, 0xe1400006,
	0x1940001f, 0x10011094, 0x1990001f, 0x10213374, 0xe1400006, 0x1940001f,
	0x10011098, 0x1990001f, 0x10213378, 0xe1400006, 0x1990001f, 0x10213378,
	0x1990001f, 0x10006b08, 0x81419801, 0xd8000865, 0x17c07c1f, 0xc0c02600,
	0x10c07c1f, 0xe8208000, 0x10006b04, 0x00000002, 0xc2802880, 0x1290841f,
	0x1800001f, 0x17cf7f36, 0x1b00001f, 0x3fffefff, 0xf0000000, 0x17c07c1f,
	0x81411801, 0xd8000d85, 0x17c07c1f, 0xe8208000, 0x1000f600, 0xd2000000,
	0x1b80001f, 0x20000208, 0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e,
	0xe0e0000e, 0xe0e0000f, 0x18c0001f, 0x100040e4, 0x1910001f, 0x100040e4,
	0xa1158404, 0xe0c00004, 0x81358404, 0xe0c00004, 0x18c0001f, 0x100110e4,
	0x1910001f, 0x100110e4, 0xa1158404, 0xe0c00004, 0x81358404, 0xe0c00004,
	0x1800001f, 0x17cf5f16, 0x1b80001f, 0x20000208, 0x1800001f, 0x17cf1f16,
	0x1b80001f, 0x20000208, 0x1800001f, 0x17cf0f16, 0x1800001f, 0x07cf0f16,
	0x1b80001f, 0x20000208, 0x1800001f, 0x07ce0f16, 0x1800001f, 0x078e0f16,
	0xa01d8400, 0x1b80001f, 0x20000034, 0x803d8400, 0x1b80001f, 0x20000152,
	0x1800001f, 0x038e0f16, 0x1b80001f, 0x20000208, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0xa1000404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0xa1000404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0x80340400, 0x17c07c1f, 0x17c07c1f, 0x1800001f, 0x038e0e12, 0x1b80001f,
	0x2000000a, 0x18c0001f, 0x10006240, 0xe0e0000d, 0x1b80001f, 0x20000100,
	0x81fa0407, 0x81f18407, 0x81f08407, 0xe8208000, 0x10006354, 0xfffe7b47,
	0xa1d80407, 0xa1dc0407, 0xc2802880, 0x1291041f, 0x8880000c, 0x2f7be75f,
	0xd8201682, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0xd00016c0, 0x17c07c1f,
	0x1b00001f, 0xbfffe7ff, 0xf0000000, 0x17c07c1f, 0x1b80001f, 0x20000fdf,
	0x1890001f, 0x10006608, 0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002,
	0x8080080d, 0xd82019a2, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0x1b80001f,
	0x20000004, 0xd800202c, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0002020,
	0x17c07c1f, 0x81f80407, 0x81fc0407, 0x1880001f, 0x10006320, 0xc0c022c0,
	0xe080000f, 0xd8001863, 0x17c07c1f, 0xe080001f, 0xa1da0407, 0x1800001f,
	0x038e0e16, 0x1800001f, 0x038e0f16, 0x18c0001f, 0x1000f5c8, 0x1910001f,
	0x1000f5c8, 0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8, 0x1910001f,
	0x100125c8, 0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8, 0x1800001f,
	0x07cf0f16, 0x1800001f, 0x17cf0f16, 0x1800001f, 0x17cf1f16, 0x1800001f,
	0x17cf7f16, 0x1b80001f, 0x20000104, 0x81411801, 0xd8001fa5, 0x17c07c1f,
	0x18c0001f, 0x10006240, 0xc0c021c0, 0x17c07c1f, 0xe8208000, 0x1000f600,
	0xd2000001, 0xc2802880, 0x1291841f, 0x1b00001f, 0x7ffff7ff, 0xf0000000,
	0x17c07c1f, 0x1900001f, 0x10006830, 0xe1000003, 0xe8208000, 0x10006834,
	0x00000000, 0xe8208000, 0x10006834, 0x00000001, 0xf0000000, 0x17c07c1f,
	0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e, 0x1b80001f, 0x20000080, 0xe0f07f12,
	0xf0000000, 0x17c07c1f, 0x11407c1f, 0x81f08407, 0x81f18407, 0x1b80001f,
	0x20000007, 0xa1d08407, 0xa1d18407, 0x1b80001f, 0x20000020, 0x812ab401,
	0x80ebb401, 0xa0c00c04, 0xd82024e3, 0x17c07c1f, 0x80c01403, 0xd82022e3,
	0x01400405, 0x1900001f, 0x10006814, 0xf0000000, 0xe1000003, 0xa1d10407,
	0x1b80001f, 0x20000020, 0xf0000000, 0x17c07c1f, 0x1a00001f, 0x10006604,
	0xd8002783, 0x17c07c1f, 0xe2200007, 0x1b80001f, 0x2000008c, 0xe2200005,
	0x1b80001f, 0x2000008c, 0xd8202843, 0x17c07c1f, 0xe2200004, 0x1b80001f,
	0x2000008c, 0xe2200006, 0x1b80001f, 0x20000208, 0xf0000000, 0x17c07c1f,
	0x18c0001f, 0x10006b6c, 0x1910001f, 0x10006b6c, 0xa1002804, 0xe0c00004,
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
	0x10006b08, 0x1a50001f, 0x10006610, 0x8246a401, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2f7be75f, 0x81469801, 0xd8004305, 0x17c07c1f,
	0x1b80001f, 0xd00f0000, 0x8880000c, 0x2f7be75f, 0xd8005e42, 0x17c07c1f,
	0xd0004340, 0x17c07c1f, 0x1b80001f, 0x500f0000, 0xe8208000, 0x10006354,
	0xfffe7b47, 0xc0c02560, 0x81401801, 0xd8004965, 0x17c07c1f, 0x81f60407,
	0x18c0001f, 0x10006200, 0xc0c05f00, 0x12807c1f, 0xe8208000, 0x1000625c,
	0x00000001, 0x1890001f, 0x1000625c, 0x81040801, 0xd8204524, 0x17c07c1f,
	0xc0c05f00, 0x1280041f, 0x18c0001f, 0x10006204, 0xc0c062a0, 0x1280041f,
	0x18c0001f, 0x10006208, 0xc0c05f00, 0x12807c1f, 0xe8208000, 0x10006248,
	0x00000000, 0x1890001f, 0x10006248, 0x81040801, 0xd8004764, 0x17c07c1f,
	0xc0c05f00, 0x1280041f, 0x18c0001f, 0x10006290, 0xc0c05f00, 0x1280041f,
	0xe8208000, 0x10006404, 0x00003101, 0xc2802880, 0x1292041f, 0x81469801,
	0xd8204ac5, 0x17c07c1f, 0x1b00001f, 0x2f7be75f, 0x1b80001f, 0x30000004,
	0x8880000c, 0x2f7be75f, 0xd8005802, 0x17c07c1f, 0xc0c06560, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0f07fff, 0xe0e00fff, 0xe0e000ff, 0x81449801,
	0xd8004d05, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200003, 0xc0c06620,
	0x17c07c1f, 0xe2200001, 0xc0c06620, 0x17c07c1f, 0xa1d38407, 0xa1d98407,
	0x1800001f, 0x00000012, 0x1800001f, 0x00000e12, 0x1800001f, 0x03800e12,
	0x1800001f, 0x038e0e12, 0xe8208000, 0x10006310, 0x0b1600f8, 0x1b00001f,
	0xbfffe7ff, 0x1b80001f, 0x90100000, 0x80c28001, 0xd8205023, 0x17c07c1f,
	0xa1dd8407, 0x1b00001f, 0x3fffefff, 0xd0004ee0, 0x17c07c1f, 0x1890001f,
	0x100063e8, 0x88c0000c, 0x2f7be75f, 0xd8005243, 0x17c07c1f, 0x80c10001,
	0xd80051c3, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0005200, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xd0004ee0, 0x17c07c1f, 0x80c10001, 0xd8205343,
	0x17c07c1f, 0xa1de0407, 0x1b00001f, 0x7fffe7ff, 0xd0004ee0, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc, 0xe0e007f8, 0xe0e00ff0,
	0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00, 0x81449801, 0xd80055e5,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200000, 0xc0c06620, 0x17c07c1f,
	0xe2200002, 0xc0c06620, 0x17c07c1f, 0x1800001f, 0x03800e12, 0x1b80001f,
	0x20000300, 0x1800001f, 0x00000e12, 0x1b80001f, 0x20000300, 0x1800001f,
	0x00000012, 0x1b80001f, 0x20000104, 0x10007c1f, 0x81f38407, 0x81f98407,
	0x81f90407, 0x81f40407, 0x81401801, 0xd8005e45, 0x17c07c1f, 0xe8208000,
	0x10006404, 0x00002101, 0x18c0001f, 0x10006290, 0x1212841f, 0xc0c06080,
	0x12807c1f, 0xc0c06080, 0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f,
	0xc0c06080, 0x12807c1f, 0xe8208000, 0x10006248, 0x00000001, 0x1890001f,
	0x10006248, 0x81040801, 0xd8205aa4, 0x17c07c1f, 0x1b80001f, 0x20000020,
	0xc0c06080, 0x1280041f, 0x18c0001f, 0x10006204, 0x1212841f, 0xc0c063e0,
	0x1280041f, 0x18c0001f, 0x10006200, 0x1212841f, 0xc0c06080, 0x12807c1f,
	0xe8208000, 0x1000625c, 0x00000000, 0x1890001f, 0x1000625c, 0x81040801,
	0xd8005d64, 0x17c07c1f, 0xc0c06080, 0x1280041f, 0x19c0001f, 0x01411820,
	0x1ac0001f, 0x55aa55aa, 0x10007c1f, 0xf0000000, 0xd8005faa, 0x17c07c1f,
	0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xd820604a, 0x17c07c1f, 0xe2e0002e,
	0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0xd800614a, 0x17c07c1f,
	0xe2e00036, 0xe2e0003e, 0x1380201f, 0xe2e0003c, 0xd820626a, 0x17c07c1f,
	0x1380201f, 0xe2e0007c, 0x1b80001f, 0x20000003, 0xe2e0005c, 0xe2e0004c,
	0xe2e0004d, 0xf0000000, 0x17c07c1f, 0xd82063a9, 0x17c07c1f, 0xe2e0000d,
	0xe2e0000c, 0xe2e0001c, 0xe2e0001e, 0xe2e00016, 0xe2e00012, 0xf0000000,
	0x17c07c1f, 0xd8206529, 0x17c07c1f, 0xe2e00016, 0x1380201f, 0xe2e0001e,
	0x1380201f, 0xe2e0001c, 0x1380201f, 0xe2e0000c, 0xe2e0000d, 0xf0000000,
	0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0x1393041f, 0xf0000000,
	0x17c07c1f, 0x1910001f, 0x10006604, 0x10cf901f, 0xd8206623, 0x17c07c1f,
	0xf0000000, 0x17c07c1f
};

static struct pcm_desc dpidle_pcm = {
	.version = "pcm_deepidle_2016_1115_mt8173",
	.base = dpidle_binary,
	.size = 824,
	.sess = 2,
	.replace = 0,
	.vec0 = EVENT_VEC(11, 1, 0, 0),		/* FUNC_26M_WAKEUP */
	.vec1 = EVENT_VEC(12, 1, 0, 35),	/* FUNC_26M_SLEEP */
	.vec2 = EVENT_VEC(30, 1, 0, 78),	/* FUNC_APSRC_WAKEUP */
	.vec3 = EVENT_VEC(31, 1, 0, 184),	/* FUNC_APSRC_SLEEP */
};

static struct pwr_ctrl dpidle_ctrl = {
	.wake_src = WAKE_SRC_FOR_DPIDLE,
	.wake_src_md32 = WAKE_SRC_FOR_MD32,
	.r0_ctrl_en = 1,
	.r7_ctrl_en = 1,
	.infra_dcm_lock = 1,
	.wfi_op = WFI_OP_AND,
	.pcm_apsrc_req = 0,
	.srclkenai_mask = 1,
	.ca15_wfi0_en = 1,
	.ca15_wfi1_en = 1,
	.ca15_wfi2_en = 1,
	.ca15_wfi3_en = 1,
	.ca7_wfi0_en = 1,
	.ca7_wfi1_en = 1,
	.ca7_wfi2_en = 1,
	.ca7_wfi3_en = 1,
	.disp_req_mask = 1,
	.mfg_req_mask = 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask = 1,
#endif
};

struct spm_lp_scen __spm_dpidle = {
	.pcmdesc = &dpidle_pcm,
	.pwrctrl = &dpidle_ctrl,
};

int __attribute__((weak)) request_uart_to_sleep(void)
{
	return 0;
}

int __attribute__((weak)) request_uart_to_wakeup(void)
{
	return 0;
}

void __attribute__((weak)) mt_cirq_clone_gic(void)
{
}

void __attribute__((weak)) mt_cirq_enable(void)
{
}

void __attribute__((weak)) mt_cirq_flush(void)
{
}

void __attribute__((weak)) mt_cirq_disable(void)
{
}

u32 __attribute__ ((weak)) spm_get_sleep_wakesrc(void)
{
	return 0;
}

/* TODO: wait pmic & cpufreqready
void __attribute__ ((weak)) mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase)
{
}
*/

int __attribute__ ((weak)) mt_cpu_dormant(unsigned long flags)
{
	return 0;
}

static void spm_trigger_wfi_for_dpidle(struct pwr_ctrl *pwrctrl)
{
	if (is_cpu_pdn(pwrctrl->pcm_flags)) {
		mt_cpu_dormant(CPU_DEEPIDLE_MODE);
	} else {
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) | ACINACTM);
		wfi_with_sync();
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) & ~ACINACTM);
	}
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

void __attribute__ ((weak)) spm_dpidle_before_wfi(void)
{
}

void __attribute__ ((weak)) spm_dpidle_after_wfi(void)
{
}

wake_reason_t spm_go_to_dpidle(u32 spm_flags, u32 spm_data)
{
	struct wake_status wakesta;
	unsigned long flags;
/* TODO: wait irq driver ready
	struct mtk_irq_mask mask;
*/
	wake_reason_t wr = WR_NONE;
	struct pcm_desc *pcmdesc = __spm_dpidle.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;
	struct spm_lp_scen *lpscen;

	lpscen = &__spm_dpidle;
	pcmdesc = lpscen->pcmdesc;
	pwrctrl = lpscen->pwrctrl;

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);


	/* set PMIC WRAP table for deepidle power control */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE); /* TODO: wait pmic & cpufreq ready */

	lockdep_off();
	spin_lock_irqsave(&__spm_lock, flags);
/* TODO: wait irq driver ready
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
*/
	mt_cirq_clone_gic();
	mt_cirq_enable();

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

	spm_trigger_wfi_for_dpidle(pwrctrl);

	spm_dpidle_after_wfi();

	__spm_get_wakeup_status(&wakesta);

	__spm_clean_after_wakeup();

	request_uart_to_wakeup();

	wr = __spm_output_wake_reason(&wakesta, pcmdesc, false);

 RESTORE_IRQ:

	mt_cirq_flush();
	mt_cirq_disable();
/* TODO: wait irq driver ready
	mt_irq_mask_restore(&mask);
*/
	spin_unlock_irqrestore(&__spm_lock, flags);
	lockdep_on();
	/* set PMIC WRAP table for normal power control */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL); /* TODO: wait pmic & cpufreq ready */

	return wr;
}

#if SPM_PWAKE_EN
#define SPM_WAKE_PERIOD         600	/* sec */
static u32 spm_get_wake_period(int pwake_time, wake_reason_t last_wr)
{
	int period = SPM_WAKE_PERIOD;

	if (pwake_time < 0) {
		/* use FG to get the period of 1% battery decrease */
/* TODO: wait battery driver ready
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
/* TODO: wait irq driver ready
	struct mtk_irq_mask mask;
*/
	struct wd_api *wd_api;
	static wake_reason_t last_wr = WR_NONE;
	struct pcm_desc *pcmdesc = __spm_dpidle.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

#if SPM_PWAKE_EN
	sec = spm_get_wake_period(-1 /* FIXME */ , last_wr);
#endif
	pwrctrl->timer_val = sec * 32768;

	pwrctrl->wake_src = spm_get_sleep_wakesrc();

	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret)
		wd_api->wd_suspend_notify();

	spin_lock_irqsave(&__spm_lock, flags);

/* TODO: wait irq driver ready
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
*/
	mt_cirq_clone_gic();
	mt_cirq_enable();

	/* set PMIC WRAP table for deepidle power control */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE); /* TODO: wait pmic & cpufreq ready */

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
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL); /* TODO: wait pmic & cpufreq ready */

	mt_cirq_flush();
	mt_cirq_disable();
/* TODO: wait irq driver ready
	mt_irq_mask_restore(&mask);
*/
	spin_unlock_irqrestore(&__spm_lock, flags);

	if (!wd_ret)
		wd_api->wd_resume_notify();

	return last_wr;
}

static void spm_set_dpidle_pcm_ver(void)
{

}

void spm_dpidle_init(void)
{
	spm_set_dpidle_pcm_ver();
}

MODULE_DESCRIPTION("SPM-DPIdle Driver v1.0");
