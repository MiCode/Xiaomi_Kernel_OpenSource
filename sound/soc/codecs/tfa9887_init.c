/*
 *Copyright 2014,215 NXP Semiconductors
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

static enum Tfa98xx_Error tfa9887_specific(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int result;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	/* all i2C registers are already set to default */

	result = TFA_SET_BF(handle, AMPE, 1);
	if (result < 0)
		return -result;

	/* some other registers must be set for optimal amplifier behaviour */
	tfa98xx_write_register16(handle, 0x05, 0x13AB);
	tfa98xx_write_register16(handle, 0x06, 0x001F);
	tfa98xx_write_register16(handle, 0x08, 0x3C4E);
	/*TFA98XX_SYSCTRL_DCA=0*/
	tfa98xx_write_register16(handle, 0x09, 0x024D);
	tfa98xx_write_register16(handle, 0x0A, 0x3EC3);
	tfa98xx_write_register16(handle, 0x41, 0x0308);
	error = tfa98xx_write_register16(handle, 0x49, 0x0E82);

	return error;
}

/*
 * register device specifics functions
 */
void tfa9887_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9887_specific;
}
