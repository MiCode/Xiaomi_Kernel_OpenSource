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

static enum Tfa98xx_Error tfa9891_specific(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	/* ----- generated code start ----- */
	/* -----  version 18.0 ----- */
	tfa98xx_write_register16(handle, 0x09, 0x025d); /* POR=0x024d */
	tfa98xx_write_register16(handle, 0x10, 0x0018); /* POR=0x0024 */
	tfa98xx_write_register16(handle, 0x22, 0x0003); /* POR=0x0023 */
	tfa98xx_write_register16(handle, 0x25, 0x0001); /* POR=0x0000 */
	tfa98xx_write_register16(handle, 0x46, 0x0000); /* POR=0x4000 */
	tfa98xx_write_register16(handle, 0x55, 0x3ffb); /* POR=0x7fff */
	/* ----- generated code end   ----- */

	return error;
}

/*
 * register device specifics functions
 */
void tfa9891_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9891_specific;

}
