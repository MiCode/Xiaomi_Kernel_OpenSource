/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2020-2021 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/*
 * Arbiter adapter between frontend and DDK
 */

#ifndef _MALI_ARBIF_ADAPTER_H_
#define _MALI_ARBIF_ADAPTER_H_

#define ARBITER_IF_DT_NAME "arm,mali_arbif"

/**
 * struct arbif_adapter_comms_fe_ad_ops - Interface from front-end to adapter.
 * This struct is used to contain callbacks used to receive command messages
 * from the front-end to the adapter.
 */
struct arbif_adapter_comms_fe_ad_ops {
	/**
	 * @arb_vm_gpu_stop: Arbiter VM GPU stop callback
	 * @ad_priv: Adapter private data
	 *
	 * Called when GPU_STOP message is received from the Arbiter.
	 */
	void (*arb_vm_gpu_stop)(void *ad_priv);

	/**
	 * @arb_vm_gpu_granted: Arbiter VM GPU granted callback
	 * @ad_priv: Adapter private data
	 * @freq: Frequency reported from Arbiter
	 *
	 * Called when GPU_GRANTED message is received from Arbiter.
	 */
	void (*arb_vm_gpu_granted)(void *ad_priv, uint32_t freq);

	/**
	 * @arb_vm_gpu_lost: Arbiter VM GPU lost callback
	 * @ad_priv: Adapter private data
	 *
	 * Called when GPU_LOST message is received by the front-end.
	 */
	void (*arb_vm_gpu_lost)(void *ad_priv);
};

/**
 * struct arbif_adapter_comms_ad_fe_ops - Interface from adapter to front-end.
 * This struct is used to contain callbacks used to send command messages
 * from the adapter to the front-end.
 */
struct arbif_adapter_comms_ad_fe_ops {
	/**
	 * @adapter_register: Registers adapter with front-end.
	 * @fe_priv: Frontend private data.
	 * @ad_priv: Adapter private data (sent to all adapter callbacks).
	 * @fe_ad_ops: Callbacks to register (front-end to adapter).
	 */
	void (*adapter_register)(void *fe_priv, void *ad_priv,
		struct arbif_adapter_comms_fe_ad_ops *fe_ad_ops);

	/**
	 * @adapter_unregister: Unregisters adapter from front-end.
	 * @fe_priv: Frontend private data.
	 */
	void (*adapter_unregister)(void *fe_priv);

	/**
	 * @vm_arb_gpu_request: Sends a GPU_REQUEST message to the Arbiter.
	 * @fe_priv: Frontend private data.
	 */
	void (*vm_arb_gpu_request)(void *fe_priv);

	/**
	 * @vm_arb_gpu_active: Sends a GPU_ACTIVE message to the Arbiter.
	 * @fe_priv: Frontend private data.
	 */
	void (*vm_arb_gpu_active)(void *fe_priv);

	/**
	 * @vm_arb_gpu_idle: Sends a GPU_IDLE message to the Arbiter.
	 * @fe_priv: Frontend private data.
	 */
	void (*vm_arb_gpu_idle)(void *fe_priv);

	/**
	 * @vm_arb_gpu_stopped: Sends a GPU_STOPPED message to the Arbiter.
	 * @fe_priv: Frontend private data.
	 * @gpu_required: The GPU is still needed to do more work.
	 */
	void (*vm_arb_gpu_stopped)(void *fe_priv, u8 gpu_required);
};

/**
 * struct arbif_comms_dev - front-end comms device.
 *
 *  @fe_priv: Front-end private data (sent with all ad to fe callbacks).
 * @ad_fe_ops: Callbacks from adapter to frontend.
 * @ad_priv: Adapter private data (sent with all fe to ad callbacks).
 * @fe_ad_ops: Callbacks from frontend to adapter.
 *
 * Provided by the front-end when registering the adapter driver.
 * This allows the adapter driver instances to register themselves
 * with the frontend.
 *
 */
struct arbif_comms_dev {
	/* Populated by the Frontend */
	void *fe_priv;
	struct arbif_adapter_comms_ad_fe_ops ad_fe_ops;

	/* Populated by the adapter */
	void *ad_priv;
	struct arbif_adapter_comms_fe_ad_ops fe_ad_ops;
};

/**
 * arbif_adapter_init_comms() - Initialize the comms channel.
 * @pdev: Adapter Platform Device to init comms on.
 * @comms_dev: Comms device of connecting module (front-end).
 *
 * Return: 0 if success, or a Linux error code
 */
int arbif_adapter_init_comms(struct platform_device *pdev,
			struct arbif_comms_dev *comms_dev);

/**
 * arbif_adapter_deinit_comms() - Deinitialize the comms channel.
 * @pdev: Adapter Platform Device to deinit comms on.
 * @comms_dev: Comms device of disconnecting module (front-end).
 *
 * Return: 0 if success, or a Linux error code
 */
int arbif_adapter_deinit_comms(struct platform_device *pdev,
			struct arbif_comms_dev *comms_dev);

/**
 * arbif_adapter_register() - Register the arbif_adapter driver
 *
 * Return: 0 if success, or a Linux error code
 */
int arbif_adapter_register(void);

/**
 * arbif_adapter_unregister() - Unregister the arbif_adapter driver
 */
void arbif_adapter_unregister(void);

#endif /* _MALI_ARBIF_ADAPTER_H_ */
