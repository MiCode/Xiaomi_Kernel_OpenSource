/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include "mhi_trace.h"

static void mhi_write_db(struct mhi_device_ctxt *mhi_dev_ctxt,
		  void __iomem *io_addr_lower,
		  uintptr_t chan, u64 val)
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
	wmb();
	if (mhi_dev_ctxt->mmio_info.chan_db_addr == io_addr) {
		mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan].
				mhi_trb_write_ptr = val;
	} else if (mhi_dev_ctxt->mmio_info.event_db_addr == io_addr) {
		if (chan < mhi_dev_ctxt->mmio_info.nr_event_rings)
			mhi_dev_ctxt->mhi_ctrl_seg->mhi_ec_list[chan].
				mhi_event_write_ptr = val;
		else
			mhi_log(MHI_MSG_ERROR,
				"Bad EV ring index: %lx\n", chan);
	}
}

int mhi_init_pcie_device(struct mhi_pcie_dev_info *mhi_pcie_dev)
{
	int ret_val = 0;
	long int sleep_time = 100000;
	struct pci_dev *pcie_device =
			(struct pci_dev *)mhi_pcie_dev->pcie_device;
	do {
		ret_val = pci_enable_device(mhi_pcie_dev->pcie_device);
		if (0 != ret_val) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to enable pcie struct device ret_val %d\n",
				ret_val);
			mhi_log(MHI_MSG_ERROR,
				"Sleeping for ~ %li uS, and retrying.\n",
				sleep_time);
			usleep(sleep_time);
		}
	} while (ret_val != 0);

	mhi_log(MHI_MSG_INFO, "Successfully enabled pcie device.\n");

	mhi_pcie_dev->core.bar0_base =
		ioremap_nocache(pci_resource_start(pcie_device, 0),
			pci_resource_len(pcie_device, 0));
	if (!mhi_pcie_dev->core.bar0_base) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to map bar 0 addr 0x%x len 0x%x.\n",
			pci_resource_start(pcie_device, 0),
			pci_resource_len(pcie_device, 0));
		goto mhi_device_list_error;
	}

	mhi_pcie_dev->core.bar0_end = mhi_pcie_dev->core.bar0_base +
		pci_resource_len(pcie_device, 0);
	mhi_pcie_dev->core.bar2_base =
		ioremap_nocache(pci_resource_start(pcie_device, 2),
			pci_resource_len(pcie_device, 2));
	if (!mhi_pcie_dev->core.bar2_base) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to map bar 2 addr 0x%x len 0x%x.\n",
			pci_resource_start(pcie_device, 2),
			pci_resource_len(pcie_device, 2));
		goto io_map_err;
	}

	mhi_pcie_dev->core.bar2_end = mhi_pcie_dev->core.bar2_base +
		pci_resource_len(pcie_device, 2);

	if (!mhi_pcie_dev->core.bar0_base) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to register for pcie resources\n");
		goto mhi_pcie_read_ep_config_err;
	}

	mhi_log(MHI_MSG_INFO, "Device BAR0 address is at 0x%p\n",
			mhi_pcie_dev->core.bar0_base);
	ret_val = pci_request_region(pcie_device, 0, "mhi");
	if (ret_val)
		mhi_log(MHI_MSG_ERROR, "Could not request BAR0 region\n");

	mhi_pcie_dev->core.manufact_id = pcie_device->vendor;
	mhi_pcie_dev->core.dev_id = pcie_device->device;
	return 0;
io_map_err:
	iounmap((void *)mhi_pcie_dev->core.bar0_base);
mhi_device_list_error:
	pci_disable_device(pcie_device);
mhi_pcie_read_ep_config_err:
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
			irq_to_affin += mhi_dev_ctxt->dev_props->irq_base;
			irq_set_affinity(irq_to_affin, get_cpu_mask(cpu));
		}
	}
}

int mhi_cpu_notifier_cb(struct notifier_block *nfb, unsigned long action,
			void *hcpu)
{
	u32 cpu = (u32)hcpu;
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

int mhi_init_gpios(struct mhi_pcie_dev_info *mhi_pcie_dev)
{
	int ret_val = 0;
	struct device *dev = &mhi_pcie_dev->pcie_device->dev;
	struct device_node *np;

	np = dev->of_node;
	mhi_log(MHI_MSG_VERBOSE,
			"Attempting to grab DEVICE_WAKE gpio\n");
	ret_val = of_get_named_gpio(np, "mhi-device-wake-gpio", 0);
	switch (ret_val) {
	case -EPROBE_DEFER:
		mhi_log(MHI_MSG_VERBOSE, "DT is not ready\n");
		return ret_val;
	case -ENOENT:
		mhi_log(MHI_MSG_ERROR, "Failed to find device wake gpio\n");
		return ret_val;
	case 0:
		mhi_log(MHI_MSG_CRITICAL,
			"Could not get gpio from struct device tree!\n");
		return -EIO;
	default:
		mhi_pcie_dev->core.device_wake_gpio = ret_val;
		mhi_log(MHI_MSG_CRITICAL,
			"Got DEVICE_WAKE GPIO nr 0x%x from struct device tree\n",
			mhi_pcie_dev->core.device_wake_gpio);
		break;
	}

	ret_val = gpio_request(mhi_pcie_dev->core.device_wake_gpio, "mhi");
	if (ret_val) {
		mhi_log(MHI_MSG_CRITICAL,
			"Could not obtain struct device WAKE gpio\n");
		return ret_val;
	}
	mhi_log(MHI_MSG_VERBOSE,
		"Attempting to set output direction to DEVICE_WAKE gpio\n");
	/* This GPIO must never sleep as it can be set in timer ctxt */
	gpio_set_value_cansleep(mhi_pcie_dev->core.device_wake_gpio, 0);

	ret_val = gpio_direction_output(mhi_pcie_dev->core.device_wake_gpio, 1);

	if (ret_val) {
		mhi_log(MHI_MSG_VERBOSE,
			"Failed to set output direction of DEVICE_WAKE gpio\n");
		goto mhi_gpio_dir_err;
	}
	return 0;

mhi_gpio_dir_err:
	gpio_free(mhi_pcie_dev->core.device_wake_gpio);
	return -EIO;
}

int get_chan_props(struct mhi_device_ctxt *mhi_dev_ctxt, int chan,
		   struct mhi_chan_info *chan_info)
{
	char dt_prop[MAX_BUF_SIZE];
	int r;

	scnprintf(dt_prop, MAX_BUF_SIZE, "%s%d", "mhi-chan-cfg-", chan);
	r = of_property_read_u32_array(
			mhi_dev_ctxt->dev_info->plat_dev->dev.of_node,
			dt_prop, (u32 *)chan_info,
			sizeof(struct mhi_chan_info) / sizeof(u32));
	if (r)
		mhi_log(MHI_MSG_VERBOSE,
			"Failed to pull chan %d info from DT, %d\n", chan, r);
	return r;
}

int mhi_release_chan_ctxt(struct mhi_chan_ctxt *cc_list,
			  struct mhi_ring *ring)
{
	if (cc_list == NULL || ring == NULL)
		return -EINVAL;

	dma_free_coherent(NULL, ring->len, ring->base,
			 cc_list->mhi_trb_ring_base_addr);
	mhi_init_chan_ctxt(cc_list, 0, 0, 0, 0, 0, ring,
				MHI_CHAN_STATE_DISABLED);
	return 0;
}

void free_tre_ring(struct mhi_client_handle *client_handle)
{
	struct mhi_chan_ctxt *chan_ctxt;
	struct mhi_device_ctxt *mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	int chan = client_handle->chan_info.chan_nr;
	int r;

	chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
	r = mhi_release_chan_ctxt(chan_ctxt,
				&mhi_dev_ctxt->mhi_local_chan_ctxt[chan]);
	if (r)
		mhi_log(MHI_MSG_ERROR,
		"Failed to release chan %d ret %d\n", chan, r);
}

static int populate_tre_ring(struct mhi_client_handle *client_handle)
{
	dma_addr_t ring_dma_addr;
	void *ring_local_addr;
	struct mhi_chan_ctxt *chan_ctxt;
	struct mhi_device_ctxt *mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	u32 chan = client_handle->chan_info.chan_nr;
	u32 nr_desc = client_handle->chan_info.max_desc;

	mhi_log(MHI_MSG_INFO,
		"Entered chan %d requested desc %d\n", chan, nr_desc);

	chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
	ring_local_addr = dma_alloc_coherent(NULL,
				 nr_desc * sizeof(union mhi_xfer_pkt),
				 &ring_dma_addr, GFP_KERNEL);

	if (ring_local_addr == NULL)
		return -ENOMEM;

	mhi_init_chan_ctxt(chan_ctxt, ring_dma_addr,
			   (uintptr_t)ring_local_addr,
			   nr_desc,
			   GET_CHAN_PROPS(CHAN_DIR,
				client_handle->chan_info.flags),
			   client_handle->chan_info.ev_ring,
			   &mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
			   MHI_CHAN_STATE_ENABLED);

	mhi_log(MHI_MSG_INFO, "Exited\n");
	return 0;
}

enum MHI_STATUS mhi_open_channel(struct mhi_client_handle *client_handle)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	struct mhi_control_seg *mhi_ctrl_seg = NULL;
	int r = 0;
	int chan;

	if (NULL == client_handle ||
	    client_handle->magic != MHI_HANDLE_MAGIC)
		return MHI_STATUS_ERROR;

	mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	r = get_chan_props(mhi_dev_ctxt,
			    client_handle->chan_info.chan_nr,
			   &client_handle->chan_info);
	if (r)
		return MHI_STATUS_ERROR;

	chan = client_handle->chan_info.chan_nr;
	mhi_log(MHI_MSG_INFO,
		"Entered: Client opening chan 0x%x\n", chan);
	if (mhi_dev_ctxt->dev_exec_env <
		GET_CHAN_PROPS(CHAN_BRINGUP_STAGE,
				    client_handle->chan_info.flags)) {
		mhi_log(MHI_MSG_INFO,
			"Chan %d, MHI exec_env %d, not ready!\n",
			chan, mhi_dev_ctxt->dev_exec_env);
		return MHI_STATUS_DEVICE_NOT_READY;
	}

	mhi_ctrl_seg = client_handle->mhi_dev_ctxt->mhi_ctrl_seg;

	r = populate_tre_ring(client_handle);
	if (r) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to initialize tre ring chan %d ret %d\n",
			chan, r);
		return MHI_STATUS_ERROR;
	}

	client_handle->event_ring_index =
		mhi_ctrl_seg->mhi_cc_list[chan].mhi_event_ring_index;
	client_handle->msi_vec =
		mhi_ctrl_seg->mhi_ec_list[
			client_handle->event_ring_index].mhi_msi_vector;
	client_handle->intmod_t = mhi_ctrl_seg->mhi_ec_list[
			client_handle->event_ring_index].mhi_intmodt;

	init_completion(&client_handle->chan_open_complete);
	ret_val = start_chan_sync(client_handle);

	if (MHI_STATUS_SUCCESS != ret_val)
		mhi_log(MHI_MSG_ERROR,
			"Failed to start chan 0x%x\n", chan);
	client_handle->chan_status = 1;
	mhi_log(MHI_MSG_INFO,
		"Exited chan 0x%x\n", chan);
	return ret_val;
}
EXPORT_SYMBOL(mhi_open_channel);

enum MHI_STATUS mhi_register_channel(struct mhi_client_handle **client_handle,
		enum MHI_CLIENT_CHANNEL chan, s32 device_index,
		struct mhi_client_info_t *client_info, void *user_data)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL;

	if (!VALID_CHAN_NR(chan))
		return MHI_STATUS_INVALID_CHAN_ERR;

	if (NULL == client_handle || device_index < 0)
		return MHI_STATUS_ERROR;

	mhi_dev_ctxt = &(mhi_devices.device_list[device_index].mhi_ctxt);

	if (NULL != mhi_dev_ctxt->client_handle_list[chan])
		return MHI_STATUS_ALREADY_REGISTERED;

	mhi_log(MHI_MSG_INFO,
			"Opened channel 0x%x for client\n", chan);

	*client_handle = kzalloc(sizeof(struct mhi_client_handle), GFP_KERNEL);
	if (NULL == *client_handle)
		return MHI_STATUS_ALLOC_ERROR;

	mhi_dev_ctxt->client_handle_list[chan] = *client_handle;
	(*client_handle)->mhi_dev_ctxt = mhi_dev_ctxt;
	(*client_handle)->user_data = user_data;
	(*client_handle)->magic = MHI_HANDLE_MAGIC;
	(*client_handle)->chan_info.chan_nr = chan;

	if (NULL != client_info)
		(*client_handle)->client_info = *client_info;

	if (MHI_CLIENT_IP_HW_0_OUT  == chan)
		(*client_handle)->intmod_t = 10;
	if (MHI_CLIENT_IP_HW_0_IN  == chan)
		(*client_handle)->intmod_t = 10;

	if (mhi_dev_ctxt->dev_exec_env == MHI_EXEC_ENV_AMSS) {
		mhi_log(MHI_MSG_INFO,
			"Exec env is AMSS notifing client now chan: 0x%x\n",
			chan);
		mhi_notify_client(*client_handle, MHI_CB_MHI_ENABLED);
	}

	mhi_log(MHI_MSG_VERBOSE,
		"Successfuly registered chan 0x%x\n", chan);
	return MHI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(mhi_register_channel);

void mhi_close_channel(struct mhi_client_handle *client_handle)
{
	u32 chan;
	int r = 0;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;

	if (!client_handle ||
	    client_handle->magic != MHI_HANDLE_MAGIC ||
	    !client_handle->chan_status)
		return;

	chan = client_handle->chan_info.chan_nr;

	mhi_log(MHI_MSG_INFO, "Client attempting to close chan 0x%x\n", chan);
	init_completion(&client_handle->chan_reset_complete);
	if (!atomic_read(&client_handle->mhi_dev_ctxt->flags.pending_ssr)) {
		ret_val = mhi_send_cmd(client_handle->mhi_dev_ctxt,
					MHI_COMMAND_RESET_CHAN, chan);
		if (ret_val != MHI_STATUS_SUCCESS) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to send reset cmd for chan %d ret %d\n",
				chan, ret_val);
		}
		r = wait_for_completion_timeout(
				&client_handle->chan_reset_complete,
				msecs_to_jiffies(MHI_MAX_CMD_TIMEOUT));
		if (!r)
			mhi_log(MHI_MSG_ERROR,
					"Failed to reset chan %d ret %d\n",
					chan, r);
	} else {
		/*
		 * Assumption: Device is not playing with our
		 * buffers after BEFORE_SHUTDOWN
		 */
		mhi_log(MHI_MSG_INFO,
			"Pending SSR local free only chan %d.\n", chan);
	}

	mhi_log(MHI_MSG_INFO, "Freeing ring for chan 0x%x\n", chan);
	free_tre_ring(client_handle);
	mhi_log(MHI_MSG_INFO, "Chan 0x%x confirmed closed.\n", chan);
	client_handle->chan_status = 0;
}
EXPORT_SYMBOL(mhi_close_channel);

void mhi_update_chan_db(struct mhi_device_ctxt *mhi_dev_ctxt,
					  u32 chan)
{
	struct mhi_ring *chan_ctxt;
	u64 db_value;

	chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	db_value = virt_to_dma(NULL, chan_ctxt->wp);
	mhi_dev_ctxt->mhi_chan_db_order[chan]++;
	if (IS_HARDWARE_CHANNEL(chan) && chan_ctxt->dir == MHI_IN) {
		if ((mhi_dev_ctxt->counters.chan_pkts_xferd[chan] %
					MHI_XFER_DB_INTERVAL) == 0)
			mhi_process_db(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.chan_db_addr,
					chan, db_value);
	} else {
		mhi_process_db(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.chan_db_addr,
				chan, db_value);
	}
}

enum MHI_STATUS mhi_check_m2_transition(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_log(MHI_MSG_VERBOSE, "state = %d\n", mhi_dev_ctxt->mhi_state);
	if (mhi_dev_ctxt->mhi_state == MHI_STATE_M2) {
		mhi_log(MHI_MSG_INFO, "M2 Transition flag value = %d\n",
			(atomic_read(&mhi_dev_ctxt->flags.m2_transition)));
		if ((atomic_read(&mhi_dev_ctxt->flags.m2_transition)) == 0) {
			if (mhi_dev_ctxt->flags.link_up) {
				mhi_assert_device_wake(mhi_dev_ctxt);
				ret_val = MHI_STATUS_CHAN_NOT_READY;
			}
		} else{
			mhi_log(MHI_MSG_INFO, "M2 transition flag is set\n");
			ret_val = MHI_STATUS_CHAN_NOT_READY;
		}
	} else {
	   ret_val = MHI_STATUS_SUCCESS;
	}

	return ret_val;
}

static inline enum MHI_STATUS mhi_queue_tre(struct mhi_device_ctxt
							*mhi_dev_ctxt,
					    u32 chan,
					    enum MHI_RING_TYPE type)
{
	struct mhi_chan_ctxt *chan_ctxt;
	unsigned long flags = 0;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	u64 db_value = 0;
	chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
	mhi_dev_ctxt->counters.m1_m0++;
	mhi_log(MHI_MSG_VERBOSE, "Entered");

	if (type == MHI_RING_TYPE_CMD_RING)
		atomic_inc(&mhi_dev_ctxt->counters.outbound_acks);

	ret_val = mhi_check_m2_transition(mhi_dev_ctxt);
	if (likely(((ret_val == MHI_STATUS_SUCCESS) &&
	    (((mhi_dev_ctxt->mhi_state == MHI_STATE_M0) ||
	      (mhi_dev_ctxt->mhi_state == MHI_STATE_M1))) &&
	    (chan_ctxt->mhi_chan_state != MHI_CHAN_STATE_ERROR)) &&
	    (!mhi_dev_ctxt->flags.pending_M3))) {
		if (likely(type == MHI_RING_TYPE_XFER_RING)) {
			spin_lock_irqsave(&mhi_dev_ctxt->db_write_lock[chan],
					   flags);
			db_value = virt_to_dma(NULL,
				mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp);
			mhi_dev_ctxt->mhi_chan_db_order[chan]++;
			mhi_update_chan_db(mhi_dev_ctxt, chan);
			spin_unlock_irqrestore(
			   &mhi_dev_ctxt->db_write_lock[chan], flags);
		} else if (type == MHI_RING_TYPE_CMD_RING) {
			db_value = virt_to_dma(NULL,
				mhi_dev_ctxt->mhi_local_cmd_ctxt->wp);
			mhi_dev_ctxt->cmd_ring_order++;
			mhi_process_db(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.cmd_db_addr,
				0, db_value);
		} else {
			mhi_log(MHI_MSG_VERBOSE,
			"Wrong type of packet = %d\n", type);
			ret_val = MHI_STATUS_ERROR;
		}
	} else {
		mhi_log(MHI_MSG_VERBOSE,
			"Wakeup, pending data state %d chan state %d\n",
						 mhi_dev_ctxt->mhi_state,
						 chan_ctxt->mhi_chan_state);
			ret_val = MHI_STATUS_SUCCESS;
	}
	return ret_val;
}

enum MHI_STATUS mhi_queue_xfer(struct mhi_client_handle *client_handle,
		dma_addr_t buf, size_t buf_len, enum MHI_FLAGS mhi_flags)
{
	union mhi_xfer_pkt *pkt_loc;
	enum MHI_STATUS ret_val;
	enum MHI_CLIENT_CHANNEL chan;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	unsigned long flags;

	if (!client_handle || !buf || !buf_len) {
		mhi_log(MHI_MSG_CRITICAL, "Bad input args\n");
		return MHI_STATUS_ERROR;
	}
	MHI_ASSERT(VALID_BUF(buf, buf_len),
			"Client buffer is of invalid length\n");
	mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	chan = client_handle->chan_info.chan_nr;
	pm_runtime_get(&mhi_dev_ctxt->dev_info->plat_dev->dev);

	pkt_loc = mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp;
	pkt_loc->data_tx_pkt.buffer_ptr = buf;
	pkt_loc->type.info = mhi_flags;
	trace_mhi_tre(pkt_loc, chan, 0);

	if (likely(0 != client_handle->intmod_t))
		MHI_TRB_SET_INFO(TX_TRB_BEI, pkt_loc, 1);
	else
		MHI_TRB_SET_INFO(TX_TRB_BEI, pkt_loc, 0);

	MHI_TRB_SET_INFO(TX_TRB_TYPE, pkt_loc, MHI_PKT_TYPE_TRANSFER);
	MHI_TX_TRB_SET_LEN(TX_TRB_LEN, pkt_loc, buf_len);

	/* Ensure writes to descriptor are flushed */
	wmb();

	mhi_log(MHI_MSG_VERBOSE,
		"Channel %d Has buf size of %d and buf addr %lx, flags 0x%x\n",
				chan, buf_len, (uintptr_t)buf, mhi_flags);

	/* Add the TRB to the correct transfer ring */
	ret_val = ctxt_add_element(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				(void *)&pkt_loc);
	if (unlikely(MHI_STATUS_SUCCESS != ret_val)) {
		mhi_log(MHI_MSG_VERBOSE,
				"Failed to insert trb in xfer ring\n");
		goto error;
	}

	read_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	if (MHI_OUT ==
	    GET_CHAN_PROPS(CHAN_DIR, client_handle->chan_info.flags))
		atomic_inc(&mhi_dev_ctxt->counters.outbound_acks);
	ret_val = mhi_queue_tre(mhi_dev_ctxt, chan, MHI_RING_TYPE_XFER_RING);
	if (unlikely(MHI_STATUS_SUCCESS != ret_val))
		mhi_log(MHI_MSG_VERBOSE, "Failed queue TRE.\n");
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	read_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);

error:
	pm_runtime_mark_last_busy(&mhi_dev_ctxt->dev_info->plat_dev->dev);
	pm_runtime_put_noidle(&mhi_dev_ctxt->dev_info->plat_dev->dev);
	return ret_val;
}
EXPORT_SYMBOL(mhi_queue_xfer);

enum MHI_STATUS mhi_send_cmd(struct mhi_device_ctxt *mhi_dev_ctxt,
			enum MHI_COMMAND cmd, u32 chan)
{
	unsigned long flags = 0;
	union mhi_cmd_pkt *cmd_pkt = NULL;
	enum MHI_CHAN_STATE from_state = MHI_CHAN_STATE_DISABLED;
	enum MHI_CHAN_STATE to_state = MHI_CHAN_STATE_DISABLED;
	enum MHI_PKT_TYPE ring_el_type = MHI_PKT_TYPE_NOOP_CMD;
	struct mutex *chan_mutex = NULL;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;

	if (chan >= MHI_MAX_CHANNELS ||
		cmd >= MHI_COMMAND_MAX_NR || mhi_dev_ctxt == NULL) {
		mhi_log(MHI_MSG_ERROR,
			"Invalid channel id, received id: 0x%x", chan);
		return MHI_STATUS_ERROR;
	}

	mhi_log(MHI_MSG_INFO,
		"Entered, MHI state %d dev_exec_env %d chan %d cmd %d\n",
			mhi_dev_ctxt->mhi_state,
			mhi_dev_ctxt->dev_exec_env,
			chan, cmd);

	pm_runtime_get(&mhi_dev_ctxt->dev_info->plat_dev->dev);
	/*
	 * If there is a cmd pending a device confirmation,
	 * do not send anymore for this channel
	 */
	if (MHI_CMD_PENDING == mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan]) {
		mhi_log(MHI_MSG_ERROR, "Cmd Pending on chan %d", chan);
		ret_val = MHI_STATUS_CMD_PENDING;
		goto error_invalid;
	}

	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	from_state =
		mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan].mhi_chan_state;

	switch (cmd) {
		break;
	case MHI_COMMAND_RESET_CHAN:
		to_state = MHI_CHAN_STATE_DISABLED;
		ring_el_type = MHI_PKT_TYPE_RESET_CHAN_CMD;
		break;
	case MHI_COMMAND_START_CHAN:
		switch (from_state) {
		case MHI_CHAN_STATE_DISABLED:
		case MHI_CHAN_STATE_ENABLED:
		case MHI_CHAN_STATE_STOP:
			to_state = MHI_CHAN_STATE_RUNNING;
			break;
		default:
			mhi_log(MHI_MSG_ERROR,
				"Invalid state transition for "
				"cmd 0x%x, from_state 0x%x\n",
				cmd, from_state);
			ret_val = MHI_STATUS_BAD_STATE;
			goto error_invalid;
		}
		ring_el_type = MHI_PKT_TYPE_START_CHAN_CMD;
		break;
	default:
		mhi_log(MHI_MSG_ERROR, "Bad command received\n");
	}

	mutex_lock(&mhi_dev_ctxt->mhi_cmd_mutex_list[PRIMARY_CMD_RING]);
	ret_val = ctxt_add_element(mhi_dev_ctxt->mhi_local_cmd_ctxt,
			(void *)&cmd_pkt);
	if (ret_val) {
		mhi_log(MHI_MSG_ERROR, "Failed to insert element\n");
		goto error_general;
	}
	chan_mutex = &mhi_dev_ctxt->mhi_chan_mutex[chan];
	mutex_lock(chan_mutex);
	MHI_TRB_SET_INFO(CMD_TRB_TYPE, cmd_pkt, ring_el_type);
	MHI_TRB_SET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
	mutex_unlock(chan_mutex);
	mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] = MHI_CMD_PENDING;

	read_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_queue_tre(mhi_dev_ctxt, 0, MHI_RING_TYPE_CMD_RING);
	read_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);

	mhi_log(MHI_MSG_VERBOSE, "Sent command 0x%x for chan %d\n",
								cmd, chan);
error_general:
	mutex_unlock(&mhi_dev_ctxt->mhi_cmd_mutex_list[PRIMARY_CMD_RING]);
error_invalid:
	pm_runtime_mark_last_busy(&mhi_dev_ctxt->dev_info->plat_dev->dev);
	pm_runtime_put_noidle(&mhi_dev_ctxt->dev_info->plat_dev->dev);

	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	mhi_log(MHI_MSG_INFO, "Exited ret %d.\n", ret_val);
	return ret_val;
}

static enum MHI_STATUS parse_outbound(struct mhi_device_ctxt *mhi_dev_ctxt,
		u32 chan, union mhi_xfer_pkt *local_ev_trb_loc, u16 xfer_len)
{
	struct mhi_result *result = NULL;
	enum MHI_STATUS ret_val = 0;
	struct mhi_client_handle *client_handle = NULL;
	struct mhi_ring *local_chan_ctxt = NULL;
	struct mhi_cb_info cb_info;
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	client_handle = mhi_dev_ctxt->client_handle_list[chan];

	/* If ring is empty */
	MHI_ASSERT(!unlikely(mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp ==
	    mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp), "Empty Event Ring\n");

	if (NULL != client_handle) {
		result = &mhi_dev_ctxt->client_handle_list[chan]->result;

		if (NULL != (&client_handle->client_info.mhi_client_cb)) {
			cb_info.cb_reason = MHI_CB_XFER;
			cb_info.result = &client_handle->result;
			cb_info.chan = chan;
			client_handle->client_info.mhi_client_cb(&cb_info);
		}
	}
	ret_val = ctxt_del_element(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
						NULL);
	atomic_dec(&mhi_dev_ctxt->counters.outbound_acks);
	mhi_log(MHI_MSG_VERBOSE,
		"Processed outbound ack chan %d Pending acks %d.\n",
		chan, atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS parse_inbound(struct mhi_device_ctxt *mhi_dev_ctxt,
		u32 chan, union mhi_xfer_pkt *local_ev_trb_loc, u16 xfer_len)
{
	struct mhi_client_handle *client_handle;
	struct mhi_ring *local_chan_ctxt;
	struct mhi_result *result;
	struct mhi_cb_info cb_info;

	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];

	MHI_ASSERT(!unlikely(mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp ==
	    mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp), "Empty Event Ring\n");

	if (NULL != mhi_dev_ctxt->client_handle_list[chan])
		result = &mhi_dev_ctxt->client_handle_list[chan]->result;

	/* If a client is registered */
	if (unlikely(IS_SOFTWARE_CHANNEL(chan))) {
		MHI_TX_TRB_SET_LEN(TX_TRB_LEN,
		local_ev_trb_loc,
		xfer_len);
		ctxt_del_element(local_chan_ctxt, NULL);
		if (NULL != client_handle->client_info.mhi_client_cb) {
			cb_info.cb_reason = MHI_CB_XFER;
			cb_info.result = &client_handle->result;
			cb_info.chan = chan;
			client_handle->client_info.mhi_client_cb(&cb_info);
		} else {
			mhi_log(MHI_MSG_VERBOSE,
				"No client registered chan %d\n", chan);
		}
	} else  {
		/* IN Hardware channel with no client
		 * registered, we are done with this TRB*/
		if (likely(NULL != client_handle)) {
			ctxt_del_element(local_chan_ctxt, NULL);
		/* A client is not registred for this IN channel */
		} else  {/* Hardware Channel, no client registerered,
				drop data */
			recycle_trb_and_ring(mhi_dev_ctxt,
				 &mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				 MHI_RING_TYPE_XFER_RING,
				 chan);
		}
	}
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS validate_xfer_el_addr(struct mhi_chan_ctxt *ring,
							uintptr_t addr)
{
	return (addr < (ring->mhi_trb_ring_base_addr) ||
			addr > (ring->mhi_trb_ring_base_addr)
			+ (ring->mhi_trb_ring_len - 1)) ?
		MHI_STATUS_ERROR : MHI_STATUS_SUCCESS;
}

enum MHI_STATUS parse_xfer_event(struct mhi_device_ctxt *ctxt,
					union mhi_event_pkt *event)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = (struct mhi_device_ctxt *)ctxt;
	struct mhi_result *result;
	u32 chan = MHI_MAX_CHANNELS;
	u16 xfer_len;
	uintptr_t phy_ev_trb_loc;
	union mhi_xfer_pkt *local_ev_trb_loc;
	struct mhi_client_handle *client_handle;
	union mhi_xfer_pkt *local_trb_loc;
	struct mhi_chan_ctxt *chan_ctxt;
	u32 nr_trb_to_parse;
	u32 i = 0;
	u32 ev_code;

	trace_mhi_ev(event);
	chan = MHI_EV_READ_CHID(EV_CHID, event);
	if (unlikely(!VALID_CHAN_NR(chan))) {
		mhi_log(MHI_MSG_ERROR, "Bad ring id.\n");
		return MHI_STATUS_ERROR;
	}
	ev_code = MHI_EV_READ_CODE(EV_TRB_CODE, event);
	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	client_handle->pkt_count++;
	result = &client_handle->result;
	mhi_log(MHI_MSG_VERBOSE,
		"Event Received, chan %d, cc_code %d\n",
		chan, ev_code);
	if (ev_code == MHI_EVENT_CC_OVERFLOW)
		result->transaction_status = MHI_STATUS_OVERFLOW;
	else
		result->transaction_status = MHI_STATUS_SUCCESS;

	switch (ev_code) {
	case MHI_EVENT_CC_OVERFLOW:
	case MHI_EVENT_CC_EOB:
	case MHI_EVENT_CC_EOT:
	{
		dma_addr_t trb_data_loc;
		u32 ieot_flag;
		enum MHI_STATUS ret_val;
		struct mhi_ring *local_chan_ctxt;

		local_chan_ctxt =
			&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
		phy_ev_trb_loc = MHI_EV_READ_PTR(EV_PTR, event);

		chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
		ret_val = validate_xfer_el_addr(chan_ctxt,
						phy_ev_trb_loc);

		if (unlikely(MHI_STATUS_SUCCESS != ret_val)) {
			mhi_log(MHI_MSG_ERROR, "Bad event trb ptr.\n");
			break;
		}

		/* Get the TRB this event points to */
		local_ev_trb_loc = dma_to_virt(NULL, phy_ev_trb_loc);
		local_trb_loc = (union mhi_xfer_pkt *)local_chan_ctxt->rp;

		trace_mhi_tre(local_trb_loc, chan, 1);

		ret_val = get_nr_enclosed_el(local_chan_ctxt,
				      local_trb_loc,
				      local_ev_trb_loc,
				      &nr_trb_to_parse);
		if (unlikely(MHI_STATUS_SUCCESS != ret_val)) {
			mhi_log(MHI_MSG_CRITICAL,
				"Failed to get nr available trbs ret: %d.\n",
				ret_val);
			return MHI_STATUS_ERROR;
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

			if (!VALID_BUF(trb_data_loc, xfer_len)) {
				mhi_log(MHI_MSG_CRITICAL,
					"Bad buffer ptr: %lx.\n",
					(uintptr_t)trb_data_loc);
				return MHI_STATUS_ERROR;
			}

			if (NULL != client_handle) {
				result->payload_buf = trb_data_loc;
				result->bytes_xferd = xfer_len;
				result->user_data = client_handle->user_data;
			}
			if (local_chan_ctxt->dir == MHI_IN) {
				parse_inbound(mhi_dev_ctxt, chan,
						local_ev_trb_loc, xfer_len);
			} else {
				parse_outbound(mhi_dev_ctxt, chan,
						local_ev_trb_loc, xfer_len);
			}
			mhi_dev_ctxt->counters.chan_pkts_xferd[chan]++;
			if (local_trb_loc ==
				(union mhi_xfer_pkt *)local_chan_ctxt->rp) {
				mhi_log(MHI_MSG_CRITICAL,
					"Done. Processed until: %lx.\n",
					(uintptr_t)trb_data_loc);
				break;
			} else {
				local_trb_loc =
					(union mhi_xfer_pkt *)local_chan_ctxt->
					rp;
			}
			i++;
		} while (i < nr_trb_to_parse);
		break;
	} /* CC_EOT */
	case MHI_EVENT_CC_OOB:
	case MHI_EVENT_CC_DB_MODE:
	{
		struct mhi_ring *chan_ctxt = NULL;
		u64 db_value = 0;
		mhi_dev_ctxt->flags.uldl_enabled = 1;
		chan = MHI_EV_READ_CHID(EV_CHID, event);
		mhi_dev_ctxt->flags.db_mode[chan] = 1;
		chan_ctxt =
			&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
		mhi_log(MHI_MSG_INFO, "DB_MODE/OOB Detected chan %d.\n", chan);
		if (chan_ctxt->wp != chan_ctxt->rp) {
			db_value = virt_to_dma(NULL, chan_ctxt->wp);
			mhi_process_db(mhi_dev_ctxt,
				     mhi_dev_ctxt->mmio_info.chan_db_addr, chan,
				     db_value);
		}
		client_handle = mhi_dev_ctxt->client_handle_list[chan];
			if (NULL != client_handle) {
				result->transaction_status =
						MHI_STATUS_DEVICE_NOT_READY;
			}
		break;
	}
	default:
		mhi_log(MHI_MSG_ERROR,
			"Unknown TX completion.\n");
		break;
	} /*switch(MHI_EV_READ_CODE(EV_TRB_CODE,event)) */
	return 0;
}

enum MHI_STATUS recycle_trb_and_ring(struct mhi_device_ctxt *mhi_dev_ctxt,
		struct mhi_ring *ring,
		enum MHI_RING_TYPE ring_type,
		u32 ring_index)
{
	enum MHI_STATUS ret_val = MHI_STATUS_ERROR;
	u64 db_value = 0;
	void *removed_element = NULL;
	void *added_element = NULL;

	ret_val = ctxt_del_element(ring, &removed_element);

	if (MHI_STATUS_SUCCESS != ret_val) {
		mhi_log(MHI_MSG_ERROR, "Could not remove element from ring\n");
		return MHI_STATUS_ERROR;
	}
	ret_val = ctxt_add_element(ring, &added_element);
	if (MHI_STATUS_SUCCESS != ret_val)
		mhi_log(MHI_MSG_ERROR, "Could not add element to ring\n");
	db_value = virt_to_dma(NULL, ring->wp);
	if (MHI_STATUS_SUCCESS != ret_val)
		return ret_val;
	if (MHI_RING_TYPE_XFER_RING == ring_type) {
		union mhi_xfer_pkt *removed_xfer_pkt =
			(union mhi_xfer_pkt *)removed_element;
		union mhi_xfer_pkt *added_xfer_pkt =
			(union mhi_xfer_pkt *)added_element;
		added_xfer_pkt->data_tx_pkt =
				*(struct mhi_tx_pkt *)removed_xfer_pkt;
	} else if (MHI_RING_TYPE_EVENT_RING == ring_type &&
		   mhi_dev_ctxt->counters.m0_m3 > 0 &&
		   IS_HARDWARE_CHANNEL(ring_index)) {
		spinlock_t *lock;
		unsigned long flags;

		if (ring_index >= mhi_dev_ctxt->mmio_info.nr_event_rings)
			return MHI_STATUS_ERROR;

		lock = &mhi_dev_ctxt->mhi_ev_spinlock_list[ring_index];
		spin_lock_irqsave(lock, flags);
		db_value = virt_to_dma(NULL, ring->wp);
		mhi_update_ctxt(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.event_db_addr,
				ring_index, db_value);

		mhi_dev_ctxt->mhi_ev_db_order[ring_index] = 1;
		mhi_dev_ctxt->counters.ev_counter[ring_index]++;
		spin_unlock_irqrestore(lock, flags);
	}
	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	/* Asserting Device Wake here, will imediately wake mdm */
	if ((MHI_STATE_M0 == mhi_dev_ctxt->mhi_state ||
	     MHI_STATE_M1 == mhi_dev_ctxt->mhi_state) &&
	     mhi_dev_ctxt->flags.link_up) {
		switch (ring_type) {
		case MHI_RING_TYPE_CMD_RING:
		{
			struct mutex *cmd_mutex = NULL;
			cmd_mutex =
				&mhi_dev_ctxt->
				mhi_cmd_mutex_list[PRIMARY_CMD_RING];
			mutex_lock(cmd_mutex);
			mhi_dev_ctxt->cmd_ring_order = 1;
			mhi_process_db(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.cmd_db_addr,
				ring_index, db_value);
			mutex_unlock(cmd_mutex);
			break;
		}
		case MHI_RING_TYPE_EVENT_RING:
		{
			spinlock_t *lock = NULL;
			unsigned long flags = 0;
			lock = &mhi_dev_ctxt->mhi_ev_spinlock_list[ring_index];
			spin_lock_irqsave(lock, flags);
			mhi_dev_ctxt->mhi_ev_db_order[ring_index] = 1;
			if ((mhi_dev_ctxt->counters.ev_counter[ring_index] %
						MHI_EV_DB_INTERVAL) == 0) {
				db_value = virt_to_dma(NULL, ring->wp);
				mhi_process_db(mhi_dev_ctxt,
					mhi_dev_ctxt->mmio_info.event_db_addr,
					ring_index, db_value);
			}
			spin_unlock_irqrestore(lock, flags);
			break;
		}
		case MHI_RING_TYPE_XFER_RING:
		{
			unsigned long flags = 0;
			spin_lock_irqsave(
				&mhi_dev_ctxt->db_write_lock[ring_index],
				flags);
			mhi_dev_ctxt->mhi_chan_db_order[ring_index] = 1;
			mhi_process_db(mhi_dev_ctxt,
					mhi_dev_ctxt->mmio_info.chan_db_addr,
					ring_index, db_value);
			spin_unlock_irqrestore(
				&mhi_dev_ctxt->db_write_lock[ring_index],
				flags);
			break;
		}
		default:
			mhi_log(MHI_MSG_ERROR, "Bad ring type\n");
		}
	}
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	return ret_val;
}

static enum MHI_STATUS reset_chan_cmd(struct mhi_device_ctxt *mhi_dev_ctxt,
						union mhi_cmd_pkt *cmd_pkt)
{
	u32 chan  = 0;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	struct mhi_ring *local_chan_ctxt;
	struct mhi_chan_ctxt *chan_ctxt;
	struct mhi_client_handle *client_handle = NULL;
	struct mutex *chan_mutex;
	int pending_el = 0;
	struct mhi_ring *ring;

	MHI_TRB_GET_INFO(CMD_TRB_CHID, cmd_pkt, chan);

	if (!VALID_CHAN_NR(chan)) {
		mhi_log(MHI_MSG_ERROR,
			"Bad channel number for CCE\n");
		return MHI_STATUS_ERROR;
	}

	chan_mutex = &mhi_dev_ctxt->mhi_chan_mutex[chan];
	mutex_lock(chan_mutex);
	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
	mhi_log(MHI_MSG_INFO, "Processed cmd reset event\n");

	/*
	 * If outbound elements are pending, they must be cleared since
	 * they will never be acked after a channel reset.
	 */
	ring = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	if (ring->dir == MHI_OUT)
		get_nr_enclosed_el(ring, ring->rp, ring->wp, &pending_el);

	mhi_log(MHI_MSG_INFO, "Decrementing chan %d out acks by %d.\n",
				chan, pending_el);

	atomic_sub(pending_el, &mhi_dev_ctxt->counters.outbound_acks);

	/* Reset the local channel context */
	local_chan_ctxt->rp = local_chan_ctxt->base;
	local_chan_ctxt->wp = local_chan_ctxt->base;
	local_chan_ctxt->ack_rp = local_chan_ctxt->base;

	/* Reset the mhi channel context */
	chan_ctxt->mhi_chan_state = MHI_CHAN_STATE_DISABLED;
	chan_ctxt->mhi_trb_read_ptr = chan_ctxt->mhi_trb_ring_base_addr;
	chan_ctxt->mhi_trb_write_ptr = chan_ctxt->mhi_trb_ring_base_addr;

	mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] = MHI_CMD_NOT_PENDING;
	mutex_unlock(chan_mutex);
	mhi_log(MHI_MSG_INFO, "Reset complete.\n");
	if (NULL != client_handle)
		complete(&client_handle->chan_reset_complete);
	return ret_val;
}

static enum MHI_STATUS start_chan_cmd(struct mhi_device_ctxt *mhi_dev_ctxt,
						union mhi_cmd_pkt *cmd_pkt)
{
	u32 chan;
	MHI_TRB_GET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
	if (!VALID_CHAN_NR(chan))
		mhi_log(MHI_MSG_ERROR, "Bad chan: 0x%x\n", chan);
	mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] =
					MHI_CMD_NOT_PENDING;
	mhi_log(MHI_MSG_INFO, "Processed START CMD chan %d\n", chan);
	if (NULL != mhi_dev_ctxt->client_handle_list[chan])
		complete(
		&mhi_dev_ctxt->client_handle_list[chan]->chan_open_complete);
	return MHI_STATUS_SUCCESS;
}

enum MHI_EVENT_CCS get_cmd_pkt(union mhi_event_pkt *ev_pkt,
			       union mhi_cmd_pkt **cmd_pkt)
{
	uintptr_t phy_trb_loc = 0;
	if (NULL != ev_pkt)
		phy_trb_loc = (uintptr_t)MHI_EV_READ_PTR(EV_PTR,
							ev_pkt);
	else
		return MHI_STATUS_ERROR;
	*cmd_pkt = dma_to_virt(NULL, phy_trb_loc);
	return MHI_EV_READ_CODE(EV_TRB_CODE, ev_pkt);
}

enum MHI_STATUS parse_cmd_event(struct mhi_device_ctxt *mhi_dev_ctxt,
						union mhi_event_pkt *ev_pkt)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	union mhi_cmd_pkt *cmd_pkt = NULL;
	u32 event_code;
	event_code = get_cmd_pkt(ev_pkt, &cmd_pkt);
	switch (event_code) {
	case MHI_EVENT_CC_SUCCESS:
	{
		u32 chan;
		MHI_TRB_GET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
		switch (MHI_TRB_READ_INFO(CMD_TRB_TYPE, cmd_pkt)) {

		mhi_log(MHI_MSG_INFO, "CCE chan %d cmd %d\n",
				chan,
				MHI_TRB_READ_INFO(CMD_TRB_TYPE, cmd_pkt));

		case MHI_PKT_TYPE_RESET_CHAN_CMD:
			if (MHI_STATUS_SUCCESS != reset_chan_cmd(mhi_dev_ctxt,
								cmd_pkt))
				mhi_log(MHI_MSG_INFO,
					"Failed to process reset cmd\n");
			break;
		case MHI_PKT_TYPE_STOP_CHAN_CMD:
			if (MHI_STATUS_SUCCESS != ret_val) {
				mhi_log(MHI_MSG_INFO,
						"Failed to set chan state\n");
				return MHI_STATUS_ERROR;
			}
			break;
		case MHI_PKT_TYPE_START_CHAN_CMD:
			if (MHI_STATUS_SUCCESS != start_chan_cmd(mhi_dev_ctxt,
								cmd_pkt))
				mhi_log(MHI_MSG_INFO,
					"Failed to process reset cmd\n");
			break;
		default:
			mhi_log(MHI_MSG_INFO,
				"Bad cmd type 0x%x\n",
				MHI_TRB_READ_INFO(CMD_TRB_TYPE, cmd_pkt));
			break;
		}
		mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] = MHI_CMD_NOT_PENDING;
		atomic_dec(&mhi_dev_ctxt->counters.outbound_acks);
		BUG_ON(atomic_read(&mhi_dev_ctxt->counters.outbound_acks) >= 0);
		break;
	}
	default:
		mhi_log(MHI_MSG_INFO, "Unhandled mhi completion code\n");
		break;
	}
	ctxt_del_element(mhi_dev_ctxt->mhi_local_cmd_ctxt, NULL);
	return MHI_STATUS_SUCCESS;
}

int mhi_poll_inbound(struct mhi_client_handle *client_handle,
		     struct mhi_result *result)
{
	struct mhi_tx_pkt *pending_trb = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL;
	u32 chan = 0;
	struct mhi_ring *local_chan_ctxt;
	struct mutex *chan_mutex = NULL;
	int ret_val = 0;

	if (NULL == client_handle || NULL == result ||
			NULL == client_handle->mhi_dev_ctxt)
		return -EINVAL;
	mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	chan = client_handle->chan_info.chan_nr;
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	chan_mutex = &mhi_dev_ctxt->mhi_chan_mutex[chan];
	mutex_lock(chan_mutex);
	if ((local_chan_ctxt->rp != local_chan_ctxt->ack_rp)) {
		pending_trb = (struct mhi_tx_pkt *)(local_chan_ctxt->ack_rp);
		result->payload_buf = pending_trb->buffer_ptr;
		result->bytes_xferd = MHI_TX_TRB_GET_LEN(TX_TRB_LEN,
					(union mhi_xfer_pkt *)pending_trb);
		result->flags = pending_trb->info;
		ret_val = delete_element(local_chan_ctxt,
					&local_chan_ctxt->ack_rp,
					&local_chan_ctxt->rp, NULL);
		if (ret_val != MHI_STATUS_SUCCESS) {
			mhi_log(MHI_MSG_ERROR,
				"Internal Failure, inconsistent ring state, ret %d chan %d\n",
				ret_val, chan);
			result->payload_buf = 0;
			result->bytes_xferd = 0;
			result->transaction_status = MHI_STATUS_ERROR;
		}
	} else {
		result->payload_buf = 0;
		result->bytes_xferd = 0;
		ret_val = MHI_STATUS_RING_EMPTY;
	}
	mutex_unlock(chan_mutex);
	return ret_val;
}
EXPORT_SYMBOL(mhi_poll_inbound);


enum MHI_STATUS validate_ev_el_addr(struct mhi_ring *ring, uintptr_t addr)
{
	return (addr < (uintptr_t)(ring->base) ||
			addr > ((uintptr_t)(ring->base)
				+ (ring->len - 1))) ?
		MHI_STATUS_ERROR : MHI_STATUS_SUCCESS;
}

enum MHI_STATUS validate_ring_el_addr(struct mhi_ring *ring, uintptr_t addr)
{
	return (addr < (uintptr_t)(ring->base) ||
		addr > ((uintptr_t)(ring->base)
			+ (ring->len - 1))) ?
		MHI_STATUS_ERROR : MHI_STATUS_SUCCESS;
}

enum MHI_STATUS mhi_wait_for_mdm(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 j = 0;

	while (mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr, MHIREGLEN)
			== 0xFFFFFFFF
			&& j <= MHI_MAX_LINK_RETRIES) {
		mhi_log(MHI_MSG_CRITICAL,
				"Could not access MDM retry %d\n", j);
		msleep(MHI_LINK_STABILITY_WAIT_MS);
		if (MHI_MAX_LINK_RETRIES == j) {
			mhi_log(MHI_MSG_CRITICAL,
				"Could not access MDM, FAILING!\n");
			return MHI_STATUS_ERROR;
		}
		j++;
	}
	return MHI_STATUS_SUCCESS;
}

int mhi_get_max_desc(struct mhi_client_handle *client_handle)
{
	return client_handle->chan_info.max_desc - 1;
}
EXPORT_SYMBOL(mhi_get_max_desc);

int mhi_get_epid(struct mhi_client_handle *client_handle)
{
	return MHI_EPID;
}

int mhi_assert_device_wake(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	if ((mhi_dev_ctxt->mmio_info.chan_db_addr) &&
	       (mhi_dev_ctxt->flags.link_up)) {
			mhi_log(MHI_MSG_VERBOSE, "LPM %d\n",
				mhi_dev_ctxt->enable_lpm);
			atomic_set(&mhi_dev_ctxt->flags.device_wake, 1);
			mhi_write_db(mhi_dev_ctxt,
				     mhi_dev_ctxt->mmio_info.chan_db_addr,
				     MHI_DEV_WAKE_DB, 1);
			mhi_dev_ctxt->device_wake_asserted = 1;
	} else {
		mhi_log(MHI_MSG_VERBOSE, "LPM %d\n", mhi_dev_ctxt->enable_lpm);
	}
	return 0;
}

inline int mhi_deassert_device_wake(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	if ((mhi_dev_ctxt->enable_lpm) &&
	    (atomic_read(&mhi_dev_ctxt->flags.device_wake)) &&
	    (mhi_dev_ctxt->mmio_info.chan_db_addr != NULL) &&
	    (mhi_dev_ctxt->flags.link_up)) {
		mhi_log(MHI_MSG_VERBOSE, "LPM %d\n", mhi_dev_ctxt->enable_lpm);
		atomic_set(&mhi_dev_ctxt->flags.device_wake, 0);
		mhi_write_db(mhi_dev_ctxt, mhi_dev_ctxt->mmio_info.chan_db_addr,
				MHI_DEV_WAKE_DB, 0);
		mhi_dev_ctxt->device_wake_asserted = 0;
	} else {
		mhi_log(MHI_MSG_VERBOSE, "LPM %d DEV_WAKE %d link %d\n",
				mhi_dev_ctxt->enable_lpm,
				atomic_read(&mhi_dev_ctxt->flags.device_wake),
				mhi_dev_ctxt->flags.link_up);
	}
	return 0;
}

int mhi_set_lpm(struct mhi_client_handle *client_handle, int enable_lpm)
{
	mhi_log(MHI_MSG_VERBOSE, "LPM Set %d\n", enable_lpm);
	client_handle->mhi_dev_ctxt->enable_lpm = enable_lpm ? 1 : 0;
	return 0;
}

int mhi_set_bus_request(struct mhi_device_ctxt *mhi_dev_ctxt,
				int index)
{
	mhi_log(MHI_MSG_INFO, "Setting bus request to index %d\n", index);
	return msm_bus_scale_client_update_request(mhi_dev_ctxt->bus_client,
								index);
}

enum MHI_STATUS mhi_deregister_channel(struct mhi_client_handle
							*client_handle) {
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	int chan;
	if (!client_handle || client_handle->magic != MHI_HANDLE_MAGIC)
		return MHI_STATUS_ERROR;
	chan = client_handle->chan_info.chan_nr;
	client_handle->magic = 0;
	client_handle->mhi_dev_ctxt->client_handle_list[chan] = NULL;
	kfree(client_handle);
	return ret_val;
}

EXPORT_SYMBOL(mhi_deregister_channel);

void mhi_process_db(struct mhi_device_ctxt *mhi_dev_ctxt,
		  void __iomem *io_addr,
		  uintptr_t chan, u32 val)
{
	mhi_log(MHI_MSG_VERBOSE,
			"db.set addr: %p io_offset 0x%lx val:0x%x\n",
			io_addr, chan, val);

	mhi_update_ctxt(mhi_dev_ctxt, io_addr, chan, val);

	/* Channel Doorbell and Polling Mode Disabled or Software Channel*/
	if (io_addr == mhi_dev_ctxt->mmio_info.chan_db_addr) {
		if (!(IS_HARDWARE_CHANNEL(chan) &&
		    mhi_dev_ctxt->flags.uldl_enabled &&
		    !mhi_dev_ctxt->flags.db_mode[chan])) {
			mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
			mhi_dev_ctxt->flags.db_mode[chan] = 0;
		}
	/* Event Doorbell and Polling mode Disabled */
	} else if (io_addr == mhi_dev_ctxt->mmio_info.event_db_addr) {
		/* Only ring for software channel */
		if (IS_SOFTWARE_CHANNEL(chan) ||
		    !mhi_dev_ctxt->flags.uldl_enabled) {
			mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
			mhi_dev_ctxt->flags.db_mode[chan] = 0;
		}
	} else {
		mhi_write_db(mhi_dev_ctxt, io_addr, chan, val);
		mhi_dev_ctxt->flags.db_mode[chan] = 0;
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
	mhi_log(MHI_MSG_VERBOSE, "d.s 0x%p off: 0x%lx 0x%x\n",
					io_addr, io_offset, val);
	iowrite32(val, io_addr + io_offset);
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
