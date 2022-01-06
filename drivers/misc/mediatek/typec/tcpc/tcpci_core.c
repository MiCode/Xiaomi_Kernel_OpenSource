// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#define TCPC_CORE_VERSION		"2.0.17_MTK"

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
	TCPC_DEVICE_ATTR(role_def, 0444),
	TCPC_DEVICE_ATTR(rp_lvl, 0444),
	TCPC_DEVICE_ATTR(pd_test, 0664),
	TCPC_DEVICE_ATTR(info, 0444),
	TCPC_DEVICE_ATTR(timer, 0664),
	TCPC_DEVICE_ATTR(caps_info, 0444),
	TCPC_DEVICE_ATTR(pe_ready, 0444),
};

enum {
	TCPC_DESC_ROLE_DEF = 0,
	TCPC_DESC_RP_LEVEL,
	TCPC_DESC_PD_TEST,
	TCPC_DESC_INFO,
	TCPC_DESC_TIMER,
	TCPC_DESC_CAP_INFO,
	TCPC_DESC_PE_READY,
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
	int i = 0, ret;
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
		ret = snprintf(buf+strlen(buf), 256, "%s = %d\n%s = %d\n",
				"local_selected_cap",
				pe_data->local_selected_cap,
				"remote_selected_cap",
				pe_data->remote_selected_cap);
		if (ret < 0)
			break;
		ret = snprintf(buf+strlen(buf), 256, "%s\n",
				"local_src_cap(type, vmin, vmax, oper)");
		if (ret < 0)
			break;
		for (i = 0; i < pd_port->local_src_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pd_port->local_src_cap.pdos[i],
				&cap);
			ret = snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				      cap.type, cap.min_mv, cap.max_mv, cap.ma);
			if (ret < 0)
				break;
		}
		ret = snprintf(buf+strlen(buf), 256, "%s\n",
				"local_snk_cap(type, vmin, vmax, ioper)");
		if (ret < 0)
			break;
		for (i = 0; i < pd_port->local_snk_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pd_port->local_snk_cap.pdos[i],
				&cap);
			ret = snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				      cap.type, cap.min_mv, cap.max_mv, cap.ma);
			if (ret < 0)
				break;
		}
		ret = snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_src_cap(type, vmin, vmax, ioper)");
		if (ret < 0)
			break;
		for (i = 0; i < pe_data->remote_src_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pe_data->remote_src_cap.pdos[i],
				&cap);
			ret = snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				      cap.type, cap.min_mv, cap.max_mv, cap.ma);
			if (ret < 0)
				break;
		}
		ret = snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_snk_cap(type, vmin, vmax, ioper)");
		if (ret < 0)
			break;
		for (i = 0; i < pe_data->remote_snk_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				pe_data->remote_snk_cap.pdos[i],
				&cap);
			ret = snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				      cap.type, cap.min_mv, cap.max_mv, cap.ma);
			if (ret < 0)
				break;
		}
		break;
#endif	/* CONFIG_USB_POWER_DELIVERY */
	case TCPC_DESC_ROLE_DEF:
		ret = snprintf(buf, 256, "%s\n", role_text[tcpc->desc.role_def]);
		if (ret < 0)
			break;
		break;
	case TCPC_DESC_RP_LEVEL:
		if (tcpc->typec_local_rp_level == TYPEC_RP_DFT) {
			ret = snprintf(buf, 256, "%s\n", "Default");
			if (ret < 0)
				break;
		} else if (tcpc->typec_local_rp_level == TYPEC_RP_1_5) {
			ret = snprintf(buf, 256, "%s\n", "1.5");
			if (ret < 0)
				break;
		} else if (tcpc->typec_local_rp_level == TYPEC_RP_3_0) {
			ret = snprintf(buf, 256, "%s\n", "3.0");
			if (ret < 0)
				break;
		}
		break;
	case TCPC_DESC_PD_TEST:
		ret = snprintf(buf, 256, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
				"1: pr_swap", "2: dr_swap", "3: vconn_swap",
				"4: soft reset", "5: hard reset",
				"6: get_src_cap", "7: get_sink_cap",
				"8: discover_id", "9: discover_cable");
		if (ret < 0)
			dev_dbg(dev, "%s: ret=%d\n", __func__, ret);
		break;
	case TCPC_DESC_INFO:
		i += snprintf(buf + i,
			256, "|^|==( %s info )==|^|\n", tcpc->desc.name);
		if (i < 0)
			break;
		i += snprintf(buf + i,
			256, "role = %s\n", role_text[tcpc->desc.role_def]);
		if (i < 0)
			break;
		if (tcpc->typec_local_rp_level == TYPEC_RP_DFT) {
			i += snprintf(buf + i, 256, "rplvl = %s\n", "Default");
			if (i < 0)
				break;
		} else if (tcpc->typec_local_rp_level == TYPEC_RP_1_5) {
			i += snprintf(buf + i, 256, "rplvl = %s\n", "1.5");
			if (i < 0)
				break;
		} else if (tcpc->typec_local_rp_level == TYPEC_RP_3_0) {
			i += snprintf(buf + i, 256, "rplvl = %s\n", "3.0");
			if (i < 0)
				break;
		}
		break;
#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_PE_READY:
		pd_port = &tcpc->pd_port;
		if (pd_port->pe_data.pe_ready) {
			ret = snprintf(buf, 256, "%s\n", "yes");
			if (ret < 0)
				break;
		} else {
			ret = snprintf(buf, 256, "%s\n", "no");
			if (ret < 0)
				break;
		}
		break;
#endif
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
	long val;

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

static void tcpc_init_work(struct work_struct *work);
static void tcpc_event_init_work(struct work_struct *work);

struct tcpc_device *tcpc_device_register(struct device *parent,
	struct tcpc_desc *tcpc_desc, struct tcpc_ops *ops, void *drv_data)
{
	struct tcpc_device *tcpc;
	int ret = 0, i = 0;

	pr_info("%s register tcpc device (%s)\n", __func__, tcpc_desc->name);
	tcpc = devm_kzalloc(parent, sizeof(*tcpc), GFP_KERNEL);
	if (!tcpc) {
		pr_err("%s : allocate tcpc memeory failed\n", __func__);
		return NULL;
	}

	tcpc->evt_wq = alloc_ordered_workqueue("%s", 0, tcpc_desc->name);
	for (i = 0; i < TCP_NOTIFY_IDX_NR; i++)
		srcu_init_notifier_head(&tcpc->evt_nh[i]);

	mutex_init(&tcpc->access_lock);
	mutex_init(&tcpc->typec_lock);
	mutex_init(&tcpc->timer_lock);
	mutex_init(&tcpc->mr_lock);
	sema_init(&tcpc->timer_enable_mask_lock, 1);
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

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	tcpc->tcpc_vconn_supply = tcpc_desc->vconn_supply;
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	device_set_of_node_from_dev(&tcpc->dev, parent);

	ret = device_register(&tcpc->dev);
	if (ret) {
		kfree(tcpc);
		return ERR_PTR(ret);
	}

	INIT_DELAYED_WORK(&tcpc->init_work, tcpc_init_work);
	INIT_DELAYED_WORK(&tcpc->event_init_work, tcpc_event_init_work);

	/* If system support "WAKE_LOCK_IDLE",
	 * please use it instead of "WAKE_LOCK_SUSPEND"
	 */
	tcpc->attach_wake_lock =
		wakeup_source_register(&tcpc->dev, "tcpc_attach_wake_lock");
	tcpc->detach_wake_lock =
		wakeup_source_register(&tcpc->dev, "tcpc_detach_wake_lock");

	tcpci_timer_init(tcpc);
#ifdef CONFIG_USB_POWER_DELIVERY
	pd_core_init(tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */

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

	schedule_delayed_work(
		&tcpc->event_init_work, msecs_to_jiffies(10*1000));

	pr_info("%s : tcpc irq enable OK!\n", __func__);
	return 0;
}

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
#ifdef ADAPT_CHARGER_V1
	tcpc->chg_psy = power_supply_get_by_name("charger");
#else
	tcpc->chg_psy = devm_power_supply_get_by_phandle(
		tcpc->dev.parent, "charger");
#endif
	if (IS_ERR_OR_NULL(tcpc->chg_psy)) {
		tcpci_unlock_typec(tcpc);
		TCPC_ERR("%s get charger psy fail\n", __func__);
		return;
	}
#endif /* CONFIG_USB_PD_WAIT_BC12 */
	tcpc->pd_inited_flag = 1; /* MTK Only */
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

static void tcpc_init_work(struct work_struct *work)
{
	struct tcpc_device *tcpc = container_of(
		work, struct tcpc_device, init_work.work);

#ifndef CONFIG_TCPC_NOTIFIER_LATE_SYNC
	if (tcpc->desc.notifier_supply_num == 0)
		return;
#endif
	pr_info("%s force start\n", __func__);

	tcpc->desc.notifier_supply_num = 0;
	tcpc_device_irq_enable(tcpc);
}

int tcpc_schedule_init_work(struct tcpc_device *tcpc)
{
#ifndef CONFIG_TCPC_NOTIFIER_LATE_SYNC
	if (tcpc->desc.notifier_supply_num == 0)
		return tcpc_device_irq_enable(tcpc);

	pr_info("%s wait %d num\n", __func__, tcpc->desc.notifier_supply_num);

	schedule_delayed_work(
		&tcpc->init_work, msecs_to_jiffies(30*1000));
#endif
	return 0;
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

#ifndef CONFIG_TCPC_NOTIFIER_LATE_SYNC
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
#endif
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

void tcpci_unlock_typec(struct tcpc_device *tcpc)
{
	mutex_unlock(&tcpc->typec_lock);
}

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


#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_TCPC_NOTIFIER_LATE_SYNC
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
static int fg_bat_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct pd_port *pd_port = container_of(nb, struct pd_port, fg_bat_nb);
	struct tcpc_device *tcpc = pd_port->tcpc;

	switch (event) {
	case EVENT_BATTERY_PLUG_OUT:
		dev_info(&tcpc->dev, "%s: fg battery absent\n", __func__);
		schedule_work(&pd_port->fg_bat_work);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_TCPC_NOTIFIER_LATE_SYNC */
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TCPC_NOTIFIER_LATE_SYNC
static int __tcpc_class_complete_work(struct device *dev, void *data)
{
	struct tcpc_device *tcpc = dev_get_drvdata(dev);
#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
	struct notifier_block *fg_bat_nb = &tcpc->pd_port.fg_bat_nb;
	int ret = 0;
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */

	if (tcpc != NULL) {
		pr_info("%s = %s\n", __func__, dev_name(dev));

		tcpc_device_irq_enable(tcpc);

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
		fg_bat_nb->notifier_call = fg_bat_notifier_call;
#if CONFIG_MTK_GAUGE_VERSION == 30
		ret = register_battery_notifier(fg_bat_nb);
#endif
		if (ret < 0) {
			pr_notice("%s: register bat notifier fail\n", __func__);
			return -EINVAL;
		}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */
	}
	return 0;
}

static int __init tcpc_class_complete_init(void)
{
	if (!IS_ERR(tcpc_class)) {
		class_for_each_device(tcpc_class, NULL, NULL,
			__tcpc_class_complete_work);
	}
	return 0;
}
late_initcall_sync(tcpc_class_complete_init);
#endif /* CONFIG_TCPC_NOTIFIER_LATE_SYNC */

MODULE_DESCRIPTION("Richtek TypeC Port Control Core");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(TCPC_CORE_VERSION);
MODULE_LICENSE("GPL");

/* Release Version
 * 2.0.17_MTK
 * (1) Add CONFIG_TYPEC_LEGACY3_ALWAYS_LOCAL_RP
 * (2) Fix a synchronization/locking problem in pd_notify_pe_error_recovery()
 * (3) Add USB_VID_MQP
 * (4) Revise the return value checking of tcpc_device_register()
 *
 * 2.0.16_MTK
 * (1) Check the return value of wait_event_interruptible()
 * (2) Revise *_get_cc()
 * (3) Revise role_def
 * (4) Fix COMMON.CHECK.PD.10
 *
 * 2.0.15_MTK
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
 * 2.0.14_MTK
 * (1) Move out typec_port registration and operation to rt_pd_manager.c
 * (2) Rename CONFIG_TYPEC_WAIT_BC12 to CONFIG_USB_PD_WAIT_BC12
 * (3) Not to set power/data/vconn role repeatedly
 * (4) Revise vconn highV protection
 * (5) Revise tcpc timer
 * (6) Reduce IBUS Iq for MT6371, MT6372 and MT6360
 * (7) Decrease VBUS present threshold (VBUS_CAL) by 60mV (2LSBs) for RT171x
 * (8) Replace \r\n with \n for resolving logs without newlines
 * (9) Remove the member time_stamp from struct pd_msg
 * (10) Remove NoResponseTimer as Sink for new PD spec
 * (11) Revise responses of Reject and Not_Supported
 * (12) Revise the usages of pd_traffic_control and typec_power_ctrl
 * (13) Revise the usages of wait_event_*()
 * (14) Add PD capability for TYPEC_ATTACHED_DBGACC_SNK
 * (15) Utilize rt-regmap to reduce I2C accesses
 *
 * 2.0.13_MTK
 * (1) Add TCPC flags for VCONN_SAFE5V_ONLY
 * (2) Add boolean property attemp_discover_svid in dts/dtsi
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
 * (11) Bypass BC1.2 for PR_SWAP from Source to Sink
 * (12) Support charging icon for AudioAccessory
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
 * 2.0.12_MTK
 * (1) Fix voltage/current steps of RDO for APDO
 * (2) Non-blocking TCPC notification by default
 * (3) Fix synchronization/locking problems
 * (4) Fix NoRp.SRC support
 *
 * 2.0.11_MTK
 * (1) Fix PD compliance failures of Ellisys and MQP
 * (2) Wait the result of BC1.2 before starting PD policy engine
 * (3) Fix compile warnings
 * (4) Fix NoRp.SRC support
 *
 * 2.0.10_MTK
 * (1) fix battery noitifier plug out cause recursive locking detected in
 *     nh->srcu.
 *
 * 2.0.9_MTK
 * (1) fix 10k A-to-C legacy cable workaround side effect when
 *     cable plug in at worakround flow.
 *
 * 2.0.8_MTK
 * (1) fix timeout thread flow for wakeup pd event thread
 *     after disable timer first.
 *
 * 2.0.7_MTK
 * (1) add extract pd source capability pdo defined in
 *     PD30 v1.1 ECN for pe40 get apdo profile.
 *
 * 2.0.6_MTK
 * (1) register battery notifier for battery plug out
 *     avoid TA hardreset 3 times will show charing icon.
 *
 * 2.0.5_MTK
 * (1) add CONFIG_TYPEC_CAP_NORP_SRC to support
 *      A-to-C No-Rp cable.
 * (2) add handler pd eint with eint mask
 *
 * 2.0.4_MTK
 * (1) add CONFIG_TCPC_NOTIFIER_LATE_SYNC to
 *      move irq_enable to late_initcall_sync stage
 *      to prevent from notifier_supply_num setting wrong.
 *
 * 2.0.3_MTK
 * (1) use local_irq_XXX to instead raw_local_irq_XXX
 *      to fix lock prov WARNING
 * (2) Remove unnecessary charger detect flow. it does
 *      not need to switch BC1-2 path on otg_en
 *
 * 2.0.2_MTK
 * (1) Fix Coverity and check patch issue
 * (2) Fix 32-bit project build error
 *
 * 2.0.1_MTK
 *	First released PD3.0 Driver for MTK Platform
 */

