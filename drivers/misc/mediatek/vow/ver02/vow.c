// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

/*****************************************************************************
 * Header Files
 *****************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/miscdevice.h>
/* #include <linux/wakelock.h> */
#include <linux/pm_wakeup.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>  /* FOR SCP REVOCER */
#ifdef SIGTEST
#include <asm/siginfo.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/pinctrl/consumer.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include "scp.h"
#endif  /* #ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT */
#include "vow.h"
#include "vow_assert.h"

/*****************************************************************************
 * Variable Definition
 ****************************************************************************/
static unsigned int VowDrv_Wait_Queue_flag;
static unsigned int VoiceData_Wait_Queue_flag;
static DECLARE_WAIT_QUEUE_HEAD(VowDrv_Wait_Queue);
static DECLARE_WAIT_QUEUE_HEAD(VoiceData_Wait_Queue);
static DEFINE_SPINLOCK(vowdrv_lock);
static DEFINE_SPINLOCK(vowdrv_dump_lock);
static struct wakeup_source *vow_suspend_lock;
static struct wakeup_source *vow_ipi_suspend_lock;
static struct dump_package_t dump_package;
static int init_flag = -1;
static const uint32_t kReadVowDumpSize = 0xA00 * 2; // 320(10ms) x 8 x 2ch= 5120 = 0x1400
/*****************************************************************************
 * Function  Declaration
 ****************************************************************************/
static void vow_service_getVoiceData(void);
static void vow_ipi_reg_ok(short uuid,
			   int confidence_lv,
			   unsigned int extradata_len,
			   unsigned int payloaddump_len);
static bool VowDrv_SetFlag(int type, unsigned int set);
static int VowDrv_GetHWStatus(void);
static void vow_pcm_dump_init(void);
static void vow_pcm_dump_deinit(void);
static bool VowDrv_SetBargeIn(unsigned int set, unsigned int irq_id);
//static int vow_service_SearchSpeakerModelWithUuid(int uuid);
static int vow_service_SearchSpeakerModelWithKeyword(int keyword);
static int vow_service_SearchSpeakerModelWithId(int id);
#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
static void vow_service_ReadPayloadDumpData(unsigned int buf_length);
#endif
static DEFINE_MUTEX(vow_vmalloc_lock);
static DEFINE_MUTEX(vow_extradata_mutex);
static DEFINE_MUTEX(voicedata_mutex);

/*****************************************************************************
 * VOW SERVICES
 *****************************************************************************/

static struct
{
	struct vow_speaker_model_t  vow_speaker_model[MAX_VOW_SPEAKER_MODEL];
	unsigned long        vow_info_apuser[MAX_VOW_INFO_LEN];
	unsigned long        voicedata_user_addr;
	unsigned long        voicedata_user_size;
	short                *voicedata_kernel_ptr;
	char                 *voicedata_scp_ptr;
	dma_addr_t           voicedata_scp_addr;
	char                 *extradata_ptr;
	dma_addr_t           extradata_addr;
	char                 *extradata_mem_ptr;
	unsigned int         extradata_bytelen;
#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
	char                 *payloaddump_scp_ptr;
	dma_addr_t           payloaddump_scp_addr;
	unsigned long        payloaddump_user_addr;
	unsigned long        payloaddump_user_max_size;
	unsigned long        payloaddump_user_return_size_addr;
	short                *payloaddump_kernel_ptr;
	unsigned int         payloaddump_length;
#endif
	unsigned int         kernel_voicedata_idx;
	bool                 scp_command_flag;
	bool                 recording_flag;
	int                  scp_command_id;
	int                  scp_command_keywordid;
	int                  confidence_level;
	int                  eint_status;
	int                  pwr_status;
	bool                 suspend_lock;
	bool                 firstRead;
	unsigned long        voicedata_user_return_size_addr;
	unsigned int         scp_shared_voice_buf_offset;
	unsigned int         scp_shared_voice_length;
	unsigned int         transfer_length;
	struct device_node   *node;
	struct pinctrl       *pinctrl;
	struct pinctrl_state *pins_eint_on;
	struct pinctrl_state *pins_eint_off;
	bool                 bypass_enter_phase3;
	unsigned int         enter_phase3_cnt;
	unsigned int         force_phase_stage;
	bool                 swip_log_enable;
	struct vow_eint_data_struct_t  vow_eint_data_struct;
	unsigned long long   scp_recognize_ok_cycle;
	unsigned long long   ap_received_ipi_cycle;
	bool                 tx_keyword_start;
	unsigned int         dump_frm_cnt;
	unsigned int         voice_sample_delay;
	unsigned int         bargein_dump_cnt1;
	unsigned int         bargein_dump_cnt2;
	short                *interleave_pcmdata_ptr;
	bool                 dump_pcm_flag;
	bool                 scp_recovering;
	bool                 vow_recovering;
	unsigned int         recog_dump_cnt1;
	unsigned int         vffp_dump_cnt1;
	unsigned int         inputmic_dump_cnt1;
	bool                 split_dumpfile_flag;
	bool                 mcps_flag;
	unsigned int         scp_dual_mic_switch;
	unsigned int         mtkif_type;
	unsigned int         google_engine_version;
	unsigned int         vow_mic_number;
	char                 alexa_engine_version[VOW_ENGINE_INFO_LENGTH_BYTE];
	char                 google_engine_arch[VOW_ENGINE_INFO_LENGTH_BYTE];
	unsigned int         custom_model_addr;
	unsigned long        custom_model_size;
} vowserv;

struct vow_dump_info_t {
	dma_addr_t    phy_addr;           // address of reseved buffer
	char          *vir_addr;          // virtual (kernel) addess of reseved buffer
	uint32_t      size;               // size of reseved buffer (bytes)
	uint32_t      scp_dump_offset[VOW_MAX_CH_NUM]; // return data offset from scp
	uint32_t      scp_dump_size[VOW_MAX_CH_NUM];   // return data size from scp
	short         *kernel_dump_addr;  // kernel internal buffer address
	unsigned int  kernel_dump_idx;    // current index of kernel_dump_addr
	unsigned int  kernel_dump_size;   // size of kernel_dump_ptr buffer (bytes)
	unsigned long user_dump_addr;     // addr of user dump buffer
	unsigned long user_dump_idx;      // current index of user_dump_addr
	unsigned long user_dump_size;     // size of user dump buffer
	unsigned long user_return_size_addr; // addr of return size of user
};
#define NUM_DELAY_INFO (2)
uint32_t delay_info[NUM_DELAY_INFO];
struct vow_dump_info_t vow_dump_info[NUM_DUMP_DATA];

/*****************************************************************************
 * DSP IPI HANDELER
 *****************************************************************************/
static void vow_ipi_rx_handle_data_msg(void *msg_data)
{
	struct vow_ipi_combined_info_t *ipi_ptr;
	unsigned long flags;

	ipi_ptr = (struct vow_ipi_combined_info_t *)msg_data;
	/* IPIMSG_VOW_BARGEIN_DUMP_INFO */
	if (ipi_ptr->ipi_type_flag & BARGEIN_DUMP_INFO_IDX_MASK) {
		delay_info[0] = ipi_ptr->dump_frm_cnt;
		delay_info[1] = ipi_ptr->voice_sample_delay;
		VOWDRV_DEBUG("[BargeIn] delay info %d %d\n",
				 delay_info[0],
				 delay_info[1]);
		vow_dump_info[DUMP_DELAY_INFO].scp_dump_size[0] = sizeof(delay_info);
		vow_dump_info[DUMP_DELAY_INFO].scp_dump_offset[0] = 0;
	}
	if (vowserv.dump_pcm_flag == true) {
		spin_lock_irqsave(&vowdrv_dump_lock, flags);
		if ((ipi_ptr->ipi_type_flag & INPUT_DUMP_IDX_MASK)) {
			if (vow_dump_info[DUMP_INPUT].scp_dump_size[0] != 0) {
				VOWDRV_DEBUG("%s WARNING dump idx 0x%x %d not handled\n",
					__func__,
					INPUT_DUMP_IDX_MASK,
					vow_dump_info[DUMP_INPUT].scp_dump_size[0]);
			}
			vow_dump_info[DUMP_INPUT].scp_dump_size[0] = ipi_ptr->mic_dump_size;
			vow_dump_info[DUMP_INPUT].scp_dump_offset[0] = ipi_ptr->mic_offset;
			if (vowserv.vow_mic_number == 2) {
				vow_dump_info[DUMP_INPUT].scp_dump_size[1] =
						ipi_ptr->mic_dump_size;
				vow_dump_info[DUMP_INPUT].scp_dump_offset[1] =
						ipi_ptr->mic_offset_R;
			}
		}
		/* IPIMSG_VOW_BARGEIN_PCMDUMP_OK */
		if ((ipi_ptr->ipi_type_flag & BARGEIN_DUMP_IDX_MASK)) {
			vow_dump_info[DUMP_BARGEIN].scp_dump_size[0] = ipi_ptr->echo_dump_size;
			vow_dump_info[DUMP_BARGEIN].scp_dump_offset[0] = ipi_ptr->echo_offset;
		}
		if ((ipi_ptr->ipi_type_flag & RECOG_DUMP_IDX_MASK)) {
			vow_dump_info[DUMP_RECOG].scp_dump_size[0] =
				ipi_ptr->recog_dump_size;
			vow_dump_info[DUMP_RECOG].scp_dump_offset[0] =
				ipi_ptr->recog_dump_offset;
			if (vowserv.vow_mic_number == 2) {
				vow_dump_info[DUMP_RECOG].scp_dump_size[1] =
					ipi_ptr->recog_dump_size;
				vow_dump_info[DUMP_RECOG].scp_dump_offset[1] =
					ipi_ptr->recog_dump_offset_R;
			}
		}
		if ((ipi_ptr->ipi_type_flag & VFFP_DUMP_IDX_MASK)) {
			vow_dump_info[DUMP_VFFP].scp_dump_size[0] =
				ipi_ptr->vffp_dump_size;
			vow_dump_info[DUMP_VFFP].scp_dump_offset[0] =
				ipi_ptr->vffp_dump_offset;
			/* 1st and 2nd are the same */
			vow_dump_info[DUMP_VFFP].scp_dump_size[1] =
				ipi_ptr->vffp_dump_size;
			vow_dump_info[DUMP_VFFP].scp_dump_offset[1] =
				ipi_ptr->vffp_dump_offset_2nd_ch;
		}
		dump_package.dump_data_type =
			   (ipi_ptr->ipi_type_flag & SCP_DUMP_DATA_MASK);
		spin_unlock_irqrestore(&vowdrv_dump_lock, flags);
	}
	/* IPIMSG_VOW_DATAREADY */
	if ((ipi_ptr->ipi_type_flag & DEBUG_DUMP_IDX_MASK) &&
		(vowserv.recording_flag)) {
		vowserv.scp_shared_voice_buf_offset = ipi_ptr->voice_buf_offset;
		vowserv.scp_shared_voice_length = ipi_ptr->voice_length;
		if (vowserv.scp_shared_voice_length > 320)
			VOWDRV_DEBUG("vow,v_len=%x\n",
					 vowserv.scp_shared_voice_length);
		vow_service_getVoiceData();
	}
}

void vow_ipi_rx_internal(unsigned int msg_id,
			 void *msg_data)
{
	switch (msg_id) {
	case IPIMSG_VOW_COMBINED_INFO: {
		struct vow_ipi_combined_info_t *ipi_ptr;
		bool bypass_flag;

		ipi_ptr = (struct vow_ipi_combined_info_t *)msg_data;
		/* IPIMSG_VOW_RECOGNIZE_OK */
		/*VOWDRV_DEBUG("[vow] IPIMSG_VOW_COMBINED_INFO, flag=0x%x\n",*/
		/*	       ipi_ptr->ipi_type_flag);*/
		bypass_flag = false;
		if (ipi_ptr->ipi_type_flag & RECOG_OK_IDX_MASK) {
			if ((vowserv.recording_flag == true) &&
			    (vowserv.tx_keyword_start == true)) {
				VOWDRV_DEBUG("%s(), bypass this recog ok\n",
					__func__);
				bypass_flag = true;
			}
			if (bypass_flag == false) {
				/* toggle wakelock for abort suspend flow */
				__pm_stay_awake(vow_ipi_suspend_lock);
				__pm_relax(vow_ipi_suspend_lock);
				VOWDRV_DEBUG("%s(), receive recog_ok_ipi\n",
					__func__);
				vowserv.ap_received_ipi_cycle =
					get_cycles();
				vowserv.scp_recognize_ok_cycle =
					ipi_ptr->recog_ok_os_timer;
				vowserv.enter_phase3_cnt++;
				if (vowserv.bypass_enter_phase3 == false) {
					vow_ipi_reg_ok(
					    (short)ipi_ptr->recog_ok_keywordid,
					    ipi_ptr->confidence_lv,
					    ipi_ptr->extra_data_len,
					    ipi_ptr->payloaddump_len);
				}
			}
		}
		// Copy scp shared buffer into kernel internal buffer
		vow_ipi_rx_handle_data_msg(msg_data);
		break;
	}
	case IPIMSG_VOW_RETURN_VALUE: {
		unsigned int return_id;
		unsigned int return_value;
		unsigned int ipi_value = *(unsigned int *)msg_data;

		return_id    = (ipi_value >> WORD_H);
		return_value = (ipi_value & WORD_L_MASK);
		VOWDRV_DEBUG("%s(), IPIMSG_VOW_RETURN_VALUE, id:%d, val:%d\r",
			__func__, return_id, return_value);
		switch (return_id) {
		case VOW_FLAG_FORCE_PHASE1_DEBUG:
		case VOW_FLAG_FORCE_PHASE2_DEBUG:
			vowserv.force_phase_stage = return_value;
			break;
		case VOW_FLAG_SWIP_LOG_PRINT:
			vowserv.swip_log_enable = return_value;
			break;
		default:
			break;
		}
		break;
	}
	case IPIMSG_VOW_ALEXA_ENGINE_VER: {
		VOWDRV_DEBUG("%s(), IPIMSG_VOW_ALEXA_ENGINE_VER %s\r",
			__func__, msg_data);
		memcpy(vowserv.alexa_engine_version, msg_data,
				 sizeof(vowserv.alexa_engine_version));
		}
		break;
	case IPIMSG_VOW_GOOGLE_ENGINE_VER: {
		unsigned int *temp = (unsigned int *)msg_data;

		VOWDRV_DEBUG("%s(), IPIMSG_VOW_GOOGLE_ENGINE_VER 0x%x\r",
			__func__, temp[0]);
		vowserv.google_engine_version = temp[0];
		}
		break;
	case IPIMSG_VOW_GOOGLE_ARCH: {
		VOWDRV_DEBUG("%s(), IPIMSG_VOW_GOOGLE_ARCH %s\r",
			__func__, msg_data);
		memcpy(vowserv.google_engine_arch, msg_data,
				 sizeof(vowserv.google_engine_arch));
		}
		break;
	default:
		break;
	}
}

bool vow_ipi_rceive_ack(unsigned int msg_id,
			unsigned int msg_data)
{
	bool result = false;

	switch (msg_id) {
	case IPIMSG_VOW_ENABLE:
	case IPIMSG_VOW_DISABLE:
	case IPIMSG_VOW_SET_MODEL:
	case IPIMSG_VOW_SET_SMART_DEVICE:
	case IPIMSG_VOW_APREGDATA_ADDR:
	case IPIMSG_VOW_PCM_DUMP_ON:
	case IPIMSG_VOW_PCM_DUMP_OFF:
	case IPIMSG_VOW_SET_FLAG:
		result = true;
		break;
	case IPIMSG_VOW_SET_BARGEIN_ON:
	case IPIMSG_VOW_SET_BARGEIN_OFF:
		result = true;
		break;
	default:
		VOWDRV_DEBUG("%s(), no relate msg id\r", __func__);
		break;
	}
	return result;
}

static void vow_ipi_reg_ok(short keyword,
			   int confidence_lv,
			   unsigned int extradata_len,
			   unsigned int payloaddump_len)
{
	int slot;

	vowserv.scp_command_flag = true;
	/* transfer keyword id to model handle id */
	slot = vow_service_SearchSpeakerModelWithKeyword(keyword);
	if (slot < 0) {
		VOWDRV_DEBUG("%s(), Fail !! Not keyword event !!, exit\n",
			     __func__);
		return;
	}
	/* vowserv.scp_command_id = vowserv.vow_speaker_model[slot].id; */
	vowserv.scp_command_keywordid = keyword;
	vowserv.confidence_level = confidence_lv;
	if (extradata_len <= VOW_EXTRA_DATA_SIZE)
		vowserv.extradata_bytelen = extradata_len;
	else
		vowserv.extradata_bytelen = 0;
#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
	vowserv.payloaddump_length = payloaddump_len;
	//VOWDRV_DEBUG("[vow PDR] payloaddump_length = 0x%x\n", ipi_ptr->payloaddump_length);
#endif

	/* VOWDRV_DEBUG("%s(), extradata_bytelen = %d\r", */
	/*	     __func__, vowserv.extradata_bytelen); */
	VowDrv_Wait_Queue_flag = 1;
	wake_up_interruptible(&VowDrv_Wait_Queue);
}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static void vow_register_feature(enum feature_id id)
{
	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, bypass register id(%d)\n", id);
		return;
	}
	scp_register_feature(id);
}

static void vow_deregister_feature(enum feature_id id)
{
	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, bypass deregister id(%d)\n", id);
		return;
	}
	scp_deregister_feature(id);
}
#endif
static void vow_service_getVoiceData(void)
{
	if (VoiceData_Wait_Queue_flag == 0) {
		VoiceData_Wait_Queue_flag = 1;
		wake_up_interruptible(&VoiceData_Wait_Queue);
	} else {
		/* VOWDRV_DEBUG("getVoiceData but no one wait for it, */
		/* may lost it!!\n"); */
	}
}

/*****************************************************************************
 * DSP SERVICE FUNCTIONS
 *****************************************************************************/
static void vow_service_Init(void)
{
	int I;
	bool ret;
	unsigned int vow_ipi_buf[3];

	VOWDRV_DEBUG("%s():%x\n", __func__, init_flag);
	/* common part */
	vowserv.scp_command_flag = false;
	vowserv.tx_keyword_start = false;
	/*Initialization*/
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	vowserv.voicedata_scp_ptr =
		(char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
		+ VOW_VOICEDATA_OFFSET;
	vowserv.voicedata_scp_addr =
		scp_get_reserve_mem_phys(VOW_MEM_ID)
		+ VOW_VOICEDATA_OFFSET;
	/*Extra data*/
	vowserv.extradata_ptr =
		(char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
		+ VOW_EXTRA_DATA_OFFSET;
	vowserv.extradata_addr =
		scp_get_reserve_mem_phys(VOW_MEM_ID)
		+ VOW_EXTRA_DATA_OFFSET;

#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
	/* use voice data R space to exchange payload data */
	vowserv.payloaddump_scp_ptr =
		(char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
		+ VOW_VOICEDATA_OFFSET + VOW_VOICEDATA_SIZE;
	vowserv.payloaddump_scp_addr =
		scp_get_reserve_mem_phys(VOW_MEM_ID)
		+ VOW_VOICEDATA_OFFSET + VOW_VOICEDATA_SIZE;
#endif
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	vow_ipi_send(IPIMSG_VOW_GET_ALEXA_ENGINE_VER, 0, NULL,
			 VOW_IPI_BYPASS_ACK);
	vow_ipi_send(IPIMSG_VOW_GET_GOOGLE_ENGINE_VER, 0, NULL,
			 VOW_IPI_BYPASS_ACK);
	vow_ipi_send(IPIMSG_VOW_GET_GOOGLE_ARCH, 0, NULL,
			 VOW_IPI_BYPASS_ACK);

	//audio_load_task(TASK_SCENE_VOW);

	if (init_flag != 1) {
		/*Initialization*/
		VowDrv_Wait_Queue_flag = 0;
		VoiceData_Wait_Queue_flag = 0;
		vowserv.recording_flag = false;
		vowserv.suspend_lock = 0;
		vowserv.scp_shared_voice_length = 0;
		vowserv.firstRead = false;
		vowserv.scp_shared_voice_buf_offset = 0;
		vowserv.bypass_enter_phase3 = false;
		vowserv.enter_phase3_cnt = 0;
		vowserv.scp_recovering = false;
		vowserv.vow_recovering = false;
		spin_lock(&vowdrv_lock);
		vowserv.pwr_status = VOW_PWR_OFF;
		vowserv.eint_status = VOW_EINT_DISABLE;
		spin_unlock(&vowdrv_lock);
		vowserv.force_phase_stage = NO_FORCE;
		vowserv.swip_log_enable = true;
		vowserv.voicedata_user_addr = 0;
		vowserv.voicedata_user_size = 0;
		vowserv.voicedata_user_return_size_addr = 0;
		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			vowserv.vow_speaker_model[I].model_ptr = NULL;
			vowserv.vow_speaker_model[I].id = -1;
			vowserv.vow_speaker_model[I].keyword = -1;
			vowserv.vow_speaker_model[I].uuid = 0;
			vowserv.vow_speaker_model[I].flag = 0;
			vowserv.vow_speaker_model[I].enabled = 0;
		}
		mutex_lock(&vow_extradata_mutex);
		vowserv.extradata_mem_ptr = NULL;
		mutex_unlock(&vow_extradata_mutex);
		vowserv.extradata_bytelen = 0;
#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
		/* use voice data R space to exchange payload data */
		vowserv.payloaddump_user_addr = 0;
		vowserv.payloaddump_user_max_size = 0;
		vowserv.payloaddump_user_return_size_addr = 0;
		vowserv.payloaddump_kernel_ptr = NULL;
		vowserv.payloaddump_length = 0;
#endif
		vowserv.voicedata_kernel_ptr = NULL;
		mutex_lock(&voicedata_mutex);
		vowserv.kernel_voicedata_idx = 0;
		mutex_unlock(&voicedata_mutex);
		memset((void *)vow_dump_info, 0, sizeof(vow_dump_info));
		init_flag = 1;
		vowserv.dump_pcm_flag = false;
		vowserv.split_dumpfile_flag = false;
		vowserv.interleave_pcmdata_ptr = NULL;
		vowserv.custom_model_addr = 0;
		vowserv.custom_model_size = 0;
		// update here when vow support more than 2 mic
		vowserv.vow_mic_number = VOW_MAX_MIC_NUM;
		vow_pcm_dump_init();
		vowserv.scp_dual_mic_switch = VOW_ENABLE_DUAL_MIC;
		vowserv.mtkif_type = 0;
		//set default value to platform identifier and version
		memset(vowserv.google_engine_arch, 0, VOW_ENGINE_INFO_LENGTH_BYTE);
		if (sprintf(vowserv.google_engine_arch, "32fe89be-5205-3d4b-b8cf-55d650d9d200") < 0)
			VOWDRV_DEBUG("%s(), sprintf fail", __func__);
		vowserv.google_engine_version = DEFAULT_GOOGLE_ENGINE_VER;
		memset(vowserv.alexa_engine_version, 0, VOW_ENGINE_INFO_LENGTH_BYTE);
	} else {
		int ipi_size;
		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			if ((vowserv.vow_speaker_model[I].flag > 1) ||
			    (vowserv.vow_speaker_model[I].enabled > 1)) {
				VOWDRV_DEBUG("reset speaker_model[%d]", I);
				vowserv.vow_speaker_model[I].model_ptr = NULL;
				vowserv.vow_speaker_model[I].id = -1;
				vowserv.vow_speaker_model[I].keyword = -1;
				vowserv.vow_speaker_model[I].uuid = 0;
				vowserv.vow_speaker_model[I].flag = 0;
				vowserv.vow_speaker_model[I].enabled = 0;
			}
		}
		vow_ipi_buf[0] = vowserv.voicedata_scp_addr;
		vow_ipi_buf[1] = vowserv.extradata_addr;
		vow_ipi_buf[2] = VOW_EXTRA_DATA_SIZE;
		ipi_size = 3;
#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
		vow_ipi_buf[3] = vowserv.payloaddump_scp_addr;
		ipi_size = 4;
#endif
		ret = vow_ipi_send(IPIMSG_VOW_APREGDATA_ADDR,
				   ipi_size,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);
		if (ret == 0) {
			VOWDRV_DEBUG(
			"IPIMSG_VOW_APREGDATA_ADDR ipi send error\n");
		}
#if VOW_PRE_LEARN_MODE
		VowDrv_SetFlag(VOW_FLAG_PRE_LEARN, true);
#endif
	}
}

static int vow_service_GetParameter(unsigned long arg)
{
	unsigned long vow_info_ap[MAX_VOW_INFO_LEN];

	if (copy_from_user((void *)(&vow_info_ap[0]),
			   (const void __user *)(arg),
			   sizeof(vow_info_ap))) {
		VOWDRV_DEBUG("vow get parameter fail\n");
		return -EFAULT;
	}
	if (vow_info_ap[3] > VOW_MODEL_SIZE ||
	    vow_info_ap[3] < VOW_MODEL_SIZE_THRES) {
		VOWDRV_DEBUG("vow Modle Size is incorrect %d\n",
			     vow_info_ap[3]);
		return -EFAULT;
	}
	memcpy(vowserv.vow_info_apuser, vow_info_ap,
				 sizeof(vow_info_ap));
	VOWDRV_DEBUG(
	"vow get parameter: id %lu, keyword %lu, mdl_ptr 0x%x, mdl_sz %lu\n",
		     vowserv.vow_info_apuser[0],
		     vowserv.vow_info_apuser[1],
		     vowserv.vow_info_apuser[2],
		     vowserv.vow_info_apuser[3]);
	VOWDRV_DEBUG(
	"vow get parameter: return size addr 0x%x, uuid %d, data 0x%x\n",
		     vowserv.vow_info_apuser[4],
		     vowserv.vow_info_apuser[5],
		     vowserv.vow_info_apuser[6]);

	return 0;
}

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static int vow_service_CopyModel(int slot)
{
	if (slot >= MAX_VOW_SPEAKER_MODEL || slot < 0) {
		VOWDRV_DEBUG("%s(), slot id=%d, over range\n", __func__, slot);
		return -EDOM;
	}
	if (copy_from_user((void *)(vowserv.vow_speaker_model[slot].model_ptr),
			   (const void __user *)(vowserv.vow_info_apuser[2]),
			   vowserv.vow_info_apuser[3])) {
		VOWDRV_DEBUG("vow Copy Speaker Model Fail\n");
		return -EFAULT;
	}

	vowserv.vow_speaker_model[slot].flag = 1;
	vowserv.vow_speaker_model[slot].enabled = 0;
	vowserv.vow_speaker_model[slot].id = vowserv.vow_info_apuser[0];
	vowserv.vow_speaker_model[slot].keyword = vowserv.vow_info_apuser[1];
	vowserv.vow_speaker_model[slot].uuid = vowserv.vow_info_apuser[5];
	vowserv.vow_speaker_model[slot].model_size = vowserv.vow_info_apuser[3];

	return 0;
}
#endif  /* #ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT */

static int vow_service_FindFreeSpeakerModel(void)
{
	int I;

	I = 0;
	do {
		if (vowserv.vow_speaker_model[I].flag == 0)
			break;
		I++;
	} while (I < MAX_VOW_SPEAKER_MODEL);

	VOWDRV_DEBUG("vow FindFreeSpeakerModel:%d\n", I);

	if (I == MAX_VOW_SPEAKER_MODEL) {
		VOWDRV_DEBUG("vow Find Free Speaker Model Fail\n");
		return -1;
	}
	return I;
}

static int vow_service_SearchSpeakerModelWithId(int id)
{
	int I;

	I = 0;
	do {
		if (vowserv.vow_speaker_model[I].id == id)
			break;
		I++;
	} while (I < MAX_VOW_SPEAKER_MODEL);

	if (I == MAX_VOW_SPEAKER_MODEL) {
		VOWDRV_DEBUG("vow Search Speaker Model By ID Fail:%x\n", id);
		return -1;
	}
	return I;
}

static int vow_service_SearchSpeakerModelWithKeyword(int keyword)
{
	int I;

	I = 0;
	do {
		if (vowserv.vow_speaker_model[I].keyword == keyword) {
			VOWDRV_DEBUG("vow Search Speaker Model By Keyword Success !, keyword:%x\n",
				     keyword);
			break;
		}
		I++;
	} while (I < MAX_VOW_SPEAKER_MODEL);

	if (I == MAX_VOW_SPEAKER_MODEL) {
		return -1;
	}
	return I;
}

static bool vow_service_SendSpeakerModel(int slot, bool release_flag)
{
	bool ret = false;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	unsigned int vow_ipi_buf[5];

	if (slot >= MAX_VOW_SPEAKER_MODEL || slot < 0) {
		VOWDRV_DEBUG("%s(), slot id=%d, over range\n", __func__, slot);
		return ret;
	}

	if (release_flag == VOW_CLEAN_MODEL) {
		if (vowserv.vow_speaker_model[slot].flag == 0) {
			VOWDRV_DEBUG("%s(), slot:%d, no model need to clean\n",
				     __func__, slot);
			return ret;
		}
		vow_ipi_buf[0] = VOW_MODEL_CLEAR;
	} else {  /* VOW_SET_MODEL */
		if ((vowserv.vow_speaker_model[slot].flag == 0) ||
		    (vowserv.vow_speaker_model[slot].id == -1) ||
		    (vowserv.vow_speaker_model[slot].keyword == -1) ||
		    (vowserv.vow_speaker_model[slot].uuid == 0) ||
		    (vowserv.vow_speaker_model[slot].model_size == 0)) {
			VOWDRV_DEBUG("%s(), slot:%d, no model need to set\n",
				     __func__, slot);
			return ret;
		}
		vow_ipi_buf[0] = VOW_MODEL_SPEAKER;
	}
	vow_ipi_buf[1] = vowserv.vow_speaker_model[slot].uuid;
	vow_ipi_buf[2] = scp_get_reserve_mem_phys(VOW_MEM_ID) +
			 (VOW_MODEL_SIZE * slot);
	vow_ipi_buf[3] = vowserv.vow_speaker_model[slot].model_size;
	vow_ipi_buf[4] = vowserv.vow_speaker_model[slot].keyword;

	VOWDRV_DEBUG(
	    "Model:slot_%d, model_%x, addr_%x, id_%x, size_%x, keyword_%x\n",
		      slot,
		      vow_ipi_buf[0],
		      vow_ipi_buf[2],
		      vow_ipi_buf[1],
		      vow_ipi_buf[3],
		      vow_ipi_buf[4]);

	ret = vow_ipi_send(IPIMSG_VOW_SET_MODEL,
			   5,
			   &vow_ipi_buf[0],
			   VOW_IPI_BYPASS_ACK);
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	return ret;
}

static bool vow_service_ReleaseSpeakerModel(int id)
{
	int I;
	bool ret = false;

	I = vow_service_SearchSpeakerModelWithId(id);

	if (I == -1) {
		VOWDRV_DEBUG("vow release Speaker Model Fail, id:%x\n", id);
		return false;
	}
	VOWDRV_DEBUG("vow ReleaseSpeakerModel:id_%x, slot_%d\n", id, I);

	ret = vow_service_SendSpeakerModel(I, VOW_CLEAN_MODEL);

	vowserv.vow_speaker_model[I].model_ptr = NULL;
	vowserv.vow_speaker_model[I].uuid = 0;
	vowserv.vow_speaker_model[I].id = -1;
	vowserv.vow_speaker_model[I].keyword = -1;
	vowserv.vow_speaker_model[I].flag = 0;
	vowserv.vow_speaker_model[I].enabled = 0;

	return ret;
}

static bool vow_service_SetSpeakerModel(unsigned long arg)
{
	bool ret = false;
	int I;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	char *ptr8;
#endif

	I = vow_service_FindFreeSpeakerModel();
	if (I == -1)
		return false;

	if (vow_service_GetParameter(arg) != 0)
		return false;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	vowserv.vow_speaker_model[I].model_ptr =
	   (void *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
	   + (VOW_MODEL_SIZE * I);

	if (vow_service_CopyModel(I) != 0)
		return false;

	ptr8 = (char *)vowserv.vow_speaker_model[I].model_ptr;
	VOWDRV_DEBUG("SetSPKModel:slot: %d, ID: %x, UUID: %x, flag: %x\n",
		      I,
		      vowserv.vow_speaker_model[I].id,
		      vowserv.vow_speaker_model[I].keyword,
		      vowserv.vow_speaker_model[I].uuid,
		      vowserv.vow_speaker_model[I].flag);
	VOWDRV_DEBUG("vow SetSPKModel:CheckValue:%x %x %x %x %x %x\n",
		      *(char *)&ptr8[0], *(char *)&ptr8[1],
		      *(char *)&ptr8[2], *(char *)&ptr8[3],
		      *(short *)&ptr8[160], *(int *)&ptr8[7960]);

	ret = vow_service_SendSpeakerModel(I, VOW_SET_MODEL);
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	return ret;
}

static bool vow_service_SetCustomModel(unsigned long arg)
{
	bool ret = false;
	struct vow_engine_info_t info;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	struct vow_engine_info_t *p_info = &info;
	phys_addr_t p_virt;
	phys_addr_t p_phys;
	phys_addr_t p_mdl_v;
	phys_addr_t p_mdl_p;
	uint32_t data_size = 0;
	unsigned int vow_ipi_buf[2];
#endif

	if (copy_from_user((void *)&info,
			   (const void __user *)arg,
			   sizeof(struct vow_engine_info_t))) {
		VOWDRV_DEBUG("vow get cust info fail\n");
		return false;
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	p_virt = scp_get_reserve_mem_virt(VOW_MEM_ID);
	p_phys = scp_get_reserve_mem_phys(VOW_MEM_ID);
	p_mdl_v = p_virt + VOW_CUSTOM_MODEL_OFFSET;
	p_mdl_p = p_phys + VOW_CUSTOM_MODEL_OFFSET;
	if (copy_from_user((void *)&data_size,
			   (const void __user *)p_info->return_size_addr,
			   sizeof(uint32_t))) {
		VOWDRV_DEBUG("vow get cust size fail\n");
		return false;
	}
	if (data_size > VOW_MAX_CUST_MODEL_SIZE || data_size == 0) {
		VOWDRV_DEBUG("vow set cust model fail, invalid size %ld\n", data_size);
		return false;
	}
	VOWDRV_DEBUG("vow set cust model, size %d\n", data_size);
	if (copy_from_user((void *)p_mdl_v,
			   (const void __user *)p_info->data_addr,
			   data_size)) {
		VOWDRV_DEBUG("vow copy cust model fail\n");
		return false;
	}
	vowserv.custom_model_addr = (unsigned int)p_mdl_p;
	vowserv.custom_model_size = (unsigned long)data_size;
	vow_ipi_buf[0] = (unsigned int)p_mdl_p;
	vow_ipi_buf[1] = (unsigned long)data_size;
	ret = vow_ipi_send(IPIMSG_VOW_SET_CUSTOM_MODEL,
			   2,
			   &vow_ipi_buf[0],
			   VOW_IPI_BYPASS_ACK);
#else
	VOWDRV_DEBUG("vow SCP is not supported\n");
#endif
	return ret;
}
static bool vow_service_SendModelStatus(int slot, bool enable)
{
	bool ret = false;
	unsigned int vow_ipi_buf[2];

	if (slot >= MAX_VOW_SPEAKER_MODEL || slot < 0) {
		VOWDRV_DEBUG("%s(), slot id=%d, over range\n", __func__, slot);
		return ret;
	}
	if (!vowserv.vow_speaker_model[slot].flag) {
		VOWDRV_DEBUG("%s(), this speaker model isn't load\n", __func__);
		return ret;
	}

	vow_ipi_buf[0] = vowserv.vow_speaker_model[slot].keyword;
	vow_ipi_buf[1] = vowserv.vow_speaker_model[slot].confidence_lv;

	if (enable == VOW_MODEL_STATUS_START) {
		ret = vow_ipi_send(IPIMSG_VOW_MODEL_START,
				   2,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);
	} else {  /* VOW_MODEL_STATUS_STOP */
		ret = vow_ipi_send(IPIMSG_VOW_MODEL_STOP,
				   2,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);
	}

	return ret;
}

static void vow_register_vendor_feature(int uuid)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	switch (uuid) {
	case VENDOR_ID_MTK:
		vow_register_feature(VOW_VENDOR_M_FEATURE_ID);
		break;
	case VENDOR_ID_AMAZON:
		vow_register_feature(VOW_VENDOR_A_FEATURE_ID);
		break;
	case VENDOR_ID_OTHERS:
		vow_register_feature(VOW_VENDOR_G_FEATURE_ID);
		break;
	default:
		VOWDRV_DEBUG("VENDOR ID not support\n");
		break;
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
}

static void vow_deregister_vendor_feature(int uuid)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	switch (uuid) {
	case VENDOR_ID_MTK:
		vow_deregister_feature(VOW_VENDOR_M_FEATURE_ID);
		break;
	case VENDOR_ID_AMAZON:
		vow_deregister_feature(VOW_VENDOR_A_FEATURE_ID);
		break;
	case VENDOR_ID_OTHERS:
		vow_deregister_feature(VOW_VENDOR_G_FEATURE_ID);
		break;
	default:
		VOWDRV_DEBUG("VENDOR ID not support\n");
		break;
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
}

static bool vow_service_SetModelStatus(bool enable, unsigned long arg)
{
	bool ret = false;
	int slot;
	struct vow_model_start_t model_start;

	if (copy_from_user((void *)&model_start,
			   (const void __user *)(arg),
			   sizeof(struct vow_model_start_t))) {
		VOWDRV_DEBUG("vow get vow_model_start data fail\n");
		return false;
	}

	if (model_start.handle > INT_MAX || model_start.handle < INT_MIN) {
		VOWDRV_DEBUG("%s(), model_start.handle will cause truncated value\n",
					__func__);
		return false;
	}
	slot = vow_service_SearchSpeakerModelWithId(model_start.handle);
	if (slot < 0) {
		VOWDRV_DEBUG("%s(), no match id\n", __func__);
		return false;
	}
	if (enable == VOW_MODEL_STATUS_START) {
		if (vowserv.vow_speaker_model[slot].enabled == 0) {
			int uuid;

			uuid = vowserv.vow_speaker_model[slot].uuid;
			vow_register_vendor_feature(uuid);
		}
		vowserv.vow_speaker_model[slot].enabled = 1;
		vowserv.vow_speaker_model[slot].confidence_lv =
			model_start.confidence_level;
		vowserv.vow_speaker_model[slot].rx_inform_addr =
			model_start.dsp_inform_addr;
		vowserv.vow_speaker_model[slot].rx_inform_size_addr =
			model_start.dsp_inform_size_addr;
		VOWDRV_DEBUG("VOW_MODEL_START, id:%d, enabled:%d, conf_lv:%d\n",
			     (int)model_start.handle,
			     vowserv.vow_speaker_model[slot].enabled,
			     vowserv.vow_speaker_model[slot].confidence_lv);
		/* send model status to scp */
		ret = vow_service_SendModelStatus(slot, enable);
		if (ret == false)
			VOWDRV_DEBUG("vow_service_SendModelStatus, err\n");
	} else {  /* VOW_MODEL_STATUS_STOP */
		vowserv.vow_speaker_model[slot].confidence_lv =
			model_start.confidence_level;
		vowserv.vow_speaker_model[slot].rx_inform_addr =
			model_start.dsp_inform_addr;
		vowserv.vow_speaker_model[slot].rx_inform_size_addr =
			model_start.dsp_inform_size_addr;
		/* send model status to scp */
		ret = vow_service_SendModelStatus(slot, enable);
		if (ret == false)
			VOWDRV_DEBUG("vow_service_SendModelStatus, err\n");
		if (vowserv.vow_speaker_model[slot].enabled == 1) {
			int uuid;

			uuid = vowserv.vow_speaker_model[slot].uuid;
			vow_deregister_vendor_feature(uuid);
		}
		vowserv.vow_speaker_model[slot].enabled = 0;
		VOWDRV_DEBUG("VOW_MODEL_STOP, id:%d\n",
			     (int)model_start.handle);
	}
	return ret;
}

static bool vow_service_SetApDumpAddr(unsigned long arg)
{
	unsigned long vow_info[MAX_VOW_INFO_LEN];
	unsigned long id, flags;

	if (copy_from_user((void *)(&vow_info[0]), (const void __user *)(arg),
			   sizeof(vowserv.vow_info_apuser))) {
		VOWDRV_DEBUG("%s()check Ap dump parameter fail\n", __func__);
		return false;
	}

	/* add return condition */
	if ((vow_info[2] == 0) || (vow_info[3] != kReadVowDumpSize) ||
	    (vow_info[4] == 0) || (vow_info[0] >= NUM_DUMP_DATA)) {
		VOWDRV_DEBUG("%s(): error id %d, addr_%x, size_%x, addr_%x\n",
		 __func__,
		 (unsigned int)vow_info[0],
		 (unsigned int)vow_info[2],
		 (unsigned int)vow_info[3],
		 (unsigned int)vow_info[4]);
		return false;
	}
	id = vow_info[0];
	spin_lock_irqsave(&vowdrv_dump_lock, flags);
	vow_dump_info[id].user_dump_addr = vow_info[2];
	vow_dump_info[id].user_dump_size = vow_info[3];
	vow_dump_info[id].user_return_size_addr = vow_info[4];
	vow_dump_info[id].user_dump_idx = 0;
	spin_unlock_irqrestore(&vowdrv_dump_lock, flags);
	//verb log
	//VOWDRV_DEBUG("%s(): id %d, addr_%x, size_%x, addr_%x\n",
	//	 __func__,
	//	 id,
	//	 (unsigned int)vow_info[2],
	//	 (unsigned int)vow_info[3],
	//	 (unsigned int)vow_info[4]);
	return true;
}

static bool vow_service_SetApAddr(unsigned long arg)
{
	unsigned long vow_info[MAX_VOW_INFO_LEN];

	if (copy_from_user((void *)(&vow_info[0]), (const void __user *)(arg),
			   sizeof(vow_info))) {
		VOWDRV_DEBUG("vow check parameter fail\n");
		return false;
	}

	/* add return condition */
	if ((vow_info[2] == 0) || (vow_info[3] != VOW_VBUF_LENGTH) ||
	    (vow_info[4] == 0)) {
		VOWDRV_DEBUG("vow SetVBufAddr:addr_%x, size_%x, addr_%x\n",
		 (unsigned int)vow_info[2],
		 (unsigned int)vow_info[3],
		 (unsigned int)vow_info[4]);
		return false;
	}

	vowserv.voicedata_user_addr = vow_info[2];
	vowserv.voicedata_user_size = vow_info[3];
	vowserv.voicedata_user_return_size_addr = vow_info[4];

	return true;
}

static bool vow_service_SetVBufAddr(unsigned long arg)
{
	unsigned long vow_info[MAX_VOW_INFO_LEN];

	if (copy_from_user((void *)(&vow_info[0]), (const void __user *)(arg),
			   sizeof(vowserv.vow_info_apuser))) {
		VOWDRV_DEBUG("vow check parameter fail\n");
		return false;
	}

	/* add return condition */
	if ((vow_info[2] == 0) || (vow_info[3] != VOW_VBUF_LENGTH) ||
	    (vow_info[4] == 0)) {
		VOWDRV_DEBUG("vow SetVBufAddr:addr_%x, size_%x, addr_%x\n",
		 (unsigned int)vow_info[2],
		 (unsigned int)vow_info[3],
		 (unsigned int)vow_info[4]);
		return false;
	}

	mutex_lock(&vow_vmalloc_lock);
	if (vowserv.voicedata_kernel_ptr != NULL) {
		vfree(vowserv.voicedata_kernel_ptr);
		vowserv.voicedata_kernel_ptr = NULL;
	}

	if (vow_info[3] != VOW_VBUF_LENGTH) {
		mutex_unlock(&vow_vmalloc_lock);
		return false;
	}

	vowserv.voicedata_kernel_ptr = vmalloc(VOW_VBUF_LENGTH);
	mutex_unlock(&vow_vmalloc_lock);
	return true;
}

static bool vow_service_Enable(void)
{
	bool ret = false;

	VOWDRV_DEBUG("+%s()\n", __func__);

	/* extra data memory locate */
	mutex_lock(&vow_extradata_mutex);
	if (vowserv.extradata_mem_ptr == NULL) {
		vowserv.extradata_mem_ptr =
		    vmalloc(VOW_EXTRA_DATA_SIZE);
	}
	mutex_unlock(&vow_extradata_mutex);
	ret = vow_ipi_send(IPIMSG_VOW_ENABLE,
			   0,
			   NULL,
			   VOW_IPI_BYPASS_ACK);

	VOWDRV_DEBUG("-%s():%d\n", __func__, ret);
	return ret;
}

static bool vow_service_Disable(void)
{
	bool ret = false;

	VOWDRV_DEBUG("+%s()\n", __func__);

	ret = vow_ipi_send(IPIMSG_VOW_DISABLE,
			   0,
			   NULL,
			   VOW_IPI_BYPASS_ACK);

	/* extra data memory release */
	mutex_lock(&vow_extradata_mutex);
	if (vowserv.extradata_mem_ptr != NULL) {
		vfree(vowserv.extradata_mem_ptr);
		vowserv.extradata_mem_ptr = NULL;
	}
	mutex_unlock(&vow_extradata_mutex);

	/* release lock */
	if (vowserv.suspend_lock == 1) {
		vowserv.suspend_lock = 0;
		/* Let AP will suspend */
		__pm_relax(vow_suspend_lock);
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	vow_deregister_feature(VOW_FEATURE_ID);
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	return ret;
}

static void vow_check_boundary(unsigned int copy_len, unsigned int bound_len)
{
	if (copy_len > bound_len) {
		VOWDRV_DEBUG("[vow check]copy_len=0x%x, bound_len=0x%x\n",
			     copy_len, bound_len);
		VOW_ASSERT(0);
	}
}

#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
static void vow_service_ReadPayloadDumpData(unsigned int buf_length)
{
	unsigned int tx_len;
	unsigned int ret;

	VOW_ASSERT(vowserv.payloaddump_kernel_ptr != NULL);

	// copy from DRAM to get payload data
	memcpy(&vowserv.payloaddump_kernel_ptr[0],
	       vowserv.payloaddump_scp_ptr, buf_length);

	//copy to user space
	tx_len = buf_length;
	VOWDRV_DEBUG("[VOW PDR] buf_len=0x%x, MAX len=0x%x\n",
		     buf_length, vowserv.payloaddump_user_max_size);

	if (buf_length > vowserv.payloaddump_user_max_size)
		tx_len = vowserv.payloaddump_user_max_size;

	ret = copy_to_user(
		      (void __user *)(vowserv.payloaddump_user_return_size_addr),
		      &tx_len,
		      sizeof(unsigned int));
	ret = copy_to_user(
		      (void __user *)vowserv.payloaddump_user_addr,
		      vowserv.payloaddump_kernel_ptr,
		      tx_len);
}
#endif

static void vow_interleaving(short *out_buf,
			     short *l_sample,
			     short *r_sample,
			     unsigned int buf_length)
{
	int i;
	int smpl_max;

	smpl_max = buf_length / 2;
	for (i = 0; i < smpl_max; i++) {
		*out_buf++ = *l_sample++;
		*out_buf++ = *r_sample++;
	}
}

static int vow_service_ReadVoiceData_Internal(unsigned int buf_offset,
					      unsigned int buf_length)
{
	int stop_condition = 0;

	if (buf_length != 0) {
		if ((vowserv.kernel_voicedata_idx + (buf_length >> 1))
		  > (vowserv.voicedata_user_size >> 1)) {
			VOWDRV_DEBUG(
			    "[vow check]data_idx=0x%x(W),buf_length=0x%x(B)\n",
			    vowserv.kernel_voicedata_idx, buf_length);
			VOWDRV_DEBUG(
		    "[vow check] user_size=0x%x(B), scp_shared_voice_buf_offset=0x%x(B)\n",
			    (unsigned int)vowserv.voicedata_user_size,
			    buf_offset);
			/* VOW_ASSERT(0); */
			vowserv.kernel_voicedata_idx = 0;
		}
		mutex_lock(&vow_vmalloc_lock);
#if defined DUAL_CH_TRANSFER
		/* start interleaving L+R */
		vow_interleaving(
			&vowserv.voicedata_kernel_ptr[vowserv.kernel_voicedata_idx],
			(short *)(vowserv.voicedata_scp_ptr + buf_offset),
			(short *)(vowserv.voicedata_scp_ptr + buf_offset +
			    VOW_VOICEDATA_SIZE),
			buf_length);
		/* end interleaving*/
#else
		memcpy(&vowserv.voicedata_kernel_ptr[vowserv.kernel_voicedata_idx],
		       vowserv.voicedata_scp_ptr + buf_offset, buf_length);
#endif
		mutex_unlock(&vow_vmalloc_lock);

		if (buf_length > VOW_VOICE_RECORD_BIG_THRESHOLD) {
			/* means now is start to transfer */
			/* keyword buffer(64kB) to AP */
			VOWDRV_DEBUG("%s(), start tx keyword, 0x%x/0x%x\n",
				     __func__,
				     vowserv.kernel_voicedata_idx,
				     buf_length);
			vowserv.tx_keyword_start = true;
		}
		mutex_lock(&voicedata_mutex);
#if defined DUAL_CH_TRANSFER
		/* 2 Channels */
		vowserv.kernel_voicedata_idx += buf_length;
#else
		/* 1 Channel */
		vowserv.kernel_voicedata_idx += (buf_length >> 1);
#endif
		mutex_unlock(&voicedata_mutex);
	}
	if (vowserv.kernel_voicedata_idx >= (VOW_VOICE_RECORD_BIG_THRESHOLD >> 1))
		vowserv.transfer_length = VOW_VOICE_RECORD_BIG_THRESHOLD;
	else
		vowserv.transfer_length = VOW_VOICE_RECORD_THRESHOLD;

	if (vowserv.kernel_voicedata_idx >= (vowserv.transfer_length >> 1)) {
		unsigned int ret;

		/*VOWDRV_DEBUG("TX Leng:%d, %d, [%d]\n",*/
		/*	     vowserv.kernel_voicedata_idx,*/
		/*	     buf_length,*/
		/*	     vowserv.transfer_length);*/

		ret = copy_to_user(
		      (void __user *)(vowserv.voicedata_user_return_size_addr),
		      &vowserv.transfer_length,
		      sizeof(unsigned int));

		mutex_lock(&vow_vmalloc_lock);
		ret = copy_to_user(
		      (void __user *)vowserv.voicedata_user_addr,
		      vowserv.voicedata_kernel_ptr,
		      vowserv.transfer_length);
		mutex_unlock(&vow_vmalloc_lock);

		/* move left data to buffer's head */
		if (vowserv.kernel_voicedata_idx > (vowserv.transfer_length >> 1)) {
			unsigned int tmp;
			unsigned int idx;

			tmp = (vowserv.kernel_voicedata_idx << 1)
			      - vowserv.transfer_length;
			vow_check_boundary(tmp, vowserv.voicedata_user_size);
			idx = (vowserv.transfer_length >> 1);
			mutex_lock(&vow_vmalloc_lock);
			memcpy(&vowserv.voicedata_kernel_ptr[0],
			       &vowserv.voicedata_kernel_ptr[idx],
			       tmp);
			mutex_unlock(&vow_vmalloc_lock);
			mutex_lock(&voicedata_mutex);
			vowserv.kernel_voicedata_idx -= idx;
			mutex_unlock(&voicedata_mutex);
		} else {
			mutex_lock(&voicedata_mutex);
			vowserv.kernel_voicedata_idx = 0;
			mutex_unlock(&voicedata_mutex);
		}

		/* speed up voicedata transfer to hal */
		if (vowserv.kernel_voicedata_idx >= VOW_VOICE_RECORD_THRESHOLD)
			vow_service_getVoiceData();

		stop_condition = 1;
	}
	if ((vowserv.tx_keyword_start == true)
	 && (vowserv.kernel_voicedata_idx < VOW_VOICE_RECORD_THRESHOLD)) {
		/* means now is end of transfer keyword buffer(64kB) to AP */
		vowserv.tx_keyword_start = false;
		VOWDRV_DEBUG("%s(), end tx keyword, 0x%x\n",
			     __func__,
			     vowserv.kernel_voicedata_idx);
	}

	return stop_condition;
}

static void vow_service_GetVowDumpData(void)
{
	unsigned int size = 0, i = 0, ret = 0, idx = 0, user_size = 0;
	unsigned long flags;

	if (vowserv.dump_pcm_flag == false)
		return;
	for (i = 0; i < NUM_DUMP_DATA; i++) {
		//copy scp shared buffer to kernel
		struct vow_dump_info_t temp_dump_info;

		temp_dump_info = vow_dump_info[i];
		idx = temp_dump_info.kernel_dump_idx;
		if (temp_dump_info.vir_addr != NULL &&
			    temp_dump_info.scp_dump_size[0] != 0) {
			if (vowserv.vow_mic_number == 2 &&
				     temp_dump_info.scp_dump_size[1] != 0) {
				/* DRAM to kernel buffer and sample interleaving */
				if ((idx +
					    (temp_dump_info.scp_dump_size[0] * 2)) >
						temp_dump_info.kernel_dump_size) {
					size = temp_dump_info.kernel_dump_size - idx;
					VOWDRV_DEBUG("%s(), WARNING2 idx %d + %d, %d\n",
						__func__,
						idx, temp_dump_info.scp_dump_size[0] * 2,
						temp_dump_info.kernel_dump_size);
				} else {
					size = temp_dump_info.scp_dump_size[0] * 2;
				}
				vow_interleaving(
					&temp_dump_info.kernel_dump_addr[idx],
					(short *)(temp_dump_info.vir_addr +
						temp_dump_info.scp_dump_offset[0]),
					(short *)(temp_dump_info.vir_addr +
						temp_dump_info.scp_dump_offset[1]),
					size / 2);
				idx += size;
			} else { // DRAM to kernel buffer. (only 1 channel)
				if ((idx +
					    temp_dump_info.scp_dump_size[0]) >
						temp_dump_info.kernel_dump_size) {
					size = temp_dump_info.kernel_dump_size - idx;
					VOWDRV_DEBUG("%s(), WARNING1 idx %d + %d, %d\n",
						__func__,
						idx, temp_dump_info.scp_dump_size[0] * 2,
						temp_dump_info.kernel_dump_size);
				} else {
					size = temp_dump_info.scp_dump_size[0];
				}
				memcpy(&temp_dump_info.kernel_dump_addr[idx],
					temp_dump_info.vir_addr +
					temp_dump_info.scp_dump_offset[0],
					size);
				idx += size;
			}
			//{
			//	short *outPtr;
			//	outPtr = (short *)temp_dump_info.kernel_dump_addr;
			//	VOWDRV_DEBUG("%s(), %d idx %d size %d 0x%x, %d %d %d %d\n",
			//		 __func__, i, idx, size, outPtr,
			//		 outPtr[0], outPtr[1], outPtr[2], outPtr[3]
			//		 );
			//}
			//copy kernel to user space
			if ((temp_dump_info.user_dump_idx + idx) >
					 temp_dump_info.user_dump_size) {
				size = temp_dump_info.user_dump_size -
						 temp_dump_info.user_dump_idx;
			} else {
				size = idx;
			}

			mutex_lock(&vow_vmalloc_lock);
			ret = copy_to_user(
					 (void __user *)(temp_dump_info.user_dump_addr +
					 temp_dump_info.user_dump_idx),
					 temp_dump_info.kernel_dump_addr,
					 size);
			mutex_unlock(&vow_vmalloc_lock);
			temp_dump_info.user_dump_idx += size;
			user_size = (unsigned int)temp_dump_info.user_dump_idx;
			ret = copy_to_user(
				  (void __user *)(temp_dump_info.user_return_size_addr),
				  &user_size,
				  sizeof(unsigned int));
			/* if there are left kernel buffer not copied to user buffer, */
			/* move them to buffer's head */
			if (idx > size) {
				unsigned int size_left;
				unsigned int idx_left;

				size_left = idx - size;
				vow_check_boundary(size_left, temp_dump_info.kernel_dump_size);
				idx_left = size;
				mutex_lock(&vow_vmalloc_lock);
				memcpy(&temp_dump_info.kernel_dump_addr[0],
					   &temp_dump_info.kernel_dump_addr[idx_left],
					   size_left);
				mutex_unlock(&vow_vmalloc_lock);
				temp_dump_info.kernel_dump_idx = size_left;
			} else
				temp_dump_info.kernel_dump_idx = 0;
			spin_lock_irqsave(&vowdrv_dump_lock, flags);
			vow_dump_info[i].kernel_dump_idx = temp_dump_info.kernel_dump_idx;
			vow_dump_info[i].user_dump_idx = temp_dump_info.user_dump_idx;
			vow_dump_info[i].scp_dump_size[0] = 0;
			vow_dump_info[i].scp_dump_size[1] = 0;
			spin_unlock_irqrestore(&vowdrv_dump_lock, flags);
			if (temp_dump_info.kernel_dump_idx != 0) {
				VOWDRV_DEBUG("-%s(), %d kernel_idx %d\n",
					 __func__, i, vow_dump_info[i].kernel_dump_idx);
			}
		} //if (temp_dump_info.scp_dump_size[0] != 0)
	} // for (i = 0; i < NUM_DUMP_DATA; i++)
}
static void vow_service_ReadVoiceData(void)
{
	int stop_condition = 0;
	int ret = 0;
	/*int rdata;*/
	while (1) {
		if (VoiceData_Wait_Queue_flag == 0) {
			ret = wait_event_interruptible_timeout(VoiceData_Wait_Queue,
				VoiceData_Wait_Queue_flag, msecs_to_jiffies(50));
			if (!ret) {
				VOWDRV_DEBUG("%s, timeout,break\n", __func__);
				break;
			}
		}

		if (VoiceData_Wait_Queue_flag == 1) {
			VoiceData_Wait_Queue_flag = 0;
			if ((VowDrv_GetHWStatus() == VOW_PWR_OFF)
			 || (vowserv.recording_flag == false)) {
				mutex_lock(&voicedata_mutex);
				vowserv.kernel_voicedata_idx = 0;
				mutex_unlock(&voicedata_mutex);
				stop_condition = 1;
				VOWDRV_DEBUG(
				    "stop read vow voice data: %d, %d\n",
				    VowDrv_GetHWStatus(),
				    vowserv.recording_flag);
			} else {
				/* To Read Voice Data from Kernel to User */
				stop_condition =
				    vow_service_ReadVoiceData_Internal(
					vowserv.scp_shared_voice_buf_offset,
					vowserv.scp_shared_voice_length);
				vowserv.scp_shared_voice_buf_offset = 0;
				vowserv.scp_shared_voice_length = 0;
				vow_service_GetVowDumpData();
			}
			if (stop_condition == 1)
				break;
		} else {
			VOWDRV_DEBUG("%s, 50ms timeout,break\n", __func__);
			break;
		}
	}
}

static void vow_service_reset(void)
{
	int I;
	bool need_disable_vow = false;
	bool ret = false;

	VOWDRV_DEBUG("+%s()\n", __func__);
	for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
		if (vowserv.vow_speaker_model[I].enabled  == 1) {
			int uuid;

			/* need to close it */
			ret = vow_service_SendModelStatus(
				I, VOW_MODEL_STATUS_STOP);
			if (ret == false)
				VOWDRV_DEBUG("%s(), err\n", __func__);

			uuid = vowserv.vow_speaker_model[I].uuid;
			vow_deregister_vendor_feature(uuid);
			vowserv.vow_speaker_model[I].enabled = 0;
			need_disable_vow = true;
		}
		if (vowserv.vow_speaker_model[I].flag  == 1) {
			int id;

			/* need to clear model */
			id = vowserv.vow_speaker_model[I].id;
			vow_service_ReleaseSpeakerModel(id);
		}
	}
	if (need_disable_vow)
		vow_service_Disable();

	VOWDRV_DEBUG("-%s()\n", __func__);
}

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static bool vow_stop_dump_wait(void)
{
	int timeout = 0;

	while (1) {
		msleep(VOW_WAITCHECK_INTERVAL_MS);
		if (timeout++ >= VOW_STOP_DUMP_WAIT)
			return false;
	}
	return true;
}

static int vow_pcm_dump_notify(bool enable)
{
	unsigned int vow_ipi_buf[7] = {0};
	bool ret;

	/* dump flag */
	vow_ipi_buf[1] = vowserv.dump_pcm_flag;
	/* TOTAL dram resrved size for barge in dump */
	vow_ipi_buf[0] = vow_dump_info[DUMP_BARGEIN].size;
	/* address for SCP using */
	vow_ipi_buf[2] = vow_dump_info[DUMP_BARGEIN].phy_addr;

	VOWDRV_DEBUG(
	"[BargeIn]dump on, dump flag:%d, resv sz:0x%x, addr:0x%x\n",
			 vow_ipi_buf[1],
			 vow_ipi_buf[0],
			 vow_ipi_buf[2]);
	/* TOTAL dram resrved size for recog data dump */
	vow_ipi_buf[3] = vow_dump_info[DUMP_RECOG].size;
	/* address for SCP using */
	vow_ipi_buf[4] = vow_dump_info[DUMP_RECOG].phy_addr;
	/* TOTAL dram resrved size for vffp data dump */
	vow_ipi_buf[5] = vow_dump_info[DUMP_VFFP].size;
	/* address for SCP using */
	vow_ipi_buf[6] = vow_dump_info[DUMP_VFFP].phy_addr;

	VOWDRV_DEBUG(
	"[Recog]dump on, dump flag:%d, resv sz:0x%x, addr:0x%x\n",
			vow_ipi_buf[1],
			vow_ipi_buf[3],
			vow_ipi_buf[4]);
	VOWDRV_DEBUG(
	"[vffp]dump on, dump flag:%d, resv sz:0x%x, addr:0x%x\n",
			vow_ipi_buf[1],
			vow_ipi_buf[5],
			vow_ipi_buf[6]);

	/* if scp reset happened, need re-send PCM dump IPI to SCP again */
	if (enable == true) {
		ret = vow_ipi_send(IPIMSG_VOW_PCM_DUMP_ON,
				   7,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);

		if (ret == 0)
			VOWDRV_DEBUG("PCM_DUMP_ON ipi send error\n");
	} else {
		ret = vow_ipi_send(IPIMSG_VOW_PCM_DUMP_OFF,
				   7,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);
		if (ret == 0)
			VOWDRV_DEBUG("PCM_DUMP_OFF ipi send error\n");
	}
	return 0;
}

static bool vow_service_AllocKernelDumpBuffer(void)
{
	unsigned int I = 0;

	mutex_lock(&vow_vmalloc_lock);
	for (I = 0; I < NUM_DUMP_DATA; I++) {
		if (vow_dump_info[I].kernel_dump_addr != NULL) {
			vfree(vow_dump_info[I].kernel_dump_addr);
			vow_dump_info[I].kernel_dump_addr = NULL;
		}
		vow_dump_info[I].kernel_dump_addr = vmalloc(kReadVowDumpSize);
		VOWDRV_DEBUG("%s vow_dump_info[%d].kernel_dump_addr = 0x%x\n",
			     __func__, I,
			     vow_dump_info[I].kernel_dump_addr);
		VOW_ASSERT(vow_dump_info[I].kernel_dump_addr != NULL);
		vow_dump_info[I].kernel_dump_idx = 0;
		vow_dump_info[I].kernel_dump_size = kReadVowDumpSize;
	}
	mutex_unlock(&vow_vmalloc_lock);
	return true;
}

static bool vow_service_FreeKernelDumpBuffer(void)
{
	unsigned int I = 0;

	mutex_lock(&vow_vmalloc_lock);
	for (I = 0; I < NUM_DUMP_DATA; I++) {
		if (vow_dump_info[I].kernel_dump_addr != NULL) {
			vfree(vow_dump_info[I].kernel_dump_addr);
			vow_dump_info[I].kernel_dump_addr = NULL;
		}
		vow_dump_info[I].kernel_dump_idx = 0;
		vow_dump_info[I].kernel_dump_size = 0;
	}
	mutex_unlock(&vow_vmalloc_lock);
	return true;
}
#endif  /* #ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT */

static int vow_pcm_dump_set(bool enable)
{
	VOWDRV_DEBUG("%s = %d, %d\n", __func__,
		     vowserv.dump_pcm_flag,
		     (unsigned int)enable);
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	vow_dump_info[DUMP_DELAY_INFO].vir_addr =
		     (void *)&delay_info[0];
	vow_dump_info[DUMP_DELAY_INFO].size =
		     sizeof(delay_info);

	vow_dump_info[DUMP_BARGEIN].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_BARGEIN_MEM_ID))
	    + VOW_BARGEIN_AFE_MEMIF_SIZE;
	vow_dump_info[DUMP_BARGEIN].phy_addr =
	    scp_get_reserve_mem_phys(VOW_BARGEIN_MEM_ID)
	    + VOW_BARGEIN_AFE_MEMIF_SIZE;
	vow_dump_info[DUMP_BARGEIN].size = BARGEIN_DUMP_TOTAL_BYTE_CNT;
	VOWDRV_DEBUG("[Barge]vir: %p, phys: 0x%x\n",
		     vow_dump_info[DUMP_BARGEIN].vir_addr,
		     (unsigned int)vow_dump_info[DUMP_BARGEIN].phy_addr);
	// input share same address of bargein
	vow_dump_info[DUMP_INPUT].vir_addr =
		     vow_dump_info[DUMP_BARGEIN].vir_addr;
	vow_dump_info[DUMP_INPUT].phy_addr =
		     vow_dump_info[DUMP_BARGEIN].phy_addr;

	vow_dump_info[DUMP_RECOG].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
	    + VOW_RECOGDATA_OFFSET;
	vow_dump_info[DUMP_RECOG].phy_addr =
	    scp_get_reserve_mem_phys(VOW_MEM_ID)
	    + VOW_RECOGDATA_OFFSET;
	vow_dump_info[DUMP_RECOG].size = RECOG_DUMP_TOTAL_BYTE_CNT;
	VOWDRV_DEBUG("[Recog]vir: %p, phys: 0x%x\n",
		     vow_dump_info[DUMP_RECOG].vir_addr,
		     (unsigned int)vow_dump_info[DUMP_RECOG].phy_addr);

	vow_dump_info[DUMP_VFFP].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
	    + VOW_VFFPDATA_OFFSET;
	vow_dump_info[DUMP_VFFP].phy_addr =
	    scp_get_reserve_mem_phys(VOW_MEM_ID)
	    + VOW_VFFPDATA_OFFSET;
	vow_dump_info[DUMP_VFFP].size = VFFP_DUMP_TOTAL_BYTE_CNT;
	VOWDRV_DEBUG("[vffp]vir: %p, phys: 0x%x\n",
		     vow_dump_info[DUMP_VFFP].vir_addr,
		     (unsigned int)vow_dump_info[DUMP_VFFP].phy_addr);

	if ((vowserv.dump_pcm_flag == false) && (enable == true)) {
		vowserv.dump_pcm_flag = true;
		vow_service_AllocKernelDumpBuffer();
		vow_pcm_dump_notify(true);
	} else if ((vowserv.dump_pcm_flag == true) && (enable == false)) {
		vowserv.dump_pcm_flag = false;
		vow_pcm_dump_notify(false);
		vow_stop_dump_wait();
		vow_service_FreeKernelDumpBuffer();
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif  /* #ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT */
	return 0;
}

static void vow_pcm_dump_init(void)
{
	VOWDRV_DEBUG("[Recog] %s()\n", __func__);
	if (vowserv.interleave_pcmdata_ptr != NULL) {
		vfree(vowserv.interleave_pcmdata_ptr);
		vowserv.interleave_pcmdata_ptr = NULL;
	}
	/* Temp buffer for doing DUALMIC L/R channels interleave */
	vowserv.interleave_pcmdata_ptr =
		vmalloc(VOW_PCM_DUMP_BYTE_SIZE << 1);
}

static void vow_pcm_dump_deinit(void)
{
	VOWDRV_DEBUG("[BargeIn] %s()\n", __func__);

	if (vowserv.interleave_pcmdata_ptr != NULL) {
		vfree(vowserv.interleave_pcmdata_ptr);
		vowserv.interleave_pcmdata_ptr = NULL;
	}
}
/*****************************************************************************
 * VOW CONTROL FUNCTIONS
 *****************************************************************************/

static int VowDrv_SetHWStatus(int status)
{
	int ret = 0;

	VOWDRV_DEBUG("%s(): set:%x, cur:%x\n",
		     __func__, status, vowserv.pwr_status);
	if ((status < NUM_OF_VOW_PWR_STATUS) && (status >= VOW_PWR_OFF)) {
		spin_lock(&vowdrv_lock);
		vowserv.pwr_status = status;
		spin_unlock(&vowdrv_lock);
	} else {
		VOWDRV_DEBUG("error input: %d\n", status);
		ret = -1;
	}
	return ret;
}

static int VowDrv_GetHWStatus(void)
{
	int ret = 0;

	spin_lock(&vowdrv_lock);
	ret = vowserv.pwr_status;
	spin_unlock(&vowdrv_lock);
	return ret;
}

int VowDrv_EnableHW(int status)
{
	int ret = 0;
	int pwr_status = 0;

	VOWDRV_DEBUG("%s(): %x\n", __func__, status);

	if (status < 0) {
		VOWDRV_DEBUG("%s(), error input: %x\n", __func__, status);
		ret = -1;
	} else {
		pwr_status = (status == 0) ? VOW_PWR_OFF : VOW_PWR_ON;

		if (pwr_status == VOW_PWR_OFF) {
			/* reset the transfer limitation to */
			/* avoid obstructing phase2.5 transferring */
			if (vowserv.tx_keyword_start == true)
				vowserv.tx_keyword_start = false;
		}
		if (pwr_status == VOW_PWR_ON) {
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
			vow_register_feature(VOW_FEATURE_ID);
#else
			VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
			/* clear enter_phase3_cnt */
			vowserv.enter_phase3_cnt = 0;
		}
		VowDrv_SetHWStatus(pwr_status);
	}
	return ret;
}

int VowDrv_ChangeStatus(void)
{
	VowDrv_Wait_Queue_flag = 1;
	wake_up_interruptible(&VowDrv_Wait_Queue);
	return 0;
}

void VowDrv_SetSmartDevice(bool enable)
{
	unsigned int eint_num;
	unsigned int ints[2] = {0, 0};
	unsigned int vow_ipi_buf[2];
	bool ret;

	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, do not support VOW\n");
		return;
	}

	VOWDRV_DEBUG("%s():%x\n", __func__, enable);
	if (vowserv.node) {
		/* query eint number from device tree */
		ret = of_property_read_u32_array(vowserv.node,
						 "debounce",
						 ints,
						 ARRAY_SIZE(ints));
		if (ret != 0) {
			VOWDRV_DEBUG("%s(), no debounce node, ret=%d\n",
				     __func__, ret);
			return;
		}

		eint_num = ints[0];

		if (enable == false)
			eint_num = 0xFF;

		vow_ipi_buf[0] = enable;
		vow_ipi_buf[1] = eint_num;
		ret = vow_ipi_send(IPIMSG_VOW_SET_SMART_DEVICE,
				   2,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);
		if (ret == 0) {
			VOWDRV_DEBUG(
			    "IPIMSG_VOW_SET_SMART_DEVICE ipi send error\n\r");
		}
	} else {
		/* no node here */
		VOWDRV_DEBUG("there is no node\n");
	}
}

void VowDrv_SetSmartDevice_GPIO(bool enable)
{
	int ret = 0;

	if (vowserv.node) {
		if (enable == false) {
			VOWDRV_DEBUG("VowDrv_SetSmartDev_gpio:OFF\n");
			ret = pinctrl_select_state(vowserv.pinctrl,
						   vowserv.pins_eint_off);
			if (ret) {
				/* pinctrl setting error */
				VOWDRV_DEBUG(
				"error, can not set gpio vow pins_eint_off\n");
			}
		} else {
			VOWDRV_DEBUG("VowDrv_SetSmartDev_gpio:ON\n");
			ret = pinctrl_select_state(vowserv.pinctrl,
						   vowserv.pins_eint_on);
			if (ret) {
				/* pinctrl setting error */
				VOWDRV_DEBUG(
				"error, can not set gpio vow pins_eint_on\n");
			}
		}
	} else {
		/* no node here */
		VOWDRV_DEBUG("there is no node\n");
	}
}

static bool VowDrv_SetFlag(int type, unsigned int set)
{
	bool ret = false;
	unsigned int vow_ipi_buf[2];

	VOWDRV_DEBUG("%s(), type:%x, set:%x\n", __func__, type, set);
	vow_ipi_buf[0] = type;
	vow_ipi_buf[1] = set;

	ret = vow_ipi_send(IPIMSG_VOW_SET_FLAG,
			   2,
			   &vow_ipi_buf[0],
			   VOW_IPI_BYPASS_ACK);
	if (ret == 0)
		VOWDRV_DEBUG("IPIMSG_VOW_SET_FLAG ipi send error\n\r");

	return ret;
}

void VowDrv_SetDmicLowPower(bool enable)
{
	VowDrv_SetFlag(VOW_FLAG_DMIC_LOWPOWER, enable);
}

static bool VowDrv_SetMtkifType(unsigned int type)
{
	bool ret = false;

	if (type == 0)
		VOWDRV_DEBUG("error, wrong MTKIF Type\n\r");
	vowserv.mtkif_type = type;
	ret = VowDrv_SetFlag(VOW_FLAG_MTKIF_TYPE, type);

	return ret;
}

static bool VowDrv_CheckMtkifType(unsigned int type)
{
	unsigned int mtkif_type = type & 0x0F;
	unsigned int ch_num = type >> 4;

	if (mtkif_type >= VOW_MTKIF_MAX || mtkif_type < 0) {
		VOWDRV_DEBUG("out of VOW_MTKIF_TYPE %d\n\r", mtkif_type);
		return false;
	}
	if (ch_num >= VOW_CH_MAX || ch_num < 0) {
		VOWDRV_DEBUG("out of VOW_MIC_NUM %d\n\r", ch_num);
		return false;
	}
	return true;
}

void VowDrv_SetPeriodicEnable(bool enable)
{
	VowDrv_SetFlag(VOW_FLAG_PERIODIC_ENABLE, enable);
}

static ssize_t VowDrv_GetPhase1Debug(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.force_phase_stage == FORCE_PHASE1) ? 1 : 0;

	return snprintf(buf, size, "Force Phase1 Setting = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetPhase1Debug(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	vowserv.force_phase_stage = (enable == 1) ? FORCE_PHASE1 : NO_FORCE;

	VowDrv_SetFlag(VOW_FLAG_FORCE_PHASE1_DEBUG, enable);
	return n;
}
DEVICE_ATTR(vow_SetPhase1,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetPhase1Debug,
	    VowDrv_SetPhase1Debug);

static ssize_t VowDrv_GetPhase2Debug(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.force_phase_stage == FORCE_PHASE2) ? 1 : 0;

	return snprintf(buf, size, "Force Phase2 Setting = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetPhase2Debug(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	vowserv.force_phase_stage = (enable == 1) ? FORCE_PHASE2 : NO_FORCE;

	VowDrv_SetFlag(VOW_FLAG_FORCE_PHASE2_DEBUG, enable);
	return n;
}
DEVICE_ATTR(vow_SetPhase2,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetPhase2Debug,
	    VowDrv_SetPhase2Debug);

static ssize_t VowDrv_GetDualMicDebug(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	char cstr[35];
	int size = sizeof(cstr);

	if (vowserv.scp_dual_mic_switch == VOW_ENABLE_DUAL_MIC)
		return snprintf(buf, size, "use Daul mic\n");
	else if (vowserv.scp_dual_mic_switch == VOW_ENABLE_SINGLE_MAIN_MIC)
		return snprintf(buf, size, "use Single mic: main mic\n");
	else if (vowserv.scp_dual_mic_switch == VOW_ENABLE_SINGLE_REF_MIC)
		return snprintf(buf, size, "use Single mic: ref mic\n");
	else
		return snprintf(buf, size, "set mic error\n");
}

static ssize_t VowDrv_SetDualMicDebug(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	vowserv.scp_dual_mic_switch = enable;

	VowDrv_SetFlag(VOW_FLAG_DUAL_MIC_SWITCH, enable);
	return n;
}
DEVICE_ATTR(vow_DualMicSwitch,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetDualMicDebug,
	    VowDrv_SetDualMicDebug);

static ssize_t VowDrv_GetSplitDumpFile(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.split_dumpfile_flag == true) ? 1 : 0;

	return snprintf(buf, size, "Split dump file = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetSplitDumpFile(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VOWDRV_DEBUG("%s(), enable = %d\n", __func__, enable);
	vowserv.split_dumpfile_flag = (enable == 1) ? true : false;

	return n;
}
DEVICE_ATTR(vow_SplitDumpFile,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetSplitDumpFile,
	    VowDrv_SetSplitDumpFile);


static ssize_t VowDrv_SetBargeInDebug(struct device *kobj,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VowDrv_SetBargeIn(enable, 1); /* temp fix irq */
	return n;
}
DEVICE_ATTR(vow_SetBargeIn,
	    0200, /*S_IWUSR*/
	    NULL,
	    VowDrv_SetBargeInDebug);

static bool VowDrv_SetBargeIn(unsigned int set, unsigned int irq_id)
{
	bool ret = false;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	unsigned int vow_ipi_buf[1];

	if (irq_id >= VOW_BARGEIN_IRQ_MAX_NUM || irq_id < 0) {
		VOWDRV_DEBUG("out of vow bargein irq range %d", irq_id);
		return ret;
	}
	vow_ipi_buf[0] = irq_id;

	VOWDRV_DEBUG("%s(), set = %d, irq = %d\n", __func__, set, irq_id);
	if (set == 1) {
		vow_register_feature(VOW_BARGEIN_FEATURE_ID);

		ret = vow_ipi_send(IPIMSG_VOW_SET_BARGEIN_ON,
				   1,
				   &vow_ipi_buf[0],
				   VOW_IPI_NEED_ACK);
	} else if (set == 0) {

		ret = vow_ipi_send(IPIMSG_VOW_SET_BARGEIN_OFF,
				   1,
				   &vow_ipi_buf[0],
				   VOW_IPI_NEED_ACK);
		vow_deregister_feature(VOW_BARGEIN_FEATURE_ID);
	} else {
		VOWDRV_DEBUG("Adb comment error\n\r");
	}
	if (ret == 0)
		VOWDRV_DEBUG("IPIMSG_BARGE_IN(%d) ipi send error\n", set);
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	return ret;
}

static ssize_t VowDrv_GetBypassPhase3Flag(struct device *kobj,
					  struct device_attribute *attr,
					  char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.bypass_enter_phase3 == true) ? 1 : 0;

	return snprintf(buf, size, "Enter Phase3 Setting is %s\n",
			(stat == 0x1) ? "Bypass" : "Allow");
}

static ssize_t VowDrv_SetBypassPhase3Flag(struct device *kobj,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	if (enable == 0) {
		VOWDRV_DEBUG("Allow enter phase3\n");
		vowserv.bypass_enter_phase3 = false;
	} else {
		VOWDRV_DEBUG("Bypass enter phase3\n");
		vowserv.bypass_enter_phase3 = true;
	}
	return n;
}
DEVICE_ATTR(vow_SetBypassPhase3,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetBypassPhase3Flag,
	    VowDrv_SetBypassPhase3Flag);

static ssize_t VowDrv_GetEnterPhase3Counter(struct device *kobj,
					    struct device_attribute *attr,
					    char *buf)
{
	char cstr[35];
	int size = sizeof(cstr);

	return snprintf(buf, size, "Enter Phase3 Counter is %u\n",
			vowserv.enter_phase3_cnt);
}
DEVICE_ATTR(vow_GetEnterPhase3Counter,
	    0444, /*S_IRUGO*/
	    VowDrv_GetEnterPhase3Counter,
	    NULL);

static ssize_t VowDrv_GetSWIPLog(struct device *kobj,
				 struct device_attribute *attr,
				 char *buf)
{
	unsigned int stat;
	char cstr[20];
	int size = sizeof(cstr);

	stat = (vowserv.swip_log_enable == true) ? 1 : 0;
	return snprintf(buf, size, "SWIP LOG is %s\n",
			(stat == true) ? "YES" : "NO");
}

static ssize_t VowDrv_SetSWIPLog(struct device *kobj,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	vowserv.swip_log_enable = (enable == 1) ? true : false;
	VowDrv_SetFlag(VOW_FLAG_SWIP_LOG_PRINT, enable);
	return n;
}
DEVICE_ATTR(vow_SetLibLog,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetSWIPLog,
	    VowDrv_SetSWIPLog);

static ssize_t VowDrv_SetEnableHW(struct device *kobj,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VowDrv_EnableHW(enable);
	VowDrv_ChangeStatus();
	return n;
}
DEVICE_ATTR(vow_SetEnableHW,
	    0200, /*S_IWUSR*/
	    NULL,
	    VowDrv_SetEnableHW);

static ssize_t VowDrv_GetMCPSflag(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.mcps_flag == true) ? 1 : 0;

	return snprintf(buf, size, "Enable Measure MCPS = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetMCPSflag(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VowDrv_SetFlag(VOW_FLAG_MCPS, enable);
	vowserv.mcps_flag = (enable == 1) ? true : false;
	VOWDRV_DEBUG("%s(),enable=%d\n", __func__, enable);
	return n;
}
DEVICE_ATTR(vow_SetMCPS,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetMCPSflag,
	    VowDrv_SetMCPSflag);

static int VowDrv_SetVowEINTStatus(int status)
{
	int ret = 0;
	int wakeup_event = 0;

	if ((status < NUM_OF_VOW_EINT_STATUS)
	 && (status >= VOW_EINT_DISABLE)) {
		spin_lock(&vowdrv_lock);
		if ((vowserv.eint_status != VOW_EINT_PASS)
		 && (status == VOW_EINT_PASS))
			wakeup_event = 1;
		vowserv.eint_status = status;
		spin_unlock(&vowdrv_lock);
	} else {
		VOWDRV_DEBUG("%s() error input: %x\n",
			     __func__, status);
		ret = -1;
	}
	return ret;
}

static int VowDrv_QueryVowEINTStatus(void)
{
	int ret = 0;

	spin_lock(&vowdrv_lock);
	ret = vowserv.eint_status;
	spin_unlock(&vowdrv_lock);
	VOWDRV_DEBUG("%s():%d\n", __func__, ret);
	return ret;
}

static int VowDrv_open(struct inode *inode, struct file *fp)
{
	VOWDRV_DEBUG("%s() do nothing inode:%p, file:%p\n",
		    __func__, inode, fp);
	return 0;
}

static int VowDrv_release(struct inode *inode, struct file *fp)
{
	VOWDRV_DEBUG("%s() inode:%p, file:%p\n", __func__, inode, fp);

	if (!(fp->f_mode & FMODE_WRITE || fp->f_mode & FMODE_READ))
		return -ENODEV;
	return 0;
}

static long VowDrv_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int timeout = 0;

	timeout = 0;
	while (vowserv.vow_recovering) {
		msleep(VOW_WAITCHECK_INTERVAL_MS);
		timeout++;
		VOW_ASSERT(timeout != VOW_RECOVERY_WAIT);
	}

	/* VOWDRV_DEBUG("VowDrv_ioctl cmd = %u arg = %lu\n", cmd, arg); */
	switch ((unsigned int)cmd) {
	case VOW_SET_CONTROL:
		switch (arg) {
		case VOWControlCmd_Init:
			VOWDRV_DEBUG("VOW_SET_CONTROL Init");
			vow_service_Init();
			break;
		case VOWControlCmd_Reset:
			VOWDRV_DEBUG("VOW_SET_CONTROL Reset");
			vow_service_reset();
			break;
		case VOWControlCmd_EnableDebug:
			VOWDRV_DEBUG("VOW_SET_CONTROL EnableDebug");
			mutex_lock(&voicedata_mutex);
			vowserv.kernel_voicedata_idx = 0;
			mutex_unlock(&voicedata_mutex);
			vowserv.recording_flag = true;
			vowserv.firstRead = true;
			/*VowDrv_SetFlag(VOW_FLAG_DEBUG, true);*/
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
			vow_register_feature(VOW_DUMP_FEATURE_ID);
#else
			VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
			if (vowserv.suspend_lock == 0) {
				vowserv.suspend_lock = 1;
				/* Let AP will not suspend */
				__pm_stay_awake(vow_suspend_lock);
				VOWDRV_DEBUG("==VOW DEBUG MODE START==\n");
			}
			break;
		case VOWControlCmd_DisableDebug:
			VOWDRV_DEBUG("VOW_SET_CONTROL DisableDebug");
			VowDrv_SetFlag(VOW_FLAG_DEBUG, false);
			vowserv.recording_flag = false;
			/* force stop vow_service_ReadVoiceData() 20180906 */
			vow_service_getVoiceData();
			if (vowserv.suspend_lock == 1) {
				vowserv.suspend_lock = 0;
				/* Let AP will suspend */
				__pm_relax(vow_suspend_lock);
				/* lock 1sec for screen on */
				VOWDRV_DEBUG("==VOW DEBUG MODE STOP==\n");
				__pm_wakeup_event(vow_suspend_lock,
						  jiffies_to_msecs(HZ));
			}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
			vow_deregister_feature(VOW_DUMP_FEATURE_ID);
#else
			VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
			break;
		case VOWControlCmd_EnableSeamlessRecord:
			VOWDRV_DEBUG("VOW_SET_CONTROL EnableSeamlessRecord");
			VowDrv_SetFlag(VOW_FLAG_SEAMLESS, true);
			break;
		case VOWControlCmd_EnableDump:
			vow_pcm_dump_set(true);
			break;
		case VOWControlCmd_DisableDump:
			vow_pcm_dump_set(false);
			break;
		case VOWControlCmd_Mic_Single:
			vowserv.vow_mic_number = 1;
			VOWDRV_DEBUG("VOW_SET_CONTROL Set Single Mic VOW");
			break;
		case VOWControlCmd_Mic_Dual:
			vowserv.vow_mic_number = 2;
			VOWDRV_DEBUG("VOW_SET_CONTROL Set Dual Mic VOW");
			break;
		default:
			VOWDRV_DEBUG("VOW_SET_CONTROL no such command = %lu",
				     arg);
			break;
		}
		break;
	case VOW_READ_VOICE_DATA:
		if (!vow_service_SetApAddr(arg))
			ret = -EFAULT;
		if ((vowserv.recording_flag == true)
		    && (vowserv.firstRead == true)) {
			vowserv.firstRead = false;
			VowDrv_SetFlag(VOW_FLAG_DEBUG, true);
		}
		vow_service_ReadVoiceData();
		break;
	case VOW_SET_SPEAKER_MODEL:
		VOWDRV_DEBUG("VOW_SET_SPEAKER_MODEL(%lu)", arg);
		if (!vow_service_SetSpeakerModel(arg))
			ret = -EFAULT;
		break;
	case VOW_CLR_SPEAKER_MODEL:
		VOWDRV_DEBUG("VOW_CLR_SPEAKER_MODEL(%lu)", arg);
		if (!vow_service_ReleaseSpeakerModel((int)arg))
			ret = -EFAULT;
		break;
	case VOW_SET_DSP_AEC_PARAMETER:
		VOWDRV_DEBUG("VOW_SET_DSP_AEC_PARAMETER(%lu)", arg);
		if (!vow_service_SetCustomModel(arg))
			ret = -EFAULT;
		break;
	case VOW_SET_APREG_INFO:
		VOWDRV_DEBUG("VOW_SET_APREG_INFO(%lu)", arg);
		if (!vow_service_SetVBufAddr(arg))
			ret = -EFAULT;
		break;
	case VOW_READ_VOW_DUMP_DATA:
		//VOWDRV_DEBUG("VOW_READ_VOW_DUMP_DATA(%lu)", arg);
		if (!vow_service_SetApDumpAddr(arg))
			ret = -EFAULT;
		break;
	case VOW_BARGEIN_ON:
		VOWDRV_DEBUG("VOW_BARGEIN_ON, irq: %d", (unsigned int)arg);
		if (!VowDrv_SetBargeIn(1, (unsigned int)arg))
			ret = -EFAULT;
		break;
	case VOW_BARGEIN_OFF:
		VOWDRV_DEBUG("VOW_BARGEIN_OFF, irq: %d", (unsigned int)arg);
		if (!VowDrv_SetBargeIn(0, (unsigned int)arg))
			ret = -EFAULT;
		break;
	case VOW_CHECK_STATUS:
		/* VOW disable already, then bypass second one */
		VowDrv_ChangeStatus();
		VOWDRV_DEBUG("VOW_CHECK_STATUS(%lu)", arg);
		break;
	case VOW_RECOG_ENABLE:
		pr_debug("+VOW_RECOG_ENABLE(%lu)+", arg);
		pr_debug("KERNEL_VOW_DRV_VER %s", KERNEL_VOW_DRV_VER);
		if (!VowDrv_CheckMtkifType((unsigned int)arg)) {
			pr_debug("+VOW_RECOG_ENABLE fail");
			break;
		}
		VowDrv_SetMtkifType((unsigned int)arg);
		VowDrv_EnableHW(1);
		VowDrv_ChangeStatus();
		vow_service_Enable();
		pr_debug("-VOW_RECOG_ENABLE(%lu)-", arg);
		break;
	case VOW_RECOG_DISABLE:
		pr_debug("+VOW_RECOG_DISABLE(%lu)+", arg);
		if (!VowDrv_CheckMtkifType((unsigned int)arg)) {
			pr_debug("+VOW_RECOG_DISABLE fail");
			break;
		}
		VowDrv_SetMtkifType((unsigned int)arg);
		VowDrv_EnableHW(0);
		VowDrv_ChangeStatus();
		vow_service_Disable();
		pr_debug("-VOW_RECOG_DISABLE(%lu)-", arg);
		break;
	case VOW_MODEL_START:
		vow_service_SetModelStatus(VOW_MODEL_STATUS_START, arg);
		break;
	case VOW_MODEL_STOP:
		vow_service_SetModelStatus(VOW_MODEL_STATUS_STOP, arg);
		break;
	case VOW_GET_GOOGLE_ENGINE_VER: {
		if (copy_to_user((void __user *)arg,
						 &vowserv.google_engine_version,
						 sizeof(vowserv.google_engine_version))) {
			VOWDRV_DEBUG("%s(), copy_to_user fail in VOW_GET_GOOGLE_ENGINE_VER",
						__func__);
		}
	}
		break;
	case VOW_GET_GOOGLE_ARCH:
	case VOW_GET_ALEXA_ENGINE_VER: {
		struct vow_engine_info_t engine_ver_temp;
		unsigned int length = VOW_ENGINE_INFO_LENGTH_BYTE;

		if (copy_from_user((void *)&engine_ver_temp,
				 (const void __user *)arg,
				 sizeof(struct vow_engine_info_t))) {
			VOWDRV_DEBUG("%s(), copy_from_user fail", __func__);
		}
		if ((unsigned int)cmd == VOW_GET_ALEXA_ENGINE_VER) {
			pr_debug("VOW_GET_ALEXA_ENGINE_VER = %s, %lu, %lu, %d",
					 vowserv.alexa_engine_version,
					 engine_ver_temp.data_addr,
					 engine_ver_temp.return_size_addr,
					 length);
			if (copy_to_user((void __user *)engine_ver_temp.data_addr,
					 vowserv.alexa_engine_version,
					 length)) {
				VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);
			}
		} else if ((unsigned int)cmd == VOW_GET_GOOGLE_ARCH) {
			pr_debug("VOW_GET_GOOGLE_ARCH = %s, %lu, %lu, %d",
					 vowserv.google_engine_arch,
					 engine_ver_temp.data_addr,
					 engine_ver_temp.return_size_addr,
					 length);
			if (copy_to_user((void __user *)engine_ver_temp.data_addr,
					 vowserv.google_engine_arch,
					 length)) {
				VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);
			}
		}
		if (copy_to_user((void __user *)engine_ver_temp.return_size_addr,
				 &length,
				 sizeof(unsigned int))) {
			VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);
		}
	}
		break;
#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
	case VOW_SET_PAYLOADDUMP_INFO: {
		struct vow_payloaddump_info_t payloaddump_temp;

		copy_from_user((void *)&payloaddump_temp,
				 (const void __user *)arg,
				 sizeof(struct vow_payloaddump_info_t));
		vowserv.payloaddump_user_addr =
		    payloaddump_temp.return_payloaddump_addr;
		vowserv.payloaddump_user_max_size =
		    payloaddump_temp.max_payloaddump_size;
		vowserv.payloaddump_user_return_size_addr =
		    payloaddump_temp.return_payloaddump_size_addr;
		pr_debug("-VOW_SET_PAYLOADDUMP_INFO(addr=%lu, sz=%lu)",
			 vowserv.payloaddump_user_addr,
			 vowserv.payloaddump_user_max_size);
		if (vowserv.payloaddump_kernel_ptr != NULL) {
			vfree(vowserv.payloaddump_kernel_ptr);
			vowserv.payloaddump_kernel_ptr = NULL;
		}
		if (vowserv.payloaddump_user_max_size > 0) {
			vowserv.payloaddump_kernel_ptr =
			    vmalloc(vowserv.payloaddump_user_max_size);
		} else {
			ret = -EFAULT;
		}
	}
		break;
#endif
	default:
		VOWDRV_DEBUG("vow WrongParameter(%lu)", arg);
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long VowDrv_compat_ioctl(struct file *fp,
				unsigned int cmd,
				unsigned long arg)
{
	long ret = 0;

	/* VOWDRV_DEBUG("++VowDrv_compat_ioctl cmd = %u, arg = %lu\n" */
	/*		, cmd, arg); */
	if (!fp->f_op || !fp->f_op->unlocked_ioctl) {
		(void)ret;
		return -ENOTTY;
	}
	switch (cmd) {
	case VOW_CLR_SPEAKER_MODEL:
	case VOW_SET_CONTROL:
	case VOW_CHECK_STATUS:
	case VOW_RECOG_ENABLE:
	case VOW_RECOG_DISABLE:
	case VOW_BARGEIN_ON:
	case VOW_BARGEIN_OFF:
	case VOW_GET_GOOGLE_ENGINE_VER:
		ret = fp->f_op->unlocked_ioctl(fp, cmd, arg);
		break;
	case VOW_MODEL_START:
	case VOW_MODEL_STOP: {
		struct vow_model_start_kernel_t __user *data32;
		struct vow_model_start_t __user *data;
		int err;
		compat_size_t l;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));

		err  = get_user(l, &data32->handle);
		err |= put_user(l, &data->handle);
		err |= get_user(l, &data32->confidence_level);
		err |= put_user(l, &data->confidence_level);
		err |= get_user(l, &data32->dsp_inform_addr);
		err |= put_user(l, &data->dsp_inform_addr);
		err |= get_user(l, &data32->dsp_inform_size_addr);
		err |= put_user(l, &data->dsp_inform_size_addr);

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)data);
	}
		break;
	case VOW_READ_VOICE_DATA:
	case VOW_SET_SPEAKER_MODEL:
	case VOW_READ_VOW_DUMP_DATA:
	case VOW_SET_APREG_INFO: {
		struct vow_model_info_kernel_t __user *data32;
		struct vow_model_info_t __user *data;
		int err;
		compat_size_t l;
		compat_uptr_t p;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));

		err  = get_user(l, &data32->id);
		err |= put_user(l, &data->id);
		err |= get_user(l, &data32->keyword);
		err |= put_user(l, &data->keyword);
		err |= get_user(l, &data32->addr);
		err |= put_user(l, &data->addr);
		err |= get_user(l, &data32->size);
		err |= put_user(l, &data->size);
		err |= get_user(l, &data32->return_size_addr);
		err |= put_user(l, &data->return_size_addr);
		err |= get_user(l, &data32->uuid);
		err |= put_user(l, &data->uuid);
		err |= get_user(p, (compat_uptr_t *)&data32->data);
		err |= put_user(p, (compat_uptr_t *)&data->data);

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)data);
	}
		break;
	case VOW_SET_DSP_AEC_PARAMETER:
	case VOW_GET_GOOGLE_ARCH:
	case VOW_GET_ALEXA_ENGINE_VER: {
		struct vow_engine_info_kernel_t __user *data32;
		struct vow_engine_info_t __user *data;
		int err;
		compat_size_t l;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));

		err  = get_user(l, &data32->return_size_addr);
		err |= put_user(l, &data->return_size_addr);
		err |= get_user(l, &data32->data_addr);
		err |= put_user(l, &data->data_addr);

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)data);
	}
		break;
	case VOW_SET_PAYLOADDUMP_INFO: {
		struct vow_payloaddump_info_kernel_t __user *data32;
		struct vow_payloaddump_info_t __user *data;
		int err;
		compat_size_t l;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));

		err  = get_user(l, &data32->return_payloaddump_addr);
		err |= put_user(l, &data->return_payloaddump_addr);
		err |= get_user(l, &data32->return_payloaddump_size_addr);
		err |= put_user(l, &data->return_payloaddump_size_addr);
		err |= get_user(l, &data32->max_payloaddump_size);
		err |= put_user(l, &data->max_payloaddump_size);

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)data);
	}
		break;
	default:
		break;
	}
	/* VOWDRV_DEBUG("--VowDrv_compat_ioctl\n"); */
	return ret;
}
#endif

static ssize_t VowDrv_write(struct file *fp,
			    const char __user *data,
			    size_t count,
			    loff_t *offset)
{
	/* VOWDRV_DEBUG("+VowDrv_write = %p count = %d\n",fp ,count); */
	return 0;
}

static ssize_t VowDrv_read(struct file *fp,
			   char __user *data,
			   size_t count,
			   loff_t *offset)
{
	unsigned int read_count = 0;
	unsigned int time_diff_scp_ipi = 0;
	unsigned int time_diff_ipi_read = 0;
	unsigned long long vow_read_cycle = 0;
	int ret = 0;
	unsigned int ret_data = 0;
	int slot = 0;
	bool dsp_inform_tx_flag = false;

	VOWDRV_DEBUG("+%s()+\n", __func__);
	VowDrv_SetVowEINTStatus(VOW_EINT_RETRY);

	if (VowDrv_Wait_Queue_flag == 0)
		ret = wait_event_interruptible(VowDrv_Wait_Queue,
					       VowDrv_Wait_Queue_flag);
	if (VowDrv_Wait_Queue_flag == 1) {
		VowDrv_Wait_Queue_flag = 0;
		if (VowDrv_GetHWStatus() == VOW_PWR_OFF) {
			VOWDRV_DEBUG("vow Enter_phase3_cnt = %d\n",
				      vowserv.enter_phase3_cnt);
			vowserv.scp_command_flag = false;
			VowDrv_SetVowEINTStatus(VOW_EINT_DISABLE);
		} else {
			if (vowserv.scp_command_flag) {
				VowDrv_SetVowEINTStatus(VOW_EINT_PASS);
				vow_read_cycle = get_cycles();
				time_diff_scp_ipi =
				    (unsigned int)CYCLE_TO_NS *
				    (unsigned int)(vowserv.ap_received_ipi_cycle
				    - vowserv.scp_recognize_ok_cycle);
				time_diff_ipi_read =
				    (unsigned int)CYCLE_TO_NS *
				    (unsigned int)(vow_read_cycle
				    - vowserv.ap_received_ipi_cycle);
				VOWDRV_DEBUG("vow Wakeup by SCP\n");
				VOWDRV_DEBUG("SCP->IPI:%d(ns),IPI->AP:%d(ns)\n",
					     time_diff_scp_ipi,
					     time_diff_ipi_read);
				if (vowserv.suspend_lock == 0) {
					/* lock 1sec for screen on */
					__pm_wakeup_event(vow_suspend_lock,
							  jiffies_to_msecs(HZ));
				}
				vowserv.scp_command_flag = false;
				if (vowserv.extradata_bytelen > 0)
					dsp_inform_tx_flag = true;
			} else {
				VOWDRV_DEBUG("vow Wakeup by other(%d,%d)\n",
					     VowDrv_Wait_Queue_flag,
					     VowDrv_GetHWStatus());
			}
		}
	}
	slot = vow_service_SearchSpeakerModelWithKeyword(
		  vowserv.scp_command_keywordid);
	if (slot < 0) {
		/* there is no pair id */
		VOWDRV_DEBUG("%s(), search ID fail, not keyword event, exit\n",
			     __func__);
		vowserv.scp_command_id =  0;
	} else {
		vowserv.scp_command_id = vowserv.vow_speaker_model[slot].id;
	}
	vowserv.vow_eint_data_struct.id = vowserv.scp_command_id;
	vowserv.vow_eint_data_struct.eint_status = VowDrv_QueryVowEINTStatus();
	vowserv.vow_eint_data_struct.data[0] = (char)vowserv.confidence_level;
	vowserv.confidence_level = 0;
	read_count = copy_to_user((void __user *)data,
				  &vowserv.vow_eint_data_struct,
				  sizeof(struct vow_eint_data_struct_t));
	if (dsp_inform_tx_flag) {
		/* int i; */

		dsp_inform_tx_flag = false;

		if (vowserv.extradata_mem_ptr == NULL)
			goto exit;
		if (vowserv.extradata_ptr == NULL)
			goto exit;
		if (vowserv.vow_speaker_model[slot].rx_inform_size_addr == 0)
			goto exit;
		if (vowserv.vow_speaker_model[slot].rx_inform_addr == 0)
			goto exit;
		VOWDRV_DEBUG("%s(), copy to user, extra data len=%d\n",
		     __func__, vowserv.extradata_bytelen);
#if 0
		for (i = 0; i < vowserv.extradata_bytelen; i++) {
			VOWDRV_DEBUG("data[%d] = %x\n",
				i, *(vowserv.extradata_ptr + i));
		}
#endif
#ifdef CONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
		vow_service_ReadPayloadDumpData(vowserv.payloaddump_length);
#endif
		/* copy extra data from DRAM */
		mutex_lock(&vow_extradata_mutex);
		memcpy(vowserv.extradata_mem_ptr,
		       vowserv.extradata_ptr,
		       vowserv.extradata_bytelen);
		mutex_unlock(&vow_extradata_mutex);
		/* copy extra data into user space */
		ret_data = copy_to_user(
		    (void __user *)
		    vowserv.vow_speaker_model[slot].rx_inform_size_addr,
		    &vowserv.extradata_bytelen,
		    sizeof(unsigned int));
		if (ret_data != 0) {
			/* fail, print the fail size */
			VOWDRV_DEBUG("[vow dsp inform1]CopytoUser fail sz:%d\n",
				     ret_data);
		}
		mutex_lock(&vow_extradata_mutex);
		ret_data = copy_to_user(
		    (void __user *)
		    vowserv.vow_speaker_model[slot].rx_inform_addr,
		    vowserv.extradata_mem_ptr,
		    vowserv.extradata_bytelen);
		mutex_unlock(&vow_extradata_mutex);
		if (ret_data != 0) {
			/* fail, print the fail size */
			VOWDRV_DEBUG("[vow dsp inform2]CopytoUser fail sz:%d\n",
				     ret_data);
		}
	}

exit:
	VOWDRV_DEBUG("+%s()-, recog id: %d, confidence_lv=%d\n",
		     __func__,
		     vowserv.scp_command_id,
		     vowserv.confidence_level);
	return read_count;
}

static int VowDrv_flush(struct file *flip, fl_owner_t id)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	return 0;
}

static int VowDrv_fasync(int fd, struct file *flip, int mode)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	/*return fasync_helper(fd, flip, mode, &VowDrv_fasync);*/
	return 0;
}

static int VowDrv_remap_mmap(struct file *flip, struct vm_area_struct *vma)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	return -1;
}

int VowDrv_setup_smartdev_eint(struct platform_device *pdev)
{
	int ret;
	unsigned int ints[2] = {0, 0};

	/* gpio setting */
	vowserv.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(vowserv.pinctrl)) {
		ret = PTR_ERR(vowserv.pinctrl);
		VOWDRV_DEBUG("Cannot find Vow pinctrl!\n");
		return ret;
	}
	vowserv.pins_eint_on = pinctrl_lookup_state(vowserv.pinctrl,
						    "vow_smartdev_eint_on");
	if (IS_ERR(vowserv.pins_eint_on)) {
		ret = PTR_ERR(vowserv.pins_eint_on);
		VOWDRV_DEBUG("Cannot find vow pinctrl eint_on!\n");
		return ret;
	}

	vowserv.pins_eint_off = pinctrl_lookup_state(vowserv.pinctrl,
						     "vow_smartdev_eint_off");
	if (IS_ERR(vowserv.pins_eint_off)) {
		ret = PTR_ERR(vowserv.pins_eint_off);
		VOWDRV_DEBUG("Cannot find vow pinctrl eint_off!\n");
		return ret;
	}
	/* eint setting */
	vowserv.node = pdev->dev.of_node;
	if (vowserv.node) {
		ret = of_property_read_u32_array(vowserv.node,
					   "debounce",
					   ints,
					   ARRAY_SIZE(ints));
		if (ret != 0) {
			VOWDRV_DEBUG("%s(), no debounce node, ret=%d\n",
				      __func__, ret);
			return ret;
		}

		VOWDRV_DEBUG("VOW EINT ID: %x, %x\n", ints[0], ints[1]);
	} else {
		/* no node here */
		VOWDRV_DEBUG("%s(), there is no this node\n", __func__);
	}
	return 0;
}

bool vow_service_GetScpRecoverStatus(void)
{
	return vowserv.scp_recovering;
}

bool vow_service_GetVowRecoverStatus(void)
{
	return vowserv.vow_recovering;
}

/*****************************************************************************
 * SCP Recovery Register
 *****************************************************************************/
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static int vow_scp_recover_event(struct notifier_block *this,
				 unsigned long event,
				 void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY: {
		int I;
		bool ret = false;
		unsigned int vow_ipi_buf[2];

		vowserv.vow_recovering = true;
		vowserv.scp_recovering = false;
		VOWDRV_DEBUG("%s(), SCP_EVENT_READY\n", __func__);
		if (!vow_check_scp_status()) {
			VOWDRV_DEBUG("SCP is Off, don't recover VOW\n");
			return NOTIFY_DONE;
		}
		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover1\n");
			break;
		}
		vow_service_Init();
		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover2\n");
			break;
		}
		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			if (!vow_service_SendSpeakerModel(I, VOW_SET_MODEL))
				VOWDRV_DEBUG("fail: SendSpeakerModel\n");
		}
		// send AEC custom model
		vow_ipi_buf[0] = vowserv.custom_model_addr;
		vow_ipi_buf[1] = vowserv.custom_model_size;
		ret = vow_ipi_send(IPIMSG_VOW_SET_CUSTOM_MODEL,
				   2, &vow_ipi_buf[0], VOW_IPI_BYPASS_ACK);
		if (!ret)
			VOWDRV_DEBUG("fail: vow_service_SetCustomModel\n");

		/* if vow is not enable, then return */
		if (VowDrv_GetHWStatus() != VOW_PWR_ON) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow not enable\n");
			break;
		}
		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover3\n");
			break;
		}
		/* pcm dump recover */
		VOWDRV_DEBUG("recording_flag = %d, dump_pcm_flag = %d\n",
				vowserv.recording_flag, vowserv.dump_pcm_flag);
		if (vowserv.recording_flag == true)
			VowDrv_SetFlag(VOW_FLAG_DEBUG, true);

		if (vowserv.dump_pcm_flag == true)
			vow_pcm_dump_notify(true);

		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover4\n");
			break;
		}

		if (!VowDrv_SetMtkifType(vowserv.mtkif_type))
			VOWDRV_DEBUG("fail: vow_SetMtkifType\n");

		if (!vow_service_Enable())
			VOWDRV_DEBUG("fail: vow_service_Enable\n");

		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			if (vowserv.vow_speaker_model[I].enabled) {
				vow_service_SendModelStatus(
					I, VOW_MODEL_STATUS_START);
				VOWDRV_DEBUG("send Model start, slot%d\n", I);
			}
		}
		vowserv.vow_recovering = false;
		break;
	}
	case SCP_EVENT_STOP:
		vowserv.scp_recovering = true;
		VOWDRV_DEBUG("%s(), SCP_EVENT_STOP\n", __func__);
		/* Check if VOW is running phase2.5, then stop this */
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block vow_scp_recover_notifier = {
	.notifier_call = vow_scp_recover_event,
};
#endif  /* #ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT */

/*****************************************************************************
 * VOW platform driver Registration
 *****************************************************************************/
static int VowDrv_probe(struct platform_device *dev)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	VowDrv_setup_smartdev_eint(dev);
	return 0;
}

static int VowDrv_remove(struct platform_device *dev)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	return 0;
}

static void VowDrv_shutdown(struct platform_device *dev)
{
	VOWDRV_DEBUG("%s()\n", __func__);
}

static int VowDrv_suspend(struct platform_device *dev, pm_message_t state)
{
	/* only one suspend mode */
	VOWDRV_DEBUG("%s()\n", __func__);
	return 0;
}

static int VowDrv_resume(struct platform_device *dev) /* wake up */
{
	VOWDRV_DEBUG("%s()\n", __func__);
	return 0;
}

static const struct file_operations VOW_fops = {
	.owner   = THIS_MODULE,
	.open    = VowDrv_open,
	.release = VowDrv_release,
	.unlocked_ioctl   = VowDrv_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = VowDrv_compat_ioctl,
#endif
	.write   = VowDrv_write,
	.read    = VowDrv_read,
	.flush   = VowDrv_flush,
	.fasync  = VowDrv_fasync,
	.mmap    = VowDrv_remap_mmap
};

static struct miscdevice VowDrv_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = VOW_DEVNAME,
	.fops = &VOW_fops,
};

const struct dev_pm_ops VowDrv_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = NULL,
};

#ifdef CONFIG_OF
static const struct of_device_id vow_of_match[] = {
	{.compatible = "mediatek,vow"},
	{},
};
#endif

static struct platform_driver VowDrv_driver = {
	.probe    = VowDrv_probe,
	.remove   = VowDrv_remove,
	.shutdown = VowDrv_shutdown,
	.suspend  = VowDrv_suspend,
	.resume   = VowDrv_resume,
	.driver   = {
#ifdef CONFIG_PM
	.pm       = &VowDrv_pm_ops,
#endif
	.name     = "VOW_driver_device",
#ifdef CONFIG_OF
	.of_match_table = vow_of_match,
#endif
	},
};

static int __init VowDrv_mod_init(void)
{
	int ret = 0;

	VOWDRV_DEBUG("+%s()\n", __func__);

	/* Register platform DRIVER */
	ret = platform_driver_register(&VowDrv_driver);
	if (ret != 0) {
		VOWDRV_DEBUG("VowDrv Fail:%d - Register DRIVER\n", ret);
		return ret;
	}

	/* register MISC device */
	ret = misc_register(&VowDrv_misc_device);
	if (ret != 0) {
		VOWDRV_DEBUG("VowDrv_probe misc_register Fail:%d\n", ret);
		return ret;
	}

	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetPhase1);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetPhase2);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetBypassPhase3);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_GetEnterPhase3Counter);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetLibLog);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetEnableHW);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetBargeIn);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_DualMicSwitch);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SplitDumpFile);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetMCPS);
	if (unlikely(ret != 0))
		return ret;
	/* ipi register */
	vow_ipi_register(vow_ipi_rx_internal, vow_ipi_rceive_ack);

	vow_service_Init();
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	scp_A_register_notify(&vow_scp_recover_notifier);
#endif
	vow_suspend_lock = wakeup_source_register(NULL, "vow wakelock");
	vow_ipi_suspend_lock = wakeup_source_register(NULL, "vow ipi wakelock");

	if (!vow_suspend_lock)
		pr_debug("vow wakeup source init failed.\n");

	if (!vow_ipi_suspend_lock)
		pr_debug("vow ipi wakeup source init failed.\n");
	VOWDRV_DEBUG("-%s(): Init Audio WakeLock\n", __func__);

	return 0;
}

static void __exit VowDrv_mod_exit(void)
{
	VOWDRV_DEBUG("+%s()\n", __func__);
	wakeup_source_unregister(vow_suspend_lock);
	wakeup_source_unregister(vow_ipi_suspend_lock);
	vow_pcm_dump_deinit();
	VOWDRV_DEBUG("-%s()\n", __func__);
}
late_initcall(VowDrv_mod_init);
module_exit(VowDrv_mod_exit);


/*****************************************************************************
 * License
 *****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek VOW Driver");
MODULE_AUTHOR("Charlie Lu<charlie.lu@mediatek.com>");
