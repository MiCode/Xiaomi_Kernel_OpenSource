/*
 * mdm_ctrl_board.h
 *
 * Header for the Modem control driver.
 *
 * Copyright (C) 2010, 2011 Intel Corporation. All rights reserved.
 *
 * Contact: Frederic BERAT <fredericx.berat@intel.com>
 *          Faouaz TENOUTIT <faouazx.tenoutit@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __MDM_CTRL_BOARD_H__
#define __MDM_CTRL_BOARD_H__

#include <asm/intel-mid.h>
#include <linux/module.h>

#define DEVICE_NAME "modem_control"
#define DRVNAME "mdm_ctrl"

/* Supported PMIC IDs*/
enum {
	PMIC_UNSUP,
	PMIC_MFLD,
	PMIC_CLVT,
	PMIC_MRFL,
	PMIC_BYT,
	PMIC_MOOR,
	PMIC_CHT
};

/* Supported CPU IDs*/
enum {
	CPU_UNSUP,
	CPU_PWELL,
	CPU_CLVIEW,
	CPU_TANGIER,
	CPU_VVIEW2,
	CPU_ANNIEDALE,
	CPU_CHERRYVIEW
};

struct mdm_ops {
	int	(*init)(void *data);
	int	(*cleanup)(void *data);
	int	(*get_cflash_delay)(void *data);
	int	(*get_wflash_delay)(void *data);
	int	(*power_on)(void *data, int gpio_rst, int gpio_pwr);
	int	(*power_off)(void *data, int gpio_rst);
	int	(*warm_reset)(void *data, int gpio_rst);
};

struct cpu_ops {
	int	(*init)(void *data);
	int	(*cleanup)(void *data);
	int	(*get_mdm_state)(void *data);
	int	(*get_irq_cdump)(void *data);
	int	(*get_irq_rst)(void *data);
	int	(*get_gpio_rst)(void *data);
	int	(*get_gpio_pwr)(void *data);
};

struct pmic_ops {
	int	(*init)(void *data);
	int	(*cleanup)(void *data);
	int	(*power_on_mdm)(void *data);
	int	(*power_off_mdm)(void *data);
	int	(*get_early_pwr_on)(void *data);
	int	(*get_early_pwr_off)(void *data);
};

struct mcd_base_info {
	/* modem infos */
	int		mdm_ver;
	struct	mdm_ops mdm;
	void	*modem_data;

	/* cpu infos */
	int		cpu_ver;
	struct	cpu_ops cpu;
	void	*cpu_data;

	/* pmic infos */
	int		pmic_ver;
	struct	pmic_ops pmic;
	void	*pmic_data;

	/* board type */
	int		board_type;
};

/* GPIO names */
#define GPIO_RST_OUT	"ifx_mdm_rst_out"
#define GPIO_PWR_ON	"ifx_mdm_pwr_on"
#define GPIO_PWR_ON_2	"xenon_trigger"
#define GPIO_RST_BBN	"ifx_mdm_rst_pmu"
#define GPIO_CDUMP	"modem-gpio2"
#define GPIO_CDUMP_MRFL	"MODEM_CORE_DUMP"

/* Retrieve modem parameters on ACPI framework */
int retrieve_modem_platform_data(struct platform_device *pdev);
int get_nb_mdms(void);

int mcd_register_mdm_info(struct mcd_base_info *info,
			  struct platform_device *pdev);

void mcd_set_mdm(struct mcd_base_info *info, int mdm_ver);
int mcd_finalize_cpu_data(struct mcd_base_info *mcd_reg_info);

/* struct mcd_cpu_data
 * @gpio_rst_out: Reset out gpio (self reset indicator)
 * @gpio_pwr_on: Power on gpio (ON1 - Power up pin)
 * @gpio_rst_bbn: RST_BB_N gpio (Reset pin)
 * @gpio_cdump: CORE DUMP indicator
 * @irq_cdump: CORE DUMP irq
 * @irq_reset: RST_BB_N irq
 * @gpio_rst_usbhub: USB HUB reset
 */
struct mdm_ctrl_cpu_data {
	struct gpio_desc	*entries[5];

	/* GPIOs */
	char	*gpio_rst_out_name;
	struct gpio_desc	*gpio_rst_out;
	char	*gpio_pwr_on_name;
	struct gpio_desc	*gpio_pwr_on;
	char	*gpio_rst_bbn_name;
	struct gpio_desc	*gpio_rst_bbn;
	char	*gpio_cdump_name;
	struct gpio_desc	*gpio_cdump;
	char	*gpio_wwan_disable_name;
	char	*gpio_wake_on_wwan_name;

	/* NGFF specific */
	struct gpio_desc	*gpio_wwan_disable;
	struct gpio_desc	*gpio_rst_usbhub;

	/* IRQs */
	int	irq_cdump;
	int	irq_reset;
	int	irq_wake_on_wwan;
};

/* struct mdm_ctrl_pmic_data
 * @chipctrl: PMIC base address
 * @chipctrlon: Modem power on PMIC value
 * @chipctrloff: Modem power off PMIC value
 * @early_pwr_on: call to power_on on probe indicator
 * @early_pwr_off: call to power_off on probe indicator
 * @pwr_down_duration:Powering down duration (us)
 */
struct mdm_ctrl_pmic_data {
	int		chipctrl;
	int		chipctrlon;
	int		chipctrloff;
	int		chipctrl_mask;
	bool	early_pwr_on;
	bool	early_pwr_off;
	int		pwr_down_duration;
};

/* struct mdm_ctrl_device_info - Board and modem infos
 *
 * @pre_on_delay:Delay before pulse on ON1 (us)
 * @on_duration:Pulse on ON1 duration (us)
 * @pre_wflash_delay:Delay before flashing window, after warm_reset (ms)
 * @pre_cflash_delay:Delay before flashing window, after cold_reset (ms)
 * @flash_duration:Flashing window durtion (ms)int  Not used ?
 * @warm_rst_duration:Warm reset duration (ms)
 */
struct mdm_ctrl_mdm_data {
	int	pre_on_delay;
	int	on_duration;
	int	pre_wflash_delay;
	int	pre_cflash_delay;
	int	flash_duration;
	int	warm_rst_duration;
	int	pre_pwr_down_delay;
};
#endif				/* __MDM_CTRL_BOARD_H__ */
