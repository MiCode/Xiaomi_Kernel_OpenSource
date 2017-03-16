/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/msm-bus.h>
#include "cam_sensor_soc_api.h"

#define NO_SET_RATE -1
#define INIT_RATE -2

#ifdef CONFIG_CAM_SOC_API_DBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

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
			CDBG("%s enable %s\n", __func__, clk_info[i].clk_name);
			clk_ptr[i] = clk_get(dev, clk_info[i].clk_name);
			if (IS_ERR(clk_ptr[i])) {
				pr_err("%s get failed\n", clk_info[i].clk_name);
				rc = PTR_ERR(clk_ptr[i]);
				goto cam_clk_get_err;
			}
			if (clk_info[i].clk_rate > 0) {
				clk_rate = clk_round_rate(clk_ptr[i],
					clk_info[i].clk_rate);
				if (clk_rate < 0) {
					pr_err("%s round failed\n",
						   clk_info[i].clk_name);
					goto cam_clk_set_err;
				}
				rc = clk_set_rate(clk_ptr[i],
					clk_rate);
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
			if (clk_info[i].delay > 20)
				msleep(clk_info[i].delay);
			else if (clk_info[i].delay)
				usleep_range(clk_info[i].delay * 1000,
					(clk_info[i].delay * 1000) + 1000);
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
			} else {
				j = i;
			}
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
			if (regulator_count_voltages(reg_ptr[j]) > 0) {
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
					rc = regulator_set_load(
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
			} else {
				j = i;
			}
			curr_vreg = &cam_vreg[j];
			if (reg_ptr[j]) {
				if (regulator_count_voltages(reg_ptr[j]) > 0) {
					if (curr_vreg->op_mode >= 0) {
						regulator_set_load(
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
	if (regulator_count_voltages(reg_ptr[j]) > 0)
		regulator_set_load(reg_ptr[j], 0);

vreg_set_opt_mode_fail:
	if (regulator_count_voltages(reg_ptr[j]) > 0)
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
		} else {
			j = i;
		}
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
	const char *vreg_name = NULL;

	if (!dev || !cam_vreg || !reg_ptr) {
		pr_err("%s: get failed NULL parameter\n", __func__);
		goto vreg_get_fail;
	}
	if (cam_vreg->type == VREG_TYPE_CUSTOM) {
		if (cam_vreg->custom_vreg_name == NULL) {
			pr_err("%s : can't find sub reg name",
				__func__);
			goto vreg_get_fail;
		}
		vreg_name = cam_vreg->custom_vreg_name;
	} else {
		if (cam_vreg->reg_name == NULL) {
			pr_err("%s : can't find reg name", __func__);
			goto vreg_get_fail;
		}
		vreg_name = cam_vreg->reg_name;
	}

	if (config) {
		CDBG("%s enable %s\n", __func__, vreg_name);
		*reg_ptr = regulator_get(dev, vreg_name);
		if (IS_ERR(*reg_ptr)) {
			pr_err("%s: %s get failed\n", __func__, vreg_name);
			*reg_ptr = NULL;
			goto vreg_get_fail;
		}
		if (regulator_count_voltages(*reg_ptr) > 0) {
			CDBG("%s: voltage min=%d, max=%d\n",
				__func__, cam_vreg->min_voltage,
				cam_vreg->max_voltage);
			rc = regulator_set_voltage(
				*reg_ptr, cam_vreg->min_voltage,
				cam_vreg->max_voltage);
			if (rc < 0) {
				pr_err("%s: %s set voltage failed\n",
					__func__, vreg_name);
				goto vreg_set_voltage_fail;
			}
			if (cam_vreg->op_mode >= 0) {
				rc = regulator_set_load(*reg_ptr,
					cam_vreg->op_mode);
				if (rc < 0) {
					pr_err(
					"%s: %s set optimum mode failed\n",
					__func__, vreg_name);
					goto vreg_set_opt_mode_fail;
				}
			}
		}
		rc = regulator_enable(*reg_ptr);
		if (rc < 0) {
			pr_err("%s: %s regulator_enable failed\n", __func__,
				vreg_name);
			goto vreg_unconfig;
		}
	} else {
		CDBG("%s disable %s\n", __func__, vreg_name);
		if (*reg_ptr) {
			CDBG("%s disable %s\n", __func__, vreg_name);
			regulator_disable(*reg_ptr);
			if (regulator_count_voltages(*reg_ptr) > 0) {
				if (cam_vreg->op_mode >= 0)
					regulator_set_load(*reg_ptr, 0);
				regulator_set_voltage(
					*reg_ptr, 0, cam_vreg->max_voltage);
			}
			regulator_put(*reg_ptr);
			*reg_ptr = NULL;
		} else {
			pr_err("%s can't disable %s\n", __func__, vreg_name);
		}
	}

	return 0;

vreg_unconfig:
	if (regulator_count_voltages(*reg_ptr) > 0)
		regulator_set_load(*reg_ptr, 0);

vreg_set_opt_mode_fail:
	if (regulator_count_voltages(*reg_ptr) > 0)
		regulator_set_voltage(*reg_ptr, 0,
			cam_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(*reg_ptr);
	*reg_ptr = NULL;

vreg_get_fail:
	return -EINVAL;
}

int msm_camera_request_gpio_table(struct gpio *gpio_tbl, uint8_t size,
	int gpio_en)
{
	int rc = 0, i = 0, err = 0;

	if (!gpio_tbl || !size) {
		pr_err("%s:%d invalid gpio_tbl %pK / size %d\n", __func__,
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

/* Get all clocks from DT */
static int msm_camera_get_clk_info_internal(struct device *dev,
			struct msm_cam_clk_info **clk_info,
			struct clk ***clk_ptr,
			size_t *num_clk)
{
	int rc = 0;
	size_t cnt, tmp;
	uint32_t *rates, i = 0;
	const char *clk_ctl = NULL;
	bool clock_cntl_support = false;
	struct device_node *of_node;

	of_node = dev->of_node;

	cnt = of_property_count_strings(of_node, "clock-names");
	if (cnt <= 0) {
		pr_err("err: No clocks found in DT=%zu\n", cnt);
		return -EINVAL;
	}

	tmp = of_property_count_u32_elems(of_node, "qcom,clock-rates");
	if (tmp <= 0) {
		pr_err("err: No clk rates device tree, count=%zu", tmp);
		return -EINVAL;
	}

	if (cnt != tmp) {
		pr_err("err: clk name/rates mismatch, strings=%zu, rates=%zu\n",
			cnt, tmp);
		return -EINVAL;
	}

	if (of_property_read_bool(of_node, "qcom,clock-cntl-support")) {
		tmp = of_property_count_strings(of_node,
				"qcom,clock-control");
		if (tmp <= 0) {
			pr_err("err: control strings not found in DT count=%zu",
				tmp);
			return -EINVAL;
		}
		if (cnt != tmp) {
			pr_err("err: controls mismatch, strings=%zu, ctl=%zu\n",
				cnt, tmp);
			return -EINVAL;
		}
		clock_cntl_support = true;
	}

	*num_clk = cnt;

	*clk_info = devm_kcalloc(dev, cnt,
				sizeof(struct msm_cam_clk_info), GFP_KERNEL);
	if (!*clk_info)
		return -ENOMEM;

	*clk_ptr = devm_kcalloc(dev, cnt, sizeof(struct clk *),
				GFP_KERNEL);
	if (!*clk_ptr) {
		rc = -ENOMEM;
		goto free_clk_info;
	}

	rates = devm_kcalloc(dev, cnt, sizeof(long), GFP_KERNEL);
	if (!rates) {
		rc = -ENOMEM;
		goto free_clk_ptr;
	}

	rc = of_property_read_u32_array(of_node, "qcom,clock-rates",
		rates, cnt);
	if (rc < 0) {
		pr_err("err: failed reading clock rates\n");
		rc = -EINVAL;
		goto free_rates;
	}

	for (i = 0; i < cnt; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &((*clk_info)[i].clk_name));
		if (rc < 0) {
			pr_err("%s reading clock-name failed index %d\n",
				__func__, i);
			rc = -EINVAL;
			goto free_rates;
		}

		CDBG("dbg: clk-name[%d] = %s\n", i, (*clk_info)[i].clk_name);
		if (clock_cntl_support) {
			rc = of_property_read_string_index(of_node,
				"qcom,clock-control", i, &clk_ctl);
			if (rc < 0) {
				pr_err("%s reading clock-control failed index %d\n",
					__func__, i);
				rc = -EINVAL;
				goto free_rates;
			}

			if (!strcmp(clk_ctl, "NO_SET_RATE")) {
				(*clk_info)[i].clk_rate = NO_SET_RATE;
			} else if (!strcmp(clk_ctl, "INIT_RATE")) {
				(*clk_info)[i].clk_rate = INIT_RATE;
			} else if (!strcmp(clk_ctl, "SET_RATE")) {
				(*clk_info)[i].clk_rate = rates[i];
			} else {
				pr_err("%s: error: clock control has invalid value\n",
					 __func__);
				rc = -EINVAL;
				goto free_rates;
			}
		} else {
			(*clk_info)[i].clk_rate =
				(rates[i] == 0) ? (long)-1 : rates[i];
		}

		CDBG("dbg: clk-rate[%d] = rate: %ld\n",
			i, (*clk_info)[i].clk_rate);

		(*clk_ptr)[i] =
			devm_clk_get(dev, (*clk_info)[i].clk_name);
		if (IS_ERR((*clk_ptr)[i])) {
			rc = PTR_ERR((*clk_ptr)[i]);
			goto release_clk;
		}
		CDBG("clk ptr[%d] :%pK\n", i, (*clk_ptr)[i]);
	}

	devm_kfree(dev, rates);

	return rc;

release_clk:
	for (--i; i >= 0; i--)
		devm_clk_put(dev, (*clk_ptr)[i]);
free_rates:
	devm_kfree(dev, rates);
free_clk_ptr:
	devm_kfree(dev, *clk_ptr);
free_clk_info:
	devm_kfree(dev, *clk_info);
	return rc;
}

/* Get all clocks from DT  for I2C devices */
int msm_camera_i2c_dev_get_clk_info(struct device *dev,
			struct msm_cam_clk_info **clk_info,
			struct clk ***clk_ptr,
			size_t *num_clk)
{
	int rc = 0;

	if (!dev || !clk_info || !clk_ptr || !num_clk)
		return -EINVAL;

	rc = msm_camera_get_clk_info_internal(dev, clk_info, clk_ptr, num_clk);
	return rc;
}
EXPORT_SYMBOL(msm_camera_i2c_dev_get_clk_info);

/* Get all clocks from DT  for platform devices */
int msm_camera_get_clk_info(struct platform_device *pdev,
			struct msm_cam_clk_info **clk_info,
			struct clk ***clk_ptr,
			size_t *num_clk)
{
	int rc = 0;

	if (!pdev || !&pdev->dev || !clk_info || !clk_ptr || !num_clk)
		return -EINVAL;

	rc = msm_camera_get_clk_info_internal(&pdev->dev,
			clk_info, clk_ptr, num_clk);
	return rc;
}
EXPORT_SYMBOL(msm_camera_get_clk_info);

/* Get all clocks and multiple rates from DT */
int msm_camera_get_clk_info_and_rates(
			struct platform_device *pdev,
			struct msm_cam_clk_info **pclk_info,
			struct clk ***pclks,
			uint32_t ***pclk_rates,
			size_t *num_set,
			size_t *num_clk)
{
	int rc = 0, tmp_var, cnt, tmp;
	uint32_t i = 0, j = 0;
	struct device_node *of_node;
	uint32_t **rates;
	struct clk **clks;
	struct msm_cam_clk_info *clk_info;

	if (!pdev || !pclk_info || !num_clk
		|| !pclk_rates || !pclks || !num_set)
		return -EINVAL;

	of_node = pdev->dev.of_node;

	cnt = of_property_count_strings(of_node, "clock-names");
	if (cnt <= 0) {
		pr_err("err: No clocks found in DT=%d\n", cnt);
		return -EINVAL;
	}

	tmp = of_property_count_u32_elems(of_node, "qcom,clock-rates");
	if (tmp <= 0) {
		pr_err("err: No clk rates device tree, count=%d\n", tmp);
		return -EINVAL;
	}

	if ((tmp % cnt) != 0) {
		pr_err("err: clk name/rates mismatch, strings=%d, rates=%d\n",
			cnt, tmp);
		return -EINVAL;
	}

	*num_clk = cnt;
	*num_set = (tmp / cnt);

	clk_info = devm_kcalloc(&pdev->dev, cnt,
				sizeof(struct msm_cam_clk_info), GFP_KERNEL);
	if (!clk_info)
		return -ENOMEM;

	clks = devm_kcalloc(&pdev->dev, cnt, sizeof(struct clk *),
				GFP_KERNEL);
	if (!clks) {
		rc = -ENOMEM;
		goto free_clk_info;
	}

	rates = devm_kcalloc(&pdev->dev, *num_set,
		sizeof(uint32_t *), GFP_KERNEL);
	if (!rates) {
		rc = -ENOMEM;
		goto free_clk;
	}

	for (i = 0; i < *num_set; i++) {
		rates[i] = devm_kcalloc(&pdev->dev, *num_clk,
			sizeof(uint32_t), GFP_KERNEL);
		if (!rates[i]) {
			rc = -ENOMEM;
			for (--i; i >= 0; i--)
				devm_kfree(&pdev->dev, rates[i]);
			goto free_rate;
		}
	}

	tmp_var = 0;
	for (i = 0; i < *num_set; i++) {
		for (j = 0; j < *num_clk; j++) {
			rc = of_property_read_u32_index(of_node,
				"qcom,clock-rates", tmp_var++, &rates[i][j]);
			if (rc < 0) {
				pr_err("err: failed reading clock rates\n");
				rc = -EINVAL;
				goto free_rate_array;
			}
			CDBG("Clock rate idx %d idx %d value %d\n",
				i, j, rates[i][j]);
		}
	}
	for (i = 0; i < *num_clk; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &clk_info[i].clk_name);
		if (rc < 0) {
			pr_err("%s reading clock-name failed index %d\n",
				__func__, i);
			rc = -EINVAL;
			goto free_rate_array;
		}

		CDBG("dbg: clk-name[%d] = %s\n", i, clk_info[i].clk_name);

		clks[i] =
			devm_clk_get(&pdev->dev, clk_info[i].clk_name);
		if (IS_ERR(clks[i])) {
			rc = PTR_ERR(clks[i]);
			goto release_clk;
		}
		CDBG("clk ptr[%d] :%pK\n", i, clks[i]);
	}
	*pclk_info = clk_info;
	*pclks = clks;
	*pclk_rates = rates;

	return rc;

release_clk:
	for (--i; i >= 0; i--)
		devm_clk_put(&pdev->dev, clks[i]);
free_rate_array:
	for (i = 0; i < *num_set; i++)
		devm_kfree(&pdev->dev, rates[i]);
free_rate:
	devm_kfree(&pdev->dev, rates);
free_clk:
	devm_kfree(&pdev->dev, clks);
free_clk_info:
	devm_kfree(&pdev->dev, clk_info);
	return rc;
}
EXPORT_SYMBOL(msm_camera_get_clk_info_and_rates);

/* Enable/Disable all clocks */
int msm_camera_clk_enable(struct device *dev,
		struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable)
{
	int i;
	int rc = 0;
	long clk_rate;

	if (enable) {
		for (i = 0; i < num_clk; i++) {
			pr_err("enable %s\n", clk_info[i].clk_name);
			if (clk_info[i].clk_rate > 0) {
				clk_rate = clk_round_rate(clk_ptr[i],
					clk_info[i].clk_rate);
				if (clk_rate < 0) {
					pr_err("%s round failed\n",
						   clk_info[i].clk_name);
					goto cam_clk_set_err;
				}
				rc = clk_set_rate(clk_ptr[i],
					clk_rate);
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
			rc = clk_prepare_enable(clk_ptr[i]);
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
				pr_err("%s disable %s\n", __func__,
					clk_info[i].clk_name);
				clk_disable_unprepare(clk_ptr[i]);
			}
		}
	}
	return rc;

cam_clk_enable_err:
cam_clk_set_err:
	for (i--; i >= 0; i--) {
		if (clk_ptr[i] != NULL)
			clk_disable_unprepare(clk_ptr[i]);
	}
	return rc;
}
EXPORT_SYMBOL(msm_camera_clk_enable);

/* Set rate on a specific clock */
long msm_camera_clk_set_rate(struct device *dev,
			struct clk *clk,
			long clk_rate)
{
	int rc = 0;
	long rate = 0;

	if (!dev || !clk || (clk_rate < 0))
		return -EINVAL;

	CDBG("clk : %pK, enable : %ld\n", clk, clk_rate);

	if (clk_rate > 0) {
		rate = clk_round_rate(clk, clk_rate);
		if (rate < 0) {
			pr_err("round rate failed\n");
			return -EINVAL;
		}

		rc = clk_set_rate(clk, rate);
		if (rc < 0) {
			pr_err("set rate failed\n");
			return -EINVAL;
		}
	}

	return rate;
}
EXPORT_SYMBOL(msm_camera_clk_set_rate);

/* release memory allocated for clocks */
static int msm_camera_put_clk_info_internal(struct device *dev,
				struct msm_cam_clk_info **clk_info,
				struct clk ***clk_ptr, int cnt)
{
	int i;

	for (i = cnt - 1; i >= 0; i--) {
		if (clk_ptr[i] != NULL)
			devm_clk_put(dev, (*clk_ptr)[i]);

		CDBG("clk ptr[%d] :%pK\n", i, (*clk_ptr)[i]);
	}
	devm_kfree(dev, *clk_info);
	devm_kfree(dev, *clk_ptr);
	*clk_info = NULL;
	*clk_ptr = NULL;
	return 0;
}

/* release memory allocated for clocks for i2c devices */
int msm_camera_i2c_dev_put_clk_info(struct device *dev,
				struct msm_cam_clk_info **clk_info,
				struct clk ***clk_ptr, int cnt)
{
	int rc = 0;

	if (!dev || !clk_info || !clk_ptr)
		return -EINVAL;

	rc = msm_camera_put_clk_info_internal(dev, clk_info, clk_ptr, cnt);
	return rc;
}
EXPORT_SYMBOL(msm_camera_i2c_dev_put_clk_info);

/* release memory allocated for clocks for platform devices */
int msm_camera_put_clk_info(struct platform_device *pdev,
				struct msm_cam_clk_info **clk_info,
				struct clk ***clk_ptr, int cnt)
{
	int rc = 0;

	if (!pdev || !&pdev->dev || !clk_info || !clk_ptr)
		return -EINVAL;

	rc = msm_camera_put_clk_info_internal(&pdev->dev,
			clk_info, clk_ptr, cnt);
	return rc;
}
EXPORT_SYMBOL(msm_camera_put_clk_info);

int msm_camera_put_clk_info_and_rates(struct platform_device *pdev,
		struct msm_cam_clk_info **clk_info,
		struct clk ***clk_ptr, uint32_t ***clk_rates,
		size_t set, size_t cnt)
{
	int i;

	for (i = set - 1; i >= 0; i--)
		devm_kfree(&pdev->dev, (*clk_rates)[i]);

	devm_kfree(&pdev->dev, *clk_rates);
	for (i = cnt - 1; i >= 0; i--) {
		if (clk_ptr[i] != NULL)
			devm_clk_put(&pdev->dev, (*clk_ptr)[i]);
		CDBG("clk ptr[%d] :%pK\n", i, (*clk_ptr)[i]);
	}
	devm_kfree(&pdev->dev, *clk_info);
	devm_kfree(&pdev->dev, *clk_ptr);
	*clk_info = NULL;
	*clk_ptr = NULL;
	*clk_rates = NULL;

	return 0;
}
EXPORT_SYMBOL(msm_camera_put_clk_info_and_rates);

/* Get regulators from DT */
int msm_camera_get_regulator_info(struct platform_device *pdev,
				struct msm_cam_regulator **vdd_info,
				int *num_reg)
{
	uint32_t cnt;
	int i, rc;
	struct device_node *of_node;
	char prop_name[32];
	struct msm_cam_regulator *tmp_reg;

	if (!pdev || !vdd_info || !num_reg)
		return -EINVAL;

	of_node = pdev->dev.of_node;

	if (!of_get_property(of_node, "qcom,vdd-names", NULL)) {
		pr_err("err: Regulators property not found\n");
		return -EINVAL;
	}

	cnt = of_property_count_strings(of_node, "qcom,vdd-names");
	if (cnt <= 0) {
		pr_err("err: no regulators found in device tree, count=%d",
			cnt);
		return -EINVAL;
	}

	tmp_reg = devm_kcalloc(&pdev->dev, cnt,
				sizeof(struct msm_cam_regulator), GFP_KERNEL);
	if (!tmp_reg)
		return -ENOMEM;

	for (i = 0; i < cnt; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,vdd-names", i, &tmp_reg[i].name);
		if (rc < 0) {
			pr_err("Fail to fetch regulators: %d\n", i);
			rc = -EINVAL;
			goto err1;
		}

		CDBG("regulator-names[%d] = %s\n", i, tmp_reg[i].name);

		snprintf(prop_name, 32, "%s-supply", tmp_reg[i].name);

		if (of_get_property(of_node, prop_name, NULL)) {
			tmp_reg[i].vdd =
				devm_regulator_get(&pdev->dev, tmp_reg[i].name);
			if (IS_ERR(tmp_reg[i].vdd)) {
				rc = -EINVAL;
				pr_err("Fail to get regulator :%d\n", i);
				goto err1;
			}
		} else {
			pr_err("Regulator phandle not found :%s\n",
				tmp_reg[i].name);
			rc = -EINVAL;
			goto err1;
		}
		CDBG("vdd ptr[%d] :%pK\n", i, tmp_reg[i].vdd);
	}

	*num_reg = cnt;
	*vdd_info = tmp_reg;

	return 0;

err1:
	for (--i; i >= 0; i--)
		devm_regulator_put(tmp_reg[i].vdd);
	devm_kfree(&pdev->dev, tmp_reg);
	return rc;
}
EXPORT_SYMBOL(msm_camera_get_regulator_info);


/* Enable/Disable regulators */
int msm_camera_regulator_enable(struct msm_cam_regulator *vdd_info,
				int cnt, int enable)
{
	int i;
	int rc;
	struct msm_cam_regulator *tmp = vdd_info;

	if (!tmp) {
		pr_err("Invalid params");
		return -EINVAL;
	}
	CDBG("cnt : %d\n", cnt);

	for (i = 0; i < cnt; i++) {
		if (tmp && !IS_ERR_OR_NULL(tmp->vdd)) {
			CDBG("name : %s, enable : %d\n", tmp->name, enable);
			if (enable) {
				rc = regulator_enable(tmp->vdd);
				if (rc < 0) {
					pr_err("regulator enable failed %d\n",
						i);
					goto disable_reg;
				}
			} else {
				rc = regulator_disable(tmp->vdd);
				if (rc < 0)
					pr_err("regulator disable failed %d\n",
						i);
			}
		}
		tmp++;
	}

	return 0;
disable_reg:
	for (--i; i > 0; i--) {
		--tmp;
		if (!IS_ERR_OR_NULL(tmp->vdd))
			regulator_disable(tmp->vdd);
	}
	return rc;
}
EXPORT_SYMBOL(msm_camera_regulator_enable);

/* Put regulators regulators */
void msm_camera_put_regulators(struct platform_device *pdev,
	struct msm_cam_regulator **vdd_info, int cnt)
{
	int i;

	if (!vdd_info || !*vdd_info) {
		pr_err("Invalid params\n");
		return;
	}

	for (i = cnt - 1; i >= 0; i--) {
		if (vdd_info[i] && !IS_ERR_OR_NULL(vdd_info[i]->vdd))
			devm_regulator_put(vdd_info[i]->vdd);
			CDBG("vdd ptr[%d] :%pK\n", i, vdd_info[i]->vdd);
	}

	devm_kfree(&pdev->dev, *vdd_info);
	*vdd_info = NULL;
}
EXPORT_SYMBOL(msm_camera_put_regulators);

struct resource *msm_camera_get_irq(struct platform_device *pdev,
							char *irq_name)
{
	if (!pdev || !irq_name) {
		pr_err("Invalid params\n");
		return NULL;
	}

	CDBG("Get irq for %s\n", irq_name);
	return platform_get_resource_byname(pdev, IORESOURCE_IRQ, irq_name);
}
EXPORT_SYMBOL(msm_camera_get_irq);

int msm_camera_register_irq(struct platform_device *pdev,
			struct resource *irq, irq_handler_t handler,
			unsigned long irqflags, char *irq_name, void *dev_id)
{
	int rc = 0;

	if (!pdev || !irq || !handler || !irq_name || !dev_id) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	rc = devm_request_irq(&pdev->dev, irq->start, handler,
		irqflags, irq_name, dev_id);
	if (rc < 0) {
		pr_err("irq request fail\n");
		rc = -EINVAL;
	}

	CDBG("Registered irq for %s[resource - %pK]\n", irq_name, irq);

	return rc;
}
EXPORT_SYMBOL(msm_camera_register_irq);

int msm_camera_register_threaded_irq(struct platform_device *pdev,
			struct resource *irq, irq_handler_t handler_fn,
			irq_handler_t thread_fn, unsigned long irqflags,
			const char *irq_name, void *dev_id)
{
	int rc = 0;

	if (!pdev || !irq || !irq_name || !dev_id) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	rc = devm_request_threaded_irq(&pdev->dev, irq->start, handler_fn,
			thread_fn, irqflags, irq_name, dev_id);
	if (rc < 0) {
		pr_err("irq request fail\n");
		rc = -EINVAL;
	}

	CDBG("Registered irq for %s[resource - %pK]\n", irq_name, irq);

	return rc;
}
EXPORT_SYMBOL(msm_camera_register_threaded_irq);

int msm_camera_enable_irq(struct resource *irq, int enable)
{
	if (!irq) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	CDBG("irq Enable %d\n", enable);
	if (enable)
		enable_irq(irq->start);
	else
		disable_irq(irq->start);

	return 0;
}
EXPORT_SYMBOL(msm_camera_enable_irq);

int msm_camera_unregister_irq(struct platform_device *pdev,
	struct resource *irq, void *dev_id)
{

	if (!pdev || !irq || !dev_id) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	CDBG("Un Registering irq for [resource - %pK]\n", irq);
	devm_free_irq(&pdev->dev, irq->start, dev_id);

	return 0;
}
EXPORT_SYMBOL(msm_camera_unregister_irq);

void __iomem *msm_camera_get_reg_base(struct platform_device *pdev,
		char *device_name, int reserve_mem)
{
	struct resource *mem;
	void *base;

	if (!pdev || !device_name) {
		pr_err("Invalid params\n");
		return NULL;
	}

	CDBG("device name :%s\n", device_name);
	mem = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, device_name);
	if (!mem) {
		pr_err("err: mem resource %s not found\n", device_name);
		return NULL;
	}

	if (reserve_mem) {
		CDBG("device:%pK, mem : %pK, size : %d\n",
			&pdev->dev, mem, (int)resource_size(mem));
		if (!devm_request_mem_region(&pdev->dev, mem->start,
			resource_size(mem),
			device_name)) {
			pr_err("err: no valid mem region for device:%s\n",
				device_name);
			return NULL;
		}
	}

	base = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!base) {
		devm_release_mem_region(&pdev->dev, mem->start,
				resource_size(mem));
		pr_err("err: ioremap failed: %s\n", device_name);
		return NULL;
	}

	CDBG("base : %pK\n", base);
	return base;
}
EXPORT_SYMBOL(msm_camera_get_reg_base);

uint32_t msm_camera_get_res_size(struct platform_device *pdev,
	char *device_name)
{
	struct resource *mem;

	if (!pdev || !device_name) {
		pr_err("Invalid params\n");
		return 0;
	}

	CDBG("device name :%s\n", device_name);
	mem = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, device_name);
	if (!mem) {
		pr_err("err: mem resource %s not found\n", device_name);
		return 0;
	}
	return resource_size(mem);
}
EXPORT_SYMBOL(msm_camera_get_res_size);


int msm_camera_put_reg_base(struct platform_device *pdev,
	void __iomem *base, char *device_name, int reserve_mem)
{
	struct resource *mem;

	if (!pdev || !base || !device_name) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	CDBG("device name :%s\n", device_name);
	mem = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, device_name);
	if (!mem) {
		pr_err("err: mem resource %s not found\n", device_name);
		return -EINVAL;
	}
	CDBG("mem : %pK, size : %d\n", mem, (int)resource_size(mem));

	devm_iounmap(&pdev->dev, base);
	if (reserve_mem)
		devm_release_mem_region(&pdev->dev,
			mem->start, resource_size(mem));

	return 0;
}
EXPORT_SYMBOL(msm_camera_put_reg_base);
