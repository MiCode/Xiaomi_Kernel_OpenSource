/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __RPOC_MTK_CCU_H
#define __RPOC_MTK_CCU_H

#include <linux/remoteproc.h>
#include <linux/platform_device.h>

typedef void (*mtk_ccu_ipc_handle_t) (uint32_t data, uint32_t len, void *priv);

enum CCU_SMC_REQ {
	CCU_SMC_REQ_NONE = 0,
	CCU_SMC_REQ_INIT,
	CCU_SMC_REQ_LOAD,
	CCU_SMC_REQ_RUN,
	CCU_SMC_REQ_CLEAR_INT,
	CCU_SMC_REQ_STOP,
	CCU_SMC_REQ_MAX,
};

/**
 * enum mtk_ccu_to_ap_msg_id - the id of inter-processor interrupt from CCU
 *
 * @MTK_CCU_MSG_TO_APMCU_FLUSH_LOG:   CCU Request APMCU to print out CCU logs
 * @MTK_CCU_MSG_TO_APMCU_CCU_ASSERT:  CCU inform APMCU that CCU ASSERT occurs
 * @MTK_CCU_MSG_TO_APMCU_CCU_WARNING: CCU inform APMCU that CCU WARNING occurs
 * @MTK_CCU_MSG_TO_APMCU_MAX:    The maximum ipc number
 */
enum mtk_ccu_to_ap_msg_id {
	MTK_CCU_MSG_TO_APMCU_AAO_TEST_DONE,
	MTK_CCU_MSG_TO_APMCU_FLUSH_LOG,
	MTK_CCU_MSG_TO_APMCU_CCU_ASSERT,
	MTK_CCU_MSG_TO_APMCU_CCU_WARNING,
	MTK_CCU_MSG_TO_APMCU_DVFS_STATUS,
	MTK_CCU_MSG_TO_APMCU_MAX,
};

/**
 * enum mtk_ccu_feature_type - the id of feature in CCU
 *
 * @MTK_CCU_FEATURE_MAX:    The maximum feature number
 */
enum mtk_ccu_feature_type {
	MTK_CCU_FEATURE_UNDEF = 0x0,
	MTK_CCU_FEATURE_MIN = 0x1,
	MTK_CCU_FEATURE_SYSCTRL = MTK_CCU_FEATURE_MIN,
	MTK_CCU_FEATURE_3ACTRL,
	MTK_CCU_FEATURE_AF,
	MTK_CCU_FEATURE_AE,
	MTK_CCU_FEATURE_FMCTRL,
	MTK_CCU_FEATURE_CAMSYS,
	MTK_CCU_FEATURE_ISPDVFS,
	MTK_CCU_FEATURE_MAX,
};

/**
 * mtk_ccu_get_pdev - get CCU's platform device
 *
 * @pdev:  the platform device of the module requesting CCU platform
 *     device for using CCU API.
 *
 * Return: Return NULL if it is failed, otherwise it is CCU's platform device.
 **/
struct platform_device *mtk_ccu_get_pdev(struct platform_device *pdev);

/**
 * mtk_ccu_rproc_ipc_send - send data from AP to ccu.
 *
 * @pdev:  CCU platform device
 * @feature:  feature id in CCU to receive this data
 * @msg_id:  ipc ID
 * @buf:  the data buffer
 * @len:  the data buffer length
 *
 * This function is thread-safe. When this function returns,
 * CCU has received the data and finsh the processing.
 *
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int mtk_ccu_rproc_ipc_send(struct platform_device *pdev,
	enum mtk_ccu_feature_type feature,
	uint32_t msg_id, void *buf, uint32_t len);

/**
 * mtk_ccu_ipc_register - register an ipc function from CCU
 *
 * @pdev:  CCU platform device
 * @msg_id:  ipc ID
 * @handler:  ipc handler
 * @priv:  private data for ipc handler
 *
 * Register an ipc function to receive ipc interrupt from CCU.
 *
 * Return: Return 0 if ipc registers successfully, otherwise it is failed.
 */
int mtk_ccu_ipc_register(struct platform_device *pdev,
	enum mtk_ccu_to_ap_msg_id msg_id,
	mtk_ccu_ipc_handle_t handle, void *priv);

/**
 * mtk_ccu_ipc_unregister - unregister an ipc function from CCU
 *
 * @pdev:  CCU platform device
 * @msg_id:  ipc ID
 *
 * Unregister an ipc function to receive ipc interrupt from CCU.
 */
void mtk_ccu_ipc_unregister(struct platform_device *pdev,
	enum mtk_ccu_to_ap_msg_id msg_id);

/**
 * mtk_ccu_da_to_va - perform address translations
 *
 * @rproc:  CCU remote processor
 * @da:  device address of buffer in CCU domain
 * @len:  the buffer length
 *
 * Return: Return NULL if it is failed, otherwise it is va in kernel.
 */
void *mtk_ccu_da_to_va(struct rproc *rproc, u64 da, size_t lens, bool *is_iomem);

/**
 * mtk_ccu_rproc_get_inforeg - Query spare register value
 *
 * @pdev:  CCU platform device
 * @regno: register number
 * @data:  register value
 *
 * Return: Return 0 if succed, otherwise it has been failed.
 */
int mtk_ccu_rproc_get_inforeg(struct platform_device *pdev,
	uint32_t regno, uint32_t *data);
#endif /*__RPOC_MTK_CCU_H */
