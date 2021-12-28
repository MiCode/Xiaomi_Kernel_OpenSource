// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2015, 2017-2021 The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/qcom/spmi-pmic-arb.h>
#include <linux/pinctrl/qcom-pinctrl.h>
#include <linux/delay.h>
#include <linux/sde_io_util.h>
#include <linux/sde_vm_event.h>
#include "sde_dbg.h"

#define MAX_I2C_CMDS  16
void dss_reg_w(struct dss_io_data *io, u32 offset, u32 value, u32 debug)
{
	u32 in_val;

	if (!io || !io->base) {
		DEV_ERR("%pS->%s: invalid input\n",
			__builtin_return_address(0), __func__);
		return;
	}

	if (offset > io->len) {
		DEV_ERR("%pS->%s: offset out of range\n",
			__builtin_return_address(0), __func__);
		return;
	}

	writel_relaxed(value, io->base + offset);
	if (debug) {
		in_val = readl_relaxed(io->base + offset);
		DEV_DBG("[%08x] => %08x [%08x]\n",
			(u32)(unsigned long)(io->base + offset),
			value, in_val);
	}
	SDE_REG_LOG(SDE_REG_LOG_RSCC, value, offset);
} /* dss_reg_w */
EXPORT_SYMBOL(dss_reg_w);

u32 dss_reg_r(struct dss_io_data *io, u32 offset, u32 debug)
{
	u32 value;

	if (!io || !io->base) {
		DEV_ERR("%pS->%s: invalid input\n",
			__builtin_return_address(0), __func__);
		return -EINVAL;
	}

	if (offset > io->len) {
		DEV_ERR("%pS->%s: offset out of range\n",
			__builtin_return_address(0), __func__);
		return -EINVAL;
	}

	value = readl_relaxed(io->base + offset);
	if (debug)
		DEV_DBG("[%08x] <= %08x\n",
			(u32)(unsigned long)(io->base + offset), value);

	return value;
} /* dss_reg_r */
EXPORT_SYMBOL(dss_reg_r);

void dss_reg_dump(void __iomem *base, u32 length, const char *prefix,
	u32 debug)
{
	if (debug)
		print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET, 32, 4,
			(void *)base, length, false);
} /* dss_reg_dump */
EXPORT_SYMBOL(dss_reg_dump);

static struct resource *msm_dss_get_res_byname(struct platform_device *pdev,
	unsigned int type, const char *name)
{
	struct resource *res = NULL;

	res = platform_get_resource_byname(pdev, type, name);
	if (!res)
		DEV_ERR("%s: '%s' resource not found\n", __func__, name);

	return res;
} /* msm_dss_get_res_byname */

int msm_dss_ioremap_byname(struct platform_device *pdev,
	struct dss_io_data *io_data, const char *name)
{
	struct resource *res = NULL;

	if (!pdev || !io_data) {
		DEV_ERR("%pS->%s: invalid input\n",
			__builtin_return_address(0), __func__);
		return -EINVAL;
	}

	res = msm_dss_get_res_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		DEV_ERR("%pS->%s: '%s' msm_dss_get_res_byname failed\n",
			__builtin_return_address(0), __func__, name);
		return -ENODEV;
	}

	io_data->len = (u32)resource_size(res);
	io_data->base = ioremap(res->start, io_data->len);
	if (!io_data->base) {
		DEV_ERR("%pS->%s: '%s' ioremap failed\n",
			__builtin_return_address(0), __func__, name);
		return -EIO;
	}

	return 0;
} /* msm_dss_ioremap_byname */
EXPORT_SYMBOL(msm_dss_ioremap_byname);

void msm_dss_iounmap(struct dss_io_data *io_data)
{
	if (!io_data) {
		DEV_ERR("%pS->%s: invalid input\n",
			__builtin_return_address(0), __func__);
		return;
	}

	if (io_data->base) {
		iounmap(io_data->base);
		io_data->base = NULL;
	}
	io_data->len = 0;
} /* msm_dss_iounmap */
EXPORT_SYMBOL(msm_dss_iounmap);

int msm_dss_get_gpio_io_mem(const int gpio_pin, struct list_head *mem_list)
{
	struct msm_io_mem_entry *io_mem;
	struct resource res;
	bool gpio_pin_status = false;
	int rc = 0;

	if (!gpio_is_valid(gpio_pin))
		return -EINVAL;

	io_mem = kzalloc(sizeof(struct msm_io_mem_entry), GFP_KERNEL);
	if (!io_mem)
		return -ENOMEM;

	gpio_pin_status = msm_gpio_get_pin_address(gpio_pin, &res);
	if (!gpio_pin_status) {
		rc = -ENODEV;
		goto parse_fail;
	}

	io_mem->base = res.start;
	io_mem->size = resource_size(&res);

	list_add(&io_mem->list, mem_list);

	return 0;

parse_fail:
	kfree(io_mem);

	return rc;
}
EXPORT_SYMBOL(msm_dss_get_gpio_io_mem);

int msm_dss_get_pmic_io_mem(struct platform_device *pdev,
		struct list_head *mem_list)
{
	struct list_head temp_head;
	struct msm_io_mem_entry *io_mem;
	struct resource *res = NULL;
	struct property *prop;
	const __be32 *cur;
	int rc = 0;
	u32 val;

	INIT_LIST_HEAD(&temp_head);

	res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	if (!res)
		return -ENOMEM;

	of_property_for_each_u32(pdev->dev.of_node, "qcom,pmic-arb-address",
			prop, cur, val) {
		rc = spmi_pmic_arb_map_address(&pdev->dev, val, res);
		if (rc < 0) {
			DEV_ERR("%pS - failed to map pmic address, rc:%d\n",
						    __func__, rc);
			goto parse_fail;
		}

		io_mem = kzalloc(sizeof(struct msm_io_mem_entry), GFP_KERNEL);
		if (!io_mem) {
			rc = -ENOMEM;
			goto parse_fail;
		}

		io_mem->base = res->start;
		io_mem->size = resource_size(res);

		list_add(&io_mem->list, &temp_head);
	}

	list_splice(&temp_head, mem_list);
	goto end;

parse_fail:
	msm_dss_clean_io_mem(&temp_head);
end:
	kfree(res);
	return rc;
}
EXPORT_SYMBOL(msm_dss_get_pmic_io_mem);

int msm_dss_get_io_mem(struct platform_device *pdev, struct list_head *mem_list)
{
	struct list_head temp_head;
	struct msm_io_mem_entry *io_mem;
	struct resource *res = NULL;
	const char *reg_name, *exclude_reg_name;
	int i, j, rc = 0;
	int num_entry, num_exclude_entry;

	INIT_LIST_HEAD(&temp_head);

	num_entry = of_property_count_strings(pdev->dev.of_node,
						  "reg-names");
	if (num_entry < 0)
		num_entry = 0;

	/*
	 * check the dt property to know whether the platform device wants
	 * to exclude any reg ranges from the IO list
	 */
	num_exclude_entry = of_property_count_strings(pdev->dev.of_node,
					  "qcom,sde-vm-exclude-reg-names");
	if (num_exclude_entry < 0)
		num_exclude_entry = 0;

	for (i = 0; i < num_entry; i++) {
		bool exclude = false;

		of_property_read_string_index(pdev->dev.of_node,
				"reg-names", i,	&reg_name);

		for (j = 0; j < num_exclude_entry; j++) {
			of_property_read_string_index(pdev->dev.of_node,
				"qcom,sde-vm-exclude-reg-names", j,
				&exclude_reg_name);

			if (!strcmp(reg_name, exclude_reg_name)) {
				exclude = true;
				break;
			}
		}

		if (exclude)
			continue;

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   reg_name);
		if (!res)
			break;

		io_mem = kzalloc(sizeof(*io_mem), GFP_KERNEL);
		if (!io_mem) {
			msm_dss_clean_io_mem(&temp_head);
			rc = -ENOMEM;
			goto parse_fail;
		}

		io_mem->base = res->start;
		io_mem->size = resource_size(res);

		list_add(&io_mem->list, &temp_head);
	}

	list_splice(&temp_head, mem_list);

	return 0;

parse_fail:
	msm_dss_clean_io_mem(&temp_head);

	return rc;
}
EXPORT_SYMBOL(msm_dss_get_io_mem);

void msm_dss_clean_io_mem(struct list_head *mem_list)
{
	struct msm_io_mem_entry *pos, *tmp;

	list_for_each_entry_safe(pos, tmp, mem_list, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}
EXPORT_SYMBOL(msm_dss_clean_io_mem);

int msm_dss_get_io_irq(struct platform_device *pdev, struct list_head *irq_list,
		       u32 label)
{
	struct msm_io_irq_entry *io_irq;
	int irq;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("invalid IRQ\n");
		return irq;
	}

	io_irq = kzalloc(sizeof(*io_irq), GFP_KERNEL);
	if (!io_irq)
		return -ENOMEM;

	io_irq->label  = label;
	io_irq->irq_num = irq;

	list_add(&io_irq->list, irq_list);

	return 0;
}
EXPORT_SYMBOL(msm_dss_get_io_irq);

void msm_dss_clean_io_irq(struct list_head *irq_list)
{
	struct msm_io_irq_entry *pos, *tmp;

	list_for_each_entry_safe(pos, tmp, irq_list, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}
EXPORT_SYMBOL(msm_dss_clean_io_irq);

int msm_dss_get_vreg(struct device *dev, struct dss_vreg *in_vreg,
	int num_vreg, int enable)
{
	int i = 0, rc = 0;
	struct dss_vreg *curr_vreg = NULL;

	if (!in_vreg || !num_vreg)
		return rc;

	if (enable) {
		for (i = 0; i < num_vreg; i++) {
			curr_vreg = &in_vreg[i];
			curr_vreg->vreg = regulator_get(dev,
				curr_vreg->vreg_name);
			rc = PTR_ERR_OR_ZERO(curr_vreg->vreg);
			if (rc) {
				DEV_ERR("%pS->%s: %s get failed. rc=%d\n",
					 __builtin_return_address(0), __func__,
					 curr_vreg->vreg_name, rc);
				curr_vreg->vreg = NULL;
				goto vreg_get_fail;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			curr_vreg = &in_vreg[i];
			if (curr_vreg->vreg) {
				regulator_put(curr_vreg->vreg);
				curr_vreg->vreg = NULL;
			}
		}
	}
	return 0;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		curr_vreg = &in_vreg[i];
		regulator_set_load(curr_vreg->vreg, 0);
		regulator_put(curr_vreg->vreg);
		curr_vreg->vreg = NULL;
	}
	return rc;
} /* msm_dss_get_vreg */
EXPORT_SYMBOL(msm_dss_get_vreg);

static bool msm_dss_is_hw_controlled(struct dss_vreg in_vreg)
{
	u32 mode = 0;
	char const *regulator_gdsc = "gdsc";

	/*
	 * For gdsc-regulator devices only, REGULATOR_MODE_FAST specifies that
	 * the GDSC is in HW controlled mode.
	 */
	mode = regulator_get_mode(in_vreg.vreg);
	if (!strcmp(regulator_gdsc, in_vreg.vreg_name) &&
			mode == REGULATOR_MODE_FAST) {
		DEV_DBG("%pS->%s: %s is HW controlled\n",
				__builtin_return_address(0), __func__,
				in_vreg.vreg_name);
		return true;
	}

	return false;
}

int msm_dss_enable_vreg(struct dss_vreg *in_vreg, int num_vreg, int enable)
{
	int i = 0, rc = 0;
	bool need_sleep;

	if (enable) {
		for (i = 0; i < num_vreg; i++) {
			rc = PTR_ERR_OR_ZERO(in_vreg[i].vreg);
			if (rc) {
				DEV_ERR("%pS->%s: %s regulator error. rc=%d\n",
					__builtin_return_address(0), __func__,
					in_vreg[i].vreg_name, rc);
				goto vreg_set_opt_mode_fail;
			}
			if (msm_dss_is_hw_controlled(in_vreg[i]))
				continue;

			need_sleep = !regulator_is_enabled(in_vreg[i].vreg);
			if (in_vreg[i].pre_on_sleep && need_sleep)
				usleep_range(in_vreg[i].pre_on_sleep * 1000,
					(in_vreg[i].pre_on_sleep * 1000) + 10);
			rc = regulator_set_load(in_vreg[i].vreg,
				in_vreg[i].enable_load);
			if (rc < 0) {
				DEV_ERR("%pS->%s: %s set opt m fail\n",
					__builtin_return_address(0), __func__,
					in_vreg[i].vreg_name);
				goto vreg_set_opt_mode_fail;
			}
			if (regulator_count_voltages(in_vreg[i].vreg) > 0)
				regulator_set_voltage(in_vreg[i].vreg,
						in_vreg[i].min_voltage,
						in_vreg[i].max_voltage);
			rc = regulator_enable(in_vreg[i].vreg);
			if (in_vreg[i].post_on_sleep && need_sleep)
				usleep_range(in_vreg[i].post_on_sleep * 1000,
					(in_vreg[i].post_on_sleep * 1000) + 10);
			if (rc < 0) {
				DEV_ERR("%pS->%s: %s enable failed\n",
					__builtin_return_address(0), __func__,
					in_vreg[i].vreg_name);
				goto disable_vreg;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			if (msm_dss_is_hw_controlled(in_vreg[i]))
				continue;

			if (in_vreg[i].pre_off_sleep)
				usleep_range(in_vreg[i].pre_off_sleep * 1000,
					(in_vreg[i].pre_off_sleep * 1000) + 10);
			regulator_disable(in_vreg[i].vreg);
			if (in_vreg[i].post_off_sleep)
				usleep_range(in_vreg[i].post_off_sleep * 1000,
				(in_vreg[i].post_off_sleep * 1000) + 10);
			regulator_set_load(in_vreg[i].vreg,
				in_vreg[i].disable_load);
			if (regulator_count_voltages(in_vreg[i].vreg) > 0)
				regulator_set_voltage(in_vreg[i].vreg, 0,
						in_vreg[i].max_voltage);
		}
	}
	return rc;

disable_vreg:
	regulator_set_load(in_vreg[i].vreg, in_vreg[i].disable_load);

vreg_set_opt_mode_fail:
	for (i--; i >= 0; i--) {
		if (in_vreg[i].pre_off_sleep)
			usleep_range(in_vreg[i].pre_off_sleep * 1000,
				(in_vreg[i].pre_off_sleep * 1000) + 10);
		regulator_disable(in_vreg[i].vreg);
		if (in_vreg[i].post_off_sleep)
			usleep_range(in_vreg[i].post_off_sleep * 1000,
				(in_vreg[i].post_off_sleep * 1000) + 10);
		regulator_set_load(in_vreg[i].vreg,
			in_vreg[i].disable_load);
	}

	return rc;
} /* msm_dss_enable_vreg */
EXPORT_SYMBOL(msm_dss_enable_vreg);

int msm_dss_enable_gpio(struct dss_gpio *in_gpio, int num_gpio, int enable)
{
	int i = 0, rc = 0;

	if (enable) {
		for (i = 0; i < num_gpio; i++) {
			DEV_DBG("%pS->%s: %s enable\n",
				__builtin_return_address(0), __func__,
				in_gpio[i].gpio_name);

			rc = gpio_request(in_gpio[i].gpio,
				in_gpio[i].gpio_name);
			if (rc < 0) {
				DEV_ERR("%pS->%s: %s enable failed\n",
					__builtin_return_address(0), __func__,
					in_gpio[i].gpio_name);
				goto disable_gpio;
			}
			gpio_set_value(in_gpio[i].gpio, in_gpio[i].value);
		}
	} else {
		for (i = num_gpio-1; i >= 0; i--) {
			DEV_DBG("%pS->%s: %s disable\n",
				__builtin_return_address(0), __func__,
				in_gpio[i].gpio_name);
			if (in_gpio[i].gpio)
				gpio_free(in_gpio[i].gpio);
		}
	}
	return rc;

disable_gpio:
	for (i--; i >= 0; i--)
		if (in_gpio[i].gpio)
			gpio_free(in_gpio[i].gpio);

	return rc;
} /* msm_dss_enable_gpio */
EXPORT_SYMBOL(msm_dss_enable_gpio);

void msm_dss_put_clk(struct dss_clk *clk_arry, int num_clk)
{
	int i;

	for (i = num_clk - 1; i >= 0; i--) {
		if (clk_arry[i].clk)
			clk_put(clk_arry[i].clk);
		clk_arry[i].clk = NULL;
	}
} /* msm_dss_put_clk */
EXPORT_SYMBOL(msm_dss_put_clk);

int msm_dss_get_clk(struct device *dev, struct dss_clk *clk_arry, int num_clk)
{
	int i, rc = 0;

	for (i = 0; i < num_clk; i++) {
		clk_arry[i].clk = clk_get(dev, clk_arry[i].clk_name);
		rc = PTR_ERR_OR_ZERO(clk_arry[i].clk);
		if (rc) {
			DEV_ERR("%pS->%s: '%s' get failed. rc=%d\n",
				__builtin_return_address(0), __func__,
				clk_arry[i].clk_name, rc);
			goto error;
		}
	}

	return rc;

error:
	for (i--; i >= 0; i--) {
		if (clk_arry[i].clk)
			clk_put(clk_arry[i].clk);
		clk_arry[i].clk = NULL;
	}

	return rc;
} /* msm_dss_get_clk */
EXPORT_SYMBOL(msm_dss_get_clk);

int msm_dss_mmrm_register(struct device *dev, struct dss_module_power *mp,
	int (*cb_fnc)(struct mmrm_client_notifier_data *data), void *phandle,
	bool *mmrm_enable)
{
	int i, rc = 0;
	struct dss_clk *clk_array = mp->clk_config;
	int num_clk = mp->num_clk;
	*mmrm_enable = false;

	for (i = 0; i < num_clk; i++) {
		struct mmrm_client_desc desc;
		char *name = (char *)desc.client_info.desc.name;
		struct dss_clk_mmrm_cb *mmrm_cb_data;

		if (clk_array[i].type != DSS_CLK_MMRM)
			continue;

		desc.client_type = MMRM_CLIENT_CLOCK;
		desc.client_info.desc.client_domain =
			MMRM_CLIENT_DOMAIN_DISPLAY;
		desc.client_info.desc.client_id =
			clk_array[i].mmrm.clk_id;
		strlcpy(name, clk_array[i].clk_name,
			sizeof(desc.client_info.desc.name));
		desc.client_info.desc.clk = clk_array[i].clk;
		desc.priority = MMRM_CLIENT_PRIOR_LOW;

		/* init callback wait queue */
		init_waitqueue_head(&clk_array[i].mmrm.mmrm_cb_wq);

		/* register the callback */
		mmrm_cb_data = kzalloc(sizeof(*mmrm_cb_data), GFP_KERNEL);
		if (!mmrm_cb_data)
			return -ENOMEM;

		mmrm_cb_data->phandle = phandle;
		mmrm_cb_data->clk = &clk_array[i];
		clk_array[i].mmrm.mmrm_cb_data = mmrm_cb_data;

		desc.pvt_data = (void *)mmrm_cb_data;
		desc.notifier_callback_fn = cb_fnc;

		clk_array[i].mmrm.mmrm_client = mmrm_client_register(&desc);
		if (!clk_array[i].mmrm.mmrm_client) {
			DEV_ERR("mmrm register error\n");
			DEV_ERR("clk[%d] type:%d id:%d name:%s\n",
				i, desc.client_type,
				desc.client_info.desc.client_id,
				desc.client_info.desc.name);

			rc = -EINVAL;
		} else {
			*mmrm_enable = true;
			DEV_DBG("mmrm register id:%d name=%s prio:%d\n",
				desc.client_info.desc.client_id,
				desc.client_info.desc.name,
				desc.priority);
		}
	}

	return rc;
} /* msm_dss_mmrm_register */
EXPORT_SYMBOL(msm_dss_mmrm_register);

void msm_dss_mmrm_deregister(struct device *dev,
	struct dss_module_power *mp)
{
	int i, ret;
	struct dss_clk *clk_array = mp->clk_config;
	int num_clk = mp->num_clk;

	for (i = 0; i < num_clk; i++) {
		if (clk_array[i].type != DSS_CLK_MMRM)
			continue;

		ret = mmrm_client_deregister(
			clk_array[i].mmrm.mmrm_client);
		if (ret) {
			DEV_DBG("fail mmrm deregister ret:%d clk:%s\n",
				ret, clk_array[i].clk_name);
			continue;
		}

		kfree(clk_array[i].mmrm.mmrm_cb_data);

		DEV_DBG("msm dss mmrm deregister clk[%d] name=%s\n",
			i, clk_array[i].clk_name);

	}
} /* msm_dss_mmrm_deregister */
EXPORT_SYMBOL(msm_dss_mmrm_deregister);

int msm_dss_single_clk_set_rate(struct dss_clk *clk)
{
	int rc = 0;

	if (!clk) {
		DEV_ERR("invalid clk struct\n");
		return -EINVAL;
	}

	DEV_DBG("%pS->%s: set_rate '%s'\n",
			__builtin_return_address(0), __func__,
			clk->clk_name);

	/* When MMRM enabled, avoid setting the rate for the branch clock,
	 * MMRM is always expecting the vote from the SRC clock only
	 */
	if (!strcmp(clk->clk_name, "branch_clk"))
		return 0;

	if (clk->type != DSS_CLK_AHB &&
	    clk->type != DSS_CLK_MMRM &&
	    !clk->mmrm.flags) {
		rc = clk_set_rate(clk->clk, clk->rate);
		if (rc)
			DEV_ERR("%pS->%s: %s failed. rc=%d\n",
					__builtin_return_address(0),
					__func__,
					clk->clk_name, rc);
	} else if (clk->type == DSS_CLK_MMRM) {
		struct mmrm_client_data client_data;

		memset(&client_data, 0, sizeof(client_data));
		client_data.num_hw_blocks = 1;
		client_data.flags = clk->mmrm.flags;

		rc = mmrm_client_set_value(
			clk->mmrm.mmrm_client,
			&client_data,
			clk->rate);
		if (rc) {
			DEV_ERR("%pS->%s: %s mmrm setval fail rc:%d\n",
				__builtin_return_address(0),
				__func__,
				clk->clk_name, rc);
		} else if (clk->mmrm.mmrm_requested_clk &&
			(clk->rate <= clk->mmrm.mmrm_requested_clk)) {

			/* notify any pending clk request from mmrm cb,
			 * new clk must be less or equal than callback
			 * request, set requested clock to zero to
			 * succeed mmrm callback
			 */
			clk->mmrm.mmrm_requested_clk = 0;

			/* notify callback */
			wake_up_all(&clk->mmrm.mmrm_cb_wq);
		}
	}

	return rc;
} /* msm_dss_single_clk_set_rate */
EXPORT_SYMBOL(msm_dss_single_clk_set_rate);

int msm_dss_clk_set_rate(struct dss_clk *clk_arry, int num_clk)
{
	int i, rc = 0;

	for (i = 0; i < num_clk; i++) {
		if (clk_arry[i].clk) {
			rc = msm_dss_single_clk_set_rate(&clk_arry[i]);
			if (rc)
				break;
		} else {
			DEV_ERR("%pS->%s: '%s' is not available\n",
				__builtin_return_address(0), __func__,
				clk_arry[i].clk_name);
			rc = -EPERM;
			break;
		}
	}

	return rc;
} /* msm_dss_clk_set_rate */
EXPORT_SYMBOL(msm_dss_clk_set_rate);

int msm_dss_enable_clk(struct dss_clk *clk_arry, int num_clk, int enable)
{
	int i, rc = 0;

	if (enable) {
		for (i = 0; i < num_clk; i++) {
			DEV_DBG("%pS->%s: enable '%s'\n",
				__builtin_return_address(0), __func__,
				clk_arry[i].clk_name);
			if (clk_arry[i].clk) {
				rc = clk_prepare_enable(clk_arry[i].clk);
				if (rc)
					DEV_ERR("%pS->%s: %s en fail. rc=%d\n",
						__builtin_return_address(0),
						__func__,
						clk_arry[i].clk_name, rc);
			} else {
				DEV_ERR("%pS->%s: '%s' is not available\n",
					__builtin_return_address(0), __func__,
					clk_arry[i].clk_name);
				rc = -EPERM;
			}

			if (rc) {
				msm_dss_enable_clk(clk_arry, i, false);
				break;
			}
		}
	} else {
		for (i = num_clk - 1; i >= 0; i--) {
			DEV_DBG("%pS->%s: disable '%s'\n",
				__builtin_return_address(0), __func__,
				clk_arry[i].clk_name);

			if (clk_arry[i].clk)
				clk_disable_unprepare(clk_arry[i].clk);
			else
				DEV_ERR("%pS->%s: '%s' is not available\n",
					__builtin_return_address(0), __func__,
					clk_arry[i].clk_name);
		}
	}

	return rc;
} /* msm_dss_enable_clk */
EXPORT_SYMBOL(msm_dss_enable_clk);


int sde_i2c_byte_read(struct i2c_client *client, uint8_t slave_addr,
			uint8_t reg_offset, uint8_t *read_buf)
{
	struct i2c_msg msgs[2];
	int ret = -1;

	pr_debug("%s: reading from slave_addr=[%x] and offset=[%x]\n",
		 __func__, slave_addr, reg_offset);

	msgs[0].addr = slave_addr >> 1;
	msgs[0].flags = 0;
	msgs[0].buf = &reg_offset;
	msgs[0].len = 1;

	msgs[1].addr = slave_addr >> 1;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = read_buf;
	msgs[1].len = 1;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 1) {
		pr_err("%s: I2C READ FAILED=[%d]\n", __func__, ret);
		return -EACCES;
	}
	pr_debug("%s: i2c buf is [%x]\n", __func__, *read_buf);
	return 0;
}
EXPORT_SYMBOL(sde_i2c_byte_read);

int sde_i2c_byte_write(struct i2c_client *client, uint8_t slave_addr,
			uint8_t reg_offset, uint8_t *value)
{
	struct i2c_msg msgs[1];
	uint8_t data[2];
	int status = -EACCES;

	pr_debug("%s: writing from slave_addr=[%x] and offset=[%x]\n",
		 __func__, slave_addr, reg_offset);

	data[0] = reg_offset;
	data[1] = *value;

	msgs[0].addr = slave_addr >> 1;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = data;

	status = i2c_transfer(client->adapter, msgs, 1);
	if (status < 1) {
		pr_err("I2C WRITE FAILED=[%d]\n", status);
		return -EACCES;
	}
	pr_debug("%s: I2C write status=%x\n", __func__, status);
	return status;
}
EXPORT_SYMBOL(sde_i2c_byte_write);
