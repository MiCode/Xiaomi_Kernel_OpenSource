/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include <linux/power_supply.h>
#include "mtk_charger_intf.h"

#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>	/*irq_to_desc*/
#include <linux/vmalloc.h>
#include <linux/preempt.h>

/* PD */
#include <tcpm.h>

int nr_num;
struct adapter_power_cap ta_cap = {0};

struct power_supply *usb_psy = NULL;
union power_supply_propval val;
int get_apdo_regain;

enum pd_type_def {
	PD_SRC_PDO_TYPE_FIXED,
	PD_SRC_PDO_TYPE_PPS,
	PD_SRC_PDO_TYPE_UNKNOW,
};

struct mtk_pd_adapter_info {
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct adapter_device *adapter_dev;
	struct task_struct *adapter_task;
	const char *adapter_dev_name;
	bool enable_kpoc_shdn;
	bool non_mtk_pps_ctr;
	struct tcpm_svid_list *adapter_svid_list;
};
#if 0
static void usbpd_mi_vdm_received_cb(struct mtk_pd_adapter_info *info,
	struct tcp_ny_uvdm uvdm);
#endif

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_pd_adapter_info *pinfo;

	pinfo = container_of(pnb, struct mtk_pd_adapter_info, pd_nb);

	chr_err("PD charger event:%d %d\n", (int)event,
		(int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case  PD_CONNECT_NONE:
			pinfo->adapter_dev->adapter_id = 0;
			pinfo->adapter_dev->adapter_svid = 0;
			pinfo->adapter_dev->uvdm_state = USBPD_UVDM_DISCONNECT;
			pinfo->adapter_dev->verifed = 0;
			pinfo->adapter_dev->verify_process = 0;
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_NONE, NULL);
			break;

		case PD_CONNECT_HARD_RESET:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_HARD_RESET, NULL);
			break;

		case PD_CONNECT_PE_READY_SNK:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_PE_READY_SNK, NULL);
			break;

		case PD_CONNECT_PE_READY_SNK_PD30:
			pinfo->adapter_dev->uvdm_state = USBPD_UVDM_CONNECT;
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_PE_READY_SNK_PD30, NULL);
			break;

		case PD_CONNECT_PE_READY_SNK_APDO:
			pinfo->adapter_dev->uvdm_state = USBPD_UVDM_CONNECT;
			get_apdo_regain = 1;
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_PE_READY_SNK_APDO, NULL);
			break;

		case PD_CONNECT_TYPEC_ONLY_SNK:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_TYPEC_ONLY_SNK, NULL);
			break;
		};
		break;
	case TCP_NOTIFY_WD_STATUS:
		notify_adapter_event(MTK_PD_ADAPTER,
			MTK_TYPEC_WD_STATUS, &noti->wd_status.water_detected);
		break;
	case TCP_NOTIFY_HARD_RESET_STATE:
		if (noti->hreset_state.state == TCP_HRESET_RESULT_DONE ||
			noti->hreset_state.state == TCP_HRESET_RESULT_FAIL) {
			pinfo->enable_kpoc_shdn = true;
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_TYPEC_HRESET_STATUS,
				&pinfo->enable_kpoc_shdn);
		} else if (noti->hreset_state.state == TCP_HRESET_SIGNAL_SEND ||
			noti->hreset_state.state == TCP_HRESET_SIGNAL_RECV) {
			pinfo->enable_kpoc_shdn = false;
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_TYPEC_HRESET_STATUS,
				&pinfo->enable_kpoc_shdn);
		}
		break;
	case TCP_NOTIFY_UVDM:
		notify_adapter_event(MTK_PD_ADAPTER,
			MTK_PD_UVDM, &noti->uvdm_msg);
		break;
	}
	return NOTIFY_OK;
}

#if 0
static void usbpd_mi_vdm_received_cb(struct mtk_pd_adapter_info *info,
	struct tcp_ny_uvdm uvdm)
{
	int i, cmd;
	int ret = -1;
	int usb_current, usb_voltage, r_cable;
	union power_supply_propval val = {0};

	usb_psy = power_supply_get_by_name("usb");

	cmd = UVDM_HDR_CMD(uvdm.uvdm_data[0]);
	pr_info("cmd = %d\n", cmd);

	pr_info("uvdm.ack: %d, uvdm.uvdm_cnt: %d, uvdm.uvdm_svid: 0x%04x\n",
			uvdm.ack, uvdm.uvdm_cnt, uvdm.uvdm_svid);

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		info->adapter_dev->vdm_data.ta_version = uvdm.uvdm_data[1];
		pr_info("ta_version:%x\n", info->adapter_dev->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		info->adapter_dev->vdm_data.ta_temp = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		pr_info("info->adapter_dev->vdm_data.ta_temp:%d\n", info->adapter_dev->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		info->adapter_dev->vdm_data.ta_voltage = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		info->adapter_dev->vdm_data.ta_voltage *= 1000; /*V->mV*/
		pr_info("ta_voltage:%d\n", info->adapter_dev->vdm_data.ta_voltage);

		if (usb_psy) {
			ret = power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
			if (ret) {
				chr_err("failed to get usb voltage now\n");
				break;
			}
			usb_voltage = val.intval;
			pr_info("usb voltage now:%d\n", usb_voltage);
			ret = power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val);
			if (ret) {
				chr_err("failed to get usb current now\n");
				break;
			}
			usb_current = val.intval / 1000;
			pr_info("usb current now:%d\n", usb_current);

			r_cable = (info->adapter_dev->vdm_data.ta_voltage - usb_voltage) / usb_current;
			pr_info("usb r_cable now:%dmohm\n", r_cable);
		}
		break;
	case USBPD_UVDM_SESSION_SEED:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			info->adapter_dev->vdm_data.s_secert[i] = uvdm.uvdm_data[i+1];
			pr_info("usbpd s_secert uvdm.uvdm_data[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			info->adapter_dev->vdm_data.digest[i] = uvdm.uvdm_data[i+1];
			pr_info("usbpd digest[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	default:
		break;
	}
	info->adapter_dev->uvdm_state = cmd;
}
#endif

static int pd_get_property(struct adapter_device *dev,
	enum adapter_property sta)
{
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return -1;

	switch (sta) {
	case TYPEC_RP_LEVEL:
		{
			return tcpm_inquire_typec_remote_rp_curr(info->tcpc);
		}
		break;
	default:
		{
		}
		break;
	}
	return -1;
}

int find_pd_type(int mV, int mA)
{
	uint8_t i = 0;

	chr_err("[%s] select pdo is mV[%d] mA[%d]\n", __func__, mV, mA);
	chr_err("[%s] ta_cap.nr is %d\n", __func__, ta_cap.nr);
	chr_err("[%s] nr_num is %d\n", __func__, nr_num);
	for (i = 0; i < ta_cap.nr; i++) {
		chr_err("[%s] TA value is [%d %d %d]\n", __func__, ta_cap.min_mv[i], ta_cap.max_mv[i], ta_cap.ma[i]);
		if ((mV == ta_cap.min_mv[i]) && (mV == ta_cap.max_mv[i]) && (mA == ta_cap.ma[i])) {
			chr_err("[%s] select value is fixed.\n", __func__);
			return PD_SRC_PDO_TYPE_FIXED;
		} else if ((mV >= ta_cap.min_mv[i]) && (mV <= ta_cap.max_mv[i]) && (mA <= ta_cap.ma[i])) {
			chr_err("[%s] select value is pps.\n", __func__);
			return PD_SRC_PDO_TYPE_PPS;
		}
	}

	chr_err("[%s] select value is not right.\n", __func__);
	return PD_SRC_PDO_TYPE_UNKNOW;
}


static int pd_set_cap(struct adapter_device *dev, enum adapter_cap_type type,
		int mV, int mA)
{
	int ret = MTK_ADAPTER_OK;
	int tcpm_ret = TCPM_SUCCESS;
	struct mtk_pd_adapter_info *info = NULL;

	chr_err("[%s] type:%d mV:%d mA:%d\n",
		__func__, type, mV, mA);


	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL) {
		chr_err("[%s] info null\n", __func__);
		return -1;
	}

	if (info->adapter_dev->verify_process)
		return -1;

	/* use xiaomi pps control state machine */
	if (info->non_mtk_pps_ctr) {
		if (find_pd_type(mV, mA) == PD_SRC_PDO_TYPE_PPS) {
			chr_err("[%s] PPS is controlled by ourself, return not support\n", __func__);
			return -1;
		}
	}

	if (type == MTK_PD_APDO_START) {
		tcpm_ret = tcpm_set_apdo_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
	} else if (type == MTK_PD_APDO_END) {
		tcpm_ret = tcpm_set_pd_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);
	} else if (type == MTK_PD_APDO) {
		tcpm_ret = tcpm_dpm_pd_request(info->tcpc, mV, mA, NULL);
	} else if (type == MTK_PD) {
		tcpm_ret = tcpm_dpm_pd_request(info->tcpc, mV,
					mA, NULL);
	}

	chr_err("[%s] type:%d mV:%d mA:%d ret:%d\n",
		__func__, type, mV, mA, tcpm_ret);


	if (tcpm_ret == TCP_DPM_RET_REJECT)
		return MTK_ADAPTER_REJECT;
	else if (tcpm_ret != 0)
		return MTK_ADAPTER_ERROR;

	return ret;
}

static int pd_set_cap_bq(struct adapter_device *dev, enum adapter_cap_type type,
		int mV, int mA)
{
	int ret = MTK_ADAPTER_OK;
	int tcpm_ret = TCPM_SUCCESS;
	struct mtk_pd_adapter_info *info;

	chr_err("[%s] type:%d mV:%d mA:%d\n",
		__func__, type, mV, mA);

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL) {
		chr_err("[%s] info null\n", __func__);
		return -1;
	}

	if (info->adapter_dev->verify_process)
		return MTK_ADAPTER_VERIFYING;

	if (type == MTK_PD_APDO_START) {
		tcpm_ret = tcpm_set_apdo_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
	} else if (type == MTK_PD_APDO_END) {
		tcpm_ret = tcpm_set_pd_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);
	} else if (type == MTK_PD_APDO) {
		tcpm_ret = tcpm_dpm_pd_request(info->tcpc, mV, mA, NULL);
	} else if (type == MTK_PD) {
		tcpm_ret = tcpm_dpm_pd_request(info->tcpc, mV,
					mA, NULL);
	}

	chr_err("[%s] type:%d mV:%d mA:%d ret:%d\n",
		__func__, type, mV, mA, tcpm_ret);


	if (tcpm_ret == TCP_DPM_RET_REJECT)
		return MTK_ADAPTER_REJECT;
	else if (tcpm_ret != 0)
		return MTK_ADAPTER_ERROR;

	return ret;
}

static int pd_set_pd_verify_process(struct adapter_device *dev, int verify_in_process)
{
	int ret = 0;
	union power_supply_propval val = {0,};

	if (usb_psy) {
		val.intval = verify_in_process;
		ret = power_supply_set_property(usb_psy, POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS, &val);
	} else {
		chr_err("[%s] usb psy not found!\n", __func__);
	}

	return ret;
}

int pd_get_output(struct adapter_device *dev, int *mV, int *mA)
{
	int ret = MTK_ADAPTER_OK;
	int tcpm_ret = TCPM_SUCCESS;
	struct pd_pps_status pps_status;
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_NOT_SUPPORT;


	tcpm_ret = tcpm_dpm_pd_get_pps_status(info->tcpc, NULL, &pps_status);
	if (tcpm_ret == TCP_DPM_RET_NOT_SUPPORT)
		return MTK_ADAPTER_NOT_SUPPORT;
	else if (tcpm_ret != 0)
		return MTK_ADAPTER_ERROR;

	*mV = pps_status.output_mv;
	*mA = pps_status.output_ma;

	return ret;
}

int pd_get_status(struct adapter_device *dev,
	struct adapter_status *sta)
{
	struct pd_status TAstatus = {0,};
	int ret = MTK_ADAPTER_OK;
	int tcpm_ret = TCPM_SUCCESS;
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

	tcpm_ret = tcpm_dpm_pd_get_status(info->tcpc, NULL, &TAstatus);

	sta->temperature = TAstatus.internal_temp;
	sta->ocp = TAstatus.event_flags & PD_STASUS_EVENT_OCP;
	sta->otp = TAstatus.event_flags & PD_STATUS_EVENT_OTP;
	sta->ovp = TAstatus.event_flags & PD_STATUS_EVENT_OVP;

	if (tcpm_ret == TCP_DPM_RET_NOT_SUPPORT)
		return MTK_ADAPTER_NOT_SUPPORT;
	else if (tcpm_ret == TCP_DPM_RET_TIMEOUT)
		return MTK_ADAPTER_TIMEOUT;
	else if (tcpm_ret == TCP_DPM_RET_SUCCESS)
		return MTK_ADAPTER_OK;
	else
		return MTK_ADAPTER_ERROR;

	return ret;

}

static int pd_get_cap(struct adapter_device *dev,
	enum adapter_cap_type type,
	struct adapter_power_cap *tacap)
{
	struct tcpm_power_cap_val apdo_cap;
	struct tcpm_remote_power_cap pd_cap;
	struct pd_source_cap_ext cap_ext;

	uint8_t cap_i = 0;
	int ret;
	int idx = 0;
	int i;
	struct mtk_pd_adapter_info *info;
	int timeout = 0;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

	if (info->adapter_dev->verify_process)
		return MTK_ADAPTER_VERIFYING;

	if (type == MTK_PD_APDO) {
		while (1) {
			ret = tcpm_inquire_pd_source_apdo(info->tcpc,
					TCPM_POWER_CAP_APDO_TYPE_PPS,
					&cap_i, &apdo_cap);
			if (ret == TCPM_ERROR_NOT_FOUND) {
				break;
			} else if (ret != TCPM_SUCCESS) {
				chr_err("[%s] tcpm_inquire_pd_source_apdo failed(%d)\n",
					__func__, ret);
				break;
			}

			ret = tcpm_dpm_pd_get_source_cap_ext(info->tcpc,
					NULL, &cap_ext);
			if (ret == TCPM_SUCCESS)
				tacap->pdp = cap_ext.source_pdp;
			else {
				tacap->pdp = 0;
				chr_err("[%s] tcpm_dpm_pd_get_source_cap_ext failed(%d)\n",
					__func__, ret);
			}

			tacap->pwr_limit[idx] = apdo_cap.pwr_limit;
			tacap->ma[idx] = apdo_cap.ma;
			tacap->max_mv[idx] = apdo_cap.max_mv;
			tacap->min_mv[idx] = apdo_cap.min_mv;
			tacap->maxwatt[idx] = apdo_cap.max_mv * apdo_cap.ma;
			tacap->minwatt[idx] = apdo_cap.min_mv * apdo_cap.ma;
			tacap->type[idx] = MTK_PD_APDO;

			idx++;
			chr_err("pps_boundary[%d], %d mv ~ %d mv, %d ma pl:%d\n",
				cap_i,
				apdo_cap.min_mv, apdo_cap.max_mv,
				apdo_cap.ma, apdo_cap.pwr_limit);
			if (idx >= ADAPTER_CAP_MAX_NR) {
				chr_err("CAP NR > %d\n", ADAPTER_CAP_MAX_NR);
				break;
			}
		}
		tacap->nr = idx;

		for (i = 0; i < tacap->nr; i++) {
			chr_err("pps_cap[%d:%d], %d mv ~ %d mv, %d ma pl:%d pdp:%d\n",
				i, (int)tacap->nr, tacap->min_mv[i],
				tacap->max_mv[i], tacap->ma[i],
				tacap->pwr_limit[i], tacap->pdp);
		}

		if (cap_i == 0)
			chr_err("no APDO for pps\n");

	} else if (type == MTK_PD) {
APDO_REGAIN:
		pd_cap.nr = 0;
		pd_cap.selected_cap_idx = 0;
		tcpm_get_remote_power_cap(info->tcpc, &pd_cap);

		tacap->nr = pd_cap.nr;
		tacap->selected_cap_idx = pd_cap.selected_cap_idx - 1;
		chr_err("[%s] nr:%d idx:%d\n",
		__func__, pd_cap.nr, pd_cap.selected_cap_idx - 1);
		for (i = 0; i < pd_cap.nr; i++) {
			tacap->ma[i] = pd_cap.ma[i];
			tacap->max_mv[i] = pd_cap.max_mv[i];
			tacap->min_mv[i] = pd_cap.min_mv[i];
			tacap->maxwatt[i] =
				tacap->max_mv[i] * tacap->ma[i];
			if (pd_cap.type[i] == 0)
				tacap->type[i] = MTK_PD;
			else if (pd_cap.type[i] == 3)
				tacap->type[i] = MTK_PD_APDO;
			else
				tacap->type[i] = MTK_CAP_TYPE_UNKNOWN;
			tacap->type[i] = pd_cap.type[i];

			chr_err("[%s]:%d mv:[%d,%d] %d max:%d min:%d type:%d %d\n",
				__func__, i, tacap->min_mv[i],
				tacap->max_mv[i], tacap->ma[i],
				tacap->maxwatt[i], tacap->minwatt[i],
				tacap->type[i], pd_cap.type[i]);
		}
	}  else if (type == MTK_PD_APDO_REGAIN) {
		get_apdo_regain = 0;
		ret = tcpm_dpm_pd_get_source_cap(info->tcpc, NULL);
		if (ret == TCPM_SUCCESS) {
			while (timeout < 10) {
				if (get_apdo_regain) {
					chr_err("[%s] ready to get pps info!\n", __func__);
					goto APDO_REGAIN;
				} else {
					mdelay(100);
					timeout++;
				}
			}
		} else {
			chr_err("[%s] tcpm_dpm_pd_get_source_cap failed!\n", __func__);
			return MTK_ADAPTER_ERROR;
		}
	}

	ta_cap.nr = tacap->nr;
	for (i = 0; i < tacap->nr; i++) {
		ta_cap.max_mv[i] = tacap->max_mv[i];
		ta_cap.min_mv[i] = tacap->min_mv[i];
		ta_cap.ma[i] = tacap->ma[i];
		if (tacap->type[i] == 3)
			val.intval = POWER_SUPPLY_PD_PPS_ACTIVE;

		if ((tacap->type[i] == 0) && (val.intval != POWER_SUPPLY_PD_PPS_ACTIVE))
			val.intval = POWER_SUPPLY_PD_ACTIVE;
	}

	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		power_supply_set_property(usb_psy,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
	} else {
		chr_err("[%s] usb psy not found!\n", __func__);
	}
	nr_num = ta_cap.nr;
	chr_err("[%s] ta_cap.nr is %d\n", __func__, ta_cap.nr);
	chr_err("[%s] nr_num is %d\n", __func__, nr_num);
	return MTK_ADAPTER_OK;
}

static int pd_get_svid(struct adapter_device *dev)
{
	struct mtk_pd_adapter_info *info;
	struct pd_source_cap_ext cap_ext;
	int ret;
	int i = 0;
	uint32_t pd_vdos[8];

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL)
		return MTK_ADAPTER_ERROR;

	pr_info("%s: enter\n", __func__);
	if (info->adapter_dev->adapter_svid != 0)
		return MTK_ADAPTER_OK;

	if (info->adapter_svid_list == NULL) {
		if (in_interrupt()) {
			info->adapter_svid_list = kmalloc(sizeof(struct tcpm_svid_list), GFP_ATOMIC);
		} else {
			info->adapter_svid_list = kmalloc(sizeof(struct tcpm_svid_list), GFP_KERNEL);
		}
		if (info->adapter_svid_list == NULL)
			chr_err("[%s] adapter_svid_list is still NULL!\n", __func__);
	}

	ret = tcpm_inquire_pd_partner_inform(info->tcpc, pd_vdos);
	if (ret == TCPM_SUCCESS) {
		pr_info("find adapter id success.\n");
		for (i = 0; i < 8; i++)
			pr_info("VDO[%d] : %08x\n", i, pd_vdos[i]);

		info->adapter_dev->adapter_svid = pd_vdos[0] & 0x0000FFFF;
		info->adapter_dev->adapter_id = pd_vdos[2] & 0x0000FFFF;
		pr_info("adapter_svid = %04x\n", info->adapter_dev->adapter_svid);
		pr_info("adapter_id = %08x\n", info->adapter_dev->adapter_id);

		ret = tcpm_inquire_pd_partner_svids(info->tcpc, info->adapter_svid_list);
		pr_info("[%s] tcpm_inquire_pd_partner_svids, ret=%d!\n", __func__, ret);
		if (ret == TCPM_SUCCESS) {
			pr_info("discover svid number is %d\n", info->adapter_svid_list->cnt);
			for (i = 0; i < info->adapter_svid_list->cnt; i++) {
				pr_info("SVID[%d] : %04x\n", i, info->adapter_svid_list->svids[i]);
				if (info->adapter_svid_list->svids[i] == USB_PD_MI_SVID)
					info->adapter_dev->adapter_svid = USB_PD_MI_SVID;
			}
		}
	} else {
		ret = tcpm_dpm_pd_get_source_cap_ext(info->tcpc,
			NULL, &cap_ext);
		if (ret == TCPM_SUCCESS) {
			info->adapter_dev->adapter_svid = cap_ext.vid & 0x0000FFFF;
			info->adapter_dev->adapter_id = cap_ext.pid & 0x0000FFFF;
			info->adapter_dev->adapter_fw_ver = cap_ext.fw_ver & 0x0000FFFF;
			info->adapter_dev->adapter_hw_ver = cap_ext.hw_ver & 0x0000FFFF;
			pr_info("adapter_svid = %04x\n", info->adapter_dev->adapter_svid);
			pr_info("adapter_id = %08x\n", info->adapter_dev->adapter_id);
			pr_info("adapter_fw_ver = %08x\n", info->adapter_dev->adapter_fw_ver);
			pr_info("adapter_hw_ver = %08x\n", info->adapter_dev->adapter_hw_ver);
		} else {
			chr_err("[%s] get adapter message failed!\n", __func__);
			return MTK_ADAPTER_ERROR;
		}
	}

	return MTK_ADAPTER_OK;
}

#if 0
static int pd_get_svid(struct adapter_device *dev)
{
	struct mtk_pd_adapter_info *info;
	struct pd_source_cap_ext cap_ext;
	int ret;
	int i = 0;
	uint32_t pd_vdos[8];

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL)
		return MTK_ADAPTER_ERROR;

	pr_info("%s: enter\n", __func__);
	if (info->adapter_dev->adapter_svid != 0)
		return MTK_ADAPTER_OK;

	ret = tcpm_inquire_pd_partner_inform(info->tcpc, pd_vdos);
	if (ret == TCPM_SUCCESS) {
		pr_info("find adapter id success.\n");
		for (i = 0; i < 8; i++)
			pr_info("VDO[%d] : %08x\n", i, pd_vdos[i]);

		info->adapter_dev->adapter_svid = pd_vdos[0] & 0x0000FFFF;
		info->adapter_dev->adapter_id = pd_vdos[2];
		pr_info("adapter_svid = %04x\n", info->adapter_dev->adapter_svid);
		pr_info("adapter_id = %08x\n", info->adapter_dev->adapter_id);
	}

	ret = tcpm_dpm_pd_get_source_cap_ext(info->tcpc,
		NULL, &cap_ext);
	if (ret == TCPM_SUCCESS) {
		info->adapter_dev->adapter_svid = cap_ext.vid & 0x0000FFFF;
		info->adapter_dev->adapter_id = cap_ext.pid & 0x0000FFFF;
		info->adapter_dev->adapter_fw_ver = cap_ext.fw_ver & 0x0000FFFF;
		info->adapter_dev->adapter_hw_ver = cap_ext.hw_ver & 0x0000FFFF;
		pr_info("adapter_svid = %04x\n", info->adapter_dev->adapter_svid);
		pr_info("adapter_id = %08x\n", info->adapter_dev->adapter_id);
		pr_info("adapter_fw_ver = %08x\n", info->adapter_dev->adapter_fw_ver);
		pr_info("adapter_hw_ver = %08x\n", info->adapter_dev->adapter_hw_ver);
	} else {
		chr_err("[%s] get adapter message failed!\n", __func__);
		//return MTK_ADAPTER_ERROR;
	}

	if (info->adapter_svid_list == NULL) {
		chr_err("[%s] adapter_svid_list is NULL!\n", __func__);

		if (in_interrupt()) {
			info->adapter_svid_list = kmalloc(sizeof(struct tcpm_svid_list), GFP_ATOMIC);
		} else {
			info->adapter_svid_list = kmalloc(sizeof(struct tcpm_svid_list), GFP_KERNEL);
		}
		if (info->adapter_svid_list == NULL)
			chr_err("[%s] adapter_svid_list is still NULL!\n", __func__);
	}

	ret = tcpm_inquire_pd_partner_svids(info->tcpc, info->adapter_svid_list);
	if (ret == TCPM_SUCCESS) {
		pr_info("discover svid number is %d\n", info->adapter_svid_list->cnt);
		for (i = 0; i < info->adapter_svid_list->cnt; i++) {
			pr_info("SVID[%d] : %04x\n", i, info->adapter_svid_list->svids[i]);
			if (info->adapter_svid_list->svids[i] == USB_PD_MI_SVID)
				info->adapter_dev->adapter_svid = USB_PD_MI_SVID;
		}
	}
	chr_err("[%s] tcpm_inquire_pd_partner_svids ret = %d\n", __func__, ret);

	return MTK_ADAPTER_OK;
}
#endif

#if 0
static int pd_get_id(struct adapter_device *dev)
{
	struct mtk_pd_adapter_info *info;
	int ret;
	int i = 0;
	uint32_t pd_vdos[8];

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL)
		return MTK_ADAPTER_ERROR;

	ret = tcpm_inquire_pd_partner_inform(info->tcpc, pd_vdos);
	if (ret == TCPM_SUCCESS) {
		pr_info("find adapter id success.\n");
		for (i = 0; i < 8; i++)
			pr_info("VDO[%d] : %08x\n", i, pd_vdos[i]);

		info->adapter_dev->adapter_svid = pd_vdos[0] & 0x0000FFFF;
		info->adapter_dev->adapter_id = pd_vdos[2];
		pr_info("adapter_svid = %04x\n", info->adapter_dev->adapter_svid);
		pr_info("adapter_id = %08x\n", info->adapter_dev->adapter_id);
	}

	return MTK_ADAPTER_OK;
}
#endif

#define BSWAP_32(x) \
	(u32)((((u32)(x) & 0xff000000) >> 24) | \
			(((u32)(x) & 0x00ff0000) >> 8) | \
			(((u32)(x) & 0x0000ff00) << 8) | \
			(((u32)(x) & 0x000000ff) << 24))

static void usbpd_sha256_bitswap32(unsigned int *array, int len)
{
	int i;

	for (i = 0; i < len; i++)
		array[i] = BSWAP_32(array[i]);
}

void charToint(char *str, int input_len, unsigned int *out, unsigned int *outlen)
{
	int i;

	if (outlen != NULL)
		*outlen = 0;
	for (i = 0; i < (input_len / 4 + 1); i++) {
		out[i] = ((str[i*4 + 3] * 0x1000000) |
				(str[i*4 + 2] * 0x10000) |
				(str[i*4 + 1] * 0x100) |
				str[i*4]);
		*outlen = *outlen + 1;
	}

	pr_info("%s: outlen = %d\n", __func__, *outlen);
	for (i = 0; i < *outlen; i++)
		pr_info("%s: out[%d] = %08x\n", __func__, i, out[i]);
	pr_info("%s: char to int done.\n", __func__);
}

static int tcp_dpm_event_cb_uvdm(struct tcpc_device *tcpc, int ret,
				 struct tcp_dpm_event *event)
{
	int i;
	struct tcp_dpm_custom_vdm_data vdm_data = event->tcp_dpm_data.vdm_data;

	pr_info("%s: vdm_data.cnt = %d\n", __func__, vdm_data.cnt);
	for (i = 0; i < vdm_data.cnt; i++)
		chr_info("%s vdm_data.vdos[%d] = 0x%08x", __func__, i,
			vdm_data.vdos[i]);
	return 0;
}

const struct tcp_dpm_event_cb_data cb_data = {
	.event_cb = tcp_dpm_event_cb_uvdm,
};

static int pd_request_vdm_cmd(struct adapter_device *dev,
	enum uvdm_state cmd,
	unsigned char *data,
	unsigned int data_len)
{
	u32 vdm_hdr = 0;
	int rc = 0;
	struct tcp_dpm_custom_vdm_data *vdm_data;
	struct mtk_pd_adapter_info *info;
	unsigned int *int_data;
	unsigned int outlen;
	// test
	int i;
	// test

	if (in_interrupt()) {
		int_data = kmalloc(40, GFP_ATOMIC);
		vdm_data = kmalloc(sizeof(*vdm_data), GFP_ATOMIC);
		pr_info("%s: kmalloc atomic ok.\n", __func__);
	} else {
		int_data = kmalloc(40, GFP_KERNEL);
		vdm_data = kmalloc(sizeof(*vdm_data), GFP_KERNEL);
		pr_info("%s: kmalloc kernel ok.\n", __func__);
	}
	memset(int_data, 0, 40);

	charToint(data, data_len, int_data, &outlen);

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

	vdm_hdr = VDM_HDR(info->adapter_dev->adapter_svid, USBPD_VDM_REQUEST, cmd);
	vdm_data->wait_resp = true;
	vdm_data->vdos[0] = vdm_hdr;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
	case USBPD_UVDM_CHARGER_TEMP:
	case USBPD_UVDM_CHARGER_VOLTAGE:
		vdm_data->cnt = 1;
		rc = tcpm_dpm_send_custom_vdm(info->tcpc, vdm_data, &cb_data);//&tcp_dpm_evt_cb_null
		if (rc < 0) {
			chr_err("failed to send %d\n", cmd);
			return rc;
		}
		break;
	case USBPD_UVDM_VERIFIED:
	case USBPD_UVDM_REMOVE_COMPENSATION:
		if (info->non_mtk_pps_ctr) {
			if (cmd == USBPD_UVDM_REMOVE_COMPENSATION)
				break;
		}

		vdm_data->cnt = 1 + USBPD_UVDM_VERIFIED_LEN;

		for (i = 0; i < USBPD_UVDM_VERIFIED_LEN; i++)
			vdm_data->vdos[i + 1] = int_data[i];
		pr_info("verify-0: %08x\n", vdm_data->vdos[1]);

		rc = tcpm_dpm_send_custom_vdm(info->tcpc, vdm_data, &cb_data);//&tcp_dpm_evt_cb_null
		if (rc < 0) {
			chr_err("failed to send %d\n", cmd);
			return rc;
		}
		break;
	case USBPD_UVDM_SESSION_SEED:
	case USBPD_UVDM_AUTHENTICATION:
		usbpd_sha256_bitswap32(int_data, USBPD_UVDM_SS_LEN);
		vdm_data->cnt = 1 + USBPD_UVDM_SS_LEN;
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++)
			vdm_data->vdos[i + 1] = int_data[i];

		for (i = 0; i < USBPD_UVDM_SS_LEN; i++)
			pr_info("%08x\n", vdm_data->vdos[i+1]);

		rc = tcpm_dpm_send_custom_vdm(info->tcpc, vdm_data, &cb_data);//&tcp_dpm_evt_cb_null
		if (rc < 0) {
			chr_err("failed to send %d\n", cmd);
			return rc;
		}
		break;
	default:
		chr_err("cmd:%d is not support\n", cmd);
		break;
	}
	kfree(int_data);
	return rc;
}

static int pd_get_power_role(struct adapter_device *dev)
{
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

	info->adapter_dev->role = tcpm_inquire_pd_power_role(info->tcpc);
	chr_err("[%s] power role is %d\n", __func__, info->adapter_dev->role);
	return MTK_ADAPTER_OK;
}

static int pd_get_current_state(struct adapter_device *dev)
{
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

	info->adapter_dev->current_state = tcpm_inquire_pd_state_curr(info->tcpc);
	chr_err("[%s] current state is %d\n", __func__, info->adapter_dev->current_state);
	return MTK_ADAPTER_OK;
}

/* 2021.02.24 longcheer jiangshitian edit for lenovo c-to-c discharge repeat HTH-142754 start */
bool pd_is_5v2a = false;

bool pd_pdos_is_5v2a(void)
{
	return pd_is_5v2a;
}
EXPORT_SYMBOL_GPL(pd_pdos_is_5v2a);
/* 2021.02.24 longcheer jiangshitian edit for lenovo c-to-c discharge repeat HTH-142754 end */


static int pd_get_pdos(struct adapter_device *dev)
{
	struct mtk_pd_adapter_info *info;
	struct tcpm_power_cap cap;
	int ret, i;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

#if 0
	info->adapter_dev->role = tcpm_inquire_pd_power_role(info->tcpc);
	chr_err("[%s] power role is %d\n", __func__, info->adapter_dev->role);

	if (info->adapter_dev->role == PD_ROLE_SINK_FOR_ADAPTER) {
		ret = tcpm_inquire_pd_sink_cap(info->tcpc, &cap);
		chr_err("[%s] tcpm_inquire_pd_sink_cap is %d.\n", __func__, ret);
	} else if  (info->adapter_dev->role == PD_ROLE_SOURCE_FOR_ADAPTER) {
		ret = tcpm_inquire_pd_source_cap(info->tcpc, &cap);
		chr_err("[%s] tcpm_inquire_pd_source_cap is %d.\n", __func__, ret);
	}
#endif

	ret = tcpm_inquire_pd_source_cap(info->tcpc, &cap);
	chr_err("[%s] tcpm_inquire_pd_source_cap is %d.\n", __func__, ret);
	if (ret)
		return MTK_ADAPTER_ERROR;
	for (i = 0; i < 7; i++) {
		info->adapter_dev->received_pdos[i] = cap.pdos[i];
		chr_err("[%s]: pdo[%d] { received_pdos is %08x, cap.pdos is %08x}\n",
			__func__, i, info->adapter_dev->received_pdos[i], cap.pdos[i]);
	}

	/* 2021.02.24 longcheer jiangshitian edit for lenovo c-to-c discharge repeat HTH-142754 start */
	if(info->adapter_dev->received_pdos[0] == 0x2701912c ||info->adapter_dev->received_pdos[0] == 0x26019096)
		pd_is_5v2a = true;
	else
		pd_is_5v2a = false;

	//chr_err("[%s]: pd_is_5v2a = %d ,adapter_dev->received_pdos[0 is %08x\n", __func__, pd_is_5v2a,info->adapter_dev->received_pdos[0]);
	/* 2021.02.24 longcheer jiangshitian edit for lenovo c-to-c discharge repeat HTH-142754 end */

	return MTK_ADAPTER_OK;
}

static struct adapter_ops adapter_ops = {
	.get_status = pd_get_status,
	.set_cap = pd_set_cap,
	.get_output = pd_get_output,
	.get_property = pd_get_property,
	.get_cap = pd_get_cap,
	.set_cap_bq = pd_set_cap_bq,
	.get_svid = pd_get_svid,
	//.get_id = pd_get_id,
	.request_vdm_cmd = pd_request_vdm_cmd,
	.get_power_role = pd_get_power_role,
	.get_current_state = pd_get_current_state,
	.get_pdos = pd_get_pdos,
	.set_pd_verify_process = pd_set_pd_verify_process,
};

static int adapter_parse_dt(struct mtk_pd_adapter_info *info,
	struct device *dev)
{
	struct device_node *np = dev->of_node;

	chr_err("%s\n", __func__);

	if (!np) {
		chr_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "adapter_name",
		&info->adapter_dev_name) < 0)
		chr_err("%s: no adapter name\n", __func__);

	info->non_mtk_pps_ctr = of_property_read_bool(np,
		"mi,non-mtk-pps-ctrl");

	return 0;
}

static int mtk_pd_adapter_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtk_pd_adapter_info *info = NULL;
	static bool is_deferred;

	chr_err("%s\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct mtk_pd_adapter_info),
			GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	adapter_parse_dt(info, &pdev->dev);

	info->adapter_dev = adapter_device_register(info->adapter_dev_name,
		&pdev->dev, info, &adapter_ops, NULL);
	if (IS_ERR_OR_NULL(info->adapter_dev)) {
		ret = PTR_ERR(info->adapter_dev);
		goto err_register_adapter_dev;
	}

	adapter_dev_set_drvdata(info->adapter_dev, info);

	info->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (info->tcpc == NULL) {
		if (is_deferred == false) {
			pr_info("%s: tcpc device not ready, defer\n", __func__);
			is_deferred = true;
			ret = -EPROBE_DEFER;
		} else {
			pr_info("%s: failed to get tcpc device\n", __func__);
			ret = -EINVAL;
		}
		goto err_get_tcpc_dev;
	}

	info->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(info->tcpc, &info->pd_nb,
				TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC | TCP_NOTIFY_TYPE_MODE);
	if (ret < 0) {
		pr_info("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}

	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		val.intval = POWER_SUPPLY_PD_INACTIVE;
		power_supply_set_property(usb_psy,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
	} else {
		chr_err("[%s] usb psy not found!\n", __func__);
	}
	return 0;

err_get_tcpc_dev:
	adapter_device_unregister(info->adapter_dev);
err_register_adapter_dev:
	devm_kfree(&pdev->dev, info);

	return ret;
}

static int mtk_pd_adapter_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pd_adapter_shutdown(struct platform_device *dev)
{
}

static const struct of_device_id mtk_pd_adapter_of_match[] = {
	{.compatible = "mediatek,pd_adapter",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pd_adapter_of_match);


static struct platform_driver mtk_pd_adapter_driver = {
	.probe = mtk_pd_adapter_probe,
	.remove = mtk_pd_adapter_remove,
	.shutdown = mtk_pd_adapter_shutdown,
	.driver = {
		   .name = "pd_adapter",
		   .of_match_table = mtk_pd_adapter_of_match,
	},
};

static int __init mtk_pd_adapter_init(void)
{
	return platform_driver_register(&mtk_pd_adapter_driver);
}
module_init(mtk_pd_adapter_init);

static void __exit mtk_pd_adapter_exit(void)
{
	platform_driver_unregister(&mtk_pd_adapter_driver);
}
module_exit(mtk_pd_adapter_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK PD Adapter Driver");
MODULE_LICENSE("GPL");

