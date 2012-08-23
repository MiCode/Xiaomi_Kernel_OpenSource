/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/android_pmem.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <mach/clk.h>
#include <linux/pm_runtime.h>
#include <mach/msm_subsystem_map.h>
#include <media/msm/vcd_api.h>
#include <media/msm/vidc_init.h>
#include "vidc_init_internal.h"
#include "vcd_res_tracker_api.h"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define VIDC_NAME "msm_vidc_reg"

#define ERR(x...) printk(KERN_ERR x)

static struct vidc_dev *vidc_device_p;
static dev_t vidc_dev_num;
static struct class *vidc_class;
static unsigned int vidc_mmu_subsystem[] = {MSM_SUBSYSTEM_VIDEO};

static const struct file_operations vidc_fops = {
	.owner = THIS_MODULE,
	.open = NULL,
	.release = NULL,
	.unlocked_ioctl = NULL,
};

struct workqueue_struct *vidc_wq;
struct workqueue_struct *vidc_timer_wq;
static irqreturn_t vidc_isr(int irq, void *dev);
static spinlock_t vidc_spin_lock;

u32 vidc_msg_timing, vidc_msg_pmem, vidc_msg_register;

#ifdef VIDC_ENABLE_DBGFS
struct dentry *vidc_debugfs_root;

struct dentry *vidc_get_debugfs_root(void)
{
	if (vidc_debugfs_root == NULL)
		vidc_debugfs_root = debugfs_create_dir("vidc", NULL);
	return vidc_debugfs_root;
}

void vidc_debugfs_file_create(struct dentry *root, const char *name,
				u32 *var)
{
	struct dentry *vidc_debugfs_file =
		debugfs_create_u32(name, S_IRUGO | S_IWUSR, root, var);
	if (!vidc_debugfs_file)
		ERR("%s(): Error creating/opening file %s\n", __func__, name);
}
#endif

static void vidc_timer_fn(unsigned long data)
{
	unsigned long flag;
	struct vidc_timer *hw_timer = NULL;
	ERR("%s() Timer expired\n", __func__);
	spin_lock_irqsave(&vidc_spin_lock, flag);
	hw_timer = (struct vidc_timer *)data;
	list_add_tail(&hw_timer->list, &vidc_device_p->vidc_timer_queue);
	spin_unlock_irqrestore(&vidc_spin_lock, flag);
	DBG("Queue the work for timer\n");
	queue_work(vidc_timer_wq, &vidc_device_p->vidc_timer_worker);
}

static void vidc_timer_handler(struct work_struct *work)
{
	unsigned long flag = 0;
	u32 islist_empty = 0;
	struct vidc_timer *hw_timer = NULL;

	ERR("%s() Timer expired\n", __func__);
	do {
		spin_lock_irqsave(&vidc_spin_lock, flag);
		islist_empty = list_empty(&vidc_device_p->vidc_timer_queue);
		if (!islist_empty) {
			hw_timer = list_first_entry(
				&vidc_device_p->vidc_timer_queue,
				struct vidc_timer, list);
			list_del(&hw_timer->list);
		}
		spin_unlock_irqrestore(&vidc_spin_lock, flag);
		if (!islist_empty && hw_timer && hw_timer->cb_func)
			hw_timer->cb_func(hw_timer->userdata);
	} while (!islist_empty);
}

static void vidc_work_handler(struct work_struct *work)
{
	DBG("vidc_work_handler()");
	vcd_read_and_clear_interrupt();
	vcd_response_handler();
	enable_irq(vidc_device_p->irq);
	DBG("vidc_work_handler() done");
}

static DECLARE_WORK(vidc_work, vidc_work_handler);

static int __devinit vidc_720p_probe(struct platform_device *pdev)
{
	struct resource *resource;
	DBG("Enter %s()\n", __func__);

	if (pdev->id) {
		ERR("Invalid plaform device ID = %d\n", pdev->id);
		return -EINVAL;
	}
	vidc_device_p->irq = platform_get_irq(pdev, 0);
	if (unlikely(vidc_device_p->irq < 0)) {
		ERR("%s(): Invalid irq = %d\n", __func__,
					 vidc_device_p->irq);
		return -ENXIO;
	}

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!resource)) {
		ERR("%s(): Invalid resource\n", __func__);
		return -ENXIO;
	}

	vidc_device_p->phys_base = resource->start;
	vidc_device_p->virt_base = ioremap(resource->start,
	resource->end - resource->start + 1);

	if (!vidc_device_p->virt_base) {
		ERR("%s() : ioremap failed\n", __func__);
		return -ENOMEM;
	}
	vidc_device_p->device = &pdev->dev;
	mutex_init(&vidc_device_p->lock);

	vidc_wq = create_singlethread_workqueue("vidc_worker_queue");
	if (!vidc_wq) {
		ERR("%s: create workque failed\n", __func__);
		return -ENOMEM;
	}
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	return 0;
}

static int __devexit vidc_720p_remove(struct platform_device *pdev)
{
	if (pdev->id) {
		ERR("Invalid plaform device ID = %d\n", pdev->id);
		return -EINVAL;
	}
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static int vidc_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int vidc_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops vidc_dev_pm_ops = {
	.runtime_suspend = vidc_runtime_suspend,
	.runtime_resume = vidc_runtime_resume,
};

static struct platform_driver msm_vidc_720p_platform_driver = {
	.probe = vidc_720p_probe,
	.remove = vidc_720p_remove,
	.driver = {
		.name = "msm_vidc",
		.pm   = &vidc_dev_pm_ops,
	},
};

static void __exit vidc_exit(void)
{
	platform_driver_unregister(&msm_vidc_720p_platform_driver);
}

static irqreturn_t vidc_isr(int irq, void *dev)
{
	DBG("\n vidc_isr() %d ", irq);
	disable_irq_nosync(irq);
	queue_work(vidc_wq, &vidc_work);
	return IRQ_HANDLED;
}

static int __init vidc_init(void)
{
	int rc = 0;
	struct device *class_devp;
#ifdef VIDC_ENABLE_DBGFS
	struct dentry *root = NULL;
#endif

	vidc_device_p = kzalloc(sizeof(struct vidc_dev), GFP_KERNEL);
	if (!vidc_device_p) {
		ERR("%s Unable to allocate memory for vidc_dev\n",
			__func__);
		return -ENOMEM;
	}

	rc = alloc_chrdev_region(&vidc_dev_num, 0, 1, VIDC_NAME);
	if (rc < 0) {
		ERR("%s: alloc_chrdev_region Failed rc = %d\n",
			__func__, rc);
		goto error_vidc_alloc_chrdev_region;
	}

	vidc_class = class_create(THIS_MODULE, VIDC_NAME);
	if (IS_ERR(vidc_class)) {
		rc = PTR_ERR(vidc_class);
		ERR("%s: couldn't create vidc_class rc = %d\n",
		__func__, rc);

		goto error_vidc_class_create;
	}

	class_devp = device_create(vidc_class, NULL, vidc_dev_num, NULL,
					VIDC_NAME);

	if (IS_ERR(class_devp)) {
		rc = PTR_ERR(class_devp);
		ERR("%s: class device_create failed %d\n",
			__func__, rc);
		goto error_vidc_class_device_create;
	}

	cdev_init(&vidc_device_p->cdev, &vidc_fops);
	vidc_device_p->cdev.owner = THIS_MODULE;
	rc = cdev_add(&(vidc_device_p->cdev), vidc_dev_num, 1);

	if (rc < 0) {
		ERR("%s: cdev_add failed %d\n", __func__, rc);
		goto error_vidc_cdev_add;
	}

	rc = platform_driver_register(&msm_vidc_720p_platform_driver);
	if (rc) {
		ERR("%s failed to load\n", __func__);
		goto error_vidc_platfom_register;
	}

	rc = request_irq(vidc_device_p->irq, vidc_isr, IRQF_TRIGGER_HIGH,
			 "vidc", vidc_device_p->device);

	if (unlikely(rc)) {
		ERR("%s() :request_irq failed\n", __func__);
		goto error_vidc_request_irq;
	}
	res_trk_init(vidc_device_p->device, vidc_device_p->irq);
	vidc_timer_wq = create_singlethread_workqueue("vidc_timer_wq");
	if (!vidc_timer_wq) {
		ERR("%s: create workque failed\n", __func__);
		rc = -ENOMEM;
		goto error_vidc_create_workqueue;
	}
	DBG("Disabling IRQ in %s()\n", __func__);
	disable_irq_nosync(vidc_device_p->irq);
	INIT_WORK(&vidc_device_p->vidc_timer_worker,
			  vidc_timer_handler);
	spin_lock_init(&vidc_spin_lock);
	INIT_LIST_HEAD(&vidc_device_p->vidc_timer_queue);

	vidc_device_p->ref_count = 0;
	vidc_device_p->firmware_refcount = 0;
	vidc_device_p->get_firmware = 0;
#ifdef VIDC_ENABLE_DBGFS
	root = vidc_get_debugfs_root();
	if (root) {
		vidc_debugfs_file_create(root, "vidc_msg_timing",
				(u32 *) &vidc_msg_timing);
		vidc_debugfs_file_create(root, "vidc_msg_pmem",
				(u32 *) &vidc_msg_pmem);
		vidc_debugfs_file_create(root, "vidc_msg_register",
				(u32 *) &vidc_msg_register);
	}
#endif
	return 0;

error_vidc_create_workqueue:
	free_irq(vidc_device_p->irq, vidc_device_p->device);
error_vidc_request_irq:
	platform_driver_unregister(&msm_vidc_720p_platform_driver);
error_vidc_platfom_register:
	cdev_del(&(vidc_device_p->cdev));
error_vidc_cdev_add:
	device_destroy(vidc_class, vidc_dev_num);
error_vidc_class_device_create:
	class_destroy(vidc_class);
error_vidc_class_create:
	unregister_chrdev_region(vidc_dev_num, 1);
error_vidc_alloc_chrdev_region:
	kfree(vidc_device_p);

	return rc;
}

void __iomem *vidc_get_ioaddr(void)
{
	return (u8 *)vidc_device_p->virt_base;
}
EXPORT_SYMBOL(vidc_get_ioaddr);

int vidc_load_firmware(void)
{
	u32 status = true;

	if (!res_trk_check_for_sec_session()) {
		mutex_lock(&vidc_device_p->lock);
		if (!vidc_device_p->get_firmware) {
			status = res_trk_download_firmware();
			if (!status)
				goto error;
			vidc_device_p->get_firmware = 1;
		}
		vidc_device_p->firmware_refcount++;
error:
		mutex_unlock(&vidc_device_p->lock);
	}
	return status;
}
EXPORT_SYMBOL(vidc_load_firmware);

void vidc_release_firmware(void)
{
	if (!res_trk_check_for_sec_session()) {
		mutex_lock(&vidc_device_p->lock);
		if (vidc_device_p->firmware_refcount > 0)
			vidc_device_p->firmware_refcount--;
		else
			vidc_device_p->firmware_refcount = 0;
		mutex_unlock(&vidc_device_p->lock);
	}
}
EXPORT_SYMBOL(vidc_release_firmware);

u32 vidc_get_fd_info(struct video_client_ctx *client_ctx,
		enum buffer_dir buffer, int pmem_fd,
		unsigned long kvaddr, int index,
		struct ion_handle **buff_handle)
{
	struct buf_addr_table *buf_addr_table;
	u32 rc = 0;
	if (!client_ctx)
		return false;
	if (buffer == BUFFER_TYPE_INPUT)
		buf_addr_table = client_ctx->input_buf_addr_table;
	else
		buf_addr_table = client_ctx->output_buf_addr_table;
	if (buf_addr_table[index].pmem_fd == pmem_fd) {
		if (buf_addr_table[index].kernel_vaddr == kvaddr) {
			rc = buf_addr_table[index].buff_ion_flag;
			*buff_handle = buf_addr_table[index].buff_ion_handle;
		} else
			*buff_handle = NULL;
	} else
		*buff_handle = NULL;
	return rc;
}
EXPORT_SYMBOL(vidc_get_fd_info);

void vidc_cleanup_addr_table(struct video_client_ctx *client_ctx,
				enum buffer_dir buffer)
{
	u32 *num_of_buffers = NULL;
	u32 i = 0;
	struct buf_addr_table *buf_addr_table;
	if (buffer == BUFFER_TYPE_INPUT) {
		buf_addr_table = client_ctx->input_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_input_buffers;
		DBG("%s(): buffer = INPUT\n", __func__);

	} else {
		buf_addr_table = client_ctx->output_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_output_buffers;
		DBG("%s(): buffer = OUTPUT\n", __func__);
	}

	if (!*num_of_buffers)
		goto bail_out_cleanup;
	if (!client_ctx->user_ion_client)
		goto bail_out_cleanup;
	for (i = 0; i < *num_of_buffers; ++i) {
		if (buf_addr_table[i].client_data) {
			msm_subsystem_unmap_buffer(
			(struct msm_mapped_buffer *)
			buf_addr_table[i].client_data);
			buf_addr_table[i].client_data = NULL;
		}
		if (!IS_ERR_OR_NULL(buf_addr_table[i].buff_ion_handle)) {
			if (!IS_ERR_OR_NULL(client_ctx->user_ion_client)) {
				ion_unmap_kernel(client_ctx->user_ion_client,
						buf_addr_table[i].
						buff_ion_handle);
				if (!res_trk_check_for_sec_session() &&
				   (res_trk_get_core_type() !=
				   (u32)VCD_CORE_720P)) {
					ion_unmap_iommu(
						client_ctx->user_ion_client,
						buf_addr_table[i].
						buff_ion_handle,
						VIDEO_DOMAIN,
						VIDEO_MAIN_POOL);
				}
				ion_free(client_ctx->user_ion_client,
						buf_addr_table[i].
						buff_ion_handle);
				buf_addr_table[i].buff_ion_handle = NULL;
			}
		}
	}
	if (client_ctx->vcd_h264_mv_buffer.client_data) {
		msm_subsystem_unmap_buffer((struct msm_mapped_buffer *)
		client_ctx->vcd_h264_mv_buffer.client_data);
		client_ctx->vcd_h264_mv_buffer.client_data = NULL;
	}
	if (!IS_ERR_OR_NULL(client_ctx->h264_mv_ion_handle)) {
		if (!IS_ERR_OR_NULL(client_ctx->user_ion_client)) {
			ion_unmap_kernel(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle);
			if (!res_trk_check_for_sec_session() &&
			    (res_trk_get_core_type() != (u32)VCD_CORE_720P)) {
				ion_unmap_iommu(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle,
					VIDEO_DOMAIN,
					VIDEO_MAIN_POOL);
			}
			ion_free(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle);
			client_ctx->h264_mv_ion_handle = NULL;
		}
	}
bail_out_cleanup:
	return;
}
EXPORT_SYMBOL(vidc_cleanup_addr_table);

u32 vidc_lookup_addr_table(struct video_client_ctx *client_ctx,
	enum buffer_dir buffer,
	u32 search_with_user_vaddr,
	unsigned long *user_vaddr,
	unsigned long *kernel_vaddr,
	unsigned long *phy_addr, int *pmem_fd,
	struct file **file, s32 *buffer_index)
{
	u32 num_of_buffers;
	u32 i;
	struct buf_addr_table *buf_addr_table;
	u32 found = false;

	if (!client_ctx)
		return false;
	mutex_lock(&client_ctx->enrty_queue_lock);
	if (buffer == BUFFER_TYPE_INPUT) {
		buf_addr_table = client_ctx->input_buf_addr_table;
		num_of_buffers = client_ctx->num_of_input_buffers;
		DBG("%s(): buffer = INPUT\n", __func__);

	} else {
		buf_addr_table = client_ctx->output_buf_addr_table;
		num_of_buffers = client_ctx->num_of_output_buffers;
		DBG("%s(): buffer = OUTPUT\n", __func__);
	}

	for (i = 0; i < num_of_buffers; ++i) {
		if (search_with_user_vaddr) {
			if (*user_vaddr == buf_addr_table[i].user_vaddr) {
				*kernel_vaddr = buf_addr_table[i].kernel_vaddr;
				found = true;
				DBG("%s() : client_ctx = %p."
				" user_virt_addr = 0x%08lx is found",
				__func__, client_ctx, *user_vaddr);
				break;
			}
		} else {
			if (*kernel_vaddr == buf_addr_table[i].kernel_vaddr) {
				*user_vaddr = buf_addr_table[i].user_vaddr;
				found = true;
				DBG("%s() : client_ctx = %p."
				" kernel_virt_addr = 0x%08lx is found",
				__func__, client_ctx, *kernel_vaddr);
				break;
			}
		}
	}

	if (found) {
		*phy_addr = buf_addr_table[i].dev_addr;
		*pmem_fd = buf_addr_table[i].pmem_fd;
		*file = buf_addr_table[i].file;
		*buffer_index = i;

		if (search_with_user_vaddr)
			DBG("kernel_vaddr = 0x%08lx, phy_addr = 0x%08lx "
			" pmem_fd = %d, struct *file	= %p "
			"buffer_index = %d\n", *kernel_vaddr,
			*phy_addr, *pmem_fd, *file, *buffer_index);
		else
			DBG("user_vaddr = 0x%08lx, phy_addr = 0x%08lx "
			" pmem_fd = %d, struct *file	= %p "
			"buffer_index = %d\n", *user_vaddr, *phy_addr,
			*pmem_fd, *file, *buffer_index);
		mutex_unlock(&client_ctx->enrty_queue_lock);
		return true;
	} else {
		if (search_with_user_vaddr)
			DBG("%s() : client_ctx = %p user_virt_addr = 0x%08lx"
			" Not Found.\n", __func__, client_ctx, *user_vaddr);
		else
			DBG("%s() : client_ctx = %p kernel_virt_addr = 0x%08lx"
			" Not Found.\n", __func__, client_ctx,
			*kernel_vaddr);
		mutex_unlock(&client_ctx->enrty_queue_lock);
		return false;
	}
}
EXPORT_SYMBOL(vidc_lookup_addr_table);

u32 vidc_insert_addr_table(struct video_client_ctx *client_ctx,
	enum buffer_dir buffer, unsigned long user_vaddr,
	unsigned long *kernel_vaddr, int pmem_fd,
	unsigned long buffer_addr_offset, unsigned int max_num_buffers,
	unsigned long length)
{
	unsigned long len, phys_addr;
	struct file *file = NULL;
	u32 *num_of_buffers = NULL;
	u32 i, flags;
	struct buf_addr_table *buf_addr_table;
	struct msm_mapped_buffer *mapped_buffer = NULL;
	struct ion_handle *buff_ion_handle = NULL;
	unsigned long ionflag = 0;
	unsigned long iova = 0;
	int ret = 0;
	unsigned long buffer_size  = 0;
	size_t ion_len;

	if (!client_ctx || !length)
		return false;
	mutex_lock(&client_ctx->enrty_queue_lock);
	if (buffer == BUFFER_TYPE_INPUT) {
		buf_addr_table = client_ctx->input_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_input_buffers;
		DBG("%s(): buffer = INPUT #Buf = %d\n",
			__func__, *num_of_buffers);

	} else {
		buf_addr_table = client_ctx->output_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_output_buffers;
		DBG("%s(): buffer = OUTPUT #Buf = %d\n",
			__func__, *num_of_buffers);
		length = length * 2; /* workaround for iommu video h/w bug */
	}

	if (*num_of_buffers == max_num_buffers) {
		ERR("%s(): Num of buffers reached max value : %d",
			__func__, max_num_buffers);
		goto bail_out_add;
	}

	i = 0;
	while (i < *num_of_buffers &&
		user_vaddr != buf_addr_table[i].user_vaddr)
		i++;
	if (i < *num_of_buffers) {
		DBG("%s() : client_ctx = %p."
			" user_virt_addr = 0x%08lx already set",
			__func__, client_ctx, user_vaddr);
		goto bail_out_add;
	} else {
		if (!vcd_get_ion_status()) {
			if (get_pmem_file(pmem_fd, &phys_addr,
					kernel_vaddr, &len, &file)) {
				ERR("%s(): get_pmem_file failed\n", __func__);
				goto bail_out_add;
			}
			put_pmem_file(file);
			flags = (buffer == BUFFER_TYPE_INPUT)
			? MSM_SUBSYSTEM_MAP_IOVA :
			MSM_SUBSYSTEM_MAP_IOVA|MSM_SUBSYSTEM_ALIGN_IOVA_8K;
			mapped_buffer = msm_subsystem_map_buffer(phys_addr,
			length, flags, vidc_mmu_subsystem,
			sizeof(vidc_mmu_subsystem)/sizeof(unsigned int));
			if (IS_ERR(mapped_buffer)) {
				pr_err("buffer map failed");
				goto bail_out_add;
			}
			buf_addr_table[*num_of_buffers].client_data = (void *)
				mapped_buffer;
			buf_addr_table[*num_of_buffers].dev_addr =
				mapped_buffer->iova[0];
		} else {
			buff_ion_handle = ion_import_dma_buf(
				client_ctx->user_ion_client, pmem_fd);
			if (IS_ERR_OR_NULL(buff_ion_handle)) {
				ERR("%s(): get_ION_handle failed\n",
				 __func__);
				goto bail_out_add;
			}
			if (ion_handle_get_flags(client_ctx->user_ion_client,
						buff_ion_handle,
						&ionflag)) {
				ERR("%s():ION flags fail\n",
				 __func__);
				goto bail_out_add;
			}
			*kernel_vaddr = (unsigned long)
				ion_map_kernel(
				client_ctx->user_ion_client,
				buff_ion_handle,
				ionflag);
			if (IS_ERR_OR_NULL((void *)*kernel_vaddr)) {
				ERR("%s():ION virtual addr fail\n",
				 __func__);
				*kernel_vaddr = (unsigned long)NULL;
				goto ion_free_error;
			}
			if (res_trk_check_for_sec_session() ||
			   (res_trk_get_core_type() == (u32)VCD_CORE_720P)) {
				if (ion_phys(client_ctx->user_ion_client,
					buff_ion_handle,
					&phys_addr, &ion_len)) {
					ERR("%s():ION physical addr fail\n",
					__func__);
					goto ion_map_error;
				}
				len = (unsigned long) ion_len;
				buf_addr_table[*num_of_buffers].client_data =
					 NULL;
				buf_addr_table[*num_of_buffers].dev_addr =
					 phys_addr;
			} else {
				ret = ion_map_iommu(client_ctx->user_ion_client,
						buff_ion_handle,
						VIDEO_DOMAIN,
						VIDEO_MAIN_POOL,
						SZ_8K,
						length,
						(unsigned long *) &iova,
						(unsigned long *) &buffer_size,
						UNCACHED,
						ION_IOMMU_UNMAP_DELAYED);
				if (ret || !iova) {
					ERR(
					"%s():ION iommu map fail, ret = %d, iova = 0x%lx\n",
						__func__, ret, iova);
					goto ion_map_error;
				}
				phys_addr = iova;
				buf_addr_table[*num_of_buffers].client_data =
						 NULL;
				buf_addr_table[*num_of_buffers].dev_addr =
						 iova;
			}
		}
		phys_addr += buffer_addr_offset;
		(*kernel_vaddr) += buffer_addr_offset;
		buf_addr_table[*num_of_buffers].user_vaddr = user_vaddr;
		buf_addr_table[*num_of_buffers].kernel_vaddr = *kernel_vaddr;
		buf_addr_table[*num_of_buffers].pmem_fd = pmem_fd;
		buf_addr_table[*num_of_buffers].file = file;
		buf_addr_table[*num_of_buffers].phy_addr = phys_addr;
		buf_addr_table[*num_of_buffers].buff_ion_handle =
						buff_ion_handle;
		buf_addr_table[*num_of_buffers].buff_ion_flag =
						ionflag;
		*num_of_buffers = *num_of_buffers + 1;
		DBG("%s() : client_ctx = %p, user_virt_addr = 0x%08lx, "
			"kernel_vaddr = 0x%08lx phys_addr=%lu inserted!",
			__func__, client_ctx, user_vaddr, *kernel_vaddr,
			phys_addr);
	}
	mutex_unlock(&client_ctx->enrty_queue_lock);
	return true;
ion_map_error:
	if (*kernel_vaddr && buff_ion_handle)
		ion_unmap_kernel(client_ctx->user_ion_client, buff_ion_handle);
ion_free_error:
	if (!IS_ERR_OR_NULL(buff_ion_handle))
		ion_free(client_ctx->user_ion_client, buff_ion_handle);
bail_out_add:
	mutex_unlock(&client_ctx->enrty_queue_lock);
	return false;
}
EXPORT_SYMBOL(vidc_insert_addr_table);

/*
 * Similar to vidc_insert_addr_table except intended for in-kernel
 * use where buffers have already been alloced and mapped properly
 */
u32 vidc_insert_addr_table_kernel(struct video_client_ctx *client_ctx,
	enum buffer_dir buffer, unsigned long user_vaddr,
	unsigned long kernel_vaddr, unsigned long phys_addr,
	unsigned int max_num_buffers,
	unsigned long length)
{
	u32 *num_of_buffers = NULL;
	u32 i;
	struct buf_addr_table *buf_addr_table;
	struct msm_mapped_buffer *mapped_buffer = NULL;

	if (!client_ctx || !length || !kernel_vaddr || !phys_addr)
		return false;
	mutex_lock(&client_ctx->enrty_queue_lock);
	if (buffer == BUFFER_TYPE_INPUT) {
		buf_addr_table = client_ctx->input_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_input_buffers;
		DBG("%s(): buffer = INPUT #Buf = %d\n",
			__func__, *num_of_buffers);

	} else {
		buf_addr_table = client_ctx->output_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_output_buffers;
		DBG("%s(): buffer = OUTPUT #Buf = %d\n",
			__func__, *num_of_buffers);
	}

	if (*num_of_buffers == max_num_buffers) {
		ERR("%s(): Num of buffers reached max value : %d",
			__func__, max_num_buffers);
		goto bail_out_add;
	}

	i = 0;
	while (i < *num_of_buffers &&
		user_vaddr != buf_addr_table[i].user_vaddr) {
		i++;
	}
	if (i < *num_of_buffers) {
		DBG("%s() : client_ctx = %p."
			" user_virt_addr = 0x%08lx already set",
			__func__, client_ctx, user_vaddr);
		goto bail_out_add;
	} else {
		mapped_buffer = NULL;
		buf_addr_table[*num_of_buffers].client_data = (void *)
			mapped_buffer;
		buf_addr_table[*num_of_buffers].dev_addr = phys_addr;
		buf_addr_table[*num_of_buffers].user_vaddr = user_vaddr;
		buf_addr_table[*num_of_buffers].kernel_vaddr = kernel_vaddr;
		buf_addr_table[*num_of_buffers].pmem_fd = -1;
		buf_addr_table[*num_of_buffers].file = NULL;
		buf_addr_table[*num_of_buffers].phy_addr = phys_addr;
		buf_addr_table[*num_of_buffers].buff_ion_handle = NULL;
		*num_of_buffers = *num_of_buffers + 1;
		DBG("%s() : client_ctx = %p, user_virt_addr = 0x%08lx, "
			"kernel_vaddr = 0x%08lx inserted!", __func__,
			client_ctx, user_vaddr, kernel_vaddr);
	}
	mutex_unlock(&client_ctx->enrty_queue_lock);
	return true;
bail_out_add:
	mutex_unlock(&client_ctx->enrty_queue_lock);
	return false;
}
EXPORT_SYMBOL(vidc_insert_addr_table_kernel);

u32 vidc_delete_addr_table(struct video_client_ctx *client_ctx,
	enum buffer_dir buffer,
	unsigned long user_vaddr,
	unsigned long *kernel_vaddr)
{
	u32 *num_of_buffers = NULL;
	u32 i;
	struct buf_addr_table *buf_addr_table;

	if (!client_ctx)
		return false;
	mutex_lock(&client_ctx->enrty_queue_lock);
	if (buffer == BUFFER_TYPE_INPUT) {
		buf_addr_table = client_ctx->input_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_input_buffers;

	} else {
		buf_addr_table = client_ctx->output_buf_addr_table;
		num_of_buffers = &client_ctx->num_of_output_buffers;
	}

	if (!*num_of_buffers)
		goto bail_out_del;

	i = 0;
	while (i < *num_of_buffers &&
		user_vaddr != buf_addr_table[i].user_vaddr)
		i++;
	if (i == *num_of_buffers) {
		pr_err("%s() : client_ctx = %p."
			" user_virt_addr = 0x%08lx NOT found",
			__func__, client_ctx, user_vaddr);
		goto bail_out_del;
	}
	if (buf_addr_table[i].client_data) {
		msm_subsystem_unmap_buffer(
		(struct msm_mapped_buffer *)buf_addr_table[i].client_data);
		buf_addr_table[i].client_data = NULL;
	}
	*kernel_vaddr = buf_addr_table[i].kernel_vaddr;
	if (buf_addr_table[i].buff_ion_handle) {
		ion_unmap_kernel(client_ctx->user_ion_client,
				buf_addr_table[i].buff_ion_handle);
		if (!res_trk_check_for_sec_session() &&
		   (res_trk_get_core_type() != (u32)VCD_CORE_720P)) {
			ion_unmap_iommu(client_ctx->user_ion_client,
				buf_addr_table[i].buff_ion_handle,
				VIDEO_DOMAIN,
				VIDEO_MAIN_POOL);
		}
		ion_free(client_ctx->user_ion_client,
				buf_addr_table[i].buff_ion_handle);
		buf_addr_table[i].buff_ion_handle = NULL;
	}
	if (i < (*num_of_buffers - 1)) {
		buf_addr_table[i].client_data =
			buf_addr_table[*num_of_buffers - 1].client_data;
		buf_addr_table[i].dev_addr =
			buf_addr_table[*num_of_buffers - 1].dev_addr;
		buf_addr_table[i].user_vaddr =
			buf_addr_table[*num_of_buffers - 1].user_vaddr;
		buf_addr_table[i].kernel_vaddr =
			buf_addr_table[*num_of_buffers - 1].kernel_vaddr;
		buf_addr_table[i].phy_addr =
			buf_addr_table[*num_of_buffers - 1].phy_addr;
		buf_addr_table[i].pmem_fd =
			buf_addr_table[*num_of_buffers - 1].pmem_fd;
		buf_addr_table[i].file =
			buf_addr_table[*num_of_buffers - 1].file;
		buf_addr_table[i].buff_ion_handle =
			buf_addr_table[*num_of_buffers - 1].buff_ion_handle;
	}
	*num_of_buffers = *num_of_buffers - 1;
	DBG("%s() : client_ctx = %p."
		" user_virt_addr = 0x%08lx is found and deleted",
		__func__, client_ctx, user_vaddr);
	mutex_unlock(&client_ctx->enrty_queue_lock);
	return true;
bail_out_del:
	mutex_unlock(&client_ctx->enrty_queue_lock);
	return false;
}
EXPORT_SYMBOL(vidc_delete_addr_table);

u32 vidc_timer_create(void (*timer_handler)(void *),
	void *user_data, void **timer_handle)
{
	struct vidc_timer *hw_timer = NULL;
	if (!timer_handler || !timer_handle) {
		DBG("%s(): timer creation failed\n ", __func__);
		return false;
	}
	hw_timer = kzalloc(sizeof(struct vidc_timer), GFP_KERNEL);
	if (!hw_timer) {
		DBG("%s(): timer creation failed in allocation\n ", __func__);
		return false;
	}
	init_timer(&hw_timer->hw_timeout);
	hw_timer->hw_timeout.data = (unsigned long)hw_timer;
	hw_timer->hw_timeout.function = vidc_timer_fn;
	hw_timer->cb_func = timer_handler;
	hw_timer->userdata = user_data;
	*timer_handle = hw_timer;
	return true;
}
EXPORT_SYMBOL(vidc_timer_create);

void  vidc_timer_release(void *timer_handle)
{
	kfree(timer_handle);
}
EXPORT_SYMBOL(vidc_timer_release);

void  vidc_timer_start(void *timer_handle, u32 time_out)
{
	struct vidc_timer *hw_timer = (struct vidc_timer *)timer_handle;
	DBG("%s(): start timer\n ", __func__);
	if (hw_timer) {
		hw_timer->hw_timeout.expires = jiffies + 1*HZ;
		add_timer(&hw_timer->hw_timeout);
	}
}
EXPORT_SYMBOL(vidc_timer_start);

void  vidc_timer_stop(void *timer_handle)
{
	struct vidc_timer *hw_timer = (struct vidc_timer *)timer_handle;
	DBG("%s(): stop timer\n ", __func__);
	if (hw_timer)
		del_timer(&hw_timer->hw_timeout);
}
EXPORT_SYMBOL(vidc_timer_stop);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Video decoder/encoder driver Init Module");
MODULE_VERSION("1.0");
module_init(vidc_init);
module_exit(vidc_exit);
