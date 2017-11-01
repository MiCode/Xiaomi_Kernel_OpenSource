/* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ESOC_MDM_H__
#define __ESOC_MDM_H__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include "esoc.h"

#define MDM_PBLRDY_CNT			20
#define INVALID_GPIO			(-1)
#define MDM_GPIO(mdm, i)		(mdm->gpios[i])
#define MDM9x25_LABEL			"MDM9x25"
#define MDM9x25_HSIC			"HSIC"
#define MDM9x35_LABEL			"MDM9x35"
#define MDM9x35_PCIE			"PCIe"
#define MDM9x35_DUAL_LINK		"HSIC+PCIe"
#define MDM9x35_HSIC			"HSIC"
#define MDM9x55_LABEL			"MDM9x55"
#define MDM9x55_PCIE			"PCIe"
#define MDM2AP_STATUS_TIMEOUT_MS	120000L
#define MDM_MODEM_TIMEOUT		3000
#define DEF_RAMDUMP_TIMEOUT		120000
#define DEF_RAMDUMP_DELAY		2000
#define RD_BUF_SIZE			100
#define SFR_MAX_RETRIES			10
#define SFR_RETRY_INTERVAL		1000
#define MDM_DBG_OFFSET			0x934
#define MDM_DBG_MODE			0x53444247
#define MDM_CTI_NAME			"coresight-cti-rpm-cpu0"
#define MDM_CTI_TRIG			0
#define MDM_CTI_CH			0

enum mdm_gpio {
	AP2MDM_WAKEUP = 0,
	AP2MDM_STATUS,
	AP2MDM_SOFT_RESET,
	AP2MDM_VDD_MIN,
	AP2MDM_CHNLRDY,
	AP2MDM_ERRFATAL,
	AP2MDM_VDDMIN,
	AP2MDM_PMIC_PWR_EN,
	MDM2AP_WAKEUP,
	MDM2AP_ERRFATAL,
	MDM2AP_PBLRDY,
	MDM2AP_STATUS,
	MDM2AP_VDDMIN,
	MDM_LINK_DETECT,
	NUM_GPIOS,
};

struct mdm_pon_ops;

struct mdm_ctrl {
	unsigned int gpios[NUM_GPIOS];
	spinlock_t status_lock;
	struct workqueue_struct *mdm_queue;
	struct delayed_work mdm2ap_status_check_work;
	struct work_struct mdm_status_work;
	struct work_struct restart_reason_work;
	struct completion debug_done;
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_booting;
	struct pinctrl_state *gpio_state_running;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	int mdm2ap_status_valid_old_config;
	int soft_reset_inverted;
	int errfatal_irq;
	int status_irq;
	int pblrdy_irq;
	int debug;
	int init;
	bool debug_fail;
	unsigned int dump_timeout_ms;
	unsigned int ramdump_delay_ms;
	struct esoc_clink *esoc;
	bool get_restart_reason;
	unsigned long irq_mask;
	bool ready;
	bool dual_interface;
	u32 status;
	void __iomem *dbg_addr;
	bool dbg_mode;
	struct coresight_cti *cti;
	int trig_cnt;
	const struct mdm_pon_ops *pon_ops;
};

struct mdm_pon_ops {
	int (*pon)(struct mdm_ctrl *mdm);
	int (*soft_reset)(struct mdm_ctrl *mdm, bool atomic);
	int (*poff_force)(struct mdm_ctrl *mdm);
	int (*poff_cleanup)(struct mdm_ctrl *mdm);
	void (*cold_reset)(struct mdm_ctrl *mdm);
	int (*dt_init)(struct mdm_ctrl *mdm);
	int (*setup)(struct mdm_ctrl *mdm);
};

struct mdm_ops {
	struct esoc_clink_ops *clink_ops;
	struct mdm_pon_ops *pon_ops;
	int (*config_hw)(struct mdm_ctrl *mdm, const struct mdm_ops *ops,
					struct platform_device *pdev);
};

static inline int mdm_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	return mdm->pon_ops->soft_reset(mdm, atomic);
}
static inline int mdm_do_first_power_on(struct mdm_ctrl *mdm)
{
	return mdm->pon_ops->pon(mdm);
}
static inline int mdm_power_down(struct mdm_ctrl *mdm)
{
	return mdm->pon_ops->poff_force(mdm);
}
static inline void mdm_cold_reset(struct mdm_ctrl *mdm)
{
	mdm->pon_ops->cold_reset(mdm);
}
static inline int mdm_pon_dt_init(struct mdm_ctrl *mdm)
{
	return mdm->pon_ops->dt_init(mdm);
}
static inline int mdm_pon_setup(struct mdm_ctrl *mdm)
{
	return mdm->pon_ops->setup(mdm);
}

extern struct mdm_pon_ops mdm9x25_pon_ops;
extern struct mdm_pon_ops mdm9x35_pon_ops;
extern struct mdm_pon_ops mdm9x55_pon_ops;
#endif
