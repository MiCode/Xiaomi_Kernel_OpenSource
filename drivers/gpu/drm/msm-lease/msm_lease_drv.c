/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2017 Keith Packard <keithp@keithp.com>
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

#include <linux/of_address.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <asm/sizes.h>
#include <drm/drm_of.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_encoder.h>
#include <drm/drm_auth.h>
#include <drm/drm_ioctl.h>
#include "../drm_internal.h"

#define MAX_LEASE_OBJECT_COUNT 64

static DEFINE_MUTEX(g_lease_mutex);
static LIST_HEAD(g_lease_list);
static int (*g_master_open)(struct drm_device *, struct drm_file *);
static void (*g_master_postclose)(struct drm_device *, struct drm_file *);
static const struct file_operations *g_master_ddev_fops;
static struct drm_master *g_master_ddev_master;
static struct kref g_master_ddev_master_ref;
static bool g_master_ddev_name_overridden;

struct msm_lease {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_minor *minor;
	struct drm_master *master;
	struct list_head head;
	u32 object_ids[MAX_LEASE_OBJECT_COUNT];
	int obj_cnt;
	const char *dev_name;
};

static struct drm_driver msm_lease_driver;

static inline struct msm_lease *_find_lease_from_minor(struct drm_minor *minor)
{
	struct msm_lease *lease;

	list_for_each_entry(lease, &g_lease_list, head) {
		if (lease->minor == minor)
			return lease;
	}

	return NULL;
}

static inline struct msm_lease *_find_lease_from_node(struct device_node *node)
{
	struct msm_lease *lease;

	list_for_each_entry(lease, &g_lease_list, head) {
		if (lease->dev->of_node == node)
			return lease;
	}

	return NULL;
}

static inline bool _find_obj_id(int id, u32 *object_ids, int object_count)
{
	int i;

	for (i = 0; i < object_count; i++) {
		if (object_ids[i] == id)
			return true;
	}

	return false;
}

static inline bool _obj_is_leased(int id,
		u32 *object_ids, int object_count)
{
	struct msm_lease *lease;

	list_for_each_entry(lease, &g_lease_list, head) {
		if (_find_obj_id(id, lease->object_ids, lease->obj_cnt))
			return true;
	}

	return _find_obj_id(id, object_ids, object_count);
}

static struct drm_master *msm_lease_get_dev_master(struct drm_device *dev)
{
	if (!g_master_ddev_master) {
		mutex_lock(&dev->master_mutex);

		if (dev->master) {
			DRM_ERROR("card0 master already opened\n");
			goto out;
		}

		g_master_ddev_master = drm_master_create(dev);
		if (!g_master_ddev_master) {
			DRM_ERROR("failed to create dev master\n");
			goto out;
		}

		dev->master = g_master_ddev_master;
		kref_init(&g_master_ddev_master_ref);

out:
		mutex_unlock(&dev->master_mutex);
	} else
		kref_get(&g_master_ddev_master_ref);

	return g_master_ddev_master;
}

static void msm_lease_destroy_dev_master(struct kref *kref)
{
	struct drm_device *dev = g_master_ddev_master->dev;

	mutex_lock(&dev->master_mutex);
	drm_master_put(&dev->master);
	mutex_unlock(&dev->master_mutex);

	g_master_ddev_master = NULL;
}

static void msm_lease_put_dev_master(struct drm_device *dev)
{
	if (!g_master_ddev_master) {
		DRM_ERROR("global master deosn't exist\n");
		return;
	}

	kref_put(&g_master_ddev_master_ref, msm_lease_destroy_dev_master);
}

static const char *msm_lease_get_dev_name(struct drm_file *file)
{
	struct msm_lease *lease;
	const char *dev_name;

	mutex_lock(&g_lease_mutex);

	lease = _find_lease_from_minor(file->minor);
	if (!lease || !lease->dev_name) {
		if (file->minor->index == 0 && g_master_ddev_name_overridden)
			dev_name = "n/a";
		else
			dev_name = file->minor->dev->driver->name;
	} else
		dev_name = lease->dev_name;

	mutex_unlock(&g_lease_mutex);
	return dev_name;
}

static int msm_lease_open(struct drm_device *dev, struct drm_file *file)
{
	struct msm_lease *lease;
	struct drm_master *lessee;
	struct drm_master *dev_master;
	struct idr leases;
	int id, i, rc;

	rc = g_master_open(dev, file);
	if (rc)
		return rc;

	mutex_lock(&g_lease_mutex);

	lease = _find_lease_from_minor(file->minor);
	if (!lease)
		goto out;

	if (!lease->master) {
		/* get device master */
		dev_master = msm_lease_get_dev_master(dev);
		if (!dev_master) {
			rc = -EBUSY;
			goto out;
		}

		/* create local idr */
		idr_init(&leases);
		for (i = 0; i < lease->obj_cnt; i++) {
			id = idr_alloc(&leases, lease,
				lease->object_ids[i],
				lease->object_ids[i] + 1, GFP_KERNEL);
			if (id < 0) {
				msm_lease_put_dev_master(dev);
				DRM_ERROR("create idr failed\n");
				rc = id;
				goto out;
			}
		}

		/* create lessee master */
		lessee = drm_master_create(dev);
		if (!lessee) {
			msm_lease_put_dev_master(dev);
			DRM_ERROR("drm_master_create failed\n");
			idr_destroy(&leases);
			rc = -ENOMEM;
			goto out;
		}

		/* create lessee id */
		mutex_lock(&dev->mode_config.idr_mutex);
		id = idr_alloc(&dev_master->lessee_idr,
				lessee, 1, 0, GFP_KERNEL);
		if (id < 0) {
			mutex_unlock(&dev->mode_config.idr_mutex);
			msm_lease_put_dev_master(dev);
			idr_destroy(&leases);
			drm_master_put(&lessee);
			rc = id;
			goto out;
		}

		/* init lessee */
		lessee->lessee_id = id;
		lessee->lessor = drm_master_get(dev_master);
		list_add_tail(&lessee->lessee_list, &dev_master->lessees);
		lessee->leases = leases;
		mutex_unlock(&dev->mode_config.idr_mutex);

		/* set file as master */
		file->master = lessee;
		file->is_master = 1;
		file->authenticated = 1;
		lease->master = drm_master_get(lessee);
	} else
		file->master = drm_master_get(lease->master);

out:
	mutex_unlock(&g_lease_mutex);

	return rc;
}

static void msm_lease_postclose(struct drm_device *dev, struct drm_file *file)
{
	struct msm_lease *lease;

	g_master_postclose(dev, file);

	mutex_lock(&g_lease_mutex);

	lease = _find_lease_from_minor(file->minor);
	if (!lease)
		goto out;

	if (drm_is_current_master(file)) {
		drm_master_put(&lease->master);
		msm_lease_put_dev_master(dev);
	}

	drm_master_release(file);

out:
	mutex_unlock(&g_lease_mutex);
}

static long msm_lease_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	if (cmd == DRM_IOCTL_VERSION) {
		const char *dev_name;
		struct drm_version v;
		u32 name_len;
		long err;

		dev_name = msm_lease_get_dev_name(filp->private_data);
		if (!dev_name)
			return -EFAULT;

		if (copy_from_user(&v, (void __user *)arg, sizeof(v)))
			return -EFAULT;

		name_len = v.name_len;

		err = drm_ioctl_kernel(filp, drm_version, &v,
			DRM_UNLOCKED|DRM_RENDER_ALLOW|DRM_CONTROL_ALLOW);
		if (err)
			return err;

		/* replace device name with card name */
		v.name_len = strlen(dev_name);
		if (v.name_len < name_len)
			name_len = v.name_len;

		if (v.name && name_len)
			if (copy_to_user(v.name, dev_name, name_len))
				return -EFAULT;

		if (copy_to_user((void __user *)arg, &v, sizeof(v)))
			return -EFAULT;

		return 0;
	}

	return g_master_ddev_fops->unlocked_ioctl(filp, cmd, arg);
}

static int msm_lease_add_connector(struct drm_device *dev, const char *name,
		u32 *object_ids, int *object_count)
{
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_crtc *crtc;
	struct drm_connector_list_iter conn_iter;
	int conn_id = -1, crtc_id = -1;
	int rc = 0;

	if (*object_count >= MAX_LEASE_OBJECT_COUNT - 1) {
		DRM_ERROR("too many objects added %d\n", *object_count);
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (!strcmp(connector->name, name)) {
			conn_id = connector->base.id;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (conn_id < 0) {
		DRM_ERROR("failed to find connector %s, defer...\n", name);
		rc = -EPROBE_DEFER;
		goto out;
	}

	if (_obj_is_leased(conn_id, object_ids, *object_count)) {
		DRM_ERROR("connector %s is already leased\n", name);
		rc = -EBUSY;
		goto out;
	}

	encoder = drm_encoder_find(dev, NULL, connector->encoder_ids[0]);
	if (!encoder) {
		DRM_ERROR("failed to find encoder for %s, defer...\n", name);
		rc = -EPROBE_DEFER;
		goto out;
	}

	drm_for_each_crtc(crtc, dev) {
		if (!(encoder->possible_crtcs & drm_crtc_mask(crtc)))
			continue;

		if (_obj_is_leased(crtc->base.id, object_ids, *object_count))
			continue;

		crtc_id = crtc->base.id;
		break;
	}

	if (crtc_id < 0) {
		DRM_ERROR("failed to find crtc for %s, defer...\n", name);
		rc = -EPROBE_DEFER;
		goto out;
	}

	object_ids[(*object_count)++] = conn_id;
	object_ids[(*object_count)++] = crtc_id;

out:
	mutex_unlock(&dev->mode_config.mutex);

	return rc;
}

static int msm_lease_add_plane(struct drm_device *dev, const char *name,
		u32 *object_ids, int *object_count)
{
	struct drm_plane *plane, *added_plane;
	int plane_id = -1;

	if (*object_count >= MAX_LEASE_OBJECT_COUNT) {
		DRM_ERROR("too many objects %d\n", *object_count);
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);
	drm_for_each_plane(plane, dev) {
		if (!strcmp(plane->name, name)) {
			plane_id = plane->base.id;
			added_plane = plane;
			break;
		}
	}
	mutex_unlock(&dev->mode_config.mutex);

	if (_obj_is_leased(plane_id, object_ids, *object_count)) {
		DRM_ERROR("plane %s is already leased\n", name);
		return -EBUSY;
	}

	if (plane_id < 0) {
		DRM_ERROR("failed to find plane for %s, defer...\n", name);
		return -EPROBE_DEFER;
	}

	object_ids[(*object_count)++] = plane_id;

	return 0;
}

static void msm_lease_fixup_crtc_primary(struct drm_device *dev,
	u32 *object_ids, int object_count)
{
	struct drm_mode_object *obj;
	struct drm_plane *planes[MAX_LEASE_OBJECT_COUNT];
	struct drm_crtc *crtcs[MAX_LEASE_OBJECT_COUNT];
	struct drm_plane *plane;
	struct drm_crtc *crtc;
	int i, plane_count = 0, crtc_count = 0;

	/* get all the leased crtcs and planes */
	for (i = 0; i < object_count; i++) {
		obj = drm_mode_object_find(dev, NULL, object_ids[i],
				DRM_MODE_OBJECT_ANY);
		if (!obj)
			continue;

		if (obj->type == DRM_MODE_OBJECT_PLANE)
			planes[plane_count++] = obj_to_plane(obj);
		else if (obj->type == DRM_MODE_OBJECT_CRTC)
			crtcs[crtc_count++] = obj_to_crtc(obj);
	}

	/* reset previous primary planes */
	for (i = 0; i < plane_count; i++) {
		if (planes[i]->type == DRM_PLANE_TYPE_PRIMARY) {
			drm_for_each_crtc(crtc, dev) {
				if (crtc->primary == planes[i]) {
					crtc->primary = NULL;
					planes[i]->crtc = NULL;
					break;
				}
			}
			planes[i]->type = DRM_PLANE_TYPE_OVERLAY;
			dev->mode_config.num_overlay_plane++;
		}
	}

	/* setup new primary planes */
	for (i = 0; i < crtc_count; i++) {
		if (crtcs[i]->primary) {
			crtcs[i]->primary->type = DRM_PLANE_TYPE_OVERLAY;
			dev->mode_config.num_overlay_plane++;
		}
		crtcs[i]->primary = planes[i];
		planes[i]->crtc = crtcs[i];
		planes[i]->type = DRM_PLANE_TYPE_PRIMARY;
		dev->mode_config.num_overlay_plane--;
	}

	/* assign primary planes for reset crtcs */
	drm_for_each_crtc(crtc, dev) {
		if (crtc->primary)
			continue;

		drm_for_each_plane(plane, dev) {
			if (plane->type == DRM_PLANE_TYPE_OVERLAY) {
				crtc->primary = plane;
				plane->type = DRM_PLANE_TYPE_PRIMARY;
				plane->crtc = crtc;
				dev->mode_config.num_overlay_plane--;
				break;
			}
		}
	}
}

static int msm_lease_parse_objs(struct drm_device *dev,
		struct device_node *of_node,
		u32 *object_ids, int *object_count)
{
	const char *name;
	int count, rc, i;

	count = of_property_count_strings(of_node, "qcom,lease-planes");
	if (!count) {
		DRM_ERROR("no planes found\n");
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		of_property_read_string_index(of_node, "qcom,lease-planes",
				i, &name);
		rc = msm_lease_add_plane(dev, name,
				object_ids, object_count);
		if (rc)
			return rc;
	}

	count = of_property_count_strings(of_node, "qcom,lease-connectors");
	if (!count) {
		DRM_ERROR("no connectors found\n");
		return -EINVAL;
	}

	if (count > *object_count) {
		DRM_ERROR("connectors are more than planes\n");
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		of_property_read_string_index(of_node, "qcom,lease-connectors",
				i, &name);
		rc = msm_lease_add_connector(dev, name,
				object_ids, object_count);
		if (rc)
			return rc;
	}

	return 0;
}

static int msm_lease_parse_misc(struct msm_lease *lease_drv)
{
	of_property_read_string(lease_drv->dev->of_node,
			"qcom,dev-name", &lease_drv->dev_name);

	return 0;
}

static int msm_lease_release(struct inode *inode, struct file *filp)
{
	return g_master_ddev_fops->release(inode, filp);
}

static int msm_lease_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return g_master_ddev_fops->mmap(filp, vma);
}

static const struct file_operations msm_lease_fops = {
	.owner              = THIS_MODULE,
	.open               = drm_open,
	.release            = msm_lease_release,
	.unlocked_ioctl     = msm_lease_ioctl,
	.compat_ioctl       = drm_compat_ioctl,
	.poll               = drm_poll,
	.read               = drm_read,
	.llseek             = no_llseek,
	.mmap               = msm_lease_mmap,
};

static int msm_lease_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drm_device *ddev, *master_ddev;
	struct drm_minor *minor;
	struct msm_lease *lease_drv;
	u32 object_ids[MAX_LEASE_OBJECT_COUNT];
	int object_count = 0;
	int ret;

	/* defer until primary drm is created */
	minor = drm_minor_acquire(0);
	if (IS_ERR(minor))
		return -EPROBE_DEFER;

	/* get master device */
	master_ddev = minor->dev;
	drm_minor_release(minor);
	if (!master_ddev)
		return -EPROBE_DEFER;

	mutex_lock(&g_lease_mutex);

	/* parse lease resources */
	ret = msm_lease_parse_objs(master_ddev, dev->of_node,
			object_ids, &object_count);
	if (ret)
		goto fail;

	lease_drv = devm_kzalloc(dev, sizeof(*lease_drv), GFP_KERNEL);
	if (!lease_drv)
		goto fail;

	platform_set_drvdata(pdev, lease_drv);
	lease_drv->dev = dev;
	lease_drv->drm_dev = master_ddev;

	/* parse misc options */
	msm_lease_parse_misc(lease_drv);

	/* create temporary device */
	ddev = drm_dev_alloc(&msm_lease_driver, master_ddev->dev);
	if (!ddev) {
		dev_err(dev, "failed to allocate drm_device\n");
		goto fail;
	}

	ret = drm_dev_register(ddev, 0);
	if (ret) {
		dev_err(dev, "failed to register drm device\n");
		drm_dev_unref(ddev);
		goto fail;
	}

	/* redirect minor to master dev */
	minor = ddev->primary;
	minor->dev = master_ddev;
	minor->type = -1;
	ddev->primary = NULL;

	/* unregister temporary driver */
	drm_dev_unregister(ddev);
	drm_dev_unref(ddev);

	/* update ids list */
	lease_drv->minor = minor;
	lease_drv->obj_cnt = object_count;
	memcpy(lease_drv->object_ids, object_ids, sizeof(u32) * object_count);
	list_add_tail(&lease_drv->head, &g_lease_list);

	/* fixup crtcs' primary planes */
	msm_lease_fixup_crtc_primary(master_ddev, object_ids, object_count);

	/* hook open/close function */
	if (!g_master_open && !g_master_postclose) {
		g_master_open = master_ddev->driver->open;
		g_master_postclose = master_ddev->driver->postclose;
		master_ddev->driver->open = msm_lease_open;
		master_ddev->driver->postclose = msm_lease_postclose;
	}

	/* hook ioctl function if dev_name is defined */
	if (!g_master_ddev_fops && lease_drv->dev_name) {
		g_master_ddev_fops = master_ddev->driver->fops;
		master_ddev->driver->fops = &msm_lease_fops;
	}

	/* if lease device has the same name, hide the original name */
	if (lease_drv->dev_name &&
	    !strcmp(lease_drv->dev_name, master_ddev->driver->name))
		g_master_ddev_name_overridden = true;

fail:
	mutex_unlock(&g_lease_mutex);
	return ret;
}

static int msm_lease_remove(struct platform_device *pdev)
{
	struct msm_lease *lease_drv;

	lease_drv = platform_get_drvdata(pdev);
	if (!lease_drv)
		return 0;

	mutex_lock(&g_lease_mutex);
	list_del_init(&lease_drv->head);
	mutex_unlock(&g_lease_mutex);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,sde-kms-lease" },
	{}
};
MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver msm_lease_platform_driver = {
	.probe      = msm_lease_probe,
	.remove     = msm_lease_remove,
	.driver     = {
		.name   = "msm_lease_drm",
		.of_match_table = dt_match,
	},
};

static int __init msm_lease_drm_register(void)
{
	return platform_driver_register(&msm_lease_platform_driver);
}

static void __exit msm_lease_drm_unregister(void)
{
	platform_driver_unregister(&msm_lease_platform_driver);
}

module_init(msm_lease_drm_register);
module_exit(msm_lease_drm_unregister);

MODULE_DESCRIPTION("MSM LEASE DRM Driver");
MODULE_LICENSE("GPL v2");
