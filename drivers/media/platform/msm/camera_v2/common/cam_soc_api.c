// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "CAM-SOC %s:%d " fmt, __func__, __LINE__
#define NO_SET_RATE -1
#define INIT_RATE -2

#ifdef CONFIG_CAM_SOC_API_DBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/msm-bus.h>
#include <linux/clk.h>
#include "cam_soc_api.h"

struct msm_cam_bus_pscale_data {
	struct msm_bus_scale_pdata *pdata;
	uint32_t bus_client;
	uint32_t num_usecases;
	uint32_t num_paths;
	unsigned int vector_index;
	bool dyn_vote;
	struct mutex lock;
};

static struct msm_cam_bus_pscale_data g_cv[CAM_BUS_CLIENT_MAX];


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
		pr_err("err: No clk rates device tree, count=%zu\n", tmp);
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
			pr_err("err: control strings not found in DT count=%zu\n",
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
	if (!*clk_ptr)
		return -ENOMEM;

	rates = devm_kcalloc(dev, cnt, sizeof(long), GFP_KERNEL);
	if (!rates)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,clock-rates",
		rates, cnt);
	if (rc < 0) {
		pr_err("err: failed reading clock rates\n");
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &((*clk_info)[i].clk_name));
		if (rc < 0) {
			pr_err("%s reading clock-name failed index %d\n",
				__func__, i);
			return -EINVAL;
		}

		CDBG("dbg: clk-name[%d] = %s\n", i, (*clk_info)[i].clk_name);
		if (clock_cntl_support) {
			rc = of_property_read_string_index(of_node,
				"qcom,clock-control", i, &clk_ctl);
			if (rc < 0) {
				pr_err("%s reading clock-control failed index %d\n",
					__func__, i);
				return -EINVAL;
			}

			if (!strcmp(clk_ctl, "NO_SET_RATE"))
				(*clk_info)[i].clk_rate = NO_SET_RATE;
			else if (!strcmp(clk_ctl, "INIT_RATE"))
				(*clk_info)[i].clk_rate = INIT_RATE;
			else if (!strcmp(clk_ctl, "SET_RATE"))
				(*clk_info)[i].clk_rate = rates[i];
			else {
				pr_err("%s: error: clock control has invalid value\n",
					 __func__);
				return -EBUSY;
			}
		} else
			(*clk_info)[i].clk_rate =
				(rates[i] == 0) ? (long)-1 : rates[i];

		CDBG("dbg: clk-rate[%d] = rate: %ld\n",
			i, (*clk_info)[i].clk_rate);

		(*clk_ptr)[i] =
			devm_clk_get(dev, (*clk_info)[i].clk_name);
		if (IS_ERR((*clk_ptr)[i])) {
			rc = PTR_ERR((*clk_ptr)[i]);
			return rc;
		}
		CDBG("clk ptr[%d] :%pK\n", i, (*clk_ptr)[i]);
	}

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

	if (!pdev || !clk_info || !clk_ptr || !num_clk)
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
	int32_t i = 0, j = 0;
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
	if (!clks)
		return -ENOMEM;

	rates = devm_kcalloc(&pdev->dev, *num_set,
		sizeof(uint32_t *), GFP_KERNEL);
	if (!rates)
		return -ENOMEM;

	for (i = 0; i < *num_set; i++) {
		rates[i] = devm_kcalloc(&pdev->dev, *num_clk,
				sizeof(uint32_t), GFP_KERNEL);
		if (!rates[i] && (i > 0))
			return -ENOMEM;
	}

	tmp_var = 0;
	for (i = 0; i < *num_set; i++) {
		for (j = 0; j < *num_clk; j++) {
			rc = of_property_read_u32_index(of_node,
				"qcom,clock-rates", tmp_var++, &rates[i][j]);
			if (rc < 0) {
				pr_err("err: failed reading clock rates\n");
				return -EINVAL;
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
			return -EINVAL;
		}

		CDBG("dbg: clk-name[%d] = %s\n", i, clk_info[i].clk_name);

		clks[i] =
			devm_clk_get(&pdev->dev, clk_info[i].clk_name);
		if (IS_ERR(clks[i])) {
			rc = PTR_ERR(clks[i]);
			return rc;
		}
		CDBG("clk ptr[%d] :%pK\n", i, clks[i]);
	}
	*pclk_info = clk_info;
	*pclks = clks;
	*pclk_rates = rates;

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
					if (clk_rate <= 0) {
						pr_err("%s round rate failed\n",
							  clk_info[i].clk_name);
						goto cam_clk_set_err;
					}
				}
				rc = clk_set_rate(clk_ptr[i], clk_rate);
				if (rc < 0) {
					pr_err("%s set rate failed\n",
						clk_info[i].clk_name);
					goto cam_clk_set_err;
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
			if (clk_ptr[i] != NULL)
				clk_disable_unprepare(clk_ptr[i]);
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

int msm_camera_set_clk_flags(struct clk *clk, unsigned long flags)
{
	if (!clk)
		return -EINVAL;

	CDBG("clk : %pK, flags : %ld\n", clk, flags);

	return clk_set_flags(clk, flags);
}
EXPORT_SYMBOL(msm_camera_set_clk_flags);

/* release memory allocated for clocks */
static int msm_camera_put_clk_info_internal(struct device *dev,
				struct msm_cam_clk_info **clk_info,
				struct clk ***clk_ptr, int cnt)
{
	int i;

	for (i = cnt - 1; i >= 0; i--)
		CDBG("clk ptr[%d] :%pK\n", i, (*clk_ptr)[i]);
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

	if (!pdev || !clk_info || !clk_ptr)
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

	for (i = cnt - 1; i >= 0; i--)
		CDBG("clk ptr[%d] :%pK\n", i, (*clk_ptr)[i]);
	*clk_info = NULL;
	*clk_ptr = NULL;
	*clk_rates = NULL;
	return 0;
}
EXPORT_SYMBOL(msm_camera_put_clk_info_and_rates);

/* Get reset info from DT */
int msm_camera_get_reset_info(struct platform_device *pdev,
		struct reset_control **micro_iface_reset)
{
	if (!pdev || !micro_iface_reset)
		return -EINVAL;

	if (of_property_match_string(pdev->dev.of_node, "reset-names",
				"micro_iface_reset")) {
		pr_err("err: Reset property not found\n");
		return -EINVAL;
	}

	*micro_iface_reset = devm_reset_control_get
				(&pdev->dev, "micro_iface_reset");
	return PTR_ERR_OR_ZERO(*micro_iface_reset);
}
EXPORT_SYMBOL(msm_camera_get_reset_info);

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
		pr_err("err: no regulators found in device tree, count=%d\n",
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
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	CDBG("cnt : %d\n", cnt);

	for (i = 0; i < cnt; i++) {
		if (tmp && !IS_ERR_OR_NULL(tmp->vdd)) {
			if (enable) {
				rc = regulator_enable(tmp->vdd);
				if (rc < 0) {
					pr_err("regulator enable failed %d\n",
						i);
					goto error;
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
error:
	for (--i; i > 0; i--) {
		--tmp;
		if (!IS_ERR_OR_NULL(tmp->vdd))
			regulator_disable(tmp->vdd);
	}
	return rc;
}
EXPORT_SYMBOL(msm_camera_regulator_enable);

/* disable/Disable regulators */
int msm_camera_regulator_disable(struct msm_cam_regulator *vdd_info,
				int cnt, int disable)
{
	int i;
	int rc;
	struct msm_cam_regulator *tmp = vdd_info;

	if (!tmp) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	for (i = cnt; i > 0; i--) {
		if (disable) {
			rc = regulator_disable(tmp[i-1].vdd);
			if (rc < 0) {
				pr_debug("%s: %s failed %d\n",
					__func__, tmp[i-1].name, i);
				return rc;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(msm_camera_regulator_disable);


/* set regulator mode */
int msm_camera_regulator_set_mode(struct msm_cam_regulator *vdd_info,
				int cnt, bool mode)
{
	int i;
	int rc;
	struct msm_cam_regulator *tmp = vdd_info;

	if (!tmp) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	CDBG("cnt : %d\n", cnt);

	for (i = 0; i < cnt; i++) {
		if (tmp && !IS_ERR_OR_NULL(tmp->vdd)) {
			CDBG("name : %s, enable : %d\n", tmp->name, mode);
			if (mode) {
				rc = regulator_set_mode(tmp->vdd,
					REGULATOR_MODE_NORMAL);
				if (rc < 0) {
					pr_err("regulator enable failed %d\n",
						i);
					goto error;
				}
			} else {
				rc = regulator_set_mode(tmp->vdd,
					REGULATOR_MODE_NORMAL);
				if (rc < 0)
					pr_err("regulator disable failed %d\n",
							i);
				goto error;
			}
		}
		tmp++;
	}

	return 0;
error:
	return rc;
}
EXPORT_SYMBOL(msm_camera_regulator_set_mode);


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
			CDBG("vdd ptr[%d] :%pK\n", i, vdd_info[i]->vdd);
	}

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

	rc =  request_threaded_irq(irq->start, handler, NULL,
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
	free_irq(irq->start, dev_id);

	return 0;
}
EXPORT_SYMBOL(msm_camera_unregister_irq);

void __iomem *msm_camera_get_reg_base(struct platform_device *pdev,
		char *device_name, int reserve_mem)
{
	struct resource *mem;
	void __iomem *base;

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
	void __iomem *base,	char *device_name, int reserve_mem)
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

	if (reserve_mem)
		devm_release_mem_region(&pdev->dev,
			mem->start, resource_size(mem));

	return 0;
}
EXPORT_SYMBOL(msm_camera_put_reg_base);

/* Register the bus client */
uint32_t msm_camera_register_bus_client(struct platform_device *pdev,
	enum cam_bus_client id)
{
	int rc = 0;
	uint32_t bus_client, num_usecases, num_paths;
	struct msm_bus_scale_pdata *pdata;
	struct device_node *of_node;

	CDBG("Register client ID: %d\n", id);

	if (id >= CAM_BUS_CLIENT_MAX || !pdev) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;

	if (!g_cv[id].pdata) {
		rc = of_property_read_u32(of_node, "qcom,msm-bus,num-cases",
				&num_usecases);
		if (rc) {
			pr_err("num-usecases not found\n");
			return -EINVAL;
		}
		rc = of_property_read_u32(of_node, "qcom,msm-bus,num-paths",
				&num_paths);
		if (rc) {
			pr_err("num-usecases not found\n");
			return -EINVAL;
		}

		if (num_paths != 1) {
			pr_err("Exceeds number of paths\n");
			return -EINVAL;
		}

		if (of_property_read_bool(of_node,
				"qcom,msm-bus-vector-dyn-vote")) {
			if (num_usecases != 2) {
				pr_err("Excess or less vectors\n");
				return -EINVAL;
			}
			g_cv[id].dyn_vote = true;
		}

		pdata = msm_bus_cl_get_pdata(pdev);
		if (!pdata) {
			pr_err("failed get_pdata client_id :%d\n", id);
			return -EINVAL;
		}
		bus_client = msm_bus_scale_register_client(pdata);
		if (!bus_client) {
			pr_err("Unable to register bus client :%d\n", id);
			return -EINVAL;
		}
	} else {
		pr_err("vector already setup client_id : %d\n", id);
		return -EINVAL;
	}

	g_cv[id].pdata = pdata;
	g_cv[id].bus_client = bus_client;
	g_cv[id].vector_index = 0;
	g_cv[id].num_usecases = num_usecases;
	g_cv[id].num_paths = num_paths;
	mutex_init(&g_cv[id].lock);
	CDBG("Exit Client ID: %d\n", id);
	return 0;
}
EXPORT_SYMBOL(msm_camera_register_bus_client);

/* Update the bus bandwidth */
uint32_t msm_camera_update_bus_bw(int id, uint64_t ab, uint64_t ib)
{
	struct msm_bus_paths *path;
	struct msm_bus_scale_pdata *pdata;
	int idx = 0;

	if (id >= CAM_BUS_CLIENT_MAX) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	if (g_cv[id].num_usecases != 2 ||
		g_cv[id].num_paths != 1 ||
		!g_cv[id].dyn_vote) {
		pr_err("dynamic update not allowed\n");
		return -EINVAL;
	}

	mutex_lock(&g_cv[id].lock);
	idx = g_cv[id].vector_index;
	idx = 1 - idx;
	g_cv[id].vector_index = idx;
	mutex_unlock(&g_cv[id].lock);

	pdata = g_cv[id].pdata;
	path = &(pdata->usecase[idx]);
	path->vectors[0].ab = ab;
	path->vectors[0].ib = ib;

	CDBG("Register client ID : %d [ab : %llx, ib : %llx], update :%d\n",
		id, ab, ib, idx);
	msm_bus_scale_client_update_request(g_cv[id].bus_client, idx);

	return 0;
}
EXPORT_SYMBOL(msm_camera_update_bus_bw);

/* Update the bus vector */
uint32_t msm_camera_update_bus_vector(enum cam_bus_client id,
	int vector_index)
{
	if (id >= CAM_BUS_CLIENT_MAX || g_cv[id].dyn_vote) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (vector_index < 0 || vector_index > g_cv[id].num_usecases) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	CDBG("Register client ID : %d vector idx: %d,\n", id, vector_index);
	msm_bus_scale_client_update_request(g_cv[id].bus_client,
		vector_index);

	return 0;
}
EXPORT_SYMBOL(msm_camera_update_bus_vector);

/* Unregister the bus client */
uint32_t msm_camera_unregister_bus_client(enum cam_bus_client id)
{
	if (id >= CAM_BUS_CLIENT_MAX) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	CDBG("UnRegister client ID: %d\n", id);

	mutex_destroy(&g_cv[id].lock);
	msm_bus_scale_unregister_client(g_cv[id].bus_client);
	g_cv[id].bus_client = 0;
	g_cv[id].num_usecases = 0;
	g_cv[id].num_paths = 0;
	g_cv[id].vector_index = 0;
	g_cv[id].dyn_vote = false;

	return 0;
}
EXPORT_SYMBOL(msm_camera_unregister_bus_client);
