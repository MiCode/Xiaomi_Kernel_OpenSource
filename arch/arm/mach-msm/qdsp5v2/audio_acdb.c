/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
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
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/socinfo.h>
#include <mach/qdsp5v2/audpreproc.h>
#include <mach/qdsp5v2/qdsp5audppcmdi.h>
#include <mach/qdsp5v2/qdsp5audpreproccmdi.h>
#include <mach/qdsp5v2/qdsp5audpreprocmsg.h>
#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/afe.h>
#include <mach/qdsp5v2/audio_acdbi.h>
#include <mach/qdsp5v2/acdb_commands.h>
#include <mach/qdsp5v2/audio_acdb_def.h>
#include <mach/debug_mm.h>
#include <mach/msm_memtypes.h>

/* this is the ACDB device ID */
#define DALDEVICEID_ACDB		0x02000069
#define ACDB_PORT_NAME			"DAL00"
#define ACDB_CPU			SMD_APPS_MODEM
#define ACDB_BUF_SIZE			4096
#define PBE_BUF_SIZE                    (33*1024)
#define FLUENCE_BUF_SIZE	498

#define ACDB_VALUES_NOT_FILLED		0
#define ACDB_VALUES_FILLED		1
#define MAX_RETRY			10

/*below macro is used to align the session info received from
Devctl driver with the state mentioned as not to alter the
Existing code*/
#define AUDREC_OFFSET	2
/* rpc table index */
enum {
	ACDB_DalACDB_ioctl = DALDEVICE_FIRST_DEVICE_API_IDX
};

enum {
	CAL_DATA_READY	= 0x1,
	AUDPP_READY	= 0x2,
	AUDREC0_READY	= 0x4,
	AUDREC1_READY	= 0x8,
	AUDREC2_READY	= 0x10,
};


struct acdb_data {
	void *handle;

	u32 phys_addr;
	u8 *virt_addr;

	struct task_struct *cb_thread_task;
	struct auddev_evt_audcal_info *device_info;

	u32 acdb_state;
	struct audpp_event_callback audpp_cb;
	struct audpreproc_event_callback audpreproc_cb;

	struct audpp_cmd_cfg_object_params_pcm *pp_iir;
	struct audpp_cmd_cfg_cal_gain *calib_gain_rx;
	struct audpp_cmd_cfg_pbe *pbe_block;
	struct audpp_cmd_cfg_object_params_mbadrc *pp_mbadrc;
	struct audpreproc_cmd_cfg_agc_params *preproc_agc;
	struct audpreproc_cmd_cfg_iir_tuning_filter_params *preproc_iir;
	struct audpreproc_cmd_cfg_cal_gain *calib_gain_tx;
	struct acdb_mbadrc_block mbadrc_block;
	struct audpreproc_cmd_cfg_lvnv_param preproc_lvnv;

	wait_queue_head_t wait;
	struct mutex acdb_mutex;
	u32 device_cb_compl;
	u32 audpp_cb_compl;
	u32 preproc_cb_compl;
	u8 preproc_stream_id;
	u8 audrec_applied;
	u32 multiple_sessions;
	u32 cur_tx_session;
	struct acdb_result acdb_result;
	u16 *pbe_extbuff;
	u16 *pbe_enable_flag;
	u32 fluence_extbuff;
	u8 *fluence_extbuff_virt;
	void *map_v_fluence;

	struct acdb_pbe_block *pbe_blk;

	spinlock_t dsp_lock;
	int dec_id;
	struct audpp_cmd_cfg_object_params_eqalizer eq;
	 /*status to enable or disable the fluence*/
	int fleuce_feature_status[MAX_AUDREC_SESSIONS];
	struct audrec_session_info session_info;
	/*pmem info*/
	int pmem_fd;
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long pmem_len;
	struct file *file;
	/* pmem for get acdb blk */
	unsigned long	get_blk_paddr;
	u8		*get_blk_kvaddr;
	void *map_v_get_blk;
	char *build_id;
};

static struct acdb_data		acdb_data;

struct acdb_cache_node {
	u32 node_status;
	s32 stream_id;
	u32 phys_addr_acdb_values;
	void *map_v_addr;
	u8 *virt_addr_acdb_values;
	struct auddev_evt_audcal_info device_info;
};

/*for RX devices  acdb values are applied based on copp ID so
the depth of tx cache is MAX number of COPP supported in the system*/
struct acdb_cache_node acdb_cache_rx[MAX_COPP_NODE_SUPPORTED];

/*for TX devices acdb values are applied based on AUDREC session and
the depth of the tx cache is define by number of AUDREC sessions supported*/
struct acdb_cache_node acdb_cache_tx[MAX_AUDREC_SESSIONS];

/*Audrec session info includes Attributes Sampling frequency and enc_id */
struct audrec_session_info session_info[MAX_AUDREC_SESSIONS];
#ifdef CONFIG_DEBUG_FS

#define RTC_MAX_TIMEOUT 500 /* 500 ms */
#define PMEM_RTC_ACDB_QUERY_MEM 4096
#define EXTRACT_HIGH_WORD(x) ((x & 0xFFFF0000)>>16)
#define EXTRACT_LOW_WORD(x) (0x0000FFFF & x)
#define	ACDB_RTC_TX 0xF1
#define	ACDB_RTC_RX 0x1F


static u32 acdb_audpp_entry[][4] = {

  { ABID_AUDIO_RTC_VOLUME_PAN_RX,\
    IID_AUDIO_RTC_VOLUME_PAN_PARAMETERS,\
    AUDPP_CMD_VOLUME_PAN,\
    ACDB_RTC_RX
   },
  { ABID_AUDIO_IIR_RX,\
     IID_AUDIO_IIR_COEFF,\
     AUDPP_CMD_IIR_TUNING_FILTER,
     ACDB_RTC_RX
   },
  { ABID_AUDIO_RTC_EQUALIZER_PARAMETERS,\
     IID_AUDIO_RTC_EQUALIZER_PARAMETERS,\
     AUDPP_CMD_EQUALIZER,\
     ACDB_RTC_RX
   },
  { ABID_AUDIO_RTC_SPA,\
     IID_AUDIO_RTC_SPA_PARAMETERS,\
     AUDPP_CMD_SPECTROGRAM,
     ACDB_RTC_RX
   },
  { ABID_AUDIO_STF_RX,\
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
	signed int ExtBuff[196];
} __attribute__((packed));

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
	MM_INFO("GET-SET ABID Open debug intf %s\n",
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
	MM_INFO("SET ABID : Cmd ID: %d Device:%d ABID:%d IID : %d cnt: %d\n",\
		write_abid.cmd_id, write_abid.acdb_id,
		write_abid.set_abid, write_abid.set_iid, cnt);
	if (write_abid.acdb_id > ACDB_ID_MAX ||
		write_abid.acdb_id < ACDB_ID_HANDSET_SPKR){
		rtc_acdb.err = ACDB_RTC_ERR_INVALID_DEVICE;
		return cnt;
	}
	if (!is_dev_opened(write_abid.acdb_id))	{
		rtc_acdb.err = ACDB_RTC_ERR_DEVICE_INACTIVE;
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

void acdb_rtc_set_err(u32 ErrCode)
{
	if (rtc_acdb.err == ACDB_RTC_PENDING_RESPONSE) {
		if (ErrCode == 0xFFFF) {
			rtc_acdb.err = ACDB_RTC_SUCCESS;
			MM_INFO("RTC READ SUCCESS---\n");
		} else if (ErrCode == 0) {
			rtc_acdb.err = ACDB_RTC_DSP_FAILURE;
			MM_INFO("RTC READ FAIL---\n");
		} else if (ErrCode == 1) {
			rtc_acdb.err = ACDB_RTC_DSP_FEATURE_NOT_AVAILABLE;
			MM_INFO("RTC READ FEAT UNAVAILABLE---\n");
		} else {
			rtc_acdb.err = ACDB_RTC_DSP_FAILURE;
			MM_ERR("RTC Err CODE---\n");
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
			MM_ERR("ACDB DATA READ ---\
				INVALID READ LEN %x\n", count);
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
				rtc_read_cmd.route_id =
					acdb_data.device_info->dev_id;
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
				MM_INFO("ACDB READ Command RC --->%x\
					Route ID=%x\n", rc,\
					acdb_data.device_info->dev_id);
			} else if (rtc_acdb.tx_rx_ctl == ACDB_RTC_TX) {
				struct rtc_audpreproc_read_data rtc_audpreproc;
				rtc_audpreproc.cmd_id =
					AUDPREPROC_CMD_FEAT_QUERY_PARAMS;
				rtc_audpreproc.stream_id =
					acdb_data.preproc_stream_id;
				rtc_audpreproc.feature_id = rtc_acdb.set_abid;
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
				MM_INFO("ACDB READ Command RC --->%x,\
					stream_id %x\n", rc,\
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
	struct audpreproc_cmd_cfg_iir_tuning_filter_params *preproc_iir;
	struct audpreproc_cmd_cfg_agc_params *preproc_agc;
	struct audpreproc_cmd_cfg_ns_params *preproc_ns;
	s32	result = 0;
	bool retval = false;
	unsigned short iircmdsize =
		sizeof(struct audpreproc_cmd_cfg_iir_tuning_filter_params);
	unsigned short iircmdid = AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS;

	rtc_acdb.err = ACDB_RTC_ERR_UNKNOWN_FAILURE;

	switch (rtc_acdb.set_abid) {

	case AUDPREPROC_CMD_CFG_AGC_PARAMS:
	case AUDPREPROC_CMD_CFG_AGC_PARAMS_2:
	{
		preproc_agc = kmalloc(sizeof(\
					struct audpreproc_cmd_cfg_agc_params),\
					GFP_KERNEL);
		if ((sizeof(struct audpreproc_cmd_cfg_agc_params) -\
			(2*sizeof(unsigned short)))
			< writecount) {
				MM_ERR("ACDB DATA WRITE --\
					AGC TX writecount > DSP struct\n");
		} else {
			if (preproc_agc != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)preproc_agc;
				offset = offsetof(struct \
						audpreproc_cmd_cfg_agc_params,\
						tx_agc_param_mask);
				offset_addr = (unsigned short *)(base + offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					preproc_agc->cmd_id =
						AUDPREPROC_CMD_CFG_AGC_PARAMS;
					preproc_agc->stream_id =
						acdb_data.preproc_stream_id;
					result = audpreproc_dsp_set_agc(
						preproc_agc,
						sizeof(struct \
						audpreproc_cmd_cfg_agc_params));
					if (result) {
						MM_ERR("ACDB=> Failed to \
							send AGC data to \
							preproc)\n");
					} else {
						retval = true;
					       }
				} else {
					MM_ERR("ACDB DATA WRITE ---\
						GC Tx copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --\
					AGC TX kalloc Failed LEN\n");
			}
		}
		if (preproc_agc != NULL)
			kfree(preproc_agc);
		break;
	}
	case AUDPREPROC_CMD_CFG_NS_PARAMS:
	{

		preproc_ns = kmalloc(sizeof(struct \
					audpreproc_cmd_cfg_ns_params),\
					GFP_KERNEL);
		if ((sizeof(struct audpreproc_cmd_cfg_ns_params) -\
				(2 * sizeof(unsigned short)))
				< writecount) {
				MM_ERR("ACDB DATA WRITE --\
					NS TX writecount > DSP struct\n");
		} else {
			if (preproc_ns != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)preproc_ns;
				offset = offsetof(struct \
						audpreproc_cmd_cfg_ns_params,\
						ec_mode_new);
				offset_addr = (unsigned short *)(base + offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					preproc_ns->cmd_id =
						AUDPREPROC_CMD_CFG_NS_PARAMS;
					preproc_ns->stream_id =
						acdb_data.preproc_stream_id;
					result = audpreproc_dsp_set_ns(
						preproc_ns,
						sizeof(struct \
						audpreproc_cmd_cfg_ns_params));
					if (result) {
						MM_ERR("ACDB=> Failed to send \
							NS data to preproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---NS Tx \
						copy_from_user Fail\n");
					}
			} else {
				MM_ERR("ACDB DATA WRITE --NS TX\
					kalloc Failed LEN\n");
			}
		}
		if (preproc_ns != NULL)
			kfree(preproc_ns);
		break;
	}
	case AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS:
	{

		preproc_iir = kmalloc(sizeof(struct \
				audpreproc_cmd_cfg_iir_tuning_filter_params),\
				GFP_KERNEL);
		if ((sizeof(struct \
			audpreproc_cmd_cfg_iir_tuning_filter_params)-\
			(2 * sizeof(unsigned short)))
			< writecount) {
			MM_ERR("ACDB DATA WRITE --IIR TX writecount\
						> DSP struct\n");
		} else {
			if (preproc_iir != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)preproc_iir;
				offset = offsetof(struct \
				audpreproc_cmd_cfg_iir_tuning_filter_params,\
				active_flag);
				offset_addr = (unsigned short *)(base + \
						offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					preproc_iir->cmd_id = iircmdid;
					preproc_iir->stream_id =
						acdb_data.preproc_stream_id;
					result = audpreproc_dsp_set_iir(\
							preproc_iir,
							iircmdsize);
					if (result) {
						MM_ERR("ACDB=> Failed to send\
						IIR data to preproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---IIR Tx \
						copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --IIR TX kalloc \
					Failed LEN\n");
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

	struct audpp_cmd_cfg_object_params_volpan *volpan_config;
	struct audpp_cmd_cfg_object_params_mbadrc *mbadrc_config;
	struct acdb_block_mbadrc_rtc *acdb_mbadrc_rtc;
	struct audpp_cmd_cfg_object_params_sidechain *stf_config;
	struct audpp_cmd_cfg_object_params_spectram *spa_config;
	struct audpp_cmd_cfg_object_params_eqalizer *eq_config;
	struct audpp_cmd_cfg_object_params_pcm *iir_config;
	unsigned short temp_spa[34];
	struct rtc_acdb_pmem *rtc_write = &rtc_acdb.rtc_write;
	s32	result = 0;
	bool retval = false;

	switch (rtc_acdb.set_abid) {
	case AUDPP_CMD_VOLUME_PAN:
	{
		volpan_config =  kmalloc(sizeof(struct \
					 audpp_cmd_cfg_object_params_volpan),\
					 GFP_KERNEL);
		if ((sizeof(struct audpp_cmd_cfg_object_params_volpan) -\
			sizeof(struct audpp_cmd_cfg_object_params_common))
			< writecount) {
			MM_ERR("ACDB DATA WRITE --\
				VolPan writecount > DSP struct\n");
		} else {
			if (volpan_config != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)volpan_config;
				offset = offsetof(struct \
					audpp_cmd_cfg_object_params_volpan,\
					volume);
				offset_addr = (unsigned short *)(base+offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					MM_ERR("ACDB RX WRITE DATA:\
						AUDPP_CMD_VOLUME_PAN\n");
					result = audpp_set_volume_and_pan(
						acdb_data.device_info->dev_id,\
						volpan_config->volume,
						volpan_config->pan,
						COPP);
					if (result) {
						MM_ERR("ACDB=> Failed to \
							send VOLPAN data to"
							" postproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---\
						copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --\
					Vol Pan kalloc Failed LEN\n");
			}
		}
	if (volpan_config != NULL)
		kfree(volpan_config);
	break;
	}

	case AUDPP_CMD_IIR_TUNING_FILTER:
	{
		iir_config =  kmalloc(sizeof(struct \
				audpp_cmd_cfg_object_params_pcm),\
				GFP_KERNEL);
		if ((sizeof(struct audpp_cmd_cfg_object_params_pcm) -\
			sizeof(struct audpp_cmd_cfg_object_params_common))
			< writecount) {
			MM_ERR("ACDB DATA WRITE --\
					IIR RX writecount > DSP struct\n");
		} else {
			if (iir_config != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)iir_config;
				offset = offsetof(struct \
					audpp_cmd_cfg_object_params_pcm,\
					active_flag);
				offset_addr = (unsigned short *)(base+offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {

					iir_config->common.cmd_id =
						AUDPP_CMD_CFG_OBJECT_PARAMS;
					iir_config->common.stream =
						AUDPP_CMD_COPP_STREAM;
					iir_config->common.stream_id = 0;
					iir_config->common.obj_cfg =
						AUDPP_CMD_OBJ0_UPDATE;
					iir_config->common.command_type = 0;
					MM_ERR("ACDB RX WRITE DATA:\
					AUDPP_CMD_IIR_TUNING_FILTER\n");
					result = audpp_dsp_set_rx_iir(
						acdb_data.device_info->dev_id,
						iir_config->active_flag,\
						iir_config, COPP);
					if (result) {
						MM_ERR("ACDB=> Failed to send\
							IIR data to\
							postproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---\
						IIR Rx copy_from_user Fail\n");
				      }
			 } else {
				MM_ERR("ACDB DATA WRITE --\
					acdb_iir_block kalloc Failed LEN\n");
			}
		}
		if (iir_config != NULL)
			kfree(iir_config);
		break;
	}
	case AUDPP_CMD_EQUALIZER:
	{
		eq_config =  kmalloc(sizeof(struct \
				audpp_cmd_cfg_object_params_eqalizer),\
				GFP_KERNEL);
	if ((sizeof(struct audpp_cmd_cfg_object_params_eqalizer) -\
			sizeof(struct audpp_cmd_cfg_object_params_common))
			< writecount) {
			MM_ERR("ACDB DATA WRITE --\
			EQ RX writecount > DSP struct\n");
		} else {
			if (eq_config != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)eq_config;
				offset = offsetof(struct \
					audpp_cmd_cfg_object_params_eqalizer,\
					eq_flag);
				offset_addr = (unsigned short *)(base+offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					eq_config->common.cmd_id =
						AUDPP_CMD_CFG_OBJECT_PARAMS;
					eq_config->common.stream =
						AUDPP_CMD_COPP_STREAM;
					eq_config->common.stream_id = 0;
					eq_config->common.obj_cfg =
						AUDPP_CMD_OBJ0_UPDATE;
					eq_config->common.command_type = 0;
					MM_ERR("ACDB RX WRITE\
					DATA:AUDPP_CMD_EQUALIZER\n");
					result = audpp_dsp_set_eq(
						acdb_data.device_info->dev_id,
						eq_config->eq_flag,\
						eq_config,
						COPP);
					if (result) {
						MM_ERR("ACDB=> Failed to \
						send EQ data to postproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---\
					EQ Rx copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --\
					EQ kalloc Failed LEN\n");
			}
		}
		if (eq_config != NULL)
			kfree(eq_config);
		break;
	}

	case AUDPP_CMD_SPECTROGRAM:
	{
		spa_config =  kmalloc(sizeof(struct \
				audpp_cmd_cfg_object_params_spectram),\
				GFP_KERNEL);
		if ((sizeof(struct audpp_cmd_cfg_object_params_spectram)-\
				sizeof(struct \
				audpp_cmd_cfg_object_params_common))
				< (2 * sizeof(unsigned short))) {
					MM_ERR("ACDB DATA WRITE --SPA \
					RX writecount > DSP struct\n");
		} else {
			if (spa_config != NULL) {
				if ((copy_from_user(&temp_spa[0],\
					(void *)ubuf,
					(34 * sizeof(unsigned short))))
					== 0x00) {
					spa_config->common.cmd_id =
						AUDPP_CMD_CFG_OBJECT_PARAMS;
					spa_config->common.stream =
						AUDPP_CMD_COPP_STREAM;
					spa_config->common.stream_id = 0;
					spa_config->common.obj_cfg =
						AUDPP_CMD_OBJ0_UPDATE;
					spa_config->common.command_type = 0;
					spa_config->sample_interval =
						temp_spa[0];
					spa_config->num_coeff = temp_spa[1];
					MM_ERR("ACDB RX WRITE DATA:\
						AUDPP_CMD_SPECTROGRAM\n");
					result = audpp_dsp_set_spa(
						acdb_data.device_info->dev_id,\
						spa_config, COPP);
					if (result) {
						MM_ERR("ACDB=> Failed to \
							send SPA data \
							to postproc\n");
					} else {
						retval = true;
					      }
				} else {
					MM_ERR("ACDB DATA WRITE \
					---SPA Rx copy_from_user\
					Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE --\
				SPA kalloc Failed LEN\n");
			       }
			}
		if (spa_config != NULL)
			kfree(spa_config);
	break;
	}
	case AUDPP_CMD_MBADRC:
	{
		acdb_mbadrc_rtc =  kmalloc(sizeof(struct \
					acdb_block_mbadrc_rtc),\
					GFP_KERNEL);
		mbadrc_config =  kmalloc(sizeof(struct \
					audpp_cmd_cfg_object_params_mbadrc),\
					GFP_KERNEL);
		if (mbadrc_config != NULL && acdb_mbadrc_rtc != NULL) {
			if ((copy_from_user(acdb_mbadrc_rtc,\
				(void *)ubuf,
				sizeof(struct acdb_block_mbadrc_rtc)))
				== 0x00) {
				mbadrc_config->common.cmd_id =
					AUDPP_CMD_CFG_OBJECT_PARAMS;
				mbadrc_config->common.stream =
					AUDPP_CMD_COPP_STREAM;
				mbadrc_config->common.stream_id = 0;
				mbadrc_config->common.obj_cfg =
					AUDPP_CMD_OBJ0_UPDATE;
				mbadrc_config->common.command_type = 0;
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
					acdb_mbadrc_rtc->ExtBuff,
					(196*sizeof(signed int)));
				result = audpp_dsp_set_mbadrc(
						acdb_data.device_info->dev_id,
						mbadrc_config->enable,
						mbadrc_config, COPP);
				if (result) {
					MM_ERR("ACDB=> Failed to \
						Send MBADRC data \
						to postproc\n");
				} else {
					retval = true;
				}
			} else {
				MM_ERR("ACDB DATA WRITE ---\
					MBADRC Rx copy_from_user Fail\n");
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
	case AUDPP_CMD_SIDECHAIN_TUNING_FILTER:
	{
		stf_config =  kmalloc(sizeof(struct \
				audpp_cmd_cfg_object_params_sidechain),\
				GFP_KERNEL);
		if ((sizeof(struct audpp_cmd_cfg_object_params_sidechain) -\
			sizeof(struct audpp_cmd_cfg_object_params_common))
			< writecount) {
				MM_ERR("ACDB DATA WRITE --\
					STF RX writecount > DSP struct\n");
		} else {
			if (stf_config != NULL) {
				char *base; unsigned short offset;
				unsigned short *offset_addr;
				base = (char *)stf_config;
				offset = offsetof(struct \
					audpp_cmd_cfg_object_params_sidechain,\
					active_flag);
				offset_addr = (unsigned short *)(base+offset);
				if ((copy_from_user(offset_addr,\
					(void *)ubuf, writecount)) == 0x00) {
					stf_config->common.cmd_id =
						AUDPP_CMD_CFG_OBJECT_PARAMS;
					stf_config->common.stream =
						AUDPP_CMD_COPP_STREAM;
					stf_config->common.stream_id = 0;
					stf_config->common.obj_cfg =
						AUDPP_CMD_OBJ0_UPDATE;
					stf_config->common.command_type = 0;
					MM_ERR("ACDB RX WRITE DATA:\
					AUDPP_CMD_SIDECHAIN_TUNING_FILTER\n");
				result = audpp_dsp_set_stf(
						acdb_data.device_info->dev_id,\
						stf_config->active_flag,\
						stf_config, COPP);
					if (result) {
						MM_ERR("ACDB=> Failed to send \
						STF data to postproc\n");
					} else {
						retval = true;
					}
				} else {
					MM_ERR("ACDB DATA WRITE ---\
					STF Rx copy_from_user Fail\n");
				}
			} else {
				MM_ERR("ACDB DATA WRITE \
					STF kalloc Failed LEN\n");
		}
	}
	if (stf_config != NULL)
		kfree(stf_config);
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
	if (acdb_data.build_id[17] == '1') {
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
			MM_ERR("SET GET ABID DATA"
					" debugfs_create_file failed\n");
			return false;
		}
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
#endif /*CONFIG_DEBUG_FS*/
static s32 acdb_set_calibration_blk(unsigned long arg)
{
	struct acdb_cmd_device acdb_cmd;
	s32 result = 0;

	MM_DBG("acdb_set_calibration_blk\n");
	if (copy_from_user(&acdb_cmd, (struct acdb_cmd_device *)arg,
			sizeof(acdb_cmd))) {
		MM_ERR("Failed copy command struct from user in"
			"acdb_set_calibration_blk\n");
		return -EFAULT;
	}
	acdb_cmd.phys_buf = (u32 *)acdb_data.paddr;

	MM_DBG("acdb_cmd.phys_buf %x\n", (u32)acdb_cmd.phys_buf);

	result = dalrpc_fcn_8(ACDB_DalACDB_ioctl, acdb_data.handle,
			(const void *)&acdb_cmd, sizeof(acdb_cmd),
			&acdb_data.acdb_result,
			sizeof(acdb_data.acdb_result));

	if (result < 0) {
		MM_ERR("ACDB=> Device Set RPC failure"
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
		MM_ERR("Failed copy command struct from user in"
			"acdb_get_calibration_blk\n");
		return -EFAULT;
	}
	acdb_cmd.phys_buf = (u32 *)acdb_data.paddr;
	MM_ERR("acdb_cmd.phys_buf %x\n", (u32)acdb_cmd.phys_buf);

	result = dalrpc_fcn_8(ACDB_DalACDB_ioctl, acdb_data.handle,
			(const void *)&acdb_cmd, sizeof(acdb_cmd),
			&acdb_data.acdb_result,
			sizeof(acdb_data.acdb_result));

	if (result < 0) {
		MM_ERR("ACDB=> Device Get RPC failure"
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
		acdb_data.dec_id    = 0;
		rc = audpp_dsp_set_eq(acdb_data.dec_id, 1,
			&acdb_data.eq, COPP);
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
		result = dalrpc_fcn_8(ACDB_DalACDB_ioctl, acdb_data.handle,
				(const void *)&acdb_cmd, sizeof(acdb_cmd),
				&acdb_data.acdb_result,
				sizeof(acdb_data.acdb_result));

		if (result < 0) {
			MM_ERR("ACDB=> Device table RPC failure"
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
			MM_DBG("Modem query for acdb values is successful"
					" (iterations = %d)\n", iterations);
			acdb_data.acdb_state |= CAL_DATA_READY;
			return result;
		} else {
			MM_ERR("ACDB=> modem failed to fill acdb values,"
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

	result = dalrpc_fcn_8(ACDB_DalACDB_ioctl, acdb_data.handle,
			(const void *)&acdb_cmd, sizeof(acdb_cmd),
			&acdb_result,
			sizeof(acdb_result));

	if (result < 0) {
		MM_ERR("ACDB=> Device Get RPC failure"
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

static u8 check_device_info_already_present(
		struct auddev_evt_audcal_info   audcal_info,
			struct acdb_cache_node *acdb_cache_free_node)
{
	if ((audcal_info.dev_id ==
				acdb_cache_free_node->device_info.dev_id) &&
		(audcal_info.sample_rate ==
				acdb_cache_free_node->device_info.\
				sample_rate) &&
			(audcal_info.acdb_id ==
				acdb_cache_free_node->device_info.acdb_id)) {
		MM_DBG("acdb values are already present\n");
		/*if acdb state is not set for CAL_DATA_READY and node status
		is filled, acdb state should be updated with CAL_DATA_READY
		state*/
		acdb_data.acdb_state |= CAL_DATA_READY;
		/*checking for cache node status if it is not filled then the
		acdb values are not cleaned from node so update node status
		with acdb value filled*/
		if ((acdb_cache_free_node->node_status != ACDB_VALUES_FILLED) &&
			((audcal_info.dev_type & RX_DEVICE) == 1)) {
			MM_DBG("device was released earlier\n");
			acdb_cache_free_node->node_status = ACDB_VALUES_FILLED;
			return 2; /*node is presnet but status as not filled*/
		}
		return 1; /*node is present but status as filled*/
	}
	MM_DBG("copying device info into node\n");
	/*as device information is not present in cache copy
	the current device information into the node*/
	memcpy(&acdb_cache_free_node->device_info,
				 &audcal_info, sizeof(audcal_info));
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
		return -1;
	}
	memset(acdb_data.pp_iir, 0, sizeof(*acdb_data.pp_iir));

	acdb_data.pp_iir->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	acdb_data.pp_iir->common.stream = AUDPP_CMD_COPP_STREAM;
	acdb_data.pp_iir->common.stream_id = 0;
	acdb_data.pp_iir->common.obj_cfg = AUDPP_CMD_OBJ0_UPDATE;
	acdb_data.pp_iir->common.command_type = 0;

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
		MM_DBG("Got IID == IID_MBADRC_PARAMETERS\n");
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
		return -1;
	}

	memset(acdb_data.pp_mbadrc, 0, sizeof(*acdb_data.pp_mbadrc));

	acdb_data.pp_mbadrc->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	acdb_data.pp_mbadrc->common.stream = AUDPP_CMD_COPP_STREAM;
	acdb_data.pp_mbadrc->common.stream_id = 0;
	acdb_data.pp_mbadrc->common.obj_cfg = AUDPP_CMD_OBJ0_UPDATE;
	acdb_data.pp_mbadrc->common.command_type = 0;

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

static struct acdb_calib_gain_rx *get_audpp_cal_gain(void)
{
	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);
		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_CALIBRATION_GAIN_RX) {
				if (prs_hdr->iid ==
					IID_AUDIO_CALIBRATION_GAIN_RX) {
					MM_DBG("Got audpp_calib_gain_rx"
					" block\n");
					return (struct acdb_calib_gain_rx *)
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

static s32 acdb_fill_audpp_cal_gain(void)
{
	struct acdb_calib_gain_rx *acdb_calib_gain_rx = NULL;

	acdb_calib_gain_rx = get_audpp_cal_gain();
	if (acdb_calib_gain_rx == NULL) {
		MM_ERR("unable to find  audpp"
			" calibration gain block returning\n");
		return -1;
	}
	MM_DBG("Calibration value"
		" for calib_gain_rx %d\n", acdb_calib_gain_rx->audppcalgain);
	memset(acdb_data.calib_gain_rx, 0, sizeof(*acdb_data.calib_gain_rx));

	acdb_data.calib_gain_rx->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	acdb_data.calib_gain_rx->common.stream = AUDPP_CMD_COPP_STREAM;
	acdb_data.calib_gain_rx->common.stream_id = 0;
	acdb_data.calib_gain_rx->common.obj_cfg = AUDPP_CMD_OBJ0_UPDATE;
	acdb_data.calib_gain_rx->common.command_type = 0;

	acdb_data.calib_gain_rx->audppcalgain =
				acdb_calib_gain_rx->audppcalgain;
	return 0;
}

static void extract_pbe_block(struct header *prs_hdr, u32 *index)
{
	if (prs_hdr->iid == IID_AUDIO_PBE_RX_ENABLE_FLAG) {
		MM_DBG("Got IID = IID_AUDIO_PBE_RX_ENABLE\n");
		acdb_data.pbe_enable_flag = (u16 *)(acdb_data.virt_addr +
							*index +
							sizeof(struct header));
		*index += prs_hdr->data_len + sizeof(struct header);
	} else if (prs_hdr->iid == IID_PBE_CONFIG_PARAMETERS) {
		MM_DBG("Got IID == IID_PBE_CONFIG_PARAMETERS\n");
		acdb_data.pbe_blk = (struct acdb_pbe_block *)
					(acdb_data.virt_addr + *index
					+ sizeof(struct header));
		*index += prs_hdr->data_len + sizeof(struct header);
	}
}

static s32 get_audpp_pbe_block(void)
{
	struct header *prs_hdr;
	u32 index = 0;
	s32 result = -1;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);

		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_PBE_RX) {
				if ((prs_hdr->iid == IID_PBE_CONFIG_PARAMETERS)
					|| (prs_hdr->iid ==
						IID_AUDIO_PBE_RX_ENABLE_FLAG)) {
					extract_pbe_block(prs_hdr, &index);
					result = 0;
				}
			} else {
				index += prs_hdr->data_len +
					sizeof(struct header);
			}
		} else {
			break;
		}
	}
	return result;
}

static s32 acdb_fill_audpp_pbe(void)
{
	s32 result = -1;

	result = get_audpp_pbe_block();
	if (IS_ERR_VALUE(result))
		return result;
	memset(acdb_data.pbe_block, 0, sizeof(*acdb_data.pbe_block));

	acdb_data.pbe_block->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	acdb_data.pbe_block->common.stream = AUDPP_CMD_COPP_STREAM;
	acdb_data.pbe_block->common.stream_id = 0;
	acdb_data.pbe_block->common.obj_cfg = AUDPP_CMD_OBJ0_UPDATE;
	acdb_data.pbe_block->common.command_type = 0;
	acdb_data.pbe_block->pbe_enable = *acdb_data.pbe_enable_flag;

	acdb_data.pbe_block->realbassmix = acdb_data.pbe_blk->realbassmix;
	acdb_data.pbe_block->basscolorcontrol =
					acdb_data.pbe_blk->basscolorcontrol;
	acdb_data.pbe_block->mainchaindelay = acdb_data.pbe_blk->mainchaindelay;
	acdb_data.pbe_block->xoverfltorder = acdb_data.pbe_blk->xoverfltorder;
	acdb_data.pbe_block->bandpassfltorder =
					acdb_data.pbe_blk->bandpassfltorder;
	acdb_data.pbe_block->adrcdelay = acdb_data.pbe_blk->adrcdelay;
	acdb_data.pbe_block->downsamplelevel =
					acdb_data.pbe_blk->downsamplelevel;
	acdb_data.pbe_block->comprmstav = acdb_data.pbe_blk->comprmstav;
	acdb_data.pbe_block->expthreshold = acdb_data.pbe_blk->expthreshold;
	acdb_data.pbe_block->expslope = acdb_data.pbe_blk->expslope;
	acdb_data.pbe_block->compthreshold = acdb_data.pbe_blk->compthreshold;
	acdb_data.pbe_block->compslope = acdb_data.pbe_blk->compslope;
	acdb_data.pbe_block->cpmpattack_lsw = acdb_data.pbe_blk->cpmpattack_lsw;
	acdb_data.pbe_block->compattack_msw = acdb_data.pbe_blk->compattack_msw;
	acdb_data.pbe_block->comprelease_lsw =
					acdb_data.pbe_blk->comprelease_lsw;
	acdb_data.pbe_block->comprelease_msw =
					acdb_data.pbe_blk->comprelease_msw;
	acdb_data.pbe_block->compmakeupgain = acdb_data.pbe_blk->compmakeupgain;
	acdb_data.pbe_block->baselimthreshold =
					acdb_data.pbe_blk->baselimthreshold;
	acdb_data.pbe_block->highlimthreshold =
					acdb_data.pbe_blk->highlimthreshold;
	acdb_data.pbe_block->basslimmakeupgain =
					acdb_data.pbe_blk->basslimmakeupgain;
	acdb_data.pbe_block->highlimmakeupgain =
					acdb_data.pbe_blk->highlimmakeupgain;
	acdb_data.pbe_block->limbassgrc = acdb_data.pbe_blk->limbassgrc;
	acdb_data.pbe_block->limhighgrc = acdb_data.pbe_blk->limhighgrc;
	acdb_data.pbe_block->limdelay = acdb_data.pbe_blk->limdelay;
	memcpy(acdb_data.pbe_block->filter_coeffs,
		acdb_data.pbe_blk->filter_coeffs, sizeof(u16)*90);
	acdb_data.pbe_block->extpartition = 0;
	acdb_data.pbe_block->extbuffsize_lsw = PBE_BUF_SIZE;
	acdb_data.pbe_block->extbuffsize_msw = 0;
	acdb_data.pbe_block->extbuffstart_lsw = ((u32)acdb_data.pbe_extbuff
							& 0xFFFF);
	acdb_data.pbe_block->extbuffstart_msw = (((u32)acdb_data.pbe_extbuff
							& 0xFFFF0000) >> 16);
	return 0;
}


static s32 acdb_calibrate_audpp(void)
{
	s32	result = 0;

	result = acdb_fill_audpp_iir();
	if (!IS_ERR_VALUE(result)) {
		result = audpp_dsp_set_rx_iir(acdb_data.device_info->dev_id,
				acdb_data.pp_iir->active_flag,
					acdb_data.pp_iir, COPP);
		if (result) {
			MM_ERR("ACDB=> Failed to send IIR data to postproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPP is calibrated with IIR parameters"
					" for COPP ID %d\n",
						acdb_data.device_info->dev_id);
	}
	result = acdb_fill_audpp_mbadrc();
	if (!IS_ERR_VALUE(result)) {
		result = audpp_dsp_set_mbadrc(acdb_data.device_info->dev_id,
					acdb_data.pp_mbadrc->enable,
					acdb_data.pp_mbadrc, COPP);
		if (result) {
			MM_ERR("ACDB=> Failed to send MBADRC data to"
					" postproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPP is calibrated with MBADRC parameters"
					" for COPP ID %d\n",
					acdb_data.device_info->dev_id);
	}
	result = acdb_fill_audpp_cal_gain();
	if (!(IS_ERR_VALUE(result))) {
		result = audpp_dsp_set_gain_rx(acdb_data.device_info->dev_id,
					acdb_data.calib_gain_rx, COPP);
		if (result) {
			MM_ERR("ACDB=> Failed to send gain_rx"
				" data to postproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPP is calibrated with calib_gain_rx\n");
	}
	result = acdb_fill_audpp_pbe();
	if (!(IS_ERR_VALUE(result))) {
		result = audpp_dsp_set_pbe(acdb_data.device_info->dev_id,
					acdb_data.pbe_block->pbe_enable,
					acdb_data.pbe_block, COPP);
		if (result) {
			MM_ERR("ACDB=> Failed to send pbe block"
				"data to postproc\n");
			result = -EINVAL;
			goto done;
		}
		MM_DBG("AUDPP is calibarted with PBE\n");
	}
done:
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
		return -1;
	}
	memset(acdb_data.preproc_agc, 0, sizeof(*acdb_data.preproc_agc));
	acdb_data.preproc_agc->cmd_id = AUDPREPROC_CMD_CFG_AGC_PARAMS;
	acdb_data.preproc_agc->stream_id = acdb_data.preproc_stream_id;
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
		return -1;
	}
	memset(acdb_data.preproc_iir, 0, sizeof(*acdb_data.preproc_iir));

	acdb_data.preproc_iir->cmd_id =
		AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS;
	acdb_data.preproc_iir->stream_id = acdb_data.preproc_stream_id;
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

	acdb_data.preproc_iir->pan_of_filter0 =
		acdb_iir->pan[0];
	acdb_data.preproc_iir->pan_of_filter1 =
		acdb_iir->pan[1];
	acdb_data.preproc_iir->pan_of_filter2 =
		acdb_iir->pan[2];
	acdb_data.preproc_iir->pan_of_filter3 =
		acdb_iir->pan[3];
	return 0;
}

static struct acdb_calib_gain_tx *get_audpreproc_cal_gain(void)
{
	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);
		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_CALIBRATION_GAIN_TX) {
				if (prs_hdr->iid ==
					IID_AUDIO_CALIBRATION_GAIN_TX) {
					MM_DBG("Got audpreproc_calib_gain_tx"
					" block\n");
					return (struct acdb_calib_gain_tx *)
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

static s32 acdb_fill_audpreproc_cal_gain(void)
{
	struct acdb_calib_gain_tx *acdb_calib_gain_tx = NULL;

	acdb_calib_gain_tx = get_audpreproc_cal_gain();
	if (acdb_calib_gain_tx == NULL) {
		MM_ERR("unable to find  audpreproc"
			" calibration block returning\n");
		return -1;
	}
	MM_DBG("Calibration value"
		" for calib_gain_tx %d\n", acdb_calib_gain_tx->audprecalgain);
	memset(acdb_data.calib_gain_tx, 0, sizeof(*acdb_data.calib_gain_tx));

	acdb_data.calib_gain_tx->cmd_id =
					AUDPREPROC_CMD_CFG_CAL_GAIN_PARAMS;
	acdb_data.calib_gain_tx->stream_id = acdb_data.preproc_stream_id;
	acdb_data.calib_gain_tx->audprecalgain =
					acdb_calib_gain_tx->audprecalgain;
	return 0;
}

static struct acdb_rmc_block *get_rmc_blk(void)
{
	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);
		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_RMC_TX) {
				if (prs_hdr->iid ==
					IID_AUDIO_RMC_PARAM) {
					MM_DBG("Got afe_rmc block\n");
					return (struct acdb_rmc_block *)
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

struct acdb_fluence_block *get_audpp_fluence_block(void)
{
	struct header *prs_hdr;
	u32 index = 0;

	while (index < acdb_data.acdb_result.used_bytes) {
		prs_hdr = (struct header *)(acdb_data.virt_addr + index);

		if (prs_hdr->dbor_signature == DBOR_SIGNATURE) {
			if (prs_hdr->abid == ABID_AUDIO_FLUENCE_TX) {
				if (prs_hdr->iid == IID_AUDIO_FLUENCE_TX) {
					MM_DBG("got fluence block\n");
					return (struct acdb_fluence_block *)
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

static s32 acdb_fill_audpreproc_fluence(void)
{
	struct acdb_fluence_block *fluence_block = NULL;
	fluence_block = get_audpp_fluence_block();
	if (!fluence_block) {
		MM_ERR("error in finding fluence block\n");
		return -EPERM;
	}
	memset(&acdb_data.preproc_lvnv, 0, sizeof(
				struct audpreproc_cmd_cfg_lvnv_param));
	memcpy(acdb_data.fluence_extbuff_virt,
			&fluence_block->cs_tuningMode,
			(sizeof(struct acdb_fluence_block) -
					sizeof(fluence_block->csmode)));
	acdb_data.preproc_lvnv.cmd_id = AUDPREPROC_CMD_CFG_LVNV_PARMS;
	acdb_data.preproc_lvnv.stream_id = acdb_data.preproc_stream_id;
	acdb_data.preproc_lvnv.cs_mode = fluence_block->csmode;
	acdb_data.preproc_lvnv.lvnv_ext_buf_size = FLUENCE_BUF_SIZE;
	acdb_data.preproc_lvnv.lvnv_ext_buf_start_lsw =\
				((u32)(acdb_data.fluence_extbuff)\
						& 0x0000FFFF);
	acdb_data.preproc_lvnv.lvnv_ext_buf_start_msw =\
				(((u32)acdb_data.fluence_extbuff\
					& 0xFFFF0000) >> 16);
	return 0;
}

s32 acdb_calibrate_audpreproc(void)
{
	s32	result = 0;
	struct acdb_rmc_block *acdb_rmc = NULL;

	result = acdb_fill_audpreproc_agc();
	if (!IS_ERR_VALUE(result)) {
		result = audpreproc_dsp_set_agc(acdb_data.preproc_agc, sizeof(
					struct audpreproc_cmd_cfg_agc_params));
		if (result) {
			MM_ERR("ACDB=> Failed to send AGC data to preproc)\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPREC is calibrated with AGC parameters"
				" for COPP ID %d and AUDREC session %d\n",
					acdb_data.device_info->dev_id,
					acdb_data.preproc_stream_id);
	}
	result = acdb_fill_audpreproc_iir();
	if (!IS_ERR_VALUE(result)) {
		result = audpreproc_dsp_set_iir(acdb_data.preproc_iir,
				sizeof(struct\
				audpreproc_cmd_cfg_iir_tuning_filter_params));
		if (result) {
			MM_ERR("ACDB=> Failed to send IIR data to preproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("audpreproc is calibrated with iir parameters"
			" for COPP ID %d and AUREC session %d\n",
					acdb_data.device_info->dev_id,
					acdb_data.preproc_stream_id);
	}
	result = acdb_fill_audpreproc_cal_gain();
	if (!(IS_ERR_VALUE(result))) {
		result = audpreproc_dsp_set_gain_tx(acdb_data.calib_gain_tx,
				sizeof(struct audpreproc_cmd_cfg_cal_gain));
		if (result) {
			MM_ERR("ACDB=> Failed to send calib_gain_tx"
				" data to preproc\n");
			result = -EINVAL;
			goto done;
		} else
			MM_DBG("AUDPREPROC is calibrated"
				" with calib_gain_tx\n");
	}
	if (acdb_data.build_id[17] != '0') {
		acdb_rmc = get_rmc_blk();
		if (acdb_rmc != NULL) {
			result = afe_config_rmc_block(acdb_rmc);
			if (result) {
				MM_ERR("ACDB=> Failed to send rmc"
					" data to afe\n");
				result = -EINVAL;
				goto done;
			} else
				MM_DBG("AFE is calibrated with rmc params\n");
		} else
			MM_DBG("RMC block was not found\n");
	}
	if (!acdb_data.fleuce_feature_status[acdb_data.preproc_stream_id]) {
		result = acdb_fill_audpreproc_fluence();
		if (!(IS_ERR_VALUE(result))) {
			result = audpreproc_dsp_set_lvnv(
					&acdb_data.preproc_lvnv,
					sizeof(struct\
					audpreproc_cmd_cfg_lvnv_param));
			if (result) {
				MM_ERR("ACDB=> Failed to send lvnv "
						"data to preproc\n");
				result = -EINVAL;
				goto done;
			} else
				MM_DBG("AUDPREPROC is calibrated"
						" with lvnv parameters\n");
		} else
			MM_ERR("fluence block is not found\n");
	} else
		MM_DBG("fluence block override\n");
done:
	return result;
}

static s32 acdb_send_calibration(void)
{
	s32 result = 0;

	if ((acdb_data.device_info->dev_type & RX_DEVICE) == 1) {
		result = acdb_calibrate_audpp();
		if (result)
			goto done;
	} else if ((acdb_data.device_info->dev_type & TX_DEVICE) == 2) {
		result = acdb_calibrate_audpreproc();
		if (result)
			goto done;
		if (acdb_data.preproc_stream_id == 0)
			acdb_data.audrec_applied |= AUDREC0_READY;
		else if (acdb_data.preproc_stream_id == 1)
			acdb_data.audrec_applied |= AUDREC1_READY;
		else if (acdb_data.preproc_stream_id == 2)
			acdb_data.audrec_applied |= AUDREC2_READY;
		MM_DBG("acdb_data.audrec_applied = %x\n",
					acdb_data.audrec_applied);
	}
done:
	return result;
}

static u8 check_tx_acdb_values_cached(void)
{
	u8 stream_id  = acdb_data.preproc_stream_id;

	if ((acdb_data.device_info->dev_id ==
		acdb_cache_tx[stream_id].device_info.dev_id) &&
		(acdb_data.device_info->sample_rate ==
		acdb_cache_tx[stream_id].device_info.sample_rate) &&
		(acdb_data.device_info->acdb_id ==
		acdb_cache_tx[stream_id].device_info.acdb_id) &&
		(acdb_cache_tx[stream_id].node_status ==
						ACDB_VALUES_FILLED))
		return 0;
	else
		return 1;
}

static void handle_tx_device_ready_callback(void)
{
	u8 i = 0;
	u8 ret = 0;
	u8 acdb_value_apply = 0;
	u8 result = 0;
	u8 stream_id = acdb_data.preproc_stream_id;

	if (acdb_data.multiple_sessions) {
		for (i = 0; i < MAX_AUDREC_SESSIONS; i++) {
			/*check is to exclude copying acdb values in the
			current node pointed by acdb_data structure*/
			if (acdb_cache_tx[i].phys_addr_acdb_values !=
							acdb_data.phys_addr) {
				ret = check_device_info_already_present(\
							*acdb_data.device_info,
							&acdb_cache_tx[i]);
				if (ret) {
					memcpy((char *)acdb_cache_tx[i].\
						virt_addr_acdb_values,
						(char *)acdb_data.virt_addr,
								ACDB_BUF_SIZE);
					acdb_cache_tx[i].node_status =
							ACDB_VALUES_FILLED;
				}
			}
		}
		acdb_data.multiple_sessions = 0;
	}
	/*check wheather AUDREC enabled before device call backs*/
	if ((acdb_data.acdb_state & AUDREC0_READY) &&
			!(acdb_data.audrec_applied & AUDREC0_READY)) {
		MM_DBG("AUDREC0 already enabled apply acdb values\n");
		acdb_value_apply |= AUDREC0_READY;
	} else if ((acdb_data.acdb_state & AUDREC1_READY) &&
			!(acdb_data.audrec_applied & AUDREC1_READY)) {
		MM_DBG("AUDREC1 already enabled apply acdb values\n");
		acdb_value_apply |= AUDREC1_READY;
	} else if ((acdb_data.acdb_state & AUDREC2_READY) &&
			!(acdb_data.audrec_applied & AUDREC2_READY)) {
		MM_DBG("AUDREC2 already enabled apply acdb values\n");
		acdb_value_apply |= AUDREC2_READY;
	}
	if (acdb_value_apply) {
		if (session_info[stream_id].sampling_freq)
			acdb_data.device_info->sample_rate =
					session_info[stream_id].sampling_freq;
		result = check_tx_acdb_values_cached();
		if (result) {
			result = acdb_get_calibration();
			if (result < 0) {
				MM_ERR("Not able to get calibration"
						" data continue\n");
				return;
			}
		}
		acdb_cache_tx[stream_id].node_status = ACDB_VALUES_FILLED;
		acdb_send_calibration();
	}
}

static struct acdb_cache_node *get_acdb_values_from_cache_tx(u32 stream_id)
{
	MM_DBG("searching node with stream_id %d\n", stream_id);
	if ((acdb_cache_tx[stream_id].stream_id == stream_id) &&
			(acdb_cache_tx[stream_id].node_status ==
					ACDB_VALUES_NOT_FILLED)) {
			return &acdb_cache_tx[stream_id];
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
	u32 i = 0;
	for (i = 0; i < MAX_COPP_NODE_SUPPORTED; i++) {
		if (acdb_cache_rx[i].node_status ==
					ACDB_VALUES_FILLED) {
			update_acdb_data_struct(&acdb_cache_rx[i]);
			if (acdb_data.acdb_state & CAL_DATA_READY)
				acdb_send_calibration();
		}
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
	u32 i = 0;
	u32 err = 0;
	/*initialize local cache */
	for (i = 0; i < MAX_AUDREC_SESSIONS; i++) {
		acdb_cache_tx[i].phys_addr_acdb_values =
				allocate_contiguous_ebi_nomap(ACDB_BUF_SIZE,
								SZ_4K);

		if (!acdb_cache_tx[i].phys_addr_acdb_values) {
			MM_ERR("ACDB=> Cannot allocate physical memory\n");
			result = -ENOMEM;
			goto error;
		}
		acdb_cache_tx[i].map_v_addr = ioremap(
					acdb_cache_tx[i].phys_addr_acdb_values,
						ACDB_BUF_SIZE);
		if (IS_ERR(acdb_cache_tx[i].map_v_addr)) {
			MM_ERR("ACDB=> Could not map physical address\n");
			result = -ENOMEM;
			free_contiguous_memory_by_paddr(
					acdb_cache_tx[i].phys_addr_acdb_values);
			goto error;
		}
		acdb_cache_tx[i].virt_addr_acdb_values =
					acdb_cache_tx[i].map_v_addr;
		memset(acdb_cache_tx[i].virt_addr_acdb_values, 0,
						ACDB_BUF_SIZE);
	}
	return result;
error:
	for (err = 0; err < i; err++) {
		iounmap(acdb_cache_tx[err].map_v_addr);
		free_contiguous_memory_by_paddr(
				acdb_cache_tx[err].phys_addr_acdb_values);
	}
	return result;
}

static u32 allocate_memory_acdb_cache_rx(void)
{
	u32 result = 0;
	u32 i = 0;
	u32 err = 0;

	/*initialize local cache */
	for (i = 0; i < MAX_COPP_NODE_SUPPORTED; i++) {
		acdb_cache_rx[i].phys_addr_acdb_values =
					allocate_contiguous_ebi_nomap(
						ACDB_BUF_SIZE, SZ_4K);

		if (!acdb_cache_rx[i].phys_addr_acdb_values) {
			MM_ERR("ACDB=> Can not allocate physical memory\n");
			result = -ENOMEM;
			goto error;
		}
		acdb_cache_rx[i].map_v_addr =
				ioremap(acdb_cache_rx[i].phys_addr_acdb_values,
					ACDB_BUF_SIZE);
		if (IS_ERR(acdb_cache_rx[i].map_v_addr)) {
			MM_ERR("ACDB=> Could not map physical address\n");
			result = -ENOMEM;
			free_contiguous_memory_by_paddr(
					acdb_cache_rx[i].phys_addr_acdb_values);
			goto error;
		}
		acdb_cache_rx[i].virt_addr_acdb_values =
					acdb_cache_rx[i].map_v_addr;
		memset(acdb_cache_rx[i].virt_addr_acdb_values, 0,
						ACDB_BUF_SIZE);
	}
	return result;
error:
	for (err = 0; err < i; err++) {
		iounmap(acdb_cache_rx[err].map_v_addr);
		free_contiguous_memory_by_paddr(
				acdb_cache_rx[err].phys_addr_acdb_values);
	}
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
	u32 i = 0;

	for (i = 0; i < MAX_COPP_NODE_SUPPORTED; i++) {
		iounmap(acdb_cache_rx[i].map_v_addr);
		free_contiguous_memory_by_paddr(
				acdb_cache_rx[i].phys_addr_acdb_values);
	}
}

static void free_memory_acdb_cache_tx(void)
{
	u32 i = 0;

	for (i = 0; i < MAX_AUDREC_SESSIONS; i++) {
		iounmap(acdb_cache_tx[i].map_v_addr);
		free_contiguous_memory_by_paddr(
				acdb_cache_tx[i].phys_addr_acdb_values);
	}
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
	acdb_data.calib_gain_rx = kmalloc(sizeof(*acdb_data.calib_gain_rx),
							GFP_KERNEL);
	if (acdb_data.calib_gain_rx == NULL) {
		MM_ERR("ACDB=> Could not allocate"
			" postproc calib_gain_rx memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
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
		kfree(acdb_data.calib_gain_rx);
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
		kfree(acdb_data.calib_gain_rx);
		kfree(acdb_data.preproc_agc);
		result = -ENOMEM;
		goto done;
	}
	acdb_data.calib_gain_tx = kmalloc(sizeof(*acdb_data.calib_gain_tx),
							GFP_KERNEL);
	if (acdb_data.calib_gain_tx == NULL) {
		MM_ERR("ACDB=> Could not allocate"
			" preproc calib_gain_tx memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		kfree(acdb_data.calib_gain_rx);
		kfree(acdb_data.preproc_agc);
		kfree(acdb_data.preproc_iir);
		result = -ENOMEM;
		goto done;
	}
	acdb_data.pbe_block = kmalloc(sizeof(*acdb_data.pbe_block),
						GFP_KERNEL);
	if (acdb_data.pbe_block == NULL) {
		MM_ERR("ACDB=> Could not allocate pbe_block memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		kfree(acdb_data.calib_gain_rx);
		kfree(acdb_data.preproc_agc);
		kfree(acdb_data.preproc_iir);
		kfree(acdb_data.calib_gain_tx);
		result = -ENOMEM;
		goto done;
	}
	acdb_data.pbe_extbuff = (u16 *) allocate_contiguous_ebi_nomap(
						PBE_BUF_SIZE, SZ_4K);
	if (!acdb_data.pbe_extbuff) {
		MM_ERR("ACDB=> Cannot allocate physical memory\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		kfree(acdb_data.calib_gain_rx);
		kfree(acdb_data.preproc_agc);
		kfree(acdb_data.preproc_iir);
		kfree(acdb_data.calib_gain_tx);
		kfree(acdb_data.pbe_block);
		result = -ENOMEM;
		goto done;
	}
	acdb_data.fluence_extbuff = allocate_contiguous_ebi_nomap(
					FLUENCE_BUF_SIZE, SZ_4K);
	if (!acdb_data.fluence_extbuff) {
		MM_ERR("ACDB=> cannot allocate physical memory for "
					"fluence block\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		kfree(acdb_data.calib_gain_rx);
		kfree(acdb_data.preproc_agc);
		kfree(acdb_data.preproc_iir);
		kfree(acdb_data.calib_gain_tx);
		kfree(acdb_data.pbe_block);
		free_contiguous_memory_by_paddr((int32_t)acdb_data.pbe_extbuff);
		result = -ENOMEM;
		goto done;
	}
	acdb_data.map_v_fluence = ioremap(
				acdb_data.fluence_extbuff,
				FLUENCE_BUF_SIZE);
	if (IS_ERR(acdb_data.map_v_fluence)) {
		MM_ERR("ACDB=> Could not map physical address\n");
		free_memory_acdb_get_blk();
		free_memory_acdb_cache_rx();
		free_memory_acdb_cache_tx();
		kfree(acdb_data.pp_iir);
		kfree(acdb_data.pp_mbadrc);
		kfree(acdb_data.calib_gain_rx);
		kfree(acdb_data.preproc_agc);
		kfree(acdb_data.preproc_iir);
		kfree(acdb_data.calib_gain_tx);
		kfree(acdb_data.pbe_block);
		free_contiguous_memory_by_paddr(
				(int32_t)acdb_data.pbe_extbuff);
		free_contiguous_memory_by_paddr(
				(int32_t)acdb_data.fluence_extbuff);
		result = -ENOMEM;
		goto done;
	} else
		acdb_data.fluence_extbuff_virt =
					acdb_data.map_v_fluence;
done:
	return result;
}

static u32 free_acdb_cache_node(union auddev_evt_data *evt)
{
	u32 session_id;
	if ((evt->audcal_info.dev_type & TX_DEVICE) == 2) {
		/*Second argument to find_first_bit should be maximum number
		of bits interested
		*/
		session_id = find_first_bit(
				(unsigned long *)&(evt->audcal_info.sessions),
				sizeof(evt->audcal_info.sessions) * 8);
		MM_DBG("freeing node %d for tx device", session_id);
		acdb_cache_tx[session_id].
			node_status = ACDB_VALUES_NOT_FILLED;
	} else {
			MM_DBG("freeing rx cache node %d\n",
						evt->audcal_info.dev_id);
			acdb_cache_rx[evt->audcal_info.dev_id].
				node_status = ACDB_VALUES_NOT_FILLED;
	}
	return 0;
}

static u8 check_device_change(struct auddev_evt_audcal_info audcal_info)
{
	if (!acdb_data.device_info) {
		MM_ERR("not pointing to previous valid device detail\n");
		return 1; /*device info will not be pointing to*/
			/* valid device when acdb driver comes up*/
	}
	if ((audcal_info.dev_id == acdb_data.device_info->dev_id) &&
		(audcal_info.sample_rate ==
				acdb_data.device_info->sample_rate) &&
		(audcal_info.acdb_id == acdb_data.device_info->acdb_id)) {
		return 0;
	}
	return 1;
}

static void device_cb(u32 evt_id, union auddev_evt_data *evt, void *private)
{
	struct auddev_evt_audcal_info	audcal_info;
	struct acdb_cache_node *acdb_cache_free_node =  NULL;
	u32 stream_id = 0;
	u8 ret = 0;
	u8 count = 0;
	u8 i = 0;
	u8 device_change = 0;

	if (!((evt_id == AUDDEV_EVT_DEV_RDY) ||
		(evt_id == AUDDEV_EVT_DEV_RLS))) {
		goto done;
	}
	/*if session value is zero it indicates that device call back is for
	voice call we will drop the request as acdb values for voice call is
	not applied from acdb driver*/
	if (!evt->audcal_info.sessions) {
		MM_DBG("no active sessions and call back is for"
				" voice call\n");
		goto done;
	}
	if (evt_id == AUDDEV_EVT_DEV_RLS) {
		MM_DBG("got release command for dev %d\n",
					evt->audcal_info.dev_id);
		acdb_data.acdb_state &= ~CAL_DATA_READY;
		free_acdb_cache_node(evt);
		/*reset the applied flag for the session routed to the device*/
		acdb_data.audrec_applied &= ~(evt->audcal_info.sessions
							<< AUDREC_OFFSET);
		goto done;
	}
	if (((evt->audcal_info.dev_type & RX_DEVICE) == 1) &&
			(evt->audcal_info.acdb_id == PSEUDO_ACDB_ID)) {
		MM_INFO("device cb is for rx device with pseudo acdb id\n");
		goto done;
	}
	audcal_info = evt->audcal_info;
	MM_DBG("dev_id = %d\n", audcal_info.dev_id);
	MM_DBG("sample_rate = %d\n", audcal_info.sample_rate);
	MM_DBG("acdb_id = %d\n", audcal_info.acdb_id);
	MM_DBG("sessions = %d\n", audcal_info.sessions);
	MM_DBG("acdb_state = %x\n", acdb_data.acdb_state);
	mutex_lock(&acdb_data.acdb_mutex);
	device_change = check_device_change(audcal_info);
	if (!device_change) {
		if ((audcal_info.dev_type & TX_DEVICE) == 2) {
			if (!(acdb_data.acdb_state & AUDREC0_READY))
				acdb_data.audrec_applied &= ~AUDREC0_READY;
			if (!(acdb_data.acdb_state & AUDREC1_READY))
				acdb_data.audrec_applied &= ~AUDREC1_READY;
			if (!(acdb_data.acdb_state & AUDREC2_READY))
				acdb_data.audrec_applied &= ~AUDREC2_READY;
				acdb_data.acdb_state &= ~CAL_DATA_READY;
				goto update_cache;
		}
	} else
		/* state is updated to querry the modem for values */
		acdb_data.acdb_state &= ~CAL_DATA_READY;

update_cache:
	if ((audcal_info.dev_type & TX_DEVICE) == 2) {
		/*loop is to take care of use case:- multiple Audrec
		sessions are routed before enabling the device in this use
		case we will get the sessions value as bits set for all the
		sessions routed before device enable, so we should take care
		of copying device info to all the sessions*/
		for (i = 0; i < MAX_AUDREC_SESSIONS; i++) {
			stream_id = ((audcal_info.sessions >> i) & 0x01);
			if (stream_id) {
				acdb_cache_free_node =	&acdb_cache_tx[i];
				ret  = check_device_info_already_present(
							audcal_info,
							acdb_cache_free_node);
				acdb_cache_free_node->stream_id = i;
				acdb_data.cur_tx_session = i;
				count++;
			}
		}
		if (count > 1)
			acdb_data.multiple_sessions = 1;
	} else {
		acdb_cache_free_node = &acdb_cache_rx[audcal_info.dev_id];
		ret = check_device_info_already_present(audcal_info,
						acdb_cache_free_node);
		if (ret == 1) {
			MM_DBG("got device ready call back for another "
					"audplay task sessions on same COPP\n");
			/*stream_id is used to keep track of number of active*/
			/*sessions active on this device*/
			acdb_cache_free_node->stream_id++;
			mutex_unlock(&acdb_data.acdb_mutex);
			goto done;
		}
		acdb_cache_free_node->stream_id++;
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

	result = auddev_register_evt_listner((AUDDEV_EVT_DEV_RDY
						| AUDDEV_EVT_DEV_RLS),
		AUDDEV_CLNT_AUDIOCAL, 0, device_cb, (void *)&acdb_data);

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
	if (id != AUDPP_MSG_CFG_MSG)
		goto done;

	if (msg[0] == AUDPP_MSG_ENA_DIS) {
		if (--acdb_cache_rx[acdb_data.\
				device_info->dev_id].stream_id <= 0) {
			acdb_data.acdb_state &= ~AUDPP_READY;
			acdb_cache_rx[acdb_data.device_info->dev_id]\
					.stream_id = 0;
			MM_DBG("AUDPP_MSG_ENA_DIS\n");
		}
		goto done;
	}

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
	if (acdb_data.device_info->dev_id == PSEUDO_ACDB_ID) {
		MM_INFO("audpreproc is routed to pseudo device\n");
		return result;
	}
	if (acdb_data.build_id[17] == '1') {
		if (session_info[stream_id].sampling_freq)
			acdb_data.device_info->sample_rate =
					session_info[stream_id].sampling_freq;
	}
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

void fluence_feature_update(int enable, int stream_id)
{
	MM_INFO("Fluence feature over ride with = %d\n", enable);
	acdb_data.fleuce_feature_status[stream_id] = enable;
}
EXPORT_SYMBOL(fluence_feature_update);

static void audpreproc_cb(void *private, u32 id, void *msg)
{
	struct audpreproc_cmd_enc_cfg_done_msg *tmp;
	u8 result = 0;
	int stream_id = 0;
	if (id != AUDPREPROC_CMD_ENC_CFG_DONE_MSG)
		goto done;

	tmp = (struct audpreproc_cmd_enc_cfg_done_msg *)msg;
	acdb_data.preproc_stream_id = tmp->stream_id;
	stream_id = acdb_data.preproc_stream_id;
	get_audrec_session_info(stream_id, &session_info[stream_id]);
	MM_DBG("rec_enc_type = %x\n", tmp->rec_enc_type);
	if ((tmp->rec_enc_type & 0x8000) ==
				AUD_PREPROC_CONFIG_DISABLED) {
		if (acdb_data.preproc_stream_id == 0) {
			acdb_data.acdb_state &= ~AUDREC0_READY;
			acdb_data.audrec_applied &= ~AUDREC0_READY;
		} else if (acdb_data.preproc_stream_id == 1) {
			acdb_data.acdb_state &= ~AUDREC1_READY;
			acdb_data.audrec_applied &= ~AUDREC1_READY;
		} else if (acdb_data.preproc_stream_id == 2) {
			acdb_data.acdb_state &= ~AUDREC2_READY;
			acdb_data.audrec_applied &= ~AUDREC2_READY;
		}
		acdb_data.fleuce_feature_status[stream_id] = 0;
		acdb_cache_tx[tmp->stream_id].node_status =\
						ACDB_VALUES_NOT_FILLED;
		acdb_data.acdb_state &= ~CAL_DATA_READY;
		goto done;
	}
	/*Following check is added to make sure that device info
	  is updated. audpre proc layer enabled without device
	  callback at this scenario we should not access
	  device information
	 */
	if (acdb_data.build_id[17] != '0') {
		if (acdb_data.device_info &&
			session_info[stream_id].sampling_freq) {
			acdb_data.device_info->sample_rate =
					session_info[stream_id].sampling_freq;
			result = check_tx_acdb_values_cached();
			if (!result) {
				MM_INFO("acdb values for the stream is" \
							" querried from modem");
				acdb_data.acdb_state |= CAL_DATA_READY;
			} else {
				acdb_data.acdb_state &= ~CAL_DATA_READY;
			}
		}
	}
	if (acdb_data.preproc_stream_id == 0)
		acdb_data.acdb_state |= AUDREC0_READY;
	else if (acdb_data.preproc_stream_id == 1)
		acdb_data.acdb_state |= AUDREC1_READY;
	else if (acdb_data.preproc_stream_id == 2)
		acdb_data.acdb_state |= AUDREC2_READY;
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
	result = auddev_unregister_evt_listner(AUDDEV_CLNT_AUDIOCAL, 0);
	if (result)
		MM_ERR("ACDB=> Could not unregister device callback\n");
err1:
	daldevice_detach(acdb_data.handle);
	acdb_data.handle = NULL;
err:
	return result;
}

static s32 initialize_modem_acdb(void)
{
	struct acdb_cmd_init_adie acdb_cmd;
	u8 codec_type = -1;
	s32 result = 0;
	u8 iterations = 0;

	codec_type = adie_get_detected_codec_type();
	if (codec_type == MARIMBA_ID)
		acdb_cmd.adie_type = ACDB_CURRENT_ADIE_MODE_MARIMBA;
	else if (codec_type == TIMPANI_ID)
		acdb_cmd.adie_type = ACDB_CURRENT_ADIE_MODE_TIMPANI;
	else
		acdb_cmd.adie_type = ACDB_CURRENT_ADIE_MODE_UNKNOWN;
	acdb_cmd.command_id = ACDB_CMD_INITIALIZE_FOR_ADIE;
	do {
		/*Initialize ACDB software on modem based on codec type*/
		result = dalrpc_fcn_8(ACDB_DalACDB_ioctl, acdb_data.handle,
				(const void *)&acdb_cmd, sizeof(acdb_cmd),
				&acdb_data.acdb_result,
				sizeof(acdb_data.acdb_result));
		if (result < 0) {
			MM_ERR("ACDB=> RPC failure result = %d\n", result);
			goto error;
		}
		/*following check is introduced to handle boot up race
		condition between AUDCAL SW peers running on apps
		and modem (ACDB_RES_BADSTATE indicates modem AUDCAL SW is
		not in initialized sate) we need to retry to get ACDB
		initialized*/
		if (acdb_data.acdb_result.result == ACDB_RES_BADSTATE) {
			msleep(500);
			iterations++;
		} else if (acdb_data.acdb_result.result == ACDB_RES_SUCCESS) {
			MM_DBG("Modem ACDB SW initialized ((iterations = %d)\n",
							iterations);
			return result;
		} else {
			MM_ERR("ACDB=> Modem ACDB SW failed to initialize"
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

static s32 acdb_calibrate_device(void *data)
{
	s32 result = 0;

	/* initialize driver */
	result = acdb_initialize_data();
	if (result)
		goto done;
	if (acdb_data.build_id[17] != '0') {
		result = initialize_modem_acdb();
		if (result < 0)
			MM_ERR("failed to initialize modem ACDB\n");
	}

	while (!kthread_should_stop()) {
		MM_DBG("Waiting for call back events\n");
		wait_event_interruptible(acdb_data.wait,
					(acdb_data.device_cb_compl
					| acdb_data.audpp_cb_compl
					| acdb_data.preproc_cb_compl));
		mutex_lock(&acdb_data.acdb_mutex);
		if (acdb_data.device_cb_compl) {
			acdb_data.device_cb_compl = 0;
			if (!(acdb_data.acdb_state & CAL_DATA_READY)) {
				if ((acdb_data.device_info->dev_type
							& RX_DEVICE) == 1) {
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
						MM_ERR("Not able to get "
							"calibration "
							"data continue\n");
						continue;
					}
				}
			}
			MM_DBG("acdb state = %d\n",
					 acdb_data.acdb_state);
			if ((acdb_data.device_info->dev_type & TX_DEVICE) == 2)
				handle_tx_device_ready_callback();
			else {
				acdb_cache_rx[acdb_data.device_info->dev_id]\
						.node_status =
						ACDB_VALUES_FILLED;
				if (acdb_data.acdb_state &
						AUDPP_READY) {
					MM_DBG("AUDPP already enabled "
							"apply acdb values\n");
					goto apply;
				}
			}
		}

		if (!(acdb_data.audpp_cb_compl ||
				acdb_data.preproc_cb_compl)) {
			MM_DBG("need to wait for either AUDPP / AUDPREPROC "
					"Event\n");
			mutex_unlock(&acdb_data.acdb_mutex);
			continue;
		} else {
			MM_DBG("got audpp / preproc call back\n");
			if (acdb_data.audpp_cb_compl) {
				send_acdb_values_for_active_devices();
				acdb_data.audpp_cb_compl = 0;
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
	acdb_data.cb_thread_task = kthread_run(acdb_calibrate_device,
		NULL, "acdb_cb_thread");

	if (IS_ERR(acdb_data.cb_thread_task)) {
		MM_ERR("ACDB=> Could not register cb thread\n");
		result = -ENODEV;
		goto err;
	}

	acdb_data.build_id = socinfo_get_build_id();
	MM_INFO("build id used is = %s\n", acdb_data.build_id);

#ifdef CONFIG_DEBUG_FS
	/*This is RTC specific INIT used only with debugfs*/
	if (!rtc_acdb_init())
		MM_ERR("RTC ACDB=>INIT Failure\n");

#endif
	init_waitqueue_head(&acdb_data.wait);

	return misc_register(&acdb_misc);
err:
	return result;
}

static void __exit acdb_exit(void)
{
	s32	result = 0;
	u32 i = 0;

	result = auddev_unregister_evt_listner(AUDDEV_CLNT_AUDIOCAL, 0);
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

	for (i = 0; i < MAX_COPP_NODE_SUPPORTED; i++) {
		if (i < MAX_AUDREC_SESSIONS) {
			iounmap(acdb_cache_tx[i].map_v_addr);
			free_contiguous_memory_by_paddr(
					acdb_cache_tx[i].phys_addr_acdb_values);
		}
		iounmap(acdb_cache_rx[i].map_v_addr);
		free_contiguous_memory_by_paddr(
					acdb_cache_rx[i].phys_addr_acdb_values);
	}
	kfree(acdb_data.device_info);
	kfree(acdb_data.pp_iir);
	kfree(acdb_data.pp_mbadrc);
	kfree(acdb_data.preproc_agc);
	kfree(acdb_data.preproc_iir);
	free_contiguous_memory_by_paddr(
				(int32_t)acdb_data.pbe_extbuff);
	iounmap(acdb_data.map_v_fluence);
	free_contiguous_memory_by_paddr(
			(int32_t)acdb_data.fluence_extbuff);
	mutex_destroy(&acdb_data.acdb_mutex);
	memset(&acdb_data, 0, sizeof(acdb_data));
	#ifdef CONFIG_DEBUG_FS
	rtc_acdb_deinit();
	#endif
}

late_initcall(acdb_init);
module_exit(acdb_exit);

MODULE_DESCRIPTION("MSM 7x30 Audio ACDB driver");
MODULE_LICENSE("GPL v2");
