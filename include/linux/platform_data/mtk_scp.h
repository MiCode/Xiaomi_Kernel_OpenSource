/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef _MTK_SCP_H
#define _MTK_SCP_H

#include <linux/platform_device.h>

typedef void (*scp_ipi_handler_t) (void *data,
				   unsigned int len,
				   void *priv);

/**
 * enum ipi_id - the id of inter-processor interrupt
 *
 * @SCP_IPI_INIT:	 The interrupt from scp is to notfiy kernel
 *			 SCP initialization completed.
 *			 IPI_SCP_INIT is sent from SCP when firmware is
 *			 loaded. AP doesn't need to send IPI_SCP_INIT
 *			 command to SCP.
 *			 For other IPI below, AP should send the request
 *			 to SCP to trigger the interrupt.
 * @SCP_IPI_MAX:	 The maximum IPI number
 */

enum scp_ipi_id {
	SCP_IPI_INIT = 0,
	SCP_IPI_VDEC_H264,
	SCP_IPI_VDEC_VP8,
	SCP_IPI_VDEC_VP9,
	SCP_IPI_VENC_H264,
	SCP_IPI_VENC_VP8,
	SCP_IPI_MDP,
	SCP_IPI_CROS_HOST_CMD,
	SCP_IPI_NS_SERVICE = 0xFF,
	SCP_IPI_MAX = 0x100,
};


/**
 * scp_ipi_register - register an ipi function
 *
 * @pdev:	SCP platform device
 * @id:		IPI ID
 * @handler:	IPI handler
 * @priv:	private data for IPI handler
 *
 * Register an ipi function to receive ipi interrupt from SCP.
 *
 * Return: Return 0 if ipi registers successfully, otherwise it is failed.
 */
int scp_ipi_register(struct platform_device *pdev,
		     enum scp_ipi_id id,
		     scp_ipi_handler_t handler,
		     void *priv);

/**
 * scp_ipi_unregister - unregister an ipi function
 *
 * @pdev:	SCP platform device
 * @id:		IPI ID
 *
 * Unregister an ipi function to receive ipi interrupt from SCP.
 */
void scp_ipi_unregister(struct platform_device *pdev, enum scp_ipi_id id);

/**
 * scp_ipi_send - send data from AP to scp.
 *
 * @pdev:	SCP platform device
 * @id:		IPI ID
 * @buf:	the data buffer
 * @len:	the data buffer length
 * @wait:	1: need ack
 *
 * This function is thread-safe. When this function returns,
 * SCP has received the data and starts the processing.
 * When the processing completes, IPI handler registered
 * by scp_ipi_register will be called in interrupt context.
 *
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int scp_ipi_send(struct platform_device *pdev,
		 enum scp_ipi_id id,
		 void *buf,
		 unsigned int len,
		 unsigned int wait);

/**
 * scp_get_pdev - get SCP's platform device
 *
 * @pdev:	the platform device of the module requesting SCP platform
 *		device for using SCP API.
 *
 * Return: Return NULL if it is failed.
 * otherwise it is SCP's platform device
 **/
struct platform_device *scp_get_pdev(struct platform_device *pdev);

/**
 * scp_get_vdec_hw_capa - get video decoder hardware capability
 *
 * @pdev:	SCP platform device
 *
 * Return: video decoder hardware capability
 **/
unsigned int scp_get_vdec_hw_capa(struct platform_device *pdev);

/**
 * scp_get_venc_hw_capa - get video encoder hardware capability
 *
 * @pdev:	SCP platform device
 *
 * Return: video encoder hardware capability
 **/
unsigned int scp_get_venc_hw_capa(struct platform_device *pdev);

/**
 * scp_mapping_dm_addr - Mapping SRAM/DRAM to kernel virtual address
 *
 * @pdev:	SCP platform device
 * @mem_addr:	SCP views memory address
 *
 * Mapping the SCP's SRAM address /
 * DMEM (Data Extended Memory) memory address /
 * Working buffer memory address to
 * kernel virtual address.
 *
 * Return: Return ERR_PTR(-EINVAL) if mapping failed,
 * otherwise the mapped kernel virtual address
 **/
void *scp_mapping_dm_addr(struct platform_device *pdev,
			  u32 mem_addr);

#define SCP_RESERVED_MEM	(1)
#if SCP_RESERVED_MEM
/* scp reserve memory ID definition*/
enum scp_reserve_mem_id_t {
	SCP_ISP_MEM_ID,
	SCP_NUMS_MEM_ID,
};

struct scp_reserve_mblock {
	enum scp_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

extern phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id);
#endif

#endif /* _MTK_SCP_H */
