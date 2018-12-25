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

#ifndef _MTK_VCU_H
#define _MTK_VCU_H

#include <linux/fdtable.h>
#include <linux/platform_device.h>

/**
 * VCU (Video Communication/Controller Unit) is a tiny processor controlling video hardware
 * related to video codec, scaling and color format converting.
 * VCU interfaces with other blocks by share memory and interrupt.
 **/

typedef int (*ipi_handler_t) (void *data,
			       unsigned int len,
			       void *priv);

/**
 * enum ipi_id - the id of inter-processor interrupt
 *
 * @IPI_VCU_INIT:	The interrupt from vcu is to notfiy kernel
 *			VCU initialization completed.
 *			IPI_VCU_INIT is sent from VCU when firmware is
 *			loaded. AP doesn't need to send IPI_VCU_INIT
 *			command to VCU.
 *			For other IPI below, AP should send the request
 *			to VCU to trigger the interrupt.
 * @IPI_VDEC_H264:	The interrupt from vcu is to notify kernel to
 *			handle H264 vidoe decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_H265:	The interrupt from vcu is to notify kernel to
 *			handle H265 vidoe decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_VP8:	The interrupt from is to notify kernel to
 *			handle VP8 video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_VP9:	The interrupt from vcu is to notify kernel to
 *			handle VP9 video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_MPEG4:	The interrupt from vcu is to notify kernel to
 *			handle MPEG4 video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_DIVX3:	The interrupt from vcu is to notify kernel to
 *			handle DIVX3 video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_MPEG12:	The interrupt from vcu is to notify kernel to
 *			handle MPEG1/2 video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_WMV:	The interrupt from vcu is to notify kernel to
 *			handle WMV video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_RV30:	The interrupt from vcu is to notify kernel to
 *			handle RV30 video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VDEC_RV40:	The interrupt from vcu is to notify kernel to
 *			handle RV40 video decoder job, and vice versa.
 *			Decode output format is always MT21 no matter what
 *			the input format is.
 * @IPI_VENC_H264:	The interrupt from vcu is to notify kernel to
 *			handle H264 video encoder job, and vice versa.
 * @IPI_VENC_VP8:	The interrupt fro vcu is to notify kernel to
 *			handle VP8 video encoder job,, and vice versa.
 * @IPI_MDP:		The interrupt from vcu is to notify kernel to
 *			handle MDP (Media Data Path) job, and vice versa.
 * @IPI_MAX:		The maximum IPI number
 */

enum ipi_id {
	IPI_VCU_INIT = 0,
	IPI_VDEC_H264,
	IPI_VDEC_H265,
	IPI_VDEC_VP8,
	IPI_VDEC_VP9,
	IPI_VDEC_MPEG4,
	IPI_VDEC_DIVX3,
	IPI_VDEC_MPEG12,
	IPI_VDEC_WMV,
	IPI_VDEC_RV30,
	IPI_VDEC_RV40,
	IPI_VENC_H264,
	IPI_VENC_VP8,
	IPI_MDP,
	IPI_MDP_1,
	IPI_MDP_2,
	IPI_MDP_3,
	IPI_MAX,
};

/**
 * vcu_ipi_register - register an ipi function
 *
 * @pdev:	VCU platform device
 * @id:		IPI ID
 * @handler:	IPI handler
 * @name:	IPI name
 * @priv:	private data for IPI handler
 *
 * Register an ipi function to receive ipi interrupt from VCU.
 *
 * Return: Return 0 if ipi registers successfully, otherwise it is failed.
 */
int vcu_ipi_register(struct platform_device *pdev, enum ipi_id id,
		     ipi_handler_t handler, const char *name, void *priv);

/**
 * vcu_ipi_send - send data from AP to vcu.
 *
 * @pdev:	VCU platform device
 * @id:		IPI ID
 * @buf:	the data buffer
 * @len:	the data buffer length
 *
 * This function is thread-safe. When this function returns,
 * VCU has received the data and starts the processing.
 * When the processing completes, IPI handler registered
 * by vcu_ipi_register will be called in interrupt context.
 *
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int vcu_ipi_send(struct platform_device *pdev,
		 enum ipi_id id, void *buf,
		 unsigned int len);

/**
 * vcu_get_plat_device - get VCU's platform device
 *
 * @pdev:	the platform device of the module requesting VCU platform
 *		device for using VCU API.
 *
 * Return: Return NULL if it is failed.
 * otherwise it is VCU's platform device
 **/
struct platform_device *vcu_get_plat_device(struct platform_device *pdev);

/**
 * vcu_get_vdec_hw_capa - get video decoder hardware capability
 *
 * @pdev:	VCU platform device
 *
 * Return: video decoder hardware capability
 **/
unsigned int vcu_get_vdec_hw_capa(struct platform_device *pdev);

/**
 * vcu_get_venc_hw_capa - get video encoder hardware capability
 *
 * @pdev:	VCU platform device
 *
 * Return: video encoder hardware capability
 **/
unsigned int vcu_get_venc_hw_capa(struct platform_device *pdev);

/**
 * vcu_load_firmware - download VCU firmware and boot it
 *
 * @pdev:	VCU platform device
 *
 * Return: Return 0 if downloading firmware successfully,
 * otherwise it is failed
 **/
int vcu_load_firmware(struct platform_device *pdev);

/**
 * vcu_compare_version - compare firmware version and expected version
 *
 * @pdev:		VCU platform device
 * @expected_version:	expected version
 *
 * Return: < 0 if firmware version is older than expected version
 *         = 0 if firmware version is equal to expected version
 *         > 0 if firmware version is newer than expected version
 **/
int vcu_compare_version(struct platform_device *pdev,
			const char *expected_version);

/**
 * vcu_mapping_dm_addr - Mapping DTCM/DMEM to kernel virtual address
 *
 * @pdev:	VCU platform device
 * @dmem_addr:	VCU's data memory address
 *
 * Mapping the VCU's DTCM (Data Tightly-Coupled Memory) /
 * DMEM (Data Extended Memory) memory address to
 * kernel virtual address.
 *
 * Return: Return ERR_PTR(-EINVAL) if mapping failed,
 * otherwise the mapped kernel virtual address
 **/
void *vcu_mapping_dm_addr(struct platform_device *pdev,
			  uintptr_t dtcm_dmem_addr);

/**
 * vcu_get_task - Get VCUD task information
 *
 * @task:	VCUD task
 * @f:		VCUD task file
 *
 * Get VCUD task information from mtk_vcu driver.
 *
 **/
void vcu_get_task(struct task_struct **task, struct files_struct **f);

#endif /* _MTK_VCU_H */
