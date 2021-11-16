/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SHA_1_H_
#define _SHA_1_H_

#include "mcu.h"

#include "itx_config.h"
#include "itx_typedef.h"

#ifndef HDCP_DEBUG_PRINTF
#define HDCP_DEBUG_PRINTF(x)
#endif				/* HDCP_DEBUG_PRINTF */

#ifndef HDCP_DEBUG_PRINTF1
#define HDCP_DEBUG_PRINTF1(x)
#endif				/* HDCP_DEBUG_PRINTF1 */

#ifndef HDCP_DEBUG_PRINTF2
#define HDCP_DEBUG_PRINTF2(x)
#endif				/* HDCP_DEBUG_PRINTF2 */


#ifndef DISABLE_HDCP
void SHA_Simple(void *p, WORD len, unsigned char *output);
void SHATransform(ULONG *h);
#endif

#endif				/* _SHA_1_H_ */
