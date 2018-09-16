/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/msm_audio_calibration.h>
#include <linux/atomic.h>
#include <linux/msm_audio_ion.h>
#include <linux/qdsp6v2/rtac.h>
#include <linux/compat.h>
#include <sound/q6asm-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/q6adm-v2.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6common.h>
#include "q6voice.h"
#include "msm-pcm-routing-v2.h"
#include <sound/adsp_err.h>


/* Max size of payload (buf size - apr header) */
#define MAX_PAYLOAD_SIZE		4076
#define RTAC_MAX_ACTIVE_VOICE_COMBOS	2
#define RTAC_MAX_ACTIVE_POPP		8
#define RTAC_BUF_SIZE			163840

#define TIMEOUT_MS	1000

struct rtac_cal_block_data	rtac_cal[MAX_RTAC_BLOCKS] = {
/* ADM_RTAC_CAL */
	{{RTAC_BUF_SIZE, 0, 0, 0}, {0, 0, 0} },
/* ASM_RTAC_CAL */
	{{RTAC_BUF_SIZE, 0, 0, 0}, {0, 0, 0} },
/* VOICE_RTAC_CAL */
	{{RTAC_BUF_SIZE, 0, 0, 0}, {0, 0, 0} },
/* AFE_RTAC_CAL */
	{{RTAC_BUF_SIZE, 0, 0, 0}, {0, 0, 0} }
};

struct rtac_common_data {
	atomic_t			usage_count;
	atomic_t			apr_err_code;
	struct mutex			rtac_fops_mutex;
};

static struct rtac_common_data		rtac_common;

/* APR data */
struct rtac_apr_data {
	void			*apr_handle;
	atomic_t		cmd_state;
	wait_queue_head_t	cmd_wait;
};

static struct rtac_apr_data rtac_adm_apr_data;
static struct rtac_apr_data rtac_asm_apr_data[ASM_ACTIVE_STREAMS_ALLOWED + 1];
static struct rtac_apr_data	rtac_afe_apr_data;
static struct rtac_apr_data	rtac_voice_apr_data[RTAC_VOICE_MODES];

/* ADM info & APR */
static struct rtac_adm		rtac_adm_data;
static u32			*rtac_adm_buffer;


/* ASM APR */
static u32			*rtac_asm_buffer;

static u32			*rtac_afe_buffer;

/* Voice info & APR */
struct rtac_voice_data_t {
	uint32_t	tx_topology_id;
	uint32_t	rx_topology_id;
	uint32_t	tx_afe_topology;
	uint32_t	rx_afe_topology;
	uint32_t	tx_afe_port;
	uint32_t	rx_afe_port;
	uint16_t	cvs_handle;
	uint16_t	cvp_handle;
	uint32_t	tx_acdb_id;
	uint32_t	rx_acdb_id;
};

struct rtac_voice {
	uint32_t			num_of_voice_combos;
	struct rtac_voice_data_t	voice[RTAC_MAX_ACTIVE_VOICE_COMBOS];
};

struct rtac_afe_user_data {
	uint32_t	buf_size;
	uint32_t	cmd_size;
	uint32_t	port_id;
	union {
		struct afe_rtac_user_data_set_v2 v2_set;
		struct afe_rtac_user_data_set_v3 v3_set;
		struct afe_rtac_user_data_get_v2 v2_get;
		struct afe_rtac_user_data_get_v3 v3_get;
	};
}  __packed;

static struct rtac_voice	rtac_voice_data;
static u32			*rtac_voice_buffer;
static u32			voice_session_id[RTAC_MAX_ACTIVE_VOICE_COMBOS];


struct mutex			rtac_adm_mutex;
struct mutex			rtac_adm_apr_mutex;
struct mutex			rtac_asm_apr_mutex;
struct mutex			rtac_voice_mutex;
struct mutex			rtac_voice_apr_mutex;
struct mutex			rtac_afe_apr_mutex;

int rtac_clear_mapping(uint32_t cal_type)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	if (cal_type >= MAX_RTAC_BLOCKS) {
		pr_debug("%s: invalid cal type %d\n", __func__, cal_type);
		result = -EINVAL;
		goto done;
	}

	rtac_cal[cal_type].map_data.map_handle = 0;
done:
	return result;
}

int rtac_allocate_cal_buffer(uint32_t cal_type)
{
	int result = 0;
	size_t len;
	pr_debug("%s\n", __func__);

	if (cal_type >= MAX_RTAC_BLOCKS) {
		pr_err("%s: cal_type %d is invalid!\n",
		       __func__, cal_type);
		result =  -EINVAL;
		goto done;
	}

	if (rtac_cal[cal_type].cal_data.paddr != 0) {
		pr_err("%s: memory already allocated! cal_type %d, paddr 0x%pK\n",
		       __func__, cal_type, &rtac_cal[cal_type].cal_data.paddr);
		result = -EPERM;
		goto done;
	}

	result = msm_audio_ion_alloc("rtac_client",
		&rtac_cal[cal_type].map_data.ion_client,
		&rtac_cal[cal_type].map_data.ion_handle,
		rtac_cal[cal_type].map_data.map_size,
		&rtac_cal[cal_type].cal_data.paddr,
		&len,
		&rtac_cal[cal_type].cal_data.kvaddr);
	if (result < 0) {
		pr_err("%s: ION create client for RTAC failed\n",
		       __func__);
		goto done;
	}

	pr_debug("%s: cal_type %d, paddr 0x%pK, kvaddr 0x%pK, map_size 0x%x\n",
		__func__, cal_type,
		&rtac_cal[cal_type].cal_data.paddr,
		rtac_cal[cal_type].cal_data.kvaddr,
		rtac_cal[cal_type].map_data.map_size);
done:
	return result;
}

int rtac_free_cal_buffer(uint32_t cal_type)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	if (cal_type >= MAX_RTAC_BLOCKS) {
		pr_err("%s: cal_type %d is invalid!\n",
		       __func__, cal_type);
		result =  -EINVAL;
		goto done;
	}

	if (rtac_cal[cal_type].map_data.ion_client == NULL) {
		pr_debug("%s: cal_type %d not allocated!\n",
		       __func__, cal_type);
		goto done;
	}

	result = msm_audio_ion_free(rtac_cal[cal_type].map_data.ion_client,
				rtac_cal[cal_type].map_data.ion_handle);
	if (result < 0) {
		pr_err("%s: ION free for RTAC failed! cal_type %d, paddr 0x%pK\n",
		       __func__, cal_type, &rtac_cal[cal_type].cal_data.paddr);
		goto done;
	}

	rtac_cal[cal_type].map_data.map_handle = 0;
	rtac_cal[cal_type].map_data.ion_client = NULL;
	rtac_cal[cal_type].map_data.ion_handle = NULL;
	rtac_cal[cal_type].cal_data.size = 0;
	rtac_cal[cal_type].cal_data.kvaddr = 0;
	rtac_cal[cal_type].cal_data.paddr = 0;
done:
	return result;
}

int rtac_map_cal_buffer(uint32_t cal_type)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	if (cal_type >= MAX_RTAC_BLOCKS) {
		pr_err("%s: cal_type %d is invalid!\n",
		       __func__, cal_type);
		result =  -EINVAL;
		goto done;
	}

	if (rtac_cal[cal_type].map_data.map_handle != 0) {
		pr_err("%s: already mapped cal_type %d\n",
			__func__, cal_type);
		result =  -EPERM;
		goto done;
	}

	if (rtac_cal[cal_type].cal_data.paddr == 0) {
		pr_err("%s: physical address is NULL cal_type %d\n",
			__func__, cal_type);
		result =  -EPERM;
		goto done;
	}

	switch (cal_type) {
	case ADM_RTAC_CAL:
		result = adm_map_rtac_block(&rtac_cal[cal_type]);
		break;
	case ASM_RTAC_CAL:
		result = q6asm_map_rtac_block(&rtac_cal[cal_type]);
		break;
	case VOICE_RTAC_CAL:
		result = voc_map_rtac_block(&rtac_cal[cal_type]);
		break;
	case AFE_RTAC_CAL:
		result = afe_map_rtac_block(&rtac_cal[cal_type]);
		break;
	}
	if (result < 0) {
		pr_err("%s: map RTAC failed! cal_type %d\n",
		       __func__, cal_type);
		goto done;
	}
done:
	return result;
}

int rtac_unmap_cal_buffer(uint32_t cal_type)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	if (cal_type >= MAX_RTAC_BLOCKS) {
		pr_err("%s: cal_type %d is invalid!\n",
		       __func__, cal_type);
		result =  -EINVAL;
		goto done;
	}

	if (rtac_cal[cal_type].map_data.map_handle == 0) {
		pr_debug("%s: nothing to unmap cal_type %d\n",
			__func__, cal_type);
		goto done;
	}

	switch (cal_type) {
	case ADM_RTAC_CAL:
		result = adm_unmap_rtac_block(
			&rtac_cal[cal_type].map_data.map_handle);
		break;
	case ASM_RTAC_CAL:
		result = q6asm_unmap_rtac_block(
			&rtac_cal[cal_type].map_data.map_handle);
		break;
	case VOICE_RTAC_CAL:
		result = voc_unmap_rtac_block(
			&rtac_cal[cal_type].map_data.map_handle);
		break;
	case AFE_RTAC_CAL:
		result = afe_unmap_rtac_block(
			&rtac_cal[cal_type].map_data.map_handle);
		break;
	}
	if (result < 0) {
		pr_err("%s: unmap RTAC failed! cal_type %d\n",
		       __func__, cal_type);
		goto done;
	}
done:
	return result;
}

static int rtac_open(struct inode *inode, struct file *f)
{
	int	result = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_common.rtac_fops_mutex);
	atomic_inc(&rtac_common.usage_count);
	mutex_unlock(&rtac_common.rtac_fops_mutex);
	return result;
}

static int rtac_release(struct inode *inode, struct file *f)
{
	int	result = 0;
	int	result2 = 0;
	int	i;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_common.rtac_fops_mutex);
	atomic_dec(&rtac_common.usage_count);
	pr_debug("%s: ref count %d!\n", __func__,
		atomic_read(&rtac_common.usage_count));

	if (atomic_read(&rtac_common.usage_count) > 0) {
		mutex_unlock(&rtac_common.rtac_fops_mutex);
		goto done;
	}

	for (i = 0; i < MAX_RTAC_BLOCKS; i++) {
		result2 = rtac_unmap_cal_buffer(i);
		if (result2 < 0) {
			pr_err("%s: unmap buffer failed! error %d!\n",
				__func__, result2);
			result = result2;
		}

		result2 = rtac_free_cal_buffer(i);
		if (result2 < 0) {
			pr_err("%s: free buffer failed! error %d!\n",
				__func__, result2);
			result = result2;
		}
	}
	mutex_unlock(&rtac_common.rtac_fops_mutex);
done:
	return result;
}


/* ADM Info */
void add_popp(u32 dev_idx, u32 port_id, u32 popp_id)
{
	u32 i = 0;

	for (; i < rtac_adm_data.device[dev_idx].num_of_popp; i++)
		if (rtac_adm_data.device[dev_idx].popp[i].popp == popp_id)
			goto done;

	if (rtac_adm_data.device[dev_idx].num_of_popp ==
			RTAC_MAX_ACTIVE_POPP) {
		pr_err("%s, Max POPP!\n", __func__);
		goto done;
	}
	rtac_adm_data.device[dev_idx].popp[
		rtac_adm_data.device[dev_idx].num_of_popp].popp = popp_id;
	rtac_adm_data.device[dev_idx].popp[
		rtac_adm_data.device[dev_idx].num_of_popp].popp_topology =
		q6asm_get_asm_topology(popp_id);
	rtac_adm_data.device[dev_idx].popp[
		rtac_adm_data.device[dev_idx].num_of_popp++].app_type =
		q6asm_get_asm_app_type(popp_id);

	pr_debug("%s: popp_id = %d, popp topology = 0x%x, popp app type = 0x%x\n",
		__func__,
		rtac_adm_data.device[dev_idx].popp[
			rtac_adm_data.device[dev_idx].num_of_popp - 1].popp,
		rtac_adm_data.device[dev_idx].popp[
		rtac_adm_data.device[dev_idx].num_of_popp - 1].popp_topology,
		rtac_adm_data.device[dev_idx].popp[
		rtac_adm_data.device[dev_idx].num_of_popp - 1].app_type);
done:
	return;
}

void rtac_update_afe_topology(u32 port_id)
{
	u32 i = 0;

	mutex_lock(&rtac_adm_mutex);
	for (i = 0; i < rtac_adm_data.num_of_dev; i++) {
		if (rtac_adm_data.device[i].afe_port == port_id) {
			rtac_adm_data.device[i].afe_topology =
						afe_get_topology(port_id);
			pr_debug("%s: port_id = 0x%x topology_id = 0x%x copp_id = %d\n",
				 __func__, port_id,
				 rtac_adm_data.device[i].afe_topology,
				 rtac_adm_data.device[i].copp);
		}
	}
	mutex_unlock(&rtac_adm_mutex);
}

void rtac_add_adm_device(u32 port_id, u32 copp_id, u32 path_id, u32 popp_id,
			 u32 app_type, u32 acdb_id)
{
	u32 i = 0;
	pr_debug("%s: num rtac devices %d port_id = %d, copp_id = %d\n",
		__func__, rtac_adm_data.num_of_dev, port_id, copp_id);

	mutex_lock(&rtac_adm_mutex);
	if (rtac_adm_data.num_of_dev == RTAC_MAX_ACTIVE_DEVICES) {
		pr_err("%s, Can't add anymore RTAC devices!\n", __func__);
		goto done;
	}

	/* Check if device already added */
	if (rtac_adm_data.num_of_dev != 0) {
		for (; i < rtac_adm_data.num_of_dev; i++) {
			if (rtac_adm_data.device[i].afe_port == port_id &&
			    rtac_adm_data.device[i].copp == copp_id) {
				add_popp(i, port_id, popp_id);
				goto done;
			}
			if (rtac_adm_data.device[i].num_of_popp ==
						RTAC_MAX_ACTIVE_POPP) {
				pr_err("%s, Max POPP!\n", __func__);
				goto done;
			}
		}
	}

	/* Add device */
	rtac_adm_data.num_of_dev++;

	rtac_adm_data.device[i].topology_id =
		adm_get_topology_for_port_from_copp_id(port_id, copp_id);
	rtac_adm_data.device[i].afe_topology =
		afe_get_topology(port_id);
	rtac_adm_data.device[i].afe_port = port_id;
	rtac_adm_data.device[i].copp = copp_id;
	rtac_adm_data.device[i].app_type = app_type;
	rtac_adm_data.device[i].acdb_dev_id = acdb_id;
	rtac_adm_data.device[i].popp[
		rtac_adm_data.device[i].num_of_popp].popp = popp_id;
	rtac_adm_data.device[i].popp[
		rtac_adm_data.device[i].num_of_popp].popp_topology =
		q6asm_get_asm_topology(popp_id);
	rtac_adm_data.device[i].popp[
		rtac_adm_data.device[i].num_of_popp++].app_type =
		q6asm_get_asm_app_type(popp_id);

	pr_debug("%s: topology = 0x%x, afe_topology = 0x%x, port_id = %d, copp_id = %d, app id = 0x%x, acdb id = %d, popp_id = %d, popp topology = 0x%x, popp app type = 0x%x\n",
		__func__,
		rtac_adm_data.device[i].topology_id,
		rtac_adm_data.device[i].afe_topology,
		rtac_adm_data.device[i].afe_port,
		rtac_adm_data.device[i].copp,
		rtac_adm_data.device[i].app_type,
		rtac_adm_data.device[i].acdb_dev_id,
		rtac_adm_data.device[i].popp[
			rtac_adm_data.device[i].num_of_popp - 1].popp,
		rtac_adm_data.device[i].popp[
		rtac_adm_data.device[i].num_of_popp - 1].popp_topology,
		rtac_adm_data.device[i].popp[
		rtac_adm_data.device[i].num_of_popp - 1].app_type);
done:
	mutex_unlock(&rtac_adm_mutex);
	return;
}

static void shift_adm_devices(u32 dev_idx)
{
	for (; dev_idx < rtac_adm_data.num_of_dev; dev_idx++) {
		memcpy(&rtac_adm_data.device[dev_idx],
			&rtac_adm_data.device[dev_idx + 1],
			sizeof(rtac_adm_data.device[dev_idx]));
		memset(&rtac_adm_data.device[dev_idx + 1], 0,
			   sizeof(rtac_adm_data.device[dev_idx]));
	}
}

static void shift_popp(u32 copp_idx, u32 popp_idx)
{
	for (; popp_idx < rtac_adm_data.device[copp_idx].num_of_popp;
							popp_idx++) {
		memcpy(&rtac_adm_data.device[copp_idx].popp[popp_idx].popp,
			&rtac_adm_data.device[copp_idx].popp[popp_idx + 1].
			popp, sizeof(uint32_t));
		memcpy(&rtac_adm_data.device[copp_idx].popp[popp_idx].
			popp_topology,
			&rtac_adm_data.device[copp_idx].popp[popp_idx + 1].
			popp_topology,
			sizeof(uint32_t));
		memset(&rtac_adm_data.device[copp_idx].popp[popp_idx + 1].
			popp, 0, sizeof(uint32_t));
		memset(&rtac_adm_data.device[copp_idx].popp[popp_idx + 1].
			popp_topology, 0, sizeof(uint32_t));
	}
}

void rtac_remove_adm_device(u32 port_id, u32 copp_id)
{
	s32 i;
	pr_debug("%s: num rtac devices %d port_id = %d, copp_id = %d\n",
		__func__, rtac_adm_data.num_of_dev, port_id, copp_id);

	mutex_lock(&rtac_adm_mutex);
	/* look for device */
	for (i = 0; i < rtac_adm_data.num_of_dev; i++) {
		if (rtac_adm_data.device[i].afe_port == port_id &&
		    rtac_adm_data.device[i].copp == copp_id) {
			memset(&rtac_adm_data.device[i], 0,
				   sizeof(rtac_adm_data.device[i]));
			rtac_adm_data.num_of_dev--;

			if (rtac_adm_data.num_of_dev >= 1) {
				shift_adm_devices(i);
				break;
			}
		}
	}

	mutex_unlock(&rtac_adm_mutex);
	return;
}

void rtac_remove_popp_from_adm_devices(u32 popp_id)
{
	s32 i, j;
	pr_debug("%s: popp_id = %d\n", __func__, popp_id);

	mutex_lock(&rtac_adm_mutex);
	for (i = 0; i < rtac_adm_data.num_of_dev; i++) {
		for (j = 0; j < rtac_adm_data.device[i].num_of_popp; j++) {
			if (rtac_adm_data.device[i].popp[j].popp ==
								popp_id) {
				rtac_adm_data.device[i].popp[j].popp = 0;
				rtac_adm_data.device[i].popp[j].
					popp_topology = 0;
				rtac_adm_data.device[i].num_of_popp--;
				shift_popp(i, j);
			}
		}
	}
	mutex_unlock(&rtac_adm_mutex);
}


/* Voice Info */
static void set_rtac_voice_data(int idx, u32 cvs_handle, u32 cvp_handle,
					u32 rx_afe_port, u32 tx_afe_port,
					u32 rx_acdb_id, u32 tx_acdb_id,
					u32 session_id)
{
	rtac_voice_data.voice[idx].tx_topology_id =
		voice_get_topology(CVP_VOC_TX_TOPOLOGY_CAL);
	rtac_voice_data.voice[idx].rx_topology_id =
		voice_get_topology(CVP_VOC_RX_TOPOLOGY_CAL);
	rtac_voice_data.voice[idx].tx_afe_topology =
		afe_get_topology(tx_afe_port);
	rtac_voice_data.voice[idx].rx_afe_topology =
		afe_get_topology(rx_afe_port);
	rtac_voice_data.voice[idx].tx_afe_port = tx_afe_port;
	rtac_voice_data.voice[idx].rx_afe_port = rx_afe_port;
	rtac_voice_data.voice[idx].tx_acdb_id = tx_acdb_id;
	rtac_voice_data.voice[idx].rx_acdb_id = rx_acdb_id;
	rtac_voice_data.voice[idx].cvs_handle = cvs_handle;
	rtac_voice_data.voice[idx].cvp_handle = cvp_handle;
	pr_debug("%s\n%s: %x\n%s: %d %s: %d\n%s: %d %s: %d\n %s: %d\n %s: %d\n%s: %d %s: %d\n%s",
		 "<---- Voice Data Info ---->", "Session id", session_id,
		 "cvs_handle", cvs_handle, "cvp_handle", cvp_handle,
		 "rx_afe_topology", rtac_voice_data.voice[idx].rx_afe_topology,
		 "tx_afe_topology", rtac_voice_data.voice[idx].tx_afe_topology,
		 "rx_afe_port", rx_afe_port, "tx_afe_port", tx_afe_port,
		 "rx_acdb_id", rx_acdb_id, "tx_acdb_id", tx_acdb_id,
		 "<-----------End----------->");

	/* Store session ID for voice RTAC */
	voice_session_id[idx] = session_id;
}

void rtac_add_voice(u32 cvs_handle, u32 cvp_handle, u32 rx_afe_port,
			u32 tx_afe_port, u32 rx_acdb_id, u32 tx_acdb_id,
			u32 session_id)
{
	u32 i = 0;
	pr_debug("%s\n", __func__);
	mutex_lock(&rtac_voice_mutex);

	if (rtac_voice_data.num_of_voice_combos ==
			RTAC_MAX_ACTIVE_VOICE_COMBOS) {
		pr_err("%s, Can't add anymore RTAC devices!\n", __func__);
		goto done;
	}

	/* Check if device already added */
	if (rtac_voice_data.num_of_voice_combos != 0) {
		for (; i < rtac_voice_data.num_of_voice_combos; i++) {
			if (rtac_voice_data.voice[i].cvs_handle ==
							cvs_handle) {
				set_rtac_voice_data(i, cvs_handle, cvp_handle,
					rx_afe_port, tx_afe_port, rx_acdb_id,
					tx_acdb_id, session_id);
				goto done;
			}
		}
	}

	/* Add device */
	rtac_voice_data.num_of_voice_combos++;
	set_rtac_voice_data(i, cvs_handle, cvp_handle,
				rx_afe_port, tx_afe_port,
				rx_acdb_id, tx_acdb_id,
				session_id);
done:
	mutex_unlock(&rtac_voice_mutex);
	return;
}

static void shift_voice_devices(u32 idx)
{
	for (; idx < rtac_voice_data.num_of_voice_combos - 1; idx++) {
		memcpy(&rtac_voice_data.voice[idx],
			&rtac_voice_data.voice[idx + 1],
			sizeof(rtac_voice_data.voice[idx]));
		voice_session_id[idx] = voice_session_id[idx + 1];
	}
}

void rtac_remove_voice(u32 cvs_handle)
{
	u32 i = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_voice_mutex);
	/* look for device */
	for (i = 0; i < rtac_voice_data.num_of_voice_combos; i++) {
		if (rtac_voice_data.voice[i].cvs_handle == cvs_handle) {
			shift_voice_devices(i);
			rtac_voice_data.num_of_voice_combos--;
			memset(&rtac_voice_data.voice[
				rtac_voice_data.num_of_voice_combos], 0,
				sizeof(rtac_voice_data.voice
				[rtac_voice_data.num_of_voice_combos]));
			voice_session_id[rtac_voice_data.num_of_voice_combos]
				= 0;
			break;
		}
	}
	mutex_unlock(&rtac_voice_mutex);
	return;
}

static u32 get_voice_session_id_cvs(u32 cvs_handle)
{
	u32 i;

	for (i = 0; i < rtac_voice_data.num_of_voice_combos; i++) {
		if (rtac_voice_data.voice[i].cvs_handle == cvs_handle)
			return voice_session_id[i];
	}

	pr_err("%s: No voice index for CVS handle %d found returning 0\n",
	       __func__, cvs_handle);
	return 0;
}

static u32 get_voice_session_id_cvp(u32 cvp_handle)
{
	u32 i;

	for (i = 0; i < rtac_voice_data.num_of_voice_combos; i++) {
		if (rtac_voice_data.voice[i].cvp_handle == cvp_handle)
			return voice_session_id[i];
	}

	pr_err("%s: No voice index for CVP handle %d found returning 0\n",
	       __func__, cvp_handle);
	return 0;
}

static int get_voice_index(u32 mode, u32 handle)
{
	if (mode == RTAC_CVP)
		return voice_get_idx_for_session(
			get_voice_session_id_cvp(handle));
	if (mode == RTAC_CVS)
		return voice_get_idx_for_session(
			get_voice_session_id_cvs(handle));

	pr_err("%s: Invalid mode %d, returning 0\n",
	       __func__, mode);
	return 0;
}


/* ADM APR */
void rtac_set_adm_handle(void *handle)
{
	pr_debug("%s: handle = %pK\n", __func__, handle);

	mutex_lock(&rtac_adm_apr_mutex);
	rtac_adm_apr_data.apr_handle = handle;
	mutex_unlock(&rtac_adm_apr_mutex);
}

bool rtac_make_adm_callback(uint32_t *payload, u32 payload_size)
{
	pr_debug("%s:cmd_state = %d\n", __func__,
			atomic_read(&rtac_adm_apr_data.cmd_state));
	if (atomic_read(&rtac_adm_apr_data.cmd_state) != 1)
		return false;

	pr_debug("%s\n", __func__);
	if (payload_size == sizeof(uint32_t))
		atomic_set(&rtac_common.apr_err_code, payload[0]);
	else if (payload_size == (2*sizeof(uint32_t)))
		atomic_set(&rtac_common.apr_err_code, payload[1]);

	atomic_set(&rtac_adm_apr_data.cmd_state, 0);
	wake_up(&rtac_adm_apr_data.cmd_wait);
	return true;
}

int send_adm_apr(void *buf, u32 opcode)
{
	s32	result;
	u32	user_buf_size = 0;
	u32	bytes_returned = 0;
	u32	copp_id;
	u32	payload_size;
	u32	data_size = 0;
	int	copp_idx;
	int	port_idx;
	struct apr_hdr	adm_params;
	pr_debug("%s\n", __func__);

	if (rtac_cal[ADM_RTAC_CAL].map_data.ion_handle == NULL) {
		result = rtac_allocate_cal_buffer(ADM_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: allocate buffer failed!",
				__func__);
			goto done;
		}
	}

	if (rtac_cal[ADM_RTAC_CAL].map_data.map_handle == 0) {
		result = rtac_map_cal_buffer(ADM_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: map buffer failed!",
				__func__);
			goto done;
		}
	}

	if (copy_from_user(&user_buf_size, (void *)buf,
						sizeof(user_buf_size))) {
		pr_err("%s: Copy from user failed! buf = 0x%pK\n",
		       __func__, buf);
		goto done;
	}
	if (user_buf_size <= 0) {
		pr_err("%s: Invalid buffer size = %d\n",
			__func__, user_buf_size);
		goto done;
	}

	if (copy_from_user(&payload_size, buf + sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy payload size from user buffer\n",
			__func__);
		goto done;
	}

	if (copy_from_user(&copp_id, buf + 2 * sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy port id from user buffer\n",
			__func__);
		goto done;
	}

	if (adm_get_indexes_from_copp_id(copp_id, &copp_idx, &port_idx) != 0) {
		pr_err("%s: Copp Id-%d is not active\n", __func__, copp_id);
		goto done;
	}

	mutex_lock(&rtac_adm_apr_mutex);
	if (rtac_adm_apr_data.apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		result = -EINVAL;
		goto err;
	}

	switch (opcode) {
	case ADM_CMD_SET_PP_PARAMS_V5:
	case ADM_CMD_SET_PP_PARAMS_V6:
		/* set payload size to in-band payload */
		/* set data size to actual out of band payload size */
		data_size = payload_size - 4 * sizeof(u32);
		if (data_size > rtac_cal[ADM_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid data size = %d\n",
				__func__, data_size);
			result = -EINVAL;
			goto err;
		}
		payload_size = 4 * sizeof(u32);

		/* Copy buffer to out-of-band payload */
		if (copy_from_user((void *)
				rtac_cal[ADM_RTAC_CAL].cal_data.kvaddr,
				buf + 7 * sizeof(u32), data_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}

		/* set payload size in packet */
		rtac_adm_buffer[8] = data_size;
		break;
	case ADM_CMD_GET_PP_PARAMS_V5:
	case ADM_CMD_GET_PP_PARAMS_V6:
		if (payload_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n",
				__func__, payload_size);
			result = -EINVAL;
			goto err;
		}

		/* Copy buffer to in-band payload */
		if (copy_from_user(rtac_adm_buffer +
				sizeof(adm_params)/sizeof(u32),
				buf + 3 * sizeof(u32), payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}
		break;
	default:
		pr_err("%s: Invalid opcode %d\n", __func__, opcode);
		result = -EINVAL;
		goto err;
	}

	/* Pack header */
	adm_params.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_params.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		payload_size);
	adm_params.src_svc = APR_SVC_ADM;
	adm_params.src_domain = APR_DOMAIN_APPS;
	adm_params.src_port = copp_id;
	adm_params.dest_svc = APR_SVC_ADM;
	adm_params.dest_domain = APR_DOMAIN_ADSP;
	adm_params.dest_port = copp_id;
	adm_params.token = port_idx << 16 | copp_idx;
	adm_params.opcode = opcode;

	/* fill for out-of-band */
	rtac_adm_buffer[5] =
		lower_32_bits(rtac_cal[ADM_RTAC_CAL].cal_data.paddr);
	rtac_adm_buffer[6] =
		msm_audio_populate_upper_32_bits(
				rtac_cal[ADM_RTAC_CAL].cal_data.paddr);
	rtac_adm_buffer[7] = rtac_cal[ADM_RTAC_CAL].map_data.map_handle;

	memcpy(rtac_adm_buffer, &adm_params, sizeof(adm_params));
	atomic_set(&rtac_adm_apr_data.cmd_state, 1);

	pr_debug("%s: Sending RTAC command ioctl 0x%x, paddr 0x%pK\n",
		__func__, opcode,
		&rtac_cal[ADM_RTAC_CAL].cal_data.paddr);

	result = apr_send_pkt(rtac_adm_apr_data.apr_handle,
					(uint32_t *)rtac_adm_buffer);
	if (result < 0) {
		pr_err("%s: Set params failed copp = %d\n", __func__, copp_id);
		goto err;
	}
	/* Wait for the callback */
	result = wait_event_timeout(rtac_adm_apr_data.cmd_wait,
		(atomic_read(&rtac_adm_apr_data.cmd_state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set params timed out copp = %d\n", __func__,
			copp_id);
		goto err;
	}
	if (atomic_read(&rtac_common.apr_err_code)) {
		pr_err("%s: DSP returned error code = [%s], opcode = 0x%x\n",
			__func__, adsp_err_get_err_str(atomic_read(
			&rtac_common.apr_err_code)),
			opcode);
		result = adsp_err_get_lnx_err_code(
					atomic_read(
					&rtac_common.apr_err_code));
		goto err;
	}

	if (opcode == ADM_CMD_GET_PP_PARAMS_V5) {
		bytes_returned = ((u32 *)rtac_cal[ADM_RTAC_CAL].cal_data.
			kvaddr)[2] + 3 * sizeof(u32);
	} else if (opcode == ADM_CMD_GET_PP_PARAMS_V6) {
		bytes_returned =
			((u32 *) rtac_cal[ADM_RTAC_CAL].cal_data.kvaddr)[3] +
			4 * sizeof(u32);
	} else {
		bytes_returned = data_size;
		goto unlock;
	}

	if (bytes_returned > rtac_cal[ADM_RTAC_CAL].map_data.map_size) {
		pr_err("%s: Invalid data size = %d\n", __func__,
		       bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (bytes_returned > user_buf_size) {
		pr_err("%s: User buf not big enough, size = 0x%x, returned size = 0x%x\n",
		       __func__, user_buf_size, bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (copy_to_user((void __user *) buf,
			 rtac_cal[ADM_RTAC_CAL].cal_data.kvaddr,
			 bytes_returned)) {
		pr_err("%s: Could not copy buffer to user,size = %d\n",
		       __func__, bytes_returned);
		result = -EFAULT;
		goto err;
	}

unlock:
	mutex_unlock(&rtac_adm_apr_mutex);
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_adm_apr_mutex);
	return result;
}


/* ASM APR */
void rtac_set_asm_handle(u32 session_id, void *handle)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_asm_apr_mutex);
	rtac_asm_apr_data[session_id].apr_handle = handle;
	mutex_unlock(&rtac_asm_apr_mutex);
}

bool rtac_make_asm_callback(u32 session_id, uint32_t *payload,
	u32 payload_size)
{
	if (atomic_read(&rtac_asm_apr_data[session_id].cmd_state) != 1)
		return false;

	pr_debug("%s\n", __func__);
	if (payload_size == sizeof(uint32_t))
		atomic_set(&rtac_common.apr_err_code, payload[0]);
	else if (payload_size == (2*sizeof(uint32_t)))
		atomic_set(&rtac_common.apr_err_code, payload[1]);

	atomic_set(&rtac_asm_apr_data[session_id].cmd_state, 0);
	wake_up(&rtac_asm_apr_data[session_id].cmd_wait);
	return true;
}

int send_rtac_asm_apr(void *buf, u32 opcode)
{
	s32	result;
	u32	user_buf_size = 0;
	u32	bytes_returned = 0;
	u32	session_id = 0;
	u32	payload_size;
	u32	data_size = 0;
	struct apr_hdr		asm_params;
	pr_debug("%s\n", __func__);

	if (rtac_cal[ASM_RTAC_CAL].map_data.ion_handle == NULL) {
		result = rtac_allocate_cal_buffer(ASM_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: allocate buffer failed!",
				__func__);
			goto done;
		}
	}

	if (rtac_cal[ASM_RTAC_CAL].map_data.map_handle == 0) {
		result = rtac_map_cal_buffer(ASM_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: map buffer failed!",
				__func__);
			goto done;
		}
	}

	if (copy_from_user(&user_buf_size, (void *)buf,
						sizeof(user_buf_size))) {
		pr_err("%s: Copy from user failed! buf = 0x%pK\n",
		       __func__, buf);
		goto done;
	}
	if (user_buf_size <= 0) {
		pr_err("%s: Invalid buffer size = %d\n",
			__func__, user_buf_size);
		goto done;
	}

	if (copy_from_user(&payload_size, buf + sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy payload size from user buffer\n",
			__func__);
		goto done;
	}

	if (copy_from_user(&session_id, buf + 2 * sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy session id from user buffer\n",
			__func__);
		goto done;
	}
	if (session_id >= (ASM_ACTIVE_STREAMS_ALLOWED + 1)) {
		pr_err("%s: Invalid Session = %d\n", __func__, session_id);
		goto done;
	}

	mutex_lock(&rtac_asm_apr_mutex);
	if (rtac_asm_apr_data[session_id].apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		result = -EINVAL;
		goto err;
	}

	switch (opcode) {
	case ASM_STREAM_CMD_SET_PP_PARAMS_V2:
	case ASM_STREAM_CMD_SET_PP_PARAMS_V3:
		/* set payload size to in-band payload */
		/* set data size to actual out of band payload size */
		data_size = payload_size - 4 * sizeof(u32);
		if (data_size > rtac_cal[ASM_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid data size = %d\n",
				__func__, data_size);
			result = -EINVAL;
			goto err;
		}
		payload_size = 4 * sizeof(u32);

		/* Copy buffer to out-of-band payload */
		if (copy_from_user((void *)
				rtac_cal[ASM_RTAC_CAL].cal_data.kvaddr,
				buf + 7 * sizeof(u32), data_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}
		/* set payload size in packet */
		rtac_asm_buffer[8] = data_size;
		break;
	case ASM_STREAM_CMD_GET_PP_PARAMS_V2:
	case ASM_STREAM_CMD_GET_PP_PARAMS_V3:
		if (payload_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n",
				__func__, payload_size);
			result = -EINVAL;
			goto err;
		}

		/* Copy buffer to in-band payload */
		if (copy_from_user(rtac_asm_buffer +
				sizeof(asm_params)/sizeof(u32),
				buf + 3 * sizeof(u32), payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}

		break;
	default:
		pr_err("%s: Invalid opcode %d\n", __func__, opcode);
		result = -EINVAL;
		goto err;
	}

	/* Pack header */
	asm_params.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	asm_params.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		payload_size);
	asm_params.src_svc = q6asm_get_apr_service_id(session_id);
	asm_params.src_domain = APR_DOMAIN_APPS;
	asm_params.src_port = (session_id << 8) | 0x0001;
	asm_params.dest_svc = APR_SVC_ASM;
	asm_params.dest_domain = APR_DOMAIN_ADSP;
	asm_params.dest_port = (session_id << 8) | 0x0001;
	asm_params.token = session_id;
	asm_params.opcode = opcode;

	/* fill for out-of-band */
	rtac_asm_buffer[5] =
		lower_32_bits(rtac_cal[ASM_RTAC_CAL].cal_data.paddr);
	rtac_asm_buffer[6] =
		msm_audio_populate_upper_32_bits(
				rtac_cal[ASM_RTAC_CAL].cal_data.paddr);
	rtac_asm_buffer[7] = rtac_cal[ASM_RTAC_CAL].map_data.map_handle;

	memcpy(rtac_asm_buffer, &asm_params, sizeof(asm_params));
	atomic_set(&rtac_asm_apr_data[session_id].cmd_state, 1);

	pr_debug("%s: Sending RTAC command ioctl 0x%x, paddr 0x%pK\n",
		__func__, opcode,
		&rtac_cal[ASM_RTAC_CAL].cal_data.paddr);

	result = apr_send_pkt(rtac_asm_apr_data[session_id].apr_handle,
				(uint32_t *)rtac_asm_buffer);
	if (result < 0) {
		pr_err("%s: Set params failed session = %d\n",
			__func__, session_id);
		goto err;
	}

	/* Wait for the callback */
	result = wait_event_timeout(rtac_asm_apr_data[session_id].cmd_wait,
		(atomic_read(&rtac_asm_apr_data[session_id].cmd_state) == 0),
		5 * HZ);
	if (!result) {
		pr_err("%s: Set params timed out session = %d\n",
			__func__, session_id);
		goto err;
	}
	if (atomic_read(&rtac_common.apr_err_code)) {
		pr_err("%s: DSP returned error code = [%s], opcode = 0x%x\n",
			__func__, adsp_err_get_err_str(atomic_read(
			&rtac_common.apr_err_code)),
			opcode);
		result = adsp_err_get_lnx_err_code(
					atomic_read(
					&rtac_common.apr_err_code));
		goto err;
	}

	if (opcode == ASM_STREAM_CMD_GET_PP_PARAMS_V2) {
		bytes_returned = ((u32 *)rtac_cal[ASM_RTAC_CAL].cal_data.
			kvaddr)[2] + 3 * sizeof(u32);
	} else if (opcode == ASM_STREAM_CMD_GET_PP_PARAMS_V3) {
		bytes_returned =
			((u32 *) rtac_cal[ASM_RTAC_CAL].cal_data.kvaddr)[3] +
			4 * sizeof(u32);
	} else {
		bytes_returned = data_size;
		goto unlock;
	}

	if (bytes_returned > rtac_cal[ASM_RTAC_CAL].map_data.map_size) {
		pr_err("%s: Invalid data size = %d\n", __func__,
		       bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (bytes_returned > user_buf_size) {
		pr_err("%s: User buf not big enough, size = 0x%x, returned size = 0x%x\n",
		       __func__, user_buf_size, bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (copy_to_user((void __user *) buf,
			 rtac_cal[ASM_RTAC_CAL].cal_data.kvaddr,
			 bytes_returned)) {
		pr_err("%s: Could not copy buffer to user,size = %d\n",
		       __func__, bytes_returned);
		result = -EFAULT;
		goto err;
	}

unlock:
	mutex_unlock(&rtac_asm_apr_mutex);
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_asm_apr_mutex);
	return result;
}

/* AFE APR */
void rtac_set_afe_handle(void *handle)
{
	mutex_lock(&rtac_afe_apr_mutex);
	rtac_afe_apr_data.apr_handle = handle;
	mutex_unlock(&rtac_afe_apr_mutex);
}

bool rtac_make_afe_callback(uint32_t *payload, uint32_t payload_size)
{
	pr_debug("%s:cmd_state = %d\n", __func__,
			atomic_read(&rtac_afe_apr_data.cmd_state));
	if (atomic_read(&rtac_afe_apr_data.cmd_state) != 1)
		return false;

	if (payload_size == sizeof(uint32_t))
		atomic_set(&rtac_common.apr_err_code, payload[0]);
	else if (payload_size == (2*sizeof(uint32_t)))
		atomic_set(&rtac_common.apr_err_code, payload[1]);

	atomic_set(&rtac_afe_apr_data.cmd_state, 0);
	wake_up(&rtac_afe_apr_data.cmd_wait);
	return true;
}

static int fill_afe_apr_hdr(struct apr_hdr *apr_hdr, uint32_t port,
			 uint32_t opcode, uint32_t apr_msg_size)
{
	if (apr_hdr == NULL) {
		pr_err("%s: invalid APR pointer", __func__);
		return -EINVAL;
	}

	apr_hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	apr_hdr->pkt_size = apr_msg_size;
	apr_hdr->src_svc = APR_SVC_AFE;
	apr_hdr->src_domain = APR_DOMAIN_APPS;
	apr_hdr->src_port = 0;
	apr_hdr->dest_svc = APR_SVC_AFE;
	apr_hdr->dest_domain = APR_DOMAIN_ADSP;
	apr_hdr->dest_port = 0;
	apr_hdr->token = port;
	apr_hdr->opcode = opcode;

	return 0;

}
static int send_rtac_afe_apr(void __user *buf, uint32_t opcode)
{
	int32_t result;
	uint32_t bytes_returned = 0;
	uint32_t payload_size = 0;
	uint32_t port_index = 0;
	uint32_t *afe_cmd = NULL;
	uint32_t apr_msg_size = 0;
	struct rtac_afe_user_data user_afe_buf;
	struct mem_mapping_hdr *mem_hdr = NULL;
	struct param_hdr_v1 *get_resp_v2;
	struct param_hdr_v3 *get_resp_v3;

	pr_debug("%s\n", __func__);

	if (rtac_cal[AFE_RTAC_CAL].map_data.ion_handle == NULL) {
		result = rtac_allocate_cal_buffer(AFE_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: allocate buffer failed! ret = %d\n",
				__func__, result);
			goto done;
		}
	}

	if (rtac_cal[AFE_RTAC_CAL].map_data.map_handle == 0) {
		result = rtac_map_cal_buffer(AFE_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: map buffer failed! ret = %d\n",
				__func__, result);
			goto done;
		}
	}

	if (copy_from_user(&user_afe_buf, (void *)buf,
		sizeof(struct rtac_afe_user_data))) {
		pr_err("%s: Copy from user failed! buf = 0x%pK\n",
		       __func__, buf);
		goto done;
	}

	if (user_afe_buf.buf_size <= 0) {
		pr_err("%s: Invalid buffer size = %d\n",
			__func__, user_afe_buf.buf_size);
		goto done;
	}

	port_index = q6audio_get_port_index(user_afe_buf.port_id);
	if (port_index >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid AFE port = 0x%x\n",
		       __func__, user_afe_buf.port_id);
		goto done;
	}

	mutex_lock(&rtac_afe_apr_mutex);
	if (rtac_afe_apr_data.apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		result = -EINVAL;
		goto err;
	}

	afe_cmd =
		(u32 *) rtac_afe_buffer + sizeof(struct apr_hdr) / sizeof(u32);

	switch (opcode) {
	case AFE_PORT_CMD_SET_PARAM_V2:
		apr_msg_size = sizeof(struct afe_port_cmd_set_param_v2);
		payload_size = user_afe_buf.v2_set.payload_size;
		if (payload_size > rtac_cal[AFE_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid payload size = %d\n", __func__,
			       payload_size);
			result = -EINVAL;
			goto err;
		}

		/* Copy the command to the rtac buffer */
		memcpy(afe_cmd, &user_afe_buf.v2_set,
		       sizeof(user_afe_buf.v2_set));

		/* Copy the param data to the out-of-band location */
		if (copy_from_user(rtac_cal[AFE_RTAC_CAL].cal_data.kvaddr,
				   (void __user *) buf +
					   offsetof(struct rtac_afe_user_data,
						    v2_set.param_hdr),
				   payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}
		break;
	case AFE_PORT_CMD_SET_PARAM_V3:
		apr_msg_size = sizeof(struct afe_port_cmd_set_param_v3);
		payload_size = user_afe_buf.v3_set.payload_size;
		if (payload_size > rtac_cal[AFE_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid payload size = %d\n", __func__,
			       payload_size);
			result = -EINVAL;
			goto err;
		}

		/* Copy the command to the rtac buffer */
		memcpy(afe_cmd, &user_afe_buf.v3_set,
		       sizeof(user_afe_buf.v3_set));

		/* Copy the param data to the out-of-band location */
		if (copy_from_user(rtac_cal[AFE_RTAC_CAL].cal_data.kvaddr,
				   (void __user *) buf +
					   offsetof(struct rtac_afe_user_data,
						    v3_set.param_hdr),
				   payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}
		break;
	case AFE_PORT_CMD_GET_PARAM_V2:
		apr_msg_size = sizeof(struct afe_port_cmd_get_param_v2);

		if (user_afe_buf.cmd_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n", __func__,
			       user_afe_buf.cmd_size);
			result = -EINVAL;
			goto err;
		}

		/* Copy the command and param data in-band */
		if (copy_from_user(afe_cmd,
				   (void __user *) buf +
					   offsetof(struct rtac_afe_user_data,
						    v2_get),
				   user_afe_buf.cmd_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
			       __func__);
			result = -EFAULT;
			goto err;
		}
		break;
	case AFE_PORT_CMD_GET_PARAM_V3:
		apr_msg_size = sizeof(struct afe_port_cmd_get_param_v3);

		if (user_afe_buf.cmd_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n", __func__,
			       user_afe_buf.cmd_size);
			result = -EINVAL;
			goto err;
		}

		/* Copy the command and param data in-band */
		if (copy_from_user(afe_cmd,
				   (void __user *) buf +
					   offsetof(struct rtac_afe_user_data,
						    v3_get),
				   user_afe_buf.cmd_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}
		break;
	default:
		pr_err("%s: Invalid opcode %d\n", __func__, opcode);
		result = -EINVAL;
		goto err;
	}

	/*
	 * The memory header is in the same location in all commands. Therefore,
	 * it doesn't matter what command the buffer is cast into.
	 */
	mem_hdr = &((struct afe_port_cmd_set_param_v3 *) rtac_afe_buffer)
			   ->mem_hdr;
	mem_hdr->data_payload_addr_lsw =
		lower_32_bits(rtac_cal[AFE_RTAC_CAL].cal_data.paddr);
	mem_hdr->data_payload_addr_msw = msm_audio_populate_upper_32_bits(
		rtac_cal[AFE_RTAC_CAL].cal_data.paddr);
	mem_hdr->mem_map_handle = rtac_cal[AFE_RTAC_CAL].map_data.map_handle;

	/* Fill the APR header at the end so we have the correct message size */
	fill_afe_apr_hdr((struct apr_hdr *) rtac_afe_buffer,
			port_index, opcode, apr_msg_size);

	atomic_set(&rtac_afe_apr_data.cmd_state, 1);

	pr_debug("%s: Sending RTAC command ioctl 0x%x, paddr 0x%pK\n",
		__func__, opcode,
		&rtac_cal[AFE_RTAC_CAL].cal_data.paddr);

	result = apr_send_pkt(rtac_afe_apr_data.apr_handle,
					(uint32_t *)rtac_afe_buffer);
	if (result < 0) {
		pr_err("%s: Set params failed port = 0x%x, ret = %d\n",
			__func__, user_afe_buf.port_id, result);
		goto err;
	}
	/* Wait for the callback */
	result = wait_event_timeout(rtac_afe_apr_data.cmd_wait,
		(atomic_read(&rtac_afe_apr_data.cmd_state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set params timed out port = 0x%x, ret = %d\n",
			__func__, user_afe_buf.port_id, result);
		goto err;
	}
	if (atomic_read(&rtac_common.apr_err_code)) {
		pr_err("%s: DSP returned error code = [%s], opcode = 0x%x\n",
			__func__, adsp_err_get_err_str(atomic_read(
			&rtac_common.apr_err_code)),
			opcode);
		result = adsp_err_get_lnx_err_code(
					atomic_read(
					&rtac_common.apr_err_code));
		goto err;
	}

	if (opcode == AFE_PORT_CMD_GET_PARAM_V2) {
		get_resp_v2 = (struct param_hdr_v1 *) rtac_cal[AFE_RTAC_CAL]
				      .cal_data.kvaddr;
		bytes_returned =
			get_resp_v2->param_size + sizeof(struct param_hdr_v1);
	} else if (opcode == AFE_PORT_CMD_GET_PARAM_V3) {
		get_resp_v3 = (struct param_hdr_v3 *) rtac_cal[AFE_RTAC_CAL]
				      .cal_data.kvaddr;
		bytes_returned =
			get_resp_v3->param_size + sizeof(struct param_hdr_v3);
	} else {
		bytes_returned = payload_size;
		goto unlock;
	}

	if (bytes_returned > rtac_cal[AFE_RTAC_CAL].map_data.map_size) {
		pr_err("%s: Invalid data size = %d\n", __func__,
		       bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (bytes_returned > user_afe_buf.buf_size) {
		pr_err("%s: user size = 0x%x, returned size = 0x%x\n", __func__,
		       user_afe_buf.buf_size, bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (copy_to_user((void __user *) buf,
			 rtac_cal[AFE_RTAC_CAL].cal_data.kvaddr,
			 bytes_returned)) {
		pr_err("%s: Could not copy buffer to user,size = %d\n",
		       __func__, bytes_returned);
		result = -EFAULT;
		goto err;
	}

unlock:
	mutex_unlock(&rtac_afe_apr_mutex);
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_afe_apr_mutex);
	return result;
}

/* Voice APR */
void rtac_set_voice_handle(u32 mode, void *handle)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_voice_apr_mutex);
	rtac_voice_apr_data[mode].apr_handle = handle;
	mutex_unlock(&rtac_voice_apr_mutex);
}

bool rtac_make_voice_callback(u32 mode, uint32_t *payload, u32 payload_size)
{
	if ((atomic_read(&rtac_voice_apr_data[mode].cmd_state) != 1) ||
		(mode >= RTAC_VOICE_MODES))
		return false;

	pr_debug("%s\n", __func__);
	if (payload_size == sizeof(uint32_t))
		atomic_set(&rtac_common.apr_err_code, payload[0]);
	else if (payload_size == (2*sizeof(uint32_t)))
		atomic_set(&rtac_common.apr_err_code, payload[1]);

	atomic_set(&rtac_voice_apr_data[mode].cmd_state, 0);
	wake_up(&rtac_voice_apr_data[mode].cmd_wait);
	return true;
}

int send_voice_apr(u32 mode, void *buf, u32 opcode)
{
	s32	result;
	u32	user_buf_size = 0;
	u32	bytes_returned = 0;
	u32	payload_size;
	u32	dest_port;
	u32	data_size = 0;
	struct apr_hdr		voice_params;
	pr_debug("%s\n", __func__);

	if (rtac_cal[VOICE_RTAC_CAL].map_data.ion_handle == NULL) {
		result = rtac_allocate_cal_buffer(VOICE_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: allocate buffer failed!",
				__func__);
			goto done;
		}
	}

	if (rtac_cal[VOICE_RTAC_CAL].map_data.map_handle == 0) {
		result = rtac_map_cal_buffer(VOICE_RTAC_CAL);
		if (result < 0) {
			pr_err("%s: map buffer failed!",
				__func__);
			goto done;
		}
	}

	if (copy_from_user(&user_buf_size, (void *)buf,
						sizeof(user_buf_size))) {
		pr_err("%s: Copy from user failed! buf = 0x%pK\n",
		       __func__, buf);
		goto done;
	}
	if (user_buf_size <= 0) {
		pr_err("%s: Invalid buffer size = %d\n",
			__func__, user_buf_size);
		goto done;
	}

	if (copy_from_user(&payload_size, buf + sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy payload size from user buffer\n",
			__func__);
		goto done;
	}

	if (copy_from_user(&dest_port, buf + 2 * sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy port id from user buffer\n",
			__func__);
		goto done;
	}

	if ((mode != RTAC_CVP) && (mode != RTAC_CVS)) {
		pr_err("%s: Invalid Mode for APR, mode = %d\n",
			__func__, mode);
		goto done;
	}

	mutex_lock(&rtac_voice_apr_mutex);
	if (rtac_voice_apr_data[mode].apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		result = -EINVAL;
		goto err;
	}

	switch (opcode) {
	case VSS_ICOMMON_CMD_SET_PARAM_V2:
	case VSS_ICOMMON_CMD_SET_PARAM_V3:
		/* set payload size to in-band payload */
		/* set data size to actual out of band payload size */
		data_size = payload_size - 4 * sizeof(u32);
		if (data_size > rtac_cal[VOICE_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid data size = %d\n",
				__func__, data_size);
			result = -EINVAL;
			goto err;
		}
		payload_size = 4 * sizeof(u32);

		/* Copy buffer to out-of-band payload */
		if (copy_from_user((void *)
				rtac_cal[VOICE_RTAC_CAL].cal_data.kvaddr,
				buf + 7 * sizeof(u32), data_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}
		/* set payload size in packet */
		rtac_voice_buffer[8] = data_size;
		/* set token for set param case */
		voice_params.token = VOC_RTAC_SET_PARAM_TOKEN;
		break;
	case VSS_ICOMMON_CMD_GET_PARAM_V2:
	case VSS_ICOMMON_CMD_GET_PARAM_V3:
		if (payload_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n",
					__func__, payload_size);
			result = -EINVAL;
			goto err;
		}

		/* Copy buffer to in-band payload */
		if (copy_from_user(rtac_voice_buffer +
				sizeof(voice_params)/sizeof(u32),
				buf + 3 * sizeof(u32), payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			result = -EFAULT;
			goto err;
		}
		/* set token for get param case */
		voice_params.token = 0;
		break;
	default:
		pr_err("%s: Invalid opcode %d\n", __func__, opcode);
		result = -EINVAL;
		goto err;
	}

	/* Pack header */
	voice_params.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	voice_params.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		payload_size);
	voice_params.src_svc = 0;
	voice_params.src_domain = APR_DOMAIN_APPS;
	voice_params.src_port = get_voice_index(mode, dest_port);
	voice_params.dest_svc = 0;
	voice_params.dest_domain = APR_DOMAIN_MODEM;
	voice_params.dest_port = (u16)dest_port;
	voice_params.opcode = opcode;

	/* fill for out-of-band */
	rtac_voice_buffer[5] = rtac_cal[VOICE_RTAC_CAL].map_data.map_handle;
	rtac_voice_buffer[6] =
		lower_32_bits(rtac_cal[VOICE_RTAC_CAL].cal_data.paddr);
	rtac_voice_buffer[7] = msm_audio_populate_upper_32_bits(
		rtac_cal[VOICE_RTAC_CAL].cal_data.paddr);

	memcpy(rtac_voice_buffer, &voice_params, sizeof(voice_params));
	atomic_set(&rtac_voice_apr_data[mode].cmd_state, 1);

	pr_debug("%s: Sending RTAC command ioctl 0x%x, paddr 0x%pK\n",
		__func__, opcode,
		&rtac_cal[VOICE_RTAC_CAL].cal_data.paddr);

	result = apr_send_pkt(rtac_voice_apr_data[mode].apr_handle,
					(uint32_t *)rtac_voice_buffer);
	if (result < 0) {
		pr_err("%s: apr_send_pkt failed opcode = %x\n",
			__func__, opcode);
		goto err;
	}
	/* Wait for the callback */
	result = wait_event_timeout(rtac_voice_apr_data[mode].cmd_wait,
		(atomic_read(&rtac_voice_apr_data[mode].cmd_state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: apr_send_pkt timed out opcode = %x\n",
			__func__, opcode);
		goto err;
	}
	if (atomic_read(&rtac_common.apr_err_code)) {
		pr_err("%s: DSP returned error code = [%s], opcode = 0x%x\n",
			__func__, adsp_err_get_err_str(atomic_read(
			&rtac_common.apr_err_code)),
			opcode);
		result = adsp_err_get_lnx_err_code(
					atomic_read(
					&rtac_common.apr_err_code));
		goto err;
	}

	if (opcode == VSS_ICOMMON_CMD_GET_PARAM_V2) {
		bytes_returned = ((u32 *)rtac_cal[VOICE_RTAC_CAL].cal_data.
			kvaddr)[2] + 3 * sizeof(u32);
	} else if (opcode == VSS_ICOMMON_CMD_GET_PARAM_V3) {
		bytes_returned =
			((u32 *) rtac_cal[VOICE_RTAC_CAL].cal_data.kvaddr)[3] +
			4 * sizeof(u32);
	} else {
		bytes_returned = data_size;
		goto unlock;
	}

	if (bytes_returned > rtac_cal[VOICE_RTAC_CAL].map_data.map_size) {
		pr_err("%s: Invalid data size = %d\n", __func__,
		       bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (bytes_returned > user_buf_size) {
		pr_err("%s: User buf not big enough, size = 0x%x, returned size = 0x%x\n",
		       __func__, user_buf_size, bytes_returned);
		result = -EINVAL;
		goto err;
	}

	if (copy_to_user((void __user *) buf,
			 rtac_cal[VOICE_RTAC_CAL].cal_data.kvaddr,
			 bytes_returned)) {
		pr_err("%s: Could not copy buffer to user, size = %d\n",
		       __func__, bytes_returned);
		result = -EFAULT;
		goto err;
	}

unlock:
	mutex_unlock(&rtac_voice_apr_mutex);
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_voice_apr_mutex);
	return result;
}

void get_rtac_adm_data(struct rtac_adm *adm_data)
{
	mutex_lock(&rtac_adm_mutex);
	memcpy(adm_data, &rtac_adm_data, sizeof(struct rtac_adm));
	mutex_unlock(&rtac_adm_mutex);
}


static long rtac_ioctl_shared(struct file *f,
		unsigned int cmd, void *arg)
{
	u32 opcode;
	int result = 0;
	if (!arg) {
		pr_err("%s: No data sent to driver!\n", __func__);
		result = -EFAULT;
		goto done;
	}

	switch (cmd) {
	case AUDIO_GET_RTAC_ADM_INFO: {
		mutex_lock(&rtac_adm_mutex);
		if (copy_to_user((void *)arg, &rtac_adm_data,
						sizeof(rtac_adm_data))) {
			pr_err("%s: copy_to_user failed for AUDIO_GET_RTAC_ADM_INFO\n",
					__func__);
			mutex_unlock(&rtac_adm_mutex);
			return -EFAULT;
		} else {
			result = sizeof(rtac_adm_data);
		}
		mutex_unlock(&rtac_adm_mutex);
		break;
	}
	case AUDIO_GET_RTAC_VOICE_INFO: {
		mutex_lock(&rtac_voice_mutex);
		if (copy_to_user((void *)arg, &rtac_voice_data,
						sizeof(rtac_voice_data))) {
			pr_err("%s: copy_to_user failed for AUDIO_GET_RTAC_VOICE_INFO\n",
					__func__);
			mutex_unlock(&rtac_voice_mutex);
			return -EFAULT;
		} else {
			result = sizeof(rtac_voice_data);
		}
		mutex_unlock(&rtac_voice_mutex);
		break;
	}

	case AUDIO_GET_RTAC_ADM_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 ADM_CMD_GET_PP_PARAMS_V6 :
				 ADM_CMD_GET_PP_PARAMS_V5;
		result = send_adm_apr((void *) arg, opcode);
		break;
	case AUDIO_SET_RTAC_ADM_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 ADM_CMD_SET_PP_PARAMS_V6 :
				 ADM_CMD_SET_PP_PARAMS_V5;
		result = send_adm_apr((void *) arg, opcode);
		break;
	case AUDIO_GET_RTAC_ASM_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 ASM_STREAM_CMD_GET_PP_PARAMS_V3 :
				 ASM_STREAM_CMD_GET_PP_PARAMS_V2;
		result = send_rtac_asm_apr((void *) arg, opcode);
		break;
	case AUDIO_SET_RTAC_ASM_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 ASM_STREAM_CMD_SET_PP_PARAMS_V3 :
				 ASM_STREAM_CMD_SET_PP_PARAMS_V2;
		result = send_rtac_asm_apr((void *) arg, opcode);
		break;
	case AUDIO_GET_RTAC_CVS_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 VSS_ICOMMON_CMD_GET_PARAM_V3 :
				 VSS_ICOMMON_CMD_GET_PARAM_V2;
		result = send_voice_apr(RTAC_CVS, (void *) arg, opcode);
		break;
	case AUDIO_SET_RTAC_CVS_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 VSS_ICOMMON_CMD_SET_PARAM_V3 :
				 VSS_ICOMMON_CMD_SET_PARAM_V2;
		result = send_voice_apr(RTAC_CVS, (void *) arg, opcode);
		break;
	case AUDIO_GET_RTAC_CVP_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 VSS_ICOMMON_CMD_GET_PARAM_V3 :
				 VSS_ICOMMON_CMD_GET_PARAM_V2;
		result = send_voice_apr(RTAC_CVP, (void *) arg, opcode);
		break;
	case AUDIO_SET_RTAC_CVP_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 VSS_ICOMMON_CMD_SET_PARAM_V3 :
				 VSS_ICOMMON_CMD_SET_PARAM_V2;
		result = send_voice_apr(RTAC_CVP, (void *) arg, opcode);
		break;
	case AUDIO_GET_RTAC_AFE_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 AFE_PORT_CMD_GET_PARAM_V3 :
				 AFE_PORT_CMD_GET_PARAM_V2;
		result = send_rtac_afe_apr((void __user *) arg, opcode);
		break;
	case AUDIO_SET_RTAC_AFE_CAL:
		opcode = q6common_is_instance_id_supported() ?
				 AFE_PORT_CMD_SET_PARAM_V3 :
				 AFE_PORT_CMD_SET_PARAM_V2;
		result = send_rtac_afe_apr((void __user *) arg, opcode);
		break;
	default:
		pr_err("%s: Invalid IOCTL, command = %d!\n",
		       __func__, cmd);
		result = -EINVAL;
	}
done:
	return result;
}

static long rtac_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	int result = 0;

	mutex_lock(&rtac_common.rtac_fops_mutex);
	if (!arg) {
		pr_err("%s: No data sent to driver!\n", __func__);
		result = -EFAULT;
	} else {
		result = rtac_ioctl_shared(f, cmd, (void __user *)arg);
	}

	mutex_unlock(&rtac_common.rtac_fops_mutex);
	return result;
}

#ifdef CONFIG_COMPAT
#define AUDIO_GET_RTAC_ADM_INFO_32   _IOR(CAL_IOCTL_MAGIC, 207, compat_uptr_t)
#define AUDIO_GET_RTAC_VOICE_INFO_32 _IOR(CAL_IOCTL_MAGIC, 208, compat_uptr_t)
#define AUDIO_GET_RTAC_ADM_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 209, compat_uptr_t)
#define AUDIO_SET_RTAC_ADM_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 210, compat_uptr_t)
#define AUDIO_GET_RTAC_ASM_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 211, compat_uptr_t)
#define AUDIO_SET_RTAC_ASM_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 212, compat_uptr_t)
#define AUDIO_GET_RTAC_CVS_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 213, compat_uptr_t)
#define AUDIO_SET_RTAC_CVS_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 214, compat_uptr_t)
#define AUDIO_GET_RTAC_CVP_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 215, compat_uptr_t)
#define AUDIO_SET_RTAC_CVP_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 216, compat_uptr_t)
#define AUDIO_GET_RTAC_AFE_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 217, compat_uptr_t)
#define AUDIO_SET_RTAC_AFE_CAL_32 _IOWR(CAL_IOCTL_MAGIC, 218, compat_uptr_t)

static long rtac_compat_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	int result = 0;

	mutex_lock(&rtac_common.rtac_fops_mutex);
	if (!arg) {
		pr_err("%s: No data sent to driver!\n", __func__);
		result = -EINVAL;
		goto done;
	}

	switch (cmd) {
	case AUDIO_GET_RTAC_ADM_INFO_32:
		cmd = AUDIO_GET_RTAC_ADM_INFO;
		goto process;
	case AUDIO_GET_RTAC_VOICE_INFO_32:
		cmd = AUDIO_GET_RTAC_VOICE_INFO;
		goto process;
	case AUDIO_GET_RTAC_AFE_CAL_32:
		cmd = AUDIO_GET_RTAC_AFE_CAL;
		goto process;
	case AUDIO_SET_RTAC_AFE_CAL_32:
		cmd = AUDIO_SET_RTAC_AFE_CAL;
		goto process;
	case AUDIO_GET_RTAC_ADM_CAL_32:
		cmd = AUDIO_GET_RTAC_ADM_CAL;
		goto process;
	case AUDIO_SET_RTAC_ADM_CAL_32:
		cmd = AUDIO_SET_RTAC_ADM_CAL;
		goto process;
	case AUDIO_GET_RTAC_ASM_CAL_32:
		cmd = AUDIO_GET_RTAC_ASM_CAL;
		goto process;
	case AUDIO_SET_RTAC_ASM_CAL_32:
		cmd =  AUDIO_SET_RTAC_ASM_CAL;
		goto process;
	case AUDIO_GET_RTAC_CVS_CAL_32:
		cmd = AUDIO_GET_RTAC_CVS_CAL;
		goto process;
	case AUDIO_SET_RTAC_CVS_CAL_32:
		cmd = AUDIO_SET_RTAC_CVS_CAL;
		goto process;
	case AUDIO_GET_RTAC_CVP_CAL_32:
		cmd =  AUDIO_GET_RTAC_CVP_CAL;
		goto process;
	case AUDIO_SET_RTAC_CVP_CAL_32:
		cmd = AUDIO_SET_RTAC_CVP_CAL;
process:
		result = rtac_ioctl_shared(f, cmd, compat_ptr(arg));
		break;
	default:
		result = -EINVAL;
		pr_err("%s: Invalid IOCTL, command = %d!\n",
		       __func__, cmd);
		break;
	}
done:
	mutex_unlock(&rtac_common.rtac_fops_mutex);
	return result;
}
#else
#define rtac_compat_ioctl NULL
#endif

static const struct file_operations rtac_fops = {
	.owner = THIS_MODULE,
	.open = rtac_open,
	.release = rtac_release,
	.unlocked_ioctl = rtac_ioctl,
	.compat_ioctl = rtac_compat_ioctl,
};

struct miscdevice rtac_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_rtac",
	.fops	= &rtac_fops,
};

static int __init rtac_init(void)
{
	int i = 0;

	/* Driver */
	atomic_set(&rtac_common.usage_count, 0);
	atomic_set(&rtac_common.apr_err_code, 0);
	mutex_init(&rtac_common.rtac_fops_mutex);

	/* ADM */
	memset(&rtac_adm_data, 0, sizeof(rtac_adm_data));
	rtac_adm_apr_data.apr_handle = NULL;
	atomic_set(&rtac_adm_apr_data.cmd_state, 0);
	init_waitqueue_head(&rtac_adm_apr_data.cmd_wait);
	mutex_init(&rtac_adm_mutex);
	mutex_init(&rtac_adm_apr_mutex);

	rtac_adm_buffer = kzalloc(
		rtac_cal[ADM_RTAC_CAL].map_data.map_size, GFP_KERNEL);
	if (rtac_adm_buffer == NULL) {
		pr_err("%s: Could not allocate payload of size = %d\n",
			__func__, rtac_cal[ADM_RTAC_CAL].map_data.map_size);
		goto nomem;
	}

	/* ASM */
	for (i = 0; i < ASM_ACTIVE_STREAMS_ALLOWED+1; i++) {
		rtac_asm_apr_data[i].apr_handle = NULL;
		atomic_set(&rtac_asm_apr_data[i].cmd_state, 0);
		init_waitqueue_head(&rtac_asm_apr_data[i].cmd_wait);
	}
	mutex_init(&rtac_asm_apr_mutex);

	rtac_asm_buffer = kzalloc(
		rtac_cal[ASM_RTAC_CAL].map_data.map_size, GFP_KERNEL);
	if (rtac_asm_buffer == NULL) {
		pr_err("%s: Could not allocate payload of size = %d\n",
			__func__, rtac_cal[ASM_RTAC_CAL].map_data.map_size);
		kzfree(rtac_adm_buffer);
		goto nomem;
	}

	/* AFE */
	rtac_afe_apr_data.apr_handle = NULL;
	atomic_set(&rtac_afe_apr_data.cmd_state, 0);
	init_waitqueue_head(&rtac_afe_apr_data.cmd_wait);
	mutex_init(&rtac_afe_apr_mutex);

	rtac_afe_buffer = kzalloc(
		rtac_cal[AFE_RTAC_CAL].map_data.map_size, GFP_KERNEL);
	if (rtac_afe_buffer == NULL) {
		pr_err("%s: Could not allocate payload of size = %d\n",
			__func__, rtac_cal[AFE_RTAC_CAL].map_data.map_size);
		kzfree(rtac_adm_buffer);
		kzfree(rtac_asm_buffer);
		goto nomem;
	}

	/* Voice */
	memset(&rtac_voice_data, 0, sizeof(rtac_voice_data));
	for (i = 0; i < RTAC_VOICE_MODES; i++) {
		rtac_voice_apr_data[i].apr_handle = NULL;
		atomic_set(&rtac_voice_apr_data[i].cmd_state, 0);
		init_waitqueue_head(&rtac_voice_apr_data[i].cmd_wait);
	}
	mutex_init(&rtac_voice_mutex);
	mutex_init(&rtac_voice_apr_mutex);

	rtac_voice_buffer = kzalloc(
		rtac_cal[VOICE_RTAC_CAL].map_data.map_size, GFP_KERNEL);
	if (rtac_voice_buffer == NULL) {
		pr_err("%s: Could not allocate payload of size = %d\n",
			__func__, rtac_cal[VOICE_RTAC_CAL].map_data.map_size);
		kzfree(rtac_adm_buffer);
		kzfree(rtac_asm_buffer);
		kzfree(rtac_afe_buffer);
		goto nomem;
	}

	return misc_register(&rtac_misc);
nomem:
	return -ENOMEM;
}

module_init(rtac_init);

MODULE_DESCRIPTION("SoC QDSP6v2 Real-Time Audio Calibration driver");
MODULE_LICENSE("GPL v2");
