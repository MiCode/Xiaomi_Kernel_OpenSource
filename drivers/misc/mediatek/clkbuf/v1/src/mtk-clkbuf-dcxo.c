// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "mtk-clkbuf-dcxo.h"

#define CLKBUF_PMIC_CENTRAL_BASE	"mediatek,clkbuf-pmic-central-base"
#define CLKBUF_XO_MODE_NUM		"mediatek,xo-mode-num"
#define CLKBUF_XO_SUPPORT_PROP_NAME	"mediatek,xo-buf-support"
#define CLKBUF_XO_ALLOW_CONTROL		"mediatek,xo-buf-allow-control"
#define CLKBUF_XO_NAME_PROP		"mediatek,xo-buf-name"
#define CLKBUF_XO_PROP			"mediatek,xo-"
#define CLKBUF_VOTER_SUPPORT		"mediatek,xo-voter-support"
#define CLKBUF_INIT_AT_KERNEL		"mediatek,clkbuf-kernel-init"
#define DCXO_DESENSE_SUPPORT		"mediatek,dcxo-de-sense-support"
#define DCXO_IMPEDANCE_SUPPORT		"mediatek,dcxo-impedance-support"
#define DCXO_DRV_CURR_SUPPORT		"mediatek,dcxo-drv-curr-support"
#define DCXO_SPMI_RW			"mediatek,dcxo-spmi-rw"
#define DCXO_PMRC_EN_SUPPORT		"mediatek,pmrc-en-support"
#define CLKBUF_PMRC_EN_ADDR		"mediatek,pmrc-en-addr"
#define XOID_NOT_FOUND			"UNSUPPORT_XOID"

/* for old project init for dct tool at kernel */
#define CLKBUF_DCT_XO_QUANTITY		"mediatek,clkbuf-quantity"
#define CLKBUF_DCT_XO_CONFIG		"mediatek,clkbuf-config"
#define CLKBUF_DCT_XO_IMPEDANCE		"mediatek,clkbuf-output-impedance"
#define CLKBUF_DCT_XO_DESNSE		"medaitek,clkbuf-controls-for-desense"
#define CLKBUF_DCT_XO_DRVCURR		"mediatek,clkbuf-driving-current"

#define XO_NAME_LEN 10

static struct dcxo_hw *dcxo;

static struct dcxo_hw *dcxos[CLKBUF_PMIC_ID_MAX] = {
	[MT6359P] = &mt6359p_dcxo,
	[MT6366] = &mt6366_dcxo,
	[MT6685] = &mt6685_dcxo,
};

int clkbuf_xo_sanity_check(u8 xo_idx)
{
	if (xo_idx >= dcxo->xo_num) {
		pr_notice("invalid xo idx: %u\n", xo_idx);
		return -EINVAL;
	}

	return 0;
}

int clkbuf_dcxo_notify(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd)
{
	struct xo_buf_ctl_t *xo_buf_ctl = NULL;
	int ret = 0;

	if (ctl_cmd->hw_id >= CLKBUF_HW_MAX)
		return -EINVAL;

	list_for_each_entry(xo_buf_ctl, &dcxo->xo_bufs[xo_idx].xo_buf_ctl_head,
			xo_buf_ctl_list) {
		switch (ctl_cmd->cmd) {
		case CLKBUF_CMD_OFF:
			if (xo_buf_ctl->clk_buf_off_ctrl)
				ret = xo_buf_ctl->clk_buf_off_ctrl(xo_idx,
					ctl_cmd, xo_buf_ctl);
			break;
		case CLKBUF_CMD_ON:
			if (xo_buf_ctl->clk_buf_on_ctrl)
				ret = xo_buf_ctl->clk_buf_on_ctrl(xo_idx,
					ctl_cmd, xo_buf_ctl);
			break;
		case CLKBUF_CMD_SW:
			if (xo_buf_ctl->clk_buf_sw_ctrl)
				ret = xo_buf_ctl->clk_buf_sw_ctrl(xo_idx,
					ctl_cmd, xo_buf_ctl);
			break;
		case CLKBUF_CMD_HW:
			if (xo_buf_ctl->clk_buf_hw_ctrl)
				ret = xo_buf_ctl->clk_buf_hw_ctrl(xo_idx,
					ctl_cmd, xo_buf_ctl);
			break;
		case CLKBUF_CMD_INIT:
			if (xo_buf_ctl->clk_buf_init_ctrl)
				ret = xo_buf_ctl->clk_buf_init_ctrl(xo_idx,
					ctl_cmd, xo_buf_ctl);
			break;
		case CLKBUF_CMD_SHOW:
			if (xo_buf_ctl->clk_buf_show_ctrl)
				ret = xo_buf_ctl->clk_buf_show_ctrl(xo_idx,
					ctl_cmd, xo_buf_ctl);
			break;
		default:
			pr_debug("unsupport command: %u\n", ctl_cmd->cmd);
		}
		if (ret) {
			pr_notice("clk_buf_ctrl failed\n");
			return ret;
		}
	}

	return ret;
}

int clkbuf_dcxo_register_op(u8 idx, struct xo_buf_ctl_t *xo_buf_ctl)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(idx);
	if (ret)
		return ret;

	list_add_tail(&(xo_buf_ctl->xo_buf_ctl_list),
		&(dcxo->xo_bufs[idx].xo_buf_ctl_head));

	return ret;
}

int clkbuf_dcxo_get_xo_num(void)
{
	return dcxo->xo_num;
}

const char *clkbuf_dcxo_get_xo_name(u8 idx)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(idx);
	if (ret)
		return XOID_NOT_FOUND;

	return dcxo->xo_bufs[idx].xo_name;
}

int clkbuf_dcxo_get_xo_id_by_name(const char *xo_name)
{
	int i;

	for (i = 0; i < dcxo->xo_num; i++)
		if (!strcmp(xo_name, dcxo->xo_bufs[i].xo_name))
			return i;

	return -EINVAL;
}

static int set_xo_en(u8 xo_idx, bool onoff)
{
	short no_lock = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	if (!clkbuf_dcxo_get_xo_controllable(xo_idx)) {
		pr_notice("xo_buf: %u not controllable\n", xo_idx);
		return -EINVAL;
	}

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret = clk_buf_write(&dcxo->hw, &dcxo->xo_bufs[xo_idx]._xo_en, onoff);

	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	if (ret)
		pr_notice("set xo en failed\n");

	return ret;
}

static int set_xo_mode(u8 xo_idx, u8 xo_mode)
{
	short no_lock = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	if (!clkbuf_dcxo_get_xo_controllable(xo_idx)) {
		pr_notice("xo_buf: %u not controllable\n", xo_idx);
		return -EINVAL;
	}

	if (xo_mode >= DCXO_MODE_MAX || xo_mode >= dcxo->xo_mode_num) {
		pr_notice("xo_mode: %u is not support\n", xo_mode);
		return -EINVAL;
	}

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret =  clk_buf_write(&dcxo->hw,
			&dcxo->xo_bufs[xo_idx]._xo_mode, xo_mode);
	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	if (ret)
		pr_notice("set xo mode failed\n");

	return ret;
}

static int set_xo_impedance(u8 xo_idx, u32 impedance)
{
	short no_lock = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret = clk_buf_write(&dcxo->hw, &dcxo->xo_bufs[xo_idx]._impedance,
			impedance);

	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	if (ret)
		pr_notice("set xo_buf%d impedance failed: %d\n",
			xo_idx, ret);

	return ret;
}

static int set_xo_desense(u8 xo_idx, u32 desense)
{
	short no_lock = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret = clk_buf_write(&dcxo->hw, &dcxo->xo_bufs[xo_idx]._de_sense,
			desense);

	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	if (ret)
		pr_notice("set xo_bufs%d desense failed: %d\n",
			xo_idx, ret);

	return ret;
}

static int set_xo_drvcurr(u8 xo_idx, u32 drvcurr)
{
	short no_lock = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret = clk_buf_write(&dcxo->hw, &dcxo->xo_bufs[xo_idx]._drv_curr,
			drvcurr);

	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	if (ret)
		pr_notice("set xo_bufs%u drv_curr failed: %d\n",
			xo_idx, ret);

	return ret;
}

int clkbuf_dcxo_set_xo_sw_en(u8 xo_idx, u8 en)
{
	short no_lock = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	if (!clkbuf_dcxo_get_xo_controllable(xo_idx)) {
		pr_notice("xo_buf: %u not controllable\n", xo_idx);
		return -EINVAL;
	}

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	dcxo->xo_bufs[xo_idx].sw_status = (en ? 1 : 0);

	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	return ret;
}

bool clkbuf_dcxo_get_xo_sw_en(u8 xo_idx)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	return dcxo->xo_bufs[xo_idx].sw_status;
}

static int clkbuf_dcxo_get_auxout(u32 auxout_sel, struct reg_t *out_reg,
		u32 *output)
{
	short no_lock = 0;
	int ret = 0;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret = clk_buf_write(&dcxo->hw, &dcxo->_static_aux_sel, auxout_sel);

	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	if (ret) {
		pr_notice("write auxout sel failed with err: %d\n", ret);
		return ret;
	}

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret = clk_buf_read(&dcxo->hw, out_reg, output);
	if (ret) {
		pr_notice("read auxout sel failed with err: %d\n", ret);
		return ret;
	}

	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	return ret;
}

int clkbuf_dcxo_get_xo_en(u8 idx, u32 *en)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(idx);
	if (ret)
		return ret;

	ret = clkbuf_dcxo_get_auxout(dcxo->xo_bufs[idx].xo_en_auxout_sel,
			&dcxo->xo_bufs[idx]._xo_en_auxout, en);
	if (ret)
		pr_notice("get xo_buf%u en failed with err: %d\n", idx, ret);

	return ret;
}

bool clkbuf_dcxo_get_xo_support(u8 idx)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(idx);
	if (ret)
		return 0;

	return dcxo->xo_bufs[idx].support && dcxo->xo_bufs[idx].in_use;
}

bool clkbuf_dcxo_get_xo_controllable(u8 idx)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(idx);
	if (ret)
		return 0;

	return dcxo->xo_bufs[idx].controllable && dcxo->xo_bufs[idx].in_use;
}

bool clkbuf_dcxo_is_xo_in_use(u8 idx)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(idx);
	if (ret)
		return 0;

	return dcxo->xo_bufs[idx].in_use;
}

int clkbuf_dcxo_get_hwbblpm_sel(u32 *en)
{
	if (!dcxo->hwbblpm_support)
		return -EHW_NOT_SUPPORT;

	return clk_buf_read(&dcxo->hw, &dcxo->_hwbblpm_sel, en);
}

int clkbuf_dcxo_get_bblpm_en(u32 *val)
{
	return clkbuf_dcxo_get_auxout(dcxo->bblpm_auxout_sel,
			&dcxo->_bblpm_auxout, val);
}

int clkbuf_dcxo_set_capid_pre(void)
{
	int ret = 0;

	ret = clk_buf_write(&dcxo->hw, &dcxo->_xo_aac_fpm_swen, 0);
	if (ret) {
		pr_notice("set aac fpm swen failed\n");
		return ret;
	}
	mdelay(1);

	ret = clk_buf_write(&dcxo->hw, &dcxo->_xo_aac_fpm_swen, 1);
	if (ret) {
		pr_notice("set aac fpm swen failed\n");
		return ret;
	}

	mdelay(5);

	return ret;
}

int clkbuf_dcxo_get_capid(u32 *capid)
{
	return clk_buf_read(&dcxo->hw, &dcxo->_xo_cdac_fpm, capid);
}

int clkbuf_dcxo_set_capid(u32 capid)
{
	int ret = 0;

	ret = clk_buf_write(&dcxo->hw, &dcxo->_xo_cdac_fpm, capid);
	if (ret) {
		pr_notice("set capid failed\n");
		return ret;
	}

	return ret;
}

int clkbuf_dcxo_get_heater(bool *on)
{
	u32 heat_sel;
	int ret = 0;

	ret =  clk_buf_read(&dcxo->hw, &dcxo->_xo_heater_sel, &heat_sel);
	if (ret) {
		pr_notice("get heat sel failed\n");
		return ret;
	}
	pr_notice("get heat sel 0x%x\n", heat_sel);
	if (!heat_sel)
		*on = false;
	else
		*on = true;

	return ret;
}

int clkbuf_dcxo_set_heater(bool on)
{
	int ret = 0;

	if (on) {
		ret = clk_buf_write(&dcxo->hw, &dcxo->_xo_heater_sel, 2);
		if (ret) {
			pr_notice("switch on heater failed\n");
			return ret;
		}
	} else {
		ret = clk_buf_write(&dcxo->hw, &dcxo->_xo_heater_sel, 0);
		if (ret) {
			pr_notice("switch off heater failed\n");
			return ret;
		}
	}

	return ret;
}

int clkbuf_dcxo_get_xo_mode(u8 xo_idx, u32 *mode)
{
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	return clk_buf_read(&dcxo->hw, &dcxo->xo_bufs[xo_idx]._xo_mode, mode);
}

int clkbuf_dcxo_dump_rc_voter_log(char *buf)
{
	u32 voter = 0;
	u32 val = 0;
	int ret = 0;
	int len = 0;
	u8 i;

	if (!dcxo->voter_support)
		return 0;

	for (i = 0; i < dcxo->xo_num; i++) {
		if (!clkbuf_dcxo_is_xo_in_use(i))
			continue;

		ret = clk_buf_read(&dcxo->hw,
				&dcxo->xo_bufs[i]._rc_voter,
				&voter);
		/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
		if (dcxo->spmi_rw)
			ret |= clk_buf_read_with_ofs(&dcxo->hw,
					&dcxo->xo_bufs[i]._rc_voter,
					&val,
					1);

		if (ret) {
			pr_notice("get %s voter failed\n",
				dcxo->xo_bufs[i].xo_name);
			return ret;
		}

		if (dcxo->spmi_rw)
			voter |= (val << 8);

		len += snprintf(buf + len, PAGE_SIZE - len, "%s voter: 0x%x\n",
			dcxo->xo_bufs[i].xo_name, voter);
	}

	return len;
}

int clkbuf_dcxo_dump_misc_log(char *buf)
{
	u32 val = 0;
	int len = 0;
	int ret = 0;
	u8 i;

	if (dcxo->ops.dcxo_dump_misc_log == NULL)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"DCXO dump misc function not implemented!!\n");
	else
		len += dcxo->ops.dcxo_dump_misc_log(buf + len);

	/* dump current impedance setting */
	if (!dcxo->impedance_support)
		goto DUMP_DESENSE_LOG;
	len += snprintf(buf + len, PAGE_SIZE - len, "XO_BUF imepdance: ");
	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		ret = clk_buf_read(&dcxo->hw,
				&dcxo->xo_bufs[i]._impedance, &val);
		if (ret)
			goto DUMP_MISC_FAILED;
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x ", val);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

DUMP_DESENSE_LOG:
	if (!dcxo->de_sense_support)
		goto DUMP_DRV_CURR_LOG;
	/* dump current desense setting */
	len += snprintf(buf + len, PAGE_SIZE - len, "XO_BUF de-sense: ");
	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		ret = clk_buf_read(&dcxo->hw,
				&dcxo->xo_bufs[i]._de_sense, &val);
		if (ret)
			continue;
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x ", val);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

DUMP_DRV_CURR_LOG:
	if (!dcxo->drv_curr_support)
		goto DUMP_MISC_DONE;
	/* dump current desense setting */
	len += snprintf(buf + len, PAGE_SIZE - len, "XO_BUF DRV_CURR: ");
	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		ret = clk_buf_read(&dcxo->hw,
				&dcxo->xo_bufs[i]._drv_curr, &val);
		if (ret)
			continue;
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x ", val);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

DUMP_MISC_DONE:
	return len;

DUMP_MISC_FAILED:
	len -= 2;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

int clkbuf_dcxo_dump_reg_log(char *buf)
{
	if (dcxo->ops.dcxo_dump_reg_log != NULL)
		return dcxo->ops.dcxo_dump_reg_log(buf);
	else
		return snprintf(buf, PAGE_SIZE, "DCXO dump reg function not implemented!!\n");
}

int clkbuf_dcxo_dump_dws(char *buf)
{
	int len = 0;
	int i;

	len += snprintf(buf + len, PAGE_SIZE - len,  "XO_BUF dws status: ");
	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x ",
			dcxo->xo_bufs[i].dct_sta);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	if (!dcxo->impedance_support)
		goto DUMP_DESENSE_DWS;

	len += snprintf(buf + len, PAGE_SIZE - len, "XO_BUF DWS imepdance: ");
	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x ",
			dcxo->xo_bufs[i].dct_impedance);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

DUMP_DESENSE_DWS:
	if (!dcxo->de_sense_support)
		goto DUMP_DRV_CURR_DWS;
	len += snprintf(buf + len, PAGE_SIZE - len, "XO_BUF DWS de-sense: ");
	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x ",
			dcxo->xo_bufs[i].dct_desense);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

DUMP_DRV_CURR_DWS:

	return len;
}

int clkbuf_dcxo_dump_pmrc_en(char *buf)
{
	u32 val = 0;
	u32 tmp = 0;
	int len = 0;
	int ret = 0;
	u8 i = 0;

	if (!dcxo->pmrc_en_support)
		return 0;

	ret = clk_buf_read(&dcxo->hw, &dcxo->_dcxo_pmrc_en, &val);
	if (ret) {
		pr_notice("read dcxo pmrc_en failed\n");
		return len;
	}

	/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
	if (dcxo->spmi_rw) {
		ret = clk_buf_read_with_ofs(
				&dcxo->hw,
				&dcxo->_dcxo_pmrc_en,
				&tmp,
				1);
		if (ret) {
			pr_notice("read dcxo pmrc_en ofs: 1 failed\n");
			return len;
		}
		val |= (tmp << 8);
	}
	len += snprintf(buf + len,
			PAGE_SIZE - len,
			"dcxo pmrc_en: 0x%x\n",
			val);

	for (i = 0; i < dcxo->pmrc_en_num; i++) {
		pr_notice("dump pmrc_en id: %u\n", i);
		ret = clk_buf_read(
				&dcxo->pmrc_en[i].hw,
				&dcxo->pmrc_en[i]._pmrc_en,
				&val);
		if (ret) {
			pr_notice("read pmic pmrc_en failed, id: %u\n", i);
			return len;
		}

		/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
		if (dcxo->spmi_rw) {
			ret = clk_buf_read_with_ofs(
					&dcxo->pmrc_en[i].hw,
					&dcxo->pmrc_en[i]._pmrc_en,
					&tmp,
					1);
			if (ret) {
				pr_notice("read pmic pmrc_en ofs: 1 failed, id: %u\n",
						i);
				return len;
			}
			val |= (tmp << 8);
		}
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"pmic: %d, pmrc_en: 0x%x ",
				i, val);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static int clkbuf_dcxo_voter_store(u8 xo_idx, const char *arg1)
{
	struct xo_buf_ctl_cmd_t ctl_cmd = { .hw_id = CLKBUF_RC_VOTER };
	int voter = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	if (!clkbuf_dcxo_is_xo_in_use(xo_idx)) {
		pr_notice("xo_buf: %u not controllable\n", xo_idx);
		return -EINVAL;
	}

	if (!dcxo->voter_support) {
		pr_notice("xo voter not support\n");
		return 0;
	}

	if (kstrtoint(arg1, 16, &voter))
		return -EPERM;

	if (voter < 0) {
		ctl_cmd.cmd = CLKBUF_CMD_INIT;
		ctl_cmd.xo_voter_mask = 0;
	} else {
		ctl_cmd.cmd = CLKBUF_CMD_ON;
		ctl_cmd.xo_voter_mask = (u32)voter;
	}

	ret = clkbuf_dcxo_notify(xo_idx, &ctl_cmd);
	if (ret) {
		pr_notice("clkbuf cmd failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int dcxo_pmic_store(const u8 xo_id, const char *cmd)
{
	struct xo_buf_ctl_cmd_t xo_cmd;
	int ret = 0;
	const char * const *match_cmd = dcxo->valid_dcxo_cmd;

	xo_cmd.hw_id = CLKBUF_DCXO;

	while (*match_cmd) {
		if (!strcmp(*match_cmd, cmd))
			break;
		match_cmd += 1;
	}
	if (*match_cmd == NULL) {
		pr_notice("unknown DCXO command %s for xo id: %u!\n",
			cmd, xo_id);
		return -EPERM;
	}

	if (xo_id >= dcxo->xo_num) {
		pr_notice("xo_id out of range: %u in %u\n",
			 xo_id, dcxo->xo_num);
		return -EPERM;
	}

	if (!strcmp(cmd, "ON")) {
		xo_cmd.cmd = CLKBUF_CMD_ON;
	} else if (!strcmp(cmd, "OFF")) {
		xo_cmd.cmd = CLKBUF_CMD_OFF;
	} else if (!strcmp(cmd, "EN_BB")) {
		xo_cmd.cmd = CLKBUF_CMD_HW;
		xo_cmd.mode = DCXO_HW1_MODE;
	} else if (!strcmp(cmd, "SIG")) {
		xo_cmd.cmd = CLKBUF_CMD_HW;
		xo_cmd.mode = DCXO_HW2_MODE;
	} else if (!strcmp(cmd, "CO_BUF")) {
		xo_cmd.cmd = CLKBUF_CMD_HW;
		xo_cmd.mode = DCXO_CO_BUF_MODE;
	} else if (!strcmp(cmd, "INIT")) {
		xo_cmd.cmd = CLKBUF_CMD_INIT;
	} else {
		xo_cmd.cmd = CLKBUF_CMD_NOOP;
	}

	ret = clkbuf_dcxo_notify(xo_id, &xo_cmd);
	if (ret)
		pr_notice("clkbuf dcxo cmd failed\n");

	return ret;
}

int clkbuf_dcxo_pmic_store(const char *cmd, const char *arg1, const char *arg2)
{
	u32 val = 0;
	int ret = 0;
	int xo_id;

	if (!strcmp(cmd, "misc")) {
		if (dcxo->ops.dcxo_misc_store != NULL)
			ret = dcxo->ops.dcxo_misc_store(arg1, arg2);
		else
			pr_info("DCXO misc store function not implemented!!\n");
		return ret;
	}

	xo_id = clkbuf_dcxo_get_xo_id_by_name(arg1);
	if (xo_id < 0) {
		pr_notice("unknown xo name: %s\n", arg1);
		return -EPERM;
	}

	if (!strcmp(cmd, "impedence")) {
		if (!dcxo->impedance_support) {
			pr_notice("dcxo impedance not support\n");
			return -EPERM;
		}
		if (kstrtouint(arg2, 16, &val))
			return -EPERM;
		return set_xo_impedance(xo_id, val);
	} else if (!strcmp(cmd, "de-sense")) {
		if (!dcxo->de_sense_support) {
			pr_notice("dcxo driving de-sense not support\n");
			return -EPERM;
		}
		if (kstrtouint(arg2, 16, &val))
			return -EPERM;
		return set_xo_desense(xo_id, val);
	} else if (!strcmp(cmd, "drv-curr")) {
		if (!dcxo->drv_curr_support) {
			pr_notice("dcxo drv_curr not support\n");
			return -EPERM;
		}
		if (kstrtouint(arg2, 16, &val))
			return -EPERM;
		return set_xo_drvcurr(xo_id, val);
	} else if (!strcmp(cmd, "DCXO")) {
		return dcxo_pmic_store(xo_id, arg2);
	} else if (!strcmp(cmd, "XO_VOTER")) {
		return clkbuf_dcxo_voter_store(xo_id, arg2);
	}

	pr_notice("unknown command: %s, arg1: %s, arg2: %s\n", cmd, arg1, arg2);
	return -EPERM;
}

static int clkbuf_dcxo_dct_init_in_k(void)
{
	int ret = 0;
	int i;

	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		if (dcxo->impedance_support) {
			ret = set_xo_impedance(i,
				dcxo->xo_bufs[i].dct_impedance);
			if (ret)
				return ret;
		}

		if (dcxo->de_sense_support) {
			ret = set_xo_desense(i, dcxo->xo_bufs[i].dct_desense);
			if (ret)
				return ret;
		}

		if (dcxo->drv_curr_support) {
			ret = set_xo_drvcurr(i, dcxo->xo_bufs[i].dct_drv_curr);
			if (ret)
				return ret;
		}
	}

	return ret;
}

int clkbuf_dcxo_post_init(void)
{
	struct xo_buf_ctl_cmd_t xo_buf_cmd = { .hw_id = CLKBUF_HW_ALL };
	u32 val = 0;
	int ret = 0;
	int i;

	if (dcxo->do_init_in_k) {
		ret = clkbuf_dcxo_dct_init_in_k();
		if (ret)
			pr_notice("clkbuf init in kernel failed\n");
	}

	for (i = 0; i < dcxo->xo_num; i++) {
		if (!dcxo->xo_bufs[i].in_use)
			continue;
		if (!dcxo->xo_bufs[i].support) {
			xo_buf_cmd.cmd = CLKBUF_CMD_OFF;
			if (dcxo->voter_support)
				xo_buf_cmd.xo_voter_mask = 0;
			ret = clkbuf_dcxo_notify(i, &xo_buf_cmd);
			if (ret)
				pr_notice("clkbuf init command failed\n");
		}

		ret = clk_buf_read(&dcxo->hw, &dcxo->xo_bufs[i]._xo_en, &val);
		if (ret) {
			pr_notice("clkbuf read init xo en_m failed\n");
			return ret;
		}
		dcxo->xo_bufs[i].init_en = val;

		ret = clk_buf_read(&dcxo->hw, &dcxo->xo_bufs[i]._xo_mode, &val);
		if (ret) {
			pr_notice("clkbuf read xo mode failed\n");
			return ret;
		}
		dcxo->xo_bufs[i].init_mode = val;

		ret = clkbuf_dcxo_get_xo_en(i, &val);
		if (ret) {
			pr_notice("clkbuf read xo en sta failed\n");
			return ret;
		}
		dcxo->xo_bufs[i].sw_status = val;

		if (!dcxo->voter_support)
			continue;
		ret = clk_buf_read(&dcxo->hw, &dcxo->xo_bufs[i]._rc_voter,
				&val);
		if (ret) {
			pr_notice("clkbuf read voter reg failed\n");
			return ret;
		}
		dcxo->xo_bufs[i].init_rc_voter = val;
	}

	return ret;
}

static int clkbuf_dcxo_dts_pmrc_en_init(struct device_node *ctl_node)
{
	struct platform_device *pmic_dev = NULL;
	struct device_node *pmic_node = NULL;
	int ret = 0;
	u8 i = 0;

	ret = of_property_count_u32_elems(ctl_node, CLKBUF_PMRC_EN_ADDR);
	if (ret <= 0) {
		pr_notice("no pmrc_en addr specified.\n");
		return 0;
	}
	if (ret % 2) {
		pr_notice("pmic node/pmrc_en addr does not paired!\n");
		return ret;
	}
	dcxo->pmrc_en_num = ret / 2;
	pr_notice("pmrc_en_num: %u\n", dcxo->pmrc_en_num);

	if (!dcxo->pmrc_en_num)
		return 0;

	dcxo->pmrc_en = kcalloc(dcxo->pmrc_en_num,
				sizeof(struct pmic_pmrc_en),
				GFP_KERNEL);
	if (!dcxo->pmrc_en)
		return -ENOMEM;

	for (i = 0; i < dcxo->pmrc_en_num; i++) {
		pr_notice("initializing pmrc_en id: %u\n", i);
		pmic_node = of_parse_phandle(ctl_node,
					CLKBUF_PMRC_EN_ADDR,
					2 * i);
		if (!pmic_node) {
			pr_notice("pmic node not found id: %u\n", 2 * i);
			return -ENODEV;
		}

		pmic_dev = of_find_device_by_node(pmic_node);
		if (!pmic_dev) {
			pr_notice("pmic dev not found id: %u\n", 2 * i);
			return -ENODEV;
		}

		dcxo->pmrc_en[i].hw.base.map = dev_get_regmap(
							&pmic_dev->dev,
							NULL);
		if (!dcxo->pmrc_en[i].hw.base.map) {
			pr_notice("get regmap failed!!\n");
			if (of_device_is_compatible(pmic_dev->dev.of_node,
						"mediatek,mt6359p"))
				pr_notice("pmic is mt6359p\n");
		}

		ret = of_property_read_u32_index(ctl_node,
				CLKBUF_PMRC_EN_ADDR,
				2 * i + 1,
				&dcxo->pmrc_en[i]._pmrc_en.ofs);
		if (ret) {
			pr_notice("read pmrc_en addr failed id: %u\n",
					2 * i + 1);
			return ret;
		}

		if (dcxo->spmi_rw)
			dcxo->pmrc_en[i]._pmrc_en.mask = 0xFF;
		else
			dcxo->pmrc_en[i]._pmrc_en.mask = 0xFFFF;
		dcxo->pmrc_en[i]._pmrc_en.shift = 0;

		dcxo->pmrc_en[i].hw.is_pmic = true;
		dcxo->pmrc_en[i].hw.enable = true;
	}

	return ret;
}

static int clkbuf_dcxo_dts_init_dct(struct device_node *node)
{
	struct device_node *clkbuf_ctl;
	int ret = 0;
	int tmp = 0;
	int i;

	clkbuf_ctl = of_parse_phandle(node, CLKBUF_CTL_PHANDLE_NAME, 0);
	if (!clkbuf_ctl) {
		pr_notice("find clkbuf_ctl node failed with err: %d\n", ret);
		return -EFIND_DTS_ERR;
	}

	ret = of_property_count_u32_elems(clkbuf_ctl, CLKBUF_DCT_XO_QUANTITY);
	if (ret != dcxo->xo_num) {
		pr_notice("xo num not equal\n");
		pr_notice("xo num in dct\n", ret);
		pr_notice("xo num in hw driver\n", dcxo->xo_num);
		return -EXO_NUM_CONFIG_ERR;
	}

	for (i = 0; i < dcxo->xo_num; i++) {
		/* get dct xo config */
		/* if clkbuf is init at kernel, and xo config is 0 */
		/* xo will be disabled in post init phase */
		ret = of_property_read_u32_index(clkbuf_ctl,
				CLKBUF_DCT_XO_CONFIG, i, &tmp);
		if (ret) {
			pr_notice("read dct xo config%d failed\n", i);
			return ret;
		}
		dcxo->xo_bufs[i].support &= (tmp ? 1 : 0);
		dcxo->xo_bufs[i].dct_sta = tmp;

		if (dcxo->impedance_support) {
			/* get dct xo impedance setting */
			ret = of_property_read_u32_index(clkbuf_ctl,
					CLKBUF_DCT_XO_IMPEDANCE, i, &tmp);
			if (ret) {
				pr_notice("read dct xo impedance%d failed\n",
					i);
				return ret;
			}
			dcxo->xo_bufs[i].dct_impedance = tmp;
		}

		if (dcxo->de_sense_support) {
			/* get dct xo de-sense setting */
			ret = of_property_read_u32_index(clkbuf_ctl,
					CLKBUF_DCT_XO_DESNSE, i, &tmp);
			if (ret) {
				pr_notice("read dct xo desense%d failed\n", i);
				return ret;
			}
			dcxo->xo_bufs[i].dct_desense = tmp;
		}

		if (dcxo->drv_curr_support) {
			/* get dct drv-curr setting */
			ret = of_property_read_u32_index(clkbuf_ctl,
					CLKBUF_DCT_XO_DRVCURR, i, &tmp);
			if (ret) {
				pr_notice("read dct xo drv-curr%d failed\n", i);
				return ret;
			}
			dcxo->xo_bufs[i].dct_drv_curr = tmp;
		}
	}

	of_node_put(clkbuf_ctl);

	return ret;
}

static int clkbuf_dcxo_dts_init_xo_by_name_prop(struct device_node *node,
		int idx)
{
	struct device_node *ctl_node;
	char xo_en_prop[20] = {0};
	char buf[XO_NAME_LEN] = {0};
	char *pp = NULL;
	char *p = NULL;
	int ret = 0;
	int tmp = 0;

	if (!dcxo->xo_bufs[idx].in_use)
		return 0;

	ctl_node = of_parse_phandle(node, CLKBUF_CTL_PHANDLE_NAME, 0);
	if (!ctl_node) {
		pr_notice("get clkbuf_ctl node failed\n");
		return -EFIND_DTS_ERR;
	}
	strncpy(buf, dcxo->xo_bufs[idx].xo_name, XO_NAME_LEN);
	p = buf;
	strsep(&p, "_");
	if (!(*p))
		return 0;
	pp = p;
	while (*pp) {
		*pp = tolower(*pp);
		pp++;
	}
	strncpy(xo_en_prop, CLKBUF_XO_PROP, XO_NAME_LEN + 10);
	strncat(xo_en_prop, p, XO_NAME_LEN + 10);
	ret = of_property_read_u32(ctl_node, xo_en_prop, &tmp);
	if (ret)
		return ret;
	dcxo->xo_bufs[idx].support &= (tmp ? 1 : 0);
	of_node_put(ctl_node);

	return 0;
}

static int clkbuf_dcxo_dts_init_xo(struct device_node *node)
{
	u32 tmp = 0;
	int ret = 0;
	int i;

	ret = of_property_count_u32_elems(node, CLKBUF_XO_SUPPORT_PROP_NAME);
	if (ret != dcxo->xo_num) {
		pr_notice("support xo num are not equal\n");
		pr_notice("xo number in dts: %u\n", ret);
		pr_notice("xo number in hw driver: %u\n", dcxo->xo_num);
		return -EXO_NUM_CONFIG_ERR;
	}

	for (i = 0; i < dcxo->xo_num; i++) {
		/* get xo support or not */
		/* if xo not support, will disable xo at post init */
		ret = of_property_read_u32_index(node,
				CLKBUF_XO_SUPPORT_PROP_NAME, i, &tmp);
		if (ret) {
			pr_notice("get xo %d support with err: %d\n", i, ret);
			return ret;
		}
		dcxo->xo_bufs[i].support = tmp;

		/* get xo name */
		ret = of_property_read_string_index(node, CLKBUF_XO_NAME_PROP,
				i, &dcxo->xo_bufs[i].xo_name);
		if (ret) {
			pr_notice("get xo %d name with err: %d\n", i, ret);
			return ret;
		}

		/* get xo sw allow control */
		/* if xo not sw controllable, it can not be debug by command */
		ret = of_property_read_u32_index(node,
				CLKBUF_XO_ALLOW_CONTROL,
				i, &tmp);
		if (ret) {
			pr_notice("get xo %d controllable with err: %d\n", i, ret);
			return ret;
		}
		dcxo->xo_bufs[i].controllable = tmp;

		clkbuf_dcxo_dts_init_xo_by_name_prop(node, i);
	}

	return ret;
}

static int clkbuf_dcxo_dts_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *ctl_node;
	int ret = 0;
	u32 tmp = 0;

	ctl_node = of_parse_phandle(node, CLKBUF_CTL_PHANDLE_NAME, 0);
	if (!ctl_node) {
		pr_notice("find clkbuf_ctl node failed with err: %d\n", ret);
		return -EFIND_DTS_ERR;
	}

	dcxo->voter_support = of_property_read_bool(node,
			CLKBUF_VOTER_SUPPORT);
	dcxo->de_sense_support = of_property_read_bool(node,
			DCXO_DESENSE_SUPPORT);
	dcxo->impedance_support = of_property_read_bool(node,
			DCXO_IMPEDANCE_SUPPORT);
	dcxo->drv_curr_support = of_property_read_bool(node,
			DCXO_DRV_CURR_SUPPORT);
	dcxo->do_init_in_k = of_property_read_bool(ctl_node,
			CLKBUF_INIT_AT_KERNEL);
	dcxo->spmi_rw = of_property_read_bool(node,
			DCXO_SPMI_RW);
	dcxo->pmrc_en_support = of_property_read_bool(node,
			DCXO_PMRC_EN_SUPPORT);

	ret = of_property_read_u32(node, CLKBUF_XO_MODE_NUM, &tmp);
	if (ret) {
		pr_notice("clkbuf get xo mode num failed: %d\n", ret);
		return ret;
	}
	dcxo->xo_mode_num = tmp;

	ret = clkbuf_dcxo_dts_init_xo(node);
	if (ret)
		return ret;

	if (dcxo->do_init_in_k) {
		ret = clkbuf_dcxo_dts_init_dct(node);
		if (ret)
			return ret;
	}

	if (dcxo->pmrc_en_support) {
		ret = clkbuf_dcxo_dts_pmrc_en_init(ctl_node);
		if (ret)
			return ret;
	}

	of_node_put(ctl_node);

	return ret;
}

static int clkbuf_dcxo_base_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mt6397_chip *chip;

	if (of_property_read_bool(node, CLKBUF_PMIC_CENTRAL_BASE)) {
		/* get regmap by old way, use pmic main chip */
		chip = dev_get_drvdata(pdev->dev.parent);
		if (!chip || !chip->regmap)
			return -ENO_PMIC_REGMAP_FOUND;
		dcxo->hw.base.map = chip->regmap;
	} else {
		/* get regmap by new way, directly from device parent */
		dcxo->hw.base.map = dev_get_regmap(pdev->dev.parent, NULL);
	}

	return 0;
}

static int clkbuf_xo_buf_on_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	int ret = 0;

	if (ctl_cmd->hw_id != CLKBUF_DCXO && ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	ret = set_xo_mode(xo_idx, 0);
	if (ret)
		return ret;

	ret = set_xo_en(xo_idx, 1);
	if (ret)
		return ret;
	udelay(400);

	return ret;
}

static int clkbuf_xo_buf_off_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	int ret = 0;

	if (ctl_cmd->hw_id != CLKBUF_DCXO && ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	ret = set_xo_en(xo_idx, 0);
	if (ret)
		return ret;
	ret = set_xo_mode(xo_idx, 0);

	return ret;
}

static int clkbuf_xo_buf_sw_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (ctl_cmd->hw_id != CLKBUF_DCXO && ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	return set_xo_mode(xo_idx, 0);
}

static int clkbuf_xo_buf_hw_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (ctl_cmd->hw_id != CLKBUF_DCXO && ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	return set_xo_mode(xo_idx, ctl_cmd->mode);
}

static int clkbuf_xo_buf_init_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	int ret = 0;

	if (ctl_cmd->hw_id != CLKBUF_DCXO && ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	ret = set_xo_en(xo_idx, dcxo->xo_bufs[xo_idx].init_en);
	if (ret)
		return ret;
	ret = set_xo_mode(xo_idx, dcxo->xo_bufs[xo_idx].init_mode);

	return ret;
}

static int clkbuf_xo_buf_show_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	char *buf = NULL;
	int len = 0;
	int ret = 0;
	u32 en = 0;

	if (ctl_cmd->hw_id != CLKBUF_DCXO && ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	buf = ctl_cmd->buf;
	if (!buf) {
		pr_notice("Null output buffer\n");
		return -EPERM;
	}

	ret = clkbuf_dcxo_get_xo_en(xo_idx, &en);
	if (ret) {
		pr_notice("get %s en failed\n", dcxo->xo_bufs[xo_idx].xo_name);
		return ret;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%s en: %u\n",
		dcxo->xo_bufs[xo_idx].xo_name, en);

	pr_notice("%s\n", buf);

	return 0;
}

static int clkbuf_set_xo_voter(u8 xo_idx, u32 mask)
{
	short no_lock = 0;
	int ret = 0;

	ret = clkbuf_xo_sanity_check(xo_idx);
	if (ret)
		return ret;

	pr_notice("xo_id: %u, mask: 0x%x\n", xo_idx, mask);

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&dcxo->lock);

	ret = clk_buf_write(&dcxo->hw, &dcxo->xo_bufs[xo_idx]._rc_voter, mask);
	if (ret) {
		pr_notice("set xo voter failed: %d\n", ret);
		goto SET_XO_VOTER_DONE;
	}

	/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
	if (dcxo->spmi_rw) {
		ret = clk_buf_write_with_ofs(&dcxo->hw,
				&dcxo->xo_bufs[xo_idx]._rc_voter,
				mask,
				1);
		if (ret) {
			pr_notice("set xo voter failed: %d\n", ret);
			goto SET_XO_VOTER_DONE;
		}
	}

SET_XO_VOTER_DONE:
	if (!no_lock)
		mutex_unlock(&dcxo->lock);

	return ret;
}

static int clkbuf_xo_voter_off_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (ctl_cmd->hw_id != CLKBUF_RC_VOTER
			&& ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	return clkbuf_set_xo_voter(xo_idx, 0);
}

static int clkbuf_xo_voter_on_ctrl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (ctl_cmd->hw_id != CLKBUF_RC_VOTER
			&& ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	return clkbuf_set_xo_voter(xo_idx, ctl_cmd->xo_voter_mask);
}

static int clkbuf_xo_voter_init_ctrl(u8 xo_idx,
		struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	if (ctl_cmd->hw_id != CLKBUF_RC_VOTER
			&& ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	return clkbuf_set_xo_voter(xo_idx,
		dcxo->xo_bufs[xo_idx].init_rc_voter);
}

static int clkbuf_xo_voter_show_ctrl(u8 xo_idx,
		struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	char *buf = NULL;
	int len = 0;
	int ret = 0;
	u32 voter = 0;
	u32 val = 0;

	if (ctl_cmd->hw_id != CLKBUF_RC_VOTER
			&& ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	buf = ctl_cmd->buf;
	if (!buf) {
		pr_notice("Null output buffer\n");
		return -EPERM;
	}
	/* TODO: check again */
	len = strlen(buf);

	if (!dcxo->voter_support) {
		pr_notice("voter not support\n");
		return 0;
	}
	ret = clk_buf_read(&dcxo->hw, &dcxo->xo_bufs[xo_idx]._rc_voter,
			&voter);
	/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
	if (dcxo->spmi_rw)
		ret |= clk_buf_read_with_ofs(&dcxo->hw,
				&dcxo->xo_bufs[xo_idx]._rc_voter,
				&val,
				1);

	if (ret) {
		pr_notice("clkbuf read voter reg failed\n");
		return ret;
	}

	if (dcxo->spmi_rw)
		voter |= (val << 8);

	len += snprintf(buf + len, PAGE_SIZE - len, "%s voter: 0x%x\n",
		dcxo->xo_bufs[xo_idx].xo_name, voter);

	pr_notice("%s\n", buf);

	return 0;
}

static void clkbuf_dcxo_init_xo_op(struct xo_buf_t *xo_buf)
{
	xo_buf->xo_buf_ctrl_op.clk_buf_on_ctrl = clkbuf_xo_buf_on_ctrl;
	xo_buf->xo_buf_ctrl_op.clk_buf_off_ctrl = clkbuf_xo_buf_off_ctrl;
	xo_buf->xo_buf_ctrl_op.clk_buf_sw_ctrl = clkbuf_xo_buf_sw_ctrl;
	xo_buf->xo_buf_ctrl_op.clk_buf_hw_ctrl = clkbuf_xo_buf_hw_ctrl;
	xo_buf->xo_buf_ctrl_op.clk_buf_init_ctrl = clkbuf_xo_buf_init_ctrl;
	xo_buf->xo_buf_ctrl_op.clk_buf_show_ctrl = clkbuf_xo_buf_show_ctrl;

	if (!dcxo->voter_support)
		return;
	xo_buf->xo_voter_ctrl_op.clk_buf_on_ctrl = clkbuf_xo_voter_on_ctrl;
	xo_buf->xo_voter_ctrl_op.clk_buf_off_ctrl = clkbuf_xo_voter_off_ctrl;
	xo_buf->xo_voter_ctrl_op.clk_buf_init_ctrl = clkbuf_xo_voter_init_ctrl;
	xo_buf->xo_voter_ctrl_op.clk_buf_show_ctrl = clkbuf_xo_voter_show_ctrl;
}

static int clkbuf_dcxo_hw_init(struct platform_device *clkbuf_pdev)
{
	int ret = 0;

	ret = clk_buf_get_pmic_id(clkbuf_pdev);
	if (ret < 0) {
		pr_notice("clkbuf can not get any pmic id: %d\n", ret);
		return ret;
	}

	dcxo = dcxos[ret];

	return 0;
}

int clkbuf_dcxo_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;
	int i;

	if (dcxo)
		return -EHW_ALREADY_INIT;

	if (!of_property_read_bool(node, ENABLE_PROP_NAME)) {
		pr_notice("clkbuf dcxo does not enabled by dts\n");
		return -EHW_NOT_SUPPORT;
	}

	ret = clkbuf_dcxo_hw_init(pdev);

	ret = clkbuf_dcxo_base_init(pdev);
	if (ret)
		goto INIT_FAILED;

	ret = clkbuf_dcxo_dts_init(pdev);
	if (ret)
		goto INIT_FAILED;

	for (i = 0; i < dcxo->xo_num; i++) {
		INIT_LIST_HEAD(&dcxo->xo_bufs[i].xo_buf_ctl_head);
		clkbuf_dcxo_init_xo_op(&dcxo->xo_bufs[i]);
		ret = clkbuf_dcxo_register_op(i,
			&dcxo->xo_bufs[i].xo_buf_ctrl_op);
		if (ret)
			goto INIT_FAILED;
		if (!dcxo->voter_support)
			continue;
		ret = clkbuf_dcxo_register_op(i,
			&dcxo->xo_bufs[i].xo_voter_ctrl_op);
		if (ret)
			goto INIT_FAILED;
	}

	mutex_init(&dcxo->lock);

	dcxo->hw.enable = true;

	return ret;
INIT_FAILED:
	pr_notice("clkbuf dcxo init failed with err: %d\n", ret);
	return ret;
}
