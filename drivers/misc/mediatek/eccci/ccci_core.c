/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*
 * CCCI common service and routine. Consider it as a "logical" layer.
 *
 * V0.1: Xiao Wang <xiao.wang@mediatek.com>
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kobject.h>

#include <mt-plat/mt_ccci_common.h>
#include "ccci_config.h"
#include "ccci_platform.h"

#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_modem.h"
#include "ccci_support.h"
#include "port_proxy.h"
#include "mdee_ctl.h"
static void *dev_class;

/* used for throttling feature - start */
unsigned long ccci_modem_boot_count[5];
unsigned long ccci_get_md_boot_count(int md_id)
{
	return ccci_modem_boot_count[md_id];
}

/* used for throttling feature - end */


char *ccci_get_ap_platform(void)
{
	return AP_PLATFORM_INFO;
}

/*
 * for debug log: 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL
#define CCCI_LOG_LEVEL 0
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;

int boot_md_show(int md_id, char *buf, int size)
{
	int curr = 0;
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md)
		curr += snprintf(&buf[curr], size, "md%d:%d/%d", md->index + 1,
				 ccci_md_get_state(md), mdee_get_ex_stage(md->mdee_obj));
	return curr;
}

int boot_md_store(int md_id)
{
	struct port_proxy *proxy_p = ccci_md_get_port_proxy_by_id(md_id);

	if (proxy_p)
		return port_proxy_start_md(proxy_p);
	return -1;
}
/* ================================================================== */
/* MD relate sys */
/* ================================================================== */
static void ccci_md_obj_release(struct kobject *kobj)
{
	struct ccci_modem *md = container_of(kobj, struct ccci_modem, kobj);

	CCCI_DEBUG_LOG(md->index, SYSFS, "md kobject release\n");
}

static ssize_t ccci_md_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr, struct ccci_md_attribute, attr);

	if (a->show)
		len = a->show(a->modem, buf);

	return len;
}

static ssize_t ccci_md_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr, struct ccci_md_attribute, attr);

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
	ccci_sysfs_add_modem(md_id, (void *)kobj, (void *)&ccci_md_ktype, boot_md_show, boot_md_store);
}
#ifdef FEATURE_SCP_CCCI_SUPPORT
#include <scp_ipi.h>
static int scp_state = SCP_CCCI_STATE_INVALID;
static struct ccci_ipi_msg scp_ipi_tx_msg;
static struct mutex scp_ipi_tx_mutex;
static struct work_struct scp_ipi_rx_work;
static wait_queue_head_t scp_ipi_rx_wq;
static struct ccci_skb_queue scp_ipi_rx_skb_list;

void scp_md_state_sync_work(struct work_struct *work)
{
	struct ccci_modem *md = container_of(work, struct ccci_modem, scp_md_state_sync_work);
	int data, ret;
	MD_STATE_FOR_USER state = get_md_state_for_user(md);

	switch (state) {
	case MD_STATE_READY:
		switch (md->index) {
		case MD_SYS1:
			ret = port_proxy_send_msg_to_md(md->port_proxy_obj, CCCI_SYSTEM_TX, CCISM_SHM_INIT, 0, 1);
			if (ret < 0)
				CCCI_ERROR_LOG(md->index, CORE, "fail to send CCISM_SHM_INIT %d\n", ret);
			break;
		case MD_SYS3:
			ret = port_proxy_send_msg_to_md(md->port_proxy_obj, CCCI_CONTROL_TX, C2K_CCISM_SHM_INIT, 0, 1);
			if (ret < 0)
				CCCI_ERROR_LOG(md->index, CORE, "fail to send CCISM_SHM_INIT %d\n", ret);
			break;
		};
		break;
	case MD_STATE_EXCEPTION:
		data = get_md_state_for_user(md);
		ccci_scp_ipi_send(md->index, CCCI_OP_MD_STATE, &data);
		break;
	default:
		break;
	};
}

static void ccci_scp_ipi_rx_work(struct work_struct *work)
{
	struct ccci_ipi_msg *ipi_msg_ptr;
	struct ccci_modem *md, *md3;
	struct sk_buff *skb = NULL;
	int data;

	while (!skb_queue_empty(&scp_ipi_rx_skb_list.skb_list)) {
		skb = ccci_skb_dequeue(&scp_ipi_rx_skb_list);
		ipi_msg_ptr = (struct ccci_ipi_msg *)skb->data;
		md = ccci_md_get_modem_by_id(ipi_msg_ptr->md_id);
		if (!md) {
			CCCI_ERROR_LOG(ipi_msg_ptr->md_id, CORE, "MD not exist\n");
			return;
		}
		switch (ipi_msg_ptr->op_id) {
		case CCCI_OP_SCP_STATE:
			switch (ipi_msg_ptr->data[0]) {
			case SCP_CCCI_STATE_BOOTING:
				if (scp_state == SCP_CCCI_STATE_RBREADY) {
					CCCI_NORMAL_LOG(md->index, CORE, "SCP reset detected\n");
					port_proxy_send_msg_to_md(md->port_proxy_obj,
							CCCI_SYSTEM_TX, CCISM_SHM_INIT, 0, 1);
					md3 = ccci_md_get_modem_by_id(MD_SYS3);
					if (md3)
						port_proxy_send_msg_to_md(md3->port_proxy_obj, CCCI_CONTROL_TX,
								C2K_CCISM_SHM_INIT, 0, 1);
				} else {
					CCCI_NORMAL_LOG(md->index, CORE, "SCP boot up\n");
				}
				/* too early to init share memory here, EMI MPU may not be ready yet */
				break;
			case SCP_CCCI_STATE_RBREADY:
				switch (md->index) {
				case MD_SYS1:
					port_proxy_send_msg_to_md(md->port_proxy_obj,
							CCCI_SYSTEM_TX, CCISM_SHM_INIT_DONE, 0, 1);
					break;
				case MD_SYS3:
					port_proxy_send_msg_to_md(md->port_proxy_obj,
							CCCI_CONTROL_TX, C2K_CCISM_SHM_INIT_DONE, 0, 1);
					break;
				};
				data = get_md_state_for_user(md);
				ccci_scp_ipi_send(md->index, CCCI_OP_MD_STATE, &data);
				break;
			default:
				break;
			};
			scp_state = ipi_msg_ptr->data[0];
			break;
		};
		ccci_free_skb(skb);
	}
}

static void ccci_scp_ipi_handler(int id, void *data, unsigned int len)
{
	struct ccci_ipi_msg *ipi_msg_ptr = (struct ccci_ipi_msg *)data;
	struct sk_buff *skb = NULL;

	if (len != sizeof(struct ccci_ipi_msg)) {
		CCCI_ERROR_LOG(-1, CORE, "IPI handler, data length wrong %d vs. %ld\n", len,
						sizeof(struct ccci_ipi_msg));
		return;
	}
	CCCI_NORMAL_LOG(ipi_msg_ptr->md_id, CORE, "IPI handler %d/0x%x, %d\n",
				ipi_msg_ptr->op_id, ipi_msg_ptr->data[0], len);

	skb = ccci_alloc_skb(len, 0, 0);
	if (!skb)
		return;
	memcpy(skb_put(skb, len), data, len);
	ccci_skb_enqueue(&scp_ipi_rx_skb_list, skb);
	schedule_work(&scp_ipi_rx_work); /* ipi_send use mutex, can not be called from ISR context */
}

static void ccci_scp_init(void)
{
	int ret;

	mutex_init(&scp_ipi_tx_mutex);
	ret = scp_ipi_registration(IPI_APCCCI, ccci_scp_ipi_handler, "AP CCCI");
	CCCI_INIT_LOG(-1, CORE, "register IPI %d %d\n", IPI_APCCCI, ret);
	INIT_WORK(&scp_ipi_rx_work, ccci_scp_ipi_rx_work);
	init_waitqueue_head(&scp_ipi_rx_wq);
	ccci_skb_queue_init(&scp_ipi_rx_skb_list, 16, 16, 0);
}

int ccci_scp_ipi_send(int md_id, int op_id, void *data)
{
	int ret = 0;

	if (scp_state == SCP_CCCI_STATE_INVALID) {
		CCCI_ERROR_LOG(md_id, CORE, "ignore IPI %d, SCP state %d!\n", op_id, scp_state);
		return -CCCI_ERR_MD_NOT_READY;
	}

	mutex_lock(&scp_ipi_tx_mutex);
	memset(&scp_ipi_tx_msg, 0, sizeof(scp_ipi_tx_msg));
	scp_ipi_tx_msg.md_id = md_id;
	scp_ipi_tx_msg.op_id = op_id;
	scp_ipi_tx_msg.data[0] = *((u32 *)data);
	CCCI_NORMAL_LOG(scp_ipi_tx_msg.md_id, CORE, "IPI send %d/0x%x, %ld\n",
				scp_ipi_tx_msg.op_id, scp_ipi_tx_msg.data[0], sizeof(struct ccci_ipi_msg));
	if (scp_ipi_send(IPI_APCCCI, &scp_ipi_tx_msg, sizeof(scp_ipi_tx_msg), 1) != DONE) {
		CCCI_ERROR_LOG(md_id, CORE, "IPI send fail!\n");
		ret = -CCCI_ERR_MD_NOT_READY;
	}
	mutex_unlock(&scp_ipi_tx_mutex);
	return ret;
}
#endif


/* ------------------------------------------------------------------------- */
static int __init ccci_init(void)
{
	CCCI_INIT_LOG(-1, CORE, "ccci core init\n");
	dev_class = class_create(THIS_MODULE, "ccci_node");
	/* init common sub-system */
	/* ccci_subsys_sysfs_init(); */
	ccci_subsys_bm_init();
	ccci_plat_common_init();
#ifdef FEATURE_SCP_CCCI_SUPPORT
	ccci_scp_init();
#endif
#ifdef FEATURE_MTK_SWITCH_TX_POWER
	swtp_init(0);
#endif
	return 0;
}


int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len)
{
	struct ccci_modem *md = NULL;
	int ret = 0;

	md = ccci_md_get_modem_by_id(md_id);
	if (!md) {
		CCCI_ERROR_LOG(md_id, CORE, "wrong MD ID from %ps for %d\n", __builtin_return_address(0), id);
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	}

	CCCI_DEBUG_LOG(md->index, CORE, "%ps execute function %d\n", __builtin_return_address(0), id);
	switch (id) {
	case ID_GET_MD_WAKEUP_SRC:
		atomic_set(&md->wakeup_src, 1);
		break;
	case ID_GET_TXPOWER:
		if (buf[0] == 0)
			ret = port_proxy_send_msg_to_md(md->port_proxy_obj, CCCI_SYSTEM_TX, MD_TX_POWER, 0, 0);
		else if (buf[0] == 1)
			ret = port_proxy_send_msg_to_md(md->port_proxy_obj, CCCI_SYSTEM_TX, MD_RF_TEMPERATURE, 0, 0);
		else if (buf[0] == 2)
			ret = port_proxy_send_msg_to_md(md->port_proxy_obj, CCCI_SYSTEM_TX, MD_RF_TEMPERATURE_3G, 0, 0);
		break;
	case ID_FORCE_MD_ASSERT:
		CCCI_NORMAL_LOG(md->index, CHAR, "Force MD assert called by %s\n", current->comm);
		ret = ccci_md_force_assert(md, MD_FORCE_ASSERT_BY_USER_TRIGGER, NULL, 0);
		break;
#ifdef MD_UMOLY_EE_SUPPORT
	case ID_MD_MPU_ASSERT:
		if (md->index == MD_SYS1) {
			if (buf != NULL && strlen(buf)) {
				CCCI_NORMAL_LOG(md->index, CHAR, "Force MD assert(MPU) called by %s\n", current->comm);
				ret = ccci_md_force_assert(md, MD_FORCE_ASSERT_BY_AP_MPU, buf, len);
			} else {
				CCCI_NORMAL_LOG(md->index, CHAR, "ACK (MPU violation) called by %s\n", current->comm);
				ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_AP_MPU_ACK_MD, 0, 0);
			}
		} else
			CCCI_NORMAL_LOG(md->index, CHAR, "MD%d MPU API called by %s\n", md->index, current->comm);
		break;
#endif
	case ID_PAUSE_LTE:
		/*
		 * MD booting/flight mode/exception mode: return >0 to DVFS.
		 * MD ready: return 0 if message delivered, return <0 if get error.
		 * DVFS will call this API with IRQ disabled.
		 */
		if (md->md_state != READY)
			ret = 1;
		else {
			ret = port_proxy_send_msg_to_md(md->port_proxy_obj,
						CCCI_SYSTEM_TX, MD_PAUSE_LTE, *((int *)buf), 1);
			if (ret == -CCCI_ERR_MD_NOT_READY || ret == -CCCI_ERR_HIF_NOT_POWER_ON)
				ret = 1;
		}
		break;
	case ID_STORE_SIM_SWITCH_MODE:
		{
			int simmode = *((int *)buf);

			ccci_store_sim_switch_mode(md, simmode);
		}
		break;
	case ID_GET_SIM_SWITCH_MODE:
		{
			int simmode = ccci_get_sim_switch_mode();

			memcpy(buf, &simmode, sizeof(int));
		}
		break;
	case ID_GET_MD_STATE:
		ret = get_md_state_for_user(md);
		break;
		/* used for throttling feature - start */
	case ID_THROTTLING_CFG:
		ret = port_proxy_send_msg_to_md(md->port_proxy_obj, CCCI_SYSTEM_TX, MD_THROTTLING, *((int *)buf), 1);
		break;
		/* used for throttling feature - end */
#if defined(FEATURE_MTK_SWITCH_TX_POWER)
	case ID_UPDATE_TX_POWER:
		{
			unsigned int msg_id = (md_id == 0) ? MD_SW_MD1_TX_POWER : MD_SW_MD2_TX_POWER;
			unsigned int mode = *((unsigned int *)buf);

			ret = port_proxy_send_msg_to_md(md->port_proxy_obj, CCCI_SYSTEM_TX, msg_id, mode, 0);
		}
		break;
#endif
	case ID_RESET_MD:
		CCCI_NOTICE_LOG(md->index, CHAR, "MD reset API called by %ps\n", __builtin_return_address(0));
		ret = port_proxy_send_msg_to_user(md->port_proxy_obj,
					CCCI_MONITOR_CH, CCCI_MD_MSG_RESET_REQUEST, 0);
		break;
	case ID_DUMP_MD_SLEEP_MODE:
		md->ops->dump_info(md, DUMP_FLAG_SMEM_MDSLP, NULL, 0);
		break;
	case ID_PMIC_INTR:
		ret = port_proxy_send_msg_to_md(md->port_proxy_obj,
					CCCI_SYSTEM_TX, PMIC_INTR_MODEM_BUCK_OC, *((int *)buf), 1);
		break;
	case ID_STOP_MD:
		CCCI_NOTICE_LOG(md->index, CHAR, "MD stop API called by %ps\n", __builtin_return_address(0));
		ret = port_proxy_send_msg_to_user(md->port_proxy_obj,
					CCCI_MONITOR_CH, CCCI_MD_MSG_FORCE_STOP_REQUEST, 0);
		break;
	case ID_START_MD:
		ret = port_proxy_send_msg_to_user(md->port_proxy_obj,
					CCCI_MONITOR_CH, CCCI_MD_MSG_FORCE_START_REQUEST, 0);
		break;
	case ID_UPDATE_MD_BOOT_MODE:
		if (*((unsigned int *)buf) > MD_BOOT_MODE_INVALID && *((unsigned int *)buf) < MD_BOOT_MODE_MAX)
			md->md_boot_mode = *((unsigned int *)buf);
		break;
	default:
		ret = -CCCI_ERR_FUNC_ID_ERROR;
		break;
	};
	return ret;
}

int aee_dump_ccci_debug_info(int md_id, void **addr, int *size)
{
	struct ccci_modem *md = NULL;
	struct ccci_smem_layout *smem_layout;
	int ret = 0;

	md_id--;		/* EE string use 1 and 2, not 0 and 1 */
	md = ccci_md_get_modem_by_id(md_id);
	if (!ret)
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	smem_layout = ccci_md_get_smem(md);

	*addr = smem_layout->ccci_exp_smem_ccci_debug_vir;
	*size = smem_layout->ccci_exp_smem_ccci_debug_size;
	if (md->md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM))
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

#if defined(FEATURE_MTK_SWITCH_TX_POWER)
static int switch_Tx_Power(int md_id, unsigned int mode)
{
	int ret = 0;
	unsigned int resv = mode;

	ret = exec_ccci_kern_func_by_md_id(md_id, ID_UPDATE_TX_POWER, (char *)&resv, sizeof(resv));

	pr_debug("[swtp] switch_MD%d_Tx_Power(%d): ret[%d]\n", md_id + 1, resv, ret);

	CCCI_DEBUG_LOG(md_id, "ctl", "switch_MD%d_Tx_Power(%d): %d\n", md_id + 1, resv, ret);

	return ret;
}

int switch_MD1_Tx_Power(unsigned int mode)
{
	return switch_Tx_Power(0, mode);
}
EXPORT_SYMBOL(switch_MD1_Tx_Power);

int switch_MD2_Tx_Power(unsigned int mode)
{
	return switch_Tx_Power(1, mode);
}
EXPORT_SYMBOL(switch_MD2_Tx_Power);
#endif

subsys_initcall(ccci_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Unified CCCI driver v0.1");
MODULE_LICENSE("GPL");
