#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>  
#include <linux/cpu.h>

#include <mach/irqs.h>
#include <mach/mt_cirq.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_gpt.h>
#include <mach/hotplug.h>
#include <mach/mt_cpuidle.h>
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

#define SPM_AEE_RR_REC 1

#define WAKE_SRC_FOR_MCDI                     \
    ( WAKE_SRC_SYSPWREQ | WAKE_SRC_CPU_IRQ )


#define WAKE_SRC_FOR_MD32  0                                          \
    //(WAKE_SRC_AUD_MD32)


#define SPM_MCDI_CORE_MAX_EXIT_TIME 100000


/**********************************************************
 * E1 PCM code for MCDI
 **********************************************************/
static const u32 mcdi_binary[] = {
	0x1212841f, 0xe2e00036, 0x1380201f, 0xe2e0003e, 0x1380201f, 0xe2e0002e,
	0x1380201f, 0xe2a00000, 0x1b80001f, 0x20000080, 0xe2e0006e, 0xe2e0004e,
	0xe2e0004c, 0x1b80001f, 0x20000020, 0xe2e0004d, 0xf0000000, 0x17c07c1f,
	0xe2e0006d, 0xe2e0002d, 0xe2a00001, 0x1b80001f, 0x20000080, 0xe2e0002f,
	0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0x1212841f, 0xe2e00036,
	0x1380201f, 0xe2e0003e, 0xe8208000, 0x10001008, 0x00000160, 0x1380201f,
	0xe2e0002e, 0x1380201f, 0x1a00001f, 0x100062b4, 0x1910001f, 0x100062b4,
	0x81322804, 0xe2000004, 0x81202804, 0xe2000004, 0x1b80001f, 0x20000208,
	0x1910001f, 0x100062b4, 0x81142804, 0xd8000604, 0x17c07c1f, 0xe2e0006e,
	0xe2e0004e, 0xe2e0004c, 0x1b80001f, 0x20000020, 0xe2e0004d, 0xe8208000,
	0x10001008, 0x00000000, 0xf0000000, 0x17c07c1f, 0xe8208000, 0x10001008,
	0x00000160, 0xe2e0006d, 0xe2e0002d, 0x1a00001f, 0x100062b4, 0x1910001f,
	0x100062b4, 0xa1002804, 0xe2000004, 0xa1122804, 0xe2000004, 0x1b80001f,
	0x20000080, 0x1910001f, 0x100062b4, 0x81142804, 0xd82009e4, 0x17c07c1f,
	0xe2e0002f, 0xe2e0003e, 0xe2e00032, 0xe8208000, 0x10001008, 0x00000000,
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
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0x11407c1f, 0xe8208000,
	0x10006310, 0x0b160008, 0x1900001f, 0x000f7bde, 0x1a00001f, 0x10200268,
	0xe2000004, 0xe8208000, 0x10006600, 0x00000000, 0x1b00001f, 0x21000001,
	0x1b80001f, 0xd0010000, 0x69200006, 0xbeefbeef, 0xd8204604, 0x17c07c1f,
	0x1910001f, 0x10006358, 0x810b1001, 0xd80042c4, 0x17c07c1f, 0x1980001f,
	0xdeaddead, 0x69200006, 0xabcdabcd, 0xd82043a4, 0x17c07c1f, 0x88900001,
	0x10006814, 0x1910001f, 0x10006400, 0x81271002, 0x1880001f, 0x10006600,
	0xe0800004, 0x1910001f, 0x10006358, 0x810b1001, 0xd8004524, 0x17c07c1f,
	0x1980001f, 0x12345678, 0x60a07c05, 0x89100002, 0x10006600, 0x80801001,
	0xd8006ea2, 0x17c07c1f, 0x1a10001f, 0x10006720, 0x82002001, 0x82201408,
	0xd8204868, 0x17c07c1f, 0x1a40001f, 0x10006200, 0x1a80001f, 0x1000625c,
	0xc2400240, 0x17c07c1f, 0xa1400405, 0x1a10001f, 0x10006720, 0x8200a001,
	0x82209408, 0xd8204a08, 0x17c07c1f, 0x1a40001f, 0x10006218, 0x1a80001f,
	0x10006264, 0xc2400240, 0x17c07c1f, 0xa1508405, 0x1a10001f, 0x10006720,
	0x82012001, 0x82211408, 0xd8204ba8, 0x17c07c1f, 0x1a40001f, 0x1000621c,
	0x1a80001f, 0x1000626c, 0xc2400240, 0x17c07c1f, 0xa1510405, 0x1a10001f,
	0x10006720, 0x8201a001, 0x82219408, 0xd8204d48, 0x17c07c1f, 0x1a40001f,
	0x10006220, 0x1a80001f, 0x10006274, 0xc2400240, 0x17c07c1f, 0xa1518405,
	0x1a10001f, 0x10006720, 0x82022001, 0x82221408, 0xd8204ec8, 0x17c07c1f,
	0x1a40001f, 0x100062a0, 0x1280041f, 0xc2400800, 0x17c07c1f, 0xa1520405,
	0x1a10001f, 0x10006720, 0x8202a001, 0x82229408, 0xd8205048, 0x17c07c1f,
	0x1a40001f, 0x100062a4, 0x1290841f, 0xc2400800, 0x17c07c1f, 0xa1528405,
	0x1a10001f, 0x10006720, 0x82032001, 0x82231408, 0xd82051c8, 0x17c07c1f,
	0x1a40001f, 0x100062a8, 0x1291041f, 0xc2400800, 0x17c07c1f, 0xa1530405,
	0x1a10001f, 0x10006720, 0x8203a001, 0x82239408, 0xd8205348, 0x17c07c1f,
	0x1a40001f, 0x100062ac, 0x1291841f, 0xc2400800, 0x17c07c1f, 0xa1538405,
	0x1b80001f, 0x20000208, 0xd8206e6c, 0x17c07c1f, 0x81001401, 0xd8205744,
	0x17c07c1f, 0x1a10001f, 0x10006918, 0x81002001, 0xb1042081, 0xb1003081,
	0xb10c3081, 0xd8205744, 0x17c07c1f, 0x1a40001f, 0x10006200, 0x1a80001f,
	0x1000625c, 0xc2400000, 0x17c07c1f, 0x89400005, 0xfffffffe, 0xe8208000,
	0x10006f00, 0x00000000, 0xe8208000, 0x10006b30, 0x00000000, 0xe8208000,
	0x100063e0, 0x00000001, 0x81009401, 0xd8205aa4, 0x17c07c1f, 0x1a10001f,
	0x10006918, 0x8100a001, 0xb104a081, 0xb1003081, 0xd8205aa4, 0x17c07c1f,
	0x1a40001f, 0x10006218, 0x1a80001f, 0x10006264, 0xc2400000, 0x17c07c1f,
	0x89400005, 0xfffffffd, 0xe8208000, 0x10006f04, 0x00000000, 0xe8208000,
	0x10006b34, 0x00000000, 0xe8208000, 0x100063e0, 0x00000002, 0x81011401,
	0xd8205e04, 0x17c07c1f, 0x1a10001f, 0x10006918, 0x81012001, 0xb1052081,
	0xb1003081, 0xd8205e04, 0x17c07c1f, 0x1a40001f, 0x1000621c, 0x1a80001f,
	0x1000626c, 0xc2400000, 0x17c07c1f, 0x89400005, 0xfffffffb, 0xe8208000,
	0x10006f08, 0x00000000, 0xe8208000, 0x10006b38, 0x00000000, 0xe8208000,
	0x100063e0, 0x00000004, 0x81019401, 0xd8206164, 0x17c07c1f, 0x1a10001f,
	0x10006918, 0x8101a001, 0xb105a081, 0xb1003081, 0xd8206164, 0x17c07c1f,
	0x1a40001f, 0x10006220, 0x1a80001f, 0x10006274, 0xc2400000, 0x17c07c1f,
	0x89400005, 0xfffffff7, 0xe8208000, 0x10006f0c, 0x00000000, 0xe8208000,
	0x10006b3c, 0x00000000, 0xe8208000, 0x100063e0, 0x00000008, 0x81021401,
	0xd82064a4, 0x17c07c1f, 0x1a10001f, 0x10006918, 0x81022001, 0xb1062081,
	0xb1003081, 0xd82064a4, 0x17c07c1f, 0x1a40001f, 0x100062a0, 0x1280041f,
	0xc2400380, 0x17c07c1f, 0x89400005, 0xffffffef, 0xe8208000, 0x10006f10,
	0x00000000, 0xe8208000, 0x10006b40, 0x00000000, 0xe8208000, 0x100063e0,
	0x00000011, 0x81029401, 0xd82067e4, 0x17c07c1f, 0x1a10001f, 0x10006918,
	0x8102a001, 0xb106a081, 0xb1003081, 0xd82067e4, 0x17c07c1f, 0x1a40001f,
	0x100062a4, 0x1290841f, 0xc2400380, 0x17c07c1f, 0x89400005, 0xffffffdf,
	0xe8208000, 0x10006f14, 0x00000000, 0xe8208000, 0x10006b44, 0x00000000,
	0xe8208000, 0x100063e0, 0x00000022, 0x81031401, 0xd8206b24, 0x17c07c1f,
	0x1a10001f, 0x10006918, 0x81032001, 0xb1072081, 0xb1003081, 0xd8206b24,
	0x17c07c1f, 0x1a40001f, 0x100062a8, 0x1291041f, 0xc2400380, 0x17c07c1f,
	0x89400005, 0xffffffbf, 0xe8208000, 0x10006f18, 0x00000000, 0xe8208000,
	0x10006b48, 0x00000000, 0xe8208000, 0x100063e0, 0x00000044, 0x81039401,
	0xd8206e64, 0x17c07c1f, 0x1a10001f, 0x10006918, 0x8103a001, 0xb107a081,
	0xb1003081, 0xd8206e64, 0x17c07c1f, 0x1a40001f, 0x100062ac, 0x1291841f,
	0xc2400380, 0x17c07c1f, 0x89400005, 0xffffff7f, 0xe8208000, 0x10006f1c,
	0x00000000, 0xe8208000, 0x10006b4c, 0x00000000, 0xe8208000, 0x100063e0,
	0x00000088, 0xd00041c0, 0x17c07c1f, 0xe8208000, 0x10006600, 0x00000000,
	0x1ac0001f, 0x55aa55aa, 0x1940001f, 0xaa55aa55, 0x1b80001f, 0x00001000,
	0xf0000000, 0x17c07c1f
};
static struct pcm_desc mcdi_pcm = {
	.version	= "pcm_mcdi_v0.2_20140224-exp42-20us",
	.base		= mcdi_binary,
	.size		= 896,
	.sess		= 2,
	.replace	= 0,
};

/**********************************************************
 * E2 PCM code for MCDI
 **********************************************************/
static const u32 mcdi_binary_e2[] = {
	0x1212841f, 0xe2e00036, 0x1380201f, 0xe2e0003e, 0x1380201f, 0xe2e0002e,
	0x1380201f, 0xe2e0002c, 0x1380201f, 0xe2a00000, 0x1b80001f, 0x20000080,
	0xe2e0006c, 0xe2e0004c, 0x1b80001f, 0x20000020, 0xe2e0004d, 0xf0000000,
	0x17c07c1f, 0xe2e0006d, 0xe2e0002d, 0xe2a00001, 0x1b80001f, 0x20000080,
	0xe2e0002f, 0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0x1212841f,
	0xe2e00036, 0x1380201f, 0xe2e0003e, 0xe8208000, 0x10001008, 0x00000160,
	0x1380201f, 0xe2e0002e, 0x1380201f, 0x1a00001f, 0x100062b4, 0x1910001f,
	0x100062b4, 0x81322804, 0xe2000004, 0x81202804, 0xe2000004, 0x1b80001f,
	0x20000034, 0x1910001f, 0x100062b4, 0x81142804, 0xd8000624, 0x17c07c1f,
	0xe2e0006e, 0xe2e0004e, 0xe2e0004c, 0x1b80001f, 0x20000020, 0xe2e0004d,
	0xe8208000, 0x10001008, 0x00000000, 0xf0000000, 0x17c07c1f, 0xe8208000,
	0x10001008, 0x00000160, 0xe2e0006d, 0xe2e0002d, 0x1a00001f, 0x100062b4,
	0x1910001f, 0x100062b4, 0xa1002804, 0xe2000004, 0xa1122804, 0xe2000004,
	0x1b80001f, 0x20000080, 0x1910001f, 0x100062b4, 0x81142804, 0xd8200a04,
	0x17c07c1f, 0xe2e0002c, 0xe2e0002e, 0xe2e0003e, 0xe2e00032, 0xe8208000,
	0x10001008, 0x00000000, 0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
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
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0x11407c1f, 0xe8208000,
	0x10006310, 0x0b160008, 0x1900001f, 0x000f7bde, 0x1a00001f, 0x10200268,
	0xe2000004, 0xe8208000, 0x10006600, 0x00000000, 0x1b00001f, 0x21000001,
	0x1b80001f, 0xd0010000, 0x69200006, 0xbeefbeef, 0xd8204604, 0x17c07c1f,
	0x1910001f, 0x10006358, 0x810b1001, 0xd80042c4, 0x17c07c1f, 0x1980001f,
	0xdeaddead, 0x69200006, 0xabcdabcd, 0xd82043a4, 0x17c07c1f, 0x88900001,
	0x10006814, 0x1910001f, 0x10006400, 0x81271002, 0x1880001f, 0x10006600,
	0xe0800004, 0x1910001f, 0x10006358, 0x810b1001, 0xd8004524, 0x17c07c1f,
	0x1980001f, 0x12345678, 0x60a07c05, 0x89100002, 0x10006600, 0x80801001,
	0xd8007902, 0x17c07c1f, 0x1a10001f, 0x10006720, 0x82002001, 0x82201408,
	0xd8204908, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x82002001, 0xd80048e8,
	0x17c07c1f, 0x1a40001f, 0x10006200, 0x1a80001f, 0x1000625c, 0xc2400260,
	0x17c07c1f, 0xa1400405, 0x1a10001f, 0x10006720, 0x8200a001, 0x82209408,
	0xd8204b48, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x8200a001, 0xd8004b28,
	0x17c07c1f, 0x1a40001f, 0x10006218, 0x1a80001f, 0x10006264, 0xc2400260,
	0x17c07c1f, 0xa1508405, 0x1a10001f, 0x10006720, 0x82012001, 0x82211408,
	0xd8204d88, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x82012001, 0xd8004d68,
	0x17c07c1f, 0x1a40001f, 0x1000621c, 0x1a80001f, 0x1000626c, 0xc2400260,
	0x17c07c1f, 0xa1510405, 0x1a10001f, 0x10006720, 0x8201a001, 0x82219408,
	0xd8204fc8, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x8201a001, 0xd8004fa8,
	0x17c07c1f, 0x1a40001f, 0x10006220, 0x1a80001f, 0x10006274, 0xc2400260,
	0x17c07c1f, 0xa1518405, 0x1a10001f, 0x10006720, 0x82022001, 0x82221408,
	0xd82051e8, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x82022001, 0xd80051c8,
	0x17c07c1f, 0x1a40001f, 0x100062a0, 0x1280041f, 0xc2400820, 0x17c07c1f,
	0xa1520405, 0x1a10001f, 0x10006720, 0x8202a001, 0x82229408, 0xd8205408,
	0x17c07c1f, 0x1a10001f, 0x10006b00, 0x8202a001, 0xd80053e8, 0x17c07c1f,
	0x1a40001f, 0x100062a4, 0x1290841f, 0xc2400820, 0x17c07c1f, 0xa1528405,
	0x1a10001f, 0x10006720, 0x82032001, 0x82231408, 0xd8205628, 0x17c07c1f,
	0x1a10001f, 0x10006b00, 0x82032001, 0xd8005608, 0x17c07c1f, 0x1a40001f,
	0x100062a8, 0x1291041f, 0xc2400820, 0x17c07c1f, 0xa1530405, 0x1a10001f,
	0x10006720, 0x8203a001, 0x82239408, 0xd8205848, 0x17c07c1f, 0x1a10001f,
	0x10006b00, 0x8203a001, 0xd8005828, 0x17c07c1f, 0x1a40001f, 0x100062ac,
	0x1291841f, 0xc2400820, 0x17c07c1f, 0xa1538405, 0x1b80001f, 0x20000208,
	0x1a10001f, 0x10006b00, 0x82042001, 0xd80078c8, 0x17c07c1f, 0x81001401,
	0xd8205d44, 0x17c07c1f, 0x1a10001f, 0x10006918, 0x81002001, 0xb1042081,
	0xb1003081, 0xb10c3081, 0xd8205d44, 0x17c07c1f, 0x1a10001f, 0x10006b00,
	0x82002001, 0xd8005be8, 0x17c07c1f, 0x1a40001f, 0x10006200, 0x1a80001f,
	0x1000625c, 0xc2400000, 0x17c07c1f, 0x89400005, 0xfffffffe, 0xe8208000,
	0x10006f00, 0x00000000, 0xe8208000, 0x10006b30, 0x00000000, 0xe8208000,
	0x100063e0, 0x00000001, 0x81009401, 0xd8206144, 0x17c07c1f, 0x1a10001f,
	0x10006918, 0x8100a001, 0xb104a081, 0xb1003081, 0xd8206144, 0x17c07c1f,
	0x1a10001f, 0x10006b00, 0x8200a001, 0xd8005fe8, 0x17c07c1f, 0x1a40001f,
	0x10006218, 0x1a80001f, 0x10006264, 0xc2400000, 0x17c07c1f, 0x89400005,
	0xfffffffd, 0xe8208000, 0x10006f04, 0x00000000, 0xe8208000, 0x10006b34,
	0x00000000, 0xe8208000, 0x100063e0, 0x00000002, 0x81011401, 0xd8206544,
	0x17c07c1f, 0x1a10001f, 0x10006918, 0x81012001, 0xb1052081, 0xb1003081,
	0xd8206544, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x82012001, 0xd80063e8,
	0x17c07c1f, 0x1a40001f, 0x1000621c, 0x1a80001f, 0x1000626c, 0xc2400000,
	0x17c07c1f, 0x89400005, 0xfffffffb, 0xe8208000, 0x10006f08, 0x00000000,
	0xe8208000, 0x10006b38, 0x00000000, 0xe8208000, 0x100063e0, 0x00000004,
	0x81019401, 0xd8206944, 0x17c07c1f, 0x1a10001f, 0x10006918, 0x8101a001,
	0xb105a081, 0xb1003081, 0xd8206944, 0x17c07c1f, 0x1a10001f, 0x10006b00,
	0x8201a001, 0xd80067e8, 0x17c07c1f, 0x1a40001f, 0x10006220, 0x1a80001f,
	0x10006274, 0xc2400000, 0x17c07c1f, 0x89400005, 0xfffffff7, 0xe8208000,
	0x10006f0c, 0x00000000, 0xe8208000, 0x10006b3c, 0x00000000, 0xe8208000,
	0x100063e0, 0x00000008, 0x81021401, 0xd8206d24, 0x17c07c1f, 0x1a10001f,
	0x10006918, 0x81022001, 0xb1062081, 0xb1003081, 0xd8206d24, 0x17c07c1f,
	0x1a10001f, 0x10006b00, 0x82022001, 0xd8006bc8, 0x17c07c1f, 0x1a40001f,
	0x100062a0, 0x1280041f, 0xc24003a0, 0x17c07c1f, 0x89400005, 0xffffffef,
	0xe8208000, 0x10006f10, 0x00000000, 0xe8208000, 0x10006b40, 0x00000000,
	0xe8208000, 0x100063e0, 0x00000010, 0x81029401, 0xd8207104, 0x17c07c1f,
	0x1a10001f, 0x10006918, 0x8102a001, 0xb106a081, 0xb1003081, 0xd8207104,
	0x17c07c1f, 0x1a10001f, 0x10006b00, 0x8202a001, 0xd8006fa8, 0x17c07c1f,
	0x1a40001f, 0x100062a4, 0x1290841f, 0xc24003a0, 0x17c07c1f, 0x89400005,
	0xffffffdf, 0xe8208000, 0x10006f14, 0x00000000, 0xe8208000, 0x10006b44,
	0x00000000, 0xe8208000, 0x100063e0, 0x00000020, 0x81031401, 0xd82074e4,
	0x17c07c1f, 0x1a10001f, 0x10006918, 0x81032001, 0xb1072081, 0xb1003081,
	0xd82074e4, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x82032001, 0xd8007388,
	0x17c07c1f, 0x1a40001f, 0x100062a8, 0x1291041f, 0xc24003a0, 0x17c07c1f,
	0x89400005, 0xffffffbf, 0xe8208000, 0x10006f18, 0x00000000, 0xe8208000,
	0x10006b48, 0x00000000, 0xe8208000, 0x100063e0, 0x00000040, 0x81039401,
	0xd82078c4, 0x17c07c1f, 0x1a10001f, 0x10006918, 0x8103a001, 0xb107a081,
	0xb1003081, 0xd82078c4, 0x17c07c1f, 0x1a10001f, 0x10006b00, 0x8203a001,
	0xd8007768, 0x17c07c1f, 0x1a40001f, 0x100062ac, 0x1291841f, 0xc24003a0,
	0x17c07c1f, 0x89400005, 0xffffff7f, 0xe8208000, 0x10006f1c, 0x00000000,
	0xe8208000, 0x10006b4c, 0x00000000, 0xe8208000, 0x100063e0, 0x00000080,
	0xd00041c0, 0x17c07c1f, 0xe8208000, 0x10006600, 0x00000000, 0xe8208000,
	0x10006b00, 0x00000000, 0x1ac0001f, 0x55aa55aa, 0x1940001f, 0xaa55aa55,
	0x1b80001f, 0x00001000, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc mcdi_pcm_e2 = {
	.version	= "pcm_mcdi_v0.5_20140721",
	.base		= mcdi_binary_e2,
	.size		= 982,
	.sess		= 2,
	.replace	= 0,
};

static struct pwr_ctrl mcdi_ctrl = {
	.wake_src		= WAKE_SRC_FOR_MCDI,
	.wake_src_md32		= WAKE_SRC_FOR_MD32,
	.wfi_op			= WFI_OP_OR,
	.mcusys_idle_mask	= 1,	
	.ca7top_idle_mask	= 1,
	.ca15top_idle_mask	= 1,
	.md1_req_mask		= 1,
	.md2_req_mask		= 1,
	.disp_req_mask		= 1,
	.mfg_req_mask		= 1,
	.md32_req_mask		= 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask		= 1,
#endif
};

struct spm_lp_scen __spm_mcdi = {
	.pcmdesc	= &mcdi_pcm,
	.pwrctrl	= &mcdi_ctrl,
};
static unsigned int spm_mcdi_lock = 0;//offload MCDI
static bool SPM_MCDI_isKICK = 0;
#if SPM_AEE_RR_REC
unsigned int *p_is_mcdi_wfi;
#else
unsigned int g_is_mcdi_wfi=0;
#endif
unsigned int g_SPM_MCDI_Abnormal_WakeUp = 0;

DEFINE_SPINLOCK(__spm_mcdi_lock);

static void spm_mcdi_cpu_wake_up_event(bool wake_up_event, bool disable_dormant_power)
{
    unsigned long flags;

    if(((spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT)&0x1)==1)&&((spm_read(SPM_CLK_CON)&CC_DISABLE_DORM_PWR)==0))
    {
    	/*MCDI is offload?*/
        spm_idle_ver("spm_mcdi_cpu_wake_up_event: SPM_SLEEP_CPU_WAKEUP_EVENT:%x, SPM_CLK_CON %x",spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT),spm_read(SPM_CLK_CON));
        return;
    }
    //Inform SPM that CPU wants to program CPU_WAKEUP_EVENT and DISABLE_CPU_DROM
    spm_write(SPM_PCM_REG_DATA_INI, 0xbeefbeef);
    spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R6);
    spm_write(SPM_PCM_PWR_IO_EN, 0);

    //Wait SPM's response, can't use sleep api
    while(spm_read(SPM_PCM_REG6_DATA)!=0xdeaddead);    

    if(disable_dormant_power)
    {
        spm_write(SPM_CLK_CON,spm_read(SPM_CLK_CON)|CC_DISABLE_DORM_PWR);
        while(spm_read(SPM_CLK_CON)!=(spm_read(SPM_CLK_CON)|CC_DISABLE_DORM_PWR));
        
    }
    else
    {
        spm_write(SPM_CLK_CON,spm_read(SPM_CLK_CON)&~CC_DISABLE_DORM_PWR);
        while(spm_read(SPM_CLK_CON)!=(spm_read(SPM_CLK_CON)&~CC_DISABLE_DORM_PWR));
    }

    spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, wake_up_event);

    while(spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT)!=wake_up_event);

    //Inform SPM to see updated setting
    spm_write(SPM_PCM_REG_DATA_INI, 0xabcdabcd);
    spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R6);
    spm_write(SPM_PCM_PWR_IO_EN, 0);

    

    while(spm_read(SPM_PCM_REG6_DATA)!=0x12345678);

    /*END OF sequence*/
    spm_write(SPM_PCM_REG_DATA_INI, 0x0);
    spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R6);
    spm_write(SPM_PCM_PWR_IO_EN, 0);

    //workaround for force offload mcdi f/w
    //if((wake_up_event==1)&&(disable_dormant_power==0))
    //    spm_write(SPM_APMCU_PWRCTL, 1);   
}

wake_reason_t spm_go_to_mcdi(u32 spm_flags, u32 spm_data)
{
    unsigned long flags;
    wake_reason_t wr = WR_NONE;
    struct pcm_desc *pcmdesc = __spm_mcdi.pcmdesc;
    struct pwr_ctrl *pwrctrl = __spm_mcdi.pwrctrl;
	
    spin_lock_irqsave(&__spm_mcdi_lock,flags);
    if(SPM_MCDI_isKICK!=0)
    {
        spin_unlock_irqrestore(&__spm_mcdi_lock,flags);
        return;
    }

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);    
#if SPM_AEE_RR_REC
    *p_is_mcdi_wfi = 0;
#else
    g_is_mcdi_wfi = 0;
#endif
    __spm_reset_and_init_pcm(pcmdesc);

    __spm_kick_im_to_fetch(pcmdesc);

    __spm_set_power_control(pwrctrl);

    __spm_set_wakeup_event(pwrctrl);

    __spm_kick_pcm_to_run(pwrctrl);

    SPM_MCDI_isKICK = 1;
	
    spin_unlock_irqrestore(&__spm_mcdi_lock,flags);
    spm_idle_ver("spm_go_to_mcdi()\n");

    return wr;   
}


u32 spm_leave_MCDI(void)
{
    u32 spm_counter=0;
    unsigned long flags;

    spin_lock_irqsave(&__spm_mcdi_lock,flags);

    if(SPM_MCDI_isKICK==0)
    {
        spin_unlock_irqrestore(&__spm_mcdi_lock,flags);
        return;
    }
	
    SPM_MCDI_isKICK = 0;

    spm_mcdi_cpu_wake_up_event(1,1);      
#if SPM_AEE_RR_REC
    while(*p_is_mcdi_wfi!=0)//don't use sleep command(wfi/wfe)
#else
    while(g_is_mcdi_wfi!=0)//don't use sleep command(wfi/wfe)
#endif
    {
        if(spm_counter >= SPM_MCDI_CORE_MAX_EXIT_TIME)
        {
#if SPM_AEE_RR_REC
            spm_idle_ver("spm_leave_MCDI: g_is_mcdi_wfi:%x\n",*p_is_mcdi_wfi);
#else
            spm_idle_ver("spm_leave_MCDI: g_is_mcdi_wfi:%x\n",g_is_mcdi_wfi);
#endif
            spm_counter=0;
        }
        spm_counter++;
    }

    /*offload MCDI F/W*/
    spm_mcdi_cpu_wake_up_event(1,0);

    // polling SPM_SLEEP_ISR_STATUS ===========================
    spm_counter = 0;

    //Polling MCDI return
    while((spm_read(SPM_PCM_REG11_DATA))!=0x55AA55AA)//or R5=0xAA55AA55
    {
        if(spm_counter >= SPM_MCDI_CORE_MAX_EXIT_TIME)
        {
            spm_idle_ver("Polling MCDI return: SPM_PCM_REG11_DATA:0x%x\n",spm_read(SPM_PCM_REG11_DATA));
            spm_idle_ver("SPM_PCM_REG15_DATA:0x%x\n",spm_read(SPM_PCM_REG15_DATA));
            spm_idle_ver("SPM_PCM_REG6_DATA:0x%x\n",spm_read(SPM_PCM_REG6_DATA));
            spm_idle_ver("SPM_PCM_REG5_DATA:0x%x\n",spm_read(SPM_PCM_REG5_DATA));
            spm_idle_ver("SPM_APMCU_PWRCTL:0x%x\n",spm_read(SPM_APMCU_PWRCTL));
            spm_idle_ver("SPM_CLK_CON:0x%x\n",spm_read(SPM_CLK_CON));
            spm_idle_ver("SPM_SLEEP_CPU_WAKEUP_EVENT:0x%x\n",spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT));
            
            spm_counter = 0;
        }
        spm_counter++;
    }

    __spm_clean_after_wakeup();

    spin_unlock_irqrestore(&__spm_mcdi_lock,flags);

    spm_idle_ver("spm_leave_MCDI : OK\n");
    return 0; 
}

extern void mcidle_before_wfi(int cpu);
extern void mcidle_after_wfi(int cpu);
extern int mcdi_xgpt_wakeup_cnt[8];
#if SPM_AEE_RR_REC
extern unsigned int *aee_rr_rec_mcdi_wfi(void);
#endif
static void spm_mcdi_wfi_sel_enter(int core_id)
{
    int core_id_val = core_id;
    CHIP_SW_VER ver=mt_get_chip_sw_ver();

#if SPM_CTRL_BIG_CPU
    if(CHIP_SW_VER_02>ver)
        core_id_val+=4;
#endif
    /*SPM WFI Select by core number*/
    switch (core_id_val)
    {
        case 0 : 
            spm_write(SPM_CA7_CPU0_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA7_WFI0_EN, 1); 
            
            break;
        case 1 : 
            spm_write(SPM_CA7_CPU1_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA7_WFI1_EN, 1);             
            break;                     
        case 2 : 
            spm_write(SPM_CA7_CPU2_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA7_WFI2_EN, 1);             
            break;                     
        case 3 : 
            spm_write(SPM_CA7_CPU3_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA7_WFI3_EN, 1);             
            break;
        case 4 : 
            spm_write(SPM_CA15_CPU0_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA15_WFI0_EN, 1);             
            break;
        case 5 : 
            spm_write(SPM_CA15_CPU1_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA15_WFI1_EN, 1);             
            break;                     
        case 6 : 
            spm_write(SPM_CA15_CPU2_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA15_WFI2_EN, 1);            
            break;                     
        case 7 : 
            spm_write(SPM_CA15_CPU3_IRQ_MASK, 1);
            spm_write(SPM_SLEEP_CA15_WFI3_EN, 1);            
            break;         
        default : break;
    }  
	

}

static void spm_mcdi_wfi_sel_leave(int core_id)
{

    int core_id_val = core_id;
    CHIP_SW_VER ver=mt_get_chip_sw_ver();
	
#if SPM_CTRL_BIG_CPU
    if(CHIP_SW_VER_02>ver)
        core_id_val+=4;
#endif

    /*SPM WFI Select by core number*/
    switch (core_id_val)
    {
        case 0 : 
            spm_write(SPM_SLEEP_CA7_WFI0_EN, 0); 
            spm_write(SPM_CA7_CPU0_IRQ_MASK, 0);
            break;
        case 1 : 
            spm_write(SPM_SLEEP_CA7_WFI1_EN, 0); 
            spm_write(SPM_CA7_CPU1_IRQ_MASK, 0);
            break;                     
        case 2 : 
            spm_write(SPM_SLEEP_CA7_WFI2_EN, 0); 
            spm_write(SPM_CA7_CPU2_IRQ_MASK, 0);
            break;                     
        case 3 : 
            spm_write(SPM_SLEEP_CA7_WFI3_EN, 0); 
            spm_write(SPM_CA7_CPU3_IRQ_MASK, 0);
            break;
        case 4 : 
            spm_write(SPM_SLEEP_CA15_WFI0_EN, 0); 
            spm_write(SPM_CA15_CPU0_IRQ_MASK, 0);
            break;
        case 5 : 
            spm_write(SPM_SLEEP_CA15_WFI1_EN, 0); 
            spm_write(SPM_CA15_CPU1_IRQ_MASK, 0);
            break;                     
        case 6 : 
            spm_write(SPM_SLEEP_CA15_WFI2_EN, 0); 
            spm_write(SPM_CA15_CPU2_IRQ_MASK, 0);
            break;                     
        case 7 : 
            spm_write(SPM_SLEEP_CA15_WFI3_EN, 0);
            spm_write(SPM_CA15_CPU3_IRQ_MASK, 0);
            break;         
        default : break;
    }  

}

bool spm_is_cpu_irq_occur(int core_id)
{	
    bool ret = 0;

    //check COREn IRQ	
    if(spm_read(SPM_SLEEP_WAKEUP_MISC)&(1<<(core_id)))		
        ret = 1;		
    //check COREn FIQ	
    if(spm_read(SPM_SLEEP_WAKEUP_MISC)&(1<<(core_id+8)))		
        ret = 1;

    return ret;
}

//#define REMOVE_MCDI_TEST
extern bool is_in_cpufreq;
bool spm_mcdi_can_enter(void)
{
    bool ret = 1;
    unsigned long flags;
    /*check is MCDI kick*/
    if((SPM_MCDI_isKICK==0)||((spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT)&0x1)==1)||((spm_read(SPM_APMCU_PWRCTL)&0x1)==1)||(is_in_cpufreq==1)||(atomic_read(&is_in_hotplug) >= 1))
        ret = 0;
    return ret;

}

bool spm_mcdi_wfi(int core_id)
{   
    bool ret = 0;
    unsigned long flags;
    int dmnt_ret = 0;
    CHIP_SW_VER ver=mt_get_chip_sw_ver();
    int mcdi_core_id = core_id;

#if SPM_CTRL_BIG_CPU
    if(CHIP_SW_VER_02>ver)
        mcdi_core_id=core_id+4;//E1 Bigcore
#endif
    
    //printk("spm_mcdi_wfi\n");
    spin_lock_irqsave(&__spm_lock, flags);
#if SPM_AEE_RR_REC
    *p_is_mcdi_wfi=( *p_is_mcdi_wfi | (1<<core_id) );
#else
    g_is_mcdi_wfi=( g_is_mcdi_wfi | (1<<core_id) );
#endif
    spin_unlock_irqrestore(&__spm_lock, flags);

    if(spm_mcdi_can_enter()==0)
    {
        spin_lock_irqsave(&__spm_lock, flags);
#if SPM_AEE_RR_REC
        *p_is_mcdi_wfi=( *p_is_mcdi_wfi &~ (1<<core_id) );
#else
        g_is_mcdi_wfi=( g_is_mcdi_wfi &~ (1<<core_id) );
#endif
        spin_unlock_irqrestore(&__spm_lock, flags); 
        //spm_idle_ver("SPM_SLEEP_CPU_WAKEUP_EVENT:%x, SPM_APMCU_PWRCTL %x",spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT),spm_read(SPM_APMCU_PWRCTL));
        return ret;    
    }

    if((spm_read(SPM_PCM_RESERVE)&(1<<mcdi_core_id))==0)      
    {
        if(spm_is_cpu_irq_occur(mcdi_core_id)==0)
        {
            /*core wfi_sel & cpu mask*/
            spm_mcdi_wfi_sel_enter(core_id);  

            /*sync core1~n local timer to XGPT*/
            mcidle_before_wfi(core_id);

            dmnt_ret=mt_cpu_dormant_interruptible(CPU_MCDI_MODE);	    

            if(dmnt_ret==MT_CPU_DORMANT_RESET)
            {
                ret = 1;
	        
                /*check if MCDI abort by unkonw IRQ*/
                while((spm_read(SPM_SLEEP_ISR_STATUS)&(1<<(mcdi_core_id+4)))==0)
                {
                    spin_lock_irqsave(&__spm_lock, flags);
                    g_SPM_MCDI_Abnormal_WakeUp|=(1<<core_id);
                    spin_unlock_irqrestore(&__spm_lock, flags);
                }

            }
            else if(dmnt_ret==MT_CPU_DORMANT_ABORT)
            {
                ret = 0;
            }

            /*clear core wfi_sel & cpu unmask*/
            spm_mcdi_wfi_sel_leave(core_id); 		

            mcidle_after_wfi(core_id);    
            spin_lock_irqsave(&__spm_lock, flags);

            /*Clear SPM SW IRQ*/
            spm_write(SPM_PCM_SW_INT_CLEAR, (0x1<<core_id) );    /* PCM_SWINT_3 */

#if SPM_CTRL_BIG_CPU
            if(CHIP_SW_VER_02>ver)
				spm_write(SPM_PCM_SW_INT_CLEAR, (0x1<<(core_id+4)) );	 /* PCM_SWINT_3 */	
#endif
            spin_unlock_irqrestore(&__spm_lock, flags);

        }
    }
    else
    {
        /*core wfi_sel & cpu mask*/
        spm_mcdi_wfi_sel_enter(core_id);
        /*sync core1~n local timer to XGPT*/
        mcidle_before_wfi(core_id);
        
        wfi_with_sync();

    	mcidle_after_wfi(core_id);         
        /*clear core wfi_sel & cpu unmask*/
        spm_mcdi_wfi_sel_leave(core_id);  
#if SPM_CTRL_BIG_CPU
        if(CHIP_SW_VER_02>ver)
            spm_write(SPM_PCM_SW_INT_CLEAR, (0x1<<(core_id+4)) );	 /* PCM_SWINT_3 */	
#endif
        /*Clear SPM SW IRQ*/
        spm_write(SPM_PCM_SW_INT_CLEAR, (0x1<<core_id) );    /* PCM_SWINT_3 */
       
        ret = 1;
    }
        

    spin_lock_irqsave(&__spm_lock, flags);
#if SPM_AEE_RR_REC
    *p_is_mcdi_wfi=( *p_is_mcdi_wfi &~ (1<<core_id) );
#else
    g_is_mcdi_wfi=( g_is_mcdi_wfi &~ (1<<core_id) );
#endif
    spin_unlock_irqrestore(&__spm_lock, flags);

    return ret;

}    


// ==============================================================================
static int spm_mcdi_probe(struct platform_device *pdev)
{    
    return 0;
}
static void spm_mcdi_early_suspend(struct early_suspend *h) 
{
    spm_mcdi_switch_on_off(SPM_MCDI_EARLY_SUSPEND,0);
}

static void spm_mcdi_late_resume(struct early_suspend *h) 
{
    spm_mcdi_switch_on_off(SPM_MCDI_EARLY_SUSPEND,1);
}



static struct platform_driver mtk_spm_mcdi_driver = {
    .remove     = NULL,
    .shutdown   = NULL,
    .probe      = spm_mcdi_probe,
    .suspend	= NULL,
    .resume     = NULL,
    .driver     = {
        .name = "mtk-spm-mcdi",
    },
};

static struct early_suspend mtk_spm_mcdi_early_suspend_driver =
{
    .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 251,
    .suspend = spm_mcdi_early_suspend,
    .resume  = spm_mcdi_late_resume,
};

static void spm_set_mcdi_pcm_ver(void)
{
    CHIP_SW_VER ver=mt_get_chip_sw_ver();

    if(CHIP_SW_VER_02<=ver)
    {
    	/*E2 Deepidle FW*/
        __spm_mcdi.pcmdesc = &mcdi_pcm_e2;
    }	
}

int spm_mcdi_init(void)
{
    int mcdi_err = 0;
#if SPM_AEE_RR_REC
    p_is_mcdi_wfi = aee_rr_rec_mcdi_wfi();

    *p_is_mcdi_wfi = 0;
#endif

    mcdi_err = platform_driver_register(&mtk_spm_mcdi_driver);
    
    if (mcdi_err)
    {
        spm_idle_ver("spm mcdi driver callback register failed..\n");
        return mcdi_err;
    }


    spm_idle_ver("spm mcdi driver callback register OK..\n");

    register_early_suspend(&mtk_spm_mcdi_early_suspend_driver);

    spm_idle_ver("spm mcdi driver early suspend callback register OK..\n");

    spm_set_mcdi_pcm_ver();
    
    return 0;

}

static void __exit spm_mcdi_exit(void)
{
    spm_idle_ver("Exit SPM-MCDI\n\r");
}

void spm_mcdi_wakeup_all_cores(void)
{
    unsigned int spm_counter = 0;
    unsigned long flags;

    spin_lock_irqsave(&__spm_mcdi_lock,flags);
    if(SPM_MCDI_isKICK==0)
    {
        spin_unlock_irqrestore(&__spm_mcdi_lock,flags);
        return;
    }

    /*disable MCDI Dormant*/
    //spm_write(SPM_CLK_CON,spm_read(SPM_CLK_CON)|CC_DISABLE_DORM_PWR);
    
    // trigger cpu wake up event
    //spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0x1);
    spm_mcdi_cpu_wake_up_event(1,1);
#if SPM_AEE_RR_REC
    while(*p_is_mcdi_wfi!=0)
#else
    while(g_is_mcdi_wfi!=0)
#endif		
    {
        if(spm_counter >= SPM_MCDI_CORE_MAX_EXIT_TIME)
        {
#if SPM_AEE_RR_REC
            spm_idle_ver("g_is_mcdi_wfi:%x\n",*p_is_mcdi_wfi);
#else
            spm_idle_ver("g_is_mcdi_wfi:%x\n",g_is_mcdi_wfi);
#endif
            spm_counter=0;
        }
        spm_counter++;
    }
    spm_counter = 0;
    // trigger cpu wake up event
    //spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0x0);  
    spm_mcdi_cpu_wake_up_event(0, 0);
    spin_unlock_irqrestore(&__spm_mcdi_lock,flags);
    /*disable MCDI Dormant*/
    //spm_write(SPM_CLK_CON,spm_read(SPM_CLK_CON)&~CC_DISABLE_DORM_PWR);
    
}

static void spm_mcdi_enable(enum spm_mcdi_lock_id id, int mcdi_en)
{
    unsigned long flags;
    if(mcdi_en)
    {
        spin_lock_irqsave(&__spm_lock,flags);
        spm_mcdi_lock&=~(1<<id);
        spin_unlock_irqrestore(&__spm_lock,flags);
    }else
    {
        spin_lock_irqsave(&__spm_lock,flags);
        spm_mcdi_lock|=(1<<id);
        spin_unlock_irqrestore(&__spm_lock,flags);	
    }
}

static unsigned int spm_mcdi_is_disable(void)
{
    return spm_mcdi_lock;
}


void spm_mcdi_switch_on_off(enum spm_mcdi_lock_id id, int mcdi_en)
{
    unsigned long flags;
    spm_mcdi_enable(id,mcdi_en);
    if(mcdi_en)
    {
        if(spm_mcdi_is_disable())
            return;          
        spm_go_to_mcdi(0, 0);
    }else
    {
        spm_leave_MCDI(); 
    }
}

MODULE_DESCRIPTION("SPM-MCDI Driver v0.1");
