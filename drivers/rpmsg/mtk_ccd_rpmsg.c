// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#include <uapi/linux/mtk_ccd_controls.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/platform_data/mtk_ccd.h>

#include "rpmsg_internal.h"
#include "mtk_ccd_rpmsg_internal.h"

static const struct rpmsg_endpoint_ops mtk_rpmsg_endpoint_ops;

void __ept_release(struct kref *kref)
{
	struct rpmsg_endpoint *ept = container_of(kref, struct rpmsg_endpoint,
						  refcount);
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mept->mtk_subdev;
	struct rpmsg_device *rpdev = ept->rpdev;

	dev_info(&mtk_subdev->pdev->dev, "free mtk rpmsg endpoint: %p\n",
		 mept);
	kfree(to_mtk_rpmsg_endpoint(ept));

	put_device(&rpdev->dev);
}

void mtk_rpmsg_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_ccd_rpmsg_endpoint *mept = priv;
	struct rpmsg_endpoint *ept = &mept->ept;
	int ret;

	ret = (*ept->cb)(ept->rpdev, data, len, ept->priv, ept->addr);
	if (ret)
		dev_info(&ept->rpdev->dev, "rpmsg handler return error = %d",
			 ret);
}

static struct rpmsg_endpoint *
__rpmsg_create_ept(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
		   struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		   u32 id)
{
	struct mtk_ccd_rpmsg_endpoint *mept;
	struct rpmsg_endpoint *ept;
	struct platform_device *pdev = mtk_subdev->pdev;

	mept = kzalloc(sizeof(*mept), GFP_KERNEL);
	if (!mept)
		return NULL;
	mept->mtk_subdev = mtk_subdev;

	ept = &mept->ept;
	kref_init(&ept->refcount);
	mutex_init(&ept->cb_lock);

	get_device(&rpdev->dev);
	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &mtk_rpmsg_endpoint_ops;
	ept->addr = id;

	mept->mchinfo.chinfo.src = id;
	mept->mchinfo.chinfo.dst = RPMSG_ADDR_ANY;
	mept->mchinfo.id = id;

	INIT_LIST_HEAD(&mept->pending_sendq.queue);
	spin_lock_init(&mept->pending_sendq.queue_lock);
	init_waitqueue_head(&mept->worker_readwq);
	init_waitqueue_head(&mept->ccd_paramswq);
	atomic_set(&mept->ccd_cmd_sent, 0);
	atomic_set(&mept->worker_read_rdy, 0);
	atomic_set(&mept->ccd_params_rdy, 0);
	atomic_set(&mept->ccd_mep_state, CCD_MENDPOINT_CREATED);

	dev_dbg(&pdev->dev, "%s: %d\n", __func__, ept->addr);
	return ept;
}

static struct rpmsg_endpoint *
mtk_rpmsg_create_ept(struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		     struct rpmsg_channel_info chinfo)
{
	struct mtk_ccd_rpmsg_endpoint *mept;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev =
		to_mtk_rpmsg_device(rpdev)->mtk_subdev;
	struct rpmsg_endpoint *ept =
		__rpmsg_create_ept(mtk_subdev, rpdev, cb, priv, chinfo.src);

	if (!ept)
		return NULL;

	mept = to_mtk_rpmsg_endpoint(ept);
	memcpy(mept->mchinfo.chinfo.name, chinfo.name, RPMSG_NAME_SIZE);
	return ept;
}

static void mtk_rpmsg_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct mtk_ccd_params *ccd_params;
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mept->mtk_subdev;

	dev_info(&mtk_subdev->pdev->dev,
		 "%s: src[%d] worker_read_rdy: %d, ccd_cmd_sent: %d\n",
		 __func__, ept->addr,
		 atomic_read(&mept->worker_read_rdy),
		 atomic_read(&mept->ccd_cmd_sent));

	atomic_set(&mept->ccd_mep_state, CCD_MENDPOINT_DESTROY);
	if (atomic_read(&mept->worker_read_rdy))
		wake_up(&mept->worker_readwq);

	while (atomic_read(&mept->ccd_cmd_sent) > 0) {
		dev_info(&mtk_subdev->pdev->dev, "%s: cmd_sent: %d\n",
			 __func__, atomic_read(&mept->ccd_cmd_sent));

		spin_lock(&mept->pending_sendq.queue_lock);
		ccd_params = list_first_entry(&mept->pending_sendq.queue,
					      struct mtk_ccd_params,
					      list_entry);
		list_del(&ccd_params->list_entry);
		atomic_dec(&mept->ccd_cmd_sent);
		spin_unlock(&mept->pending_sendq.queue_lock);

		/* Directly call callback to return */
		mutex_lock(&ept->cb_lock);
		if (ept->cb)
			ept->cb(ept->rpdev,
				ccd_params->worker_obj.sbuf,
				ccd_params->worker_obj.len,
				ept->priv, ept->addr);

		mutex_unlock(&ept->cb_lock);
	}

	/* make sure new inbound messages can't find this ept anymore */
	mutex_lock(&mtk_subdev->endpoints_lock);
	idr_remove(&mtk_subdev->endpoints, ept->addr);
	mutex_unlock(&mtk_subdev->endpoints_lock);

	kref_put(&ept->refcount, __ept_release);
}

static int mtk_rpmsg_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mept->mtk_subdev;

	return mtk_subdev->ops->ccd_send(mtk_subdev, mept, data, len, 0);
}

static int mtk_rpmsg_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev =
		to_mtk_rpmsg_endpoint(ept)->mtk_subdev;
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);

	/*
	 * TODO: This currently is same as mtk_rpmsg_send, and wait until SCP
	 * received the last command.
	 */
	return mtk_subdev->ops->ccd_send(mtk_subdev, mept, data, len, 0);
}

static const struct rpmsg_endpoint_ops mtk_rpmsg_endpoint_ops = {
	.destroy_ept = mtk_rpmsg_destroy_ept,
	.send = mtk_rpmsg_send,
	.trysend = mtk_rpmsg_trysend,
};

static void mtk_rpmsg_release_device(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct mtk_rpmsg_device *mdev = to_mtk_rpmsg_device(rpdev);

	dev_dbg(dev, "%s: rpdev %p\n", __func__, rpdev);

	kfree(mdev);
}

static const struct rpmsg_device_ops mtk_rpmsg_device_ops = {
	.create_ept = mtk_rpmsg_create_ept,
};

static struct device_node *
mtk_rpmsg_match_device_subnode(struct device_node *node, const char *channel)
{
	struct device_node *child;
	const char *name;
	int ret;

	for_each_available_child_of_node(node, child) {
		ret = of_property_read_string(child, "mtk,rpmsg-name", &name);
		if (ret)
			continue;

		if (strcmp(name, channel) == 0)
			return child;
	}
	return NULL;
}

int
mtk_rpmsg_destroy_rpmsgdev(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
			   struct rpmsg_channel_info *info)
{
	int ret;
	struct rpmsg_device *rpdev;
	struct device *dev = rpmsg_find_device(&mtk_subdev->pdev->dev, info);
	if (dev) {
		ret = rpmsg_unregister_device(&mtk_subdev->pdev->dev, info);
		if (ret)
			dev_info(dev, "%s:rpmsg_unregister_device failed, info->src(%x)\n",
				 __func__, info->src);

		rpdev = to_rpmsg_device(dev);
		rpmsg_destroy_ept(rpdev->ept);

		put_device(dev);
	} else {
		dev_info(dev, "%s:rpmsg_find_device failed, info->src(%x)\n",
			 __func__, info->src);
		ret = -EINVAL;
	}

	return ret;
}

int
mtk_destroy_client_msgdevice(struct rproc_subdev *subdev,
			     struct rpmsg_channel_info *info)
{
	int ret;
	u32 listen_obj_rdy;
	struct rpmsg_device *rpdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);
	struct device *dev = rpmsg_find_device(&mtk_subdev->pdev->dev, info);

	if (!dev)
		return -EINVAL;

	rpdev = to_rpmsg_device(dev);
	mutex_lock(&mtk_subdev->master_listen_lock);

	listen_obj_rdy = atomic_read(&mtk_subdev->listen_obj_rdy);
	if (listen_obj_rdy == CCD_LISTEN_OBJECT_READY) {
		mutex_unlock(&mtk_subdev->master_listen_lock);
		wait_event_interruptible_timeout
			(mtk_subdev->ccd_listen_wq,
			(atomic_read(&mtk_subdev->listen_obj_rdy) ==
			CCD_LISTEN_OBJECT_PREPARING),
			msecs_to_jiffies(400));
		mutex_lock(&mtk_subdev->master_listen_lock);
	}

	memcpy(mtk_subdev->listen_obj.name,
	       info->name, RPMSG_NAME_SIZE);
	mtk_subdev->listen_obj.src = info->src;
	mtk_subdev->listen_obj.cmd = CCD_MASTER_CMD_DESTROY;

	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_READY);
	wake_up(&mtk_subdev->master_listen_wq);
	mutex_unlock(&mtk_subdev->master_listen_lock);

	ret = mtk_rpmsg_destroy_rpmsgdev(mtk_subdev, info);

	dev_info(&mtk_subdev->pdev->dev, "%s %p\n", __func__, rpdev);

	put_device(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_destroy_client_msgdevice);

static struct mtk_rpmsg_device *
mtk_rpmsg_create_rpmsgdev(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
			  struct rpmsg_channel_info *info)
{
	struct rpmsg_device *rpdev;
	struct mtk_rpmsg_device *mdev;
	struct platform_device *pdev = mtk_subdev->pdev;
	int id_min, id_max;
	int ret;

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return NULL;

	rpdev = &mdev->rpdev;

	if (info->src == RPMSG_ADDR_ANY) {
		id_min = MTK_CCD_MSGDEV_ADDR + 1;
		id_max = 0;
	} else {
		id_min = info->src;
		id_max = info->src + 5;
	}

	dev_dbg(&pdev->dev, "%s %p, info->src(%x), id_min(%d), id_max(%d)\n",
		 __func__, rpdev, info->src, id_min, id_max);

	mutex_lock(&mtk_subdev->endpoints_lock);
	/* bind the endpoint to an rpmsg address (and allocate one if needed) */
	ret = idr_alloc(&mtk_subdev->endpoints,
			mdev, id_min, id_max, GFP_KERNEL);
	if (ret < 0) {
		dev_info(&pdev->dev, "idr_alloc failed: %d\n", ret);
		goto free_ept;
	}
	mutex_unlock(&mtk_subdev->endpoints_lock);

	mdev->mtk_subdev = mtk_subdev;
	rpdev->src = ret;
	rpdev->ops = &mtk_rpmsg_device_ops;
	rpdev->dst = info->dst;
	strncpy(rpdev->id.name, info->name, RPMSG_NAME_SIZE);

	rpdev->dev.of_node =
		mtk_rpmsg_match_device_subnode(pdev->dev.of_node, info->name);

	dev_dbg(&pdev->dev, "ccd msgdev addr: %d\n", rpdev->src);

	rpdev->dev.parent = &pdev->dev;
	rpdev->dev.release = mtk_rpmsg_release_device;

	ret = rpmsg_register_device(rpdev);
	if (ret) {
		mutex_lock(&mtk_subdev->endpoints_lock);
		idr_remove(&mtk_subdev->endpoints, info->src);
		mutex_unlock(&mtk_subdev->endpoints_lock);
		kfree(mdev);
		return NULL;
	}
	return mdev;

free_ept:
	mutex_unlock(&mtk_subdev->endpoints_lock);
	kfree(mdev);
	return NULL;
}

static int
mtk_rpmsg_create_ccd_rpmsgdev(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
			      struct rpmsg_channel_info *info)
{
	struct mtk_rpmsg_device *mdev =
		mtk_rpmsg_create_rpmsgdev(mtk_subdev, info);

	if (!mdev)
		return -ENOMEM;

	mtk_subdev->rpdev = &mdev->rpdev;
	return 0;
}

struct mtk_rpmsg_device *
mtk_create_client_msgdevice(struct rproc_subdev *subdev,
			    struct rpmsg_channel_info *info)
{
	int ret;
	u32 listen_obj_rdy;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);
	struct mtk_rpmsg_device *mdev =
		mtk_rpmsg_create_rpmsgdev(mtk_subdev, info);

	if (!mdev)
		return NULL;

	mutex_lock(&mtk_subdev->master_listen_lock);

	listen_obj_rdy = atomic_read(&mtk_subdev->listen_obj_rdy);
	if (listen_obj_rdy == CCD_LISTEN_OBJECT_READY) {
		mutex_unlock(&mtk_subdev->master_listen_lock);
		ret = wait_event_interruptible
			(mtk_subdev->ccd_listen_wq,
			 (atomic_read(&mtk_subdev->listen_obj_rdy) ==
			 CCD_LISTEN_OBJECT_PREPARING));

		if (ret != 0) {
			dev_info(&mtk_subdev->pdev->dev,
				"ccd listen wait error: %d\n", ret);
			mutex_lock(&mtk_subdev->endpoints_lock);
			idr_remove(&mtk_subdev->endpoints, info->src);
			mutex_unlock(&mtk_subdev->endpoints_lock);
			put_device(&mdev->rpdev.dev);
			return NULL;
		}

		mutex_lock(&mtk_subdev->master_listen_lock);
	}

	memcpy(mtk_subdev->listen_obj.name,
	       mdev->rpdev.id.name, RPMSG_NAME_SIZE);
	mtk_subdev->listen_obj.src = mdev->rpdev.src;
	mtk_subdev->listen_obj.cmd = CCD_MASTER_CMD_CREATE;

	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_READY);
	wake_up(&mtk_subdev->master_listen_wq);
	mutex_unlock(&mtk_subdev->master_listen_lock);

	return mdev;
}
EXPORT_SYMBOL_GPL(mtk_create_client_msgdevice);

int mtk_rpmsg_subdev_probe(struct rproc_subdev *subdev)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);

	dev_info(&mtk_subdev->pdev->dev, "%s: %p\n", __func__, mtk_subdev);
	return 0;
}

void mtk_rpmsg_subdev_remove(struct rproc_subdev *subdev)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);

	dev_info(&mtk_subdev->pdev->dev, "%s: %p\n", __func__, mtk_subdev);
}

struct rproc_subdev *
mtk_rpmsg_create_rproc_subdev(struct platform_device *pdev,
			      struct mtk_ccd_rpmsg_ops *ops)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct rpmsg_channel_info rp_info;
	int ret = 0;

	strncpy(rp_info.name, "mtk_ccd_msgdev", RPMSG_NAME_SIZE);
	rp_info.src = MTK_CCD_MSGDEV_ADDR;
	rp_info.dst = RPMSG_ADDR_ANY;

	mtk_subdev = kzalloc(sizeof(*mtk_subdev), GFP_KERNEL);
	if (!mtk_subdev)
		return NULL;

	dev_info(&pdev->dev, "%s mtk_ccd_msgdev addr: %d\n",
		 __func__, rp_info.src);

	mtk_subdev->pdev = pdev;
	mtk_subdev->ops = ops;
	mtk_subdev->ccd_msgdev_addr = rp_info.src;

	idr_init(&mtk_subdev->endpoints);
	mutex_init(&mtk_subdev->endpoints_lock);

	mutex_init(&mtk_subdev->master_listen_lock);
	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_PREPARING);
	init_waitqueue_head(&mtk_subdev->master_listen_wq);
	init_waitqueue_head(&mtk_subdev->ccd_listen_wq);

	ccd_msgdev_init();

	ret = mtk_rpmsg_create_ccd_rpmsgdev(mtk_subdev, &rp_info);

	return &mtk_subdev->subdev;
}
EXPORT_SYMBOL_GPL(mtk_rpmsg_create_rproc_subdev);

void mtk_rpmsg_destroy_rproc_subdev(struct rproc_subdev *subdev)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);

	idr_destroy(&mtk_subdev->endpoints);
	kfree(mtk_subdev);
}
EXPORT_SYMBOL_GPL(mtk_rpmsg_destroy_rproc_subdev);

static int ccd_msgdev_cb(struct rpmsg_device *rpdev, void *data,
			 int len, void *priv, u32 src)
{
	int ret = 0;
	struct mtk_rpmsg_device *mdev = to_mtk_rpmsg_device(rpdev);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mdev->mtk_subdev;
	struct mtk_rpmsg_device *srcmdev;
	struct rpmsg_endpoint *ept;

	dev_info(&mtk_subdev->pdev->dev, "%s: %d\n", __func__, src);

	/* use the src addr to fetch the callback of the appropriate user */
	mutex_lock(&mtk_subdev->endpoints_lock);
	srcmdev = idr_find(&mtk_subdev->endpoints, src);
	if (!srcmdev) {
		dev_info(&mtk_subdev->pdev->dev, "src ept is not exist\n");
		mutex_unlock(&mtk_subdev->endpoints_lock);
		return -1;
	}

	get_device(&srcmdev->rpdev.dev);
	mutex_unlock(&mtk_subdev->endpoints_lock);

	ept = srcmdev->rpdev.ept;
	/* let's make sure no one deallocates ept while we use it */
	if (ept)
		kref_get(&ept->refcount);

	dev_info(&mtk_subdev->pdev->dev, "%s, src: %d, ept: %p\n",
		 __func__, src, ept);

	if (ept) {
		/* make sure ept->cb doesn't go away while we use it */
		mutex_lock(&ept->cb_lock);

		if (ept->cb)
			ret = ept->cb(ept->rpdev, data, len, ept->priv, src);

		mutex_unlock(&ept->cb_lock);

		/* farewell, ept, we don't need you anymore */
		kref_put(&ept->refcount, __ept_release);
	} else {
		dev_info(&mtk_subdev->pdev->dev,
			 "msg received with no recipient\n");
	}

	put_device(&srcmdev->rpdev.dev);
	return ret;
}

static int ccd_msgdev_probe(struct rpmsg_device *rpmsg_device)
{
	struct rpmsg_driver *rpdrv = to_rpmsg_driver(rpmsg_device->dev.driver);
	struct mtk_rpmsg_device *mdev = to_mtk_rpmsg_device(rpmsg_device);

	mdev->mtk_subdev->rpdev = rpmsg_device;

	dev_info(&mdev->mtk_subdev->pdev->dev, "%s : %s\n", __func__,
		 rpdrv->id_table->name);
	return 0;
}

static void ccd_msgdev_remove(struct rpmsg_device *rpmsg_device)
{
	/*
	 * struct rpmsg_driver *rpdrv =
	 *	to_rpmsg_driver(rpmsg_device->dev.driver);
	 */
	pr_debug("ccd bus rpmsg_dev_remove: %p\n", rpmsg_device);
}

static struct rpmsg_device_id ccd_msgdev_id_table[] = {
	{.name = "mtk_ccd_msgdev"},
	{},
};

static struct rpmsg_driver ccd_msgdev_driver = {
	.drv = {.name = KBUILD_MODNAME},
	.id_table = ccd_msgdev_id_table,
	.probe = ccd_msgdev_probe,
	.callback = ccd_msgdev_cb,
	.remove = ccd_msgdev_remove,
};

inline int ccd_msgdev_init(void)
{
	return register_rpmsg_driver(&ccd_msgdev_driver);
}

inline void ccd_msgdev_exit(void)
{
	unregister_rpmsg_driver(&ccd_msgdev_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek ccd rpmsg driver");
