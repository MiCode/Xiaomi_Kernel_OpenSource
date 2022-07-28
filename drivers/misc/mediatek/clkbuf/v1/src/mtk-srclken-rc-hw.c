// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mtk_clkbuf_common.h"
#include "mtk_clkbuf_ctl.h"
#include "mtk-clkbuf-dcxo.h"
#include "mtk-srclken-rc-hw.h"
#if IS_ENABLED(CONFIG_MTK_SRCLKEN_RC_V1)
#include "mtk-srclken-rc-hw-v1.h"
#endif /* IS_ENABLED(CONFIG_MTK_SRCLKEN_RC_V1) */

#define SRCLKEN_RC_ENABLE_PROP_NAME		"mediatek,enable"
#define SRCLKEN_RC_SUBSYS_PROP_NAME		"mediatek,subsys-ctl"
#define SUBSYSID_NOT_FOUND              "UNSUPPORTED_SUBSYSID"

#define SRCLKEN_RC_SUBSYS_CTL_LEN		20

struct srclken_rc_hw rc_hw;

static char rc_dump_subsys_sta_name[21];
static char rc_dump_sta_reg_name[21];
static u8 rc_trace_dump_num = 2;

u8 srclken_rc_get_subsys_count(void)
{
	return rc_hw.subsys_num;
}

bool is_srclken_rc_init_done(void)
{
	return rc_hw.init_done;
}

const char *srclken_rc_get_subsys_name(u8 idx)
{
	if (idx > rc_hw.subsys_num)
		return SUBSYSID_NOT_FOUND;

	return rc_hw.subsys[idx].name;
}

int srclken_rc_subsys_ctrl(u8 idx, const char *mode)
{
	if (idx >= rc_hw.subsys_num)
		return -EINVAL;

	if (!strcmp(mode, "HW"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_HW, RC_NONE_REQ);
	else if (!strcmp(mode, "SW"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_NONE_REQ);
	else if (!strcmp(mode, "SW_OFF"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_LPM_REQ);
	else if (!strcmp(mode, "SW_FPM"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_FPM_REQ);
	else if (!strcmp(mode, "SW_LPM"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_LPM_VOTE_REQ);
	else if (!strcmp(mode, "INIT"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_INIT, RC_NONE_REQ);

	pr_notice("invalid mode: %s\n", mode);
	return -EPERM;
}

static int srclken_rc_dts_subsys_callback_init(struct device_node *node,
		struct srclken_rc_subsys *subsys)
{
	const char *str = NULL;
	char subsys_ctl[SRCLKEN_RC_SUBSYS_CTL_LEN];
	u8 i;
	int len;

	len = snprintf(subsys_ctl, SRCLKEN_RC_SUBSYS_CTL_LEN, "%s-ctl", subsys->name);
	if (len <= 0) {
		pr_notice("Fail to append XXX-ctl, errno: %d\n", len);
		return 0;
	}

	if (of_property_read_string(node, subsys_ctl, &str)) {
		pr_notice("no subsys_ctl node: %s found, skip\n", subsys_ctl);
		return 0;
	}

	for (i = 0; i < clk_buf_get_xo_num(); i++) {
		if (!strcmp(str, clk_buf_get_xo_name(i))) {
			__srclken_rc_xo_buf_callback_init(&subsys->xo_buf_ctl);
			clk_buf_register_xo_ctl_op(str, &subsys->xo_buf_ctl);
			return 0;
		}
	}

	pr_notice("cannot find subsys: %s to register %s callback\n",
		subsys->name, str);
	return -EXO_NOT_FOUND;
}

static int srclken_rc_dts_subsys_init(struct platform_device *pdev,
		struct device_node *node)
{
	int ret = 0;
	int i;

	ret = of_property_count_strings(node, SRCLKEN_RC_SUBSYS_PROP_NAME);
	if (ret <= 0) {
		pr_notice("get subsys count failed, ret: %d, hw count: %d\n",
			ret, rc_hw.subsys_num);
		return -EINVAL;
	}
	rc_hw.subsys_num = ret;

	rc_hw.subsys = devm_kmalloc(&pdev->dev,
			sizeof(struct srclken_rc_subsys) * rc_hw.subsys_num,
			GFP_KERNEL);
	if (!rc_hw.subsys) {
		pr_notice("allocate subsys name failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < rc_hw.subsys_num; i++) {
		ret = of_property_read_string_index(node,
				SRCLKEN_RC_SUBSYS_PROP_NAME,
				i, &rc_hw.subsys[i].name);
		if (ret) {
			pr_notice("get subsys name failed, idx: %d\n", i);
			return ret;
		}
		rc_hw.subsys[i].idx = i;
		srclken_rc_dts_subsys_callback_init(node, &rc_hw.subsys[i]);
	}

	return ret;
}

static int srclken_rc_dts_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	if (!of_property_read_bool(node, ENABLE_PROP_NAME)) {
		pr_notice("srclken_rc not enabled at dts\n");
		return -EHW_NOT_SUPPORT;
	}

	ret = srclken_rc_dts_subsys_init(pdev, node);
	if (ret)
		return ret;

	return ret;
}

int clk_buf_voter_ctrl_by_id(const uint8_t subsys_id, enum RC_CTRL_CMD rc_req)
{
	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	if (rc_req >= MAX_RC_REQ_NUM) {
		pr_notice("rc_req exceeds MAX_RC_REQ_NUM!\n");
		return -EINVAL;
	}

	pr_debug("[%s] Subsys %u change RC mode to %s\n", __func__, subsys_id, rc_req_list[rc_req]);
	return srclken_rc_subsys_ctrl(subsys_id, rc_req_list[rc_req]);
}
EXPORT_SYMBOL(clk_buf_voter_ctrl_by_id);

int srclken_dump_sta_log(void)
{
	struct xo_buf_ctl_cmd_t cmd = {
		.hw_id = CLKBUF_RC_SUBSYS,
		.cmd = CLKBUF_CMD_SHOW,
	};
	char *buf = NULL;
	int ret = 0;
	u32 val = 0;
	u8 i;

	if (!rc_hw.init_done) {
		pr_notice("clkbuf not init yet\n");
		return -ENODEV;
	}

	buf = vmalloc(PAGE_SIZE);
	if (!buf)
		return -ENOMEM;

	cmd.buf = buf;

	for (i = 0; i < clkbuf_dcxo_get_xo_num(); i++) {
		if (!clkbuf_dcxo_is_xo_in_use(i))
			continue;

		if (clkbuf_dcxo_get_xo_en(i, &val)) {
			pr_notice("get xo_buf%u en failed\n", i);
			continue;
		}

		if (val) {
			pr_notice("%s is on\n", clkbuf_dcxo_get_xo_name(i));
			ret = clkbuf_dcxo_notify(i, &cmd);
		}

		if (ret)
			pr_notice("get xo_buf%u srlkcen_rc status failed\n", i);
	}

	vfree(buf);

	return 0;
}
EXPORT_SYMBOL(srclken_dump_sta_log);

static int __srclken_rc_dump_all_cfg(char *buf)
{
	int len = 0;
	int ret = 0;
	u32 val = 0;
	u32 i;

	for (i = 0; i < srclken_rc_get_cfg_count(); i++) {
		ret = srclken_rc_get_cfg_val(srclken_rc_get_cfg_name(i), &val);
		if (ret)
			continue;

		len += snprintf(buf + len, PAGE_SIZE - len, "%s= 0x%x\n",
				srclken_rc_get_cfg_name(i),
				val);
	}

	return len;
}

int srclken_dump_cfg_log(void)
{
	char *buf = NULL;
	int len = 0;

	if (!rc_hw.init_done) {
		pr_notice("clkbuf HW not init yet\n");
		return -ENODEV;
	}

	buf = vmalloc(CLKBUF_STATUS_INFO_SIZE);
	if (!buf)
		return -ENOMEM;

	len += __srclken_rc_dump_all_cfg(buf);
	if (len <= 0) {
		vfree(buf);
		return -EAGAIN;
	}

	pr_notice("%s\n", buf);

	vfree(buf);

	return 0;
}
EXPORT_SYMBOL(srclken_dump_cfg_log);

static int __rc_dump_trace(char *buf, u32 buf_size)
{
	int len = 0;
	u8 i;

	for (i = 0; i < rc_get_trace_num() && i < rc_trace_dump_num; i++) {
		len += srclken_rc_dump_trace(i, buf + len, buf_size - len);
		len += srclken_rc_dump_time(i, buf + len, buf_size - len);
	}

	return len;
}

int srclken_dump_last_sta_log(void)
{
	char *buf = NULL;
	int len = 0;

	if (!rc_hw.init_done) {
		pr_notice("clkbuf HW not init yet\n");
		return -ENODEV;
	}

	buf = vmalloc(CLKBUF_STATUS_INFO_SIZE);
	if (!buf)
		return -ENOMEM;

	len += __rc_dump_trace(buf, CLKBUF_STATUS_INFO_SIZE);
	if (len <= 0) {
		vfree(buf);
		return -EAGAIN;
	}

	pr_notice("%s\n", buf);

	vfree(buf);

	return 0;
}
EXPORT_SYMBOL(srclken_dump_last_sta_log);

ssize_t rc_cfg_ctl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	return __srclken_rc_dump_all_cfg(buf);
}

ssize_t rc_trace_ctl_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[11] = {0};
	u32 val = 0;

	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	if (sscanf(buf, "%10s %u", cmd, &val) != 2)
		return -EPERM;

	if (!strcmp(cmd, "TRACE_NUM")) {
		rc_trace_dump_num = val;
		return count;
	}

	pr_notice("unknown cmd: %s, val %u\n", cmd, val);
	return -EPERM;
}

ssize_t rc_trace_ctl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	return __rc_dump_trace(buf, PAGE_SIZE);
}

ssize_t rc_subsys_ctl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	u8 i;

	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
			"available subsys: ");

	for (i = 0; i < srclken_rc_get_subsys_count(); i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%s, ", srclken_rc_get_subsys_name(i));

	len -= 2;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"\navailable control: HW/SW/SW_OFF/SW_FPM/SW_BBLPM\n");

	return len;
}

ssize_t rc_subsys_ctl_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char name[21];
	char mode[11];
	u8 i;

	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	if (sscanf(buf, "%20s %10s", name, mode) != 2)
		return -EPERM;

	for (i = 0; i < srclken_rc_get_subsys_count(); i++)
		if (!strcmp(srclken_rc_get_subsys_name(i), name))
			srclken_rc_subsys_ctrl(i, mode);

	return count;
}

ssize_t rc_subsys_sta_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	if (sscanf(buf, "%20s", rc_dump_subsys_sta_name) != 1)
		return -EPERM;

	return count;
}

static ssize_t __rc_subsys_sta_show(u8 idx, char *buf)
{
	int len = 0;

	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"[%s] -\n", srclken_rc_get_subsys_name(idx));

	len += srclken_rc_dump_subsys_sta(idx, buf + len);

	return len;
}

ssize_t rc_subsys_sta_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	u8 i;

	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	if (!strcmp(rc_dump_subsys_sta_name, "ALL")) {
		for (i = 0; i < srclken_rc_get_subsys_count(); i++)
			len += __rc_subsys_sta_show(i, buf + len);

		return len;
	}
	for (i = 0; i < srclken_rc_get_subsys_count(); i++) {
		if (!strcmp(rc_dump_subsys_sta_name,
				srclken_rc_get_subsys_name(i))) {
			len += __rc_subsys_sta_show(i, buf + len);

			return len;
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"unknown subsys name: %s\n",
		rc_dump_subsys_sta_name);

	return len;
}

ssize_t rc_sta_reg_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	if (sscanf(buf, "%20s", rc_dump_sta_reg_name) != 1)
		return -EINVAL;

	return count;
}

ssize_t rc_sta_reg_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	if (!rc_hw.init_done) {
		pr_notice("RC HW not init yet\n");
		return -ENODEV;
	}

	len += srclken_rc_dump_sta(rc_dump_sta_reg_name, buf + len);

	return len;
}

static int mtk_srclken_rc_probe(struct platform_device *pdev)
{
	int ret = 0, i;

	ret = srclken_rc_dts_init(pdev);
	if (ret) {
		pr_notice("dts init failed with err: %d\n", ret);
		goto RC_INIT_FAILED;
	}

	if (srclken_rc_hw_init(pdev)) {
		pr_notice("srclken_rc hw init failed\n");
		goto RC_INIT_FAILED;
	}

	/* Post init */
	for (i = 0; i < rc_hw.subsys_num; i++) {
		ret = srclken_rc_get_subsys_req_mode(i,
				&rc_hw.subsys[i].init_mode);
		if (ret) {
			pr_notice("srclken_rc get subsys req mode failed\n");
			return ret;
		}

		ret = srclken_rc_get_subsys_sw_req(i, &rc_hw.subsys[i].init_req);
		if (ret) {
			pr_notice("srclken_rc get subsys sw req failed\n");
			return ret;
		}
	}

	rc_hw.init_done = true;
	return 0;

RC_INIT_FAILED:
	rc_hw.init_done = false;
	return ret;
}

static const struct platform_device_id mtk_srclken_rc_ids[] = {
	{"mtk-srclken-rc", 0},
	{ /*sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mtk_srclken_rc_ids);

static const struct of_device_id mtk_srclken_rc_of_match[] = {
	{
		.compatible = "mediatek,srclken-rc",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_srclken_rc_of_match);

static struct platform_driver mtk_srclken_rc_driver = {
	.driver = {
		.name = "mtk-srclken-rc",
		.of_match_table = of_match_ptr(mtk_srclken_rc_of_match),
	},
	.probe = mtk_srclken_rc_probe,
	.id_table = mtk_srclken_rc_ids,
};

int srclken_rc_init(void)
{
	return platform_driver_register(&mtk_srclken_rc_driver);
}

void srclken_rc_exit(void)
{
	platform_driver_unregister(&mtk_srclken_rc_driver);
}

MODULE_AUTHOR("Ren-Ting Wang <ren-ting.wang@mediatek.com");
MODULE_DESCRIPTION("SOC Driver for MediaTek SRCLKEN_RC");
MODULE_LICENSE("GPL v2");
