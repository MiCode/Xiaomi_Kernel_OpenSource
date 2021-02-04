/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kobject.h>

#include "ccci_core.h"
#include "ccci_fsm.h"
#include "ccci_modem.h"
#include "ccci_port.h"
#include "ccci_hif.h"
#ifdef FEATURE_SCP_CCCI_SUPPORT
#include "scp_helper.h"
#endif

static void *dev_class;
#ifdef FEATURE_SCP_CCCI_SUPPORT
static int scp_stop;
#endif
/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;


char *ccci_get_ap_platform(void)
{
	return AP_PLATFORM_INFO;
}


int boot_md_show(int md_id, char *buf, int size)
{
	int curr = 0;

	if (get_modem_is_enabled(md_id))
		curr += snprintf(&buf[curr], size, "md%d:%d",
			md_id + 1, ccci_fsm_get_md_state(md_id));
	return curr;
}

int boot_md_store(int md_id)
{
	return -EACCES;
}

static void ccci_md_obj_release(struct kobject *kobj)
{
	CCCI_ERROR_LOG(-1, CORE, "md kobject release\n");
}

static ssize_t ccci_md_attr_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr,
		struct ccci_md_attribute, attr);

	if (a->show)
		len = a->show(a->modem, buf);

	return len;
}

static ssize_t ccci_md_attr_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr,
		struct ccci_md_attribute, attr);

	if (a->store)
		len = a->store(a->modem, buf, count);

	return len;
}

static const struct sysfs_ops ccci_md_sysfs_ops = {
	.show = ccci_md_attr_show,
	.store = ccci_md_attr_store
};

static struct attribute *ccci_md_default_attrs[] = {
	NULL
};

static struct kobj_type ccci_md_ktype = {
	.release = ccci_md_obj_release,
	.sysfs_ops = &ccci_md_sysfs_ops,
	.default_attrs = ccci_md_default_attrs
};

void ccci_sysfs_add_md(int md_id, void *kobj)
{
	ccci_sysfs_add_modem(md_id, (void *)kobj, (void *)&ccci_md_ktype,
		boot_md_show, boot_md_store);
}

#ifdef FEATURE_SCP_CCCI_SUPPORT
static int apsync_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY:
		fsm_scp_init0();
		if (scp_stop == 1) {
			ccci_port_send_msg_to_md(MD_SYS1,
					CCCI_SYSTEM_TX, CCISM_SHM_INIT, 0, 1);
			CCCI_NORMAL_LOG(0, CORE, "SCP reboot---\n");
			scp_stop = 0;
		}
		break;
	case SCP_EVENT_STOP:
		scp_stop = 1;
		CCCI_NORMAL_LOG(0, CORE, "SCP stop---\n");
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block apsync_notifier = {
	.notifier_call = apsync_event,
};
#endif

static int __init ccci_init(void)
{
	CCCI_INIT_LOG(-1, CORE, "ccci core init\n");
	dev_class = class_create(THIS_MODULE, "ccci_node");
	ccci_subsys_bm_init();
#ifdef FEATURE_SCP_CCCI_SUPPORT
	scp_A_register_notify(&apsync_notifier);
#endif
	return 0;
}

static void receive_wakeup_src_notify(int md_id, char *buf, unsigned int len)
{
	int tmp_data = 0;

	if (len == 0) {
		/* before spm add MD_WAKEUP_SOURCE parameter. */
		if (md_id == MD_SYS1)
			ccci_hif_set_wakeup_src(MD1_NET_HIF, 1);
#if (MD_GENERATION >= 6293)
			ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);
#endif
		if (md_id == MD_SYS3)
			ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);
		return;
	}

	/* after spm add MD_WAKEUP_SOURCE parameter. */
	if (len > sizeof(tmp_data))
		len = sizeof(tmp_data);
	memcpy((void *)&tmp_data, buf, len);
	switch (tmp_data) {
	case WAKE_SRC_HIF_CCIF0:
		ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);
		break;
	case WAKE_SRC_HIF_CLDMA:
	case WAKE_SRC_HIF_DPMAIF:
		ccci_hif_set_wakeup_src(MD1_NET_HIF, 1);
		break;
	default:
		break;
	};
}

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf,
	unsigned int len)
{
	int ret = 0;
	int tmp_data;

	if (!get_modem_is_enabled(md_id)) {
		CCCI_ERROR_LOG(md_id, CORE,
			"wrong MD ID from %ps for %d\n",
			__builtin_return_address(0), id);
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	}

	CCCI_DEBUG_LOG(md_id, CORE, "%ps execute function %d\n",
		__builtin_return_address(0), id);
	switch (id) {
	case ID_GET_MD_WAKEUP_SRC:
		receive_wakeup_src_notify(md_id, buf, len);
		break;
	case ID_GET_TXPOWER:
		if (buf[0] == 0)
			ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				MD_TX_POWER, 0, 0);
		else if (buf[0] == 1)
			ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				MD_RF_TEMPERATURE, 0, 0);
		else if (buf[0] == 2)
			ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				MD_RF_TEMPERATURE_3G, 0, 0);
		break;
	case ID_FORCE_MD_ASSERT:
		CCCI_NORMAL_LOG(md_id, CORE, "Force MD assert called by %s\n",
			current->comm);
		ret = ccci_md_force_assert(md_id,
			MD_FORCE_ASSERT_BY_USER_TRIGGER,
			NULL, 0);
		break;
	case ID_MD_MPU_ASSERT:
		if (md_id == MD_SYS1) {
			if (buf != NULL && strlen(buf)) {
				CCCI_NORMAL_LOG(md_id, CORE,
					"Force MD assert(MPU) called by %s\n",
					current->comm);
				ret = ccci_md_force_assert(md_id,
					MD_FORCE_ASSERT_BY_AP_MPU,
					buf, len);
			} else {
				CCCI_NORMAL_LOG(md_id, CORE,
					"ACK (MPU violation) called by %s\n",
					current->comm);
				ret = ccci_port_send_msg_to_md(md_id,
					CCCI_SYSTEM_TX,
					MD_AP_MPU_ACK_MD, 0, 0);
			}
		} else
			CCCI_NORMAL_LOG(md_id, CORE,
				"MD%d MPU API called by %s\n",
				md_id, current->comm);
		break;
	case ID_PAUSE_LTE:
		/*
		 * MD booting/flight mode/exception mode: return >0 to DVFS.
		 * MD ready: return 0 if message delivered,
		 * return <0 if get error.
		 * DVFS will call this API with IRQ disabled.
		 */
		if (ccci_fsm_get_md_state(md_id) != READY)
			ret = 1;
		else {
			ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
					MD_PAUSE_LTE, *((int *)buf), 1);
			if (ret == -CCCI_ERR_MD_NOT_READY ||
				ret == -CCCI_ERR_HIF_NOT_POWER_ON)
				ret = 1;
		}
		break;
	case ID_GET_MD_STATE:
		ret = ccci_fsm_get_md_state_for_user(md_id);
		break;
		/* used for throttling feature - start */
	case ID_THROTTLING_CFG:
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				MD_THROTTLING,
				*((int *)buf), 1);
		break;
		/* used for throttling feature - end */
	case ID_UPDATE_TX_POWER:
		{
			unsigned int msg_id = (md_id == 0) ?
				MD_SW_MD1_TX_POWER :
				MD_SW_MD2_TX_POWER;
			unsigned int mode = *((unsigned int *)buf);

			ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				msg_id, mode, 0);
		}
		break;
	case ID_DUMP_MD_SLEEP_MODE:
		ccci_md_dump_info(md_id, DUMP_FLAG_SMEM_MDSLP, NULL, 0);
		break;
	case ID_PMIC_INTR:
		ret = ccci_port_send_msg_to_md(md_id,
				CCCI_SYSTEM_TX, PMIC_INTR_MODEM_BUCK_OC,
				*((int *)buf), 1);
		break;
	case ID_LWA_CONTROL_MSG:
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
			LWA_CONTROL_MSG, *((int *)buf), 1);
		break;
	case MD_DISPLAY_DYNAMIC_MIPI:
		tmp_data = 0;
		memcpy((void *)&tmp_data, buf, len);
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
			id, tmp_data, 0);
		break;
	default:
		ret = -CCCI_ERR_FUNC_ID_ERROR;
		break;
	};
	return ret;
}

int aee_dump_ccci_debug_info(int md_id, void **addr, int *size)
{
	struct ccci_smem_region *mdccci_dbg;
	struct ccci_per_md *per_md_data;

	md_id--; /* EE string use 1 and 2, not 0 and 1 */
	if (!get_modem_is_enabled(md_id))
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	mdccci_dbg = ccci_md_get_smem_by_user_id(md_id,
		SMEM_USER_RAW_MDCCCI_DBG);

	*addr = mdccci_dbg->base_ap_view_vir;
	*size = mdccci_dbg->size;
	per_md_data = ccci_get_per_md_data(md_id);
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM))
		return 0;
	else
		return -1;
}

int ccci_register_dev_node(const char *name, int major_id, int minor)
{
	int ret = 0;
	dev_t dev_n;
	struct device *dev;

	dev_n = MKDEV(major_id, minor);
	dev = device_create(dev_class, NULL, dev_n, NULL, "%s", name);

	if (IS_ERR(dev))
		ret = PTR_ERR(dev);

	return ret;
}

subsys_initcall(ccci_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Evolved CCCI driver");
MODULE_LICENSE("GPL");
