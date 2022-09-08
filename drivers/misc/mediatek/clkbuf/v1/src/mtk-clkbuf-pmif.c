// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "mtk_clkbuf_common.h"
#include "mtk-clkbuf-pmif.h"

#define PMIF_PHANDLE_NAME		"pmif"
#define PMIF_PHANDLE_PAIR		2
#define SRCLKEN_RC_PHANDLE_NAME		"srclken_rc"
#define CLKBUF_CONN_INF_TARGET		"mediatek,clkbuf-conn-inf-target"
#define CLKBUF_NFC_INF_TARGET		"mediatek,clkbuf-nfc-inf-target"

#define MAX_PMIF_NUM			2

const u32 pmif_chip_to_hw_ver[CLKBUF_CHIP_ID_MAX][MAX_PMIF_NUM] = {
	[MT6765] = {
		CLKBUF_PMIF_VERSION_1,
		CLKBUF_PMIF_VERSION_1,
	},
	[MT6768] = {
		CLKBUF_PMIF_VERSION_1,
		CLKBUF_PMIF_VERSION_1,
	},
	[MT6789] = {
		CLKBUF_PMIF_VERSION_1,
		CLKBUF_PMIF_VERSION_1,
	},
	[MT6833] = {
		CLKBUF_PMIF_VERSION_1,
		CLKBUF_PMIF_VERSION_1,
	},
	[MT6855] = {
		CLKBUF_PMIF_VERSION_2,
		CLKBUF_PMIF_VERSION_2,
	},
	[MT6873] = {
		CLKBUF_PMIF_VERSION_1,
		CLKBUF_PMIF_VERSION_1,
	},
	[MT6879] = {
		CLKBUF_PMIF_VERSION_2,
		CLKBUF_PMIF_VERSION_2,
	},
	[MT6893] = {
		CLKBUF_PMIF_VERSION_1,
		CLKBUF_PMIF_VERSION_1,
	},
	[MT6895] = {
		CLKBUF_PMIF_VERSION_2,
		CLKBUF_PMIF_VERSION_2,
	},
	[MT6983] = {
		CLKBUF_PMIF_VERSION_2,
		CLKBUF_PMIF_VERSION_2,
	},
};

static struct pmif_hw pmif_hws[CLKBUF_PMIF_VER_MAX] = {
	[CLKBUF_PMIF_VERSION_1] = {
		SET_REG_BY_VERSION(conn_inf_en, CONN_INF_EN, 1)
		SET_REG_BY_VERSION(nfc_inf_en, NFC_INF_EN, 1)
		SET_REG_BY_VERSION(rc_inf_en, RC_INF_EN, 1)
		SET_REG_BY_VERSION(conn_clr_addr, CONN_CLR_CMD_DEST, 1)
		SET_REG_BY_VERSION(conn_set_addr, CONN_SET_CMD_DEST, 1)
		SET_REG_BY_VERSION(conn_clr_cmd, CONN_CLR_CMD, 1)
		SET_REG_BY_VERSION(conn_set_cmd, CONN_SET_CMD, 1)
		SET_REG_BY_VERSION(nfc_clr_addr, NFC_CLR_CMD_DEST, 1)
		SET_REG_BY_VERSION(nfc_set_addr, NFC_SET_CMD_DEST, 1)
		SET_REG_BY_VERSION(nfc_clr_cmd, NFC_CLR_CMD, 1)
		SET_REG_BY_VERSION(nfc_set_cmd, NFC_SET_CMD, 1)
		SET_REG_BY_VERSION(mode_ctrl, MODE_CTRL, 1)
		SET_REG_BY_VERSION(slp_ctrl, SLP_PROTECT, 1)
	},
	[CLKBUF_PMIF_VERSION_2] = {
		SET_REG_BY_VERSION(conn_inf_en, CONN_INF_EN, 2)
		SET_REG_BY_VERSION(nfc_inf_en, NFC_INF_EN, 2)
		SET_REG_BY_VERSION(rc_inf_en, RC_INF_EN, 2)
		SET_REG_BY_VERSION(conn_clr_addr, CONN_CLR_CMD_DEST, 2)
		SET_REG_BY_VERSION(conn_set_addr, CONN_SET_CMD_DEST, 2)
		SET_REG_BY_VERSION(conn_clr_cmd, CONN_CLR_CMD, 2)
		SET_REG_BY_VERSION(conn_set_cmd, CONN_SET_CMD, 2)
		SET_REG_BY_VERSION(nfc_clr_addr, NFC_CLR_CMD_DEST, 2)
		SET_REG_BY_VERSION(nfc_set_addr, NFC_SET_CMD_DEST, 2)
		SET_REG_BY_VERSION(nfc_clr_cmd, NFC_CLR_CMD, 2)
		SET_REG_BY_VERSION(nfc_set_cmd, NFC_SET_CMD, 2)
		SET_REG_BY_VERSION(mode_ctrl, MODE_CTRL, 2)
		SET_REG_BY_VERSION(slp_ctrl, SLP_PROTECT, 2)
	},
};

struct clkbuf_pmif_hw clkbuf_pmif;

u32 clkbuf_pmif_get_pmif_cnt(void)
{
	return clkbuf_pmif.pmif_num;
}

int clkbuf_pmif_get_misc_reg(u32 *mode_ctl, u32 *sleep_ctl, u32 pmif_idx)
{
	int ret = 0;

	if (!clkbuf_pmif.rc_enable)
		return -EHW_NOT_SUPPORT;

	ret = clk_buf_read(&clkbuf_pmif.pmif[pmif_idx]->hw,
			&clkbuf_pmif.pmif[pmif_idx]->_mode_ctrl, mode_ctl);
	if (ret)
		return ret;

	ret = clk_buf_read(&clkbuf_pmif.pmif[pmif_idx]->hw,
			&clkbuf_pmif.pmif[pmif_idx]->_slp_ctrl, sleep_ctl);
	if (ret)
		return ret;

	return ret;
}

int clkbuf_pmif_get_inf_en(enum PMIF_INF inf, u32 *en)
{
	if (inf == PMIF_CONN_INF) {
		return clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_conn_inf_en, en);
	} else if (inf == PMIF_NFC_INF) {
		return clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_nfc_inf_en, en);
	} else if (inf == PMIF_RC_INF) {
		return clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_rc_inf_en, en);
	}

	return -EINVAL;
}

int clkbuf_pmif_get_inf_data(enum PMIF_INF inf, u32 *clr_addr, u32 *set_addr,
		u32 *clr_cmd, u32 *set_cmd)
{
	int ret = 0;

	if (inf == PMIF_CONN_INF) {
		pr_notice("ofs: %x\n", clkbuf_pmif.pmif[0]->_conn_clr_addr.ofs);
		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_conn_clr_addr, clr_addr);
		if (ret) {
			pr_notice("get clr_addr failed: %d\n", ret);
			return ret;
		}

		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_conn_set_addr, set_addr);
		if (ret) {
			pr_notice("get set_addr failed: %d\n", ret);
			return ret;
		}

		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_conn_clr_cmd, clr_cmd);
		if (ret) {
			pr_notice("get clr cmd failed: %d\n", ret);
			return ret;
		}

		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_conn_set_cmd, set_cmd);
		if (ret) {
			pr_notice("get set cmd failed: %d\n", ret);
			return ret;
		}
	} else if (inf == PMIF_NFC_INF) {
		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_nfc_clr_addr, clr_addr);
		if (ret)
			return ret;
		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_nfc_set_addr, set_addr);
		if (ret)
			return ret;
		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_nfc_clr_cmd, clr_cmd);
		if (ret)
			return ret;
		ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_nfc_set_cmd, set_cmd);
		if (ret)
			return ret;
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int clkbuf_set_conn_inf(bool onoff)
{
	short no_lock = 0;
	int ret = 0;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clkbuf_pmif.lock);

	ret = clk_buf_write(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_conn_inf_en, onoff);

	if (!no_lock)
		mutex_unlock(&clkbuf_pmif.lock);

	if (ret)
		pr_notice("set conn inf failed\n");

	return ret;
}

static int clkbuf_set_nfc_inf(bool onoff)
{
	short no_lock = 0;
	int ret = 0;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clkbuf_pmif.lock);

	ret = clk_buf_write(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_nfc_inf_en, onoff);

	if (!no_lock)
		mutex_unlock(&clkbuf_pmif.lock);

	if (ret)
		pr_notice("set nfc inf failed\n");

	return ret;
}

static int clkbuf_set_rc_inf(bool onoff)
{
	short no_lock = 0;
	int ret = 0;

	if (!clkbuf_pmif.rc_enable)
		return -EHW_NOT_SUPPORT;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clkbuf_pmif.lock);

	ret = clk_buf_write(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_rc_inf_en, onoff);

	if (!no_lock)
		mutex_unlock(&clkbuf_pmif.lock);

	if (ret)
		pr_notice("set rc inf failed\n");

	return ret;
}

static int clkbuf_conn_inf_on_ctl(u8 xo_idx, struct xo_buf_ctl_cmd_t *cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (cmd->hw_id != CLKBUF_PMIF && cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	if (xo_idx != clkbuf_pmif.xo_conn_id)
		return -EINVAL;

	return clkbuf_set_conn_inf(true);
}

static int clkbuf_conn_inf_off_ctl(u8 xo_idx, struct xo_buf_ctl_cmd_t *cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (cmd->hw_id != CLKBUF_PMIF && cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	if (xo_idx != clkbuf_pmif.xo_conn_id)
		return -EINVAL;

	return clkbuf_set_conn_inf(false);
}

static int clkbuf_conn_inf_init_ctl(u8 xo_idx, struct xo_buf_ctl_cmd_t *cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (cmd->hw_id != CLKBUF_PMIF && cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	if (xo_idx != clkbuf_pmif.xo_conn_id)
		return -EINVAL;

	return clkbuf_set_conn_inf(clkbuf_pmif.conn_inf_init);
}

static int clkbuf_nfc_inf_on_ctl(u8 xo_idx, struct xo_buf_ctl_cmd_t *cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (cmd->hw_id != CLKBUF_PMIF && cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	if (xo_idx != clkbuf_pmif.xo_nfc_id)
		return -EINVAL;

	return clkbuf_set_nfc_inf(true);
}

static int clkbuf_nfc_inf_off_ctl(u8 xo_idx, struct xo_buf_ctl_cmd_t *cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (cmd->hw_id != CLKBUF_PMIF && cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	if (xo_idx != clkbuf_pmif.xo_nfc_id)
		return -EINVAL;

	return clkbuf_set_nfc_inf(false);
}

static int clkbuf_nfc_inf_init_ctl(u8 xo_idx, struct xo_buf_ctl_cmd_t *cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (cmd->hw_id != CLKBUF_PMIF && cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	if (xo_idx != clkbuf_pmif.xo_nfc_id)
		return -EINVAL;

	return clkbuf_set_nfc_inf(clkbuf_pmif.nfc_inf_init);
}

static int __clkbuf_pmif_dts_base_init(struct platform_device *pdev,
		struct device_node *node, const char *phandle_name,
		int index, struct base_hw *hw)
{
	struct device_node *p_node = of_parse_phandle(node, phandle_name, 0);
	struct resource res;
	int ret = 0;

	if (!p_node) {
		pr_notice("%s phandle not found, disable function\n",
			phandle_name);
		goto PMIF_BASE_INIT_DONE;
	}

	ret = of_address_to_resource(p_node, index, &res);
	if (ret) {
		pr_notice("pmif %d of node address to resource failed: %d\n",
			index, ret);
		goto PMIF_BASE_INIT_DONE;
	}
	hw->base.addr = devm_ioremap(&pdev->dev,
		res.start, resource_size(&res));
	if (IS_ERR(hw->base.addr))
		return -EGET_BASE_FAILED;
	hw->enable = true;

PMIF_BASE_INIT_DONE:
	of_node_put(p_node);

	return 0;
}

static int clkbuf_pmif_dts_base_init(struct platform_device *pdev)
{
	struct device_node *node;
	int ret = 0;
	int reg_idx;
	int i;

	node = of_parse_phandle(pdev->dev.of_node, CLKBUF_CTL_PHANDLE_NAME, 0);
	if (!node) {
		pr_notice("find clock_buffer_ctrl node failed\n");
		return -EFIND_DTS_ERR;
	}

	for (i = 0; i < clkbuf_pmif.pmif_num; i++) {
		ret = of_property_read_u32_index(node, PMIF_PHANDLE_NAME,
				(i * 2) + 1, &reg_idx);
		if (ret) {
			pr_notice("clkbuf get pmif reg index failed: %d\n",
				ret);
			return ret;
		}

		ret = __clkbuf_pmif_dts_base_init(pdev, node, PMIF_PHANDLE_NAME,
				reg_idx, &clkbuf_pmif.pmif[i]->hw);
		if (ret) {
			pr_notice("clkbuf pmif base address init failed: %d\n",
				ret);
			return ret;
		}
	}

	of_node_put(node);

	return ret;
}

static int clkbuf_pmif_reg_init(struct pmif_hw **pmif, u32 pmif_idx)
{
	int chip = clk_buf_get_chip_id();

	if (chip < 0)
		return chip;

	if (pmif_chip_to_hw_ver[chip][pmif_idx] <= CLKBUF_PMIF_NONE
		|| pmif_chip_to_hw_ver[chip][pmif_idx] >= CLKBUF_PMIF_VER_MAX) {
		pr_notice("no pmif hw version found, can not init\n");
		return -ECHIP_NOT_FOUND;
	}

	(*pmif) = &pmif_hws[pmif_chip_to_hw_ver[chip][pmif_idx]];

	return 0;
}

static void clkbuf_dts_get_inf_target(struct device_node *node)
{
	const char *target = NULL;
	int i;

	clkbuf_pmif.conn_inf_ctl.clk_buf_on_ctrl = clkbuf_conn_inf_on_ctl;
	clkbuf_pmif.conn_inf_ctl.clk_buf_off_ctrl = clkbuf_conn_inf_off_ctl;
	clkbuf_pmif.conn_inf_ctl.clk_buf_init_ctrl = clkbuf_conn_inf_init_ctl;

	/* find conn interface target */
	/* will use to register clk_buf_ctl callback */
	if (of_property_read_string(node, CLKBUF_CONN_INF_TARGET, &target)) {
		pr_notice("can not find conn inf target, skip\n");
	} else {
		for (i = 0; i < clk_buf_get_xo_num(); i++) {
			if (!strcmp(clk_buf_get_xo_name(i), target)) {
				clk_buf_register_xo_ctl_op(target,
					&clkbuf_pmif.conn_inf_ctl);
				clkbuf_pmif.xo_conn_id = i;
			}
		}
	}

	clkbuf_pmif.nfc_inf_ctl.clk_buf_on_ctrl = clkbuf_nfc_inf_on_ctl;
	clkbuf_pmif.nfc_inf_ctl.clk_buf_off_ctrl = clkbuf_nfc_inf_off_ctl;
	clkbuf_pmif.nfc_inf_ctl.clk_buf_init_ctrl = clkbuf_nfc_inf_init_ctl;

	/* find nfc interface target */
	/* will use to register clk_buf_ctl callback */
	if (of_property_read_string(node, CLKBUF_NFC_INF_TARGET, &target)) {
		pr_notice("can not find nfc inf target, skip\n");
	} else {
		for (i = 0; i < clk_buf_get_xo_num(); i++) {
			if (!strcmp(clk_buf_get_xo_name(i), target)) {
				clk_buf_register_xo_ctl_op(target,
					&clkbuf_pmif.nfc_inf_ctl);
				clkbuf_pmif.xo_nfc_id = i;
			}
		}
	}
}

static int clkbuf_pmif_dts_init(struct platform_device *pdev)
{
	struct device_node *ctl_node;
	struct device_node *rc_node;
	int ret = 0;

	ctl_node = of_parse_phandle(pdev->dev.of_node,
			CLKBUF_CTL_PHANDLE_NAME, 0);
	if (!ctl_node) {
		pr_notice("find clock_buffer_ctrl ctl_node failed\n");
		return -EFIND_DTS_ERR;
	}

	ret = of_property_count_u32_elems(ctl_node, PMIF_PHANDLE_NAME);
	if (ret % PMIF_PHANDLE_PAIR) {
		pr_notice("pmif phandles are not paired\n");
		return -ret;
	}
	clkbuf_pmif.pmif_num = ret / PMIF_PHANDLE_PAIR;

	clkbuf_pmif.pmif = devm_kmalloc(&pdev->dev,
		sizeof(struct pmif_hw *) * clkbuf_pmif.pmif_num, GFP_KERNEL);
	if (!clkbuf_pmif.pmif)
		return -ENOMEM;

	clkbuf_dts_get_inf_target(ctl_node);

	rc_node = of_parse_phandle(ctl_node, SRCLKEN_RC_PHANDLE_NAME, 0);
	if (!rc_node) {
		pr_notice("find rc node failed, assume rc not support\n");
		of_node_put(ctl_node);
		return 0;
	}
	clkbuf_pmif.rc_enable = of_property_read_bool(rc_node,
			ENABLE_PROP_NAME);

	of_node_put(rc_node);
	of_node_put(ctl_node);

	return 0;
}

int __clk_buf_pmif_rc_inf_store(const char *cmd)
{
	if (!strcmp(cmd, "ON"))
		return clkbuf_set_rc_inf(true);
	else if (!strcmp(cmd, "OFF"))
		return clkbuf_set_rc_inf(false);
	else if (!strcmp(cmd, "INIT"))
		return clkbuf_set_rc_inf(clkbuf_pmif.rc_inf_init);

	pr_notice("unknown cmd: %s\n", cmd);
	return -EPERM;
}

int clkbuf_pmif_hw_init(struct platform_device *pdev)
{
	int ret = 0;
	int i;

	ret = clkbuf_pmif_dts_init(pdev);
	if (ret) {
		pr_notice("clkbuf pmif dts init failed\n");
		return ret;
	}

	for (i = 0; i < clkbuf_pmif.pmif_num; i++) {
		ret = clkbuf_pmif_reg_init(&clkbuf_pmif.pmif[i], i);
		if (ret) {
			pr_notice("clkbuf pmif hw reg init failed: %d\n", ret);
			return ret;
		}
	}

	ret = clkbuf_pmif_dts_base_init(pdev);
	if (ret) {
		pr_notice("base address init failed with err: %d\n", ret);
		return ret;
	}

	mutex_init(&clkbuf_pmif.lock);

	return ret;
}

int clkbuf_pmif_post_init(void)
{
	int ret = 0;
	u32 val = 0;

	ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_conn_inf_en, &val);
	if (ret) {
		pr_notice("conn inf init val read failed: %d\n", ret);
		return ret;
	}
	clkbuf_pmif.conn_inf_init = (val ? true : false);

	ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_nfc_inf_en, &val);
	if (ret) {
		pr_notice("nfc inf init val read failed: %d\n", ret);
		return ret;
	}
	clkbuf_pmif.nfc_inf_init = (val ? true : false);

	ret = clk_buf_read(&clkbuf_pmif.pmif[0]->hw,
			&clkbuf_pmif.pmif[0]->_rc_inf_en, &val);
	if (ret) {
		pr_notice("rc inf init val read failed: %d\n", ret);
		return ret;
	}
	clkbuf_pmif.rc_inf_init = (val ? true : false);

	return ret;
}
