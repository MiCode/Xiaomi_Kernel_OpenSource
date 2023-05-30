// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/vmalloc.h>
#include <linux/preempt.h>
#include <pd_core.h>

#include "adapter_class.h"

static struct class *adapter_class;

static const char * const usbpd_state_strings[] = {
	"SRC_START",
	"SRC_DISC",
	"SRC_SEND_CAP",
	"SRC_NEG_CAP",
	"SRC_TRANS_SUPPLY",
	"SRC_TRANS_SUPPLY2",
	"SRC_Ready",
	"SRC_DISABLED",
	"SRC_CAP_RESP",
	"SRC_HRESET",
	"SRC_HRESET_RECV",
	"SRC_TRANS_DFT",
	"SRC_GET_CAP",
	"SRC_WAIT_CAP",
	"SRC_SEND_SRESET",
	"SRC_SRESET",
	"SRC_CBL_SEND_SRESET",
	"SRC_VDM_ID_REQ",
	"SRC_VDM_ID_ACK",
	"SRC_VDM_ID_NAK",
	"SRC_NO_SUPP",
	"SRC_NO_SUPP_RECV",
	"SRC_CK_RECV",
	"SRC_ALERT",
	"SRC_RECV_ALERT",
	"SRC_GIVE_CAP_EXT",
	"SRC_GIVE_STATUS",
	"SRC_GET_STATUS",
	"SNK_START",
	"SNK_DISC",
	"SNK_WAIT_CAP",
	"SNK_EVA_CAP",
	"SNK_SEL_CAP",
	"SNK_TRANS_SINK",
	"SNK_Ready",
	"SNK_HRESET",
	"SNK_TRANS_DFT",
	"SNK_GIVE_CAP",
	"SNK_GET_CAP",
	"SNK_SEND_SRESET",
	"SNK_SRESET",
	"SNK_NO_SUPP",
	"SNK_NO_SUPP_RECV",
	"SNK_CK_RECV",
	"SNK_RECV_ALERT",
	"SNK_ALERT",
	"SNK_GET_CAP_EX",
	"SNK_GET_STATUS",
	"SNK_GIVE_STATUS",
	"SNK_GET_PPS",
	"D_DFP_EVA",
	"D_DFP_ACCEPT",
	"D_DFP_CHANGE",
	"D_DFP_SEND",
	"D_DFP_REJECT",
	"D_UFP_EVA",
	"D_UFP_ACCEPT",
	"D_UFP_CHANGE",
	"D_UFP_SEND",
	"D_UFP_REJECT",
	"P_SRC_EVA",
	"P_SRC_ACCEPT",
	"P_SRC_TRANS_OFF",
	"P_SRC_ASSERT",
	"P_SRC_WAIT_ON",
	"P_SRC_SEND",
	"P_SRC_REJECT",
	"P_SNK_EVA",
	"P_SNK_ACCEPT",
	"P_SNK_TRANS_OFF",
	"P_SNK_ASSERT",
	"P_SNK_SOURCE_ON",
	"P_SNK_SEND",
	"P_SNK_REJECT",
	"DR_SRC_GET_CAP",
	"DR_SRC_GIVE_CAP",
	"DR_SNK_GET_CAP",
	"DR_SNK_GIVE_CAP",
	"DR_SNK_GIVE_CAP_EXT",
	"DR_SRC_GET_CAP_EXT",
	"V_SEND",
	"V_EVA",
	"V_ACCEPT",
	"V_REJECT",
	"V_WAIT_VCONN",
	"V_TURN_OFF",
	"V_TURN_ON",
	"V_PS_RDY",
	"U_GET_ID",
	"U_GET_SVID",
	"U_GET_MODE",
	"U_EVA_MODE",
	"U_MODE_EX",
	"U_ATTENTION",
	"U_D_STATUS",
	"U_D_CONFIG",
	"D_UID_REQ",
	"D_UID_A",
	"D_UID_N",
	"D_CID_REQ",
	"D_CID_ACK",
	"D_CID_NAK",
	"D_SVID_REQ",
	"D_SVID_ACK",
	"D_SVID_NAK",
	"D_MODE_REQ",
	"D_MODE_ACK",
	"D_MODE_NAK",
	"D_MODE_EN_REQ",
	"D_MODE_EN_ACK",
	"D_MODE_EN_NAK",
	"D_MODE_EX_REQ",
	"D_MODE_EX_ACK",
	"D_ATTENTION",
	"D_C_SRESET",
	"D_C_CRESET",
	"D_DP_STATUS_REQ",
	"D_DP_STATUS_ACK",
	"D_DP_STATUS_NAK",
	"D_DP_CONFIG_REQ",
	"D_DP_CONFIG_ACK",
	"D_DP_CONFIG_NAK",
	"U_UVDM_RECV",
	"D_UVDM_SEND",
	"D_UVDM_ACKED",
	"D_UVDM_NAKED",
	"U_SEND_NAK",
	"GET_BAT_CAP",
	"GIVE_BAT_CAP",
	"GET_BAT_STATUS",
	"GIVE_BAT_STATUS",
	"GET_MFRS_INFO",
	"GIVE_MFRS_INFO",
	"GET_CC",
	"GIVE_CC",
	"GET_CI",
	"GIVE_CI",
	"VDM_NO_SUPP",
	"DBG_READY",
	"REJECT",
	"ERR_RECOVERY",
	"ERR_RECOVERY1",
	"BIST_TD",
	"BIST_C2",
	"UNEXPECTED_TX",
	"SEND_SRESET_TX",
	"RECV_SRESET_TX",
	"SEND_SRESET_STANDBY",
	"IDLE1",
	"IDLE2",
};

static ssize_t name_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);

	return snprintf(buf, 20, "%s\n",
		       adapter_dev->props.alias_name ?
		       adapter_dev->props.alias_name : "anonymous");
}

static void adapter_device_release(struct device *dev)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);

	kfree(adapter_dev);
}

int adapter_dev_get_property(struct adapter_device *adapter_dev,
	enum adapter_property sta)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->get_property)
		return adapter_dev->ops->get_property(adapter_dev, sta);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_get_property);

int adapter_dev_get_status(struct adapter_device *adapter_dev,
	struct adapter_status *sta)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->get_status)
		return adapter_dev->ops->get_status(adapter_dev, sta);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_get_status);

int adapter_dev_get_output(struct adapter_device *adapter_dev, int *mV, int *mA)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->get_output)
		return adapter_dev->ops->get_output(adapter_dev, mV, mA);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_get_output);

int adapter_dev_set_cap(struct adapter_device *adapter_dev,
	enum adapter_cap_type type,
	int mV, int mA)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->set_cap)
		return adapter_dev->ops->set_cap(adapter_dev, type, mV, mA);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_set_cap);


int adapter_dev_get_cap(struct adapter_device *adapter_dev,
	enum adapter_cap_type type,
	struct adapter_power_cap *cap)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
		adapter_dev->ops->get_cap)
		return adapter_dev->ops->get_cap(adapter_dev, type, cap);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_get_cap);

int adapter_dev_authentication(struct adapter_device *adapter_dev,
			       struct adapter_auth_data *data)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->authentication)
		return adapter_dev->ops->authentication(adapter_dev, data);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_authentication);

int adapter_dev_is_cc(struct adapter_device *adapter_dev, bool *cc)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->is_cc)
		return adapter_dev->ops->is_cc(adapter_dev, cc);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_is_cc);

int adapter_dev_set_wdt(struct adapter_device *adapter_dev, u32 ms)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->set_wdt)
		return adapter_dev->ops->set_wdt(adapter_dev, ms);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_set_wdt);

int adapter_dev_enable_wdt(struct adapter_device *adapter_dev, bool en)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->enable_wdt)
		return adapter_dev->ops->enable_wdt(adapter_dev, en);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_enable_wdt);

int adapter_dev_sync_volt(struct adapter_device *adapter_dev, u32 mV)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->sync_volt)
		return adapter_dev->ops->sync_volt(adapter_dev, mV);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_sync_volt);

int adapter_dev_send_hardreset(struct adapter_device *adapter_dev)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->send_hardreset)
		return adapter_dev->ops->send_hardreset(adapter_dev);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_send_hardreset);

int adapter_dev_set_cap_xm(struct adapter_device *adapter_dev,
	enum adapter_cap_type type,
	int mV, int mA)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->set_cap_xm)
		return adapter_dev->ops->set_cap_xm(adapter_dev, type, mV, mA);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_set_cap_xm);
int adapter_dev_get_id(struct adapter_device *adapter_dev)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_svid)
		return adapter_dev->ops->get_svid(adapter_dev);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_get_id);
int adapter_dev_get_svid(struct adapter_device *adapter_dev)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_svid)
		return adapter_dev->ops->get_svid(adapter_dev);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_get_svid);
int adapter_dev_request_vdm_cmd(struct adapter_device *adapter_dev, enum uvdm_state cmd, unsigned char *data, unsigned int data_len)
{
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->request_vdm_cmd)
		return adapter_dev->ops->request_vdm_cmd(adapter_dev, cmd, data, data_len);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(adapter_dev_request_vdm_cmd);
static ssize_t adapter_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_svid)
		adapter_dev->ops->get_svid(adapter_dev);
	pr_info("%s: adapter_id is %08x\n", __func__, adapter_dev->adapter_id);
	return snprintf(buf, PAGE_SIZE, "%08x\n", adapter_dev->adapter_id);
}
static ssize_t adapter_svid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_svid)
		adapter_dev->ops->get_svid(adapter_dev);
	pr_info("%s: adapter_svid is %04x\n", __func__, adapter_dev->adapter_svid);
	return snprintf(buf, PAGE_SIZE, "%04x\n", adapter_dev->adapter_svid);
}
static int StringToHex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while (cnt < (tmplen / 2)) {
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p++;
		cnt++;
	}
	if (tmplen % 2 != 0)
		out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
	if (outlen != NULL)
		*outlen = tmplen / 2 + tmplen % 2;
	return tmplen / 2 + tmplen % 2;
}
static ssize_t request_vdm_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int cmd, ret;
	unsigned char buffer[64];
	unsigned char *data;
	unsigned int count;
	int i;
	if (in_interrupt()) {
		data = kmalloc(40, GFP_ATOMIC);
		pr_info("%s: kmalloc atomic ok.\n", __func__);
	} else {
		data = kmalloc(40, GFP_KERNEL);
		pr_info("%s: kmalloc kernel ok.\n", __func__);
	}
	memset(data, 0, 40);
	ret = sscanf(buf, "%d,%s\n", &cmd, buffer);
	pr_info("%s:cmd:%d, buffer:%s\n", __func__, cmd, buffer);
	StringToHex(buffer, data, &count);
	pr_info("%s:count = %d\n", __func__, count);
	for (i = 0; i < count; i++)
		pr_info("%02x", data[i]);
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->request_vdm_cmd) {
		adapter_dev->ops->request_vdm_cmd(adapter_dev, cmd, data, count);
	}
	kfree(data);
	return size;
}
static ssize_t request_vdm_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int i;
	char data[16], str_buf[128] = {0};
	int cmd = adapter_dev->uvdm_state;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		return snprintf(buf, PAGE_SIZE, "%d,%x\n", cmd, adapter_dev->vdm_data.ta_version);
	case USBPD_UVDM_CHARGER_TEMP:
		return snprintf(buf, PAGE_SIZE, "%d,%d\n", cmd, adapter_dev->vdm_data.ta_temp);
	case USBPD_UVDM_CHARGER_VOLTAGE:
		return snprintf(buf, PAGE_SIZE, "%d,%d\n", cmd, adapter_dev->vdm_data.ta_voltage);
	case USBPD_UVDM_SESSION_SEED:
	case USBPD_UVDM_CONNECT:
	case USBPD_UVDM_DISCONNECT:
	case USBPD_UVDM_VERIFIED:
	case USBPD_UVDM_REMOVE_COMPENSATION:
	case USBPD_UVDM_NAN_ACK:
		return snprintf(buf, PAGE_SIZE, "%d,Null\n", cmd);
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			memset(data, 0, sizeof(data));
			snprintf(data, sizeof(data), "%08lx", adapter_dev->vdm_data.digest[i]);
			strlcat(str_buf, data, sizeof(str_buf));
		}
		return snprintf(buf, PAGE_SIZE, "%d,%s\n", cmd, str_buf);
	case USBPD_UVDM_REVERSE_AUTHEN:
		return snprintf(buf, PAGE_SIZE, "%d,%d", cmd, adapter_dev->vdm_data.reauth);
	default:
		pr_err("feedbak cmd:%d is not support\n", cmd);
		break;
	}
	return snprintf(buf, PAGE_SIZE, "%d,%s\n", cmd, str_buf);
}
static ssize_t verify_process_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int val;
	if (sscanf(buf, "%d\n", &val) != 1) {
		adapter_dev->verify_process = 0;
		return -EINVAL;
	}
	adapter_dev->verify_process = !!val;
	pr_info("%s: batterysecret verify process :%d\n", __func__, adapter_dev->verify_process);
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->set_pd_verify_process)
		adapter_dev->ops->set_pd_verify_process(adapter_dev, adapter_dev->verify_process);
	return size;
}
static ssize_t verify_process_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", adapter_dev->verify_process);
}
static ssize_t usbpd_verifed_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int val = 0;
	struct adapter_power_cap cap = {0};
	if (sscanf(buf, "%d\n", &val) != 1) {
		adapter_dev->verifed = 0;
		return -EINVAL;
	}
	pr_info("%s: batteryd set usbpd verifyed :%d\n", __func__, val);
	adapter_dev->verifed = !!val;
	if (adapter_dev->verifed) {
		if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_cap)
			adapter_dev->ops->get_cap(adapter_dev, MTK_PD_APDO_REGAIN, &cap);
	} else {
		if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_cap)
			adapter_dev->ops->get_cap(adapter_dev, MTK_CAP_TYPE_UNKNOWN, &cap);
	}
	return size;
}
static ssize_t usbpd_verifed_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", adapter_dev->verifed);
}
static ssize_t current_pr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	const char *pr = "none";
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_power_role)
		adapter_dev->ops->get_power_role(adapter_dev);
	pr_info("%s: current_pr is %d\n", __func__, adapter_dev->role);
	if (adapter_dev->role == PD_ROLE_SINK_FOR_ADAPTER)
		pr = "sink";
	else if (adapter_dev->role == PD_ROLE_SOURCE_FOR_ADAPTER)
		pr = "source";
	return snprintf(buf, PAGE_SIZE, "%s\n", pr);
}
static ssize_t current_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_current_state)
		adapter_dev->ops->get_current_state(adapter_dev);
	pr_err("%s: current_state is %d\n", __func__, adapter_dev->current_state);
	if (adapter_dev->current_state >= (sizeof(usbpd_state_strings) / sizeof(usbpd_state_strings[0])))
		adapter_dev->current_state = 0;
	pr_err("%s: %s\n", __func__, usbpd_state_strings[adapter_dev->current_state]);
	return snprintf(buf, PAGE_SIZE, "%s\n", usbpd_state_strings[adapter_dev->current_state]);
}
static ssize_t pdo_n_show(struct device *dev, struct device_attribute *attr, char *buf);
#define PDO_ATTR(n) {					\
	.attr	= { .name = __stringify(pdo##n), .mode = 0444 },	\
	.show	= pdo_n_show,				\
}
static struct device_attribute dev_attr_pdos[] = {
	PDO_ATTR(1),
	PDO_ATTR(2),
	PDO_ATTR(3),
	PDO_ATTR(4),
	PDO_ATTR(5),
	PDO_ATTR(6),
	PDO_ATTR(7),
};
static ssize_t pdo_n_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int i;
	if (adapter_dev != NULL && adapter_dev->ops != NULL && adapter_dev->ops->get_pdos)
		adapter_dev->ops->get_pdos(adapter_dev);
	for (i = 0; i < ARRAY_SIZE(dev_attr_pdos); i++) {
		if (attr == &dev_attr_pdos[i])
			return snprintf(buf, PAGE_SIZE, "%08x\n", adapter_dev->received_pdos[i]);
	}
	pr_err("%s: Invalid PDO index\n", __func__);
	return -EINVAL;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(adapter_id);
static DEVICE_ATTR_RO(adapter_svid);
static DEVICE_ATTR_RW(request_vdm_cmd);
static DEVICE_ATTR_RW(verify_process);
static DEVICE_ATTR_RW(usbpd_verifed);
static DEVICE_ATTR_RO(current_pr);
static DEVICE_ATTR_RO(current_state);

static struct attribute *adapter_class_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_request_vdm_cmd.attr,
	&dev_attr_current_state.attr,
	&dev_attr_adapter_id.attr,
	&dev_attr_adapter_svid.attr,
	&dev_attr_verify_process.attr,
	&dev_attr_usbpd_verifed.attr,
	&dev_attr_current_pr.attr,
	&dev_attr_pdos[0].attr,
	&dev_attr_pdos[1].attr,
	&dev_attr_pdos[2].attr,
	&dev_attr_pdos[3].attr,
	&dev_attr_pdos[4].attr,
	&dev_attr_pdos[5].attr,
	&dev_attr_pdos[6].attr,
	NULL,
};

static const struct attribute_group adapter_group = {
	.attrs = adapter_class_attrs,
};

static const struct attribute_group *adapter_groups[] = {
	&adapter_group,
	NULL,
};

int register_adapter_device_notifier(struct adapter_device *adapter_dev,
				struct notifier_block *nb)
{
	int ret = 0;

	if (!adapter_dev)
		return -ENODEV;

	ret = srcu_notifier_chain_register(&adapter_dev->evt_nh, nb);
	return ret;
}
EXPORT_SYMBOL(register_adapter_device_notifier);

int unregister_adapter_device_notifier(struct adapter_device *adapter_dev,
				struct notifier_block *nb)
{
	if (!adapter_dev)
		return -ENODEV;

	return srcu_notifier_chain_unregister(&adapter_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_adapter_device_notifier);

/**
 * adapter_device_register - create and register a new object of
 *   adapter_device class.
 * @name: the name of the new object
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use.
 * The methods may retrieve it by using adapter_get_data(adapter_dev).
 * @ops: the charger operations structure.
 *
 * Creates and registers new charger device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct adapter_device *adapter_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct adapter_ops *ops,
		const struct adapter_properties *props)
{
	struct adapter_device *adapter_dev;
	static struct lock_class_key key;
	struct srcu_notifier_head *head;
	int rc;

	pr_notice("%s: name=%s\n", __func__, name);
	adapter_dev = kzalloc(sizeof(*adapter_dev), GFP_KERNEL);
	if (!adapter_dev)
		return ERR_PTR(-ENOMEM);

	head = &adapter_dev->evt_nh;
	srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);
	mutex_init(&adapter_dev->ops_lock);
	adapter_dev->dev.class = adapter_class;
	adapter_dev->dev.parent = parent;
	adapter_dev->dev.release = adapter_device_release;
	dev_set_name(&adapter_dev->dev, name);
	dev_set_drvdata(&adapter_dev->dev, devdata);

	/* Copy properties */
	if (props) {
		memcpy(&adapter_dev->props, props,
		       sizeof(struct adapter_properties));
	}
	rc = device_register(&adapter_dev->dev);
	if (rc) {
		kfree(adapter_dev);
		return ERR_PTR(rc);
	}
	adapter_dev->ops = ops;
	return adapter_dev;
}
EXPORT_SYMBOL(adapter_device_register);

/**
 * adapter_device_unregister - unregisters a switching charger device
 * object.
 * @adapter_dev: the switching charger device object to be unregistered
 * and freed.
 *
 * Unregisters a previously registered via adapter_device_register object.
 */
void adapter_device_unregister(struct adapter_device *adapter_dev)
{
	if (!adapter_dev)
		return;

	mutex_lock(&adapter_dev->ops_lock);
	adapter_dev->ops = NULL;
	mutex_unlock(&adapter_dev->ops_lock);
	device_unregister(&adapter_dev->dev);
}
EXPORT_SYMBOL(adapter_device_unregister);


static int adapter_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct adapter_device *get_adapter_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return (struct adapter_device *)NULL;
	dev = class_find_device(adapter_class, NULL, name,
				adapter_match_device_by_name);

	return dev ? to_adapter_device(dev) : NULL;

}
EXPORT_SYMBOL(get_adapter_by_name);

static void __exit adapter_class_exit(void)
{
	class_destroy(adapter_class);
}

static int __init adapter_class_init(void)
{
	adapter_class = class_create(THIS_MODULE, "Charging_Adapter");
	if (IS_ERR(adapter_class)) {
		pr_notice("Unable to create Charging Adapter class; errno = %ld\n",
			PTR_ERR(adapter_class));
		return PTR_ERR(adapter_class);
	}
	adapter_class->dev_groups = adapter_groups;
	return 0;
}

module_init(adapter_class_init);
module_exit(adapter_class_exit);

MODULE_DESCRIPTION("Adapter Class Device");
MODULE_AUTHOR("Wy Chuang <wy.chuang@mediatek.com>");
MODULE_LICENSE("GPL");
