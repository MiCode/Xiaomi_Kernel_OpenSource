/*
 * Copyright (c) 2013, Applied Micro Circuits Corporation
 * Copyright (c) 2012-2013, Linaro Limited
 *
 * Author: Feng Kan <fkan@apm.com>
 * Author: Philipp Tomsich <philipp.tomsich@theobroma-systems.com>
 *
 * The code is adopted from the memcpy routine by Linaro Limited.
 *
 * This file is free software: you may copy, redistribute and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or (at your
 * option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *      1 Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      2 Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      3 Neither the name of the Linaro nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <asm/assembler.h>

dstin	.req x0
src	.req x1
count	.req x2
tmp1	.req x3
tmp1w	.req w3
tmp2	.req x4
tmp2w	.req w4
tmp3	.req x5
tmp3w	.req w5
dst	.req x6

A_l	.req x7
A_h	.req x8
B_l	.req x9
B_h	.req x10
C_l	.req x11
C_h	.req x12
D_l	.req x13
D_h	.req x14

	mov	dst, dstin
	cmp	count, #64
	b.ge	.Lcpy_not_short
	cmp	count, #15
	b.le	.Ltail15tiny

	/*
	 * Deal with small copies quickly by dropping straight into the
	 * exit block.
	 */
.Ltail63:
	/*
	 * Copy up to 48 bytes of data.  At this point we only need the
	 * bottom 6 bits of count to be accurate.
	 */
	ands	tmp1, count, #0x30
	b.eq	.Ltail15
	add	dst, dst, tmp1
	add	src, src, tmp1
	cmp	tmp1w, #0x20
	b.eq	1f
	b.lt	2f
	USER(8f, ldp A_l, A_h, [src, #-48])
	USER(8f, stp A_l, A_h, [dst, #-48])
1:
	USER(8f, ldp A_l, A_h, [src, #-32])
	USER(8f, stp A_l, A_h, [dst, #-32])
2:
	USER(8f, ldp A_l, A_h, [src, #-16])
	USER(8f, stp A_l, A_h, [dst, #-16])

.Ltail15:
	ands	count, count, #15
	beq	1f
	add	src, src, count
	USER(9f, ldp A_l, A_h, [src, #-16])
	add	dst, dst, count
	USER(9f, stp A_l, A_h, [dst, #-16])
1:
	b	.Lsuccess

.Ltail15tiny:
	/*
	 * Copy up to 15 bytes of data.  Does not assume additional data
	 * being copied.
	 */
	tbz	count, #3, 1f
	USER(10f, ldr tmp1, [src], #8)
	USER(10f, str tmp1, [dst], #8)
1:
	tbz	count, #2, 1f
	USER(10f, ldr tmp1w, [src], #4)
	USER(10f, str tmp1w, [dst], #4)
1:
	tbz	count, #1, 1f
	USER(10f, ldrh tmp1w, [src], #2)
	USER(10f, strh tmp1w, [dst], #2)
1:
	tbz	count, #0, 1f
	USER(10f, ldrb tmp1w, [src])
	USER(10f, strb tmp1w, [dst])
1:
	b	.Lsuccess

.Lcpy_not_short:
	/*
	 * We don't much care about the alignment of DST, but we want SRC
	 * to be 128-bit (16 byte) aligned so that we don't cross cache line
	 * boundaries on both loads and stores.
	 */
	neg	tmp2, src
	ands	tmp2, tmp2, #15		/* Bytes to reach alignment.  */
	b.eq	2f
	sub	count, count, tmp2
	/*
	 * Copy more data than needed; it's faster than jumping
	 * around copying sub-Quadword quantities.  We know that
	 * it can't overrun.
	 */
	USER(11f, ldp A_l, A_h, [src])
	add	src, src, tmp2
	USER(11f, stp A_l, A_h, [dst])
	add	dst, dst, tmp2
	/* There may be less than 63 bytes to go now.  */
	cmp	count, #63
	b.le	.Ltail63
2:
	subs	count, count, #128
	b.ge	.Lcpy_body_large
	/*
	 * Less than 128 bytes to copy, so handle 64 here and then jump
	 * to the tail.
	 */
	USER(12f, ldp A_l, A_h, [src])
	USER(12f, ldp B_l, B_h, [src, #16])
	USER(12f, ldp C_l, C_h, [src, #32])
	USER(12f, ldp D_l, D_h, [src, #48])
	USER(12f, stp A_l, A_h, [dst])
	USER(12f, stp B_l, B_h, [dst, #16])
	USER(12f, stp C_l, C_h, [dst, #32])
	USER(12f, stp D_l, D_h, [dst, #48])
	tst	count, #0x3f
	add	src, src, #64
	add	dst, dst, #64
	b.ne	.Ltail63
	b	.Lsuccess

	/*
	 * Critical loop.  Start at a new cache line boundary.  Assuming
	 * 64 bytes per line this ensures the entire loop is in one line.
	 */
	.p2align 6
.Lcpy_body_large:
	/* There are at least 128 bytes to copy.  */
	USER(12f, ldp A_l, A_h, [src, #0])
	sub	dst, dst, #16			/* Pre-bias.  */
	USER(13f, ldp B_l, B_h, [src, #16])
	USER(13f, ldp C_l, C_h, [src, #32])
	USER(13f, ldp D_l, D_h, [src, #48]!)	/* src += 64 - Pre-bias. */
1:
	USER(13f, stp A_l, A_h, [dst, #16])
	USER(13f, ldp A_l, A_h, [src, #16])
	USER(13f, stp B_l, B_h, [dst, #32])
	USER(13f, ldp B_l, B_h, [src, #32])
	USER(13f, stp C_l, C_h, [dst, #48])
	USER(13f, ldp C_l, C_h, [src, #48])
	USER(13f, stp D_l, D_h, [dst, #64]!)
	USER(13f, ldp D_l, D_h, [src, #64]!)
	subs	count, count, #64
	b.ge	1b
	USER(14f, stp A_l, A_h, [dst, #16])
	USER(14f, stp B_l, B_h, [dst, #32])
	USER(14f, stp C_l, C_h, [dst, #48])
	USER(14f, stp D_l, D_h, [dst, #64])
	add	src, src, #16
	add	dst, dst, #64 + 16
	tst	count, #0x3f
	b.ne	.Ltail63
.Lsuccess:
	/* Nothing left to copy */
	mov	x0, #0
	ret

	.macro	copy_abort_table
8:
	/*
	 * Count bytes remain
	 * dst points to (dst + tmp1)
	 */
	mov	x0, count
	sub	dst, dst, tmp1
	b	.Lfinalize
9:
	/*
	 * 16 bytes remain
	 * dst is accurate
	 */
	mov	x0, #16
	b	.Lfinalize
10:
	/*
	 * count is accurate
	 * dst is accurate
	 */
	mov	x0, count
	b	.Lfinalize
11:
	/*
	 *(count + tmp2) bytes remain
	 * dst points to the start of the remaining bytes
	 */
	add	x0, count, tmp2
	b	.Lfinalize
12:
	/*
	 * (count + 128) bytes remain
	 * dst is accurate
	 */
	add	x0, count, #128
	b	.Lfinalize
13:
	/*
	 * (count + 128) bytes remain
	 * dst is pre-biased to (dst + 16)
	 */
	add	x0, count, #128
	sub	dst, dst, #16
	b	.Lfinalize
14:
	/*
	 * count is accurate
	 * dst is pre-biased to (dst + 16)
	 */
	mov	x0, count
	sub	dst, dst, #16
	/* fall-through */
.Lfinalize:
	/*
	 * Zeroize remaining destination-buffer
	 */
	mov	count, x0
20:
	/* Zero remaining buffer space */
	strb	wzr, [dst], #1
	subs	count, count, #1
	b.ne	20b
	ret
	.endm
