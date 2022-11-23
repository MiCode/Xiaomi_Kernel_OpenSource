/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */
#include <linux/console.h>
#include <linux/pstore.h>
#include <linux/seq_file.h>

extern void set_ptp_devinfo_0(u32 val);
extern void set_ptp_devinfo_1(u32 val);
extern void set_ptp_devinfo_2(u32 val);
extern void set_ptp_devinfo_3(u32 val);
extern void set_ptp_devinfo_4(u32 val);
extern void set_ptp_devinfo_5(u32 val);
extern void set_ptp_devinfo_6(u32 val);
extern void set_ptp_devinfo_7(u32 val);
extern void set_ptp_e0(u32 val);
extern void set_ptp_e1(u32 val);
extern void set_ptp_e2(u32 val);
extern void set_ptp_e3(u32 val);
extern void set_ptp_e4(u32 val);
extern void set_ptp_e5(u32 val);
extern void set_ptp_e6(u32 val);
extern void set_ptp_e7(u32 val);
extern void set_ptp_e8(u32 val);
extern void set_ptp_e9(u32 val);
extern void set_ptp_e10(u32 val);
extern void set_ptp_e11(u32 val);
extern void set_ptp_vboot(u64 val);
extern void set_ptp_cpu_big_volt(u64 val);
extern void set_ptp_cpu_big_volt_1(u64 val);
extern void set_ptp_cpu_big_volt_2(u64 val);
extern void set_ptp_cpu_big_volt_3(u64 val);
extern void set_ptp_gpu_volt(u64 val);
extern void set_ptp_gpu_volt_1(u64 val);
extern void set_ptp_gpu_volt_2(u64 val);
extern void set_ptp_gpu_volt_3(u64 val);
extern void set_ptp_cpu_little_volt(u64 val);
extern void set_ptp_cpu_little_volt_1(u64 val);
extern void set_ptp_cpu_little_volt_2(u64 val);
extern void set_ptp_cpu_little_volt_3(u64 val);
extern void set_ptp_cpu_2_little_volt(u64 val);
extern void set_ptp_cpu_2_little_volt_1(u64 val);
extern void set_ptp_cpu_2_little_volt_2(u64 val);
extern void set_ptp_cpu_2_little_volt_3(u64 val);
extern void set_ptp_cpu_cci_volt(u64 val);
extern void set_ptp_cpu_cci_volt_1(u64 val);
extern void set_ptp_cpu_cci_volt_2(u64 val);
extern void set_ptp_cpu_cci_volt_3(u64 val);
extern void set_ptp_temp(u64 val);
extern void set_ptp_status(u8 val);

extern u32 get_ptp_devinfo_0(void);
extern u32 get_ptp_devinfo_1(void);
extern u32 get_ptp_devinfo_2(void);
extern u32 get_ptp_devinfo_3(void);
extern u32 get_ptp_devinfo_4(void);
extern u32 get_ptp_devinfo_5(void);
extern u32 get_ptp_devinfo_6(void);
extern u32 get_ptp_devinfo_7(void);
extern u32 get_ptp_e0(void);
extern u32 get_ptp_e1(void);
extern u32 get_ptp_e2(void);
extern u32 get_ptp_e3(void);
extern u32 get_ptp_e4(void);
extern u32 get_ptp_e5(void);
extern u32 get_ptp_e6(void);
extern u32 get_ptp_e7(void);
extern u32 get_ptp_e8(void);
extern u32 get_ptp_e9(void);
extern u32 get_ptp_e10(void);
extern u32 get_ptp_e11(void);
extern u64 get_ptp_vboot(void);
extern u64 get_ptp_cpu_big_volt(void);
extern u64 get_ptp_cpu_big_volt_1(void);
extern u64 get_ptp_cpu_big_volt_2(void);
extern u64 get_ptp_cpu_big_volt_3(void);
extern u64 get_ptp_gpu_volt(void);
extern u64 get_ptp_gpu_volt_1(void);
extern u64 get_ptp_gpu_volt_2(void);
extern u64 get_ptp_gpu_volt_3(void);
extern u64 get_ptp_cpu_little_volt(void);
extern u64 get_ptp_cpu_little_volt_1(void);
extern u64 get_ptp_cpu_little_volt_2(void);
extern u64 get_ptp_cpu_little_volt_3(void);
extern u64 get_ptp_cpu_2_little_volt(void);
extern u64 get_ptp_cpu_2_little_volt_1(void);
extern u64 get_ptp_cpu_2_little_volt_2(void);
extern u64 get_ptp_cpu_2_little_volt_3(void);
extern u64 get_ptp_cpu_cci_volt(void);
extern u64 get_ptp_cpu_cci_volt_1(void);
extern u64 get_ptp_cpu_cci_volt_2(void);
extern u64 get_ptp_cpu_cci_volt_3(void);
extern u64 get_ptp_temp(void);
extern u8 get_ptp_status(void);

