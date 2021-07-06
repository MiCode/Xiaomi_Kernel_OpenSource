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

#ifndef _HDMITX_H_
#define _HDMITX_H_
/*#define unsigned char unsigned char*/
#include "IO.h"
#include "utility.h"

#include "debug_hdmi.h"
#include "hdmitx_drv.h"
#include "itx_config.h"
#include "itx_typedef.h"

#define HDMITX_MAX_DEV_COUNT 1

/* ///////////////////////////////////////////////////////////////////// */
/* Output Mode Type */
/* ///////////////////////////////////////////////////////////////////// */

#define RES_ASPEC_4x3 0
#define RES_ASPEC_16x9 1
#define F_MODE_REPT_NO 0
#define F_MODE_REPT_TWICE 1
#define F_MODE_REPT_QUATRO 3
#define F_MODE_CSC_ITU601 0
#define F_MODE_CSC_ITU709 1

#define TIMER_LOOP_LEN 10
#define MS(x) (((x) + (TIMER_LOOP_LEN - 1)) / TIMER_LOOP_LEN)
/* for timer loop */

/* #define SUPPORT_AUDI_AudSWL 16 // Jeilin case. */
#define SUPPORT_AUDI_AudSWL 24 /* Jeilin case. */

#if (SUPPORT_AUDI_AudSWL == 16)
#define CHTSTS_SWCODE 0x02
#elif (SUPPORT_AUDI_AudSWL == 18)
#define CHTSTS_SWCODE 0x04
#elif (SUPPORT_AUDI_AudSWL == 20)
#define CHTSTS_SWCODE 0x03
#else
#define CHTSTS_SWCODE 0x0B
#endif

#endif /* _HDMITX_H_ */
