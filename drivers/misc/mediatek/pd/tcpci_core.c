/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/tcpci_core.c
 * Richtek TypeC Port Control Interface Core Driver
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/list.h>

#ifdef CONFIG_RT7207_ADAPTER
#include "inc/pd_policy_engine.h"
#endif /* CONFIG_RT7207_ADAPTER */
#include "inc/tcpci.h"

#ifdef CONFIG_USB_POWER_DELIVERY
#include "pd_dpm_prv.h"
#endif /* CONFIG_USB_POWER_DELIVERY */

static ssize_t tcpc_show_property(struct device *dev,
				  struct device_attribute *attr, char *buf);
static ssize_t tcpc_store_property(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

#define TCPC_DEVICE_ATTR(_name, _mode)					\
{									\
	.attr = { .name = #_name, .mode = _mode },			\
	.show = tcpc_show_property,					\
	.store = tcpc_store_property,					\
}

static struct class *tcpc_class;
EXPORT_SYMBOL_GPL(tcpc_class);

static struct device_type tcpc_dev_type;

static struct device_attribute tcpc_device_attributes[] = {
	TCPC_DEVICE_ATTR(role_def, S_IRUGO),
	TCPC_DEVICE_ATTR(rp_lvl, S_IRUGO),
	TCPC_DEVICE_ATTR(pd_test, 0666/*S_IRUGO | S_IWOTH*/),
	TCPC_DEVICE_ATTR(info, S_IRUGO),
	TCPC_DEVICE_ATTR(timer, S_IRUGO | S_IWOTH),
	TCPC_DEVICE_ATTR(caps_info, S_IRUGO),
#ifdef CONFIG_RT7207_ADAPTER
	TCPC_DEVICE_ATTR(test, 0666/*S_IRUGO | S_IWOTH*/),
#endif /* CONFIG_RT7207_ADAPTER */
};

enum {
	TCPC_DESC_ROLE_DEF = 0,
	TCPC_DESC_RP_LEVEL,
	TCPC_DESC_PD_TEST,
	TCPC_DESC_INFO,
	TCPC_DESC_TIMER,
	TCPC_DESC_CAP_INFO,
#ifdef CONFIG_RT7207_ADAPTER
	TCPC_DESC_TEST,
#endif /* CONFIG_RT7207_ADAPTER */
};

static struct attribute *__tcpc_attrs[ARRAY_SIZE(tcpc_device_attributes) + 1];
static struct attribute_group tcpc_attr_group = {
	.attrs = __tcpc_attrs,
};

static const struct attribute_group *tcpc_attr_groups[] = {
	&tcpc_attr_group,
	NULL,
};

static const char * const role_text[] = {
	"SNK Only",
	"SRC Only",
	"DRP",
	"Try.SRC",
	"Try.SNK",
};

static ssize_t tcpc_show_property(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct tcpc_device *tcpc = to_tcpc_device(dev);
	const ptrdiff_t offset = attr - tcpc_device_attributes;
	int i = 0;
#ifdef CONFIG_USB_POWER_DELIVERY
	int vmin, vmax, ioper;
#endif /* CONFIG_USB_POWER_DELIVERY */

	switch (offset) {
#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_CAP_INFO:
		snprintf(buf+strlen(buf), 256, "%s = %d\n%s = %d\n",
			"local_selected_cap",
			tcpc->pd_port.local_selected_cap,
			"remote_selected_cap",
			tcpc->pd_port.remote_selected_cap);

		snprintf(buf+strlen(buf), 256, "%s\n",
				"local_src_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.local_src_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.local_src_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf+strlen(buf), 256, "%d %d %d\n",
				vmin, vmax, ioper);
		}
		snprintf(buf+strlen(buf), 256, "%s\n",
				"local_snk_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.local_snk_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.local_snk_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf+strlen(buf), 256, "%d %d %d\n",
				vmin, vmax, ioper);
		}
		snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_src_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.remote_src_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.remote_src_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf+strlen(buf), 256, "%d %d %d\n",
				vmin, vmax, ioper);
		}
		snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_snk_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.remote_snk_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.remote_snk_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf+strlen(buf), 256, "%d %d %d\n",
				vmin, vmax, ioper);
		}
		break;
#endif /* CONFIG_USB_POWER_DELIVERY */
	case TCPC_DESC_ROLE_DEF:
		snprintf(buf, 256, "%s\n", role_text[tcpc->desc.role_def]);
		break;
	case TCPC_DESC_RP_LEVEL:
		if (tcpc->typec_local_rp_level == TYPEC_CC_RP_DFT)
			snprintf(buf, 256, "%s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_1_5)
			snprintf(buf, 256, "%s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_3_0)
			snprintf(buf, 256, "%s\n", "3.0");
		break;
	case TCPC_DESC_PD_TEST:
		snprintf(buf,
			256, "%s\n%s\n%s\n%s\n%s\n", "1: Power Role Swap Test",
				"2: Data Role Swap Test", "3: Vconn Swap Test",
				"4: soft reset", "5: hard reset");
		break;
	case TCPC_DESC_INFO:
		i += snprintf(buf + i,
			256, "|^|==( %s info )==|^|\n", tcpc->desc.name);
		i += snprintf(buf + i,
			256, "role = %s\n", role_text[tcpc->desc.role_def]);
		if (tcpc->typec_local_rp_level == TYPEC_CC_RP_DFT)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_1_5)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_3_0)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "3.0");
		break;
	default:
		break;
	}
	return strlen(buf);
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t tcpc_store_property(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct tcpc_device *tcpc = to_tcpc_device(dev);
#ifdef CONFIG_USB_POWER_DELIVERY
	struct tcpm_power_cap cap;
#endif /* CONFIG_USB_POWER_DELIVERY */
	const ptrdiff_t offset = attr - tcpc_device_attributes;
	int ret;
	long int val;

	switch (offset) {
#ifdef CONFIG_RT7207_ADAPTER
	case TCPC_DESC_TEST:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		if (val == 1)
			vdm_put_dpm_vdm_request_event(&tcpc->pd_port,
					PD_DPM_VDM_REQUEST_DISCOVER_ID);
		else if (val == 2)
			tcpm_vdm_request_rt7207(tcpc, 0x1015, 0);
		break;
#endif /* CONFIG_RT7207_ADAPTER */
	case TCPC_DESC_TIMER:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		#ifdef CONFIG_USB_POWER_DELIVERY
		if (val > 0 && val <= PD_PE_TIMER_END_ID)
			pd_enable_timer(&tcpc->pd_port, val);
		else if (val > PD_PE_TIMER_END_ID && val < PD_TIMER_NR)
			tcpc_enable_timer(tcpc, val);
		#else
		if (val > 0 && val < PD_TIMER_NR)
			tcpc_enable_timer(tcpc, val);
		#endif /* CONFIG_USB_POWER_DELIVERY */
		break;
	#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_PD_TEST:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		switch (val) {
		case 1: /* Power Role Swap */
			tcpm_power_role_swap(tcpc);
			break;
		case 2: /* Data Role Swap */
			tcpm_data_role_swap(tcpc);
			break;
		case 3: /* Vconn Swap */
			tcpm_vconn_swap(tcpc);
			break;
		case 4: /* Software Reset */
			tcpm_soft_reset(tcpc);
			break;
		case 5: /* Hardware Reset */
			tcpm_hard_reset(tcpc);
			break;
		case 6:
			tcpm_get_source_cap(tcpc, &cap);
			break;
		case 7:
			tcpm_get_sink_cap(tcpc, &cap);
			break;
		default:
			break;
		}
		break;
	#endif /* CONFIG_USB_POWER_DELIVERY */
	default:
		break;
	}
	return count;
}

#if 1 /*kernel version 3.9.0 */
static int tcpc_match_device_by_name(struct device *dev, const void *data)
#else
static int tcpc_match_device_by_name(struct device *dev, void *data)
#endif
{
	const char *name = data;
	struct tcpc_device *tcpc = dev_get_drvdata(dev);

	return strcmp(tcpc->desc.name, name) == 0;
}

struct tcpc_device *tcpc_dev_get_by_name(const char *name)
{
#if 1 /* kernel version 3.9.0 */
	struct device *dev = class_find_device(tcpc_class,
			NULL, (const void *)name, tcpc_match_device_by_name);
#else
	struct device *dev = class_find_device(tcpc_class,
			NULL, (void *)name, tcpc_match_device_by_name);
#endif
	return dev ? dev_get_drvdata(dev) : NULL;
}

static void tcpc_device_release(struct device *dev)
{
	struct tcpc_device *tcpc_dev = to_tcpc_device(dev);

	pr_info("%s : %s device release\n", __func__, dev_name(dev));
	BUG_ON(tcpc_dev == NULL);
	/* Un-init pe thread */
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpci_event_deinit(tcpc_dev);
#endif /* CONFIG_USB_POWER_DELIVERY */
	/* Un-init timer thread */
	tcpci_timer_deinit(tcpc_dev);
	/* Un-init Mutex */
	/* Do initialization */
	devm_kfree(dev, tcpc_dev);
}

static void tcpc_init_work(struct work_struct *work);

struct tcpc_device *tcpc_device_register(struct device *parent,
	struct tcpc_desc *tcpc_desc, struct tcpc_ops *ops, void *drv_data)
{
	struct tcpc_device *tcpc;
	int ret = 0;

	pr_info("%s register tcpc device (%s)\n", __func__, tcpc_desc->name);
	tcpc = devm_kzalloc(parent, sizeof(*tcpc), GFP_KERNEL);
	if (!tcpc) {
		pr_err("%s : allocate tcpc memeory failed\n", __func__);
		return NULL;
	}

	tcpc->dev.class = tcpc_class;
	tcpc->dev.type = &tcpc_dev_type;
	tcpc->dev.parent = parent;
	tcpc->dev.release = tcpc_device_release;
	dev_set_drvdata(&tcpc->dev, tcpc);
	tcpc->drv_data = drv_data;
	dev_set_name(&tcpc->dev, tcpc_desc->name);
	tcpc->desc = *tcpc_desc;
	tcpc->ops = ops;
	tcpc->typec_local_rp_level = tcpc_desc->rp_lvl;

	ret = device_register(&tcpc->dev);
	if (ret) {
		kfree(tcpc);
		return ERR_PTR(ret);
	}

	srcu_init_notifier_head(&tcpc->evt_nh);
	INIT_DELAYED_WORK(&tcpc->init_work, tcpc_init_work);

	mutex_init(&tcpc->access_lock);
	mutex_init(&tcpc->typec_lock);
	mutex_init(&tcpc->timer_lock);
	sema_init(&tcpc->timer_enable_mask_lock, 1);
	sema_init(&tcpc->timer_tick_lock, 1);

	/* If system support "WAKE_LOCK_IDLE",
	 * please use it instead of "WAKE_LOCK_SUSPEND" */
	wake_lock_init(&tcpc->attach_wake_lock, WAKE_LOCK_SUSPEND,
		"tcpc_attach_wakelock");
	wake_lock_init(&tcpc->dettach_temp_wake_lock, WAKE_LOCK_SUSPEND,
		"tcpc_detach_wakelock");

	tcpci_timer_init(tcpc);
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpci_event_init(tcpc);
	pd_core_init(tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	ret = tcpc_dual_role_phy_init(tcpc);
	if (ret < 0)
		dev_err(&tcpc->dev, "dual role usb init fail\n");
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

	return tcpc;
}
EXPORT_SYMBOL(tcpc_device_register);

static int tcpc_device_irq_enable(struct tcpc_device *tcpc)
{
	int ret;

	if (!tcpc->ops->init) {
		pr_err("%s Please implment tcpc ops init function\n",
		__func__);
		return -EINVAL;
	}

	ret = tcpci_init(tcpc, false);
	if (ret < 0) {
		pr_err("%s tcpc init fail\n", __func__);
		return ret;
	}

	ret = tcpc_typec_init(tcpc, tcpc->desc.role_def + 1);
	if (ret < 0) {
		pr_err("%s : tcpc typec init fail\n", __func__);
		return ret;
	}

	pr_info("%s : tcpc irq enable OK!\n", __func__);
	return 0;
}

static void tcpc_init_work(struct work_struct *work)
{
	struct tcpc_device *tcpc = container_of(
		work, struct tcpc_device, init_work.work);

	if (tcpc->desc.notifier_supply_num == 0)
		return;

	pr_info("%s force start\n", __func__);

	tcpc->desc.notifier_supply_num = 0;
	tcpc_device_irq_enable(tcpc);
}

int tcpc_schedule_init_work(struct tcpc_device *tcpc)
{
	if (tcpc->desc.notifier_supply_num == 0)
		return tcpc_device_irq_enable(tcpc);

	schedule_delayed_work(
		&tcpc->init_work, msecs_to_jiffies(30*1000));
	return 0;
}
EXPORT_SYMBOL(tcpc_schedule_init_work);

int register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
			      struct notifier_block *nb)
{
	int ret;

	ret = srcu_notifier_chain_register(&tcp_dev->evt_nh, nb);
	if (ret != 0)
		return ret;

	if (tcp_dev->desc.notifier_supply_num == 0) {
		pr_info("%s already started\n", __func__);
		return 0;
	}

	tcp_dev->desc.notifier_supply_num--;
	pr_info("%s supply_num = %d\n", __func__,
		tcp_dev->desc.notifier_supply_num);

	if (tcp_dev->desc.notifier_supply_num == 0) {
		cancel_delayed_work(&tcp_dev->init_work);
		tcpc_device_irq_enable(tcp_dev);
	}

	return ret;
}
EXPORT_SYMBOL(register_tcp_dev_notifier);

int unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&tcp_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_tcp_dev_notifier);


void tcpc_device_unregister(struct device *dev, struct tcpc_device *tcpc)
{
	if (!tcpc)
		return;

	tcpc_typec_deinit(tcpc);

	wake_lock_destroy(&tcpc->dettach_temp_wake_lock);
	wake_lock_destroy(&tcpc->attach_wake_lock);

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	devm_dual_role_instance_unregister(&tcpc->dev, tcpc->dr_usb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

	device_unregister(&tcpc->dev);

}
EXPORT_SYMBOL(tcpc_device_unregister);

void *tcpc_get_dev_data(struct tcpc_device *tcpc)
{
	return tcpc->drv_data;
}
EXPORT_SYMBOL(tcpc_get_dev_data);

void tcpci_lock_typec(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->typec_lock);
}
EXPORT_SYMBOL(tcpci_lock_typec);

void tcpci_unlock_typec(struct tcpc_device *tcpc)
{
	mutex_unlock(&tcpc->typec_lock);
}
EXPORT_SYMBOL(tcpci_unlock_typec);

static void tcpc_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = tcpc_attr_groups;
	for (i = 0; i < ARRAY_SIZE(tcpc_device_attributes); i++)
		__tcpc_attrs[i] = &tcpc_device_attributes[i].attr;
}

static int __init tcpc_class_init(void)
{
	pr_info("%s\n", __func__);

#ifdef CONFIG_USB_POWER_DELIVERY
	dpm_check_supported_modes();
#endif /* CONFIG_USB_POWER_DELIVERY */

	tcpc_class = class_create(THIS_MODULE, "tcpc");
	if (IS_ERR(tcpc_class)) {
		pr_info("Unable to create tcpc class; errno = %ld\n",
		       PTR_ERR(tcpc_class));
		return PTR_ERR(tcpc_class);
	}
	tcpc_init_attrs(&tcpc_dev_type);
	tcpc_class->suspend = NULL;
	tcpc_class->resume = NULL;

	pr_info("TCPC class init OK\n");
	return 0;
}

static void __exit tcpc_class_exit(void)
{
	class_destroy(tcpc_class);
	pr_info("TCPC class un-init OK\n");
}

subsys_initcall(tcpc_class_init);
module_exit(tcpc_class_exit);

MODULE_DESCRIPTION("Richtek TypeC Port Control Core");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION("1.0.4_G");
MODULE_LICENSE("GPL");
