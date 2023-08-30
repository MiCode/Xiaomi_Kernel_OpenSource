
#include "inc/xm_pd_adapter.h"
#include "inc/xm_adapter_class.h"
#include "inc/rt17xx_iio.h"

static struct class *adapter_class;

static int log_level = 2;
#define class_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[xm_adapter_class] " fmt, ##__VA_ARGS__);	\
} while (0)

#define class_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[xm_adapter_class] " fmt, ##__VA_ARGS__);	\
} while (0)

#define class_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[xm_adapter_class] " fmt, ##__VA_ARGS__);	\
} while (0)

static const char * const usbpd_state_strings[] = {
	"UNKNOWN",
/******************* Source *******************/
#ifdef CONFIG_USB_PD_PE_SOURCE
	"SRC_STARTUP",
	"SRC_DISCOVERY",
	"SRC_SEND_CAPABILITIES",
	"SRC_NEGOTIATE_CAPABILITIES",
	"SRC_TRANSITION_SUPPLY",
	"SRC_TRANSITION_SUPPLY2",
	"SRC_Ready",
	"SRC_DISABLED",
	"SRC_CAPABILITY_RESPONSE",
	"SRC_HARD_RESET",
	"SRC_HARD_RESET_RECEIVED",
	"SRC_TRANSITION_TO_DEFAULT",
	"SRC_GET_SINK_CAP",
	"SRC_WAIT_NEW_CAPABILITIES",
	"SRC_SEND_SOFT_RESET",
	"SRC_SOFT_RESET",
/* Source Startup Discover Cable */
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#ifdef CONFIG_PD_SRC_RESET_CABLE
	"SRC_CBL_SEND_SOFT_RESET",
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
	"SRC_VDM_IDENTITY_REQUEST",
	"SRC_VDM_IDENTITY_ACKED",
	"SRC_VDM_IDENTITY_NAKED",
#endif	/* PD_CAP_PE_SRC_STARTUP_DISCOVER_ID */
/* Source for PD30 */
#ifdef CONFIG_USB_PD_REV30
	"SRC_SEND_NOT_SUPPORTED",
	"SRC_NOT_SUPPORTED_RECEIVED",
	"SRC_CHUNK_RECEIVED",
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	"SRC_SEND_SOURCE_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	"SRC_SINK_ALERT_RECEIVED",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	"SRC_GIVE_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	"SRC_GIVE_SOURCE_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	"SRC_GET_SINK_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	"SRC_GIVE_PPS_STATUS",
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SOURCE */
/******************* Sink *******************/
#ifdef CONFIG_USB_PD_PE_SINK
/* Sink Init */
	"SNK_STARTUP",
	"SNK_DISCOVERY",
	"SNK_WAIT_FOR_CAPABILITIES",
	"SNK_EVALUATE_CAPABILITY",
	"SNK_SELECT_CAPABILITY",
	"SNK_TRANSITION_SINK",
	"SNK_Ready",
	"SNK_HARD_RESET",
	"SNK_TRANSITION_TO_DEFAULT",
	"SNK_GIVE_SINK_CAP",
	"SNK_GET_SOURCE_CAP",

	"SNK_SEND_SOFT_RESET",
	"SNK_SOFT_RESET",
/* Sink for PD30 */
#ifdef CONFIG_USB_PD_REV30
	"SNK_SEND_NOT_SUPPORTED",
	"SNK_NOT_SUPPORTED_RECEIVED",
	"SNK_CHUNK_RECEIVED",
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	"SNK_SOURCE_ALERT_RECEIVED",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	"SNK_SEND_SINK_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	"SNK_GET_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	"SNK_GET_SOURCE_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	"SNK_GIVE_SINK_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	"SNK_GET_PPS_STATUS",
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SINK */
/******************* DR_SWAP *******************/
#ifdef CONFIG_USB_PD_DR_SWAP
/* DR_SWAP_DFP */
	"DRS_DFP_UFP_EVALUATE_DR_SWAP",
	"DRS_DFP_UFP_ACCEPT_DR_SWAP",
	"DRS_DFP_UFP_CHANGE_TO_UFP",
	"DRS_DFP_UFP_SEND_DR_SWAP",
	"DRS_DFP_UFP_REJECT_DR_SWAP",
/* DR_SWAP_UFP */
	"DRS_UFP_DFP_EVALUATE_DR_SWAP",
	"DRS_UFP_DFP_ACCEPT_DR_SWAP",
	"DRS_UFP_DFP_CHANGE_TO_DFP",
	"DRS_UFP_DFP_SEND_DR_SWAP",
	"DRS_UFP_DFP_REJECT_DR_SWAP",
#endif	/* CONFIG_USB_PD_DR_SWAP */
/******************* PR_SWAP *******************/
#ifdef CONFIG_USB_PD_PR_SWAP
/* PR_SWAP_SRC */
	"PRS_SRC_SNK_EVALUATE_PR_SWAP",
	"PRS_SRC_SNK_ACCEPT_PR_SWAP",
	"PRS_SRC_SNK_TRANSITION_TO_OFF",
	"PRS_SRC_SNK_ASSERT_RD",
	"PRS_SRC_SNK_WAIT_SOURCE_ON",
	"PRS_SRC_SNK_SEND_SWAP",
	"PRS_SRC_SNK_REJECT_PR_SWAP",
/* PR_SWAP_SNK */
	"PRS_SNK_SRC_EVALUATE_PR_SWAP",
	"PRS_SNK_SRC_ACCEPT_PR_SWAP",
	"PRS_SNK_SRC_TRANSITION_TO_OFF",
	"PRS_SNK_SRC_ASSERT_RP",
	"PRS_SNK_SRC_SOURCE_ON",
	"PRS_SNK_SRC_SEND_SWAP",
	"PRS_SNK_SRC_REJECT_SWAP",
/* get same role cap */
	"DR_SRC_GET_SOURCE_CAP",
	"DR_SRC_GIVE_SINK_CAP",
	"DR_SNK_GET_SINK_CAP",
	"DR_SNK_GIVE_SOURCE_CAP",
/* get same role cap for PD30 */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	"DR_SNK_GIVE_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	"DR_SRC_GET_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PR_SWAP */
/******************* VCONN_SWAP *******************/
#ifdef CONFIG_USB_PD_VCONN_SWAP
	"VCS_SEND_SWAP",
	"VCS_EVALUATE_SWAP",
	"VCS_ACCEPT_SWAP",
	"VCS_REJECT_VCONN_SWAP",
	"VCS_WAIT_FOR_VCONN",
	"VCS_TURN_OFF_VCONN",
	"VCS_TURN_ON_VCONN",
	"VCS_SEND_PS_RDY",
#endif	/* CONFIG_USB_PD_VCONN_SWAP */
/******************* UFP_VDM *******************/
	"UFP_VDM_GET_IDENTITY",
	"UFP_VDM_GET_SVIDS",
	"UFP_VDM_GET_MODES",
	"UFP_VDM_EVALUATE_MODE_ENTRY",
	"UFP_VDM_MODE_EXIT",
	"UFP_VDM_ATTENTION_REQUEST",
#ifdef CONFIG_USB_PD_ALT_MODE
	"UFP_VDM_DP_STATUS_UPDATE",
	"UFP_VDM_DP_CONFIGURE",
#endif/* CONFIG_USB_PD_ALT_MODE */
/******************* DFP_VDM *******************/
	"DFP_UFP_VDM_IDENTITY_REQUEST",
	"DFP_UFP_VDM_IDENTITY_ACKED",
	"DFP_UFP_VDM_IDENTITY_NAKED",
	"DFP_CBL_VDM_IDENTITY_REQUEST",
	"DFP_CBL_VDM_IDENTITY_ACKED",
	"DFP_CBL_VDM_IDENTITY_NAKED",
	"DFP_VDM_SVIDS_REQUEST",
	"DFP_VDM_SVIDS_ACKED",
	"DFP_VDM_SVIDS_NAKED",
	"DFP_VDM_MODES_REQUEST",
	"DFP_VDM_MODES_ACKED",
	"DFP_VDM_MODES_NAKED",
	"DFP_VDM_MODE_ENTRY_REQUEST",
	"DFP_VDM_MODE_ENTRY_ACKED",
	"DFP_VDM_MODE_ENTRY_NAKED",
	"DFP_VDM_MODE_EXIT_REQUEST",
	"DFP_VDM_MODE_EXIT_ACKED",
	"DFP_VDM_ATTENTION_REQUEST",
#ifdef CONFIG_PD_DFP_RESET_CABLE
	"DFP_CBL_SEND_SOFT_RESET",
	"DFP_CBL_SEND_CABLE_RESET",
#endif	/* CONFIG_PD_DFP_RESET_CABLE */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	"DFP_VDM_DP_STATUS_UPDATE_REQUEST",
	"DFP_VDM_DP_STATUS_UPDATE_ACKED",
	"DFP_VDM_DP_STATUS_UPDATE_NAKED",
	"DFP_VDM_DP_CONFIGURATION_REQUEST",
	"DFP_VDM_DP_CONFIGURATION_ACKED",
	"DFP_VDM_DP_CONFIGURATION_NAKED",
#endif/* CONFIG_USB_PD_ALT_MODE_DFP */
/******************* UVDM & SVDM *******************/
#ifdef CONFIG_USB_PD_CUSTOM_VDM
	"UFP_UVDM_RECV",
	"DFP_UVDM_SEND",
	"DFP_UVDM_ACKED",
	"DFP_UVDM_NAKED",
#endif/* CONFIG_USB_PD_CUSTOM_VDM */
/******************* PD30 Common *******************/
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	"GET_BATTERY_CAP",
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
	"GIVE_BATTERY_CAP",
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	"GET_BATTERY_STATUS",
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
	"GIVE_BATTERY_STATUS",
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	"GET_MANUFACTURER_INFO",
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
	"GIVE_MANUFACTURER_INFO",
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	"GET_COUNTRY_CODES",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
	"GIVE_COUNTRY_CODES",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	"GET_COUNTRY_INFO",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
	"GIVE_COUNTRY_INFO",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */
	"VDM_NOT_SUPPORTED",
#endif /* CONFIG_USB_PD_REV30 */
/******************* Others *******************/
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	"DBG_READY",
#endif/* CONFIG_USB_PD_CUSTOM_DBGACC */
#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
	"OVER_RECV_HRESET_LIMIT",
#endif/* CONFIG_USB_PD_RECV_HRESET_COUNTER */
	"REJECT",
	"ERROR_RECOVERY",
#ifdef CONFIG_USB_PD_ERROR_RECOVERY_ONCE
	"ERROR_RECOVERY_ONCE",
#endif	/* CONFIG_USB_PD_ERROR_RECOVERY_ONCE */
	"BIST_TEST_DATA",
	"BIST_CARRIER_MODE_2",
/* Wait tx finished */
	"IDLE1",
	"IDLE2",
	"PD_NR_PE_STATES",
};

static ssize_t adapter_show_name(struct device *dev,
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

static ssize_t adapter_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);

	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
		adapter_dev->ops->get_svid) {
		adapter_dev->ops->get_svid(adapter_dev);
	}
	class_info("%s: adapter_id is %08x\n", __func__, adapter_dev->adapter_id);

	return snprintf(buf, PAGE_SIZE, "%08x\n", adapter_dev->adapter_id);
}
static DEVICE_ATTR_RO(adapter_id);

static ssize_t adapter_svid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);

	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
		adapter_dev->ops->get_svid) {
		adapter_dev->ops->get_svid(adapter_dev);
	}
	class_info("%s: adapter_svid is %04x\n", __func__, adapter_dev->adapter_svid);

	return snprintf(buf, PAGE_SIZE, "%04x\n", adapter_dev->adapter_svid);
}
static DEVICE_ATTR_RO(adapter_svid);

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

static ssize_t request_vdm_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int cmd, ret;
	unsigned char buffer[64];
	unsigned char *data;
	unsigned int count;
	int i;

	if (in_interrupt()) {
		data = kmalloc(40, GFP_ATOMIC);
		class_info("%s: kmalloc atomic ok.\n", __func__);
	} else {
		data = kmalloc(40, GFP_KERNEL);
		class_info("%s: kmalloc kernel ok.\n", __func__);
	}
	memset(data, 0, 40);

	ret = sscanf(buf, "%d,%s\n", &cmd, buffer);
	class_info("%s:cmd:%d, buffer:%s\n", __func__, cmd, buffer);

	StringToHex(buffer, data, &count);
	class_info("%s:count = %d\n", __func__, count);

	for (i = 0; i < count; i++)
		class_info("%02x", data[i]);

	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->request_vdm_cmd) {
		adapter_dev->ops->request_vdm_cmd(adapter_dev,
							cmd, data, count);
	}
	kfree(data);

	return size;
}

static ssize_t request_vdm_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int i;
	char data[16], str_buf[128] = {0};
	int cmd = adapter_dev->uvdm_state;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		return snprintf(buf, PAGE_SIZE, "%d,%x\n",
				cmd, adapter_dev->vdm_data.ta_version);
	case USBPD_UVDM_CHARGER_TEMP:
		return snprintf(buf, PAGE_SIZE, "%d,%d\n",
				cmd, adapter_dev->vdm_data.ta_temp);
	case USBPD_UVDM_CHARGER_VOLTAGE:
		return snprintf(buf, PAGE_SIZE, "%d,%d\n",
				cmd, adapter_dev->vdm_data.ta_voltage);
	case USBPD_UVDM_SESSION_SEED:
	case USBPD_UVDM_CONNECT:
	case USBPD_UVDM_DISCONNECT:
	case USBPD_UVDM_VERIFIED:
	case USBPD_UVDM_REMOVE_COMPENSATION:
	case USBPD_UVDM_NAN_ACK:
		return snprintf(buf, PAGE_SIZE, "%d,Null\n", cmd);
	case USBPD_UVDM_REVERSE_AUTHEN:
		return snprintf(buf, PAGE_SIZE, "%d,%d", cmd,
			adapter_dev->vdm_data.reauth);
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			memset(data, 0, sizeof(data));
			snprintf(data, sizeof(data), "%08lx",
					adapter_dev->vdm_data.digest[i]);
			strlcat(str_buf, data, sizeof(str_buf));
		}
		return snprintf(buf, PAGE_SIZE, "%d,%s\n", cmd, str_buf);
	default:
		class_err("feedbak cmd:%d is not support\n", cmd);
		break;
	}
	return snprintf(buf, PAGE_SIZE, "%d,%s\n", cmd, str_buf);

}
static DEVICE_ATTR_RW(request_vdm_cmd);

static ssize_t verify_process_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int val;

	if (sscanf(buf, "%d\n", &val) != 1) {
		adapter_dev->verify_process = 0;
		return -EINVAL;
	}

	adapter_dev->verify_process = !!val;

	class_info("%s: batterysecret verify process :%d\n",
		__func__, adapter_dev->verify_process);

	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
	    adapter_dev->ops->set_pd_verify_process) {
		adapter_dev->ops->set_pd_verify_process(adapter_dev,
							adapter_dev->verify_process);
	}

	return size;
}

static ssize_t verify_process_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", adapter_dev->verify_process);
}
static DEVICE_ATTR_RW(verify_process);

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
	class_info("%s: batteryd set usbpd verifyed :%d\n", __func__, val);
	adapter_dev->verifed = !!val;

	if (adapter_dev->verifed) {
		if (adapter_dev != NULL && adapter_dev->ops != NULL &&
			adapter_dev->ops->get_cap) {
			adapter_dev->ops->get_cap(adapter_dev, XM_PD_APDO_REGAIN, &cap);
		}
	}

	return size;
}

static ssize_t usbpd_verifed_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", adapter_dev->verifed);
}
static DEVICE_ATTR_RW(usbpd_verifed);

static ssize_t current_pr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	const char *pr = "none";

	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
		adapter_dev->ops->get_power_role) {
		adapter_dev->ops->get_power_role(adapter_dev);
	}
	class_info("%s: current_pr is %d\n", __func__, adapter_dev->role);
	if (adapter_dev->role == PD_ROLE_SINK_FOR_ADAPTER)
		pr = "sink";
	else if (adapter_dev->role == PD_ROLE_SOURCE_FOR_ADAPTER)
		pr = "source";

	return snprintf(buf, PAGE_SIZE, "%s\n", pr);
}
static DEVICE_ATTR_RO(current_pr);

static ssize_t current_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);

	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
		adapter_dev->ops->get_current_state) {
		adapter_dev->ops->get_current_state(adapter_dev);
	}
	class_err("%s: current_state is %d\n", __func__, adapter_dev->current_state);

	if (adapter_dev->current_state >= (sizeof(usbpd_state_strings) / sizeof(usbpd_state_strings[0])))
		adapter_dev->current_state = 0;

	class_err("%s: %s\n", __func__,
		usbpd_state_strings[adapter_dev->current_state]);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			usbpd_state_strings[adapter_dev->current_state]);
}
static DEVICE_ATTR_RO(current_state);

static ssize_t pdo_n_show(struct device *dev, struct device_attribute *attr,
		char *buf);

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
static ssize_t pdo_n_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adapter_device *adapter_dev = to_adapter_device(dev);
	int i;

	if (adapter_dev != NULL && adapter_dev->ops != NULL &&
		adapter_dev->ops->get_pdos) {
		adapter_dev->ops->get_pdos(adapter_dev);
	}

	for (i = 0; i < ARRAY_SIZE(dev_attr_pdos); i++) {
		if (attr == &dev_attr_pdos[i])
			/* dump the PDO as a hex string */
			return snprintf(buf, PAGE_SIZE, "%08x\n",
				adapter_dev->received_pdos[i]);
	}

	class_err("%s: Invalid PDO index\n", __func__);
	return -EINVAL;
}

static DEVICE_ATTR(name, 0444, adapter_show_name, NULL);

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
	struct adapter_device *adapter_dev = NULL;
	static struct lock_class_key key;
	struct srcu_notifier_head *head = NULL;
	int rc;

	class_err("%s: name=%s\n", __func__, name);
	adapter_dev = kzalloc(sizeof(*adapter_dev), GFP_KERNEL);
	if (!adapter_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&adapter_dev->ops_lock);
	adapter_dev->dev.class = adapter_class;
	adapter_dev->dev.parent = parent;
	adapter_dev->dev.release = adapter_device_release;
	dev_set_name(&adapter_dev->dev, "%s", name);
	dev_set_drvdata(&adapter_dev->dev, devdata);
	head = &adapter_dev->evt_nh;
	srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);

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

static int adapter_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct adapter_device *get_adapter_by_name(const char *name)
{
	struct device *dev = NULL;

	if (!name)
		return (struct adapter_device *)NULL;
	dev = class_find_device(adapter_class, NULL, name,
				adapter_match_device_by_name);

	return dev ? to_adapter_device(dev) : NULL;

}

void adapter_class_exit(void)
{
	class_destroy(adapter_class);
}

int adapter_class_init(void)
{
	adapter_class = class_create(THIS_MODULE, "Charging_Adapter");
	if (IS_ERR(adapter_class)) {
		class_err("Unable to create Charging Adapter class; errno = %ld\n",
			PTR_ERR(adapter_class));
		return PTR_ERR(adapter_class);
	}
	adapter_class->dev_groups = adapter_groups;

	return 0;
}

