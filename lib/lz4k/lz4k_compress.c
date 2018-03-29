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

/*
 *  LZ4K Compressor by Vovo
 */
#include <linux/lz4k.h>
#include <linux/types.h>
#include <linux/string.h>

unsigned short lz4k_matchlen_encode[32] = {
	0, 0, 0, 1024, 1026, 2049, 2053, 2057, 2061, 3091, 3123, 3083, 2563, 3643, 3707, 3115, 3099,
4119, 3591, 4247, 3655, 4791, 4183, 4311, 3623, 5047, 4727, 4983, 3687, 4855, 5111, 4151, };

unsigned short lz4k_matchoff_encode[32] = {
	1024, 1032, 1292, 1028, 1554, 1308, 1586, 1546, 1578, 1562, 1830, 1594, 1894, 1282, 1814,
1542, 2158, 1878, 2286, 1846, 2078, 1910, 2206, 1806, 2142, 2270, 2110, 1870, 2238, 1838, 2174, 2302, };

unsigned short lz4k_literallen_encode[18] = {
	0, 128, 385, 517, 525, 515, 651, 775, 807, 667, 919, 983, 951, 1015, 911, 975, 943, 1007, };

unsigned short lz4k_literalch_encode[256] = {
	2048, 3092, 3596, 3660, 3628, 4154, 4282, 4218, 3692, 3612, 3676, 4346, 4102, 4230, 4166,
4294, 3644, 4134, 4262, 4198, 4326, 4861, 5117, 4611, 4118, 4867, 4246, 4182, 4310, 4150, 4278, 4214, 3124, 4342, 4110,
4238, 4174, 4302, 4739, 4995, 3708, 4142, 4675, 4931, 4270, 4803, 4206, 4334, 3586, 4126, 4254, 4190, 4318, 5059, 4643,
4899, 4158, 4771, 5027, 4707, 4963, 4835, 5091, 4627, 2564, 3650, 4286, 4222, 4350, 4883, 4755, 5011, 4097, 4691, 4947,
4225, 4161, 4289, 4129, 4257, 3618, 3682, 4193, 4321, 4113, 4819, 5075, 4659, 4241, 4915, 4787, 5043, 4723, 4979, 4851,
4177, 4305, 3602, 4145, 4273, 4209, 3666, 4337, 4105, 4233, 3634, 5107, 4619, 4169, 4297, 3698, 3594, 3658, 4137, 3626,
4265, 3690, 4201, 4329, 4875, 4121, 4249, 4747, 5003, 4683, 4939, 4811, 5067, 3610, 4651, 4907, 4779, 4185, 5035, 4715,
4971, 4313, 4843, 5099, 4635, 4891, 4763, 5019, 4699, 4153, 4955, 4827, 5083, 4667, 4923, 4795, 5051, 4281, 4731, 4987,
4859, 5115, 4615, 4871, 4743, 4217, 4999, 4679, 4935, 4807, 5063, 4647, 4903, 4345, 4775, 5031, 4711, 4967, 4839, 5095,
4631, 4101, 4887, 4759, 5015, 4229, 4695, 4951, 4823, 4165, 5079, 4663, 4919, 4293, 4791, 5047, 4727, 4133, 4983, 4855,
5111, 4623, 4879, 4751, 5007, 4261, 4687, 4943, 4815, 5071, 4655, 4911, 4783, 4197, 4325, 4117, 4245, 4181, 4309, 5039,
4719, 4149, 4975, 4847, 5103, 4639, 4895, 4767, 5023, 3674, 4277, 4703, 4213, 4959, 4341, 4831, 5087, 4109, 4671, 4927,
4799, 4237, 4173, 4301, 5055, 4141, 4269, 4205, 4333, 4125, 4253, 4735, 4189, 4317, 4157, 4285, 4991, 4221, 4863, 5119,
2056, };

#define RESERVE_16_BITS() \
{if (bitstobeoutput >= 16) { \
	*((unsigned short *)op) = (unsigned short) (bits_buffer32 & 0xffff); \
	op += 2; \
	bits_buffer32 = bits_buffer32 >> 16; \
	bitstobeoutput -= 16; \
} }

#define STORE_BITS(bits, code) do { bits_buffer32 |= (code) << bitstobeoutput; bitstobeoutput += (bits); } while (0)
static size_t
_lz4k_do_compress_zram(const unsigned char *in, size_t in_len,
	unsigned char *out, size_t *out_len, void *wrkmem, int *checksum)
{
	const unsigned char *ip = in;
	unsigned char *op = out;
	const unsigned char * const in_end = in + in_len;
	const unsigned char * const ip_end = in + in_len - 3;
	const unsigned char *ii = ip;
	const unsigned char ** const dict = wrkmem;
	unsigned int bitstobeoutput = 0;
	unsigned int bits_buffer32 = 0;
	int hash = (int)0x4e4de069; /* initial hash value chosen randomly */

	bitstobeoutput = 1;
	for (;;) {
		const unsigned char *m_pos;
		{
			size_t dindex;
			unsigned int ip_content = *(unsigned int *)ip;
			unsigned int hash_temp = ip_content ^ (ip_content >> 12);

			dindex = hash_temp & 0xfff;
			m_pos = dict[dindex];
			dict[dindex] = ip;

			if (m_pos < in || m_pos >= ip ||
			((*(unsigned int *)m_pos << 8) != (ip_content << 8))) {
				++ip;
				dindex = (hash_temp >> 8) & 0xfff;
				m_pos = dict[dindex];
				dict[dindex] = ip;
				if (m_pos < in || m_pos >= ip ||
					((*(unsigned int *)m_pos << 8) !=
					(ip_content & 0xffffff00))) {
					++ip;
					if (__builtin_expect(!!(ip >= ip_end), 0))
						break;
					continue;
				}
			}
		}
		hash = ((hash << 5) + hash) + (int)bits_buffer32;
		{
		size_t lit = ip - ii;

		if (lit <= 0) {
			if (bitstobeoutput == 32) {
				*((unsigned int *)op) = bits_buffer32;
				op += 4;
				bits_buffer32 = 1;
				bitstobeoutput = 1;
			} else {
				bits_buffer32 |= 1 << bitstobeoutput;
				bitstobeoutput += 1;
			}
		} else if (lit == 1) {
			int value, bits, code;

			RESERVE_16_BITS();
			value = lz4k_literalch_encode[*ii++];
			bits = value >> 9;
			code = (value & 0x1ff) << 2;
			STORE_BITS(bits + 2, code);
		} else if (lit == 2) {
			int value, bits, code;
			int value2, bits2, code2;

			RESERVE_16_BITS();
			if (bitstobeoutput > (32 - 22)) {
				*op++ = (unsigned char) (bits_buffer32 & 0xff);
				bits_buffer32 = bits_buffer32 >> 8;
				bitstobeoutput -= 8;
			}
			value = lz4k_literalch_encode[*ii++];
			bits = value >> 9;
			code = value & 0x1ff;
			value2 = lz4k_literalch_encode[*ii++];
			bits2 = value2 >> 9;
			code2 = value2 & 0x1ff;
			bits_buffer32 |=
			((((code2 << bits) | code) << 4) | 2) << bitstobeoutput;
			bitstobeoutput += bits2 + bits + 4;
		} else {
			if (lit <= 17) {
				int value, bits, code;

				RESERVE_16_BITS();
				value = lz4k_literallen_encode[lit];
				bits = value >> 7;
				code = (value & 0x7f) << 1;
				STORE_BITS(bits + 1, code);
			} else {
				int code = ((lit - 1) << 6) | 0x3e;

				RESERVE_16_BITS();
				if (bitstobeoutput > (32 - 18)) {
					*op++ =
					(unsigned char) (bits_buffer32 & 0xff);
					bits_buffer32 = bits_buffer32 >> 8;
					bitstobeoutput -= 8;
				}
				STORE_BITS(17 + 1, code);
			}
			while (1) {
				while (bitstobeoutput < 24) {
					int value, bits, code;

					value = lz4k_literalch_encode[*ii++];
					bits = value >> 9;
					code = value & 0x1ff;
					STORE_BITS(bits, code);
					if (__builtin_expect(!!(ii == ip), 0))
						goto break_literal_1;
				}
				/* update hash */
				hash += (int)bits_buffer32;
				*((unsigned int *)op) = bits_buffer32;
				op += 3;
				bits_buffer32 = bits_buffer32 >> 24;
				bitstobeoutput -= 24;
			}
		}
		/* update hash */
		hash += (int)bits_buffer32;
		}

break_literal_1:

		m_pos += 3;
		ip += 3;

		if (__builtin_expect(!!(ip < in_end), 1) && *m_pos == *ip) {
			m_pos++, ip++;
			while (__builtin_expect(!!(ip < (in_end-1)), 1)
				&& *(unsigned short *)m_pos == *(unsigned short *)ip)
				m_pos += 2, ip += 2;
			if (__builtin_expect(!!(ip < in_end), 1) && *m_pos == *ip)
				m_pos += 1, ip += 1;
		}

		RESERVE_16_BITS();

		{
			size_t m_off = ip - m_pos;

			if ((m_off & 3) == 0 && m_off <= 128) {
				int value = lz4k_matchoff_encode[(m_off / 4) - 1];
				int bits = value >> 8;
				int code = value & 0xff;

				STORE_BITS(bits, code);
			} else {
				int code = (m_off << 1) | 0x1;

				STORE_BITS(13, code);
			}
		}
		RESERVE_16_BITS();

		{
			size_t m_len = ip - ii;

			if (m_len < 32) {
				int value = lz4k_matchlen_encode[m_len];
				int bits = value >> 9;
				int code = value & 0x1ff;

				STORE_BITS(bits, code);
			} else {
				int code = (m_len << 4) | 0xf;

				STORE_BITS(16, code);
			}
		}

		ii = ip;
		if (__builtin_expect(!!(ip >= ip_end), 0))
			break;
	}

	if ((in_end - ii) > 0) {
		size_t t = in_end - ii;

		if (t == 1) {
			int value, bits, code;

			RESERVE_16_BITS();
			value = lz4k_literalch_encode[*ii++];
			bits = value >> 9;
			code = (value & 0x1ff) << 2;
			bits_buffer32 |= code << bitstobeoutput;
			bitstobeoutput += bits + 2;
		} else {
			while (bitstobeoutput >= 8) {
				*op++ = (unsigned char) (bits_buffer32 & 0xff);
				bits_buffer32 = bits_buffer32 >> 8;
				bitstobeoutput -= 8;
			}
			bitstobeoutput += 1;

			if (t <= 17) {
				int value = lz4k_literallen_encode[t];
				int bits = value >> 7;
				int code = value & 0x7f;

				bits_buffer32 |= code << bitstobeoutput;
				bitstobeoutput += bits;
			} else {
				int code = ((t - 1) << 5) | 0x1f;

				bits_buffer32 |= code << bitstobeoutput;
				bitstobeoutput += 17;
			}

			while (1) {
				while (bitstobeoutput < 24) {
					int value, bits, code;

					value = lz4k_literalch_encode[*ii++];
					bits = value >> 9;
					code = value & 0x1ff;
					bits_buffer32 |= code << bitstobeoutput;
					bitstobeoutput += bits;
					if (__builtin_expect(!!(--t == 0), 0))
						goto break_literal_2;
				}
				*((unsigned int *)op) = bits_buffer32;
				op += 3;
				bits_buffer32 = bits_buffer32 >> 24;
				bitstobeoutput -= 24;
			}
		}
	}

	/* update hash */
	hash += (hash << 7) + (int)bits_buffer32;

break_literal_2:
	while (bitstobeoutput >= 8) {
		*op++ = (unsigned char) (bits_buffer32 & 0xff);
		bits_buffer32 = bits_buffer32 >> 8;
		bitstobeoutput -= 8;
	}
	if (bitstobeoutput != 0)
		*op++ = (unsigned char) (bits_buffer32 & 0xff);
	/* 4 bytes padding */
	*((unsigned int *)op) = LZ4K_TAG;
	op += 4;

	/* update hash */
	hash += op - out;
	hash += hash << 13;
	*checksum = hash;

	*out_len = op - out;

	return 0;
}

static size_t
_lz4k_do_compress(const unsigned char *in, size_t in_len,
		  unsigned char *out, size_t *out_len, void *wrkmem)
{
	const unsigned char *ip = in;
	unsigned char *op = out;
	const unsigned char *const in_end = in + in_len;
	const unsigned char *const ip_end = in + in_len - 3;
	const unsigned char *ii = ip;
	const unsigned char **const dict = wrkmem;
	unsigned int bitstobeoutput = 0;
	unsigned int bits_buffer32 = 0;

	bitstobeoutput = 1;

	for (;;) {
		const unsigned char *m_pos;
		{
			size_t dindex;
			unsigned int ip_content = *(unsigned int *)ip;
			unsigned int hash_temp = ip_content ^ (ip_content >> 12);

			dindex = hash_temp & 0xfff;
			m_pos = dict[dindex];
			dict[dindex] = ip;

			if (m_pos < in || m_pos >= ip
			    || ((*(unsigned int *)m_pos << 8) != (ip_content << 8))) {
				++ip;
				dindex = (hash_temp >> 8) & 0xfff;
				m_pos = dict[dindex];
				dict[dindex] = ip;
				if (m_pos < in || m_pos >= ip
				    || ((*(unsigned int *)m_pos << 8) !=
					(ip_content & 0xffffff00))) {
					++ip;
					if (__builtin_expect(!!(ip >= ip_end), 0))
						break;
					continue;
				}
			}
		}

		{
		size_t lit = ip - ii;

		if (lit <= 0) {
			if (bitstobeoutput == 32) {
				*((unsigned int *)op) = bits_buffer32;
				op += 4;
				bits_buffer32 = 1;
				bitstobeoutput = 1;
			} else {
				bits_buffer32 |= 1 << bitstobeoutput;
				bitstobeoutput += 1;
			}
		} else if (lit == 1) {
			int value, bits, code;

			RESERVE_16_BITS();
			value = lz4k_literalch_encode[*ii++];
			bits = value >> 9;
			code = (value & 0x1ff) << 2;
			STORE_BITS(bits + 2, code);
		} else if (lit == 2) {
			int value, bits, code;
			int value2, bits2, code2;

			RESERVE_16_BITS();
			if (bitstobeoutput > (32 - 22)) {
				*op++ = (unsigned char)(bits_buffer32 & 0xff);
				bits_buffer32 = bits_buffer32 >> 8;
				bitstobeoutput -= 8;
			}
			value = lz4k_literalch_encode[*ii++];
			bits = value >> 9;
			code = value & 0x1ff;
			value2 = lz4k_literalch_encode[*ii++];
			bits2 = value2 >> 9;
			code2 = value2 & 0x1ff;
			bits_buffer32 |=
			    ((((code2 << bits) | code) << 4) | 2) << bitstobeoutput;
			bitstobeoutput += bits2 + bits + 4;
		} else {
			if (lit <= 17) {
				int value, bits, code;

				RESERVE_16_BITS();
				value = lz4k_literallen_encode[lit];
				bits = value >> 7;
				code = (value & 0x7f) << 1;
				STORE_BITS(bits + 1, code);
			} else {
				int code = ((lit - 1) << 6) | 0x3e;

				RESERVE_16_BITS();
				if (bitstobeoutput > (32 - 18)) {
					*op++ =
					    (unsigned char)(bits_buffer32 & 0xff);
					bits_buffer32 = bits_buffer32 >> 8;
					bitstobeoutput -= 8;
				}
				STORE_BITS(17 + 1, code);
			}

			while (1) {
				while (bitstobeoutput < 24) {
					int value, bits, code;

					value = lz4k_literalch_encode[*ii++];
					bits = value >> 9;
					code = value & 0x1ff;
					STORE_BITS(bits, code);
					if (__builtin_expect(!!(ii == ip), 0))
						goto break_literal_1;
				}
				*((unsigned int *)op) = bits_buffer32;
				op += 3;
				bits_buffer32 = bits_buffer32 >> 24;
				bitstobeoutput -= 24;
			}
		}
		}

break_literal_1:

		m_pos += 3;
		ip += 3;

		if (__builtin_expect(!!(ip < in_end), 1) && *m_pos == *ip) {
			m_pos++, ip++;
			while (__builtin_expect(!!(ip < (in_end - 1)), 1)
			       && *(unsigned short *)m_pos == *(unsigned short *)ip)
				m_pos += 2, ip += 2;
			if (__builtin_expect(!!(ip < in_end), 1) && *m_pos == *ip)
				m_pos += 1, ip += 1;
		}

		RESERVE_16_BITS();

		{
			size_t m_off = ip - m_pos;

			if ((m_off & 3) == 0 && m_off <= 128) {
				int value = lz4k_matchoff_encode[(m_off / 4) - 1];
				int bits = value >> 8;
				int code = value & 0xff;

				STORE_BITS(bits, code);
			} else {
				int code = (m_off << 1) | 0x1;

				STORE_BITS(13, code);
			}
		}
		RESERVE_16_BITS();

		{
			size_t m_len = ip - ii;

			if (m_len < 32) {
				int value = lz4k_matchlen_encode[m_len];
				int bits = value >> 9;
				int code = value & 0x1ff;

				STORE_BITS(bits, code);
			} else {
				int code = (m_len << 4) | 0xf;

				STORE_BITS(16, code);
			}
		}

		ii = ip;
		if (__builtin_expect(!!(ip >= ip_end), 0))
			break;
	}

	if ((in_end - ii) > 0) {
		size_t t = in_end - ii;

		if (t == 1) {
			int value, bits, code;

			RESERVE_16_BITS();
			value = lz4k_literalch_encode[*ii++];
			bits = value >> 9;
			code = (value & 0x1ff) << 2;
			bits_buffer32 |= code << bitstobeoutput;
			bitstobeoutput += bits + 2;
		} else {
			while (bitstobeoutput >= 8) {
				*op++ = (unsigned char)(bits_buffer32 & 0xff);
				bits_buffer32 = bits_buffer32 >> 8;
				bitstobeoutput -= 8;
			}
			bitstobeoutput += 1;

			if (t <= 17) {
				int value = lz4k_literallen_encode[t];
				int bits = value >> 7;
				int code = value & 0x7f;

				bits_buffer32 |= code << bitstobeoutput;
				bitstobeoutput += bits;
			} else {
				int code = ((t - 1) << 5) | 0x1f;

				bits_buffer32 |= code << bitstobeoutput;
				bitstobeoutput += 17;
			}

			while (1) {
				while (bitstobeoutput < 24) {
					int value, bits, code;

					value = lz4k_literalch_encode[*ii++];
					bits = value >> 9;
					code = value & 0x1ff;
					bits_buffer32 |= code << bitstobeoutput;
					bitstobeoutput += bits;
					if (__builtin_expect(!!(--t == 0), 0))
						goto break_literal_2;
				}
				*((unsigned int *)op) = bits_buffer32;
				op += 3;
				bits_buffer32 = bits_buffer32 >> 24;
				bitstobeoutput -= 24;
			}
		}
	}

break_literal_2:
	while (bitstobeoutput >= 8) {
		*op++ = (unsigned char)(bits_buffer32 & 0xff);
		bits_buffer32 = bits_buffer32 >> 8;
		bitstobeoutput -= 8;
	}
	if (bitstobeoutput != 0)
		*op++ = (unsigned char)(bits_buffer32 & 0xff);

	*((unsigned int *)op) = LZ4K_TAG;
	op += 4;

	*out_len = op - out;

	return 0;
}
int lz4k_compress_zram(const unsigned char *in, size_t in_len, unsigned char *out,
			size_t *out_len, void *wrkmem, int *checksum)
{
	unsigned char *op = out;

	if (in_len > 4096)
		return -1;

	if (__builtin_expect(!!(in_len == 0), 0)) {
		*out_len = 0;
		return -1;
	}
	memset(wrkmem, 0, LZ4K_MEM_COMPRESS);
	_lz4k_do_compress_zram(in, in_len, op, out_len, wrkmem, checksum);

	if (*out_len <= 0)
		return -1;
	return 0;
}
int lz4k_compress(const unsigned char *in, size_t in_len, unsigned char *out,
		  size_t *out_len, void *wrkmem)
{
	unsigned char *op = out;

	if (in_len > 4096)
		return -1;

	if (__builtin_expect(!!(in_len == 0), 0)) {
		*out_len = 0;
		return -1;
	}

	memset(wrkmem, 0, LZ4K_MEM_COMPRESS);
	_lz4k_do_compress(in, in_len, op, out_len, wrkmem);

	if (*out_len <= 0)
		return -1;

	return 0;
}
