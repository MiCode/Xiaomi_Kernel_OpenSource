/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

        .global write_cntpctl
write_cntpctl:
        .func
        msr	CNTP_CTL_EL0, x0
        ret
        .endfunc

        .global read_cntpctl
read_cntpctl:
        .func
        mrs	x0, CNTP_CTL_EL0
        ret
        .endfunc

        .global read_cpu_id
read_cpu_id:
        .func
        mrs	x0, MPIDR_EL1
        ands	x0, x0, #0xff
        ret
        .endfunc

        .global read_cluster_id
read_cluster_id:
        .func
        mrs	x0, MPIDR_EL1
        lsr	x0, x0, #0x8
        ands	x0, x0, #0xff
        ret
        .endfunc

				.global mt_save_generic_timer
mt_save_generic_timer:
				.func
				mrs	x2, CNTKCTL_EL1
				mrs	x3, CNTP_CVAL_EL0
				stp	x2, x3, [x0, #0]
				mrs	x2, CNTP_TVAL_EL0
				mrs	x3, CNTP_CTL_EL0
				stp	x2, x3, [x0, #16]
				mrs	x2, CNTV_TVAL_EL0
				mrs	x3, CNTV_CTL_EL0
				stp	x2, x3, [x0, #32]
				ret
				.endfunc

				.global mt_restore_generic_timer
mt_restore_generic_timer:
				.func
				ldp	x2, x3, [x0, #0]
				msr	CNTKCTL_EL1, x2
				msr	CNTP_CVAL_EL0, x3
				ldp	x2, x3, [x0, #16]
				msr	CNTP_TVAL_EL0, x2
				msr	CNTP_CTL_EL0, x3
				ldp	x2, x3, [x0, #32]
				msr	CNTV_TVAL_EL0, x2
				msr	CNTV_CTL_EL0, x3
				ret
				.endfunc

