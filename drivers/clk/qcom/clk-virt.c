/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/reset-controller.h>
#include <linux/delay.h>
#include <linux/habmm.h>
#include <dt-bindings/clock/qcom,gcc-sm8150.h>
#include <dt-bindings/clock/qcom,scc-sm8150.h>

enum virtclk_cmd {
	CLK_MSG_GETID = 1,
	CLK_MSG_ENABLE,
	CLK_MSG_DISABLE,
	CLK_MSG_RESET,
	CLK_MSG_SETFREQ,
	CLK_MSG_GETFREQ,
	CLK_MSG_MAX
};

#define CLOCK_FLAG_NODE_TYPE_REMOTE	0xff00

struct clk_msg_header {
	u32 cmd;
	u32 len;
	u32 clk_id;
} __packed;

struct clk_msg_rsp {
	struct clk_msg_header header;
	u32 rsp;
} __packed;

struct clk_msg_setfreq {
	struct clk_msg_header header;
	u32 freq;
} __packed;

struct clk_msg_reset {
	struct clk_msg_header header;
	u32 reset;
} __packed;

struct clk_msg_getid {
	struct clk_msg_header header;
	char name[32];
} __packed;

struct clk_msg_getfreq {
	struct clk_msg_rsp rsp;
	u32 freq;
} __packed;

struct virt_reset_map {
	const char *clk_name;
	int clk_id;
};

struct clk_virt {
	int id;
	struct clk_hw hw;
	u32 flag;
};

struct clk_virt_desc {
	struct clk_hw **clks;
	size_t num_clks;
	struct virt_reset_map *resets;
	size_t num_resets;
};

struct virt_cc {
	struct reset_controller_dev rcdev;
	struct virt_reset_map *resets;
	struct clk_onecell_data data;
	struct clk *clks[];
};

static DEFINE_MUTEX(virt_clk_lock);
static int hab_handle;

static inline struct clk_virt *to_clk_virt(struct clk_hw *_hw)
{
	return container_of(_hw, struct clk_virt, hw);
}

static int clk_virt_init_iface(void)
{
	int ret = 0;
	int handle;

	mutex_lock(&virt_clk_lock);

	if (hab_handle)
		goto out;

	ret = habmm_socket_open(&handle, MM_CLK_VM1, 0, 0);
	if (ret) {
		pr_err("open habmm socket failed (%d)\n", ret);
		goto out;
	}

	hab_handle = handle;

out:
	mutex_unlock(&virt_clk_lock);
	return ret;
}

static int clk_virt_get_id(struct clk_hw *hw)
{
	struct clk_virt *v = to_clk_virt(hw);
	struct clk_msg_getid msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	if (v->id)
		return ret;

	msg.header.cmd = CLK_MSG_GETID | v->flag;
	msg.header.len = sizeof(msg);
	strlcpy(msg.name, clk_hw_get_name(hw), sizeof(msg.name));

	mutex_lock(&virt_clk_lock);

	handle = hab_handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size,
			UINT_MAX, HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", clk_hw_get_name(hw),
				rsp.rsp);
		ret = -EIO;
	} else
		v->id = rsp.header.clk_id;

	mutex_unlock(&virt_clk_lock);

	return ret;

err_out:
	habmm_socket_close(handle);
	hab_handle = 0;
	mutex_unlock(&virt_clk_lock);
	return ret;
}

static int clk_virt_prepare(struct clk_hw *hw)
{
	struct clk_virt *v = to_clk_virt(hw);
	struct clk_msg_header msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = clk_virt_init_iface();
	if (ret)
		return ret;

	ret = clk_virt_get_id(hw);
	if (ret)
		return ret;

	msg.clk_id = v->id;
	msg.cmd = CLK_MSG_ENABLE | v->flag;
	msg.len = sizeof(struct clk_msg_header);

	mutex_lock(&virt_clk_lock);

	handle = hab_handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", clk_hw_get_name(hw),
				rsp.rsp);
		ret = -EIO;
	}

	mutex_unlock(&virt_clk_lock);
	return ret;

err_out:
	habmm_socket_close(handle);
	hab_handle = 0;
	mutex_unlock(&virt_clk_lock);
	return ret;
}

static void clk_virt_unprepare(struct clk_hw *hw)
{
	struct clk_virt *v = to_clk_virt(hw);
	struct clk_msg_header msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = clk_virt_init_iface();
	if (ret)
		return;

	ret = clk_virt_get_id(hw);
	if (ret)
		return;

	msg.clk_id = v->id;
	msg.cmd = CLK_MSG_DISABLE | v->flag;
	msg.len = sizeof(struct clk_msg_header);

	mutex_lock(&virt_clk_lock);

	handle = hab_handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	if (rsp.rsp)
		pr_err("%s: error response (%d)\n", clk_hw_get_name(hw),
				rsp.rsp);

	mutex_unlock(&virt_clk_lock);
	return;

err_out:
	habmm_socket_close(handle);
	hab_handle = 0;
	mutex_unlock(&virt_clk_lock);
}

static int clk_virt_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct clk_virt *v = to_clk_virt(hw);
	struct clk_msg_setfreq msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = clk_virt_init_iface();
	if (ret)
		return ret;

	ret = clk_virt_get_id(hw);
	if (ret)
		return ret;

	msg.header.clk_id = v->id;
	msg.header.cmd = CLK_MSG_SETFREQ | v->flag;
	msg.header.len = sizeof(msg);
	msg.freq = (u32)rate;

	mutex_lock(&virt_clk_lock);

	handle = hab_handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n",
				clk_hw_get_name(hw),
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s (%luHz): error response (%d)\n", clk_hw_get_name(hw),
				rate, rsp.rsp);
		ret = -EIO;
	}

	mutex_unlock(&virt_clk_lock);
	return ret;

err_out:
	habmm_socket_close(handle);
	hab_handle = 0;
	mutex_unlock(&virt_clk_lock);
	return ret;
}

static long clk_virt_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	return rate;
}

static unsigned long clk_virt_get_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_virt *v = to_clk_virt(hw);
	struct clk_msg_header msg;
	struct clk_msg_getfreq rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = clk_virt_init_iface();
	if (ret)
		return 0;

	ret = clk_virt_get_id(hw);
	if (ret)
		return 0;

	msg.clk_id = v->id;
	msg.cmd = CLK_MSG_GETFREQ | v->flag;
	msg.len = sizeof(msg);

	mutex_lock(&virt_clk_lock);

	handle = hab_handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		ret = 0;
		pr_err("%s: habmm socket send failed (%d)\n",
				clk_hw_get_name(hw), ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		ret = 0;
		pr_err("%s: habmm socket receive failed (%d)\n",
				clk_hw_get_name(hw),
				ret);
		goto err_out;
	}

	if (rsp.rsp.rsp) {
		pr_err("%s: error response (%d)\n", clk_hw_get_name(hw),
				rsp.rsp.rsp);
		ret = 0;
	} else
		ret = rsp.freq;

	mutex_unlock(&virt_clk_lock);
	return ret;

err_out:
	habmm_socket_close(handle);
	hab_handle = 0;
	mutex_unlock(&virt_clk_lock);
	return ret;
}

static const struct clk_ops clk_virt_ops = {
	.prepare = clk_virt_prepare,
	.unprepare = clk_virt_unprepare,
	.set_rate = clk_virt_set_rate,
	.round_rate = clk_virt_round_rate,
	.recalc_rate = clk_virt_get_rate,
};

#define rc_to_vcc(r) \
	container_of(r, struct virt_cc, rcdev)

static int virtrc_get_clk_id(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	struct virt_cc *vcc;
	struct virt_reset_map *map;
	struct clk_msg_getid msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	vcc = rc_to_vcc(rcdev);
	map = &vcc->resets[id];
	msg.header.cmd = CLK_MSG_GETID;
	msg.header.len = sizeof(msg);
	strlcpy(msg.name, map->clk_name, sizeof(msg.name));

	mutex_lock(&virt_clk_lock);

	handle = hab_handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size,
			UINT_MAX, 0);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", map->clk_name, rsp.rsp);
		ret = -EIO;
	} else
		map->clk_id = rsp.header.clk_id;

	mutex_unlock(&virt_clk_lock);

	return ret;

err_out:
	habmm_socket_close(handle);
	hab_handle = 0;
	mutex_unlock(&virt_clk_lock);
	return ret;
}

static int __virtrc_reset(struct reset_controller_dev *rcdev,
		unsigned long id, unsigned int action)
{
	struct virt_cc *vcc;
	struct virt_reset_map *map;
	struct clk_msg_reset msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	vcc = rc_to_vcc(rcdev);
	map = &vcc->resets[id];

	ret = clk_virt_init_iface();
	if (ret)
		return ret;

	ret = virtrc_get_clk_id(rcdev, id);
	if (ret)
		return ret;

	msg.header.clk_id = map->clk_id;
	msg.header.cmd = CLK_MSG_RESET;
	msg.header.len = sizeof(struct clk_msg_header);
	msg.reset = action;

	mutex_lock(&virt_clk_lock);

	handle = hab_handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX, 0);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", map->clk_name, rsp.rsp);
		ret = -EIO;
	}

	mutex_unlock(&virt_clk_lock);

	pr_debug("%s(%lu): do %s\n", map->clk_name, id,
			action == 1 ? "assert" : "deassert");

	return ret;

err_out:
	habmm_socket_close(handle);
	hab_handle = 0;
	mutex_unlock(&virt_clk_lock);
	return ret;
}

static int virtrc_reset(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	int ret = 0;

	ret = __virtrc_reset(rcdev, id, 1);
	if (ret)
		return ret;

	udelay(1);

	return __virtrc_reset(rcdev, id, 0);
}

static int virtrc_reset_assert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return __virtrc_reset(rcdev, id, 1);
}

static int virtrc_reset_deassert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return __virtrc_reset(rcdev, id, 0);
}

static const struct reset_control_ops virtrc_ops = {
	.reset = virtrc_reset,
	.assert = virtrc_reset_assert,
	.deassert = virtrc_reset_deassert,
};

static struct virt_reset_map sm8150_gcc_virt_resets[] = {
	[GCC_QUSB2PHY_PRIM_BCR] = { "gcc_qusb2phy_prim_bcr" },
	[GCC_QUSB2PHY_SEC_BCR] = { "gcc_qusb2phy_sec_bcr" },
	[GCC_USB3_PHY_PRIM_BCR] = { "gcc_usb3_phy_prim_bcr" },
	[GCC_USB3_DP_PHY_PRIM_BCR] = { "gcc_usb3_dp_phy_prim_bcr" },
	[GCC_USB3_PHY_SEC_BCR] = { "gcc_usb3_phy_sec_bcr" },
	[GCC_USB3PHY_PHY_SEC_BCR] = { "gcc_usb3phy_phy_sec_bcr" },
	[GCC_USB30_PRIM_BCR] = { "gcc_usb30_prim_master_clk" },
	[GCC_USB30_SEC_BCR] = { "gcc_usb30_sec_master_clk" },
};

static struct clk_virt gcc_qupv3_wrap0_s0_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s0_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap0_s1_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s1_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap0_s2_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s2_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap0_s3_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s3_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap0_s4_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s4_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap0_s5_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s5_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap0_s6_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s6_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap0_s7_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap0_s7_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap1_s0_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap1_s0_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap1_s1_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap1_s1_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap1_s2_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap1_s2_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap1_s3_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap1_s3_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap1_s4_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap1_s4_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap1_s5_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap1_s5_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap2_s0_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap2_s0_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap2_s1_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap2_s1_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap2_s2_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap2_s2_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap2_s3_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap2_s3_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap2_s4_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap2_s4_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap2_s5_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap2_s5_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap_0_m_ahb_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap_0_m_ahb_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap_0_s_ahb_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap_0_s_ahb_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap_1_m_ahb_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap_1_m_ahb_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap_1_s_ahb_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap_1_s_ahb_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap_2_m_ahb_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap_2_m_ahb_clk",
	},
};

static struct clk_virt gcc_qupv3_wrap_2_s_ahb_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_qupv3_wrap_2_s_ahb_clk",
	},
};

static struct clk_virt gcc_usb30_prim_master_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb30_prim_master_clk",
	},
};

static struct clk_virt gcc_cfg_noc_usb3_prim_axi_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_cfg_noc_usb3_prim_axi_clk",
	},
};

static struct clk_virt gcc_aggre_usb3_prim_axi_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_aggre_usb3_prim_axi_clk",
	},
};

static struct clk_virt gcc_usb30_prim_mock_utmi_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb30_prim_mock_utmi_clk",
	},
};

static struct clk_virt gcc_usb30_prim_sleep_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb30_prim_sleep_clk",
	},
};

static struct clk_virt gcc_usb3_sec_clkref_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_sec_clkref_en",
	},
};

static struct clk_virt gcc_usb3_prim_phy_aux_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_prim_phy_aux_clk",
	},
};

static struct clk_virt gcc_usb3_prim_phy_pipe_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_prim_phy_pipe_clk",
	},
};

static struct clk_virt gcc_usb3_prim_clkref_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_prim_clkref_en",
	},
};

static struct clk_virt gcc_usb3_prim_phy_com_aux_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_prim_phy_com_aux_clk",
	},
};

static struct clk_virt gcc_usb30_sec_master_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb30_sec_master_clk",
	},
};

static struct clk_virt gcc_cfg_noc_usb3_sec_axi_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_cfg_noc_usb3_sec_axi_clk",
	},
};

static struct clk_virt gcc_aggre_usb3_sec_axi_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_aggre_usb3_sec_axi_clk",
	},
};

static struct clk_virt gcc_usb30_sec_mock_utmi_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb30_sec_mock_utmi_clk",
	},
};

static struct clk_virt gcc_usb30_sec_sleep_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb30_sec_sleep_clk",
	},
};

static struct clk_virt gcc_usb3_sec_phy_aux_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_sec_phy_aux_clk",
	},
};

static struct clk_virt gcc_usb3_sec_phy_pipe_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_sec_phy_pipe_clk",
	},
};

static struct clk_virt gcc_usb3_sec_phy_com_aux_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "gcc_usb3_sec_phy_com_aux_clk",
	},
};

static struct clk_hw *sm8150_gcc_virt_clocks[] = {
	[GCC_QUPV3_WRAP0_S0_CLK] = &gcc_qupv3_wrap0_s0_clk.hw,
	[GCC_QUPV3_WRAP0_S1_CLK] = &gcc_qupv3_wrap0_s1_clk.hw,
	[GCC_QUPV3_WRAP0_S2_CLK] = &gcc_qupv3_wrap0_s2_clk.hw,
	[GCC_QUPV3_WRAP0_S3_CLK] = &gcc_qupv3_wrap0_s3_clk.hw,
	[GCC_QUPV3_WRAP0_S4_CLK] = &gcc_qupv3_wrap0_s4_clk.hw,
	[GCC_QUPV3_WRAP0_S5_CLK] = &gcc_qupv3_wrap0_s5_clk.hw,
	[GCC_QUPV3_WRAP0_S6_CLK] = &gcc_qupv3_wrap0_s6_clk.hw,
	[GCC_QUPV3_WRAP0_S7_CLK] = &gcc_qupv3_wrap0_s7_clk.hw,
	[GCC_QUPV3_WRAP1_S0_CLK] = &gcc_qupv3_wrap1_s0_clk.hw,
	[GCC_QUPV3_WRAP1_S1_CLK] = &gcc_qupv3_wrap1_s1_clk.hw,
	[GCC_QUPV3_WRAP1_S2_CLK] = &gcc_qupv3_wrap1_s2_clk.hw,
	[GCC_QUPV3_WRAP1_S3_CLK] = &gcc_qupv3_wrap1_s3_clk.hw,
	[GCC_QUPV3_WRAP1_S4_CLK] = &gcc_qupv3_wrap1_s4_clk.hw,
	[GCC_QUPV3_WRAP1_S5_CLK] = &gcc_qupv3_wrap1_s5_clk.hw,
	[GCC_QUPV3_WRAP2_S0_CLK] = &gcc_qupv3_wrap2_s0_clk.hw,
	[GCC_QUPV3_WRAP2_S1_CLK] = &gcc_qupv3_wrap2_s1_clk.hw,
	[GCC_QUPV3_WRAP2_S2_CLK] = &gcc_qupv3_wrap2_s2_clk.hw,
	[GCC_QUPV3_WRAP2_S3_CLK] = &gcc_qupv3_wrap2_s3_clk.hw,
	[GCC_QUPV3_WRAP2_S4_CLK] = &gcc_qupv3_wrap2_s4_clk.hw,
	[GCC_QUPV3_WRAP2_S5_CLK] = &gcc_qupv3_wrap2_s5_clk.hw,
	[GCC_QUPV3_WRAP_0_M_AHB_CLK] = &gcc_qupv3_wrap_0_m_ahb_clk.hw,
	[GCC_QUPV3_WRAP_0_S_AHB_CLK] = &gcc_qupv3_wrap_0_s_ahb_clk.hw,
	[GCC_QUPV3_WRAP_1_M_AHB_CLK] = &gcc_qupv3_wrap_1_m_ahb_clk.hw,
	[GCC_QUPV3_WRAP_1_S_AHB_CLK] = &gcc_qupv3_wrap_1_s_ahb_clk.hw,
	[GCC_QUPV3_WRAP_2_M_AHB_CLK] = &gcc_qupv3_wrap_2_m_ahb_clk.hw,
	[GCC_QUPV3_WRAP_2_S_AHB_CLK] = &gcc_qupv3_wrap_2_s_ahb_clk.hw,
	[GCC_USB30_PRIM_MASTER_CLK] = &gcc_usb30_prim_master_clk.hw,
	[GCC_CFG_NOC_USB3_PRIM_AXI_CLK] = &gcc_cfg_noc_usb3_prim_axi_clk.hw,
	[GCC_AGGRE_USB3_PRIM_AXI_CLK] = &gcc_aggre_usb3_prim_axi_clk.hw,
	[GCC_USB30_PRIM_MOCK_UTMI_CLK] = &gcc_usb30_prim_mock_utmi_clk.hw,
	[GCC_USB30_PRIM_SLEEP_CLK] = &gcc_usb30_prim_sleep_clk.hw,
	[GCC_USB3_SEC_CLKREF_CLK] = &gcc_usb3_sec_clkref_clk.hw,
	[GCC_USB3_PRIM_PHY_AUX_CLK] = &gcc_usb3_prim_phy_aux_clk.hw,
	[GCC_USB3_PRIM_PHY_PIPE_CLK] = &gcc_usb3_prim_phy_pipe_clk.hw,
	[GCC_USB3_PRIM_CLKREF_CLK] = &gcc_usb3_prim_clkref_clk.hw,
	[GCC_USB3_PRIM_PHY_COM_AUX_CLK] = &gcc_usb3_prim_phy_com_aux_clk.hw,
	[GCC_USB30_SEC_MASTER_CLK] = &gcc_usb30_sec_master_clk.hw,
	[GCC_CFG_NOC_USB3_SEC_AXI_CLK] = &gcc_cfg_noc_usb3_sec_axi_clk.hw,
	[GCC_AGGRE_USB3_SEC_AXI_CLK] = &gcc_aggre_usb3_sec_axi_clk.hw,
	[GCC_USB30_SEC_MOCK_UTMI_CLK] = &gcc_usb30_sec_mock_utmi_clk.hw,
	[GCC_USB30_SEC_SLEEP_CLK] = &gcc_usb30_sec_sleep_clk.hw,
	[GCC_USB3_SEC_PHY_AUX_CLK] = &gcc_usb3_sec_phy_aux_clk.hw,
	[GCC_USB3_SEC_PHY_PIPE_CLK] = &gcc_usb3_sec_phy_pipe_clk.hw,
	[GCC_USB3_SEC_PHY_COM_AUX_CLK] = &gcc_usb3_sec_phy_com_aux_clk.hw,
};

static const struct clk_virt_desc clk_virt_sm8150_gcc = {
	.clks = sm8150_gcc_virt_clocks,
	.num_clks = ARRAY_SIZE(sm8150_gcc_virt_clocks),
	.resets = sm8150_gcc_virt_resets,
	.num_resets = ARRAY_SIZE(sm8150_gcc_virt_resets),
};

static struct clk_virt scc_qupv3_se0_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "scc_qupv3_se0_clk",
	},
};

static struct clk_virt scc_qupv3_se1_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "scc_qupv3_se1_clk",
	},
};

static struct clk_virt scc_qupv3_se2_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "scc_qupv3_se2_clk",
	},
};

static struct clk_virt scc_qupv3_se3_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "scc_qupv3_se3_clk",
	},
};

static struct clk_virt scc_qupv3_m_hclk_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "scc_qupv3_m_hclk_clk",
	},
};

static struct clk_virt scc_qupv3_s_hclk_clk = {
	.hw.init = &(struct clk_init_data) {
		.ops = &clk_virt_ops,
		.name = "scc_qupv3_s_hclk_clk",
	},
};

static struct clk_hw *sm8150_scc_virt_clocks[] = {
	[SCC_QUPV3_SE0_CLK] = &scc_qupv3_se0_clk.hw,
	[SCC_QUPV3_SE1_CLK] = &scc_qupv3_se1_clk.hw,
	[SCC_QUPV3_SE2_CLK] = &scc_qupv3_se2_clk.hw,
	[SCC_QUPV3_SE3_CLK] = &scc_qupv3_se3_clk.hw,
	[SCC_QUPV3_M_HCLK_CLK] = &scc_qupv3_m_hclk_clk.hw,
	[SCC_QUPV3_S_HCLK_CLK] = &scc_qupv3_s_hclk_clk.hw,
};

static const struct clk_virt_desc clk_virt_sm8150_scc = {
	.clks = sm8150_scc_virt_clocks,
	.num_clks = ARRAY_SIZE(sm8150_scc_virt_clocks),
};

static const struct of_device_id clk_virt_match_table[] = {
	{
		.compatible = "qcom,virt-clk-sm8150-gcc",
		.data = &clk_virt_sm8150_gcc
	},
	{
		.compatible = "qcom,virt-clk-sm8150-scc",
		.data = &clk_virt_sm8150_scc
	},
	{ }
};
MODULE_DEVICE_TABLE(of, clk_virt_match_table);

static int clk_virt_probe(struct platform_device *pdev)
{
	struct clk **clks;
	struct clk *clk;
	struct virt_cc *vcc;
	struct clk_onecell_data *data;
	int ret;
	size_t num_clks, num_resets, i;
	struct clk_hw **hw_clks;
	struct virt_reset_map *resets;
	struct clk_virt *virt_clk;
	const struct clk_virt_desc *desc;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc) {
		ret = -EINVAL;
		goto err;
	}

	hw_clks = desc->clks;
	num_clks = desc->num_clks;
	resets = desc->resets;
	num_resets = desc->num_resets;

	vcc = devm_kzalloc(&pdev->dev, sizeof(*vcc) + sizeof(*clks) * num_clks,
			GFP_KERNEL);
	if (!vcc) {
		ret = -ENOMEM;
		goto err;
	}

	clks = vcc->clks;
	data = &vcc->data;
	data->clks = clks;
	data->clk_num = num_clks;

	for (i = 0; i < num_clks; i++) {
		if (!hw_clks[i]) {
			clks[i] = ERR_PTR(-ENOENT);
			continue;
		}

		virt_clk = to_clk_virt(hw_clks[i]);

		clk = devm_clk_register(&pdev->dev, hw_clks[i]);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			goto err;
		}

		clks[i] = clk;
	}

	ret = of_clk_add_provider(pdev->dev.of_node, of_clk_src_onecell_get,
				  data);
	if (ret)
		goto err;

	vcc->rcdev.of_node = pdev->dev.of_node;
	vcc->rcdev.ops = &virtrc_ops;
	vcc->rcdev.owner = pdev->dev.driver->owner;
	vcc->rcdev.nr_resets = desc->num_resets;
	vcc->resets = desc->resets;

	ret = devm_reset_controller_register(&pdev->dev, &vcc->rcdev);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered virtual clocks\n");

	return ret;
err:
	dev_err(&pdev->dev, "Error registering virtual clock driver (%d)\n",
			ret);
	return ret;
}

static struct platform_driver clk_virt_driver = {
	.probe		= clk_virt_probe,
	.driver		= {
		.name	= "clk-virt",
		.of_match_table = clk_virt_match_table,
	},
};

static int __init clk_virt_init(void)
{
	return platform_driver_register(&clk_virt_driver);
}
fs_initcall(clk_virt_init);

static void __exit clk_virt_exit(void)
{
	platform_driver_unregister(&clk_virt_driver);
}
module_exit(clk_virt_exit);

MODULE_DESCRIPTION("QTI Virtual Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:clk-virt");
