/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "dbgprint.h"
#include "tfa_service.h"
#include "tfa_internal.h"
#include "tfa_container.h"
#include "tfa98xx_tfafieldnames.h"

 /* The CurrentSense4 registers are not in the datasheet */
#define TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF (1<<2)
#define TFA98XX_CURRENTSENSE4 0x49

/***********************************************************************************/
/* GLOBAL (Defaults)                                                               */
/***********************************************************************************/
static enum Tfa98xx_Error no_overload_function_available(struct tfa_device *tfa, int not_used)
{
	(void)tfa;
	(void)not_used;

	return Tfa98xx_Error_Ok;
}

static enum Tfa98xx_Error no_overload_function_available2(struct tfa_device *tfa)
{
	(void)tfa;

	return Tfa98xx_Error_Ok;
}

/* tfa98xx_dsp_system_stable
*  return: *ready = 1 when clocks are stable to allow DSP subsystem access
*/
static enum Tfa98xx_Error tfa_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short status;
	int value;

	/* check the contents of the STATUS register */
	value = TFA_READ_REG(tfa, AREFS);
	if (value < 0) {
		error = -value;
		*ready = 0;
		_ASSERT(error);		/* an error here can be fatal */
		return error;
	}
	status = (unsigned short)value;

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(tfa, AREFS, status) == 0)
		|| (TFA_GET_BF_VALUE(tfa, CLKS, status) == 0));

	return error;
}

/* tfa98xx_toggle_mtp_clock
 * Allows to stop clock for MTP/FAim needed for PLMA5505 */
static enum Tfa98xx_Error tfa_faim_protect(struct tfa_device *tfa, int state)
{
	(void)tfa;
	(void)state;

	return Tfa98xx_Error_Ok;
}

/** Set internal oscillator into power down mode.
 *
 *  This function is a worker for tfa98xx_set_osc_powerdown().
 *
 *  @param[in] tfa device description structure
 *  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
 *
 *  @return Tfa98xx_Error_Ok when successfull, error otherwise.
 */
static enum Tfa98xx_Error tfa_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return Tfa98xx_Error_Ok;
}
static enum Tfa98xx_Error tfa_update_lpm(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return Tfa98xx_Error_Ok;
}
static enum Tfa98xx_Error tfa_dsp_reset(struct tfa_device *tfa, int state)
{
	/* generic function */
	TFA_SET_BF_VOLATILE(tfa, RST, (uint16_t)state);

	return Tfa98xx_Error_Ok;
}

int tfa_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int mtpk, active_value = tfa->profile;

	/* Also set the new value in the struct */
	tfa->profile = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWPROFIL, new_value); /* set current profile */
	TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swprofile(struct tfa_device *tfa)
{
	return tfa->profile;
}

static int tfa_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	int mtpk, active_value = tfa->vstep;

	/* Also set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWVSTEP, new_value); /* set current vstep */
	TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swvstep(struct tfa_device *tfa)
{
	int value = 0;
	/* Set the new value in the hw register */
	value = TFA_GET_BF(tfa, SWVSTEP);

	/* Also set the new value in the struct */
	tfa->vstep = value - 1;

	return value - 1; /* invalid if 0 */
}

static int tfa_get_mtpb(struct tfa_device *tfa)
{

	int value = 0;

	/* Set the new value in the hw register */
	value = TFA_GET_BF(tfa, MTPB);

	return value;
}

static enum Tfa98xx_Error
tfa_set_mute_nodsp(struct tfa_device *tfa, int mute)
{
	(void)tfa;
	(void)mute;

	return Tfa98xx_Error_Ok;
}

void set_ops_defaults(struct tfa_device_ops *ops)
{
	/* defaults */
	ops->reg_read = tfa98xx_read_register16;
	ops->reg_write = tfa98xx_write_register16;
	ops->mem_read = tfa98xx_dsp_read_mem;
	ops->mem_write = tfa98xx_dsp_write_mem_word;
	ops->dsp_msg = tfa_dsp_msg;
	ops->dsp_msg_read = tfa_dsp_msg_read;
	ops->dsp_write_tables = no_overload_function_available;
	ops->dsp_reset = tfa_dsp_reset;
	ops->dsp_system_stable = tfa_dsp_system_stable;
	ops->auto_copy_mtp_to_iic = no_overload_function_available2;
	ops->factory_trimmer = no_overload_function_available2;
	ops->set_swprof = tfa_set_swprofile;
	ops->get_swprof = tfa_get_swprofile;
	ops->set_swvstep = tfa_set_swvstep;
	ops->get_swvstep = tfa_get_swvstep;
	ops->get_mtpb = tfa_get_mtpb;
	ops->set_mute = tfa_set_mute_nodsp;
	ops->faim_protect = tfa_faim_protect;
	ops->set_osc_powerdown = tfa_set_osc_powerdown;
	ops->update_lpm = tfa_update_lpm;
}

/***********************************************************************************/
/* no TFA
 *  external DSP SB instance                                                               */
 /***********************************************************************************/
static short tfanone_swvstep, swprof; //TODO emulate in hal plugin
static enum Tfa98xx_Error tfanone_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	(void)tfa; /* suppress warning */
	*ready = 1; /* assume always ready */

	return Tfa98xx_Error_Ok;
}

static int tfanone_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	swprof = new_value;

	return active_value;
}

static int tfanone_get_swprofile(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */
	return swprof;
}

static int tfanone_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfanone_swvstep = new_value;

	return new_value;
}

static int tfanone_get_swvstep(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */
	return tfanone_swvstep;
}

void tfanone_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->dsp_system_stable = tfanone_dsp_system_stable;
	ops->set_swprof = tfanone_set_swprofile;
	ops->get_swprof = tfanone_get_swprofile;
	ops->set_swvstep = tfanone_set_swvstep;
	ops->get_swvstep = tfanone_get_swvstep;

}

/***********************************************************************************/
/* TFA9912                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9912_faim_protect(struct tfa_device *tfa, int status)
{
	enum Tfa98xx_Error ret = Tfa98xx_Error_Fail;

	if (tfa) {
		if (status == 0 || status == 1) {
			ret = -(tfa_set_bf(tfa, TFA9912_BF_SSFAIME, (uint16_t)status));
		}
	}

	return ret;
}

static enum Tfa98xx_Error tfa9912_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short value, xor;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* Unlock keys to write settings */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);

	/* The optimal settings */
	if (tfa->rev == 0x1a13) {

		/* ----- generated code start ----- */
		/* -----  version 1.43  ----- */
		reg_write(tfa, 0x00, 0x0255); //POR=0x0245
		reg_write(tfa, 0x01, 0x838a); //POR=0x83ca
		reg_write(tfa, 0x02, 0x2dc8); //POR=0x2828
		reg_write(tfa, 0x05, 0x762a); //POR=0x766a
		reg_write(tfa, 0x22, 0x543c); //POR=0x545c
		reg_write(tfa, 0x26, 0x0100); //POR=0x0010
		reg_write(tfa, 0x51, 0x0000); //POR=0x0080
		reg_write(tfa, 0x52, 0x551c); //POR=0x1afc
		reg_write(tfa, 0x53, 0x003e); //POR=0x001e
		reg_write(tfa, 0x61, 0x000c); //POR=0x0018
		reg_write(tfa, 0x63, 0x0a96); //POR=0x0a9a
		reg_write(tfa, 0x65, 0x0a82); //POR=0x0a8b
		reg_write(tfa, 0x66, 0x0701); //POR=0x0700
		reg_write(tfa, 0x6c, 0x00d5); //POR=0x02d5
		reg_write(tfa, 0x70, 0x26f8); //POR=0x06e0
		reg_write(tfa, 0x71, 0x3074); //POR=0x2074
		reg_write(tfa, 0x75, 0x4484); //POR=0x4585
		reg_write(tfa, 0x76, 0x72ea); //POR=0x54a2
		reg_write(tfa, 0x83, 0x0716); //POR=0x0617
		reg_write(tfa, 0x89, 0x0013); //POR=0x0014
		reg_write(tfa, 0xb0, 0x4c08); //POR=0x4c00
		reg_write(tfa, 0xc6, 0x004e); //POR=0x000e /* PLMA5539: Please make sure bit 6 is always on! */
		/* ----- generated code end   ----- */

		/* PLMA5505: MTP key open makes vulanable for MTP corruption */
		tfa9912_faim_protect(tfa, 0);
	} else {
		pr_info("Warning: Optimal settings not found for device with revid = 0x%x \n", tfa->rev);
	}

	return error;
}

static enum Tfa98xx_Error tfa9912_factory_trimmer(struct tfa_device *tfa)
{
	unsigned short currentValue, delta;
	int result;

	/* Factory trimming for the Boost converter */
	/* check if there is a correction needed */
	result = TFA_GET_BF(tfa, DCMCCAPI);
	if (result) {
		/* Get currentvalue of DCMCC and the Delta value */
		currentValue = (unsigned short)TFA_GET_BF(tfa, DCMCC);
		delta = (unsigned short)TFA_GET_BF(tfa, USERDEF);

		/* check the sign bit (+/-) */
		result = TFA_GET_BF(tfa, DCMCCSB);
		if (result == 0) {
			/* Do not exceed the maximum value of 15 */
			if (currentValue + delta < 15) {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, currentValue + delta);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: %d \n", currentValue + delta);
			} else {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, 15);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: 15 \n");
			}
		} else if (result == 1) {
			/* Do not exceed the minimum value of 0 */
			if (currentValue - delta > 0) {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, currentValue - delta);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: %d \n", currentValue - delta);
			} else {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, 0);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: 0 \n");
			}
		}
	}

	return Tfa98xx_Error_Ok;
}

static enum Tfa98xx_Error tfa9912_auto_copy_mtp_to_iic(struct tfa_device *tfa)
{
	/* Set auto_copy_mtp_to_iic (bit 5 of A3) to 1. Workaround for 72, 88 and 9912/9892(see PLMA5290) */
	return reg_write(tfa, 0xA3, 0x20);
}

static int tfa9912_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9912_BF_SWPROFIL, new_value);

	return active_value;
}

static int tfa9912_get_swprofile(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9912_BF_SWPROFIL) - 1;
}

static int tfa9912_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9912_BF_SWVSTEP, new_value);

	return new_value;
}

static int tfa9912_get_swvstep(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9912_BF_SWVSTEP) - 1;
}

static enum Tfa98xx_Error
tfa9912_set_mute(struct tfa_device *tfa, int mute)
{
	tfa_set_bf(tfa, TFA9912_BF_CFSM, (const uint16_t)mute);

	return Tfa98xx_Error_Ok;
}

/* Maksimum value for combination of boost_voltage and vout calibration offset (see PLMA5322, PLMA5528). */
#define TFA9912_VBOOST_MAX		57
#define TFA9912_CALIBR_BOOST_MAX	63
#define TFA9912_DCDCCNT6_REG		(TFA9912_BF_DCVOF >> 8)
#define TFA9912_CALIBR_REG		0xf1

static uint16_t tfa9912_vboost_fixup(struct tfa_device *tfa, uint16_t dcdc_cnt6)
{
	unsigned short cal_offset;
	unsigned short boost_v_1st, boost_v_2nd;
	uint16_t new_dcdc_cnt6;

	/* Get current calibr_vout_offset, this register is not supported by bitfields */
	reg_read(tfa, TFA9912_CALIBR_REG, &cal_offset);
	cal_offset = (cal_offset & 0x001f);
	new_dcdc_cnt6 = dcdc_cnt6;

	/* Get current boost_volatage values */
	boost_v_1st = tfa_get_bf_value(TFA9912_BF_DCVOF, new_dcdc_cnt6);
	boost_v_2nd = tfa_get_bf_value(TFA9912_BF_DCVOS, new_dcdc_cnt6);

	/* Check boost voltages */
	if (boost_v_1st > TFA9912_VBOOST_MAX)
		boost_v_1st = TFA9912_VBOOST_MAX;

	if (boost_v_2nd > TFA9912_VBOOST_MAX)
		boost_v_2nd = TFA9912_VBOOST_MAX;

	/* Recalculate values, max for the sum is TFA9912_CALIBR_BOOST_MAX */
	if (boost_v_1st + cal_offset > TFA9912_CALIBR_BOOST_MAX)
		boost_v_1st = TFA9912_CALIBR_BOOST_MAX - cal_offset;

	if (boost_v_2nd + cal_offset > TFA9912_CALIBR_BOOST_MAX)
		boost_v_2nd = TFA9912_CALIBR_BOOST_MAX - cal_offset;

	tfa_set_bf_value(TFA9912_BF_DCVOF, boost_v_1st, &new_dcdc_cnt6);
	tfa_set_bf_value(TFA9912_BF_DCVOS, boost_v_2nd, &new_dcdc_cnt6);

	/* Change register value only when it's neccesary */
	if (new_dcdc_cnt6 != dcdc_cnt6) {
		if (tfa->verbose)
			pr_debug("tfa9912: V boost fixup applied. Old 0x%04x, new 0x%04x\n",
				dcdc_cnt6, new_dcdc_cnt6);
		dcdc_cnt6 = new_dcdc_cnt6;
	}

	return dcdc_cnt6;
}

/* PLMA5322, PLMA5528 - Limit values of DCVOS and DCVOF to range specified in datasheet. */
enum Tfa98xx_Error tfa9912_reg_write(struct tfa_device *tfa, unsigned char subaddress, unsigned short value)
{
	if (subaddress == TFA9912_DCDCCNT6_REG) {
		/* Correct V boost (first and secondary) to ensure 12V is not exceeded. */
		value = tfa9912_vboost_fixup(tfa, value);
	}

	return tfa98xx_write_register16(tfa, subaddress, value);
}

/** Set internal oscillator into power down mode for TFA9912.
*
*  This function is a worker for tfa98xx_set_osc_powerdown().
*
*  @param[in] tfa device description structure
*  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
*
*  @return Tfa98xx_Error_Ok when successfull, error otherwise.
*/
static enum Tfa98xx_Error tfa9912_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	if (state == 1 || state == 0) {
		return -tfa_set_bf(tfa, TFA9912_BF_MANAOOSC, (uint16_t)state);
	}

	return Tfa98xx_Error_Bad_Parameter;
}
/** update low power mode of the device.
*
*  @param[in] tfa device description structure
*  @param[in] state State of the low power mode1 detector control
*  0 - low power mode1 detector control enabled,
*  1 - low power mode1 detector control disabled(low power mode is also disabled).
*
*  @return Tfa98xx_Error_Ok when successfull, error otherwise.
*/
static enum Tfa98xx_Error tfa9912_update_lpm(struct tfa_device *tfa, int state)
{
	if (state == 1 || state == 0) {
		return -tfa_set_bf(tfa, TFA9912_BF_LPM1DIS, (uint16_t)state);
	}
	return Tfa98xx_Error_Bad_Parameter;
}

void tfa9912_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9912_specific;
	/* PLMA5322, PLMA5528 - Limits values of DCVOS and DCVOF. */
	ops->reg_write = tfa9912_reg_write;
	ops->factory_trimmer = tfa9912_factory_trimmer;
	ops->auto_copy_mtp_to_iic = tfa9912_auto_copy_mtp_to_iic;
	ops->set_swprof = tfa9912_set_swprofile;
	ops->get_swprof = tfa9912_get_swprofile;
	ops->set_swvstep = tfa9912_set_swvstep;
	ops->get_swvstep = tfa9912_get_swvstep;
	ops->set_mute = tfa9912_set_mute;
	ops->faim_protect = tfa9912_faim_protect;
	ops->set_osc_powerdown = tfa9912_set_osc_powerdown;
	ops->update_lpm = tfa9912_update_lpm;
}

/***********************************************************************************/
/* TFA9872                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9872_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint16_t MANAOOSC = 0x0140; /* version 17 */
	unsigned short value, xor;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* Unlock key 1 and 2 */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);
	tfa98xx_key2(tfa, 0);

	switch (tfa->rev) {
	case 0x1a72:
	case 0x2a72:
		/* ----- generated code start ----- */
		/* -----  version 26 ----- */
		reg_write(tfa, 0x00, 0x1801); //POR=0x0001
		reg_write(tfa, 0x02, 0x2dc8); //POR=0x2028
		reg_write(tfa, 0x20, 0x0890); //POR=0x2890
		reg_write(tfa, 0x22, 0x043c); //POR=0x045c
		reg_write(tfa, 0x51, 0x0000); //POR=0x0080
		reg_write(tfa, 0x52, 0x1a1c); //POR=0x7ae8
		reg_write(tfa, 0x58, 0x161c); //POR=0x101c
		reg_write(tfa, 0x61, 0x0198); //POR=0x0000
		reg_write(tfa, 0x65, 0x0a8b); //POR=0x0a9a
		reg_write(tfa, 0x70, 0x07f5); //POR=0x06e6
		reg_write(tfa, 0x74, 0xcc84); //POR=0xd823
		reg_write(tfa, 0x82, 0x01ed); //POR=0x000d
		reg_write(tfa, 0x83, 0x0014); //POR=0x0013
		reg_write(tfa, 0x84, 0x0021); //POR=0x0020
		reg_write(tfa, 0x85, 0x0001); //POR=0x0003
		/* ----- generated code end   ----- */
		break;
	case 0x1b72:
	case 0x2b72:
	case 0x3b72:
		/* ----- generated code start ----- */
		/*  -----  version 25.00 ----- */
		reg_write(tfa, 0x02, 0x2dc8); //POR=0x2828
		reg_write(tfa, 0x20, 0x0890); //POR=0x2890
		reg_write(tfa, 0x22, 0x043c); //POR=0x045c
		reg_write(tfa, 0x23, 0x0001); //POR=0x0003
		reg_write(tfa, 0x51, 0x0000); //POR=0x0080
		reg_write(tfa, 0x52, 0x5a1c); //POR=0x7a08
		reg_write(tfa, 0x61, 0x0198); //POR=0x0000
		reg_write(tfa, 0x63, 0x0a9a); //POR=0x0a93
		reg_write(tfa, 0x65, 0x0a82); //POR=0x0a8d
		reg_write(tfa, 0x6f, 0x01e3); //POR=0x02e4
		reg_write(tfa, 0x70, 0x06fd); //POR=0x06e6
		reg_write(tfa, 0x71, 0x307e); //POR=0x207e
		reg_write(tfa, 0x74, 0xcc84); //POR=0xd913
		reg_write(tfa, 0x75, 0x1132); //POR=0x118a
		reg_write(tfa, 0x82, 0x01ed); //POR=0x000d
		reg_write(tfa, 0x83, 0x001a); //POR=0x0013
		/* ----- generated code end   ----- */
		break;
	default:
		pr_info("\nWarning: Optimal settings not found for device with revid = 0x%x \n", tfa->rev);
		break;
	}

	/* Turn off the osc1m to save power: PLMA4928 */
	error = tfa_set_bf(tfa, MANAOOSC, 1);

	/* Bypass OVP by setting bit 3 from register 0xB0 (bypass_ovp=1): PLMA5258 */
	error = reg_read(tfa, 0xB0, &value);
	value |= 1 << 3;
	error = reg_write(tfa, 0xB0, value);

	return error;
}

static enum Tfa98xx_Error tfa9872_auto_copy_mtp_to_iic(struct tfa_device *tfa)
{
	/* Set auto_copy_mtp_to_iic (bit 5 of A3) to 1. Workaround for 72 and 88 (see PLMA5290) */
	return reg_write(tfa, 0xA3, 0x20);
}

static int tfa9872_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9872_BF_SWPROFIL, new_value);

	return active_value;
}

static int tfa9872_get_swprofile(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9872_BF_SWPROFIL) - 1;
}

static int tfa9872_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{

	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9872_BF_SWVSTEP, new_value);

	return new_value;
}

static int tfa9872_get_swvstep(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9872_BF_SWVSTEP) - 1;
}

void tfa9872_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9872_specific;
	ops->auto_copy_mtp_to_iic = tfa9872_auto_copy_mtp_to_iic;
	ops->set_swprof = tfa9872_set_swprofile;
	ops->get_swprof = tfa9872_get_swprofile;
	ops->set_swvstep = tfa9872_set_swvstep;
	ops->get_swvstep = tfa9872_get_swvstep;
	ops->set_mute = tfa_set_mute_nodsp;
}

/***********************************************************************************/
/* TFA9874                                                                         */
/***********************************************************************************/

static enum Tfa98xx_Error tfa9874_faim_protect(struct tfa_device *tfa, int status)
{
	enum Tfa98xx_Error ret = Tfa98xx_Error_Ok;
	/* 0b = FAIM protection enabled 1b = FAIM protection disabled*/
	ret = tfa_set_bf_volatile(tfa, TFA9874_BF_OPENMTP, (uint16_t)(status));
	return ret;
}


static enum Tfa98xx_Error tfa9874_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short value, xor;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* Unlock key 1 and 2 */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);
	tfa98xx_key2(tfa, 0);

	switch (tfa->rev) {
	case 0x0a74: /* Initial revision ID */
		/* ----- generated code start ----- */
		/* V25 */
		reg_write(tfa, 0x02, 0x22a8); //POR=0x25c8
		reg_write(tfa, 0x51, 0x0020); //POR=0x0000
		reg_write(tfa, 0x52, 0x57dc); //POR=0x56dc
		reg_write(tfa, 0x58, 0x16a4); //POR=0x1614
		reg_write(tfa, 0x61, 0x0110); //POR=0x0198
		reg_write(tfa, 0x66, 0x0701); //POR=0x0700
		reg_write(tfa, 0x6f, 0x00a3); //POR=0x01a3
		reg_write(tfa, 0x70, 0x07f8); //POR=0x06f8
		reg_write(tfa, 0x73, 0x0007); //POR=0x0005
		reg_write(tfa, 0x74, 0x5068); //POR=0xcc80
		reg_write(tfa, 0x75, 0x0d28); //POR=0x1138
		reg_write(tfa, 0x83, 0x0594); //POR=0x061a
		reg_write(tfa, 0x84, 0x0001); //POR=0x0021
		reg_write(tfa, 0x85, 0x0001); //POR=0x0003
		reg_write(tfa, 0x88, 0x0000); //POR=0x0002
		reg_write(tfa, 0xc4, 0x2001); //POR=0x0001
		/* ----- generated code end   ----- */
		break;
	case 0x0b74:
		/* ----- generated code start ----- */
		/* V1.6 */
		reg_write(tfa, 0x02, 0x22a8); //POR=0x25c8
		reg_write(tfa, 0x51, 0x0020); //POR=0x0000
		reg_write(tfa, 0x52, 0x57dc); //POR=0x56dc
		reg_write(tfa, 0x58, 0x16a4); //POR=0x1614
		reg_write(tfa, 0x61, 0x0110); //POR=0x0198
		reg_write(tfa, 0x66, 0x0701); //POR=0x0700
		reg_write(tfa, 0x6f, 0x00a3); //POR=0x01a3
		reg_write(tfa, 0x70, 0x07f8); //POR=0x06f8
		reg_write(tfa, 0x73, 0x0047); //POR=0x0045
		reg_write(tfa, 0x74, 0x5068); //POR=0xcc80
		reg_write(tfa, 0x75, 0x0d28); //POR=0x1138
		reg_write(tfa, 0x83, 0x0595); //POR=0x061a
		reg_write(tfa, 0x84, 0x0001); //POR=0x0021
		reg_write(tfa, 0x85, 0x0001); //POR=0x0003
		reg_write(tfa, 0x88, 0x0000); //POR=0x0002
		reg_write(tfa, 0xc4, 0x2001); //POR=0x0001
		/* ----- generated code end   ----- */
		break;
	case 0x0c74:
		/* ----- generated code start ----- */
		/* V1.16 */
		reg_write(tfa, 0x02, 0x22c8); //POR=0x25c8
		reg_write(tfa, 0x52, 0x57dc); //POR=0x56dc
		reg_write(tfa, 0x53, 0x003e); //POR=0x001e
		reg_write(tfa, 0x56, 0x0400); //POR=0x0600
		reg_write(tfa, 0x61, 0x0110); //POR=0x0198
		reg_write(tfa, 0x6f, 0x00a5); //POR=0x01a3
		reg_write(tfa, 0x70, 0x07f8); //POR=0x06f8
		reg_write(tfa, 0x73, 0x0047); //POR=0x0045
		reg_write(tfa, 0x74, 0x5098); //POR=0xcc80
		reg_write(tfa, 0x75, 0x8d28); //POR=0x1138
		reg_write(tfa, 0x80, 0x0000); //POR=0x0003
		reg_write(tfa, 0x83, 0x0799); //POR=0x061a
		reg_write(tfa, 0x84, 0x0081); //POR=0x0021
		/* ----- generated code end   ----- */
		break;
	default:
		pr_info("\nWarning: Optimal settings not found for device with revid = 0x%x \n", tfa->rev);
		break;
	}

	return error;
}

static int tfa9874_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9874_BF_SWPROFIL, new_value);

	return active_value;
}

static int tfa9874_get_swprofile(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9874_BF_SWPROFIL) - 1;
}

static int tfa9874_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{

	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9874_BF_SWVSTEP, new_value);

	return new_value;
}

static int tfa9874_get_swvstep(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9874_BF_SWVSTEP) - 1;
}

/* tfa98xx_dsp_system_stable
*  return: *ready = 1 when clocks are stable to allow DSP subsystem access
*/
static enum Tfa98xx_Error tfa9874_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	/* check CLKS: ready if set */
	*ready = tfa_get_bf(tfa, TFA9874_BF_CLKS) == 1;

	return error;
}

static int tfa9874_get_mtpb(struct tfa_device *tfa)
{

	int value;
	value = tfa_get_bf(tfa, TFA9874_BF_MTPB);
	return value;
}

void tfa9874_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9874_specific;
	ops->set_swprof = tfa9874_set_swprofile;
	ops->get_swprof = tfa9874_get_swprofile;
	ops->set_swvstep = tfa9874_set_swvstep;
	ops->get_swvstep = tfa9874_get_swvstep;
	ops->dsp_system_stable = tfa9874_dsp_system_stable;
	ops->faim_protect = tfa9874_faim_protect;
	ops->get_mtpb = tfa9874_get_mtpb;
	ops->set_mute = tfa_set_mute_nodsp;
}
/***********************************************************************************/
/* TFA9878                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9878_faim_protect(struct tfa_device *tfa, int status)
{
	enum Tfa98xx_Error ret = Tfa98xx_Error_Ok;
	/* 0b = FAIM protection enabled 1b = FAIM protection disabled*/
	ret = tfa_set_bf_volatile(tfa, TFA9878_BF_OPENMTP, (uint16_t)(status));
	return ret;
}


static enum Tfa98xx_Error tfa9878_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short value, xor;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* Unlock key 1 and 2 */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);
	tfa98xx_key2(tfa, 0);

	switch (tfa->rev) {
	case 0x0a78: /* Initial revision ID */
/* ----- generated code start ----- */
/* -----  version 28 ----- */
		reg_write(tfa, 0x01, 0x2e18); //POR=0x2e88
		reg_write(tfa, 0x02, 0x0628); //POR=0x0008
		reg_write(tfa, 0x04, 0x0240); //POR=0x0340
		reg_write(tfa, 0x52, 0x587c); //POR=0x57dc
		reg_write(tfa, 0x61, 0x0183); //POR=0x0a82
		reg_write(tfa, 0x63, 0x055a); //POR=0x0a9a
		reg_write(tfa, 0x65, 0x0542); //POR=0x0a82
		reg_write(tfa, 0x71, 0x303e); //POR=0x307e
		reg_write(tfa, 0x83, 0x009a); //POR=0x0799
		/* ----- generated code end   ----- */


		break;
	case 0x1a78: /* Initial revision ID */
		/* ----- generated code start ----- */
		/* -----  version 12 ----- */
		reg_write(tfa, 0x01, 0x2e18); //POR=0x2e88
		reg_write(tfa, 0x02, 0x0628); //POR=0x0008
		reg_write(tfa, 0x04, 0x0241); //POR=0x0340
		reg_write(tfa, 0x52, 0x587c); //POR=0x57dc
		reg_write(tfa, 0x61, 0x0183); //POR=0x0a82
		reg_write(tfa, 0x63, 0x055a); //POR=0x0a9a
		reg_write(tfa, 0x65, 0x0542); //POR=0x0a82
		reg_write(tfa, 0x70, 0xb7ff); //POR=0x37ff
		reg_write(tfa, 0x71, 0x303e); //POR=0x307e
		reg_write(tfa, 0x83, 0x009a); //POR=0x0799
		reg_write(tfa, 0x84, 0x0211); //POR=0x0011
		reg_write(tfa, 0x8c, 0x0210); //POR=0x0010
		reg_write(tfa, 0xce, 0x2202); //POR=0xa202
		reg_write(tfa, 0xd5, 0x0000); //POR=0x0100
		/* ----- generated code end   ----- */

		break;
	default:
		pr_info("\nWarning: Optimal settings not found for device with revid = 0x%x \n", tfa->rev);
		break;
	}

	return error;
}

static int tfa9878_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9878_BF_SWPROFIL, new_value);

	return active_value;
}

static int tfa9878_get_swprofile(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9878_BF_SWPROFIL) - 1;
}

static int tfa9878_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{

	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfa_set_bf_volatile(tfa, TFA9878_BF_SWVSTEP, new_value);

	return new_value;
}

static int tfa9878_get_swvstep(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9878_BF_SWVSTEP) - 1;
}

/* tfa98xx_dsp_system_stable
*  return: *ready = 1 when clocks are stable to allow DSP subsystem access
*/
static enum Tfa98xx_Error tfa9878_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	/* check CLKS: ready if set */
	*ready = tfa_get_bf(tfa, TFA9878_BF_CLKS) == 1;

	return error;
}

static int tfa9878_get_mtpb(struct tfa_device *tfa)
{

	int value;
	value = tfa_get_bf(tfa, TFA9878_BF_MTPB);
	return value;
}

void tfa9878_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9878_specific;
	ops->set_swprof = tfa9878_set_swprofile;
	ops->get_swprof = tfa9878_get_swprofile;
	ops->set_swvstep = tfa9878_set_swvstep;
	ops->get_swvstep = tfa9878_get_swvstep;
	ops->dsp_system_stable = tfa9878_dsp_system_stable;
	ops->faim_protect = tfa9878_faim_protect;
	ops->get_mtpb = tfa9878_get_mtpb;
	ops->set_mute = tfa_set_mute_nodsp;
}
/***********************************************************************************/
/* TFA9888                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9888_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short value, xor;
	int patch_version;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* Unlock keys to write settings */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);

	/* Only N1C2 is supported */
	/* ----- generated code start ----- */
	/* --------- Version v1 ---------- */
	if (tfa->rev == 0x2c88) {
		reg_write(tfa, 0x00, 0x164d); //POR=0x064d
		reg_write(tfa, 0x01, 0x828b); //POR=0x92cb
		reg_write(tfa, 0x02, 0x1dc8); //POR=0x1828
		reg_write(tfa, 0x0e, 0x0080); //POR=0x0000
		reg_write(tfa, 0x20, 0x089e); //POR=0x0890
		reg_write(tfa, 0x22, 0x543c); //POR=0x545c
		reg_write(tfa, 0x23, 0x0006); //POR=0x0000
		reg_write(tfa, 0x24, 0x0014); //POR=0x0000
		reg_write(tfa, 0x25, 0x000a); //POR=0x0000
		reg_write(tfa, 0x26, 0x0100); //POR=0x0000
		reg_write(tfa, 0x28, 0x1000); //POR=0x0000
		reg_write(tfa, 0x51, 0x0000); //POR=0x00c0
		reg_write(tfa, 0x52, 0xfafe); //POR=0xbaf6
		reg_write(tfa, 0x70, 0x3ee4); //POR=0x3ee6
		reg_write(tfa, 0x71, 0x1074); //POR=0x3074
		reg_write(tfa, 0x83, 0x0014); //POR=0x0013
		/* ----- generated code end   ----- */
	} else {
		pr_info("Warning: Optimal settings not found for device with revid = 0x%x \n", tfa->rev);
	}

	patch_version = tfa_cnt_get_patch_version(tfa);
	if (patch_version >= 0x060401)
		tfa->partial_enable = 1;

	return error;
}

static enum Tfa98xx_Error tfa9888_tfa_dsp_write_tables(struct tfa_device *tfa, int sample_rate)
{
	unsigned char buffer[15] = { 0 };
	int size = 15 * sizeof(char);

	/* Write the fractional delay in the hardware register 'cs_frac_delay' */
	switch (sample_rate) {
	case 0:	/* 8kHz */
		TFA_SET_BF(tfa, FRACTDEL, 40);
		break;
	case 1:	/* 11.025KHz */
		TFA_SET_BF(tfa, FRACTDEL, 38);
		break;
	case 2:	/* 12kHz */
		TFA_SET_BF(tfa, FRACTDEL, 37);
		break;
	case 3:	/* 16kHz */
		TFA_SET_BF(tfa, FRACTDEL, 59);
		break;
	case 4:	/* 22.05KHz */
		TFA_SET_BF(tfa, FRACTDEL, 56);
		break;
	case 5:	/* 24kHz */
		TFA_SET_BF(tfa, FRACTDEL, 56);
		break;
	case 6:	/* 32kHz */
		TFA_SET_BF(tfa, FRACTDEL, 52);
		break;
	case 7:	/* 44.1kHz */
		TFA_SET_BF(tfa, FRACTDEL, 48);
		break;
	case 8:
	default:/* 48kHz */
		TFA_SET_BF(tfa, FRACTDEL, 46);
		break;
	}

	/* First copy the msg_id to the buffer */
	buffer[0] = (uint8_t)0;
	buffer[1] = (uint8_t)MODULE_FRAMEWORK + 128;
	buffer[2] = (uint8_t)FW_PAR_ID_SET_SENSES_DELAY;

	/* Required for all FS exept 8kHz (8kHz is all zero) */
	if (sample_rate != 0) {
		buffer[5] = 1;	/* Vdelay_P */
		buffer[8] = 0;	/* Idelay_P */
		buffer[11] = 1; /* Vdelay_S */
		buffer[14] = 0; /* Idelay_S */
	}

	/* send SetSensesDelay msg */
	return dsp_msg(tfa, size, (char *)buffer);
}

static enum Tfa98xx_Error tfa9888_auto_copy_mtp_to_iic(struct tfa_device *tfa)
{
	/* Set auto_copy_mtp_to_iic (bit 5 of A3) to 1. Workaround for 72 and 88 (see PLMA5290) */
	return reg_write(tfa, 0xA3, 0x20);
}

static enum Tfa98xx_Error tfa9888_factory_trimmer(struct tfa_device *tfa)
{
	unsigned short currentValue, delta;
	int result;

	/* Factory trimming for the Boost converter */
	/* check if there is a correction needed */
	result = TFA_GET_BF(tfa, DCMCCAPI);
	if (result) {
		/* Get currentvalue of DCMCC and the Delta value */
		currentValue = (unsigned short)TFA_GET_BF(tfa, DCMCC);
		delta = (unsigned short)TFA_GET_BF(tfa, USERDEF);

		/* check the sign bit (+/-) */
		result = TFA_GET_BF(tfa, DCMCCSB);
		if (result == 0) {
			/* Do not exceed the maximum value of 15 */
			if (currentValue + delta < 15) {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, currentValue + delta);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: %d \n", currentValue + delta);
			} else {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, 15);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: 15 \n");
			}
		} else if (result == 1) {
			/* Do not exceed the minimum value of 0 */
			if (currentValue - delta > 0) {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, currentValue - delta);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: %d \n", currentValue - delta);
			} else {
				TFA_SET_BF_VOLATILE(tfa, DCMCC, 0);
				if (tfa->verbose)
					pr_debug("Max coil current is set to: 0 \n");
			}
		}
	}

	return Tfa98xx_Error_Ok;
}

static enum Tfa98xx_Error
tfa9888_set_mute(struct tfa_device *tfa, int mute)
{
	TFA_SET_BF(tfa, CFSMR, (const uint16_t)mute);
	TFA_SET_BF(tfa, CFSML, (const uint16_t)mute);

	return Tfa98xx_Error_Ok;
}

void tfa9888_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9888_specific;
	ops->dsp_write_tables = tfa9888_tfa_dsp_write_tables;
	ops->auto_copy_mtp_to_iic = tfa9888_auto_copy_mtp_to_iic;
	ops->factory_trimmer = tfa9888_factory_trimmer;
	ops->set_mute = tfa9888_set_mute;
}

/***********************************************************************************/
/* TFA9896                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9896_faim_protect(struct tfa_device *tfa, int status)
{
	enum Tfa98xx_Error ret = Tfa98xx_Error_Ok;

	if ((tfa->rev == 0x2b96) || (tfa->rev == 0x3b96)) {
		ret = tfa_set_bf_volatile(tfa, TFA9896_BF_OPENMTP, (uint16_t)status);
	}

	return ret;
}

/***********************************************************************************/
/* TFA9896                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9896_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short check_value;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* all i2C registers must already set to default POR value */

	/* $48:[3] - 1 ==> 0; iddqtestbst - default value changed.
	* When Iddqtestbst is set to "0", the slewrate is reduced.
	* This will lower the overshoot on IN-B to avoid NMOS damage of booster.
	*/
	if (tfa->rev == 0x1b96) {
		/* ----- generated code start v17 ----- */
		reg_write(tfa, 0x06, 0x000b); //POR=0x0001
		reg_write(tfa, 0x07, 0x3e7f); //POR=0x1e7f
		reg_write(tfa, 0x0a, 0x0d8a); //POR=0x0592
		reg_write(tfa, 0x48, 0x0300); //POR=0x0308
		reg_write(tfa, 0x88, 0x0100); //POR=0x0000
		/* ----- generated code end   ----- */
	} else if (tfa->rev == 0x2b96) {
		/* ----- generated code start ----- v1*/
		reg_write(tfa, 0x06, 0x000b); //POR=0x0001
		reg_write(tfa, 0x07, 0x3e7f); //POR=0x1e7f
		reg_write(tfa, 0x0a, 0x0d8a); //POR=0x0592
		reg_write(tfa, 0x48, 0x0300); //POR=0x0308
		reg_write(tfa, 0x88, 0x0100); //POR=0x0000
		/* ----- generated code end   ----- */
	} else if (tfa->rev == 0x3b96) {
		/* ----- generated code start ----- v1*/
		reg_write(tfa, 0x06, 0x000b); //POR=0x0001
		reg_write(tfa, 0x07, 0x3e7f); //POR=0x1e7f
		reg_write(tfa, 0x0a, 0x0d8a); //POR=0x0592
		reg_write(tfa, 0x48, 0x0300); //POR=0x0308
		reg_write(tfa, 0x88, 0x0100); //POR=0x0000
		/* ----- generated code end   ----- */
	}
	/* $49:[0] - 1 ==> 0; CLIP - default value changed. 0 means CLIPPER on */
	error = reg_read(tfa, 0x49, &check_value);
	check_value &= ~0x1;
	error = reg_write(tfa, 0x49, check_value);
	return error;
}

/*
* the int24 values for the vsfw delay table
*/
static unsigned char tfa9896_vsfwdelay_table[] = {
	0, 0, 2, /* Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0, /* Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0, /* Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 2, /* Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 2, /* Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 2, /* Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 2, /* Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2, /* Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 3  /* Index 8 - Current/Volt Fractional Delay for 48KHz */
};

/*
* TODO make this tfa98xx
*  Note that the former products write this table via the patch
*  so moving this to the tfa98xx API requires also updating all patches
*/
static enum Tfa98xx_Error tfa9896_dsp_write_vsfwdelay_table(struct tfa_device *tfa)
{
	return tfa_dsp_cmd_id_write(tfa, MODULE_FRAMEWORK, TFA1_FW_PAR_ID_SET_CURRENT_DELAY, sizeof(tfa9896_vsfwdelay_table), tfa9896_vsfwdelay_table);
}

/*
* The int24 values for the fracdelay table
* For now applicable only for 8 and 48 kHz
*/
static unsigned char tfa9896_cvfracdelay_table[] = {
	0, 0, 51, /* Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0, /* Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0, /* Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 38, /* Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 34, /* Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 33, /* Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 11, /* Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2,  /* Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 62  /* Index 8 - Current/Volt Fractional Delay for 48KHz */
};

static enum Tfa98xx_Error tfa9896_dsp_write_cvfracdelay_table(struct tfa_device *tfa)
{
	return tfa_dsp_cmd_id_write(tfa, MODULE_FRAMEWORK, TFA1_FW_PAR_ID_SET_CURFRAC_DELAY, sizeof(tfa9896_cvfracdelay_table), tfa9896_cvfracdelay_table);;
}

static enum Tfa98xx_Error tfa9896_tfa_dsp_write_tables(struct tfa_device *tfa, int sample_rate)
{
	enum Tfa98xx_Error error;

	/* Not used for max1! */
	(void)sample_rate;

	error = tfa9896_dsp_write_vsfwdelay_table(tfa);
	if (error == Tfa98xx_Error_Ok) {
		error = tfa9896_dsp_write_cvfracdelay_table(tfa);
	}

	return error;
}

void tfa9896_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9896_specific;
	ops->dsp_write_tables = tfa9896_tfa_dsp_write_tables;
	ops->faim_protect = tfa9896_faim_protect;
}

/***********************************************************************************/
/* TFA9897                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9897_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short check_value;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* all i2C registers must already set to default POR value */

	/* $48:[3] - 1 ==> 0; iddqtestbst - default value changed.
	* When Iddqtestbst is set to "0", the slewrate is reduced.
	* This will lower the overshoot on IN-B to avoid NMOS damage of booster */
	error = reg_write(tfa, 0x48, 0x0300); /* POR value = 0x308 */

	/* $49:[0] - 1 ==> 0; CLIP - default value changed. 0 means CLIPPER on */
	error = reg_read(tfa, 0x49, &check_value);
	check_value &= ~0x1;
	error = reg_write(tfa, 0x49, check_value);

	return error;
}

/*
* the int24 values for the vsfw delay table
*/
static unsigned char tfa9897_vsfwdelay_table[] = {
	0, 0, 2, /* Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0, /* Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0, /* Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 2, /* Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 2, /* Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 2, /* Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 2, /* Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2, /* Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 3  /* Index 8 - Current/Volt Fractional Delay for 48KHz */
};

/*
* TODO make this tfa98xx
*  Note that the former products write this table via the patch
*  so moving this to the tfa98xx API requires also updating all patches
*/
static enum Tfa98xx_Error tfa9897_dsp_write_vsfwdelay_table(struct tfa_device *tfa)
{
	return tfa_dsp_cmd_id_write(tfa, MODULE_FRAMEWORK, TFA1_FW_PAR_ID_SET_CURRENT_DELAY, sizeof(tfa9897_vsfwdelay_table), tfa9897_vsfwdelay_table);;
}

/*
* The int24 values for the fracdelay table
* For now applicable only for 8 and 48 kHz
*/
static unsigned char tfa9897_cvfracdelay_table[] = {
	0, 0, 51, /* Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0, /* Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0, /* Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 38, /* Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 34, /* Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 33, /* Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 11, /* Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2,  /* Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 62  /* Index 8 - Current/Volt Fractional Delay for 48KHz */
};

static enum Tfa98xx_Error tfa9897_dsp_write_cvfracdelay_table(struct tfa_device *tfa)
{
	return tfa_dsp_cmd_id_write(tfa, MODULE_FRAMEWORK, TFA1_FW_PAR_ID_SET_CURFRAC_DELAY, sizeof(tfa9897_cvfracdelay_table), tfa9897_cvfracdelay_table);;
}

static enum Tfa98xx_Error tfa9897_tfa_dsp_write_tables(struct tfa_device *tfa, int sample_rate)
{
	enum Tfa98xx_Error error;

	/* Not used for max1! */
	(void)sample_rate;

	error = tfa9897_dsp_write_vsfwdelay_table(tfa);
	if (error == Tfa98xx_Error_Ok) {
		error = tfa9897_dsp_write_cvfracdelay_table(tfa);
	}

	return error;
}

void tfa9897_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9897_specific;
	ops->dsp_write_tables = tfa9897_tfa_dsp_write_tables;
}

/***********************************************************************************/
/* TFA9895                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9895_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int result;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* all i2C registers are already set to default */

	result = TFA_SET_BF(tfa, AMPE, 1);
	if (result < 0)
		return -result;

	/* some other registers must be set for optimal amplifier behaviour */
	reg_write(tfa, 0x05, 0x13AB);
	reg_write(tfa, 0x06, 0x001F);
	/* peak voltage protection is always on, but may be written */
	reg_write(tfa, 0x08, 0x3C4E);
	/*TFA98XX_SYSCTRL_DCA=0*/
	reg_write(tfa, 0x09, 0x024D);
	reg_write(tfa, 0x41, 0x0308);
	error = reg_write(tfa, 0x49, 0x0E82);

	return error;
}

void tfa9895_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9895_specific;
}

/***********************************************************************************/
/* TFA9891                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9891_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* ----- generated code start ----- */
	/* -----  version 18.0 ----- */
	reg_write(tfa, 0x09, 0x025d); //POR=0x024d
	reg_write(tfa, 0x10, 0x0018); //POR=0x0024
	reg_write(tfa, 0x22, 0x0003); //POR=0x0023
	reg_write(tfa, 0x25, 0x0001); //POR=0x0000
	reg_write(tfa, 0x46, 0x0000); //POR=0x4000
	reg_write(tfa, 0x55, 0x3ffb); //POR=0x7fff
	/* ----- generated code end   ----- */

	return error;
}

void tfa9891_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9891_specific;
}

/***********************************************************************************/
/* TFA9890                                                                         */
/***********************************************************************************/
static enum Tfa98xx_Error tfa9890_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short regRead = 0;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	/* all i2C registers are already set to default for N1C2 */

	/* some PLL registers must be set optimal for amplifier behaviour */
	error = reg_write(tfa, 0x40, 0x5a6b);
	if (error)
		return error;
	reg_read(tfa, 0x59, &regRead);
	regRead |= 0x3;
	reg_write(tfa, 0x59, regRead);
	error = reg_write(tfa, 0x40, 0x0000);
	error = reg_write(tfa, 0x47, 0x7BE1);

	return error;
}

/*
* Disable clock gating
*/
static enum Tfa98xx_Error tfa9890_clockgating(struct tfa_device *tfa, int on)
{
	enum Tfa98xx_Error error;
	unsigned short value;

	/* for TFA9890 temporarily disable clock gating when dsp reset is used */
	error = reg_read(tfa, TFA98XX_CURRENTSENSE4, &value);
	if (error)
		return error;

	if (Tfa98xx_Error_Ok == error) {
		if (on)  /* clock gating on - clear the bit */
			value &= ~TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;
		else  /* clock gating off - set the bit */
			value |= TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;

		error = reg_write(tfa, TFA98XX_CURRENTSENSE4, value);
	}

	return error;
}

/*
* Tfa9890_DspReset will deal with clock gating control in order
* to reset the DSP for warm state restart
*/
static enum Tfa98xx_Error tfa9890_dsp_reset(struct tfa_device *tfa, int state)
{
	enum Tfa98xx_Error error;

	/* for TFA9890 temporarily disable clock gating
	when dsp reset is used */
	tfa9890_clockgating(tfa, 0);

	TFA_SET_BF(tfa, RST, (uint16_t)state);

	/* clock gating restore */
	error = tfa9890_clockgating(tfa, 1);

	return error;
}

/*
 * Tfa9890_DspSystemStable will compensate for the wrong behavior of CLKS
 * to determine if the DSP subsystem is ready for patch and config loading.
 *
 * A MTP calibration register is checked for non-zero.
 *
 * Note: This only works after i2c reset as this will clear the MTP contents.
 * When we are configured then the DSP communication will synchronize access.
 *
 */
static enum Tfa98xx_Error tfa9890_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short status, mtp0;
	int result, tries;

	/* check the contents of the STATUS register */
	result = TFA_READ_REG(tfa, AREFS);
	if (result < 0) {
		error = -result;
		goto errorExit;
	}
	status = (unsigned short)result;

	/* if AMPS is set then we were already configured and running
	 *   no need to check further
	 */
	*ready = (TFA_GET_BF_VALUE(tfa, AMPS, status) == 1);
	if (*ready)		/* if  ready go back */
		return error;	/* will be Tfa98xx_Error_Ok */

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(tfa, AREFS, status) == 0)
		|| (TFA_GET_BF_VALUE(tfa, CLKS, status) == 0));
	if (!*ready)		/* if not ready go back */
		return error;	/* will be Tfa98xx_Error_Ok */

	/* check MTPB
	 *   mtpbusy will be active when the subsys copies MTP to I2C
	 *   2 times retry avoids catching this short mtpbusy active period
	 */
	for (tries = 2; tries > 0; tries--) {
		result = TFA_GET_BF(tfa, MTPB);/*TODO_MTPB*/
		if (result < 0) {
			error = -result;
			goto errorExit;
		}
		status = (unsigned short)result;

		/* check the contents of the STATUS register */
		*ready = (result == 0);
		if (*ready)	/* if ready go on */
			break;
	}
	if (tries == 0)		/* ready will be 0 if retries exausted */
		return Tfa98xx_Error_Ok;

	/* check the contents of  MTP register for non-zero,
	 *  this indicates that the subsys is ready  */

	error = reg_read(tfa, 0x84, &mtp0);
	if (error)
		goto errorExit;

	*ready = (mtp0 != 0);	/* The MTP register written? */

	return error;

errorExit:
	*ready = 0;
	return error;
}

void tfa9890_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9890_specific;
	ops->dsp_reset = tfa9890_dsp_reset;
	ops->dsp_system_stable = tfa9890_dsp_system_stable;
}

/***********************************************************************************/
/* TFA9894                                                                         */
/***********************************************************************************/
static int tfa9894_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;
	tfa_set_bf_volatile(tfa, TFA9894_BF_SWPROFIL, new_value);
	return active_value;
}

static int tfa9894_get_swprofile(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9894_BF_SWPROFIL) - 1;
}

static int tfa9894_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;
	tfa_set_bf_volatile(tfa, TFA9894_BF_SWVSTEP, new_value);
	return new_value;
}

static int tfa9894_get_swvstep(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9894_BF_SWVSTEP) - 1;
}

static int tfa9894_get_mtpb(struct tfa_device *tfa)
{
	int value = 0;
	value = tfa_get_bf(tfa, TFA9894_BF_MTPB);
	return value;
}

/** Set internal oscillator into power down mode for TFA9894.
*
*  This function is a worker for tfa98xx_set_osc_powerdown().
*
*  @param[in] tfa device description structure
*  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
*
*  @return Tfa98xx_Error_Ok when successfull, error otherwise.
*/
static enum Tfa98xx_Error tfa9894_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	if (state == 1 || state == 0) {
		return -tfa_set_bf(tfa, TFA9894_BF_MANAOOSC, (uint16_t)state);
	}

	return Tfa98xx_Error_Bad_Parameter;
}

static enum Tfa98xx_Error tfa9894_faim_protect(struct tfa_device *tfa, int status)
{
	enum Tfa98xx_Error ret = Tfa98xx_Error_Ok;
	/* 0b = FAIM protection enabled 1b = FAIM protection disabled*/
	ret = tfa_set_bf_volatile(tfa, TFA9894_BF_OPENMTP, (uint16_t)(status));
	return ret;
}

static enum Tfa98xx_Error tfa9894_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short value, xor;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;
	if (tfa->verbose)
		if (is_94_N2_device(tfa))
			pr_debug("check_correct\n");
	/* Unlock keys to write settings */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);
	pr_debug("Device REFID:%x\n", tfa->rev);
	/* The optimal settings */
	if (tfa->rev == 0x0a94) {
		/* V36 */
		/* ----- generated code start ----- */
		reg_write(tfa, 0x00, 0xa245); //POR=0x8245
		reg_write(tfa, 0x02, 0x51e8); //POR=0x55c8
		reg_write(tfa, 0x52, 0xbe17); //POR=0xb617
		reg_write(tfa, 0x57, 0x0344); //POR=0x0366
		reg_write(tfa, 0x61, 0x0033); //POR=0x0073
		reg_write(tfa, 0x71, 0x00cf); //POR=0x018d
		reg_write(tfa, 0x72, 0x34a9); //POR=0x44e8
		reg_write(tfa, 0x73, 0x3808); //POR=0x3806
		reg_write(tfa, 0x76, 0x0067); //POR=0x0065
		reg_write(tfa, 0x80, 0x0000); //POR=0x0003
		reg_write(tfa, 0x81, 0x5715); //POR=0x561a
		reg_write(tfa, 0x82, 0x0104); //POR=0x0044
		/* ----- generated code end   ----- */
	} else if (tfa->rev == 0x1a94) {
		/* V17 */
		/* ----- generated code start ----- */
		reg_write(tfa, 0x00, 0xa245); //POR=0x8245
		reg_write(tfa, 0x01, 0x15da); //POR=0x11ca
		reg_write(tfa, 0x02, 0x5288); //POR=0x55c8
		reg_write(tfa, 0x52, 0xbe17); //POR=0xb617
		reg_write(tfa, 0x53, 0x0dbe); //POR=0x0d9e
		reg_write(tfa, 0x56, 0x05c3); //POR=0x07c3
		reg_write(tfa, 0x57, 0x0344); //POR=0x0366
		reg_write(tfa, 0x61, 0x0032); //POR=0x0073
		reg_write(tfa, 0x71, 0x00cf); //POR=0x018d
		reg_write(tfa, 0x72, 0x34a9); //POR=0x44e8
		reg_write(tfa, 0x73, 0x38c8); //POR=0x3806
		reg_write(tfa, 0x76, 0x0067); //POR=0x0065
		reg_write(tfa, 0x80, 0x0000); //POR=0x0003
		reg_write(tfa, 0x81, 0x5799); //POR=0x561a
		reg_write(tfa, 0x82, 0x0104); //POR=0x0044
		/* ----- generated code end ----- */

	} else if (tfa->rev == 0x2a94 || tfa->rev == 0x3a94) {
		/* ----- generated code start ----- */
		/* -----  version 25.00 ----- */
		reg_write(tfa, 0x01, 0x15da); //POR=0x11ca
		reg_write(tfa, 0x02, 0x51e8); //POR=0x55c8
		reg_write(tfa, 0x04, 0x0200); //POR=0x0000
		reg_write(tfa, 0x52, 0xbe17); //POR=0xb617
		reg_write(tfa, 0x53, 0x0dbe); //POR=0x0d9e
		reg_write(tfa, 0x57, 0x0344); //POR=0x0366
		reg_write(tfa, 0x61, 0x0032); //POR=0x0073
		reg_write(tfa, 0x71, 0x6ecf); //POR=0x6f8d
		reg_write(tfa, 0x72, 0xb4a9); //POR=0x44e8
		reg_write(tfa, 0x73, 0x38c8); //POR=0x3806
		reg_write(tfa, 0x76, 0x0067); //POR=0x0065
		reg_write(tfa, 0x80, 0x0000); //POR=0x0003
		reg_write(tfa, 0x81, 0x5799); //POR=0x561a
		reg_write(tfa, 0x82, 0x0104); //POR=0x0044
	/* ----- generated code end   ----- */
	}
	return error;
}

static enum Tfa98xx_Error
tfa9894_set_mute(struct tfa_device *tfa, int mute)
{
	tfa_set_bf(tfa, TFA9894_BF_CFSM, (const uint16_t)mute);
	return Tfa98xx_Error_Ok;
}

static enum Tfa98xx_Error tfa9894_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	/* check CLKS: ready if set */
	*ready = tfa_get_bf(tfa, TFA9894_BF_CLKS) == 1;

	return error;
}

void tfa9894_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9894_specific;
	ops->dsp_system_stable = tfa9894_dsp_system_stable;
	ops->set_mute = tfa9894_set_mute;
	ops->faim_protect = tfa9894_faim_protect;
	ops->get_mtpb = tfa9894_get_mtpb;
	ops->set_swprof = tfa9894_set_swprofile;
	ops->get_swprof = tfa9894_get_swprofile;
	ops->set_swvstep = tfa9894_set_swvstep;
	ops->get_swvstep = tfa9894_get_swvstep;
	//ops->auto_copy_mtp_to_iic = tfa9894_auto_copy_mtp_to_iic;
	ops->set_osc_powerdown = tfa9894_set_osc_powerdown;
}
