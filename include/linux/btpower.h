/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_BLUETOOTH_POWER_H
#define __LINUX_BLUETOOTH_POWER_H

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>

/*
 * voltage regulator information required for configuring the
 * bluetooth chipset
 */
enum bt_power_modes {
	BT_POWER_DISABLE = 0,
	BT_POWER_ENABLE,
	BT_POWER_RETENTION
};

struct log_index {
	int init;
	int crash;
};

struct bt_power_vreg_data {
	struct regulator *reg;  /* voltage regulator handle */
	const char *name;       /* regulator name */
	u32 min_vol;            /* min voltage level */
	u32 max_vol;            /* max voltage level */
	u32 load_curr;          /* current */
	bool is_enabled;        /* is this regulator enabled? */
	bool is_retention_supp; /* does this regulator support retention mode */
	struct log_index indx;  /* Index for reg. w.r.t init & crash */
};

struct bt_power {
	char compatible[32];
	struct bt_power_vreg_data *vregs;
	int num_vregs;
};

struct bt_power_clk_data {
	struct clk *clk;  /* clock regulator handle */
	const char *name; /* clock name */
	bool is_enabled;  /* is this clock enabled? */
};

/*
 * Platform data for the bluetooth power driver.
 */
struct btpower_platform_data {
	struct platform_device *pdev;
	int bt_gpio_sys_rst;                   /* Bluetooth reset gpio */
	int wl_gpio_sys_rst;                   /* Wlan reset gpio */
	int bt_gpio_sw_ctrl;                   /* Bluetooth sw_ctrl gpio */
	int bt_gpio_debug;                     /* Bluetooth debug gpio */
#ifdef CONFIG_MSM_BT_OOBS
	int bt_gpio_dev_wake;                  /* Bluetooth bt_wake */
	int bt_gpio_host_wake;                 /* Bluetooth bt_host_wake */
	int irq;                               /* Bluetooth host_wake IRQ */
#endif
	int xo_gpio_clk;                       /* XO clock gpio*/
	struct device *slim_dev;
	struct bt_power_vreg_data *vreg_info;  /* VDDIO voltage regulator */
	struct bt_power_clk_data *bt_chip_clk; /* bluetooth reference clock */
	int (*bt_power_setup)(int id); /* Bluetooth power setup function */
	char compatible[32]; /*Bluetooth SoC name */
	int num_vregs;
	struct mbox_client mbox_client_data;
	struct mbox_chan *mbox_chan;
	const char *vreg_ipa;
	const char *tsens;
#ifdef CONFIG_MSM_BT_OOBS
	struct file *reffilp_obs;
	struct task_struct *reftask_obs;
#endif
};

int btpower_register_slimdev(struct device *dev);
int btpower_get_chipset_version(void);
int btpower_aop_mbox_init(struct btpower_platform_data *pdata);

#define BT_CMD_SLIM_TEST		0xbfac
#define BT_CMD_PWR_CTRL			0xbfad
#define BT_CMD_CHIPSET_VERS		0xbfae
#define BT_CMD_GET_CHIPSET_ID	0xbfaf
#define BT_CMD_CHECK_SW_CTRL	0xbfb0
#define BT_CMD_GETVAL_POWER_SRCS	0xbfb1
#define BT_CMD_SET_IPA_TCS_INFO  0xbfc0

#ifdef CONFIG_MSM_BT_OOBS
#define BT_CMD_OBS_SIGNAL_TASK		0xbfd0
#define BT_CMD_OBS_VOTE_CLOCK		0xbfd1

/**
 * enum btpower_obs_param: OOBS low power param
 * @BTPOWER_OBS_CLK_OFF: Transport bus is no longer acquired
 * @BTPOWER_OBS_CLK_ON: Acquire transport bus for either transmitting or receiving
 * @BTPOWER_OBS_DEV_OFF: Bluetooth is released because of no more transmission
 * @BTPOWER_OBS_DEV_ON: Wake up the Bluetooth controller for transmission
 */
enum btpower_obs_param {
	BTPOWER_OBS_CLK_OFF = 0,
	BTPOWER_OBS_CLK_ON,
	BTPOWER_OBS_DEV_OFF,
	BTPOWER_OBS_DEV_ON,
};
#endif

#endif /* __LINUX_BLUETOOTH_POWER_H */
