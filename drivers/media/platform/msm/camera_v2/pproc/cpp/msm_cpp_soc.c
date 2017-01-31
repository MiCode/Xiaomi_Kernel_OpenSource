/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "MSM-CPP-SOC %s:%d " fmt, __func__, __LINE__

#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/delay.h>
#include <media/msmb_pproc.h>
#include "msm_cpp.h"


#define CPP_DT_READ_U32_ERR(_dev, _key, _str, _ret, _out) { \
		_key = _str; \
		_ret = of_property_read_u32(_dev, _key, &_out); \
		if (_ret) \
			break; \
	}

#define CPP_DT_READ_U32(_dev, _str, _out) { \
		of_property_read_u32(_dev, _str, &_out); \
	}

void msm_cpp_fetch_dt_params(struct cpp_device *cpp_dev)
{
	int rc = 0;
	struct device_node *of_node = cpp_dev->pdev->dev.of_node;

	if (!of_node) {
		pr_err("%s: invalid params\n", __func__);
		return;
	}

	of_property_read_u32(of_node, "cell-index", &cpp_dev->pdev->id);

	rc = of_property_read_u32(of_node, "qcom,min-clock-rate",
			&cpp_dev->min_clk_rate);
	if (rc < 0) {
		pr_debug("min-clk-rate not defined, setting it to 0\n");
		cpp_dev->min_clk_rate = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,bus-master",
			&cpp_dev->bus_master_flag);
	if (rc)
		cpp_dev->bus_master_flag = 0;

	if (of_property_read_bool(of_node, "qcom,micro-reset"))
		cpp_dev->micro_reset = 1;
	else
		cpp_dev->micro_reset = 0;
}

int msm_cpp_get_clock_index(struct cpp_device *cpp_dev, const char *clk_name)
{
	uint32_t i = 0;

	for (i = 0; i < cpp_dev->num_clks; i++) {
		if (!strcmp(clk_name, cpp_dev->clk_info[i].clk_name))
			return i;
	}
	return -EINVAL;
}

static int cpp_get_clk_freq_tbl_dt(struct cpp_device *cpp_dev)
{
	uint32_t i, count, min_clk_rate;
	uint32_t idx = 0;
	struct device_node *of_node;
	uint32_t *rates;
	int32_t rc = 0;
	struct cpp_hw_info *hw_info;

	if (cpp_dev == NULL) {
		pr_err("Bad parameter\n");
		rc = -EINVAL;
		goto err;
	}

	of_node = cpp_dev->pdev->dev.of_node;
	min_clk_rate = cpp_dev->min_clk_rate;
	hw_info = &cpp_dev->hw_info;

	if ((hw_info == NULL) || (of_node == NULL)) {
		pr_err("Invalid hw_info %pK or ofnode %pK\n", hw_info, of_node);
		rc = -EINVAL;
		goto err;

	}
	count = of_property_count_u32_elems(of_node, "qcom,src-clock-rates");
	if ((count == 0) || (count > MAX_FREQ_TBL)) {
		pr_err("Clock count is invalid\n");
		rc = -EINVAL;
		goto err;
	}

	rates = devm_kcalloc(&cpp_dev->pdev->dev, count, sizeof(uint32_t),
		GFP_KERNEL);
	if (!rates) {
		rc = -ENOMEM;
		goto err;
	}

	rc = of_property_read_u32_array(of_node, "qcom,src-clock-rates",
		rates, count);
	if (rc) {
		rc = -EINVAL;
		goto mem_free;
	}

	for (i = 0; i < count; i++) {
		pr_debug("entry=%d\n", rates[i]);
		if (rates[i] >= 0) {
			if (rates[i] >= min_clk_rate) {
				hw_info->freq_tbl[idx++] = rates[i];
				pr_debug("tbl[%d]=%d\n", idx-1, rates[i]);
			}
		} else {
			pr_debug("rate is invalid entry/end %d\n", rates[i]);
			break;
		}
	}

	pr_debug("%s: idx %d\n", __func__, idx);
	hw_info->freq_tbl_count = idx;

mem_free:
	devm_kfree(&cpp_dev->pdev->dev, rates);
err:
	return rc;
}

int msm_cpp_set_micro_clk(struct cpp_device *cpp_dev)
{
	int rc;

	rc = reset_control_assert(cpp_dev->micro_iface_reset);
	if (rc) {
		pr_err("%s:micro_iface_reset assert failed\n",
		__func__);
		return -EINVAL;
	}

	/*
	 * Below usleep values are chosen based on experiments
	 * and this was the smallest number which works. This
	 * sleep is needed to leave enough time for Microcontroller
	 * to resets all its registers.
	 */
	usleep_range(1000, 1200);

	rc = reset_control_deassert(cpp_dev->micro_iface_reset);
	if (rc) {
		pr_err("%s:micro_iface_reset de-assert failed\n", __func__);
		return -EINVAL;
	}

	/*
	 * Below usleep values are chosen based on experiments
	 * and this was the smallest number which works. This
	 * sleep is needed to leave enough time for Microcontroller
	 * to resets all its registers.
	 */
	usleep_range(1000, 1200);
	return 0;
}

int msm_update_freq_tbl(struct cpp_device *cpp_dev)
{
	uint32_t msm_cpp_core_clk_idx;
	int rc = 0;

	msm_cpp_core_clk_idx = msm_cpp_get_clock_index(cpp_dev, "cpp_core_clk");
	if (msm_cpp_core_clk_idx < 0)  {
		pr_err("%s: fail to get clock index\n", __func__);
		rc = msm_cpp_core_clk_idx;
		return rc;
	}
	rc = cpp_get_clk_freq_tbl_dt(cpp_dev);
	if (rc < 0)  {
		pr_err("%s: fail to get frequency table\n", __func__);
		return rc;
	}

	return rc;
}

long msm_cpp_set_core_clk(struct cpp_device *cpp_dev, long rate, int idx)
{
	long rc = 0;

	rc = msm_camera_clk_set_rate(&cpp_dev->pdev->dev,
		cpp_dev->cpp_clk[idx], rate);
	if (rc < 0) {
		pr_err("%s: fail to get frequency table\n", __func__);
		return rc;
	}

	return rc;
}

int msm_cpp_read_payload_params_from_dt(struct cpp_device *cpp_dev)
{
	struct platform_device *pdev = cpp_dev->pdev;
	struct device_node *fw_info_node = NULL, *dev_node = NULL;
	char *key = "qcom,cpp-fw-payload-info";
	struct msm_cpp_payload_params *payload_params;
	int ret = 0;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("%s: Invalid platform device/node\n", __func__);
		ret = -ENODEV;
		goto no_cpp_node;
	}

	dev_node = pdev->dev.of_node;
	fw_info_node = of_find_node_by_name(dev_node, key);
	if (!fw_info_node) {
		ret = -ENODEV;
		goto no_binding;
	}
	payload_params = &cpp_dev->payload_params;
	memset(payload_params, 0x0, sizeof(struct msm_cpp_payload_params));

	do {
		CPP_DT_READ_U32_ERR(fw_info_node, key, "qcom,stripe-base", ret,
			payload_params->stripe_base);
		CPP_DT_READ_U32_ERR(fw_info_node, key, "qcom,plane-base", ret,
			payload_params->plane_base);
		CPP_DT_READ_U32_ERR(fw_info_node, key, "qcom,stripe-size", ret,
			payload_params->stripe_size);
		CPP_DT_READ_U32_ERR(fw_info_node, key, "qcom,plane-size", ret,
			payload_params->plane_size);
		CPP_DT_READ_U32_ERR(fw_info_node, key, "qcom,fe-ptr-off", ret,
			payload_params->rd_pntr_off);
		CPP_DT_READ_U32_ERR(fw_info_node, key, "qcom,we-ptr-off", ret,
			payload_params->wr_0_pntr_off);

		CPP_DT_READ_U32(fw_info_node, "qcom,ref-fe-ptr-off",
			payload_params->rd_ref_pntr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,ref-we-ptr-off",
			payload_params->wr_ref_pntr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,we-meta-ptr-off",
			payload_params->wr_0_meta_data_wr_pntr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,fe-mmu-pf-ptr-off",
			payload_params->fe_mmu_pf_ptr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,ref-fe-mmu-pf-ptr-off",
			payload_params->ref_fe_mmu_pf_ptr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,we-mmu-pf-ptr-off",
			payload_params->we_mmu_pf_ptr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,dup-we-mmu-pf-ptr-off",
			payload_params->dup_we_mmu_pf_ptr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,ref-we-mmu-pf-ptr-off",
			payload_params->ref_we_mmu_pf_ptr_off);
		CPP_DT_READ_U32(fw_info_node, "qcom,set-group-buffer-len",
			payload_params->set_group_buffer_len);
		CPP_DT_READ_U32(fw_info_node, "qcom,dup-frame-indicator-off",
			payload_params->dup_frame_indicator_off);
	} while (0);

no_binding:
	if (ret)
		pr_err("%s: Error reading binding %s, ret %d\n",
			__func__, key, ret);
no_cpp_node:
	return ret;
}
