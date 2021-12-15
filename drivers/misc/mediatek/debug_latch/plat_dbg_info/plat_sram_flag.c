// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include "archcounter_timesync.h"
#include "plat_sram_flag.h"
#ifdef CONFIG_MTK_BUS_TRACER
#include <asm/arch_timer.h>
#endif

static struct plat_sram_flag *plat;

static inline unsigned int extract_n2mbits(unsigned int input,
		unsigned int n, unsigned int m)
{
	/*
	 * 1. ~0 = 1111 1111 1111 1111 1111 1111 1111 1111
	 * 2. ~0 << (m - n + 1) = 1111 1111 1111 1111 1100 0000 0000 0000
	 * assuming we are extracting 14 bits,
	 * the +1 is added for inclusive selection
	 * 3. ~(~0 << (m - n + 1)) = 0000 0000 0000 0000 0011 1111 1111 1111
	 */
	int mask;

	if (n > m) {
		n = n + m;
		m = n - m;
		n = n - m;
	}
	mask = ~(~0 << (m - n + 1));
	return (input >> n) & mask;
}

static int check_sram_base(void)
{
	if (plat)
		return 0;

	pr_notice("%s:%d: sram_base == 0x0\n", __func__, __LINE__);
	return -1;
}

/* return negative integer if fails */
int set_sram_flag_lastpc_valid(void)
{
	if (check_sram_base() < 0)
		return -1;

	plat->plat_sram_flag0 =
		(plat->plat_sram_flag0 | (1 << OFFSET_LASTPC_VALID));
	return 0;
}
EXPORT_SYMBOL(set_sram_flag_lastpc_valid);

/* return negative integer if fails */
int set_sram_flag_etb_user(unsigned int etb_id, unsigned int user_id)
{
	if (check_sram_base() < 0)
		return -1;

	if (etb_id >= MAX_ETB_NUM) {
		pr_notice("%s:%d: etb_id > MAX_ETB_NUM\n",
				__func__, __LINE__);
		return -1;
	}

	if (user_id >= MAX_ETB_USER_NUM) {
		pr_notice("%s:%d: user_id > MAX_ETB_USER_NUM\n",
				__func__, __LINE__);
		return -1;
	}

	plat->plat_sram_flag0 =
		(plat->plat_sram_flag0 & ~(0x7 << (OFFSET_ETB_0 + etb_id*3)))
		| ((user_id & 0x7) << (OFFSET_ETB_0 + etb_id*3));

	return 0;
}
EXPORT_SYMBOL(set_sram_flag_etb_user);

#ifdef CONFIG_MTK_BUS_TRACER
int set_sram_flag_timestamp(void)
{
	u64 tick, ts, boot_time;
	if (check_sram_base() < 0)
		return -1;
	ts = sched_clock_get_cyc(&tick);
	pr_notice("%s: tick=0x%llx, ts=%llu\n", __func__, tick, ts);
	ts = sched_clock();
	boot_time = mtk_get_archcounter_time(arch_counter_get_cntvct());
	boot_time -= ts;
#if BITS_PER_LONG == 32
	boot_time = div_u64((boot_time*13), 1000);
#else
	boot_time = (boot_time*13)/1000;
#endif
	plat->plat_sram_flag0 = boot_time;
	pr_notice("%s: kernel_start_tick = 0x%x\n", __func__,
			plat->plat_sram_flag0);
	return 0;
}
EXPORT_SYMBOL(set_sram_flag_timestamp);
#endif

/* return negative integer if fails */
int set_sram_flag_dfd_valid(void)
{
	if (check_sram_base() < 0)
		return -1;

	plat->plat_sram_flag1 =
		(plat->plat_sram_flag1 | (1 << OFFSET_DFD_VALID));

	return 0;
}
EXPORT_SYMBOL(set_sram_flag_dfd_valid);

static struct platform_driver plat_sram_flag_drv = {
	.driver = {
		.name = "plat_sram_flag",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
};

static ssize_t plat_sram_flag_dump_show(struct device_driver *driver,
		char *buf)
{
	unsigned int i;
	char *wp = buf;

	if (!plat) {
		pr_notice("%s:%d: sram_base == 0x0\n", __func__, __LINE__);
		return snprintf(buf, PAGE_SIZE, "sram_base == 0x0\n");
	}

	wp += snprintf(wp, PAGE_SIZE,
			"plat_sram_flag0 = 0x%x\n", plat->plat_sram_flag0);
	wp += snprintf(wp, PAGE_SIZE,
			"plat_sram_flag1 = 0x%x\nplat_sram_flag2 = 0x%x\n",
			plat->plat_sram_flag1, plat->plat_sram_flag2);

	wp += snprintf(wp, PAGE_SIZE, "\n-------------\n");

	wp += snprintf(wp, PAGE_SIZE, "lastpc_valid = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag0,
				OFFSET_LASTPC_VALID, OFFSET_LASTPC_VALID));
	wp += snprintf(wp, PAGE_SIZE, "lastpc_valid_before_reboot = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag0,
				OFFSET_LASTPC_VALID_BEFORE_REBOOT,
				OFFSET_LASTPC_VALID_BEFORE_REBOOT));

	for (i = 0; i <= MAX_ETB_NUM-1; ++i)
		wp += snprintf(wp, PAGE_SIZE,
			"user_id_of_multi_user_etb_%d = 0x%03x\n",
			i, extract_n2mbits(plat->plat_sram_flag0,
				OFFSET_ETB_0 + i*3, OFFSET_ETB_0 + i*3 + 2));

	wp += snprintf(wp, PAGE_SIZE, "dfd_valid = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag1,
				OFFSET_DFD_VALID, OFFSET_DFD_VALID));
	wp += snprintf(wp, PAGE_SIZE, "dfd_valid_before_reboot = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag1,
				OFFSET_DFD_VALID_BEFORE_REBOOT,
				OFFSET_DFD_VALID_BEFORE_REBOOT));

	return strlen(buf);
}

static DRIVER_ATTR_RO(plat_sram_flag_dump);

static int __init plat_sram_flag_init(void)
{
	int ret = 0;
	unsigned int size;

	plat = (struct plat_sram_flag *)get_dbg_info_base(PLAT_SRAM_FLAG_KEY);
	if (!plat)
		return -EINVAL;

	size = get_dbg_info_size(PLAT_SRAM_FLAG_KEY);
	if (size != sizeof(struct plat_sram_flag)) {
		pr_debug("[SRAM FLAG] Can't match plat_sram_flag size\n");
		return -EINVAL;
	}

	ret = platform_driver_register(&plat_sram_flag_drv);
	if (ret)
		return -ENODEV;

	ret = driver_create_file(&plat_sram_flag_drv.driver,
			&driver_attr_plat_sram_flag_dump);
	if (ret)
		pr_notice("%s:%d: driver_create_file failed.\n",
				__func__, __LINE__);


	return 0;
}

core_initcall(plat_sram_flag_init);
