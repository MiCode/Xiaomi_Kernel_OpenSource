/* Copyright (c) 2011-2014, The Linux Foundataion. All rights reserved.
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
#include <linux/err.h>
#include <soc/qcom/camera2.h>
#include <mach/gpiomux.h>
#include <linux/msm-bus.h>
#include "msm_camera_io_util.h"

#define BUFF_SIZE_128 128

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

void msm_camera_io_w(u32 data, void __iomem *addr)
{
	CDBG("%s: 0x%p %08x\n", __func__,  (addr), (data));
	writel_relaxed((data), (addr));
}

void msm_camera_io_w_mb(u32 data, void __iomem *addr)
{
	CDBG("%s: 0x%p %08x\n", __func__,  (addr), (data));
	wmb();
	writel_relaxed((data), (addr));
	wmb();
}

u32 msm_camera_io_r(void __iomem *addr)
{
	uint32_t data = readl_relaxed(addr);
	CDBG("%s: 0x%p %08x\n", __func__,  (addr), (data));
	return data;
}

u32 msm_camera_io_r_mb(void __iomem *addr)
{
	uint32_t data;
	rmb();
	data = readl_relaxed(addr);
	rmb();
	CDBG("%s: 0x%p %08x\n", __func__,  (addr), (data));
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
			snprintf(p_str, 12, "0x%p: ",  p);
			p_str += 10;
		}
		data = readl_relaxed(p++);
		snprintf(p_str, 12, "%d ", data);
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

int msm_cam_clk_sel_src(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct msm_cam_clk_info *clk_src_info, int num_clk)
{
	int i;
	int rc = 0;
	struct clk *mux_clk = NULL;
	struct clk *src_clk = NULL;

	for (i = 0; i < num_clk; i++) {
		if (clk_src_info[i].clk_name) {
			mux_clk = clk_get(dev, clk_info[i].clk_name);
			if (IS_ERR(mux_clk)) {
				pr_err("%s get failed\n",
					 clk_info[i].clk_name);
				continue;
			}
			src_clk = clk_get(dev, clk_src_info[i].clk_name);
			if (IS_ERR(src_clk)) {
				pr_err("%s get failed\n",
					clk_src_info[i].clk_name);
				continue;
			}
			clk_set_parent(mux_clk, src_clk);
		}
	}
	return rc;
}

int msm_cam_clk_enable(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable)
{
	int i;
	int rc = 0;
	long clk_rate;
	if (enable) {
		for (i = 0; i < num_clk; i++) {
			CDBG("%s enable %s\n", __func__,
				clk_info[i].clk_name);
			clk_ptr[i] = clk_get(dev, clk_info[i].clk_name);
			if (IS_ERR(clk_ptr[i])) {
				pr_err("%s get failed\n", clk_info[i].clk_name);
				rc = PTR_ERR(clk_ptr[i]);
				goto cam_clk_get_err;
			}
			if (clk_info[i].clk_rate > 0) {
				rc = clk_set_rate(clk_ptr[i],
							clk_info[i].clk_rate);
				if (rc < 0) {
					pr_err("%s set failed\n",
						   clk_info[i].clk_name);
					goto cam_clk_set_err;
				}
			} else if (clk_info[i].clk_rate == INIT_RATE) {
				clk_rate = clk_get_rate(clk_ptr[i]);
				if (clk_rate == 0) {
					clk_rate =
						  clk_round_rate(clk_ptr[i], 0);
					if (clk_rate < 0) {
						pr_err("%s round rate failed\n",
							  clk_info[i].clk_name);
						goto cam_clk_set_err;
					}
					rc = clk_set_rate(clk_ptr[i],
								clk_rate);
					if (rc < 0) {
						pr_err("%s set rate failed\n",
							  clk_info[i].clk_name);
						goto cam_clk_set_err;
					}
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
			if (clk_info[i].delay > 20) {
				msleep(clk_info[i].delay);
			} else if (clk_info[i].delay) {
				usleep_range(clk_info[i].delay * 1000,
					(clk_info[i].delay * 1000) + 1000);
			}
		}
	} else {
		for (i = num_clk - 1; i >= 0; i--) {
			if (clk_ptr[i] != NULL) {
				CDBG("%s disable %s\n", __func__,
					clk_info[i].clk_name);
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
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int config)
{
	int i = 0, j = 0;
	int rc = 0;
	struct camera_vreg_t *curr_vreg;

	if (num_vreg_seq > num_vreg) {
		pr_err("%s:%d vreg sequence invalid\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (!num_vreg_seq)
		num_vreg_seq = num_vreg;

	if (config) {
		for (i = 0; i < num_vreg_seq; i++) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			curr_vreg = &cam_vreg[j];
			reg_ptr[j] = regulator_get(dev,
				curr_vreg->reg_name);
			if (IS_ERR(reg_ptr[j])) {
				pr_err("%s: %s get failed\n",
					 __func__,
					 curr_vreg->reg_name);
				reg_ptr[j] = NULL;
				goto vreg_get_fail;
			}
			if (curr_vreg->type == REG_LDO) {
				rc = regulator_set_voltage(
					reg_ptr[j],
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
						reg_ptr[j],
						curr_vreg->op_mode);
					if (rc < 0) {
						pr_err(
						"%s:%s set optimum mode fail\n",
						__func__,
						curr_vreg->reg_name);
						goto vreg_set_opt_mode_fail;
					}
				}
			}
		}
	} else {
		for (i = num_vreg_seq-1; i >= 0; i--) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			curr_vreg = &cam_vreg[j];
			if (reg_ptr[j]) {
				if (curr_vreg->type == REG_LDO) {
					if (curr_vreg->op_mode >= 0) {
						regulator_set_optimum_mode(
							reg_ptr[j], 0);
					}
					regulator_set_voltage(
						reg_ptr[j], 0, curr_vreg->
						max_voltage);
				}
				regulator_put(reg_ptr[j]);
				reg_ptr[j] = NULL;
			}
		}
	}
	return 0;

vreg_unconfig:
if (curr_vreg->type == REG_LDO)
	regulator_set_optimum_mode(reg_ptr[j], 0);

vreg_set_opt_mode_fail:
if (curr_vreg->type == REG_LDO)
	regulator_set_voltage(reg_ptr[j], 0,
		curr_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(reg_ptr[j]);
	reg_ptr[j] = NULL;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		if (vreg_seq) {
			j = vreg_seq[i];
			if (j >= num_vreg)
				continue;
		} else
			j = i;
		curr_vreg = &cam_vreg[j];
		goto vreg_unconfig;
	}
	return -ENODEV;
}

int msm_camera_enable_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int enable)
{
	int i = 0, j = 0, rc = 0;

	if (num_vreg_seq > num_vreg) {
		pr_err("%s:%d vreg sequence invalid\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (!num_vreg_seq)
		num_vreg_seq = num_vreg;

	if (enable) {
		for (i = 0; i < num_vreg_seq; i++) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			if (IS_ERR(reg_ptr[j])) {
				pr_err("%s: %s null regulator\n",
					__func__, cam_vreg[j].reg_name);
				goto disable_vreg;
			}
			rc = regulator_enable(reg_ptr[j]);
			if (rc < 0) {
				pr_err("%s: %s enable failed\n",
					__func__, cam_vreg[j].reg_name);
				goto disable_vreg;
			}
			if (cam_vreg[j].delay > 20)
				msleep(cam_vreg[j].delay);
			else if (cam_vreg[j].delay)
				usleep_range(cam_vreg[j].delay * 1000,
					(cam_vreg[j].delay * 1000) + 1000);
		}
	} else {
		for (i = num_vreg_seq-1; i >= 0; i--) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			regulator_disable(reg_ptr[j]);
			if (cam_vreg[j].delay > 20)
				msleep(cam_vreg[j].delay);
			else if (cam_vreg[j].delay)
				usleep_range(cam_vreg[j].delay * 1000,
					(cam_vreg[j].delay * 1000) + 1000);
		}
	}
	return rc;
disable_vreg:
	for (i--; i >= 0; i--) {
		if (vreg_seq) {
			j = vreg_seq[i];
			if (j >= num_vreg)
				continue;
		} else
			j = i;
		regulator_disable(reg_ptr[j]);
		if (cam_vreg[j].delay > 20)
			msleep(cam_vreg[j].delay);
		else if (cam_vreg[j].delay)
			usleep_range(cam_vreg[j].delay * 1000,
				(cam_vreg[j].delay * 1000) + 1000);
	}
	return rc;
}

void msm_camera_bus_scale_cfg(uint32_t bus_perf_client,
		enum msm_bus_perf_setting perf_setting)
{
	int rc = 0;
	if (!bus_perf_client) {
		pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		return;
	}

	switch (perf_setting) {
	case S_EXIT:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 1);
		msm_bus_scale_unregister_client(bus_perf_client);
		break;
	case S_PREVIEW:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 1);
		break;
	case S_VIDEO:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 2);
		break;
	case S_CAPTURE:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 3);
		break;
	case S_ZSL:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 4);
		break;
	case S_LIVESHOT:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 5);
		break;
	case S_DEFAULT:
		break;
	default:
		pr_warning("%s: INVALID CASE\n", __func__);
	}
}

int msm_camera_set_gpio_table(struct msm_gpio_set_tbl *gpio_tbl,
	uint8_t gpio_tbl_size, int gpio_en)
{
	int rc = 0, i;

	if (gpio_en) {
		for (i = 0; i < gpio_tbl_size; i++) {
			gpio_set_value_cansleep(gpio_tbl[i].gpio,
				gpio_tbl[i].flags);
			usleep_range(gpio_tbl[i].delay,
				gpio_tbl[i].delay + 1000);
		}
	} else {
		for (i = gpio_tbl_size - 1; i >= 0; i--) {
			if (gpio_tbl[i].flags)
				gpio_set_value_cansleep(gpio_tbl[i].gpio,
					GPIOF_OUT_INIT_LOW);
		}
	}
	return rc;
}

int msm_camera_config_single_vreg(struct device *dev,
	struct camera_vreg_t *cam_vreg, struct regulator **reg_ptr, int config)
{
	int rc = 0;
	if (config) {
		if (!dev || !cam_vreg || !reg_ptr) {
			pr_err("%s: get failed NULL parameter\n", __func__);
			goto vreg_get_fail;
		}
		CDBG("%s enable %s\n", __func__, cam_vreg->reg_name);
		*reg_ptr = regulator_get(dev, cam_vreg->reg_name);
		if (IS_ERR_OR_NULL(*reg_ptr)) {
			pr_err("%s: %s get failed\n", __func__,
				cam_vreg->reg_name);
			*reg_ptr = NULL;
			goto vreg_get_fail;
		}
		if (cam_vreg->type == REG_LDO) {
			rc = regulator_set_voltage(
				*reg_ptr, cam_vreg->min_voltage,
				cam_vreg->max_voltage);
			if (rc < 0) {
				pr_err("%s: %s set voltage failed\n",
					__func__, cam_vreg->reg_name);
				goto vreg_set_voltage_fail;
			}
			if (cam_vreg->op_mode >= 0) {
				rc = regulator_set_optimum_mode(*reg_ptr,
					cam_vreg->op_mode);
				if (rc < 0) {
					pr_err(
					"%s: %s set optimum mode failed\n",
					__func__, cam_vreg->reg_name);
					goto vreg_set_opt_mode_fail;
				}
			}
		}
		rc = regulator_enable(*reg_ptr);
		if (rc < 0) {
			pr_err("%s: %s enable failed\n",
				__func__, cam_vreg->reg_name);
			goto vreg_unconfig;
		}
	} else {
		if (*reg_ptr) {
			CDBG("%s disable %s\n", __func__, cam_vreg->reg_name);
			regulator_disable(*reg_ptr);
			if (cam_vreg->type == REG_LDO) {
				if (cam_vreg->op_mode >= 0)
					regulator_set_optimum_mode(*reg_ptr, 0);
				regulator_set_voltage(
					*reg_ptr, 0, cam_vreg->max_voltage);
			}
			regulator_put(*reg_ptr);
			*reg_ptr = NULL;
		}
	}
	return 0;

vreg_unconfig:
if (cam_vreg->type == REG_LDO)
	regulator_set_optimum_mode(*reg_ptr, 0);

vreg_set_opt_mode_fail:
if (cam_vreg->type == REG_LDO)
	regulator_set_voltage(*reg_ptr, 0, cam_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(*reg_ptr);
	*reg_ptr = NULL;

vreg_get_fail:
	return -ENODEV;
}

int msm_camera_request_gpio_table(struct gpio *gpio_tbl, uint8_t size,
	int gpio_en)
{
	int rc = 0, i = 0, err = 0;

	if (!gpio_tbl || !size) {
		pr_err("%s:%d invalid gpio_tbl %p / size %d\n", __func__,
			__LINE__, gpio_tbl, size);
		return -EINVAL;
	}
	for (i = 0; i < size; i++) {
		CDBG("%s:%d i %d, gpio %d dir %ld\n", __func__, __LINE__, i,
			gpio_tbl[i].gpio, gpio_tbl[i].flags);
	}
	if (gpio_en) {
		for (i = 0; i < size; i++) {
			err = gpio_request_one(gpio_tbl[i].gpio,
				gpio_tbl[i].flags, gpio_tbl[i].label);
			if (err) {
				/*
				* After GPIO request fails, contine to
				* apply new gpios, outout a error message
				* for driver bringup debug
				*/
				pr_err("%s:%d gpio %d:%s request fails\n",
					__func__, __LINE__,
					gpio_tbl[i].gpio, gpio_tbl[i].label);
			}
		}
	} else {
		gpio_free_array(gpio_tbl, size);
	}
	return rc;
}
