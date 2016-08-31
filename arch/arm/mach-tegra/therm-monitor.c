/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 *
 * arch/arm/mach-tegra/therm-monitor.c
 *
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/platform_data/tmon_tmp411.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <asm/io.h>
#include "therm-monitor.h"

static struct therm_monitor_ldep_data *lc_temp_reg_data;

static struct tmon_plat_data tmon_pdata;

/* For now assume only one entry */
struct i2c_board_info __initdata tgr_i2c_board_info[1];

/*  T30 USB Base address:                  USB1,       USB2,       USB3     */
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static unsigned int s_tegra_usb_base[] = {0x7D000000, 0x7D004000, 0x7D008000};
#endif

/*  Fuse USB Calib Value. */
static unsigned int s_fuse_usb_calib_value;

static void get_fuse_usb_calib_value(void)
{
	s_fuse_usb_calib_value = readl(IO_TO_VIRT(TEGRA_FUSE_BASE +
						FUSE_USB_CALIB_0));
}

static inline void pg_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_APB_MISC_BASE) + offset);
}

static void utmip_temp_dep_update(int curr_rtemp, int utmip_temp_bound)
{
	static int prev_temp;
	static char initial_update = 1;
	unsigned int fuse_usb_calib_value = s_fuse_usb_calib_value;
	int i;
	char utmip_update_require = 0;

	/* Extract bits[3:0]. */
	fuse_usb_calib_value = fuse_usb_calib_value & 0xF;

	/* If previous and currentt temperatures falls in the same temperature
	   boundary then no need to update the  UTMIP_SPARE_CFG */
	if ((prev_temp >= utmip_temp_bound) && (curr_rtemp >= utmip_temp_bound))
		utmip_update_require = 0;
	else if ((prev_temp < utmip_temp_bound) &&
				(curr_rtemp < utmip_temp_bound))
		utmip_update_require = 0;
	else
		utmip_update_require = 1;

	/*  For Initial call , need to update the UTMIP_SPARE_CFG */
	if (initial_update) {
		utmip_update_require = 1;
		initial_update = 0;
		prev_temp = curr_rtemp;
	}

	if (utmip_update_require) {
		prev_temp = curr_rtemp;
		if (curr_rtemp >= utmip_temp_bound) {
			fuse_usb_calib_value += 0x1;
			/*  Check if there is a overflow. */
			if (fuse_usb_calib_value > UTMIP_XCVR_SETUP_MAX_VAL)
				fuse_usb_calib_value = UTMIP_XCVR_SETUP_MAX_VAL;
		}

		for (i = 0; i < ARRAY_SIZE(s_tegra_usb_base); i++) {
			unsigned int regval;
			regval = readl(IO_TO_VIRT(s_tegra_usb_base[i] +
					UTMIP_SPARE_CFG0));
			regval &= ~FUSE_SETUP_SEL;
			writel(regval, IO_TO_VIRT(s_tegra_usb_base[i] +
					UTMIP_SPARE_CFG0));

			regval = readl(IO_TO_VIRT(s_tegra_usb_base[i] +
					UTMIP_XCVR_CFG0));
			/* If low_to_high, then write 0x2 to HSSLEW_MSB
			 else write 0x8 */
			regval &= ~UTMIP_XCVR_HSSLEW_MSB_MSK;
			regval |= (curr_rtemp >= utmip_temp_bound) ?
					UTMIP_XCVR_HSSLEW_MSB_HIGH_TEMP_VAL :
					UTMIP_XCVR_HSSLEW_MSB_LOW_TEMP_VAL;
			/* write fuse_usb_calib_value to SETUP field
			of UTMIP_XCVR_CFG0. */
			regval &= ~UTMIP_XCVR_SETUP_MSK;
			regval |= fuse_usb_calib_value;
			writel(regval, IO_TO_VIRT(s_tegra_usb_base[i] +
					UTMIP_XCVR_CFG0));
		}
	}
}

/* Call back function, invoked by driver */
static void ltemp_dependent_reg_update(int curr_ltemp)
{
	int i, j;
	for (i = 0; lc_temp_reg_data[i].reg_addr != INVALID_ADDR; i++) {
		for (j = 0; ((j < MAX_NUM_TEMPERAT) &&
			(lc_temp_reg_data[i].temperat[j] != INT_MAX)); j++) {
			if (curr_ltemp <= lc_temp_reg_data[i].temperat[j]) {
				if (lc_temp_reg_data[i].previous_val !=
						lc_temp_reg_data[i].value[j]) {
					pg_writel(lc_temp_reg_data[i].value[j],
						lc_temp_reg_data[i].reg_addr);
					lc_temp_reg_data[i].previous_val =
						lc_temp_reg_data[i].value[j];
				}
				break;
			}
		}
	}
}

void register_therm_monitor(struct therm_monitor_data *brd_therm_monitor_data)
{
	/* Array which has list of register values with temperature ranges */
	lc_temp_reg_data = brd_therm_monitor_data->brd_ltemp_reg_data;

	/* Thermal monitor operational parameters */
	tmon_pdata.delta_temp = brd_therm_monitor_data->delta_temp;
	tmon_pdata.delta_time = brd_therm_monitor_data->delta_time;
	tmon_pdata.remote_offset = brd_therm_monitor_data->remote_offset;

	/* Local temperature monitoring: Used for pad controls */
	if (brd_therm_monitor_data->local_temp_update)
		tmon_pdata.ltemp_dependent_reg_update =
			ltemp_dependent_reg_update;

	/* utmip registers update */
	if (brd_therm_monitor_data->utmip_reg_update) {
		tmon_pdata.utmip_temp_bound =
			brd_therm_monitor_data->utmip_temp_bound;
		tmon_pdata.utmip_temp_dep_update =
			utmip_temp_dep_update;
	}

	/* Fill the i2c board info */
	strcpy(tgr_i2c_board_info[0].type,
		brd_therm_monitor_data->i2c_dev_name);
	tgr_i2c_board_info[0].addr = brd_therm_monitor_data->i2c_dev_addrs;
	tgr_i2c_board_info[0].platform_data = &tmon_pdata;

	i2c_register_board_info(brd_therm_monitor_data->i2c_bus_num,
				tgr_i2c_board_info, 1);
	get_fuse_usb_calib_value();
}
