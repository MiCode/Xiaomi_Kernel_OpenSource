/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */



/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
/* #include <mach/irqs.h> */
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
/* #include <mach/mt_reg_base.h> */
#include <asm/div64.h>
#include <mt-plat/aee.h>
/* #include <mach/pmic_mt6325_sw.h> */
/* #include <mach/upmu_common.h> */
/* #include <mach/upmu_hw.h> */
/* #include <mach/mt_gpio.h> */
/* #include <mach/mt_typedefs.h> */
/* #include <mt-plat/upmu_common.h> */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
/* #include <asm/mach-types.h> */

#include "mt_soc_afe_common.h"
#include "mt_soc_afe_def.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_digital_type.h"


#ifdef DEBUG_AUDDRV
#define PRINTK_AUDDRV(format, args...) pr_debug(format, ##args)
#else
#define PRINTK_AUDDRV(format, args...)
#endif

/* mutex lock */
static DEFINE_MUTEX(afe_connection_mutex);

/**
* here define conenction table for input and output
*/
static const char mConnectionTable[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output]
	= {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18 */
	{  3,  3, -1,  3,  3,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1,  3,  1, -1, -1}, /* I00 */
	{  3,  3, -1,  3,  3, -1,  3, -1, -1, -1, -1, -1, -1, -1, -1,  1,  3, -1, -1}, /* I01 */
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I02 */
	{  1,  1, -1,  1,  1,  1, -1,  1, -1,  1, -1, -1, -1, -1, -1,  1,  1,  1, -1}, /* I03 */
	{  1,  1, -1,  1,  1, -1,  1, -1,  1, -1,  1, -1, -1, -1, -1,  1,  1, -1,  1}, /* I04 */
	{  3,  3, -1,  3,  3,  1, -1,  1, -1,  0, -1, -1, -1,  1,  1, -1, -1,  0, -1}, /* I05 */
	{  3,  3, -1,  3,  3, -1,  1, -1,  1, -1,  0, -1,  0,  1,  1, -1, -1, -1,  0}, /* I06 */
	{  3,  3, -1,  3,  3,  1, -1,  0, -1,  0, -1, -1, -1,  1,  1, -1, -1,  0, -1}, /* I07 */
	{  3,  3, -1,  3,  3, -1,  1, -1,  0, -1,  0, -1,  0,  1,  1, -1, -1, -1,  0}, /* I08 */
	{  1,  1, -1,  1,  1,  1, -1, -1, -1, -1, -1, -1,  1, -1, -1,  1,  1, -1, -1}, /* I09 */
	{  3, -1, -1,  3, -1,  0, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1, -1}, /* I10 */
	{ -1,  3, -1, -1,  3, -1,  0, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1}, /* I11 */
	{  3, -1, -1,  3, -1,  0, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1, -1}, /* I12 */
	{ -1,  3, -1, -1,  3, -1,  0, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1}, /* I13 */
	{  1,  1, -1,  1,  1,  1, -1, -1, -1, -1, -1, -1,  1, -1, -1,  1,  1, -1, -1}, /* I14 */
	/* {  3,  3,  3,  3,  3,  3, -1, -1, -1,  1, -1, -1, -1, -1, -1,  3,  1, -1, -1}, // I15 */
	/* {  3,  3,  3,  3,  3, -1,  3, -1, -1, -1,  1, -1, -1, -1, -1,  1,  3, -1, -1}, // I16 */
};


/**
* connection bits of certain bits
*/
static const char mConnectionbits[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18 */
	{  0, 16, -1, 16,  0, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 22, -1, -1}, /* I00 */
	{  1, 17, -1, 17,  1, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, 17, 23, -1, -1}, /* I01 */
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I02 */
	{  3, 19, -1, 19,  3, 18, -1, 26, -1,  0, -1, -1, -1, -1, -1, 19, 25, 13, -1}, /* I03 */
	{  4, 20, -1, 20,  4, -1, 23, -1, 29, -1,  3, -1, -1, -1, -1, 20, 26, -1, 16}, /* I04 */
	{  5, 21, -1, 21,  5, 19, -1, 27, -1,  1, -1, -1, -1, 16, 20, -1, -1, 14, -1}, /* I05 */
	{  6, 22, -1, 22,  6, -1, 24, -1, 30, -1,  4, -1,  9, 17, 21, -1, -1, -1, 17}, /* I06 */
	{  7, 23, -1, 23,  7, 20, -1, 28, -1,  2, -1, -1, -1, 18, 22, -1, -1, 15, -1}, /* I07 */
	{  8, 24, -1, 24,  8, -1, 25, -1, 31, -1,  5, -1, 10, 19, 23, -1, -1, -1, 18}, /* I08 */
	{  9, 25, -1, 25,  9, 21, 27, -1, -1, -1, -1, -1, 11, -1, -1, 21, 27, -1, -1}, /* I09 */
	{  0, -1, -1,  8, -1, 12, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1}, /* I10 */
	{ -1,  2, -1, -1, 10, -1, 13, -1, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25}, /* I11 */
	{  0, -1, -1,  8, -1, 12, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1,  6, -1}, /* I12 */
	{ -1,  2, -1, -1, 10, -1, 13, -1, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,  7}, /* I13 */
	{ 12, 17, -1, 27,  0,  5, -1, -1, -1, -1, -1, -1, 12, -1, -1, 28,  1, -1, -1}, /* I14 */
	/* { 13, 18, 23, 28,  1,  6, -1, -1, -1, 10, -1, -1, -1, -1, -1, 29,  2, -1, -1}, // I15 */
	/* { 14, 19, 24, 29,  2, -1,  8, -1, -1, -1, 11, -1, -1, -1, -1, 30,  3, -1, -1}, // I16 */
};
/**
* connection shift bits of certain bits
*/
static const char mShiftConnectionbits[Soc_Aud_InterConnectionInput_Num_Input]
				[Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0   1   2   3   4   5   6    7  8   9  10  11  12  13  14  15  16  17  18 */
	{ 10, 26, -1, 26, 10, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, 31, -1, -1, -1}, /* I00 */
	{ 11, 27, -1, 27, 11, -1, 20, -1, -1, -1, -1, -1, -1, -1, -1, -1,  4, -1, -1}, /* I01 */
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I02 */
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I03 */
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I04 */
	{ 12, 28, -1, 28, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I05 */
	{ 13, 29, -1, 29, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I06 */
	{ 14, 30, -1, 30, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I07 */
	{ 15, 31, -1, 31, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I08 */
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I09 */
	{  1, -1, -1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I10 */
	{ -1,  3, -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I11 */
	{  1, -1, -1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I12 */
	{ -1,  3, -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I13 */
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, /* I14 */
	/* { 15, 20, 25, 30,  3,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0, -1, -1, -1}, // I15 */
	/* { 16, 21, 26, 31,  4, -1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1,  5, -1, -1}, // I16 */
};

/**
* connection of register
*/
static const short mConnectionReg[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0   1 2   3   4   5    6     7      8      9  10  11  12  13     14     15     16   17 18 */
	{0x20, 0x20, -1, 0x24, 0x28, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x438, 0x438,   -1,   -1}, /* I00 */
	{0x20, 0x20, -1, 0x24, 0x28, -1, 0x28, -1, -1, -1,  -1, -1, -1, -1,   -1, 0x438, 0x438,   -1,   -1}, /* I01 */
	{  -1, -1, -1, -1, -1,  -1,  -1, -1, -1, -1,  -1, -1,  -1, -1,   -1,   -1,   -1,   -1,   -1}, /* I02 */
	{0x20, 0x20, -1, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C,  -1, -1, -1, -1, -1, 0x438, 0x438, 0x30, -1},/*I03*/
	{0x20, 0x20, -1, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, -1, -1, -1, 0x438, 0x438, -1, 0x30},/*I04*/
	{0x20, 0x20, -1, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C,  -1, -1, -1, 0x420, 0x420, -1, -1, 0x30, -1},/*I05*/
	{0x20, 0x20, -1, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C, 0x420, 0x420, -1, -1, -1, 0x30},/*I06*/
	{0x20, 0x20, -1, 0x24, 0x28, 0x28, -1, 0x28,  -1, 0x2C,  -1, -1, -1, 0x420, 0x420, -1, -1, 0x30, -1},/*I07*/
	{0x20, 0x20, -1, 0x24, 0x28, -1, 0x28,  -1, 0x28, -1, 0x2C, -1, 0x2C, 0x420, 0x420, -1, -1, -1, 0x30},/*I08*/
	{0x20, 0x20, -1, 0x24, 0x28, 0x28, -1,  -1,  -1, -1, -1, -1, 0x2C,   -1, -1, 0x438, 0x438, -1, -1}, /*I09*/
	{0x420, -1, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1,  -1, -1,  -1,   -1, -1, -1,   -1, 0x420, -1},/* I10*/
	{   -1, 0x420, -1, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, 0x420}, /* I11*/
	{0x438, -1, -1, 0x438, -1, 0x438,   -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1,   -1, 0x440,   -1}, /* I12*/
	{   -1, 0x438, -1, -1, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, 0x440}, /*I13*/
	{ 0x2C, 0x2C, -1, 0x2C, 0x30, 0x30,  -1,   -1, -1, -1, -1, -1, 0x30, -1, -1, 0x438, 0x440, -1, -1}, /*I14*/
};


/**
* shift connection of register
*/
static const short mShiftConnectionReg[Soc_Aud_InterConnectionInput_Num_Input]
				[Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0      1      2      3      4      5     6  7  8  9 10 11 12 13 14    15    16 17 18 */
	{  0x20,  0x20,    -1,  0x24,  0x28, 0x30,  -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x438,   -1, -1, -1}, /* I00 */
	{  0x20,  0x20,    -1,  0x24,  0x28,  -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1,   -1, 0x440, -1, -1}, /* I01 */
	{    -1,    -1,    -1,    -1,    -1,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I02 */
	{    -1,    -1,    -1,    -1,    -1,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I03 */
	{    -1,    -1,    -1,    -1,    -1,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I04 */
	{  0x20,  0x20,    -1,  0x24,  0x28,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I05 */
	{  0x20,  0x20,    -1,  0x24,  0x28,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I06 */
	{  0x20,  0x20,    -1,  0x24,  0x28,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I07 */
	{  0x20,  0x20,    -1,  0x24,  0x28,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I08 */
	{    -1,    -1,    -1,    -1,    -1,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I09 */
	{ 0x420,    -1,    -1, 0x420,    -1,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I10 */
	{ 0x420, 0x420,    -1,    -1, 0x420,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I11 */
	{ 0x438,    -1,    -1, 0x438,    -1,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I12 */
	{    -1, 0x438,    -1,    -1, 0x438,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I13 */
	{    -1,    -1,    -1,    -1,    -1,  -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,   -1,   -1, -1, -1}, /* I14 */
};

/**
* connection state of register
*/
static char mConnectionState[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I00 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I01 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I02 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I03 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I04 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I05 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I06 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I07 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I08 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I09 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I10 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I11 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I12 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I13 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I14 */
	/* {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // I15 */
	/* {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}  // I16 */
};

static bool CheckBitsandReg(short regaddr, char bits)
{
	if (regaddr <= 0 || bits < 0) {
		pr_debug("regaddr = %x bits = %d\n", regaddr, bits);
		return false;
	}
	return true;
}
bool mt_afe_set_hdmi_connection(uint32_t ConnectionState, uint32_t Input, uint32_t Output)
{
	uint32_t input_index;
	uint32_t output_index;

	/* check if connection request is valid */
	if (Input < HDMI_INTER_CONN_INPUT_BASE || Input > HDMI_INTER_CONN_INPUT_MAX ||
	    Output < HDMI_INTER_CONN_OUTPUT_BASE || Output > HDMI_INTER_CONN_OUTPUT_MAX) {
		return false;
	}

	input_index = Input - HDMI_INTER_CONN_INPUT_BASE;
	output_index = Output - HDMI_INTER_CONN_OUTPUT_BASE;

	if (ConnectionState)
		mt_afe_set_reg(AFE_HDMI_CONN0, (input_index << (3 * output_index)), (0x7 << (3 * output_index)));
	else
		mt_afe_set_reg(AFE_HDMI_CONN0, 0x0, 0x3FFFFFFF);
	return true;
}

bool mt_afe_set_connection(uint32_t ConnectionState, uint32_t Input, uint32_t Output)/*SetConnectionState*/
{
	/* pr_debug("SetinputConnection ConnectionState = %d Input = %d Output = %d\n",
	   ConnectionState, Input, Output); */
	if ((mConnectionTable[Input][Output]) < 0)
		pr_warn("no connection mpConnectionTable[%d][%d] = %d\n", Input, Output,
			mConnectionTable[Input][Output]);
	else if ((mConnectionTable[Input][Output]) == 0)
		pr_warn("test only !! mpConnectionTable[%d][%d] = %d\n", Input, Output,
			mConnectionTable[Input][Output]);
	else {
		if (mConnectionTable[Input][Output]) {
			int connectionBits = 0;
			int connectReg = 0;

			switch (ConnectionState) {
			case Soc_Aud_InterCon_DisConnect:{
				/* pr_debug("nConnectionState = %d\n", ConnectionState); */
				if ((mConnectionState[Input][Output] & Soc_Aud_InterCon_Connection)
					== Soc_Aud_InterCon_Connection) {
					/* here to disconnect connect bits */
					connectionBits = mConnectionbits[Input][Output];
					connectReg = mConnectionReg[Input][Output];
					if (CheckBitsandReg(connectReg, connectionBits)) {
						mt_afe_set_reg(connectReg, 0 << connectionBits, 1 << connectionBits);
						mConnectionState[Input][Output] &= ~(Soc_Aud_InterCon_Connection);
					}
				}
				if ((mConnectionState[Input][Output] & Soc_Aud_InterCon_ConnectionShift)
					== Soc_Aud_InterCon_ConnectionShift) {
					/* here to disconnect connect shift bits */
					connectionBits = mShiftConnectionbits[Input][Output];
					connectReg = mShiftConnectionReg[Input][Output];
					if (CheckBitsandReg(connectReg, connectionBits)) {
						mt_afe_set_reg(connectReg, 0 << connectionBits, 1 << connectionBits);
						mConnectionState[Input][Output] &= ~(Soc_Aud_InterCon_ConnectionShift);
					}
				}
				break;
			}
			case Soc_Aud_InterCon_Connection:{
				/* pr_debug("nConnectionState = %d\n", ConnectionState); */
				/* here to disconnect connect shift bits */
				connectionBits = mConnectionbits[Input][Output];
				connectReg = mConnectionReg[Input][Output];
				if (CheckBitsandReg(connectReg, connectionBits)) {
					mt_afe_set_reg(connectReg, 1 << connectionBits, 1 << connectionBits);
					mConnectionState[Input][Output] |= Soc_Aud_InterCon_Connection;
				}
				break;
			}
			case Soc_Aud_InterCon_ConnectionShift:{
				/* pr_debug("nConnectionState = %d\n", ConnectionState); */
				if ((mConnectionTable[Input][Output] & Soc_Aud_InterCon_ConnectionShift)
					!= Soc_Aud_InterCon_ConnectionShift) {
					pr_warn("don't support shift opeartion\n");
					break;
				}
				connectionBits = mShiftConnectionbits[Input][Output];
				connectReg = mShiftConnectionReg[Input][Output];
				if (CheckBitsandReg(connectReg, connectionBits)) {
					mt_afe_set_reg(connectReg, 1 << connectionBits, 1 << connectionBits);
					mConnectionState[Input][Output] |= Soc_Aud_InterCon_ConnectionShift;
					}
				break;
			}
			default:
				pr_warn("no this state ConnectionState = %d\n", ConnectionState);
				break;
			}
		}
	}
	return true;
}
EXPORT_SYMBOL(mt_afe_set_connection);
