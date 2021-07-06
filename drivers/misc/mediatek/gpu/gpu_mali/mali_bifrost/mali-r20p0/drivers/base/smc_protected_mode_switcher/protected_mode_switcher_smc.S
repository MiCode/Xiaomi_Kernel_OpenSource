/*
 *
 * (C) COPYRIGHT 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

#include <linux/linkage.h>

/* u64 invoke_protected_mode_switch_smc(u64 function_id, u64 arg0, u64 arg1,
		u64 arg2) */
ENTRY(__invoke_protected_mode_switch_smc)
	smc	#0
	ret
ENDPROC(__invoke_protected_mode_switch_smc)
