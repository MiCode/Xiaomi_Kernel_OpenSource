/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_A6XX_HFI_H
#define __ADRENO_A6XX_HFI_H

#include "adreno_hfi.h"

/**
 * struct a6xx_hfi - HFI control structure
 * @seqnum: atomic counter that is incremented for each message sent. The
 *	value of the counter is used as sequence number for HFI message
 * @bw_table: HFI BW table buffer
 * @acd_table: HFI table for ACD data
 */
struct a6xx_hfi {
	/** @irq: HFI interrupt line */
	int irq;
	atomic_t seqnum;
	/** @hfi_mem: Memory descriptor for the hfi memory */
	struct kgsl_memdesc *hfi_mem;
	struct hfi_bwtable_cmd bw_table;
	struct hfi_acd_table_cmd acd_table;
	/** @dcvs_table: HFI table for gpu dcvs levels */
	struct hfi_dcvstable_cmd dcvs_table;
};

struct a6xx_gmu_device;

/* a6xx_hfi_irq_handler - IRQ handler for HFI interripts */
irqreturn_t a6xx_hfi_irq_handler(int irq, void *data);

/**
 * a6xx_hfi_start - Send the various HFIs during device boot up
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_start(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_start - Send the various HFIs during device boot up
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
void a6xx_hfi_stop(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_init - Initialize hfi resources
 * @adreno_dev: Pointer to the adreno device
 *
 * This function allocates and sets up hfi queues
 * when a process creates the very first kgsl instance
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_init(struct adreno_device *adreno_dev);

/* Helper function to get to a6xx hfi struct from adreno device */
struct a6xx_hfi *to_a6xx_hfi(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_queue_write - Write a command to hfi queue
 * @adreno_dev: Pointer to the adreno device
 * @queue_idx: destination queue id
 * @msg: Data to be written to the queue
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_queue_write(struct adreno_device *adreno_dev, u32 queue_idx,
	u32 *msg);

/**
 * a6xx_hfi_queue_read - Read data from hfi queue
 * @gmu: Pointer to the a6xx gmu device
 * @queue_idx: queue id to read from
 * @output: Pointer to read the data into
 * @max_size: Number of bytes to read from the queue
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_queue_read(struct a6xx_gmu_device *gmu, u32 queue_idx,
	u32 *output, u32 max_size);

/**
 * a6xx_receive_ack_cmd - Process ack type packets
 * @gmu: Pointer to the a6xx gmu device
 * @rcvd: Pointer to the data read from hfi queue
 * @ret_cmd: Container for the hfi packet for which this ack is received
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_receive_ack_cmd(struct a6xx_gmu_device *gmu, void *rcvd,
	struct pending_cmd *ret_cmd);

/**
 * a6xx_hfi_send_feature_ctrl - Enable gmu feature via hfi
 * @adreno_dev: Pointer to the adreno device
 * @feature: feature to be enabled or disabled
 * enable: Set 1 to enable or 0 to disable a feature
 * @data: payload for the send feature hfi packet
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_feature_ctrl(struct adreno_device *adreno_dev,
	u32 feature, u32 enable, u32 data);

/**
 * a6xx_hfi_send_set_value - Send gmu set_values via hfi
 * @adreno_dev: Pointer to the adreno device
 * @type: GMU set_value type
 * @subtype: GMU set_value subtype
 * @data: Value to set
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_set_value(struct adreno_device *adreno_dev,
		u32 type, u32 subtype, u32 data);

/**
 * a6xx_hfi_send_core_fw_start - Send the core fw start hfi
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_core_fw_start(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_send_acd_feature_ctrl - Send the acd table and acd feature
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_acd_feature_ctrl(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_send_lm_feature_ctrl -  Send the lm feature hfi packet
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_lm_feature_ctrl(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_send_generic_req -  Send a generic hfi packet
 * @adreno_dev: Pointer to the adreno device
 * @cmd: Pointer to the hfi packet header and data
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_generic_req(struct adreno_device *adreno_dev, void *cmd);

/**
 * a6xx_hfi_send_bcl_feature_ctrl -  Send the bcl feature hfi packet
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_bcl_feature_ctrl(struct adreno_device *adreno_dev);

/*
 * a6xx_hfi_process_queue - Check hfi queue for messages from gmu
 * @gmu: Pointer to the a6xx gmu device
 * @queue_idx: queue id to be processed
 * @ret_cmd: Container for data needed for waiting for the ack
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_process_queue(struct a6xx_gmu_device *gmu,
	u32 queue_idx, struct pending_cmd *ret_cmd);

/**
 * a6xx_hfi_cmdq_write - Write a command to command queue
 * @adreno_dev: Pointer to the adreno device
 * @msg: Data to be written to the queue
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_cmdq_write(struct adreno_device *adreno_dev, u32 *msg);
void adreno_a6xx_receive_err_req(struct a6xx_gmu_device *gmu, void *rcvd);
void adreno_a6xx_receive_debug_req(struct a6xx_gmu_device *gmu, void *rcvd);
#endif
