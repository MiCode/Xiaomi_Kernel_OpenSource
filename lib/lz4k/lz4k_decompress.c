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
 *  LZ4K Decompressor by Vovo
 */
#include <linux/lz4k.h>
#include <linux/types.h>

static unsigned short lz4k_matchlen_decode[128] = {
	780, 1298, 1035, 0, 1033, 1553, 1040, 0, 780, 1304, 1039, 0, 1034, 1567, 1293, 0, 780, 1300,
	1035, 0, 1033, 1558, 1040, 0, 780, 1308, 1039, 0, 1034, 1818, 1294, 0, 780, 1298, 1035, 0, 1033,
	1555, 1040, 0, 780, 1304, 1039, 0, 1034, 1813, 1293, 0, 780, 1300, 1035, 0, 1033, 1559, 1040, 0,
	780, 1308, 1039, 0, 1034, 1821, 1294, 0, 780, 1298, 1035, 0, 1033, 1553, 1040, 0, 780, 1304, 1039,
	0, 1034, 1567, 1293, 0, 780, 1300, 1035, 0, 1033, 1558, 1040, 0, 780, 1308, 1039, 0, 1034, 1819,
	1294, 0, 780, 1298, 1035, 0, 1033, 1555, 1040, 0, 780, 1304, 1039, 0, 1034, 1817, 1293, 0, 780, 1300,
	1035, 0, 1033, 1559, 1040, 0, 780, 1308, 1039, 0, 1034, 1822, 1294, 0, };

static unsigned short lz4k_matchlen_decode_hc[256] = {
	514, 772, 515, 1031, 514, 1029, 515, 1545, 514, 772, 515, 1288, 514, 1030, 515, 2061, 514,
	772, 515, 1031, 514, 1029, 515, 1551, 514, 772, 515, 1291, 514, 1030, 515, 0, 514, 772, 515,
	1031, 514, 1029, 515, 1548, 514, 772, 515, 1288, 514, 1030, 515, 2067, 514, 772, 515, 1031, 514,
	1029, 515, 1802, 514, 772, 515, 1291, 514, 1030, 515, 0, 514, 772, 515, 1031, 514, 1029, 515, 1545,
	514, 772, 515, 1288, 514, 1030, 515, 2065, 514, 772, 515, 1031, 514, 1029, 515, 1551, 514, 772, 515,
	1291, 514, 1030, 515, 0, 514, 772, 515, 1031, 514, 1029, 515, 1548, 514, 772, 515, 1288, 514, 1030,
	515, 2072, 514, 772, 515, 1031, 514, 1029, 515, 1808, 514, 772, 515, 1291, 514, 1030, 515, 0,
	514, 772, 515, 1031, 514, 1029, 515, 1545, 514, 772, 515, 1288, 514, 1030, 515, 2062, 514,
	772, 515, 1031, 514, 1029, 515, 1551, 514, 772, 515, 1291, 514, 1030, 515, 0, 514, 772,
	515, 1031, 514, 1029, 515, 1548, 514, 772, 515, 1288, 514, 1030, 515, 2068, 514, 772,
	515, 1031, 514, 1029, 515, 1802, 514, 772, 515, 1291, 514, 1030, 515, 0,
	514, 772, 515, 1031, 514, 1029, 515, 1545, 514, 772, 515, 1288, 514, 1030, 515, 2066, 514,
	772, 515, 1031, 514, 1029, 515, 1551, 514, 772, 515, 1291, 514, 1030, 515, 0, 514, 772,
	515, 1031, 514, 1029, 515, 1548, 514, 772, 515, 1288, 514, 1030, 515, 2076, 514, 772,
	515, 1031, 514, 1029, 515, 1808, 514, 772, 515, 1291, 514, 1030, 515, 0, };

static unsigned short lz4k_matchoff_decode[128] = {
	1028, 1336, 1040, 1600, 1032, 1568, 1292, 1888, 1028, 1556, 1040, 1852, 1032, 1576, 1304,
	2132, 1028, 1336, 1040, 1836, 1032, 1572, 1292, 1912, 1028, 1564, 1040, 1872, 1032, 1584, 1304, 2156, 1028,
	1336, 1040, 1600, 1032, 1568, 1292, 1904, 1028, 1556, 1040, 1864, 1032, 1576, 1304, 2148, 1028, 1336, 1040,
	1844, 1032, 1572, 1292, 2116, 1028, 1564, 1040,	1880, 1032, 1584, 1304, 2172, 1028, 1336, 1040, 1600, 1032,
	1568, 1292, 1888, 1028, 1556, 1040, 1852, 1032, 1576, 1304, 2140, 1028, 1336, 1040, 1836, 1032, 1572, 1292,
	1912, 1028, 1564, 1040, 1872, 1032, 1584, 1304, 2164, 1028, 1336, 1040, 1600, 1032, 1568, 1292, 1904, 1028,
	1556, 1040, 1864, 1032, 1576, 1304, 2152, 1028, 1336, 1040, 1844, 1032, 1572, 1292, 2124, 1028, 1564, 1040,
	1880, 1032, 1584, 1304, 2176, };

static unsigned short lz4k_matchoff_decode_hc[128] = {
	768, 1560, 1284, 1814, 768, 1836, 1296, 1876, 768, 1820, 1288, 1860, 768, 1856, 1548, 2160,
	768, 1793, 1284, 1822, 768, 1848, 1296, 2144, 768, 1828, 1288, 1868, 768, 1806, 1556, 2176, 768, 1560, 1284,
	1818, 768, 1840, 1296, 2136, 768, 1824, 1288, 1864, 768, 1802, 1548, 2168, 768, 1798, 1284, 1844, 768, 1852,
	1296, 2152, 768, 1832, 1288, 1872, 768, 1810, 1556, 2082, 768, 1560, 1284, 1814, 768, 1836, 1296, 1876, 768,
	1820, 1288, 1860, 768, 1856, 1548, 2164, 768, 1793, 1284, 1822, 768, 1848, 1296, 2148, 768, 1828, 1288, 1868,
	768, 1806, 1556, 2050, 768, 1560, 1284, 1818, 768, 1840, 1296, 2140, 768, 1824, 1288, 1864, 768, 1802, 1548,
	2172, 768, 1798, 1284, 1844, 768, 1852, 1296, 2156, 768, 1832, 1288, 1872, 768, 1810, 1556, 2180, };

static unsigned short lz4k_literallen_decode[128] = {
	257, 770, 257, 1029, 257, 1027, 257, 1543, 257, 770, 257, 1286, 257, 1028, 257, 1806, 257, 770, 257, 1029,
	257, 1027, 257, 1802, 257, 770, 257, 1289, 257, 1028, 257, 0, 257, 770, 257, 1029, 257, 1027, 257, 1544, 257,
	770, 257, 1286, 257, 1028, 257, 1808, 257, 770, 257, 1029, 257, 1027, 257, 1804, 257, 770, 257, 1289, 257,
	1028, 257, 0, 257, 770, 257, 1029, 257, 1027, 257, 1543, 257, 770, 257, 1286, 257, 1028, 257, 1807, 257,
	770, 257, 1029, 257, 1027, 257, 1803, 257, 770, 257, 1289, 257, 1028, 257, 0, 257, 770, 257, 1029, 257, 1027,
	257, 1544, 257, 770, 257, 1286, 257, 1028, 257, 1809, 257, 770, 257, 1029, 257, 1027, 257, 1805, 257, 770,
	257, 1289, 257, 1028, 257, 0, };

static unsigned short lz4k_literalch_decode[512] = { 1024, 2120, 1840, 2327, 1344, 2224, 2060, 2461, 1279, 2151, 1903,
	2411, 1794, 2280, 2082, 2500, 1024, 2132, 1889, 2367, 1537, 2258, 2072, 2479, 1279, 2168, 1920, 2443, 1801,
	2292, 2097, 2524, 1024, 2126, 1872, 2358, 1344, 2240, 2065, 2470, 1279, 2161, 1906, 2433, 1796, 2288, 2089,
	2509, 1024, 2146, 1897, 2391, 1568, 2264, 2077, 2490, 1279, 2192, 2053, 2452, 1808, 2297, 2104, 2537, 1024,
	2124, 1857, 2346, 1344, 2232, 2062, 2466, 1279, 2156, 1904, 2428, 1795, 2285, 2084, 2505, 1024, 2143, 1893,
	2377, 1537, 2260, 2075, 2485, 1279, 2180, 2016, 2447, 1802, 2295, 2099, 2530, 1024, 2130, 1873, 2363, 1344,
	2256, 2067, 2475, 1279, 2165, 1908, 2438, 1800, 2290, 2094, 2519, 1024, 2148, 1902, 2396, 1568, 2275, 2079,
	2495, 1279, 2208, 2055, 2457, 1832, 2300, 2115, 2550, 1024, 2123, 1840, 2342, 1344, 2228, 2061, 2463, 1279,
	2152, 1903, 2426, 1794, 2284, 2083, 2502, 1024, 2136, 1889, 2374, 1537, 2259, 2074, 2482, 1279, 2169, 1920,
	2445, 1801, 2293, 2098, 2526, 1024, 2127, 1872, 2361, 1344, 2248, 2066, 2473, 1279, 2163, 1906, 2435, 1796,
	2289, 2092, 2511, 1024, 2147, 1897, 2394, 1568, 2273, 2078, 2493, 1279, 2200, 2054, 2454, 1808, 2298, 2114,
	2539, 1024, 2125, 1857, 2349, 1344, 2236, 2063, 2468, 1279, 2157, 1904, 2430, 1795, 2286, 2085,
	    2507, 1024, 2144, 1893, 2389, 1537, 2261, 2076, 2487, 1279, 2184, 2016, 2450, 1802,
	    2296, 2100, 2534, 1024, 2131, 1873, 2365, 1344, 2257, 2068, 2477, 1279, 2166, 1908,
	    2441, 1800, 2291, 2095, 2522, 1024, 2150, 1902, 2398, 1568, 2277, 2081, 2498, 1279,
	    2216, 2059, 2459, 1832, 2325, 2116, 2557,
	1024, 2120, 1840, 2329, 1344, 2224, 2060, 2462, 1279, 2151, 1903, 2423, 1794, 2280, 2082,
	    2501, 1024, 2132, 1889, 2373, 1537, 2258, 2072, 2481, 1279, 2168, 1920, 2444, 1801,
	    2292, 2097, 2525, 1024, 2126, 1872, 2359, 1344, 2240, 2065, 2471, 1279, 2161, 1906,
	    2434, 1796, 2288, 2089, 2510, 1024, 2146, 1897, 2393, 1568, 2264, 2077, 2491, 1279,
	    2192, 2053, 2453, 1808, 2297, 2104, 2538,
	1024, 2124, 1857, 2347, 1344, 2232, 2062, 2467, 1279, 2156, 1904, 2429, 1795, 2285, 2084,
	    2506, 1024, 2143, 1893, 2378, 1537, 2260, 2075, 2486, 1279, 2180, 2016, 2449, 1802,
	    2295, 2099, 2532, 1024, 2130, 1873, 2364, 1344, 2256, 2067, 2476, 1279, 2165, 1908,
	    2439, 1800, 2290, 2094, 2521, 1024, 2148, 1902, 2397, 1568, 2275, 2079, 2497, 1279,
	    2208, 2055, 2458, 1832, 2300, 2115, 2555,
	1024, 2123, 1840, 2343, 1344, 2228, 2061, 2465, 1279, 2152, 1903, 2427, 1794, 2284, 2083,
	    2503, 1024, 2136, 1889, 2375, 1537, 2259, 2074, 2483, 1279, 2169, 1920, 2446, 1801,
	    2293, 2098, 2527, 1024, 2127, 1872, 2362, 1344, 2248, 2066, 2474, 1279, 2163, 1906,
	    2437, 1796, 2289, 2092, 2518, 1024, 2147, 1897, 2395, 1568, 2273, 2078, 2494, 1279,
	    2200, 2054, 2455, 1808, 2298, 2114, 2543,
	1024, 2125, 1857, 2357, 1344, 2236, 2063, 2469, 1279, 2157, 1904, 2431, 1795, 2286, 2085,
	    2508, 1024, 2144, 1893, 2390, 1537, 2261, 2076, 2489, 1279, 2184, 2016, 2451, 1802,
	    2296, 2100, 2535, 1024, 2131, 1873, 2366, 1344, 2257, 2068, 2478, 1279, 2166, 1908,
	    2442, 1800, 2291, 2095, 2523, 1024, 2150, 1902, 2410, 1568, 2277, 2081, 2499, 1279,
	    2216, 2059, 2460, 1832, 2326, 2116, 2558,
};

#define RESERVE_16_BITS() \
{if (remaining_bits <= 16) { \
	bits_buffer32 = bits_buffer32 | (*((unsigned short *)ip)) << remaining_bits; \
	ip += 2; \
	remaining_bits += 16; \
} }

static int lz4k_decompress_simple(const unsigned char *in, size_t in_len, unsigned char *out,
				  unsigned char *const op_end)
{
	unsigned char *op = out;
	const unsigned char *ip = in;
	unsigned int bits_buffer32 = 0;
	unsigned int remaining_bits = 32;

	bits_buffer32 = *((unsigned int *)ip);
	ip += 4;
	bits_buffer32 = bits_buffer32 >> 1;
	remaining_bits -= 1;

	/* check lz4k tag */
	if (*((unsigned int *)(in + in_len - 4)) != LZ4K_TAG)
		return -1;

	while (1) {
		RESERVE_16_BITS();
		if ((bits_buffer32 & 1) == 0) {
			if ((bits_buffer32 & 2) == 0) {
				unsigned short value;
				int bits;

				value = lz4k_literalch_decode[(bits_buffer32 >> 2) & 0x1ff];
				bits = value >> 8;
				*op++ = value & 0xff;
				bits_buffer32 = bits_buffer32 >> (bits + 2);
				remaining_bits -= bits + 2;
			} else {
				int litlen;
				unsigned short value;

				value = lz4k_literallen_decode[(bits_buffer32 >> 1) & 0x7f];
				if (value != 0) {
					int bits = value >> 8;

					litlen = value & 0xff;
					bits_buffer32 = bits_buffer32 >> (bits + 1);
					remaining_bits -= bits + 1;
				} else {
					if (remaining_bits < 18) {
						bits_buffer32 =
						    bits_buffer32 | ((*ip) << remaining_bits);
						ip += 1;
						remaining_bits += 8;
					}
					litlen = ((bits_buffer32 >> 6) & 0xfff) + 1;
					bits_buffer32 = bits_buffer32 >> 18;
					remaining_bits -= 18;
				}

				while (1) {
					while (remaining_bits > 8) {
						unsigned short value;
						int bits;

						value =
						    lz4k_literalch_decode[bits_buffer32 & 0x1ff];
						bits = value >> 8;
						*op++ = value & 0xff;
						bits_buffer32 = bits_buffer32 >> bits;
						remaining_bits -= bits;
					if (--litlen == 0)
						goto break_literal;
					}
					bits_buffer32 =
					    bits_buffer32 | (*((unsigned int *)ip)) <<
					    remaining_bits;
					ip += 3;
					remaining_bits += 24;
				}
			}

 break_literal:
			if (__builtin_expect(!!(op == op_end), 0))
				break;
		} else {
			bits_buffer32 = bits_buffer32 >> 1;
			remaining_bits -= 1;
		}

		{
			int offset;
			int len;

			RESERVE_16_BITS();
			if ((bits_buffer32 & 1) == 0) {
				unsigned short value =
				    lz4k_matchoff_decode[(bits_buffer32 & 0xff) >> 1];
				int bits = value >> 8;

				offset = value & 0xff;
				bits_buffer32 = bits_buffer32 >> bits;
				remaining_bits -= bits;
			} else {
				offset = (bits_buffer32 >> 1) & 0xfff;
				bits_buffer32 = bits_buffer32 >> 13;
				remaining_bits -= 13;
			}
			RESERVE_16_BITS();

			if ((bits_buffer32 & 1) == 0) {
				len = 3 + ((bits_buffer32 >> 1) & 1);
				bits_buffer32 = bits_buffer32 >> 2;
				remaining_bits -= 2;
			} else if ((bits_buffer32 & 2) == 0) {
				len = 5 + ((bits_buffer32 >> 2) & 3);
				bits_buffer32 = bits_buffer32 >> 4;
				remaining_bits -= 4;
			} else {
				unsigned short value =
				    lz4k_matchlen_decode[(bits_buffer32 >> 2) & 0x7f];
				if (value != 0) {
					int bits = value >> 8;

					len = value & 0xff;
					bits_buffer32 = bits_buffer32 >> (bits + 2);
					remaining_bits -= (bits + 2);
				} else {
					len = (bits_buffer32 >> 4) & 0xfff;
					bits_buffer32 = bits_buffer32 >> 16;
					remaining_bits -= 16;
				}
			}

			if (__builtin_expect(!!((offset >> 2) != 0), 1)) {
				const unsigned char *m_pos = op - offset;

				if ((len & 1) != 0) {
					*op++ = *m_pos++;
					--len;
				}
				if ((len & 2) != 0) {
					*(unsigned short *)op = *(unsigned short *)m_pos;
					op += 2;
					m_pos += 2;
					len -= 2;
				}
				while (len > 0) {
					*(unsigned int *)op = *(unsigned int *)m_pos;
					op += 4;
					m_pos += 4;
					len -= 4;
				}
			} else if (__builtin_expect(!!(offset == 1), 1)) {
				unsigned int value = *(op - 1);

				value = value | (value << 8);
				value = value | (value << 16);
				if ((len & 1) != 0) {
					*op++ = (unsigned char)value;
					--len;
				}
				if ((len & 2) != 0) {
					*(unsigned short *)op = (unsigned short)value;
					op += 2;
					len -= 2;
				}
				while (len > 0) {
					*(unsigned int *)op = (unsigned int)value;
					op += 4;
					len -= 4;
				}
			} else {
				const unsigned char *m_pos = op - offset;

				if ((len & 1) != 0) {
					*op++ = *m_pos++;
					--len;
				}
				do {
					*((unsigned short *)op) = *((unsigned short *)m_pos);
					op += 2;
					m_pos += 2;
					len -= 2;
				} while (len != 0);
			}
		}
		if (__builtin_expect(!!(op == op_end), 0))
			break;
	}
	if (op != op_end)
		return -1;

	return 0;
}

static int lz4k_decompress_hc(const unsigned char *in, size_t in_len, unsigned char *out,
			      unsigned char *const op_end)
{
	unsigned char *op = out;
	const unsigned char *ip = in;
	unsigned int bits_buffer32 = 0;
	unsigned int remaining_bits = 32;
	int previous_off = 0;

	bits_buffer32 = *((unsigned int *)ip);
	ip += 4;
	bits_buffer32 = bits_buffer32 >> 1;
	remaining_bits -= 1;

	while (1) {
		RESERVE_16_BITS();
		if ((bits_buffer32 & 1) == 0) {
			if ((bits_buffer32 & 2) == 0) {
				*op++ = (bits_buffer32 >> 2) & 0xff;
				bits_buffer32 = bits_buffer32 >> (8 + 2);
				remaining_bits -= 8 + 2;
			} else {
				int litlen;
				unsigned short value;

				value = lz4k_literallen_decode[(bits_buffer32 >> 1) & 0x7f];
				if (value != 0) {
					int bits = value >> 8;

					litlen = value & 0xff;
					bits_buffer32 = bits_buffer32 >> (bits + 1);
					remaining_bits -= bits + 1;
				} else {
					if (remaining_bits < 18) {
						bits_buffer32 =
						    bits_buffer32 | ((*ip) << remaining_bits);
						ip += 1;
						remaining_bits += 8;
					}
					litlen = ((bits_buffer32 >> 6) & 0xfff) + 1;
					bits_buffer32 = bits_buffer32 >> 18;
					remaining_bits -= 18;
				}

				RESERVE_16_BITS();
				if ((litlen & 1) != 0) {
					*op++ = bits_buffer32 & 0xff;
					bits_buffer32 = bits_buffer32 >> 8;
					remaining_bits -= 8;
					--litlen;
					RESERVE_16_BITS();
				}
				do {
					*(unsigned short *)op = bits_buffer32 & 0xffff;
					bits_buffer32 =
					    (bits_buffer32 >> 16) | (*(unsigned short *)ip) <<
					    (remaining_bits - 16);
					ip += 2;
					op += 2;
					litlen -= 2;
				} while (litlen > 0);
			}

 /*break_literal:*/
			if (__builtin_expect(!!(op == op_end), 0))
				break;
		} else {
			bits_buffer32 = bits_buffer32 >> 1;
			remaining_bits -= 1;
		}

		{
			int offset;
			int len;

			RESERVE_16_BITS();
			if ((bits_buffer32 & 1) == 0) {
				unsigned short value =
				    lz4k_matchoff_decode_hc[(bits_buffer32 & 0xff) >> 1];
				int bits = value >> 8;
				int code = value & 0xff;

				if (code == 0) {	/* previous */
					offset = previous_off;
				} else {
					offset = code;
				}
				bits_buffer32 = bits_buffer32 >> bits;
				remaining_bits -= bits;
			} else {
				int index = op - out;
				int bits = 32 - __builtin_clz(index);

				offset = (bits_buffer32 >> 1) & ((1 << bits) - 1);
				bits_buffer32 = bits_buffer32 >> (bits + 1);
				remaining_bits -= bits + 1;
			}
			previous_off = offset;
			RESERVE_16_BITS();

			{
				if ((bits_buffer32 & 0x1f) != 0x1f) {
					unsigned short value =
					    lz4k_matchlen_decode_hc[(bits_buffer32) & 0xff];
					int bits = value >> 8;

					len = value & 0xff;
					bits_buffer32 = bits_buffer32 >> bits;
					remaining_bits -= bits;
				} else {
					len = (bits_buffer32 >> 5) & 0xfff;
					bits_buffer32 = bits_buffer32 >> 17;
					remaining_bits -= 17;
				}
			}

			if (__builtin_expect(!!((offset >> 2) != 0), 1)) {
				const unsigned char *m_pos = op - offset;

				if ((len & 1) != 0) {
					*op++ = *m_pos++;
					--len;
				}
				if ((len & 2) != 0) {
					*(unsigned short *)op = *(unsigned short *)m_pos;
					op += 2;
					m_pos += 2;
					len -= 2;
				}
				while (len > 0) {
					*(unsigned int *)op = *(unsigned int *)m_pos;
					op += 4;
					m_pos += 4;
					len -= 4;
				}
			} else if (__builtin_expect(!!(offset == 1), 1)) {
				unsigned int value = *(op - 1);

				value = value | (value << 8);
				value = value | (value << 16);
				if ((len & 1) != 0) {
					*op++ = (unsigned char)value;
					--len;
				}
				if ((len & 2) != 0) {
					*(unsigned short *)op = (unsigned short)value;
					op += 2;
					len -= 2;
				}
				while (len > 0) {
					*(unsigned int *)op = (unsigned int)value;
					op += 4;
					len -= 4;
				}
			} else {
				const unsigned char *m_pos = op - offset;

				if ((len & 1) != 0) {
					*op++ = *m_pos++;
					--len;
				}
				do {
					*((unsigned short *)op) = *((unsigned short *)m_pos);
					op += 2;
					m_pos += 2;
					len -= 2;
				} while (len != 0);
			}
		}
		if (__builtin_expect(!!(op == op_end), 0))
			break;
	}
	if (op != op_end)
		return -1;

	return 0;
}

int lz4k_decompress_safe(const unsigned char *in, size_t in_len, unsigned char *out,
			 size_t *pout_len)
{
	int result = 0;
	unsigned char *const op_end = out + *pout_len;

	if (*pout_len > 4096)
		return -1;

	if (in_len == 0)
		return -1;

	result = lz4k_decompress_simple(in, in_len, out, op_end);


	return result;
}


int lz4k_decompress_ubifs(const unsigned char *in, size_t in_len, unsigned char *out,
			  size_t *pout_len)
{
	int result = 0;
	unsigned char *const op_end = out + *pout_len;


	if (*pout_len > 4096)
		return -1;

	if (in_len == 0)
		return -1;

	if ((*in & 1) == 0)
		result = lz4k_decompress_simple(in, in_len, out, op_end);
	else
		result = lz4k_decompress_hc(in, in_len, out, op_end);


	return result;
}
