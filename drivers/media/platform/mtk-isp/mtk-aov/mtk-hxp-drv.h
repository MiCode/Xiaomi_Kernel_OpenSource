/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HXP_DRV_H
#define MTK_HXP_DRV_H

#include <linux/cdev.h>
#include <linux/platform_device.h>

#include "mtk-hxp-config.h"
#include "mtk-hxp-core.h"
#include "mtk-hxp-aee.h"
#include "mtk-hxp-ctrl.h"

/**
 * struct mtk_hxp - hcp driver data
 * @hcp_desc:            hcp descriptor
 * @dev:                 hcp struct device
 * @mem_ops:             memory operations
 * @hcp_mutex:           protect mtk_hxp (except recv_buf) and ensure only
 *                       one client to use hcp service at a time.
 * @file:                hcp daemon file pointer
 * @is_open:             the flag to indicate if hcp device is open.
 * @ack_wq:              the wait queue for each client. When sleeping
 *                       processes wake up, they will check the condition
 *                       "hcp_id_ack" to run the corresponding action or
 *                       go back to sleep.
 * @hcp_id_ack:          The ACKs for registered HCP function.
 * @ipi_got:             The flags for IPI message polling from user.
 * @ipi_done:            The flags for IPI message polling from user again,
 *       which means the previous messages has been dispatched
 *                       done in daemon.
 * @user_obj:            Temporary share_buf used for hcp_msg_get.
 * @hcp_devno:           The hcp_devno for hcp init hcp character device
 * @hcp_cdev:            The point of hcp character device.
 * @hcp_class:           The class_create for create hcp device
 * @hcp_device:          hcp struct device
 * @hcpname:             hcp struct device name in dtsi
 * @ cm4_support_list    to indicate which module can run in cm4 or it will send
 *                       to user space for running action.
 * @ current_task        hcp current task struct
 */
struct mtk_hxp {
	struct hxp_core core_info;
	struct hxp_ctrl ctrl_info;
	struct hxp_aee aee_info;

	dev_t hcp_devno;
	struct device *dev;
	struct cdev hcp_cdev;
	struct class *hcp_class;
	struct device *hcp_device;
	bool is_open;
	uint32_t op_mode;
};

#endif /* MTK_HXP_DRV_H */
