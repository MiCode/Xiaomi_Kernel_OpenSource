/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <drm/drm_edid.h>
#include <drm/drm_dp_helper.h>
#include "dp_debug.h"
#include "dp_mst_sim.h"

struct dp_sim_dpcd_reg {
	struct list_head head;
	u32 addr;
	u8 val;
};

#define DP_SIM_BRIDGE_PRIV_FLAG (1 << 31)

#define MAX_BUILTIN_DPCD_ADDR SZ_2K
#define MAX_MST_PORT 8

struct dp_sim_device {
	struct device *dev;
	struct dp_aux_bridge bridge;
	void *host_dev;
	int (*hpd_cb)(void *, bool, bool);

	struct mutex lock;
	const char *label;

	struct dentry *debugfs_dir;
	struct dentry *debugfs_edid_dir;

	u8 dpcd_reg[MAX_BUILTIN_DPCD_ADDR];
	struct list_head dpcd_reg_list;
	u32 dpcd_write_addr;
	u32 dpcd_write_size;

	u32 link_training_cnt;
	u32 link_training_remain;
	u32 link_training_lane_cnt;
	bool link_training_mismatch;

	struct dp_mst_sim_port *ports;
	u32 port_num;
	u32 current_port_num;
	u32 sim_mode;

	u32 edid_seg;
	u32 edid_seg_int;
	u32 edid_addr;

	bool skip_edid;
	bool skip_dpcd;
	bool skip_link_training;
	bool skip_config;
	bool skip_hpd;
	bool skip_mst;
};

struct dp_sim_debug_edid_entry {
	struct dp_sim_device *sim_dev;
	u32 index;
};

#define to_dp_sim_dev(x) container_of((x), struct dp_sim_device, bridge)

static const struct dp_mst_sim_port output_port = {
	false, false, true, 3, false, 0x12,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	0, 0, 2520, 2520, NULL, 0
};

#ifdef CONFIG_DYNAMIC_DEBUG
static void dp_sim_aux_hex_dump(struct drm_dp_aux_msg *msg)
{
	char prefix[64];
	int i, linelen, remaining = msg->size;
	const int rowsize = 16;
	u8 linebuf[64];

	snprintf(prefix, sizeof(prefix), "%s %s %4xh(%2zu): ",
		(msg->request & DP_AUX_I2C_MOT) ? "I2C" : "NAT",
		(msg->request & DP_AUX_I2C_READ) ? "RD" : "WR",
		msg->address, msg->size);

	for (i = 0; i < msg->size; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(msg->buffer + i, linelen, rowsize, 1,
			linebuf, sizeof(linebuf), false);

		DP_DEBUG("%s%s\n", prefix, linebuf);
	}
}
#else
static void dp_sim_aux_hex_dump(struct drm_dp_aux_msg *msg)
{
}
#endif

static int dp_sim_register_hpd(struct dp_aux_bridge *bridge,
	int (*hpd_cb)(void *, bool, bool), void *dev)
{
	struct dp_sim_device *sim_dev = to_dp_sim_dev(bridge);

	sim_dev->host_dev = dev;
	sim_dev->hpd_cb = hpd_cb;

	if (sim_dev->skip_hpd)
		hpd_cb(dev, true, false);

	return 0;
}

static u8 dp_sim_read_dpcd(struct dp_sim_device *sim_dev,
		u32 addr)
{
	struct dp_sim_dpcd_reg *reg;

	if (addr < MAX_BUILTIN_DPCD_ADDR) {
		return sim_dev->dpcd_reg[addr];
	} else {
		list_for_each_entry(reg, &sim_dev->dpcd_reg_list, head) {
			if (reg->addr == addr)
				return reg->val;
		}
	}

	return 0;
}

static void dp_sim_write_dpcd(struct dp_sim_device *sim_dev,
		u32 addr, u8 val)
{
	struct dp_sim_dpcd_reg *dpcd_reg;

	if (addr < MAX_BUILTIN_DPCD_ADDR) {
		sim_dev->dpcd_reg[addr] = val;
	} else {
		list_for_each_entry(dpcd_reg, &sim_dev->dpcd_reg_list, head) {
			if (dpcd_reg->addr == addr) {
				dpcd_reg->val = val;
				return;
			}
		}

		dpcd_reg = devm_kzalloc(sim_dev->dev,
				sizeof(*dpcd_reg), GFP_KERNEL);
		if (!dpcd_reg)
			return;

		dpcd_reg->addr = addr;
		dpcd_reg->val = val;
		list_add_tail(&dpcd_reg->head, &sim_dev->dpcd_reg_list);
	}
}

static int dp_sim_read_dpcd_regs(struct dp_sim_device *sim_dev,
		u8 *buf, u32 size, u32 offset)
{
	u32 i;

	if (offset + size <= MAX_BUILTIN_DPCD_ADDR) {
		memcpy(buf, &sim_dev->dpcd_reg[offset], size);
	} else {
		for (i = 0; i < size; i++)
			buf[i] = dp_sim_read_dpcd(sim_dev, offset + i);
	}

	return size;
}

static int dp_sim_read_edid(struct dp_sim_device *sim_dev,
		struct drm_dp_aux_msg *msg)
{
	u8 *buf = (u8 *)msg->buffer;
	u32 addr;

	if (!sim_dev->port_num || !msg->size)
		return 0;

	if (msg->request & DP_AUX_I2C_READ) {
		addr = (sim_dev->edid_seg_int << 8) + sim_dev->edid_addr;
		if (addr + msg->size <= sim_dev->ports[0].edid_size) {
			memcpy(msg->buffer, &sim_dev->ports[0].edid[addr],
					msg->size);
		} else if (addr < sim_dev->ports[0].edid_size) {
			memcpy(msg->buffer, &sim_dev->ports[0].edid[addr],
					sim_dev->ports[0].edid_size - addr);
		}
		sim_dev->edid_addr += msg->size;
		sim_dev->edid_addr &= 0xFF;
	} else {
		if (msg->address == 0x30) {
			sim_dev->edid_seg = buf[0];
		} else if (msg->address == 0x50) {
			sim_dev->edid_seg_int = sim_dev->edid_seg;
			sim_dev->edid_addr = buf[0];
			sim_dev->edid_seg = 0;
		}
	}

	return msg->size;
}

static int dp_sim_link_training(struct dp_sim_device *sim_dev,
		struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	u8 *link_status = msg->buffer;
	int ret, i;

	if (msg->request == DP_AUX_NATIVE_READ &&
			msg->address == DP_LANE0_1_STATUS) {
		/*
		 * remain is an option to allow limited actual
		 * link training. this is needed for some device
		 * when actual read is needed.
		 */
		if (sim_dev->link_training_remain) {
			sim_dev->link_training_remain--;
			ret = drm_aux->transfer(drm_aux, msg);
			if (ret >= 0)
				link_status[2] &= ~DP_LINK_STATUS_UPDATED;
			return ret;
		}

		memcpy(msg->buffer, &sim_dev->dpcd_reg[msg->address],
				msg->size);

		/*
		 * when mismatch happens, clear status and fail the link
		 * training.
		 */
		if (sim_dev->link_training_mismatch) {
			link_status[0] = 0;
			link_status[1] = 0;
		}

		return msg->size;
	}

	if (msg->request == DP_AUX_NATIVE_WRITE) {
		if (msg->address == DP_TRAINING_LANE0_SET) {
			const u8 mask = DP_TRAIN_VOLTAGE_SWING_MASK |
					DP_TRAIN_PRE_EMPHASIS_MASK;
			/*
			 * when link training is set, only pre-set vx/px is
			 * going through. here we will fail the initial
			 * vx/px and correct them automatically.
			 */
			sim_dev->link_training_mismatch = false;
			for (i = 0; i < sim_dev->link_training_lane_cnt; i++) {
				if ((link_status[i] & mask) !=
					(sim_dev->dpcd_reg[
					DP_TRAINING_LANE0_SET + i] & mask)) {
					sim_dev->link_training_mismatch = true;
					break;
				}
			}
		} else if (msg->address == DP_TRAINING_PATTERN_SET) {
			sim_dev->link_training_remain =
				sim_dev->link_training_cnt;
		} else if (msg->address == DP_LINK_BW_SET) {
			sim_dev->link_training_lane_cnt =
				link_status[1] & 0x1F;
		}
	}

	return 0;
}

static ssize_t dp_sim_transfer(struct dp_aux_bridge *bridge,
	struct drm_dp_aux *drm_aux,
	struct drm_dp_aux_msg *msg)
{
	struct dp_sim_device *sim_dev = to_dp_sim_dev(bridge);
	int ret;

	mutex_lock(&sim_dev->lock);

	if (sim_dev->skip_link_training &&
			!(sim_dev->sim_mode & DP_SIM_MODE_LINK_TRAIN)) {
		ret = dp_sim_link_training(sim_dev, drm_aux, msg);
		if (ret)
			goto end;
	}

	if ((sim_dev->sim_mode & DP_SIM_MODE_MST) || sim_dev->skip_mst) {
		ret = dp_mst_sim_transfer(sim_dev->bridge.mst_ctx, msg);
		if (ret >= 0) {
			ret = msg->size;
			goto end;
		}
	}

	if (msg->request == DP_AUX_NATIVE_WRITE) {
		sim_dev->dpcd_write_addr = msg->address;
		sim_dev->dpcd_write_size = msg->size;
	}

	if (((sim_dev->sim_mode & DP_SIM_MODE_EDID) ||
			sim_dev->skip_edid) &&
			(msg->request & DP_AUX_I2C_MOT))
		ret = dp_sim_read_edid(sim_dev, msg);
	else if (((sim_dev->sim_mode & DP_SIM_MODE_DPCD_READ) ||
			sim_dev->skip_dpcd) &&
			msg->request == DP_AUX_NATIVE_READ)
		ret = dp_sim_read_dpcd_regs(sim_dev, msg->buffer,
				msg->size, msg->address);
	else if (((sim_dev->sim_mode & DP_SIM_MODE_DPCD_WRITE) ||
			sim_dev->skip_config) &&
			msg->request == DP_AUX_NATIVE_WRITE)
		ret = msg->size;
	else
		ret = drm_aux->transfer(drm_aux, msg);

end:
	dp_sim_aux_hex_dump(msg);

	mutex_unlock(&sim_dev->lock);

	return ret;
}

static void dp_sim_host_hpd_irq(void *host_dev)
{
	struct dp_sim_device *sim_dev = host_dev;

	if (sim_dev->hpd_cb)
		sim_dev->hpd_cb(sim_dev->host_dev, true, true);
}

int dp_sim_set_sim_mode(struct dp_aux_bridge *bridge, u32 sim_mode)
{
	struct dp_sim_device *sim_dev;

	if (!bridge || !(bridge->flag & DP_SIM_BRIDGE_PRIV_FLAG))
		return -EINVAL;

	sim_dev = to_dp_sim_dev(bridge);

	sim_dev->sim_mode = sim_mode;

	return 0;
}

int dp_sim_update_port_num(struct dp_aux_bridge *bridge, u32 port_num)
{
	struct dp_sim_device *sim_dev;
	struct dp_mst_sim_port *ports;
	u32 i, rc;

	if (!bridge || !(bridge->flag & DP_SIM_BRIDGE_PRIV_FLAG))
		return -EINVAL;

	sim_dev = to_dp_sim_dev(bridge);

	if (port_num > sim_dev->port_num) {
		ports = devm_kzalloc(sim_dev->dev,
				port_num * sizeof(*ports), GFP_KERNEL);
		if (!ports)
			return -ENOMEM;

		memcpy(ports, sim_dev->ports,
				sim_dev->port_num * sizeof(*ports));

		if (sim_dev->ports)
			devm_kfree(sim_dev->dev, sim_dev->ports);

		sim_dev->ports = ports;

		for (i = sim_dev->port_num; i < port_num; i++) {
			memcpy(&ports[i], &output_port, sizeof(*ports));
			ports[i].peer_guid[0] = i;
		}

		sim_dev->port_num = port_num;
	}

	rc = dp_mst_sim_update(sim_dev->bridge.mst_ctx,
			port_num, sim_dev->ports);
	if (rc)
		return rc;

	sim_dev->current_port_num = port_num;

	return rc;
}

int dp_sim_update_port_status(struct dp_aux_bridge *bridge,
		int port, enum drm_connector_status status)
{
	struct dp_sim_device *sim_dev;

	if (!bridge || !(bridge->flag & DP_SIM_BRIDGE_PRIV_FLAG))
		return -EINVAL;

	sim_dev = to_dp_sim_dev(bridge);

	if (port < 0 || port >= sim_dev->current_port_num)
		return -EINVAL;

	sim_dev->ports[port].pdt = (status == connector_status_connected) ?
			DP_PEER_DEVICE_SST_SINK : DP_PEER_DEVICE_NONE;

	return dp_mst_sim_update(sim_dev->bridge.mst_ctx,
			sim_dev->current_port_num, sim_dev->ports);
}

int dp_sim_update_port_edid(struct dp_aux_bridge *bridge,
		int port, const u8 *edid, u32 size)
{
	struct dp_sim_device *sim_dev;
	struct dp_mst_sim_port *sim_port;

	if (!bridge || !(bridge->flag & DP_SIM_BRIDGE_PRIV_FLAG))
		return -EINVAL;

	sim_dev = to_dp_sim_dev(bridge);

	if (port < 0 || port >= sim_dev->current_port_num)
		return -EINVAL;

	sim_port = &sim_dev->ports[port];

	if (size != sim_port->edid_size) {
		if (sim_port->edid)
			devm_kfree(sim_dev->dev, (u8 *)sim_port->edid);

		sim_port->edid = devm_kzalloc(sim_dev->dev,
				size, GFP_KERNEL);
		if (!sim_port->edid)
			return -ENOMEM;

		sim_port->edid_size = size;
	}

	memcpy((u8 *)sim_port->edid, edid, size);

	return dp_mst_sim_update(sim_dev->bridge.mst_ctx,
			sim_dev->current_port_num, sim_dev->ports);
}

int dp_sim_write_dpcd_reg(struct dp_aux_bridge *bridge,
		const u8 *dpcd, u32 size, u32 offset)
{
	struct dp_sim_device *sim_dev;
	int i;

	if (!bridge || !(bridge->flag & DP_SIM_BRIDGE_PRIV_FLAG))
		return -EINVAL;

	sim_dev = to_dp_sim_dev(bridge);

	for (i = 0; i < size; i++)
		dp_sim_write_dpcd(sim_dev, offset + i, dpcd[i]);

	return 0;
}

int dp_sim_read_dpcd_reg(struct dp_aux_bridge *bridge,
		u8 *dpcd, u32 size, u32 offset)
{
	struct dp_sim_device *sim_dev;

	if (!bridge || !(bridge->flag & DP_SIM_BRIDGE_PRIV_FLAG))
		return -EINVAL;

	sim_dev = to_dp_sim_dev(bridge);

	return dp_sim_read_dpcd_regs(sim_dev, dpcd, size, offset);
}

static void dp_sim_update_dtd(struct edid *edid,
		struct drm_display_mode *mode)
{
	struct detailed_timing *dtd = &edid->detailed_timings[0];
	struct detailed_pixel_timing *pd = &dtd->data.pixel_data;
	u32 h_blank = mode->htotal - mode->hdisplay;
	u32 v_blank = mode->vtotal - mode->vdisplay;
	u32 h_img = 0, v_img = 0;

	dtd->pixel_clock = cpu_to_le16(mode->clock / 10);

	pd->hactive_lo = mode->hdisplay & 0xFF;
	pd->hblank_lo = h_blank & 0xFF;
	pd->hactive_hblank_hi = ((h_blank >> 8) & 0xF) |
			((mode->hdisplay >> 8) & 0xF) << 4;

	pd->vactive_lo = mode->vdisplay & 0xFF;
	pd->vblank_lo = v_blank & 0xFF;
	pd->vactive_vblank_hi = ((v_blank >> 8) & 0xF) |
			((mode->vdisplay >> 8) & 0xF) << 4;

	pd->hsync_offset_lo =
		(mode->hsync_start - mode->hdisplay) & 0xFF;
	pd->hsync_pulse_width_lo =
		(mode->hsync_end - mode->hsync_start) & 0xFF;
	pd->vsync_offset_pulse_width_lo =
		(((mode->vsync_start - mode->vdisplay) & 0xF) << 4) |
		((mode->vsync_end - mode->vsync_start) & 0xF);

	pd->hsync_vsync_offset_pulse_width_hi =
		((((mode->hsync_start - mode->hdisplay) >> 8) & 0x3) << 6) |
		((((mode->hsync_end - mode->hsync_start) >> 8) & 0x3) << 4) |
		((((mode->vsync_start - mode->vdisplay) >> 4) & 0x3) << 2) |
		((((mode->vsync_end - mode->vsync_start) >> 4) & 0x3) << 0);

	pd->width_mm_lo = h_img & 0xFF;
	pd->height_mm_lo = v_img & 0xFF;
	pd->width_height_mm_hi = (((h_img >> 8) & 0xF) << 4) |
		((v_img >> 8) & 0xF);

	pd->hborder = 0;
	pd->vborder = 0;
	pd->misc = 0;
}

static void dp_sim_update_checksum(struct edid *edid)
{
	u8 *data = (u8 *)edid;
	u32 i, sum = 0;

	for (i = 0; i < EDID_LENGTH - 1; i++)
		sum += data[i];

	edid->checksum = 0x100 - (sum & 0xFF);
}

static int dp_sim_parse_edid_from_node(struct dp_sim_device *sim_dev,
		int index, struct device_node *node)
{
	struct dp_mst_sim_port *port;
	struct drm_display_mode mode_buf, *mode = &mode_buf;
	u16 h_front_porch, h_pulse_width, h_back_porch;
	u16 v_front_porch, v_pulse_width, v_back_porch;
	bool h_active_high, v_active_high;
	u32 flags = 0;
	int rc;
	struct edid *edid;

	const u8 edid_buf[EDID_LENGTH] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x44, 0x6D,
		0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1B, 0x10, 0x01, 0x03,
		0x80, 0x50, 0x2D, 0x78, 0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47,
		0x98, 0x27, 0x12, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
	};

	rc = of_property_read_u16(node, "qcom,mode-h-active",
					&mode->hdisplay);
	if (rc) {
		DP_ERR("failed to read h-active, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u16(node, "qcom,mode-h-front-porch",
					&h_front_porch);
	if (rc) {
		DP_ERR("failed to read h-front-porch, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u16(node, "qcom,mode-h-pulse-width",
					&h_pulse_width);
	if (rc) {
		DP_ERR("failed to read h-pulse-width, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u16(node, "qcom,mode-h-back-porch",
					&h_back_porch);
	if (rc) {
		DP_ERR("failed to read h-back-porch, rc=%d\n", rc);
		goto fail;
	}

	h_active_high = of_property_read_bool(node,
					"qcom,mode-h-active-high");

	rc = of_property_read_u16(node, "qcom,mode-v-active",
					&mode->vdisplay);
	if (rc) {
		DP_ERR("failed to read v-active, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u16(node, "qcom,mode-v-front-porch",
					&v_front_porch);
	if (rc) {
		DP_ERR("failed to read v-front-porch, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u16(node, "qcom,mode-v-pulse-width",
					&v_pulse_width);
	if (rc) {
		DP_ERR("failed to read v-pulse-width, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u16(node, "qcom,mode-v-back-porch",
					&v_back_porch);
	if (rc) {
		DP_ERR("failed to read v-back-porch, rc=%d\n", rc);
		goto fail;
	}

	v_active_high = of_property_read_bool(node,
					"qcom,mode-v-active-high");

	rc = of_property_read_u32(node, "qcom,mode-clock-in-khz",
					&mode->clock);
	if (rc) {
		DP_ERR("failed to read clock, rc=%d\n", rc);
		goto fail;
	}

	mode->hsync_start = mode->hdisplay + h_front_porch;
	mode->hsync_end = mode->hsync_start + h_pulse_width;
	mode->htotal = mode->hsync_end + h_back_porch;
	mode->vsync_start = mode->vdisplay + v_front_porch;
	mode->vsync_end = mode->vsync_start + v_pulse_width;
	mode->vtotal = mode->vsync_end + v_back_porch;
	if (h_active_high)
		flags |= DRM_MODE_FLAG_PHSYNC;
	else
		flags |= DRM_MODE_FLAG_NHSYNC;
	if (v_active_high)
		flags |= DRM_MODE_FLAG_PVSYNC;
	else
		flags |= DRM_MODE_FLAG_NVSYNC;
	mode->flags = flags;

	edid = devm_kzalloc(sim_dev->dev, sizeof(*edid), GFP_KERNEL);
	if (!edid) {
		rc = -ENOMEM;
		goto fail;
	}

	memcpy(edid, edid_buf, sizeof(edid_buf));
	dp_sim_update_dtd(edid, mode);
	dp_sim_update_checksum(edid);

	port = &sim_dev->ports[index];
	memcpy(port, &output_port, sizeof(*port));
	port->peer_guid[0] = index;

	if (port->edid)
		devm_kfree(sim_dev->dev, (u8 *)port->edid);

	port->edid = (u8 *)edid;
	port->edid_size = sizeof(*edid);

fail:
	return rc;
}

static int dp_sim_parse_edid_from_data(struct dp_sim_device *sim_dev,
		int index, const char *data, int len)
{
	struct dp_mst_sim_port *port;
	u8 *edid_data;

	edid_data = devm_kzalloc(sim_dev->dev, len, GFP_KERNEL);
	if (!edid_data)
		return -ENOMEM;

	memcpy(edid_data, data, len);

	port = &sim_dev->ports[index];
	memcpy(port, &output_port, sizeof(*port));
	port->peer_guid[0] = index;

	if (port->edid)
		devm_kfree(sim_dev->dev, (u8 *)port->edid);

	port->edid = edid_data;
	port->edid_size = len;

	return 0;
}

static int dp_sim_parse_edid(struct dp_sim_device *sim_dev)
{
	struct dp_mst_sim_port *ports;
	struct device_node *of_node = sim_dev->bridge.of_node;
	struct device_node *node;
	const char *data;
	int rc, port_num, i, len;

	port_num = of_get_child_count(of_node);

	if (!port_num)
		port_num = 1;

	if (port_num >= 15)
		return -EINVAL;

	ports = devm_kzalloc(sim_dev->dev,
			port_num * sizeof(*ports), GFP_KERNEL);
	if (!ports)
		return -ENOMEM;

	sim_dev->ports = ports;
	sim_dev->port_num = port_num;
	sim_dev->current_port_num = port_num;

	i = 0;
	for_each_child_of_node(of_node, node) {
		data = of_get_property(node, "qcom,edid", &len);

		if (data)
			rc = dp_sim_parse_edid_from_data(sim_dev, i,
					data, len);
		else
			rc = dp_sim_parse_edid_from_node(sim_dev, i,
					node);

		if (rc)
			return rc;

		i++;
	}

	if (i == 0)
		memcpy(ports, &output_port, sizeof(*ports));

	return 0;
}

static int dp_sim_parse_dpcd(struct dp_sim_device *sim_dev)
{
	struct device_node *node = sim_dev->bridge.of_node;
	u32 val, i;
	const __be32 *arr;
	int rc;

	rc = of_property_read_u32(node, "qcom,dpcd-max-rate", &val);
	if (!rc)
		sim_dev->dpcd_reg[DP_MAX_LINK_RATE] = val;

	rc = of_property_read_u32(node, "qcom,dpcd-max-lane", &val);
	if (!rc)
		sim_dev->dpcd_reg[DP_MAX_LANE_COUNT] = val;

	rc = of_property_read_u32(node, "qcom,dpcd-mst", &val);
	if (!rc)
		sim_dev->dpcd_reg[DP_MSTM_CAP] = val;

	arr = of_get_property(node, "qcom,dpcd-regs", &val);
	if (arr) {
		val /= sizeof(u32);
		val &= ~0x1;
		for (i = 0; i < val; i += 2)
			dp_sim_write_dpcd(sim_dev,
					be32_to_cpu(arr[i]),
					be32_to_cpu(arr[i+1]));
	}

	rc = of_property_read_u32(node, "qcom,voltage-swing", &val);
	if (!rc)
		for (i = 0; i < 4; i++) {
			sim_dev->dpcd_reg[DP_TRAINING_LANE0_SET + i] |=
					val;
			sim_dev->dpcd_reg[DP_ADJUST_REQUEST_LANE0_1 + (i/2)] |=
					(val & 0x3) << ((i & 0x1) << 2);
		}

	rc = of_property_read_u32(node, "qcom,pre-emphasis", &val);
	if (!rc)
		for (i = 0; i < 4; i++) {
			sim_dev->dpcd_reg[DP_TRAINING_LANE0_SET + i] |=
					val << 3;
			sim_dev->dpcd_reg[DP_ADJUST_REQUEST_LANE0_1 + (i/2)] |=
					(val & 0x3) << (((i & 0x1) << 2) + 2);
		}

	rc = of_property_read_u32(node, "qcom,link-training-cnt", &val);
	if (!rc)
		sim_dev->link_training_cnt = val;
	else
		sim_dev->link_training_cnt = 0;

	return 0;
}

static int dp_sim_parse_misc(struct dp_sim_device *sim_dev)
{
	struct device_node *node = sim_dev->bridge.of_node;

	sim_dev->skip_edid = of_property_read_bool(node,
			"qcom,skip-edid");

	sim_dev->skip_dpcd = of_property_read_bool(node,
			"qcom,skip-dpcd-read");

	sim_dev->skip_link_training = of_property_read_bool(node,
			"qcom,skip-link-training");

	sim_dev->skip_config = of_property_read_bool(node,
			"qcom,skip-dpcd-write");

	sim_dev->skip_hpd = of_property_read_bool(node,
			"qcom,skip-hpd");

	sim_dev->skip_mst = of_property_read_bool(node,
			"qcom,skip-mst");

	DP_DEBUG("skip: edid=%d dpcd=%d LT=%d config=%d hpd=%d mst=%d\n",
			sim_dev->skip_edid,
			sim_dev->skip_dpcd,
			sim_dev->skip_link_training,
			sim_dev->skip_config,
			sim_dev->skip_hpd,
			sim_dev->skip_mst);

	return 0;
}

static ssize_t dp_sim_debug_write_edid(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_debug_edid_entry *entry = file->private_data;
	struct dp_sim_device *debug;
	struct dp_mst_sim_port *port;
	u8 *buf = NULL, *buf_t = NULL;
	const int char_to_nib = 2;
	size_t edid_size = 0;
	size_t size = 0, edid_buf_index = 0;
	ssize_t rc = count;

	if (!entry)
		return -ENODEV;

	debug = entry->sim_dev;
	if (!debug || entry->index >= debug->port_num)
		return -EINVAL;

	port = &debug->ports[entry->index];

	mutex_lock(&debug->lock);

	if (*ppos)
		goto bail;

	size = min_t(size_t, count, SZ_1K);

	buf = kzalloc(size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		rc = -ENOMEM;
		goto bail;
	}

	if (copy_from_user(buf, user_buff, size))
		goto bail;

	edid_size = size / char_to_nib;
	buf_t = buf;

	if (edid_size != port->edid_size) {
		if (port->edid)
			devm_kfree(debug->dev, (u8 *)port->edid);

		port->edid = devm_kzalloc(debug->dev,
					edid_size, GFP_KERNEL);
		if (!port->edid) {
			rc = -ENOMEM;
			goto bail;
		}
		port->edid_size = edid_size;
	}

	while (edid_size--) {
		char t[3];
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		if (kstrtoint(t, 16, &d)) {
			DP_ERR("kstrtoint error\n");
			goto bail;
		}

		if (port->edid && (edid_buf_index < port->edid_size))
			((u8 *)port->edid)[edid_buf_index++] = d;

		buf_t += char_to_nib;
	}

	if (debug->skip_mst)
		dp_mst_sim_update(debug->bridge.mst_ctx,
				debug->port_num, debug->ports);

	debug->skip_edid = true;

bail:
	kfree(buf);

	mutex_unlock(&debug->lock);
	return rc;
}

static ssize_t dp_sim_debug_write_dpcd(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	u8 *buf = NULL, *buf_t = NULL;
	const int char_to_nib = 2;
	size_t dpcd_size = 0;
	size_t size = 0, dpcd_buf_index = 0;
	ssize_t rc = count;
	char offset_ch[5];
	u32 offset, data_len;

	if (!debug)
		return -ENODEV;

	mutex_lock(&debug->lock);

	if (*ppos)
		goto bail;

	size = min_t(size_t, count, SZ_2K);
	if (size < 4)
		goto bail;

	buf = kzalloc(size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		rc = -ENOMEM;
		goto bail;
	}

	if (copy_from_user(buf, user_buff, size))
		goto bail;

	memcpy(offset_ch, buf, 4);
	offset_ch[4] = '\0';

	if (kstrtoint(offset_ch, 16, &offset)) {
		DP_ERR("offset kstrtoint error\n");
		goto bail;
	}

	if (offset == 0xFFFF) {
		DP_ERR("clearing dpcd\n");
		memset(debug->dpcd_reg, 0, sizeof(debug->dpcd_reg));
		goto bail;
	}

	size -= 4;
	if (size == 0)
		goto bail;

	dpcd_size = size / char_to_nib;
	data_len = dpcd_size;
	buf_t = buf + 4;

	dpcd_buf_index = offset;

	while (dpcd_size--) {
		char t[3];
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		if (kstrtoint(t, 16, &d)) {
			DP_ERR("kstrtoint error\n");
			goto bail;
		}

		dp_sim_write_dpcd(debug, dpcd_buf_index, d);
		dpcd_buf_index++;

		buf_t += char_to_nib;
	}

	debug->skip_dpcd = true;
	debug->skip_config = true;

bail:
	kfree(buf);

	mutex_unlock(&debug->lock);
	return rc;
}

static ssize_t dp_sim_debug_read_dpcd(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	char *buf;
	int const buf_size = SZ_4K;
	u32 offset = 0;
	u32 len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf, buf_size, "0x%x", debug->dpcd_write_addr);

	while (1) {
		if (debug->dpcd_write_addr + offset >= buf_size ||
		    offset >= debug->dpcd_write_size)
			break;

		len += snprintf(buf + len, buf_size - len, "0x%x",
			debug->dpcd_reg[debug->dpcd_write_addr + offset++]);
	}

	len = min_t(size_t, count, len);
	if (!copy_to_user(user_buff, buf, len))
		*ppos += len;

	kfree(buf);
	return len;
}

static ssize_t dp_sim_debug_write_hpd(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int hpd = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &hpd) != 0)
		goto end;

	if (debug->hpd_cb)
		debug->hpd_cb(debug->host_dev, !!hpd, false);

end:
	return len;
}

static ssize_t dp_sim_debug_write_skip_link_training(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int skip_lk, lk_cnt;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (sscanf(buf, "%d %u", &skip_lk, &lk_cnt) != 2) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	mutex_lock(&debug->lock);
	debug->skip_link_training = !!skip_lk;
	debug->link_training_cnt = lk_cnt;
	mutex_unlock(&debug->lock);
end:
	return len;
}

static ssize_t dp_sim_debug_write_skip_edid(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int val = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &val) != 0)
		goto end;

	mutex_lock(&debug->lock);
	debug->skip_edid = !!val;
	mutex_unlock(&debug->lock);
end:
	return len;
}

static ssize_t dp_sim_debug_write_skip_dpcd(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int val = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &val) != 0)
		goto end;

	mutex_lock(&debug->lock);
	debug->skip_dpcd = !!val;
	mutex_unlock(&debug->lock);
end:
	return len;
}

static ssize_t dp_sim_debug_write_skip_config(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int val = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &val) != 0)
		goto end;

	mutex_lock(&debug->lock);
	debug->skip_config = !!val;
	mutex_unlock(&debug->lock);
end:
	return len;
}

static ssize_t dp_sim_debug_write_mst_hpd(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_debug_edid_entry *entry = file->private_data;
	struct dp_sim_device *debug;
	char buf[SZ_8];
	size_t len = 0;
	int hpd = 0;

	if (!entry)
		return -ENODEV;

	debug = entry->sim_dev;
	if (!debug || entry->index >= debug->port_num)
		return -EINVAL;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &hpd) != 0)
		goto end;

	dp_sim_update_port_status(&debug->bridge,
				entry->index, hpd ?
				connector_status_connected :
				connector_status_disconnected);

end:
	return len;
}

static const struct file_operations sim_edid_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_edid,
};

static const struct file_operations sim_mst_hpd_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_mst_hpd,
};

static ssize_t dp_sim_debug_write_mst_mode(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_sim_device *debug = file->private_data;
	char buf[SZ_16];
	size_t len = 0;
	int mst_sideband_mode = 0;
	u32 mst_port_cnt = 0;
	u32 mst_old_port_cnt;
	struct dp_sim_debug_edid_entry *edid_entry;
	u8 *edid;
	u32 i, rc;

	if (!debug)
		return -ENODEV;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "%d %u", &mst_sideband_mode, &mst_port_cnt) != 2) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	if (mst_port_cnt >= MAX_MST_PORT) {
		DP_ERR("port cnt:%d exceeding max:%d\n", mst_port_cnt,
				MAX_MST_PORT);
		return -EINVAL;
	}

	if (!mst_port_cnt)
		mst_port_cnt = 1;

	debug->skip_mst = !mst_sideband_mode;
	DP_DEBUG("mst_sideband_mode: %d port_cnt:%d\n",
			mst_sideband_mode, mst_port_cnt);

	mst_old_port_cnt = debug->port_num;
	rc = dp_sim_update_port_num(&debug->bridge, mst_port_cnt);
	if (rc)
		return rc;

	/* write mst */
	dp_sim_write_dpcd(debug, DP_MSTM_CAP, debug->skip_mst);

	/* create default edid nodes */
	for (i = mst_old_port_cnt; i < mst_port_cnt; i++) {
		edid_entry = devm_kzalloc(debug->dev,
				sizeof(*edid_entry), GFP_KERNEL);
		if (!edid_entry)
			continue;

		edid_entry->index = i;
		edid_entry->sim_dev = debug;
		scnprintf(buf, sizeof(buf), "edid-%d", i);
		debugfs_create_file(buf,
				0444,
				debug->debugfs_edid_dir,
				edid_entry,
				&sim_edid_fops);
		scnprintf(buf, sizeof(buf), "hpd-%d", i);
		debugfs_create_file(buf,
				0444,
				debug->debugfs_edid_dir,
				edid_entry,
				&sim_mst_hpd_fops);

		if (!debug->ports[0].edid_size)
			continue;

		edid = devm_kzalloc(debug->dev,
				debug->ports[0].edid_size, GFP_KERNEL);
		if (!edid)
			return -ENOMEM;

		memcpy(edid, debug->ports[0].edid, debug->ports[0].edid_size);
		debug->ports[i].edid = edid;
		debug->ports[i].edid_size = debug->ports[0].edid_size;
	}

	return count;
}

static const struct file_operations sim_dpcd_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_dpcd,
	.read = dp_sim_debug_read_dpcd,
};

static const struct file_operations sim_hpd_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_hpd,
};

static const struct file_operations sim_skip_link_training_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_skip_link_training,
};

static const struct file_operations sim_skip_edid_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_skip_edid,
};

static const struct file_operations sim_skip_dpcd_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_skip_dpcd,
};

static const struct file_operations sim_skip_config_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_skip_config,
};

static const struct file_operations sim_mst_mode_fops = {
	.open = simple_open,
	.write = dp_sim_debug_write_mst_mode,
};

static int dp_sim_debug_init(struct dp_sim_device *sim_dev)
{
	struct dp_sim_debug_edid_entry *edid_entry;
	struct dentry *dir, *file, *edid_dir;
	char name[SZ_16];
	int rc = 0, i;

	if (!sim_dev->label)
		return 0;

	dir = debugfs_create_dir(sim_dev->label, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		DP_ERR("[%s] debugfs create dir failed, rc = %d\n",
				sim_dev->label, rc);
		goto error;
	}

	edid_dir = debugfs_create_dir("mst_edid", dir);
	if (IS_ERR_OR_NULL(edid_dir)) {
		rc = PTR_ERR(edid_dir);
		DP_ERR("[%s] debugfs create dir failed, rc = %d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	for (i = 0; i < sim_dev->port_num; i++) {
		edid_entry = devm_kzalloc(sim_dev->dev,
				sizeof(*edid_entry), GFP_KERNEL);
		edid_entry->index = i;
		edid_entry->sim_dev = sim_dev;
		scnprintf(name, sizeof(name), "edid-%d", i);
		file = debugfs_create_file(name,
				0444,
				edid_dir,
				edid_entry,
				&sim_edid_fops);
		if (IS_ERR_OR_NULL(file)) {
			rc = PTR_ERR(file);
			DP_ERR("[%s] debugfs create edid failed, rc=%d\n",
					sim_dev->label, rc);
			goto error_remove_dir;
		}
		scnprintf(name, sizeof(name), "hpd-%d", i);
		file = debugfs_create_file(name,
				0444,
				edid_dir,
				edid_entry,
				&sim_mst_hpd_fops);
		if (IS_ERR_OR_NULL(file)) {
			rc = PTR_ERR(file);
			DP_ERR("[%s] debugfs create hpd failed, rc=%d\n",
					sim_dev->label, rc);
			goto error_remove_dir;
		}
	}

	file = debugfs_create_symlink("edid", dir, "./mst_edid/edid-0");
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create edid link failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("dpcd",
			0444,
			dir,
			sim_dev,
			&sim_dpcd_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("hpd",
			0444,
			dir,
			sim_dev,
			&sim_hpd_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("skip_link_training",
			0444,
			dir,
			sim_dev,
			&sim_skip_link_training_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("skip_edid",
			0444,
			dir,
			sim_dev,
			&sim_skip_edid_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("skip_dpcd_read",
			0444,
			dir,
			sim_dev,
			&sim_skip_dpcd_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("skip_dpcd_write",
			0444,
			dir,
			sim_dev,
			&sim_skip_config_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("mst_sideband_mode",
			0444,
			dir,
			sim_dev,
			&sim_mst_mode_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create failed, rc=%d\n",
				sim_dev->label, rc);
		goto error_remove_dir;
	}

	sim_dev->debugfs_dir = dir;
	sim_dev->debugfs_edid_dir = edid_dir;

	return 0;

error_remove_dir:
	debugfs_remove_recursive(dir);
error:
	return rc;
}

static int dp_sim_parse(struct dp_sim_device *sim_dev)
{
	int rc;

	sim_dev->label = of_get_property(sim_dev->bridge.of_node,
			"label", NULL);

	rc = dp_sim_parse_dpcd(sim_dev);
	if (rc) {
		DP_ERR("failed to parse DPCD nodes\n");
		return rc;
	}

	rc = dp_sim_parse_edid(sim_dev);
	if (rc) {
		DP_ERR("failed to parse EDID nodes\n");
		return rc;
	}

	rc = dp_sim_parse_misc(sim_dev);
	if (rc) {
		DP_ERR("failed to parse misc nodes\n");
		return rc;
	}

	return 0;
}

int dp_sim_create_bridge(struct device *dev, struct dp_aux_bridge **bridge)
{
	struct dp_sim_device *dp_sim_dev;
	struct dp_mst_sim_cfg cfg;
	int ret;

	dp_sim_dev = devm_kzalloc(dev, sizeof(*dp_sim_dev), GFP_KERNEL);
	if (!dp_sim_dev)
		return -ENOMEM;

	dp_sim_dev->dev = dev;
	dp_sim_dev->bridge.of_node = dev->of_node;
	dp_sim_dev->bridge.register_hpd = dp_sim_register_hpd;
	dp_sim_dev->bridge.transfer = dp_sim_transfer;
	dp_sim_dev->bridge.dev_priv = dp_sim_dev;
	dp_sim_dev->bridge.flag = DP_AUX_BRIDGE_MST | DP_SIM_BRIDGE_PRIV_FLAG;
	INIT_LIST_HEAD(&dp_sim_dev->dpcd_reg_list);
	mutex_init(&dp_sim_dev->lock);

	memset(&cfg, 0, sizeof(cfg));
	cfg.host_dev = dp_sim_dev;
	cfg.host_hpd_irq = dp_sim_host_hpd_irq;

	ret = dp_mst_sim_create(&cfg, &dp_sim_dev->bridge.mst_ctx);
	if (ret) {
		devm_kfree(dev, dp_sim_dev);
		return ret;
	}

	/* default dpcd reg value */
	dp_sim_dev->dpcd_reg[DP_DPCD_REV] = 0x12;
	dp_sim_dev->dpcd_reg[DP_MAX_LINK_RATE] = 0x14;
	dp_sim_dev->dpcd_reg[DP_MAX_LANE_COUNT] = 0xc4;
	dp_sim_dev->dpcd_reg[DP_SINK_COUNT] = 0x1;
	dp_sim_dev->dpcd_reg[DP_LANE0_1_STATUS] = 0x77;
	dp_sim_dev->dpcd_reg[DP_LANE2_3_STATUS] = 0x77;
	dp_sim_dev->dpcd_reg[DP_LANE_ALIGN_STATUS_UPDATED] = 0x1;
	dp_sim_dev->dpcd_reg[DP_SINK_STATUS] = 0x3;
	dp_sim_dev->dpcd_reg[DP_PAYLOAD_TABLE_UPDATE_STATUS] = 0x3;

	/* enable link training by default */
	dp_sim_dev->skip_link_training = true;
	dp_sim_dev->link_training_cnt = (u32)-1;

	*bridge = &dp_sim_dev->bridge;
	return 0;
}

int dp_sim_destroy_bridge(struct dp_aux_bridge *bridge)
{
	struct dp_sim_device *dp_sim_dev;
	struct dp_sim_dpcd_reg *reg, *p;

	if (!bridge || !(bridge->flag & DP_SIM_BRIDGE_PRIV_FLAG))
		return -EINVAL;

	dp_sim_dev = to_dp_sim_dev(bridge);

	dp_mst_sim_destroy(dp_sim_dev->bridge.mst_ctx);

	list_for_each_entry_safe(reg, p, &dp_sim_dev->dpcd_reg_list, head) {
		list_del(&reg->head);
		devm_kfree(dp_sim_dev->dev, reg);
	}

	if (dp_sim_dev->ports)
		devm_kfree(dp_sim_dev->dev, dp_sim_dev->ports);

	devm_kfree(dp_sim_dev->dev, dp_sim_dev);

	return 0;
}

int dp_sim_probe(struct platform_device *pdev)
{
	struct dp_sim_device *dp_sim_dev;
	struct dp_aux_bridge *bridge;
	int ret;

	ret = dp_sim_create_bridge(&pdev->dev, &bridge);
	if (ret)
		return ret;

	dp_sim_dev = to_dp_sim_dev(bridge);

	ret = dp_sim_parse(dp_sim_dev);
	if (ret)
		goto fail;

	if (dp_sim_dev->skip_hpd)
		dp_sim_dev->bridge.flag |= DP_AUX_BRIDGE_HPD;

	ret = dp_mst_sim_update(dp_sim_dev->bridge.mst_ctx,
			dp_sim_dev->port_num, dp_sim_dev->ports);
	if (ret)
		goto fail;

	ret = dp_sim_debug_init(dp_sim_dev);
	if (ret)
		goto fail;

	ret = dp_aux_add_bridge(&dp_sim_dev->bridge);
	if (ret)
		goto fail;

	platform_set_drvdata(pdev, dp_sim_dev);

	return 0;

fail:
	dp_sim_destroy_bridge(bridge);
	return ret;
}

int dp_sim_remove(struct platform_device *pdev)
{
	struct dp_sim_device *dp_sim_dev;

	dp_sim_dev = platform_get_drvdata(pdev);
	if (!dp_sim_dev)
		return 0;

	debugfs_remove_recursive(dp_sim_dev->debugfs_dir);

	dp_sim_destroy_bridge(&dp_sim_dev->bridge);

	return 0;
}

#if 0
static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,dp-mst-sim"},
	{},
};

static struct platform_driver dp_sim_driver = {
	.probe = dp_sim_probe,
	.remove = dp_sim_remove,
	.driver = {
		.name = "dp_sim",
		.of_match_table = dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init dp_sim_register(void)
{
	return platform_driver_register(&dp_sim_driver);
}

static void __exit dp_sim_unregister(void)
{
	platform_driver_unregister(&dp_sim_driver);
}

module_init(dp_sim_register);
module_exit(dp_sim_unregister);

#endif