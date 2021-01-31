/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_GENC_HFI_H
#define __ADRENO_GENC_HFI_H

#include "adreno_hfi.h"

/**
 * struct genc_hfi - HFI control structure
 */
struct genc_hfi {
	/** @irq: HFI interrupt line */
	int irq;
	/** @seqnum: atomic counter that is incremented for each message sent.
	 *   The value of the counter is used as sequence number for HFI message.
	 */
	atomic_t seqnum;
	/** @hfi_mem: Memory descriptor for the hfi memory */
	struct gmu_memdesc *hfi_mem;
	/** @bw_table: HFI BW table buffer */
	struct hfi_bwtable_cmd bw_table;
	/** @acd_table: HFI table for ACD data */
	struct hfi_acd_table_cmd acd_table;
	/** @dcvs_table: HFI table for gpu dcvs levels */
	struct hfi_dcvstable_cmd dcvs_table;
};

struct genc_gmu_device;

/* genc_hfi_irq_handler - IRQ handler for HFI interripts */
irqreturn_t genc_hfi_irq_handler(int irq, void *data);

/**
 * genc_hfi_start - Send the various HFIs during device boot up
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_start(struct adreno_device *adreno_dev);

/**
 * genc_hfi_start - Send the various HFIs during device boot up
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
void genc_hfi_stop(struct adreno_device *adreno_dev);

/**
 * genc_hfi_init - Initialize hfi resources
 * @adreno_dev: Pointer to the adreno device
 *
 * This function allocates and sets up hfi queues
 * when a process creates the very first kgsl instance
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_init(struct adreno_device *adreno_dev);

/* Helper function to get to genc hfi struct from adreno device */
struct genc_hfi *to_genc_hfi(struct adreno_device *adreno_dev);

/**
 * genc_hfi_queue_write - Write a command to hfi queue
 * @adreno_dev: Pointer to the adreno device
 * @queue_idx: destination queue id
 * @msg: Data to be written to the queue
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_queue_write(struct adreno_device *adreno_dev, u32 queue_idx,
		u32 *msg);

/**
 * genc_hfi_queue_read - Read data from hfi queue
 * @gmu: Pointer to the genc gmu device
 * @queue_idx: queue id to read from
 * @output: Pointer to read the data into
 * @max_size: Number of bytes to read from the queue
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_queue_read(struct genc_gmu_device *gmu, u32 queue_idx,
		u32 *output, u32 max_size);

/**
 * genc_receive_ack_cmd - Process ack type packets
 * @gmu: Pointer to the genc gmu device
 * @rcvd: Pointer to the data read from hfi queue
 * @ret_cmd: Container for the hfi packet for which this ack is received
 *
 * Return: 0 on success or negative error on failure
 */
int genc_receive_ack_cmd(struct genc_gmu_device *gmu, void *rcvd,
		struct pending_cmd *ret_cmd);

/**
 * genc_hfi_send_feature_ctrl - Enable gmu feature via hfi
 * @adreno_dev: Pointer to the adreno device
 * @feature: feature to be enabled or disabled
 * enable: Set 1 to enable or 0 to disable a feature
 * @data: payload for the send feature hfi packet
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_send_feature_ctrl(struct adreno_device *adreno_dev,
		u32 feature, u32 enable, u32 data);

/**
 * genc_hfi_send_core_fw_start - Send the core fw start hfi
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_send_core_fw_start(struct adreno_device *adreno_dev);

/**
 * genc_hfi_send_acd_feature_ctrl - Send the acd table and acd feature
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_send_acd_feature_ctrl(struct adreno_device *adreno_dev);

/**
 * genc_hfi_send_generic_req - Send a generic hfi packet
 * @adreno_dev: Pointer to the adreno device
 * @cmd: Pointer to the hfi packet header and data
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_send_generic_req(struct adreno_device *adreno_dev, void *cmd);

/**
 * genc_hfi_send_bcl_feature_ctrl - Send the bcl feature hfi packet
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_send_bcl_feature_ctrl(struct adreno_device *adreno_dev);

/*
 * genc_hfi_process_queue - Check hfi queue for messages from gmu
 * @gmu: Pointer to the genc gmu device
 * @queue_idx: queue id to be processed
 * @ret_cmd: Container for data needed for waiting for the ack
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_process_queue(struct genc_gmu_device *gmu,
		u32 queue_idx, struct pending_cmd *ret_cmd);

/**
 * genc_hfi_cmdq_write - Write a command to command queue
 * @adreno_dev: Pointer to the adreno device
 * @msg: Data to be written to the queue
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hfi_cmdq_write(struct adreno_device *adreno_dev, u32 *msg);
void adreno_genc_receive_err_req(struct genc_gmu_device *gmu, void *rcvd);
void adreno_genc_receive_debug_req(struct genc_gmu_device *gmu, void *rcvd);
#endif
