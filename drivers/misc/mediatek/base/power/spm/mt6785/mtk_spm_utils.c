/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <asm/setup.h>

#include <mtk_spm_internal.h>
#include <mtk_spm_suspend_internal.h>
#include <mt-plat/mtk_ccci_common.h>

#define CREATE_TRACE_POINTS
#include "mtk_spm_events.h"

void __attribute__ ((weak))
__iomem *get_smem_start_addr(int md_id, enum SMEM_USER_ID user_id,
	int *size_o)
{
	printk_deferred("[name:spm&][SPM] %s not ready\n", __func__);
	return 0;
}

static int local_spm_load_firmware_status = -1;
int spm_load_firmware_status(void)
{
	if (local_spm_load_firmware_status < 2)
		local_spm_load_firmware_status =
			SMC_CALL(FIRMWARE_STATUS, 0, 0, 0);
	/* -1 not init, 0: not loaded, 1: loaded, 2: loaded and kicked */
	return local_spm_load_firmware_status;
}

bool spm_is_md_sleep(void)
{
	return !((spm_read(PCM_REG13_DATA) & R13_MD_SRCCLKENA_0) |
		 (spm_read(PCM_REG13_DATA) & R13_MD_SRCCLKENA_1));
}
EXPORT_SYMBOL(spm_is_md_sleep);

bool spm_is_md1_sleep(void)
{
	return !(spm_read(PCM_REG13_DATA) & R13_MD_SRCCLKENA_0);
}
EXPORT_SYMBOL(spm_is_md1_sleep);

bool spm_is_md2_sleep(void)
{
	return !(spm_read(PCM_REG13_DATA) & R13_MD_SRCCLKENA_1);
}
EXPORT_SYMBOL(spm_is_md2_sleep);

bool spm_is_conn_sleep(void)
{
	return !(spm_read(PCM_REG13_DATA) & R13_CONN_SRCCLKENA);
}
EXPORT_SYMBOL(spm_is_conn_sleep);

static void check_ap_mdsrc_ack(void)
{
	u32 i = 0;
	u32 md_sleep = 0;

	/* if md_apsrc_req = 1'b0, wait 26M settling time (3ms) */
	if ((spm_read(PCM_REG13_DATA) & R13_MD_APSRC_REQ_0) == 0) {
		md_sleep = 1;
		mdelay(3);
	}

	/* Check ap_mdsrc_ack = 1'b1 */
	while ((spm_read(AP_MDSRC_REQ) & AP_MDSMSRC_ACK_LSB) == 0) {
		if (i++ < 10) {
			mdelay(1);
		} else {
			printk_deferred("[name:spm&][SPM] WARNING: MD SLEEP = %d\n",
				md_sleep);
			printk_deferred("[name:spm&][SPM] %s CAN NOT polling AP_MD1SRC_ACK\n",
				__func__);
			break;
		}
	}
}

void spm_ap_mdsrc_req(u8 set)
{
	unsigned long flags;

	if (set) {
		spin_lock_irqsave(&__spm_lock, flags);

		if (spm_ap_mdsrc_req_cnt < 0) {
			printk_deferred(
				"[name:spm&][SPM] warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n",
				set, spm_ap_mdsrc_req_cnt);
			spin_unlock_irqrestore(&__spm_lock, flags);
		} else {
			spm_ap_mdsrc_req_cnt++;

			SMC_CALL(AP_MDSRC_REQ, 1, 0, 0);

			spin_unlock_irqrestore(&__spm_lock, flags);

			check_ap_mdsrc_ack();
		}
	} else {
		spin_lock_irqsave(&__spm_lock, flags);

		spm_ap_mdsrc_req_cnt--;

		if (spm_ap_mdsrc_req_cnt < 0) {
			printk_deferred(
				"[name:spm&][SPM] ]warning: set = %d spm_ap_mdsrc_req_cnt = %d\n",
				set, spm_ap_mdsrc_req_cnt);
		} else {
			if (spm_ap_mdsrc_req_cnt == 0)
				SMC_CALL(AP_MDSRC_REQ, 0, 0, 0);
		}

		spin_unlock_irqrestore(&__spm_lock, flags);
	}
}
EXPORT_SYMBOL(spm_ap_mdsrc_req);

void spm_adsp_mem_protect(void)
{
	SMC_CALL(ARGS,
		 SPM_ARGS_SUSPEND_CALLBACK,
		 SUSPEND_CALLBACK_USER_ADSP,
		 SUSPEND_CALLBACK);
}
EXPORT_SYMBOL(spm_adsp_mem_protect);

void spm_adsp_mem_unprotect(void)
{
	SMC_CALL(ARGS,
		 SPM_ARGS_SUSPEND_CALLBACK,
		 SUSPEND_CALLBACK_USER_ADSP,
		 RESUME_CALLBACK);
}
EXPORT_SYMBOL(spm_adsp_mem_unprotect);

ssize_t get_spm_sleep_count(char *ToUserBuf
		, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz
				, "%d\n", spm_sleep_count);
	return (bLen > sz) ? sz : bLen;
}
EXPORT_SYMBOL(get_spm_sleep_count);

ssize_t get_spm_last_wakeup_src(char *ToUserBuf
		, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz
				, "0x%x\n", spm_wakesta.r12);
	return (bLen > sz) ? sz : bLen;
}
EXPORT_SYMBOL(get_spm_last_wakeup_src);

ssize_t get_spm_last_debug_flag(char *ToUserBuf
		, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz
				, "0x%x\n", spm_wakesta.debug_flag);
	return (bLen > sz) ? sz : bLen;
}
EXPORT_SYMBOL(get_spm_last_debug_flag);

ssize_t get_spmfw_version(char *ToUserBuf
		, size_t sz, void *priv)
{
	int index = 0;
	const char *version;
	char *p = ToUserBuf;

#undef log
#define log(fmt, args...) ({\
	p += scnprintf(p, sz - strlen(ToUserBuf), fmt, ##args); p; })

	struct device_node *node =
	of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node == NULL) {
		log("No Found mediatek,sleep\n");
		goto return_size;
	}

	while (!of_property_read_string_index(node,
		"spmfw_version", index, &version)) {
		log("%d: %s\n", index, version);
		index++;
	}

	log("spmfw index: %d\n", spm_get_spmfw_idx());

	if (node)
		of_node_put(node);
return_size:
	return p - ToUserBuf;
}
EXPORT_SYMBOL(get_spmfw_version);

void spm_output_sleep_option(void)
{
	printk_deferred("[name:spm&][SPM] PWAKE_EN:%d, PCMWDT_EN:%d, BYPASS_SYSPWREQ:%d\n",
		   SPM_PWAKE_EN, SPM_PCMWDT_EN, SPM_BYPASS_SYSPWREQ);
}
EXPORT_SYMBOL(spm_output_sleep_option);

/* record last wakesta */
u32 spm_get_last_wakeup_src(void)
{
	return spm_wakesta.r12;
}
EXPORT_SYMBOL(spm_get_last_wakeup_src);

u32 spm_get_last_wakeup_misc(void)
{
	return spm_wakesta.wake_misc;
}
EXPORT_SYMBOL(spm_get_last_wakeup_misc);

