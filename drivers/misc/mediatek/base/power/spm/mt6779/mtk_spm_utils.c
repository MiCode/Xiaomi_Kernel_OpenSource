// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
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

void __attribute__ ((weak))
__iomem *get_smem_start_addr(int md_id, enum SMEM_USER_ID user_id,
	int *size_o)
{
	pr_info("%s not ready\n", __func__);
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
			pr_info("[SPM] WARNING: MD SLEEP = %d\n", md_sleep);
			pr_info("%s CAN NOT polling AP_MD1SRC_ACK\n",
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
			pr_info(
				"[SPM] warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n",
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
			pr_info(
				"[SPM ]warning: set = %d spm_ap_mdsrc_req_cnt = %d\n",
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

ssize_t get_spm_system_stats(char *ToUserBuf
		, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	u32 req_sta_0, req_sta_1, req_sta_4;
	u32 src_req;

#undef log
#define log(fmt, args...) ({\
		p += scnprintf(p, sz - strlen(ToUserBuf), fmt, ##args); p; })

	log("26M:%llx:%llx\n", spm_26M_off_count, spm_26M_off_duration);

	/* dump spm req_sta for who block system low power mode */
	log("System low power mode is blocked by ");
	req_sta_0 = spm_read(SRC_REQ_STA_0);
	if (req_sta_0 & 0x3fff)
		log("MD ");
	if (req_sta_0 & (0x3f << 14))
		log("CONN ");

	req_sta_1 = spm_read(SRC_REQ_STA_1);
	if (req_sta_1 & 0x3f)
		log("SCP ");
	if (req_sta_1 & (0x3f << 6))
		log("ADSP ");
	if (req_sta_1 & (0x3f << 12))
		log("UFS ");
	if (req_sta_1 & (0x3f << 18))
		log("DISP ");
	if (req_sta_1 & (0x1f << 24))
		log("GCE ");
	if (req_sta_1 & (0x7 << 29))
		log("INFRASYS ");

	req_sta_4 = spm_read(SRC_REQ_STA_4);
	if (req_sta_4 & (0x3ffff << 6))
		log("APU ");

	src_req = spm_read(SPM_SRC_REQ);
	if (src_req & 0x19B)
		log("SPM ");

	log("\n");

	return p - ToUserBuf;
}
EXPORT_SYMBOL(get_spm_system_stats);


#define MD_SLEEP_INFO_SMEM_OFFEST (14)
ssize_t get_spm_subsystem_stats(char *ToUserBuf
		, size_t sz, void *priv)
{
	/* dump subsystem sleep info */
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	u32 *share_mem = NULL;
	struct md_sleep_status data;
	u64 slp_cnt, slp_time;
	u32 read_cnt = 0;
	u32 len = 0;

	share_mem = (u32 *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_RAW_DBM, NULL);
	share_mem = share_mem - MD_SLEEP_INFO_SMEM_OFFEST;

	do {
		read_cnt++;
		memset(&data, 0, sizeof(struct md_sleep_status));
		memcpy(&data, share_mem, sizeof(struct md_sleep_status));
	} while ((data.slp_sleep_info1 != data.slp_sleep_info2)
		&& read_cnt <= 10);

	slp_cnt = (u64)data.slp_cnt_high << 32 | data.slp_cnt_low;
	slp_time = (u64)data.slp_sleep_time_high << 32
				| data.slp_sleep_time_low;

	len = snprintf(ToUserBuf, sz
			, "26M:%llx:%llx\nAP:%llx:%llx\nMD:%llx:%llx\n",
			spm_26M_off_count, spm_26M_off_duration,
			ap_pd_count, ap_slp_duration, slp_cnt, slp_time);

	return (len > sz) ? sz : len;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(get_spm_subsystem_stats);

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

	log("spmfw index: %d", spm_get_spmfw_idx());

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

