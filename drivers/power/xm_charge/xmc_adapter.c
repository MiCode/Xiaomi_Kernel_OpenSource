
/*******************************************************************************************
* Description:	XIAOMI-BSP-CHARGE
* 		This xmc_adapter.c contains the ops to operate USBPD PDO and authenticate
* ------------------------------ Revision History: --------------------------------
* <version>	<date>		<author>			<desc>
* 1.0		2022-02-22	chenyichun@xiaomi.com		Created for new architecture
********************************************************************************************/

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/preempt.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/types.h>

#include "xmc_core.h"

#define BSWAP_32(x) \
	(u32)((((u32)(x) & 0xff000000) >> 24) | \
			(((u32)(x) & 0x00ff0000) >> 8) | \
			(((u32)(x) & 0x0000ff00) << 8) | \
			(((u32)(x) & 0x000000ff) << 24))

/* adapter_state_strings must in the same order with pe_state_name defined in tcpc/pd_policy_engine.c */
static const char * const adapter_state_strings[] = {
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

static void adapter_sha256_bitswap32(unsigned int *array, int len)
{
	int i;

	for (i = 0; i < len; i++)
		array[i] = BSWAP_32(array[i]);
}

static void adapter_charToint(char *str, int input_len, unsigned int *out, unsigned int *outlen)
{
	int i;

	if (outlen != NULL)
		*outlen = 0;

	for (i = 0; i < (input_len / 4 + 1); i++) {
		out[i] = ((str[i * 4 + 3] * 0x1000000) |
				(str[i * 4 + 2] * 0x10000) |
				(str[i * 4 + 1] * 0x100) |
				str[i * 4]);
		*outlen = *outlen + 1;
	}
}

static void adapter_stringtohex(char *str, unsigned char *out, unsigned int *outlen)
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
}

static ssize_t adapter_pd_request_vdm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct charge_chip *chip = dev_get_drvdata(dev);
	int cmd = 0;
	unsigned char buffer[64];
	unsigned char *data;
	unsigned int count = 0;

	if (in_interrupt())
		data = kmalloc(40, GFP_ATOMIC);
	else
		data = kmalloc(40, GFP_KERNEL);

	memset(data, 0, 40);
	sscanf(buf, "%d,%s\n", &cmd, buffer);
	adapter_stringtohex(buffer, data, &count);
	xmc_info("[XMC_AUTH] store request VDM, CMD = %d, buffer = %s, count = %d\n", cmd, buffer, count);

	if (chip->adapter_dev && chip->adapter_dev->ops && chip->adapter_dev->ops->request_vdm_cmd) {
		chip->adapter_dev->ops->request_vdm_cmd(chip->adapter_dev, cmd, data, count);
	}
	kfree(data);
	return size;
}

static ssize_t adapter_pd_request_vdm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charge_chip *chip = dev_get_drvdata(dev);
	int i = 0;
	char data[16] = { 0 }, str_buf[128] = { 0 };
	enum uvdm_state cmd = chip->adapter.uvdm_state;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		return snprintf(buf, PAGE_SIZE, "%d,%x\n", cmd, chip->adapter.version);
	case USBPD_UVDM_CHARGER_TEMP:
		return snprintf(buf, PAGE_SIZE, "%d,%d\n", cmd, chip->adapter.temp);
	case USBPD_UVDM_CHARGER_VOLTAGE:
		return snprintf(buf, PAGE_SIZE, "%d,%d\n", cmd, chip->adapter.voltage);
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
			snprintf(data, sizeof(data), "%08lx", chip->adapter.digest[i]);
			strlcat(str_buf, data, sizeof(str_buf));
		}
		return snprintf(buf, PAGE_SIZE, "%d,%s\n", cmd, str_buf);
	case USBPD_UVDM_REVERSE_AUTHEN:
		return snprintf(buf, PAGE_SIZE, "%d,%d", cmd, chip->adapter.reauth);
	default:
		xmc_err("feedbak cmd:%d is not support\n", cmd);
		break;
	}
	return snprintf(buf, PAGE_SIZE, "%d,%s\n", cmd, str_buf);
}

static ssize_t adapter_pd_current_pr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charge_chip *chip = dev_get_drvdata(dev);
	const char *power_role_text = "none";

	chip->adapter.power_role = tcpm_inquire_pd_power_role(chip->tcpc_dev);

	if (chip->adapter.power_role == 0)
		power_role_text = "sink";
	else if (chip->adapter.power_role == 1)
		power_role_text = "source";

	return snprintf(buf, PAGE_SIZE, "%s\n", power_role_text);
}

static ssize_t adapter_pd_current_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charge_chip *chip = dev_get_drvdata(dev);

	chip->adapter.current_state = tcpm_inquire_pd_state_curr(chip->tcpc_dev);

	if (chip->adapter.current_state >= (sizeof(adapter_state_strings) / sizeof(adapter_state_strings[0])))
		chip->adapter.current_state = 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", adapter_state_strings[chip->adapter.current_state]);
}

static ssize_t adapter_pd_svid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charge_chip *chip = dev_get_drvdata(dev);

	xmc_ops_get_pd_id(chip->adapter_dev);

	return snprintf(buf, PAGE_SIZE, "%04x\n", chip->adapter.adapter_svid);
}

static ssize_t adapter_pd_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charge_chip *chip = dev_get_drvdata(dev);

	xmc_ops_get_pd_id(chip->adapter_dev);

	return snprintf(buf, PAGE_SIZE, "%08x\n", chip->adapter.adapter_id);
}

static ssize_t adapter_pd_authenticate_process_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct charge_chip *chip = dev_get_drvdata(dev);
	int value = 0;

	if (sscanf(buf, "%d\n", &value) != 1) {
		chip->adapter.authenticate_process = 0;
		return size;
	}

	chip->adapter.authenticate_process = !!value;
	xmc_info("[XMC_AUTH] process = %d\n", chip->adapter.authenticate_process);

	return size;
}

static ssize_t adapter_pd_authenticate_process_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charge_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->adapter.authenticate_process);
}

static ssize_t adapter_pd_authenticate_success_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct charge_chip *chip = dev_get_drvdata(dev);
	int value = 0;

	if (sscanf(buf, "%d\n", &value) != 1) {
		chip->adapter.authenticate_success = 0;
		return size;
	}

	chip->adapter.authenticate_success = !!value;
	xmc_info("[XMC_AUTH] result = %d\n", chip->adapter.authenticate_success);
	tcpm_dpm_pd_get_source_cap(chip->tcpc_dev, NULL);

	if (chip->adapter.authenticate_success)
		power_supply_changed(chip->usb_psy);

	return size;
}

static ssize_t adapter_pd_authenticate_success_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charge_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->adapter.authenticate_success);
}

static DEVICE_ATTR(pd_request_vdm, S_IRUGO | S_IWUSR, adapter_pd_request_vdm_show, adapter_pd_request_vdm_store);
static DEVICE_ATTR(pd_current_pr, S_IRUGO, adapter_pd_current_pr_show, NULL);
static DEVICE_ATTR(pd_current_state, S_IRUGO, adapter_pd_current_state_show, NULL);
static DEVICE_ATTR(pd_svid, S_IRUGO, adapter_pd_svid_show, NULL);
static DEVICE_ATTR(pd_id, S_IRUGO, adapter_pd_id_show, NULL);
static DEVICE_ATTR(pd_authenticate_process, S_IRUGO | S_IWUSR, adapter_pd_authenticate_process_show, adapter_pd_authenticate_process_store);
static DEVICE_ATTR(pd_authenticate_success, S_IRUGO | S_IWUSR, adapter_pd_authenticate_success_show, adapter_pd_authenticate_success_store);

static struct attribute *adapter_attributes[] = {
	&dev_attr_pd_request_vdm.attr,
	&dev_attr_pd_current_pr.attr,
	&dev_attr_pd_current_state.attr,
	&dev_attr_pd_svid.attr,
	&dev_attr_pd_id.attr,
	&dev_attr_pd_authenticate_process.attr,
	&dev_attr_pd_authenticate_success.attr,
	NULL,
};

static const struct attribute_group adapter_attr_group = {
	.attrs = adapter_attributes,
};

static int adapter_dpm_cb_func(struct tcpc_device *tcpc, int ret, struct tcp_dpm_event *event)
{
	int i;
	struct tcp_dpm_custom_vdm_data vdm_data = event->tcp_dpm_data.vdm_data;

	for (i = 0; i < vdm_data.cnt; i++)
		xmc_info("[XMC_AUTH] send VDM callback, VDO[%d] = 0x%08x", i, vdm_data.vdos[i]);

	return 0;
}

const struct tcp_dpm_event_cb_data adapter_dpm_cb = {
	.event_cb = adapter_dpm_cb_func,
};

static int adapter_request_vdm_cmd(struct xmc_device *dev, enum uvdm_state cmd, unsigned char *data, unsigned int data_len)
{
	struct charge_chip *chip = (struct charge_chip *)xmc_ops_get_data(dev);
	struct tcp_dpm_custom_vdm_data *vdm_data;
	uint32_t vdm_hdr = 0;
	unsigned int *int_data;
	unsigned int outlen;
	int rc = 0;
	int i;

	if (in_interrupt()) {
		int_data = kmalloc(40, GFP_ATOMIC);
		vdm_data = kmalloc(sizeof(*vdm_data), GFP_ATOMIC);
	} else {
		int_data = kmalloc(40, GFP_KERNEL);
		vdm_data = kmalloc(sizeof(*vdm_data), GFP_KERNEL);
	}

	memset(int_data, 0, 40);
	adapter_charToint(data, data_len, int_data, &outlen);
	vdm_hdr = (((chip->adapter.adapter_svid) << 16) | (0 << 15) | ((USBPD_UVDM_REQUEST) << 8) | (cmd));
	vdm_data->wait_resp = true;
	vdm_data->vdos[0] = vdm_hdr;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
	case USBPD_UVDM_CHARGER_TEMP:
	case USBPD_UVDM_CHARGER_VOLTAGE:
		vdm_data->cnt = 1;
		rc = tcpm_dpm_send_custom_vdm(chip->tcpc_dev, vdm_data, &adapter_dpm_cb);
		if (rc < 0) {
			xmc_err("failed to send %d\n", cmd);
			goto done;
		}
		break;
	case USBPD_UVDM_VERIFIED:
	case USBPD_UVDM_REMOVE_COMPENSATION:
		vdm_data->cnt = 1 + USBPD_UVDM_VERIFIED_LEN;

		for (i = 0; i < USBPD_UVDM_VERIFIED_LEN; i++)
			vdm_data->vdos[i + 1] = int_data[i];

		rc = tcpm_dpm_send_custom_vdm(chip->tcpc_dev, vdm_data, &adapter_dpm_cb);
		if (rc < 0) {
			xmc_err("failed to send %d\n", cmd);
			goto done;
		}
		break;
	case USBPD_UVDM_SESSION_SEED:
	case USBPD_UVDM_AUTHENTICATION:
	case USBPD_UVDM_REVERSE_AUTHEN:
		adapter_sha256_bitswap32(int_data, USBPD_UVDM_SS_LEN);
		vdm_data->cnt = 1 + USBPD_UVDM_SS_LEN;
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++)
			vdm_data->vdos[i + 1] = int_data[i];

		rc = tcpm_dpm_send_custom_vdm(chip->tcpc_dev, vdm_data, &adapter_dpm_cb);
		if (rc < 0) {
			xmc_err("failed to send %d\n", cmd);
			goto done;
		}
		break;
	default:
		xmc_err("cmd:%d is not support\n", cmd);
		break;
	}
 done:
	if (int_data != NULL)
		kfree(int_data);
	if (vdm_data != NULL)
		kfree(vdm_data);
	return rc;
}

static int adapter_set_cap(struct xmc_device *dev, enum xmc_pdo_type type, int mV, int mA)
{
	struct charge_chip *chip = (struct charge_chip *)xmc_ops_get_data(dev);
	int ret = TCPM_SUCCESS;

	if (type == XMC_PDO_APDO_START)
		ret = tcpm_set_apdo_charging_policy(chip->tcpc_dev, DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
	else if (type == XMC_PDO_APDO_END)
		ret = tcpm_set_pd_charging_policy(chip->tcpc_dev, DPM_CHARGING_POLICY_VSAFE5V, NULL);
	else if (type == XMC_PDO_APDO)
		ret = tcpm_dpm_pd_request(chip->tcpc_dev, mV, mA, NULL);
	else if (type == XMC_PDO_PD)
		ret = tcpm_dpm_pd_request(chip->tcpc_dev, mV, mA, NULL);

	xmc_info("[XMC_PDM] request PDO, type = %d, voltage = %d, current = %d, ret = %d\n", type, mV, mA, ret);

	return ret;
}

static int adapter_get_cap(struct xmc_device *dev, enum xmc_pdo_type type, struct xmc_pd_cap *tacap)
{
	struct charge_chip *chip = (struct charge_chip *)xmc_ops_get_data(dev);
	struct tcpm_power_cap_val apdo_cap;
	struct tcpm_remote_power_cap pd_cap;
	struct pd_source_cap_ext cap_ext;
	uint8_t cap_i = 0;
	int ret = 0, idx = 0;
	unsigned int i = 0;

	if (type == XMC_PDO_APDO) {
		while (1) {
			ret = tcpm_inquire_pd_source_apdo(chip->tcpc_dev,
					TCPM_POWER_CAP_APDO_TYPE_PPS,
					&cap_i, &apdo_cap);
			if (ret == TCPM_ERROR_NOT_FOUND) {
				break;
			} else if (ret != TCPM_SUCCESS) {
				xmc_info("[%s] tcpm_inquire_pd_source_apdo failed(%d)\n",
					__func__, ret);
				break;
			}

			ret = tcpm_dpm_pd_get_source_cap_ext(chip->tcpc_dev,
					NULL, &cap_ext);
			if (ret == TCPM_SUCCESS)
				tacap->pdp = cap_ext.source_pdp;
			else {
				tacap->pdp = 0;
				xmc_info("[%s] tcpm_dpm_pd_get_source_cap_ext failed(%d)\n",
					__func__, ret);
			}

			tacap->pwr_limit[idx] = apdo_cap.pwr_limit;
			/* If TA has PDP, we set pwr_limit as true */
			if (tacap->pdp > 0 && !tacap->pwr_limit[idx])
				tacap->pwr_limit[idx] = 1;
			tacap->ma[idx] = apdo_cap.ma;
			tacap->max_mv[idx] = apdo_cap.max_mv;
			tacap->min_mv[idx] = apdo_cap.min_mv;
			tacap->maxwatt[idx] = apdo_cap.max_mv * apdo_cap.ma;
			tacap->minwatt[idx] = apdo_cap.min_mv * apdo_cap.ma;
			tacap->type[idx] = XMC_PDO_APDO;

			idx++;
			xmc_info("pps_boundary[%d], %d mv ~ %d mv, %d ma pl:%d\n",
				cap_i,
				apdo_cap.min_mv, apdo_cap.max_mv,
				apdo_cap.ma, apdo_cap.pwr_limit);
			if (idx >= ADAPTER_CAP_MAX_NR) {
				xmc_info("CAP NR > %d\n", ADAPTER_CAP_MAX_NR);
				break;
			}
		}
		tacap->nr = idx;

		for (i = 0; i < tacap->nr; i++) {
			xmc_info("pps_cap[%d:%d], %d mv ~ %d mv, %d ma pl:%d pdp:%d\n",
				i, (int)tacap->nr, tacap->min_mv[i],
				tacap->max_mv[i], tacap->ma[i],
				tacap->pwr_limit[i], tacap->pdp);
		}

		if (cap_i == 0)
			xmc_info("no APDO for pps\n");

	} else if (type == XMC_PDO_PD) {
		pd_cap.nr = 0;
		ret = tcpm_get_remote_power_cap(chip->tcpc_dev, &pd_cap);
		if (pd_cap.nr != 0) {
			tacap->nr = pd_cap.nr;
			for (i = 0; i < pd_cap.nr; i++) {
				tacap->ma[i] = pd_cap.ma[i];
				tacap->max_mv[i] = pd_cap.max_mv[i];
				tacap->min_mv[i] = pd_cap.min_mv[i];
				tacap->maxwatt[i] = tacap->max_mv[i] * tacap->ma[i];
				tacap->type[i] = pd_cap.type[i];
			}
		}
	}

	return ret;
}

static int adapter_get_pd_id(struct xmc_device *dev)
{
	struct charge_chip *chip = (struct charge_chip *)xmc_ops_get_data(dev);
	struct pd_source_cap_ext cap_ext;
	uint32_t pd_vdos[VDO_MAX_NR];
	int ret = 0;

	if (!chip->adapter.adapter_svid || !chip->adapter.adapter_id) {
		ret = tcpm_inquire_pd_partner_inform(chip->tcpc_dev, pd_vdos);
		if (ret == TCPM_SUCCESS) {
			xmc_info("[XMC_AUTH] PD_VDO = 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
				pd_vdos[0], pd_vdos[1], pd_vdos[2], pd_vdos[3], pd_vdos[4], pd_vdos[5]);
			chip->adapter.adapter_svid = pd_vdos[0] & 0x0000FFFF;
			chip->adapter.adapter_id = pd_vdos[2] & 0x0000FFFF;
		} else {
			xmc_info("[XMC_AUTH] failed to copy PD_VDO, try to send DPM event, ret = %d\n", ret);
			ret = tcpm_dpm_pd_get_source_cap_ext(chip->tcpc_dev, NULL, &cap_ext);
			if (ret == TCPM_SUCCESS) {
				chip->adapter.adapter_svid = cap_ext.vid & 0x0000FFFF;
				chip->adapter.adapter_id = cap_ext.pid & 0x0000FFFF;
				chip->adapter.adapter_fw_ver = cap_ext.fw_ver & 0x0000FFFF;
				chip->adapter.adapter_hw_ver = cap_ext.hw_ver & 0x0000FFFF;
				xmc_info("[XMC_AUTH] get SRC_CAP_EXT, [VID PID FW FW] = [%08x %08x %08x %08x]\n",
					chip->adapter.adapter_svid, chip->adapter.adapter_id, chip->adapter.adapter_fw_ver, chip->adapter.adapter_hw_ver);
			} else {
				xmc_err("[XMC_AUTH] failed to get PD ID, ret = %d\n", ret);
			}
		}
	}

	return ret;
}

static const struct xmc_ops adapter_ops = {
	.request_vdm_cmd = adapter_request_vdm_cmd,
	.set_cap = adapter_set_cap,
	.get_cap = adapter_get_cap,
	.get_pd_id = adapter_get_pd_id,
};

bool xmc_adapter_init(struct charge_chip *chip)
{
	int ret = 0;

	chip->adapter_dev = xmc_device_register("xmc_adapter", &adapter_ops, chip);
	if (!chip->adapter_dev) {
		xmc_err("failed to register adapter_dev\n");
		return false;
	}

	ret = sysfs_create_group(&chip->dev->kobj, &adapter_attr_group);
	if (ret) {
		xmc_err("failed to register adapter_groups\n");
		return false;
	}

	return true;
}
