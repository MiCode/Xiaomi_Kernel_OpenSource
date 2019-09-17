// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mmqos_wrapper.h"

static DEFINE_MUTEX(bw_mutex);
static u32 max_bw_bound;

struct wrapper_data {
	const u32 max_ostd;
	const u32 icc_dst_id;
};

static struct wrapper_data wrapper_data_mt6779 = {
	.max_ostd = 40,
	.icc_dst_id = SLAVE_COMMON(0),
};

static struct wrapper_data *mmqos_wrapper;
static struct device *dev;

static BLOCKING_NOTIFIER_HEAD(hrt_bw_throttle_notifier);

s32 mm_qos_add_request(struct list_head *owner_list,
	struct mm_qos_request *req, u32 smi_master_id)
{
	if (!req) {
		pr_notice("mm_add: Invalid req pointer\n");
		return -EINVAL;
	}

	if (req->init) {
		pr_notice("mm_add(0x%08x) req is init\n", req->master_id);
		return -EINVAL;
	}

	req->master_id = smi_master_id;
	req->bw_value = 0;
	req->hrt_value = 0;
	INIT_LIST_HEAD(&(req->owner_node));
	list_add_tail(&(req->owner_node), owner_list);
	req->icc_path = icc_get(dev, smi_master_id, mmqos_wrapper->icc_dst_id);
	if (IS_ERR_OR_NULL(req->icc_path)) {
		pr_notice("get icc path fail: src=%#x dst=%#x\n",
			smi_master_id, mmqos_wrapper->icc_dst_id);
		return -EINVAL;
	}
	req->init = true;

	return 0;
}
EXPORT_SYMBOL_GPL(mm_qos_add_request);

s32 mm_qos_set_request(struct mm_qos_request *req, u32 bw_value,
	u32 hrt_value, u32 comp_type)
{
	if (!req)
		return -EINVAL;

	if (!req->init || comp_type >= BW_COMP_END) {
		pr_notice("mm_set(0x%08x) invalid req\n", req->master_id);
		return -EINVAL;
	}

	if (bw_value != MTK_MMQOS_MAX_BW && hrt_value != MTK_MMQOS_MAX_BW &&
		(bw_value > max_bw_bound || hrt_value > max_bw_bound)) {
		pr_notice("mm_set(0x%08x) invalid bw=%d hrt=%d bw_bound=%d\n",
			req->master_id, bw_value,
			hrt_value, max_bw_bound);
		return -EINVAL;
	}

	if (req->hrt_value == hrt_value &&
		req->bw_value == bw_value &&
		req->comp_type == comp_type) {
		return 0;
	}

	mutex_lock(&bw_mutex);
	req->updated = true;
	req->hrt_value = hrt_value;
	req->bw_value = bw_value;
	req->comp_type = comp_type;
	mutex_unlock(&bw_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(mm_qos_set_request);

s32 mm_qos_set_bw_request(struct mm_qos_request *req,
	u32 bw_value, s32 comp_type)
{
	return mm_qos_set_request(req, bw_value, req->hrt_value, comp_type);
}
EXPORT_SYMBOL_GPL(mm_qos_set_bw_request);

s32 mm_qos_set_hrt_request(struct mm_qos_request *req,
	u32 hrt_value)
{
	return mm_qos_set_request(req, req->bw_value, hrt_value, 0);
}
EXPORT_SYMBOL_GPL(mm_qos_set_hrt_request);


#define COMP_BW_UPDATE(bw_value) ((bw_value) * 7 / 10)
static inline u32 get_comp_value(u32 bw_value, u32 comp_type)
{
	if (comp_type == BW_COMP_DEFAULT)
		return COMP_BW_UPDATE(bw_value);

	return bw_value;
}

void mm_qos_update_all_request(struct list_head *owner_list)
{
	struct mm_qos_request *req = NULL;
	u64 comp_bw;

	if (!owner_list || list_empty(owner_list)) {
		pr_notice("%s: owner_list is invalid\n", __func__);
		return;
	}

	mutex_lock(&bw_mutex);
	list_for_each_entry(req, owner_list, owner_node) {
		if (!req->updated)
			continue;
		comp_bw = get_comp_value(req->bw_value, req->comp_type);
		icc_set_bw(req->icc_path,
			(req->bw_value == MTK_MMQOS_MAX_BW)
			? MTK_MMQOS_MAX_BW : MBps_to_icc(comp_bw),
			(req->hrt_value == MTK_MMQOS_MAX_BW)
			? MTK_MMQOS_MAX_BW : MBps_to_icc(req->hrt_value));
		req->updated = false;
	}
	mutex_unlock(&bw_mutex);
}
EXPORT_SYMBOL_GPL(mm_qos_update_all_request);

void mm_qos_remove_all_request(struct list_head *owner_list)
{
	struct mm_qos_request *temp, *req = NULL;

	mutex_lock(&bw_mutex);
	list_for_each_entry_safe(req, temp, owner_list, owner_node) {
		pr_notice("mm_del(0x%08x)\n", req->master_id);
		list_del(&(req->owner_node));
		req->init = false;
	}
	mutex_unlock(&bw_mutex);
}
EXPORT_SYMBOL_GPL(mm_qos_remove_all_request);

void mm_qos_update_all_request_zero(struct list_head *owner_list)
{
	struct mm_qos_request *req = NULL;

	list_for_each_entry(req, owner_list, owner_node) {
		mm_qos_set_request(req, 0, 0, 0);
	}
	mm_qos_update_all_request(owner_list);
}
EXPORT_SYMBOL_GPL(mm_qos_update_all_request_zero);

s32 mm_hrt_get_available_hrt_bw(u32 master_id)
{
	/* Todo: Check master_id to define HRT type */
	return mtk_mmqos_get_avail_hrt_bw(HRT_DISP);
}
EXPORT_SYMBOL_GPL(mm_hrt_get_available_hrt_bw);

s32 mm_hrt_add_bw_throttle_notifier(struct notifier_block *nb)
{
	return mtk_mmqos_register_bw_throttle_notifier(nb);
}
EXPORT_SYMBOL_GPL(mm_hrt_add_bw_throttle_notifier);

s32 mm_hrt_remove_bw_throttle_notifier(struct notifier_block *nb)
{
	return mtk_mmqos_unregister_bw_throttle_notifier(nb);
}
EXPORT_SYMBOL_GPL(mm_hrt_remove_bw_throttle_notifier);

s32 get_virtual_port(enum virtual_source_id id)
{
	switch (id) {
	case VIRTUAL_DISP:
		return PORT_VIRTUAL_DISP;
	case VIRTUAL_CCU_COMMON:
		return PORT_VIRTUAL_CCU_COMMON;
	default:
		pr_notice("invalid source id:%u\n", id);
		return -1;
	}
}
EXPORT_SYMBOL_GPL(get_virtual_port);

static const struct of_device_id of_mmqos_wrapper_match_tbl[] = {
	{
		.compatible = "mediatek,mt6779-mmqos-wrapper",
		.data = &wrapper_data_mt6779,
	},
	{}
};

static int mmqos_wrapper_probe(struct platform_device *pdev)
{
	dev = &pdev->dev;
	mmqos_wrapper =
		(struct wrapper_data *)of_device_get_match_data(&pdev->dev);

	/* 256:Write BW, 2: HRT */
	max_bw_bound = mmqos_wrapper->max_ostd * 256 * 2;

	return 0;
}

static struct platform_driver mmqos_wrapper_drv = {
	.probe = mmqos_wrapper_probe,
	.driver = {
		.name = "mtk-mmqos-wrapper",
		.owner = THIS_MODULE,
		.of_match_table = of_mmqos_wrapper_match_tbl,
	},
};

static int __init mtk_mmqos_wrapper_init(void)
{
	s32 status;

	status = platform_driver_register(&mmqos_wrapper_drv);
	if (status) {
		pr_notice(
			"Failed to register MMQoS wrapper driver(%d)\n",
			status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mmqos_wrapper_exit(void)
{
	platform_driver_unregister(&mmqos_wrapper_drv);
}

module_init(mtk_mmqos_wrapper_init);
module_exit(mtk_mmqos_wrapper_exit);

#define UT_MAX_REQUEST 6
static s32 qos_ut_case;
static struct list_head ut_req_list;
static bool ut_req_init;
struct mm_qos_request ut_req[UT_MAX_REQUEST] = {};
int mmqos_ut_set(const char *val, const struct kernel_param *kp)
{
	int result, value;
	u32 req_id, master;

	result = sscanf(val, "%d %d %i %d", &qos_ut_case,
		&req_id, &master, &value);
	if (result != 4) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	if (req_id >= UT_MAX_REQUEST) {
		pr_notice("invalid req_id: %u\n", req_id);
		return -EINVAL;
	}

	pr_notice("ut with (case_id,req_id,master,value)=(%d,%u,%u,%d)\n",
		qos_ut_case, req_id, master, value);
	if (!ut_req_init) {
		INIT_LIST_HEAD(&ut_req_list);
		ut_req_init = true;
	}
	switch (qos_ut_case) {
	case 0:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value, 0, BW_COMP_NONE);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 1:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value, value, BW_COMP_NONE);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 2:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_bw_request(&ut_req[req_id], value, BW_COMP_NONE);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 3:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_hrt_request(&ut_req[req_id], value);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 4:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value, 0, BW_COMP_DEFAULT);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 5:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value,
			value, BW_COMP_DEFAULT);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case -1:
		mm_qos_remove_all_request(&ut_req_list);
		break;
	case -2:
		mm_qos_update_all_request_zero(&ut_req_list);
		break;
	default:
		pr_notice("invalid case_id: %d\n", qos_ut_case);
		break;
	}

	pr_notice("Call SMI Dump API Begin\n");
	/* smi_debug_bus_hang_detect(false, "MMDVFS"); */
	pr_notice("Call SMI Dump API END\n");
	return 0;
}

static struct kernel_param_ops qos_ut_case_ops = {
	.set = mmqos_ut_set,
	.get = param_get_int,
};
module_param_cb(qos_ut_case, &qos_ut_case_ops, &qos_ut_case, 0644);
MODULE_PARM_DESC(qos_ut_case, "force mmdvfs UT test case");

MODULE_DESCRIPTION("MTK MMQoS wrapper driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
