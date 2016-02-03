/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/msm_ion.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <media/videobuf2-core.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include "msm_fd_dev.h"
#include "msm_fd_hw.h"
#include "msm_fd_regs.h"
#include "cam_smmu_api.h"
#include "msm_camera_io_util.h"

/* After which revision misc irq for engine is needed */
#define MSM_FD_MISC_IRQ_FROM_REV 0x10010000
/* Face detection workqueue name */
#define MSM_FD_WORQUEUE_NAME "face-detection"
/* Face detection bus client name */
#define MSM_FD_BUS_CLIENT_NAME "msm_face_detect"
/* Face detection processing timeout in ms */
#define MSM_FD_PROCESSING_TIMEOUT_MS 500
/* Face detection halt timeout in ms */
#define MSM_FD_HALT_TIMEOUT_MS 100
/* Smmu callback name */
#define MSM_FD_SMMU_CB_NAME "camera_fd"
/*
 * enum msm_fd_reg_setting_entries - FD register setting entries in DT.
 * @MSM_FD_REG_ADDR_OFFSET_IDX: Register address offset index.
 * @MSM_FD_REG_VALUE_IDX: Register value index.
 * @MSM_FD_REG_MASK_IDX: Regester mask index.
 * @MSM_FD_REG_LAST_IDX: Index count.
 */
enum msm_fd_dt_reg_setting_index {
	MSM_FD_REG_ADDR_OFFSET_IDX,
	MSM_FD_REG_VALUE_IDX,
	MSM_FD_REG_MASK_IDX,
	MSM_FD_REG_LAST_IDX
};

/*
 * msm_fd_hw_read_reg - Fd read from register.
 * @fd: Pointer to fd device.
 * @base_idx: Fd memory resource index.
 * @reg: Register addr need to be read from.
 */
static inline u32 msm_fd_hw_read_reg(struct msm_fd_device *fd,
	enum msm_fd_mem_resources base_idx, u32 reg)
{
	return msm_camera_io_r(fd->iomem_base[base_idx] + reg);
}

/*
 * msm_fd_hw_read_reg - Fd write to register.
 * @fd: Pointer to fd device.
 * @base_idx: Fd memory resource index.
 * @reg: Register addr need to be read from.
 e @value: Value to be written.
 */
static inline void msm_fd_hw_write_reg(struct msm_fd_device *fd,
	enum msm_fd_mem_resources base_idx, u32 reg, u32 value)
{
	msm_camera_io_w(value, fd->iomem_base[base_idx] + reg);
}

/*
 * msm_fd_hw_reg_clr - Fd clear register bits.
 * @fd: Pointer to fd device.
 * @base_idx: Fd memory resource index.
 * @reg: Register addr need to be read from.
 * @clr_bits: Bits need to be clear from register.
 */
static inline void msm_fd_hw_reg_clr(struct msm_fd_device *fd,
	enum msm_fd_mem_resources mmio_range, u32 reg, u32 clr_bits)
{
	u32 bits = msm_fd_hw_read_reg(fd, mmio_range, reg);

	msm_fd_hw_write_reg(fd, mmio_range, reg, (bits & ~clr_bits));
}

/*
 * msm_fd_hw_reg_clr - Fd set register bits.
 * @fd: Pointer to fd device.
 * @base_idx: Fd memory resource index.
 * @reg: Register addr need to be read from.
 * @set_bits: Bits need to be set to register.
 */
static inline void msm_fd_hw_reg_set(struct msm_fd_device *fd,
	enum msm_fd_mem_resources mmio_range, u32 reg, u32 set_bits)
{
	u32 bits = msm_fd_hw_read_reg(fd, mmio_range, reg);

	msm_fd_hw_write_reg(fd, mmio_range, reg, (bits | set_bits));
}

/*
 * msm_fd_hw_reg_clr - Fd set size mode register.
 * @fd: Pointer to fd device.
 * @mode: Size mode to be set.
 */
static inline void msm_fd_hw_set_size_mode(struct msm_fd_device *fd, u32 mode)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_IMAGE_SIZE, mode);
}

/*
 * msm_fd_hw_reg_clr - Fd set crop registers.
 * @fd: Pointer to fd device.
 * @crop: Pointer to v4l2 crop struct containing the crop information
 */
static inline void msm_fd_hw_set_crop(struct msm_fd_device *fd,
	struct v4l2_rect *crop)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_START_X,
		(crop->top & MSM_FD_START_X_MASK));

	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_START_Y,
		(crop->left & MSM_FD_START_Y_MASK));

	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_SIZE_X,
		(crop->width & MSM_FD_SIZE_X_MASK));

	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_SIZE_Y,
		(crop->height & MSM_FD_SIZE_Y_MASK));
}

/*
 * msm_fd_hw_reg_clr - Fd set bytes per line register.
 * @fd: Pointer to fd device.
 * @b: Bytes per line need to be set.
 */
static inline void msm_fd_hw_set_bytesperline(struct msm_fd_device *fd, u32 b)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_LINE_BYTES,
		(b & MSM_FD_LINE_BYTES_MASK));
}

/*
 * msm_fd_hw_reg_clr - Fd set image address.
 * @fd: Pointer to fd device.
 * @addr: Input image address need to be set.
 */
static inline void msm_fd_hw_set_image_addr(struct msm_fd_device *fd, u32 addr)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_IMAGE_ADDR, addr);
}

/*
 * msm_fd_hw_set_work_addr - Fd set working buffer address.
 * @fd: Pointer to fd device.
 * @addr: Working buffer address need to be set.
 */
static inline void msm_fd_hw_set_work_addr(struct msm_fd_device *fd, u32 addr)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_WORK_ADDR, addr);
}

/*
 * msm_fd_hw_set_direction_angle - Fd set face direction and face angle.
 * @fd: Pointer to fd device.
 * @direction: Face direction need to be set.
 * @angle: Face angle need to be set.
 */
static inline void msm_fd_hw_set_direction_angle(struct msm_fd_device *fd,
	u32 direction, u32 angle)
{
	u32 reg;
	u32 value;

	value = direction | (angle ? 1 << (angle + 1) : 0);
	if (value > MSM_FD_CONDT_DIR_MAX)
		value = MSM_FD_CONDT_DIR_MAX;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONDT);

	reg &= ~MSM_FD_CONDT_DIR_MASK;
	reg |= (value << MSM_FD_CONDT_DIR_SHIFT);

	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONDT, reg);
}

/*
 * msm_fd_hw_set_min_face - Fd set minimum face size register.
 * @fd: Pointer to fd device.
 * @size: Minimum face size need to be set.
 */
static inline void msm_fd_hw_set_min_face(struct msm_fd_device *fd, u32 size)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONDT);

	reg &= ~MSM_FD_CONDT_MIN_MASK;
	reg |= (size << MSM_FD_CONDT_MIN_SHIFT);

	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONDT, reg);
}

/*
 * msm_fd_hw_set_threshold - Fd set detection threshold register.
 * @fd: Pointer to fd device.
 * @c: Maximum face count need to be set.
 */
static inline void msm_fd_hw_set_threshold(struct msm_fd_device *fd, u32 thr)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_DHINT,
		(thr & MSM_FD_DHINT_MASK));
}

/*
 * msm_fd_hw_srst - Sw reset control registers.
 * @fd: Pointer to fd device.
 *
 * Before every processing we need to toggle this bit,
 * This functions set sw reset control bit to 1/0.
 */
static inline void msm_fd_hw_srst(struct msm_fd_device *fd)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONTROL,
		MSM_FD_CONTROL_SRST);
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONTROL, 0);
}

/*
 * msm_fd_hw_get_face_count - Fd read face count register.
 * @fd: Pointer to fd device.
 */
int msm_fd_hw_get_face_count(struct msm_fd_device *fd)
{
	u32 reg;
	u32 value;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_RESULT_CNT);

	value = reg & MSM_FD_RESULT_CNT_MASK;
	if (value > MSM_FD_MAX_FACES_DETECTED) {
		dev_warn(fd->dev, "Face count %d out of limit\n", value);
		value = MSM_FD_MAX_FACES_DETECTED;
	}

	return value;
}

/*
 * msm_fd_hw_run - Starts face detection engine.
 * @fd: Pointer to fd device.
 *
 * Before call this function make sure that control sw reset is perfomed
 * (see function msm_fd_hw_srst).
 * NOTE: Engine need to be reset before started again.
 */
static inline void msm_fd_hw_run(struct msm_fd_device *fd)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONTROL,
		MSM_FD_CONTROL_RUN);
}

/*
 * msm_fd_hw_is_finished - Check if fd hw engine is done with processing.
 * @fd: Pointer to fd device.
 *
 * NOTE: If finish bit is not set, we should not read the result.
 */
static int msm_fd_hw_is_finished(struct msm_fd_device *fd)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONTROL);

	return reg & MSM_FD_CONTROL_FINISH;
}

/*
 * msm_fd_hw_is_runnig - Check if fd hw engine is busy.
 * @fd: Pointer to fd device.
 */
static int msm_fd_hw_is_runnig(struct msm_fd_device *fd)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE, MSM_FD_CONTROL);

	return reg & MSM_FD_CONTROL_RUN;
}

/*
 * msm_fd_hw_misc_irq_is_core - Check if fd received misc core irq.
 * @fd: Pointer to fd device.
 */
static int msm_fd_hw_misc_irq_is_core(struct msm_fd_device *fd)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_MISC,
		MSM_FD_MISC_IRQ_STATUS);

	return reg & MSM_FD_MISC_IRQ_STATUS_CORE_IRQ;
}

/*
 * msm_fd_hw_misc_irq_is_halt - Check if fd received misc halt irq.
 * @fd: Pointer to fd device.
 */
static int msm_fd_hw_misc_irq_is_halt(struct msm_fd_device *fd)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_MISC,
		MSM_FD_MISC_IRQ_STATUS);

	return reg & MSM_FD_MISC_IRQ_STATUS_HALT_REQ;
}

/*
* msm_fd_hw_misc_clear_all_irq - Clear all misc irq statuses.
* @fd: Pointer to fd device.
*/
static void msm_fd_hw_misc_clear_all_irq(struct msm_fd_device *fd)
{
	msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_MISC, MSM_FD_MISC_IRQ_CLEAR,
		MSM_FD_MISC_IRQ_CLEAR_HALT | MSM_FD_MISC_IRQ_CLEAR_CORE);
}

/*
* msm_fd_hw_misc_irq_enable - Enable fd misc core and halt irq.
* @fd: Pointer to fd device.
*/
static void msm_fd_hw_misc_irq_enable(struct msm_fd_device *fd)
{
	msm_fd_hw_reg_set(fd, MSM_FD_IOMEM_MISC, MSM_FD_MISC_IRQ_MASK,
		MSM_FD_MISC_IRQ_CLEAR_HALT | MSM_FD_MISC_IRQ_CLEAR_CORE);
}

/*
* msm_fd_hw_misc_irq_disable - Disable fd misc core and halt irq.
* @fd: Pointer to fd device.
*/
static void msm_fd_hw_misc_irq_disable(struct msm_fd_device *fd)
{
	msm_fd_hw_reg_clr(fd, MSM_FD_IOMEM_MISC, MSM_FD_MISC_IRQ_MASK,
		MSM_FD_MISC_IRQ_CLEAR_HALT | MSM_FD_MISC_IRQ_CLEAR_CORE);
}

/*
 * msm_fd_hw_get_revision - Get hw revision and store in to device.
 * @fd: Pointer to fd device.
 */
int msm_fd_hw_get_revision(struct msm_fd_device *fd)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_MISC,
		MSM_FD_MISC_HW_VERSION);

	dev_dbg(fd->dev, "Face detection hw revision 0x%x\n", reg);

	return reg;
}

/*
 * msm_fd_hw_get_result_x - Get fd result center x coordinate.
 * @fd: Pointer to fd device.
 * @idx: Result face index
 */
int msm_fd_hw_get_result_x(struct msm_fd_device *fd, int idx)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE,
		MSM_FD_RESULT_CENTER_X(idx));

	return reg;
}

/*
 * msm_fd_hw_get_result_y - Get fd result center y coordinate.
 * @fd: Pointer to fd device.
 * @idx: Result face index
 */
int msm_fd_hw_get_result_y(struct msm_fd_device *fd, int idx)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE,
		MSM_FD_RESULT_CENTER_Y(idx));

	return reg;
}

/*
 * msm_fd_hw_get_result_conf_size - Get fd result confident level and size.
 * @fd: Pointer to fd device.
 * @idx: Result face index.
 * @conf: Pointer to confident value need to be filled.
 * @size: Pointer to size value need to be filled.
 */
void msm_fd_hw_get_result_conf_size(struct msm_fd_device *fd,
	int idx, u32 *conf, u32 *size)
{
	u32 reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE,
		MSM_FD_RESULT_CONF_SIZE(idx));

	*conf = (reg >> MSM_FD_RESULT_CONF_SHIFT) & MSM_FD_RESULT_CONF_MASK;
	*size = (reg >> MSM_FD_RESULT_SIZE_SHIFT) & MSM_FD_RESULT_SIZE_MASK;
}

/*
 * msm_fd_hw_get_result_angle_pose - Get fd result angle and pose.
 * @fd: Pointer to fd device.
 * @idx: Result face index.
 * @angle: Pointer to angle value need to be filled.
 * @pose: Pointer to pose value need to be filled.
 */
void msm_fd_hw_get_result_angle_pose(struct msm_fd_device *fd, int idx,
	u32 *angle, u32 *pose)
{
	u32 reg;
	u32 pose_reg;

	reg = msm_fd_hw_read_reg(fd, MSM_FD_IOMEM_CORE,
		MSM_FD_RESULT_ANGLE_POSE(idx));
	*angle = (reg >> MSM_FD_RESULT_ANGLE_SHIFT) & MSM_FD_RESULT_ANGLE_MASK;
	pose_reg = (reg >> MSM_FD_RESULT_POSE_SHIFT) & MSM_FD_RESULT_POSE_MASK;

	switch (pose_reg) {
	case MSM_FD_RESULT_POSE_FRONT:
		*pose = MSM_FD_POSE_FRONT;
		break;
	case MSM_FD_RESULT_POSE_RIGHT_DIAGONAL:
		*pose = MSM_FD_POSE_RIGHT_DIAGONAL;
		break;
	case MSM_FD_RESULT_POSE_RIGHT:
		*pose = MSM_FD_POSE_RIGHT;
		break;
	case MSM_FD_RESULT_POSE_LEFT_DIAGONAL:
		*pose = MSM_FD_POSE_LEFT_DIAGONAL;
		break;
	case MSM_FD_RESULT_POSE_LEFT:
		*pose = MSM_FD_POSE_LEFT;
		break;
	default:
		dev_err(fd->dev, "Invalid pose from the engine\n");
		*pose = MSM_FD_POSE_FRONT;
		break;
	}
}

/*
 * msm_fd_hw_misc_irq_supported - Check if misc irq is supported.
 * @fd: Pointer to fd device.
 */
static int msm_fd_hw_misc_irq_supported(struct msm_fd_device *fd)
{
	return fd->hw_revision >= MSM_FD_MISC_IRQ_FROM_REV;
}

/*
 * msm_fd_hw_halt - Halt fd core.
 * @fd: Pointer to fd device.
 */
static void msm_fd_hw_halt(struct msm_fd_device *fd)
{
	unsigned long time;

	if (msm_fd_hw_misc_irq_supported(fd)) {
		init_completion(&fd->hw_halt_completion);

		msm_fd_hw_write_reg(fd, MSM_FD_IOMEM_MISC, MSM_FD_HW_STOP, 1);

		time = wait_for_completion_timeout(&fd->hw_halt_completion,
			msecs_to_jiffies(MSM_FD_HALT_TIMEOUT_MS));
		if (!time)
			dev_err(fd->dev, "Face detection halt timeout\n");

	}
}

/*
 * msm_fd_core_irq - Face detection core irq handler.
 * @irq: Irq number.
 * @dev_id: Pointer to fd device.
 */
static irqreturn_t msm_fd_hw_core_irq(int irq, void *dev_id)
{
	struct msm_fd_device *fd = dev_id;

	if (msm_fd_hw_is_finished(fd))
		queue_work(fd->work_queue, &fd->work);
	else
		dev_err(fd->dev, "Something wrong! FD still running\n");

	return IRQ_HANDLED;
}

/*
 * msm_fd_hw_misc_irq - Face detection misc irq handler.
 * @irq: Irq number.
 * @dev_id: Pointer to fd device.
 */
static irqreturn_t msm_fd_hw_misc_irq(int irq, void *dev_id)
{
	struct msm_fd_device *fd = dev_id;

	if (msm_fd_hw_misc_irq_is_core(fd))
		msm_fd_hw_core_irq(irq, dev_id);

	if (msm_fd_hw_misc_irq_is_halt(fd))
		complete_all(&fd->hw_halt_completion);

	msm_fd_hw_misc_clear_all_irq(fd);

	return IRQ_HANDLED;
}

/*
 * msm_fd_hw_request_irq - Configure and enable vbif interface.
 * @pdev: Pointer to platform device.
 * @fd: Pointer to fd device.
 * @work_func: Pointer to work func used for irq bottom half.
 */
int msm_fd_hw_request_irq(struct platform_device *pdev,
	struct msm_fd_device *fd, work_func_t work_func)
{
	int ret;

	fd->irq_num = platform_get_irq(pdev, 0);
	if (fd->irq_num < 0) {
		dev_err(fd->dev, "Can not get fd core irq resource\n");
		ret = -ENODEV;
		goto error_irq;
	}

	/* If vbif is shared we will need wrapper irq for releasing vbif */
	if (msm_fd_hw_misc_irq_supported(fd)) {
		ret = devm_request_irq(fd->dev, fd->irq_num,
			msm_fd_hw_misc_irq, IRQF_TRIGGER_RISING,
			dev_name(&pdev->dev), fd);
		if (ret) {
			dev_err(fd->dev, "Can not claim wrapper IRQ %d\n",
				fd->irq_num);
			goto error_irq;
		}
	} else {
		ret = devm_request_irq(fd->dev, fd->irq_num,
			msm_fd_hw_core_irq, IRQF_TRIGGER_RISING,
			dev_name(fd->dev), fd);
		if (ret) {
			dev_err(&pdev->dev, "Can not claim core IRQ %d\n",
				fd->irq_num);
			goto error_irq;
		}

	}

	fd->work_queue = alloc_workqueue(MSM_FD_WORQUEUE_NAME,
		WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!fd->work_queue) {
		dev_err(fd->dev, "Can not register workqueue\n");
		ret = -ENOMEM;
		goto error_alloc_workqueue;
	}
	INIT_WORK(&fd->work, work_func);

	return 0;

error_alloc_workqueue:
	devm_free_irq(fd->dev, fd->irq_num, fd);
error_irq:
	return ret;
}

/*
 * msm_fd_hw_release_irq - Free core and wrap irq.
 * @fd: Pointer to fd device.
 */
void msm_fd_hw_release_irq(struct msm_fd_device *fd)
{
	if (fd->irq_num >= 0) {
		devm_free_irq(fd->dev, fd->irq_num, fd);
		fd->irq_num = -1;
	}
	if (fd->work_queue) {
		destroy_workqueue(fd->work_queue);
		fd->work_queue = NULL;
	}
}

/*
 * msm_fd_hw_set_dt_parms_by_name() - read DT params and write to registers.
 * @fd: Pointer to fd device.
 * @dt_prop_name: Name of the device tree property to read.
 * @base_idx: Fd memory resource index.
 *
 * This function reads register offset and value pairs from dtsi based on
 * device tree property name and writes to FD registers.
 *
 * Return: 0 on success and negative error on failure.
 */
int32_t msm_fd_hw_set_dt_parms_by_name(struct msm_fd_device *fd,
			const char *dt_prop_name,
			enum msm_fd_mem_resources base_idx)
{
	struct device_node *of_node;
	int32_t i = 0 , rc = 0;
	uint32_t *dt_reg_settings = NULL;
	uint32_t dt_count = 0;

	of_node = fd->dev->of_node;
	pr_debug("%s:%d E\n", __func__, __LINE__);

	if (!of_get_property(of_node, dt_prop_name, &dt_count)) {
		pr_err("%s: Error property does not exist\n", __func__);
		return -ENOENT;
	}
	if (dt_count % (sizeof(int32_t) * MSM_FD_REG_LAST_IDX)) {
		pr_err("%s: Error invalid entries\n", __func__);
		return -EINVAL;
	}
	dt_count /= sizeof(int32_t);
	if (dt_count != 0) {
		dt_reg_settings = kcalloc(dt_count,
			sizeof(uint32_t),
			GFP_KERNEL);

		if (!dt_reg_settings)
			return -ENOMEM;

		rc = of_property_read_u32_array(of_node,
				dt_prop_name,
				dt_reg_settings,
				dt_count);
		if (rc < 0) {
			pr_err("%s: No reg info\n", __func__);
			kfree(dt_reg_settings);
			return -EINVAL;
		}

		for (i = 0; i < dt_count; i = i + MSM_FD_REG_LAST_IDX) {
			msm_fd_hw_reg_clr(fd, base_idx,
				dt_reg_settings[i + MSM_FD_REG_ADDR_OFFSET_IDX],
				dt_reg_settings[i + MSM_FD_REG_MASK_IDX]);
			msm_fd_hw_reg_set(fd, base_idx,
				dt_reg_settings[i + MSM_FD_REG_ADDR_OFFSET_IDX],
				dt_reg_settings[i + MSM_FD_REG_VALUE_IDX] &
				dt_reg_settings[i + MSM_FD_REG_MASK_IDX]);
			pr_debug("%s:%d] %p %08x\n", __func__, __LINE__,
				fd->iomem_base[base_idx] +
				dt_reg_settings[i + MSM_FD_REG_ADDR_OFFSET_IDX],
				dt_reg_settings[i + MSM_FD_REG_VALUE_IDX] &
				dt_reg_settings[i + MSM_FD_REG_MASK_IDX]);
		}
		kfree(dt_reg_settings);
	}
	return 0;
}

/*
 * msm_fd_hw_set_dt_parms() - set FD device tree configuration.
 * @fd: Pointer to fd device.
 *
 * This function holds an array of device tree property names and calls
 * msm_fd_hw_set_dt_parms_by_name() for each property.
 *
 * Return: 0 on success and negative error on failure.
 */
int msm_fd_hw_set_dt_parms(struct msm_fd_device *fd)
{
	int rc = 0;
	uint8_t dt_prop_cnt = MSM_FD_IOMEM_LAST;
	char *dt_prop_name[MSM_FD_IOMEM_LAST] = {"qcom,fd-core-reg-settings",
		"qcom,fd-misc-reg-settings", "qcom,fd-vbif-reg-settings"};

	while (dt_prop_cnt) {
		dt_prop_cnt--;
		rc = msm_fd_hw_set_dt_parms_by_name(fd,
			dt_prop_name[dt_prop_cnt],
			dt_prop_cnt);
		if (rc == -ENOENT) {
			pr_debug("%s: No %s property\n", __func__,
				dt_prop_name[dt_prop_cnt]);
			rc = 0;
		} else if (rc < 0) {
			pr_err("%s: %s params set fail\n", __func__,
				dt_prop_name[dt_prop_cnt]);
			return rc;
		}
	}
	return rc;
}

/*
 * msm_fd_hw_release_mem_resources - Releases memory resources.
 * @fd: Pointer to fd device.
 */
void msm_fd_hw_release_mem_resources(struct msm_fd_device *fd)
{
	int i;

	/* Prepare memory resources */
	for (i = 0; i < MSM_FD_IOMEM_LAST; i++) {
		if (fd->iomem_base[i]) {
			iounmap(fd->iomem_base[i]);
			fd->iomem_base[i] = NULL;
		}
		if (fd->ioarea[i]) {
			release_mem_region(fd->res_mem[i]->start,
				resource_size(fd->res_mem[i]));
			fd->ioarea[i] = NULL;
		}
		fd->res_mem[i] = NULL;
	}
}

/*
 * msm_fd_hw_get_mem_resources - Get memory resources.
 * @pdev: Pointer to fd platform device.
 * @fd: Pointer to fd device.
 *
 * Get and ioremap platform memory resources.
 */
int msm_fd_hw_get_mem_resources(struct platform_device *pdev,
	struct msm_fd_device *fd)
{
	int i;
	int ret = 0;

	/* Prepare memory resources */
	for (i = 0; i < MSM_FD_IOMEM_LAST; i++) {
		/* Get resources */
		fd->res_mem[i] = platform_get_resource(pdev,
			IORESOURCE_MEM, i);
		if (!fd->res_mem[i]) {
			dev_err(fd->dev, "Fail get resource idx %d\n",
				i);
			ret = -ENODEV;
			break;
		}

		fd->ioarea[i] = request_mem_region(fd->res_mem[i]->start,
			resource_size(fd->res_mem[i]), fd->res_mem[i]->name);
		if (!fd->ioarea[i]) {
			dev_err(fd->dev, "%s can not request mem\n",
				fd->res_mem[i]->name);
			ret = -ENODEV;
			break;
		}

		fd->iomem_base[i] = ioremap(fd->res_mem[i]->start,
			resource_size(fd->res_mem[i]));
		if (!fd->iomem_base[i]) {
			dev_err(fd->dev, "%s can not remap region\n",
				fd->res_mem[i]->name);
			ret = -ENODEV;
			break;
		}
	}

	if (ret < 0)
		msm_fd_hw_release_mem_resources(fd);

	return ret;
}

/*
 * msm_fd_hw_get_regulators - Get fd regulators.
 * @fd: Pointer to fd device.
 *
 * Read regulator information from device tree and perform get regulator.
 */
int msm_fd_hw_get_regulators(struct msm_fd_device *fd)
{
	const char *regulator_name;
	uint32_t cnt;
	int i;
	int ret;

	if (of_get_property(fd->dev->of_node, "qcom,vdd-names", NULL)) {
		cnt = of_property_count_strings(fd->dev->of_node,
						 "qcom,vdd-names");

		if ((cnt == 0) || (cnt == -EINVAL)) {
			dev_err(fd->dev, "no regulators found, count=%d\n",
				 cnt);
			return -EINVAL;
		}

		if (cnt > MSM_FD_MAX_REGULATOR_NUM) {
			dev_err(fd->dev,
				 "Exceed max number of regulators %d\n", cnt);
			return -EINVAL;
		}

		for (i = 0; i < cnt; i++) {
			ret = of_property_read_string_index(fd->dev->of_node,
						"qcom,vdd-names",
						i, &regulator_name);
			if (ret < 0) {
				dev_err(fd->dev,
					 "Cannot read regulator name %d\n", i);
				return ret;
			}

			fd->vdd[i] = regulator_get(fd->dev, regulator_name);
			if (IS_ERR(fd->vdd[i])) {
				ret = PTR_ERR(fd->vdd[i]);
				fd->vdd[i] = NULL;
				dev_err(fd->dev, "Error regulator get %s\n",
					 regulator_name);
				goto regulator_get_error;
			}
			dev_dbg(fd->dev, "Regulator name idx %d %s\n", i,
				 regulator_name);
		}
		fd->regulator_num = cnt;
	} else {
		fd->regulator_num = 1;
		fd->vdd[0] = regulator_get(fd->dev, "vdd");
		if (IS_ERR(fd->vdd[0])) {
			dev_err(fd->dev, "Fail to get vdd regulator\n");
			ret = PTR_ERR(fd->vdd[0]);
			fd->vdd[0] = NULL;
			return ret;
		}
	}
	return 0;

regulator_get_error:
	for (; i > 0; i--) {
		if (!IS_ERR_OR_NULL(fd->vdd[i - 1]))
			regulator_put(fd->vdd[i - 1]);
	}
	return ret;
}

/*
 * msm_fd_hw_put_regulators - Put fd regulators.
 * @fd: Pointer to fd device.
 */
int msm_fd_hw_put_regulators(struct msm_fd_device *fd)
{
	int i;

	for (i = fd->regulator_num - 1; i >= 0; i--) {
		if (!IS_ERR_OR_NULL(fd->vdd[i]))
			regulator_put(fd->vdd[i]);
	}
	return 0;
}

/*
 * msm_fd_hw_enable_regulators - Prepare and enable fd regulators.
 * @fd: Pointer to fd device.
 */
static int msm_fd_hw_enable_regulators(struct msm_fd_device *fd)
{
	int i;
	int ret;

	for (i = 0; i < fd->regulator_num; i++) {

		ret = regulator_enable(fd->vdd[i]);
		if (ret < 0) {
			dev_err(fd->dev, "regulator enable failed %d\n", i);
			regulator_put(fd->vdd[i]);
			goto error;
		}
	}

	return 0;
error:
	for (; i > 0; i--) {
		if (!IS_ERR_OR_NULL(fd->vdd[i - 1])) {
			regulator_disable(fd->vdd[i - 1]);
			regulator_put(fd->vdd[i - 1]);
		}
	}
	return ret;
}

/*
 * msm_fd_hw_disable_regulators - Disable fd regulator.
 * @fd: Pointer to fd device.
 */
static void msm_fd_hw_disable_regulators(struct msm_fd_device *fd)
{
	int i;

	for (i = fd->regulator_num - 1; i >= 0; i--) {
		if (!IS_ERR_OR_NULL(fd->vdd[i]))
			regulator_disable(fd->vdd[i]);
	}
}

/*
 * msm_fd_hw_get_clocks - Get fd clocks.
 * @fd: Pointer to fd device.
 *
 * Read clock information from device tree and perform get clock.
 */
int msm_fd_hw_get_clocks(struct msm_fd_device *fd)
{
	const char *clk_name;
	size_t cnt;
	int clk_rates;
	int i;
	int ret;

	cnt = of_property_count_strings(fd->dev->of_node, "clock-names");
	if (cnt > MSM_FD_MAX_CLK_NUM) {
		dev_err(fd->dev, "Exceed max number of clocks %zu\n", cnt);
		return -EINVAL;
	}

	clk_rates = 0;
	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(fd->dev->of_node,
			"clock-names", i, &clk_name);
		if (ret < 0) {
			dev_err(fd->dev, "Can not read clock name %d\n", i);
			goto error;
		}

		fd->clk[i] = clk_get(fd->dev, clk_name);
		if (IS_ERR(fd->clk[i])) {
			ret = -ENOENT;
			dev_err(fd->dev, "Error clock get %s\n", clk_name);
			goto error;
		}
		dev_dbg(fd->dev, "Clock name idx %d %s\n", i, clk_name);
	}
	fd->clk_num = cnt;

	cnt = 0;
	for (clk_rates = 0; clk_rates < MSM_FD_MAX_CLK_RATES; clk_rates++) {
		for (i = 0; i < fd->clk_num; i++) {
			ret = of_property_read_u32_index(fd->dev->of_node,
				"clock-rates", cnt++,
				&fd->clk_rates[clk_rates][i]);
			if (ret < 0)
				break;
			dev_dbg(fd->dev, "Clock rate idx %d idx %d value %d\n",
				clk_rates, i, fd->clk_rates[clk_rates][i]);

		}
		if (ret < 0)
			break;
	}
	fd->clk_rates_num = clk_rates;
	if (fd->clk_rates_num == 0) {
		ret = -ENOENT;
		dev_err(fd->dev, "Can not get clock rates\n");
		goto error;
	}

	return 0;
error:
	for (; i > 0; i--)
		clk_put(fd->clk[i - 1]);

	return ret;
}

/*
 * msm_fd_hw_get_clocks - Put fd clocks.
 * @fd: Pointer to fd device.
 */
int msm_fd_hw_put_clocks(struct msm_fd_device *fd)
{
	int i;

	for (i = 0; i < fd->clk_num; i++) {
		if (!IS_ERR_OR_NULL(fd->clk[i]))
			clk_put(fd->clk[i]);
		fd->clk_num = 0;
	}
	return 0;
}

/*
 * msm_fd_hw_set_clock_rate_idx - Set clock rate based on the index.
 * @fd: Pointer to fd device.
 * @idx: Clock Array index described in device tree.
 */
static int msm_fd_hw_set_clock_rate_idx(struct msm_fd_device *fd,
		unsigned int idx)
{
	int ret;
	long clk_rate;
	int i;

	if (idx >= fd->clk_rates_num) {
		dev_err(fd->dev, "Invalid clock index %u\n", idx);
		return -EINVAL;
	}

	for (i = 0; i < fd->clk_num; i++) {

		clk_rate = clk_round_rate(fd->clk[i], fd->clk_rates[idx][i]);
		if (clk_rate < 0) {
			dev_dbg(fd->dev, "Clk raund rate fail skip %d\n", i);
			continue;
		}

		ret = clk_set_rate(fd->clk[i], clk_rate);
		if (ret < 0) {
			dev_err(fd->dev, "Fail clock rate %ld\n", clk_rate);
			return -EINVAL;
		}
		dev_dbg(fd->dev, "Clk rate %d-%ld idx %d\n", i, clk_rate, idx);
	}

	return 0;
}
/*
 * msm_fd_hw_enable_clocks - Prepare and enable fd clocks.
 * @fd: Pointer to fd device.
 */
static int msm_fd_hw_enable_clocks(struct msm_fd_device *fd)
{
	int i;
	int ret;

	for (i = 0; i < fd->clk_num; i++) {
		ret = clk_prepare(fd->clk[i]);
		if (ret < 0) {
			dev_err(fd->dev, "clock prepare failed %d\n", i);
			goto error;
		}

		ret = clk_enable(fd->clk[i]);
		if (ret < 0) {
			dev_err(fd->dev, "clock enable %d\n", i);
			clk_unprepare(fd->clk[i]);
			goto error;
		}
	}

	return 0;
error:
	for (; i > 0; i--) {
		clk_disable(fd->clk[i - 1]);
		clk_unprepare(fd->clk[i - 1]);
	}
	return ret;
}
/*
 * msm_fd_hw_disable_clocks - Disable fd clock.
 * @fd: Pointer to fd device.
 */
static void msm_fd_hw_disable_clocks(struct msm_fd_device *fd)
{
	int i;

	for (i = 0; i < fd->clk_num; i++) {
		clk_disable(fd->clk[i]);
		clk_unprepare(fd->clk[i]);
	}
}

/*
 * msm_fd_hw_get_bus - Get bus bandwidth.
 * @fd: Pointer to fd device.
 *
 * Read bus bandwidth information from device tree.
 */
int msm_fd_hw_get_bus(struct msm_fd_device *fd)
{
	size_t cnt;
	unsigned int ab;
	unsigned int ib;
	unsigned int idx;
	int usecase;
	int ret;

	idx = MSM_FD_MAX_CLK_RATES;

	fd->bus_vectors = kzalloc(sizeof(*fd->bus_vectors) * idx, GFP_KERNEL);
	if (!fd->bus_vectors) {
		dev_err(fd->dev, "No memory for bus vectors\n");
		return -ENOMEM;
	}

	fd->bus_paths = kzalloc(sizeof(*fd->bus_paths) * idx, GFP_KERNEL);
	if (!fd->bus_paths) {
		dev_err(fd->dev, "No memory for bus paths\n");
		kfree(fd->bus_vectors);
		fd->bus_vectors = NULL;
		return -ENOMEM;
	}

	cnt = 0;
	for (usecase = 0; usecase < idx; usecase++) {
		ret = of_property_read_u32_index(fd->dev->of_node,
			"qcom,bus-bandwidth-vectors", cnt++, &ab);
		if (ret < 0)
			break;

		ret = of_property_read_u32_index(fd->dev->of_node,
			"qcom,bus-bandwidth-vectors", cnt++, &ib);
		if (ret < 0)
			break;

		fd->bus_vectors[usecase].src = MSM_BUS_MASTER_CPP;
		fd->bus_vectors[usecase].dst = MSM_BUS_SLAVE_EBI_CH0;
		fd->bus_vectors[usecase].ab = ab;
		fd->bus_vectors[usecase].ib = ib;

		fd->bus_paths[usecase].num_paths = 1;
		fd->bus_paths[usecase].vectors = &fd->bus_vectors[usecase];

		dev_dbg(fd->dev, "Bus bandwidth idx %d ab %u ib %u\n",
			usecase, ab, ib);
	}

	fd->bus_scale_data.usecase = fd->bus_paths;
	fd->bus_scale_data.num_usecases = usecase;
	fd->bus_scale_data.name = MSM_FD_BUS_CLIENT_NAME;

	return 0;
}

/*
 * msm_fd_hw_put_bus - Put bus bandwidth.
 * @fd: Pointer to fd device.
 */
void msm_fd_hw_put_bus(struct msm_fd_device *fd)
{
	kfree(fd->bus_vectors);
	fd->bus_vectors = NULL;

	kfree(fd->bus_paths);
	fd->bus_paths = NULL;

	fd->bus_scale_data.num_usecases = 0;
}
/*
 * msm_fd_hw_bus_request - Request bus for memory access.
 * @fd: Pointer to fd device.
 * @idx: Bus bandwidth array index described in device tree.
 */
static int msm_fd_hw_bus_request(struct msm_fd_device *fd, unsigned int idx)
{
	int ret;

	fd->bus_client = msm_bus_scale_register_client(&fd->bus_scale_data);
	if (!fd->bus_client) {
		dev_err(fd->dev, "Fail to register bus client\n");
		return -ENOENT;
	}

	ret = msm_bus_scale_client_update_request(fd->bus_client, idx);
	if (ret < 0) {
		dev_err(fd->dev, "Fail bus scale update %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * msm_fd_hw_bus_release - Release memory access bus.
 * @fd: Pointer to fd device.
 */
static void msm_fd_hw_bus_release(struct msm_fd_device *fd)
{
	if (fd->bus_client) {
		msm_bus_scale_unregister_client(fd->bus_client);
		fd->bus_client = 0;
	}
}

/*
 * msm_fd_hw_get - Get fd hw for performing any hw operation.
 * @fd: Pointer to fd device.
 * @clock_rate_idx: Clock rate index.
 *
 * Prepare fd hw for operation. Have reference count protected by
 * fd device mutex.
 */
int msm_fd_hw_get(struct msm_fd_device *fd, unsigned int clock_rate_idx)
{
	int ret;

	mutex_lock(&fd->lock);

	if (fd->ref_count == 0) {
		ret = msm_fd_hw_enable_regulators(fd);
		if (ret < 0) {
			dev_err(fd->dev, "Fail to enable vdd\n");
			goto error;
		}

		ret = msm_fd_hw_bus_request(fd, clock_rate_idx);
		if (ret < 0) {
			dev_err(fd->dev, "Fail bus request\n");
			goto error_bus_request;
		}

		ret = msm_fd_hw_set_clock_rate_idx(fd, clock_rate_idx);
		if (ret < 0) {
			dev_err(fd->dev, "Fail to set clock rate idx\n");
			goto error_clocks;
		}

		ret = msm_fd_hw_enable_clocks(fd);
		if (ret < 0) {
			dev_err(fd->dev, "Fail to enable clocks\n");
			goto error_clocks;
		}

		if (msm_fd_hw_misc_irq_supported(fd))
			msm_fd_hw_misc_irq_enable(fd);

		ret = msm_fd_hw_set_dt_parms(fd);
		if (ret < 0)
			goto error_set_dt;
	}

	fd->ref_count++;
	mutex_unlock(&fd->lock);

	return 0;

error_set_dt:
	if (msm_fd_hw_misc_irq_supported(fd))
		msm_fd_hw_misc_irq_disable(fd);
	msm_fd_hw_disable_clocks(fd);
error_clocks:
	msm_fd_hw_bus_release(fd);
error_bus_request:
	msm_fd_hw_disable_regulators(fd);
error:
	mutex_unlock(&fd->lock);
	return ret;
}

/*
 * msm_fd_hw_get - Put fd hw.
 * @fd: Pointer to fd device.
 *
 * Release fd hw. Have reference count protected by
 * fd device mutex.
 */
void msm_fd_hw_put(struct msm_fd_device *fd)
{
	mutex_lock(&fd->lock);
	BUG_ON(fd->ref_count == 0);

	if (--fd->ref_count == 0) {
		msm_fd_hw_halt(fd);

		if (msm_fd_hw_misc_irq_supported(fd))
			msm_fd_hw_misc_irq_disable(fd);

		msm_fd_hw_bus_release(fd);
		msm_fd_hw_disable_clocks(fd);
		msm_fd_hw_disable_regulators(fd);
	}
	mutex_unlock(&fd->lock);
}

/*
 * msm_fd_hw_attach_iommu - Attach iommu to face detection engine.
 * @fd: Pointer to fd device.
 *
 * Iommu attach have reference count protected by
 * fd device mutex.
 */
static int msm_fd_hw_attach_iommu(struct msm_fd_device *fd)
{
	int ret;

	mutex_lock(&fd->lock);

	if (fd->iommu_attached_cnt == UINT_MAX) {
		dev_err(fd->dev, "Max count reached! can not attach iommu\n");
		goto error;
	}

	if (fd->iommu_attached_cnt == 0) {
		ret = cam_smmu_get_handle(MSM_FD_SMMU_CB_NAME, &fd->iommu_hdl);
		if (ret < 0) {
			dev_err(fd->dev, "get handle failed\n");
			ret = -ENOMEM;
			goto error;
		}
		ret = cam_smmu_ops(fd->iommu_hdl, CAM_SMMU_ATTACH);
		if (ret < 0) {
			dev_err(fd->dev, "Can not attach iommu domain.\n");
			goto error_attach;
		}
	}
	fd->iommu_attached_cnt++;
	mutex_unlock(&fd->lock);

	return 0;

error_attach:
	cam_smmu_destroy_handle(fd->iommu_hdl);
error:
	mutex_unlock(&fd->lock);
	return ret;
}

/*
 * msm_fd_hw_detach_iommu - Detach iommu from face detection engine.
 * @fd: Pointer to fd device.
 *
 * Iommu detach have reference count protected by
 * fd device mutex.
 */
static void msm_fd_hw_detach_iommu(struct msm_fd_device *fd)
{
	mutex_lock(&fd->lock);
	if (fd->iommu_attached_cnt == 0) {
		dev_err(fd->dev, "There is no attached device\n");
		mutex_unlock(&fd->lock);
		return;
	}
	if (--fd->iommu_attached_cnt == 0) {
		cam_smmu_ops(fd->iommu_hdl, CAM_SMMU_DETACH);
		cam_smmu_destroy_handle(fd->iommu_hdl);
	}
	mutex_unlock(&fd->lock);
}

/*
 * msm_fd_hw_map_buffer - Map buffer to fd hw mmu.
 * @pool: Pointer to fd memory pool.
 * @fd: Ion fd.
 * @buf: Fd buffer handle, for storing mapped buffer information.
 *
 * It will map ion fd to fd hw mmu.
 */
int msm_fd_hw_map_buffer(struct msm_fd_mem_pool *pool, int fd,
	struct msm_fd_buf_handle *buf)
{
	int ret;

	if (!pool || fd < 0)
		return -EINVAL;

	ret = msm_fd_hw_attach_iommu(pool->fd_device);
	if (ret < 0)
		return -ENOMEM;

	buf->pool = pool;
	buf->fd = fd;
	ret = cam_smmu_get_phy_addr(pool->fd_device->iommu_hdl,
			buf->fd, CAM_SMMU_MAP_RW,
			&buf->addr, &buf->size);
	if (ret < 0) {
		pr_err("Error: cannot get phy addr\n");
		return -ENOMEM;
	}
	return buf->size;
}

/*
 * msm_fd_hw_unmap_buffer - Unmap buffer from fd hw mmu.
 * @buf: Fd buffer handle, for storing mapped buffer information.
 */
void msm_fd_hw_unmap_buffer(struct msm_fd_buf_handle *buf)
{
	if (buf->size) {
		cam_smmu_put_phy_addr(buf->pool->fd_device->iommu_hdl,
			buf->fd);
		msm_fd_hw_detach_iommu(buf->pool->fd_device);
	}

	buf->fd = -1;
	buf->pool = NULL;
}

/*
 * msm_fd_hw_enable - Configure and enable fd hw.
 * @fd: Fd device.
 * @buffer: Buffer need to be processed.
 *
 * Configure and starts fd processing with given buffer.
 * NOTE: Fd will not be enabled if engine is in running state.
 */
static int msm_fd_hw_enable(struct msm_fd_device *fd,
	struct msm_fd_buffer *buffer)
{
	struct msm_fd_buf_handle *buf_handle =
		buffer->vb.planes[0].mem_priv;

	if (msm_fd_hw_is_runnig(fd)) {
		dev_err(fd->dev, "Device is busy we can not enable\n");
		return 0;
	}

	msm_fd_hw_srst(fd);
	msm_fd_hw_set_size_mode(fd, buffer->format.size->reg_val);
	msm_fd_hw_set_crop(fd, &buffer->format.crop);
	msm_fd_hw_set_bytesperline(fd, buffer->format.bytesperline);
	msm_fd_hw_set_image_addr(fd, buf_handle->addr);
	msm_fd_hw_set_work_addr(fd, buffer->work_addr);
	msm_fd_hw_set_min_face(fd, buffer->settings.min_size_index);
	msm_fd_hw_set_threshold(fd, buffer->settings.threshold);
	msm_fd_hw_set_direction_angle(fd, buffer->settings.direction_index,
		buffer->settings.angle_index);
	msm_fd_hw_run(fd);
	return 1;
}

/*
 * msm_fd_hw_try_enable - Try to enable fd hw.
 * @fd: Fd device.
 * @buffer: Buffer need to be processed.
 * @state: Enable on device state
 *
 * It will enable fd hw if actual device state is equal with state argument.
 */
static int msm_fd_hw_try_enable(struct msm_fd_device *fd,
	struct msm_fd_buffer *buffer, enum msm_fd_device_state state)
{
	int enabled = 0;

	if (state == fd->state) {

		fd->state = MSM_FD_DEVICE_RUNNING;
		atomic_set(&buffer->active, 1);

		msm_fd_hw_enable(fd, buffer);
		enabled = 1;
	}
	return enabled;
}

/*
 * msm_fd_hw_next_buffer - Get next buffer from fd device processing queue.
 * @fd: Fd device.
 */
static struct msm_fd_buffer *msm_fd_hw_next_buffer(struct msm_fd_device *fd)
{
	struct msm_fd_buffer *buffer = NULL;

	if (!list_empty(&fd->buf_queue))
		buffer = list_first_entry(&fd->buf_queue,
			struct msm_fd_buffer, list);

	return buffer;
}

/*
 * msm_fd_hw_add_buffer - Add buffer to fd device processing queue.
 * @fd: Fd device.
 */
void msm_fd_hw_add_buffer(struct msm_fd_device *fd,
	struct msm_fd_buffer *buffer)
{
	spin_lock(&fd->slock);

	atomic_set(&buffer->active, 0);
	init_completion(&buffer->completion);

	INIT_LIST_HEAD(&buffer->list);
	list_add_tail(&buffer->list, &fd->buf_queue);
	spin_unlock(&fd->slock);
}

/*
 * msm_fd_hw_remove_buffers_from_queue - Removes buffer from
 *  fd device processing queue.
 * @fd: Fd device.
 */
void msm_fd_hw_remove_buffers_from_queue(struct msm_fd_device *fd,
	struct vb2_queue *vb2_q)
{
	struct msm_fd_buffer *curr_buff;
	struct msm_fd_buffer *temp;
	struct msm_fd_buffer *active_buffer;
	unsigned long time;

	spin_lock(&fd->slock);

	active_buffer = NULL;
	list_for_each_entry_safe(curr_buff, temp, &fd->buf_queue, list) {
		if (curr_buff->vb.vb2_queue == vb2_q) {

			if (atomic_read(&curr_buff->active))
				active_buffer = curr_buff;
			else
				list_del(&curr_buff->list);

		}
	}
	spin_unlock(&fd->slock);

	/* We need to wait active buffer to finish */
	if (active_buffer) {
		time = wait_for_completion_timeout(&active_buffer->completion,
			msecs_to_jiffies(MSM_FD_PROCESSING_TIMEOUT_MS));
		if (!time) {
			/* Remove active buffer */
			msm_fd_hw_get_active_buffer(fd);
			/* Schedule if other buffers are present in device */
			msm_fd_hw_schedule_next_buffer(fd);
		}
	}

	return;
}

/*
 * msm_fd_hw_buffer_done - Mark as done and removes from processing queue.
 * @fd: Fd device.
 * @buffer: Fd buffer.
 */
int msm_fd_hw_buffer_done(struct msm_fd_device *fd,
	struct msm_fd_buffer *buffer)
{
	int ret = 0;

	spin_lock(&fd->slock);

	if (atomic_read(&buffer->active)) {
		atomic_set(&buffer->active, 0);
		complete_all(&buffer->completion);
	} else {
		dev_err(fd->dev, "Buffer is not active\n");
		ret = -1;
	}

	spin_unlock(&fd->slock);

	return ret;
}

/*
 * msm_fd_hw_get_active_buffer - Get active buffer from fd processing queue.
 * @fd: Fd device.
 */
struct msm_fd_buffer *msm_fd_hw_get_active_buffer(struct msm_fd_device *fd)
{
	struct msm_fd_buffer *buffer = NULL;

	spin_lock(&fd->slock);
	if (!list_empty(&fd->buf_queue)) {
		buffer = list_first_entry(&fd->buf_queue,
			struct msm_fd_buffer, list);
		list_del(&buffer->list);
	}
	spin_unlock(&fd->slock);

	return buffer;
}

/*
 * msm_fd_hw_schedule_and_start - Schedule active buffer and start processing.
 * @fd: Fd device.
 *
 * This can be executed only when device is in idle state.
 */
int msm_fd_hw_schedule_and_start(struct msm_fd_device *fd)
{
	struct msm_fd_buffer *buf;

	spin_lock(&fd->slock);
	buf = msm_fd_hw_next_buffer(fd);
	if (buf)
		msm_fd_hw_try_enable(fd, buf, MSM_FD_DEVICE_IDLE);

	spin_unlock(&fd->slock);

	return 0;
}

/*
 * msm_fd_hw_schedule_next_buffer - Schedule next buffer and start processing.
 * @fd: Fd device.
 *
 * NOTE: This can be executed only when device is in running state.
 */
int msm_fd_hw_schedule_next_buffer(struct msm_fd_device *fd)
{
	struct msm_fd_buffer *buf;
	int ret;

	spin_lock(&fd->slock);

	/* We can schedule next buffer only in running state */
	if (fd->state != MSM_FD_DEVICE_RUNNING) {
		dev_err(fd->dev, "Can not schedule next buffer\n");
		spin_unlock(&fd->slock);
		return -EBUSY;
	}

	buf = msm_fd_hw_next_buffer(fd);
	if (buf) {
		ret = msm_fd_hw_try_enable(fd, buf, MSM_FD_DEVICE_RUNNING);
		if (0 == ret) {
			dev_err(fd->dev, "Ouch can not process next buffer\n");
			spin_unlock(&fd->slock);
			return -EBUSY;
		}
	} else {
		fd->state = MSM_FD_DEVICE_IDLE;
	}
	spin_unlock(&fd->slock);

	return 0;
}
