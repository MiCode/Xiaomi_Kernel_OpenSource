/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Richtek TypeC Port Control Interface Core Driver
 *
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
#include <linux/power_supply.h>
#include <linux/workqueue.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#ifdef CONFIG_USB_POWER_DELIVERY
#include "pd_dpm_prv.h"
#include "inc/tcpm.h"
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
#include "mtk_battery.h"
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */
#include "inc/pd_dbg_info.h"
#include "inc/rt-regmap.h"

#define TCPC_CORE_VERSION		"2.0.18_G"
//extern int cc_vendor_pid;

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

struct class *tcpc_class;
EXPORT_SYMBOL_GPL(tcpc_class);

static struct device_type tcpc_dev_type;

static struct device_attribute tcpc_device_attributes[] = {
	TCPC_DEVICE_ATTR(role_def, 0444),
	TCPC_DEVICE_ATTR(rp_lvl, 0444),
	TCPC_DEVICE_ATTR(pd_test, 0664),
	TCPC_DEVICE_ATTR(info, 0444),
	TCPC_DEVICE_ATTR(timer, 0664),
	TCPC_DEVICE_ATTR(caps_info, 0444),
	TCPC_DEVICE_ATTR(pe_ready, 0444),
	TCPC_DEVICE_ATTR(vendor_id, 0664),
};

enum {
	TCPC_DESC_ROLE_DEF = 0,
	TCPC_DESC_RP_LEVEL,
	TCPC_DESC_PD_TEST,
	TCPC_DESC_INFO,
	TCPC_DESC_TIMER,
	TCPC_DESC_CAP_INFO,
	TCPC_DESC_PE_READY,
	TCPC_DESC_VENDOR_ID,
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
	"Unknown",
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
	struct pe_data *pe_data;
	struct pd_port *pd_port;
	struct tcpm_power_cap_val cap;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	switch (offset) {
#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_CAP_INFO:
		pd_port = &tcpc->pd_port;
		pe_data = &pd_port->pe_data;
		snprintf(buf+strlen(buf), 256, "selected_cap = %d\n", pe_data->selected_cap);
		snprintf(buf+strlen(buf), 256, "%s\n",
				"local_src_cap(type, vmin, vmax, oper)");
		for (i = 0; i < pd_port->local_src_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pd_port->local_src_cap.pdos[i],
				&cap);
			snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				cap.type, cap.min_mv, cap.max_mv, cap.ma);
		}
		snprintf(buf+strlen(buf), 256, "%s\n",
				"local_snk_cap(type, vmin, vmax, ioper)");
		for (i = 0; i < pd_port->local_snk_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pd_port->local_snk_cap.pdos[i],
				&cap);
			snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				cap.type, cap.min_mv, cap.max_mv, cap.ma);
		}
		snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_src_cap(type, vmin, vmax, ioper)");
		for (i = 0; i < pe_data->remote_src_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pe_data->remote_src_cap.pdos[i],
				&cap);
			snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				cap.type, cap.min_mv, cap.max_mv, cap.ma);
		}
		snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_snk_cap(type, vmin, vmax, ioper)");
		for (i = 0; i < pe_data->remote_snk_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pe_data->remote_snk_cap.pdos[i],
				&cap);
			snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				cap.type, cap.min_mv, cap.max_mv, cap.ma);
		}
		break;
#endif	/* CONFIG_USB_POWER_DELIVERY */
	case TCPC_DESC_ROLE_DEF:
		snprintf(buf, 256, "%s\n", role_text[tcpc->desc.role_def]);
		break;
	case TCPC_DESC_RP_LEVEL:
		if (tcpc->typec_local_rp_level == TYPEC_RP_DFT)
			snprintf(buf, 256, "%s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_RP_1_5)
			snprintf(buf, 256, "%s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_RP_3_0)
			snprintf(buf, 256, "%s\n", "3.0");
		break;
	case TCPC_DESC_PD_TEST:
		snprintf(buf, 256, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			"1: pr_swap", "2: dr_swap", "3: vconn_swap",
			"4: soft reset", "5: hard reset",
			"6: get_src_cap", "7: get_sink_cap",
			"8: discover_id", "9: discover_cable");
		break;
	case TCPC_DESC_INFO:
		i += snprintf(buf + i,
			256, "|^|==( %s info )==|^|\n", tcpc->desc.name);
		i += snprintf(buf + i,
			256, "role = %s\n", role_text[tcpc->desc.role_def]);
		if (tcpc->typec_local_rp_level == TYPEC_RP_DFT)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_RP_1_5)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_RP_3_0)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "3.0");
		break;
#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_PE_READY:
		pd_port = &tcpc->pd_port;
		if (pd_port->pe_data.pe_ready)
			snprintf(buf, 256, "%s\n", "yes");
		else
			snprintf(buf, 256, "%s\n", "no");
		break;
#endif
	case TCPC_DESC_VENDOR_ID:
		/*pr_info("%s :cc_vendor_id = %x \n", __func__, cc_vendor_pid);
		if (0x1711 == cc_vendor_pid) {
			snprintf(buf, 256, "cc_vendor:1 \n");
		} else {
			snprintf(buf, 256, "cc_vendor:2 \n");
		}*/
		break;
	default:
		break;
	}
	return strlen(buf);
}

static int get_parameters(char *buf, unsigned long *param, int num_of_par)
{
	int cnt = 0;
	char *token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if (kstrtoul(token, 0, &param[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}

	return 0;
}

static ssize_t tcpc_store_property(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
#ifdef CONFIG_USB_POWER_DELIVERY
	uint8_t role;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	struct tcpc_device *tcpc = to_tcpc_device(dev);
	const ptrdiff_t offset = attr - tcpc_device_attributes;
	int ret;
	unsigned long val;

	switch (offset) {
	case TCPC_DESC_ROLE_DEF:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}

		tcpm_typec_change_role(tcpc, val);
		break;
	case TCPC_DESC_TIMER:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		if (val < PD_TIMER_NR)
			tcpc_enable_timer(tcpc, val);
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
			role = tcpm_inquire_pd_power_role(tcpc);
			if (role == PD_ROLE_SINK)
				role = PD_ROLE_SOURCE;
			else
				role = PD_ROLE_SINK;
			tcpm_dpm_pd_power_swap(tcpc, role, NULL);
			break;
		case 2: /* Data Role Swap */
			role = tcpm_inquire_pd_data_role(tcpc);
			if (role == PD_ROLE_UFP)
				role = PD_ROLE_DFP;
			else
				role = PD_ROLE_UFP;
			tcpm_dpm_pd_data_swap(tcpc, role, NULL);
			break;
		case 3: /* Vconn Swap */
			role = tcpm_inquire_pd_vconn_role(tcpc);
			if (role == PD_ROLE_VCONN_OFF)
				role = PD_ROLE_VCONN_ON;
			else
				role = PD_ROLE_VCONN_OFF;
			tcpm_dpm_pd_vconn_swap(tcpc, role, NULL);
			break;
		case 4: /* Software Reset */
			tcpm_dpm_pd_soft_reset(tcpc, NULL);
			break;
		case 5: /* Hardware Reset */
			tcpm_dpm_pd_hard_reset(tcpc, NULL);
			break;
		case 6:
			tcpm_dpm_pd_get_source_cap(tcpc, NULL);
			break;
		case 7:
			tcpm_dpm_pd_get_sink_cap(tcpc, NULL);
			break;
		case 8:
			tcpm_dpm_vdm_discover_id(tcpc, NULL);
			break;
		case 9:
			tcpm_dpm_vdm_discover_cable(tcpc, NULL);
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

static int tcpc_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct tcpc_device *tcpc = dev_get_drvdata(dev);

	return strcmp(tcpc->desc.name, name) == 0;
}

struct tcpc_device *tcpc_dev_get_by_name(const char *name)
{
	struct device *dev = class_find_device(tcpc_class,
			NULL, (const void *)name, tcpc_match_device_by_name);
	return dev ? dev_get_drvdata(dev) : NULL;
}
EXPORT_SYMBOL(tcpc_dev_get_by_name);

static void tcpc_device_release(struct device *dev)
{
	struct tcpc_device *tcpc = to_tcpc_device(dev);

	pr_info("%s : %s device release\n", __func__, dev_name(dev));
	PD_BUG_ON(tcpc == NULL);
	/* Un-init pe thread */
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpci_event_deinit(tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */
	/* Un-init timer thread */
	tcpci_timer_deinit(tcpc);
	/* Un-init Mutex */
	/* Do initialization */
}

static void tcpc_event_init_work(struct work_struct *work);

struct tcpc_device *tcpc_device_register(struct device *parent,
	struct tcpc_desc *tcpc_desc, struct tcpc_ops *ops, void *drv_data)
{
	struct tcpc_device *tcpc;
	int ret = 0, i = 0;

	pr_info("%s register tcpc device (%s)\n", __func__, tcpc_desc->name);
	tcpc = devm_kzalloc(parent, sizeof(*tcpc), GFP_KERNEL);
	if (!tcpc) {
		pr_err("%s : allocate tcpc memory failed\n", __func__);
		return NULL;
	}

	tcpc->evt_wq = alloc_ordered_workqueue("%s", 0, tcpc_desc->name);
	for (i = 0; i < TCP_NOTIFY_IDX_NR; i++)
		srcu_init_notifier_head(&tcpc->evt_nh[i]);

	mutex_init(&tcpc->access_lock);
	mutex_init(&tcpc->typec_lock);
	mutex_init(&tcpc->timer_lock);
	mutex_init(&tcpc->mr_lock);
	spin_lock_init(&tcpc->timer_tick_lock);

	tcpc->dev.class = tcpc_class;
	tcpc->dev.type = &tcpc_dev_type;
	tcpc->dev.parent = parent;
	tcpc->dev.release = tcpc_device_release;
	dev_set_drvdata(&tcpc->dev, tcpc);
	tcpc->drv_data = drv_data;
	dev_set_name(&tcpc->dev, "%s", tcpc_desc->name);
	tcpc->desc = *tcpc_desc;
	tcpc->ops = ops;
	tcpc->typec_local_rp_level = tcpc_desc->rp_lvl;
	tcpc->typec_remote_rp_level = TYPEC_CC_VOLT_SNK_DFT;
	tcpc->typec_polarity = false;

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	tcpc->tcpc_vconn_supply = tcpc_desc->vconn_supply;
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	device_set_of_node_from_dev(&tcpc->dev, parent);

	ret = device_register(&tcpc->dev);
	if (ret) {
		kfree(tcpc);
		return ERR_PTR(ret);
	}

	INIT_DELAYED_WORK(&tcpc->event_init_work, tcpc_event_init_work);

	tcpc->attach_wake_lock =
		wakeup_source_register(&tcpc->dev, "tcpc_attach_wake_lock");
	tcpc->detach_wake_lock =
		wakeup_source_register(&tcpc->dev, "tcpc_detach_wake_lock");

	tcpci_timer_init(tcpc);
#ifdef CONFIG_USB_POWER_DELIVERY
	pd_core_init(tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */

#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
	ret = tcpc_dual_role_phy_init(tcpc);
	if (ret < 0)
		dev_err(&tcpc->dev, "dual role usb init fail\n");
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

	return tcpc;
}
EXPORT_SYMBOL(tcpc_device_register);

int tcpc_device_irq_enable(struct tcpc_device *tcpc)
{
	int ret;

	if (!tcpc->ops->init) {
		pr_notice("%s Please implment tcpc ops init function\n",
			  __func__);
		return -EINVAL;
	}

	tcpci_lock_typec(tcpc);
	ret = tcpci_init(tcpc, false);
	if (ret < 0) {
		tcpci_unlock_typec(tcpc);
		pr_err("%s tcpc init fail\n", __func__);
		return ret;
	}

	ret = tcpc_typec_init(tcpc, tcpc->desc.role_def);
	tcpci_unlock_typec(tcpc);
	if (ret < 0) {
		pr_err("%s : tcpc typec init fail\n", __func__);
		return ret;
	}

	schedule_delayed_work(&tcpc->event_init_work, 0);

	pr_info("%s : tcpc irq enable OK!\n", __func__);
	return 0;
}
EXPORT_SYMBOL(tcpc_device_irq_enable);

#ifdef CONFIG_USB_PD_REV30
static void bat_update_work_func(struct work_struct *work)
{
	struct tcpc_device *tcpc = container_of(work,
		struct tcpc_device, bat_update_work.work);
	union power_supply_propval value;
	int ret;

	ret = power_supply_get_property(
			tcpc->bat_psy, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret == 0) {
		TCPC_INFO("%s battery update soc = %d\n",
					__func__, value.intval);
		tcpc->bat_soc = value.intval;
	} else
		TCPC_ERR("%s get battery capacity fail\n", __func__);

	ret = power_supply_get_property(tcpc->bat_psy,
		POWER_SUPPLY_PROP_STATUS, &value);
	if (ret == 0) {
		if (value.intval == POWER_SUPPLY_STATUS_CHARGING) {
			TCPC_INFO("%s Battery Charging\n", __func__);
			tcpc->charging_status = BSDO_BAT_INFO_CHARGING;
		} else if (value.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
			TCPC_INFO("%s Battery Discharging\n", __func__);
			tcpc->charging_status = BSDO_BAT_INFO_DISCHARGING;
		} else {
			TCPC_INFO("%s Battery Idle\n", __func__);
			tcpc->charging_status = BSDO_BAT_INFO_IDLE;
		}
	}
	if (ret < 0)
		TCPC_ERR("%s get battery charger now fail\n", __func__);

	tcpm_update_bat_status_soc(tcpc,
		PD_BAT_REF_FIXED0, tcpc->charging_status, tcpc->bat_soc * 10);
}

static int bat_nb_call_func(
	struct notifier_block *nb, unsigned long val, void *v)
{
	struct tcpc_device *tcpc = container_of(nb, struct tcpc_device, bat_nb);
	struct power_supply *psy = (struct power_supply *)v;

	if (!tcpc) {
		TCPC_ERR("%s tcpc is null\n", __func__);
		return NOTIFY_OK;
	}

	if (val == PSY_EVENT_PROP_CHANGED &&
		strcmp(psy->desc->name, "battery") == 0)
		schedule_delayed_work(&tcpc->bat_update_work, 0);
	return NOTIFY_OK;
}
#endif /* CONFIG_USB_PD_REV30 */

static void tcpc_event_init_work(struct work_struct *work)
{
#ifdef CONFIG_USB_POWER_DELIVERY
	struct tcpc_device *tcpc = container_of(
			work, struct tcpc_device, event_init_work.work);
#ifdef CONFIG_USB_PD_REV30
	int retval;
#endif /* CONFIG_USB_PD_REV30 */

	tcpci_lock_typec(tcpc);
	tcpci_event_init(tcpc);
#ifdef CONFIG_USB_PD_WAIT_BC12
	tcpc->usb_psy = power_supply_get_by_name("usb");
	if (!tcpc->usb_psy) {
		tcpci_unlock_typec(tcpc);
		TCPC_ERR("%s get usb psy fail\n", __func__);
		return;
	}
#endif /* CONFIG_USB_PD_WAIT_BC12 */
	tcpc->pd_inited_flag = 1;
	pr_info("%s typec attach new = %d\n",
			__func__, tcpc->typec_attach_new);
	if (tcpc->typec_attach_new)
		pd_put_cc_attached_event(tcpc, tcpc->typec_attach_new);
	tcpci_unlock_typec(tcpc);

#ifdef CONFIG_USB_PD_REV30
	INIT_DELAYED_WORK(&tcpc->bat_update_work, bat_update_work_func);
	tcpc->bat_psy = power_supply_get_by_name("battery");
	if (!tcpc->bat_psy) {
		TCPC_ERR("%s get battery psy fail\n", __func__);
		return;
	}
	tcpc->charging_status = BSDO_BAT_INFO_IDLE;
	tcpc->bat_soc = 0;
	tcpc->bat_nb.notifier_call = bat_nb_call_func;
	tcpc->bat_nb.priority = 0;
	retval = power_supply_reg_notifier(&tcpc->bat_nb);
	if (retval < 0)
		pr_err("%s register power supply notifier fail\n", __func__);
#endif /* CONFIG_USB_PD_REV30 */

#endif /* CONFIG_USB_POWER_DELIVERY */
}

struct tcp_notifier_block_wrapper {
	struct notifier_block stub_nb;
	struct notifier_block *action_nb;
};

static int tcp_notifier_func_stub(struct notifier_block *nb,
	unsigned long action, void *data)
{
	struct tcp_notifier_block_wrapper *nb_wrapper =
		container_of(nb, struct tcp_notifier_block_wrapper, stub_nb);
	struct notifier_block *action_nb = nb_wrapper->action_nb;

	return nb_wrapper->action_nb->notifier_call(action_nb, action, data);
}

struct tcpc_managed_res {
	void *res;
	void *key;
	int prv_id;
	struct tcpc_managed_res *next;
};


static int __add_wrapper_to_managed_res_list(struct tcpc_device *tcp_dev,
	void *res, void *key, int prv_id)
{
	struct tcpc_managed_res *tail;
	struct tcpc_managed_res *mres;

	mres = devm_kzalloc(&tcp_dev->dev, sizeof(*mres), GFP_KERNEL);
	if (!mres)
		return -ENOMEM;
	mres->res = res;
	mres->key = key;
	mres->prv_id = prv_id;
	mutex_lock(&tcp_dev->mr_lock);
	tail = tcp_dev->mr_head;
	if (tail) {
		while (tail->next)
			tail = tail->next;
		tail->next = mres;
	} else
		tcp_dev->mr_head = mres;
	mutex_unlock(&tcp_dev->mr_lock);

	return 0;
}

static int __register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
	struct notifier_block *nb, uint8_t idx)
{
	struct tcp_notifier_block_wrapper *nb_wrapper;
	int retval;

	nb_wrapper = devm_kzalloc(
		&tcp_dev->dev, sizeof(*nb_wrapper), GFP_KERNEL);
	if (!nb_wrapper)
		return -ENOMEM;
	nb_wrapper->action_nb = nb;
	nb_wrapper->stub_nb.notifier_call = tcp_notifier_func_stub;
	retval = srcu_notifier_chain_register(
		tcp_dev->evt_nh + idx, &nb_wrapper->stub_nb);
	if (retval < 0) {
		devm_kfree(&tcp_dev->dev, nb_wrapper);
		return retval;
	}
	retval = __add_wrapper_to_managed_res_list(
				tcp_dev, nb_wrapper, nb, idx);
	if (retval < 0)
		dev_warn(&tcp_dev->dev,
			"Failed to add resource to manager(%d)\n", retval);

	return 0;
}

static bool __is_mulit_bits_set(uint32_t flags)
{
	if (flags) {
		flags &= (flags - 1);
		return flags ? true : false;
	}

	return false;
}

int register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
			struct notifier_block *nb, uint8_t flags)
{
	int ret = 0, i = 0;

	if (__is_mulit_bits_set(flags)) {
		for (i = 0; i < TCP_NOTIFY_IDX_NR; i++) {
			if (flags & (1 << i)) {
				ret = __register_tcp_dev_notifier(
							tcp_dev, nb, i);
				if (ret < 0)
					return ret;
			}
		}
	} else { /* single bit */
		for (i = 0; i < TCP_NOTIFY_IDX_NR; i++) {
			if (flags & (1 << i)) {
				ret = srcu_notifier_chain_register(
				&tcp_dev->evt_nh[i], nb);
				break;
			}
		}
	}

	return ret;
}
EXPORT_SYMBOL(register_tcp_dev_notifier);


static void *__remove_wrapper_from_managed_res_list(
	struct tcpc_device *tcp_dev, void *key, int prv_id)
{
	void *retval = NULL;
	struct tcpc_managed_res *mres = tcp_dev->mr_head;
	struct tcpc_managed_res *prev = NULL;

	mutex_lock(&tcp_dev->mr_lock);
	if (mres) {
		while (mres) {
			if (mres->key == key && mres->prv_id == prv_id) {
				retval = mres->res;
				if (prev)
					prev->next = mres->next;
				else
					tcp_dev->mr_head = NULL;
				devm_kfree(&tcp_dev->dev, mres);
				break;
			}
			prev = mres;
			mres = mres->next;
		}
	}
	mutex_unlock(&tcp_dev->mr_lock);

	return retval;
}

static int __unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
	struct notifier_block *nb, uint8_t idx)
{
	struct tcp_notifier_block_wrapper *nb_wrapper;
	int retval;

	nb_wrapper = __remove_wrapper_from_managed_res_list(tcp_dev, nb, idx);
	if (nb_wrapper) {
		retval = srcu_notifier_chain_unregister(
			tcp_dev->evt_nh + idx, &nb_wrapper->stub_nb);
		devm_kfree(&tcp_dev->dev, nb_wrapper);
		return retval;
	}

	return -ENOENT;
}

int unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb, uint8_t flags)
{
	int i = 0, ret = 0;

	for (i = 0; i < TCP_NOTIFY_IDX_NR; i++) {
		if (flags & (1 << i)) {
			ret = __unregister_tcp_dev_notifier(tcp_dev, nb, i);
			if (ret == -ENOENT)
				ret = srcu_notifier_chain_unregister(
					tcp_dev->evt_nh + i, nb);
			if (ret < 0)
				return ret;
		}
	}
	return ret;
}
EXPORT_SYMBOL(unregister_tcp_dev_notifier);


void tcpc_device_unregister(struct device *dev, struct tcpc_device *tcpc)
{
	if (!tcpc)
		return;

	tcpc_typec_deinit(tcpc);

#ifdef CONFIG_USB_PD_REV30
	wakeup_source_unregister(tcpc->pd_port.pps_request_wake_lock);
#endif /* CONFIG_USB_PD_REV30 */
	wakeup_source_unregister(tcpc->detach_wake_lock);
	wakeup_source_unregister(tcpc->attach_wake_lock);

#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
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
	pr_info("%s (%s)\n", __func__, TCPC_CORE_VERSION);

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

	pd_dbg_info_init();
	regmap_plat_init();

	pr_info("TCPC class init OK\n");
	return 0;
}

static void __exit tcpc_class_exit(void)
{
	regmap_plat_exit();
	pd_dbg_info_exit();

	class_destroy(tcpc_class);
	pr_info("TCPC class un-init OK\n");
}

subsys_initcall(tcpc_class_init);
module_exit(tcpc_class_exit);

void __attribute__((weak)) sched_set_fifo(struct task_struct *p)
{
	struct sched_param sp = { .sched_priority = MAX_RT_PRIO / 2 };

	WARN_ON_ONCE(sched_setscheduler_nocheck(p, SCHED_FIFO, &sp) != 0);
}

MODULE_DESCRIPTION("Richtek TypeC Port Control Core");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(TCPC_CORE_VERSION);
MODULE_LICENSE("GPL");

/* Release Version
 * 2.0.18_G
 * (1) Fix typos
 * (2) Revise tcpci_alert.c
 * (3) Request the previous voltage/current first with new Source_Capabilities
 * (4) #undef CONFIG_USB_PD_ALT_MODE_RTDC
 * (5) Use typec_state in typec_is_cc_attach()
 * (6) Add epr_source_pdp into struct pd_source_cap_ext for new PD spec
 * (7) Revise get_status_once, pd_traffic_idle
 * (8) Revise IRQ handling
 * (9) Start PD policy engine without delay
 * (10) Revise typec_remote_rp_level
 * (11) Revise Hard Reset timer as PD Sink
 * (12) Set mismatch to false in dpm_build_default_request_info()
 * (13) Revise tcpc_timer
 * (14) Remove notifier_supply_num
 * (15) Fix COMMON.CHECK.PD.5 Check Unexpected Messages and Signals
 * (16) Fix TEST.PD.PROT.PORT3.4 Invalid Battery Capabilities Reference
 * (17) Fix TEST.PD.VDM.SRC.1 Discovery Process and Enter Mode
 * (18) Add PD_TIMER_VSAFE5V_DELAY
 * (19) Revise SOP' communication
 * (20) Revise PD request as Sink
 * (21) Disable old legacy cable workaround
 * (22) Enable PD Safe0V Timeout
 * (23) Replace calling of sched_setscheduler() with sched_set_fifo()
 *
 * 2.0.17_G
 * (1) Add CONFIG_TYPEC_LEGACY3_ALWAYS_LOCAL_RP
 * (2) Fix a synchronization/locking problem in pd_notify_pe_error_recovery()
 * (3) Add USB_VID_MQP
 * (4) Revise the return value checking of tcpc_device_register()
 *
 * 2.0.16_G
 * (1) Check the return value of wait_event_interruptible()
 * (2) Revise *_get_cc()
 * (3) Revise role_def
 * (4) Fix COMMON.CHECK.PD.10
 *
 * 2.0.15_G
 * (1) undef CONFIG_COMPATIBLE_APPLE_TA
 * (2) Fix TEST.PD.PROT.ALL.5 Unrecognized Message (PD2)
 * (3) Fix TEST.PD.PROT.ALL3.3 Invalid Manufacturer Info Target
 * (4) Fix TEST.PD.PROT.ALL3.4 Invalid Manufacturer Info Ref
 * (5) Fix TEST.PD.PROT.SRC.11 Unexpected Message Received in Ready State (PD2)
 * (6) Fix TEST.PD.PROT.SRC.13 PR_Swap - GoodCRC not sent in Response to PS_RDY
 * (7) Fix TEST.PD.VDM.SRC.2 Invalid Fields - Discover Identity (PD2)
 * (8) Revise the usages of PD_TIMER_NO_RESPONSE
 * (9) Retry to send Source_Capabilities after PR_Swap
 * (10) Fix tcpm_get_remote_power_cap() and __tcpm_inquire_select_source_cap()
 * (11) Increase the threshold to enter PE_ERROR_RECOVERY_ONCE from 2 to 4
 * (12) Change wait_event() back to wait_event_interruptible() for not being
 *	detected as hung tasks
 *
 * 2.0.14_G
 * (1) Move out typec_port registration and operation to rt_pd_manager.c
 * (2) Rename CONFIG_TYPEC_WAIT_BC12 to CONFIG_USB_PD_WAIT_BC12
 * (3) Not to set power/data/vconn role repeatedly
 * (4) Revise vconn highV protection
 * (5) Revise tcpc timer
 * (6) Decrease VBUS present threshold (VBUS_CAL) by 60mV (2LSBs) for RT171x
 * (7) Replace \r\n with \n for resolving logs without newlines
 * (8) Remove the member time_stamp from struct pd_msg
 * (9) Remove NoResponseTimer as Sink for new PD spec
 * (10) Revise responses of Reject and Not_Supported
 * (11) Revise the usages of pd_traffic_control and typec_power_ctrl
 * (12) Revise the usages of wait_event_*()
 * (13) Add PD capability for TYPEC_ATTACHED_DBGACC_SNK
 * (14) Utilize rt-regmap to reduce I2C accesses
 *
 * 2.0.13_G
 * (1) Add TCPC flags for VCONN_SAFE5V_ONLY
 * (2) Add boolean property attempt_discover_svid in dts/dtsi
 * (3) Add a TCPM API for postponing Type-C role change until unattached
 * (4) Update VDOs according new PD spec
 * (5) Add an option for enabling/disabling the support of DebugAccessory.SRC
 * (6) Add the workaround for delayed ps_change related to PS_RDY
 *     during PR_SWAP
 * (7) Always Back to PE ready state in pd_dpm_dfp_inform_id() and
 *     pd_dpm_dfp_inform_svids()
 * (8) Re-fetch triggered_timer and enable_mask after lock acquisition
 * (9) Leave low power mode only when CC is detached
 * (10) Revise code related to pd_check_rev30()
 * (11) Bypass BC1.2 for PR_SWAP from Source to Sink (MTK only)
 * (12) Support charging icon for AudioAccessory (MTK only)
 * (13) Replace tcpc_dev with tcpc
 * (14) TCPCI Alert V10 and V20 co-exist
 * (15) Resolve DP Source/Sink Both Connected when acting as DFP_U
 * (16) Change CONFIG_TYPEC_SNK_CURR_DFT from 150 to 100 (mA)
 * (17) Define CONFIG_USB_PD_PR_SWAP_ERROR_RECOVERY by default
 * (18) Add an option for TCPC log with port name
 * (19) USB-C states go from ErrorRecovery to Unattached.SRC with Try.SRC role
 * (20) Revise dts/dtsi value for DisplayPort Alternative Mode
 * (21) Mask vSafe0V IRQ before entering low power mode
 * (22) Disable auto idle mode before entering low power mode
 * (23) Reset Protocol FSM and clear RX alerts twice before clock gating
 *
 * 2.0.12_G
 * (1) Fix voltage/current steps of RDO for APDO
 * (2) Non-blocking TCPC notification by default
 * (3) Fix synchronization/locking problems
 * (4) Fix NoRp.SRC support
 *
 * 2.0.11_G
 * (1) Fix PD compliance failures of Ellisys and MQP
 * (2) Wait the result of BC1.2 before starting PD policy engine
 * (3) Fix compile warnings
 * (4) Fix NoRp.SRC support
 *
 * 2.0.10_G
 * (1) fix battery noitifier plug out cause recursive locking detected in
 *     nh->srcu.
 *
 * 2.0.9_G
 * (1) fix 10k A-to-C legacy cable workaround side effect when
 *     cable plug in at worakround flow.
 *
 * 2.0.8_G
 * (1) fix timeout thread flow for wakeup pd event thread
 *     after disable timer first.
 *
 * 2.0.7_G
 * (1) add extract pd source capability pdo defined in
 *     PD30 v1.1 ECN for pe40 get apdo profile.
 *
 * 2.0.6_G
 * (1) register battery notifier for battery plug out
 *     avoid TA hardreset 3 times will show charing icon.
 *
 * 2.0.5_G
 * (1) add CONFIG_TYPEC_CAP_NORP_SRC to support
 *      A-to-C No-Rp cable.
 * (2) add handler pd eint with eint mask
 *
 * 2.0.4_G
 * (1) add CONFIG_TCPC_NOTIFIER_LATE_SYNC to
 *      move irq_enable to late_initcall_sync stage
 *      to prevent from notifier_supply_num setting wrong.
 *
 * 2.0.3_G
 * (1) use local_irq_XXX to instead raw_local_irq_XXX
 *      to fix lock prov WARNING
 * (2) Remove unnecessary charger detect flow. it does
 *      not need to switch BC1-2 path on otg_en
 *
 * 2.0.2_G
 * (1) Fix Coverity and check patch issue
 * (2) Fix 32-bit project build error
 *
 * 2.0.1_G
 *	First released PD3.0 Driver
 */
