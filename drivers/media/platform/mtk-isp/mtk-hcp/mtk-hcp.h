/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Andrew-CT Chen <andrew-ct.chen@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_HCP_H
#define MTK_HCP_H

#include <linux/fdtable.h>
#include <linux/platform_device.h>
#include "scp_ipi.h"

/**
 * HCP (Hetero Control Processor ) is a tiny processor controlling
 * the methodology of register programming. If the module support
 * to run on CM4 then it will send data to CM4 to program register.
 * Or it will send the data to user library and let RED to program
 * register.
 *
 **/

typedef void (*hcp_handler_t) (void *data,
			       unsigned int len,
			       void *priv);


/**
 * hcp ID definition
 */
enum hcp_id {
	HCP_INIT_ID = 0,
	HCP_ISP_CMD_ID,
	HCP_ISP_FRAME_ID,
	HCP_DIP_INIT_ID,
	HCP_DIP_FRAME_ID,
	HCP_DIP_HW_TIMEOUT_ID,
	HCP_FD_CMD_ID,
	HCP_FD_FRAME_ID,
	HCP_RSC_INIT_ID,
	HCP_RSC_FRAME_ID,
	HCP_MAX_ID,
};

/**
 * module ID definition
 */
enum module_id {
	MODULE_ISP = 0,
	MODULE_DIP,
	MODULE_FD,
	MODULE_RSC,
	MODULE_MAX_ID,
};

/**
 * mtk_hcp_register - Register an hcp function
 *
 * @pdev:	HCP platform device
 * @id:	HCP ID
 * @handler: HCP handler
 * @name:	HCP name
 * @priv:	private data for HCP handler
 *
 * Return: Return 0 if hcp registers successfully, otherwise it is failed.
 */
int mtk_hcp_register(struct platform_device *pdev, enum hcp_id id,
		     hcp_handler_t handler, const char *name, void *priv);

/**
 * mtk_hcp_unregister - Unregister an hcp function
 *
 * @pdev:	HCP platform device
 * @id:	HCP ID
 *
 * Return: Return 0 if hcp unregisters successfully, otherwise it is failed.
 */
int mtk_hcp_unregister(struct platform_device *pdev, enum hcp_id id);

/**
 * mtk_hcp_send - Send data from camera kernel driver to HCP and wait the
 *                               command send to demand.
 *
 * @pdev:	HCP platform device
 * @id:	HCP ID
 * @buf:	the data buffer
 * @len:	the data buffer length
 *
 * This function is thread-safe. When this function returns,
 * HCP has received the data and save the data in the workqueue.
 * After that it will schedule work to dequeue to send data to CM4 or
 * RED for programming register.
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int mtk_hcp_send(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len);

/**
 * mtk_hcp_send_async - Send data from camera kernel driver to HCP without
 *                                            waiting demand receives the data.
 *
 * @pdev:	HCP platform device
 * @id:	HCP ID
 * @buf:	the data buffer
 * @len:	the data buffer length
 *
 * This function is thread-safe. When this function returns,
 * HCP has received the data and save the data in the workqueue.
 * After that it will schedule work to dequeue to send data to CM4 or
 * RED for programming register.
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int mtk_hcp_send_async(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len);

/**
 * mtk_hcp_get_plat_device - Get HCP's platform device
 *
 * @pdev:	the platform device of the module requesting HCP platform
 *		device for using HCP API.
 *
 * Return: Return NULL if it is failed.
 * otherwise it is HCP's platform device
 **/
struct platform_device *mtk_hcp_get_plat_device(struct platform_device *pdev);

#define HCP_RESERVED_MEM	(1)

#if HCP_RESERVED_MEM
/* hcp reserve memory ID definition*/
enum mtk_hcp_reserve_mem_id_t {
	ISP_MEM_ID,
	DIP_MEM_FOR_HW_ID,
	DIP_MEM_FOR_SW_ID,
	MDP_MEM_ID,
	FD_MEM_ID,
	NUMS_MEM_ID,
};

struct mtk_hcp_reserve_mblock {
	enum mtk_hcp_reserve_mem_id_t num;
	phys_addr_t start_phys;
	void *start_virt;
	phys_addr_t start_dma;
	phys_addr_t size;
	uint8_t is_dma_buf;
};

extern phys_addr_t mtk_hcp_get_reserve_mem_phys(
	enum mtk_hcp_reserve_mem_id_t id);
extern phys_addr_t mtk_hcp_get_reserve_mem_virt(
	enum mtk_hcp_reserve_mem_id_t id);
extern phys_addr_t mtk_hcp_get_reserve_mem_dma(
	enum mtk_hcp_reserve_mem_id_t id);
extern phys_addr_t mtk_hcp_get_reserve_mem_size(
	enum mtk_hcp_reserve_mem_id_t id);
#endif


#endif /* _MTK_HCP_H */
