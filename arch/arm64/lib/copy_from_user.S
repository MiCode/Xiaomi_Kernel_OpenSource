/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/linkage.h>

/*
 * Copy from user space to a kernel buffer (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - to
 *	x1 - from
 *	x2 - n
 * Returns:
 *	x0 - bytes not copied
 */
ENTRY(__copy_from_user)
#include "copy_template.S"
ENDPROC(__copy_from_user)

	.section .fixup,"ax"
	.align    2
9:
	/*
	 * count is accurate
	 * dst is accurate
	 */
	mov	x0, count
	sub	dst, dst, tmp1
	b	.Lfinalize

10:
	/*
	 * count is in the last 15 bytes
	 * dst is somewhere in there
	 */
	mov	x0, count
	sub	dst, limit, count
	b	.Lfinalize
11:
	/*
	 * count over counted by tmp2
	 * dst could be anywhere in there
	 */
	add	x0, count, tmp2
	sub	dst, limit, x0
	b	.Lfinalize
12:
	/*
	 * (count + 64) bytes remain
	 * dst is accurate
	 */
	adds	x0, count, #64
	b	.Lfinalize
13:
	/*
	 * (count + 128) bytes remain
	 * dst is pre-biased to (dst + 16)
	 */
	adds	x0, count, #128
	add	dst, dst, #16
	b	.Lfinalize
14:
	/*
	 * (count + 64) bytes remain
	 * dst is pre-biased to (dst + 16)
	 */
	adds	x0, count, #64
	add	dst, dst, #16

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
	.previous
