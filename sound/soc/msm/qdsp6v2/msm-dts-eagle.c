/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/vmalloc.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/q6adm-v2.h>
#include <sound/q6asm-v2.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/audio_effects.h>
#include <sound/hwdep.h>
#include <sound/msm-dts-eagle.h>
#include <sound/q6core.h>

#include "msm-pcm-routing-v2.h"

#define ION_MEM_SIZE  131072
#define DEPC_MAX_SIZE 524288

#define MPST				AUDPROC_MODULE_ID_DTS_HPX_POSTMIX
#define MPRE				AUDPROC_MODULE_ID_DTS_HPX_PREMIX

#define eagle_vol_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_DRIVER_VOLUME: " fmt "\n", ##__VA_ARGS__)
#define eagle_vol_err(fmt, ...) \
	pr_err("DTS_EAGLE_DRIVER_VOLUME: " fmt "\n", ##__VA_ARGS__)
#define eagle_drv_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_DRIVER: " fmt "\n", ##__VA_ARGS__)
#define eagle_drv_err(fmt, ...) \
	pr_err("DTS_EAGLE_DRIVER: " fmt "\n", ##__VA_ARGS__)
#define eagle_precache_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_PRE: " fmt "\n", ##__VA_ARGS__)
#define eagle_precache_err(fmt, ...) \
	pr_err("DTS_EAGLE_DRIVER_SENDCACHE_PRE: " fmt "\n", ##__VA_ARGS__)
#define eagle_postcache_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_DRIVER_SENDCACHE_POST: " fmt "\n", ##__VA_ARGS__)
#define eagle_postcache_err(fmt, ...) \
	pr_err("DTS_EAGLE_DRIVER_SENDCACHE_POST: " fmt "\n", ##__VA_ARGS__)
#define eagle_ioctl_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_DRIVER_IOCTL: " fmt "\n", ##__VA_ARGS__)
#define eagle_ioctl_err(fmt, ...) \
	pr_err("DTS_EAGLE_DRIVER_IOCTL: " fmt "\n", ##__VA_ARGS__)
#define eagle_asm_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_DRIVER_ASM: " fmt "\n", ##__VA_ARGS__)
#define eagle_asm_err(fmt, ...) \
	pr_err("DTS_EAGLE_DRIVER_ASM: " fmt "\n", ##__VA_ARGS__)
#define eagle_adm_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_DRIVER_ADM: " fmt "\n", ##__VA_ARGS__)
#define eagle_adm_err(fmt, ...) \
	pr_err("DTS_EAGLE_DRIVER_ADM: " fmt "\n", ##__VA_ARGS__)
#define eagle_enable_dbg(fmt, ...) \
	pr_debug("DTS_EAGLE_ENABLE: " fmt "\n", ##__VA_ARGS__)
#define eagle_enable_err(fmt, ...) \
	pr_err("DTS_EAGLE_ENABLE: " fmt "\n", ##__VA_ARGS__)
#define eagle_ioctl_info(fmt, ...) \
	pr_err("DTS_EAGLE_IOCTL: " fmt "\n", ##__VA_ARGS__)

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

static s32 _fx_logN(s32 x)
{
	s32 t, y = 0xa65af;
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

static inline void *_getd(struct dts_eagle_param_desc *depd)
{
	return (void *)(((char *)depd) + sizeof(struct dts_eagle_param_desc));
}

static int _ref_cnt;
/* dts eagle parameter cache */
static char *_depc;
static u32 _depc_size;
static s32 _c_bl[CB_COUNT][CBD_COUNT];
static u32 _device_primary;
static u32 _device_all;
/* ION states */
static struct ion_client *_ion_client;
static struct ion_handle *_ion_handle;
static struct param_outband _po;
static struct audio_client *_ac_NT;
static struct ion_client *_ion_client_NT;
static struct ion_handle *_ion_handle_NT;
static struct param_outband _po_NT;

#define SEC_BLOB_MAX_CNT 10
#define SEC_BLOB_MAX_SIZE 0x4004 /*extra 4 for size*/
static char *_sec_blob[SEC_BLOB_MAX_CNT];

/* multi-copp support */
static int _cidx[AFE_MAX_PORTS] = {-1};

/* volume controls */
#define VOL_CMD_CNT_MAX 10
static u32 _vol_cmd_cnt;
static s32 **_vol_cmds;
struct vol_cmds_d {
	s32 d[4];
};
static struct vol_cmds_d *_vol_cmds_d;
static const s32 _log10_10_inv_x20 = 0x0008af84;

/* hpx master control */
static u32 _is_hpx_enabled;

static void _volume_cmds_free(void)
{
	int i;
	for (i = 0; i < _vol_cmd_cnt; i++)
		kfree(_vol_cmds[i]);
	_vol_cmd_cnt = 0;
	kfree(_vol_cmds);
	kfree(_vol_cmds_d);
	_vol_cmds = NULL;
	_vol_cmds_d = NULL;
}

static s32 _volume_cmds_alloc1(s32 size)
{
	_volume_cmds_free();
	_vol_cmd_cnt = size;
	_vol_cmds = kzalloc(_vol_cmd_cnt * sizeof(int *), GFP_KERNEL);
	if (_vol_cmds) {
		_vol_cmds_d = kzalloc(_vol_cmd_cnt * sizeof(struct vol_cmds_d),
					GFP_KERNEL);
	}
	if (_vol_cmds_d)
		return 0;
	_volume_cmds_free();
	return -ENOMEM;
}

/* assumes size is equal or less than 0xFFF */
static s32 _volume_cmds_alloc2(s32 idx, s32 size)
{
	kfree(_vol_cmds[idx]);
	_vol_cmds[idx] = kzalloc(size, GFP_KERNEL);
	if (_vol_cmds[idx])
		return 0;
	_vol_cmds_d[idx].d[0] = 0;
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

static u32 _get_dev_mask_for_pid(int pid)
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

static int _get_pid_from_dev(u32 device)
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

static s32 _get_cb_for_dev(int device)
{
	s32 i;
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
	eagle_drv_err("%s: device %i not found", __func__, device);
	return -EINVAL;
}

static int _is_port_open_and_eagle(int pid)
{
	if (msm_routing_check_backend_enabled(pid))
		return 1;
	return 1;
}

static int _isNTDevice(u32 device)
{
	if (device &
		((1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO) |
		(1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) |
		(1 << AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT) |
		(1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP) |
		(1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES) |
		(1 << AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER) |
		(1 << AUDIO_DEVICE_OUT_AUX_DIGITAL)))
		return 1;
	return 0;
}

static void _reg_ion_mem(void)
{
	int rc;
	rc = msm_audio_ion_alloc("DTS_EAGLE", &_ion_client, &_ion_handle,
			    ION_MEM_SIZE, &_po.paddr, &_po.size, &_po.kvaddr);
	if (rc)
		eagle_drv_err("%s: msm audio ion alloc failed with %i",
				__func__, rc);
}

static void _unreg_ion_mem(void)
{
	int rc;
	rc = msm_audio_ion_free(_ion_client, _ion_handle);
	if (rc)
		eagle_drv_err("%s: msm audio ion alloc failed with %i",
				__func__, rc);
}

static void _reg_ion_mem_NT(void)
{
	int rc;
	eagle_drv_dbg("%s: NT ion mem", __func__);
	rc = msm_audio_ion_alloc("DTS_EAGLE", &_ion_client_NT,
				 &_ion_handle_NT, ION_MEM_SIZE,
				 &_po_NT.paddr, &_po_NT.size, &_po_NT.kvaddr);
	if (rc) {
		eagle_drv_err("%s: msm audio ion alloc failed",	__func__);
		return;
	}
	rc = q6asm_memory_map(_ac_NT, _po_NT.paddr,
			      IN, _po_NT.size, 1);
	if (rc < 0) {
		eagle_drv_err("%s: memory map failed", __func__);
		msm_audio_ion_free(_ion_client_NT, _ion_handle_NT);
		_ion_client_NT = NULL;
		_ion_handle_NT = NULL;
	}
}

static void _unreg_ion_mem_NT(void)
{
	int rc;
	rc = q6asm_memory_unmap(_ac_NT,	_po_NT.paddr, IN);
	if (rc < 0)
		eagle_drv_err("%s: mem unmap failed", __func__);
	rc = msm_audio_ion_free(_ion_client_NT, _ion_handle_NT);
	if (rc < 0)
		eagle_drv_err("%s: mem free failed", __func__);

	_ion_client_NT = NULL;
	_ion_handle_NT = NULL;
}

static struct audio_client *_getNTDeviceAC(void)
{
	return _ac_NT;
}

static void _set_audioclient(struct audio_client *ac)
{
	_ac_NT = ac;
	_reg_ion_mem_NT();
}

static void _clear_audioclient(void)
{
	_unreg_ion_mem_NT();
	_ac_NT = NULL;
}


static int _sendcache_pre(struct audio_client *ac)
{
	uint32_t offset, size;
	int32_t cidx, cmd, err = 0;
	cidx = _get_cb_for_dev(_device_primary);
	if (cidx < 0) {
		eagle_precache_err("%s: no cache for primary device %i found",
			__func__, _device_primary);
		return -EINVAL;
	}
	offset = _c_bl[cidx][CBD_OFFSG];
	cmd = _c_bl[cidx][CBD_CMD0];
	size = _c_bl[cidx][CBD_SZ0];
	/* check for integer overflow */
	if (offset > (UINT_MAX - size))
		err = -EINVAL;
	if ((_depc_size == 0) || !_depc || (size == 0) ||
		cmd == 0 || ((offset + size) > _depc_size) || (err != 0)) {
		eagle_precache_err("%s: primary device %i cache index %i general error - cache size = %u, cache ptr = %pK, offset = %u, size = %u, cmd = %i",
			__func__, _device_primary, cidx, _depc_size, _depc,
			offset, size, cmd);
		return -EINVAL;
	}

	if ((offset < (UINT_MAX - 124)) && ((offset + 124) < _depc_size))
		eagle_precache_dbg("%s: first 6 integers %i %i %i %i %i %i (30th %i)",
			__func__, *((int *)&_depc[offset]),
			*((int *)&_depc[offset+4]),
			*((int *)&_depc[offset+8]),
			*((int *)&_depc[offset+12]),
			*((int *)&_depc[offset+16]),
			*((int *)&_depc[offset+20]),
			*((int *)&_depc[offset+120]));
	eagle_precache_dbg("%s: sending full data block to port, with cache index = %d device mask 0x%X, param = 0x%X, offset = %u, and size = %u",
		  __func__, cidx, _c_bl[cidx][CBD_DEV_MASK], cmd, offset, size);

	if (q6asm_dts_eagle_set(ac, cmd, size, (void *)&_depc[offset],
				NULL, MPRE))
		eagle_precache_err("%s: q6asm_dts_eagle_set failed with id = %d and size = %u",
			__func__, cmd, size);
	else
		eagle_precache_dbg("%s: q6asm_dts_eagle_set succeeded with id = %d and size = %u",
			 __func__, cmd, size);
	return 0;
}

static int _sendcache_post(int port_id, int copp_idx, int topology)
{
	int cidx = -1, cmd, mask, index, err = 0;
	uint32_t offset, size;

	if (port_id == -1) {
		cidx = _get_cb_for_dev(_device_primary);
		if (cidx < 0) {
			eagle_postcache_err("%s: no cache for primary device %i found. Port id was 0x%X",
				__func__, _device_primary, port_id);
			return -EINVAL;
		}
		goto NT_MODE_GOTO;
	}

	index = adm_validate_and_get_port_index(port_id);
	if (index < 0) {
		eagle_postcache_err("%s: Invalid port idx %d port_id %#x",
			__func__, index, port_id);
		return -EINVAL;
	}
	eagle_postcache_dbg("%s: valid port idx %d for port_id %#x set to %i",
		__func__, index, port_id, copp_idx);
	_cidx[index] = copp_idx;

	mask = _get_dev_mask_for_pid(port_id);
	if (mask & _device_primary) {
		cidx = _get_cb_for_dev(_device_primary);
		if (cidx < 0) {
			eagle_postcache_err("%s: no cache for primary device %i found. Port id was 0x%X",
				__func__, _device_primary, port_id);
			return -EINVAL;
		}
	} else if (mask & _device_all) {
		cidx = _get_cb_for_dev(_device_all);
		if (cidx < 0) {
			eagle_postcache_err("%s: no cache for combo device %i found. Port id was 0x%X",
				__func__, _device_all, port_id);
			return -EINVAL;
		}
	} else {
		eagle_postcache_err("%s: port id 0x%X not for primary or combo device %i",
			__func__, port_id, _device_primary);
		return -EINVAL;
	}

NT_MODE_GOTO:
	offset = _c_bl[cidx][CBD_OFFSG] + _c_bl[cidx][CBD_OFFS2];
	cmd = _c_bl[cidx][CBD_CMD2];
	size = _c_bl[cidx][CBD_SZ2];

	/* check for integer overflow */
	if (offset > (UINT_MAX - size))
		err = -EINVAL;
	if ((_depc_size == 0) || !_depc || (err != 0) || (size == 0) ||
		(cmd == 0) || (offset + size) > _depc_size) {
		eagle_postcache_err("%s: primary device %i cache index %i port_id 0x%X general error - cache size = %u, cache ptr = %pK, offset = %u, size = %u, cmd = %i",
			__func__, _device_primary, cidx, port_id,
			_depc_size, _depc, offset, size, cmd);
		return -EINVAL;
	}

	if ((offset < (UINT_MAX - 24)) && ((offset + 24) < _depc_size))
		eagle_postcache_dbg("%s: first 6 integers %i %i %i %i %i %i",
			__func__, *((int *)&_depc[offset]),
			*((int *)&_depc[offset+4]),
			*((int *)&_depc[offset+8]),
			*((int *)&_depc[offset+12]),
			*((int *)&_depc[offset+16]),
			*((int *)&_depc[offset+20]));
	eagle_postcache_dbg("%s: sending full data block to port, with cache index = %d device mask 0x%X, port_id = 0x%X, param = 0x%X, offset = %u, and size = %u",
		__func__, cidx, _c_bl[cidx][CBD_DEV_MASK], port_id, cmd,
		offset, size);

	if (_ac_NT) {
		eagle_postcache_dbg("%s: NT Route detected", __func__);
		if (q6asm_dts_eagle_set(_getNTDeviceAC(), cmd, size,
					(void *)&_depc[offset],
					&_po_NT, MPST))
			eagle_postcache_err("%s: q6asm_dts_eagle_set failed with id = 0x%X and size = %u",
				__func__, cmd, size);
	} else if (adm_dts_eagle_set(port_id, copp_idx, cmd,
			      (void *)&_depc[offset], size) < 0)
		eagle_postcache_err("%s: adm_dts_eagle_set failed with id = 0x%X and size = %u",
			__func__, cmd, size);
	else
		eagle_postcache_dbg("%s: adm_dts_eagle_set succeeded with id = 0x%X and size = %u",
			 __func__, cmd, size);
	return 0;
}

static int _enable_post_get_control(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = _is_hpx_enabled;
	return 0;
}

static int _enable_post_put_control(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int idx = 0, be_index = 0, port_id, topology;
	int flag = ucontrol->value.integer.value[0];
	struct msm_pcm_routing_bdai_data msm_bedai;
	eagle_drv_dbg("%s: flag %d", __func__, flag);

	_is_hpx_enabled = flag ? true : false;
	msm_pcm_routing_acquire_lock();
	/* send cache postmix params when hpx is set On */
	for (be_index = 0; be_index < MSM_BACKEND_DAI_MAX; be_index++) {
		msm_pcm_routing_get_bedai_info(be_index, &msm_bedai);
		port_id = msm_bedai.port_id;
		if (!(((port_id == SLIMBUS_0_RX) ||
		      (port_id == RT_PROXY_PORT_001_RX)) &&
		      msm_bedai.active))
			continue;
		for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++) {
			topology = adm_get_topology_for_port_copp_idx(
								port_id, idx);
			if (topology ==
				ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX) {
				msm_dts_eagle_enable_adm(port_id, idx,
							 _is_hpx_enabled);
			}
		}
	}
	msm_pcm_routing_release_lock();
	return 0;
}

static const struct snd_kcontrol_new _hpx_enabled_controls[] = {
	SOC_SINGLE_EXT("Set HPX OnOff", SND_SOC_NOPM, 0, 1, 0,
	_enable_post_get_control, _enable_post_put_control)
};

/**
 * msm_dts_ion_memmap() - helper function to map ION memory
 * @po_:	Out of band memory structure used as memory.
 *
 * Assign already allocated ION memory for mapping it to dsp.
 *
 * Return: No return value.
 */
void msm_dts_ion_memmap(struct param_outband *po_)
{
	po_->size = ION_MEM_SIZE;
	po_->kvaddr = _po.kvaddr;
	po_->paddr = _po.paddr;
}

/**
 * msm_dts_eagle_enable_asm() - Enable/disable dts module
 * @ac:	Enable/disable module in ASM session associated with this audio client.
 * @enable:	Enable/disable the dts module.
 * @module:	module id.
 *
 * Enable/disable specified dts module id in asm.
 *
 * Return: Return failure if any.
 */
int msm_dts_eagle_enable_asm(struct audio_client *ac, u32 enable, int module)
{
	int ret = 0;
	eagle_enable_dbg("%s: enable = %i on module %i",
		 __func__, enable, module);
	_is_hpx_enabled = enable;
	ret = q6asm_dts_eagle_set(ac, AUDPROC_PARAM_ID_ENABLE,
				      sizeof(enable), &enable,
				      NULL, module);
	if (_is_hpx_enabled) {
		if (module == MPRE)
			_sendcache_pre(ac);
		else if (module == MPST)
			_sendcache_post(-1, 0, 0);
	}
	return ret;
}

/**
 * msm_dts_eagle_enable_adm() - Enable/disable dts module in adm
 * @port_id:	Send enable/disable param to this port id.
 * @copp_idx:	Send enable/disable param to the relevant copp.
 * @enable:	Enable/disable the dts module.
 *
 * Enable/disable dts module in adm.
 *
 * Return: Return failure if any.
 */
int msm_dts_eagle_enable_adm(int port_id, int copp_idx, u32 enable)
{
	int ret = 0;
	eagle_enable_dbg("%s: enable = %i", __func__, enable);
	_is_hpx_enabled = enable;
	ret = adm_dts_eagle_set(port_id, copp_idx, AUDPROC_PARAM_ID_ENABLE,
			     (char *)&enable, sizeof(enable));
	if (_is_hpx_enabled)
		_sendcache_post(port_id, copp_idx, MPST);
	return ret;
}

/**
 * msm_dts_eagle_add_controls() -  Add mixer control to Enable/Disable DTS HPX
 * @platform:	Add mixer controls to this platform.
 *
 * Add mixer control to Enable/Disable DTS HPX module in ADM.
 *
 * Return: No return value.
 */
void msm_dts_eagle_add_controls(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, _hpx_enabled_controls,
				      ARRAY_SIZE(_hpx_enabled_controls));
}

/**
 * msm_dts_eagle_set_stream_gain() -  Set stream gain to DTS Premix module
 * @ac:	Set stream gain to ASM session associated with this audio client.
 * @lgain:	Left gain value.
 * @rgain:	Right gain value.
 *
 * Set stream gain to DTS Premix module in ASM.
 *
 * Return: failure or success.
 */
int msm_dts_eagle_set_stream_gain(struct audio_client *ac, int lgain, int rgain)
{
	u32 i, val;
	s32 idx, err = 0;

	eagle_vol_dbg("%s: - entry: vol_cmd_cnt = %u, lgain = %i, rgain = %i",
		 __func__, _vol_cmd_cnt, lgain, rgain);

	if (_depc_size == 0) {
		eagle_vol_dbg("%s: driver cache not initialized", __func__);
		return -EINVAL;
	}

	for (i = 0; i < _vol_cmd_cnt; i++) {
		if (_vol_cmds_d[i].d[0] & 0x8000) {
			idx = (sizeof(struct dts_eagle_param_desc)/sizeof(int))
				+ (_vol_cmds_d[i].d[0] & 0x3FF);
			val = _fx_logN(((s32)(lgain+rgain)) << 2);
			val = ((long long)val * _log10_10_inv_x20) >> 16;
			_vol_cmds[i][idx] = (s32)clamp((int)(((long long)val *
						    _vol_cmds_d[i].d[1]) >> 16),
						    _vol_cmds_d[i].d[2],
						    _vol_cmds_d[i].d[3]);
			 eagle_vol_dbg("%s: loop %u cmd desc found %i, idx = %i. volume info: lgain = %i, rgain = %i, volume = %i (scale %i, min %i, max %i)",
				 __func__, i, _vol_cmds_d[i].d[0], idx, lgain,
				 rgain, _vol_cmds[i][idx], _vol_cmds_d[i].d[1],
				 _vol_cmds_d[i].d[2], _vol_cmds_d[i].d[3]);
		}
		idx = _get_cb_for_dev(_device_primary);
		if (idx < 0) {
			eagle_vol_err("%s: no cache for primary device %i found",
				__func__, _device_primary);
			return -EINVAL;
		}
		val = _c_bl[idx][CBD_OFFSG] + _vol_cmds[i][2];
		/* check for integer overflow */
		if (val > (UINT_MAX - _vol_cmds[i][1]))
			err = -EINVAL;
		if ((err != 0) || ((val + _vol_cmds[i][1]) > _depc_size)) {
			eagle_vol_err("%s: volume size (%u) + offset (%i) out of bounds %i",
				__func__, val, _vol_cmds[i][1], _depc_size);
			return -EINVAL;
		}
		memcpy((void *)&_depc[val], &_vol_cmds[i][4], _vol_cmds[i][1]);
		if (q6asm_dts_eagle_set(ac, _vol_cmds[i][0],
			_vol_cmds[i][1], (void *)&_depc[val], NULL, MPRE))
			eagle_vol_err("%s: loop %u - volume set failed with id 0x%X, size %i, offset %i, cmd_desc %i, scale %i, min %i, max %i, data(...) %i",
				__func__, i, _vol_cmds[i][0], _vol_cmds[i][1],
				_vol_cmds[i][2], _vol_cmds_d[i].d[0],
				_vol_cmds_d[i].d[1], _vol_cmds_d[i].d[2],
				_vol_cmds_d[i].d[3], _vol_cmds[i][4]);
		else
			eagle_vol_dbg("%s: loop %u - volume set succeeded with id 0x%X, size %i, offset %i, cmd_desc %i, scale %i, min %i, max %i, data(...) %i",
				 __func__, i, _vol_cmds[i][0], _vol_cmds[i][1],
				 _vol_cmds[i][2], _vol_cmds_d[i].d[0],
				 _vol_cmds_d[i].d[1], _vol_cmds_d[i].d[2],
				 _vol_cmds_d[i].d[3], _vol_cmds[i][4]);
	}
	return 0;
}

/**
 * msm_dts_eagle_handle_asm() - Set or Get params from ASM
 * @depd:	DTS Eagle Params structure.
 * @buf:	Buffer to get queried param value.
 * @for_pre:	For premix module or postmix module.
 * @get:	Getting param from DSP or setting param.
 * @ac:	Set/Get from ASM session associated with this audio client.
 * @po:	Out of band memory to set or get postmix params.
 *
 * Set or Get params from modules in ASM session.
 *
 * Return: Return failure if any.
 */
int msm_dts_eagle_handle_asm(struct dts_eagle_param_desc *depd, char *buf,
			     bool for_pre, bool get, struct audio_client *ac,
			     struct param_outband *po)
{
	struct dts_eagle_param_desc depd_ = {0};
	s32 ret = 0, isALSA = 0, err = 0, i, mod = for_pre ? MPRE : MPST;
	u32 offset;

	eagle_asm_dbg("%s: set/get asm", __func__);

	/* special handling for ALSA route, to accommodate 64 bit platforms */
	if (depd == NULL) {
		long *arg_ = (long *)buf;
		depd = &depd_;
		depd->id = (u32)*arg_++;
		depd->size = (u32)*arg_++;
		depd->offset = (s32)*arg_++;
		depd->device = (u32)*arg_++;
		buf = (char *)arg_;
		isALSA = 1;
	}

	if (depd->size & 1) {
		eagle_asm_err("%s: parameter size %u is not a multiple of 2",
			__func__, depd->size);
		return -EINVAL;
	}

	if (get) {
		void *buf_, *buf_m = NULL;
		eagle_asm_dbg("%s: get requested", __func__);
		if (depd->offset == -1) {
			eagle_asm_dbg("%s: get from dsp requested", __func__);
			if (depd->size > 0 && depd->size <= DEPC_MAX_SIZE) {
				buf_ = buf_m = vzalloc(depd->size);
			} else {
				eagle_asm_err("%s: get size %u invalid",
					      __func__, depd->size);
				return -EINVAL;
			}
			if (!buf_m) {
				eagle_asm_err("%s: out of memory", __func__);
				return -ENOMEM;
			} else if (q6asm_dts_eagle_get(ac, depd->id,
						       depd->size, buf_m,
						       po, mod) < 0) {
				eagle_asm_err("%s: asm get failed", __func__);
				ret = -EFAULT;
				goto DTS_EAGLE_IOCTL_GET_PARAM_PRE_EXIT;
			}
			eagle_asm_dbg("%s: get result: param id 0x%x value %d size %u",
				 __func__, depd->id, *(int *)buf_m, depd->size);
		} else {
			s32 tgt = _get_cb_for_dev(depd->device);
			if (tgt < 0) {
				eagle_asm_err("%s: no cache for device %u found",
					__func__, depd->device);
				return -EINVAL;
			}
			offset = _c_bl[tgt][CBD_OFFSG] + depd->offset;
			/* check for integer overflow */
			if (offset > (UINT_MAX - depd->size))
				err = -EINVAL;
			if ((err != 0) || (offset + depd->size) > _depc_size) {
				eagle_asm_err("%s: invalid size %u and/or offset %u",
					__func__, depd->size, offset);
				return -EINVAL;
			}
			buf_ = (u32 *)&_depc[offset];
		}
		if (isALSA) {
			if (depd->size == 2) {
				*(long *)buf = (long)*(__u16 *)buf_;
				eagle_asm_dbg("%s: asm out 16 bit value %li",
						__func__, *(long *)buf);
			} else {
				s32 *pbuf = (s32 *)buf_;
				long *bufl = (long *)buf;
				for (i = 0; i < (depd->size >> 2); i++) {
					*bufl++ = (long)*pbuf++;
					eagle_asm_dbg("%s: asm out value %li",
							 __func__, *(bufl-1));
				}
			}
		} else {
			memcpy(buf, buf_, depd->size);
		}
DTS_EAGLE_IOCTL_GET_PARAM_PRE_EXIT:
		vfree(buf_m);
		return (int)ret;
	} else {
		s32 tgt = _get_cb_for_dev(depd->device);
		if (tgt < 0) {
			eagle_asm_err("%s: no cache for device %u found",
				__func__, depd->device);
			return -EINVAL;
		}
		offset = _c_bl[tgt][CBD_OFFSG] + depd->offset;
		/* check for integer overflow */
		if (offset > (UINT_MAX - depd->size))
			err = -EINVAL;
		if ((err != 0) || ((offset + depd->size) > _depc_size)) {
			eagle_asm_err("%s: invalid size %u and/or offset %u for parameter (cache is size %u)",
				__func__, depd->size, offset, _depc_size);
			return -EINVAL;
		}
		if (isALSA) {
			if (depd->size == 2) {
				*(__u16 *)&_depc[offset] = (__u16)*(long *)buf;
				eagle_asm_dbg("%s: asm in 16 bit value %li",
						__func__, *(long *)buf);
			} else {
				s32 *pbuf = (s32 *)&_depc[offset];
				long *bufl = (long *)buf;
				for (i = 0; i < (depd->size >> 2); i++) {
					*pbuf++ = (s32)*bufl++;
					eagle_asm_dbg("%s: asm in value %i",
							__func__, *(pbuf-1));
				}
			}
		} else {
			memcpy(&_depc[offset], buf, depd->size);
		}
		eagle_asm_dbg("%s: param info: param = 0x%X, size = %u, offset = %i, device = %u, cache block %i, global offset = %u, first bytes as integer = %i",
			__func__, depd->id, depd->size, depd->offset,
			depd->device,
			tgt, offset, *(int *)&_depc[offset]);
		if (q6asm_dts_eagle_set(ac, depd->id, depd->size,
					(void *)&_depc[offset], po, mod))
			eagle_asm_err("%s: q6asm_dts_eagle_set failed with id = 0x%X, size = %u, offset = %d",
				__func__, depd->id, depd->size, depd->offset);
		else
			eagle_asm_dbg("%s: q6asm_dts_eagle_set succeeded with id = 0x%X, size = %u, offset = %d",
				 __func__, depd->id, depd->size, depd->offset);
	}
	return (int)ret;
}

/**
 * msm_dts_eagle_handle_adm() - Set or Get params from ADM
 * @depd:	DTS Eagle Params structure used to set or get.
 * @buf:	Buffer to get queried param value in NT mode.
 * @for_pre:	For premix module or postmix module.
 * @get:	Getting param from DSP or setting param.
 *
 * Set or Get params from modules in ADM session.
 *
 * Return: Return failure if any.
 */
int msm_dts_eagle_handle_adm(struct dts_eagle_param_desc *depd, char *buf,
			     bool for_pre, bool get)
{
	u32 pid = _get_pid_from_dev(depd->device), cidx;
	s32 ret = 0;

	eagle_adm_dbg("%s: set/get adm", __func__);

	if (_isNTDevice(depd->device)) {
		eagle_adm_dbg("%s: NT Route detected", __func__);
		ret = msm_dts_eagle_handle_asm(depd, buf, for_pre, get,
					       _getNTDeviceAC(), &_po_NT);
		if (ret < 0)
			eagle_adm_err("%s: NT Route set failed with id = 0x%X, size = %u, offset = %i, device = %u",
				__func__, depd->id, depd->size, depd->offset,
				depd->device);
	} else if (get) {
		cidx = adm_validate_and_get_port_index(pid);
		eagle_adm_dbg("%s: get from qdsp requested (port id 0x%X)",
			 __func__, pid);
		if (adm_dts_eagle_get(pid, _cidx[cidx], depd->id,
				      buf, depd->size) < 0) {
			eagle_adm_err("%s: get from qdsp via adm with port id 0x%X failed",
				 __func__, pid);
			return -EFAULT;
		}
	} else if (_is_port_open_and_eagle(pid)) {
		cidx = adm_validate_and_get_port_index(pid);
		eagle_adm_dbg("%s: adm_dts_eagle_set called with id = 0x%X, size = %u, offset = %i, device = %u, port id = %u, copp index = %u",
				__func__, depd->id, depd->size, depd->offset,
				depd->device, pid, cidx);
		ret = adm_dts_eagle_set(pid, _cidx[cidx], depd->id,
					(void *)buf, depd->size);
		if (ret < 0)
			eagle_adm_err("%s: adm_dts_eagle_set failed", __func__);
		else
			eagle_adm_dbg("%s: adm_dts_eagle_set succeeded",
				__func__);
	} else {
		ret = -EINVAL;
		eagle_adm_dbg("%s: port id 0x%X not active or not Eagle",
			 __func__, pid);
	}
	return (int)ret;
}

/**
 * msm_dts_eagle_ioctl() - ioctl handler function
 * @cmd:	cmd to handle.
 * @arg:	argument to the cmd.
 *
 * Handle DTS Eagle ioctl cmds.
 *
 * Return: Return failure if any.
 */
int msm_dts_eagle_ioctl(unsigned int cmd, unsigned long arg)
{
	s32 ret = 0;
	switch (cmd) {
	case DTS_EAGLE_IOCTL_GET_CACHE_SIZE: {
		eagle_ioctl_info("%s: called with control 0x%X (get param cache size)",
			__func__, cmd);
		if (copy_to_user((void *)arg, &_depc_size,
				 sizeof(_depc_size))) {
			eagle_ioctl_err("%s: error writing size", __func__);
			return -EFAULT;
		}
		break;
	}
	case DTS_EAGLE_IOCTL_SET_CACHE_SIZE: {
		u32 size = 0;
		eagle_ioctl_info("%s: called with control 0x%X (allocate param cache)",
			__func__, cmd);
		if (copy_from_user((void *)&size, (void *)arg, sizeof(size))) {
			eagle_ioctl_err("%s: error copying size (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, &size, sizeof(size));
			return -EFAULT;
		} else if (size > DEPC_MAX_SIZE) {
			eagle_ioctl_err("%s: cache size %u not allowed (min 0, max %u)",
				__func__, size, DEPC_MAX_SIZE);
			return -EINVAL;
		}
		if (_depc) {
			eagle_ioctl_dbg("%s: previous param cache of size %u freed",
				__func__, _depc_size);
			_depc_size = 0;
			vfree(_depc);
			_depc = NULL;
		}
		if (size)
			_depc = vzalloc(size);
		else
			eagle_ioctl_dbg("%s: %u bytes requested for param cache, nothing allocated",
				__func__, size);
		if (_depc) {
			eagle_ioctl_dbg("%s: %u bytes allocated for param cache",
				__func__, size);
			_depc_size = size;
		} else {
			eagle_ioctl_err("%s: error allocating param cache (vzalloc failed on %u bytes)",
				__func__, size);
			_depc_size = 0;
			return -ENOMEM;
		}
		break;
	}
	case DTS_EAGLE_IOCTL_GET_PARAM: {
		struct dts_eagle_param_desc depd;
		s32 for_pre = 0, get_from_core = 0, err = 0;
		u32 offset;
		void *buf, *buf_m = NULL;
		eagle_ioctl_info("%s: control 0x%X (get param)",
			__func__, cmd);
		if (copy_from_user((void *)&depd, (void *)arg, sizeof(depd))) {
			eagle_ioctl_err("%s: error copying dts_eagle_param_desc (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, &depd, sizeof(depd));
			return -EFAULT;
		}
		if (depd.device & DTS_EAGLE_FLAG_IOCTL_PRE) {
			eagle_ioctl_dbg("%s: using for premix", __func__);
			for_pre = 1;
		}
		if (depd.device & DTS_EAGLE_FLAG_IOCTL_GETFROMCORE) {
			eagle_ioctl_dbg("%s: 'get from core' requested",
				__func__);
			get_from_core = 1;
			depd.offset = -1;
		}
		depd.device &= DTS_EAGLE_FLAG_IOCTL_MASK;
		if (depd.offset == -1) {
			if (depd.size > 0 && depd.size <= DEPC_MAX_SIZE) {
				buf = buf_m = vzalloc(depd.size);
			} else {
				eagle_ioctl_err("%s: get size %u invalid",
						__func__, depd.size);
				return -EINVAL;
			}
			if (!buf_m) {
				eagle_ioctl_err("%s: out of memory", __func__);
				return -ENOMEM;
			}
			if (get_from_core)
				ret = core_dts_eagle_get(depd.id, depd.size,
							 buf);
			else
				ret = msm_dts_eagle_handle_adm(&depd, buf,
								for_pre, true);
		} else {
			s32 cb = _get_cb_for_dev(depd.device);
			if (cb < 0) {
				eagle_ioctl_err("%s: no cache for device %u found",
					__func__, depd.device);
				return -EINVAL;
			}
			offset = _c_bl[cb][CBD_OFFSG] + depd.offset;
			/* check for integer overflow */
			if (offset > (UINT_MAX - depd.size))
				err = -EINVAL;
			if ((err != 0) ||
			    ((offset + depd.size) > _depc_size)) {
				eagle_ioctl_err("%s: invalid size %u and/or offset %u",
					__func__, depd.size, offset);
				return -EINVAL;
			}
			buf = (void *)&_depc[offset];
		}
		if (ret < 0)
			eagle_ioctl_err("%s: error %i getting data", __func__,
				ret);
		else if (copy_to_user((void *)(((char *)arg)+sizeof(depd)),
						  buf, depd.size)) {
			eagle_ioctl_err("%s: error copying get data", __func__);
			ret = -EFAULT;
		}
		vfree(buf_m);
		break;
	}
	case DTS_EAGLE_IOCTL_SET_PARAM: {
		struct dts_eagle_param_desc depd;
		s32 just_set_cache = 0, for_pre = 0, err = 0;
		u32 offset;
		s32 tgt;
		eagle_ioctl_info("%s: control 0x%X (set param)",
			__func__, cmd);
		if (copy_from_user((void *)&depd, (void *)arg, sizeof(depd))) {
			eagle_ioctl_err("%s: error copying dts_eagle_param_desc (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, &depd, sizeof(depd));
			return -EFAULT;
		}
		if (depd.device & DTS_EAGLE_FLAG_IOCTL_PRE) {
			eagle_ioctl_dbg("%s: using for premix", __func__);
			for_pre = 1;
		}
		if (depd.device & DTS_EAGLE_FLAG_IOCTL_JUSTSETCACHE) {
			eagle_ioctl_dbg("%s: 'just set cache' requested",
				__func__);
			just_set_cache = 1;
		}
		depd.device &= DTS_EAGLE_FLAG_IOCTL_MASK;
		tgt = _get_cb_for_dev(depd.device);
		if (tgt < 0) {
			eagle_ioctl_err("%s: no cache for device %u found",
				__func__, depd.device);
			return -EINVAL;
		}
		offset = _c_bl[tgt][CBD_OFFSG] + depd.offset;
		/* check for integer overflow */
		if (offset > (UINT_MAX - depd.size))
			err = -EINVAL;
		if ((err != 0) || ((offset + depd.size) > _depc_size)) {
			eagle_ioctl_err("%s: invalid size %u and/or offset %u for parameter (target cache block %i with offset %i, global cache is size %u)",
				__func__, depd.size, offset, tgt,
				_c_bl[tgt][CBD_OFFSG], _depc_size);
			return -EINVAL;
		}
		if (copy_from_user((void *)&_depc[offset],
				   (void *)(((char *)arg)+sizeof(depd)),
					depd.size)) {
			eagle_ioctl_err("%s: error copying param to cache (src:%pK, tgt:%pK, size:%u)",
				__func__, ((char *)arg)+sizeof(depd),
				&_depc[offset], depd.size);
			return -EFAULT;
		}
		eagle_ioctl_dbg("%s: param info: param = 0x%X, size = %u, offset = %i, device = %u, cache block %i, global offset = %u, first bytes as integer = %i",
			__func__, depd.id, depd.size, depd.offset,
			depd.device, tgt, offset, *(int *)&_depc[offset]);
		if (!just_set_cache) {
			ret = msm_dts_eagle_handle_adm(&depd, &_depc[offset],
						       for_pre, false);
		}
		break;
	}
	case DTS_EAGLE_IOCTL_SET_CACHE_BLOCK: {
		u32 b_[CBD_COUNT+1], *b = &b_[1], cb;
		eagle_ioctl_info("%s: with control 0x%X (set param cache block)",
			 __func__, cmd);
		if (copy_from_user((void *)b_, (void *)arg, sizeof(b_))) {
			eagle_ioctl_err("%s: error copying cache block data (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, b_, sizeof(b_));
			return -EFAULT;
		}
		cb = b_[0];
		if (cb >= CB_COUNT) {
			eagle_ioctl_err("%s: cache block %u out of range (max %u)",
				__func__, cb, CB_COUNT-1);
			return -EINVAL;
		}
		eagle_ioctl_dbg("%s: cache block %i set: devices 0x%X, global offset %i, offsets 1:%u 2:%u 3:%u, cmds/sizes 0:0x%X %u 1:0x%X %u 2:0x%X %u 3:0x%X %u",
		__func__, cb, _c_bl[cb][CBD_DEV_MASK], _c_bl[cb][CBD_OFFSG],
		_c_bl[cb][CBD_OFFS1], _c_bl[cb][CBD_OFFS2],
		_c_bl[cb][CBD_OFFS3], _c_bl[cb][CBD_CMD0], _c_bl[cb][CBD_SZ0],
		_c_bl[cb][CBD_CMD1], _c_bl[cb][CBD_SZ1], _c_bl[cb][CBD_CMD2],
		_c_bl[cb][CBD_SZ2], _c_bl[cb][CBD_CMD3], _c_bl[cb][CBD_SZ3]);
		if ((b[CBD_OFFSG]+b[CBD_OFFS1]+b[CBD_SZ1]) > _depc_size ||
			(b[CBD_OFFSG]+b[CBD_OFFS2]+b[CBD_SZ2]) > _depc_size ||
			(b[CBD_OFFSG]+b[CBD_OFFS3]+b[CBD_SZ3]) > _depc_size) {
			eagle_ioctl_err("%s: cache block bounds out of range",
					__func__);
			return -EINVAL;
		}
		memcpy(_c_bl[cb], b, sizeof(_c_bl[cb]));
		break;
	}
	case DTS_EAGLE_IOCTL_SET_ACTIVE_DEVICE: {
		u32 data[2];
		eagle_ioctl_dbg("%s: with control 0x%X (set active device)",
			 __func__, cmd);
		if (copy_from_user((void *)data, (void *)arg, sizeof(data))) {
			eagle_ioctl_err("%s: error copying active device data (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, data, sizeof(data));
			return -EFAULT;
		}
		if (data[1] != 0) {
			_device_primary = data[0];
			eagle_ioctl_dbg("%s: primary device %i", __func__,
				 data[0]);
		} else {
			_device_all = data[0];
			eagle_ioctl_dbg("%s: all devices 0x%X", __func__,
				 data[0]);
		}
		break;
	}
	case DTS_EAGLE_IOCTL_GET_LICENSE: {
		u32 target = 0, size = 0;
		s32 size_only;
		eagle_ioctl_dbg("%s: with control 0x%X (get license)",
			 __func__, cmd);
		if (copy_from_user((void *)&target, (void *)arg,
				   sizeof(target))) {
			eagle_ioctl_err("%s: error reading license index. (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, &target, sizeof(target));
			return -EFAULT;
		}
		size_only = target & (1<<31) ? 1 : 0;
		target &= 0x7FFFFFFF;
		if (target >= SEC_BLOB_MAX_CNT) {
			eagle_ioctl_err("%s: license index %u out of bounds (max index is %i)",
				   __func__, target, SEC_BLOB_MAX_CNT);
			return -EINVAL;
		}
		if (_sec_blob[target] == NULL) {
			eagle_ioctl_err("%s: license index %u never initialized",
				   __func__, target);
			return -EINVAL;
		}
		size = ((u32 *)_sec_blob[target])[0];
		if ((size == 0) || (size > SEC_BLOB_MAX_SIZE)) {
			eagle_ioctl_err("%s: license size %u for index %u invalid (min size is 1, max size is %u)",
				   __func__, size, target, SEC_BLOB_MAX_SIZE);
			return -EINVAL;
		}
		if (size_only) {
			eagle_ioctl_dbg("%s: reporting size of license data only",
					__func__);
			if (copy_to_user((void *)(((char *)arg)+sizeof(target)),
				 (void *)&size, sizeof(size))) {
				eagle_ioctl_err("%s: error copying license size",
						__func__);
				return -EFAULT;
			}
		} else if (copy_to_user((void *)(((char *)arg)+sizeof(target)),
			   (void *)&(((s32 *)_sec_blob[target])[1]), size)) {
			eagle_ioctl_err("%s: error copying license data",
				__func__);
			return -EFAULT;
		} else
			eagle_ioctl_info("%s: license file %u bytes long from license index %u returned to user",
				  __func__, size, target);
		break;
	}
	case DTS_EAGLE_IOCTL_SET_LICENSE: {
		u32 target[2] = {0, 0};
		eagle_ioctl_dbg("%s: control 0x%X (set license)", __func__,
				cmd);
		if (copy_from_user((void *)target, (void *)arg,
				   sizeof(target))) {
			eagle_ioctl_err("%s: error reading license index (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, target, sizeof(target));
			return -EFAULT;
		}
		if (target[0] >= SEC_BLOB_MAX_CNT) {
			eagle_ioctl_err("%s: license index %u out of bounds (max index is %u)",
				   __func__, target[0], SEC_BLOB_MAX_CNT-1);
			return -EINVAL;
		}
		if (target[1] == 0) {
			eagle_ioctl_dbg("%s: request to free license index %u",
				 __func__, target[0]);
			kfree(_sec_blob[target[0]]);
			_sec_blob[target[0]] = NULL;
			break;
		}
		if ((target[1] == 0) || (target[1] >= SEC_BLOB_MAX_SIZE)) {
			eagle_ioctl_err("%s: license size %u for index %u invalid (min size is 1, max size is %u)",
				__func__, target[1], target[0],
				SEC_BLOB_MAX_SIZE);
			return -EINVAL;
		}
		if (_sec_blob[target[0]] != NULL) {
			if (((u32 *)_sec_blob[target[0]])[1] != target[1]) {
				eagle_ioctl_dbg("%s: request new size for already allocated license index %u",
					 __func__, target[0]);
				kfree(_sec_blob[target[0]]);
				_sec_blob[target[0]] = NULL;
			}
		}
		eagle_ioctl_dbg("%s: allocating %u bytes for license index %u",
				__func__, target[1], target[0]);
		_sec_blob[target[0]] = kzalloc(target[1] + 4, GFP_KERNEL);
		if (!_sec_blob[target[0]]) {
			eagle_ioctl_err("%s: error allocating license index %u (kzalloc failed on %u bytes)",
					__func__, target[0], target[1]);
			return -ENOMEM;
		}
		((u32 *)_sec_blob[target[0]])[0] = target[1];
		if (copy_from_user(
				(void *)&(((u32 *)_sec_blob[target[0]])[1]),
				(void *)(((char *)arg)+sizeof(target)),
				target[1])) {
			eagle_ioctl_err("%s: error copying license to index %u, size %u (src:%pK, tgt:%pK, size:%u)",
					__func__, target[0], target[1],
					((char *)arg)+sizeof(target),
					&(((u32 *)_sec_blob[target[0]])[1]),
					target[1]);
			return -EFAULT;
		} else
			eagle_ioctl_info("%s: license file %u bytes long copied to index license index %u",
				  __func__, target[1], target[0]);
		break;
	}
	case DTS_EAGLE_IOCTL_SEND_LICENSE: {
		u32 target = 0;
		eagle_ioctl_dbg("%s: control 0x%X (send license)", __func__,
				cmd);
		if (copy_from_user((void *)&target, (void *)arg,
				   sizeof(target))) {
			eagle_ioctl_err("%s: error reading license index (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, &target, sizeof(target));
			return -EFAULT;
		}
		if (target >= SEC_BLOB_MAX_CNT) {
			eagle_ioctl_err("%s: license index %u out of bounds (max index is %i)",
					__func__, target, SEC_BLOB_MAX_CNT-1);
			return -EINVAL;
		}
		if (!_sec_blob[target] ||
		    ((u32 *)_sec_blob[target])[0] == 0) {
			eagle_ioctl_err("%s: license index %u is invalid",
				__func__, target);
			return -EINVAL;
		}
		if (core_dts_eagle_set(((s32 *)_sec_blob[target])[0],
				(char *)&((s32 *)_sec_blob[target])[1]) < 0)
			eagle_ioctl_err("%s: core_dts_eagle_set failed with id = %u",
				__func__, target);
		else
			eagle_ioctl_info("%s: core_dts_eagle_set succeeded with id = %u",
				 __func__, target);
		break;
	}
	case DTS_EAGLE_IOCTL_SET_VOLUME_COMMANDS: {
		s32 spec = 0;
		eagle_ioctl_info("%s: control 0x%X (set volume commands)",
				__func__, cmd);
		if (copy_from_user((void *)&spec, (void *)arg,
					sizeof(spec))) {
			eagle_ioctl_err("%s: error reading volume command specifier (src:%pK, tgt:%pK, size:%zu)",
				__func__, (void *)arg, &spec, sizeof(spec));
			return -EFAULT;
		}
		if (spec & 0x80000000) {
			u32 idx = (spec & 0x0000F000) >> 12;
			s32 size = spec & 0x00000FFF;
			eagle_ioctl_dbg("%s: setting volume command %i size: %i",
				__func__, idx, size);
			if (idx >= _vol_cmd_cnt) {
				eagle_ioctl_err("%s: volume command index %u out of bounds (only %u allocated)",
					__func__, idx, _vol_cmd_cnt);
				return -EINVAL;
			}
			if (_volume_cmds_alloc2(idx, size) < 0) {
				eagle_ioctl_err("%s: error allocating memory for volume controls",
						__func__);
				return -ENOMEM;
			}
			if (copy_from_user((void *)&_vol_cmds_d[idx],
					(void *)(((char *)arg) + sizeof(int)),
					sizeof(struct vol_cmds_d))) {
				eagle_ioctl_err("%s: error reading volume command descriptor (src:%pK, tgt:%pK, size:%zu)",
					__func__, ((char *)arg) + sizeof(int),
					&_vol_cmds_d[idx],
					sizeof(struct vol_cmds_d));
				return -EFAULT;
			}
			eagle_ioctl_dbg("%s: setting volume command %i spec (size %zu): %i %i %i %i",
				  __func__, idx, sizeof(struct vol_cmds_d),
				  _vol_cmds_d[idx].d[0], _vol_cmds_d[idx].d[1],
				  _vol_cmds_d[idx].d[2], _vol_cmds_d[idx].d[3]);
			if (copy_from_user((void *)_vol_cmds[idx],
					(void *)(((char *)arg) + (sizeof(int) +
					sizeof(struct vol_cmds_d))), size)) {
				eagle_ioctl_err("%s: error reading volume command string (src:%pK, tgt:%pK, size:%i)",
					__func__, ((char *)arg) + (sizeof(int) +
					sizeof(struct vol_cmds_d)),
					_vol_cmds[idx], size);
				return -EFAULT;
			}
		} else {
			eagle_ioctl_dbg("%s: setting volume command size",
					__func__);
			if (spec < 0 || spec > VOL_CMD_CNT_MAX) {
				eagle_ioctl_err("%s: volume command count %i out of bounds (min 0, max %i)",
					__func__, spec, VOL_CMD_CNT_MAX);
				return -EINVAL;
			} else if (spec == 0) {
				eagle_ioctl_dbg("%s: request to free volume commands",
						__func__);
				_volume_cmds_free();
				break;
			}
			eagle_ioctl_dbg("%s: setting volume command size requested = %i",
				  __func__, spec);
			if (_volume_cmds_alloc1(spec) < 0) {
				eagle_ioctl_err("%s: error allocating memory for volume controls",
						__func__);
				return -ENOMEM;
			}
		}
		break;
	}
	default: {
		eagle_ioctl_err("%s: control 0x%X (invalid control)",
			 __func__, cmd);
		ret = -EINVAL;
	}
	}
	return (int)ret;
}

/**
 * msm_dts_eagle_compat_ioctl() - To handle 32bit to 64bit ioctl compatibility
 * @cmd:	cmd to handle.
 * @arg:	argument to the cmd.
 *
 * Handle DTS Eagle ioctl cmds from 32bit userspace.
 *
 * Return: Return failure if any.
 */
#ifdef CONFIG_COMPAT
int msm_dts_eagle_compat_ioctl(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case DTS_EAGLE_IOCTL_GET_CACHE_SIZE32:
		cmd = DTS_EAGLE_IOCTL_GET_CACHE_SIZE;
		break;
	case DTS_EAGLE_IOCTL_SET_CACHE_SIZE32:
		cmd = DTS_EAGLE_IOCTL_SET_CACHE_SIZE;
		break;
	case DTS_EAGLE_IOCTL_GET_PARAM32:
		cmd = DTS_EAGLE_IOCTL_GET_PARAM;
		break;
	case DTS_EAGLE_IOCTL_SET_PARAM32:
		cmd = DTS_EAGLE_IOCTL_SET_PARAM;
		break;
	case DTS_EAGLE_IOCTL_SET_CACHE_BLOCK32:
		cmd = DTS_EAGLE_IOCTL_SET_CACHE_BLOCK;
		break;
	case DTS_EAGLE_IOCTL_SET_ACTIVE_DEVICE32:
		cmd = DTS_EAGLE_IOCTL_SET_ACTIVE_DEVICE;
		break;
	case DTS_EAGLE_IOCTL_GET_LICENSE32:
		cmd = DTS_EAGLE_IOCTL_GET_LICENSE;
		break;
	case DTS_EAGLE_IOCTL_SET_LICENSE32:
		cmd = DTS_EAGLE_IOCTL_SET_LICENSE;
		break;
	case DTS_EAGLE_IOCTL_SEND_LICENSE32:
		cmd = DTS_EAGLE_IOCTL_SEND_LICENSE;
		break;
	case DTS_EAGLE_IOCTL_SET_VOLUME_COMMANDS32:
		cmd = DTS_EAGLE_IOCTL_SET_VOLUME_COMMANDS;
		break;
	default:
		break;
	}
	return msm_dts_eagle_ioctl(cmd, arg);
}
#endif
/**
 * msm_dts_eagle_init_pre() - Initialize DTS premix module
 * @ac:	Initialize premix module in the ASM session.
 *
 * Initialize DTS premix module on provided ASM session
 *
 * Return: Return failure if any.
 */
int msm_dts_eagle_init_pre(struct audio_client *ac)
{
	return msm_dts_eagle_enable_asm(ac, _is_hpx_enabled,
				 AUDPROC_MODULE_ID_DTS_HPX_PREMIX);
}

/**
 * msm_dts_eagle_deinit_pre() - Deinitialize DTS premix module
 * @ac:	Deinitialize premix module in the ASM session.
 *
 * Deinitialize DTS premix module on provided ASM session
 *
 * Return: Currently does nothing so 0.
 */
int msm_dts_eagle_deinit_pre(struct audio_client *ac)
{
	return 0;
}

/**
 * msm_dts_eagle_init_post() - Initialize DTS postmix module
 * @port_id:	Port id for the ADM session.
 * @copp_idx:	Copp idx for the ADM session.
 *
 * Initialize DTS postmix module on ADM session
 *
 * Return: Return failure if any.
 */
int msm_dts_eagle_init_post(int port_id, int copp_idx)
{
	return msm_dts_eagle_enable_adm(port_id, copp_idx, _is_hpx_enabled);
}

/**
 * msm_dts_eagle_deinit_post() - Deinitialize DTS postmix module
 * @port_id:	Port id for the ADM session.
 * @topology:	Topology in use.
 *
 * Deinitialize DTS postmix module on ADM session
 *
 * Return: Currently does nothing so 0.
 */
int msm_dts_eagle_deinit_post(int port_id, int topology)
{
	return 0;
}

/**
 * msm_dts_eagle_init_master_module() - Initialize both DTS modules
 * @ac:	Initialize modules in the ASM session.
 *
 * Initialize DTS modules on ASM session
 *
 * Return: Success.
 */
int msm_dts_eagle_init_master_module(struct audio_client *ac)
{
	_set_audioclient(ac);
	msm_dts_eagle_enable_asm(ac, _is_hpx_enabled,
				 AUDPROC_MODULE_ID_DTS_HPX_PREMIX);
	msm_dts_eagle_enable_asm(ac, _is_hpx_enabled,
				 AUDPROC_MODULE_ID_DTS_HPX_POSTMIX);
	return 0;
}

/**
 * msm_dts_eagle_deinit_master_module() - Deinitialize both DTS modules
 * @ac:	Deinitialize modules in the ASM session.
 *
 * Deinitialize DTS modules on ASM session
 *
 * Return: Success.
 */
int msm_dts_eagle_deinit_master_module(struct audio_client *ac)
{
	msm_dts_eagle_deinit_pre(ac);
	msm_dts_eagle_deinit_post(-1, 0);
	_clear_audioclient();
	return 0;
}

/**
 * msm_dts_eagle_is_hpx_on() - Check if HPX effects are On
 *
 * Check if HPX effects are On
 *
 * Return: On/Off.
 */
int msm_dts_eagle_is_hpx_on(void)
{
	return _is_hpx_enabled;
}

/**
 * msm_dts_eagle_pcm_new() - Create hwdep node
 * @runtime:	snd_soc_pcm_runtime structure.
 *
 * Create hwdep node
 *
 * Return: Success.
 */
int msm_dts_eagle_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	if (!_ref_cnt++) {
		_init_cb_descs();
		_reg_ion_mem();
	}
	return 0;
}

/**
 * msm_dts_eagle_pcm_free() - remove hwdep node
 * @runtime:	snd_soc_pcm_runtime structure.
 *
 * Remove hwdep node
 *
 * Return: void.
 */
void msm_dts_eagle_pcm_free(struct snd_pcm *pcm)
{
	if (!--_ref_cnt)
		_unreg_ion_mem();
	vfree(_depc);
}

MODULE_DESCRIPTION("DTS EAGLE platform driver");
MODULE_LICENSE("GPL v2");
