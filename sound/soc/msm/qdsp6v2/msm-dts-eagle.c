/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/msm_ion.h>
#include <linux/mm.h>
#include <linux/msm_audio_ion.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/q6adm-v2.h>
#include <sound/q6asm-v2.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/audio_effects.h>
#include <sound/hwdep.h>

#include "msm-pcm-routing-v2.h"
#include "msm-dts-eagle.h"
#include "q6core.h"

#define ION_MEM_SIZE  131072
#define DEPC_MAX_SIZE 524288

enum {
	AUDIO_DEVICE_OUT_EARPIECE = 0,
	AUDIO_DEVICE_OUT_SPEAKER,
	AUDIO_DEVICE_OUT_WIRED_HEADSET,
	AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
	AUDIO_DEVICE_OUT_BLUETOOTH_SCO,
	AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET,
	AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT,
	AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
	AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES,
	AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER,
	AUDIO_DEVICE_OUT_AUX_DIGITAL,
	AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET,
	AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET,
	AUDIO_DEVICE_OUT_USB_ACCESSORY,
	AUDIO_DEVICE_OUT_USB_DEVICE,
	AUDIO_DEVICE_OUT_REMOTE_SUBMIX,
	AUDIO_DEVICE_OUT_ANC_HEADSET,
	AUDIO_DEVICE_OUT_ANC_HEADPHONE,
	AUDIO_DEVICE_OUT_PROXY,
	AUDIO_DEVICE_OUT_FM,
	AUDIO_DEVICE_OUT_FM_TX,

	AUDIO_DEVICE_OUT_COUNT
};

#define AUDIO_DEVICE_COMBO 0x400000 /* bit 23 */

enum { /* cache block */
	CB_0 = 0,
	CB_1,
	CB_2,
	CB_3,
	CB_4,
	CB_5,
	CB_6,
	CB_7,

	CB_COUNT
};

enum { /* cache block description */
	CBD_DEV_MASK = 0,
	CBD_OFFSG,
	CBD_CMD0,
	CBD_SZ0,
	CBD_OFFS1,
	CBD_CMD1,
	CBD_SZ1,
	CBD_OFFS2,
	CBD_CMD2,
	CBD_SZ2,
	CBD_OFFS3,
	CBD_CMD3,
	CBD_SZ3,

	CBD_COUNT,
};

static __s32 fx_logN(__s32 x)
{
	__s32 t, y = 0xa65af;
	if (x < 0x00008000) {
		x <<= 16; y -= 0xb1721; }
	if (x < 0x00800000) {
		x <<= 8; y -= 0x58b91; }
	if (x < 0x08000000) {
		x <<= 4; y -= 0x2c5c8; }
	if (x < 0x20000000) {
		x <<= 2; y -= 0x162e4; }
	if (x < 0x40000000) {
		x <<= 1; y -= 0x0b172; }
	t = x + (x >> 1);
	if ((t & 0x80000000) == 0) {
		x = t; y -= 0x067cd; }
	t = x + (x >> 2);
	if ((t & 0x80000000) == 0) {
		x = t; y -= 0x03920; }
	t = x + (x >> 3);
	if ((t & 0x80000000) == 0) {
		x = t; y -= 0x01e27; }
	t = x + (x >> 4);
	if ((t & 0x80000000) == 0) {
		x = t; y -= 0x00f85; }
	t = x + (x >> 5);
	if ((t & 0x80000000) == 0) {
		x = t; y -= 0x007e1; }
	t = x + (x >> 6);
	if ((t & 0x80000000) == 0) {
		x = t; y -= 0x003f8; }
	t = x + (x >> 7);
	if ((t & 0x80000000) == 0) {
		x = t; y -= 0x001fe; }
	x = 0x80000000 - x;
	y -= x >> 15;
	return y;
}

static inline void *getd(struct dts_eagle_param_desc *depd)
{
	return (void *)(((char *)depd) + sizeof(struct dts_eagle_param_desc));
}


static int ref_cnt;
/* dts eagle parameter cache */
static char *_depc;
static __s32 _depc_size;
static __s32 _c_bl[CB_COUNT][CBD_COUNT];
static __u32 _device_primary;
static __u32 _device_all;
/* ION states */
static struct ion_client *ion_client;
static struct ion_handle *ion_handle;
static phys_addr_t paddr;
static size_t pa_len;
static void *vaddr;

#define SEC_BLOB_MAX_CNT 10
#define SEC_BLOB_MAX_SIZE 0x4004 /*extra 4 for size*/
static char *sec_blob[SEC_BLOB_MAX_CNT];

#define MAX_INBAND_PAYLOAD_SIZE 4000

/* multi-copp support */
static int _cidx[AFE_MAX_PORTS] = {-1};

/* volume controls */
#define VOL_CMD_CNT_MAX 10
static __s32 vol_cmd_cnt;
static __s32 **vol_cmds;
struct vol_cmds_d_ {
	__s32 d[4];
};
static struct vol_cmds_d_ *vol_cmds_d;

static void volume_cmds_free(void)
{
	int i;
	for (i = 0; i < vol_cmd_cnt; i++)
		kfree(vol_cmds[i]);
	vol_cmd_cnt = 0;
	kfree(vol_cmds);
	kfree(vol_cmds_d);
	vol_cmds = NULL;
	vol_cmds_d = NULL;
}

static __s32 volume_cmds_alloc1(__s32 size)
{
	volume_cmds_free();
	vol_cmd_cnt = size;
	vol_cmds = kzalloc(vol_cmd_cnt * sizeof(int *), GFP_KERNEL);
	if (vol_cmds) {
		vol_cmds_d = kzalloc(vol_cmd_cnt * sizeof(struct vol_cmds_d_),
					GFP_KERNEL);
	}
	if (vol_cmds_d)
		return 0;
	volume_cmds_free();
	return -ENOMEM;
}

/* assumes size is equal or less than 0xFFF */
static __s32 volume_cmds_alloc2(__s32 idx, __s32 size)
{
	kfree(vol_cmds[idx]);
	vol_cmds[idx] = kzalloc(size, GFP_KERNEL);
	if (vol_cmds[idx])
		return 0;
	vol_cmds_d[idx].d[0] = 0;
	return -ENOMEM;
}

static void _init_cb_descs(void)
{
	int i;
	for (i = 0; i < CB_COUNT; i++) {
		_c_bl[i][CBD_DEV_MASK] = 0;
		_c_bl[i][CBD_OFFSG] = _c_bl[i][CBD_OFFS1] =
		_c_bl[i][CBD_OFFS2] = _c_bl[i][CBD_OFFS3] =
		0xFFFFFFFF;
		_c_bl[i][CBD_CMD0] = _c_bl[i][CBD_SZ0] =
		_c_bl[i][CBD_CMD1] = _c_bl[i][CBD_SZ1] =
		_c_bl[i][CBD_CMD2] = _c_bl[i][CBD_SZ2] =
		_c_bl[i][CBD_CMD3] = _c_bl[i][CBD_SZ3] = 0;
	}
}

static __u32 _get_dev_mask_for_pid(int pid)
{
	switch (pid) {
	case SLIMBUS_0_RX:
		return (1 << AUDIO_DEVICE_OUT_EARPIECE) |
			(1 << AUDIO_DEVICE_OUT_SPEAKER) |
			(1 << AUDIO_DEVICE_OUT_WIRED_HEADSET) |
			(1 << AUDIO_DEVICE_OUT_WIRED_HEADPHONE) |
			(1 << AUDIO_DEVICE_OUT_ANC_HEADSET) |
			(1 << AUDIO_DEVICE_OUT_ANC_HEADPHONE);
	case INT_BT_SCO_RX:
		return (1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO) |
			(1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) |
			(1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT);
	case RT_PROXY_PORT_001_RX:
		return (1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP) |
			(1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES) |
			(1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER) |
			(1 << AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET) |
			(1 << AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) |
			(1 << AUDIO_DEVICE_OUT_USB_ACCESSORY) |
			(1 << AUDIO_DEVICE_OUT_USB_DEVICE) |
			(1 << AUDIO_DEVICE_OUT_PROXY);
	case HDMI_RX:
		return 1 << AUDIO_DEVICE_OUT_AUX_DIGITAL;
	case INT_FM_RX:
		return 1 << AUDIO_DEVICE_OUT_FM;
	case INT_FM_TX:
		return 1 << AUDIO_DEVICE_OUT_FM_TX;
	default:
		return 0;
	}
}

static int _get_pid_from_dev(__u32 device)
{
	if (device & (1 << AUDIO_DEVICE_OUT_EARPIECE) ||
	    device & (1 << AUDIO_DEVICE_OUT_SPEAKER) ||
	    device & (1 << AUDIO_DEVICE_OUT_WIRED_HEADSET) ||
	    device & (1 << AUDIO_DEVICE_OUT_WIRED_HEADPHONE) ||
	    device & (1 << AUDIO_DEVICE_OUT_ANC_HEADSET) ||
	    device & (1 << AUDIO_DEVICE_OUT_ANC_HEADPHONE)) {
		return SLIMBUS_0_RX;
	} else if (device & (1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO) ||
		   device & (1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) ||
		   device & (1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT)) {
		return INT_BT_SCO_RX;
	} else if (device & (1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP) ||
		   device & (1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES) ||
		   device & (1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER) ||
		   device & (1 << AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET) ||
		   device & (1 << AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ||
		   device & (1 << AUDIO_DEVICE_OUT_USB_ACCESSORY) ||
		   device & (1 << AUDIO_DEVICE_OUT_USB_DEVICE) ||
		   device & (1 << AUDIO_DEVICE_OUT_PROXY)) {
		return RT_PROXY_PORT_001_RX;
	} else if (device & (1 << AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
		return HDMI_RX;
	} else if (device & (1 << AUDIO_DEVICE_OUT_FM)) {
		return INT_FM_RX;
	} else if (device & (1 << AUDIO_DEVICE_OUT_FM_TX)) {
		return INT_FM_TX;
	}
	return 0;
}

static __u32 _get_cb_for_dev(int device)
{
	__u32 i;
	if (device & AUDIO_DEVICE_COMBO) {
		for (i = 0; i < CB_COUNT; i++) {
			if ((_c_bl[i][CBD_DEV_MASK] & device) == device)
				return i;
		}
	} else {
		for (i = 0; i < CB_COUNT; i++) {
			if ((_c_bl[i][CBD_DEV_MASK] & device) &&
			    !(_c_bl[i][CBD_DEV_MASK] & AUDIO_DEVICE_COMBO))
				return i;
		}
	}
	pr_err("DTS_EAGLE_DRIVER: %s - device %i not found, returning 0\n",
		   __func__, device);
	return 0;
}

static int _is_port_open_and_eagle(int pid)
{
	return 1; /*
	if (msm_routing_check_backend_enabled(pid))
		return 1;
	return 0;*/
}

static void reg_ion_mem(void)
{
	msm_audio_ion_alloc("DTS_EAGLE", &ion_client, &ion_handle, ION_MEM_SIZE,
				 &paddr, &pa_len, &vaddr);
}

void msm_dts_ion_memmap(struct param_outband *po)
{
	po->size = ION_MEM_SIZE;
	po->kvaddr = vaddr;
	po->paddr = paddr;
}

static void unreg_ion_mem(void)
{
	msm_audio_ion_free(ion_client, ion_handle);
}

int msm_dts_eagle_handler_pre(struct audio_client *ac, long *arg)
{
	struct dts_eagle_param_desc depd_s, *depd = &depd_s;
	__s32 offset, ret = 0;

	pr_info("DTS_EAGLE_DRIVER_PRE: %s called (set pre-param)\n", __func__);

	depd->id = (__u32) *arg++;
	depd->size = (__s32) *arg++;
	depd->offset = (__s32) *arg++;
	depd->device = (__u32) *arg++;

	if (depd->device & 0x80000000) {
		void *buf, *buf_m = NULL;
		pr_debug("DTS_EAGLE_DRIVER_PRE: get requested\n");
		depd->device &= 0x7FFFFFFF;
		if (depd->offset == -1) {
			pr_debug("DTS_EAGLE_DRIVER_PRE: get from dsp requested\n");
			if (depd->size > MAX_INBAND_PAYLOAD_SIZE) {
				pr_err("DTS_EAGLE_DRIVER_PRE: param size for inband get too big (size is %d, max inband size is %d)\n",
				depd->size, MAX_INBAND_PAYLOAD_SIZE);
				return -EINVAL;
			}
			buf = buf_m = kzalloc(depd->size, GFP_KERNEL);
			if (!buf_m) {
				pr_err("DTS_EAGLE_DRIVER_PRE: out of memory\n");
				return -ENOMEM;
			} else if (q6asm_dts_eagle_get(ac, depd->id,
							depd->size, buf) < 0) {
				pr_info("DTS_EAGLE_DRIVER_PRE: asm failed, trying core\n");
				if (core_dts_eagle_get(depd->id, depd->size,
						       buf) < 0) {
					pr_err("DTS_EAGLE_DRIVER_PRE: get from qdsp failed (topology not eagle or closed)\n");
					ret = -EFAULT;
					goto DTS_EAGLE_IOCTL_GET_PARAM_PRE_EXIT;
				}
			}
		} else {
			__u32 tgt = _get_cb_for_dev(depd->device);
			offset = _c_bl[tgt][CBD_OFFSG] + depd->offset;
			if ((offset + depd->size) > _depc_size) {
				pr_err("DTS_EAGLE_DRIVER_PRE: invalid size %d and/or offset %d\n",
					depd->size, offset);
				return -EINVAL;
			}
			buf = (void *)&_depc[offset];
		}
DTS_EAGLE_IOCTL_GET_PARAM_PRE_EXIT:
		kfree(buf_m);
		return (int)ret;
	} else {
		__u32 tgt = _get_cb_for_dev(depd->device), i, *p_depc;
		offset = _c_bl[tgt][CBD_OFFSG] + depd->offset;
		if ((offset + depd->size) > _depc_size) {
			pr_err("DTS_EAGLE_DRIVER_PRE: invalid size %i and/or offset %i for parameter (cache is size %u)\n",
					depd->size, offset, _depc_size);
			return -EINVAL;
		}
		p_depc = (__u32 *)(&_depc[offset]);
		for (i = 0; i < depd->size; i++)
			*p_depc++ = (__u32)*arg++;
		pr_debug("DTS_EAGLE_DRIVER_PRE: param info: param = 0x%X, size = %i, offset = %i, device = %i, cache block %i, global offset = %i, first bytes as integer = %i.\n",
			  depd->id, depd->size, depd->offset, depd->device,
			  tgt, offset, *(int *)&_depc[offset]);
		if (q6asm_dts_eagle_set(ac, depd->id, depd->size,
					(void *)&_depc[offset])) {
			pr_err("DTS_EAGLE_DRIVER_PRE: q6asm_dts_eagle_set failed with id = 0x%X, size = %d, offset = %d\n",
				depd->id, depd->size, depd->offset);
		} else {
			pr_debug("DTS_EAGLE_DRIVER_PRE: q6asm_dts_eagle_set succeeded with id = 0x%X, size = %d, offset = %d\n",
				 depd->id, depd->size, depd->offset);
		}
	}
	return (int)ret;
}

static const __s32 log10_10_inv_x20 = 0x0008af84;
int msm_dts_eagle_set_volume(struct audio_client *ac, int lgain, int rgain)
{
	__s32 i, val;
	__u32 idx;

	pr_debug("DTS_EAGLE_DRIVER_VOLUME: entry: vol_cmd_cnt = %i, lgain = %i, rgain = %i",
		 vol_cmd_cnt, lgain, rgain);

	if (_depc_size == 0) {
		pr_err("DTS_EAGLE_DRIVER_VOLUME: driver cache not initialized.\n");
		return -EINVAL;
	}

	for (i = 0; i < vol_cmd_cnt; i++) {
		if (vol_cmds_d[i].d[0] & 0x8000) {
			idx = (sizeof(struct dts_eagle_param_desc)/sizeof(int))
				+ (vol_cmds_d[i].d[0] & 0x3FF);
			val = fx_logN(((__s32)(lgain+rgain)) << 2);
			val = ((long long)val * log10_10_inv_x20) >> 16;
			vol_cmds[i][idx] = (__s32)clamp((int)(((long long)val *
						     vol_cmds_d[i].d[1]) >> 16),
						     vol_cmds_d[i].d[2],
						     vol_cmds_d[i].d[3]);
			pr_debug("DTS_EAGLE_DRIVER_VOLUME: loop %i cmd desc found %i, idx = %i. volume info: lgain = %i, rgain = %i, volume = %i (scale %i, min %i, max %i)\n",
				 i, vol_cmds_d[i].d[0], idx, lgain,
				 rgain, vol_cmds[i][idx], vol_cmds_d[i].d[1],
				 vol_cmds_d[i].d[2], vol_cmds_d[i].d[3]);
		}
		idx = _get_cb_for_dev(_device_primary);
		val = _c_bl[idx][CBD_OFFSG] + vol_cmds[i][2];
		if ((val + vol_cmds[i][1]) > _depc_size) {
			pr_err("DTS_EAGLE_DRIVER_VOLUME: volume size (%i) + offset (%i) out of bounds %i.\n",
				val, vol_cmds[i][1], _depc_size);
			return -EINVAL;
		}
		memcpy((void *)&_depc[val], &vol_cmds[i][4], vol_cmds[i][1]);
		if (q6asm_dts_eagle_set(ac, vol_cmds[i][0],
			vol_cmds[i][1], (void *)&_depc[val])) {
			pr_err("DTS_EAGLE_DRIVER_VOLUME: loop %i - volume set failed with id 0x%X, size %i, offset %i, cmd_desc %i, scale %i, min %i, max %i, data(...) %i\n",
				i, vol_cmds[i][0], vol_cmds[i][1],
				vol_cmds[i][2], vol_cmds_d[i].d[0],
				vol_cmds_d[i].d[1], vol_cmds_d[i].d[2],
				vol_cmds_d[i].d[3], vol_cmds[i][4]);
		} else {
			pr_debug("DTS_EAGLE_DRIVER_VOLUME: loop %i - volume set succeeded with id 0x%X, size %i, offset %i, cmd_desc %i, scale %i, min %i, max %i, data(...) %i\n",
				 i, vol_cmds[i][0], vol_cmds[i][1],
				 vol_cmds[i][2], vol_cmds_d[i].d[0],
				 vol_cmds_d[i].d[1], vol_cmds_d[i].d[2],
				 vol_cmds_d[i].d[3], vol_cmds[i][4]);
		}
	}
	return 0;
}

int msm_dts_eagle_ioctl(unsigned int cmd, unsigned long arg)
{
	__s32 ret = 0;
	switch (cmd) {
	case DTS_EAGLE_IOCTL_GET_CACHE_SIZE: {
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (get param cache size)\n",
			__func__, cmd);
		if (copy_to_user((void *)arg, &_depc_size,
				 sizeof(_depc_size))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error writing size\n");
			return -EFAULT;
		}
		break;
	}
	case DTS_EAGLE_IOCTL_SET_CACHE_SIZE: {
		__s32 size = 0;
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (allocate param cache)\n",
			__func__, cmd);
		if (copy_from_user((void *)&size, (void *)arg, sizeof(size))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying size (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, &size, sizeof(size));
			return -EFAULT;
		} else if (size < 0 || size > DEPC_MAX_SIZE) {
			pr_err("DTS_EAGLE_DRIVER_POST: cache size %d not allowed (min 0, max %d)\n",
				size, DEPC_MAX_SIZE);
			return -EINVAL;
		}
		if (_depc) {
			pr_info("DTS_EAGLE_DRIVER_POST: previous param cache of size %u freed\n",
				_depc_size);
			_depc_size = 0;
			kfree(_depc);
			_depc = NULL;
		}
		if (size) {
			_depc = kzalloc(size, GFP_KERNEL);
		} else {
			pr_info("DTS_EAGLE_DRIVER_POST: %d bytes requested for param cache, nothing allocated\n",
				size);
		}
		if (_depc) {
			pr_info("DTS_EAGLE_DRIVER_POST: %d bytes allocated for param cache\n",
				size);
			_depc_size = size;
		} else {
			pr_err("DTS_EAGLE_DRIVER_POST: error allocating param cache (kzalloc failed on %d bytes)\n",
				size);
			_depc_size = 0;
			return -ENOMEM;
		}
		break;
	}
	case DTS_EAGLE_IOCTL_GET_PARAM: {
		struct dts_eagle_param_desc depd;
		__s32 offset = 0;
		void *buf, *buf_m = NULL;
		pr_info("DTS_EAGLE_DRIVER_POST: %s called, control 0x%X (get param)\n",
			__func__, cmd);
		if (copy_from_user((void *)&depd, (void *)arg, sizeof(depd))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying dts_eagle_param_desc (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, &depd, sizeof(depd));
			return -EFAULT;
		}
		if (depd.offset == -1) {
			__u32 pid = _get_pid_from_dev(depd.device), cidx;
			cidx = adm_validate_and_get_port_index(pid);
			pr_info("DTS_EAGLE_DRIVER_POST: get from qdsp requested (port id 0x%X)\n",
				pid);
			if (depd.size > MAX_INBAND_PAYLOAD_SIZE) {
				pr_err("DTS_EAGLE_DRIVER_POST: param size for inband get too big (size is %d, max inband size is %d)\n",
				depd.size, MAX_INBAND_PAYLOAD_SIZE);
				return -EINVAL;
			}
			buf = buf_m = kzalloc(depd.size, GFP_KERNEL);
			if (!buf_m) {
				pr_err("DTS_EAGLE_DRIVER_POST: out of memory\n");
				return -ENOMEM;
			} else if (adm_dts_eagle_get(pid, _cidx[cidx], depd.id,
						     buf, depd.size) < 0) {
				pr_info("DTS_EAGLE_DRIVER_POST: get from qdsp via adm with port id 0x%X failed, trying core\n",
					pid);
				if (core_dts_eagle_get(depd.id, depd.size,
							buf) < 0) {
					pr_err("DTS_EAGLE_DRIVER_POST: get from qdsp failed\n");
					ret = -EFAULT;
					goto DTS_EAGLE_IOCTL_GET_PARAM_EXIT;
				}
			}
		} else {
			__u32 cb = _get_cb_for_dev(depd.device);
			offset = _c_bl[cb][CBD_OFFSG] + depd.offset;
			if ((offset + depd.size) > _depc_size) {
				pr_err("DTS_EAGLE_DRIVER_POST: invalid size %d and/or offset %d\n",
					depd.size, offset);
				return -EINVAL;
			}
			buf = (void *)&_depc[offset];
		}
		if (copy_to_user((void *)(((char *)arg)+sizeof(depd)),
						  buf, depd.size)) {
			pr_err("DTS_EAGLE_DRIVER_POST: error getting param\n");
			ret = -EFAULT;
			goto DTS_EAGLE_IOCTL_GET_PARAM_EXIT;
		}
DTS_EAGLE_IOCTL_GET_PARAM_EXIT:
		kfree(buf_m);
		break;
	}
	case DTS_EAGLE_IOCTL_SET_PARAM: {
		struct dts_eagle_param_desc depd;
		__s32 offset = 0, just_set_cache = 0;
		__u32 tgt;
		pr_info("DTS_EAGLE_DRIVER_POST: %s called, control 0x%X (set param)\n",
			__func__, cmd);
		if (copy_from_user((void *)&depd, (void *)arg, sizeof(depd))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying dts_eagle_param_desc (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, &depd, sizeof(depd));
			return -EFAULT;
		}
		if (depd.device & (1<<31)) {
			pr_info("DTS_EAGLE_DRIVER_POST: 'just set cache' requested.\n");
			just_set_cache = 1;
			depd.device &= 0x7FFFFFFF;
		}
		tgt = _get_cb_for_dev(depd.device);
		offset = _c_bl[tgt][CBD_OFFSG] + depd.offset;
		if ((offset + depd.size) > _depc_size) {
			pr_err("DTS_EAGLE_DRIVER_POST: invalid size %i and/or offset %i for parameter (target cache block %i with offset %i, global cache is size %u)\n",
				depd.size, offset, tgt,
				_c_bl[tgt][CBD_OFFSG], _depc_size);
			return -EINVAL;
		}
		if (copy_from_user((void *)&_depc[offset],
				   (void *)(((char *)arg)+sizeof(depd)),
					depd.size)) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying param to cache (src:%p, tgt:%p, size:%i)\n",
				((char *)arg)+sizeof(depd),
				&_depc[offset], depd.size);
			return -EFAULT;
		}
		pr_debug("DTS_EAGLE_DRIVER_POST: param info: param = 0x%X, size = %i, offset = %i, device = %i, cache block %i, global offset = %i, first bytes as integer = %i.\n",
			  depd.id, depd.size, depd.offset, depd.device,
			  tgt, offset, *(int *)&_depc[offset]);
		if (!just_set_cache) {
			__u32 pid = _get_pid_from_dev(depd.device), cidx;
			cidx = adm_validate_and_get_port_index(pid);
			pr_debug("DTS_EAGLE_DRIVER_POST: checking for active Eagle for device %i (port id 0x%X)\n",
				depd.device, pid);
			if (_is_port_open_and_eagle(pid)) {
				if (adm_dts_eagle_set(pid, _cidx[cidx], depd.id,
				    (void *)&_depc[offset], depd.size) < 0) {
					pr_err("DTS_EAGLE_DRIVER_POST: adm_dts_eagle_set failed with id = 0x%X, size = %i, offset = %i, device = %i, global offset = %i\n",
						depd.id, depd.size, depd.offset,
						depd.device, offset);
				} else {
					pr_debug("DTS_EAGLE_DRIVER_POST: adm_dts_eagle_set succeeded with id = 0x%X, size = %i, offset = %i, device = %i, global offset = %i\n",
						depd.id, depd.size, depd.offset,
						depd.device, offset);
				}
			} else {
				pr_debug("DTS_EAGLE_DRIVER_POST: port id 0x%X not active or not Eagle\n",
					 pid);
			}
		}
		break;
	}
	case DTS_EAGLE_IOCTL_SET_CACHE_BLOCK: {
		__u32 b_[CBD_COUNT+1], *b = &b_[1], cb;
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (set param cache block)\n",
			__func__, cmd);
		if (copy_from_user((void *)b_, (void *)arg, sizeof(b_))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying cache block data (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, b_, sizeof(b_));
			return -EFAULT;
		}
		cb = b_[0];
		if (cb >= CB_COUNT) {
			pr_err("DTS_EAGLE_DRIVER_POST: cache block %u out of range (max %u)\n",
			cb, CB_COUNT-1);
			return -EINVAL;
		}
		if ((b[CBD_OFFSG]+b[CBD_OFFS1]+b[CBD_SZ1]) >= _depc_size ||
			(b[CBD_OFFSG]+b[CBD_OFFS2]+b[CBD_SZ2]) >= _depc_size ||
			(b[CBD_OFFSG]+b[CBD_OFFS3]+b[CBD_SZ3]) >= _depc_size) {
			pr_err("DTS_EAGLE_DRIVER_POST: cache block bounds out of range\n");
			return -EINVAL;
		}
		memcpy(_c_bl[cb], b, sizeof(_c_bl[cb]));
		pr_debug("DTS_EAGLE_DRIVER_POST: cache block %i set: devices 0x%X, global offset %u, offsets 1:%u 2:%u 3:%u, cmds/sizes 0:0x%X %u 1:0x%X %u 2:0x%X %u 3:0x%X %u\n",
		cb, _c_bl[cb][CBD_DEV_MASK], _c_bl[cb][CBD_OFFSG],
		_c_bl[cb][CBD_OFFS1], _c_bl[cb][CBD_OFFS2],
		_c_bl[cb][CBD_OFFS3], _c_bl[cb][CBD_CMD0], _c_bl[cb][CBD_SZ0],
		_c_bl[cb][CBD_CMD1], _c_bl[cb][CBD_SZ1], _c_bl[cb][CBD_CMD2],
		_c_bl[cb][CBD_SZ2], _c_bl[cb][CBD_CMD3], _c_bl[cb][CBD_SZ3]);
		break;
	}
	case DTS_EAGLE_IOCTL_SET_ACTIVE_DEVICE: {
		__u32 data[2];
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (set active device)\n",
			__func__, cmd);
		if (copy_from_user((void *)data, (void *)arg, sizeof(data))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying active device data (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, data, sizeof(data));
			return -EFAULT;
		}
		if (data[1] != 0) {
			_device_primary = data[0];
			pr_debug("DTS_EAGLE_DRIVER_POST: primary device %i\n",
				 data[0]);
		} else {
			_device_all = data[0];
			pr_debug("DTS_EAGLE_DRIVER_POST: all devices 0x%X\n",
				 data[0]);
		}
		break;
	}
	case DTS_EAGLE_IOCTL_GET_LICENSE: {
		__u32 target = 0;
		__s32 size = 0, size_only;
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (get license)\n",
			__func__, cmd);
		if (copy_from_user((void *)&target, (void *)arg,
				   sizeof(target))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error reading license index. (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, &target, sizeof(target));
			return -EFAULT;
		}
		size_only = target & (1<<31) ? 1 : 0;
		target &= 0x7FFFFFFF;
		if (target < 0 || target >= SEC_BLOB_MAX_CNT) {
			pr_err("DTS_EAGLE_DRIVER_POST: license index %i out of bounds (max index is %i)\n",
				   target, SEC_BLOB_MAX_CNT);
			return -EINVAL;
		}
		if (sec_blob[target] == NULL) {
			pr_err("DTS_EAGLE_DRIVER_POST: license index %i never initialized.\n",
				   target);
			return -EINVAL;
		}
		size = ((__s32 *)sec_blob[target])[0];
		if (size <= 0 || size > SEC_BLOB_MAX_SIZE) {
			pr_err("DTS_EAGLE_DRIVER_POST: license size %i for index %i invalid (min size is 1, max size is %i).\n",
				   size, target, SEC_BLOB_MAX_SIZE);
			return -EINVAL;
		}
		if (size_only) {
			pr_info("DTS_EAGLE_DRIVER_POST: reporting size of license data only\n");
			if (copy_to_user((void *)(((char *)arg)+sizeof(target)),
				 (void *)&size, sizeof(size))) {
				pr_err("DTS_EAGLE_DRIVER_POST: error copying license size.\n");
				return -EFAULT;
			}
		} else if (copy_to_user((void *)(((char *)arg)+sizeof(target)),
			   (void *)&(((__s32 *)sec_blob[target])[1]), size)) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying license data.\n");
			return -EFAULT;
		} else {
			pr_debug("DTS_EAGLE_DRIVER_POST: license file %i bytes long from license index %i returned to user.\n",
				  size, target);
		}
		break;
	}
	case DTS_EAGLE_IOCTL_SET_LICENSE: {
		__s32 target[2] = {0, 0};
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (set license)\n",
			__func__, cmd);
		if (copy_from_user((void *)target, (void *)arg,
				   sizeof(target))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error reading license index (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, target, sizeof(target));
			return -EFAULT;
		}
		if (target[0] < 0 || target[0] >= SEC_BLOB_MAX_CNT) {
			pr_err("DTS_EAGLE_DRIVER_POST: license index %i out of bounds (max index is %i)\n",
				   target[0], SEC_BLOB_MAX_CNT-1);
			return -EINVAL;
		}
		if (target[1] == 0) {
			pr_info("DTS_EAGLE_DRIVER_POST: request to free license index %i\n",
				 target[0]);
			kfree(sec_blob[target[0]]);
			sec_blob[target[0]] = NULL;
			break;
		}
		if (target[1] <= 0 || target[1] >= SEC_BLOB_MAX_SIZE) {
			pr_err("DTS_EAGLE_DRIVER_POST: license size %i for index %i invalid (min size is 1, max size is %i).\n",
				   target[1], target[0], SEC_BLOB_MAX_SIZE);
			return -EINVAL;
		}
		if (sec_blob[target[0]] != NULL) {
			if (((__s32 *)sec_blob[target[0]])[1] != target[1]) {
				pr_info("DTS_EAGLE_DRIVER_POST: request new size for already allocated license index %i\n",
						target[0]);
				kfree(sec_blob[target[0]]);
				sec_blob[target[0]] = NULL;
			}
		}
		pr_debug("DTS_EAGLE_DRIVER_POST: allocating %i bytes for license index %i\n",
				  target[1], target[0]);
		sec_blob[target[0]] = kzalloc(target[1] + 4, GFP_KERNEL);
		if (!sec_blob[target[0]]) {
			pr_err("DTS_EAGLE_DRIVER_POST: error allocating license index %i (kzalloc failed on %i bytes)\n",
					target[0], target[1]);
			return -ENOMEM;
		}
		((__s32 *)sec_blob[target[0]])[0] = target[1];
		if (copy_from_user((void *)&(((__s32 *)sec_blob[target[0]])[1]),
				(void *)(((char *)arg)+sizeof(target)),
				target[1])) {
			pr_err("DTS_EAGLE_DRIVER_POST: error copying license to index %i, size %i (src:%p, tgt:%p, size:%i)\n",
					target[0], target[1],
					((char *)arg)+sizeof(target),
					&(((__s32 *)sec_blob[target[0]])[1]),
					target[1]);
			return -EFAULT;
		} else {
			pr_debug("DTS_EAGLE_DRIVER_POST: license file %i bytes long copied to index license index %i\n",
				  target[1], target[0]);
		}
		break;
	}
	case DTS_EAGLE_IOCTL_SEND_LICENSE: {
		__s32 target = 0;
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (send license)\n",
			__func__, cmd);
		if (copy_from_user((void *)&target, (void *)arg,
				   sizeof(target))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error reading license index (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, &target, sizeof(target));
			return -EFAULT;
		}
		if (target >= SEC_BLOB_MAX_CNT) {
			pr_err("DTS_EAGLE_DRIVER_POST: license index %i out of bounds (max index is %i)\n",
					target, SEC_BLOB_MAX_CNT-1);
			return -EINVAL;
		}
		if (!sec_blob[target] || ((__s32 *)sec_blob[target])[0] <= 0) {
			pr_err("DTS_EAGLE_DRIVER_POST: license index %i is invalid\n",
				target);
			return -EINVAL;
		}
		if (core_dts_eagle_set(((__s32 *)sec_blob[target])[0],
				(char *)&((__s32 *)sec_blob[target])[1]) < 0) {
			pr_err("DTS_EAGLE_DRIVER_POST: core_dts_eagle_set failed with id = %i\n",
				target);
		} else {
			pr_debug("DTS_EAGLE_DRIVER_POST: core_dts_eagle_set succeeded with id = %i\n",
				 target);
		}
		break;
	}
	case DTS_EAGLE_IOCTL_SET_VOLUME_COMMANDS: {
		__s32 spec = 0;
		pr_info("DTS_EAGLE_DRIVER_POST: %s called with control 0x%X (set volume commands)\n",
			__func__, cmd);
		if (copy_from_user((void *)&spec, (void *)arg,
					sizeof(spec))) {
			pr_err("DTS_EAGLE_DRIVER_POST: error reading volume command specifier (src:%p, tgt:%p, size:%zu)\n",
				(void *)arg, &spec, sizeof(spec));
			return -EFAULT;
		}
		if (spec & 0x80000000) {
			__u32 idx = (spec & 0x0000F000) >> 12;
			__s32 size = spec & 0x00000FFF;
			pr_debug("DTS_EAGLE_DRIVER_POST: setting volume command %i size: %i\n",
				 idx, size);
			if (idx >= vol_cmd_cnt) {
				pr_err("DTS_EAGLE_DRIVER_POST: volume command index %i out of bounds (only %i allocated).\n",
					idx, vol_cmd_cnt);
				return -EINVAL;
			}
			if (volume_cmds_alloc2(idx, size) < 0) {
				pr_err("DTS_EAGLE_DRIVER_POST: error allocating memory for volume controls.\n");
				return -ENOMEM;
			}
			if (copy_from_user((void *)&vol_cmds_d[idx],
					(void *)(((char *)arg) + sizeof(int)),
					sizeof(struct vol_cmds_d_))) {
				pr_err("DTS_EAGLE_DRIVER_POST: error reading volume command descriptor (src:%p, tgt:%p, size:%zu)\n",
					((char *)arg) + sizeof(int),
					&vol_cmds_d[idx],
					sizeof(struct vol_cmds_d_));
				return -EFAULT;
			}
			pr_debug("DTS_EAGLE_DRIVER_POST: setting volume command %i spec (size %zu): %i %i %i %i\n",
				  idx, sizeof(struct vol_cmds_d_),
				  vol_cmds_d[idx].d[0], vol_cmds_d[idx].d[1],
				  vol_cmds_d[idx].d[2], vol_cmds_d[idx].d[3]);
			if (copy_from_user((void *)vol_cmds[idx],
					(void *)(((char *)arg) + (sizeof(int) +
					sizeof(struct vol_cmds_d_))), size)) {
				pr_err("DTS_EAGLE_DRIVER_POST: error reading volume command string (src:%p, tgt:%p, size:%i)\n",
					((char *)arg) + (sizeof(int) +
					sizeof(struct vol_cmds_d_)),
					vol_cmds[idx], size);
				return -EFAULT;
			}
		} else {
			pr_debug("DTS_EAGLE_DRIVER_POST: setting volume command size\n");
			if (spec < 0 || spec > VOL_CMD_CNT_MAX) {
				pr_err("DTS_EAGLE_DRIVER_POST: volume command count %i out of bounds (min 0, max %i).\n",
				spec, VOL_CMD_CNT_MAX);
				return -EINVAL;
			} else if (spec == 0) {
				pr_info("DTS_EAGLE_DRIVER_POST: request to free volume commands.\n");
				volume_cmds_free();
				break;
			}
			pr_debug("DTS_EAGLE_DRIVER_POST: setting volume command size requested = %i\n",
				  spec);
			if (volume_cmds_alloc1(spec) < 0) {
				pr_err("DTS_EAGLE_DRIVER_POST: error allocating memory for volume controls.\n");
				return -ENOMEM;
			}
		}
		break;
	}
	default: {
		pr_info("DTS_EAGLE_DRIVER_POST: %s called, control 0x%X (invalid control)\n",
			__func__, cmd);
		ret = -EINVAL;
	}
	}
	return (int)ret;
}

int msm_dts_eagle_init_pre(struct audio_client *ac)
{
	int offset, cidx, size, cmd;
	cidx = _get_cb_for_dev(_device_primary);
	if (cidx < 0) {
		pr_err("DTS_EAGLE_DRIVER_SENDCACHE_PRE: in %s, no cache for primary device %i found.\n",
			__func__, _device_primary);
		return -EINVAL;
	}
	offset = _c_bl[cidx][CBD_OFFSG];
	cmd = _c_bl[cidx][CBD_CMD0];
	size = _c_bl[cidx][CBD_SZ0];

	if (_depc_size == 0 || !_depc || offset < 0 || size <= 0 || cmd == 0 ||
	    (offset + size) > _depc_size) {
		pr_err("DTS_EAGLE_DRIVER_SENDCACHE_PRE: in %s, primary device %i cache index %i general error - cache size = %u, cache ptr = %p, offset = %i, size = %i, cmd = %i\n",
			__func__, _device_primary, cidx, _depc_size, _depc,
			offset, size, cmd);
		return -EINVAL;
	}

	pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_PRE: first 6 integers %i %i %i %i %i %i (30th %i)\n",
		  *((int *)&_depc[offset]), *((int *)&_depc[offset+4]),
		  *((int *)&_depc[offset+8]), *((int *)&_depc[offset+12]),
		  *((int *)&_depc[offset+16]), *((int *)&_depc[offset+20]),
		  *((int *)&_depc[offset+120]));
	pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_PRE: sending full data block to port, with cache index = %d device mask 0x%X, param = 0x%X, offset = %d, and size = %d\n",
		  cidx, _c_bl[cidx][CBD_DEV_MASK], cmd, offset, size);

	if (q6asm_dts_eagle_set(ac, cmd, size, (void *)&_depc[offset])) {
		pr_err("DTS_EAGLE_DRIVER_SENDCACHE_PRE: in %s, q6asm_dts_eagle_set failed with id = %d and size = %d\n",
			__func__, cmd, size);
	} else {
		pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_PRE: in %s, q6asm_dts_eagle_set succeeded with id = %d and size = %d\n",
			 __func__, cmd, size);
	}
	return 0;
}

int msm_dts_eagle_deinit_pre(struct audio_client *ac)
{
	return 1;
}

int msm_dts_eagle_init_post(int port_id, int copp_idx, int topology)
{
	int offset, cidx = -1, size, cmd, mask;


	{
	int index = adm_validate_and_get_port_index(port_id);
	if (index < 0) {
		pr_err("DTS_EAGLE_DRIVER_SENDCACHE_POST :%s: Invalid port idx %d port_id %#x\n",
			__func__, index, port_id);
	} else {
		pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_POST : %s valid port idx %d for port_id %#x set to %i",
			 __func__, index, port_id, copp_idx);
	}
	_cidx[index] = copp_idx;
	}




	mask = _get_dev_mask_for_pid(port_id);
	if (mask & _device_primary) {
		cidx = _get_cb_for_dev(_device_primary);
		if (cidx < 0) {
			pr_err("DTS_EAGLE_DRIVER_SENDCACHE_POST: in %s, no cache for primary device %i found. Port id was 0x%X.\n",
				__func__, _device_primary, port_id);
			return -EINVAL;
		}
	} else if (mask & _device_all) {
		cidx = _get_cb_for_dev(_device_all);
		if (cidx < 0) {
			pr_err("DTS_EAGLE_DRIVER_SENDCACHE_POST: in %s, no cache for combo device %i found. Port id was 0x%X.\n",
				__func__, _device_primary, port_id);
			return -EINVAL;
		}
	} else {
		pr_err("DTS_EAGLE_DRIVER_SENDCACHE_POST: in %s, port id 0x%X not for primary or combo device %i.\n",
			__func__, port_id, _device_primary);
		return -EINVAL;
	}
	offset = _c_bl[cidx][CBD_OFFSG] + _c_bl[cidx][CBD_OFFS2];
	cmd = _c_bl[cidx][CBD_CMD2];
	size = _c_bl[cidx][CBD_SZ2];

	if (_depc_size == 0 || !_depc || offset < 0 || size <= 0 || cmd == 0 ||
		(offset + size) > _depc_size) {
		pr_err("DTS_EAGLE_DRIVER_SENDCACHE_POST: in %s, primary device %i cache index %i port_id 0x%X general error - cache size = %u, cache ptr = %p, offset = %i, size = %i, cmd = %i\n",
			__func__, _device_primary, cidx, port_id,
			_depc_size, _depc, offset, size, cmd);
		return -EINVAL;
	}

	pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_POST: first 6 integers %i %i %i %i %i %i\n",
		  *((int *)&_depc[offset]), *((int *)&_depc[offset+4]),
		  *((int *)&_depc[offset+8]), *((int *)&_depc[offset+12]),
		  *((int *)&_depc[offset+16]), *((int *)&_depc[offset+20]));
	pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_POST: sending full data block to port, with cache index = %d device mask 0x%X, port_id = 0x%X, param = 0x%X, offset = %d, and size = %d\n",
		  cidx, _c_bl[cidx][CBD_DEV_MASK], port_id, cmd, offset, size);

	if (adm_dts_eagle_set(port_id, copp_idx, cmd,
			      (void *)&_depc[offset], size) < 0) {
		pr_err("DTS_EAGLE_DRIVER_SENDCACHE_POST: in %s, adm_dts_eagle_set failed with id = 0x%X and size = %d\n",
			__func__, cmd, size);
	} else {
		pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_POST: in %s, adm_dts_eagle_set succeeded with id = 0x%X and size = %d\n",
			 __func__, cmd, size);
	}
	return 0;
}

int msm_dts_eagle_deinit_post(int port_id, int topology)
{
	return 1;
}

int msm_dts_eagle_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	if (!ref_cnt) {
		_init_cb_descs();
		reg_ion_mem();
	}
	ref_cnt++;

	return 0;
}

void msm_dts_eagle_pcm_free(struct snd_pcm *pcm)
{
	/* TODO: Remove hwdep interface */
	ref_cnt--;
	if (!ref_cnt)
		unreg_ion_mem();
}

MODULE_DESCRIPTION("DTS EAGLE platform driver");
MODULE_LICENSE("GPL v2");
