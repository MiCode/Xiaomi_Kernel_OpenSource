/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Function naming determines intended use:
 *
 *     <x>_r(void) : Returns the offset for register <x>.
 *
 *     <x>_o(void) : Returns the offset for element <x>.
 *
 *     <x>_w(void) : Returns the word offset for word (4 byte) element <x>.
 *
 *     <x>_<y>_s(void) : Returns size of field <y> of register <x> in bits.
 *
 *     <x>_<y>_f(u32 v) : Returns a value based on 'v' which has been shifted
 *         and masked to place it at field <y> of register <x>.  This value
 *         can be |'d with others to produce a full register value for
 *         register <x>.
 *
 *     <x>_<y>_m(void) : Returns a mask for field <y> of register <x>.  This
 *         value can be ~'d and then &'d to clear the value of field <y> for
 *         register <x>.
 *
 *     <x>_<y>_<z>_f(void) : Returns the constant value <z> after being shifted
 *         to place it at field <y> of register <x>.  This value can be |'d
 *         with others to produce a full register value for <x>.
 *
 *     <x>_<y>_v(u32 r) : Returns the value of field <y> from a full register
 *         <x> value 'r' after being shifted to place its LSB at bit 0.
 *         This value is suitable for direct comparison with other unshifted
 *         values appropriate for use in field <y> of register <x>.
 *
 *     <x>_<y>_<z>_v(void) : Returns the constant value for <z> defined for
 *         field <y> of register <x>.  This value is suitable for direct
 *         comparison with unshifted values appropriate for use in field <y>
 *         of register <x>.
 */
#ifndef _hw_gmmu_gk20a_h_
#define _hw_gmmu_gk20a_h_

static inline u32 gmmu_pde_aperture_big_w(void)
{
	return 0;
}
static inline u32 gmmu_pde_aperture_big_invalid_f(void)
{
	return 0x0;
}
static inline u32 gmmu_pde_aperture_big_video_memory_f(void)
{
	return 0x1;
}
static inline u32 gmmu_pde_size_w(void)
{
	return 0;
}
static inline u32 gmmu_pde_size_full_f(void)
{
	return 0x0;
}
static inline u32 gmmu_pde_address_big_sys_f(u32 v)
{
	return (v & 0xfffffff) << 4;
}
static inline u32 gmmu_pde_address_big_sys_w(void)
{
	return 0;
}
static inline u32 gmmu_pde_aperture_small_w(void)
{
	return 1;
}
static inline u32 gmmu_pde_aperture_small_invalid_f(void)
{
	return 0x0;
}
static inline u32 gmmu_pde_aperture_small_video_memory_f(void)
{
	return 0x1;
}
static inline u32 gmmu_pde_vol_small_w(void)
{
	return 1;
}
static inline u32 gmmu_pde_vol_small_true_f(void)
{
	return 0x4;
}
static inline u32 gmmu_pde_vol_small_false_f(void)
{
	return 0x0;
}
static inline u32 gmmu_pde_vol_big_w(void)
{
	return 1;
}
static inline u32 gmmu_pde_vol_big_true_f(void)
{
	return 0x8;
}
static inline u32 gmmu_pde_vol_big_false_f(void)
{
	return 0x0;
}
static inline u32 gmmu_pde_address_small_sys_f(u32 v)
{
	return (v & 0xfffffff) << 4;
}
static inline u32 gmmu_pde_address_small_sys_w(void)
{
	return 1;
}
static inline u32 gmmu_pde_address_shift_v(void)
{
	return 0x0000000c;
}
static inline u32 gmmu_pde__size_v(void)
{
	return 0x00000008;
}
static inline u32 gmmu_pte__size_v(void)
{
	return 0x00000008;
}
static inline u32 gmmu_pte_valid_w(void)
{
	return 0;
}
static inline u32 gmmu_pte_valid_true_f(void)
{
	return 0x1;
}
static inline u32 gmmu_pte_address_sys_f(u32 v)
{
	return (v & 0xfffffff) << 4;
}
static inline u32 gmmu_pte_address_sys_w(void)
{
	return 0;
}
static inline u32 gmmu_pte_vol_w(void)
{
	return 1;
}
static inline u32 gmmu_pte_vol_true_f(void)
{
	return 0x1;
}
static inline u32 gmmu_pte_vol_false_f(void)
{
	return 0x0;
}
static inline u32 gmmu_pte_aperture_w(void)
{
	return 1;
}
static inline u32 gmmu_pte_aperture_video_memory_f(void)
{
	return 0x0;
}
static inline u32 gmmu_pte_read_only_w(void)
{
	return 0;
}
static inline u32 gmmu_pte_read_only_true_f(void)
{
	return 0x4;
}
static inline u32 gmmu_pte_write_disable_w(void)
{
	return 1;
}
static inline u32 gmmu_pte_write_disable_true_f(void)
{
	return 0x80000000;
}
static inline u32 gmmu_pte_read_disable_w(void)
{
	return 1;
}
static inline u32 gmmu_pte_read_disable_true_f(void)
{
	return 0x40000000;
}
static inline u32 gmmu_pte_comptagline_f(u32 v)
{
	return (v & 0x1ffff) << 12;
}
static inline u32 gmmu_pte_comptagline_w(void)
{
	return 1;
}
static inline u32 gmmu_pte_address_shift_v(void)
{
	return 0x0000000c;
}
static inline u32 gmmu_pte_kind_f(u32 v)
{
	return (v & 0xff) << 4;
}
static inline u32 gmmu_pte_kind_w(void)
{
	return 1;
}
static inline u32 gmmu_pte_kind_invalid_v(void)
{
	return 0x000000ff;
}
static inline u32 gmmu_pte_kind_pitch_v(void)
{
	return 0x00000000;
}
static inline u32 gmmu_pte_kind_z16_v(void)
{
	return 0x00000001;
}
static inline u32 gmmu_pte_kind_z16_2c_v(void)
{
	return 0x00000002;
}
static inline u32 gmmu_pte_kind_z16_ms2_2c_v(void)
{
	return 0x00000003;
}
static inline u32 gmmu_pte_kind_z16_ms4_2c_v(void)
{
	return 0x00000004;
}
static inline u32 gmmu_pte_kind_z16_ms8_2c_v(void)
{
	return 0x00000005;
}
static inline u32 gmmu_pte_kind_z16_ms16_2c_v(void)
{
	return 0x00000006;
}
static inline u32 gmmu_pte_kind_z16_2z_v(void)
{
	return 0x00000007;
}
static inline u32 gmmu_pte_kind_z16_ms2_2z_v(void)
{
	return 0x00000008;
}
static inline u32 gmmu_pte_kind_z16_ms4_2z_v(void)
{
	return 0x00000009;
}
static inline u32 gmmu_pte_kind_z16_ms8_2z_v(void)
{
	return 0x0000000a;
}
static inline u32 gmmu_pte_kind_z16_ms16_2z_v(void)
{
	return 0x0000000b;
}
static inline u32 gmmu_pte_kind_z16_4cz_v(void)
{
	return 0x0000000c;
}
static inline u32 gmmu_pte_kind_z16_ms2_4cz_v(void)
{
	return 0x0000000d;
}
static inline u32 gmmu_pte_kind_z16_ms4_4cz_v(void)
{
	return 0x0000000e;
}
static inline u32 gmmu_pte_kind_z16_ms8_4cz_v(void)
{
	return 0x0000000f;
}
static inline u32 gmmu_pte_kind_z16_ms16_4cz_v(void)
{
	return 0x00000010;
}
static inline u32 gmmu_pte_kind_s8z24_v(void)
{
	return 0x00000011;
}
static inline u32 gmmu_pte_kind_s8z24_1z_v(void)
{
	return 0x00000012;
}
static inline u32 gmmu_pte_kind_s8z24_ms2_1z_v(void)
{
	return 0x00000013;
}
static inline u32 gmmu_pte_kind_s8z24_ms4_1z_v(void)
{
	return 0x00000014;
}
static inline u32 gmmu_pte_kind_s8z24_ms8_1z_v(void)
{
	return 0x00000015;
}
static inline u32 gmmu_pte_kind_s8z24_ms16_1z_v(void)
{
	return 0x00000016;
}
static inline u32 gmmu_pte_kind_s8z24_2cz_v(void)
{
	return 0x00000017;
}
static inline u32 gmmu_pte_kind_s8z24_ms2_2cz_v(void)
{
	return 0x00000018;
}
static inline u32 gmmu_pte_kind_s8z24_ms4_2cz_v(void)
{
	return 0x00000019;
}
static inline u32 gmmu_pte_kind_s8z24_ms8_2cz_v(void)
{
	return 0x0000001a;
}
static inline u32 gmmu_pte_kind_s8z24_ms16_2cz_v(void)
{
	return 0x0000001b;
}
static inline u32 gmmu_pte_kind_s8z24_2cs_v(void)
{
	return 0x0000001c;
}
static inline u32 gmmu_pte_kind_s8z24_ms2_2cs_v(void)
{
	return 0x0000001d;
}
static inline u32 gmmu_pte_kind_s8z24_ms4_2cs_v(void)
{
	return 0x0000001e;
}
static inline u32 gmmu_pte_kind_s8z24_ms8_2cs_v(void)
{
	return 0x0000001f;
}
static inline u32 gmmu_pte_kind_s8z24_ms16_2cs_v(void)
{
	return 0x00000020;
}
static inline u32 gmmu_pte_kind_s8z24_4cszv_v(void)
{
	return 0x00000021;
}
static inline u32 gmmu_pte_kind_s8z24_ms2_4cszv_v(void)
{
	return 0x00000022;
}
static inline u32 gmmu_pte_kind_s8z24_ms4_4cszv_v(void)
{
	return 0x00000023;
}
static inline u32 gmmu_pte_kind_s8z24_ms8_4cszv_v(void)
{
	return 0x00000024;
}
static inline u32 gmmu_pte_kind_s8z24_ms16_4cszv_v(void)
{
	return 0x00000025;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc12_v(void)
{
	return 0x00000026;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc4_v(void)
{
	return 0x00000027;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc8_v(void)
{
	return 0x00000028;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc24_v(void)
{
	return 0x00000029;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc12_1zv_v(void)
{
	return 0x0000002e;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc4_1zv_v(void)
{
	return 0x0000002f;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc8_1zv_v(void)
{
	return 0x00000030;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc24_1zv_v(void)
{
	return 0x00000031;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc12_2cs_v(void)
{
	return 0x00000032;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc4_2cs_v(void)
{
	return 0x00000033;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc8_2cs_v(void)
{
	return 0x00000034;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc24_2cs_v(void)
{
	return 0x00000035;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc12_2czv_v(void)
{
	return 0x0000003a;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc4_2czv_v(void)
{
	return 0x0000003b;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc8_2czv_v(void)
{
	return 0x0000003c;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc24_2czv_v(void)
{
	return 0x0000003d;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc12_2zv_v(void)
{
	return 0x0000003e;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc4_2zv_v(void)
{
	return 0x0000003f;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc8_2zv_v(void)
{
	return 0x00000040;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc24_2zv_v(void)
{
	return 0x00000041;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc12_4cszv_v(void)
{
	return 0x00000042;
}
static inline u32 gmmu_pte_kind_v8z24_ms4_vc4_4cszv_v(void)
{
	return 0x00000043;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc8_4cszv_v(void)
{
	return 0x00000044;
}
static inline u32 gmmu_pte_kind_v8z24_ms8_vc24_4cszv_v(void)
{
	return 0x00000045;
}
static inline u32 gmmu_pte_kind_z24s8_v(void)
{
	return 0x00000046;
}
static inline u32 gmmu_pte_kind_z24s8_1z_v(void)
{
	return 0x00000047;
}
static inline u32 gmmu_pte_kind_z24s8_ms2_1z_v(void)
{
	return 0x00000048;
}
static inline u32 gmmu_pte_kind_z24s8_ms4_1z_v(void)
{
	return 0x00000049;
}
static inline u32 gmmu_pte_kind_z24s8_ms8_1z_v(void)
{
	return 0x0000004a;
}
static inline u32 gmmu_pte_kind_z24s8_ms16_1z_v(void)
{
	return 0x0000004b;
}
static inline u32 gmmu_pte_kind_z24s8_2cs_v(void)
{
	return 0x0000004c;
}
static inline u32 gmmu_pte_kind_z24s8_ms2_2cs_v(void)
{
	return 0x0000004d;
}
static inline u32 gmmu_pte_kind_z24s8_ms4_2cs_v(void)
{
	return 0x0000004e;
}
static inline u32 gmmu_pte_kind_z24s8_ms8_2cs_v(void)
{
	return 0x0000004f;
}
static inline u32 gmmu_pte_kind_z24s8_ms16_2cs_v(void)
{
	return 0x00000050;
}
static inline u32 gmmu_pte_kind_z24s8_2cz_v(void)
{
	return 0x00000051;
}
static inline u32 gmmu_pte_kind_z24s8_ms2_2cz_v(void)
{
	return 0x00000052;
}
static inline u32 gmmu_pte_kind_z24s8_ms4_2cz_v(void)
{
	return 0x00000053;
}
static inline u32 gmmu_pte_kind_z24s8_ms8_2cz_v(void)
{
	return 0x00000054;
}
static inline u32 gmmu_pte_kind_z24s8_ms16_2cz_v(void)
{
	return 0x00000055;
}
static inline u32 gmmu_pte_kind_z24s8_4cszv_v(void)
{
	return 0x00000056;
}
static inline u32 gmmu_pte_kind_z24s8_ms2_4cszv_v(void)
{
	return 0x00000057;
}
static inline u32 gmmu_pte_kind_z24s8_ms4_4cszv_v(void)
{
	return 0x00000058;
}
static inline u32 gmmu_pte_kind_z24s8_ms8_4cszv_v(void)
{
	return 0x00000059;
}
static inline u32 gmmu_pte_kind_z24s8_ms16_4cszv_v(void)
{
	return 0x0000005a;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc12_v(void)
{
	return 0x0000005b;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc4_v(void)
{
	return 0x0000005c;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc8_v(void)
{
	return 0x0000005d;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc24_v(void)
{
	return 0x0000005e;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc12_1zv_v(void)
{
	return 0x00000063;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc4_1zv_v(void)
{
	return 0x00000064;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc8_1zv_v(void)
{
	return 0x00000065;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc24_1zv_v(void)
{
	return 0x00000066;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc12_2cs_v(void)
{
	return 0x00000067;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc4_2cs_v(void)
{
	return 0x00000068;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc8_2cs_v(void)
{
	return 0x00000069;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc24_2cs_v(void)
{
	return 0x0000006a;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc12_2czv_v(void)
{
	return 0x0000006f;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc4_2czv_v(void)
{
	return 0x00000070;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc8_2czv_v(void)
{
	return 0x00000071;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc24_2czv_v(void)
{
	return 0x00000072;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc12_2zv_v(void)
{
	return 0x00000073;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc4_2zv_v(void)
{
	return 0x00000074;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc8_2zv_v(void)
{
	return 0x00000075;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc24_2zv_v(void)
{
	return 0x00000076;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc12_4cszv_v(void)
{
	return 0x00000077;
}
static inline u32 gmmu_pte_kind_z24v8_ms4_vc4_4cszv_v(void)
{
	return 0x00000078;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc8_4cszv_v(void)
{
	return 0x00000079;
}
static inline u32 gmmu_pte_kind_z24v8_ms8_vc24_4cszv_v(void)
{
	return 0x0000007a;
}
static inline u32 gmmu_pte_kind_zf32_v(void)
{
	return 0x0000007b;
}
static inline u32 gmmu_pte_kind_zf32_1z_v(void)
{
	return 0x0000007c;
}
static inline u32 gmmu_pte_kind_zf32_ms2_1z_v(void)
{
	return 0x0000007d;
}
static inline u32 gmmu_pte_kind_zf32_ms4_1z_v(void)
{
	return 0x0000007e;
}
static inline u32 gmmu_pte_kind_zf32_ms8_1z_v(void)
{
	return 0x0000007f;
}
static inline u32 gmmu_pte_kind_zf32_ms16_1z_v(void)
{
	return 0x00000080;
}
static inline u32 gmmu_pte_kind_zf32_2cs_v(void)
{
	return 0x00000081;
}
static inline u32 gmmu_pte_kind_zf32_ms2_2cs_v(void)
{
	return 0x00000082;
}
static inline u32 gmmu_pte_kind_zf32_ms4_2cs_v(void)
{
	return 0x00000083;
}
static inline u32 gmmu_pte_kind_zf32_ms8_2cs_v(void)
{
	return 0x00000084;
}
static inline u32 gmmu_pte_kind_zf32_ms16_2cs_v(void)
{
	return 0x00000085;
}
static inline u32 gmmu_pte_kind_zf32_2cz_v(void)
{
	return 0x00000086;
}
static inline u32 gmmu_pte_kind_zf32_ms2_2cz_v(void)
{
	return 0x00000087;
}
static inline u32 gmmu_pte_kind_zf32_ms4_2cz_v(void)
{
	return 0x00000088;
}
static inline u32 gmmu_pte_kind_zf32_ms8_2cz_v(void)
{
	return 0x00000089;
}
static inline u32 gmmu_pte_kind_zf32_ms16_2cz_v(void)
{
	return 0x0000008a;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_v(void)
{
	return 0x0000008b;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_v(void)
{
	return 0x0000008c;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_v(void)
{
	return 0x0000008d;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_v(void)
{
	return 0x0000008e;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1cs_v(void)
{
	return 0x0000008f;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_1cs_v(void)
{
	return 0x00000090;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_1cs_v(void)
{
	return 0x00000091;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_1cs_v(void)
{
	return 0x00000092;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1zv_v(void)
{
	return 0x00000097;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_1zv_v(void)
{
	return 0x00000098;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_1zv_v(void)
{
	return 0x00000099;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_1zv_v(void)
{
	return 0x0000009a;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1czv_v(void)
{
	return 0x0000009b;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_1czv_v(void)
{
	return 0x0000009c;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_1czv_v(void)
{
	return 0x0000009d;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_1czv_v(void)
{
	return 0x0000009e;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_2cs_v(void)
{
	return 0x0000009f;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_2cs_v(void)
{
	return 0x000000a0;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_2cs_v(void)
{
	return 0x000000a1;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_2cs_v(void)
{
	return 0x000000a2;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_2cszv_v(void)
{
	return 0x000000a3;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_2cszv_v(void)
{
	return 0x000000a4;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_2cszv_v(void)
{
	return 0x000000a5;
}
static inline u32 gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_2cszv_v(void)
{
	return 0x000000a6;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_v(void)
{
	return 0x000000a7;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_v(void)
{
	return 0x000000a8;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_v(void)
{
	return 0x000000a9;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_v(void)
{
	return 0x000000aa;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1cs_v(void)
{
	return 0x000000ab;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_1cs_v(void)
{
	return 0x000000ac;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_1cs_v(void)
{
	return 0x000000ad;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_1cs_v(void)
{
	return 0x000000ae;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1zv_v(void)
{
	return 0x000000b3;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_1zv_v(void)
{
	return 0x000000b4;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_1zv_v(void)
{
	return 0x000000b5;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_1zv_v(void)
{
	return 0x000000b6;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1czv_v(void)
{
	return 0x000000b7;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_1czv_v(void)
{
	return 0x000000b8;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_1czv_v(void)
{
	return 0x000000b9;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_1czv_v(void)
{
	return 0x000000ba;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_2cs_v(void)
{
	return 0x000000bb;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_2cs_v(void)
{
	return 0x000000bc;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_2cs_v(void)
{
	return 0x000000bd;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_2cs_v(void)
{
	return 0x000000be;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_2cszv_v(void)
{
	return 0x000000bf;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_2cszv_v(void)
{
	return 0x000000c0;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_2cszv_v(void)
{
	return 0x000000c1;
}
static inline u32 gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_2cszv_v(void)
{
	return 0x000000c2;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_v(void)
{
	return 0x000000c3;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_1cs_v(void)
{
	return 0x000000c4;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms2_1cs_v(void)
{
	return 0x000000c5;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms4_1cs_v(void)
{
	return 0x000000c6;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms8_1cs_v(void)
{
	return 0x000000c7;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms16_1cs_v(void)
{
	return 0x000000c8;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_2cszv_v(void)
{
	return 0x000000ce;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms2_2cszv_v(void)
{
	return 0x000000cf;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms4_2cszv_v(void)
{
	return 0x000000d0;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms8_2cszv_v(void)
{
	return 0x000000d1;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms16_2cszv_v(void)
{
	return 0x000000d2;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_2cs_v(void)
{
	return 0x000000d3;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms2_2cs_v(void)
{
	return 0x000000d4;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms4_2cs_v(void)
{
	return 0x000000d5;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms8_2cs_v(void)
{
	return 0x000000d6;
}
static inline u32 gmmu_pte_kind_zf32_x24s8_ms16_2cs_v(void)
{
	return 0x000000d7;
}
static inline u32 gmmu_pte_kind_generic_16bx2_v(void)
{
	return 0x000000fe;
}
static inline u32 gmmu_pte_kind_c32_2c_v(void)
{
	return 0x000000d8;
}
static inline u32 gmmu_pte_kind_c32_2cbr_v(void)
{
	return 0x000000d9;
}
static inline u32 gmmu_pte_kind_c32_2cba_v(void)
{
	return 0x000000da;
}
static inline u32 gmmu_pte_kind_c32_2cra_v(void)
{
	return 0x000000db;
}
static inline u32 gmmu_pte_kind_c32_2bra_v(void)
{
	return 0x000000dc;
}
static inline u32 gmmu_pte_kind_c32_ms2_2c_v(void)
{
	return 0x000000dd;
}
static inline u32 gmmu_pte_kind_c32_ms2_2cbr_v(void)
{
	return 0x000000de;
}
static inline u32 gmmu_pte_kind_c32_ms2_2cra_v(void)
{
	return 0x000000cc;
}
static inline u32 gmmu_pte_kind_c32_ms4_2c_v(void)
{
	return 0x000000df;
}
static inline u32 gmmu_pte_kind_c32_ms4_2cbr_v(void)
{
	return 0x000000e0;
}
static inline u32 gmmu_pte_kind_c32_ms4_2cba_v(void)
{
	return 0x000000e1;
}
static inline u32 gmmu_pte_kind_c32_ms4_2cra_v(void)
{
	return 0x000000e2;
}
static inline u32 gmmu_pte_kind_c32_ms4_2bra_v(void)
{
	return 0x000000e3;
}
static inline u32 gmmu_pte_kind_c32_ms8_ms16_2c_v(void)
{
	return 0x000000e4;
}
static inline u32 gmmu_pte_kind_c32_ms8_ms16_2cra_v(void)
{
	return 0x000000e5;
}
static inline u32 gmmu_pte_kind_c64_2c_v(void)
{
	return 0x000000e6;
}
static inline u32 gmmu_pte_kind_c64_2cbr_v(void)
{
	return 0x000000e7;
}
static inline u32 gmmu_pte_kind_c64_2cba_v(void)
{
	return 0x000000e8;
}
static inline u32 gmmu_pte_kind_c64_2cra_v(void)
{
	return 0x000000e9;
}
static inline u32 gmmu_pte_kind_c64_2bra_v(void)
{
	return 0x000000ea;
}
static inline u32 gmmu_pte_kind_c64_ms2_2c_v(void)
{
	return 0x000000eb;
}
static inline u32 gmmu_pte_kind_c64_ms2_2cbr_v(void)
{
	return 0x000000ec;
}
static inline u32 gmmu_pte_kind_c64_ms2_2cra_v(void)
{
	return 0x000000cd;
}
static inline u32 gmmu_pte_kind_c64_ms4_2c_v(void)
{
	return 0x000000ed;
}
static inline u32 gmmu_pte_kind_c64_ms4_2cbr_v(void)
{
	return 0x000000ee;
}
static inline u32 gmmu_pte_kind_c64_ms4_2cba_v(void)
{
	return 0x000000ef;
}
static inline u32 gmmu_pte_kind_c64_ms4_2cra_v(void)
{
	return 0x000000f0;
}
static inline u32 gmmu_pte_kind_c64_ms4_2bra_v(void)
{
	return 0x000000f1;
}
static inline u32 gmmu_pte_kind_c64_ms8_ms16_2c_v(void)
{
	return 0x000000f2;
}
static inline u32 gmmu_pte_kind_c64_ms8_ms16_2cra_v(void)
{
	return 0x000000f3;
}
static inline u32 gmmu_pte_kind_c128_2c_v(void)
{
	return 0x000000f4;
}
static inline u32 gmmu_pte_kind_c128_2cr_v(void)
{
	return 0x000000f5;
}
static inline u32 gmmu_pte_kind_c128_ms2_2c_v(void)
{
	return 0x000000f6;
}
static inline u32 gmmu_pte_kind_c128_ms2_2cr_v(void)
{
	return 0x000000f7;
}
static inline u32 gmmu_pte_kind_c128_ms4_2c_v(void)
{
	return 0x000000f8;
}
static inline u32 gmmu_pte_kind_c128_ms4_2cr_v(void)
{
	return 0x000000f9;
}
static inline u32 gmmu_pte_kind_c128_ms8_ms16_2c_v(void)
{
	return 0x000000fa;
}
static inline u32 gmmu_pte_kind_c128_ms8_ms16_2cr_v(void)
{
	return 0x000000fb;
}
static inline u32 gmmu_pte_kind_x8c24_v(void)
{
	return 0x000000fc;
}
static inline u32 gmmu_pte_kind_pitch_no_swizzle_v(void)
{
	return 0x000000fd;
}
static inline u32 gmmu_pte_kind_smsked_message_v(void)
{
	return 0x000000ca;
}
static inline u32 gmmu_pte_kind_smhost_message_v(void)
{
	return 0x000000cb;
}
#endif
