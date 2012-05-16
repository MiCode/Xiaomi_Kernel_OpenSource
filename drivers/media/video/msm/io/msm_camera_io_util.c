/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/gpiomux.h>

#define BUFF_SIZE_128 128

void msm_camera_io_w(u32 data, void __iomem *addr)
{
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	writel_relaxed((data), (addr));
}

void msm_camera_io_w_mb(u32 data, void __iomem *addr)
{
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	wmb();
	writel_relaxed((data), (addr));
	wmb();
}

u32 msm_camera_io_r(void __iomem *addr)
{
	uint32_t data = readl_relaxed(addr);
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	return data;
}

u32 msm_camera_io_r_mb(void __iomem *addr)
{
	uint32_t data;
	rmb();
	data = readl_relaxed(addr);
	rmb();
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	return data;
}

void msm_camera_io_memcpy_toio(void __iomem *dest_addr,
	void __iomem *src_addr, u32 len)
{
	int i;
	u32 *d = (u32 *) dest_addr;
	u32 *s = (u32 *) src_addr;

	for (i = 0; i < len; i++)
		writel_relaxed(*s++, d++);
}

void msm_camera_io_dump(void __iomem *addr, int size)
{
	char line_str[BUFF_SIZE_128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;
	CDBG("%s: %p %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			snprintf(p_str, 12, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = readl_relaxed(p++);
		snprintf(p_str, 12, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			CDBG("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CDBG("%s\n", line_str);
}

void msm_camera_io_memcpy(void __iomem *dest_addr,
	void __iomem *src_addr, u32 len)
{
	CDBG("%s: %p %p %d\n", __func__, dest_addr, src_addr, len);
	msm_camera_io_memcpy_toio(dest_addr, src_addr, len / 4);
	msm_camera_io_dump(dest_addr, len);
}

int msm_cam_clk_enable(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable)
{
	int i;
	int rc = 0;
	if (enable) {
		for (i = 0; i < num_clk; i++) {
			clk_ptr[i] = clk_get(dev, clk_info[i].clk_name);
			if (IS_ERR(clk_ptr[i])) {
				pr_err("%s get failed\n", clk_info[i].clk_name);
				rc = PTR_ERR(clk_ptr[i]);
				goto cam_clk_get_err;
			}
			if (clk_info[i].clk_rate >= 0) {
				rc = clk_set_rate(clk_ptr[i],
							clk_info[i].clk_rate);
				if (rc < 0) {
					pr_err("%s set failed\n",
						   clk_info[i].clk_name);
					goto cam_clk_set_err;
				}
			}
			rc = clk_prepare(clk_ptr[i]);
			if (rc < 0) {
				pr_err("%s prepare failed\n",
					   clk_info[i].clk_name);
				goto cam_clk_prepare_err;
			}

			rc = clk_enable(clk_ptr[i]);
			if (rc < 0) {
				pr_err("%s enable failed\n",
					   clk_info[i].clk_name);
				goto cam_clk_enable_err;
			}
		}
	} else {
		for (i = num_clk - 1; i >= 0; i--) {
			if (clk_ptr[i] != NULL) {
				clk_disable(clk_ptr[i]);
				clk_unprepare(clk_ptr[i]);
				clk_put(clk_ptr[i]);
			}
		}
	}
	return rc;


cam_clk_enable_err:
	clk_unprepare(clk_ptr[i]);
cam_clk_prepare_err:
cam_clk_set_err:
	clk_put(clk_ptr[i]);
cam_clk_get_err:
	for (i--; i >= 0; i--) {
		if (clk_ptr[i] != NULL) {
			clk_disable(clk_ptr[i]);
			clk_unprepare(clk_ptr[i]);
			clk_put(clk_ptr[i]);
		}
	}
	return rc;
}

int msm_camera_config_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, struct regulator **reg_ptr, int config)
{
	int i = 0;
	int rc = 0;
	struct camera_vreg_t *curr_vreg;
	if (config) {
		for (i = 0; i < num_vreg; i++) {
			curr_vreg = &cam_vreg[i];
			reg_ptr[i] = regulator_get(dev,
				curr_vreg->reg_name);
			if (IS_ERR(reg_ptr[i])) {
				pr_err("%s: %s get failed\n",
					 __func__,
					 curr_vreg->reg_name);
				reg_ptr[i] = NULL;
				goto vreg_get_fail;
			}
			if (curr_vreg->type == REG_LDO) {
				rc = regulator_set_voltage(
					reg_ptr[i],
					curr_vreg->min_voltage,
					curr_vreg->max_voltage);
				if (rc < 0) {
					pr_err("%s: %s set voltage failed\n",
						__func__,
						curr_vreg->reg_name);
					goto vreg_set_voltage_fail;
				}
				if (curr_vreg->op_mode >= 0) {
					rc = regulator_set_optimum_mode(
						reg_ptr[i],
						curr_vreg->op_mode);
					if (rc < 0) {
						pr_err(
						"%s: %s set optimum mode failed\n",
						__func__,
						curr_vreg->reg_name);
						goto vreg_set_opt_mode_fail;
					}
				}
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			curr_vreg = &cam_vreg[i];
			if (reg_ptr[i]) {
				if (curr_vreg->type == REG_LDO) {
					if (curr_vreg->op_mode >= 0) {
						regulator_set_optimum_mode(
							reg_ptr[i], 0);
					}
					regulator_set_voltage(
						reg_ptr[i], 0, curr_vreg->
						max_voltage);
				}
				regulator_put(reg_ptr[i]);
				reg_ptr[i] = NULL;
			}
		}
	}
	return 0;

vreg_unconfig:
if (curr_vreg->type == REG_LDO)
	regulator_set_optimum_mode(reg_ptr[i], 0);

vreg_set_opt_mode_fail:
if (curr_vreg->type == REG_LDO)
	regulator_set_voltage(reg_ptr[i], 0,
		curr_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(reg_ptr[i]);
	reg_ptr[i] = NULL;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		curr_vreg = &cam_vreg[i];
		goto vreg_unconfig;
	}
	return -ENODEV;
}

int msm_camera_enable_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, struct regulator **reg_ptr, int enable)
{
	int i = 0, rc = 0;
	if (enable) {
		for (i = 0; i < num_vreg; i++) {
			if (IS_ERR(reg_ptr[i])) {
				pr_err("%s: %s null regulator\n",
					__func__, cam_vreg[i].reg_name);
				goto disable_vreg;
			}
			rc = regulator_enable(reg_ptr[i]);
			if (rc < 0) {
				pr_err("%s: %s enable failed\n",
					__func__, cam_vreg[i].reg_name);
				goto disable_vreg;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--)
			regulator_disable(reg_ptr[i]);
	}
	return rc;
disable_vreg:
	for (i--; i >= 0; i--) {
		regulator_disable(reg_ptr[i]);
		goto disable_vreg;
	}
	return rc;
}

static int config_gpio_table(struct msm_camera_gpio_conf *gpio)
{
	int rc = 0, i = 0;
	uint32_t *table_on;
	uint32_t *table_off;
	uint32_t len;

	table_on = gpio->camera_on_table;
	table_off = gpio->camera_off_table;
	len = gpio->camera_on_table_size;

	for (i = 0; i < len; i++) {
		rc = gpio_tlmm_config(table_on[i], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("%s not able to get gpio\n", __func__);
			for (i--; i >= 0; i--)
				gpio_tlmm_config(table_off[i],
					GPIO_CFG_ENABLE);
			break;
		}
	}
	return rc;
}

int msm_camera_request_gpio_table(struct msm_camera_sensor_info *sinfo,
	int gpio_en)
{
	int rc = 0;
	struct msm_camera_gpio_conf *gpio_conf =
		sinfo->sensor_platform_info->gpio_conf;

	if (!gpio_conf->gpio_no_mux) {
		if (gpio_conf->cam_gpio_req_tbl == NULL ||
			gpio_conf->cam_gpio_common_tbl == NULL) {
			pr_err("%s: NULL camera gpio table\n", __func__);
			return -EFAULT;
		}
	}
	if (gpio_conf->gpio_no_mux)
		config_gpio_table(gpio_conf);

	if (gpio_en) {
		if (!gpio_conf->gpio_no_mux) {
			if (gpio_conf->cam_gpiomux_conf_tbl != NULL) {
				msm_gpiomux_install(
					(struct msm_gpiomux_config *)
					gpio_conf->cam_gpiomux_conf_tbl,
					gpio_conf->cam_gpiomux_conf_tbl_size);
			}
			rc = gpio_request_array(gpio_conf->cam_gpio_common_tbl,
				gpio_conf->cam_gpio_common_tbl_size);
			if (rc < 0) {
				pr_err("%s common gpio request failed\n"
						, __func__);
				return rc;
			}
		}
		if (gpio_conf->cam_gpio_req_tbl_size) {
			rc = gpio_request_array(gpio_conf->cam_gpio_req_tbl,
				gpio_conf->cam_gpio_req_tbl_size);
			if (rc < 0) {
				pr_err("%s camera gpio"
					"request failed\n", __func__);
				gpio_free_array(gpio_conf->cam_gpio_common_tbl,
					gpio_conf->cam_gpio_common_tbl_size);
				return rc;
			}
		}
	} else {
		gpio_free_array(gpio_conf->cam_gpio_req_tbl,
				gpio_conf->cam_gpio_req_tbl_size);
		if (!gpio_conf->gpio_no_mux)
			gpio_free_array(gpio_conf->cam_gpio_common_tbl,
				gpio_conf->cam_gpio_common_tbl_size);
	}
	return rc;
}

int msm_camera_config_gpio_table(struct msm_camera_sensor_info *sinfo,
	int gpio_en)
{
	struct msm_camera_gpio_conf *gpio_conf =
		sinfo->sensor_platform_info->gpio_conf;
	int rc = 0, i;

	if (gpio_en) {
		for (i = 0; i < gpio_conf->cam_gpio_set_tbl_size; i++) {
			gpio_set_value_cansleep(
				gpio_conf->cam_gpio_set_tbl[i].gpio,
				gpio_conf->cam_gpio_set_tbl[i].flags);
			usleep_range(gpio_conf->cam_gpio_set_tbl[i].delay,
				gpio_conf->cam_gpio_set_tbl[i].delay + 1000);
		}
	} else {
		for (i = gpio_conf->cam_gpio_set_tbl_size - 1; i >= 0; i--) {
			if (gpio_conf->cam_gpio_set_tbl[i].flags)
				gpio_set_value_cansleep(
					gpio_conf->cam_gpio_set_tbl[i].gpio,
					GPIOF_OUT_INIT_LOW);
		}
	}
	return rc;
}
