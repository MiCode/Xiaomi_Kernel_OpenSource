/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/completion.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/pm_runtime.h>

#include "mhi_sys.h"
#include "mhi.h"
#include "mhi_hwio.h"
#include "mhi_macros.h"
#include "mhi_bhi.h"
#include "mhi_trace.h"

static int enable_bb_ctxt(struct mhi_device_ctxt *mhi_dev_ctxt,
			  struct mhi_ring *bb_ctxt,
			  int nr_el,
			  int chan,
			  size_t max_payload)
{
	int i;
	struct mhi_buf_info *mhi_buf_info;

	bb_ctxt->el_size = sizeof(struct mhi_buf_info);
	bb_ctxt->len     = bb_ctxt->el_size * nr_el;
	bb_ctxt->base    = kzalloc(bb_ctxt->len, GFP_KERNEL);
	bb_ctxt->wp	 = bb_ctxt->base;
	bb_ctxt->rp	 = bb_ctxt->base;
	bb_ctxt->ack_rp  = bb_ctxt->base;
	if (!bb_ctxt->base)
		return -ENOMEM;

	if (mhi_dev_ctxt->flags.bb_required) {
		char pool_name[32];

		snprintf(pool_name, sizeof(pool_name), "mhi%d_%d",
			 mhi_dev_ctxt->plat_dev->id, chan);

		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Creating pool %s for chan:%d payload: 0x%lx\n",
			pool_name, chan, max_payload);

		bb_ctxt->dma_pool = dma_pool_create(pool_name,
			&mhi_dev_ctxt->plat_dev->dev, max_payload, 0, 0);
		if (unlikely(!bb_ctxt->dma_pool))
			goto dma_pool_error;

		mhi_buf_info = (struct mhi_buf_info *)bb_ctxt->base;
		for (i = 0; i < nr_el; i++, mhi_buf_info++) {
			mhi_buf_info->pre_alloc_v_addr =
				dma_pool_alloc(bb_ctxt->dma_pool, GFP_KERNEL,
					       &mhi_buf_info->pre_alloc_p_addr);
			if (unlikely(!mhi_buf_info->pre_alloc_v_addr))
				goto dma_alloc_error;
			mhi_buf_info->pre_alloc_len = max_payload;
		}
	}

	return 0;

dma_alloc_error:
	for (--i, --mhi_buf_info; i >= 0; i--, mhi_buf_info--)
		dma_pool_free(bb_ctxt->dma_pool, mhi_buf_info->pre_alloc_v_addr,
			      mhi_buf_info->pre_alloc_p_addr);

	dma_pool_destroy(bb_ctxt->dma_pool);
	bb_ctxt->dma_pool = NULL;
dma_pool_error:
	kfree(bb_ctxt->base);
	bb_ctxt->base = NULL;
	return -ENOMEM;
}

static void mhi_write_db(struct mhi_device_ctxt *mhi_dev_ctxt,
			 void __iomem *io_addr_lower,
			 unsigned int chan,
			 dma_addr_t val)
{
	uintptr_t io_offset = chan * sizeof(u64);
	void __iomem *io_addr_upper =
		(void __iomem *)((uintptr_t)io_addr_lower + 4);
	mhi_reg_write(mhi_dev_ctxt, io_addr_upper, io_offset, val >> 32);
	mhi_reg_write(mhi_dev_ctxt, io_addr_lower, io_offset, (u32)val);
}

static void mhi_update_ctxt(struct mhi_device_ctxt *mhi_dev_ctxt,
			    void *__iomem *io_addr,
			    uintptr_t chan,
			    u64 val)
{
	if (mhi_dev_ctxt->mmio_info.chan_db_addr == io_addr) {
		mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[chan].
				mhi_trb_write_ptr = val;
	} else if (mhi_dev_ctxt->mmio_info.event_db_addr == io_addr) {
		if (chan < mhi_dev_ctxt->mmio_info.nr_event_rings) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"EV ctxt:  %ld val 0x%llx WP 0x%llx RP: 0x%llx",
				chan, val,
				mhi_dev_ctxt->dev_space.ring_ctxt.
					ec_list[chan].mhi_event_read_ptr,
				mhi_dev_ctxt->dev_space.ring_ctxt.
					ec_list[chan].mhi_event_write_ptr);
			mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[chan].
				mhi_event_write_ptr = val;
		} else {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Bad EV ring index: %lx\n", chan);
		}
	}
	/* Flush ctxt update to main memory for device visibility */
	wmb();
}

int mhi_init_pcie_device(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret_val = 0;
	long int sleep_time = 100;
	struct pci_dev *pcie_device = mhi_dev_ctxt->pcie_device;
	struct pcie_core_info *core = &mhi_dev_ctxt->core;

	do {
		ret_val = pci_enable_device(pcie_device);
		if (0 != ret_val) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Failed to enable pcie struct device r: %d\n",
				ret_val);
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Sleeping for ~ %li uS, and retrying.\n",
				sleep_time);
			msleep(sleep_time);
		}
	} while (ret_val != 0);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Successfully enabled pcie device.\n");

	core->bar0_base = ioremap_nocache(pci_resource_start(pcie_device, 0),
					  pci_resource_len(pcie_device, 0));
	if (!core->bar0_base)
		goto mhi_device_list_error;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Device BAR0 address is at 0x%p\n", core->bar0_base);

	ret_val = pci_request_region(pcie_device, 0, "mhi");
	if (ret_val)
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Could not request BAR0 region\n");

	core->manufact_id = pcie_device->vendor;
	core->dev_id = pcie_device->device;
	return 0;

mhi_device_list_error:
	pci_disable_device(pcie_device);
	return -EIO;
}

static void mhi_move_interrupts(struct mhi_device_ctxt *mhi_dev_ctxt, u32 cpu)
{
	u32 irq_to_affin = 0;
	int i;

	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; ++i) {
		if (MHI_ER_DATA_TYPE ==
			GET_EV_PROPS(EV_TYPE,
				mhi_dev_ctxt->ev_ring_props[i].flags)) {
			irq_to_affin = mhi_dev_ctxt->ev_ring_props[i].msi_vec;
			irq_to_affin += mhi_dev_ctxt->core.irq_base;
			irq_set_affinity(irq_to_affin, get_cpu_mask(cpu));
		}
	}
}

int mhi_cpu_notifier_cb(struct notifier_block *nfb, unsigned long action,
			void *hcpu)
{
	uintptr_t cpu = (uintptr_t)hcpu;
	struct mhi_device_ctxt *mhi_dev_ctxt = container_of(nfb,
						struct mhi_device_ctxt,
						mhi_cpu_notifier);

	switch (action) {
	case CPU_ONLINE:
		if (cpu > 0)
			mhi_move_interrupts(mhi_dev_ctxt, cpu);
		break;

	case CPU_DEAD:
		for_each_online_cpu(cpu) {
			if (cpu > 0) {
				mhi_move_interrupts(mhi_dev_ctxt, cpu);
				break;
			}
		}
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

int get_chan_props(struct mhi_device_ctxt *mhi_dev_ctxt, int chan,
		   struct mhi_chan_info *chan_info)
{
	char dt_prop[MAX_BUF_SIZE];
	int r;

	scnprintf(dt_prop, MAX_BUF_SIZE, "%s%d", "mhi-chan-cfg-", chan);
	r = of_property_read_u32_array(
			mhi_dev_ctxt->plat_dev->dev.of_node,
			dt_prop,
			(u32 *)chan_info,
			sizeof(struct mhi_chan_info) / sizeof(u32));
	return r;
}

int mhi_release_chan_ctxt(struct mhi_device_ctxt *mhi_dev_ctxt,
			  struct mhi_chan_ctxt *cc_list,
			  struct mhi_ring *ring)
{
	if (cc_list == NULL || ring == NULL)
		return -EINVAL;

	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev,
			  ring->len,
			  ring->base,
			  cc_list->mhi_trb_ring_base_addr);
	mhi_init_chan_ctxt(cc_list, 0, 0, 0, 0, 0, ring,
			   MHI_CHAN_STATE_DISABLED,
			   false,
			   MHI_BRSTMODE_DEFAULT);
	return 0;
}

void free_tre_ring(struct mhi_device_ctxt *mhi_dev_ctxt, int chan)
{
	struct mhi_chan_ctxt *chan_ctxt;
	int r;

	chan_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[chan];
	r = mhi_release_chan_ctxt(mhi_dev_ctxt, chan_ctxt,
				&mhi_dev_ctxt->mhi_local_chan_ctxt[chan]);
	if (r)
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to release chan %d ret %d\n", chan, r);
}

static int populate_tre_ring(struct mhi_client_config *client_config)
{
	dma_addr_t ring_dma_addr;
	void *ring_local_addr;
	struct mhi_chan_ctxt *chan_ctxt;
	struct mhi_device_ctxt *mhi_dev_ctxt = client_config->mhi_dev_ctxt;
	u32 chan = client_config->chan_info.chan_nr;
	u32 nr_desc = client_config->chan_info.max_desc;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Entered chan %d requested desc %d\n", chan, nr_desc);

	chan_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[chan];
	ring_local_addr =
		dma_alloc_coherent(&mhi_dev_ctxt->plat_dev->dev,
				   nr_desc * sizeof(union mhi_xfer_pkt),
				   &ring_dma_addr,
				   GFP_KERNEL);

	if (ring_local_addr == NULL)
		return -ENOMEM;

	mhi_init_chan_ctxt(chan_ctxt, ring_dma_addr,
			   (uintptr_t)ring_local_addr,
			   nr_desc,
			   GET_CHAN_PROPS(CHAN_DIR,
				client_config->chan_info.flags),
			   client_config->chan_info.ev_ring,
			   &mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
			   MHI_CHAN_STATE_ENABLED,
			   GET_CHAN_PROPS(PRESERVE_DB_STATE,
					  client_config->chan_info.flags),
			   GET_CHAN_PROPS(BRSTMODE,
					  client_config->chan_info.flags));
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Exited\n");
	return 0;
}

void mhi_notify_client(struct mhi_client_handle *client_handle,
		       enum MHI_CB_REASON reason)
{
	struct mhi_cb_info cb_info = {0};
	struct mhi_result result = {0};
	struct mhi_client_config *client_config;

	cb_info.result = NULL;
	cb_info.cb_reason = reason;

	if (client_handle == NULL)
		return;

	client_config = client_handle->client_config;

	if (client_config->client_info.mhi_client_cb) {
		result.user_data = client_config->user_data;
		cb_info.chan = client_config->chan_info.chan_nr;
		cb_info.result = &result;
		mhi_log(client_config->mhi_dev_ctxt, MHI_MSG_INFO,
			"Calling back for chan %d, reason %d\n",
			cb_info.chan,
			reason);
		client_config->client_info.mhi_client_cb(&cb_info);
	}
}

void mhi_notify_clients(struct mhi_device_ctxt *mhi_dev_ctxt,
					enum MHI_CB_REASON reason)
{
	int i;
	struct mhi_client_handle *client_handle = NULL;

	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
		if (VALID_CHAN_NR(i)) {
			client_handle = mhi_dev_ctxt->client_handle_list[i];
			mhi_notify_client(client_handle, reason);
		}
	}
}

int mhi_open_channel(struct mhi_client_handle *client_handle)
{
	int ret_val = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	struct mhi_ring *chan_ring;
	int chan;
	struct mhi_chan_cfg *cfg;
	struct mhi_cmd_complete_event_pkt cmd_event_pkt;
	union mhi_cmd_pkt cmd_pkt;
	enum MHI_EVENT_CCS ev_code;
	struct mhi_client_config *client_config = client_handle->client_config;

	if (client_config->magic != MHI_HANDLE_MAGIC)
		return -EINVAL;

	mhi_dev_ctxt = client_config->mhi_dev_ctxt;

	chan = client_config->chan_info.chan_nr;
	cfg = &mhi_dev_ctxt->mhi_chan_cfg[chan];
	chan_ring = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	mutex_lock(&cfg->chan_lock);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Entered: Client opening chan 0x%x\n", chan);
	if (mhi_dev_ctxt->dev_exec_env <
		GET_CHAN_PROPS(CHAN_BRINGUP_STAGE,
			       client_config->chan_info.flags)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Chan %d, MHI exec_env %d, not ready!\n",
			chan, mhi_dev_ctxt->dev_exec_env);
		mutex_unlock(&cfg->chan_lock);
		return -ENOTCONN;
	}
	ret_val = populate_tre_ring(client_config);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to initialize tre ring chan %d ret %d\n",
			chan, ret_val);
		goto error_tre_ring;
	}
	client_config->event_ring_index =
		mhi_dev_ctxt->dev_space.ring_ctxt.
				cc_list[chan].mhi_event_ring_index;

	client_config->msi_vec =
		mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[
			client_config->event_ring_index].mhi_msi_vector;
	client_config->intmod_t =
		mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[
			client_config->event_ring_index].mhi_intmodt;

	init_completion(&cfg->cmd_complete);
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	if (unlikely(mhi_dev_ctxt->mhi_pm_state == MHI_PM_DISABLE)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"MHI State is disabled\n");
		read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
		ret_val = -EIO;
		goto error_pm_state;
	}

	mhi_dev_ctxt->assert_wake(mhi_dev_ctxt, false);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->runtime_get(mhi_dev_ctxt);

	ret_val = mhi_send_cmd(client_config->mhi_dev_ctxt,
			       MHI_COMMAND_START_CHAN,
			       chan);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to send start cmd for chan %d ret %d\n",
			chan, ret_val);
		goto error_completion;
	}
	ret_val = wait_for_completion_timeout(&cfg->cmd_complete,
				msecs_to_jiffies(MHI_MAX_CMD_TIMEOUT));
	if (!ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to receive cmd completion for %d\n", chan);
		ret_val = -EIO;
		goto error_completion;
	} else {
		ret_val = 0;
	}

	spin_lock_irq(&cfg->event_lock);
	cmd_event_pkt = cfg->cmd_event_pkt;
	cmd_pkt = cfg->cmd_pkt;
	spin_unlock_irq(&cfg->event_lock);

	ev_code = MHI_EV_READ_CODE(EV_TRB_CODE,
				   ((union mhi_event_pkt *)&cmd_event_pkt));
	if (ev_code != MHI_EVENT_CC_SUCCESS) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Error to receive event comp. ev_code:0x%x\n", ev_code);
		ret_val = -EIO;
		goto error_completion;
	}

	spin_lock_irq(&chan_ring->ring_lock);
	chan_ring->ch_state = MHI_CHAN_STATE_ENABLED;
	spin_unlock_irq(&chan_ring->ring_lock);
	client_config->chan_status = 1;

	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
	mutex_unlock(&cfg->chan_lock);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"chan:%d opened successfully\n", chan);
	return 0;

error_completion:
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
error_pm_state:
	free_tre_ring(mhi_dev_ctxt, chan);
error_tre_ring:
	mutex_unlock(&cfg->chan_lock);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Exited chan 0x%x ret:%d\n", chan, ret_val);
	return ret_val;
}
EXPORT_SYMBOL(mhi_open_channel);

bool mhi_is_device_ready(const struct device const *dev,
			 const char *node_name)
{
	struct mhi_device_ctxt *itr;
	const struct device_node const *of_node;
	bool match_found = false;

	if (!mhi_device_drv)
		return false;
	if (dev->of_node == NULL)
		return false;

	of_node = of_parse_phandle(dev->of_node, node_name, 0);
	if (!of_node)
		return false;

	mutex_lock(&mhi_device_drv->lock);
	list_for_each_entry(itr, &mhi_device_drv->head, node) {
		struct platform_device *pdev = itr->plat_dev;

		if (pdev->dev.of_node == of_node) {
			match_found = true;
			break;
		}
	}
	mutex_unlock(&mhi_device_drv->lock);
	return match_found;
}
EXPORT_SYMBOL(mhi_is_device_ready);

int mhi_register_channel(struct mhi_client_handle **client_handle,
			 struct mhi_client_info_t *client_info)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL, *itr;
	const struct device_node const *of_node;
	struct mhi_client_config *client_config;
	const char *node_name;
	enum MHI_CLIENT_CHANNEL chan;
	struct mhi_chan_info chan_info = {0};
	int ret;

	if (!client_info || client_info->dev->of_node == NULL)
		return -EINVAL;

	node_name = client_info->node_name;
	chan = client_info->chan;
	of_node = of_parse_phandle(client_info->dev->of_node, node_name, 0);
	if (!of_node || !mhi_device_drv || chan >= MHI_MAX_CHANNELS)
		return -EINVAL;

	/* Traverse thru the list */
	mutex_lock(&mhi_device_drv->lock);
	list_for_each_entry(itr, &mhi_device_drv->head, node) {
		struct platform_device *pdev = itr->plat_dev;

		if (pdev->dev.of_node == of_node) {
			mhi_dev_ctxt = itr;
			break;
		}
	}
	mutex_unlock(&mhi_device_drv->lock);

	if (!mhi_dev_ctxt)
		return -EINVAL;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Registering channel 0x%x for client\n", chan);

	/* check if it's a supported channel by endpoint */
	ret = get_chan_props(mhi_dev_ctxt, chan, &chan_info);
	if (ret) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Client try to register unsupported chan:%d\n", chan);
		return -EINVAL;
	}

	*client_handle = kzalloc(sizeof(struct mhi_client_handle), GFP_KERNEL);
	if (NULL == *client_handle)
		return -ENOMEM;
	(*client_handle)->client_config =
		kzalloc(sizeof(*(*client_handle)->client_config), GFP_KERNEL);
	if ((*client_handle)->client_config == NULL) {
		kfree(*client_handle);
		*client_handle = NULL;
		return -ENOMEM;
	}

	mhi_dev_ctxt->client_handle_list[chan] = *client_handle;
	(*client_handle)->dev_id = mhi_dev_ctxt->core.dev_id;
	(*client_handle)->domain = mhi_dev_ctxt->core.domain;
	(*client_handle)->bus = mhi_dev_ctxt->core.bus;
	(*client_handle)->slot = mhi_dev_ctxt->core.slot;
	(*client_handle)->enabled = false;
	client_config = (*client_handle)->client_config;
	client_config->mhi_dev_ctxt = mhi_dev_ctxt;
	client_config->user_data = client_info->user_data;
	client_config->magic = MHI_HANDLE_MAGIC;
	client_config->chan_info.chan_nr = chan;

	if (NULL != client_info)
		client_config->client_info = *client_info;

	if (MHI_CLIENT_IP_HW_0_OUT  == chan)
		client_config->intmod_t = 10;
	if (MHI_CLIENT_IP_HW_0_IN  == chan)
		client_config->intmod_t = 10;

	client_config->chan_info = chan_info;
	ret = enable_bb_ctxt(mhi_dev_ctxt, &mhi_dev_ctxt->chan_bb_list[chan],
			     client_config->chan_info.max_desc, chan,
			     client_config->client_info.max_payload);
	if (ret) {
		kfree(mhi_dev_ctxt->client_handle_list[chan]->client_config);
		kfree(mhi_dev_ctxt->client_handle_list[chan]);
		mhi_dev_ctxt->client_handle_list[chan] =  NULL;
		return -ENOMEM;
	}

	if (mhi_dev_ctxt->dev_exec_env == MHI_EXEC_ENV_AMSS &&
	    mhi_dev_ctxt->flags.mhi_initialized)
		(*client_handle)->enabled = true;

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Successfuly registered chan:%u\n", chan);
	return 0;
}
EXPORT_SYMBOL(mhi_register_channel);

void mhi_close_channel(struct mhi_client_handle *client_handle)
{
	u32 chan;
	int ret_val = 0;
	struct mhi_chan_cfg *cfg;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	struct mhi_cmd_complete_event_pkt cmd_event_pkt;
	union mhi_cmd_pkt cmd_pkt;
	struct mhi_ring *chan_ring;
	enum MHI_EVENT_CCS ev_code;
	struct mhi_client_config *client_config =
		client_handle->client_config;

	if (client_config->magic != MHI_HANDLE_MAGIC ||
	    !client_config->chan_status)
		return;

	mhi_dev_ctxt = client_config->mhi_dev_ctxt;
	chan = client_config->chan_info.chan_nr;
	cfg = &mhi_dev_ctxt->mhi_chan_cfg[chan];

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Client attempting to close chan 0x%x\n", chan);

	chan_ring = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	mutex_lock(&cfg->chan_lock);

	/* No more processing events for this channel */
	spin_lock_irq(&chan_ring->ring_lock);
	if (chan_ring->ch_state != MHI_CHAN_STATE_ENABLED) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Chan %d is not enabled, cur state:0x%x\n",
			chan, chan_ring->ch_state);
		spin_unlock_irq(&chan_ring->ring_lock);
		mutex_unlock(&cfg->chan_lock);
		return;
	}
	chan_ring->ch_state = MHI_CHAN_STATE_DISABLED;
	spin_unlock_irq(&chan_ring->ring_lock);
	init_completion(&cfg->cmd_complete);
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	BUG_ON(mhi_dev_ctxt->mhi_pm_state == MHI_PM_DISABLE);
	mhi_dev_ctxt->assert_wake(mhi_dev_ctxt, false);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->runtime_get(mhi_dev_ctxt);
	ret_val = mhi_send_cmd(mhi_dev_ctxt,
			       MHI_COMMAND_RESET_CHAN, chan);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to send reset cmd for chan %d ret %d\n",
			chan, ret_val);
		goto error_completion;
	}
	ret_val = wait_for_completion_timeout(&cfg->cmd_complete,
				msecs_to_jiffies(MHI_MAX_CMD_TIMEOUT));
	if (!ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to receive cmd completion for %d\n", chan);
		goto error_completion;
	}

	spin_lock_irq(&cfg->event_lock);
	cmd_event_pkt = cfg->cmd_event_pkt;
	cmd_pkt = cfg->cmd_pkt;
	spin_unlock_irq(&cfg->event_lock);
	ev_code = MHI_EV_READ_CODE(EV_TRB_CODE,
				   ((union mhi_event_pkt *)&cmd_event_pkt));
	if (ev_code != MHI_EVENT_CC_SUCCESS) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Error to receive event completion ev_cod:0x%x\n",
			ev_code);
	}

error_completion:
	mhi_reset_chan(mhi_dev_ctxt, chan);

	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"resetting bb_ring for chan 0x%x\n", chan);
	mhi_dev_ctxt->chan_bb_list[chan].rp =
		mhi_dev_ctxt->chan_bb_list[chan].base;
	mhi_dev_ctxt->chan_bb_list[chan].wp =
		mhi_dev_ctxt->chan_bb_list[chan].base;
	mhi_dev_ctxt->chan_bb_list[chan].ack_rp =
		mhi_dev_ctxt->chan_bb_list[chan].base;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Freeing ring for chan 0x%x\n", chan);
	free_tre_ring(mhi_dev_ctxt, chan);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Chan 0x%x confirmed closed.\n", chan);
	client_config->chan_status = 0;
	mutex_unlock(&cfg->chan_lock);
}
EXPORT_SYMBOL(mhi_close_channel);

void mhi_update_chan_db(struct mhi_device_ctxt *mhi_dev_ctxt,
					  u32 chan)
{
	struct mhi_ring *chan_ctxt;
	u64 db_value;

	chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	db_value = mhi_v2p_addr(mhi_dev_ctxt, MHI_RING_TYPE_XFER_RING, chan,
						(uintptr_t) chan_ctxt->wp);
	chan_ctxt->db_mode.process_db(mhi_dev_ctxt,
				      mhi_dev_ctxt->mmio_info.chan_db_addr,
				      chan,
				      db_value);

}

static inline int mhi_queue_tre(struct mhi_device_ctxt
				*mhi_dev_ctxt,
				u32 chan,
				enum MHI_RING_TYPE type)
{
	struct mhi_chan_ctxt *chan_ctxt;
	unsigned long flags = 0;
	u64 db_value = 0;

	chan_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[chan];

	if (!MHI_DB_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state))
		return -EACCES;

	if (likely(type == MHI_RING_TYPE_XFER_RING)) {
		struct mhi_ring *mhi_ring =
			&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
		spin_lock_irqsave(&mhi_ring->ring_lock, flags);
		mhi_update_chan_db(mhi_dev_ctxt, chan);
		spin_unlock_irqrestore(&mhi_ring->ring_lock, flags);
	} else {
		struct mhi_ring *cmd_ring = &mhi_dev_ctxt->
				mhi_local_cmd_ctxt[PRIMARY_CMD_RING];
		db_value = mhi_v2p_addr(mhi_dev_ctxt,
					MHI_RING_TYPE_CMD_RING,
					PRIMARY_CMD_RING,
					(uintptr_t)cmd_ring->wp);
		cmd_ring->db_mode.process_db
			(mhi_dev_ctxt,
			 mhi_dev_ctxt->mmio_info.cmd_db_addr,
			 0,
			 db_value);
	}
	return 0;
}

static int create_bb(struct mhi_device_ctxt *mhi_dev_ctxt,
		  int chan, void *buf, size_t buf_len,
		  enum dma_data_direction dir, struct mhi_buf_info **bb)
{

	struct mhi_ring *bb_ctxt = &mhi_dev_ctxt->chan_bb_list[chan];
	struct mhi_buf_info *bb_info;
	int r;
	uintptr_t bb_index, ctxt_index_wp, ctxt_index_rp;

	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW,
		"Entered chan %d\n", chan);
	get_element_index(bb_ctxt, bb_ctxt->wp, &bb_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
			   mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp,
			   &ctxt_index_wp);
	get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
			   mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp,
			   &ctxt_index_rp);
	BUG_ON(bb_index != ctxt_index_wp);
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Chan RP index %ld Chan WP index %ld, chan %d\n",
		ctxt_index_rp, ctxt_index_wp, chan);
	r = ctxt_add_element(bb_ctxt, (void **)&bb_info);
	if (r)
		return r;

	bb_info->buf_len = buf_len;
	bb_info->client_buf = buf;
	bb_info->dir = dir;
	bb_info->bb_p_addr = dma_map_single(
					&mhi_dev_ctxt->plat_dev->dev,
					bb_info->client_buf,
					bb_info->buf_len,
					bb_info->dir);
	bb_info->bb_active = 0;
	if (!VALID_BUF(bb_info->bb_p_addr, bb_info->buf_len, mhi_dev_ctxt)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Buffer outside DMA range 0x%lx, size 0x%zx\n",
			(uintptr_t)bb_info->bb_p_addr, buf_len);
		dma_unmap_single(&mhi_dev_ctxt->plat_dev->dev,
				bb_info->bb_p_addr,
				bb_info->buf_len,
				bb_info->dir);

		if (likely((mhi_dev_ctxt->flags.bb_required &&
			    bb_info->pre_alloc_len >= bb_info->buf_len))) {
			bb_info->bb_p_addr = bb_info->pre_alloc_p_addr;
			bb_info->bb_v_addr = bb_info->pre_alloc_v_addr;
			mhi_dev_ctxt->counters.bb_used[chan]++;
			if (dir == DMA_TO_DEVICE) {
				mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
					"Copying client buf into BB.\n");
				memcpy(bb_info->bb_v_addr, buf,
				       bb_info->buf_len);
			}
			bb_info->bb_active = 1;
		} else
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"No BB allocated\n");
	}
	*bb = bb_info;
	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW, "Exited chan %d\n", chan);
	return 0;
}

static void disable_bb_ctxt(struct mhi_device_ctxt *mhi_dev_ctxt,
			    struct mhi_ring *bb_ctxt)
{
	if (mhi_dev_ctxt->flags.bb_required) {
		struct mhi_buf_info *bb =
			(struct mhi_buf_info *)bb_ctxt->base;
		int nr_el = bb_ctxt->len / bb_ctxt->el_size;
		int i = 0;

		for (i = 0; i < nr_el; i++, bb++)
			dma_pool_free(bb_ctxt->dma_pool, bb->pre_alloc_v_addr,
				      bb->pre_alloc_p_addr);
		dma_pool_destroy(bb_ctxt->dma_pool);
		bb_ctxt->dma_pool = NULL;
	}

	kfree(bb_ctxt->base);
	bb_ctxt->base = NULL;
}

static void free_bounce_buffer(struct mhi_device_ctxt *mhi_dev_ctxt,
			       struct mhi_buf_info *bb)
{
	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW, "Entered\n");
	if (!bb->bb_active)
		/* This buffer was maped directly to device */
		dma_unmap_single(&mhi_dev_ctxt->plat_dev->dev,
				 bb->bb_p_addr, bb->buf_len, bb->dir);

	bb->bb_active = 0;
	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW, "Exited\n");
}

static int mhi_queue_dma_xfer(
		struct mhi_client_config *client_config,
		dma_addr_t buf, size_t buf_len, enum MHI_FLAGS mhi_flags)
{
	union mhi_xfer_pkt *pkt_loc;
	int ret_val;
	enum MHI_CLIENT_CHANNEL chan;
	struct mhi_device_ctxt *mhi_dev_ctxt;

	mhi_dev_ctxt = client_config->mhi_dev_ctxt;
	MHI_ASSERT(VALID_BUF(buf, buf_len, mhi_dev_ctxt),
			"Client buffer is of invalid length\n");
	chan = client_config->chan_info.chan_nr;

	pkt_loc = mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp;
	pkt_loc->data_tx_pkt.buffer_ptr = buf;
	pkt_loc->type.info = mhi_flags;
	trace_mhi_tre(pkt_loc, chan, 0);

	if (likely(0 != client_config->intmod_t))
		MHI_TRB_SET_INFO(TX_TRB_BEI, pkt_loc, 1);
	else
		MHI_TRB_SET_INFO(TX_TRB_BEI, pkt_loc, 0);

	MHI_TRB_SET_INFO(TX_TRB_TYPE, pkt_loc, MHI_PKT_TYPE_TRANSFER);
	MHI_TX_TRB_SET_LEN(TX_TRB_LEN, pkt_loc, buf_len);

	/* Ensure writes to descriptor are flushed */
	wmb();

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Channel %d Has buf size of %zd and buf addr %lx, flags 0x%x\n",
		chan, buf_len, (uintptr_t)buf, mhi_flags);

	/* Add the TRB to the correct transfer ring */
	ret_val = ctxt_add_element(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				(void *)&pkt_loc);
	if (unlikely(0 != ret_val)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
			"Failed to insert trb in xfer ring\n");
		return ret_val;
	}

	if (MHI_OUT ==
	    GET_CHAN_PROPS(CHAN_DIR, client_config->chan_info.flags))
		atomic_inc(&mhi_dev_ctxt->counters.outbound_acks);

	return ret_val;
}

int mhi_queue_xfer(struct mhi_client_handle *client_handle,
		void *buf, size_t buf_len, enum MHI_FLAGS mhi_flags)
{
	int r;
	enum dma_data_direction dma_dir;
	struct mhi_buf_info *bb;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	u32 chan;
	unsigned long flags;
	struct mhi_client_config *client_config;

	if (!client_handle || !buf || !buf_len)
		return -EINVAL;

	client_config = client_handle->client_config;
	mhi_dev_ctxt = client_config->mhi_dev_ctxt;
	chan = client_config->chan_info.chan_nr;

	read_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);
	if (mhi_dev_ctxt->mhi_pm_state == MHI_PM_DISABLE) {
		read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"MHI is not in active state\n");
		return -EINVAL;
	}
	mhi_dev_ctxt->runtime_get(mhi_dev_ctxt);
	mhi_dev_ctxt->assert_wake(mhi_dev_ctxt, false);
	read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);

	if (MHI_OUT == GET_CHAN_PROPS(CHAN_DIR, client_config->chan_info.flags))
		dma_dir = DMA_TO_DEVICE;
	else
		dma_dir = DMA_FROM_DEVICE;

	r = create_bb(client_config->mhi_dev_ctxt,
		      client_config->chan_info.chan_nr,
		      buf,
		      buf_len,
		      dma_dir,
		      &bb);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
			"Failed to create BB, chan %d ret %d\n", chan, r);
		mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
		read_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);
		mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
		read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);
		return r;
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Queueing to HW: Client Buf 0x%p, size 0x%zx, DMA %llx, chan %d\n",
		buf, buf_len, (u64)bb->bb_p_addr,
		client_config->chan_info.chan_nr);
	r = mhi_queue_dma_xfer(client_config,
				bb->bb_p_addr,
				bb->buf_len,
				mhi_flags);

	/*
	 * Assumption: If create_bounce_buffer did not fail, we do not
	 * expect mhi_queue_dma_xfer to fail, if it does, the bb list will be
	 * out of sync with the descriptor list which is problematic.
	 */
	BUG_ON(r);

	read_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);
	mhi_queue_tre(mhi_dev_ctxt, chan, MHI_RING_TYPE_XFER_RING);
	if (dma_dir == DMA_FROM_DEVICE) {
		mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
		mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	}
	read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);
	return 0;
}
EXPORT_SYMBOL(mhi_queue_xfer);

int mhi_send_cmd(struct mhi_device_ctxt *mhi_dev_ctxt,
			enum MHI_COMMAND cmd, u32 chan)
{
	union mhi_cmd_pkt *cmd_pkt = NULL;
	enum MHI_PKT_TYPE ring_el_type = MHI_PKT_TYPE_NOOP_CMD;
	int ret_val = 0;
	unsigned long flags, flags2;
	struct mhi_ring *mhi_ring = &mhi_dev_ctxt->
		mhi_local_cmd_ctxt[PRIMARY_CMD_RING];

	if (chan >= MHI_MAX_CHANNELS || cmd >= MHI_COMMAND_MAX_NR) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Invalid channel id, received id: 0x%x", chan);
		return -EINVAL;
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Entered, MHI state %s dev_exec_env %d chan %d cmd %d\n",
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state),
		mhi_dev_ctxt->dev_exec_env, chan, cmd);

	switch (cmd) {
		break;
	case MHI_COMMAND_RESET_CHAN:
		ring_el_type = MHI_PKT_TYPE_RESET_CHAN_CMD;
		break;
	case MHI_COMMAND_START_CHAN:
		ring_el_type = MHI_PKT_TYPE_START_CHAN_CMD;
		break;
	default:
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Bad command received\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&mhi_ring->ring_lock, flags);
	ret_val = ctxt_add_element(mhi_ring, (void *)&cmd_pkt);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to insert element\n");
		spin_unlock_irqrestore(&mhi_ring->ring_lock, flags);
		return ret_val;
	}

	MHI_TRB_SET_INFO(CMD_TRB_TYPE, cmd_pkt, ring_el_type);
	MHI_TRB_SET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
	read_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags2);
	mhi_queue_tre(mhi_dev_ctxt, 0, MHI_RING_TYPE_CMD_RING);
	read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags2);
	spin_unlock_irqrestore(&mhi_ring->ring_lock, flags);
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Sent command 0x%x for chan %d\n", cmd, chan);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Exited ret %d.\n", ret_val);

	return ret_val;
}

static void parse_inbound_bb(struct mhi_device_ctxt *mhi_dev_ctxt,
			     struct mhi_ring *bb_ctxt,
			     struct mhi_result *result,
			     size_t bounced_data_size)
{

	struct mhi_buf_info *bb;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Entered\n");
	bb = bb_ctxt->rp;
	bb->filled_size = bounced_data_size;

	/* Data corruption will occur */
	BUG_ON(bb->dir != DMA_FROM_DEVICE);
	BUG_ON(bb->filled_size > bb->buf_len);

	if (bb->bb_active) {
		/* This is coherent memory, no cache management is needed */
		memcpy(bb->client_buf, bb->bb_v_addr, bb->filled_size);
		mhi_log(mhi_dev_ctxt, MHI_MSG_RAW,
			"Bounce from BB:0x%p to Client Buf: 0x%p Len 0x%zx\n",
			bb->client_buf, bb->bb_v_addr, bb->filled_size);
	}

	result->buf_addr = bb->client_buf;
	result->bytes_xferd = bb->filled_size;

	/* At this point the bounce buffer is no longer necessary
	 * Whatever was received from the device was copied back to the
	 * user buffer. Free up the bounce buffer, but do not move the bb ring
	 * rp, since it can be moved async by mhi_poll_inbound
	 */
	free_bounce_buffer(mhi_dev_ctxt, bb);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Exited\n");
}

static void parse_outbound_bb(struct mhi_device_ctxt *mhi_dev_ctxt,
			      struct mhi_ring *bb_ctxt,
			      struct mhi_result *result,
			      size_t bounced_data_size)
{
	struct mhi_buf_info *bb;

	bb = bb_ctxt->rp;
	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW, "Entered\n");
	BUG_ON(bb->dir != DMA_TO_DEVICE);
	bb->filled_size = bounced_data_size;
	BUG_ON(bb->filled_size != bb->buf_len);
	result->buf_addr = bb->client_buf;
	result->bytes_xferd = bb->filled_size;
	result->transaction_status = 0;
	free_bounce_buffer(mhi_dev_ctxt, bb);
	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW, "Exited\n");
}

static int parse_outbound(struct mhi_device_ctxt *mhi_dev_ctxt,
		u32 chan, union mhi_xfer_pkt *local_ev_trb_loc, u16 xfer_len)
{
	struct mhi_result *result = NULL;
	int ret_val = 0;
	struct mhi_client_handle *client_handle = NULL;
	struct mhi_client_config *client_config;
	struct mhi_ring *local_chan_ctxt = NULL;
	struct mhi_cb_info cb_info;
	struct mhi_ring *bb_ctxt = &mhi_dev_ctxt->chan_bb_list[chan];

	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	client_config = client_handle->client_config;

	/* If ring is empty */
	MHI_ASSERT(!unlikely(mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp ==
	    mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp), "Empty Event Ring\n");

	parse_outbound_bb(mhi_dev_ctxt, bb_ctxt,
				&client_config->result, xfer_len);

	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW,
		"Removing BB from head, chan %d\n", chan);
	atomic_dec(&mhi_dev_ctxt->counters.outbound_acks);
	mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
	ret_val = ctxt_del_element(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
						NULL);
	BUG_ON(ret_val);
	ret_val = ctxt_del_element(bb_ctxt, NULL);
	BUG_ON(ret_val);

	result = &client_config->result;
	if (NULL != (&client_config->client_info.mhi_client_cb)) {
		client_config->result.user_data =
			client_config->user_data;
		cb_info.cb_reason = MHI_CB_XFER;
		cb_info.result = result;
		cb_info.chan = chan;
		client_config->client_info.mhi_client_cb(&cb_info);
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW,
		"Processed outbound ack chan %d Pending acks %d.\n",
		chan, atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
	return 0;
}

static int parse_inbound(struct mhi_device_ctxt *mhi_dev_ctxt,
			 u32 chan, union mhi_xfer_pkt *local_ev_trb_loc,
			 u16 xfer_len, unsigned ev_ring)
{
	struct mhi_client_handle *client_handle;
	struct mhi_client_config *client_config;
	struct mhi_ring *local_chan_ctxt;
	struct mhi_result *result;
	struct mhi_cb_info cb_info;
	struct mhi_ring *bb_ctxt = &mhi_dev_ctxt->chan_bb_list[chan];
	bool ev_managed = GET_EV_PROPS(EV_MANAGED,
				mhi_dev_ctxt->ev_ring_props[ev_ring].flags);
	int r;
	uintptr_t bb_index, ctxt_index_rp, ctxt_index_wp;

	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	client_config = client_handle->client_config;
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];

	MHI_ASSERT(!unlikely(mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp ==
	    mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp), "Empty Event Ring\n");

	result = &client_config->result;
	parse_inbound_bb(mhi_dev_ctxt, bb_ctxt, result, xfer_len);

	if (ev_managed) {
		MHI_TX_TRB_SET_LEN(TX_TRB_LEN, local_ev_trb_loc, xfer_len);
		r = ctxt_del_element(local_chan_ctxt, NULL);
		BUG_ON(r);
		r = ctxt_del_element(bb_ctxt, NULL);
		BUG_ON(r);
		get_element_index(bb_ctxt, bb_ctxt->rp, &bb_index);
		get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				   mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp,
				   &ctxt_index_rp);
		get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				   mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp,
				   &ctxt_index_wp);
		mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
			"Chan RP index %ld Chan WP index %ld chan %d\n",
			ctxt_index_rp, ctxt_index_wp, chan);
		BUG_ON(bb_index != ctxt_index_rp);
		if (NULL != client_config->client_info.mhi_client_cb) {
			client_config->result.user_data =
						client_config->user_data;
			cb_info.cb_reason = MHI_CB_XFER;
			cb_info.result = &client_config->result;
			cb_info.chan = chan;
			client_config->client_info.mhi_client_cb(&cb_info);
		} else {
			mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
				"No client registered chan %d\n", chan);
		}
	} else  {
		if (likely(client_handle)) {
			/* Move the rp for both the descriptor and
			 * the bb rings. The caller will get all the buffer
			 * references in the result structure. We do not need
			 * to keep further track of the user buffer.
			 */
			ctxt_del_element(bb_ctxt, NULL);
			ctxt_del_element(local_chan_ctxt, NULL);
			get_element_index(bb_ctxt, bb_ctxt->rp, &bb_index);
			get_element_index(
				&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp,
				&ctxt_index_rp);
			get_element_index(
				    &mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				    mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp,
				    &ctxt_index_wp);
			mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
				"Chan RP index %ld Chan WP index %ld chan %d\n",
				ctxt_index_rp, ctxt_index_wp, chan);
			BUG_ON(bb_index != ctxt_index_rp);
		} else  {
			BUG();

		}
	}
	return 0;
}

static int validate_xfer_el_addr(struct mhi_chan_ctxt *ring,
							uintptr_t addr)
{
	return (addr < (ring->mhi_trb_ring_base_addr) ||
			addr > (ring->mhi_trb_ring_base_addr)
			+ (ring->mhi_trb_ring_len - 1)) ?
		-ERANGE : 0;
}

static void print_tre(struct mhi_device_ctxt *mhi_dev_ctxt,
		      int chan,
		      struct mhi_ring *ring,
		      struct mhi_tx_pkt *tre)
{
	uintptr_t el_index;

	get_element_index(ring, tre, &el_index);
	mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
		"Printing TRE 0x%p index %lx for channel %d:\n",
		tre, el_index, chan);
	mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
		"Buffer Pointer 0x%llx, len 0x%x, info 0x%x\n",
		tre->buffer_ptr, tre->buf_len, tre->info);
}

int parse_xfer_event(struct mhi_device_ctxt *mhi_dev_ctxt,
				union mhi_event_pkt *event, u32 event_id)
{
	struct mhi_result *result;
	u32 chan = MHI_MAX_CHANNELS;
	u16 xfer_len;
	uintptr_t phy_ev_trb_loc;
	union mhi_xfer_pkt *local_ev_trb_loc;
	struct mhi_client_handle *client_handle;
	struct mhi_client_config *client_config;
	union mhi_xfer_pkt *local_trb_loc;
	struct mhi_chan_ctxt *chan_ctxt;
	u32 nr_trb_to_parse;
	u32 i = 0;
	u32 ev_code;
	struct mhi_ring *local_chan_ctxt;

	trace_mhi_ev(event);
	chan = MHI_EV_READ_CHID(EV_CHID, event);
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	ev_code = MHI_EV_READ_CODE(EV_TRB_CODE, event);
	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	client_config = client_handle->client_config;
	client_config->pkt_count++;
	result = &client_config->result;
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Event Received, chan %d, cc_code %d\n", chan, ev_code);
	if (ev_code == MHI_EVENT_CC_OVERFLOW)
		result->transaction_status = -EOVERFLOW;
	else
		result->transaction_status = 0;

	switch (ev_code) {
	case MHI_EVENT_CC_OVERFLOW:
	case MHI_EVENT_CC_EOB:
	case MHI_EVENT_CC_EOT:
	{
		dma_addr_t trb_data_loc;
		u32 ieot_flag;
		int ret_val;

		phy_ev_trb_loc = MHI_EV_READ_PTR(EV_PTR, event);

		chan_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[chan];
		BUG_ON(validate_xfer_el_addr(chan_ctxt, phy_ev_trb_loc));

		/* Get the TRB this event points to */
		local_ev_trb_loc = (void *)mhi_p2v_addr(mhi_dev_ctxt,
					MHI_RING_TYPE_XFER_RING, chan,
					phy_ev_trb_loc);
		local_trb_loc = (union mhi_xfer_pkt *)local_chan_ctxt->rp;

		trace_mhi_tre(local_trb_loc, chan, 1);

		ret_val = get_nr_enclosed_el(local_chan_ctxt,
				      local_trb_loc,
				      local_ev_trb_loc,
				      &nr_trb_to_parse);
		if (unlikely(ret_val)) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
				"Failed to get nr available trbs ret: %d.\n",
				ret_val);
			return ret_val;
		}
		do {
			u64 phy_buf_loc;

			MHI_TRB_GET_INFO(TX_TRB_IEOT, local_trb_loc, ieot_flag);
			phy_buf_loc = local_trb_loc->data_tx_pkt.buffer_ptr;
			trb_data_loc = (dma_addr_t)phy_buf_loc;
			if (local_chan_ctxt->dir == MHI_IN)
				xfer_len = MHI_EV_READ_LEN(EV_LEN, event);
			else
				xfer_len = MHI_TX_TRB_GET_LEN(TX_TRB_LEN,
							local_trb_loc);

			if (!VALID_BUF(trb_data_loc, xfer_len, mhi_dev_ctxt)) {
				mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
					"Bad buf ptr: %llx.\n", trb_data_loc);
				return -EINVAL;
			}
			if (local_chan_ctxt->dir == MHI_IN) {
				parse_inbound(mhi_dev_ctxt, chan,
					      local_ev_trb_loc, xfer_len,
					      event_id);
			} else {
				parse_outbound(mhi_dev_ctxt, chan,
						local_ev_trb_loc, xfer_len);
			}
			mhi_dev_ctxt->counters.chan_pkts_xferd[chan]++;
			if (local_trb_loc ==
			   (union mhi_xfer_pkt *)local_chan_ctxt->rp) {
				mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
					"Done. Processed until: %lx.\n",
					(uintptr_t)trb_data_loc);
				break;
			}
			local_trb_loc =
			(union mhi_xfer_pkt *)local_chan_ctxt->rp;
			i++;
		} while (i < nr_trb_to_parse);
		break;
	} /* CC_EOT */
	case MHI_EVENT_CC_OOB:
	case MHI_EVENT_CC_DB_MODE:
	{
		u64 db_value = 0;

		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"DB_MODE/OOB Detected chan %d.\n", chan);

		local_chan_ctxt->db_mode.db_mode = 1;
		if (local_chan_ctxt->wp != local_chan_ctxt->rp) {
			db_value = mhi_v2p_addr(mhi_dev_ctxt,
					MHI_RING_TYPE_XFER_RING, chan,
					(uintptr_t) local_chan_ctxt->wp);
			local_chan_ctxt->db_mode.process_db(mhi_dev_ctxt,
				     mhi_dev_ctxt->mmio_info.chan_db_addr, chan,
				     db_value);
		}
		break;
	}
	case MHI_EVENT_CC_BAD_TRE:
		phy_ev_trb_loc = MHI_EV_READ_PTR(EV_PTR, event);
		local_ev_trb_loc = (void *)mhi_p2v_addr(mhi_dev_ctxt,
					MHI_RING_TYPE_EVENT_RING, event_id,
					phy_ev_trb_loc);
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Received BAD TRE event for ring %d, pointer 0x%p\n",
			chan, local_ev_trb_loc);
		print_tre(mhi_dev_ctxt, chan,
			  &mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
			  (struct mhi_tx_pkt *)local_ev_trb_loc);
		BUG();
	break;
	default:
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Unknown TX completion.\n");

		break;
	} /*switch(MHI_EV_READ_CODE(EV_TRB_CODE,event)) */
	return 0;
}

int recycle_trb_and_ring(struct mhi_device_ctxt *mhi_dev_ctxt,
		struct mhi_ring *ring,
		enum MHI_RING_TYPE ring_type,
		u32 ring_index)
{
	int ret_val = 0;
	u64 db_value = 0;
	void *removed_element = NULL;
	void *added_element = NULL;
	unsigned long flags;
	struct mhi_ring *mhi_ring = &mhi_dev_ctxt->
		mhi_local_event_ctxt[ring_index];

	ret_val = ctxt_del_element(ring, &removed_element);

	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Could not remove element from ring\n");
		return ret_val;
	}
	ret_val = ctxt_add_element(ring, &added_element);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Could not add element to ring\n");
		return ret_val;
	}

	if (!MHI_DB_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state))
		return -EACCES;

	read_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);
	db_value = mhi_v2p_addr(mhi_dev_ctxt,
				ring_type,
				ring_index,
				(uintptr_t) ring->wp);
	mhi_ring->db_mode.process_db(mhi_dev_ctxt,
				     mhi_dev_ctxt->mmio_info.event_db_addr,
				     ring_index, db_value);
	read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);

	return 0;

}

void mhi_reset_chan(struct mhi_device_ctxt *mhi_dev_ctxt, int chan)
{
	struct mhi_ring *local_chan_ctxt;
	struct mhi_ring *ev_ring;
	struct mhi_chan_ctxt *chan_ctxt;
	struct mhi_event_ctxt *ev_ctxt = NULL;
	int pending_el = 0, i;
	unsigned long flags;
	union mhi_event_pkt *local_rp = NULL;
	union mhi_event_pkt *device_rp = NULL;

	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	chan_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[chan];
	ev_ring = &mhi_dev_ctxt->
		mhi_local_event_ctxt[chan_ctxt->mhi_event_ring_index];
	ev_ctxt = &mhi_dev_ctxt->
		dev_space.ring_ctxt.ec_list[chan_ctxt->mhi_event_ring_index];
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Marking all events for chan:%d as stale\n", chan);

	/* Clear all stale events related to Channel */
	spin_lock_irqsave(&ev_ring->ring_lock, flags);
	device_rp = (union mhi_event_pkt *)mhi_p2v_addr(
					mhi_dev_ctxt,
					MHI_RING_TYPE_EVENT_RING,
					chan_ctxt->mhi_event_ring_index,
					ev_ctxt->mhi_event_read_ptr);
	local_rp = (union mhi_event_pkt *)ev_ring->rp;
	while (device_rp != local_rp) {
		if (MHI_TRB_READ_INFO(EV_TRB_TYPE, local_rp) ==
		    MHI_PKT_TYPE_TX_EVENT) {
			u32 ev_chan = MHI_EV_READ_CHID(EV_CHID, local_rp);

			/* Mark as stale event */
			if (ev_chan == chan)
				MHI_TRB_SET_INFO(EV_TRB_TYPE,
						 local_rp,
						 MHI_PKT_TYPE_STALE_EVENT);
		}

		local_rp++;
		if (local_rp == (ev_ring->base + ev_ring->len))
			local_rp = ev_ring->base;
	}
	spin_unlock_irqrestore(&ev_ring->ring_lock, flags);

	/*
	 * If outbound elements are pending, they must be cleared since
	 * they will never be acked after a channel reset.
	 */
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	if (local_chan_ctxt->dir == MHI_OUT)
		get_nr_enclosed_el(local_chan_ctxt,
				   local_chan_ctxt->rp,
				   local_chan_ctxt->wp,
				   &pending_el);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Decrementing chan %d out acks by %d.\n", chan, pending_el);

	atomic_sub(pending_el, &mhi_dev_ctxt->counters.outbound_acks);
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	for (i = 0; i < pending_el; i++)
		mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);

	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);

	for (i = 0; i < pending_el; i++) {
		mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
	}

	/* Reset the local channel context */
	local_chan_ctxt->rp = local_chan_ctxt->base;
	local_chan_ctxt->wp = local_chan_ctxt->base;
	local_chan_ctxt->ack_rp = local_chan_ctxt->base;

	/* Reset the mhi channel context */
	chan_ctxt->chstate = MHI_CHAN_STATE_DISABLED;
	chan_ctxt->mhi_trb_read_ptr = chan_ctxt->mhi_trb_ring_base_addr;
	chan_ctxt->mhi_trb_write_ptr = chan_ctxt->mhi_trb_ring_base_addr;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Reset complete.\n");
}

enum MHI_EVENT_CCS get_cmd_pkt(struct mhi_device_ctxt *mhi_dev_ctxt,
				union mhi_event_pkt *ev_pkt,
				union mhi_cmd_pkt **cmd_pkt,
				u32 event_index)
{
	uintptr_t phy_trb_loc = 0;

	if (NULL != ev_pkt)
		phy_trb_loc = (uintptr_t)MHI_EV_READ_PTR(EV_PTR,
							ev_pkt);
	else
		return -EINVAL;
	*cmd_pkt = (union mhi_cmd_pkt *)mhi_p2v_addr(mhi_dev_ctxt,
					MHI_RING_TYPE_CMD_RING, event_index,
					 phy_trb_loc);
	return MHI_EV_READ_CODE(EV_TRB_CODE, ev_pkt);
}

int mhi_poll_inbound(struct mhi_client_handle *client_handle,
		     struct mhi_result *result)
{
	struct mhi_tx_pkt *pending_trb = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL;
	struct mhi_ring *local_chan_ctxt = NULL;
	struct mhi_ring *bb_ctxt = NULL;
	struct mhi_buf_info *bb = NULL;
	struct mhi_client_config *client_config;
	int  chan = 0, r = -EIO;
	unsigned long flags;

	if (!client_handle || !result)
		return -EINVAL;
	client_config = client_handle->client_config;
	mhi_dev_ctxt = client_config->mhi_dev_ctxt;

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Entered\n");

	chan = client_config->chan_info.chan_nr;
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	bb_ctxt = &mhi_dev_ctxt->chan_bb_list[chan];

	spin_lock_irqsave(&local_chan_ctxt->ring_lock, flags);
	if (local_chan_ctxt->ch_state == MHI_CHAN_STATE_ENABLED) {
		if (bb_ctxt->rp != bb_ctxt->ack_rp) {
			pending_trb =
				(struct mhi_tx_pkt *)(local_chan_ctxt->ack_rp);
			result->flags = pending_trb->info;
			bb = bb_ctxt->ack_rp;
			if (bb->bb_active) {
				mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
					"Bounce buffer active chan %d, copying data\n",
					chan);
			}
			result->buf_addr = bb->client_buf;
			result->bytes_xferd = bb->filled_size;
			result->transaction_status = 0;
			r = delete_element(local_chan_ctxt,
					   &local_chan_ctxt->ack_rp,
					   &local_chan_ctxt->rp, NULL);
			WARN_ON(r);
			r = delete_element(bb_ctxt,
					   &bb_ctxt->ack_rp,
					   &bb_ctxt->rp, NULL);
			WARN_ON(r);
		} else {
			result->buf_addr = 0;
			result->bytes_xferd = 0;
			r = -ENODATA;
		}
	}
	spin_unlock_irqrestore(&local_chan_ctxt->ring_lock, flags);
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Exited Result: Buf addr: 0x%p Bytes xfed 0x%zx chan %d\n",
		result->buf_addr, result->bytes_xferd, chan);
	return r;
}
EXPORT_SYMBOL(mhi_poll_inbound);

int validate_ev_el_addr(struct mhi_ring *ring, uintptr_t addr)
{
	return (addr < (uintptr_t)(ring->base) ||
			addr > ((uintptr_t)(ring->base)
				+ (ring->len - 1))) ?
		-ERANGE : 0;
}

int validate_ring_el_addr(struct mhi_ring *ring, uintptr_t addr)
{
	return (addr < (uintptr_t)(ring->base) ||
		addr > ((uintptr_t)(ring->base)
			+ (ring->len - 1))) ?
		-ERANGE : 0;
}

int mhi_wait_for_mdm(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 j = 0;

	while (mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr, MHIREGLEN)
			== 0xFFFFFFFF
			&& j <= MHI_MAX_LINK_RETRIES) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Could not access device retry %d\n", j);
		msleep(MHI_LINK_STABILITY_WAIT_MS);
		if (MHI_MAX_LINK_RETRIES == j) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
				"Could not access device, FAILING!\n");
			return -ETIME;
		}
		j++;
	}
	return 0;
}

int mhi_get_max_desc(struct mhi_client_handle *client_handle)
{
	struct mhi_client_config *client_config;

	if (!client_handle)
		return -EINVAL;
	client_config = client_handle->client_config;
	return client_config->chan_info.max_desc - 1;
}
EXPORT_SYMBOL(mhi_get_max_desc);

int mhi_get_epid(struct mhi_client_handle *client_handle)
{
	return MHI_EPID;
}

void mhi_master_mode_runtime_get(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	pm_runtime_get(&mhi_dev_ctxt->pcie_device->dev);
}

void mhi_master_mode_runtime_put(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	pm_runtime_mark_last_busy(&mhi_dev_ctxt->pcie_device->dev);
	pm_runtime_put_noidle(&mhi_dev_ctxt->pcie_device->dev);
}

void mhi_slave_mode_runtime_get(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	mhi_dev_ctxt->bus_master_rt_get(mhi_dev_ctxt->pcie_device);
}

void mhi_slave_mode_runtime_put(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	mhi_dev_ctxt->bus_master_rt_put(mhi_dev_ctxt->pcie_device);
}

/*
 * mhi_assert_device_wake - Set WAKE_DB register
 * force_set - if true, will set bit regardless of counts
 */
void mhi_assert_device_wake(struct mhi_device_ctxt *mhi_dev_ctxt,
			    bool force_set)
{
	unsigned long flags;

	if (unlikely(force_set)) {
		spin_lock_irqsave(&mhi_dev_ctxt->dev_wake_lock, flags);
		atomic_inc(&mhi_dev_ctxt->counters.device_wake);
		if (MHI_WAKE_DB_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state))
			mhi_write_db(mhi_dev_ctxt,
				     mhi_dev_ctxt->mmio_info.chan_db_addr,
				     MHI_DEV_WAKE_DB, 1);
		spin_unlock_irqrestore(&mhi_dev_ctxt->dev_wake_lock, flags);
	} else {
		if (likely(atomic_add_unless(&mhi_dev_ctxt->
					     counters.device_wake,
					     1,
					     0)))
			return;

		spin_lock_irqsave(&mhi_dev_ctxt->dev_wake_lock, flags);
		if ((atomic_inc_return(&mhi_dev_ctxt->counters.device_wake)
		     == 1) &&
		    MHI_WAKE_DB_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
			mhi_write_db(mhi_dev_ctxt,
				     mhi_dev_ctxt->mmio_info.chan_db_addr,
				     MHI_DEV_WAKE_DB,
				     1);
		}
		spin_unlock_irqrestore(&mhi_dev_ctxt->dev_wake_lock, flags);
	}
}

void mhi_deassert_device_wake(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	unsigned long flags;

	BUG_ON(atomic_read(&mhi_dev_ctxt->counters.device_wake) == 0);

	if (likely(atomic_add_unless
		   (&mhi_dev_ctxt->counters.device_wake, -1, 1)))
		return;

	spin_lock_irqsave(&mhi_dev_ctxt->dev_wake_lock, flags);
	if ((atomic_dec_return(&mhi_dev_ctxt->counters.device_wake) == 0) &&
	    MHI_WAKE_DB_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state))
		mhi_write_db(mhi_dev_ctxt,
			     mhi_dev_ctxt->mmio_info.chan_db_addr,
			     MHI_DEV_WAKE_DB,
			     0);
	spin_unlock_irqrestore(&mhi_dev_ctxt->dev_wake_lock, flags);
}

int mhi_set_lpm(struct mhi_client_handle *client_handle, bool enable_lpm)
{
	struct mhi_client_config *client_config = client_handle->client_config;
	struct mhi_device_ctxt *mhi_dev_ctxt = client_config->mhi_dev_ctxt;
	unsigned long flags;

	read_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);

	/* Disable low power mode by asserting Wake */
	if (enable_lpm == false)
		mhi_dev_ctxt->assert_wake(mhi_dev_ctxt, false);
	else
		mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);

	read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);

	return 0;
}
EXPORT_SYMBOL(mhi_set_lpm);

int mhi_set_bus_request(struct mhi_device_ctxt *mhi_dev_ctxt,
				int index)
{
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Setting bus request to index %d\n", index);
	return msm_bus_scale_client_update_request(mhi_dev_ctxt->bus_client,
								index);
}

int mhi_deregister_channel(struct mhi_client_handle *client_handle)
{
	int ret_val = 0;
	int chan;
	struct mhi_client_config *client_config;
	struct mhi_device_ctxt *mhi_dev_ctxt;

	if (!client_handle)
		return -EINVAL;

	client_config = client_handle->client_config;
	mhi_dev_ctxt = client_config->mhi_dev_ctxt;
	chan = client_config->chan_info.chan_nr;
	client_config->magic = 0;
	mhi_dev_ctxt->client_handle_list[chan] = NULL;
	disable_bb_ctxt(mhi_dev_ctxt, &mhi_dev_ctxt->chan_bb_list[chan]);
	kfree(client_config);
	kfree(client_handle);
	return ret_val;
}
EXPORT_SYMBOL(mhi_deregister_channel);

int mhi_register_device(struct mhi_device *mhi_device,
			const char *node_name,
			void *user_data)
{
	const struct device_node *of_node;
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL, *itr;
	struct pcie_core_info *core_info;
	struct pci_dev *pci_dev = mhi_device->pci_dev;
	u32 domain = pci_domain_nr(pci_dev->bus);
	u32 bus = pci_dev->bus->number;
	u32 dev_id = pci_dev->device;
	u32 slot = PCI_SLOT(pci_dev->devfn);
	int ret, i;
	char node[32];
	struct pcie_core_info *core;

	of_node = of_parse_phandle(mhi_device->dev->of_node, node_name, 0);
	if (!of_node)
		return -EINVAL;

	if (!mhi_device_drv)
		return -EPROBE_DEFER;

	/* Traverse thru the list */
	mutex_lock(&mhi_device_drv->lock);
	list_for_each_entry(itr, &mhi_device_drv->head, node) {
		struct platform_device *pdev = itr->plat_dev;

		core = &itr->core;
		if (pdev->dev.of_node == of_node && core->domain == domain &&
		    core->bus == bus && core->slot == slot &&
		    (core->dev_id == PCI_ANY_ID || (core->dev_id == dev_id))) {
			/* change default dev_id to current dev_id */
			core->dev_id = dev_id;
			mhi_dev_ctxt = itr;
			break;
		}
	}
	mutex_unlock(&mhi_device_drv->lock);

	/* perhaps we've not probed yet */
	if (!mhi_dev_ctxt)
		return -EPROBE_DEFER;

	snprintf(node, sizeof(node), "mhi_%04x_%02u.%02u.%02u",
		 core->dev_id, core->domain, core->bus, core->slot);
	mhi_dev_ctxt->mhi_ipc_log =
		ipc_log_context_create(MHI_IPC_LOG_PAGES, node, 0);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Registering Domain:%02u Bus:%04u dev:0x%04x slot:%04u\n",
		domain, bus, dev_id, slot);

	/* Set up pcie dev info */
	mhi_dev_ctxt->pcie_device = pci_dev;
	mhi_dev_ctxt->mhi_pm_state = MHI_PM_DISABLE;
	INIT_WORK(&mhi_dev_ctxt->process_m1_worker, process_m1_transition);
	INIT_WORK(&mhi_dev_ctxt->st_thread_worker, mhi_state_change_worker);
	INIT_WORK(&mhi_dev_ctxt->process_sys_err_worker, mhi_sys_err_worker);
	mutex_init(&mhi_dev_ctxt->pm_lock);
	rwlock_init(&mhi_dev_ctxt->pm_xfer_lock);
	spin_lock_init(&mhi_dev_ctxt->dev_wake_lock);
	init_completion(&mhi_dev_ctxt->cmd_complete);
	mhi_dev_ctxt->flags.link_up = 1;
	core_info = &mhi_dev_ctxt->core;
	core_info->manufact_id = pci_dev->vendor;
	core_info->pci_master = false;

	/* Go thru resources and set up */
	for (i = 0; i < ARRAY_SIZE(mhi_device->resources); i++) {
		const struct resource *res = &mhi_device->resources[i];

		switch (resource_type(res)) {
		case IORESOURCE_MEM:
			/* bus master already mapped it */
			core_info->bar0_base = (void __iomem *)res->start;
			core_info->bar0_end = (void __iomem *)res->end;
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"bar mapped to:0x%llx - 0x%llx (virtual)\n",
				res->start, res->end);
			break;
		case IORESOURCE_IRQ:
			core_info->irq_base = (u32)res->start;
			core_info->max_nr_msis = (u32)resource_size(res);
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"irq mapped to: %u size:%u\n",
				core_info->irq_base,
				core_info->max_nr_msis);
			break;
		};
	}

	if (!core_info->bar0_base || !core_info->irq_base)
		return -EINVAL;
	if (mhi_device->support_rddm && !mhi_device->rddm_size)
		return -EINVAL;

	mhi_dev_ctxt->bus_master_rt_get = mhi_device->pm_runtime_get;
	mhi_dev_ctxt->bus_master_rt_put = mhi_device->pm_runtime_put_noidle;
	mhi_dev_ctxt->status_cb = mhi_device->status_cb;
	mhi_dev_ctxt->priv_data = user_data;
	if (!mhi_dev_ctxt->bus_master_rt_get || !mhi_dev_ctxt->bus_master_rt_put
	    || !mhi_dev_ctxt->status_cb)
		return -EINVAL;

	ret = mhi_ctxt_init(mhi_dev_ctxt);
	if (ret) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"MHI Initialization failed, ret %d\n", ret);
		return ret;
	}
	mhi_init_debugfs(mhi_dev_ctxt);

	/* setup shadow pm functions */
	mhi_dev_ctxt->assert_wake = mhi_assert_device_wake;
	mhi_dev_ctxt->deassert_wake = mhi_deassert_device_wake;
	mhi_dev_ctxt->runtime_get = mhi_slave_mode_runtime_get;
	mhi_dev_ctxt->runtime_put = mhi_slave_mode_runtime_put;
	mhi_device->mhi_dev_ctxt = mhi_dev_ctxt;

	/* Store RDDM information */
	if (mhi_device->support_rddm) {
		mhi_dev_ctxt->bhi_ctxt.support_rddm = true;
		mhi_dev_ctxt->bhi_ctxt.rddm_size = mhi_device->rddm_size;

		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Device support rddm of size:0x%lx bytes\n",
			mhi_dev_ctxt->bhi_ctxt.rddm_size);
	}

	/* notify all the registered clients we probed */
	for (i = 0; i < MHI_MAX_CHANNELS; i++) {
		struct mhi_client_handle *client_handle =
			mhi_dev_ctxt->client_handle_list[i];

		if (!client_handle)
			continue;
		client_handle->dev_id = core->dev_id;
		mhi_notify_client(client_handle, MHI_CB_MHI_PROBED);
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Exit success\n");
	return 0;
}
EXPORT_SYMBOL(mhi_register_device);

int mhi_xfer_rddm(struct mhi_device *mhi_device, enum mhi_rddm_segment seg,
		  struct scatterlist **sg_list)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = mhi_device->mhi_dev_ctxt;
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	int segments = 0;

	*sg_list = NULL;
	switch (seg) {
	case MHI_RDDM_FW_SEGMENT:
		*sg_list = bhi_ctxt->fw_table.sg_list;
		segments = bhi_ctxt->fw_table.segment_count;
		break;
	case MHI_RDDM_RD_SEGMENT:
		*sg_list = bhi_ctxt->rddm_table.sg_list;
		segments = bhi_ctxt->rddm_table.segment_count;
		break;
	}
	return segments;
}
EXPORT_SYMBOL(mhi_xfer_rddm);

void mhi_process_db_brstmode(struct mhi_device_ctxt *mhi_dev_ctxt,
			     void __iomem *io_addr,
			     unsigned int chan,
			     dma_addr_t val)
{
	struct mhi_ring *ring_ctxt =
		&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];

	if (io_addr == mhi_dev_ctxt->mmio_info.chan_db_addr)
		ring_ctxt = &mhi_dev_ctxt->
			mhi_local_chan_ctxt[chan];
	else
		ring_ctxt = &mhi_dev_ctxt->
			mhi_local_event_ctxt[chan];

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"db.set addr: %p io_offset %u val:0x%llx\n",
		io_addr, chan, val);

	mhi_update_ctxt(mhi_dev_ctxt, io_addr, chan, val);

	if (ring_ctxt->db_mode.db_mode) {
		mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
		ring_ctxt->db_mode.db_mode = 0;
	} else {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Not ringing xfer db, chan %u, brstmode %d db_mode %d\n",
			chan, ring_ctxt->db_mode.brstmode,
			ring_ctxt->db_mode.db_mode);
	}
}

void mhi_process_db_brstmode_disable(struct mhi_device_ctxt *mhi_dev_ctxt,
			     void __iomem *io_addr,
			     unsigned int chan,
			     dma_addr_t val)
{
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"db.set addr: %p io_offset %u val:0x%llx\n",
		io_addr, chan, val);
	mhi_update_ctxt(mhi_dev_ctxt, io_addr, chan, val);
	mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
}

void mhi_process_db(struct mhi_device_ctxt *mhi_dev_ctxt,
		    void __iomem *io_addr,
		    unsigned int chan,
		    dma_addr_t val)
{

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"db.set addr: %p io_offset %u val:0x%llx\n",
		io_addr, chan, val);

	mhi_update_ctxt(mhi_dev_ctxt, io_addr, chan, val);

	/* Channel Doorbell and Polling Mode Disabled or Software Channel*/
	if (io_addr == mhi_dev_ctxt->mmio_info.chan_db_addr) {
		struct mhi_ring *chan_ctxt =
			&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
		if (!(IS_HARDWARE_CHANNEL(chan) &&
		    !chan_ctxt->db_mode.db_mode)) {
			mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
			chan_ctxt->db_mode.db_mode = 0;
		} else {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"Not ringing xfer db, chan %u, brstmode %d db_mode %d\n",
				chan, chan_ctxt->db_mode.brstmode,
				chan_ctxt->db_mode.db_mode);
		}
	/* Event Doorbell and Polling mode Disabled */
	} else if (io_addr == mhi_dev_ctxt->mmio_info.event_db_addr) {
		struct mhi_ring *ev_ctxt =
			&mhi_dev_ctxt->mhi_local_event_ctxt[chan];
		/* Only ring for software channel or db mode*/
		if (!(IS_HW_EV_RING(mhi_dev_ctxt, chan) &&
		    !ev_ctxt->db_mode.db_mode)) {
			mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
		}
	} else {
		mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
	}
}

void mhi_reg_write_field(struct mhi_device_ctxt *mhi_dev_ctxt,
			 void __iomem *io_addr,
			 uintptr_t io_offset,
			 u32 mask, u32 shift, u32 val)
{
	u32 reg_val;

	reg_val = mhi_reg_read(io_addr, io_offset);
	reg_val &= ~mask;
	reg_val = reg_val | (val << shift);
	mhi_reg_write(mhi_dev_ctxt, io_addr, io_offset, reg_val);
}

void mhi_reg_write(struct mhi_device_ctxt *mhi_dev_ctxt,
		   void __iomem *io_addr,
		   uintptr_t io_offset, u32 val)
{
	mhi_log(mhi_dev_ctxt, MHI_MSG_RAW,
		"d.s 0x%p off: 0x%lx 0x%x\n", io_addr, io_offset, val);
	iowrite32(val, io_addr + io_offset);
	/* Flush write to device */
	wmb();
}

u32 mhi_reg_read_field(void __iomem *io_addr, uintptr_t io_offset,
			 u32 mask, u32 shift)
{
	return (mhi_reg_read(io_addr, io_offset) & mask) >> shift;
}

u32 mhi_reg_read(void __iomem *io_addr, uintptr_t io_offset)
{
	return ioread32(io_addr + io_offset);
}
