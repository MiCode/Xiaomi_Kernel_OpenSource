/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Andrew-CT Chen <andrew-ct.chen@mediatek.com>
 */

#ifndef MTK_VCU_H
#define MTK_VCU_H

#include <aee.h>
#include <linux/fdtable.h>
#include <linux/platform_device.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define vcu_aee_print(string, args...) do {\
	char vcu_name[100];\
	int ret;\
	ret = snprintf(vcu_name, 100, "[VCU] "string, ##args); \
	if (ret > 0)\
		aee_kernel_warning_api(__FILE__, __LINE__, \
			DB_OPT_MMPROFILE_BUFFER | DB_OPT_NE_JBT_TRACES, \
			vcu_name, "[VCU] error:"string, ##args); \
	pr_info("[VCU] error:"string, ##args);  \
	} while (0)
#else
#define vcu_aee_print(string, args...) \
		pr_info("[VCU] error:"string, ##args)

#endif

/**
 * VCU (Video Communication/Controller Unit)
 * is a tiny processor controlling video hardware
 * related to video codec, scaling and color format converting.
 * VCU interfaces with other blocks by share memory and interrupt.
 **/

extern int mtk_vcodec_vcp;

typedef int (*ipi_handler_t)(void *data,
							 unsigned int len,
							 void *priv);

/**
 * enum ipi_id - the id of inter-processor interrupt
 *
 * @IPI_VCU_INIT:       The interrupt from vcu is to notfiy kernel
 *                      VCU initialization completed.
 *                      IPI_VCU_INIT is sent from VCU when firmware is
 *                      loaded. AP doesn't need to send IPI_VCU_INIT
 *                      command to VCU.
 *                      For other IPI below, AP should send the request
 *                      to VCU to trigger the interrupt.
 * @IPI_VDEC_COMMON:    The interrupt from vcu is to notify kernel to
 *                      handle video codecs job, and vice versa.
 * @IPI_VENC_COMMON:    The interrupt from vcu is to notify kernel to
 *                      handle video codecs job, and vice versa.
 * @IPI_MDP:            The interrupt from vcu is to notify kernel to
 *                      handle MDP (Media Data Path) job, and vice versa.
 * @IPI_CAMERA: The interrupt from vcu is to notify kernel to
 *                      handle camera job, and vice versa.
 * @IPI_MAX:            The maximum IPI number
 */

enum ipi_id {
	IPI_VCU_INIT = 0,
	IPI_VDEC_COMMON,
	IPI_VDEC_RESOURCE,
	IPI_VENC_COMMON,
	IPI_MDP,
	IPI_MDP_1,
	IPI_MDP_2,
	IPI_MDP_3,
	IPI_CAMERA,
	IPI_MAX = 20,
};

enum vcu_codec_ipi_type {
	VCU_VDEC = 0,
	VCU_VENC,
	VCU_RESOURCE,
	VCU_CODEC_MAX
};

struct vcu_v4l2_callback_func {
	void (*enc_prepare)(void *ctx_prepare,
		unsigned int core_id, unsigned long *flags);
	void (*enc_unprepare)(void *ctx_unprepare,
		unsigned int core_id, unsigned long *flags);
	void (*enc_pmqos_gce_begin)(void *ctx_begin,
		unsigned int core_id, int job_cnt);
	void (*enc_pmqos_gce_end)(void *ctx_end,
		unsigned int core_id, int job_cnt);
	void (*gce_timeout_dump)(void *ctx);
	void (*vdec_realease_lock)(void *ctx);
	int (*enc_lock)(void *ctx_lock, int core_id, bool sec);
	void (*enc_unlock)(void *ctx_unlock, int core_id);
};

struct vcu_v4l2_func {
	struct platform_device *(*vcu_get_plat_device)(struct platform_device *pdev);
	int (*vcu_load_firmware)(struct platform_device *pdev);
	int (*vcu_compare_version)(struct platform_device *pdev,
				const char *expected_version);
	void (*vcu_get_task)(struct task_struct **task, int reset);
	int (*vcu_set_v4l2_callback)(struct platform_device *pdev,
		struct vcu_v4l2_callback_func *call_back);
	int (*vcu_get_ctx_ipi_binding_lock)(struct platform_device *pdev,
		struct mutex **mutex, unsigned long type);
	int (*vcu_set_codec_ctx)(struct platform_device *pdev,
			 void *codec_ctx, struct vb2_buffer *src_vb,
			 struct vb2_buffer *dst_vb, unsigned long type);
	int (*vcu_clear_codec_ctx)(struct platform_device *pdev,
			 void *codec_ctx, unsigned long type);
	void *(*vcu_mapping_dm_addr)(struct platform_device *pdev,
				  uintptr_t dtcm_dmem_addr);
	int (*vcu_ipi_register)(struct platform_device *pdev,
				 enum ipi_id id, ipi_handler_t handler,
				 const char *name, void *priv);
	int (*vcu_ipi_send)(struct platform_device *pdev,
			 enum ipi_id id, void *buf,
			 unsigned int len, void *priv);
};
extern struct vcu_v4l2_func vcu_func;

/**
 * vcu_ipi_register - register an ipi function
 *
 * @pdev:       VCU platform device
 * @id:         IPI ID
 * @handler:    IPI handler
 * @name:       IPI name
 * @priv:       private data for IPI handler
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
 * @pdev:       VCU platform device
 * @id:         IPI ID
 * @buf:        the data buffer
 * @len:        the data buffer length
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
				 unsigned int len, void *priv);

/**
 * vcu_get_plat_device - get VCU's platform device
 *
 * @pdev:       the platform device of the module requesting VCU platform
 *              device for using VCU API.
 *
 * Return: Return NULL if it is failed.
 * otherwise it is VCU's platform device
 **/
struct platform_device *vcu_get_plat_device(struct platform_device *pdev);

/**
 * vcu_load_firmware - download VCU firmware and boot it
 *
 * @pdev:       VCU platform device
 *
 * Return: Return 0 if downloading firmware successfully,
 * otherwise it is failed
 **/
int vcu_load_firmware(struct platform_device *pdev);

/**
 * vcu_compare_version - compare firmware version and expected version
 *
 * @pdev:               VCU platform device
 * @expected_version:   expected version
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
 * @pdev:       VCU platform device
 * @dmem_addr:  VCU's data memory address
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
 * @task:       VCUD task
 * @f:          VCUD task filie
 * @reset:      flag to reset task and file
 *
 * Get VCUD task information from mtk_vcu driver.
 *
 **/
void vcu_get_task(struct task_struct **task, int reset);
int vcu_set_v4l2_callback(struct platform_device *pdev,
	struct vcu_v4l2_callback_func *call_back);
int vcu_get_ctx_ipi_binding_lock(struct platform_device *pdev,
	struct mutex **mutex, unsigned long type);
int vcu_set_codec_ctx(struct platform_device *pdev,
		 void *codec_ctx, struct vb2_buffer *src_vb,
		 struct vb2_buffer *dst_vb, unsigned long type);
int vcu_clear_codec_ctx(struct platform_device *pdev,
		 void *codec_ctx, unsigned long type);
void vcu_get_gce_lock(struct platform_device *pdev, unsigned long codec_type);
void vcu_put_gce_lock(struct platform_device *pdev, unsigned long codec_type);

extern void venc_encode_prepare(void *ctx_prepare,
		unsigned int core_id, unsigned long *flags);
extern void venc_encode_unprepare(void *ctx_prepare,
		unsigned int core_id, unsigned long *flags);
extern int venc_lock(void *ctx_lock, int core_id, bool sec);
extern void venc_unlock(void *ctx_unlock, int core_id);
extern void venc_encode_pmqos_gce_begin(void *ctx_begin,
		unsigned int core_id, int job_cnt);
extern void venc_encode_pmqos_gce_end(void *ctx_end,
		unsigned int core_id, int job_cnt);
extern void vdec_check_release_lock(void *ctx_check);
extern void mtk_vcodec_gce_timeout_dump(void *ctx);

#endif /* _MTK_VCU_H */
