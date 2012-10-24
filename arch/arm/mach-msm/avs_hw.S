/*
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

	.text

#ifdef CONFIG_MSM_CPU_AVS
	.global avs_test_delays
avs_test_delays:

/*      Read r1=CPMR and enable Never Sleep for VSLPDLY */
		mrc  p15, 7, r1, c15, c0, 5
		orr  r12, r1, #3, 24
		mcr  p15, 7, r12, c15, c0, 5

/*      Read r2=CPACR and enable full access to CP10 and CP11 space */
		mrc p15, 0, r2, c1, c0, 2
		orr r12, r2, #(0xf << 20)
		mcr p15, 0, r12, c1, c0, 2
		isb

/*      Read r3=FPEXC and or in FP enable, VFP/ASE enable = FPEXC[30]; */
		fmrx r3, fpexc
		orr  r12, r3, #1, 2
		fmxr fpexc, r12

/*
 *      Do floating-point operations to prime the VFP pipeline.   Use
 *      fcpyd d0, d0 as a floating point nop.  This avoids changing VFP
 *      state.
 */
		fcpyd d0, d0
		fcpyd d0, d0
		fcpyd d0, d0

/*      Read r0=AVSCSR to get status from CPU, VFP, and L2 ring oscillators */
		mrc p15, 7, r0, c15, c1, 7

/*      Restore FPEXC */
		fmxr fpexc, r3

/*      Restore CPACR */
                MCR p15, 0, r2, c1, c0, 2

/*      Restore CPMR */
		mcr p15, 7, r1, c15, c0, 5
                isb

		bx lr
#endif


	.global avs_get_avscsr
/*      Read r0=AVSCSR to get status from CPU, VFP, and L2 ring oscillators */

avs_get_avscsr:
		mrc p15, 7, r0, c15, c1, 7
                bx lr

        .global avs_get_avsdscr
/*      Read r0=AVSDSCR to get the AVS Delay Synthesizer control settings */

avs_get_avsdscr:
		mrc p15, 7, r0, c15, c0, 6
                bx lr




	.global avs_get_tscsr
/*      Read r0=TSCSR to get temperature sensor control and status */

avs_get_tscsr:
		mrc p15, 7, r0, c15, c1, 0
                bx lr

        .global avs_set_tscsr
/*      Write TSCSR=r0 to set temperature sensor control and status  */

avs_set_tscsr:
		mcr p15, 7, r0, c15, c1, 0
                bx lr





	.global avs_reset_delays
avs_reset_delays:

/*      AVSDSCR(dly) to program delay */
		mcr p15, 7, r0, c15, c0, 6

/*      Read r0=AVSDSCR */
		mrc p15, 7, r0, c15, c0, 6
		bx lr

	.global avs_enable
avs_enable:
/*	Restore the avs_scr register */
		mcr p15, 7, r0, c15, c1, 7
		bx lr

        .global avs_disable
avs_disable:

/*	Get the AVSCSR value */
		mrc p15, 7, r0, c15, c1, 7
/*      Clear AVSCSR */
		mov r1, #0
/*      Write AVSCSR */
		mcr p15, 7, r1, c15, c1, 7

		bx lr

	.end


