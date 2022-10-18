/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012, 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_IO_UTIL_H__
#define __SDE_IO_UTIL_H__

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/soc/qcom/msm_mmrm.h>

#ifdef DEBUG
#define DEV_DBG(fmt, args...)   pr_err(fmt, ##args)
#else
#define DEV_DBG(fmt, args...)   pr_debug(fmt, ##args)
#endif
#define DEV_INFO(fmt, args...)  pr_info(fmt, ##args)
#define DEV_WARN(fmt, args...)  pr_warn(fmt, ##args)
#define DEV_ERR(fmt, args...)   pr_err(fmt, ##args)

struct dss_io_data {
	u32 len;
	void __iomem *base;
};

void dss_reg_w(struct dss_io_data *io, u32 offset, u32 value, u32 debug);
u32 dss_reg_r(struct dss_io_data *io, u32 offset, u32 debug);
void dss_reg_dump(void __iomem *base, u32 len, const char *prefix, u32 debug);

#define DSS_REG_W_ND(io, offset, val)  dss_reg_w(io, offset, val, false)
#define DSS_REG_W(io, offset, val)     dss_reg_w(io, offset, val, true)
#define DSS_REG_R_ND(io, offset)       dss_reg_r(io, offset, false)
#define DSS_REG_R(io, offset)          dss_reg_r(io, offset, true)

enum dss_vreg_type {
	DSS_REG_LDO,
	DSS_REG_VS,
};

struct dss_vreg {
	struct regulator *vreg; /* vreg handle */
	char vreg_name[32];
	int min_voltage;
	int max_voltage;
	int enable_load;
	int disable_load;
	int pre_on_sleep;
	int post_on_sleep;
	int pre_off_sleep;
	int post_off_sleep;
};

struct dss_gpio {
	unsigned int gpio;
	unsigned int value;
	char gpio_name[32];
};

enum dss_clk_type {
	DSS_CLK_AHB, /* no set rate. rate controlled through rpm */
	DSS_CLK_PCLK,
	DSS_CLK_MMRM, /* set rate called through mmrm driver */
	DSS_CLK_OTHER,
};

struct dss_clk_mmrm_cb {
	void *phandle;
	struct dss_clk *clk;
};

struct dss_clk_mmrm {
	unsigned int clk_id;
	unsigned int flags;
	struct mmrm_client *mmrm_client;
	struct dss_clk_mmrm_cb *mmrm_cb_data;
	unsigned long mmrm_requested_clk;
	wait_queue_head_t mmrm_cb_wq;
};

struct dss_clk {
	struct clk *clk; /* clk handle */
	char clk_name[32];
	enum dss_clk_type type;
	unsigned long rate;
	unsigned long max_rate;
	struct dss_clk_mmrm mmrm;
};

struct dss_module_power {
	unsigned int num_vreg;
	struct dss_vreg *vreg_config;
	unsigned int num_gpio;
	struct dss_gpio *gpio_config;
	unsigned int num_clk;
	struct dss_clk *clk_config;
};

int msm_dss_ioremap_byname(struct platform_device *pdev,
	struct dss_io_data *io_data, const char *name);
void msm_dss_iounmap(struct dss_io_data *io_data);
int msm_dss_get_io_mem(struct platform_device *pdev,
		       struct list_head *mem_list);
void msm_dss_clean_io_mem(struct list_head *mem_list);
int msm_dss_get_pmic_io_mem(struct platform_device *pdev,
		       struct list_head *mem_list);
int msm_dss_get_gpio_io_mem(const int gpio_pin, struct list_head *mem_list);
int msm_dss_get_io_irq(struct platform_device *pdev,
		       struct list_head *irq_list, u32 label);
void msm_dss_clean_io_irq(struct list_head *irq_list);
int msm_dss_enable_gpio(struct dss_gpio *in_gpio, int num_gpio, int enable);
int msm_dss_gpio_enable(struct dss_gpio *in_gpio, int num_gpio, int enable);

int msm_dss_get_vreg(struct device *dev, struct dss_vreg *in_vreg,
	int num_vreg, int enable);
int msm_dss_enable_vreg(struct dss_vreg *in_vreg, int num_vreg,	int enable);

int msm_dss_get_clk(struct device *dev, struct dss_clk *clk_arry, int num_clk);
int msm_dss_mmrm_register(struct device *dev, struct dss_module_power *mp,
	int (*cb_fnc)(struct mmrm_client_notifier_data *data), void *phandle,
	bool *mmrm_enable);
void msm_dss_mmrm_deregister(struct device *dev, struct dss_module_power *mp);
void msm_dss_put_clk(struct dss_clk *clk_arry, int num_clk);
int msm_dss_clk_set_rate(struct dss_clk *clk_arry, int num_clk);
int msm_dss_single_clk_set_rate(struct dss_clk *clk);
int msm_dss_enable_clk(struct dss_clk *clk_arry, int num_clk, int enable);

int sde_i2c_byte_read(struct i2c_client *client, uint8_t slave_addr,
		       uint8_t reg_offset, uint8_t *read_buf);
int sde_i2c_byte_write(struct i2c_client *client, uint8_t slave_addr,
			uint8_t reg_offset, uint8_t *value);

#endif /* __SDE_IO_UTIL_H__ */
