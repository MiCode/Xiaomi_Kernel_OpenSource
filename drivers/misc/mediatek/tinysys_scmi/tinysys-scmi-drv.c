// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/scmi_protocol.h>

#include "tinysys-scmi.h"


static struct scmi_tinysys_info_st *t_info;
static const struct scmi_tinysys_proto_ops *tinysys_ops;

f_handler_t cb_array[SCMI_TINYSYS_CB_MAX];

struct scmi_tinysys_info_st *get_scmi_tinysys_info(void)
{
	return t_info;
}
EXPORT_SYMBOL(get_scmi_tinysys_info);

int scmi_tinysys_common_set(const struct scmi_protocol_handle *ph, u32 feature_id,
	u32 p1, u32 p2, u32 p3, u32 p4, u32 p5)
{
	return tinysys_ops->common_set(ph, feature_id, p1, p2, p3, p4, p5);
}
EXPORT_SYMBOL(scmi_tinysys_common_set);

int scmi_tinysys_common_get(const struct scmi_protocol_handle *ph, u32 feature_id,
	u32 p1, struct scmi_tinysys_status *rvalue)
{
	return tinysys_ops->common_get(ph, feature_id, p1, rvalue);
}
EXPORT_SYMBOL(scmi_tinysys_common_get);

int scmi_tinysys_notifier_fn(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct scmi_tinysys_notifier_report *r = data;
	scmi_tinysys_report* report = (scmi_tinysys_report *)&(r->f_id);
	f_handler_t func;

	/* pr_notice("scmi notify report ktime:%ld f_id:%d p1:%d %d %d %d\n",
		r->timestamp, r->f_id, r->p1_status, r->p2_status, r->p3_status,
		r->p4_status);
	*/
	if(r->f_id < SCMI_TINYSYS_CB_MAX) {

		func = cb_array[r->f_id];
		if(func)
			func(r->f_id, report);
	}
	return NOTIFY_OK;
}

static struct notifier_block tinysys_nb = {
	.notifier_call = scmi_tinysys_notifier_fn,
};

int scmi_tinysys_event_notify(u32 feature_id, u32 notify_enable)
{
	int ret;
	int f_id = feature_id;
	const struct scmi_handle *handle = t_info->sdev->handle;

	if(notify_enable) {
		ret = handle->notify_ops->register_event_notifier(handle, SCMI_PROTOCOL_TINYSYS,
			SCMI_EVENT_TINYSYS_NOTIFIER, &f_id, &tinysys_nb);	//fix NULL
		if(ret)
			pr_notice("scmi register_event_notifier f_id:%d ret:%d\n",f_id, ret);

	} else {
		ret = handle->notify_ops->unregister_event_notifier(handle, SCMI_PROTOCOL_TINYSYS,
			SCMI_EVENT_TINYSYS_NOTIFIER, &f_id, &tinysys_nb);	//fix NULL
		if(ret)
			pr_notice("scmi unregister_event_notifier f_id:%d ret:%d\n",f_id, ret);

	}
	return ret;
}
EXPORT_SYMBOL(scmi_tinysys_event_notify);

void scmi_tinysys_register_event_notifier(u32 feature_id, f_handler_t hand)
{
	if(feature_id < SCMI_TINYSYS_CB_MAX)
	{
			cb_array[feature_id] = hand;
	} else {
		pr_notice("feature_id %d >= SCMI_TINYSYS_CB_MAX\n", feature_id);
	}
}
EXPORT_SYMBOL(scmi_tinysys_register_event_notifier);

#ifdef TINYSYS_SCMI_DEBUG
void test_callback(u32 feature_id, scmi_tinysys_report* r)
{
	pr_notice("scmi test_callback %d, %x %x %x %x %x\n",
		feature_id, r->feature_id, r->p1, r->p2, r->p3, r->p4);
}

static ssize_t tinysys_scmi_debug_store(struct device *kobj,
	struct device_attribute *attr, const char *buf, size_t n)
{

	int ret;
	int pro_id, f_id, p1, p2, p3, p4, p5;
	struct scmi_tinysys_info_st * tt;
	struct scmi_tinysys_status rvalue;
	char *prompt = "SCMI:";

	if (sscanf(buf, "%d %d %d %d %d %d %d", &pro_id, &f_id, &p1, &p2, &p3, &p4, &p5) != 7)
		return -EINVAL;
	pr_notice("%s pro_id:%d f_id:%d para:%d %d %d %d  %d\n", prompt, pro_id, f_id, p1, p2, p3, p4, p5);

	tt = get_scmi_tinysys_info();

	switch (pro_id) {
	case 0:
		ret = scmi_tinysys_common_set(tt->ph, f_id, p1, p2, p3, p4, p5);
		if(ret)
			pr_notice("%s ret = %d\n", prompt, ret);
		break;
	case 1:
		ret = scmi_tinysys_common_get(tt->ph, f_id, p1, &rvalue);
		if(ret)
			pr_notice("%s scmi_tinysys_common_get error ret = %d\n", prompt, ret);
		else
			pr_notice("%s scmi_tinysys_common_get r1:%d r2:%d r3:%d\n", prompt,  rvalue.r1, rvalue.r2, rvalue.r3);
		break;
	case 2:
		scmi_tinysys_register_event_notifier(f_id, (f_handler_t)test_callback);
		ret = scmi_tinysys_event_notify(f_id, 1);
		break;
	case 3:
		ret = scmi_tinysys_event_notify(f_id, 0);
		break;

	default:
		break;
	}
	return n;

}

static ssize_t tinysys_scmi_debug_show(struct device *dev
			, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "tinysys_scmi_debug_show\n");
}

DEVICE_ATTR_RW(tinysys_scmi_debug);
#endif


static int scmi_tinysys_probe(struct scmi_device *sdev)
{
	struct device *dev = &sdev->dev;
	const struct scmi_handle *handle = sdev->handle;
	struct scmi_protocol_handle *ph;

	if (!handle)
		return -ENODEV;

	scmi_tinysys_register();

	tinysys_ops = handle->devm_get_protocol(sdev, SCMI_PROTOCOL_TINYSYS, &ph);
	if (IS_ERR(tinysys_ops))
		return PTR_ERR(tinysys_ops);

	t_info = devm_kzalloc(dev, sizeof(*t_info), GFP_KERNEL);
	if (!t_info)
		return -ENOMEM;

	t_info->sdev = sdev;
	t_info->ph = ph;
#ifdef TINYSYS_SCMI_DEBUG
	if (device_create_file(dev, &dev_attr_tinysys_scmi_debug))
		pr_notice("tinysys scmi debug ret fail\n");
#endif

	return 0;
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_TINYSYS, "tinysys" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_tinysys_driver = {
	.name = "scmi-tinysys",
	.probe = scmi_tinysys_probe,
	.id_table = scmi_id_table,
};
module_scmi_driver(scmi_tinysys_driver);

MODULE_DESCRIPTION("SCMI tinysys driver");
MODULE_LICENSE("GPL v2");

