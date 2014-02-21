/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_acdb.h>
#include <linux/atomic.h>
#include <linux/msm_audio_ion.h>
#include <linux/qdsp6v2/rtac.h>
#include <sound/q6asm-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/apr_audio-v2.h>
#include "q6voice.h"
#include "audio_acdb.h"


#ifndef CONFIG_RTAC

void rtac_add_adm_device(u32 port_id, u32 copp_id, u32 path_id, u32 popp_id) {}
void rtac_remove_adm_device(u32 port_id, u32 copp_id) {}
void rtac_remove_popp_from_adm_devices(u32 popp_id) {}
void rtac_set_adm_handle(void *handle) {}
bool rtac_make_adm_callback(uint32_t *payload, u32 payload_size)
	{return false; }
void rtac_set_asm_handle(u32 session_id, void *handle) {}
bool rtac_make_asm_callback(u32 session_id, uint32_t *payload,
	u32 payload_size) {return false; }
void rtac_add_voice(u32 cvs_handle, u32 cvp_handle, u32 rx_afe_port,
	u32 tx_afe_port, u32 session_id) {}
void rtac_remove_voice(u32 cvs_handle) {}
void rtac_set_voice_handle(u32 mode, void *handle) {}
bool rtac_make_voice_callback(u32 mode, uint32_t *payload,
		u32 payload_size) {return false; }
int rtac_clear_mapping(uint32_t cal_type)
{return -EINVAL; }

#else

/* Max size of payload (buf size - apr header) */
#define MAX_PAYLOAD_SIZE		4076
#define RTAC_MAX_ACTIVE_DEVICES		4
#define RTAC_MAX_ACTIVE_VOICE_COMBOS	2
#define RTAC_MAX_ACTIVE_POPP		8
#define RTAC_BUF_SIZE			8192

#define TIMEOUT_MS	1000

struct rtac_cal_block_data	rtac_cal[MAX_RTAC_BLOCKS] = {
/* ADM_RTAC_CAL */
	{{RTAC_BUF_SIZE, 0, 0, 0}, {0, 0, 0} },
/* ASM_RTAC_CAL */
	{{RTAC_BUF_SIZE, 0, 0, 0}, {0, 0, 0} },
/* VOICE_RTAC_CAL */
	{{RTAC_BUF_SIZE, 0, 0, 0}, {0, 0, 0} }
};

struct rtac_common_data {
	atomic_t			usage_count;
	atomic_t			apr_err_code;
};

static struct rtac_common_data		rtac_common;

/* APR data */
struct rtac_apr_data {
	void			*apr_handle;
	atomic_t		cmd_state;
	wait_queue_head_t	cmd_wait;
};

static struct rtac_apr_data	rtac_adm_apr_data;
static struct rtac_apr_data	rtac_asm_apr_data[SESSION_MAX+1];
static struct rtac_apr_data	rtac_voice_apr_data[RTAC_VOICE_MODES];


/* ADM info & APR */
struct rtac_adm_data {
	uint32_t	topology_id;
	uint32_t	afe_port;
	uint32_t	copp;
	uint32_t	num_of_popp;
	uint32_t	popp[RTAC_MAX_ACTIVE_POPP];
};

struct rtac_adm {
	uint32_t		num_of_dev;
	struct rtac_adm_data	device[RTAC_MAX_ACTIVE_DEVICES];
};
static struct rtac_adm		rtac_adm_data;


/* ADM V2 data */
struct rtac_popp_data {
	uint32_t	popp;
	uint32_t	popp_topology;
};

struct rtac_adm_data_v2 {
	uint32_t		topology_id;
	uint32_t		afe_port;
	uint32_t		copp;
	uint32_t		num_of_popp;
	struct rtac_popp_data	popp[RTAC_MAX_ACTIVE_POPP];
};

struct rtac_adm_v2 {
	uint32_t			num_of_dev;
	struct rtac_adm_data_v2		device[RTAC_MAX_ACTIVE_DEVICES];
};

static struct rtac_adm_v2	rtac_adm_data_v2;
static u32			*rtac_adm_buffer;


/* ASM APR */
static u32			*rtac_asm_buffer;


/* Voice info & APR */
struct rtac_voice_data {
	uint32_t	tx_topology_id;
	uint32_t	rx_topology_id;
	uint32_t	tx_afe_port;
	uint32_t	rx_afe_port;
	uint16_t	cvs_handle;
	uint16_t	cvp_handle;
};

struct rtac_voice {
	uint32_t		num_of_voice_combos;
	struct rtac_voice_data	voice[RTAC_MAX_ACTIVE_VOICE_COMBOS];
};

static struct rtac_voice	rtac_voice_data;
static u32			*rtac_voice_buffer;
static u32			voice_session_id[RTAC_MAX_ACTIVE_VOICE_COMBOS];


struct mutex			rtac_adm_mutex;
struct mutex			rtac_adm_apr_mutex;
struct mutex			rtac_asm_apr_mutex;
struct mutex			rtac_voice_mutex;
struct mutex			rtac_voice_apr_mutex;

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
		pr_err("%s: memory already allocated! cal_type %d, paddr 0x%pa\n",
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

	pr_debug("%s: cal_type %d, paddr 0x%pa, kvaddr 0x%p, map_size 0x%x\n",
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
		pr_err("%s: ION free for RTAC failed! cal_type %d, paddr 0x%pa\n",
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

	atomic_inc(&rtac_common.usage_count);
	return result;
}

static int rtac_release(struct inode *inode, struct file *f)
{
	int	result = 0;
	int	result2 = 0;
	int	i;
	pr_debug("%s\n", __func__);

	atomic_dec(&rtac_common.usage_count);
	pr_debug("%s: ref count %d!\n", __func__,
		atomic_read(&rtac_common.usage_count));

	if (atomic_read(&rtac_common.usage_count) > 0)
		goto done;

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
done:
	return result;
}


/* ADM Info V2 */
static void add_popp_v2(u32 dev_idx, u32 port_id, u32 popp_id)
{
	u32 i = 0;

	for (; i < rtac_adm_data_v2.device[dev_idx].num_of_popp; i++)
		if (rtac_adm_data_v2.device[dev_idx].popp[i].popp == popp_id)
			goto done;

	if (rtac_adm_data_v2.device[dev_idx].num_of_popp ==
			RTAC_MAX_ACTIVE_POPP) {
		pr_err("%s, Max POPP!\n", __func__);
		goto done;
	}
	rtac_adm_data_v2.device[dev_idx].popp[
		rtac_adm_data_v2.device[dev_idx].num_of_popp].popp = popp_id;
	rtac_adm_data_v2.device[dev_idx].popp[
		rtac_adm_data_v2.device[dev_idx].num_of_popp++].popp_topology =
		get_asm_topology();
done:
	return;
}

static void rtac_add_adm_device_v2(u32 port_id, u32 copp_id, u32 path_id,
								u32 popp_id)
{
	u32 i = 0;
	pr_debug("%s: port_id = %d, popp_id = %d\n", __func__, port_id,
		popp_id);

	if (rtac_adm_data_v2.num_of_dev == RTAC_MAX_ACTIVE_DEVICES) {
		pr_err("%s, Can't add anymore RTAC devices!\n", __func__);
		goto done;
	}

	/* Check if device already added */
	if (rtac_adm_data_v2.num_of_dev != 0) {
		for (; i < rtac_adm_data_v2.num_of_dev; i++) {
			if (rtac_adm_data_v2.device[i].afe_port == port_id) {
				add_popp_v2(i, port_id, popp_id);
				goto done;
			}
			if (rtac_adm_data_v2.device[i].num_of_popp ==
						RTAC_MAX_ACTIVE_POPP) {
				pr_err("%s, Max POPP!\n", __func__);
				goto done;
			}
		}
	}

	/* Add device */
	rtac_adm_data_v2.num_of_dev++;

	if (path_id == ADM_PATH_PLAYBACK)
		rtac_adm_data_v2.device[i].topology_id =
						get_adm_rx_topology();
	else
		rtac_adm_data_v2.device[i].topology_id =
						get_adm_tx_topology();
	rtac_adm_data_v2.device[i].afe_port = port_id;
	rtac_adm_data_v2.device[i].copp = copp_id;
	rtac_adm_data_v2.device[i].popp[
		rtac_adm_data_v2.device[i].num_of_popp].popp = popp_id;
	rtac_adm_data_v2.device[i].popp[
		rtac_adm_data_v2.device[i].num_of_popp++].popp_topology =
		get_asm_topology();
done:
	return;
}

static void shift_adm_devices_v2(u32 dev_idx)
{
	for (; dev_idx < rtac_adm_data_v2.num_of_dev; dev_idx++) {
		memcpy(&rtac_adm_data_v2.device[dev_idx],
			&rtac_adm_data_v2.device[dev_idx + 1],
			sizeof(rtac_adm_data_v2.device[dev_idx]));
		memset(&rtac_adm_data_v2.device[dev_idx + 1], 0,
			   sizeof(rtac_adm_data_v2.device[dev_idx]));
	}
}

static void shift_popp_v2(u32 copp_idx, u32 popp_idx)
{
	for (; popp_idx < rtac_adm_data_v2.device[copp_idx].num_of_popp;
							popp_idx++) {
		memcpy(&rtac_adm_data_v2.device[copp_idx].popp[popp_idx].popp,
			&rtac_adm_data_v2.device[copp_idx].popp[popp_idx + 1].
			popp, sizeof(uint32_t));
		memcpy(&rtac_adm_data_v2.device[copp_idx].popp[popp_idx].
			popp_topology,
			&rtac_adm_data_v2.device[copp_idx].popp[popp_idx + 1].
			popp_topology,
			sizeof(uint32_t));
		memset(&rtac_adm_data_v2.device[copp_idx].popp[popp_idx + 1].
			popp, 0, sizeof(uint32_t));
		memset(&rtac_adm_data_v2.device[copp_idx].popp[popp_idx + 1].
			popp_topology, 0, sizeof(uint32_t));
	}
}

static void rtac_remove_adm_device_v2(u32 port_id)
{
	s32 i;
	pr_debug("%s: port_id = %d\n", __func__, port_id);

	/* look for device */
	for (i = 0; i < rtac_adm_data_v2.num_of_dev; i++) {
		if (rtac_adm_data_v2.device[i].afe_port == port_id) {
			memset(&rtac_adm_data_v2.device[i], 0,
				   sizeof(rtac_adm_data_v2.device[i]));
			rtac_adm_data_v2.num_of_dev--;

			if (rtac_adm_data_v2.num_of_dev >= 1) {
				shift_adm_devices_v2(i);
				break;
			}
		}
	}
	return;
}

static void rtac_remove_popp_from_adm_devices_v2(u32 popp_id)
{
	s32 i, j;
	pr_debug("%s: popp_id = %d\n", __func__, popp_id);

	for (i = 0; i < rtac_adm_data_v2.num_of_dev; i++) {
		for (j = 0; j < rtac_adm_data_v2.device[i].num_of_popp; j++) {
			if (rtac_adm_data_v2.device[i].popp[j].popp ==
								popp_id) {
				rtac_adm_data_v2.device[i].popp[j].popp = 0;
				rtac_adm_data_v2.device[i].popp[j].
					popp_topology = 0;
				rtac_adm_data_v2.device[i].num_of_popp--;
				shift_popp_v2(i, j);
			}
		}
	}
}

/* ADM Info */
void add_popp(u32 dev_idx, u32 port_id, u32 popp_id)
{
	u32 i = 0;

	for (; i < rtac_adm_data.device[dev_idx].num_of_popp; i++)
		if (rtac_adm_data.device[dev_idx].popp[i] == popp_id)
			goto done;

	if (rtac_adm_data.device[dev_idx].num_of_popp ==
			RTAC_MAX_ACTIVE_POPP) {
		pr_err("%s, Max POPP!\n", __func__);
		goto done;
	}
	rtac_adm_data.device[dev_idx].popp[
		rtac_adm_data.device[dev_idx].num_of_popp++] = popp_id;
done:
	return;
}

void rtac_add_adm_device(u32 port_id, u32 copp_id, u32 path_id, u32 popp_id)
{
	u32 i = 0;
	pr_debug("%s: port_id = %d, popp_id = %d\n", __func__, port_id,
		popp_id);

	mutex_lock(&rtac_adm_mutex);
	rtac_add_adm_device_v2(port_id, copp_id, path_id, popp_id);

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

	if (path_id == ADM_PATH_PLAYBACK)
		rtac_adm_data.device[i].topology_id =
						get_adm_rx_topology();
	else
		rtac_adm_data.device[i].topology_id =
						get_adm_tx_topology();
	rtac_adm_data.device[i].afe_port = port_id;
	rtac_adm_data.device[i].copp = copp_id;
	rtac_adm_data.device[i].popp[
		rtac_adm_data.device[i].num_of_popp++] = popp_id;
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
		memcpy(&rtac_adm_data.device[copp_idx].popp[popp_idx],
			&rtac_adm_data.device[copp_idx].popp[popp_idx + 1],
			sizeof(uint32_t));
		memset(&rtac_adm_data.device[copp_idx].popp[popp_idx + 1], 0,
			sizeof(uint32_t));
	}
}

void rtac_remove_adm_device(u32 port_id, u32 copp_id)
{
	s32 i;
	pr_debug("%s: port_id = %d\n", __func__, port_id);

	mutex_lock(&rtac_adm_mutex);
	rtac_remove_adm_device_v2(port_id);

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
	rtac_remove_popp_from_adm_devices_v2(popp_id);

	for (i = 0; i < rtac_adm_data.num_of_dev; i++) {
		for (j = 0; j < rtac_adm_data.device[i].num_of_popp; j++) {
			if (rtac_adm_data.device[i].popp[j] == popp_id) {
				rtac_adm_data.device[i].popp[j] = 0;
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
					u32 session_id)
{
	rtac_voice_data.voice[idx].tx_topology_id = get_voice_tx_topology();
	rtac_voice_data.voice[idx].rx_topology_id = get_voice_rx_topology();
	rtac_voice_data.voice[idx].tx_afe_port = tx_afe_port;
	rtac_voice_data.voice[idx].rx_afe_port = rx_afe_port;
	rtac_voice_data.voice[idx].cvs_handle = cvs_handle;
	rtac_voice_data.voice[idx].cvp_handle = cvp_handle;

	/* Store session ID for voice RTAC */
	voice_session_id[idx] = session_id;
}

void rtac_add_voice(u32 cvs_handle, u32 cvp_handle, u32 rx_afe_port,
			u32 tx_afe_port, u32 session_id)
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
					rx_afe_port, tx_afe_port,
					session_id);
				goto done;
			}
		}
	}

	/* Add device */
	rtac_voice_data.num_of_voice_combos++;
	set_rtac_voice_data(i, cvs_handle, cvp_handle,
				rx_afe_port, tx_afe_port,
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
	pr_debug("%s: handle = %d\n", __func__, (unsigned int)handle);

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

u32 send_adm_apr(void *buf, u32 opcode)
{
	s32	result;
	u32	user_buf_size = 0;
	u32	bytes_returned = 0;
	u32	port_index = 0;
	u32	copp_id;
	u32	payload_size;
	u32	data_size = 0;
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
		pr_err("%s: Copy from user failed! buf = 0x%x\n",
		       __func__, (unsigned int)buf);
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

	for (port_index = 0; port_index < AFE_MAX_PORTS; port_index++) {
		if (adm_get_copp_id(port_index) == copp_id)
			break;
		if (adm_get_lowlatency_copp_id(port_index) == copp_id)
			break;
	}
	if (port_index >= AFE_MAX_PORTS) {
		pr_err("%s: Could not find port index for copp = %d\n",
		       __func__, copp_id);
		goto done;
	}

	mutex_lock(&rtac_adm_apr_mutex);
	if (rtac_adm_apr_data.apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		goto err;
	}

	if (opcode == ADM_CMD_SET_PP_PARAMS_V5) {
		/* set payload size to in-band payload */
		/* set data size to actual out of band payload size */
		data_size = payload_size - 4 * sizeof(u32);
		if (data_size > rtac_cal[ADM_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid data size = %d\n",
				__func__, data_size);
			goto done;
		}
		payload_size = 4 * sizeof(u32);

		/* Copy buffer to out-of-band payload */
		if (copy_from_user((void *)
				rtac_cal[ADM_RTAC_CAL].cal_data.kvaddr,
				buf + 7 * sizeof(u32), data_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			goto err;
		}
		/* set payload size in packet */
		rtac_adm_buffer[8] = data_size;
	} else {
		if (payload_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n",
				__func__, payload_size);
			goto done;
		}

		/* Copy buffer to in-band payload */
		if (copy_from_user(rtac_adm_buffer +
				sizeof(adm_params)/sizeof(u32),
				buf + 3 * sizeof(u32), payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			goto err;
		}
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
	adm_params.token = copp_id;
	adm_params.opcode = opcode;

	/* fill for out-of-band */
	rtac_adm_buffer[5] = rtac_cal[ADM_RTAC_CAL].cal_data.paddr;
	rtac_adm_buffer[6] = 0;
	rtac_adm_buffer[7] = rtac_cal[ADM_RTAC_CAL].map_data.map_handle;

	memcpy(rtac_adm_buffer, &adm_params, sizeof(adm_params));
	atomic_set(&rtac_adm_apr_data.cmd_state, 1);

	pr_debug("%s: Sending RTAC command ioctl 0x%x, paddr 0x%pa\n",
		__func__, opcode,
		&rtac_cal[ADM_RTAC_CAL].cal_data.paddr);

	result = apr_send_pkt(rtac_adm_apr_data.apr_handle,
					(uint32_t *)rtac_adm_buffer);
	if (result < 0) {
		pr_err("%s: Set params failed port = %d, copp = %d\n",
			__func__, port_index, copp_id);
		goto err;
	}
	/* Wait for the callback */
	result = wait_event_timeout(rtac_adm_apr_data.cmd_wait,
		(atomic_read(&rtac_adm_apr_data.cmd_state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	mutex_unlock(&rtac_adm_apr_mutex);
	if (!result) {
		pr_err("%s: Set params timed out port = %d, copp = %d\n",
			__func__, port_index, copp_id);
		goto done;
	}
	if (atomic_read(&rtac_common.apr_err_code)) {
		pr_err("%s: DSP returned error code = %d, opcode = 0x%x\n",
			__func__, atomic_read(&rtac_common.apr_err_code),
			opcode);
		goto done;
	}

	if (opcode == ADM_CMD_GET_PP_PARAMS_V5) {
		bytes_returned = ((u32 *)rtac_cal[ADM_RTAC_CAL].cal_data.
			kvaddr)[2] + 3 * sizeof(u32);

		if (bytes_returned > user_buf_size) {
			pr_err("%s: User buf not big enough, size = 0x%x, returned size = 0x%x\n",
				__func__, user_buf_size, bytes_returned);
			goto done;
		}

		if (copy_to_user(buf, (void *)
				rtac_cal[ADM_RTAC_CAL].cal_data.kvaddr,
				bytes_returned)) {
			pr_err("%s: Could not copy buffer to user,size = %d\n",
				__func__, bytes_returned);
			goto done;
		}
	} else {
		bytes_returned = data_size;
	}

done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_adm_apr_mutex);
	return bytes_returned;
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

u32 send_rtac_asm_apr(void *buf, u32 opcode)
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
		pr_err("%s: Copy from user failed! buf = 0x%x\n",
		       __func__, (unsigned int)buf);
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
	if (session_id >= (SESSION_MAX + 1)) {
		pr_err("%s: Invalid Session = %d\n", __func__, session_id);
		goto done;
	}

	mutex_lock(&rtac_asm_apr_mutex);
	if (rtac_asm_apr_data[session_id].apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		goto err;
	}

	if (opcode == ASM_STREAM_CMD_SET_PP_PARAMS_V2) {
		/* set payload size to in-band payload */
		/* set data size to actual out of band payload size */
		data_size = payload_size - 4 * sizeof(u32);
		if (data_size > rtac_cal[ASM_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid data size = %d\n",
				__func__, data_size);
			goto done;
		}
		payload_size = 4 * sizeof(u32);

		/* Copy buffer to out-of-band payload */
		if (copy_from_user((void *)
				rtac_cal[ASM_RTAC_CAL].cal_data.kvaddr,
				buf + 7 * sizeof(u32), data_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			goto err;
		}
		/* set payload size in packet */
		rtac_asm_buffer[8] = data_size;

	} else {
		if (payload_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n",
				__func__, payload_size);
			goto done;
		}

		/* Copy buffer to in-band payload */
		if (copy_from_user(rtac_asm_buffer +
				sizeof(asm_params)/sizeof(u32),
				buf + 3 * sizeof(u32), payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			goto err;
		}
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
	rtac_asm_buffer[5] = rtac_cal[ASM_RTAC_CAL].cal_data.paddr;
	rtac_asm_buffer[6] = 0;
	rtac_asm_buffer[7] = rtac_cal[ASM_RTAC_CAL].map_data.map_handle;

	memcpy(rtac_asm_buffer, &asm_params, sizeof(asm_params));
	atomic_set(&rtac_asm_apr_data[session_id].cmd_state, 1);

	pr_debug("%s: Sending RTAC command ioctl 0x%x, paddr 0x%pa\n",
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
	mutex_unlock(&rtac_asm_apr_mutex);
	if (!result) {
		pr_err("%s: Set params timed out session = %d\n",
			__func__, session_id);
		goto done;
	}
	if (atomic_read(&rtac_common.apr_err_code)) {
		pr_err("%s: DSP returned error code = %d, opcode = 0x%x\n",
			__func__, atomic_read(&rtac_common.apr_err_code),
			opcode);
		goto done;
	}

	if (opcode == ASM_STREAM_CMD_GET_PP_PARAMS_V2) {
		bytes_returned = ((u32 *)rtac_cal[ASM_RTAC_CAL].cal_data.
			kvaddr)[2] + 3 * sizeof(u32);

		if (bytes_returned > user_buf_size) {
			pr_err("%s: User buf not big enough, size = 0x%x, returned size = 0x%x\n",
				__func__, user_buf_size, bytes_returned);
			goto done;
		}

		if (copy_to_user(buf, (void *)
				rtac_cal[ASM_RTAC_CAL].cal_data.kvaddr,
				bytes_returned)) {
			pr_err("%s: Could not copy buffer to user,size = %d\n",
				 __func__, bytes_returned);
			goto done;
		}
	} else {
		bytes_returned = data_size;
	}
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_asm_apr_mutex);
	return bytes_returned;
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

u32 send_voice_apr(u32 mode, void *buf, u32 opcode)
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
		pr_err("%s: Copy from user failed! buf = 0x%x\n",
		       __func__, (unsigned int)buf);
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
		goto err;
	}

	if (opcode == VOICE_CMD_SET_PARAM) {
		/* set payload size to in-band payload */
		/* set data size to actual out of band payload size */
		data_size = payload_size - 4 * sizeof(u32);
		if (data_size > rtac_cal[VOICE_RTAC_CAL].map_data.map_size) {
			pr_err("%s: Invalid data size = %d\n",
				__func__, data_size);
			goto done;
		}
		payload_size = 4 * sizeof(u32);

		/* Copy buffer to out-of-band payload */
		if (copy_from_user((void *)
				rtac_cal[VOICE_RTAC_CAL].cal_data.kvaddr,
				buf + 7 * sizeof(u32), data_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			goto err;
		}
		/* set payload size in packet */
		rtac_voice_buffer[8] = data_size;
	} else {
		if (payload_size > MAX_PAYLOAD_SIZE) {
			pr_err("%s: Invalid payload size = %d\n",
					__func__, payload_size);
			goto done;
		}

		/* Copy buffer to in-band payload */
		if (copy_from_user(rtac_voice_buffer +
				sizeof(voice_params)/sizeof(u32),
				buf + 3 * sizeof(u32), payload_size)) {
			pr_err("%s: Could not copy payload from user buffer\n",
				__func__);
			goto err;
		}
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
	voice_params.token = 0;
	voice_params.opcode = opcode;

	/* fill for out-of-band */
	rtac_voice_buffer[5] = rtac_cal[VOICE_RTAC_CAL].map_data.map_handle;
	rtac_voice_buffer[6] = rtac_cal[VOICE_RTAC_CAL].cal_data.paddr;
	rtac_voice_buffer[7] = 0;

	memcpy(rtac_voice_buffer, &voice_params, sizeof(voice_params));
	atomic_set(&rtac_voice_apr_data[mode].cmd_state, 1);

	pr_debug("%s: Sending RTAC command ioctl 0x%x, paddr 0x%pa\n",
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
	mutex_unlock(&rtac_voice_apr_mutex);
	if (!result) {
		pr_err("%s: apr_send_pkt timed out opcode = %x\n",
			__func__, opcode);
		goto done;
	}
	if (atomic_read(&rtac_common.apr_err_code)) {
		pr_err("%s: DSP returned error code = %d, opcode = 0x%x\n",
			__func__, atomic_read(&rtac_common.apr_err_code),
			opcode);
		goto done;
	}

	if (opcode == VOICE_CMD_GET_PARAM) {
		bytes_returned = ((u32 *)rtac_cal[VOICE_RTAC_CAL].cal_data.
			kvaddr)[2] + 3 * sizeof(u32);

		if (bytes_returned > user_buf_size) {
			pr_err("%s: User buf not big enough, size = 0x%x, returned size = 0x%x\n",
				__func__, user_buf_size, bytes_returned);
			goto done;
		}

		if (copy_to_user(buf, (void *)
				rtac_cal[VOICE_RTAC_CAL].cal_data.kvaddr,
				bytes_returned)) {
			pr_err("%s: Could not copy buffer to user, size = %d\n",
				 __func__, bytes_returned);
			goto done;
		}
	} else {
		bytes_returned = data_size;
	}
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_voice_apr_mutex);
	return bytes_returned;
}



static long rtac_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	s32 result = 0;
	pr_debug("%s\n", __func__);

	if (arg == 0) {
		pr_err("%s: No data sent to driver!\n", __func__);
		result = -EFAULT;
		goto done;
	}

	switch (cmd) {
	case AUDIO_GET_RTAC_ADM_INFO:
		if (copy_to_user((void *)arg, &rtac_adm_data,
						sizeof(rtac_adm_data)))
			pr_err("%s: Could not copy to userspace!\n", __func__);
		else
			result = sizeof(rtac_adm_data);
		break;
	case AUDIO_GET_RTAC_ADM_INFO_V2:
		if (copy_to_user((void *)arg, &rtac_adm_data_v2,
						sizeof(rtac_adm_data_v2)))
			pr_err("%s: Could not copy to userspace!\n", __func__);
		else
			result = sizeof(rtac_adm_data_v2);
		break;
	case AUDIO_GET_RTAC_VOICE_INFO:
		if (copy_to_user((void *)arg, &rtac_voice_data,
						sizeof(rtac_voice_data)))
			pr_err("%s: Could not copy to userspace!\n", __func__);
		else
			result = sizeof(rtac_voice_data);
		break;
	case AUDIO_GET_RTAC_ADM_CAL:
		result = send_adm_apr((void *)arg, ADM_CMD_GET_PP_PARAMS_V5);
		break;
	case AUDIO_SET_RTAC_ADM_CAL:
		result = send_adm_apr((void *)arg, ADM_CMD_SET_PP_PARAMS_V5);
		break;
	case AUDIO_GET_RTAC_ASM_CAL:
		result = send_rtac_asm_apr((void *)arg,
			ASM_STREAM_CMD_GET_PP_PARAMS_V2);
		break;
	case AUDIO_SET_RTAC_ASM_CAL:
		result = send_rtac_asm_apr((void *)arg,
			ASM_STREAM_CMD_SET_PP_PARAMS_V2);
		break;
	case AUDIO_GET_RTAC_CVS_CAL:
		result = send_voice_apr(RTAC_CVS, (void *)arg,
			VOICE_CMD_GET_PARAM);
		break;
	case AUDIO_SET_RTAC_CVS_CAL:
		result = send_voice_apr(RTAC_CVS, (void *)arg,
			VOICE_CMD_SET_PARAM);
		break;
	case AUDIO_GET_RTAC_CVP_CAL:
		result = send_voice_apr(RTAC_CVP, (void *)arg,
			VOICE_CMD_GET_PARAM);
		break;
	case AUDIO_SET_RTAC_CVP_CAL:
		result = send_voice_apr(RTAC_CVP, (void *)arg,
			VOICE_CMD_SET_PARAM);
		break;
	default:
		pr_err("%s: Invalid IOCTL, command = %d!\n",
		       __func__, cmd);
	}
done:
	return result;
}


static const struct file_operations rtac_fops = {
	.owner = THIS_MODULE,
	.open = rtac_open,
	.release = rtac_release,
	.unlocked_ioctl = rtac_ioctl,
};

struct miscdevice rtac_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_rtac",
	.fops	= &rtac_fops,
};

static int __init rtac_init(void)
{
	int i = 0;
	pr_debug("%s\n", __func__);

	/* Driver */
	atomic_set(&rtac_common.usage_count, 0);
	atomic_set(&rtac_common.apr_err_code, 0);

	/* ADM */
	memset(&rtac_adm_data, 0, sizeof(rtac_adm_data));
	memset(&rtac_adm_data_v2, 0, sizeof(rtac_adm_data_v2));
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
	for (i = 0; i < SESSION_MAX+1; i++) {
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
		goto nomem;
	}

	return misc_register(&rtac_misc);
nomem:
	return -ENOMEM;
}

module_init(rtac_init);

MODULE_DESCRIPTION("SoC QDSP6v2 Real-Time Audio Calibration driver");
MODULE_LICENSE("GPL v2");

#endif
