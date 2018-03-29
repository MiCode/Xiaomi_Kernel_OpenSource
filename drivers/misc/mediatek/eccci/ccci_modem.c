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


#include "ccci_config.h"
#include "ccci_platform.h"

#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_support.h"
#include "port_proxy.h"
#include "mdee_ctl.h"
#include <mt-plat/mt_ccci_common.h>
#include <mt-plat/mt_boot_common.h>
#if defined(ENABLE_32K_CLK_LESS)
#include <mt-plat/mtk_rtc.h>
#endif

#define TAG "md"

static LIST_HEAD(modem_list);	/* don't use array, due to MD index may not be continuous */
int ccci_md_get_ex_type(struct ccci_modem *md)
{
	return mdee_get_ee_type(md->mdee_obj);
}
void ccci_md_exception_notify(struct ccci_modem *md, MD_EX_STAGE stage)
{
	if (md->mdee_obj == NULL) {
		CCCI_ERROR_LOG(md->index, KERN, "%s:md_ee=null, notify fail\n", __func__);
		return;
	}
	mdee_state_notify(md->mdee_obj, stage);
}

/* setup function is only for data structure initialization */
struct ccci_modem *ccci_md_alloc(int private_size)
{
	struct ccci_modem *md = kzalloc(sizeof(struct ccci_modem), GFP_KERNEL);

	if (!md) {
		CCCI_ERROR_LOG(-1, TAG, "fail to allocate memory for modem structure\n");
		goto out;
	}
	md->private_data = kzalloc(private_size, GFP_KERNEL);
	md->config.setting |= MD_SETTING_FIRST_BOOT;
	md->md_state = INVALID;
	md->is_in_ee_dump = 0;
	md->is_force_asserted = 0;
	atomic_set(&md->wakeup_src, 0);
	INIT_LIST_HEAD(&md->entry);
	ccci_reset_seq_num(md);
	md->md_dbg_dump_flag = MD_DBG_DUMP_ALL;

#ifdef FEATURE_SCP_CCCI_SUPPORT
	INIT_WORK(&md->scp_md_state_sync_work, scp_md_state_sync_work);
#endif
 out:
	return md;
}
/*
 * most of this file is copied from mtk_ccci_helper.c, we use this function to
 * translate legacy data structure into current CCCI core.
 */
void ccci_md_config(struct ccci_modem *md)
{
	phys_addr_t md_resv_mem_addr = 0, md_resv_smem_addr = 0;
	/* void __iomem *smem_base_vir; */
	unsigned int md_resv_mem_size = 0, md_resv_smem_size = 0;

	/* setup config */
	md->config.load_type = get_md_img_type(md->index);
	if (get_modem_is_enabled(md->index))
		md->config.setting |= MD_SETTING_ENABLE;
	else
		md->config.setting &= ~MD_SETTING_ENABLE;

	/* Get memory info */
	get_md_resv_mem_info(md->index, &md_resv_mem_addr, &md_resv_mem_size, &md_resv_smem_addr, &md_resv_smem_size);
	/* setup memory layout */
	/* MD image */
	md->mem_layout.md_region_phy = md_resv_mem_addr;
	md->mem_layout.md_region_size = md_resv_mem_size;
	md->mem_layout.md_region_vir = ioremap_nocache(md->mem_layout.md_region_phy, MD_IMG_DUMP_SIZE);
		/* do not remap whole region, consume too much vmalloc space */
	/* DSP image */
	md->mem_layout.dsp_region_phy = 0;
	md->mem_layout.dsp_region_size = 0;
	md->mem_layout.dsp_region_vir = 0;
	/* Share memory */
	md->mem_layout.smem_region_phy = md_resv_smem_addr;
	md->mem_layout.smem_region_size = md_resv_smem_size;
	md->mem_layout.smem_region_vir =
	    ioremap_nocache(md->mem_layout.smem_region_phy, md->mem_layout.smem_region_size);

	/* exception region */
	md->smem_layout.ccci_exp_smem_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_EXCEPTION;
	md->smem_layout.ccci_exp_smem_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_EXCEPTION;
	md->smem_layout.ccci_exp_smem_size = CCCI_SMEM_SIZE_EXCEPTION;
	md->smem_layout.ccci_exp_dump_size = CCCI_SMEM_DUMP_SIZE;

	/* dump region */
	md->smem_layout.ccci_exp_smem_ccci_debug_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_CCCI_DEBUG;
	md->smem_layout.ccci_exp_smem_ccci_debug_size = CCCI_SMEM_SIZE_CCCI_DEBUG;
	md->smem_layout.ccci_exp_smem_mdss_debug_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_MDSS_DEBUG;
#ifdef MD_UMOLY_EE_SUPPORT
	if (md->index == MD_SYS1)
		md->smem_layout.ccci_exp_smem_mdss_debug_size = CCCI_SMEM_SIZE_MDSS_DEBUG_UMOLY;
	else
#endif
		md->smem_layout.ccci_exp_smem_mdss_debug_size = CCCI_SMEM_SIZE_MDSS_DEBUG;

#ifdef FEATURE_FORCE_ASSERT_CHECK_EN
	md->smem_layout.ccci_exp_smem_force_assert_vir =
		md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_FORCE_ASSERT;
	md->smem_layout.ccci_exp_smem_force_assert_size = CCCI_SMEM_FORCE_ASSERT_SIZE;
#endif

	md->smem_layout.ccci_exp_smem_sleep_debug_vir =
		md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_SLEEP_MODE_DBG;
	md->smem_layout.ccci_exp_smem_sleep_debug_size = CCCI_SMEM_SLEEP_MODE_DBG_DUMP;
#ifdef FEATURE_DBM_SUPPORT
	md->smem_layout.ccci_exp_smem_dbm_debug_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_DBM_DEBUG;
#endif

	/*runtime region */
	md->smem_layout.ccci_rt_smem_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_RUNTIME;
	md->smem_layout.ccci_rt_smem_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_RUNTIME;
	md->smem_layout.ccci_rt_smem_size = CCCI_SMEM_SIZE_RUNTIME;

	/* CCISM region */
#ifdef FEATURE_SCP_CCCI_SUPPORT
	md->smem_layout.ccci_ccism_smem_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_CCISM;
	md->smem_layout.ccci_ccism_smem_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_CCISM;
	md->smem_layout.ccci_ccism_smem_size = CCCI_SMEM_SIZE_CCISM;
	md->smem_layout.ccci_ccism_dump_size = CCCI_SMEM_CCISM_DUMP_SIZE;
#endif
	/* CCB DHL region */
#ifdef FEATURE_DHL_CCB_RAW_SUPPORT
	md->smem_layout.ccci_ccb_dhl_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_CCB_DHL;
	md->smem_layout.ccci_ccb_dhl_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_CCB_DHL;
	md->smem_layout.ccci_ccb_dhl_size = CCCI_SMEM_SIZE_CCB_DHL;
	md->smem_layout.ccci_raw_dhl_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_RAW_DHL;
	md->smem_layout.ccci_raw_dhl_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_RAW_DHL;
	md->smem_layout.ccci_raw_dhl_size = CCCI_SMEM_SIZE_RAW_DHL;
#endif
	/* direct tethering region */
#ifdef FEATURE_DIRECT_TETHERING_LOGGING
	md->smem_layout.ccci_dt_netd_smem_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_DT_NETD;
	md->smem_layout.ccci_dt_netd_smem_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_DT_NETD;
	md->smem_layout.ccci_dt_netd_smem_size = CCCI_SMEM_SIZE_DT_NETD;
	md->smem_layout.ccci_dt_usb_smem_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_DT_USB;
	md->smem_layout.ccci_dt_usb_smem_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_DT_USB;
	md->smem_layout.ccci_dt_usb_smem_size = CCCI_SMEM_SIZE_DT_USB;
#endif

	/* AP<->MD CCIF share memory region */
#ifdef CCCI_SMEM_OFFSET_CCIF_SMEM
	if (md->index == MD_SYS3) {
		md->smem_layout.ccci_ccif_smem_base_phy = md->mem_layout.smem_region_phy + CCCI_SMEM_OFFSET_CCIF_SMEM;
		md->smem_layout.ccci_ccif_smem_base_vir = md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_CCIF_SMEM;
		md->smem_layout.ccci_ccif_smem_size = CCCI_SMEM_SIZE_CCIF_SMEM;
	}
#endif
	/* smart logging region */
#ifdef FEATURE_SMART_LOGGING
	if (md->index == MD_SYS1) {
		md->smem_layout.ccci_smart_logging_base_phy = md->mem_layout.smem_region_phy +
								CCCI_SMEM_OFFSET_SMART_LOGGING;
		md->smem_layout.ccci_smart_logging_base_vir = md->mem_layout.smem_region_vir +
								CCCI_SMEM_OFFSET_SMART_LOGGING;
		md->smem_layout.ccci_smart_logging_size = md->mem_layout.smem_region_size -
								CCCI_SMEM_OFFSET_SMART_LOGGING;
	}
#endif

	/* md1 md3 shared memory region and remap */
	get_md1_md3_resv_smem_info(md->index, &md->mem_layout.md1_md3_smem_phy,
		&md->mem_layout.md1_md3_smem_size);
	md->mem_layout.md1_md3_smem_vir =
	    ioremap_nocache(md->mem_layout.md1_md3_smem_phy, md->mem_layout.md1_md3_smem_size);

	/* updae image info */
	md->img_info[IMG_MD].type = IMG_MD;
	md->img_info[IMG_MD].address = md->mem_layout.md_region_phy;
	md->img_info[IMG_DSP].type = IMG_DSP;
	md->img_info[IMG_DSP].address = md->mem_layout.dsp_region_phy;
	md->img_info[IMG_ARMV7].type = IMG_ARMV7;

	if (md->config.setting & MD_SETTING_ENABLE)
		ccci_set_mem_remap(md, md_resv_smem_addr - md_resv_mem_addr,
				   ALIGN(md_resv_mem_addr + md_resv_mem_size + md_resv_smem_size, 0x2000000));
#if 0
	CCCI_INF_MSG(md->index, CORE, "dump memory layout\n");
	ccci_mem_dump(md->index, &md->mem_layout, sizeof(struct ccci_mem_layout));
	ccci_mem_dump(md->index, &md->smem_layout, sizeof(struct ccci_smem_layout));
#endif
}

struct ccci_modem *ccci_md_get_modem_by_id(int md_id)
{
	struct ccci_modem *md = NULL;

	list_for_each_entry(md, &modem_list, entry) {
		if (md->index == md_id)
			return md;
	}
	return NULL;
}

int ccci_md_register(struct ccci_modem *md)
{
	int ret;

	/* init per-modem sub-system */
	CCCI_INIT_LOG(md->index, TAG, "register modem\n");
	/* init modem */
	/* TODO: check modem->ops for all must-have functions */
	ret = md->ops->init(md);
	if (ret < 0)
		return ret;
	ccci_md_config(md);

	md->mdee_obj = mdee_alloc(md->index, md);
	/* must be after modem config to get smem layout */
	md->port_proxy_obj = port_proxy_alloc(md->index, md->capability, md->napi_queue_mask, md);

	list_add_tail(&md->entry, &modem_list);
	ccci_sysfs_add_md(md->index, (void *)&md->kobj);
	ccci_platform_init(md);
	ccci_fsm_init(md);
	return 0;
}


struct port_proxy *ccci_md_get_port_proxy(int md_id, int major)
{
	struct ccci_modem *md = NULL;
	struct port_proxy *proxy_p = NULL;

	list_for_each_entry(md, &modem_list, entry) {
		proxy_p = md->port_proxy_obj;
		if (md_id >= 0 && md->index == md_id)
			return md->port_proxy_obj;
		if (major >= 0 && proxy_p->major == major)
			return proxy_p;
	}
	return NULL;
}

int ccci_md_set_boot_data(struct ccci_modem *md, unsigned int data[], int len)
{
	int ret = 0;

	if (len < 0 || data == NULL)
		return -1;

	md->mdlg_mode = data[MD_CFG_MDLOG_MODE];
	md->sbp_code  = data[MD_CFG_SBP_CODE];
	md->md_dbg_dump_flag = data[MD_CFG_DUMP_FLAG] == MD_DBG_DUMP_INVALID ?
		md->md_dbg_dump_flag : data[MD_CFG_DUMP_FLAG];

	return ret;
}

struct ccci_modem *ccci_md_get_another(int md_id)
{
	struct ccci_modem *another_md = NULL;

	if (md_id == MD_SYS1 && get_modem_is_enabled(MD_SYS3))
		another_md = ccci_md_get_modem_by_id(MD_SYS3);

	if (md_id == MD_SYS3 && get_modem_is_enabled(MD_SYS1))
		another_md = ccci_md_get_modem_by_id(MD_SYS1);

	return another_md;
}

int ccci_md_get_state_by_id(int md_id)
{
	struct ccci_modem *md = NULL;

	list_for_each_entry(md, &modem_list, entry) {
		if (md->index == md_id)
			return md->md_state;
	}
	return -CCCI_ERR_MD_INDEX_NOT_FOUND;
}

int ccci_md_force_assert(struct ccci_modem *md, MD_FORCE_ASSERT_TYPE type, char *param, int len)
{
	int ret = 0;
#ifdef FEATURE_FORCE_ASSERT_CHECK_EN
	struct ccci_force_assert_shm_fmt *ccci_fa_smem_ptr = NULL;
#endif

	if (type == MD_FORCE_ASSERT_BY_AP_MPU) {
		mdee_set_ex_mpu_str(md->mdee_obj, param);
		ret = md->ops->force_assert(md, CCIF_MPU_INTR);
	} else {
#ifdef FEATURE_FORCE_ASSERT_CHECK_EN
		ccci_fa_smem_ptr =
				(struct ccci_force_assert_shm_fmt *)(md->smem_layout.ccci_exp_smem_force_assert_vir);
		if (ccci_fa_smem_ptr) {
			ccci_fa_smem_ptr->error_code = type;
			if (param != NULL && len > 0) {
				if (len > md->smem_layout.ccci_exp_smem_force_assert_size
							- sizeof(struct ccci_force_assert_shm_fmt))
					len = md->smem_layout.ccci_exp_smem_force_assert_size
							- sizeof(struct ccci_force_assert_shm_fmt);
				memcpy_toio(ccci_fa_smem_ptr->param, param, len);
			}
		}
#endif

		ret = md->ops->force_assert(md, CCIF_INTERRUPT);
	}
	md->is_force_asserted = 1;
	return ret;
}

MD_STATE_FOR_USER get_md_state_for_user(struct ccci_modem *md)
{
	switch (md->md_state) {
	case INVALID:
		/*fall through*/
	case WAITING_TO_STOP:
		/*fall through*/
	case GATED:
		return MD_STATE_INVALID;

	case RESET:
		/*fall through*/
	case BOOT_WAITING_FOR_HS1:
		/*fall through*/
	case BOOT_WAITING_FOR_HS2:
		return MD_STATE_BOOTING;

	case READY:
		return MD_STATE_READY;
	case EXCEPTION:
		return MD_STATE_EXCEPTION;
	default:
		CCCI_ERROR_LOG(md->index, CORE, "Invalid md state\n");
		return MD_STATE_INVALID;
	}
}


/*timeout: seconds. if timeout == 0, block wait*/
int ccci_md_check_ee_done(struct ccci_modem *md, int timeout)
{
	int count = 0;
	bool is_ee_done = 0;
	int time_step = 200; /*ms*/
	int loop_max = timeout * 1000 / time_step;

	CCCI_BOOTUP_LOG(md->index, KERN, "checking EE status\n");
	while (md->md_state == EXCEPTION) {
		if (port_proxy_get_critical_user(md->port_proxy_obj, CRIT_USR_MDLOG)) {
			CCCI_DEBUG_LOG(md->index, TAG, "mdlog running, waiting for EE dump done\n");
			is_ee_done = !mdee_flow_is_start(md->mdee_obj)
				&& port_proxy_get_mdlog_dump_done(md->port_proxy_obj);
		} else
			is_ee_done = !mdee_flow_is_start(md->mdee_obj);
		if (!is_ee_done) {
			msleep(time_step);
			count++;
		} else
			break;

		if (loop_max && (count > loop_max)) {
			CCCI_ERROR_LOG(md->index, TAG, "wait EE done timeout\n");
			return -1;
		}
	}
	CCCI_BOOTUP_LOG(md->index, TAG, "check EE done\n");
	return 0;
}

void ccci_md_set_reload_type(struct ccci_modem *md, int type)
{
	if (type != md->config.load_type) {
		if (set_modem_support_cap(md->index, type) == 0) {
			md->config.load_type = type;
			md->config.setting |= MD_SETTING_RELOAD;
		}
	}
}

int ccci_md_store_load_type(struct ccci_modem *md, int type)
{
	int ret = 0;
	int md_id = md->index;

	md->config.load_type_saving = type;

	CCCI_BOOTUP_LOG(md_id, TAG, "storing md type(%d) in kernel space!\n",
			 md->config.load_type_saving);
	ccci_event_log("md%d: storing md type(%d) in kernel space!\n", md_id,
			 md->config.load_type_saving);
	if (md->config.load_type_saving >= 1 && md->config.load_type_saving <= MAX_IMG_NUM) {
		if (md->config.load_type_saving != md->config.load_type)
			CCCI_BOOTUP_LOG(md_id, TAG,
					 "Maybe Wrong: md type storing not equal with current setting!(%d %d)\n",
					 md->config.load_type_saving, md->config.load_type);
	} else {
		CCCI_BOOTUP_LOG(md_id, TAG, "store md type fail: invalid md type(0x%x)\n",
				 md->config.load_type_saving);
		ret = -EFAULT;
	}
	return ret;
}

void ccci_md_status_notice(struct ccci_modem *md, DIRECTION dir, int filter_ch_no, int filter_queue_idx, MD_STATE state)
{
	port_proxy_md_status_notice(md->port_proxy_obj, dir, filter_ch_no, filter_queue_idx, state);
}

int ccci_md_send_msg_to_user(struct ccci_modem *md, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv)
{
	return port_proxy_send_msg_to_user(md->port_proxy_obj, ch, msg, resv);
}

int ccci_send_msg_to_md(struct ccci_modem *md, CCCI_CH ch, u32 msg, u32 resv, int blocking)
{
	return port_proxy_send_msg_to_md(md->port_proxy_obj, ch, msg, resv, blocking);
}

void ccci_md_dump_port_status(struct ccci_modem *md)
{
	port_proxy_dump_status(md->port_proxy_obj);
}

static void append_runtime_feature(char **p_rt_data, struct ccci_runtime_feature *rt_feature, void *data)
{
	CCCI_DEBUG_LOG(-1, KERN, "append rt_data %p, feature %u len %u\n",
		     *p_rt_data, rt_feature->feature_id, rt_feature->data_len);
	memcpy_toio(*p_rt_data, rt_feature, sizeof(struct ccci_runtime_feature));
	*p_rt_data += sizeof(struct ccci_runtime_feature);
	if (data != NULL) {
		memcpy_toio(*p_rt_data, data, rt_feature->data_len);
		*p_rt_data += rt_feature->data_len;
	}
}

static unsigned int get_booting_start_id(struct ccci_modem *md)
{
	LOGGING_MODE mdlog_flag = MODE_IDLE;
	u32 booting_start_id;

	mdlog_flag = md->mdlg_mode;

	if (md->md_boot_mode != MD_BOOT_MODE_INVALID) {
		if (md->md_boot_mode == MD_BOOT_MODE_META)
			booting_start_id = ((char)mdlog_flag << 8 | META_BOOT_ID);
		else if ((get_boot_mode() == FACTORY_BOOT || get_boot_mode() == ATE_FACTORY_BOOT))
			booting_start_id = ((char)mdlog_flag << 8 | FACTORY_BOOT_ID);
		else
			booting_start_id = ((char)mdlog_flag << 8 | NORMAL_BOOT_ID);
	} else {
		if (is_meta_mode() || is_advanced_meta_mode())
			booting_start_id = ((char)mdlog_flag << 8 | META_BOOT_ID);
		else if ((get_boot_mode() == FACTORY_BOOT || get_boot_mode() == ATE_FACTORY_BOOT))
			booting_start_id = ((char)mdlog_flag << 8 | FACTORY_BOOT_ID);
		else
			booting_start_id = ((char)mdlog_flag << 8 | NORMAL_BOOT_ID);
	}

	CCCI_BOOTUP_LOG(md->index, KERN, "get_booting_start_id 0x%x\n", booting_start_id);
	return booting_start_id;
}

static void config_ap_side_feature(struct ccci_modem *md, struct md_query_ap_feature *ap_side_md_feature)
{

	md->runtime_version = AP_MD_HS_V2;
	ap_side_md_feature->feature_set[BOOT_INFO].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[EXCEPTION_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	if (md->index == MD_SYS1)
		ap_side_md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	else
		ap_side_md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;

#ifdef FEATURE_SCP_CCCI_SUPPORT
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

#ifdef FEATURE_DHL_CCB_RAW_SUPPORT
	/* notice: CCB_SHARE_MEMORY should be set to support when at least one CCB region exists */
	ap_side_md_feature->feature_set[CCB_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[CCB_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

#ifdef FEATURE_DIRECT_TETHERING_LOGGING
	ap_side_md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

#ifdef FEATURE_SMART_LOGGING
	ap_side_md_feature->feature_set[SMART_LOGGING_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#else
	ap_side_md_feature->feature_set[SMART_LOGGING_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

#ifdef FEATURE_MD1MD3_SHARE_MEM
	ap_side_md_feature->feature_set[MD1MD3_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MD1MD3_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

	ap_side_md_feature->feature_set[MISC_INFO_HIF_DMA_REMAP].support_mask = CCCI_FEATURE_MUST_SUPPORT;

#if defined(ENABLE_32K_CLK_LESS)
	if (crystal_exist_status()) {
		CCCI_DEBUG_LOG(md->index, KERN, "MISC_32K_LESS no support, crystal_exist_status 1\n");
		ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	} else {
		CCCI_DEBUG_LOG(md->index, KERN, "MISC_32K_LESS support\n");
		ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	}
#else
	CCCI_DEBUG_LOG(md->index, KERN, "ENABLE_32K_CLK_LESS disabled\n");
	ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature->feature_set[MISC_INFO_RANDOM_SEED_NUM].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_GPS_COCLOCK].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_SBP_ID].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_CCCI].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#ifdef FEATURE_MD_GET_CLIB_TIME
	ap_side_md_feature->feature_set[MISC_INFO_CLIB_TIME].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_CLIB_TIME].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
#ifdef FEATURE_C2K_ALWAYS_ON
	ap_side_md_feature->feature_set[MISC_INFO_C2K].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_C2K].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature->feature_set[MD_IMAGE_START_MEMORY].support_mask = CCCI_FEATURE_OPTIONAL_SUPPORT;
	ap_side_md_feature->feature_set[EE_AFTER_EPOF].support_mask = CCCI_FEATURE_MUST_SUPPORT;

	ap_side_md_feature->feature_set[CCMNI_MTU].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#ifdef FEATURE_TC1_CUSTOMER_VAL
	ap_side_md_feature->feature_set[MISC_INFO_CUSTOMER_VAL].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_CUSTOMER_VAL].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
#ifdef ENABLE_FAST_HEADER
	ap_side_md_feature->feature_set[CCCI_FAST_HEADER].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#endif
#ifdef FEATURE_SYNC_C2K_MEID
	ap_side_md_feature->feature_set[MISC_INFO_C2K_MEID].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_C2K_MEID].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

}

int ccci_md_prepare_runtime_data(struct ccci_modem *md, struct sk_buff *skb)
{
	u8 i = 0;
	u32 total_len;
	/*runtime data buffer */
	char *rt_data = (char *)md->smem_layout.ccci_rt_smem_base_vir;

	struct ccci_runtime_feature rt_feature;
	/*runtime feature type */
	struct ccci_runtime_share_memory rt_shm;
	struct ccci_misc_info_element rt_f_element;

	struct md_query_ap_feature *md_feature;
	struct md_query_ap_feature md_feature_ap;
	struct ccci_runtime_boot_info boot_info;
	unsigned int random_seed = 0;
	struct timeval t;
#ifdef FEATURE_C2K_ALWAYS_ON
	unsigned int c2k_flags = 0;
#endif

	CCCI_BOOTUP_LOG(md->index, KERN, "prepare_runtime_data  AP total %u features\n", MD_RUNTIME_FEATURE_ID_MAX);

	memset(&md_feature_ap, 0, sizeof(struct md_query_ap_feature));
	config_ap_side_feature(md, &md_feature_ap);

	md_feature = (struct md_query_ap_feature *)skb_pull(skb, sizeof(struct ccci_header));

	if (md_feature->head_pattern != MD_FEATURE_QUERY_PATTERN ||
	    md_feature->tail_pattern != MD_FEATURE_QUERY_PATTERN) {
		CCCI_BOOTUP_LOG(md->index, KERN, "md_feature pattern is wrong: head 0x%x, tail 0x%x\n",
			     md_feature->head_pattern, md_feature->tail_pattern);
		if (md->index == MD_SYS3)
			md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
		return -1;
	}

	for (i = BOOT_INFO; i < FEATURE_COUNT; i++) {
		memset(&rt_feature, 0, sizeof(struct ccci_runtime_feature));
		memset(&rt_shm, 0, sizeof(struct ccci_runtime_share_memory));
		memset(&rt_f_element, 0, sizeof(struct ccci_misc_info_element));
		rt_feature.feature_id = i;
		if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_MUST_SUPPORT &&
		    md_feature_ap.feature_set[i].support_mask < CCCI_FEATURE_MUST_SUPPORT) {
			CCCI_BOOTUP_LOG(md->index, KERN, "feature %u not support for AP\n", rt_feature.feature_id);
			return -1;
		}

		CCCI_BOOTUP_DUMP_LOG(md->index, KERN, "ftr %u mask %u, ver %u\n",
				rt_feature.feature_id, md_feature->feature_set[i].support_mask,
				md_feature->feature_set[i].version);

		if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_NOT_EXIST) {
			rt_feature.support_info = md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_MUST_SUPPORT) {
			rt_feature.support_info = md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_OPTIONAL_SUPPORT) {
			if (md_feature->feature_set[i].version == md_feature_ap.feature_set[i].version &&
			    md_feature_ap.feature_set[i].support_mask >= CCCI_FEATURE_MUST_SUPPORT) {
				rt_feature.support_info.support_mask = CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask = CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			}
		} else if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT) {
			if (md_feature->feature_set[i].version >= md_feature_ap.feature_set[i].version) {
				rt_feature.support_info.support_mask = CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask = CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			}
		}

		if (rt_feature.support_info.support_mask == CCCI_FEATURE_MUST_SUPPORT) {
			switch (rt_feature.feature_id) {
			case BOOT_INFO:
				memset(&boot_info, 0, sizeof(boot_info));
				rt_feature.data_len = sizeof(boot_info);
				boot_info.boot_channel = CCCI_CONTROL_RX;
				boot_info.booting_start_id = get_booting_start_id(md);
				append_runtime_feature(&rt_data, &rt_feature, &boot_info);
				break;

			case EXCEPTION_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_exp_smem_base_phy -
				    md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_exp_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case CCIF_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_ccif_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_ccif_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case CCISM_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_ccism_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_ccism_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case CCB_SHARE_MEMORY:
				/* notice: we should add up all CCB region size here */
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_ccb_dhl_base_phy -
					md->mem_layout.smem_offset_AP_to_MD + 4; /* for 64bit alignment */
				rt_shm.size = md->smem_layout.ccci_ccb_dhl_size - 4;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case DHL_RAW_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_raw_dhl_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_raw_dhl_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case DT_NETD_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_dt_netd_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_dt_netd_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case DT_USB_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_dt_usb_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_dt_usb_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case SMART_LOGGING_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_smart_logging_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_smart_logging_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case MD1MD3_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->mem_layout.md1_md3_smem_phy - md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->mem_layout.md1_md3_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;

			case MISC_INFO_HIF_DMA_REMAP:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RTC_32K_LESS:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RANDOM_SEED_NUM:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				get_random_bytes(&random_seed, sizeof(int));
				rt_f_element.feature[0] = random_seed;
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_GPS_COCLOCK:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_SBP_ID:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				rt_f_element.feature[0] = md->sbp_code;
				if (md->config.load_type < modem_ultg)
					rt_f_element.feature[1] = 0;
				else
					rt_f_element.feature[1] = get_wm_bitmap_for_ubin();
				CCCI_BOOTUP_LOG(md->index, KERN, "sbp=0x%x,wmid[%d]\n",
					rt_f_element.feature[0], rt_f_element.feature[1]);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CCCI:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
#ifdef FEATURE_SEQ_CHECK_EN
				rt_f_element.feature[0] |= (1 << 0);
#endif
#ifdef FEATURE_POLL_MD_EN
				rt_f_element.feature[0] |= (1 << 1);
#endif
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CLIB_TIME:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				do_gettimeofday(&t);

				/*set seconds information */
				rt_f_element.feature[0] = ((unsigned int *)&t.tv_sec)[0];
				rt_f_element.feature[1] = ((unsigned int *)&t.tv_sec)[1];
				/*sys_tz.tz_minuteswest; */
				rt_f_element.feature[2] = current_time_zone;
				/*not used for now */
				rt_f_element.feature[3] = sys_tz.tz_dsttime;
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_C2K:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
#ifdef FEATURE_C2K_ALWAYS_ON
				c2k_flags = 0;

#if defined(CONFIG_MTK_MD3_SUPPORT) && (CONFIG_MTK_MD3_SUPPORT > 0)
				c2k_flags |= (1 << 0);
#endif

				if (ccci_get_opt_val("opt_c2k_lte_mode") == 1) /* SVLTE_MODE */
					c2k_flags |= (1 << 1);

				if (ccci_get_opt_val("opt_c2k_lte_mode") == 2) /* SRLTE_MODE */
					c2k_flags |= (1 << 2);

#ifdef CONFIG_MTK_C2K_OM_SOLUTION1
				c2k_flags |=  (1 << 3);
#endif
#ifdef CONFIG_CT6M_SUPPORT
				c2k_flags |= (1 << 4)
#endif
				rt_f_element.feature[0] = c2k_flags;
#endif
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MD_IMAGE_START_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->img_info[IMG_MD].address;
				rt_shm.size = md->img_info[IMG_MD].size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case EE_AFTER_EPOF:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case CCMNI_MTU:
				rt_feature.data_len = sizeof(unsigned int);
				random_seed = NET_RX_BUF - sizeof(struct ccci_header);
				append_runtime_feature(&rt_data, &rt_feature, &random_seed);
				break;
			case CCCI_FAST_HEADER:
				rt_feature.data_len = sizeof(unsigned int);
				random_seed = 1;
				append_runtime_feature(&rt_data, &rt_feature, &random_seed);
				break;
			default:
				break;
			};
		} else {
			rt_feature.data_len = 0;
			append_runtime_feature(&rt_data, &rt_feature, NULL);
		}

	}

	total_len = rt_data - (char *)md->smem_layout.ccci_rt_smem_base_vir;
	ccci_util_mem_dump(md->index, CCCI_DUMP_BOOTUP, md->smem_layout.ccci_rt_smem_base_vir, total_len);

	return 0;
}

struct ccci_runtime_feature *ccci_md_get_rt_feature_by_id(struct ccci_modem *md, u8 feature_id, u8 ap_query_md)
{
	struct ccci_runtime_feature *rt_feature = NULL;
	u8 i = 0;
	u8 max_id = 0;

	if (ap_query_md) {
		rt_feature = (struct ccci_runtime_feature *)(md->smem_layout.ccci_rt_smem_base_vir +
			CCCI_SMEM_SIZE_RUNTIME_AP);
		max_id = AP_RUNTIME_FEATURE_ID_MAX;
	} else {
		rt_feature = (struct ccci_runtime_feature *)(md->smem_layout.ccci_rt_smem_base_vir);
		max_id = MD_RUNTIME_FEATURE_ID_MAX;
	}
	while (i < max_id) {
		if (feature_id == rt_feature->feature_id)
			return rt_feature;
		/*todo: valid data len check*/
		if (rt_feature->data_len > sizeof(struct ccci_misc_info_element)) {
			CCCI_ERROR_LOG(md->index, KERN, "get invalid feature, id %u\n", i);
			return NULL;
		}
		rt_feature = (struct ccci_runtime_feature *) ((char *)rt_feature->data + rt_feature->data_len);
		i++;
	}

	return NULL;
}

int ccci_md_parse_rt_feature(struct ccci_modem *md, struct ccci_runtime_feature *rt_feature, void *data, u32 data_len)
{
	if (unlikely(!rt_feature)) {
		CCCI_ERROR_LOG(md->index, KERN, "parse_md_rt_feature: rt_feature == NULL\n");
		return -EFAULT;
	}
	if (unlikely(rt_feature->data_len > data_len || rt_feature->data_len == 0)) {
		CCCI_ERROR_LOG(md->index, KERN, "rt_feature %u data_len = %u, expected data_len %u\n",
			rt_feature->feature_id, rt_feature->data_len, data_len);
		return -EFAULT;
	}

	memcpy(data, (const void *)((char *)rt_feature->data), rt_feature->data_len);

	return 0;
}
static void ccci_md_dump_log_rec(struct ccci_modem *md, struct ccci_log *log)
{
	u64 ts_nsec = log->tv;
	unsigned long rem_nsec;

	if (ts_nsec == 0)
		return;
	rem_nsec = do_div(ts_nsec, 1000000000);
	if (!log->droped) {
		CCCI_MEM_LOG(md->index, CORE, "%08X %08X %08X %08X  %5lu.%06lu\n",
		       log->msg.data[0], log->msg.data[1], *(((u32 *)&log->msg) + 2),
		       log->msg.reserved, (unsigned long)ts_nsec, rem_nsec / 1000);
	} else {
		CCCI_MEM_LOG(md->index, CORE, "%08X %08X %08X %08X  %5lu.%06lu -\n",
		       log->msg.data[0], log->msg.data[1], *(((u32 *)&log->msg) + 2),
		       log->msg.reserved, (unsigned long)ts_nsec, rem_nsec / 1000);
	}
}

void ccci_md_add_log_history(struct ccci_modem *md, DIRECTION dir,
	int queue_index, struct ccci_header *msg, int is_droped)
{
#ifdef PACKET_HISTORY_DEPTH
	if (dir == OUT) {
		memcpy(&md->tx_history[queue_index][md->tx_history_ptr[queue_index]].msg, msg,
		       sizeof(struct ccci_header));
		md->tx_history[queue_index][md->tx_history_ptr[queue_index]].tv = local_clock();
		md->tx_history[queue_index][md->tx_history_ptr[queue_index]].droped = is_droped;
		md->tx_history_ptr[queue_index]++;
		md->tx_history_ptr[queue_index] &= (PACKET_HISTORY_DEPTH - 1);
	}
	if (dir == IN) {
		memcpy(&md->rx_history[queue_index][md->rx_history_ptr[queue_index]].msg, msg,
		       sizeof(struct ccci_header));
		md->rx_history[queue_index][md->rx_history_ptr[queue_index]].tv = local_clock();
		md->rx_history[queue_index][md->rx_history_ptr[queue_index]].droped = is_droped;
		md->rx_history_ptr[queue_index]++;
		md->rx_history_ptr[queue_index] &= (PACKET_HISTORY_DEPTH - 1);
	}
#endif
}

void ccci_md_dump_log_history(struct ccci_modem *md, int dump_multi_rec, int tx_queue_num, int rx_queue_num)
{
#ifdef PACKET_HISTORY_DEPTH
	int i, j;

	if (dump_multi_rec) {
		for (i = 0; i < ((tx_queue_num <= MAX_TXQ_NUM) ? tx_queue_num : MAX_TXQ_NUM); i++) {
			CCCI_MEM_LOG_TAG(md->index, CORE, "dump txq%d packet history, ptr=%d\n", i,
			       md->tx_history_ptr[i]);
			for (j = 0; j < PACKET_HISTORY_DEPTH; j++)
				ccci_md_dump_log_rec(md, &md->tx_history[i][j]);
		}
		for (i = 0; i < ((rx_queue_num <= MAX_RXQ_NUM) ? rx_queue_num : MAX_RXQ_NUM); i++) {
			CCCI_MEM_LOG_TAG(md->index, CORE, "dump rxq%d packet history, ptr=%d\n", i,
			       md->rx_history_ptr[i]);
			for (j = 0; j < PACKET_HISTORY_DEPTH; j++)
				ccci_md_dump_log_rec(md, &md->rx_history[i][j]);
		}
	} else {
		CCCI_MEM_LOG_TAG(md->index, CORE, "dump txq%d packet history, ptr=%d\n", tx_queue_num,
		       md->tx_history_ptr[tx_queue_num]);
		for (j = 0; j < PACKET_HISTORY_DEPTH; j++)
			ccci_md_dump_log_rec(md, &md->tx_history[tx_queue_num][j]);
		CCCI_MEM_LOG_TAG(md->index, CORE, "dump rxq%d packet history, ptr=%d\n", rx_queue_num,
		       md->rx_history_ptr[rx_queue_num]);
		for (j = 0; j < PACKET_HISTORY_DEPTH; j++)
			ccci_md_dump_log_rec(md, &md->rx_history[rx_queue_num][j]);
	}
#endif
}


