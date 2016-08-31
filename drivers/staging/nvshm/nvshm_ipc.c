/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_iobuf.h"
#include "nvshm_ipc.h"
#include "nvshm_queue.h"

#include <linux/interrupt.h>
#include <asm/mach/map.h>
#include <mach/tegra_bb.h>
#include <asm/cacheflush.h>

#define NVSHM_WAKE_TIMEOUT_NS (20 * NSEC_PER_MSEC)
#define NVSHM_WAKE_MAX_COUNT (50)

static int ipc_readconfig(struct nvshm_handle *handle)
{
	struct nvshm_config *conf;
	int chan;

	pr_debug("%s\n", __func__);

	conf = (struct nvshm_config *)(handle->mb_base_virt
				       + NVSHM_CONFIG_OFFSET);
	/* No change in v2.x kernel prevents from running v1.3 modems, so let's
	 * ensure some continuity of service.
	 */
	if ((conf->version == NVSHM_CONFIG_VERSION_1_3) &&
	    (NVSHM_MAJOR(NVSHM_CONFIG_VERSION) == 2)) {
		pr_warn("%s BBC version 1.3, statistics not available\n",
			__func__);
	} else if (NVSHM_MAJOR(conf->version) !=
					NVSHM_MAJOR(NVSHM_CONFIG_VERSION)) {
		pr_err("%s SHM version mismatch: BBC: %d.%d / AP: %d.%d\n",
		       __func__,
		       NVSHM_MAJOR(conf->version),
		       NVSHM_MINOR(conf->version),
		       NVSHM_MAJOR(NVSHM_CONFIG_VERSION),
		       NVSHM_MINOR(NVSHM_CONFIG_VERSION));
		return -1;
	} else if (NVSHM_MINOR(conf->version) !=
					NVSHM_MINOR(NVSHM_CONFIG_VERSION)) {
		pr_warn("%s SHM versions differ: BBC: %d.%d / AP: %d.%d\n",
			__func__,
			NVSHM_MAJOR(conf->version),
			NVSHM_MINOR(conf->version),
			NVSHM_MAJOR(NVSHM_CONFIG_VERSION),
			NVSHM_MINOR(NVSHM_CONFIG_VERSION));
	}

	if (handle->ipc_size != conf->shmem_size) {
		pr_warn("%s shmem mapped/reported not matching: 0x%x/0x%x\n",
			__func__, (unsigned int)handle->ipc_size,
			conf->shmem_size);
	}
	handle->desc_base_virt = handle->ipc_base_virt
		+ conf->region_ap_desc_offset;
	pr_debug("%s desc_base_virt=0x%p\n",
		 __func__, handle->desc_base_virt);

	handle->desc_size = conf->region_ap_desc_size;
	pr_debug("%s desc_size=%d\n",
		 __func__, (int)handle->desc_size);

	/* Data is cached */
	handle->data_base_virt = handle->ipc_base_virt
		+ conf->region_ap_data_offset;
	pr_debug("%s data_base_virt=0x%p\n",
		 __func__, handle->data_base_virt);

	handle->data_size = conf->region_ap_data_size;
	pr_debug("%s data_size=%d\n", __func__, (int)handle->data_size);

	if (NVSHM_MAJOR(conf->version) < 2) {
		handle->stats_base_virt = 0;
		handle->stats_size = 0;
	} else {
		handle->stats_base_virt = handle->mb_base_virt
			+ conf->region_dxp1_stats_offset;
		pr_debug("%s stats_base_virt=0x%p\n",
			 __func__, handle->stats_base_virt);

		handle->stats_size = conf->region_dxp1_stats_size;
		pr_debug("%s stats_size=%lu\n", __func__, handle->stats_size);
	}

#ifndef CONFIG_TEGRA_BASEBAND_SIMU
	handle->shared_queue_head =
		(struct nvshm_iobuf *)(handle->ipc_base_virt
				     + conf->queue_bb_offset);
	pr_debug("%s shared_queue_head offset=0x%lx\n",
		 __func__,
		 (long)handle->shared_queue_head - (long)handle->ipc_base_virt);
#else
	handle->shared_queue_head =
		(struct nvshm_iobuf *)(handle->ipc_base_virt
				      + conf->queue_ap_offset);
	pr_debug("%s shared_queue_head offset=0x%lx\n",
		 __func__,
		 (long)handle->shared_queue_head - (long)handle->ipc_base_virt);
#endif
	handle->shared_queue_tail =
		(struct nvshm_iobuf *)(handle->ipc_base_virt
				     + conf->queue_ap_offset);
	pr_debug("%s shared_queue_tail offset=0x%lx\n",
		 __func__, (long)handle->shared_queue_tail -
		 (long)handle->ipc_base_virt);

	for (chan = 0; chan < NVSHM_MAX_CHANNELS; chan++) {
		handle->chan[chan].index = chan;
		handle->chan[chan].map = conf->chan_map[chan];
		if (handle->chan[chan].map.type != NVSHM_CHAN_UNMAP) {
			pr_debug("%s chan[%d]=%s\n",
				 __func__, chan, handle->chan[chan].map.name);
		}
	}

	/* Serial number (e.g BBC PCID) */
	tegra_bb_set_ipc_serial(handle->tegra_bb, conf->serial);

	/* Invalidate cache for IPC region before use */
	INV_CPU_DCACHE(handle->ipc_base_virt, handle->ipc_size);
	handle->conf = conf;
	handle->configured = 1;
	return 0;
}

static int init_interfaces(struct nvshm_handle *handle)
{
	int nlog = 0, ntty = 0, nnet = 0, nrpc = 0;
	int chan;

	for (chan = 0; chan < NVSHM_MAX_CHANNELS; chan++) {
		handle->chan[chan].xoff = 0;
		switch (handle->chan[chan].map.type) {
		case NVSHM_CHAN_UNMAP:
			break;
		case NVSHM_CHAN_TTY:
		case NVSHM_CHAN_LOG:
			ntty++;
			handle->chan[chan].rate_counter = NVSHM_RATE_LIMIT_TTY;
			break;
		case NVSHM_CHAN_NET:
			handle->chan[chan].rate_counter = NVSHM_RATE_LIMIT_NET;
			nnet++;
			break;
		case NVSHM_CHAN_RPC:
			handle->chan[chan].rate_counter = NVSHM_RATE_LIMIT_RPC;
			nrpc++;
			break;
		default:
			break;
		}
	}

	if (ntty) {
		pr_debug("%s init %d tty channels\n", __func__, ntty);
		nvshm_tty_init(handle);
	}

	if (nlog)
		pr_debug("%s init %d log channels\n", __func__, nlog);

	if (nnet) {
		pr_debug("%s init %d net channels\n", __func__, nnet);
		nvshm_net_init(handle);
	}

	if (nrpc) {
		pr_debug("%s init %d rpc channels\n", __func__, nrpc);
		nvshm_rpc_init(handle);
		nvshm_rpc_dispatcher_init();
	}

	pr_debug("%s init statistics support\n", __func__);
	nvshm_stats_init(handle);

	return 0;
}

static int cleanup_interfaces(struct nvshm_handle *handle)
{
	int nlog = 0, ntty = 0, nnet = 0, nrpc = 0;
	int chan;

	/* No need to protect this as configuration will arrive after cleanup
	 * is propagated to userland
	 */
	handle->configured = 0;

	for (chan = 0; chan < NVSHM_MAX_CHANNELS; chan++) {
		switch (handle->chan[chan].map.type) {
		case NVSHM_CHAN_TTY:
		case NVSHM_CHAN_LOG:
			ntty++;
			break;
		case NVSHM_CHAN_NET:
			nnet++;
			break;
		case NVSHM_CHAN_RPC:
			nrpc++;
			break;
		default:
			break;
		}
	}

	if (ntty) {
		pr_debug("%s cleanup %d tty channels\n", __func__, ntty);
		nvshm_tty_cleanup();
	}

	if (nlog)
		pr_debug("%s cleanup %d log channels\n", __func__, nlog);

	if (nnet) {
		pr_debug("%s cleanup %d net channels\n", __func__, nnet);
		nvshm_net_cleanup();
	}

	if (nrpc) {
		pr_debug("%s cleanup %d rpc channels\n", __func__, nrpc);
		nvshm_rpc_dispatcher_cleanup();
		nvshm_rpc_cleanup();
	}

	pr_debug("%s cleanup statistics support\n", __func__);
	nvshm_stats_cleanup();

	/* Remove serial sysfs entry */
	tegra_bb_set_ipc_serial(handle->tegra_bb, NULL);

	return 0;
}

static void ipc_work(struct work_struct *work)
{
	struct nvshm_handle *handle = container_of(work,
						   struct nvshm_handle,
						   nvshm_work);
	int new_state;
	int cmd;

	if (!wake_lock_active(&handle->dl_lock))
		wake_lock(&handle->dl_lock);
	new_state = *((int *)handle->mb_base_virt);
	cmd = new_state & 0xFFFF;
	if (((~new_state >> 16) ^ (cmd)) & 0xFFFF) {
		pr_err("%s: IPC check failure msg=0x%x\n",
		       __func__, new_state);
		if (handle->configured) {
			nvshm_abort_queue(handle);
			cleanup_interfaces(handle);
		}
		goto ipc_exit;
	}
	switch (cmd) {
	case NVSHM_IPC_READY:
		/* most encountered message - process queue */
		if (cmd != handle->old_status) {
			if (ipc_readconfig(handle))
				goto ipc_exit;

			nvshm_iobuf_init(handle);
			nvshm_init_queue(handle);
			init_interfaces(handle);
		}
		/* Process IPC queue but do not notify sysfs */
		if (handle->configured) {
			nvshm_process_queue(handle);
			if (handle->errno) {
				pr_err("%s: cleanup interfaces\n",
				       __func__);
				nvshm_abort_queue(handle);
				cleanup_interfaces(handle);
				break;
			}
		}
		break;
	case NVSHM_IPC_BOOT_FW_REQ:
	case NVSHM_IPC_BOOT_RESTART_FW_REQ:
		if (handle->configured) {
			nvshm_abort_queue(handle);
			cleanup_interfaces(handle);
			pr_debug("%s: cleanup done\n", __func__);
		}
		break;
	case NVSHM_IPC_BOOT_ERROR_BT2_HDR:
	case NVSHM_IPC_BOOT_ERROR_BT2_SIGN:
	case NVSHM_IPC_BOOT_ERROR_HWID:
	case NVSHM_IPC_BOOT_ERROR_APP_HDR:
	case NVSHM_IPC_BOOT_ERROR_APP_SIGN:
	case NVSHM_IPC_BOOT_ERROR_UNLOCK_HEADER:
	case NVSHM_IPC_BOOT_ERROR_UNLOCK_SIGN:
	case NVSHM_IPC_BOOT_ERROR_UNLOCK_PCID:
		pr_err("%s BB startup failure: msg=0x%x\n",
		       __func__, new_state);
		break;
	case NVSHM_IPC_BOOT_COLD_BOOT_IND:
	case NVSHM_IPC_BOOT_FW_CONF:
		/* Should not have these - something went wrong... */
		pr_err("%s IPC IT error: msg=0x%x\n",
		       __func__, new_state);
		break;
	default:
		pr_err("%s unknown IPC message found: msg=0x%x\n",
		       __func__, new_state);
	}
	handle->old_status = cmd;
ipc_exit:
	wake_unlock(&handle->dl_lock);
	enable_irq(handle->bb_irq);
}

static void start_tx_worker(struct work_struct *work)
{
	struct nvshm_channel *chan = container_of(work,
						  struct nvshm_channel,
						  start_tx_work);

	pr_warn("%s: start tx on chan %d\n", __func__, chan->index);
	if (chan->ops)
		chan->ops->start_tx(chan);
}

static void nvshm_ipc_handler(void *data)
{
	struct nvshm_handle *handle = (struct nvshm_handle *)data;
	int ret;
	pr_debug("%s\n", __func__);
	ret = queue_work(handle->nvshm_wq, &handle->nvshm_work);
}

static enum hrtimer_restart nvshm_ipc_timer_func(struct hrtimer *timer)
{
	struct nvshm_handle *handle =
		container_of(timer, struct nvshm_handle, wake_timer);

	if (tegra_bb_check_ipc(handle->tegra_bb) == 1) {
		pr_debug("%s AP2BB is cleared\n", __func__);
		wake_unlock(&handle->ul_lock);
		return HRTIMER_NORESTART;
	}
	if (handle->timeout++ > NVSHM_WAKE_MAX_COUNT) {
		pr_warn("%s AP2BB not cleared in 1s - aborting\n", __func__);
		tegra_bb_abort_ipc(handle->tegra_bb);
		wake_unlock(&handle->ul_lock);
		return HRTIMER_NORESTART;
	}
	pr_debug("%s AP2BB is still set\n", __func__);
	hrtimer_forward_now(timer, ktime_set(0, NVSHM_WAKE_TIMEOUT_NS));
	return HRTIMER_RESTART;
}

int nvshm_register_ipc(struct nvshm_handle *handle)
{
	int chan;

	pr_debug("%s\n", __func__);
	snprintf(handle->wq_name, 15, "nvshm_queue%d", handle->instance);
	handle->nvshm_wq = create_singlethread_workqueue(handle->wq_name);
	INIT_WORK(&handle->nvshm_work, ipc_work);

	for (chan = 0; chan < NVSHM_MAX_CHANNELS; chan++)
		INIT_WORK(&handle->chan[chan].start_tx_work, start_tx_worker);

	hrtimer_init(&handle->wake_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
	handle->wake_timer.function = nvshm_ipc_timer_func;

	tegra_bb_register_ipc(handle->tegra_bb, nvshm_ipc_handler, handle);
	return 0;
}

int nvshm_unregister_ipc(struct nvshm_handle *handle)
{
	pr_debug("%s flush workqueue\n", __func__);
	flush_workqueue(handle->nvshm_wq);

	pr_debug("%s destroy workqueue\n", __func__);
	destroy_workqueue(handle->nvshm_wq);

	pr_debug("%s unregister tegra_bb\n", __func__);
	tegra_bb_register_ipc(handle->tegra_bb, NULL, NULL);

	hrtimer_cancel(&handle->wake_timer);
	return 0;
}

int nvshm_generate_ipc(struct nvshm_handle *handle)
{
	/* take wake lock until BB ack our irq */
	if (!wake_lock_active(&handle->ul_lock))
		wake_lock(&handle->ul_lock);

	if (!hrtimer_active(&handle->wake_timer)) {
		handle->timeout = 0;
		hrtimer_start(&handle->wake_timer,
			      ktime_set(0, NVSHM_WAKE_TIMEOUT_NS),
			      HRTIMER_MODE_REL);
	}
	/* generate ipc */
	tegra_bb_generate_ipc(handle->tegra_bb);
	return 0;
}

