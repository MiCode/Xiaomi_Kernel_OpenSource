// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include "inc/mtk_aee_rr.h"

uint32_t ptp_devinfo_0;
uint32_t ptp_devinfo_1;
uint32_t ptp_devinfo_2;
uint32_t ptp_devinfo_3;
uint32_t ptp_devinfo_4;
uint32_t ptp_devinfo_5;
uint32_t ptp_devinfo_6;
uint32_t ptp_devinfo_7;
uint32_t ptp_e0;
uint32_t ptp_e1;
uint32_t ptp_e2;
uint32_t ptp_e3;
uint32_t ptp_e4;
uint32_t ptp_e5;
uint32_t ptp_e6;
uint32_t ptp_e7;
uint32_t ptp_e8;
uint32_t ptp_e9;
uint32_t ptp_e10;
uint32_t ptp_e11;
uint64_t ptp_vboot;
uint64_t ptp_cpu_big_volt;
uint64_t ptp_cpu_big_volt_1;
uint64_t ptp_cpu_big_volt_2;
uint64_t ptp_cpu_big_volt_3;
uint64_t ptp_cpu_2_little_volt;
uint64_t ptp_cpu_2_little_volt_1;
uint64_t ptp_cpu_2_little_volt_2;
uint64_t ptp_cpu_2_little_volt_3;
uint64_t ptp_cpu_little_volt;
uint64_t ptp_cpu_little_volt_1;
uint64_t ptp_cpu_little_volt_2;
uint64_t ptp_cpu_little_volt_3;
uint64_t ptp_cpu_cci_volt;
uint64_t ptp_cpu_cci_volt_1;
uint64_t ptp_cpu_cci_volt_2;
uint64_t ptp_cpu_cci_volt_3;
uint64_t ptp_gpu_volt;
uint64_t ptp_gpu_volt_1;
uint64_t ptp_gpu_volt_2;
uint64_t ptp_gpu_volt_3;
uint64_t ptp_temp;
uint8_t ptp_status;

void set_ptp_devinfo_0(u32 val)
{
	ptp_devinfo_0 = val;
}

void set_ptp_devinfo_1(u32 val)
{
	ptp_devinfo_1 = val;
}

void set_ptp_devinfo_2(u32 val)
{
	ptp_devinfo_2 = val;
}

void set_ptp_devinfo_3(u32 val)
{
	ptp_devinfo_3 = val;
}

void set_ptp_devinfo_4(u32 val)
{
	ptp_devinfo_4 = val;
}

void set_ptp_devinfo_5(u32 val)
{
	ptp_devinfo_5 = val;
}

void set_ptp_devinfo_6(u32 val)
{
	ptp_devinfo_6 = val;
}

void set_ptp_devinfo_7(u32 val)
{
	ptp_devinfo_7 = val;
}

void set_ptp_e0(u32 val)
{
	ptp_e0 = val;
}

void set_ptp_e1(u32 val)
{
	ptp_e1 = val;
}

void set_ptp_e2(u32 val)
{
	ptp_e2 = val;
}

void set_ptp_e3(u32 val)
{
	ptp_e3 = val;
}

void set_ptp_e4(u32 val)
{
	ptp_e4 = val;
}

void set_ptp_e5(u32 val)
{
	ptp_e5 = val;
}

void set_ptp_e6(u32 val)
{
	ptp_e6 = val;
}

void set_ptp_e7(u32 val)
{
	ptp_e7 = val;
}

void set_ptp_e8(u32 val)
{
	ptp_e8 = val;
}

void set_ptp_e9(u32 val)
{
	ptp_e9 = val;
}

void set_ptp_e10(u32 val)
{
	ptp_e10 = val;
}

void set_ptp_e11(u32 val)
{
	ptp_e11 = val;
}

void set_ptp_vboot(u64 val)
{
	ptp_vboot = val;
}

void set_ptp_cpu_big_volt(u64 val)
{
	ptp_cpu_big_volt = val;
}

void set_ptp_cpu_big_volt_1(u64 val)
{
	ptp_cpu_big_volt_1 = val;
}

void set_ptp_cpu_big_volt_2(u64 val)
{
	ptp_cpu_big_volt_2 = val;
}

void set_ptp_cpu_big_volt_3(u64 val)
{
	ptp_cpu_big_volt_3 = val;
}

void set_ptp_cpu_2_little_volt(u64 val)
{
	ptp_cpu_2_little_volt = val;
}

void set_ptp_cpu_2_little_volt_1(u64 val)
{
	ptp_cpu_2_little_volt_1 = val;
}

void set_ptp_cpu_2_little_volt_2(u64 val)
{
	ptp_cpu_2_little_volt_2 = val;
}

void set_ptp_cpu_2_little_volt_3(u64 val)
{
	ptp_cpu_2_little_volt_3 = val;
}

void set_ptp_cpu_little_volt(u64 val)
{
	ptp_cpu_little_volt = val;
}

void set_ptp_cpu_little_volt_1(u64 val)
{
	ptp_cpu_little_volt_1 = val;
}

void set_ptp_cpu_little_volt_2(u64 val)
{
	ptp_cpu_little_volt_2 = val;
}

void set_ptp_cpu_little_volt_3(u64 val)
{
	ptp_cpu_little_volt_3 = val;
}

void set_ptp_cpu_cci_volt(u64 val)
{
	ptp_cpu_cci_volt = val;
}

void set_ptp_cpu_cci_volt_1(u64 val)
{
	ptp_cpu_cci_volt_1 = val;
}

void set_ptp_cpu_cci_volt_2(u64 val)
{
	ptp_cpu_cci_volt_2 = val;
}

void set_ptp_cpu_cci_volt_3(u64 val)
{
	ptp_cpu_cci_volt_3 = val;
}

void set_ptp_gpu_volt(u64 val)
{
	ptp_gpu_volt = val;
}
EXPORT_SYMBOL(set_ptp_gpu_volt);

void set_ptp_gpu_volt_1(u64 val)
{
	ptp_gpu_volt_1 = val;
}
EXPORT_SYMBOL(set_ptp_gpu_volt_1);

void set_ptp_gpu_volt_2(u64 val)
{
	ptp_gpu_volt_2 = val;
}
EXPORT_SYMBOL(set_ptp_gpu_volt_2);

void set_ptp_gpu_volt_3(u64 val)
{
	ptp_gpu_volt_3 = val;
}
EXPORT_SYMBOL(set_ptp_gpu_volt_3);

void set_ptp_temp(u64 val)
{
	ptp_temp = val;
}
EXPORT_SYMBOL(set_ptp_temp);

void set_ptp_status(u8 val)
{
	ptp_status = val;
}
EXPORT_SYMBOL(set_ptp_status);


u32 get_ptp_devinfo_0(void)
{
	return ptp_devinfo_0;
}

u32 get_ptp_devinfo_1(void)
{
	return ptp_devinfo_1;
}

u32 get_ptp_devinfo_2(void)
{
	return ptp_devinfo_2;
}

u32 get_ptp_devinfo_3(void)
{
	return ptp_devinfo_3;
}

u32 get_ptp_devinfo_4(void)
{
	return ptp_devinfo_4;
}

u32 get_ptp_devinfo_5(void)
{
	return ptp_devinfo_5;
}

u32 get_ptp_devinfo_6(void)
{
	return ptp_devinfo_6;
}

u32 get_ptp_devinfo_7(void)
{
	return ptp_devinfo_7;
}

u32 get_ptp_e0(void)
{
	return ptp_e0;
}

u32 get_ptp_e1(void)
{
	return ptp_e1;
}

u32 get_ptp_e2(void)
{
	return ptp_e2;
}

u32 get_ptp_e3(void)
{
	return ptp_e3;
}

u32 get_ptp_e4(void)
{
	return ptp_e4;
}

u32 get_ptp_e5(void)
{
	return ptp_e5;
}

u32 get_ptp_e6(void)
{
	return ptp_e6;
}

u32 get_ptp_e7(void)
{
	return ptp_e7;
}

u32 get_ptp_e8(void)
{
	return ptp_e8;
}

u32 get_ptp_e9(void)
{
	return ptp_e9;
}

u32 get_ptp_e10(void)
{
	return ptp_e10;
}

u32 get_ptp_e11(void)
{
	return ptp_e11;
}

u64 get_ptp_vboot(void)
{
	return ptp_vboot;
}

u64 get_ptp_cpu_big_volt(void)
{
	return ptp_cpu_big_volt;
}

u64 get_ptp_cpu_big_volt_1(void)
{
	return ptp_cpu_big_volt_1;
}

u64 get_ptp_cpu_big_volt_2(void)
{
	return ptp_cpu_big_volt_2;
}

u64 get_ptp_cpu_big_volt_3(void)
{
	return ptp_cpu_big_volt_3;
}

u64 get_ptp_cpu_2_little_volt(void)
{
	return ptp_cpu_2_little_volt;
}

u64 get_ptp_cpu_2_little_volt_1(void)
{
	return ptp_cpu_2_little_volt_1;
}

u64 get_ptp_cpu_2_little_volt_2(void)
{
	return ptp_cpu_2_little_volt_2;
}

u64 get_ptp_cpu_2_little_volt_3(void)
{
	return ptp_cpu_2_little_volt_3;
}

u64 get_ptp_cpu_little_volt(void)
{
	return ptp_cpu_little_volt;
}

u64 get_ptp_cpu_little_volt_1(void)
{
	return ptp_cpu_little_volt_1;
}

u64 get_ptp_cpu_little_volt_2(void)
{
	return ptp_cpu_little_volt_2;
}

u64 get_ptp_cpu_little_volt_3(void)
{
	return ptp_cpu_little_volt_3;
}

u64 get_ptp_cpu_cci_volt(void)
{
	return ptp_cpu_cci_volt;
}

u64 get_ptp_cpu_cci_volt_1(void)
{
	return ptp_cpu_cci_volt_1;
}

u64 get_ptp_cpu_cci_volt_2(void)
{
	return ptp_cpu_cci_volt_2;
}

u64 get_ptp_cpu_cci_volt_3(void)
{
	return ptp_cpu_cci_volt_3;
}

u64 get_ptp_gpu_volt(void)
{
	return ptp_gpu_volt;
}
EXPORT_SYMBOL(get_ptp_gpu_volt);

u64 get_ptp_gpu_volt_1(void)
{
	return ptp_gpu_volt_1;
}
EXPORT_SYMBOL(get_ptp_gpu_volt_1);

u64 get_ptp_gpu_volt_2(void)
{
	return ptp_gpu_volt_2;
}
EXPORT_SYMBOL(get_ptp_gpu_volt_2);

u64 get_ptp_gpu_volt_3(void)
{
	return ptp_gpu_volt_3;
}
EXPORT_SYMBOL(get_ptp_gpu_volt_3);

u64 get_ptp_temp(void)
{
	return ptp_temp;
}
EXPORT_SYMBOL(get_ptp_temp);

u8 get_ptp_status(void)
{
	return ptp_status;
}
EXPORT_SYMBOL(get_ptp_status);

