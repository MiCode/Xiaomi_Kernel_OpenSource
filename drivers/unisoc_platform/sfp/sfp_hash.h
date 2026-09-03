/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef SFP_HASH_H_
#define SFP_HASH_H_

#include <net/netfilter/nf_conntrack_tuple.h>

#define SFP_ENTRIES_HASH_SIZE 2048

/*
 * sfp_rol32 - rotate a 32-bit value left
 * @word: value to rotate
 * @shift: bits to roll
 */
static inline __u32 sfp_rol32(__u32 word, unsigned int shift)
{
	return (word << shift) | (word >> ((-shift) & 31));
}

/* An arbitrary initial parameter */
#define JHASH_INITVAL		0xdeadbeef

static inline u32 sfp_jhash2(const u32 *k, u32 length, u32 initval)
{
	u32 a, b, c;

	/* Set up the internal state */
	a = JHASH_INITVAL + (length << 2) + initval;
	b = a;
	c = a;

	/* Handle most of the key */
	while (length > 3) {
		a += k[0];
		b += k[1];
		c += k[2];

		a -= c;  a ^= sfp_rol32(c, 4);  c += b;
		b -= a;  b ^= sfp_rol32(a, 6);  a += c;
		c -= b;  c ^= sfp_rol32(b, 8);  b += a;
		a -= c;  a ^= sfp_rol32(c, 16); c += b;
		b -= a;  b ^= sfp_rol32(a, 19); a += c;
		c -= b;  c ^= sfp_rol32(b, 4);  b += a;

		length -= 3;
		k += 3;
	}

	/* Handle the last 3 u32's: all the case statements */
	switch (length) {
	case 3:
		c += k[2];
		fallthrough;
	case 2:
		b += k[1];
		fallthrough;
	case 1:
		a += k[0];
		c ^= b; c -= sfp_rol32(b, 14);
		a ^= c; a -= sfp_rol32(c, 11);
		b ^= a; b -= sfp_rol32(a, 25);
		c ^= b; c -= sfp_rol32(b, 16);
		a ^= c; a -= sfp_rol32(c, 4);
		b ^= a; b -= sfp_rol32(a, 14);
		c ^= b; c -= sfp_rol32(b, 24);
		fallthrough;
	case 0:	/* Nothing left to add */
		break;
	}

	return c;
}

static inline u32 sfp_hash_conntrack(const struct nf_conntrack_tuple *tuple)
{
	unsigned int n;
	u32 sum;
	/* The direction must be ignored, so we hash everything up to the
	 * destination ports (which is a multiple of 4) and treat the last
	 * three bytes manually.
	 */
	n = (sizeof(tuple->src) + sizeof(tuple->dst.u3)) / sizeof(u32);
	sum =  sfp_jhash2((u32 *)tuple, n,
			  (((__force __u16)tuple->dst.u.all << 16) |
			  tuple->dst.protonum));
	sum = sum & (SFP_ENTRIES_HASH_SIZE - 1);
	return sum;
}
#endif
