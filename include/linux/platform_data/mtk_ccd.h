/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef _MTK_CCD_H
#define _MTK_CCD_H

#include <linux/platform_device.h>
#include <linux/cdev.h>

typedef void (*ccd_ipi_handler_t) (void *data,
				   unsigned int len,
				   void *priv);

struct dma_buf;
struct mtk_ccd_memory;
struct mtk_rpmsg_rproc_subdev;
struct mtk_ccd_rpmsg_endpoint;
struct ccd_master_status_item;
struct ccd_master_listen_item;
struct ccd_worker_item;
enum ccd_ipi_id;
struct mtk_ccd_memory;

/**
 * struct mem_obj - memory buffer allocated in kernel
 *
 * @iova:	iova of buffer
 * @len:	buffer length
 * @va: kernel virtual address
 */
struct mem_obj {
	dma_addr_t iova;
	unsigned int len;
	void *va;
};

enum ccd_map_hw_reg_id {
	CCD_CAM1, /* 0x16030000 */
	CCD_MAP_HW_REG_NUM
};

struct map_hw_reg {
	unsigned long base;
	unsigned long len;
};

struct ccd_master_status {
	unsigned int state;
};

struct rp_scp_ipi_desc {
	ccd_ipi_handler_t handler;
	void *priv;
};

struct mtk_ccd {
	struct device *dev;
	struct rproc *rproc;

	dev_t ccd_devno;
	struct cdev ccd_cdev;
	struct class *ccd_class;

	struct map_hw_reg map_base[CCD_MAP_HW_REG_NUM];

	struct rproc_subdev *rpmsg_subdev;
	struct ccd_master_status master_status;
	struct mtk_ccd_memory *ccd_memory;

	struct mutex *ccd_open_mutex;
};

/**
 * ccd_ipi_register - register an ipi function
 *
 * @pdev:	CCD platform device
 * @id:		IPI ID
 * @handler:	IPI handler
 * @priv:	private data for IPI handler
 *
 * Register an ipi function to receive ipi interrupt from CCD.
 *
 * Return: Return 0 if ipi registers successfully, otherwise it is failed.
 */
int ccd_ipi_register(struct platform_device *pdev,
		     enum ccd_ipi_id id,
		     ccd_ipi_handler_t handler,
		     void *priv);

/**
 * ccd_ipi_unregister - unregister an ipi function
 *
 * @pdev:	CCD platform device
 * @id:		IPI ID
 *
 * Unregister an ipi function to receive ipi interrupt from CCD.
 */
void ccd_ipi_unregister(struct platform_device *pdev, enum ccd_ipi_id id);

/**
 * rpmsg_ccd_ipi_send - send data from AP to ccd.
 *
 * @pdev:	CCD platform device
 * @id:		IPI ID
 * @buf:	the data buffer
 * @len:	the data buffer length
 * @wait:	1: need ack
 *
 * This function is thread-safe. When this function returns,
 * CCD has received the data and starts the processing.
 * When the processing completes, IPI handler registered
 * by ccd_ipi_register will be called in interrupt context.
 *
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int rpmsg_ccd_ipi_send(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
		       struct mtk_ccd_rpmsg_endpoint *mept,
		       void *buf, unsigned int len, unsigned int wait);

void ccd_master_listen(struct mtk_ccd *ccd,
			      struct ccd_master_listen_item *listen_obj);

void ccd_master_destroy(struct mtk_ccd *ccd,
			struct ccd_master_status_item *master_obj);

int ccd_worker_read(struct mtk_ccd *ccd,
		     struct ccd_worker_item *read_obj);

void ccd_worker_write(struct mtk_ccd *ccd,
		      struct ccd_worker_item *write_obj);

/**
 * ccd_get_pdev - get CCD's platform device
 *
 * @pdev:	the platform device of the module requesting CCD platform
 *		device for using CCD API.
 *
 * Return: Return NULL if it is failed.
 * otherwise it is CCD's platform device
 **/
struct platform_device *ccd_get_pdev(struct platform_device *pdev);

/**
 * ccd_mapping_dm_addr - Mapping SRAM/DRAM to kernel virtual address
 *
 * @pdev:	CCD platform device
 * @mem_addr:	CCD views memory address
 *
 * Mapping the CCD's SRAM address /
 * DMEM (Data Extended Memory) memory address /
 * Working buffer memory address to
 * kernel virtual address.
 *
 * Return: Return ERR_PTR(-EINVAL) if mapping failed,
 * otherwise the mapped kernel virtual address
 **/
void *ccd_mapping_dm_addr(struct platform_device *pdev,
			  u32 mem_addr);

void mtk_ccd_get_service(struct mtk_ccd *ccd,
			 struct task_struct **task,
			 struct files_struct **f);

void *mtk_ccd_get_buffer(struct mtk_ccd *ccd,
			 struct mem_obj *mem_buff_data);
int mtk_ccd_put_buffer(struct mtk_ccd *ccd,
			struct mem_obj *mem_buff_data);

int mtk_ccd_get_buffer_fd(struct mtk_ccd *ccd, void *mem_priv);
int mtk_ccd_put_buffer_fd(struct mtk_ccd *ccd,
			struct mem_obj *mem_buff_data,
unsigned int target_fd);
struct dma_buf *mtk_ccd_get_buffer_dmabuf(struct mtk_ccd *ccd,
			void *mem_priv);
#endif /* _MTK_CCD_H */
