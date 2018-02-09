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

static enum Tfa98xx_Error tfa9897_specific(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short check_value;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	/* all i2C registers must already set to default POR value */

	/* $48:[3] - 1 ==> 0; iddqtestbst - default value changed.
	* When Iddqtestbst is set to "0", the slewrate is reduced.
	* This will lower the overshoot on IN-B to avoid NMOS damage of booster.
	*/
	error = tfa98xx_write_register16(handle, 0x48, 0x0300); /* POR value = 0x308 */

	/* $49:[0] - 1 ==> 0; CLIP - default value changed. 0 means CLIPPER on
	*/
	error = tfa98xx_read_register16(handle, 0x49, &check_value);
	check_value &= ~0x1;
	error = tfa98xx_write_register16(handle, 0x49, check_value);

	return error;
}

/*
 * the int24 values for the vsfw delay table
 */
static unsigned char vsfwdelay_table[] = {
	0, 0, 2,		/*Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0,		/*Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0,		/*Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 2,		/*Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 2,		/*Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 2,		/*Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 2,		/*Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2,		/*Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 3			/*Index 8 - Current/Volt Fractional Delay for 48KHz */
};

/*
 * TODO make this tfa98xx
 *  Note that the former products write this table via the patch
 *  so moving this to the tfa98xx API requires also updating all patches
 */
static enum Tfa98xx_Error tfa9897_dsp_write_vsfwdelay_table(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error;

	error = tfa_dsp_cmd_id_write(handle, MODULE_FRAMEWORK,
				TFA1_FW_PAR_ID_SET_CURRENT_DELAY,
				sizeof(vsfwdelay_table),
				vsfwdelay_table);
	return error;
}

/*
 * The int24 values for the fracdelay table
 * For now applicable only for 8 and 48 kHz
 */
static unsigned char cvfracdelay_table[] = {
	0, 0, 51,		/*Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0,		/*Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0,		/*Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 38,		/*Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 34,		/*Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 33,		/*Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 11,		/*Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2,		/*Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 62		/*Index 8 - Current/Volt Fractional Delay for 48KHz */
};

enum Tfa98xx_Error tfa9897_dsp_write_cvfracdelay_table(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error;

	error = tfa_dsp_cmd_id_write(handle, MODULE_FRAMEWORK,
				TFA1_FW_PAR_ID_SET_CURFRAC_DELAY,
				sizeof(cvfracdelay_table),
				cvfracdelay_table);
	return error;
}

static enum Tfa98xx_Error tfa9897_tfa_dsp_write_tables(Tfa98xx_handle_t dev_idx, int sample_rate)
{
	enum Tfa98xx_Error error;

	/* Not used for max1! */
	sample_rate = sample_rate;

	error = tfa9897_dsp_write_vsfwdelay_table(dev_idx);
	if (error == Tfa98xx_Error_Ok)
		error = tfa9897_dsp_write_cvfracdelay_table(dev_idx);

	tfa98xx_dsp_reset(dev_idx, 1);
	tfa98xx_dsp_reset(dev_idx, 0);

	return error;
}

/*
 * register device specifics functions
 */
void tfa9897_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9897_specific;
	ops->tfa_dsp_write_tables = tfa9897_tfa_dsp_write_tables;
}
