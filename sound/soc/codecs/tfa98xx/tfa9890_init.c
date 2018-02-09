/*
 *Copyright 2014,2015 NXP Semiconductors
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

#include "tfa_dsp_fw.h"
#include "tfa_service.h"
#include "tfa_internal.h"

#include "tfa98xx_tfafieldnames.h"

static enum Tfa98xx_Error tfa9890_specific(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short regRead = 0;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	/* all i2C registers are already set to default for N1C2 */

	/* some PLL registers must be set optimal for amplifier behaviour
	 */
	error = tfa98xx_write_register16(handle, 0x40, 0x5a6b);
	if (error)
		return error;
	tfa98xx_read_register16(handle, 0x59, &regRead);
	regRead |= 0x3;
	tfa98xx_write_register16(handle, 0x59, regRead);
	error = tfa98xx_write_register16(handle, 0x40, 0x0000);

	error = tfa98xx_write_register16(handle, 0x47, 0x7BE1);

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
static enum Tfa98xx_Error tfa9890_dsp_system_stable(Tfa98xx_handle_t handle, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short status, mtp0;
	int result, tries;

	/* check the contents of the STATUS register */
	result = TFA_READ_REG(handle, AREFS);
	if (result < 0) {
		error = -result;
		goto errorExit;
	}
	status = (unsigned short)result;

	/* if AMPS is set then we were already configured and running
	 *   no need to check further
	 */
	*ready = (TFA_GET_BF_VALUE(handle, AMPS, status) == 1);
	if (*ready)		/* if  ready go back */
		return error;	/* will be Tfa98xx_Error_Ok */

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(handle, AREFS, status) == 0)
		   || (TFA_GET_BF_VALUE(handle, CLKS, status) == 0));
	if (!*ready)		/* if not ready go back */
		return error;	/* will be Tfa98xx_Error_Ok */

	/* check MTPB
	 *   mtpbusy will be active when the subsys copies MTP to I2C
	 *   2 times retry avoids catching this short mtpbusy active period
	 */
	for (tries = 2; tries > 0; tries--) {
		result = TFA_GET_BF(handle, MTPB);
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

	error = tfa98xx_read_register16(handle, 0x84, &mtp0);
	if (error)
		goto errorExit;

	*ready = (mtp0 != 0);	/* The MTP register written? */

	return error;

errorExit:
	*ready = 0;
	return error;
}

/*
 * The CurrentSense4 register is not in the datasheet, define local
 */
#define TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF (1<<2)
#define TFA98XX_CURRENTSENSE4 0x49
/*
 * Disable clock gating
 */
static enum Tfa98xx_Error tfa9890_clockgating(Tfa98xx_handle_t handle, int on)
{
	enum Tfa98xx_Error error;
	unsigned short value;

	/* for TFA9890 temporarily disable clock gating when dsp reset is used */
	error = tfa98xx_read_register16(handle, TFA98XX_CURRENTSENSE4, &value);
	if (error)
		return error;

	if (Tfa98xx_Error_Ok == error) {
		if (on)  /* clock gating on - clear the bit */
			value &= ~TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;
		else  /* clock gating off - set the bit */
			value |= TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;

		error = tfa98xx_write_register16(handle, TFA98XX_CURRENTSENSE4, value);
	}

	return error;
}

/*
 * Tfa9890_DspReset will deal with clock gating control in order
 * to reset the DSP for warm state restart
 */
static enum Tfa98xx_Error tfa9890_dsp_reset(Tfa98xx_handle_t handle, int state)
{
	enum Tfa98xx_Error error;

	/* for TFA9890 temporarily disable clock gating
	   when dsp reset is used */
	tfa9890_clockgating(handle, 0);

	TFA_SET_BF(handle, RST, (uint16_t)state);

	/* clock gating restore */
	error = tfa9890_clockgating(handle, 1);

	return error;
}

/*
 * register device specifics functions
 */
void tfa9890_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9890_specific;
	ops->tfa_dsp_reset = tfa9890_dsp_reset;
	ops->tfa_dsp_system_stable = tfa9890_dsp_system_stable;
}

