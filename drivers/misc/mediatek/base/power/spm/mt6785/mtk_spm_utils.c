/*
 * Copyright (C) 2017 MediaTek Inc.
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

#define MD_SLEEP_INFO_SMEM_OFFEST (14)
u64 get_md_slp_duration(void)
{
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	static struct md_sleep_status md_data;
	u32 *share_mem = NULL;
	u64 slp_time;
	u32 read_cnt = 0;

	share_mem = (u32 *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_RAW_DBM, NULL);

	if (share_mem == NULL)
		return 0;

	share_mem = share_mem - MD_SLEEP_INFO_SMEM_OFFEST;

	do {
		read_cnt++;
		memset(&md_data, 0, sizeof(struct md_sleep_status));
		memcpy(&md_data, share_mem, sizeof(struct md_sleep_status));
	} while ((md_data.slp_sleep_info1 != md_data.slp_sleep_info2)
		&& read_cnt <= 10);

	slp_time = (u64)md_data.slp_sleep_time_high << 32
				| md_data.slp_sleep_time_low;

	return slp_time;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(get_md_slp_duration);

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

ssize_t get_spm_subsystem_stats(char *ToUserBuf
		, size_t sz, void *priv)
{
	/* dump subsystem sleep info */
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	static struct md_sleep_status data;
	u32 *share_mem = NULL;
	u64 slp_cnt, slp_time;
	u32 read_cnt = 0;
	u32 len = 0;

	share_mem = (u32 *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_RAW_DBM, NULL);

	if (share_mem == NULL)
		return 0;

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

	log("spmfw index: %d\n", spm_get_spmfw_idx());

	if (node)
		of_node_put(node);
return_size:
	return p - ToUserBuf;
}
EXPORT_SYMBOL(get_spmfw_version);

struct timer_list spm_lp_ratio_timer;
struct timer_list spm_resource_req_timer;
u64 current_spm_26M_slp_duration;
u64 current_ap_slp_duration;
u64 current_md_slp_duration;

u64 before_spm_26M_slp_duration;
u64 before_ap_slp_duration;
u64 before_md_slp_duration;
ktime_t before_ktime;
ktime_t current_ktime;

u32 spm_lp_ratio_timer_is_enabled;
u32 spm_lp_ratio_timer_ms;

u32 spm_resource_req_timer_is_enabled;
u32 spm_resource_req_timer_ms;
static void spm_lp_ratio_timer_fn(unsigned long data)
{
	unsigned int spm_26M_ratio, ap_ratio, md_ratio;
	u64 usecs64;
	int usecs;

	current_ktime = ktime_get();
	usecs64 = ktime_to_ns(ktime_sub(current_ktime, before_ktime));
	do_div(usecs64, NSEC_PER_USEC);
	usecs = usecs64;

	/* get current idle ratio */
	current_spm_26M_slp_duration = spm_26M_off_duration;
	current_ap_slp_duration = ap_slp_duration;
	current_md_slp_duration = get_md_slp_duration();

	spm_26M_ratio =
		(current_spm_26M_slp_duration - before_spm_26M_slp_duration)
		*100*1000000/usecs/PCM_32K_TICKS_PER_SEC;
	ap_ratio = (current_ap_slp_duration - before_ap_slp_duration)
		*100*1000000/usecs/PCM_32K_TICKS_PER_SEC;
	md_ratio = (current_md_slp_duration - before_md_slp_duration)
		*100*1000000/usecs/PCM_32K_TICKS_PER_SEC;

	if (md_ratio > 100)
		md_ratio = 100;

	trace_SPM__lp_ratio_0(
		spm_26M_ratio, ap_ratio,
		md_ratio);

	/* backup current idle ratio as before */
	before_spm_26M_slp_duration = current_spm_26M_slp_duration;
	before_ap_slp_duration = current_ap_slp_duration;
	before_md_slp_duration = current_md_slp_duration;
	before_ktime = current_ktime;

	spm_lp_ratio_timer.expires = jiffies +
		msecs_to_jiffies(spm_lp_ratio_timer_ms);
	add_timer(&spm_lp_ratio_timer);
}

static void spm_resource_req_timer_fn(unsigned long data)
{
	u32 req_sta_0, req_sta_1, req_sta_4;
	u32 src_req;
	u32 md = 0, conn = 0, scp = 0, adsp = 0;
	u32 ufs = 0, disp = 0, apu = 0, spm = 0;

	req_sta_0 = spm_read(SRC_REQ_STA_0);
	if (req_sta_0 & 0x3fff)
		md = 1;
	if (req_sta_0 & (0x3f << 14))
		conn = 1;

	req_sta_1 = spm_read(SRC_REQ_STA_1);
	if (req_sta_1 & 0x3f)
		scp = 1;
	if (req_sta_1 & (0x3f << 6))
		adsp = 1;
	if (req_sta_1 & (0x3f << 12))
		ufs = 1;
	if (req_sta_1 & (0x3f << 18))
		disp = 1;

	req_sta_4 = spm_read(SRC_REQ_STA_4);
	if (req_sta_4 & (0x3ffff << 6))
		apu = 1;

	src_req = spm_read(SPM_SRC_REQ);
	if (src_req & 0x19B)
		spm = 1;

	trace_SPM__resource_req_0(
		md, conn,
		scp, adsp,
		ufs, disp,
		apu, spm);

	spm_resource_req_timer.expires = jiffies +
		msecs_to_jiffies(spm_resource_req_timer_ms);
	add_timer(&spm_resource_req_timer);
}

static void spm_lp_ratio_timer_en(u32 enable, u32 timer_ms)
{
	if (enable) {
		/* if spm lp ratio timer doesn't init */
		if (spm_lp_ratio_timer.function == NULL) {
			init_timer(&spm_lp_ratio_timer);
			spm_lp_ratio_timer.function = spm_lp_ratio_timer_fn;
			spm_lp_ratio_timer.data = 0;
			spm_lp_ratio_timer_is_enabled = false;
		}

		if (spm_lp_ratio_timer_is_enabled)
			return;

		spm_lp_ratio_timer_ms = timer_ms;

		spm_lp_ratio_timer.expires = jiffies +
			msecs_to_jiffies(spm_lp_ratio_timer_ms);
		add_timer(&spm_lp_ratio_timer);
		spm_lp_ratio_timer_is_enabled = true;
	} else if (spm_lp_ratio_timer_is_enabled) {
		del_timer(&spm_lp_ratio_timer);
		spm_lp_ratio_timer_is_enabled = false;
	}
}

static void spm_resource_req_timer_en(u32 enable, u32 timer_ms)
{
	if (enable) {
		/* if spm resource request timer doesn't init */
		if (spm_resource_req_timer.function == NULL) {
			init_timer(&spm_resource_req_timer);
			spm_resource_req_timer.function =
				spm_resource_req_timer_fn;
			spm_resource_req_timer.data = 0;
			spm_resource_req_timer_is_enabled = false;
		}

		if (spm_resource_req_timer_is_enabled)
			return;

		spm_resource_req_timer_ms = timer_ms;
		spm_resource_req_timer.expires = jiffies +
			msecs_to_jiffies(spm_resource_req_timer_ms);
		add_timer(&spm_resource_req_timer);
		spm_resource_req_timer_is_enabled = true;
	} else if (spm_resource_req_timer_is_enabled) {
		del_timer(&spm_resource_req_timer);
		spm_resource_req_timer_is_enabled = false;
	}
}

ssize_t get_spm_lp_ratio_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz
				, "spm idle ratio timer is enabled: %d\n",
				spm_lp_ratio_timer_is_enabled);
	return (bLen > sz) ? sz : bLen;
}

ssize_t set_spm_lp_ratio_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	u32 is_enable;
	u32 timer_ms;

	if (!ToUserBuf)
		return -EINVAL;

	if (sscanf(ToUserBuf, "%d %d", &is_enable, &timer_ms) == 2) {
		spm_lp_ratio_timer_en(is_enable, timer_ms);
		return sz;
	}

	return -EINVAL;
}

ssize_t get_spm_resource_req_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz
				, "spm resource request timer is enabled: %d\n",
				spm_resource_req_timer_is_enabled);
	return (bLen > sz) ? sz : bLen;
}

ssize_t set_spm_resource_req_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	u32 is_enable;
	u32 timer_ms;

	if (!ToUserBuf)
		return -EINVAL;

	if (sscanf(ToUserBuf, "%d %d", &is_enable, &timer_ms) == 2) {
		spm_resource_req_timer_en(is_enable, timer_ms);
		return sz;
	}

	return -EINVAL;
}

u64 before_rx_bytes;
u64 before_rx_packets;
u64 before_tx_bytes;
u64 before_tx_packets;
ssize_t set_network_traffic(char *ToUserBuf
		, size_t sz, void *priv)
{
	u64 rx_bytes;
	u64 rx_packets;
	u64 tx_bytes;
	u64 tx_packets;

	if (!ToUserBuf)
		return -EINVAL;

	if (sscanf(ToUserBuf, "%lld %lld %lld %lld", &rx_bytes,
			&rx_packets, &tx_bytes, &tx_packets) == 4) {

		trace_Network__traffic_0(
		rx_bytes - before_rx_bytes, rx_packets - before_rx_packets,
		tx_bytes - before_tx_bytes, tx_packets - before_tx_packets);

		before_rx_bytes = rx_bytes;
		before_rx_packets = rx_packets;
		before_tx_bytes = tx_bytes;
		before_tx_packets = tx_packets;
		return sz;
	}

	return -EINVAL;
}

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

