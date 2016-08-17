/*
 * arch/arm/mach-tegra/board.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Erik Gilling <konkers@google.com>
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

#ifndef __MACH_TEGRA_BOARD_H
#define __MACH_TEGRA_BOARD_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/memory.h>

#ifdef CONFIG_TEGRA_NVDUMPER
#define NVDUMPER_RESERVED_SIZE 4096UL
#endif

#define ADD_FIXED_VOLTAGE_REG(_name)	(&_name##_fixed_voltage_device)

/* Macro for defining fixed voltage regulator */
#define FIXED_VOLTAGE_REG_INIT(_id, _name, _microvolts, _gpio,		\
		_startup_delay, _enable_high, _enabled_at_boot,		\
		_valid_ops_mask, _always_on)				\
	static struct regulator_init_data _name##_initdata = {		\
		.consumer_supplies = _name##_consumer_supply,		\
		.num_consumer_supplies =				\
				ARRAY_SIZE(_name##_consumer_supply),	\
		.constraints = {					\
			.valid_ops_mask = _valid_ops_mask ,		\
			.always_on = _always_on,			\
		},							\
	};								\
	static struct fixed_voltage_config _name##_config = {		\
		.supply_name		= #_name,			\
		.microvolts		= _microvolts,			\
		.gpio			= _gpio,			\
		.startup_delay		= _startup_delay,		\
		.enable_high		= _enable_high,			\
		.enabled_at_boot	= _enabled_at_boot,		\
		.init_data		= &_name##_initdata,		\
	};								\
	static struct platform_device _name##_fixed_voltage_device = {	\
		.name			= "reg-fixed-voltage",		\
		.id			= _id,				\
		.dev			= {				\
			.platform_data	= &_name##_config,		\
		},							\
	}

#if defined(CONFIG_TEGRA_NVMAP)
#define NVMAP_HEAP_CARVEOUT_IRAM_INIT	\
	{	.name		= "iram",					\
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,			\
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,	\
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,	\
		.buddy_size	= 0, /* no buddy allocation for IRAM */		\
	}
#endif

/* This information is passed by bootloader */
#define COMMCHIP_DEFAULT		0
#define COMMCHIP_NOCHIP			1
#define COMMCHIP_BROADCOM_BCM4329	2
#define COMMCHIP_BROADCOM_BCM4330	3
#define COMMCHIP_MARVELL_SD8797		4
#define COMMCHIP_TI_WL18XX		5
#define COMMCHIP_BROADCOM_BCM43241	6

struct memory_accessor;

void tegra_assert_system_reset(char mode, const char *cmd);
void get_mac_addr(struct memory_accessor *, void *);

void __init tegra20_init_early(void);
void __init tegra30_init_early(void);
void __init tegra11x_init_early(void);
void __init tegra_map_common_io(void);
void __init tegra_init_irq(void);
void __init tegra_dt_init_irq(void);
void __init tegra_reserve(unsigned long carveout_size, unsigned long fb_size,
	unsigned long fb2_size);
void __init tegra_release_bootloader_fb(void);
void __init tegra_protected_aperture_init(unsigned long aperture);
int  __init tegra_init_board_info(void);
void __tegra_move_framebuffer(struct platform_device *pdev,
			      unsigned long to, unsigned long from,
			      unsigned long size);
static inline void tegra_move_framebuffer(unsigned long to, unsigned long from,
					  unsigned long size)
{
	__tegra_move_framebuffer(NULL, to, from, size);
}
void __tegra_clear_framebuffer(struct platform_device *pdev,
			       unsigned long to, unsigned long size);
static inline void tegra_clear_framebuffer(unsigned long to, unsigned long size)
{
	__tegra_clear_framebuffer(NULL, to, size);
}
bool is_tegra_debug_uartport_hs(void);
int get_tegra_uart_debug_port_id(void);
int arb_lost_recovery(int scl_gpio, int sda_gpio);
int __init tegra_register_fuse(void);

#ifdef CONFIG_ANDROID_RAM_CONSOLE
void __init tegra_ram_console_debug_reserve(unsigned long ram_console_size);
void __init tegra_ram_console_debug_init(void);
#else
static inline void __init tegra_ram_console_debug_reserve(unsigned long ram_console_size)
{}
static inline void __init tegra_ram_console_debug_init(void)
{}
#endif

extern unsigned long tegra_bootloader_fb_start;
extern unsigned long tegra_bootloader_fb_size;
extern unsigned long tegra_bootloader_fb2_start;
extern unsigned long tegra_bootloader_fb2_size;
extern unsigned long tegra_fb_start;
extern unsigned long tegra_fb_size;
extern unsigned long tegra_fb2_start;
extern unsigned long tegra_fb2_size;
extern unsigned long tegra_carveout_start;
extern unsigned long tegra_carveout_size;
extern unsigned long tegra_vpr_start;
extern unsigned long tegra_vpr_size;
extern unsigned long tegra_lp0_vec_start;
extern unsigned long tegra_lp0_vec_size;
#ifdef CONFIG_TEGRA_NVDUMPER
extern unsigned long nvdumper_reserved;
#endif
extern bool tegra_lp0_vec_relocate;
extern unsigned long tegra_grhost_aperture;
#ifdef CONFIG_TEGRA_USE_NCT
/* info for NCK(NCT for Kernel) carveout area */
extern unsigned long tegra_nck_start;
extern unsigned long tegra_nck_size;
#endif

extern struct sys_timer tegra_timer;

enum board_fab {
	BOARD_FAB_A = 0,
	BOARD_FAB_B,
	BOARD_FAB_C,
	BOARD_FAB_D,
};

struct board_info {
	u16 board_id;
	u16 sku;
	u8  fab;
	u8  major_revision;
	u8  minor_revision;
};

enum panel_type {
	panel_type_lvds = 0,
	panel_type_dsi,
};

enum audio_codec_type {
	audio_codec_none,
	audio_codec_wm8903,
};

enum image_type {
	system_image = 0,
	rck_image,
};

void tegra_get_board_info(struct board_info *);
void tegra_get_pmu_board_info(struct board_info *bi);
void tegra_get_display_board_info(struct board_info *bi);
void tegra_get_camera_board_info(struct board_info *bi);
void tegra_get_io_board_info(struct board_info *bi);
void tegra_get_button_board_info(struct board_info *bi);
void tegra_get_joystick_board_info(struct board_info *bi);
void tegra_get_rightspeaker_board_info(struct board_info *bi);
void tegra_get_leftspeaker_board_info(struct board_info *bi);
int tegra_get_board_panel_id(void);

int get_core_edp(void);
enum panel_type get_panel_type(void);
int tegra_get_usb_port_owner_info(void);
int tegra_get_modem_id(void);
int tegra_get_commchip_id(void);
u8 get_power_config(void);
enum power_supply_type get_power_supply_type(void);
enum audio_codec_type get_audio_codec_type(void);
int get_maximum_cpu_current_supported(void);
int get_maximum_core_current_supported(void);
int get_emc_max_dvfs(void);
int tegra_get_memory_type(void);
void tegra_enable_pinmux(void);
enum image_type get_tegra_image_type(void);
int tegra_get_cvb_alignment_uV(void);
int tegra_soc_device_init(const char *machine);
int get_pwr_i2c_clk_rate(void);
int tegra_get_pmic_rst_reason(void);
#ifdef CONFIG_ANDROID
bool get_androidboot_mode_charger(void);
#endif
extern void tegra_set_usb_vbus_internal_wake(bool enable);
extern void tegra_set_usb_id_internal_wake(bool enable);
#endif
