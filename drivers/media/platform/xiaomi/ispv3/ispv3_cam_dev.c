/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/workqueue.h>
#include <linux/genalloc.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include "ispv3_cam_dev.h"

static int ispv3_open(struct file *file)
{
	int ret;
	struct ispv3_v4l2_dev *priv = video_drvdata(file);

	if (!priv)
		return -ENODEV;

	mutex_lock(&priv->isp_lock);
	ret = v4l2_fh_open(file);
	if (ret)
		goto end;

	priv->open_cnt++;
	mutex_unlock(&priv->isp_lock);

	dev_info(priv->dev, "ispv3 open cnt = %d", priv->open_cnt);
	return ret;

end:
	mutex_unlock(&priv->isp_lock);
	return ret;
}

static __poll_t ispv3_poll(struct file *file,
			   struct poll_table_struct *pll_table)
{
	struct ispv3_v4l2_dev *priv = video_drvdata(file);
	int ret = 0;

	poll_wait(file, &priv->wait, pll_table);

	if (atomic_read(&priv->int_sof)) {
		atomic_set(&priv->int_sof, 0);
		ret = POLLOUT | POLLWRNORM;
	}

	if (atomic_read(&priv->int_eof)) {
		atomic_set(&priv->int_eof, 0);
		ret = POLLIN | POLLRDNORM;
	}

	return ret;
}

static int ispv3_close(struct file *file)
{
	struct ispv3_v4l2_dev *priv = video_drvdata(file);

	mutex_lock(&priv->isp_lock);

	if (priv->open_cnt <= 0) {
		mutex_unlock(&priv->isp_lock);
		return -EINVAL;
	}

	priv->open_cnt--;
	v4l2_fh_release(file);
	mutex_unlock(&priv->isp_lock);

	return 0;
}

/* maps the PCIe BAR into user space for memory-like access using mmap() */
static int ispv3_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ispv3_v4l2_dev *priv = video_drvdata(file);
	struct ispv3_data *data = priv->pdata;
	unsigned long vsize;
	unsigned long paddr;
	unsigned long psize;
	unsigned long off;
	int ret;

	off = vma->vm_pgoff << PAGE_SHIFT;
	vsize = vma->vm_end - vma->vm_start;

	switch (off) {
	case ISP_MITOP_REG_MAPOFFSET:
		paddr = data->bar_res[0].start;
		psize = MITOP_REG_SIZE;
		break;
	case ISP_MIPORT_REG_MAPOFFSET:
		paddr = data->bar_res[0].start + MIPORT_REG_OFFSET;
		psize = MIPORT_REG_SIZE;
		break;
	case ISP_MITOP_OCRAM_MAPOFFSET:
		paddr = data->bar_res[1].start;
		psize = MITOP_OCRAM_SIZE;
		break;
	case ISP_MITOP_DDR_MAPOFFSET:
		paddr = data->bar_res[2].start;
		psize = MITOP_DDR_SIZE;
		break;
	default:
		return -EINVAL;

	}

	if (vsize > psize)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

	ret = io_remap_pfn_range(vma, vma->vm_start, paddr >> PAGE_SHIFT,
				 vsize, vma->vm_page_prot);

	dev_dbg(priv->dev, "vma=%ps, vma->vm_start=0x%lx, phys=0x%lx, vsize=%lu, psize=%lu, rv=%d\n",
		vma, vma->vm_start, paddr >> PAGE_SHIFT, vsize, psize, ret);

	if (ret)
		return -EAGAIN;

	return 0;
}

static struct v4l2_file_operations ispv3_v4l2_fops = {
	.owner  = THIS_MODULE,
	.open	 = ispv3_open,
	.poll	 = ispv3_poll,
	.release = ispv3_close,
	.mmap    = ispv3_mmap,
};

static int isp_v4l2_device_setup(struct ispv3_v4l2_dev *priv)
{
	int ret = 0;

	/* register v4l2 device */
	priv->v4l2_dev = kzalloc(sizeof(*priv->v4l2_dev),
		GFP_KERNEL);
	if (!priv->v4l2_dev)
		return -ENOMEM;

	ret = v4l2_device_register(priv->dev, priv->v4l2_dev);
	if (ret)
		goto v4l2_fail;

	/* register media device */
	priv->v4l2_dev->mdev = kzalloc(sizeof(*priv->v4l2_dev->mdev), GFP_KERNEL);
	if (!priv->v4l2_dev->mdev) {
		ret = -ENOMEM;
		goto v4l2_fail;
	}

	media_device_init(priv->v4l2_dev->mdev);
	priv->v4l2_dev->mdev->dev = priv->dev;
	strlcpy(priv->v4l2_dev->mdev->model, ISPV3_VNODE_NAME,
		sizeof(priv->v4l2_dev->mdev->model));

	ret = media_device_register(priv->v4l2_dev->mdev);
	if (ret)
		goto media_fail;

	/* register video device */
	priv->video = video_device_alloc();
	if (!priv->video) {
		ret = -ENOMEM;
		goto media_fail;
	}

	priv->video->v4l2_dev = priv->v4l2_dev;

	strlcpy(priv->video->name, "ispv3", sizeof(priv->video->name));
	priv->video->release = video_device_release;
	priv->video->fops = &ispv3_v4l2_fops;
	priv->video->minor = -1;
	priv->video->vfl_type = VFL_TYPE_VIDEO;
	priv->video->device_caps = V4L2_CAP_VIDEO_CAPTURE;
	ret = video_register_device(priv->video, VFL_TYPE_VIDEO, -1);
	if (ret)
		goto video_fail;

	video_set_drvdata(priv->video, priv);

	ret = media_entity_pads_init(&priv->video->entity, 0, NULL);
	if (ret)
		goto entity_fail;

	priv->video->entity.function = ISP_VNODE_DEVICE_TYPE;
	priv->video->entity.name = video_device_node_name(priv->video);

	return ret;

entity_fail:
	video_unregister_device(priv->video);
video_fail:
	video_device_release(priv->video);
	priv->video = NULL;
media_fail:
	kfree(priv->v4l2_dev->mdev);
	priv->v4l2_dev->mdev = NULL;
v4l2_fail:
	kfree(priv->v4l2_dev);
	priv->v4l2_dev = NULL;
	return ret;
}

static int ispv3_v4l2_probe(struct platform_device *pdev)
{
	struct ispv3_v4l2_dev *priv;
	struct ispv3_data *data;
	int ret = -EIO;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct ispv3_v4l2_dev),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	data = dev_get_drvdata(pdev->dev.parent);
	if (!data) {
		dev_err(&pdev->dev, "The ispv3 data struct is NULL!\n");
		return -EINVAL;
	}

	mutex_init(&priv->isp_lock);
	priv->pdata = data;
	priv->open_cnt = 0;
	priv->dev = &pdev->dev;

	platform_set_drvdata(pdev, priv);

	ret = isp_v4l2_device_setup(priv);
	if (ret)
		return ret;

	return 0;
}

static int ispv3_v4l2_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver ispv3_v4l2_driver = {
	.probe = ispv3_v4l2_probe,
	.remove = ispv3_v4l2_remove,
	.driver = {
		.name = "ispv3-v4l2",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ispv3_v4l2_driver);
MODULE_LICENSE("GPL v2");
