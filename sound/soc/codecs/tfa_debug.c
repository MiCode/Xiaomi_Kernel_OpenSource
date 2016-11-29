/*
 * Copyright (c), NXP Semiconductors
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * (C)NXP Semiconductors
 *
 * NXP reserves the right to make changes without notice at any time.
 * This code is distributed in the hope that it will be useful,
 * but NXP makes NO WARRANTY, expressed, implied or statutory, including but
 * not limited to any implied warranty of MERCHANTABILITY or FITNESS FOR ANY
 * PARTICULAR PURPOSE, or that the use will not infringe any third party patent,
 * copyright or trademark. NXP must not be liable for any loss or damage
 * arising from its use. (c) PLMA, NXP Semiconductors.
 */

#define pr_fmt(fmt) "%s(): " fmt, __func__
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include "tfa98xx-core.h"
#include "tfa98xx_regs.h"
#include "tfa_container.h"
#include "tfa_dsp.h"



struct tfa98xx_regs {
	int reg;
	int value;
};

/**
 *  register definition structure
 */
struct regdef {
	unsigned char offset; /**< subaddress offset */
	unsigned short pwronDefault;
			      /**< register contents after poweron */
	unsigned short pwronTestmask;
			      /**< mask of bits not test */
	char *name;	      /**< short register name */
};

struct regdef regdefs[] = {
	{ 0x00, 0x081d, 0xfeff, "statusreg"},
	{ 0x01, 0x0, 0x0, "batteryvoltage"},
	{ 0x02, 0x0, 0x0, "temperature"},
	{ 0x03, 0x0012, 0xffff, "revisionnumber"},
	{ 0x04, 0x888b, 0xffff, "i2sreg"},
	{ 0x05, 0x13aa, 0xffff, "bat_prot"},
	{ 0x06, 0x001f, 0xffff, "audio_ctr"},
	{ 0x07, 0x0fe6, 0xffff, "dcdcboost"},
	{ 0x08, 0x0800, 0x3fff, "spkr_calibration"},
	{ 0x09, 0x041d, 0xffff, "sys_ctrl"},
	{ 0x0a, 0x3ec3, 0x7fff, "i2s_sel_reg"},
	{ 0x40, 0x0, 0x00ff, "hide_unhide_key"},
	{ 0x41, 0x0, 0x0, "pwm_control"},
	{ 0x46, 0x0, 0x0, "currentsense1"},
	{ 0x47, 0x0, 0x0, "currentsense2"},
	{ 0x48, 0x0, 0x0, "currentsense3"},
	{ 0x49, 0x0, 0x0, "currentsense4"},
	{ 0x4c, 0x0, 0xffff, "abisttest"},
	{ 0x62, 0x0, 0, "mtp_copy"},
	{ 0x70, 0x0, 0xffff, "cf_controls"},
	{ 0x71, 0x0, 0, "cf_mad"},
	{ 0x72, 0x0, 0, "cf_mem"},
	{ 0x73, 0x00ff, 0xffff, "cf_status"},
	{ 0x80, 0x0, 0, "mtp"},
	{ 0x83, 0x0, 0, "mtp_re0"},
	{ 0xff, 0, 0, NULL}
};

#define MAXREGS ((sizeof(regdefs)/sizeof(struct regdef))-1)


#define TFA98XX_STATUSREG_ERRORS_SET_MSK (  \
		TFA98XX_STATUSREG_OCDS)


#define TFA98XX_STATUSREG_ERRORS_CLR_MSK (TFA98XX_STATUSREG_VDDS  |\
		TFA98XX_STATUSREG_UVDS  |  \
		TFA98XX_STATUSREG_OVDS  |  \
		TFA98XX_STATUSREG_OTDS)


#define RWTEST_REG TFA98XX_CF_MAD

#define MAX_STRING_DEBUG	256

TFA_NAMETABLE
/**
 * print all the bitfields of the register
 * @param fd output file
 * @param reg address
 * @param regval register value
 * @return 0 if at least 1 name was found
 */
int tfaRunBitfieldDump(unsigned char reg, unsigned short regval)
{
	union {
		u16 field;
		struct nxpTfaBfEnum Enum;
	} bfUni;

	int n = sizeof(TfaBfNames)/sizeof(struct TfaBfName);
	int havename = 0;

	do {
		bfUni.field = TfaBfNames[n].bfEnum;
		if (bfUni.Enum.address == reg) {
			pr_debug("%s:%d ", TfaBfNames[n].bfName, (regval >> bfUni.Enum.pos) & (1 << (bfUni.Enum.len)));
			havename = 1;
		}
	} while (n--);

	return !havename == 1;
}


int tfa98xxDiagRegisterDump(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	unsigned short regval;
	int i;


	for (i = 0; i < MAXREGS; i++) {

		regval = snd_soc_read(codec, regdefs[i].offset);
		pr_debug("0x%02x:0x%04x\n", regdefs[i].offset, regval);

	}
	return 0;
}
