/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/msm_audio.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/memory_alloc.h>
#include <linux/mfd/marimba.h>
#include <mach/dal.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/socinfo.h>
#include <mach/qdsp5/qdsp5audpp.h>
#include <mach/qdsp5/qdsp5audpreproc.h>
#include <mach/qdsp5/qdsp5audppcmdi.h>
#include <mach/qdsp5/qdsp5audpreproccmdi.h>
#include <mach/qdsp5/qdsp5audpreprocmsg.h>
#include <mach/qdsp5/qdsp5audppmsg.h>
#include <mach/qdsp5/audio_acdbi.h>
#include <mach/qdsp5/acdb_commands.h>
#include <mach/qdsp5/audio_acdb_def.h>
#include <mach/debug_mm.h>
#include <mach/msm_memtypes.h>

#include "audmgr.h"

/* this is the ACDB device ID */
#define DALDEVICEID_ACDB		0x02000069
#define ACDB_PORT_NAME			"DAL00"
#define ACDB_CPU			SMD_APPS_MODEM
#define ACDB_BUF_SIZE			4096
#define FLUENCE_BUF_SIZE	498

#define ACDB_VALUES_NOT_FILLED		0
#define ACDB_VALUES_FILLED		1
#define MAX_RETRY			10

#define COMMON_OBJ_ID                   6

/*below macro is used to align the session info received from
Devctl driver with the state mentioned as not to alter the
Existing code*/
#define AUDREC_OFFSET	2
/* rpc table index */
enum {
	ACDB_DAL_IOCTL = DALDEVICE_FIRST_DEVICE_API_IDX
};

enum {
	CAL_DATA_READY	= 0x1,
	AUDPP_READY	= 0x2,
	AUDREC_READY	= 0x4,
};

struct acdb_data {
	void *handle;

	u32 phys_addr;
	u8 *virt_addr;

	struct task_struct *cb_thread_task;
	struct device_info_callback dev_cb;

	u32 acdb_state;
	struct audpp_event_callback audpp_cb;
	struct audpreproc_event_callback audpreproc_cb;
	struct dev_evt_msg *device_info;

	audpp_cmd_cfg_object_params_pcm *pp_iir;
	audpp_cmd_cfg_object_params_mbadrc *pp_mbadrc;
	audpreproc_cmd_cfg_agc_params *preproc_agc;
	audpreproc_cmd_cfg_iir_tuning_filter_params *preproc_iir;
	audpreproc_cmd_cfg_ns_params *preproc_ns;
	struct acdb_mbadrc_block mbadrc_block;

	wait_queue_head_t wait;
	struct mutex acdb_mutex;
	u32 device_cb_compl;
	u32 audpp_cb_compl;
	u32 preproc_cb_compl;
	u32 audpp_cb_reenable_compl;
	u8 preproc_stream_id;
	u8 audrec_applied;
	u32 multiple_sessions;
	u32 cur_tx_session;
	struct acdb_result acdb_result;
	uint32_t audpp_disabled_features;

	spinlock_t dsp_lock;
	int dec_id;
	audpp_cmd_cfg_object_params_eqalizer eq;
	struct audrec_session_info session_info;
	/*pmem info*/
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long pmem_len;
	struct file *file;
	/* pmem for get acdb blk */
	unsigned long	get_blk_paddr;
	u8		*get_blk_kvaddr;
	void *map_v_get_blk;
};

static struct acdb_data		acdb_data;

struct acdb_cache_node {
	u32 node_status;
	s32 stream_id;
	u32 phys_addr_acdb_values;
	void *map_v_addr;
	u8 *virt_addr_acdb_values;
	struct dev_evt_msg device_info;
};

struct acdb_cache_node acdb_cache_rx;

/*for TX devices acdb values are applied based on AUDREC session and
the depth of the tx cache is define by number of AUDREC sessions supported*/
struct acdb_cache_node acdb_cache_tx;

/*Audrec session info includes Attributes Sampling frequency and enc_id */
struct audrec_session_info session_info;
#ifdef CONFIG_DEBUG_FS

#define RTC_MAX_TIMEOUT 500 /* 500 ms */
#define PMEM_RTC_ACDB_QUERY_MEM 4096
#define EXTRACT_HIGH_WORD(x) ((x & 0xFFFF0000)>>16)
#define EXTRACT_LOW_WORD(x) (0x0000FFFF & x)
#define	ACDB_RTC_TX 0xF1
#define	ACDB_RTC_RX 0x1F


static u32 acdb_audpp_entry[][4] = {

	{
	ABID_AUDIO_RTC_VOLUME_PAN_RX,\
	IID_AUDIO_RTC_VOLUME_PAN_PARAMETERS,\
	AUDPP_CMD_VOLUME_PAN,\
	ACDB_RTC_RX
	},
	{
	ABID_AUDIO_IIR_RX,\
	IID_AUDIO_IIR_COEFF,\
	AUDPP_CMD_IIR_TUNING_FILTER,
	ACDB_RTC_RX
	},
	{
	ABID_AUDIO_RTC_EQUALIZER_PARAMETERS,\
	IID_AUDIO_RTC_EQUALIZER_PARAMETERS,\
	AUDPP_CMD_EQUALIZER,\
	ACDB_RTC_RX
	},
	{
	ABID_AUDIO_RTC_SPA,\
	IID_AUDIO_RTC_SPA_PARAMETERS,\
	AUDPP_CMD_SPECTROGRAM,
	ACDB_RTC_RX
	},
	{
	ABID_AUDIO_STF_RX,\
	IID_AUDIO_IIR_COEFF,\
	AUDPP_CMD_SIDECHAIN_TUNING_FILTER,\
	ACDB_RTC_RX
	},
	{
	ABID_AUDIO_MBADRC_RX,\
	IID_AUDIO_RTC_MBADRC_PARAMETERS,\
	AUDPP_CMD_MBADRC,\
	ACDB_RTC_RX
	},
	{
	ABID_AUDIO_AGC_TX,\
	IID_AUDIO_AGC_PARAMETERS,\
	AUDPREPROC_CMD_CFG_AGC_PARAMS,\
	ACDB_RTC_TX
	},
	{
	ABID_AUDIO_AGC_TX,\
	IID_AUDIO_RTC_AGC_PARAMETERS,\
	AUDPREPROC_CMD_CFG_AGC_PARAMS,\
	ACDB_RTC_TX
	},
	{
	ABID_AUDIO_NS_TX,\
	IID_NS_PARAMETERS,\
	AUDPREPROC_CMD_CFG_NS_PARAMS,\
	ACDB_RTC_TX
	},
	{
	ABID_AUDIO_IIR_TX,\
	IID_AUDIO_RTC_TX_IIR_COEFF,\
	AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS,\
	ACDB_RTC_TX
	},
	{
	ABID_AUDIO_IIR_TX,\
	IID_AUDIO_IIR_COEFF,\
	AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS,\
	ACDB_RTC_TX
	}
 /*Any new entries should be added here*/
};

static struct dentry *get_set_abid_dentry;
static struct dentry *get_set_abid_data_dentry;

struct rtc_acdb_pmem {
	u8 *viraddr;
	int32_t phys;
	void *map_v_rtc;
};

struct rtc_acdb_data {
	u32 acdb_id;
	u32 cmd_id;
	u32 set_abid;
	u32 set_iid;
	u32 abid;
	u32 err;
	bool valid_abid;
	u32 tx_rx_ctl;
	struct rtc_acdb_pmem rtc_read;
	struct rtc_acdb_pmem rtc_write;
	wait_queue_head_t  wait;
};

struct get_abid {
	u32	cmd_id;
	u32	acdb_id;
	u32	set_abid;
	u32	set_iid;
};

struct acdb_block_mbadrc_rtc {
	u16 enable;
	u16 num_bands;
	u16 down_samp_level;
	u16 adrc_delay;
	u16 ext_buf_size;
	u16 ext_partition;
	u16 ext_buf_msw;
	u16 ext_buf_lsw;
	struct adrc_config adrc_band[AUDPP_MAX_MBADRC_BANDS];
	signed int ext_buff[196];
} __packed;

enum {
	ACDB_RTC_SUCCESS,
	ACDB_RTC_ERR_INVALID_DEVICE,
	ACDB_RTC_ERR_DEVICE_INACTIVE,
	ACDB_RTC_ERR_INVALID_ABID,
	ACDB_RTC_DSP_FAILURE,
	ACDB_RTC_DSP_FEATURE_NOT_AVAILABLE,
	ACDB_RTC_ERR_INVALID_LEN,
	ACDB_RTC_ERR_UNKNOWN_FAILURE,
	ACDB_RTC_PENDING_RESPONSE,
	ACDB_RTC_INIT_FAILURE,
};

static  struct rtc_acdb_data rtc_acdb;

static int rtc_getsetabid_dbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	MM_DBG("GET-SET ABID Open debug intf %s\n",\
			(char *) file->private_data);
	return 0;
}

static bool get_feature_id(u32 set_abid, u32 iid, unsigned short *feature_id)
{
	bool ret_value = false;
	int i = 0;

	for (; i < (sizeof(acdb_audpp_entry) / sizeof(acdb_audpp_entry[0]));\
		i++) {
		if (acdb_audpp_entry[i][0] == set_abid &&
			acdb_audpp_entry[i][1] == iid) {
			*feature_id =  acdb_audpp_entry[i][2];
			rtc_acdb.tx_rx_ctl = acdb_audpp_entry[i][3];
			ret_value = true;
			break;
		}
	}
	return ret_value;
}
static ssize_t rtc_getsetabid_dbg_write(struct file *filp,
					const char __user *ubuf,
					size_t cnt, loff_t *ppos)
{
	struct  get_abid write_abid;
	unsigned short feat_id = 0;
	rtc_acdb.valid_abid = false;

	if (copy_from_user(&write_abid, \
		(void *)ubuf, sizeof(struct get_abid))) {
		MM_ERR("ACDB DATA WRITE - INVALID READ LEN\n");
		rtc_acdb.err = ACDB_RTC_ERR_INVALID_LEN;
		return cnt;
	}
	MM_DBG("SET ABID : Cmd ID: %d Device:%d ABID:%d IID : %d cnt: %d\n",\
		write_abid.cmd_id, write_abid.acdb_id,\
		write_abid.set_abid, write_abid.set_iid, cnt);
	if (write_abid.acdb_id > ACDB_ID_MAX ||
		write_abid.acdb_id < ACDB_ID_HANDSET_SPKR){
		rtc_acdb.err = ACDB_RTC_ERR_INVALID_DEVICE;
		return cnt;
	}

	rtc_acdb.err = ACDB_RTC_ERR_INVALID_ABID;
	rtc_acdb.abid = write_abid.set_abid;
	if (get_feature_id(write_abid.set_abid, \
		write_abid.set_iid, &feat_id)) {
		rtc_acdb.err = ACDB_RTC_SUCCESS;
		rtc_acdb.cmd_id = write_abid.cmd_id;
		rtc_acdb.acdb_id = write_abid.acdb_id;
		rtc_acdb.set_abid = feat_id;
		rtc_acdb.valid_abid = true;
		rtc_acdb.set_iid = write_abid.set_iid;
	}
	return cnt;
}
static ssize_t	rtc_getsetabid_dbg_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	static char buffer[1024];
	int n = 0;
	u32 msg = rtc_acdb.err;
	memcpy(buffer, &rtc_acdb.cmd_id, sizeof(struct get_abid));
	memcpy(buffer+16, &msg, 4);
	n = 20;
	MM_INFO("SET ABID : Cmd ID: %x Device:%x ABID:%x IID : %x Err: %d\n",\
		rtc_acdb.cmd_id, rtc_acdb.acdb_id, rtc_acdb.set_abid,\
		rtc_acdb.set_iid, rtc_acdb.err);
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static int rtc_getsetabid_data_dbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	MM_INFO("GET-SET ABID DATA Open debug intf %s\n",
		(char *) file->private_data);
	return 0;
}

void acdb_rtc_set_err(u32 err_code)
{
	if (rtc_acdb.err == ACDB_RTC_PENDING_RESPONSE) {
		if (err_code == 0xFFFF) {
			rtc_acdb.err = ACDB_RTC_SUCCESS;
			MM_INFO("RTC READ SUCCESS---\n");
		} else if (err_code == 0) {
			rtc_acdb.err = ACDB_RTC_DSP_FAILURE;
			MM_INFO("RTC READ FAIL---\n");
		} else if (err_code == 1) {
			rtc_acdb.err = ACDB_RTC_DSP_FEATURE_NOT_AVAILABLE;
			MM_INFO("RTC READ FEAT UNAVAILABLE---\n");
		} else {
			rtc_acdb.err = ACDB_RTC_DSP_FAILURE;
			MM_INFO("RTC Err CODE---\n");
		}
	} else {
		rtc_acdb.err = ACDB_RTC_DSP_FAILURE;
		MM_ERR("RTC Err code Invalid State\n");
	}
	wake_up(&rtc_acdb.wait);
}

static ssize_t	rtc_getsetabid_data_dbg_read(struct file *file,
					char __user *buf, size_t count,
					loff_t *ppos)
{
	static char buffer[PMEM_RTC_ACDB_QUERY_MEM];
	int rc, n = 0;
	int counter = 0;
	struct rtc_acdb_pmem *rtc_read = &rtc_acdb.rtc_read;
	memset(&buffer, 0, PMEM_RTC_ACDB_QUERY_MEM);

	if (rtc_acdb.valid_abid != true) {
		MM_ERR("ACDB DATA READ ---INVALID ABID\n");
		n = 0;
		rtc_acdb.err = ACDB_RTC_ERR_INVALID_ABID;
	} else {
		if (PMEM_RTC_ACDB_QUERY_MEM < count) {
			MM_ERR("ACDB DATA READ ---"\
				"INVALID READ LEN %x\n", count);
			n = 0;
			rtc_acdb.err = ACDB_RTC_ERR_INVALID_LEN;
		} else {
			rtc_acdb.err = ACDB_RTC_PENDING_RESPONSE;
			if (rtc_read->viraddr != NULL) {
				memset(rtc_read->viraddr,
					0, PMEM_RTC_ACDB_QUERY_MEM);
			}
			if (rtc_acdb.tx_rx_ctl == ACDB_RTC_RX) {
				struct rtc_audpp_read_data rtc_read_cmd;
				rtc_read_cmd.cmd_id =
					AUDPP_CMD_PP_FEAT_QUERY_PARAMS;
				rtc_read_cmd.obj_id =
					AUDPP_CMD_COPP_STREAM;
				rtc_read_cmd.feature_id = rtc_acdb.set_abid;
				rtc_read_cmd.extbufsizemsw =
					EXTRACT_HIGH_WORD(\
						PMEM_RTC_ACDB_QUERY_MEM);
				rtc_read_cmd.extbufsizelsw =
					EXTRACT_LOW_WORD(\
						PMEM_RTC_ACDB_QUERY_MEM);
				rtc_read_cmd.extpart = 0x0000;
				rtc_read_cmd.extbufstartmsw =
					EXTRACT_HIGH_WORD(rtc_read->phys);
				rtc_read_cmd.extbufstartlsw =
					EXTRACT_LOW_WORD(rtc_read->phys);
				rc = audpp_send_queue2(&rtc_read_cmd,
						sizeof(rtc_read_cmd));
			} else if (rtc_acdb.tx_rx_ctl == ACDB_RTC_TX) {
				struct rtc_audpreproc_read_data rtc_audpreproc;
				rtc_audpreproc.cmd_id =
					AUDPREPROC_CMD_FEAT_QUERY_PARAMS;
				rtc_audpreproc.feature_id = rtc_acdb.set_abid;
				 /*AUDREC1 is used for pcm recording */
				rtc_audpreproc.stream_id = 1;
				rtc_audpreproc.extbufsizemsw =
					EXTRACT_HIGH_WORD(\
						PMEM_RTC_ACDB_QUERY_MEM);
				rtc_audpreproc.extbufsizelsw =
					EXTRACT_LOW_WORD(\
						PMEM_RTC_ACDB_QUERY_MEM);
				rtc_audpreproc.extpart = 0x0000;
				rtc_audpreproc.extbufstartmsw =
					EXTRACT_HIGH_WORD(rtc_read->phys);
				rtc_audpreproc.extbufstartlsw =
					EXTRACT_LOW_WORD(rtc_read->phys);
				rc =  audpreproc_send_preproccmdqueue(
						&rtc_audpreproc,\
						sizeof(rtc_audpreproc));
				MM_INFO("ACDB READ Command RC --->%x,"\
					"stream_id %x\n", rc,
					acdb_data.preproc_stream_id);
			}
		rc = wait_event_timeout(rtc_acdb.wait,
					(rtc_acdb.err !=
					ACDB_RTC_PENDING_RESPONSE),
					msecs_to_jiffies(RTC_MAX_TIMEOUT));
		MM_INFO("ACDB READ ACK Count = %x Err = %x\n",
			count, rtc_acdb.err);
		{
			if (rtc_acdb.err == ACDB_RTC_SUCCESS
				&& rtc_read->viraddr != NULL) {
				memcpy(buffer, rtc_read->viraddr, count);
				n = count;
				while (counter < count) {
					MM_DBG("%x", \
						rtc_read->viraddr[counter]);
					counter++;
					}
				}
		}
	}
	}
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static bool acdb_set_tx_rtc(const char *ubuf, size_t writecount)
{
	audpreproc_cmd_cfg_iir_tuning_filter_params *preproc_iir;
	audpreproc_cmd_cfg_agc_params *preproc_agc;
	audpreproc_cmd_cfg_ns_params *preproc_ns;
	s32	result = 0;
	bool retval = false;
	unsigned short iircmdsize =
		sizeof(audpreproc_cmd_cfg_iir_tuning_filter_params);
	unsigned short iircmdid = AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS;

	rtc_acdb.err = ACDB_RTC_ERR_UNKNOWN_FAILURE;

	switch (rtc_acdb.set_abid) {

	case AUDPREPROC_CMD_CFG_AGC_PARAMS:
	{
		preproc_agc = kmalloc(sizeof(\
					audpreproc_cmd_cfg_agc_params),\
					GFP_KERNEL);
		if ((sizeof(audpreproc_cmd_cfg_agc_params) -\
			(sizeof(unsigned short)))
			< writecount) {
				MM_ERR("ACDB DATA WRITE --"\
					"AGC TX writecount > DSP struct\n");
		} else {
			if (preproc_agc != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)preproc_agc;
				offset = offsetof(\
					audpreproc_cmd_cfg_agc_params,\
						tx_agc_param_mask);
				offset_addr = (unsigned short *)(base + offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					preproc_agc->cmd_id =
						AUDPREPROC_CMD_CFG_AGC_PARAMS;

					result = audpreproc_dsp_set_agc(
						preproc_agc,
						sizeof(\
						audpreproc_cmd_cfg_agc_params));
					if (result) {
						MM_ERR("ACDB=> Failed to "\
							"send AGC data to "\
							"preproc)\n");
					} else {
						retval = true;
					       }
				} else {
					MM_ERR("ACDB DATA WRITE ---"\
						"GC Tx copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --"\
					"AGC TX kalloc Failed LEN\n");
			}
		}
		if (preproc_agc != NULL)
			kfree(preproc_agc);
		break;
	}
	case AUDPREPROC_CMD_CFG_NS_PARAMS:
	{

		preproc_ns = kmalloc(sizeof(\
					audpreproc_cmd_cfg_ns_params),\
					GFP_KERNEL);
		if ((sizeof(audpreproc_cmd_cfg_ns_params) -\
				(sizeof(unsigned short)))
				< writecount) {
				MM_ERR("ACDB DATA WRITE --"\
					"NS TX writecount > DSP struct\n");
		} else {
			if (preproc_ns != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)preproc_ns;
				offset = offsetof(\
						audpreproc_cmd_cfg_ns_params,\
						ec_mode_new);
				offset_addr = (unsigned short *)(base + offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					preproc_ns->cmd_id =
						AUDPREPROC_CMD_CFG_NS_PARAMS;
					result = audpreproc_dsp_set_ns(
						preproc_ns,
						sizeof(\
						audpreproc_cmd_cfg_ns_params));
					if (result) {
						MM_ERR("ACDB=> Failed to send "\
							"NS data to preproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---NS Tx "\
						"copy_from_user Fail\n");
					}
			} else {
				MM_ERR("ACDB DATA WRITE --NS TX "\
					"kalloc Failed LEN\n");
			}
		}
		if (preproc_ns != NULL)
			kfree(preproc_ns);
		break;
	}
	case AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS:
	{

		preproc_iir = kmalloc(sizeof(\
				audpreproc_cmd_cfg_iir_tuning_filter_params),\
				GFP_KERNEL);
		if ((sizeof(\
			audpreproc_cmd_cfg_iir_tuning_filter_params)-\
			(sizeof(unsigned short)))
			< writecount) {
			MM_ERR("ACDB DATA WRITE --IIR TX writecount "\
						"> DSP struct\n");
		} else {
			if (preproc_iir != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)preproc_iir;
				offset = offsetof(\
				audpreproc_cmd_cfg_iir_tuning_filter_params,\
				active_flag);
				offset_addr = (unsigned short *)(base + \
						offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					preproc_iir->cmd_id = iircmdid;
					result = audpreproc_dsp_set_iir(\
							preproc_iir,
							iircmdsize);
					if (result) {
						MM_ERR("ACDB=> Failed to send "\
						"IIR data to preproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---IIR Tx "\
						"copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --IIR TX kalloc "\
					"Failed LEN\n");
		     }
		}
		if (preproc_iir != NULL)
			kfree(preproc_iir);
		break;
	}
	}
	return retval;
}

static bool acdb_set_rx_rtc(const char *ubuf, size_t writecount)
{

	audpp_cmd_cfg_object_params_volume *volpan_config;
	audpp_cmd_cfg_object_params_mbadrc *mbadrc_config;
	struct acdb_block_mbadrc_rtc *acdb_mbadrc_rtc;
	audpp_cmd_cfg_object_params_eqalizer *eq_config;
	audpp_cmd_cfg_object_params_pcm *iir_config;
	struct rtc_acdb_pmem *rtc_write = &rtc_acdb.rtc_write;
	s32	result = 0;
	bool retval = false;

	switch (rtc_acdb.set_abid) {
	case AUDPP_CMD_VOLUME_PAN:
	{
		volpan_config =  kmalloc(sizeof(\
					 audpp_cmd_cfg_object_params_volume),\
					 GFP_KERNEL);
		if ((sizeof(audpp_cmd_cfg_object_params_volume) -\
			sizeof(audpp_cmd_cfg_object_params_common))
			< writecount) {
			MM_ERR("ACDB DATA WRITE -- "\
				"VolPan writecount > DSP struct\n");
		} else {
			if (volpan_config != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)volpan_config;
				offset = offsetof(\
					audpp_cmd_cfg_object_params_volume,\
					volume);
				offset_addr = (unsigned short *)(base+offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					MM_ERR("ACDB RX WRITE DATA: "\
						"AUDPP_CMD_VOLUME_PAN\n");
					result = audpp_set_volume_and_pan(
						COMMON_OBJ_ID,
						volpan_config->volume,
						volpan_config->pan);
					if (result) {
						MM_ERR("ACDB=> Failed to "\
							"send VOLPAN data to"
							" postproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---"\
						"copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --"\
					"Vol Pan kalloc Failed LEN\n");
			}
		}
	if (volpan_config != NULL)
		kfree(volpan_config);
	break;
	}

	case AUDPP_CMD_IIR_TUNING_FILTER:
	{
		iir_config =  kmalloc(sizeof(\
				audpp_cmd_cfg_object_params_pcm),\
				GFP_KERNEL);
		if ((sizeof(audpp_cmd_cfg_object_params_pcm) -\
			sizeof(audpp_cmd_cfg_object_params_common))
			< writecount) {
			MM_ERR("ACDB DATA WRITE --"\
					"IIR RX writecount > DSP struct\n");
		} else {
			if (iir_config != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)iir_config;
				offset = offsetof(\
					audpp_cmd_cfg_object_params_pcm,\
					active_flag);
				offset_addr = (unsigned short *)(base+offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					MM_ERR("ACDB RX WRITE DATA:"\
					"AUDPP_CMD_IIR_TUNING_FILTER\n");
					result = audpp_dsp_set_rx_iir(
						COMMON_OBJ_ID,
						iir_config->active_flag,\
						iir_config);
					if (result) {
						MM_ERR("ACDB=> Failed to send"\
							"IIR data to"\
							"postproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---"\
						"IIR Rx copy_from_user Fail\n");
				      }
			 } else {
				MM_ERR("ACDB DATA WRITE --"\
					"acdb_iir_block kalloc Failed LEN\n");
			}
		}
		if (iir_config != NULL)
			kfree(iir_config);
		break;
	}
	case AUDPP_CMD_EQUALIZER:
	{
		eq_config =  kmalloc(sizeof(\
				audpp_cmd_cfg_object_params_eqalizer),\
				GFP_KERNEL);
	if ((sizeof(audpp_cmd_cfg_object_params_eqalizer) -\
			sizeof(audpp_cmd_cfg_object_params_common))
			< writecount) {
			MM_ERR("ACDB DATA WRITE --"\
			"EQ RX writecount > DSP struct\n");
		} else {
			if (eq_config != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)eq_config;
				offset = offsetof(\
					audpp_cmd_cfg_object_params_eqalizer,\
					eq_flag);
				offset_addr = (unsigned short *)(base+offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					MM_ERR("ACDB RX WRITE"\
					"DATA:AUDPP_CMD_EQUALIZER\n");
					result = audpp_dsp_set_eq(
						COMMON_OBJ_ID,
						eq_config->eq_flag,\
						eq_config);
					if (result) {
						MM_ERR("ACDB=> Failed to "\
						"send EQ data to postproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---"\
					"EQ Rx copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --"\
					"EQ kalloc Failed LEN\n");
			}
		}
		if (eq_config != NULL)
			kfree(eq_config);
		break;
	}

	case AUDPP_CMD_MBADRC:
	{
		acdb_mbadrc_rtc =  kmalloc(sizeof(struct \
					acdb_block_mbadrc_rtc),\
					GFP_KERNEL);
		mbadrc_config =  kmalloc(sizeof(\
					audpp_cmd_cfg_object_params_mbadrc),\
					GFP_KERNEL);
		if (mbadrc_config != NULL && acdb_mbadrc_rtc != NULL) {
			if ((copy_from_user(acdb_mbadrc_rtc,\
				(void *)ubuf,
				sizeof(struct acdb_block_mbadrc_rtc)))
				== 0x00) {

				memset(mbadrc_config, 0,
					sizeof(\
					audpp_cmd_cfg_object_params_mbadrc));

				mbadrc_config->enable =
						acdb_mbadrc_rtc->enable;
				mbadrc_config->num_bands =
						acdb_mbadrc_rtc->num_bands;
				mbadrc_config->down_samp_level =
				acdb_mbadrc_rtc->down_samp_level;
				mbadrc_config->adrc_delay =
					acdb_mbadrc_rtc->adrc_delay;
				memcpy(mbadrc_config->adrc_band,\
					acdb_mbadrc_rtc->adrc_band,\
					AUDPP_MAX_MBADRC_BANDS *\
					sizeof(struct adrc_config));
				if (mbadrc_config->num_bands > 1) {
					mbadrc_config->ext_buf_size =
						(97 * 2) + (33 * 2 * \
					(mbadrc_config->num_bands - 2));
				}
				mbadrc_config->ext_partition = 0;
				mbadrc_config->ext_buf_lsw =
					(u16) EXTRACT_LOW_WORD(\
						rtc_write->phys);
				mbadrc_config->ext_buf_msw =
					(u16) EXTRACT_HIGH_WORD(\
						rtc_write->phys);
				memcpy(rtc_write->viraddr,
					acdb_mbadrc_rtc->ext_buff,
					(196*sizeof(signed int)));
				result = audpp_dsp_set_mbadrc(
						COMMON_OBJ_ID,
						mbadrc_config->enable,
						mbadrc_config);
				if (result) {
					MM_ERR("ACDB=> Failed to "\
						"Send MBADRC data "\
						"to postproc\n");
				} else {
					retval = true;
				}
			} else {
				MM_ERR("ACDB DATA WRITE ---"\
					"MBADRC Rx copy_from_user Fail\n");
			}
		} else {
			MM_ERR("ACDB DATA WRITE --MBADRC kalloc Failed LEN\n");
		}
		if (mbadrc_config != NULL)
			kfree(mbadrc_config);
		if (acdb_mbadrc_rtc != NULL)
			kfree(acdb_mbadrc_rtc);
	break;
	}
	}
	return retval;
}
static ssize_t rtc_getsetabid_data_dbg_write(struct file *filp,
						const char __user *ubuf,
						size_t cnt, loff_t *ppos)
{
	if (rtc_acdb.valid_abid != true) {
		MM_INFO("ACDB DATA READ ---INVALID ABID\n");
		rtc_acdb.err = ACDB_RTC_ERR_INVALID_ABID;
	} else {
		if (rtc_acdb.tx_rx_ctl == ACDB_RTC_RX) {
			if (acdb_set_rx_rtc(ubuf, cnt)) {
				rtc_acdb.err = ACDB_RTC_SUCCESS;
			} else {
				rtc_acdb.err = ACDB_RTC_ERR_UNKNOWN_FAILURE;
				cnt = 0;
			}
		} else if (rtc_acdb.tx_rx_ctl == ACDB_RTC_TX) {
			if (acdb_set_tx_rtc(ubuf, cnt)) {
				rtc_acdb.err = ACDB_RTC_SUCCESS;
			} else {
				rtc_acdb.err = ACDB_RTC_ERR_UNKNOWN_FAILURE;
				cnt = 0;
			}
		}
	}
	return cnt;
}


static const	struct file_operations rtc_acdb_data_debug_fops = {
	.open = rtc_getsetabid_data_dbg_open,
	.write = rtc_getsetabid_data_dbg_write,
	.read = rtc_getsetabid_data_dbg_read
};

static const	struct file_operations rtc_acdb_debug_fops = {
	.open = rtc_getsetabid_dbg_open,
	.write = rtc_getsetabid_dbg_write,
	.read = rtc_getsetabid_dbg_read
};

static void rtc_acdb_deinit(void)
{
	struct rtc_acdb_pmem *rtc_read = &rtc_acdb.rtc_read;
	struct rtc_acdb_pmem *rtc_write = &rtc_acdb.rtc_write;
	if (get_set_abid_dentry) {
		MM_DBG("GetSet ABID remove debugfs\n");
		debugfs_remove(get_set_abid_dentry);
	}

	if (get_set_abid_data_dentry) {
		MM_DBG("GetSet ABID remove debugfs\n");
		debugfs_remove(get_set_abid_data_dentry);
	}
	rtc_acdb.abid = 0;
	rtc_acdb.acdb_id = 0;
	rtc_acdb.cmd_id = 0;
	rtc_acdb.err = 1;
	rtc_acdb.set_abid = 0;
	rtc_acdb.set_iid = 0;
	rtc_acdb.tx_rx_ctl = 0;
	rtc_acdb.valid_abid = false;

	if (rtc_read->viraddr != NULL || ((void *)rtc_read->phys) != NULL) {
		iounmap(rtc_read->map_v_rtc);
		free_contiguous_memory_by_paddr(rtc_read->phys);
	}
	if (rtc_write->viraddr != NULL || ((void *)rtc_write->phys) != NULL) {
		iounmap(rtc_write->map_v_rtc);
		free_contiguous_memory_by_paddr(rtc_write->phys);
	}
}

static bool rtc_acdb_init(void)
{
	struct rtc_acdb_pmem *rtc_read = &rtc_acdb.rtc_read;
	struct rtc_acdb_pmem *rtc_write = &rtc_acdb.rtc_write;
	s32 result = 0;
	char name[sizeof "get_set_abid"+1];
	char name1[sizeof "get_set_abid_data"+1];
	rtc_acdb.abid = 0;
	rtc_acdb.acdb_id = 0;
	rtc_acdb.cmd_id = 0;
	rtc_acdb.err = 1;
	rtc_acdb.set_abid = 0;
	rtc_acdb.set_iid = 0;
	rtc_acdb.valid_abid = false;
	rtc_acdb.tx_rx_ctl = 0;

	snprintf(name, sizeof name, "get_set_abid");
	get_set_abid_dentry = debugfs_create_file(name,
			S_IFREG | S_IRUGO | S_IWUGO,
			NULL, NULL, &rtc_acdb_debug_fops);
	if (IS_ERR(get_set_abid_dentry)) {
		MM_ERR("SET GET ABID debugfs_create_file failed\n");
		return false;
	}

	snprintf(name1, sizeof name1, "get_set_abid_data");
	get_set_abid_data_dentry = debugfs_create_file(name1,
			S_IFREG | S_IRUGO | S_IWUGO,
			NULL, NULL,
			&rtc_acdb_data_debug_fops);
	if (IS_ERR(get_set_abid_data_dentry)) {
		MM_ERR("SET GET ABID DATA"\
				" debugfs_create_file failed\n");
		return false;
	}

	rtc_read->phys = allocate_contiguous_ebi_nomap(PMEM_RTC_ACDB_QUERY_MEM,
								 SZ_4K);

	if (!rtc_read->phys) {
		MM_ERR("ACDB Cannot allocate physical memory\n");
		result = -ENOMEM;
		goto error;
	}
	rtc_read->map_v_rtc = ioremap(rtc_read->phys,
				PMEM_RTC_ACDB_QUERY_MEM);

	if (IS_ERR(rtc_read->map_v_rtc)) {
		MM_ERR("ACDB Could not map physical address\n");
		result = -ENOMEM;
		goto error;
	}
	rtc_read->viraddr = rtc_read->map_v_rtc;
	memset(rtc_read->viraddr, 0, PMEM_RTC_ACDB_QUERY_MEM);

	rtc_write->phys = allocate_contiguous_ebi_nomap(PMEM_RTC_ACDB_QUERY_MEM,
								SZ_4K);

	if (!rtc_write->phys) {
		MM_ERR("ACDB Cannot allocate physical memory\n");
		result = -ENOMEM;
		goto error;
	}
	rtc_write->map_v_rtc = ioremap(rtc_write->phys,
				PMEM_RTC_ACDB_QUERY_MEM);

	if (IS_ERR(rtc_write->map_v_rtc)) {
		MM_ERR("ACDB Could not map physical address\n");
		result = -ENOMEM;
		goto error;
	}
	rtc_write->viraddr = rtc_write->map_v_rtc;
	memset(rtc_write->viraddr, 0, PMEM_RTC_ACDB_QUERY_MEM);
	init_waitqueue_head(&rtc_acdb.wait);
	return true;
error:
	MM_DBG("INIT RTC FAILED REMOVING RTC DEBUG FS\n");
	if (get_set_abid_dentry) {
		MM_DBG("GetSet ABID remove debugfs\n");
		debugfs_remove(get_set_abid_dentry);
	}

	if (get_set_abid_data_dentry) {
		MM_DBG("GetSet ABID remove debugfs\n");
		debugfs_remove(get_set_abid_data_dentry);
	}
	if (rtc_read->viraddr != NULL || ((void *)rtc_read->phys) != NULL) {
		iounmap(rtc_read->map_v_rtc);
		free_contiguous_memory_by_paddr(rtc_read->phys);
	}
	if (rtc_write->viraddr != NULL || ((void *)rtc_write->phys) != NULL) {
		iounmap(rtc_write->map_v_rtc);
		free_contiguous_memory_by_paddr(rtc_write->phys);
	}
	return false;
}
#else
void acdb_rtc_set_err(u32 err_code)
{
	return 0
}
#endif /*CONFIG_DEBUG_FS*/
static s32 acdb_set_calibration_blk(unsigned long arg)
{
	struct acdb_cmd_device acdb_cmd;
	s32 result = 0;

	MM_DBG("acdb_set_calibration_blk\n");
	if (copy_from_user(&acdb_cmd, (struct acdb_cmd_device *)arg,
			sizeof(acdb_cmd))) {
		MM_ERR("Failed copy command struct from user in"\
			"acdb_set_calibration_blk\n");
		return -EFAULT;
	}
	acdb_cmd.phys_buf = (u32 *)acdb_data.paddr;

	MM_DBG("acdb_cmd.phys_buf %x\n", (u32)acdb_cmd.phys_buf);

	result = dalrpc_fcn_8(ACDB_DAL_IOCTL, acdb_data.handle,
			(const void *)&acdb_cmd, sizeof(acdb_cmd),
			&acdb_data.acdb_result,
			sizeof(acdb_data.acdb_result));

	if (result < 0) {
		MM_ERR("ACDB=> Device Set RPC failure"\
			" result = %d\n", result);
		return -EINVAL;
	} else {
		MM_ERR("ACDB=> Device Set RPC success\n");
		if (acdb_data.acdb_result.result == ACDB_RES_SUCCESS)
			MM_DBG("ACDB_SET_DEVICE Success\n");
		else if (acdb_data.acdb_result.result == ACDB_RES_FAILURE)
			MM_ERR("ACDB_SET_DEVICE Failure\n");
		else if (acdb_data.acdb_result.result == ACDB_RES_BADPARM)
			MM_ERR("ACDB_SET_DEVICE BadParams\n");
		else
			MM_ERR("Unknown error\n");
	}
	return result;
}

static s32 acdb_get_calibration_blk(unsigned long arg)
{
	s32 result = 0;
	struct acdb_cmd_device acdb_cmd;

	MM_DBG("acdb_get_calibration_blk\n");

	if (copy_from_user(&acdb_cmd, (struct acdb_cmd_device *)arg,
			sizeof(acdb_cmd))) {
		MM_ERR("Failed copy command struct from user in"\
			"acdb_get_calibration_blk\n");
		return -EFAULT;
	}
	acdb_cmd.phys_buf = (u32 *)acdb_data.paddr;
	MM_ERR("acdb_cmd.phys_buf %x\n", (u32)acdb_cmd.phys_buf);

	result = dalrpc_fcn_8(ACDB_DAL_IOCTL, acdb_data.handle,
			(const void *)&acdb_cmd, sizeof(acdb_cmd),
			&acdb_data.acdb_result,
			sizeof(acdb_data.acdb_result));

	if (result < 0) {
		MM_ERR("ACDB=> Device Get RPC failure"\
			" result = %d\n", result);
		return -EINVAL;
	} else {
		MM_ERR("ACDB=> Device Get RPC Success\n");
		if (acdb_data.acdb_result.result == ACDB_RES_SUCCESS)
			MM_DBG("ACDB_GET_DEVICE Success\n");
		else if (acdb_data.acdb_result.result == ACDB_RES_FAILURE)
			MM_ERR("ACDB_GET_DEVICE Failure\n");
		else if (acdb_data.acdb_result.result == ACDB_RES_BADPARM)
			MM_ERR("ACDB_GET_DEVICE BadParams\n");
		else
			MM_ERR("Unknown error\n");
	}
	return result;
}

static int audio_acdb_open(struct inode *inode, struct file *file)
{
	MM_DBG("%s\n", __func__);
	return 0;
}
static int audio_acdb_release(struct inode *inode, struct file *file)
{
	MM_DBG("%s\n", __func__);
	return 0;
}

static long audio_acdb_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	int rc = 0;
	unsigned long flags = 0;

	MM_DBG("%s\n", __func__);

	switch (cmd) {
	case AUDIO_SET_EQ:
		MM_DBG("IOCTL SET_EQ_CONFIG\n");
		if (copy_from_user(&acdb_data.eq.num_bands, (void *) arg,
				sizeof(acdb_data.eq) -
				(AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2))) {
			rc = -EFAULT;
			break;
		}
		spin_lock_irqsave(&acdb_data.dsp_lock, flags);
		rc = audpp_dsp_set_eq(COMMON_OBJ_ID, 1,
			&acdb_data.eq);
		if (rc < 0)
			MM_ERR("AUDPP returned err =%d\n", rc);
		spin_unlock_irqrestore(&acdb_data.dsp_lock, flags);
		break;
	case AUDIO_SET_ACDB_BLK:
		MM_DBG("IOCTL AUDIO_SET_ACDB_BLK\n");
		rc = acdb_set_calibration_blk(arg);
		break;
	case AUDIO_GET_ACDB_BLK:
		MM_DBG("IOiCTL AUDIO_GET_ACDB_BLK\n");
		rc = acdb_get_calibration_blk(arg);
		break;
	default:
		MM_DBG("Unknown IOCTL%d\n", cmd);
		rc = -EINVAL;
	}
	return rc;
}

static const struct file_operations acdb_fops = {
	.owner = THIS_MODULE,
	.open = audio_acdb_open,
	.release = audio_acdb_release,
	.llseek = no_llseek,
	.unlocked_ioctl = audio_acdb_ioctl
};

struct miscdevice acdb_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_acdb",
	.fops	= &acdb_fops,
};

static s32 acdb_get_calibration(void)
{
	struct acdb_cmd_get_device_table	acdb_cmd;
	s32					result = 0;
	u32 iterations = 0;

	MM_DBG("acdb state = %d\n", acdb_data.acdb_state);

	acdb_cmd.command_id = ACDB_GET_DEVICE_TABLE;
	acdb_cmd.device_id = acdb_data.device_info->acdb_id;
	acdb_cmd.network_id = 0x0108B153;
	acdb_cmd.sample_rate_id = acdb_data.device_info->sample_rate;
	acdb_cmd.total_bytes = ACDB_BUF_SIZE;
	acdb_cmd.phys_buf = (u32 *)acdb_data.phys_addr;
	MM_DBG("device_id = %d, sampling_freq = %d\n",
				acdb_cmd.device_id, acdb_cmd.sample_rate_id);

	do {
		result = dalrpc_fcn_8(ACDB_DAL_IOCTL, acdb_data.handle,
				(const void *)&acdb_cmd, sizeof(acdb_cmd),
				&acdb_data.acdb_result,
				sizeof(acdb_data.acdb_result));

		if (result < 0) {
			MM_ERR("ACDB=> Device table RPC failure"\
				" result = %d\n", result);
			goto error;
		}
		/*following check is introduced to handle boot up race
		condition between AUDCAL SW peers running on apps
		and modem (ACDB_RES_BADSTATE indicates modem AUDCAL SW is
		not in initialized sate) we need to retry to get ACDB
		values*/
		if (acdb_data.acdb_result.result == ACDB_RES_BADSTATE) {
			msleep(500);
			iterations++;
		} else if (acdb_data.acdb_result.result == ACDB_RES_SUCCESS) {
			MM_DBG("Modem query for acdb values is successful"\
					" (iterations = %d)\n", iterations);
			acdb_data.acdb_state |= CAL_DATA_READY;
			return result;
		} else {
			MM_ERR("ACDB=> modem failed to fill acdb values,"\
					" reuslt = %d, (iterations = %d)\n",
					acdb_data.acdb_result.result,
					iterations);
			goto error;
		}
	} while (iterations < MAX_RETRY);
	MM_ERR("ACDB=> AUDCAL SW on modem is not in intiailized state (%d)\n",
			acdb_data.acdb_result.result);
error:
	result = -EINVAL;
	return result;
}

s32 acdb_get_calibration_data(struct acdb_get_block *get_block)
{
	s32 result = -EINVAL;
	struct acdb_cmd_device acdb_cmd;
	struct acdb_result acdb_result;

	MM_DBG("acdb_get_calibration_data\n");

	acdb_cmd.command_id = ACDB_GET_DEVICE;
	acdb_cmd.network_id = 0x0108B153;
	acdb_cmd.device_id = get_block->acdb_id;
	acdb_cmd.sample_rate_id = get_block->sample_rate_id;
	acdb_cmd.interface_id = get_block->interface_id;
	acdb_cmd.algorithm_block_id = get_block->algorithm_block_id;
	acdb_cmd.total_bytes = get_block->total_bytes;
	acdb_cmd.phys_buf = (u32 *)acdb_data.get_blk_paddr;

	result = dalrpc_fcn_8(ACDB_DAL_IOCTL, acdb_data.handle,
			(const void *)&acdb_cmd, sizeof(acdb_cmd),
			&acdb_result,
			sizeof(acdb_result));

	if (result < 0) {
		MM_ERR("ACDB=> Device Get RPC failure"\
			" result = %d\n", result);
		goto err_state;
	} else {
		MM_DBG("ACDB=> Device Get RPC Success\n");
		if (acdb_result.result == ACDB_RES_SUCCESS) {
			MM_DBG("ACDB_GET_DEVICE Success\n");
			result = 0;
			memcpy(get_block->buf_ptr, acdb_data.get_blk_kvaddr,
					get_block->total_bytes);
		} else if (acdb_result.result == ACDB_RES_FAILURE)
			MM_ERR("ACDB_GET_DEVICE Failure\n");
		else if (acdb_result.result == ACDB_RES_BADPARM)
			MM_ERR("ACDB_GET_DEVICE BadParams\n");
		else
			MM_ERR("Unknown error\n");
	}
err_state:
	return result;
}
EXPORT_SYMBOL(acdb_get_calibration_data);

int is_acdb_enabled()
{
	if (acdb_data.handle != NULL)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_acdb_enabled);

static u8 check_device_info_already_present(
		struct dev_evt_msg device_info,
			struct acdb_cache_node *acdb_cache_free_node)
{
	if ((device_info.sample_rate ==
				acdb_cache_free_node->device_info.\
				sample_rate) &&
			(device_info.acdb_id ==
				acdb_cache_free_node->device_info.acdb_id)) {
		MM_DBG("acdb values are already present\n");
		/*if acdb state is not set for CAL_DATA_READY and node status
		is filled, acdb state should be updated with CAL_DATA_READY
		state*/
		acdb_data.acdb_state |= CAL_DATA_READY;
		return 1; /*node is present but status as filled*/
	}
	MM_DBG("copying device info into node\n");
	/*as device information is not present in cache copy
	the current device information into the node*/
	memcpy(&acdb_cache_free_node->device_info,
				 &device_info, sizeof(device_info));
	return 0; /*cant find the node*/
}

static struct acdb_iir_block *get_audpp_irr_block(void)
{
	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);
		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_IIR_RX) {
				if (prs_hdr->iid == IID_AUDIO_IIR_COEFF)
					return (struct acdb_iir_block *)
						(acdb_data.virt_addr + index
						 + sizeof(struct header));
			} else {
				index += prs_hdr->data_len +
						sizeof(struct header);
			}
		} else {
			break;
		}
	}
	return NULL;
}


static s32 acdb_fill_audpp_iir(void)
{
	struct acdb_iir_block *acdb_iir;
	s32 i = 0;

	acdb_iir = get_audpp_irr_block();
	if (acdb_iir == NULL) {
		MM_ERR("unable to find  audpp iir block returning\n");
		return -EINVAL;
	}
	memset(acdb_data.pp_iir, 0, sizeof(*acdb_data.pp_iir));

	acdb_data.pp_iir->active_flag = acdb_iir->enable_flag;
	acdb_data.pp_iir->num_bands = acdb_iir->stage_count;
	for (; i < acdb_iir->stage_count; i++) {
		acdb_data.pp_iir->params_filter.filter_4_params.
			numerator_filter[i].numerator_b0_filter_lsw =
			acdb_iir->stages[i].b0_lo;
		acdb_data.pp_iir->params_filter.filter_4_params.
			numerator_filter[i].numerator_b0_filter_msw =
			acdb_iir->stages[i].b0_hi;
		acdb_data.pp_iir->params_filter.filter_4_params.
			numerator_filter[i].numerator_b1_filter_lsw =
			acdb_iir->stages[i].b1_lo;
		acdb_data.pp_iir->params_filter.filter_4_params.
			numerator_filter[i].numerator_b1_filter_msw =
			acdb_iir->stages[i].b1_hi;
		acdb_data.pp_iir->params_filter.filter_4_params.
			numerator_filter[i].numerator_b2_filter_lsw =
			acdb_iir->stages[i].b2_lo;
		acdb_data.pp_iir->params_filter.filter_4_params.
			numerator_filter[i].numerator_b2_filter_msw =
			acdb_iir->stages[i].b2_hi;
		acdb_data.pp_iir->params_filter.filter_4_params.
			denominator_filter[i].denominator_a0_filter_lsw =
			acdb_iir->stages_a[i].a1_lo;
		acdb_data.pp_iir->params_filter.filter_4_params.
			denominator_filter[i].denominator_a0_filter_msw =
			acdb_iir->stages_a[i].a1_hi;
		acdb_data.pp_iir->params_filter.filter_4_params.
			denominator_filter[i].denominator_a1_filter_lsw =
			acdb_iir->stages_a[i].a2_lo;
		acdb_data.pp_iir->params_filter.filter_4_params.
			denominator_filter[i].denominator_a1_filter_msw =
			acdb_iir->stages_a[i].a2_hi;
		acdb_data.pp_iir->params_filter.filter_4_params.
			shift_factor_filter[i].shift_factor_0 =
			acdb_iir->shift_factor[i];
		acdb_data.pp_iir->params_filter.filter_4_params.pan_filter[i].
			pan_filter_0 = acdb_iir->pan[i];
	}
	return 0;
}

static void extract_mbadrc(u32 *phy_addr, struct header *prs_hdr, u32 *index)
{
	if (prs_hdr->iid == IID_MBADRC_EXT_BUFF) {
		MM_DBG("Got IID = IID_MBADRC_EXT_BUFF\n");
		*phy_addr = acdb_data.phys_addr	+ *index +
					sizeof(struct header);
		memcpy(acdb_data.mbadrc_block.ext_buf,
				(acdb_data.virt_addr + *index +
					sizeof(struct header)), 196*2);
		MM_DBG("phy_addr = %x\n", *phy_addr);
		*index += prs_hdr->data_len + sizeof(struct header);
	} else if (prs_hdr->iid == IID_MBADRC_BAND_CONFIG) {
		MM_DBG("Got IID == IID_MBADRC_BAND_CONFIG\n");
		memcpy(acdb_data.mbadrc_block.band_config, (acdb_data.virt_addr
					+ *index + sizeof(struct header)),
				sizeof(struct mbadrc_band_config_type) *
					 acdb_data.mbadrc_block.parameters.\
						mbadrc_num_bands);
		*index += prs_hdr->data_len + sizeof(struct header);
	} else if (prs_hdr->iid == IID_MBADRC_PARAMETERS) {
		struct mbadrc_parameter *tmp;
		tmp = (struct mbadrc_parameter *)(acdb_data.virt_addr + *index
						+ sizeof(struct header));
		MM_DBG("Got IID == IID_MBADRC_PARAMETERS");
		acdb_data.mbadrc_block.parameters.mbadrc_enable =
							tmp->mbadrc_enable;
		acdb_data.mbadrc_block.parameters.mbadrc_num_bands =
							tmp->mbadrc_num_bands;
		acdb_data.mbadrc_block.parameters.mbadrc_down_sample_level =
						tmp->mbadrc_down_sample_level;
		acdb_data.mbadrc_block.parameters.mbadrc_delay =
							tmp->mbadrc_delay;
		*index += prs_hdr->data_len + sizeof(struct header);
	}
}

static void get_audpp_mbadrc_block(u32 *phy_addr)
{
	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);

		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_MBADRC_RX) {
				if ((prs_hdr->iid == IID_MBADRC_EXT_BUFF)
					|| (prs_hdr->iid ==
						IID_MBADRC_BAND_CONFIG)
					|| (prs_hdr->iid ==
						IID_MBADRC_PARAMETERS)) {
					extract_mbadrc(phy_addr, prs_hdr,
								&index);
				}
			} else {
				index += prs_hdr->data_len +
						sizeof(struct header);
			}
		} else {
			break;
		}
	}
}

static s32 acdb_fill_audpp_mbadrc(void)
{
	u32 mbadrc_phys_addr = -1;
	get_audpp_mbadrc_block(&mbadrc_phys_addr);
	if (IS_ERR_VALUE(mbadrc_phys_addr)) {
		MM_ERR("failed to get mbadrc block\n");
		return -EINVAL;
	}

	memset(acdb_data.pp_mbadrc, 0, sizeof(*acdb_data.pp_mbadrc));

	acdb_data.pp_mbadrc->enable = acdb_data.mbadrc_block.\
					parameters.mbadrc_enable;
	acdb_data.pp_mbadrc->num_bands =
				acdb_data.mbadrc_block.\
					parameters.mbadrc_num_bands;
	acdb_data.pp_mbadrc->down_samp_level =
				acdb_data.mbadrc_block.parameters.\
					mbadrc_down_sample_level;
	acdb_data.pp_mbadrc->adrc_delay =
				acdb_data.mbadrc_block.parameters.\
					mbadrc_delay;

	if (acdb_data.mbadrc_block.parameters.mbadrc_num_bands > 1)
		acdb_data.pp_mbadrc->ext_buf_size = (97 * 2) +
			(33 * 2 * (acdb_data.mbadrc_block.parameters.\
					mbadrc_num_bands - 2));

	acdb_data.pp_mbadrc->ext_partition = 0;
	acdb_data.pp_mbadrc->ext_buf_lsw = (u16)(mbadrc_phys_addr\
						 & 0xFFFF);
	acdb_data.pp_mbadrc->ext_buf_msw = (u16)((mbadrc_phys_addr\
						 & 0xFFFF0000) >> 16);
	memcpy(acdb_data.pp_mbadrc->adrc_band, acdb_data.mbadrc_block.\
					band_config,
		sizeof(struct mbadrc_band_config_type) *
		acdb_data.mbadrc_block.parameters.mbadrc_num_bands);
	return 0;
}

static s32 acdb_calibrate_audpp(void)
{
	s32	result = 0;

	result = acdb_fill_audpp_iir();
	if (!IS_ERR_VALUE(result)) {
		result = audpp_dsp_set_rx_iir(COMMON_OBJ_ID,
				acdb_data.pp_iir->active_flag,
					acdb_data.pp_iir);
		if (result) {
			MM_ERR("ACDB=> Failed to send IIR data to postproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPP is calibrated with IIR parameters\n");
	}
	result = acdb_fill_audpp_mbadrc();
	if (!IS_ERR_VALUE(result)) {
		result = audpp_dsp_set_mbadrc(COMMON_OBJ_ID,
					acdb_data.pp_mbadrc->enable,
					acdb_data.pp_mbadrc);
		if (result) {
			MM_ERR("ACDB=> Failed to send MBADRC data to"\
					" postproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPP is calibrated with MBADRC parameters");
	}
done:
	return result;
}

static s32 acdb_re_enable_audpp(void)
{
	s32	result = 0;

	if ((acdb_data.audpp_disabled_features &
			(1 << AUDPP_CMD_IIR_TUNING_FILTER))
			== (1 << AUDPP_CMD_IIR_TUNING_FILTER)) {
		result = audpp_dsp_set_rx_iir(COMMON_OBJ_ID,
				acdb_data.pp_iir->active_flag,
				acdb_data.pp_iir);
		if (result) {
			MM_ERR("ACDB=> Failed to send IIR data to postproc\n");
			result = -EINVAL;
		} else {
			MM_DBG("Re-enable IIR parameters");
		}
	}
	if ((acdb_data.audpp_disabled_features & (1 << AUDPP_CMD_MBADRC))
			== (1 << AUDPP_CMD_MBADRC)) {
		result = audpp_dsp_set_mbadrc(COMMON_OBJ_ID,
				acdb_data.pp_mbadrc->enable,
				acdb_data.pp_mbadrc);
		if (result) {
			MM_ERR("ACDB=> Failed to send MBADRC data to"\
					" postproc\n");
			result = -EINVAL;
		} else {
			MM_DBG("Re-enable MBADRC parameters");
		}
	}
	acdb_data.audpp_disabled_features = 0;
	return result;
}

static struct acdb_agc_block *get_audpreproc_agc_block(void)
{
	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);
		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_AGC_TX) {
				if (prs_hdr->iid == IID_AUDIO_AGC_PARAMETERS) {
					MM_DBG("GOT ABID_AUDIO_AGC_TX\n");
					return (struct acdb_agc_block *)
						(acdb_data.virt_addr + index
						 + sizeof(struct header));
				}
			} else {
				index += prs_hdr->data_len +
						sizeof(struct header);
			}
		} else {
			break;
		}
	}
	return NULL;
}

static s32 acdb_fill_audpreproc_agc(void)
{
	struct acdb_agc_block	*acdb_agc;

	acdb_agc = get_audpreproc_agc_block();
	if (!acdb_agc) {
		MM_DBG("unable to find preproc agc parameters winding up\n");
		return -EINVAL;
	}
	memset(acdb_data.preproc_agc, 0, sizeof(*acdb_data.preproc_agc));
	acdb_data.preproc_agc->cmd_id = AUDPREPROC_CMD_CFG_AGC_PARAMS;
	/* 0xFE00 to configure all parameters */
	acdb_data.preproc_agc->tx_agc_param_mask = 0xFFFF;
	if (acdb_agc->enable_status)
		acdb_data.preproc_agc->tx_agc_enable_flag =
			AUDPREPROC_CMD_TX_AGC_ENA_FLAG_ENA;
	else
		acdb_data.preproc_agc->tx_agc_enable_flag =
			AUDPREPROC_CMD_TX_AGC_ENA_FLAG_DIS;

	acdb_data.preproc_agc->comp_rlink_static_gain =
		acdb_agc->comp_rlink_static_gain;
	acdb_data.preproc_agc->comp_rlink_aig_flag =
		acdb_agc->comp_rlink_aig_flag;
	acdb_data.preproc_agc->expander_rlink_th =
		acdb_agc->exp_rlink_threshold;
	acdb_data.preproc_agc->expander_rlink_slope =
		acdb_agc->exp_rlink_slope;
	acdb_data.preproc_agc->compressor_rlink_th =
		acdb_agc->comp_rlink_threshold;
	acdb_data.preproc_agc->compressor_rlink_slope =
		acdb_agc->comp_rlink_slope;

	/* 0xFFF0 to configure all parameters */
	acdb_data.preproc_agc->tx_adc_agc_param_mask = 0xFFFF;

	acdb_data.preproc_agc->comp_rlink_aig_attackk =
		acdb_agc->comp_rlink_aig_attack_k;
	acdb_data.preproc_agc->comp_rlink_aig_leak_down =
		acdb_agc->comp_rlink_aig_leak_down;
	acdb_data.preproc_agc->comp_rlink_aig_leak_up =
		acdb_agc->comp_rlink_aig_leak_up;
	acdb_data.preproc_agc->comp_rlink_aig_max =
		acdb_agc->comp_rlink_aig_max;
	acdb_data.preproc_agc->comp_rlink_aig_min =
		acdb_agc->comp_rlink_aig_min;
	acdb_data.preproc_agc->comp_rlink_aig_releasek =
		acdb_agc->comp_rlink_aig_release_k;
	acdb_data.preproc_agc->comp_rlink_aig_leakrate_fast =
		acdb_agc->comp_rlink_aig_sm_leak_rate_fast;
	acdb_data.preproc_agc->comp_rlink_aig_leakrate_slow =
		acdb_agc->comp_rlink_aig_sm_leak_rate_slow;
	acdb_data.preproc_agc->comp_rlink_attackk_msw =
		acdb_agc->comp_rlink_attack_k_msw;
	acdb_data.preproc_agc->comp_rlink_attackk_lsw =
		acdb_agc->comp_rlink_attack_k_lsw;
	acdb_data.preproc_agc->comp_rlink_delay =
		acdb_agc->comp_rlink_delay;
	acdb_data.preproc_agc->comp_rlink_releasek_msw =
		acdb_agc->comp_rlink_release_k_msw;
	acdb_data.preproc_agc->comp_rlink_releasek_lsw =
		acdb_agc->comp_rlink_release_k_lsw;
	acdb_data.preproc_agc->comp_rlink_rms_tav =
		acdb_agc->comp_rlink_rms_trav;
	return 0;
}

static struct acdb_iir_block *get_audpreproc_irr_block(void)
{

	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);

		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_IIR_TX) {
				if (prs_hdr->iid == IID_AUDIO_IIR_COEFF)
					return (struct acdb_iir_block *)
						(acdb_data.virt_addr + index
						 + sizeof(struct header));
			} else {
				index += prs_hdr->data_len +
						sizeof(struct header);
			}
		} else {
			break;
		}
	}
	return NULL;
}


static s32 acdb_fill_audpreproc_iir(void)
{
	struct acdb_iir_block	*acdb_iir;


	acdb_iir =  get_audpreproc_irr_block();
	if (!acdb_iir) {
		MM_DBG("unable to find preproc iir parameters winding up\n");
		return -EINVAL;
	}
	memset(acdb_data.preproc_iir, 0, sizeof(*acdb_data.preproc_iir));

	acdb_data.preproc_iir->cmd_id =
		AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS;
	acdb_data.preproc_iir->active_flag = acdb_iir->enable_flag;
	acdb_data.preproc_iir->num_bands = acdb_iir->stage_count;

	acdb_data.preproc_iir->numerator_coeff_b0_filter0_lsw =
		acdb_iir->stages[0].b0_lo;
	acdb_data.preproc_iir->numerator_coeff_b0_filter0_msw =
		acdb_iir->stages[0].b0_hi;
	acdb_data.preproc_iir->numerator_coeff_b1_filter0_lsw =
		acdb_iir->stages[0].b1_lo;
	acdb_data.preproc_iir->numerator_coeff_b1_filter0_msw =
		acdb_iir->stages[0].b1_hi;
	acdb_data.preproc_iir->numerator_coeff_b2_filter0_lsw =
		acdb_iir->stages[0].b2_lo;
	acdb_data.preproc_iir->numerator_coeff_b2_filter0_msw =
		acdb_iir->stages[0].b2_hi;

	acdb_data.preproc_iir->numerator_coeff_b0_filter1_lsw =
		acdb_iir->stages[1].b0_lo;
	acdb_data.preproc_iir->numerator_coeff_b0_filter1_msw =
		acdb_iir->stages[1].b0_hi;
	acdb_data.preproc_iir->numerator_coeff_b1_filter1_lsw =
		acdb_iir->stages[1].b1_lo;
	acdb_data.preproc_iir->numerator_coeff_b1_filter1_msw =
		acdb_iir->stages[1].b1_hi;
	acdb_data.preproc_iir->numerator_coeff_b2_filter1_lsw =
		acdb_iir->stages[1].b2_lo;
	acdb_data.preproc_iir->numerator_coeff_b2_filter1_msw =
		acdb_iir->stages[1].b2_hi;

	acdb_data.preproc_iir->numerator_coeff_b0_filter2_lsw =
		acdb_iir->stages[2].b0_lo;
	acdb_data.preproc_iir->numerator_coeff_b0_filter2_msw =
		acdb_iir->stages[2].b0_hi;
	acdb_data.preproc_iir->numerator_coeff_b1_filter2_lsw =
		acdb_iir->stages[2].b1_lo;
	acdb_data.preproc_iir->numerator_coeff_b1_filter2_msw =
		acdb_iir->stages[2].b1_hi;
	acdb_data.preproc_iir->numerator_coeff_b2_filter2_lsw =
		acdb_iir->stages[2].b2_lo;
	acdb_data.preproc_iir->numerator_coeff_b2_filter2_msw =
		acdb_iir->stages[2].b2_hi;

	acdb_data.preproc_iir->numerator_coeff_b0_filter3_lsw =
		acdb_iir->stages[3].b0_lo;
	acdb_data.preproc_iir->numerator_coeff_b0_filter3_msw =
		acdb_iir->stages[3].b0_hi;
	acdb_data.preproc_iir->numerator_coeff_b1_filter3_lsw =
		acdb_iir->stages[3].b1_lo;
	acdb_data.preproc_iir->numerator_coeff_b1_filter3_msw =
		acdb_iir->stages[3].b1_hi;
	acdb_data.preproc_iir->numerator_coeff_b2_filter3_lsw =
		acdb_iir->stages[3].b2_lo;
	acdb_data.preproc_iir->numerator_coeff_b2_filter3_msw =
		acdb_iir->stages[3].b2_hi;

	acdb_data.preproc_iir->denominator_coeff_a0_filter0_lsw =
		acdb_iir->stages_a[0].a1_lo;
	acdb_data.preproc_iir->denominator_coeff_a0_filter0_msw =
		acdb_iir->stages_a[0].a1_hi;
	acdb_data.preproc_iir->denominator_coeff_a1_filter0_lsw =
		acdb_iir->stages_a[0].a2_lo;
	acdb_data.preproc_iir->denominator_coeff_a1_filter0_msw =
		acdb_iir->stages_a[0].a2_hi;

	acdb_data.preproc_iir->denominator_coeff_a0_filter1_lsw =
		acdb_iir->stages_a[1].a1_lo;
	acdb_data.preproc_iir->denominator_coeff_a0_filter1_msw =
		acdb_iir->stages_a[1].a1_hi;
	acdb_data.preproc_iir->denominator_coeff_a1_filter1_lsw =
		acdb_iir->stages_a[1].a2_lo;
	acdb_data.preproc_iir->denominator_coeff_a1_filter1_msw =
		acdb_iir->stages_a[1].a2_hi;

	acdb_data.preproc_iir->denominator_coeff_a0_filter2_lsw =
		acdb_iir->stages_a[2].a1_lo;
	acdb_data.preproc_iir->denominator_coeff_a0_filter2_msw =
		acdb_iir->stages_a[2].a1_hi;
	acdb_data.preproc_iir->denominator_coeff_a1_filter2_lsw =
		acdb_iir->stages_a[2].a2_lo;
	acdb_data.preproc_iir->denominator_coeff_a1_filter2_msw =
		acdb_iir->stages_a[2].a2_hi;

	acdb_data.preproc_iir->denominator_coeff_a0_filter3_lsw =
		acdb_iir->stages_a[3].a1_lo;
	acdb_data.preproc_iir->denominator_coeff_a0_filter3_msw =
		acdb_iir->stages_a[3].a1_hi;
	acdb_data.preproc_iir->denominator_coeff_a1_filter3_lsw =
		acdb_iir->stages_a[3].a2_lo;
	acdb_data.preproc_iir->denominator_coeff_a1_filter3_msw =
		acdb_iir->stages_a[3].a2_hi;

	acdb_data.preproc_iir->shift_factor_filter0 =
		acdb_iir->shift_factor[0];
	acdb_data.preproc_iir->shift_factor_filter1 =
		acdb_iir->shift_factor[1];
	acdb_data.preproc_iir->shift_factor_filter2 =
		acdb_iir->shift_factor[2];
	acdb_data.preproc_iir->shift_factor_filter3 =
		acdb_iir->shift_factor[3];

	acdb_data.preproc_iir->channel_selected0 =
		acdb_iir->pan[0];
	acdb_data.preproc_iir->channel_selected1 =
		acdb_iir->pan[1];
	acdb_data.preproc_iir->channel_selected2 =
		acdb_iir->pan[2];
	acdb_data.preproc_iir->channel_selected3 =
		acdb_iir->pan[3];
	return 0;
}

static struct acdb_ns_tx_block *get_audpreproc_ns_block(void)
{

	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);

		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_NS_TX) {
				if (prs_hdr->iid == IID_NS_PARAMETERS)
					return (struct acdb_ns_tx_block *)
						(acdb_data.virt_addr + index
						 + sizeof(struct header));
			} else {
				index += prs_hdr->data_len +
						sizeof(struct header);
			}
		} else {
			break;
		}
	}
	return NULL;
}

static s32 acdb_fill_audpreproc_ns(void)
{
	struct acdb_ns_tx_block	*acdb_ns;
	/* TO DO: do we enable_status_filled */
	acdb_ns = get_audpreproc_ns_block();
	if (!acdb_ns) {
		MM_DBG("unable to find preproc ns parameters winding up\n");
		return -EINVAL;
	}
	memset(acdb_data.preproc_ns, 0, sizeof(*acdb_data.preproc_ns));
	acdb_data.preproc_ns->cmd_id = AUDPREPROC_CMD_CFG_NS_PARAMS;

	acdb_data.preproc_ns->ec_mode_new  = acdb_ns->ec_mode_new;
	acdb_data.preproc_ns->dens_gamma_n = acdb_ns->dens_gamma_n;
	acdb_data.preproc_ns->dens_nfe_block_size  =
					acdb_ns->dens_nfe_block_size;
	acdb_data.preproc_ns->dens_limit_ns = acdb_ns->dens_limit_ns;
	acdb_data.preproc_ns->dens_limit_ns_d  = acdb_ns->dens_limit_ns_d;
	acdb_data.preproc_ns->wb_gamma_e  = acdb_ns->wb_gamma_e;
	acdb_data.preproc_ns->wb_gamma_n  = acdb_ns->wb_gamma_n;

	return 0;
}

s32 acdb_calibrate_audpreproc(void)
{
	s32	result = 0;

	result = acdb_fill_audpreproc_agc();
	if (!IS_ERR_VALUE(result)) {
		result = audpreproc_dsp_set_agc(acdb_data.preproc_agc, sizeof(
					audpreproc_cmd_cfg_agc_params));
		if (result) {
			MM_ERR("ACDB=> Failed to send AGC data to preproc)\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPREC is calibrated with AGC parameters");
	}
	result = acdb_fill_audpreproc_iir();
	if (!IS_ERR_VALUE(result)) {
		result = audpreproc_dsp_set_iir(acdb_data.preproc_iir,
				sizeof(\
				audpreproc_cmd_cfg_iir_tuning_filter_params));
		if (result) {
			MM_ERR("ACDB=> Failed to send IIR data to preproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("audpreproc is calibrated with iir parameters");
	}

	result = acdb_fill_audpreproc_ns();
	if (!IS_ERR_VALUE(result)) {
		result = audpreproc_dsp_set_ns(acdb_data.preproc_ns,
						sizeof(\
						audpreproc_cmd_cfg_ns_params));
		if (result) {
			MM_ERR("ACDB=> Failed to send NS data to preproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("audpreproc is calibrated with NS parameters");
	}
done:
	return result;
}

static s32 acdb_send_calibration(void)
{
	s32 result = 0;

	if (acdb_data.device_info->dev_type.rx_device) {
		result = acdb_calibrate_audpp();
		if (result)
			goto done;
	} else if (acdb_data.device_info->dev_type.tx_device) {
		result = acdb_calibrate_audpreproc();
		if (result)
			goto done;
		acdb_data.audrec_applied |= AUDREC_READY;
		MM_DBG("acdb_data.audrec_applied = %x\n",
					acdb_data.audrec_applied);
	}
done:
	return result;
}

static u8 check_tx_acdb_values_cached(void)
{
	if ((acdb_data.device_info->sample_rate ==
		acdb_cache_tx.device_info.sample_rate) &&
		(acdb_data.device_info->acdb_id ==
		acdb_cache_tx.device_info.acdb_id) &&
		(acdb_cache_tx.node_status ==
						ACDB_VALUES_FILLED))
		return 0;
	else
		return 1;
}

static void handle_tx_device_ready_callback(void)
{
	u8 acdb_value_apply = 0;
	u8 result = 0;

	/*check wheather AUDREC enabled before device call backs*/
	if ((acdb_data.acdb_state & AUDREC_READY) &&
			!(acdb_data.audrec_applied & AUDREC_READY)) {
		MM_DBG("AUDREC already enabled apply acdb values\n");
		acdb_value_apply |= AUDREC_READY;
	}
	if (acdb_value_apply) {
		if (session_info.sampling_freq)
			acdb_data.device_info->sample_rate =
					session_info.sampling_freq;
		result = check_tx_acdb_values_cached();
		if (result) {
			result = acdb_get_calibration();
			if (result < 0) {
				MM_ERR("Not able to get calibration"\
						" data continue\n");
				return;
			}
		}
		acdb_cache_tx.node_status = ACDB_VALUES_FILLED;
		acdb_send_calibration();
	}
}

static struct acdb_cache_node *get_acdb_values_from_cache_tx(u32 stream_id)
{
	MM_DBG("searching node with stream_id");
	if ((acdb_cache_tx.stream_id == stream_id) &&
			(acdb_cache_tx.node_status ==
					ACDB_VALUES_NOT_FILLED)) {
			return &acdb_cache_tx;
	}
	MM_DBG("Error! in finding node\n");
	return NULL;
}

static void update_acdb_data_struct(struct acdb_cache_node *cur_node)
{
	if (cur_node) {
		acdb_data.device_info = &cur_node->device_info;
		acdb_data.virt_addr = cur_node->virt_addr_acdb_values;
		acdb_data.phys_addr = cur_node->phys_addr_acdb_values;
	} else
		MM_ERR("error in curent node\n");
}

static void send_acdb_values_for_active_devices(void)
{
	if (acdb_cache_rx.node_status ==
			ACDB_VALUES_FILLED) {
		update_acdb_data_struct(&acdb_cache_rx);
		if (acdb_data.acdb_state & CAL_DATA_READY)
			acdb_send_calibration();
	}
}

static s32 initialize_rpc(void)
{
	s32 result = 0;

	result = daldevice_attach(DALDEVICEID_ACDB, ACDB_PORT_NAME,
			ACDB_CPU, &acdb_data.handle);

	if (result) {
		MM_ERR("ACDB=> Device Attach failed\n");
		result = -ENODEV;
		goto done;
	}
done:
	return result;
}

static u32 allocate_memory_acdb_cache_tx(void)
{
	u32 result = 0;
	/*initialize local cache */
	acdb_cache_tx.phys_addr_acdb_values =
		allocate_contiguous_ebi_nomap(ACDB_BUF_SIZE,
				SZ_4K);

	if (!acdb_cache_tx.phys_addr_acdb_values) {
		MM_ERR("ACDB=> Cannot allocate physical memory\n");
		result = -ENOMEM;
		goto error;
	}
	acdb_cache_tx.map_v_addr = ioremap(
			acdb_cache_tx.phys_addr_acdb_values,
			ACDB_BUF_SIZE);
	if (IS_ERR(acdb_cache_tx.map_v_addr)) {
		MM_ERR("ACDB=> Could not map physical address\n");
		result = -ENOMEM;
		free_contiguous_memory_by_paddr(
				acdb_cache_tx.phys_addr_acdb_values);
		goto error;
	}
	acdb_cache_tx.virt_addr_acdb_values =
		acdb_cache_tx.map_v_addr;
	memset(acdb_cache_tx.virt_addr_acdb_values, 0,
			ACDB_BUF_SIZE);
	return result;
error:
	iounmap(acdb_cache_tx.map_v_addr);
	free_contiguous_memory_by_paddr(
			acdb_cache_tx.phys_addr_acdb_values);
	return result;
}

static u32 allocate_memory_acdb_cache_rx(void)
{
	u32 result = 0;

	/*initialize local cache */
	acdb_cache_rx.phys_addr_acdb_values =
		allocate_contiguous_ebi_nomap(
				ACDB_BUF_SIZE, SZ_4K);

	if (!acdb_cache_rx.phys_addr_acdb_values) {
		MM_ERR("ACDB=> Can not allocate physical memory\n");
		result = -ENOMEM;
		goto error;
	}
	acdb_cache_rx.map_v_addr =
		ioremap(acdb_cache_rx.phys_addr_acdb_values,
				ACDB_BUF_SIZE);
	if (IS_ERR(acdb_cache_rx.map_v_addr)) {
		MM_ERR("ACDB=> Could not map physical address\n");
		result = -ENOMEM;
		free_contiguous_memory_by_paddr(
				acdb_cache_rx.phys_addr_acdb_values);
		goto error;
	}
	acdb_cache_rx.virt_addr_acdb_values =
		acdb_cache_rx.map_v_addr;
	memset(acdb_cache_rx.virt_addr_acdb_values, 0,
			ACDB_BUF_SIZE);
	return result;
error:
	iounmap(acdb_cache_rx.map_v_addr);
	free_contiguous_memory_by_paddr(
			acdb_cache_rx.phys_addr_acdb_values);
	return result;
}

static u32 allocate_memory_acdb_get_blk(void)
{
	u32 result = 0;
	acdb_data.get_blk_paddr = allocate_contiguous_ebi_nomap(
						ACDB_BUF_SIZE, SZ_4K);
	if (!acdb_data.get_blk_paddr) {
		MM_ERR("ACDB=> Cannot allocate physical memory\n");
		result = -ENOMEM;
		goto error;
	}
	acdb_data.map_v_get_blk = ioremap(acdb_data.get_blk_paddr,
					ACDB_BUF_SIZE);
	if (IS_ERR(acdb_data.map_v_get_blk)) {
		MM_ERR("ACDB=> Could not map physical address\n");
		result = -ENOMEM;
		free_contiguous_memory_by_paddr(
					acdb_data.get_blk_paddr);
		goto error;
	}
	acdb_data.get_blk_kvaddr = acdb_data.map_v_get_blk;
	memset(acdb_data.get_blk_kvaddr, 0, ACDB_BUF_SIZE);
error:
	return result;
}

static void free_memory_acdb_cache_rx(void)
{
	iounmap(acdb_cache_rx.map_v_addr);
	free_contiguous_memory_by_paddr(
			acdb_cache_rx.phys_addr_acdb_values);
}

static void free_memory_acdb_cache_tx(void)
{

	iounmap(acdb_cache_tx.map_v_addr);
	free_contiguous_memory_by_paddr(
			acdb_cache_tx.phys_addr_acdb_values);
}

static void free_memory_acdb_get_blk(void)
{
	iounmap(acdb_data.map_v_get_blk);
	free_contiguous_memory_by_paddr(acdb_data.get_blk_paddr);
}

static s32 initialize_memory(void)
{
	s32 result = 0;

	result = allocate_memory_acdb_get_blk();
	if (result < 0) {
		MM_ERR("memory allocation for get blk failed\n");
		goto done;
	}

	result = allocate_memory_acdb_cache_rx();
	if (result < 0) {
		MM_ERR("memory allocation for rx cache is failed\n");
		free_memory_acdb_get_blk();
		goto done;
	}
	result = allocate_memory_acdb_cache_tx();
	if (result < 0) {
		MM_ERR("memory allocation for tx cache is failed\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		goto done;
	}
	acdb_data.pp_iir = kmalloc(sizeof(*acdb_data.pp_iir),
		GFP_KERNEL);
	if (acdb_data.pp_iir == NULL) {
		MM_ERR("ACDB=> Could not allocate postproc iir memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		result = -ENOMEM;
		goto done;
	}

	acdb_data.pp_mbadrc = kmalloc(sizeof(*acdb_data.pp_mbadrc), GFP_KERNEL);
	if (acdb_data.pp_mbadrc == NULL) {
		MM_ERR("ACDB=> Could not allocate postproc mbadrc memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		result = -ENOMEM;
		goto done;
	}

	acdb_data.preproc_agc = kmalloc(sizeof(*acdb_data.preproc_agc),
							GFP_KERNEL);
	if (acdb_data.preproc_agc == NULL) {
		MM_ERR("ACDB=> Could not allocate preproc agc memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		result = -ENOMEM;
		goto done;
	}

	acdb_data.preproc_iir = kmalloc(sizeof(*acdb_data.preproc_iir),
							GFP_KERNEL);
	if (acdb_data.preproc_iir == NULL) {
		MM_ERR("ACDB=> Could not allocate preproc iir memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		kfree(acdb_data.preproc_agc);
		result = -ENOMEM;
		goto done;
	}

	acdb_data.preproc_ns = kmalloc(sizeof(*acdb_data.preproc_ns),
							GFP_KERNEL);
	if (acdb_data.preproc_ns == NULL) {
		MM_ERR("ACDB=> Could not allocate preproc ns memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		kfree(acdb_data.preproc_agc);
		kfree(acdb_data.preproc_iir);
		result = -ENOMEM;
		goto done;
	}
done:
	return result;
}

static u8 check_device_change(struct dev_evt_msg device_info)
{
	if (!acdb_data.device_info) {
		MM_ERR("not pointing to previous valid device detail\n");
		return 1; /*device info will not be pointing to*/
			/* valid device when acdb driver comes up*/
	}
	if ((device_info.sample_rate ==
				acdb_data.device_info->sample_rate) &&
		(device_info.acdb_id == acdb_data.device_info->acdb_id)) {
		return 0;
	}
	return 1;
}

static void device_cb(struct dev_evt_msg *evt, void *private)
{
	struct cad_device_info_type dev_type;
	struct acdb_cache_node *acdb_cache_free_node =  NULL;
	u32 session_id = 0;
	u8 ret = 0;
	u8 device_change = 0;

	/*if session value is zero it indicates that device call back is for
	voice call we will drop the request as acdb values for voice call is
	not applied from acdb driver*/
	if (!evt->session_info) {
		MM_DBG("no active sessions and call back is for"\
				" voice call\n");
		goto done;
	}

	if ((evt->dev_type.rx_device) &&
			(evt->acdb_id == PSEUDO_ACDB_ID)) {
		MM_INFO("device cb is for rx device with pseudo acdb id\n");
		goto done;
	}
	dev_type = evt->dev_type;
	MM_DBG("sample_rate = %d\n", evt->sample_rate);
	MM_DBG("acdb_id = %d\n", evt->acdb_id);
	MM_DBG("sessions = %d\n", evt->session_info);
	MM_DBG("acdb_state = %x\n", acdb_data.acdb_state);
	mutex_lock(&acdb_data.acdb_mutex);
	device_change = check_device_change(*evt);
	if (!device_change) {
		if (dev_type.tx_device) {
			if (!(acdb_data.acdb_state & AUDREC_READY))
				acdb_data.audrec_applied &= ~AUDREC_READY;

			acdb_data.acdb_state &= ~CAL_DATA_READY;
			goto update_cache;
		}
	} else
		/* state is updated to query the modem for values */
		acdb_data.acdb_state &= ~CAL_DATA_READY;

update_cache:
	if (dev_type.tx_device) {
		/*Only one recording session possible*/
		session_id = 0;
		acdb_cache_free_node =	&acdb_cache_tx;
		ret  = check_device_info_already_present(
				*evt,
				acdb_cache_free_node);
		acdb_cache_free_node->stream_id = session_id;
		acdb_data.cur_tx_session = session_id;
	} else {
		acdb_cache_free_node = &acdb_cache_rx;
		ret = check_device_info_already_present(*evt,
						acdb_cache_free_node);
		if (ret == 1) {
			MM_DBG("got device ready call back for another "\
					"audplay task sessions on same COPP\n");
			mutex_unlock(&acdb_data.acdb_mutex);
			goto done;
		}
	}
	update_acdb_data_struct(acdb_cache_free_node);
	acdb_data.device_cb_compl = 1;
	mutex_unlock(&acdb_data.acdb_mutex);
	wake_up(&acdb_data.wait);
done:
	return;
}

static s32 register_device_cb(void)
{
	s32 result = 0;
	acdb_data.dev_cb.func = device_cb;
	acdb_data.dev_cb.private = (void *)&acdb_data;

	result = audmgr_register_device_info_callback(&acdb_data.dev_cb);

	if (result) {
		MM_ERR("ACDB=> Could not register device callback\n");
		result = -ENODEV;
		goto done;
	}
done:
	return result;
}

static void audpp_cb(void *private, u32 id, u16 *msg)
{
	MM_DBG("\n");

	if (id == AUDPP_MSG_PP_DISABLE_FEEDBACK) {
		acdb_data.audpp_disabled_features |=
			((uint32_t)(msg[AUDPP_DISABLE_FEATS_MSW] << 16) |
			 msg[AUDPP_DISABLE_FEATS_LSW]);
		MM_INFO("AUDPP disable feedback: %x",
				acdb_data.audpp_disabled_features);
		goto done;
	} else if (id == AUDPP_MSG_PP_FEATS_RE_ENABLE) {
		MM_INFO("AUDPP re-enable messaage: %x",
				acdb_data.audpp_disabled_features);
		acdb_data.audpp_cb_reenable_compl = 1;
		wake_up(&acdb_data.wait);
		return;
	}

	if (id != AUDPP_MSG_CFG_MSG)
		goto done;

	if (msg[0] == AUDPP_MSG_ENA_DIS) {
		if (--acdb_cache_rx.stream_id <= 0) {
			acdb_data.acdb_state &= ~AUDPP_READY;
			acdb_cache_rx.stream_id = 0;
			MM_DBG("AUDPP_MSG_ENA_DIS\n");
		}
		goto done;
	}
	/*stream_id is used to keep track of number of active*/
	/*sessions active on this device*/
	acdb_cache_rx.stream_id++;

	acdb_data.acdb_state |= AUDPP_READY;
	acdb_data.audpp_cb_compl = 1;
	wake_up(&acdb_data.wait);
done:
	return;
}

static s8 handle_audpreproc_cb(void)
{
	struct acdb_cache_node *acdb_cached_values;
	s8 result = 0;
	u8 stream_id = acdb_data.preproc_stream_id;
	acdb_data.preproc_cb_compl = 0;
	acdb_cached_values = get_acdb_values_from_cache_tx(stream_id);
	if (acdb_cached_values == NULL) {
		MM_DBG("ERROR: to get chached acdb values\n");
		return -EPERM;
	}
	update_acdb_data_struct(acdb_cached_values);

	if (session_info.sampling_freq)
		acdb_data.device_info->sample_rate =
			session_info.sampling_freq;

	if (!(acdb_data.acdb_state & CAL_DATA_READY)) {
		result = check_tx_acdb_values_cached();
		if (result) {
			result = acdb_get_calibration();
			if (result < 0) {
				MM_ERR("failed to get calibration data\n");
				return result;
			}
		}
		acdb_cached_values->node_status = ACDB_VALUES_FILLED;
	}
	return result;
}

static void audpreproc_cb(void *private, u32 id, void *event_data)
{
	u8 result = 0;
	uint16_t *msg = event_data;
	int stream_id = 0; /* Only single tunnel mode recording supported */
	if (id != AUDPREPROC_MSG_CMD_CFG_DONE_MSG)
		goto done;

	acdb_data.preproc_stream_id = stream_id;
	get_audrec_session_info(&session_info);
	MM_DBG("status_flag = %x\n", msg[0]);
	if (msg[0]  == AUDPREPROC_MSG_STATUS_FLAG_DIS) {
		acdb_data.acdb_state &= ~AUDREC_READY;
		acdb_cache_tx.node_status =\
						ACDB_VALUES_NOT_FILLED;
		acdb_data.acdb_state &= ~CAL_DATA_READY;
		goto done;
	}
	/*Following check is added to make sure that device info
	  is updated. audpre proc layer enabled without device
	  callback at this scenario we should not access
	  device information
	 */
	if (acdb_data.device_info &&
			session_info.sampling_freq) {
		acdb_data.device_info->sample_rate =
			session_info.sampling_freq;
		result = check_tx_acdb_values_cached();
		if (!result) {
			MM_INFO("acdb values for the stream is" \
					" querried from modem");
			acdb_data.acdb_state |= CAL_DATA_READY;
		} else {
			acdb_data.acdb_state &= ~CAL_DATA_READY;
		}
	}
	acdb_data.acdb_state |= AUDREC_READY;

	acdb_data.preproc_cb_compl = 1;
	MM_DBG("acdb_data.acdb_state = %x\n", acdb_data.acdb_state);
	wake_up(&acdb_data.wait);
done:
	return;
}

static s32 register_audpp_cb(void)
{
	s32 result = 0;

	acdb_data.audpp_cb.fn = audpp_cb;
	acdb_data.audpp_cb.private = NULL;
	result = audpp_register_event_callback(&acdb_data.audpp_cb);
	if (result) {
		MM_ERR("ACDB=> Could not register audpp callback\n");
		result = -ENODEV;
		goto done;
	}
done:
	return result;
}

static s32 register_audpreproc_cb(void)
{
	s32 result = 0;

	acdb_data.audpreproc_cb.fn = audpreproc_cb;
	acdb_data.audpreproc_cb.private = NULL;
	result = audpreproc_register_event_callback(&acdb_data.audpreproc_cb);
	if (result) {
		MM_ERR("ACDB=> Could not register audpreproc callback\n");
		result = -ENODEV;
		goto done;
	}

done:
	return result;
}

static s32 acdb_initialize_data(void)
{
	s32	result = 0;

	mutex_init(&acdb_data.acdb_mutex);

	result = initialize_rpc();
	if (result)
		goto err;

	result = initialize_memory();
	if (result)
		goto err1;

	result = register_device_cb();
	if (result)
		goto err2;

	result = register_audpp_cb();
	if (result)
		goto err3;

	result = register_audpreproc_cb();
	if (result)
		goto err4;


	return result;

err4:
	result = audpreproc_unregister_event_callback(&acdb_data.audpreproc_cb);
	if (result)
		MM_ERR("ACDB=> Could not unregister audpreproc callback\n");
err3:
	result = audpp_unregister_event_callback(&acdb_data.audpp_cb);
	if (result)
		MM_ERR("ACDB=> Could not unregister audpp callback\n");
err2:
	result = audmgr_deregister_device_info_callback(&acdb_data.dev_cb);
	if (result)
		MM_ERR("ACDB=> Could not unregister device callback\n");
err1:
	daldevice_detach(acdb_data.handle);
	acdb_data.handle = NULL;
err:
	return result;
}

static s32 acdb_calibrate_device(void *data)
{
	s32 result = 0;

	/* initialize driver */
	result = acdb_initialize_data();
	if (result)
		goto done;

	while (!kthread_should_stop()) {
		MM_DBG("Waiting for call back events\n");
		wait_event_interruptible(acdb_data.wait,
					(acdb_data.device_cb_compl
					| acdb_data.audpp_cb_compl
					| acdb_data.audpp_cb_reenable_compl
					| acdb_data.preproc_cb_compl));
		mutex_lock(&acdb_data.acdb_mutex);
		if (acdb_data.device_cb_compl) {
			acdb_data.device_cb_compl = 0;
			if (!(acdb_data.acdb_state & CAL_DATA_READY)) {
				if (acdb_data.device_info->dev_type.rx_device) {
					/*we need to get calibration values
					only for RX device as resampler
					moved to start of the pre - proc chain
					tx calibration value will be based on
					sampling frequency what audrec is
					configured, calibration values for tx
					device are fetch in audpreproc
					callback*/
					result = acdb_get_calibration();
					if (result < 0) {
						mutex_unlock(
							&acdb_data.acdb_mutex);
						MM_ERR("Not able to get "\
							"calibration "\
							"data continue\n");
						continue;
					}
				}
			}
			MM_DBG("acdb state = %d\n",
					 acdb_data.acdb_state);
			if (acdb_data.device_info->dev_type.tx_device)
				handle_tx_device_ready_callback();
			else {
				if (acdb_data.audpp_cb_reenable_compl) {
					MM_INFO("Reset disabled feature flag");
					acdb_data.audpp_disabled_features = 0;
					acdb_data.audpp_cb_reenable_compl = 0;
				}
				acdb_cache_rx.node_status =\
						ACDB_VALUES_FILLED;
				if (acdb_data.acdb_state &
						AUDPP_READY) {
					MM_DBG("AUDPP already enabled "\
							"apply acdb values\n");
					goto apply;
				}
			}
		}

		if (!(acdb_data.audpp_cb_compl ||
				acdb_data.audpp_cb_reenable_compl ||
				acdb_data.preproc_cb_compl)) {
			MM_DBG("need to wait for either AUDPP / AUDPREPROC "\
					"Event\n");
			mutex_unlock(&acdb_data.acdb_mutex);
			continue;
		} else {
			MM_DBG("got audpp / preproc call back\n");
			if (acdb_data.audpp_cb_compl) {
				if (acdb_data.audpp_cb_reenable_compl) {
					MM_INFO("Reset disabled feature flag");
					acdb_data.audpp_disabled_features = 0;
					acdb_data.audpp_cb_reenable_compl = 0;
				}
				send_acdb_values_for_active_devices();
				acdb_data.audpp_cb_compl = 0;
				mutex_unlock(&acdb_data.acdb_mutex);
				continue;
			} else if (acdb_data.audpp_cb_reenable_compl) {
				acdb_re_enable_audpp();
				acdb_data.audpp_disabled_features = 0;
				acdb_data.audpp_cb_reenable_compl = 0;
				mutex_unlock(&acdb_data.acdb_mutex);
				continue;
			} else {
				result = handle_audpreproc_cb();
				if (result < 0) {
					mutex_unlock(&acdb_data.acdb_mutex);
					continue;
				}
			}
		}
apply:
		if (acdb_data.acdb_state & CAL_DATA_READY)
			result = acdb_send_calibration();

		mutex_unlock(&acdb_data.acdb_mutex);
	}
done:
	return 0;
}

static int __init acdb_init(void)
{

	s32 result = 0;

	memset(&acdb_data, 0, sizeof(acdb_data));
	spin_lock_init(&acdb_data.dsp_lock);
	init_waitqueue_head(&acdb_data.wait);
	acdb_data.cb_thread_task = kthread_run(acdb_calibrate_device,
		NULL, "acdb_cb_thread");

	if (IS_ERR(acdb_data.cb_thread_task)) {
		MM_ERR("ACDB=> Could not register cb thread\n");
		result = -ENODEV;
		goto err;
	}

#ifdef CONFIG_DEBUG_FS
	/*This is RTC specific INIT used only with debugfs*/
	if (!rtc_acdb_init())
		MM_ERR("RTC ACDB=>INIT Failure\n");

#endif

	return misc_register(&acdb_misc);
err:
	return result;
}

static void __exit acdb_exit(void)
{
	s32	result = 0;

	result = audmgr_deregister_device_info_callback(&acdb_data.dev_cb);
	if (result)
		MM_ERR("ACDB=> Could not unregister device callback\n");

	result = audpp_unregister_event_callback(&acdb_data.audpp_cb);
	if (result)
		MM_ERR("ACDB=> Could not unregister audpp callback\n");

	result = audpreproc_unregister_event_callback(&acdb_data.\
				audpreproc_cb);
	if (result)
		MM_ERR("ACDB=> Could not unregister audpreproc callback\n");

	result = kthread_stop(acdb_data.cb_thread_task);
	if (result)
		MM_ERR("ACDB=> Could not stop kthread\n");

	free_memory_acdb_get_blk();

	iounmap(acdb_cache_tx.map_v_addr);
	free_contiguous_memory_by_paddr(
			acdb_cache_tx.phys_addr_acdb_values);
	iounmap(acdb_cache_rx.map_v_addr);
	free_contiguous_memory_by_paddr(
			acdb_cache_rx.phys_addr_acdb_values);
	kfree(acdb_data.device_info);
	kfree(acdb_data.pp_iir);
	kfree(acdb_data.pp_mbadrc);
	kfree(acdb_data.preproc_agc);
	kfree(acdb_data.preproc_iir);
	kfree(acdb_data.preproc_ns);
	mutex_destroy(&acdb_data.acdb_mutex);
	memset(&acdb_data, 0, sizeof(acdb_data));
	#ifdef CONFIG_DEBUG_FS
	rtc_acdb_deinit();
	#endif
}

late_initcall(acdb_init);
module_exit(acdb_exit);

MODULE_DESCRIPTION("MSM 8x25 Audio ACDB driver");
MODULE_LICENSE("GPL v2");
