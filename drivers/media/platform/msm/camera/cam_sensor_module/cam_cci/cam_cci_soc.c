/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "cam_cci_dev.h"
#include "cam_cci_core.h"

static int32_t cam_cci_pinctrl_init(struct cci_device *cci_dev)
{
	struct msm_pinctrl_info *cci_pctrl = NULL;

	cci_pctrl = &cci_dev->cci_pinctrl;
	cci_pctrl->pinctrl = devm_pinctrl_get(&cci_dev->v4l2_dev_str.pdev->dev);
	if (IS_ERR_OR_NULL(cci_pctrl->pinctrl)) {
		pr_err("%s:%d devm_pinctrl_get cci_pinctrl failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	cci_pctrl->gpio_state_active = pinctrl_lookup_state(
						cci_pctrl->pinctrl,
						CCI_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(cci_pctrl->gpio_state_active)) {
		pr_err("%s:%d look up state  for active state failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	cci_pctrl->gpio_state_suspend = pinctrl_lookup_state(
						cci_pctrl->pinctrl,
						CCI_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(cci_pctrl->gpio_state_suspend)) {
		pr_err("%s:%d look up state for suspend state failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

int cam_cci_init(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl)
{
	uint8_t i = 0, j = 0;
	int32_t rc = 0, ret = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master = MASTER_0;
	uint32_t *clk_rates  = NULL;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev || !c_ctrl) {
		pr_err("%s:%d failed: invalid params %pK %pK\n", __func__,
			__LINE__, cci_dev, c_ctrl);
		rc = -EINVAL;
		return rc;
	}

	if (cci_dev->ref_count++) {
		CDBG("%s ref_count %d\n", __func__, cci_dev->ref_count);
		master = c_ctrl->cci_info->cci_i2c_master;
		CDBG("%s:%d master %d\n", __func__, __LINE__, master);
		if (master < MASTER_MAX && master >= 0) {
			mutex_lock(&cci_dev->cci_master_info[master].mutex);
			flush_workqueue(cci_dev->write_wq[master]);
			/* Re-initialize the completion */
			reinit_completion(&cci_dev->
				cci_master_info[master].reset_complete);
			for (i = 0; i < NUM_QUEUES; i++)
				reinit_completion(&cci_dev->
					cci_master_info[master].report_q[i]);
			/* Set reset pending flag to TRUE */
			cci_dev->cci_master_info[master].reset_pending = TRUE;
			/* Set proper mask to RESET CMD address */
			if (master == MASTER_0)
				cam_io_w_mb(CCI_M0_RESET_RMSK,
					cci_dev->base + CCI_RESET_CMD_ADDR);
			else
				cam_io_w_mb(CCI_M1_RESET_RMSK,
					cci_dev->base + CCI_RESET_CMD_ADDR);
			/* wait for reset done irq */
			rc = wait_for_completion_timeout(
				&cci_dev->cci_master_info[master].
				reset_complete,
				CCI_TIMEOUT);
			if (rc <= 0)
				pr_err("%s:%d wait failed %d\n", __func__,
					__LINE__, rc);
			mutex_unlock(&cci_dev->cci_master_info[master].mutex);
		}
		return 0;
	}

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.compressed_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.uncompressed_bw = CAM_CPAS_DEFAULT_AXI_BW;

	rc = cam_cpas_start(cci_dev->cpas_handle,
		&ahb_vote, &axi_vote);
	if (rc != 0) {
		pr_err("%s:%d CPAS start failed\n",
			__func__, __LINE__);
	}

	ret = cam_cci_pinctrl_init(cci_dev);
	if (ret < 0) {
		pr_err("%s:%d Initialization of pinctrl failed\n",
				__func__, __LINE__);
		cci_dev->cci_pinctrl_status = 0;
	} else {
		cci_dev->cci_pinctrl_status = 1;
	}
	rc = msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 1);
	if (cci_dev->cci_pinctrl_status) {
		ret = pinctrl_select_state(cci_dev->cci_pinctrl.pinctrl,
				cci_dev->cci_pinctrl.gpio_state_active);
		if (ret)
			pr_err("%s:%d cannot set pin to active state\n",
				__func__, __LINE__);
	}
	if (rc < 0) {
		CDBG("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_vreg, cci_dev->regulator_count, NULL, 0,
		&cci_dev->cci_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d cci config_vreg failed\n", __func__, __LINE__);
		goto clk_enable_failed;
	}

	rc = msm_camera_enable_vreg(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_vreg, cci_dev->regulator_count, NULL, 0,
		&cci_dev->cci_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d cci enable_vreg failed\n", __func__, __LINE__);
		goto reg_enable_failed;
	}

	clk_rates = cam_cci_get_clk_rates(cci_dev, c_ctrl);
	if (!clk_rates) {
		pr_err("%s: clk enable failed\n", __func__);
		goto reg_enable_failed;
	}

	for (i = 0; i < cci_dev->num_clk; i++) {
		cci_dev->cci_clk_info[i].clk_rate =
			clk_rates[i];
	}
	rc = msm_camera_clk_enable(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_clk_info, cci_dev->cci_clk,
		cci_dev->num_clk, true);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto reg_enable_failed;
	}

	/* Re-initialize the completion */
	reinit_completion(&cci_dev->cci_master_info[master].reset_complete);
	for (i = 0; i < NUM_QUEUES; i++)
		reinit_completion(&cci_dev->cci_master_info[master].
			report_q[i]);
	rc = msm_camera_enable_irq(cci_dev->irq, true);
	if (rc < 0) {
		pr_err("%s: irq enable failed\n", __func__);
		return -EINVAL;
	}
	cci_dev->hw_version = cam_io_r_mb(cci_dev->base +
		CCI_HW_VERSION_ADDR);
	CDBG("%s:%d: hw_version = 0x%x\n", __func__, __LINE__,
		cci_dev->hw_version);

	cci_dev->payload_size =
		MSM_CCI_WRITE_DATA_PAYLOAD_SIZE_11;
	cci_dev->support_seq_write = 1;

	for (i = 0; i < NUM_MASTERS; i++) {
		for (j = 0; j < NUM_QUEUES; j++) {
			if (j == QUEUE_0)
				cci_dev->cci_i2c_queue_info[i][j].
					max_queue_size =
						CCI_I2C_QUEUE_0_SIZE;
			else
				cci_dev->cci_i2c_queue_info[i][j].
					max_queue_size =
						CCI_I2C_QUEUE_1_SIZE;

			CDBG("CCI Master[%d] :: Q0 size: %d Q1 size: %d\n", i,
				cci_dev->cci_i2c_queue_info[i][j].
				max_queue_size,
				cci_dev->cci_i2c_queue_info[i][j].
				max_queue_size);
		}
	}

	cci_dev->cci_master_info[MASTER_0].reset_pending = TRUE;
	cam_io_w_mb(CCI_RESET_CMD_RMSK, cci_dev->base +
			CCI_RESET_CMD_ADDR);
	cam_io_w_mb(0x1, cci_dev->base + CCI_RESET_CMD_ADDR);
	rc = wait_for_completion_timeout(
		&cci_dev->cci_master_info[MASTER_0].reset_complete,
		CCI_TIMEOUT);
	if (rc <= 0) {
		pr_err("%s: wait_for_completion_timeout %d\n",
			 __func__, __LINE__);
		if (rc == 0)
			rc = -ETIMEDOUT;
		goto reset_complete_failed;
	}
	for (i = 0; i < MASTER_MAX; i++)
		cci_dev->i2c_freq_mode[i] = I2C_MAX_MODES;
	cam_io_w_mb(CCI_IRQ_MASK_0_RMSK,
		cci_dev->base + CCI_IRQ_MASK_0_ADDR);
	cam_io_w_mb(CCI_IRQ_MASK_0_RMSK,
		cci_dev->base + CCI_IRQ_CLEAR_0_ADDR);
	cam_io_w_mb(0x1, cci_dev->base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	for (i = 0; i < MASTER_MAX; i++) {
		if (!cci_dev->write_wq[i]) {
			pr_err("Failed to flush write wq\n");
			rc = -ENOMEM;
			goto reset_complete_failed;
		} else {
			flush_workqueue(cci_dev->write_wq[i]);
		}
	}
	cci_dev->cci_state = CCI_STATE_ENABLED;

	return 0;

reset_complete_failed:
	msm_camera_enable_irq(cci_dev->irq, false);
	msm_camera_clk_enable(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_clk_info, cci_dev->cci_clk,
		cci_dev->num_clk, false);
reg_enable_failed:
	msm_camera_config_vreg(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_vreg, cci_dev->regulator_count, NULL, 0,
		&cci_dev->cci_reg_ptr[0], 0);
clk_enable_failed:
	if (cci_dev->cci_pinctrl_status) {
		ret = pinctrl_select_state(cci_dev->cci_pinctrl.pinctrl,
				cci_dev->cci_pinctrl.gpio_state_suspend);
		if (ret)
			pr_err("%s:%d cannot set pin to suspend state\n",
				__func__, __LINE__);
	}
	msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 0);
request_gpio_failed:
	cci_dev->ref_count--;
	cam_cpas_stop(cci_dev->cpas_handle);

	return rc;
}

void cam_cci_soc_remove(struct platform_device *pdev,
	struct cci_device *cci_dev)
{
	msm_camera_put_clk_info_and_rates(pdev,
		&cci_dev->cci_clk_info, &cci_dev->cci_clk,
		&cci_dev->cci_clk_rates, cci_dev->num_clk_cases,
		cci_dev->num_clk);

	msm_camera_put_reg_base(pdev, cci_dev->base, "cci", true);
}

static void cam_cci_init_cci_params(struct cci_device *new_cci_dev)
{
	uint8_t i = 0, j = 0;

	for (i = 0; i < NUM_MASTERS; i++) {
		new_cci_dev->cci_master_info[i].status = 0;
		mutex_init(&new_cci_dev->cci_master_info[i].mutex);
		init_completion(&new_cci_dev->
			cci_master_info[i].reset_complete);

		for (j = 0; j < NUM_QUEUES; j++) {
			mutex_init(&new_cci_dev->cci_master_info[i].mutex_q[j]);
			init_completion(&new_cci_dev->
				cci_master_info[i].report_q[j]);
			}
	}
}

static int32_t cam_cci_init_gpio_params(struct cci_device *cci_dev)
{
	int32_t rc = 0, i = 0;
	uint32_t *val_array = NULL;
	uint8_t tbl_size = 0;
	struct device_node *of_node = cci_dev->v4l2_dev_str.pdev->dev.of_node;
	struct gpio *gpio_tbl = NULL;

	cci_dev->cci_gpio_tbl_size = tbl_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, tbl_size);
	if (!tbl_size) {
		pr_err("%s:%d gpio count 0\n", __func__, __LINE__);
		return -EINVAL;
	}

	gpio_tbl = cci_dev->cci_gpio_tbl =
		kzalloc(sizeof(struct gpio) * tbl_size, GFP_KERNEL);
	if (!gpio_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < tbl_size; i++) {
		gpio_tbl[i].gpio = of_get_gpio(of_node, i);
		CDBG("%s gpio_tbl[%d].gpio = %d\n", __func__, i,
			gpio_tbl[i].gpio);
	}

	val_array = kcalloc(tbl_size, sizeof(uint32_t),
		GFP_KERNEL);
	if (!val_array) {
		rc = -ENOMEM;
		goto free_gpio_tbl;
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-tbl-flags",
		val_array, tbl_size);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_val_array;
	}
	for (i = 0; i < tbl_size; i++) {
		gpio_tbl[i].flags = val_array[i];
		CDBG("%s gpio_tbl[%d].flags = %ld\n", __func__, i,
			gpio_tbl[i].flags);
	}

	for (i = 0; i < tbl_size; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,gpio-tbl-label", i, &gpio_tbl[i].label);
		CDBG("%s gpio_tbl[%d].label = %s\n", __func__, i,
			gpio_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_val_array;
		}
	}

	kfree(val_array);
	return rc;

free_val_array:
	kfree(val_array);
free_gpio_tbl:
	kfree(cci_dev->cci_gpio_tbl);
	cci_dev->cci_gpio_tbl = NULL;
	cci_dev->cci_gpio_tbl_size = 0;
	return rc;
}

static void cam_cci_init_default_clk_params(struct cci_device *cci_dev,
	uint8_t index)
{
	/* default clock params are for 100Khz */
	cci_dev->cci_clk_params[index].hw_thigh = 201;
	cci_dev->cci_clk_params[index].hw_tlow = 174;
	cci_dev->cci_clk_params[index].hw_tsu_sto = 204;
	cci_dev->cci_clk_params[index].hw_tsu_sta = 231;
	cci_dev->cci_clk_params[index].hw_thd_dat = 22;
	cci_dev->cci_clk_params[index].hw_thd_sta = 162;
	cci_dev->cci_clk_params[index].hw_tbuf = 227;
	cci_dev->cci_clk_params[index].hw_scl_stretch_en = 0;
	cci_dev->cci_clk_params[index].hw_trdhld = 6;
	cci_dev->cci_clk_params[index].hw_tsp = 3;
	cci_dev->cci_clk_params[index].cci_clk_src = 37500000;
}

static void cam_cci_init_clk_params(struct cci_device *cci_dev)
{
	int32_t rc = 0;
	uint32_t val = 0;
	uint8_t count = 0;
	struct device_node *of_node = cci_dev->v4l2_dev_str.pdev->dev.of_node;
	struct device_node *src_node = NULL;

	for (count = 0; count < I2C_MAX_MODES; count++) {

		if (count == I2C_STANDARD_MODE)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_standard_mode");
		else if (count == I2C_FAST_MODE)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_fast_mode");
		else if (count == I2C_FAST_PLUS_MODE)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_fast_plus_mode");
		else
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_custom_mode");

		rc = of_property_read_u32(src_node, "qcom,hw-thigh", &val);
		CDBG("%s qcom,hw-thigh %d, rc %d\n", __func__, val, rc);
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thigh = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tlow",
				&val);
			CDBG("%s qcom,hw-tlow %d, rc %d\n", __func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tlow = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tsu-sto",
				&val);
			CDBG("%s qcom,hw-tsu-sto %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsu_sto = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tsu-sta",
				&val);
			CDBG("%s qcom,hw-tsu-sta %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsu_sta = val;
			rc = of_property_read_u32(src_node, "qcom,hw-thd-dat",
				&val);
			CDBG("%s qcom,hw-thd-dat %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thd_dat = val;
			rc = of_property_read_u32(src_node, "qcom,hw-thd-sta",
				&val);
			CDBG("%s qcom,hw-thd-sta %d, rc %d\n", __func__,
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thd_sta = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tbuf",
				&val);
			CDBG("%s qcom,hw-tbuf %d, rc %d\n", __func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tbuf = val;
			rc = of_property_read_u32(src_node,
				"qcom,hw-scl-stretch-en", &val);
			CDBG("%s qcom,hw-scl-stretch-en %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_scl_stretch_en = val;
			rc = of_property_read_u32(src_node, "qcom,hw-trdhld",
				&val);
			CDBG("%s qcom,hw-trdhld %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_trdhld = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tsp",
				&val);
			CDBG("%s qcom,hw-tsp %d, rc %d\n", __func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsp = val;
			val = 0;
			rc = of_property_read_u32(src_node, "qcom,cci-clk-src",
				&val);
			CDBG("%s qcom,cci-clk-src %d, rc %d\n",
				__func__, val, rc);
			cci_dev->cci_clk_params[count].cci_clk_src = val;
		} else
			cam_cci_init_default_clk_params(cci_dev, count);

		of_node_put(src_node);
	}
}

int cam_cci_parse_dt_info(struct platform_device *pdev,
	struct cci_device *new_cci_dev)
{
	int rc = 0, i = 0;

	/* Get Clock Info*/
	rc = msm_camera_get_clk_info_and_rates(pdev,
		&new_cci_dev->cci_clk_info, &new_cci_dev->cci_clk,
		&new_cci_dev->cci_clk_rates, &new_cci_dev->num_clk_cases,
		&new_cci_dev->num_clk);
	if (rc < 0) {
		pr_err("%s: cam_cci_get_clk_info() failed", __func__);
		kfree(new_cci_dev);
		new_cci_dev = NULL;
		return -EFAULT;
	}

	new_cci_dev->ref_count = 0;
	new_cci_dev->base = msm_camera_get_reg_base(pdev, "cci", true);
	if (!new_cci_dev->base) {
		pr_err("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}
	new_cci_dev->irq = msm_camera_get_irq(pdev, "cci");
	if (!new_cci_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		return -ENODEV;
	}
	CDBG("%s line %d cci irq start %d end %d\n", __func__,
		__LINE__,
		(int) new_cci_dev->irq->start,
		(int) new_cci_dev->irq->end);
	rc = msm_camera_register_irq(pdev, new_cci_dev->irq,
		cam_cci_irq, IRQF_TRIGGER_RISING, "cci", new_cci_dev);
	if (rc < 0) {
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto cci_release_mem;
	}

	msm_camera_enable_irq(new_cci_dev->irq, false);
	new_cci_dev->v4l2_dev_str.pdev = pdev;
	cam_cci_init_cci_params(new_cci_dev);
	cam_cci_init_clk_params(new_cci_dev);
	rc = cam_cci_init_gpio_params(new_cci_dev);
	if (rc < 0) {
		pr_err("%s:%d :Error: In Initializing GPIO params:%d\n",
			__func__, __LINE__, rc);
		goto cci_release_mem;
	}

	rc = cam_sensor_get_dt_vreg_data(new_cci_dev->
		v4l2_dev_str.pdev->dev.of_node,
		&(new_cci_dev->cci_vreg), &(new_cci_dev->regulator_count));
	if (rc < 0) {
		pr_err("%s: cam_sensor_get_dt_vreg_data fail\n", __func__);
		rc = -EFAULT;
		goto cci_release_mem;
	}

	/* Parse VREG data */
	if ((new_cci_dev->regulator_count < 0) ||
		(new_cci_dev->regulator_count > MAX_REGULATOR)) {
		pr_err("%s: invalid reg count = %d, max is %d\n", __func__,
			new_cci_dev->regulator_count, MAX_REGULATOR);
		rc = -EFAULT;
		goto cci_invalid_vreg_data;
	}

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc)
		pr_err("%s: failed to add child nodes, rc=%d\n", __func__, rc);
	for (i = 0; i < MASTER_MAX; i++) {
		new_cci_dev->write_wq[i] = create_singlethread_workqueue(
			"cam_cci_wq");
		if (!new_cci_dev->write_wq[i])
			pr_err("Failed to create write wq\n");
	}
	CDBG("%s line %d\n", __func__, __LINE__);
	return 0;

cci_invalid_vreg_data:
	kfree(new_cci_dev->cci_vreg);
	new_cci_dev->cci_vreg = NULL;
cci_release_mem:
	msm_camera_put_reg_base(pdev, new_cci_dev->base, "cci", true);

	return rc;
}

int cam_cci_soc_release(struct cci_device *cci_dev)
{
	uint8_t i = 0, rc = 0;

	if (!cci_dev->ref_count || cci_dev->cci_state != CCI_STATE_ENABLED) {
		pr_err("%s invalid ref count %d / cci state %d\n",
			__func__, cci_dev->ref_count, cci_dev->cci_state);
		return -EINVAL;
	}
	if (--cci_dev->ref_count) {
		CDBG("%s ref_count Exit %d\n", __func__, cci_dev->ref_count);
		return 0;
	}
	for (i = 0; i < MASTER_MAX; i++)
		if (cci_dev->write_wq[i])
			flush_workqueue(cci_dev->write_wq[i]);

	msm_camera_enable_irq(cci_dev->irq, false);
	msm_camera_clk_enable(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_clk_info, cci_dev->cci_clk,
		cci_dev->num_clk, false);

	rc = msm_camera_enable_vreg(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_vreg, cci_dev->regulator_count, NULL, 0,
		&cci_dev->cci_reg_ptr[0], 0);
	if (rc < 0)
		pr_err("%s:%d cci disable_vreg failed\n", __func__, __LINE__);

	rc = msm_camera_config_vreg(&cci_dev->v4l2_dev_str.pdev->dev,
		cci_dev->cci_vreg, cci_dev->regulator_count, NULL, 0,
		&cci_dev->cci_reg_ptr[0], 0);
	if (rc < 0)
		pr_err("%s:%d cci unconfig_vreg failed\n", __func__, __LINE__);

	if (cci_dev->cci_pinctrl_status) {
		rc = pinctrl_select_state(cci_dev->cci_pinctrl.pinctrl,
				cci_dev->cci_pinctrl.gpio_state_suspend);
		if (rc)
			pr_err("%s:%d cannot set pin to active state\n",
				__func__, __LINE__);
	}
	cci_dev->cci_pinctrl_status = 0;
	msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 0);
	for (i = 0; i < MASTER_MAX; i++)
		cci_dev->i2c_freq_mode[i] = I2C_MAX_MODES;
	cci_dev->cci_state = CCI_STATE_DISABLED;
	cci_dev->cycles_per_us = 0;
	cci_dev->cci_clk_src = 0;

	return rc;
}
