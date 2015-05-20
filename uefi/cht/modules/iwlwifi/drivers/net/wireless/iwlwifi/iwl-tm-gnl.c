/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2010 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include <linux/export.h>
#include <net/genetlink.h>
#include "iwl-drv.h"
#include "iwl-io.h"
#include "iwl-fh.h"
#include "iwl-prph.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-tm-gnl.h"
#include "iwl-tm-infc.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dispatch.h"
#include "iwl-csr.h"

/**
 * iwl_tm_validate_fw_cmd() - Validates FW host command input data
 * @data_in:	Input to be validated
 *
 */
static int iwl_tm_validate_fw_cmd(struct iwl_tm_data *data_in)
{
	struct iwl_tm_cmd_request *cmd_req;
	u32 data_buf_size;

	if (!data_in->data ||
	    (data_in->len < sizeof(struct iwl_tm_cmd_request)))
		return -EINVAL;

	cmd_req = (struct iwl_tm_cmd_request *)data_in->data;

	data_buf_size = data_in->len - sizeof(struct iwl_tm_cmd_request);
	if (data_buf_size < cmd_req->len)
		return -EINVAL;

	return 0;
}

/**
 * iwl_tm_validate_reg_ops() - Checks the input data for registers operations
 * @data_in:	data is casted to iwl_tm_regs_request len in
 *		the size of the request struct in bytes.
 */
static int iwl_tm_validate_reg_ops(struct iwl_tm_data *data_in)
{
	struct iwl_tm_regs_request *request;
	u32 request_size;
	u32 idx;

	if (!data_in->data ||
	    (data_in->len < sizeof(struct iwl_tm_regs_request)))
		return -EINVAL;

	request = (struct iwl_tm_regs_request *)(data_in->data);
	request_size = sizeof(struct iwl_tm_regs_request) +
		       request->num * sizeof(struct iwl_tm_reg_op);
	if (data_in->len < request_size)
		return -EINVAL;

	/*
	 * Calculate result size - result is returned only for read ops
	 * Also, verifying inputs
	 */
	for (idx = 0;  idx < request->num; idx++) {
		if (request->reg_ops[idx].op_type >= IWL_TM_REG_OP_MAX)
			return -EINVAL;

		/*
		* Allow access only to FH/CSR/HBUS in direct mode.
		* Since we don't have the upper bounds for the CSR
		* and HBUS segments, we will use only the upper
		* bound of FH for sanity check.
		*/
		if (!IS_AL_ADDR(request->reg_ops[idx].address)) {
			if (request->reg_ops[idx].address >=
			    FH_MEM_UPPER_BOUND)
				return -EINVAL;
		}
	}

	return 0;
}

/**
 * iwl_tm_trace_end - Ends debug trace. Common for all op modes.
 * @dev: testmode device struct
 */
static int iwl_tm_trace_end(struct iwl_tm_gnl_dev *dev)
{
	struct iwl_trans *trans = dev->trans;
	struct iwl_test_trace *trace = &dev->tst.trace;

	if (!trace->enabled)
		return -EILSEQ;

	if (trace->cpu_addr && trace->dma_addr)
		dma_free_coherent(trans->dev, trace->size,
				  trace->cpu_addr, trace->dma_addr);
	memset(trace, 0, sizeof(struct iwl_test_trace));

	return 0;
}

/**
 * iwl_tm_trace_begin() - Checks input data for trace request
 * @dev:	testmode device struct
 * @data_in:	Only size is relevant - Desired size of trace buffer.
 * @data_out:	Trace request data (address & size)
 */
static int iwl_tm_trace_begin(struct iwl_tm_gnl_dev *dev,
			      struct iwl_tm_data *data_in,
			      struct iwl_tm_data *data_out)
{
	struct iwl_tm_trace_request *req = data_in->data;
	struct iwl_tm_trace_request *resp;

	if (!data_in->data ||
	    data_in->len < sizeof(struct iwl_tm_trace_request))
		return -EINVAL;

	req = data_in->data;

	/* size zero means use the default */
	if (!req->size)
		req->size = TRACE_BUFF_SIZE_DEF;
	else if (req->size < TRACE_BUFF_SIZE_MIN ||
		 req->size > TRACE_BUFF_SIZE_MAX)
		return -EINVAL;
	else if (!dev->dnt->mon_buf_cpu_addr)
		return -ENOMEM;

	resp = kmalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		return -ENOMEM;
	}
	resp->size = dev->dnt->mon_buf_size;
	/* Casting to avoid compilation warnings when DMA address is 32bit */
	resp->addr = (u64)dev->dnt->mon_base_addr;

	data_out->data = resp;
	data_out->len = sizeof(*resp);

	return 0;
}

/**
 * iwl_tm_validate_sram_write_req() - Checks input data of SRAM write request
 * @dev:	testmode device struct
 * @data_in:	SRAM access request
 */
static int iwl_tm_validate_sram_write_req(struct iwl_tm_gnl_dev *dev,
					  struct iwl_tm_data *data_in)
{
	struct iwl_tm_sram_write_request *cmd_in;
	u32 data_buf_size;

	if (!dev->trans->op_mode) {
		IWL_ERR(dev->trans, "No op_mode!\n");
		return -ENODEV;
	}

	if (!data_in->data ||
	    data_in->len < sizeof(struct iwl_tm_sram_write_request))
		return -EINVAL;

	cmd_in = data_in->data;

	data_buf_size =
		data_in->len - sizeof(struct iwl_tm_sram_write_request);
	if (data_buf_size < cmd_in->len)
		return -EINVAL;

	if (dev->trans->op_mode->ops->test_ops.valid_hw_addr(cmd_in->offset))
		return 0;

	if ((cmd_in->offset < IWL_ABS_PRPH_START)  &&
	    (cmd_in->offset >= IWL_ABS_PRPH_START + PRPH_END))
		return 0;

	return -EINVAL;
}

/**
 * iwl_tm_validate_sram_read_req() - Checks input data of SRAM read request
 * @dev:	testmode device struct
 * @data_in:	SRAM access request
 */
static int iwl_tm_validate_sram_read_req(struct iwl_tm_gnl_dev *dev,
					 struct iwl_tm_data *data_in)
{
	struct iwl_tm_sram_read_request *cmd_in;

	if (!dev->trans->op_mode) {
		IWL_ERR(dev->trans, "No op_mode!\n");
		return -ENODEV;
	}

	if (!data_in->data ||
	    data_in->len < sizeof(struct iwl_tm_sram_read_request))
		return -EINVAL;

	cmd_in = data_in->data;

	if (dev->trans->op_mode->ops->test_ops.valid_hw_addr(cmd_in->offset))
		return 0;

	if ((cmd_in->offset < IWL_ABS_PRPH_START)  &&
	    (cmd_in->offset >= IWL_ABS_PRPH_START + PRPH_END))
		return 0;

	return -EINVAL;
}

/**
 * iwl_tm_notifications_en() - Checks input for enable test notifications
 * @tst:	Device's test data pointer
 * @data_in:	u32 notification (flag)
 */
static int iwl_tm_notifications_en(struct iwl_test *tst,
				   struct iwl_tm_data *data_in)
{
	u32 notification_en;

	if (!data_in->data || (data_in->len != sizeof(u32)))
		return -EINVAL;

	notification_en = *(u32 *)data_in->data;
	if ((notification_en != NOTIFICATIONS_ENABLE) &&
	    (notification_en != NOTIFICATIONS_DISABLE))
		return -EINVAL;

	tst->notify = notification_en == NOTIFICATIONS_ENABLE;

	return 0;
}

/**
 * iwl_tm_validate_tx_cmd() - Validates FW host command input data
 * @data_in:	Input to be validated
 *
 */
static int iwl_tm_validate_tx_cmd(struct iwl_tm_data *data_in)
{
	struct iwl_tm_mod_tx_request *cmd_req;
	u32 data_buf_size;

	if (!data_in->data ||
	    (data_in->len < sizeof(struct iwl_tm_mod_tx_request)))
		return -EINVAL;

	cmd_req = (struct iwl_tm_mod_tx_request *)data_in->data;

	data_buf_size = data_in->len -
			sizeof(struct iwl_tm_mod_tx_request);
	if (data_buf_size < cmd_req->len)
		return -EINVAL;

	if (cmd_req->sta_id >= IWL_TM_STATION_COUNT)
		return -EINVAL;

	return 0;
}

/**
 * iwl_tm_validate_rx_hdrs_mode_req() - Validates RX headers mode request
 * @data_in:	Input to be validated
 *
 */
static int iwl_tm_validate_rx_hdrs_mode_req(struct iwl_tm_data *data_in)
{
	if (!data_in->data ||
	    (data_in->len < sizeof(struct iwl_xvt_rx_hdrs_mode_request)))
		return -EINVAL;

	return 0;
}

static int iwl_tm_validate_get_chip_id(struct iwl_trans *trans)
{
	if (strcmp(trans->dev->bus->name, BUS_TYPE_IDI))
		return -EINVAL;
	return 0;

}

/**
 * iwl_tm_validate_apmg_pd_mode_req() - Validates apmg rx mode request
 * @data_in:	Input to be validated
 *
 */
static int iwl_tm_validate_apmg_pd_mode_req(struct iwl_tm_data *data_in)
{
	if (!data_in->data ||
	    (data_in->len != sizeof(struct iwl_xvt_apmg_pd_mode_request)))
		return -EINVAL;

	return 0;
}

static int iwl_tm_get_device_status(struct iwl_tm_gnl_dev *dev,
				    struct iwl_tm_data *data_in,
				    struct iwl_tm_data *data_out)
{
	__u32 *status;

	status = kmalloc(sizeof(__u32), GFP_KERNEL);
	if (!status)
		return -ENOMEM;

	*status = dev->dnt->iwl_dnt_status;

	data_out->data = status;
	data_out->len = sizeof(__u32);

	return 0;
}

#if IS_ENABLED(CPTCFG_IWLXVT)
static int iwl_tm_switch_op_mode(struct iwl_tm_gnl_dev *dev,
				 struct iwl_tm_data *data_in)
{
	struct iwl_switch_op_mode *switch_cmd = data_in->data;
	struct iwl_drv *drv;
	int ret = 0;

	if (data_in->len < sizeof(*switch_cmd))
		return -EINVAL;

	drv = iwl_drv_get_dev_container(dev->trans->dev);
	if (!drv) {
		IWL_ERR(dev->trans, "Couldn't retrieve device information\n");
		return -ENODEV;
	}

	/* Executing switch command */
	ret = iwl_drv_switch_op_mode(drv, switch_cmd->new_op_mode);

	if (ret < 0)
		IWL_ERR(dev->trans, "Failed to switch op mode to %s (err:%d)\n",
			switch_cmd->new_op_mode, ret);

	return ret;
}
#endif

static int iwl_tm_gnl_get_sil_step(struct iwl_trans *trans,
				   struct iwl_tm_data *data_out)
{
	struct iwl_sil_step *resp;
	data_out->data =  kmalloc(sizeof(struct iwl_sil_step), GFP_KERNEL);
	if (!data_out->data)
		return -ENOMEM;
	data_out->len = sizeof(struct iwl_sil_step);
	resp = (struct iwl_sil_step *)data_out->data;
	resp->silicon_step = CSR_HW_REV_STEP(trans->hw_rev);
	return 0;
}

static int iwl_tm_gnl_get_build_info(struct iwl_trans *trans,
				     struct iwl_tm_data *data_out)
{
	struct iwl_tm_build_info *resp;

	data_out->data =  kmalloc(sizeof(*resp), GFP_KERNEL);
	if (!data_out->data)
		return -ENOMEM;
	data_out->len = sizeof(struct iwl_tm_build_info);
	resp = (struct iwl_tm_build_info *)data_out->data;

	memset(resp, 0 , sizeof(*resp));
	strncpy(resp->driver_version, BACKPORTS_GIT_TRACKED,
		sizeof(resp->driver_version));
#ifdef BACKPORTS_BRANCH_TSTAMP
	strncpy(resp->branch_time, BACKPORTS_BRANCH_TSTAMP,
		sizeof(resp->branch_time));
#endif
	strncpy(resp->build_time, BACKPORTS_BUILD_TSTAMP,
		sizeof(resp->build_time));

	return 0;
}

/*
 * Testmode GNL family types (This NL family
 * will eventually replace nl80211 support in
 * iwl xVM modules)
 */

#define IWL_TM_GNL_FAMILY_NAME	"iwl_tm_gnl"
#define IWL_TM_GNL_MC_GRP_NAME	"iwl_tm_mcgrp"
#define IWL_TM_GNL_VERSION_NR	1

#define IWL_TM_GNL_DEVNAME_LEN	256


/**
 * struct iwl_tm_gnl_cmd - Required data for command execution.
 * @dev_name:	Target device
 * @cmd:	Command index
 * @data_in:	Input data
 * @data_out:	Output data, to be sent when
 *		command is done.
 */
struct iwl_tm_gnl_cmd {
	const char *dev_name;
	u32 cmd;
	struct iwl_tm_data data_in;
	struct iwl_tm_data data_out;
};

static struct list_head dev_list; /* protected by mutex or RCU */
static struct mutex dev_list_mtx;

/* Testmode GNL family command attributes  */
enum iwl_tm_gnl_cmd_attr_t {
	IWL_TM_GNL_MSG_ATTR_INVALID = 0,
	IWL_TM_GNL_MSG_ATTR_DEVNAME,
	IWL_TM_GNL_MSG_ATTR_CMD,
	IWL_TM_GNL_MSG_ATTR_DATA,
	IWL_TM_GNL_MSG_ATTR_MAX
};

/* TM GNL family definition */
static struct genl_family iwl_tm_gnl_family = {
	.id		= GENL_ID_GENERATE,
	.hdrsize	= 0,
	.name		= IWL_TM_GNL_FAMILY_NAME,
	.version	= IWL_TM_GNL_VERSION_NR,
	.maxattr	= IWL_TM_GNL_MSG_ATTR_MAX,
};

static __genl_const struct genl_multicast_group iwl_tm_gnl_mcgrps[] = {
	{ .name = IWL_TM_GNL_MC_GRP_NAME, },
};

/* TM GNL bus policy */
static const struct nla_policy
iwl_tm_gnl_msg_policy[IWL_TM_GNL_MSG_ATTR_MAX] = {
	[IWL_TM_GNL_MSG_ATTR_DEVNAME] =	{
			.type = NLA_NUL_STRING,
			.len = IWL_TM_GNL_DEVNAME_LEN-1 },
	[IWL_TM_GNL_MSG_ATTR_CMD]  = { .type = NLA_U32, },
	[IWL_TM_GNL_MSG_ATTR_DATA] = { .type = NLA_BINARY, },
};

/**
 * iwl_tm_gnl_get_dev() - Retrieve device information
 * @dev_name:	Device to be found
 *
 * Finds the device information according to device name,
 * must be protected by list mutex when used (mutex is not
 * locked inside the function to allow code flexability)
 */
static struct iwl_tm_gnl_dev *iwl_tm_gnl_get_dev(const char *dev_name)
{
	struct iwl_tm_gnl_dev *dev_itr, *dev = NULL;

	lockdep_assert_held(&dev_list_mtx);

	list_for_each_entry(dev_itr, &dev_list, list) {
		if (!strcmp(dev_itr->dev_name, dev_name)) {
			dev = dev_itr;
			break;
		}
	}

	return dev;
}

/**
 * iwl_tm_gnl_create_message() - Creates a genl message
 * @pid:	Netlink PID that the message is addressed to
 * @seq:	sequence number (usually the one of the sender)
 * @cmd_data:	Message's data
 * @flags:	Resources allocation flags
 */
static struct sk_buff *iwl_tm_gnl_create_msg(u32 pid, u32 seq,
					     struct iwl_tm_gnl_cmd cmd_data,
					     gfp_t flags)
{
	void *nlmsg_head;
	struct sk_buff *skb;
	int ret;

	skb = genlmsg_new(NLMSG_GOODSIZE, flags);
	if (!skb)
		goto send_msg_err;

	nlmsg_head = genlmsg_put(skb, pid, seq,
				 &iwl_tm_gnl_family, 0,
				 IWL_TM_GNL_CMD_EXECUTE);
	if (!nlmsg_head)
		goto send_msg_err;

	ret = nla_put_string(skb, IWL_TM_GNL_MSG_ATTR_DEVNAME,
			     cmd_data.dev_name);
	if (ret)
		goto send_msg_err;

	ret = nla_put_u32(skb, IWL_TM_GNL_MSG_ATTR_CMD,
			  cmd_data.cmd);
	if (ret)
		goto send_msg_err;

	if (cmd_data.data_out.len && cmd_data.data_out.data) {
		ret = nla_put(skb, IWL_TM_GNL_MSG_ATTR_DATA,
			cmd_data.data_out.len, cmd_data.data_out.data);
		if (ret)
			goto send_msg_err;
	}

	genlmsg_end(skb, nlmsg_head);

	return skb;

send_msg_err:
	if (skb)
		kfree_skb(skb);

	return NULL;
}

/**
 * iwl_tm_gnl_send_msg() - Sends a message to mcast or userspace listener
 * @trans:	transport
 * @cmd:	Command index
 * @check_notify: only send when notify is set
 * @data_out:	Data to be sent
 *
 * Initiate a message sending to user space (as apposed
 * to replying to a message that was initiated by user
 * space). Uses multicast broadcasting method.
 */
int iwl_tm_gnl_send_msg(struct iwl_trans *trans, u32 cmd, bool check_notify,
			void *data_out, u32 data_len, gfp_t flags)
{
	struct iwl_tm_gnl_dev *dev;
	struct iwl_tm_gnl_cmd cmd_data;
	struct sk_buff *skb;
	u32 nlportid;

	if (WARN_ON_ONCE(!trans))
		return -EINVAL;

	if (!trans->tmdev)
		return 0;
	dev = trans->tmdev;

	nlportid = ACCESS_ONCE(dev->nl_events_portid);

	if (check_notify && !dev->tst.notify)
		return 0;

	memset(&cmd_data, 0 , sizeof(struct iwl_tm_gnl_cmd));
	cmd_data.dev_name = dev_name(trans->dev);
	cmd_data.cmd = cmd;
	cmd_data.data_out.data = data_out;
	cmd_data.data_out.len = data_len;

	skb = iwl_tm_gnl_create_msg(nlportid, 0, cmd_data, flags);
	if (!skb)
		return -EINVAL;

	if (nlportid)
		return genlmsg_unicast(&init_net, skb, nlportid);
	return genlmsg_multicast(&iwl_tm_gnl_family, skb, 0, 0, flags);
}
IWL_EXPORT_SYMBOL(iwl_tm_gnl_send_msg);

/**
 * iwl_tm_gnl_reply() - Sends command's results back to user space
 * @info:	info struct received in .doit callback
 * @cmd_data:	Data of command to be responded
 */
static int iwl_tm_gnl_reply(struct genl_info *info,
			    struct iwl_tm_gnl_cmd cmd_data)
{
	struct sk_buff *skb;

	skb = iwl_tm_gnl_create_msg(genl_info_snd_portid(info), info->snd_seq,
				    cmd_data, GFP_KERNEL);
	if (!skb)
		return -EINVAL;

	return genlmsg_reply(skb, info);
}

static int iwl_op_mode_tm_execute_cmd(struct iwl_tm_gnl_dev *dev,
				      u32 cmd,
				      struct iwl_tm_data *data_in,
				      struct iwl_tm_data *data_out)
{
	const struct iwl_test_ops *test_ops;

	if (!dev->trans->op_mode) {
		IWL_ERR(dev->trans, "No op_mode!\n");
		return -ENODEV;
	}

	test_ops = &dev->trans->op_mode->ops->test_ops;

	if (test_ops->cmd_execute)
		return test_ops->cmd_execute(dev->trans->op_mode,
					     cmd, data_in, data_out);

	return -EOPNOTSUPP;
}

/**
 * iwl_tm_gnl_cmd_execute() - Execute IWL testmode GNL command
 * @cmd_data:	Pointer to the data of command to be executed
 */
static int iwl_tm_gnl_cmd_execute(struct iwl_tm_gnl_cmd *cmd_data)
{
	struct iwl_tm_gnl_dev *dev;
	bool common_op = false;
	int ret = 0;
	mutex_lock(&dev_list_mtx);
	dev = iwl_tm_gnl_get_dev(cmd_data->dev_name);
	mutex_unlock(&dev_list_mtx);
	if (!dev)
		return -ENODEV;

	IWL_DEBUG_INFO(dev->trans, "%s cmd=0x%X\n", __func__, cmd_data->cmd);
	switch (cmd_data->cmd) {

	case IWL_TM_USER_CMD_HCMD:
		ret = iwl_tm_validate_fw_cmd(&cmd_data->data_in);
		break;

	case IWL_TM_USER_CMD_REG_ACCESS:
		ret = iwl_tm_validate_reg_ops(&cmd_data->data_in);
		break;

	case IWL_TM_USER_CMD_SRAM_WRITE:
		ret = iwl_tm_validate_sram_write_req(dev, &cmd_data->data_in);
		break;

	case IWL_TM_USER_CMD_BEGIN_TRACE:
		ret = iwl_tm_trace_begin(dev,
					 &cmd_data->data_in,
					 &cmd_data->data_out);
		common_op = true;
		break;

	case IWL_TM_USER_CMD_END_TRACE:
		ret = iwl_tm_trace_end(dev);
		common_op = true;
		break;

	case IWL_XVT_CMD_MOD_TX:
		ret = iwl_tm_validate_tx_cmd(&cmd_data->data_in);
		break;

	case IWL_XVT_CMD_RX_HDRS_MODE:
		ret =  iwl_tm_validate_rx_hdrs_mode_req(&cmd_data->data_in);
		break;

	case IWL_XVT_CMD_APMG_PD_MODE:
		ret =  iwl_tm_validate_apmg_pd_mode_req(&cmd_data->data_in);
		break;

	case IWL_TM_USER_CMD_NOTIFICATIONS:
		ret = iwl_tm_notifications_en(&dev->tst, &cmd_data->data_in);
		common_op = true;
		break;

	case IWL_TM_USER_CMD_GET_DEVICE_STATUS:
		ret = iwl_tm_get_device_status(dev, &cmd_data->data_in,
					       &cmd_data->data_out);
		break;
#if IS_ENABLED(CPTCFG_IWLXVT)
	case IWL_TM_USER_CMD_SWITCH_OP_MODE:
		ret = iwl_tm_switch_op_mode(dev, &cmd_data->data_in);
		common_op = true;
		break;
#endif
	case IWL_XVT_CMD_GET_CHIP_ID:
		ret = iwl_tm_validate_get_chip_id(dev->trans);
		break;

	case IWL_TM_USER_CMD_GET_SIL_STEP:
		ret = iwl_tm_gnl_get_sil_step(dev->trans, &cmd_data->data_out);
		common_op = true;
		break;

	case IWL_TM_USER_CMD_GET_DRIVER_BUILD_INFO:
		ret = iwl_tm_gnl_get_build_info(dev->trans,
						&cmd_data->data_out);
		common_op = true;
		break;
	}
	if (ret) {
		IWL_ERR(dev->trans, "%s Error=%d\n", __func__, ret);
		return ret;
	}

	if (!common_op)
		ret = iwl_op_mode_tm_execute_cmd(dev, cmd_data->cmd,
						 &cmd_data->data_in,
						 &cmd_data->data_out);

	if (ret)
		IWL_ERR(dev->trans, "%s ret=%d\n", __func__, ret);
	else
		IWL_DEBUG_INFO(dev->trans, "%s ended Ok\n", __func__);
	return ret;
}

/**
 * iwl_tm_mem_dump() - Returns memory buffer data
 * @dev:	testmode device struct
 * @data_in:	input data
 * @data_out:	Dump data
 */
static int iwl_tm_mem_dump(struct iwl_tm_gnl_dev *dev,
			   struct iwl_tm_data *data_in,
			   struct iwl_tm_data *data_out)
{
	int ret;

	ret = iwl_tm_validate_sram_read_req(dev, data_in);
	if (ret)
		return ret;

	return iwl_op_mode_tm_execute_cmd(dev, IWL_TM_USER_CMD_SRAM_READ,
					  data_in, data_out);
}

/**
 * iwl_tm_trace_dump() - Returns trace buffer data
 * @tst:	Device's test data
 * @data_out:	Dump data
 */
static int iwl_tm_trace_dump(struct iwl_tm_gnl_dev *dev,
			     struct iwl_tm_data *data_out)
{
	int ret;
	u32 buf_size;

	if (!(dev->dnt->iwl_dnt_status & IWL_DNT_STATUS_MON_CONFIGURED)) {
		IWL_ERR(dev->trans, "Invalid monitor status\n");
		return -EINVAL;
	}

	if (dev->dnt->mon_buf_size == 0) {
		IWL_ERR(dev->trans, "No available monitor buffer\n");
		return -ENOMEM;
	}

	buf_size = dev->dnt->mon_buf_size;
	data_out->data =  kmalloc(buf_size, GFP_KERNEL);
	if (!data_out->data)
		return -ENOMEM;

	ret = iwl_dnt_dispatch_pull(dev->trans, data_out->data,
				    buf_size, MONITOR);
	if (ret < 0) {
		kfree(data_out->data);
		return ret;
	}
	data_out->len = ret;

	return 0;
}

/**
 * iwl_tm_gnl_command_dump() - Returns dump buffer data
 * @cmd_data:	Pointer to the data of dump command.
 *		Only device name and command index are the relevant input.
 *		Data out is the start address of the buffer, and it's size.
 */
static int iwl_tm_gnl_command_dump(struct iwl_tm_gnl_cmd *cmd_data)
{
	struct iwl_tm_gnl_dev *dev;
	int ret = 0;

	mutex_lock(&dev_list_mtx);
	dev = iwl_tm_gnl_get_dev(cmd_data->dev_name);
	mutex_unlock(&dev_list_mtx);
	if (!dev)
		return -ENODEV;

	switch (cmd_data->cmd) {

	case IWL_TM_USER_CMD_TRACE_DUMP:
		ret = iwl_tm_trace_dump(dev, &cmd_data->data_out);
		break;

	case IWL_TM_USER_CMD_SRAM_READ:
		ret = iwl_tm_mem_dump(dev,
				      &cmd_data->data_in,
				      &cmd_data->data_out);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return ret;
}

/**
 * iwl_tm_gnl_parse_msg - Extract input cmd data out of netlink attributes
 * @attrs:	Input netlink attributes
 * @cmd_data:	Command
 */
static int iwl_tm_gnl_parse_msg(struct nlattr **attrs,
				struct iwl_tm_gnl_cmd *cmd_data)
{
	memset(cmd_data, 0 , sizeof(struct iwl_tm_gnl_cmd));

	if (!attrs[IWL_TM_GNL_MSG_ATTR_DEVNAME] ||
	    !attrs[IWL_TM_GNL_MSG_ATTR_CMD])
		return -EINVAL;

	cmd_data->dev_name = nla_data(attrs[IWL_TM_GNL_MSG_ATTR_DEVNAME]);
	cmd_data->cmd = nla_get_u32(attrs[IWL_TM_GNL_MSG_ATTR_CMD]);

	if (attrs[IWL_TM_GNL_MSG_ATTR_DATA]) {
		cmd_data->data_in.data =
			nla_data(attrs[IWL_TM_GNL_MSG_ATTR_DATA]);
		cmd_data->data_in.len =
			nla_len(attrs[IWL_TM_GNL_MSG_ATTR_DATA]);
	}

	return 0;
}

/**
 * iwl_tm_gnl_cmd_do() - Executes IWL testmode GNL command
 */
static int iwl_tm_gnl_cmd_do(struct sk_buff *skb, struct genl_info *info)
{
	struct iwl_tm_gnl_cmd cmd_data;
	int ret;

	ret = iwl_tm_gnl_parse_msg(info->attrs, &cmd_data);
	if (ret)
		return ret;

	ret = iwl_tm_gnl_cmd_execute(&cmd_data);
	if (!ret && cmd_data.data_out.len) {
		ret = iwl_tm_gnl_reply(info, cmd_data);
		/*
		 * In this case, data out should be allocated in
		 * iwl_tm_gnl_cmd_execute so it should be freed
		 * here
		 */
		kfree(cmd_data.data_out.data);
	}

	return ret;
}

/**
 * iwl_tm_gnl_dump() - Executes IWL testmode GNL command
 * cb->args[0]:	Command number, incremented by one (as it may be zero)
 *		We're keeping the command so we can tell if is it the
 *		first dump call or not.
 * cb->args[1]:	Buffer data to dump
 * cb->args[2]:	Buffer size
 * cb->args[3]: Buffer offset from where to dump in the next round
 */
static int iwl_tm_gnl_dump(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct iwl_tm_gnl_cmd cmd_data;
	void *nlmsg_head = NULL;
	struct nlattr *attrs[IWL_TM_GNL_MSG_ATTR_MAX];
	void *dump_addr;
	unsigned long dump_offset;
	int dump_size, chunk_size, ret;

	if (!cb->args[0]) {
		/*
		 * This is the first part of the dump - Parse dump data
		 * out of the data in the netlink header and set up the
		 * dump in cb->args[].
		 */
		ret = nlmsg_parse(cb->nlh, GENL_HDRLEN, attrs,
				  IWL_TM_GNL_MSG_ATTR_MAX - 1,
				  iwl_tm_gnl_msg_policy);
		if (ret)
			return ret;

		ret = iwl_tm_gnl_parse_msg(attrs, &cmd_data);
		if (ret)
			return ret;

		ret = iwl_tm_gnl_command_dump(&cmd_data);
		if (ret)
			return ret;

		/* Incrementing command since command number may be zero */
		cb->args[0] = cmd_data.cmd + 1;
		cb->args[1] = (unsigned long)cmd_data.data_out.data;
		cb->args[2] = cmd_data.data_out.len;
		cb->args[3] = 0;

		if (!cb->args[2])
			return -ENODATA;
	}

	dump_addr = (u8 *)cb->args[1];
	dump_size = cb->args[2];
	dump_offset = cb->args[3];

	nlmsg_head = genlmsg_put(skb, NETLINK_CB_PORTID(cb->skb),
				cb->nlh->nlmsg_seq,
				&iwl_tm_gnl_family, NLM_F_MULTI,
				IWL_TM_GNL_CMD_EXECUTE);

	/*
	 * Reserve some room for NL attribute header,
	 * 16 bytes should be enough.
	 */
	chunk_size = skb_tailroom(skb) - 16;
	if (chunk_size <= 0) {
		ret = -ENOMEM;
		goto dump_err;
	}

	if (chunk_size > dump_size - dump_offset)
		chunk_size = dump_size - dump_offset;

	if (chunk_size) {
		ret = nla_put(skb, IWL_TM_GNL_MSG_ATTR_DATA,
			      chunk_size, dump_addr + dump_offset);
		if (ret)
			goto dump_err;
	}

	genlmsg_end(skb, nlmsg_head);

	/* move offset */
	cb->args[3] += chunk_size;

	return cb->args[2] - cb->args[3];

dump_err:
	genlmsg_cancel(skb, nlmsg_head);
	return ret;
}

static int iwl_tm_gnl_done(struct netlink_callback *cb)
{
	switch (cb->args[0] - 1) {
	case IWL_TM_USER_CMD_SRAM_READ:
	case IWL_TM_USER_CMD_TRACE_DUMP:
		kfree((void *)cb->args[1]);
		return 0;
	}

	return -EOPNOTSUPP;
}

static int iwl_tm_gnl_cmd_subscribe(struct sk_buff *skb, struct genl_info *info)
{
	struct iwl_tm_gnl_dev *dev;
	const char *dev_name;
	int ret;

	if (!info->attrs[IWL_TM_GNL_MSG_ATTR_DEVNAME])
		return -EINVAL;

	dev_name = nla_data(info->attrs[IWL_TM_GNL_MSG_ATTR_DEVNAME]);

	mutex_lock(&dev_list_mtx);
	dev = iwl_tm_gnl_get_dev(dev_name);
	if (!dev) {
		ret = -ENODEV;
		goto unlock;
	}

	if (dev->nl_events_portid) {
		ret = -EBUSY;
		goto unlock;
	}

	dev->nl_events_portid = genl_info_snd_portid(info);
	ret = 0;

 unlock:
	mutex_unlock(&dev_list_mtx);
	return ret;
}

/*
 * iwl_tm_gnl_ops - GNL Family commands.
 * There is only one NL command, and only one callback,
 * which handles all NL messages.
 */
static __genl_const struct genl_ops iwl_tm_gnl_ops[] = {
	{
	  .cmd = IWL_TM_GNL_CMD_EXECUTE,
	  .policy = iwl_tm_gnl_msg_policy,
	  .doit = iwl_tm_gnl_cmd_do,
	  .dumpit = iwl_tm_gnl_dump,
	  .done = iwl_tm_gnl_done,
	},
	{
		.cmd = IWL_TM_GNL_CMD_SUBSCRIBE_EVENTS,
		.policy = iwl_tm_gnl_msg_policy,
		.doit = iwl_tm_gnl_cmd_subscribe,
	},
};

/**
 * iwl_tm_gnl_add() - Registers a devices/op-mode in the iwl-tm-gnl list
 * @trans:	transport struct for the device to register for
 */
void iwl_tm_gnl_add(struct iwl_trans *trans)
{
	struct iwl_tm_gnl_dev *dev;

	if (!trans)
		return;

	if (trans->tmdev)
		return;

	mutex_lock(&dev_list_mtx);

	if (iwl_tm_gnl_get_dev(dev_name(trans->dev)))
		goto unlock;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		goto unlock;

	dev->dev_name = dev_name(trans->dev);
	trans->tmdev = dev;
	dev->trans = trans;
	list_add_tail_rcu(&dev->list, &dev_list);

unlock:
	mutex_unlock(&dev_list_mtx);
}

/**
 * iwl_tm_gnl_remove() - Unregisters a devices/op-mode in the iwl-tm-gnl list
 * @trans:	transport struct for the device
 */
void iwl_tm_gnl_remove(struct iwl_trans *trans)
{
	struct iwl_tm_gnl_dev *dev_itr, *tmp;

	if (WARN_ON_ONCE(!trans))
		return;

	/* Searching for operation mode in list */
	mutex_lock(&dev_list_mtx);
	list_for_each_entry_safe(dev_itr, tmp, &dev_list, list) {
		if (dev_itr->trans == trans) {
			/*
			 * Device found. Removing it from list
			 * and releasing it's resources
			 */
			list_del_rcu(&dev_itr->list);
			synchronize_rcu();
			kfree(dev_itr);
			break;
		}
	}

	trans->tmdev = NULL;
	mutex_unlock(&dev_list_mtx);
}

static int iwl_tm_gnl_netlink_notify(struct notifier_block *nb,
				     unsigned long state,
				     void *_notify)
{
	struct netlink_notify *notify = _notify;
	struct iwl_tm_gnl_dev *dev;

	rcu_read_lock();
	list_for_each_entry_rcu(dev, &dev_list, list) {
		if (dev->nl_events_portid == netlink_notify_portid(notify))
			dev->nl_events_portid = 0;
	}
	rcu_read_unlock();

	return NOTIFY_OK;
}

static struct notifier_block iwl_tm_gnl_netlink_notifier = {
	.notifier_call = iwl_tm_gnl_netlink_notify,
};


/**
 * iwl_tm_gnl_init() - Registers tm-gnl module
 *
 * Registers Testmode GNL family and initializes
 * TM GNL global variables
 */
int iwl_tm_gnl_init(void)
{
	int ret;

	INIT_LIST_HEAD(&dev_list);
	mutex_init(&dev_list_mtx);

	ret = genl_register_family_with_ops_groups(&iwl_tm_gnl_family,
						   iwl_tm_gnl_ops,
						   iwl_tm_gnl_mcgrps);
	if (ret)
		return ret;
	ret = netlink_register_notifier(&iwl_tm_gnl_netlink_notifier);
	if (ret)
		genl_unregister_family(&iwl_tm_gnl_family);
	return ret;
}

/**
 * iwl_tm_gnl_exit() - Unregisters Testmode GNL family
 */
int iwl_tm_gnl_exit(void)
{
	netlink_unregister_notifier(&iwl_tm_gnl_netlink_notifier);
	return genl_unregister_family(&iwl_tm_gnl_family);
}
