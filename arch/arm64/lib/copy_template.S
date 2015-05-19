/*
 * Copyright (c) 2013, Applied Micro Circuits Corporation
 * Copyright (c) 2012-2013, Linaro Limited
 *
 * Author: Feng Kan <fkan@xxxxxxx>
 * Author: Philipp Tomsich <philipp.tomsich@xxxxxxxxxxxxxxxxxxxxx>
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
#include <asm/cache.h>

dstin	.req x0
src	.req x1
count	.req x2
tmp1	.req x3
tmp1w	.req w3
tmp2	.req x4
tmp2w	.req w4
limit	.req x5
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
	add	limit, dst, count
	cmp	count, #16
	b.lo	.Ltail15

	/*
	 * We don't much care about the alignment of DST, but we want SRC
	 * to be 128-bit (16 byte) aligned so that we don't cross cache line
	 * boundaries on both loads and stores.
	 */
	ands	tmp2, src, #15
	b.eq	.LSrcAligned
	sub	count, count, tmp2

	tbz	tmp2, #0, 1f
	USER(11f, ldrb	tmp1w, [src], #1)
	USER(11f, strb	tmp1w, [dst], #1)
1:
	tbz	tmp2, #1, 2f
	USER(11f, ldrh	tmp1w, [src], #2)
	USER(11f, strh	tmp1w, [dst], #2)
2:
	tbz	tmp2, #2, 3f
	USER(11f, ldr	tmp1w, [src], #4)
	USER(11f, str	tmp1w, [dst], #4)
3:
	tbz	tmp2, #3, .LSrcAligned
	USER(11f, ldr	tmp1, [src], #8)
	USER(11f, str	tmp1, [dst], #8)

.LSrcAligned:
	/* There may be less than 63 bytes to go now.  */
	cmp	count, #64
	b.ge	.Lcpy_over64

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
	USER(9f, ldp A_l, A_h, [src, #-48])
	USER(9f, stp A_l, A_h, [dst, #-48])
1:
	USER(9f, ldp A_l, A_h, [src, #-32])
	USER(9f, stp A_l, A_h, [dst, #-32])
2:
	USER(9f, ldp A_l, A_h, [src, #-16])
	USER(9f, stp A_l, A_h, [dst, #-16])

.Ltail15:
	ands	count, count, #15
	beq	.Lsuccess	/* Quick exit if we are done*/
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
	USER(10f, ldrb tmp1w, [src], #1)
	USER(10f, strb tmp1w, [dst], #1)
1:
	b	.Lsuccess

.Lcpy_over64:
	subs	count, count, #128
	b.ge	.Lcpy_body_large
	/*
	 * Less than 128 bytes to copy, so handle 64 here and then jump
	 * to the tail.
	 */
	adds	count, count, #64
	USER(12f, ldp A_l, A_h, [src])
	USER(12f, ldp B_l, B_h, [src, #16])
	USER(12f, ldp C_l, C_h, [src, #32])
	USER(12f, ldp D_l, D_h, [src, #48])
	USER(12f, stp A_l, A_h, [dst])
	USER(12f, stp B_l, B_h, [dst, #16])
	USER(12f, stp C_l, C_h, [dst, #32])
	USER(12f, stp D_l, D_h, [dst, #48])
	add	src, src, #64
	add	dst, dst, #64
	tst	count, #0x3f
	b.ne	.Ltail63
	b	.Lsuccess

	/*
	 * Critical loop.  Start at a new cache line boundary.  Assuming
	 * 64 bytes per line this ensures the entire loop is in one line.
	 */
	.p2align L1_CACHE_SHIFT
.Lcpy_body_large:
	/* There are at least 128 bytes to copy.  */
	sub	dst, dst, #16			/* Pre-bias.  */
	USER(13f, ldp A_l, A_h, [src, #0])
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
	USER(14f, stp D_l, D_h, [dst, #64]!)
	USER(14f, ldp D_l, D_h, [src, #64]!)
	subs	count, count, #64
	b.ge	1b
	USER(13f, stp A_l, A_h, [dst, #16])
	USER(13f, stp B_l, B_h, [dst, #32])
	USER(13f, stp C_l, C_h, [dst, #48])
	USER(13f, stp D_l, D_h, [dst, #64])
	add	src, src, #16
	add	dst, dst, #80		/* 64 bytes + 16 prebias */
	adds	count, count, #64
	tst	count, #0x3f
	b.ne	.Ltail63
.Lsuccess:
	/* Nothing left to copy */
	mov	x0, #0
	ret
