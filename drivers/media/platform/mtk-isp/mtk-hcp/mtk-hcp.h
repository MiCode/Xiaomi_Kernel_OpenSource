/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_H
#define MTK_HCP_H

#include <linux/fdtable.h>
#include <linux/platform_device.h>

//#include "scp_ipi.h"

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
	HCP_DIP_INIT_ID, //CHRISTBD
	HCP_IMGSYS_INIT_ID = HCP_DIP_INIT_ID,
	HCP_DIP_FRAME_ID,
	HCP_IMGSYS_FRAME_ID = HCP_DIP_FRAME_ID,
	HCP_DIP_HW_TIMEOUT_ID,
	HCP_IMGSYS_HW_TIMEOUT_ID = HCP_DIP_HW_TIMEOUT_ID,
	HCP_DIP_DEQUE_DUMP_ID,
	HCP_IMGSYS_DEQUE_DONE_ID,
	HCP_IMGSYS_DEINIT_ID,
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
	MODULE_IMG = MODULE_DIP,
	MODULE_FD,
	MODULE_RSC,
	MODULE_MAX_ID,
};

/**
 * mtk_hcp_register - register an hcp function
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
 * mtk_hcp_unregister - unregister an hcp function
 *
 * @pdev:	HCP platform device
 * @id:	HCP ID
 *
 * Return: Return 0 if hcp unregisters successfully, otherwise it is failed.
 */
int mtk_hcp_unregister(struct platform_device *pdev, enum hcp_id id);

/**
 * mtk_hcp_send - send data from camera kernel driver to HCP and wait the
 *		  command send to demand.
 *
 * @pdev:		HCP platform device
 * @id:			HCP ID
 * @buf:		the data buffer
 * @len:		the data buffer length
 * @frame_no:	frame number
 *
 * This function is thread-safe. When this function returns,
 * HCP has received the data and save the data in the workqueue.
 * After that it will schedule work to dequeue to send data to CM4 or
 * RED for programming register.
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int mtk_hcp_send(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int frame_no);

/**
 * mtk_hcp_send_async - send data from camera kernel driver to HCP without
 *			waiting demand receives the command.
 *
 * @pdev:		HCP platform device
 * @id:			HCP ID
 * @buf:		the data buffer
 * @len:		the data buffer length
 * @frame_no:	frame number
 *
 * This function is thread-safe. When this function returns,
 * HCP has received the data and save the data in the workqueue.
 * After that it will schedule work to dequeue to send data to CM4 or
 * RED for programming register.
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int mtk_hcp_send_async(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int frame_no);

/**
 * mtk_hcp_get_plat_device - get HCP's platform device
 *
 * @pdev:	the platform device of the module requesting HCP platform
 *		device for using HCP API.
 *
 * Return: Return NULL if it is failed.
 * otherwise it is HCP's platform device
 **/
struct platform_device *mtk_hcp_get_plat_device(struct platform_device *pdev);

/**
 * mtk_hcp_get_current_task - get hcp driver's task struct.
 *
 * @pdev:	HCP platform device
 *
 * This function returns the current task inside HCP platform device,
 * which is initialized when hcp device being opened.
 **/
struct task_struct *mtk_hcp_get_current_task(struct platform_device *pdev);

/**
 * mtk_hcp_allocate_working_buffer - allocate driver working buffer.
 *
 * @pdev:	HCP platform device
 *
 * This function allocate working buffers and store the information
 * in mtk_hcp_reserve_mblock.
 **/
int mtk_hcp_allocate_working_buffer(struct platform_device *pdev);

/**
 * mtk_hcp_release_working_buffer - release driver working buffer.
 *
 * @pdev:	HCP platform device
 *
 * This function release working buffers
 **/
int mtk_hcp_release_working_buffer(struct platform_device *pdev);

/**
 * mtk_hcp_purge_msg - purge messages
 *
 * @pdev:	HCP platform device
 *
 * This function purges messages
 **/
void mtk_hcp_purge_msg(struct platform_device *pdev);


#define HCP_RESERVED_MEM	(1)
#define MTK_CM4_SUPPORT     (0)

#if HCP_RESERVED_MEM
/* hcp reserve memory ID definition*/
enum mtk_hcp_reserve_mem_id_t {
	#ifdef NEED_LEGACY_MEM
	ISP_MEM_ID,
	DIP_MEM_FOR_HW_ID,
	DIP_MEM_FOR_SW_ID,
	MDP_MEM_ID,
	FD_MEM_ID,
	#else
	DIP_MEM_FOR_HW_ID,
	IMG_MEM_FOR_HW_ID = DIP_MEM_FOR_HW_ID, /*shared buffer for ipi_param*/
	#endif
	/*need replace DIP_MEM_FOR_HW_ID & DIP_MEM_FOR_SW_ID*/
	WPE_MEM_C_ID,	/*module cq buffer*/
	WPE_MEM_T_ID,	/*module tdr buffer*/
	TRAW_MEM_C_ID,	/*module cq buffer*/
	TRAW_MEM_T_ID,	/*module tdr buffer*/
	DIP_MEM_C_ID,	/*module cq buffer*/
	DIP_MEM_T_ID,	/*module tdr buffer*/
	PQDIP_MEM_C_ID,	/*module cq buffer*/
	PQDIP_MEM_T_ID,	/*module tdr buffer*/
	ADL_MEM_C_ID,	/*module cq buffer*/
	ADL_MEM_T_ID,	/*module tdr buffer*/
	IMG_MEM_G_ID,	/*gce cmd buffer*/
	NUMS_MEM_ID,
};

/**
 * struct mtk_hcp_reserve_mblock - info about memory buffer allocated in kernel
 *
 * @num:				vb2_dc_buf
 * @start_phys:			starting addr(phy/iova) about allocated buffer
 * @start_virt:			starting addr(kva) about allocated buffer
 * @start_dma:			starting addr(iova) about allocated buffer
 * @size:				allocated buffer size
 * @is_dma_buf:			attribute: is_dma_buf or not
 * @mmap_cnt:			counter about mmap times
 * @mem_priv:			vb2_dc_buf
 * @d_buf:				dma_buf
 * @fd:					buffer fd
 */
struct mtk_hcp_reserve_mblock {
	enum mtk_hcp_reserve_mem_id_t num;
	phys_addr_t start_phys;
	void *start_virt;
	phys_addr_t start_dma;
	phys_addr_t size;
	uint8_t is_dma_buf;
	/*new add*/
	int mmap_cnt;
	void *mem_priv;
	struct dma_buf *d_buf;
	int fd;
	struct ion_handle *pIonHandle;
};

int hcp_allocate_buffer(struct platform_device *pdev,
	enum mtk_hcp_reserve_mem_id_t id,
	unsigned int size);
int hcp_get_ion_buffer_fd(struct platform_device *pdev,
	enum mtk_hcp_reserve_mem_id_t id);
void hcp_close_ion_buffer_fd(struct platform_device *pdev,
	enum mtk_hcp_reserve_mem_id_t id);
int hcp_free_buffer(struct platform_device *pdev,
	enum mtk_hcp_reserve_mem_id_t id);
extern phys_addr_t mtk_hcp_get_reserve_mem_phys(
					enum mtk_hcp_reserve_mem_id_t id);
extern void *mtk_hcp_get_reserve_mem_virt(enum mtk_hcp_reserve_mem_id_t id);
extern void mtk_hcp_set_reserve_mem_virt(enum mtk_hcp_reserve_mem_id_t id,
	void *virmem);
extern phys_addr_t mtk_hcp_get_reserve_mem_dma(
					enum mtk_hcp_reserve_mem_id_t id);
extern phys_addr_t mtk_hcp_get_reserve_mem_size(
					enum mtk_hcp_reserve_mem_id_t id);
extern uint32_t mtk_hcp_get_reserve_mem_fd(
					enum mtk_hcp_reserve_mem_id_t id);
extern void mtk_hcp_set_reserve_mem_fd(enum mtk_hcp_reserve_mem_id_t id,
	uint32_t fd);
#endif


#endif /* _MTK_HCP_H */
