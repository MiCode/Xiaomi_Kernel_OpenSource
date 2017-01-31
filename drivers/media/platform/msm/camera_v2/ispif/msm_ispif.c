/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/iopoll.h>
#include <linux/compat.h>
#include <media/msmb_isp.h>
#include <linux/ratelimit.h>

#include "msm_ispif.h"
#include "msm.h"
#include "msm_sd.h"
#include "msm_camera_io_util.h"
#include "cam_hw_ops.h"
#include "cam_soc_api.h"

#ifdef CONFIG_MSM_ISPIF_V1
#include "msm_ispif_hwreg_v1.h"
#elif defined CONFIG_MSM_ISPIF_V2
#include "msm_ispif_hwreg_v2.h"
#else
#include "msm_ispif_hwreg_v3.h"
#endif

#define V4L2_IDENT_ISPIF                      50001
#define MSM_ISPIF_DRV_NAME                    "msm_ispif"

#define ISPIF_INTF_CMD_DISABLE_FRAME_BOUNDARY 0x00
#define ISPIF_INTF_CMD_ENABLE_FRAME_BOUNDARY  0x01
#define ISPIF_INTF_CMD_DISABLE_IMMEDIATELY    0x02

#define ISPIF_TIMEOUT_SLEEP_US                1000
#define ISPIF_TIMEOUT_ALL_US               1000000
#define ISPIF_SOF_DEBUG_COUNT                    5

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static int msm_ispif_clk_ahb_enable(struct ispif_device *ispif, int enable);
static int ispif_close_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);
static long msm_ispif_subdev_ioctl_unlocked(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg);

int msm_ispif_get_clk_info(struct ispif_device *ispif_dev,
	struct platform_device *pdev);

static void msm_ispif_io_dump_reg(struct ispif_device *ispif)
{
	if (!ispif->enb_dump_reg)
		return;
	msm_camera_io_dump(ispif->base, 0x250, 0);
}


static inline int msm_ispif_is_intf_valid(uint32_t csid_version,
	uint8_t intf_type)
{
	return ((csid_version <= CSID_VERSION_V22 && intf_type != VFE0) ||
		(intf_type >= VFE_MAX)) ? false : true;
}

static struct msm_cam_clk_info ispif_8626_reset_clk_info[] = {
	{"ispif_ahb_clk", NO_SET_RATE},
	{"camss_top_ahb_clk", NO_SET_RATE},
	{"csi0_ahb_clk", NO_SET_RATE},
	{"csi0_src_clk", NO_SET_RATE},
	{"csi0_phy_clk", NO_SET_RATE},
	{"csi0_clk", NO_SET_RATE},
	{"csi0_pix_clk", NO_SET_RATE},
	{"csi0_rdi_clk", NO_SET_RATE},
	{"csi1_ahb_clk", NO_SET_RATE},
	{"csi1_src_clk", NO_SET_RATE},
	{"csi1_phy_clk", NO_SET_RATE},
	{"csi1_clk", NO_SET_RATE},
	{"csi1_pix_clk", NO_SET_RATE},
	{"csi1_rdi_clk", NO_SET_RATE},
	{"camss_vfe_vfe_clk", NO_SET_RATE},
	{"camss_csi_vfe_clk", NO_SET_RATE},
};

#ifdef CONFIG_COMPAT
struct ispif_cfg_data_ext_32 {
	enum ispif_cfg_type_t cfg_type;
	compat_caddr_t data;
	uint32_t size;
};

#define VIDIOC_MSM_ISPIF_CFG_EXT_COMPAT \
	_IOWR('V', BASE_VIDIOC_PRIVATE+1, struct ispif_cfg_data_ext_32)
#endif

static void msm_ispif_get_pack_mask_from_cfg(
	struct msm_ispif_pack_cfg *pack_cfg,
	struct msm_ispif_params_entry *entry,
	uint32_t *pack_mask)
{
	int i;
	uint32_t temp;

	if (WARN_ON(!entry))
		return;

	memset(pack_mask, 0, sizeof(uint32_t) * 2);
	for (i = 0; i < entry->num_cids; i++) {
		temp = (pack_cfg[entry->cids[i]].pack_mode & 0x3)|
			(pack_cfg[entry->cids[i]].even_odd_sel & 0x1) << 2 |
			(pack_cfg[entry->cids[i]].pixel_swap_en & 0x1) << 3;
		temp = (temp & 0xF) << ((entry->cids[i] % CID8) * 4);

		if (entry->cids[i] > CID7)
			pack_mask[1] |= temp;
		else
			pack_mask[0] |= temp;
		CDBG("%s:num %d cid %d mode %d pack_mask %x %x\n",
			__func__, entry->num_cids, entry->cids[i],
			pack_cfg[i].pack_mode,
			pack_mask[0], pack_mask[1]);

	}
}

static int msm_ispif_config2(struct ispif_device *ispif,
	void *data)
{
	int rc = 0, i = 0;
	enum msm_ispif_intftype intftype;
	enum msm_ispif_vfe_intf vfe_intf;
	uint32_t pack_cfg_mask[2];
	struct msm_ispif_param_data_ext *params =
		(struct msm_ispif_param_data_ext *)data;

	if (WARN_ON(!ispif) || WARN_ON(!params))
		return -EINVAL;

	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		rc = -EPERM;
		return rc;
	}
	if (params->num > MAX_PARAM_ENTRIES) {
		pr_err("%s: invalid param entries %d\n", __func__,
			params->num);
		rc = -EINVAL;
		return rc;
	}

	for (i = 0; i < params->num; i++) {
		intftype = params->entries[i].intftype;
		vfe_intf = params->entries[i].vfe_intf;

		CDBG("%s, num %d intftype %x, vfe_intf %d, csid %d\n", __func__,
			params->num, intftype, vfe_intf,
			params->entries[i].csid);

		if ((intftype >= INTF_MAX) ||
			(vfe_intf >=  ispif->vfe_info.num_vfe) ||
			(ispif->csid_version <= CSID_VERSION_V22 &&
			(vfe_intf > VFE0))) {
			pr_err("%s: VFEID %d and CSID version %d mismatch\n",
				__func__, vfe_intf, ispif->csid_version);
			return -EINVAL;
		}

		msm_ispif_get_pack_mask_from_cfg(params->pack_cfg,
				&params->entries[i], pack_cfg_mask);
		msm_ispif_cfg_pack_mode(ispif, intftype, vfe_intf,
			pack_cfg_mask);
	}
	return rc;
}

static long msm_ispif_cmd_ext(struct v4l2_subdev *sd,
	void *arg)
{
	long rc = 0;
	struct ispif_device *ispif =
		(struct ispif_device *)v4l2_get_subdevdata(sd);
	struct ispif_cfg_data_ext pcdata;
	struct msm_ispif_param_data_ext *params = NULL;
#ifdef CONFIG_COMPAT
	struct ispif_cfg_data_ext_32 *pcdata32 =
		(struct ispif_cfg_data_ext_32 *)arg;

	if (pcdata32 == NULL) {
		pr_err("Invalid params passed from user\n");
		return -EINVAL;
	}
	pcdata.cfg_type  = pcdata32->cfg_type;
	pcdata.size = pcdata32->size;
	pcdata.data = compat_ptr(pcdata32->data);

#else
	struct ispif_cfg_data_ext *pcdata64 =
		(struct ispif_cfg_data_ext *)arg;

	if (pcdata64 == NULL) {
		pr_err("Invalid params passed from user\n");
		return -EINVAL;
	}
	pcdata.cfg_type  = pcdata64->cfg_type;
	pcdata.size = pcdata64->size;
	pcdata.data = pcdata64->data;
#endif
	if (pcdata.size != sizeof(struct msm_ispif_param_data_ext)) {
		pr_err("%s: payload size mismatch\n", __func__);
		return -EINVAL;
	}

	params = kzalloc(sizeof(struct msm_ispif_param_data_ext), GFP_KERNEL);
	if (!params) {
		CDBG("%s: params alloc failed\n", __func__);
		return -ENOMEM;
	}
	if (copy_from_user(params, (void __user *)(pcdata.data),
		pcdata.size)) {
		kfree(params);
		return -EFAULT;
	}

	mutex_lock(&ispif->mutex);
	switch (pcdata.cfg_type) {
	case ISPIF_CFG2:
		rc = msm_ispif_config2(ispif, params);
		msm_ispif_io_dump_reg(ispif);
		break;
	default:
		pr_err("%s: invalid cfg_type\n", __func__);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&ispif->mutex);
	kfree(params);
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_ispif_subdev_ioctl_compat(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	if (WARN_ON(!sd))
		return -EINVAL;

	switch (cmd) {
	case VIDIOC_MSM_ISPIF_CFG_EXT_COMPAT:
		return msm_ispif_cmd_ext(sd, arg);

	default:
		return msm_ispif_subdev_ioctl_unlocked(sd, cmd, arg);
	}
}
static long msm_ispif_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	if (is_compat_task())
		return msm_ispif_subdev_ioctl_compat(sd, cmd, arg);
	else
		return msm_ispif_subdev_ioctl_unlocked(sd, cmd, arg);
}
#else
static long msm_ispif_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	return msm_ispif_subdev_ioctl_unlocked(sd, cmd, arg);
}
#endif
static void msm_ispif_put_regulator(struct ispif_device *ispif_dev)
{
	int i;

	for (i = 0; i < ispif_dev->ispif_vdd_count; i++) {
		regulator_put(ispif_dev->ispif_vdd[i]);
		ispif_dev->ispif_vdd[i] = NULL;
	}
	for (i = 0; i < ispif_dev->vfe_vdd_count; i++) {
		regulator_put(ispif_dev->vfe_vdd[i]);
		ispif_dev->vfe_vdd[i] = NULL;
	}
}

static inline int __get_vdd(struct platform_device *pdev,
				struct regulator **reg, const char *vdd)
{
	int rc = 0;
	*reg = regulator_get(&pdev->dev, vdd);
	if (IS_ERR_OR_NULL(*reg)) {
		rc = PTR_ERR(*reg);
		rc = rc ? rc : -EINVAL;
		pr_err("%s: Regulator %s get failed %d\n", __func__, vdd, rc);
		*reg = NULL;
	}
	return rc;
}

static int msm_ispif_get_regulator_info(struct ispif_device *ispif_dev,
					struct platform_device *pdev)
{
	int rc;
	const char *vdd_name;
	struct device_node *of_node;
	int i;
	int count;

	of_node = pdev->dev.of_node;

	count = of_property_count_strings(of_node,
					"qcom,vdd-names");
	if (0 == count) {
		pr_err("%s: no regulators found\n", __func__);
		return -EINVAL;
	}

	BUG_ON(count > (ISPIF_VDD_INFO_MAX + ISPIF_VFE_VDD_INFO_MAX));
	ispif_dev->vfe_vdd_count = 0;
	ispif_dev->ispif_vdd_count = 0;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(
				of_node, "qcom,vdd-names",
				i, &vdd_name);
		if (rc < 0) {
			pr_err("%s: read property qcom,ispif-vdd-names at index %d failed\n",
				__func__, i);
			goto err;
		}
		if (strnstr(vdd_name, "vfe", strlen(vdd_name))) {
			BUG_ON(ispif_dev->vfe_vdd_count >=
				ISPIF_VFE_VDD_INFO_MAX);
			rc = __get_vdd(pdev,
				&ispif_dev->vfe_vdd[ispif_dev->vfe_vdd_count],
				vdd_name);
			if (0 == rc)
				ispif_dev->vfe_vdd_count++;
		} else {
			BUG_ON(ispif_dev->vfe_vdd_count >=
				ISPIF_VDD_INFO_MAX);
			rc = __get_vdd(pdev,
				&ispif_dev->ispif_vdd
					[ispif_dev->ispif_vdd_count],
				vdd_name);
			if (0 == rc)
				ispif_dev->ispif_vdd_count++;
		}
		if (rc)
			goto err;
	}
	return 0;
err:
	for (i = 0; i < ispif_dev->vfe_vdd_count; i++) {
		regulator_put(ispif_dev->vfe_vdd[i]);
		ispif_dev->vfe_vdd[i] = NULL;
	}
	for (i = 0; i < ispif_dev->ispif_vdd_count; i++) {
		regulator_put(ispif_dev->ispif_vdd[i]);
		ispif_dev->ispif_vdd[i] = NULL;
	}
	ispif_dev->ispif_vdd_count = 0;
	ispif_dev->vfe_vdd_count = 0;
	return rc;
}

static int msm_ispif_set_regulators(struct regulator **regs, int count,
	uint8_t enable)
{
	int rc = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (enable) {
			rc = regulator_enable(regs[i]);
			if (rc)
				goto err;
		} else {
			rc |= regulator_disable(regs[i]);
		}
	}
	if (rc)
		pr_err("%s: Regulator disable failed\n", __func__);
	return rc;
err:
	pr_err("%s: Regulator enable failed\n", __func__);
	for (i--; i >= 0; i--)
		regulator_disable(regs[i]);
	return rc;
}

static int msm_ispif_reset_hw(struct ispif_device *ispif)
{
	int rc = 0;
	long timeout = 0;
	struct clk *reset_clk1[ARRAY_SIZE(ispif_8626_reset_clk_info)];
	ispif->clk_idx = 0;

	/* Turn ON VFE regulators before enabling the vfe clocks */
	rc = msm_ispif_set_regulators(ispif->vfe_vdd, ispif->vfe_vdd_count, 1);
	if (rc < 0)
		return rc;

	rc = msm_camera_clk_enable(&ispif->pdev->dev,
		ispif->clk_info, ispif->clks,
		ispif->num_clk, 1);
	if (rc < 0) {
		pr_err("%s: cannot enable clock, error = %d\n",
			__func__, rc);
		rc = msm_camera_clk_enable(&ispif->pdev->dev,
			ispif_8626_reset_clk_info, reset_clk1,
			ARRAY_SIZE(ispif_8626_reset_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: cannot enable clock, error = %d",
				__func__, rc);
			goto reg_disable;
		} else {
			/* This is set when device is 8x26 */
			ispif->clk_idx = 2;
		}
	} else {
		/* This is set when device is 8974 */
		ispif->clk_idx = 1;
	}

	atomic_set(&ispif->reset_trig[VFE0], 1);
	/* initiate reset of ISPIF */
	msm_camera_io_w(ISPIF_RST_CMD_MASK,
				ispif->base + ISPIF_RST_CMD_ADDR);

	timeout = wait_for_completion_interruptible_timeout(
			&ispif->reset_complete[VFE0], msecs_to_jiffies(500));
	CDBG("%s: VFE0 done\n", __func__);

	if (timeout <= 0) {
		rc = -ETIMEDOUT;
		pr_err("%s: VFE0 reset wait timeout\n", __func__);
		goto clk_disable;
	}

	if (ispif->hw_num_isps > 1) {
		atomic_set(&ispif->reset_trig[VFE1], 1);
		msm_camera_io_w(ISPIF_RST_CMD_1_MASK,
					ispif->base + ISPIF_RST_CMD_1_ADDR);
		timeout = wait_for_completion_interruptible_timeout(
				&ispif->reset_complete[VFE1],
				msecs_to_jiffies(500));
		CDBG("%s: VFE1 done\n", __func__);
		if (timeout <= 0) {
			pr_err("%s: VFE1 reset wait timeout\n", __func__);
			rc = -ETIMEDOUT;
		}
	}

clk_disable:
	if (ispif->clk_idx == 1) {
		rc = rc ? rc : msm_camera_clk_enable(&ispif->pdev->dev,
			ispif->clk_info, ispif->clks,
			ispif->num_clk, 0);
	}

	if (ispif->clk_idx == 2) {
		rc = rc ? rc :  msm_camera_clk_enable(&ispif->pdev->dev,
			ispif_8626_reset_clk_info, reset_clk1,
			ARRAY_SIZE(ispif_8626_reset_clk_info), 0);
	}
reg_disable:
	rc = rc ? rc :  msm_ispif_set_regulators(ispif->vfe_vdd,
					ispif->vfe_vdd_count, 0);

	return rc;
}

int msm_ispif_get_clk_info(struct ispif_device *ispif_dev,
	struct platform_device *pdev)
{
	uint32_t num_ahb_clk = 0, non_ahb_clk = 0;
	size_t num_clks;
	int i, rc;
	int j;
	struct clk **clks, **temp_clks;
	struct msm_cam_clk_info *clk_info, *temp_clk_info;

	struct device_node *of_node;
	of_node = pdev->dev.of_node;

	rc = msm_camera_get_clk_info(pdev, &clk_info,
			&clks, &num_clks);

	if (rc)
		return rc;

	/*
	 * reshuffle the clock arrays so that the ahb clocks are
	 * at the beginning of array
	 */
	temp_clks = kcalloc(num_clks, sizeof(struct clk *),
				GFP_KERNEL);
	temp_clk_info = kcalloc(num_clks, sizeof(struct msm_cam_clk_info),
				GFP_KERNEL);
	if (!temp_clks || !temp_clk_info) {
		rc = -ENOMEM;
		kfree(temp_clk_info);
		kfree(temp_clks);
		goto alloc_fail;
	}
	j = 0;
	for (i = 0; i < num_clks; i++) {
		if (strnstr(clk_info[i].clk_name,
			"ahb", strlen(clk_info[i].clk_name))) {
			temp_clk_info[j] = clk_info[i];
			temp_clks[j] = clks[i];
			j++;
			num_ahb_clk++;
		}
	}
	for (i = 0; i < num_clks; i++) {
		if (!strnstr(clk_info[i].clk_name,
			"ahb", strlen(clk_info[i].clk_name))) {
			temp_clk_info[j] = clk_info[i];
			temp_clks[j] = clks[i];
			j++;
			non_ahb_clk++;
		}
	}

	for (i = 0; i < num_clks; i++) {
		clk_info[i] = temp_clk_info[i];
		clks[i] = temp_clks[i];
	}
	kfree(temp_clk_info);
	kfree(temp_clks);

	ispif_dev->ahb_clk = clks;
	ispif_dev->ahb_clk_info = clk_info;
	ispif_dev->num_ahb_clk = num_ahb_clk;
	ispif_dev->clk_info = clk_info + num_ahb_clk;
	ispif_dev->clks = clks + num_ahb_clk;
	ispif_dev->num_clk = non_ahb_clk;

	return 0;
alloc_fail:
	msm_camera_put_clk_info(pdev, &clk_info, &clks, num_clks);
	return rc;
}

static int msm_ispif_clk_ahb_enable(struct ispif_device *ispif, int enable)
{
	int rc = 0;

	rc = msm_cam_clk_enable(&ispif->pdev->dev,
		ispif->ahb_clk_info, ispif->ahb_clk,
		ispif->num_ahb_clk, enable);
	if (rc < 0) {
		pr_err("%s: cannot enable clock, error = %d",
			__func__, rc);
	}

	return rc;
}

static int msm_ispif_reset(struct ispif_device *ispif)
{
	int rc = 0;
	int i;

	BUG_ON(!ispif);

	memset(ispif->sof_count, 0, sizeof(ispif->sof_count));
	for (i = 0; i < ispif->vfe_info.num_vfe; i++) {

		msm_camera_io_w(1 << PIX0_LINE_BUF_EN_BIT,
			ispif->base + ISPIF_VFE_m_CTRL_0(i));
		msm_camera_io_w(0, ispif->base + ISPIF_VFE_m_IRQ_MASK_0(i));
		msm_camera_io_w(0, ispif->base + ISPIF_VFE_m_IRQ_MASK_1(i));
		msm_camera_io_w(0, ispif->base + ISPIF_VFE_m_IRQ_MASK_2(i));
		msm_camera_io_w(0xFFFFFFFF, ispif->base +
			ISPIF_VFE_m_IRQ_CLEAR_0(i));
		msm_camera_io_w(0xFFFFFFFF, ispif->base +
			ISPIF_VFE_m_IRQ_CLEAR_1(i));
		msm_camera_io_w(0xFFFFFFFF, ispif->base +
			ISPIF_VFE_m_IRQ_CLEAR_2(i));

		msm_camera_io_w(0, ispif->base + ISPIF_VFE_m_INPUT_SEL(i));

		msm_camera_io_w(ISPIF_STOP_INTF_IMMEDIATELY,
			ispif->base + ISPIF_VFE_m_INTF_CMD_0(i));
		msm_camera_io_w(ISPIF_STOP_INTF_IMMEDIATELY,
			ispif->base + ISPIF_VFE_m_INTF_CMD_1(i));
		pr_debug("%s: base %lx", __func__, (unsigned long)ispif->base);
		msm_camera_io_w(0, ispif->base +
			ISPIF_VFE_m_PIX_INTF_n_CID_MASK(i, 0));
		msm_camera_io_w(0, ispif->base +
			ISPIF_VFE_m_PIX_INTF_n_CID_MASK(i, 1));
		msm_camera_io_w(0, ispif->base +
			ISPIF_VFE_m_RDI_INTF_n_CID_MASK(i, 0));
		msm_camera_io_w(0, ispif->base +
			ISPIF_VFE_m_RDI_INTF_n_CID_MASK(i, 1));
		msm_camera_io_w(0, ispif->base +
			ISPIF_VFE_m_RDI_INTF_n_CID_MASK(i, 2));

		msm_camera_io_w(0, ispif->base +
			ISPIF_VFE_m_PIX_INTF_n_CROP(i, 0));
		msm_camera_io_w(0, ispif->base +
			ISPIF_VFE_m_PIX_INTF_n_CROP(i, 1));
	}

	msm_camera_io_w_mb(ISPIF_IRQ_GLOBAL_CLEAR_CMD, ispif->base +
		ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	return rc;
}

static void msm_ispif_sel_csid_core(struct ispif_device *ispif,
	uint8_t intftype, uint8_t csid, uint8_t vfe_intf)
{
	uint32_t data;

	BUG_ON(!ispif);

	if (!msm_ispif_is_intf_valid(ispif->csid_version, vfe_intf)) {
		pr_err("%s: invalid interface type\n", __func__);
		return;
	}

	data = msm_camera_io_r(ispif->base + ISPIF_VFE_m_INPUT_SEL(vfe_intf));
	switch (intftype) {
	case PIX0:
		data &= ~(BIT(1) | BIT(0));
		data |= (uint32_t) csid;
		break;
	case RDI0:
		data &= ~(BIT(5) | BIT(4));
		data |= ((uint32_t) csid) << 4;
		break;
	case PIX1:
		data &= ~(BIT(9) | BIT(8));
		data |= ((uint32_t) csid) << 8;
		break;
	case RDI1:
		data &= ~(BIT(13) | BIT(12));
		data |= ((uint32_t) csid) << 12;
		break;
	case RDI2:
		data &= ~(BIT(21) | BIT(20));
		data |= ((uint32_t) csid) << 20;
		break;
	}

	msm_camera_io_w_mb(data, ispif->base +
		ISPIF_VFE_m_INPUT_SEL(vfe_intf));
}

static void msm_ispif_enable_crop(struct ispif_device *ispif,
	uint8_t intftype, uint8_t vfe_intf, uint16_t start_pixel,
	uint16_t end_pixel)
{
	uint32_t data;
	BUG_ON(!ispif);

	if (!msm_ispif_is_intf_valid(ispif->csid_version, vfe_intf)) {
		pr_err("%s: invalid interface type\n", __func__);
		return;
	}

	data = msm_camera_io_r(ispif->base + ISPIF_VFE_m_CTRL_0(vfe_intf));
	data |= (1 << (intftype + 7));
	if (intftype == PIX0)
		data |= 1 << PIX0_LINE_BUF_EN_BIT;
	msm_camera_io_w(data,
		ispif->base + ISPIF_VFE_m_CTRL_0(vfe_intf));

	if (intftype == PIX0)
		msm_camera_io_w_mb(start_pixel | (end_pixel << 16),
			ispif->base + ISPIF_VFE_m_PIX_INTF_n_CROP(vfe_intf, 0));
	else if (intftype == PIX1)
		msm_camera_io_w_mb(start_pixel | (end_pixel << 16),
			ispif->base + ISPIF_VFE_m_PIX_INTF_n_CROP(vfe_intf, 1));
	else {
		pr_err("%s: invalid intftype=%d\n", __func__, intftype);
		BUG_ON(1);
		return;
	}
}

static void msm_ispif_enable_intf_cids(struct ispif_device *ispif,
	uint8_t intftype, uint16_t cid_mask, uint8_t vfe_intf, uint8_t enable)
{
	uint32_t intf_addr, data;

	BUG_ON(!ispif);

	if (!msm_ispif_is_intf_valid(ispif->csid_version, vfe_intf)) {
		pr_err("%s: invalid interface type\n", __func__);
		return;
	}

	switch (intftype) {
	case PIX0:
		intf_addr = ISPIF_VFE_m_PIX_INTF_n_CID_MASK(vfe_intf, 0);
		break;
	case RDI0:
		intf_addr = ISPIF_VFE_m_RDI_INTF_n_CID_MASK(vfe_intf, 0);
		break;
	case PIX1:
		intf_addr = ISPIF_VFE_m_PIX_INTF_n_CID_MASK(vfe_intf, 1);
		break;
	case RDI1:
		intf_addr = ISPIF_VFE_m_RDI_INTF_n_CID_MASK(vfe_intf, 1);
		break;
	case RDI2:
		intf_addr = ISPIF_VFE_m_RDI_INTF_n_CID_MASK(vfe_intf, 2);
		break;
	default:
		pr_err("%s: invalid intftype=%d\n", __func__, intftype);
		BUG_ON(1);
		return;
	}

	data = msm_camera_io_r(ispif->base + intf_addr);
	if (enable)
		data |=  (uint32_t) cid_mask;
	else
		data &= ~((uint32_t) cid_mask);
	msm_camera_io_w_mb(data, ispif->base + intf_addr);
}

static int msm_ispif_validate_intf_status(struct ispif_device *ispif,
	uint8_t intftype, uint8_t vfe_intf)
{
	int rc = 0;
	uint32_t data = 0;

	BUG_ON(!ispif);

	if (!msm_ispif_is_intf_valid(ispif->csid_version, vfe_intf)) {
		pr_err("%s: invalid interface type\n", __func__);
		return -EINVAL;
	}

	switch (intftype) {
	case PIX0:
		data = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_PIX_INTF_n_STATUS(vfe_intf, 0));
		break;
	case RDI0:
		data = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 0));
		break;
	case PIX1:
		data = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_PIX_INTF_n_STATUS(vfe_intf, 1));
		break;
	case RDI1:
		data = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 1));
		break;
	case RDI2:
		data = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 2));
		break;
	}
	if ((data & 0xf) != 0xf)
		rc = -EBUSY;
	return rc;
}

static void msm_ispif_select_clk_mux(struct ispif_device *ispif,
	uint8_t intftype, uint8_t csid, uint8_t vfe_intf)
{
	uint32_t data = 0;

	switch (intftype) {
	case PIX0:
		data = msm_camera_io_r(ispif->clk_mux_base);
		data &= ~(0xf << (vfe_intf * 8));
		data |= (csid << (vfe_intf * 8));
		msm_camera_io_w(data, ispif->clk_mux_base);
		break;

	case RDI0:
		data = msm_camera_io_r(ispif->clk_mux_base +
			ISPIF_RDI_CLK_MUX_SEL_ADDR);
		data &= ~(0xf << (vfe_intf * 12));
		data |= (csid << (vfe_intf * 12));
		msm_camera_io_w(data, ispif->clk_mux_base +
			ISPIF_RDI_CLK_MUX_SEL_ADDR);
		break;

	case PIX1:
		data = msm_camera_io_r(ispif->clk_mux_base);
		data &= ~(0xf0 << (vfe_intf * 8));
		data |= (csid << (4 + (vfe_intf * 8)));
		msm_camera_io_w(data, ispif->clk_mux_base);
		break;

	case RDI1:
		data = msm_camera_io_r(ispif->clk_mux_base +
			ISPIF_RDI_CLK_MUX_SEL_ADDR);
		data &= ~(0xf << (4 + (vfe_intf * 12)));
		data |= (csid << (4 + (vfe_intf * 12)));
		msm_camera_io_w(data, ispif->clk_mux_base +
			ISPIF_RDI_CLK_MUX_SEL_ADDR);
		break;

	case RDI2:
		data = msm_camera_io_r(ispif->clk_mux_base +
			ISPIF_RDI_CLK_MUX_SEL_ADDR);
		data &= ~(0xf << (8 + (vfe_intf * 12)));
		data |= (csid << (8 + (vfe_intf * 12)));
		msm_camera_io_w(data, ispif->clk_mux_base +
			ISPIF_RDI_CLK_MUX_SEL_ADDR);
		break;
	}
	CDBG("%s intftype %d data %x\n", __func__, intftype, data);
	/* ensure clk mux is enabled */
	mb();
	return;
}

static uint16_t msm_ispif_get_cids_mask_from_cfg(
	struct msm_ispif_params_entry *entry)
{
	int i;
	uint16_t cids_mask = 0;
	BUG_ON(!entry);

	for (i = 0; i < entry->num_cids && i < MAX_CID_CH_PARAM_ENTRY; i++)
		cids_mask |= (1 << entry->cids[i]);

	return cids_mask;
}
static int msm_ispif_config(struct ispif_device *ispif,
	void *data)
{
	int rc = 0, i = 0;
	uint16_t cid_mask;
	enum msm_ispif_intftype intftype;
	enum msm_ispif_vfe_intf vfe_intf;
	struct msm_ispif_param_data *params =
		(struct msm_ispif_param_data *)data;

	BUG_ON(!ispif);
	BUG_ON(!params);

	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		rc = -EPERM;
		return rc;
	}
	if (params->num > MAX_PARAM_ENTRIES) {
		pr_err("%s: invalid param entries %d\n", __func__,
			params->num);
		rc = -EINVAL;
		return rc;
	}

	for (i = 0; i < params->num; i++) {
		vfe_intf = params->entries[i].vfe_intf;
		if (!msm_ispif_is_intf_valid(ispif->csid_version,
				vfe_intf)) {
			pr_err("%s: invalid interface type\n", __func__);
			return -EINVAL;
		}
		msm_camera_io_w(0x0, ispif->base +
			ISPIF_VFE_m_IRQ_MASK_0(vfe_intf));
		msm_camera_io_w(0x0, ispif->base +
			ISPIF_VFE_m_IRQ_MASK_1(vfe_intf));
		msm_camera_io_w_mb(0x0, ispif->base +
			ISPIF_VFE_m_IRQ_MASK_2(vfe_intf));
	}

	for (i = 0; i < params->num; i++) {
		intftype = params->entries[i].intftype;

		vfe_intf = params->entries[i].vfe_intf;

		CDBG("%s intftype %x, vfe_intf %d, csid %d\n", __func__,
			intftype, vfe_intf, params->entries[i].csid);

		if ((intftype >= INTF_MAX) ||
			(vfe_intf >=  ispif->vfe_info.num_vfe) ||
			(ispif->csid_version <= CSID_VERSION_V22 &&
			(vfe_intf > VFE0))) {
			pr_err("%s: VFEID %d and CSID version %d mismatch\n",
				__func__, vfe_intf, ispif->csid_version);
			return -EINVAL;
		}

		if (ispif->csid_version >= CSID_VERSION_V30)
			msm_ispif_select_clk_mux(ispif, intftype,
				params->entries[i].csid, vfe_intf);

		rc = msm_ispif_validate_intf_status(ispif, intftype, vfe_intf);
		if (rc) {
			pr_err("%s:validate_intf_status failed, rc = %d\n",
				__func__, rc);
			return rc;
		}

		msm_ispif_sel_csid_core(ispif, intftype,
			params->entries[i].csid, vfe_intf);
		cid_mask = msm_ispif_get_cids_mask_from_cfg(
				&params->entries[i]);
		msm_ispif_enable_intf_cids(ispif, intftype,
			cid_mask, vfe_intf, 1);
		if (params->entries[i].crop_enable)
			msm_ispif_enable_crop(ispif, intftype, vfe_intf,
				params->entries[i].crop_start_pixel,
				params->entries[i].crop_end_pixel);
	}

	for (vfe_intf = 0; vfe_intf < 2; vfe_intf++) {
		msm_camera_io_w(ISPIF_IRQ_STATUS_MASK, ispif->base +
			ISPIF_VFE_m_IRQ_MASK_0(vfe_intf));

		msm_camera_io_w(ISPIF_IRQ_STATUS_MASK, ispif->base +
			ISPIF_VFE_m_IRQ_CLEAR_0(vfe_intf));

		msm_camera_io_w(ISPIF_IRQ_STATUS_1_MASK, ispif->base +
			ISPIF_VFE_m_IRQ_MASK_1(vfe_intf));

		msm_camera_io_w(ISPIF_IRQ_STATUS_1_MASK, ispif->base +
			ISPIF_VFE_m_IRQ_CLEAR_1(vfe_intf));

		msm_camera_io_w(ISPIF_IRQ_STATUS_2_MASK, ispif->base +
			ISPIF_VFE_m_IRQ_MASK_2(vfe_intf));

		msm_camera_io_w(ISPIF_IRQ_STATUS_2_MASK, ispif->base +
			ISPIF_VFE_m_IRQ_CLEAR_2(vfe_intf));
	}

	msm_camera_io_w_mb(ISPIF_IRQ_GLOBAL_CLEAR_CMD, ispif->base +
		ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	return rc;
}

static void msm_ispif_intf_cmd(struct ispif_device *ispif, uint32_t cmd_bits,
	struct msm_ispif_param_data *params)
{
	uint8_t vc;
	int i, k;
	enum msm_ispif_intftype intf_type;
	enum msm_ispif_cid cid;
	enum msm_ispif_vfe_intf vfe_intf;

	BUG_ON(!ispif);
	BUG_ON(!params);

	for (i = 0; i < params->num; i++) {
		vfe_intf = params->entries[i].vfe_intf;
		if (!msm_ispif_is_intf_valid(ispif->csid_version, vfe_intf)) {
			pr_err("%s: invalid interface type\n", __func__);
			return;
		}
		if (params->entries[i].num_cids > MAX_CID_CH_PARAM_ENTRY) {
			pr_err("%s: out of range of cid_num %d\n",
				__func__, params->entries[i].num_cids);
			return;
		}
	}

	for (i = 0; i < params->num; i++) {
		intf_type = params->entries[i].intftype;
		vfe_intf = params->entries[i].vfe_intf;
		for (k = 0; k < params->entries[i].num_cids; k++) {
			cid = params->entries[i].cids[k];
			vc = cid / 4;
			if (intf_type == RDI2) {
				/* zero out two bits */
				ispif->applied_intf_cmd[vfe_intf].intf_cmd1 &=
					~(0x3 << (vc * 2 + 8));
				/* set cmd bits */
				ispif->applied_intf_cmd[vfe_intf].intf_cmd1 |=
					(cmd_bits << (vc * 2 + 8));
			} else {
				/* zero 2 bits */
				ispif->applied_intf_cmd[vfe_intf].intf_cmd &=
					~(0x3 << (vc * 2 + intf_type * 8));
				/* set cmd bits */
				ispif->applied_intf_cmd[vfe_intf].intf_cmd |=
					(cmd_bits << (vc * 2 + intf_type * 8));
			}
		}
		/* cmd for PIX0, PIX1, RDI0, RDI1 */
		if (ispif->applied_intf_cmd[vfe_intf].intf_cmd != 0xFFFFFFFF)
			msm_camera_io_w_mb(
				ispif->applied_intf_cmd[vfe_intf].intf_cmd,
				ispif->base + ISPIF_VFE_m_INTF_CMD_0(vfe_intf));

		/* cmd for RDI2 */
		if (ispif->applied_intf_cmd[vfe_intf].intf_cmd1 != 0xFFFFFFFF)
			msm_camera_io_w_mb(
				ispif->applied_intf_cmd[vfe_intf].intf_cmd1,
				ispif->base + ISPIF_VFE_m_INTF_CMD_1(vfe_intf));
	}
}

static int msm_ispif_stop_immediately(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int i, rc = 0;
	uint16_t cid_mask = 0;
	BUG_ON(!ispif);
	BUG_ON(!params);

	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		rc = -EPERM;
		return rc;
	}

	if (params->num > MAX_PARAM_ENTRIES) {
		pr_err("%s: invalid param entries %d\n", __func__,
			params->num);
		rc = -EINVAL;
		return rc;
	}
	msm_ispif_intf_cmd(ispif, ISPIF_INTF_CMD_DISABLE_IMMEDIATELY, params);

	/* after stop the interface we need to unmask the CID enable bits */
	for (i = 0; i < params->num; i++) {
		cid_mask = msm_ispif_get_cids_mask_from_cfg(
			&params->entries[i]);
		msm_ispif_enable_intf_cids(ispif, params->entries[i].intftype,
			cid_mask, params->entries[i].vfe_intf, 0);
	}

	return rc;
}

static int msm_ispif_start_frame_boundary(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int rc = 0;

	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		rc = -EPERM;
		return rc;
	}
	if (params->num > MAX_PARAM_ENTRIES) {
		pr_err("%s: invalid param entries %d\n", __func__,
			params->num);
		rc = -EINVAL;
		return rc;
	}
	msm_ispif_intf_cmd(ispif, ISPIF_INTF_CMD_ENABLE_FRAME_BOUNDARY, params);

	return rc;
}

static int msm_ispif_restart_frame_boundary(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int rc = 0, i;
	long timeout = 0;
	uint16_t cid_mask;
	enum msm_ispif_intftype intftype;
	enum msm_ispif_vfe_intf vfe_intf;
	uint32_t vfe_mask = 0;
	uint32_t intf_addr;

	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		rc = -EPERM;
		return rc;
	}
	if (params->num > MAX_PARAM_ENTRIES) {
		pr_err("%s: invalid param entries %d\n", __func__,
			params->num);
		rc = -EINVAL;
		return rc;
	}

	for (i = 0; i < params->num; i++) {
		vfe_intf = params->entries[i].vfe_intf;
		if (vfe_intf >= VFE_MAX) {
			pr_err("%s: %d invalid i %d vfe_intf %d\n", __func__,
				__LINE__, i, vfe_intf);
			return -EINVAL;
		}
		vfe_mask |= (1 << vfe_intf);
	}

	/* Turn ON regulators before enabling the clocks*/
	rc = msm_ispif_set_regulators(ispif->vfe_vdd,
					ispif->vfe_vdd_count, 1);
	if (rc < 0)
		return -EFAULT;

	rc = msm_camera_clk_enable(&ispif->pdev->dev,
		ispif->clk_info, ispif->clks,
		ispif->num_clk, 1);
	if (rc < 0)
		goto disable_regulator;

	if (vfe_mask & (1 << VFE0)) {
		atomic_set(&ispif->reset_trig[VFE0], 1);
		/* initiate reset of ISPIF */
		msm_camera_io_w(ISPIF_RST_CMD_MASK_RESTART,
				ispif->base + ISPIF_RST_CMD_ADDR);
		timeout = wait_for_completion_interruptible_timeout(
			&ispif->reset_complete[VFE0], msecs_to_jiffies(500));
		if (timeout <= 0) {
			pr_err("%s: VFE0 reset wait timeout\n", __func__);
			rc = -ETIMEDOUT;
			goto disable_clk;
		}
	}

	if (ispif->hw_num_isps > 1  && (vfe_mask & (1 << VFE1))) {
		atomic_set(&ispif->reset_trig[VFE1], 1);
		msm_camera_io_w(ISPIF_RST_CMD_1_MASK_RESTART,
			ispif->base + ISPIF_RST_CMD_1_ADDR);
		timeout = wait_for_completion_interruptible_timeout(
				&ispif->reset_complete[VFE1],
				msecs_to_jiffies(500));
		if (timeout <= 0) {
			pr_err("%s: VFE1 reset wait timeout\n", __func__);
			rc = -ETIMEDOUT;
			goto disable_clk;
		}
	}

	pr_info("%s: ISPIF reset hw done, Restarting", __func__);
	rc = msm_camera_clk_enable(&ispif->pdev->dev,
		ispif->clk_info, ispif->clks,
		ispif->num_clk, 0);
	if (rc < 0)
		goto disable_regulator;

	/* Turn OFF regulators after disabling clocks */
	rc = msm_ispif_set_regulators(ispif->vfe_vdd, ispif->vfe_vdd_count, 0);
	if (rc < 0)
		goto end;

	for (i = 0; i < params->num; i++) {
		intftype = params->entries[i].intftype;
		vfe_intf = params->entries[i].vfe_intf;

		switch (params->entries[0].intftype) {
		case PIX0:
			intf_addr = ISPIF_VFE_m_PIX_INTF_n_STATUS(vfe_intf, 0);
			break;
		case RDI0:
			intf_addr = ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 0);
			break;
		case PIX1:
			intf_addr = ISPIF_VFE_m_PIX_INTF_n_STATUS(vfe_intf, 1);
			break;
		case RDI1:
			intf_addr = ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 1);
			break;
		case RDI2:
			intf_addr = ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 2);
			break;
		default:
			pr_err("%s: invalid intftype=%d\n", __func__,
			params->entries[i].intftype);
			rc = -EPERM;
			goto end;
		}

		msm_ispif_intf_cmd(ispif,
			ISPIF_INTF_CMD_ENABLE_FRAME_BOUNDARY, params);
	}

	for (i = 0; i < params->num; i++) {
		intftype = params->entries[i].intftype;

		vfe_intf = params->entries[i].vfe_intf;


		cid_mask = msm_ispif_get_cids_mask_from_cfg(
			&params->entries[i]);

		msm_ispif_enable_intf_cids(ispif, intftype,
			cid_mask, vfe_intf, 1);
	}
	return rc;

disable_clk:
	msm_camera_clk_enable(&ispif->pdev->dev,
		ispif->clk_info, ispif->clks,
		ispif->num_clk, 0);
disable_regulator:
	/* Turn OFF regulators */
	msm_ispif_set_regulators(ispif->vfe_vdd, ispif->vfe_vdd_count, 0);
end:
	return rc;
}

static int msm_ispif_stop_frame_boundary(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int i, rc = 0;
	uint16_t cid_mask = 0;
	uint32_t intf_addr;
	enum msm_ispif_vfe_intf vfe_intf;
	uint32_t stop_flag = 0;

	BUG_ON(!ispif);
	BUG_ON(!params);


	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		rc = -EPERM;
		return rc;
	}

	if (params->num > MAX_PARAM_ENTRIES) {
		pr_err("%s: invalid param entries %d\n", __func__,
			params->num);
		rc = -EINVAL;
		return rc;
	}

	for (i = 0; i < params->num; i++) {
		if (!msm_ispif_is_intf_valid(ispif->csid_version,
				params->entries[i].vfe_intf)) {
			pr_err("%s: invalid interface type\n", __func__);
			rc = -EINVAL;
			goto end;
		}
	}

	msm_ispif_intf_cmd(ispif,
		ISPIF_INTF_CMD_DISABLE_FRAME_BOUNDARY, params);

	for (i = 0; i < params->num; i++) {
		cid_mask =
			msm_ispif_get_cids_mask_from_cfg(&params->entries[i]);
		vfe_intf = params->entries[i].vfe_intf;

		switch (params->entries[i].intftype) {
		case PIX0:
			intf_addr = ISPIF_VFE_m_PIX_INTF_n_STATUS(vfe_intf, 0);
			break;
		case RDI0:
			intf_addr = ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 0);
			break;
		case PIX1:
			intf_addr = ISPIF_VFE_m_PIX_INTF_n_STATUS(vfe_intf, 1);
			break;
		case RDI1:
			intf_addr = ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 1);
			break;
		case RDI2:
			intf_addr = ISPIF_VFE_m_RDI_INTF_n_STATUS(vfe_intf, 2);
			break;
		default:
			pr_err("%s: invalid intftype=%d\n", __func__,
				params->entries[i].intftype);
			rc = -EPERM;
			goto end;
		}

		rc = readl_poll_timeout(ispif->base + intf_addr, stop_flag,
					(stop_flag & 0xF) == 0xF,
					ISPIF_TIMEOUT_SLEEP_US,
					ISPIF_TIMEOUT_ALL_US);
		if (rc < 0)
			goto end;

		/* disable CIDs in CID_MASK register */
		msm_ispif_enable_intf_cids(ispif, params->entries[i].intftype,
			cid_mask, vfe_intf, 0);
	}

end:
	return rc;
}

static void ispif_process_irq(struct ispif_device *ispif,
	struct ispif_irq_status *out, enum msm_ispif_vfe_intf vfe_id)
{
	BUG_ON(!ispif);
	BUG_ON(!out);

	if (out[vfe_id].ispifIrqStatus0 &
			ISPIF_IRQ_STATUS_PIX_SOF_MASK) {
		if (ispif->ispif_sof_debug < ISPIF_SOF_DEBUG_COUNT)
			pr_err("%s: PIX0 frame id: %u\n", __func__,
				ispif->sof_count[vfe_id].sof_cnt[PIX0]);
		ispif->sof_count[vfe_id].sof_cnt[PIX0]++;
		ispif->ispif_sof_debug++;
	}
	if (out[vfe_id].ispifIrqStatus0 &
			ISPIF_IRQ_STATUS_RDI0_SOF_MASK) {
		if (ispif->ispif_rdi0_debug < ISPIF_SOF_DEBUG_COUNT)
			pr_err("%s: RDI0 frame id: %u\n", __func__,
				ispif->sof_count[vfe_id].sof_cnt[RDI0]);
		ispif->sof_count[vfe_id].sof_cnt[RDI0]++;
		ispif->ispif_rdi0_debug++;
	}
	if (out[vfe_id].ispifIrqStatus1 &
			ISPIF_IRQ_STATUS_RDI1_SOF_MASK) {
		if (ispif->ispif_rdi1_debug < ISPIF_SOF_DEBUG_COUNT)
			pr_err("%s: RDI1 frame id: %u\n", __func__,
				ispif->sof_count[vfe_id].sof_cnt[RDI1]);
		ispif->sof_count[vfe_id].sof_cnt[RDI1]++;
		ispif->ispif_rdi1_debug++;
	}
	if (out[vfe_id].ispifIrqStatus2 &
			ISPIF_IRQ_STATUS_RDI2_SOF_MASK) {
		if (ispif->ispif_rdi2_debug < ISPIF_SOF_DEBUG_COUNT)
			pr_err("%s: RDI2 frame id: %u\n", __func__,
				ispif->sof_count[vfe_id].sof_cnt[RDI2]);
		ispif->sof_count[vfe_id].sof_cnt[RDI2]++;
		ispif->ispif_rdi2_debug++;
	}
}

static inline void msm_ispif_read_irq_status(struct ispif_irq_status *out,
	void *data)
{
	struct ispif_device *ispif = (struct ispif_device *)data;
	bool fatal_err = false;
	int i = 0;

	BUG_ON(!ispif);
	BUG_ON(!out);

	out[VFE0].ispifIrqStatus0 = msm_camera_io_r(ispif->base +
		ISPIF_VFE_m_IRQ_STATUS_0(VFE0));
	msm_camera_io_w(out[VFE0].ispifIrqStatus0,
		ispif->base + ISPIF_VFE_m_IRQ_CLEAR_0(VFE0));

	out[VFE0].ispifIrqStatus1 = msm_camera_io_r(ispif->base +
		ISPIF_VFE_m_IRQ_STATUS_1(VFE0));
	msm_camera_io_w(out[VFE0].ispifIrqStatus1,
		ispif->base + ISPIF_VFE_m_IRQ_CLEAR_1(VFE0));

	out[VFE0].ispifIrqStatus2 = msm_camera_io_r(ispif->base +
		ISPIF_VFE_m_IRQ_STATUS_2(VFE0));
	msm_camera_io_w_mb(out[VFE0].ispifIrqStatus2,
		ispif->base + ISPIF_VFE_m_IRQ_CLEAR_2(VFE0));

	if (ispif->vfe_info.num_vfe > 1) {
		out[VFE1].ispifIrqStatus0 = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_IRQ_STATUS_0(VFE1));
		msm_camera_io_w(out[VFE1].ispifIrqStatus0,
			ispif->base + ISPIF_VFE_m_IRQ_CLEAR_0(VFE1));

		out[VFE1].ispifIrqStatus1 = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_IRQ_STATUS_1(VFE1));
		msm_camera_io_w(out[VFE1].ispifIrqStatus1,
				ispif->base + ISPIF_VFE_m_IRQ_CLEAR_1(VFE1));

		out[VFE1].ispifIrqStatus2 = msm_camera_io_r(ispif->base +
			ISPIF_VFE_m_IRQ_STATUS_2(VFE1));
		msm_camera_io_w_mb(out[VFE1].ispifIrqStatus2,
			ispif->base + ISPIF_VFE_m_IRQ_CLEAR_2(VFE1));
	}
	msm_camera_io_w_mb(ISPIF_IRQ_GLOBAL_CLEAR_CMD, ispif->base +
	ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	if (out[VFE0].ispifIrqStatus0 & ISPIF_IRQ_STATUS_MASK) {
		if (out[VFE0].ispifIrqStatus0 & RESET_DONE_IRQ) {
			if (atomic_dec_and_test(&ispif->reset_trig[VFE0]))
				complete(&ispif->reset_complete[VFE0]);
		}

		if (out[VFE0].ispifIrqStatus0 & PIX_INTF_0_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE0 pix0 overflow.\n",
				__func__);
			fatal_err = true;
		}

		if (out[VFE0].ispifIrqStatus0 & RAW_INTF_0_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE0 rdi0 overflow.\n",
				__func__);
			fatal_err = true;
		}

		if (out[VFE0].ispifIrqStatus1 & RAW_INTF_1_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE0 rdi1 overflow.\n",
				__func__);
			fatal_err = true;
		}

		if (out[VFE0].ispifIrqStatus2 & RAW_INTF_2_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE0 rdi2 overflow.\n",
				__func__);
			fatal_err = true;
		}

		ispif_process_irq(ispif, out, VFE0);
	}
	if (ispif->hw_num_isps > 1) {
		if (out[VFE1].ispifIrqStatus0 & RESET_DONE_IRQ) {
			if (atomic_dec_and_test(&ispif->reset_trig[VFE1]))
				complete(&ispif->reset_complete[VFE1]);
		}

		if (out[VFE1].ispifIrqStatus0 & PIX_INTF_0_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE1 pix0 overflow.\n",
				__func__);
			fatal_err = true;
		}

		if (out[VFE1].ispifIrqStatus0 & RAW_INTF_0_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE1 rdi0 overflow.\n",
				__func__);
			fatal_err = true;
		}

		if (out[VFE1].ispifIrqStatus1 & RAW_INTF_1_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE1 rdi1 overflow.\n",
				__func__);
			fatal_err = true;
		}

		if (out[VFE1].ispifIrqStatus2 & RAW_INTF_2_OVERFLOW_IRQ) {
			pr_err_ratelimited("%s: VFE1 rdi2 overflow.\n",
				__func__);
			fatal_err = true;
		}

		ispif_process_irq(ispif, out, VFE1);
	}

	if (fatal_err == true) {
		pr_err_ratelimited("%s: fatal error, stop ispif immediately\n",
				__func__);
		for (i = 0; i < ispif->vfe_info.num_vfe; i++) {
			msm_camera_io_w(0x0,
				ispif->base + ISPIF_VFE_m_IRQ_MASK_0(i));
			msm_camera_io_w(0x0,
				ispif->base + ISPIF_VFE_m_IRQ_MASK_1(i));
			msm_camera_io_w(0x0,
				ispif->base + ISPIF_VFE_m_IRQ_MASK_2(i));
			msm_camera_io_w(ISPIF_STOP_INTF_IMMEDIATELY,
				ispif->base + ISPIF_VFE_m_INTF_CMD_0(i));
			msm_camera_io_w(ISPIF_STOP_INTF_IMMEDIATELY,
				ispif->base + ISPIF_VFE_m_INTF_CMD_1(i));
		}
	}
}

static irqreturn_t msm_io_ispif_irq(int irq_num, void *data)
{
	struct ispif_irq_status irq[VFE_MAX];

	msm_ispif_read_irq_status(irq, data);
	return IRQ_HANDLED;
}

static int msm_ispif_set_vfe_info(struct ispif_device *ispif,
	struct msm_ispif_vfe_info *vfe_info)
{
	if (!vfe_info || (vfe_info->num_vfe == 0) ||
		(vfe_info->num_vfe > ispif->hw_num_isps)) {
		pr_err("Invalid VFE info: %pK %d\n", vfe_info,
			   (vfe_info ? vfe_info->num_vfe : 0));
		return -EINVAL;
	}

	memcpy(&ispif->vfe_info, vfe_info, sizeof(struct msm_ispif_vfe_info));

	return 0;
}

static int msm_ispif_init(struct ispif_device *ispif,
	uint32_t csid_version)
{
	int rc = 0;

	BUG_ON(!ispif);

	if (ispif->ispif_state == ISPIF_POWER_UP) {
		pr_err("%s: ispif already initted state = %d\n", __func__,
			ispif->ispif_state);
		rc = -EPERM;
		return rc;
	}

	/* can we set to zero? */
	ispif->applied_intf_cmd[VFE0].intf_cmd  = 0xFFFFFFFF;
	ispif->applied_intf_cmd[VFE0].intf_cmd1 = 0xFFFFFFFF;
	ispif->applied_intf_cmd[VFE1].intf_cmd  = 0xFFFFFFFF;
	ispif->applied_intf_cmd[VFE1].intf_cmd1 = 0xFFFFFFFF;
	memset(ispif->sof_count, 0, sizeof(ispif->sof_count));

	ispif->csid_version = csid_version;

	if (ispif->csid_version >= CSID_VERSION_V30 && !ispif->clk_mux_base) {
		ispif->clk_mux_base = msm_camera_get_reg_base(ispif->pdev,
							"csi_clk_mux", 1);
		if (!ispif->clk_mux_base)
			return -ENOMEM;
	}

	rc = cam_config_ahb_clk(NULL, 0,
			CAM_AHB_CLIENT_ISPIF, CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	rc = msm_ispif_reset_hw(ispif);
	if (rc)
		goto error_ahb;

	rc = msm_ispif_reset(ispif);
	if (rc)
		goto error_ahb;
	ispif->ispif_state = ISPIF_POWER_UP;
	return 0;

error_ahb:
	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_ISPIF,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);
	return rc;
}

static void msm_ispif_release(struct ispif_device *ispif)
{
	BUG_ON(!ispif);

	msm_camera_enable_irq(ispif->irq, 0);

	ispif->ispif_state = ISPIF_POWER_DOWN;

	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_ISPIF,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);
}

static long msm_ispif_cmd(struct v4l2_subdev *sd, void *arg)
{
	long rc = 0;
	struct ispif_cfg_data *pcdata = (struct ispif_cfg_data *)arg;
	struct ispif_device *ispif =
		(struct ispif_device *)v4l2_get_subdevdata(sd);

	BUG_ON(!sd);
	BUG_ON(!pcdata);

	mutex_lock(&ispif->mutex);
	switch (pcdata->cfg_type) {
	case ISPIF_ENABLE_REG_DUMP:
		ispif->enb_dump_reg = pcdata->reg_dump; /* save dump config */
		break;
	case ISPIF_INIT:
		rc = msm_ispif_init(ispif, pcdata->csid_version);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_CFG:
		rc = msm_ispif_config(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_START_FRAME_BOUNDARY:
		rc = msm_ispif_start_frame_boundary(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_RESTART_FRAME_BOUNDARY:
		rc = msm_ispif_restart_frame_boundary(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;

	case ISPIF_STOP_FRAME_BOUNDARY:
		rc = msm_ispif_stop_frame_boundary(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_STOP_IMMEDIATELY:
		rc = msm_ispif_stop_immediately(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_RELEASE:
		msm_ispif_reset(ispif);
		msm_ispif_reset_hw(ispif);
		break;
	case ISPIF_SET_VFE_INFO:
		rc = msm_ispif_set_vfe_info(ispif, &pcdata->vfe_info);
		break;
	default:
		pr_err("%s: invalid cfg_type\n", __func__);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&ispif->mutex);
	return rc;
}
static struct v4l2_file_operations msm_ispif_v4l2_subdev_fops;

static long msm_ispif_subdev_ioctl_unlocked(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct ispif_device *ispif =
		(struct ispif_device *)v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_MSM_ISPIF_CFG:
		return msm_ispif_cmd(sd, arg);
	case VIDIOC_MSM_ISPIF_CFG_EXT:
		return msm_ispif_cmd_ext(sd, arg);
	case MSM_SD_NOTIFY_FREEZE: {
		ispif->ispif_sof_debug = 0;
		ispif->ispif_rdi0_debug = 0;
		ispif->ispif_rdi1_debug = 0;
		ispif->ispif_rdi2_debug = 0;
		return 0;
	}
	case MSM_SD_SHUTDOWN:
		return 0;
	default:
		pr_err_ratelimited("%s: invalid cmd 0x%x received\n",
			__func__, cmd);
		return -ENOIOCTLCMD;
	}
}

static long msm_ispif_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	return msm_ispif_subdev_ioctl(sd, cmd, arg);
}

static long msm_ispif_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_ispif_subdev_do_ioctl);
}

static int ispif_open_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ispif_device *ispif = v4l2_get_subdevdata(sd);
	int rc;

	mutex_lock(&ispif->mutex);
	if (0 == ispif->open_cnt) {
		/* enable regulator and clocks on first open */
		rc = msm_ispif_set_regulators(ispif->ispif_vdd,
					ispif->ispif_vdd_count, 1);
		if (rc)
			goto unlock;

		rc = msm_ispif_clk_ahb_enable(ispif, 1);
		if (rc)
			goto ahb_clk_enable_fail;
		rc = msm_camera_enable_irq(ispif->irq, 1);
		if (rc)
			goto irq_enable_fail;
	}
	/* mem remap is done in init when the clock is on */
	ispif->open_cnt++;
	mutex_unlock(&ispif->mutex);
	return rc;
ahb_clk_enable_fail:
	msm_ispif_set_regulators(ispif->ispif_vdd, ispif->ispif_vdd_count, 0);
irq_enable_fail:
	msm_ispif_clk_ahb_enable(ispif, 0);
unlock:
	mutex_unlock(&ispif->mutex);
	return rc;
}

static int ispif_close_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct ispif_device *ispif = v4l2_get_subdevdata(sd);
	if (!ispif) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ispif->mutex);
	if (ispif->open_cnt == 0) {
		pr_err("%s: Invalid close\n", __func__);
		rc = -ENODEV;
		goto end;
	}
	ispif->open_cnt--;
	if (ispif->open_cnt == 0) {
		msm_ispif_release(ispif);
		/* disable clocks and regulator on last close */
		msm_ispif_clk_ahb_enable(ispif, 0);
		msm_ispif_set_regulators(ispif->ispif_vdd,
					ispif->ispif_vdd_count, 0);
	}
end:
	mutex_unlock(&ispif->mutex);
	return rc;
}

static struct v4l2_subdev_core_ops msm_ispif_subdev_core_ops = {
	.ioctl = &msm_ispif_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_ispif_subdev_ops = {
	.core = &msm_ispif_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_ispif_internal_ops = {
	.open = ispif_open_node,
	.close = ispif_close_node,
};

static int ispif_probe(struct platform_device *pdev)
{
	int rc;
	struct ispif_device *ispif;

	ispif = kzalloc(sizeof(struct ispif_device), GFP_KERNEL);
	if (!ispif) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		of_property_read_u32((&pdev->dev)->of_node,
		"cell-index", &pdev->id);
		rc = of_property_read_u32((&pdev->dev)->of_node,
		"qcom,num-isps", &ispif->hw_num_isps);
		if (rc)
			/* backward compatibility */
			ispif->hw_num_isps = 1;
		/* not an error condition */
		rc = 0;
	}

	rc = msm_ispif_get_regulator_info(ispif, pdev);
	if (rc < 0)
		goto regulator_fail;

	rc = msm_ispif_get_clk_info(ispif, pdev);
	if (rc < 0) {
		pr_err("%s: msm_isp_get_clk_info() failed", __func__);
		rc = -EFAULT;
		goto get_clk_fail;
	}
	mutex_init(&ispif->mutex);
	ispif->base = msm_camera_get_reg_base(pdev, "ispif", 1);
	if (!ispif->base) {
		rc = -ENOMEM;
		goto reg_base_fail;
	}

	ispif->irq = msm_camera_get_irq(pdev, "ispif");
	if (!ispif->irq) {
		rc = -ENODEV;
		goto get_irq_fail;
	}
	rc = msm_camera_register_irq(pdev, ispif->irq, msm_io_ispif_irq,
			IRQF_TRIGGER_RISING, "ispif", ispif);
	if (rc) {
		rc = -ENODEV;
		goto get_irq_fail;
	}
	rc = msm_camera_enable_irq(ispif->irq, 0);
	if (rc)
		goto sd_reg_fail;

	ispif->pdev = pdev;

	v4l2_subdev_init(&ispif->msm_sd.sd, &msm_ispif_subdev_ops);
	ispif->msm_sd.sd.internal_ops = &msm_ispif_internal_ops;
	ispif->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	snprintf(ispif->msm_sd.sd.name,
		ARRAY_SIZE(ispif->msm_sd.sd.name), MSM_ISPIF_DRV_NAME);
	v4l2_set_subdevdata(&ispif->msm_sd.sd, ispif);

	platform_set_drvdata(pdev, &ispif->msm_sd.sd);

	media_entity_init(&ispif->msm_sd.sd.entity, 0, NULL, 0);
	ispif->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ispif->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ISPIF;
	ispif->msm_sd.sd.entity.name = pdev->name;
	ispif->msm_sd.close_seq = MSM_SD_CLOSE_1ST_CATEGORY | 0x1;
	rc = msm_sd_register(&ispif->msm_sd);
	if (rc) {
		pr_err("%s: msm_sd_register error = %d\n", __func__, rc);
		goto sd_reg_fail;
	}
	msm_cam_copy_v4l2_subdev_fops(&msm_ispif_v4l2_subdev_fops);
	msm_ispif_v4l2_subdev_fops.unlocked_ioctl =
		msm_ispif_subdev_fops_ioctl;
#ifdef CONFIG_COMPAT
	msm_ispif_v4l2_subdev_fops.compat_ioctl32 = msm_ispif_subdev_fops_ioctl;
#endif
	ispif->msm_sd.sd.devnode->fops = &msm_ispif_v4l2_subdev_fops;
	ispif->ispif_state = ISPIF_POWER_DOWN;
	ispif->open_cnt = 0;
	init_completion(&ispif->reset_complete[VFE0]);
	init_completion(&ispif->reset_complete[VFE1]);
	atomic_set(&ispif->reset_trig[VFE0], 0);
	atomic_set(&ispif->reset_trig[VFE1], 0);
	return 0;

sd_reg_fail:
	msm_camera_unregister_irq(pdev, ispif->irq, ispif);
get_irq_fail:
	msm_camera_put_reg_base(pdev, ispif->base, "ispif", 1);
reg_base_fail:
	msm_camera_put_clk_info(pdev, &ispif->ahb_clk_info,
		&ispif->ahb_clk,
		ispif->num_ahb_clk + ispif->num_clk);
get_clk_fail:
	msm_ispif_put_regulator(ispif);
regulator_fail:
	mutex_destroy(&ispif->mutex);
	kfree(ispif);
	return rc;
}

static const struct of_device_id msm_ispif_dt_match[] = {
	{.compatible = "qcom,ispif"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ispif_dt_match);

static struct platform_driver ispif_driver = {
	.probe = ispif_probe,
	.driver = {
		.name = MSM_ISPIF_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_ispif_dt_match,
	},
};

static int __init msm_ispif_init_module(void)
{
	return platform_driver_register(&ispif_driver);
}

static void __exit msm_ispif_exit_module(void)
{
	platform_driver_unregister(&ispif_driver);
}

module_init(msm_ispif_init_module);
module_exit(msm_ispif_exit_module);
MODULE_DESCRIPTION("MSM ISP Interface driver");
MODULE_LICENSE("GPL v2");
