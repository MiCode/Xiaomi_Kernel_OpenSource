// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/platform_data/mtk_ccd_controls.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>

#include "mtk_ccd_rpmsg_internal.h"

int ccd_ipi_register(struct platform_device *pdev,
		     enum ccd_ipi_id id,
		     ccd_ipi_handler_t handler,
		     void *priv)
{
	struct mtk_ccd *ccd = platform_get_drvdata(pdev);

	if (!ccd) {
		dev_err(&pdev->dev, "ccd device is not ready\n");
		return -EPROBE_DEFER;
	}

	dev_info(ccd->dev, "ipi id: %d\n", id);

	return 0;
}
EXPORT_SYMBOL_GPL(ccd_ipi_register);

void ccd_ipi_unregister(struct platform_device *pdev, enum ccd_ipi_id id)
{
	struct mtk_ccd *ccd = platform_get_drvdata(pdev);

	if (!ccd)
		return;

	if (WARN_ON(id < 0) || WARN_ON(id >= CCD_IPI_MAX))
		return;
}
EXPORT_SYMBOL_GPL(ccd_ipi_unregister);

int rpmsg_ccd_ipi_send(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
		       struct mtk_ccd_rpmsg_endpoint *mept,
		       void *buf, unsigned int len, unsigned int wait)
{
	struct mtk_ccd *ccd = platform_get_drvdata(mtk_subdev->pdev);
	struct mtk_ccd_params *ccd_params = kzalloc(sizeof(*ccd_params),
						    GFP_KERNEL);
	int ret = 0;

	ccd_params->worker_obj.src = mept->mchinfo.chinfo.src;
	ccd_params->worker_obj.id = mept->mchinfo.id;

	/* TBD: Allocate shared memory for additional buffer
	 * If no buffer ready now, wait or not depending on parameter
	 */
#if MTK_CCD_ALLOC_ADDITIONAL_BUF
	if (!ccd_params->data && !wait)
		return -ENOMEM;

	while (!ccd_params->data) {
		dev_info(&mtk_subdev->pdev->dev, "wait buffer ready\n");
		ret = wait_event_interruptible_timeout
					(mept->ccd_paramswq,
					 atomic_read(&mept->ccd_params_rdy),
					 msecs_to_jiffies(15000));

		/* TBD: Allocate shared memory for "*buf" content
		 * If no buffer ready now, wait or not depending on parameter
		 * buffer_get();
		 */

		atomic_set(&mept->ccd_params_rdy, 0);
		/* timeout ? */
		if (!ret) {
			dev_err(&mtk_subdev->pdev->dev,
				"timeout waiting for ccd params\n");
			return -ERESTARTSYS;
		}
	}
#endif
	if (len)
		memcpy(ccd_params->worker_obj.sbuf, buf, len);

	ccd_params->worker_obj.len = len;

	/* No need to use spin_lock_irqsave for all non-irq context */
	spin_lock(&mept->pending_sendq.queue_lock);
	list_add_tail(&ccd_params->list_entry, &mept->pending_sendq.queue);
	atomic_inc(&mept->ccd_cmd_sent);
	spin_unlock(&mept->pending_sendq.queue_lock);

	if (atomic_read(&mept->worker_read_rdy))
		wake_up(&mept->worker_readwq);

	dev_info(ccd->dev, "%s: ccd: %p id: %d\n",
		 __func__, ccd, mept->mchinfo.id);
	return ret;
}
EXPORT_SYMBOL_GPL(rpmsg_ccd_ipi_send);

void ccd_master_destroy(struct mtk_ccd *ccd,
			struct ccd_master_status_item *master_obj)
{
	int id;
	struct rpmsg_endpoint *ept;
	struct mtk_rpmsg_device *srcmdev;
	struct mtk_ccd_rpmsg_endpoint *mept;
	struct list_head mchinfo_list;
	struct mtk_ccd_mchinfo_entry *mchinfo_item;
	struct mtk_ccd_mchinfo_entry *mchinfo_tmp;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev =
		to_mtk_subdev(ccd->rpmsg_subdev);

	dev_info(&mtk_subdev->pdev->dev, "%s, master_obj: %d\n",
		 __func__, master_obj->state);

	INIT_LIST_HEAD(&mchinfo_list);

	/* use the src addr to fetch the callback of the appropriate user */
	mutex_lock(&mtk_subdev->endpoints_lock);
	idr_for_each_entry(&mtk_subdev->endpoints, srcmdev, id) {
		if (id == MTK_CCD_MSGDEV_ADDR)
			continue;

		ept = srcmdev->rpdev.ept;
		/* let's make sure no one deallocates ept while we use it */
		if (ept)
			kref_get(&ept->refcount);

		dev_info(&mtk_subdev->pdev->dev, "%s, src: %d, ept: %p\n",
			 __func__, id, ept);

		if (ept) {
			/* make sure ept->cb doesn't go away while we use it */
			mutex_lock(&ept->cb_lock);

			if (ept->cb)
				ept->cb(ept->rpdev, NULL, 0, ept->priv, -1);

			mutex_unlock(&ept->cb_lock);

			/* farewell, ept, we don't need you anymore */
			kref_put(&ept->refcount, __ept_release);

			mept = to_mtk_rpmsg_endpoint(ept);
			mchinfo_item = kzalloc(sizeof(*mchinfo_item),
					       GFP_KERNEL);
			mchinfo_item->mchinfo = &mept->mchinfo;
			list_add_tail(&mchinfo_item->list_entry, &mchinfo_list);
		} else {
			dev_warn(&mtk_subdev->pdev->dev,
				 "msg received with no recipient\n");
		}
	}
	mutex_unlock(&mtk_subdev->endpoints_lock);

	list_for_each_entry_safe(mchinfo_item, mchinfo_tmp,
				 &mchinfo_list, list_entry) {
		mtk_rpmsg_destroy_rpmsgdev(mtk_subdev,
					   &mchinfo_item->mchinfo->chinfo);
		kfree(mchinfo_item);
	}
}
EXPORT_SYMBOL_GPL(ccd_master_destroy);

void ccd_master_listen(struct mtk_ccd *ccd,
		       struct ccd_master_listen_item *listen_obj)
{
	int ret;
	u32 listen_obj_rdy;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev =
		to_mtk_subdev(ccd->rpmsg_subdev);

	mutex_lock(&mtk_subdev->master_listen_lock);

	listen_obj_rdy = atomic_read(&mtk_subdev->listen_obj_rdy);
	if (listen_obj_rdy == CCD_LISTEN_OBJECT_PREPARING) {
		mutex_unlock(&mtk_subdev->master_listen_lock);
		ret = wait_event_interruptible
			(mtk_subdev->master_listen_wq,
			 (atomic_read(&mtk_subdev->listen_obj_rdy) ==
			 CCD_LISTEN_OBJECT_READY));
		if (ret != 0) {
			dev_err(ccd->dev,
				"master listen wait error: %d\n", ret);
			return;
		}
		mutex_lock(&mtk_subdev->master_listen_lock);
	}

	/* Could be memory copied directly */
	memcpy(listen_obj->name, mtk_subdev->listen_obj.name,
	       sizeof(listen_obj->name));
	listen_obj->src = mtk_subdev->listen_obj.src;
	listen_obj->cmd = mtk_subdev->listen_obj.cmd;

	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_PREPARING);
	wake_up(&mtk_subdev->ccd_listen_wq);
	mutex_unlock(&mtk_subdev->master_listen_lock);

	dev_info(ccd->dev, "%s, src: %d\n", __func__,
		 mtk_subdev->listen_obj.src);
}
EXPORT_SYMBOL_GPL(ccd_master_listen);

void ccd_worker_read(struct mtk_ccd *ccd,
		     struct ccd_worker_item *read_obj)
{
	int ret;
	struct mtk_ccd_params *ccd_params;
	struct mtk_rpmsg_device *srcmdev;
	struct mtk_ccd_rpmsg_endpoint *mept;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev =
		to_mtk_subdev(ccd->rpmsg_subdev);

	dev_info(ccd->dev, "%s, src: %d, %p\n", __func__,
		 read_obj->src, mtk_subdev);

	/* use the src addr to fetch the callback of the appropriate user */
	mutex_lock(&mtk_subdev->endpoints_lock);
	srcmdev = idr_find(&mtk_subdev->endpoints, read_obj->src);
	if (!srcmdev) {
		dev_err(ccd->dev, "src ept is not exist\n");
		mutex_unlock(&mtk_subdev->endpoints_lock);
		return;
	}

	if (!srcmdev->rpdev.ept) {
		dev_err(ccd->dev, "src ept is not ready\n");
		mutex_unlock(&mtk_subdev->endpoints_lock);
		return;
	}
	kref_get(&srcmdev->rpdev.ept->refcount);
	mutex_unlock(&mtk_subdev->endpoints_lock);

	mept = to_mtk_rpmsg_endpoint(srcmdev->rpdev.ept);
	dev_info(ccd->dev, "mept: %p src: %d id: %d\n",
		 mept, mept->mchinfo.chinfo.src, mept->mchinfo.id);

	if (atomic_read(&mept->ccd_mep_state) == CCD_MENDPOINT_DESTROY) {
		dev_info(ccd->dev, "mept: %p src: %d is destroyed\n",
			 mept, mept->mchinfo.chinfo.src);
		goto err_ret;
	}

	if (atomic_read(&mept->ccd_cmd_sent) == 0) {
		atomic_set(&mept->worker_read_rdy, 1);
		ret = wait_event_interruptible
			(mept->worker_readwq,
			 (atomic_read(&mept->ccd_cmd_sent) > 0) ||
			 (atomic_read(&mept->ccd_mep_state) !=
			 CCD_MENDPOINT_CREATED));

		atomic_set(&mept->worker_read_rdy, 0);
		if (ret != 0) {
			dev_err(ccd->dev,
				"worker read wait error: %d\n", ret);
			goto err_ret;
		}
	}

	if (atomic_read(&mept->ccd_mep_state) == CCD_MENDPOINT_DESTROY) {
		dev_info(ccd->dev, "mept: %p src: %d would destroy\n",
			 mept, mept->mchinfo.chinfo.src);
		goto err_ret;
	}

	spin_lock(&mept->pending_sendq.queue_lock);
	ccd_params = list_first_entry(&mept->pending_sendq.queue,
				      struct mtk_ccd_params,
				      list_entry);
	list_del(&ccd_params->list_entry);
	atomic_dec(&mept->ccd_cmd_sent);
	spin_unlock(&mept->pending_sendq.queue_lock);

	memcpy(read_obj, &ccd_params->worker_obj, sizeof(*read_obj));
	kfree(ccd_params);
err_ret:
	kref_put(&mept->ept.refcount, __ept_release);
}
EXPORT_SYMBOL_GPL(ccd_worker_read);

void ccd_worker_write(struct mtk_ccd *ccd,
		      struct ccd_worker_item *write_obj)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev =
		to_mtk_subdev(ccd->rpmsg_subdev);
	struct rpmsg_endpoint *ept;
	struct mtk_rpmsg_device *srcmdev;
	struct mtk_ccd_rpmsg_endpoint *mept;

	mutex_lock(&mtk_subdev->endpoints_lock);
	srcmdev = idr_find(&mtk_subdev->endpoints, write_obj->src);
	if (!srcmdev) {
		dev_err(ccd->dev, "src ept is not exist\n");
		mutex_unlock(&mtk_subdev->endpoints_lock);
		return;
	}

	if (!srcmdev->rpdev.ept) {
		dev_err(ccd->dev, "src ept is not ready\n");
		mutex_unlock(&mtk_subdev->endpoints_lock);
		return;
	}
	kref_get(&srcmdev->rpdev.ept->refcount);
	mutex_unlock(&mtk_subdev->endpoints_lock);

	mept = to_mtk_rpmsg_endpoint(srcmdev->rpdev.ept);
	dev_info(ccd->dev, "mept: %p src: %d id: %d\n",
		 mept, mept->mchinfo.chinfo.src, mept->mchinfo.id);

	if (atomic_read(&mept->ccd_mep_state) == CCD_MENDPOINT_DESTROY) {
		dev_info(ccd->dev, "mept: %p src: %d is destroyed\n",
			 mept, mept->mchinfo.chinfo.src);
		goto err_ret;
	}

	ept = srcmdev->rpdev.ept;

	dev_info(ccd->dev, "%s, src: %d, ept: %p\n", __func__,
		 write_obj->src, ept);

	mutex_lock(&ept->cb_lock);

	if (ept->cb)
		ept->cb(ept->rpdev, write_obj->sbuf, write_obj->len, ept->priv,
			write_obj->src);

	mutex_unlock(&ept->cb_lock);

err_ret:
	kref_put(&mept->ept.refcount, __ept_release);
	/* TBD: Free shared memory for additional buffer
	 * If no buffer ready now, wait or not depending on parameter
	 */
}
EXPORT_SYMBOL_GPL(ccd_worker_write);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek ccd IPI interface");
