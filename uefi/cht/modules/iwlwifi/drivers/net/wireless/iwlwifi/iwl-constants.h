/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2014 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2014 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef __IWL_CONSTANTS_H
#define __IWL_CONSTANTS_H

/**
 * enum iwl_wakelock_modes - iwlwifi wakelock management mode
 * @IWL_WAKELOCK_MODE_IDLE: take wakelocks while the driver is
 *	doing some activity, and release when it's idle.
 * @IWL_WAKELOCK_MODE_OFF: don't take any wakelock.
 * @IWL_WAKELOCK_MODE_ALWAYS_ON: wake wakelock while driver is up
 *	and started (there is interface up).
 */
enum iwl_wakelock_mode {
	IWL_WAKELOCK_MODE_IDLE		= 0,
	IWL_WAKELOCK_MODE_OFF		= 1,
	IWL_WAKELOCK_MODE_ALWAYS_ON	= 2,
};

enum {
	IWL_D0I3_DBG_KEEP_BUS		= BIT(0),
	IWL_D0I3_DBG_KEEP_WAKE_LOCK	= BIT(1),
	IWL_D0I3_DBG_IGNORE_RX		= BIT(2),
};

#ifndef CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES
#ifdef CONFIG_HAS_WAKELOCK
/* wakelock timeout to use when all the references were released */
#define IWL_WAKELOCK_TIMEOUT_MS		1500
#endif /* CONFIG_HAS_WAKELOCK */
#else /* CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES */
#define IWL_D0I3_DEBUG			(trans->dbg_cfg.d0i3_debug)
#ifdef CONFIG_HAS_WAKELOCK
#define IWL_WAKELOCK_TIMEOUT_MS		(trans->dbg_cfg.WAKELOCK_TIMEOUT_MS)
#endif /* CONFIG_HAS_WAKELOCK */
#endif /* CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES */

#endif /* __IWL_CONSTANTS_H */
