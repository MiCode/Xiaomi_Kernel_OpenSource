/*
  Copyright (C) 2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
/**
 * This code was gotten from the freshest kernel's sources (3.4 branch)
 * for compatibility with old kernels.
 */
#include <linux/ctype.h>

/**
 * __bitmap_parselist - convert list format ASCII string to bitmap
 * @buf: read nul-terminated user string from this buffer
 * @buflen: buffer size in bytes.  If string is smaller than this
 *    then it must be terminated with a \0.
 * @is_user: location of buffer, 0 indicates kernel space
 * @maskp: write resulting mask here
 * @nmaskbits: number of bits in mask to be written
 *
 * Input format is a comma-separated list of decimal numbers and
 * ranges.  Consecutively set bits are shown as two hyphen-separated
 * decimal numbers, the smallest and largest bit numbers set in
 * the range.
 *
 * Returns 0 on success, -errno on invalid input strings.
 * Error values:
 *    %-EINVAL: second number in range smaller than first
 *    %-EINVAL: invalid character in string
 *    %-ERANGE: bit number specified too large for mask
 */
static int __bitmap_parselist(const char *buf, unsigned int buflen,
		int is_user, unsigned long *maskp,
		int nmaskbits)
{
	unsigned a, b;
	int c, old_c, totaldigits;
	const char __user __force *ubuf = (const char __user __force *)buf;
	int exp_digit, in_range;

	totaldigits = c = 0;
	bitmap_zero(maskp, nmaskbits);
	do {
		exp_digit = 1;
		in_range = 0;
		a = b = 0;

		/* Get the next cpu# or a range of cpu#'s */
		while (buflen) {
			old_c = c;
			if (is_user) {
				if (__get_user(c, ubuf++))
					return -EFAULT;
			} else
				c = *buf++;
			buflen--;
			if (isspace(c))
				continue;

			/*
			 * If the last character was a space and the current
			 * character isn't '\0', we've got embedded whitespace.
			 * This is a no-no, so throw an error.
			 */
			if (totaldigits && c && isspace(old_c))
				return -EINVAL;

			/* A '\0' or a ',' signal the end of a cpu# or range */
			if (c == '\0' || c == ',')
				break;

			if (c == '-') {
				if (exp_digit || in_range)
					return -EINVAL;
				b = 0;
				in_range = 1;
				exp_digit = 1;
				continue;
			}

			if (!isdigit(c))
				return -EINVAL;

			b = b * 10 + (c - '0');
			if (!in_range)
				a = b;
			exp_digit = 0;
			totaldigits++;
		}
		if (!(a <= b))
			return -EINVAL;
		if (b >= nmaskbits)
			return -ERANGE;
		while (a <= b) {
			set_bit(a, maskp);
			a++;
		}
	} while (buflen && c == ',');
	return 0;
}

/**
 * bitmap_parselist_user()
 *
 * @ubuf: pointer to user buffer containing string.
 * @ulen: buffer size in bytes.  If string is smaller than this
 *    then it must be terminated with a \0.
 * @maskp: pointer to bitmap array that will contain result.
 * @nmaskbits: size of bitmap, in bits.
 *
 * Wrapper for bitmap_parselist(), providing it with user buffer.
 *
 * We cannot have this as an inline function in bitmap.h because it needs
 * linux/uaccess.h to get the access_ok() declaration and this causes
 * cyclic dependencies.
 */
static inline int bitmap_parselist_user(const char __user *ubuf,
                        unsigned int ulen, unsigned long *maskp,
                        int nmaskbits)
{
        if (!access_ok(VERIFY_READ, ubuf, ulen))
                return -EFAULT;
        return __bitmap_parselist((const char __force *)ubuf,
                                        ulen, 1, maskp, nmaskbits);
}

/**
 * cpumask_parselist_user - extract a cpumask from a user string
 * @buf: the buffer to extract from
 * @len: the length of the buffer
 * @dstp: the cpumask to set.
 *
 * Returns -errno, or 0 for success.
 */
static inline int cpumask_parselist_user(const char __user *buf, int len,
                                     struct cpumask *dstp)
{
        return bitmap_parselist_user(buf, len, cpumask_bits(dstp),
                                                        nr_cpumask_bits);
}
