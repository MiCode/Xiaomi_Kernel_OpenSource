/* Copyright (c) 2012, 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <dt-bindings/clock/qcom,aop-qmp.h>
#include <linux/coresight.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>

#include "coresight-priv.h"

static DEFINE_MUTEX(coresight_mutex);
static struct coresight_device *curr_sink;
/**
 * struct coresight_node - elements of a path, from source to sink
 * @csdev:	Address of an element.
 * @link:	hook to the list.
 */
struct coresight_node {
	struct coresight_device *csdev;
	struct list_head link;
};

/*
 * struct coresight_path - path from source to sink
 * @path:	Address of path list.
 * @link:	hook to the list.
 */
struct coresight_path {
	struct list_head *path;
	struct list_head link;
};

static LIST_HEAD(cs_active_paths);

/*
 * When losing synchronisation a new barrier packet needs to be inserted at the
 * beginning of the data collected in a buffer.  That way the decoder knows that
 * it needs to look for another sync sequence.
 */
const u32 barrier_pkt[5] = {0x7fffffff, 0x7fffffff,
			    0x7fffffff, 0x7fffffff, 0x0};

static int coresight_id_match(struct device *dev, void *data)
{
	int trace_id, i_trace_id;
	struct coresight_device *csdev, *i_csdev;

	csdev = data;
	i_csdev = to_coresight_device(dev);

	/*
	 * No need to care about oneself and components that are not
	 * sources or not enabled
	 */
	if (i_csdev == csdev || !i_csdev->enable ||
	    i_csdev->type != CORESIGHT_DEV_TYPE_SOURCE)
		return 0;

	/* Get the source ID for both compoment */
	trace_id = source_ops(csdev)->trace_id(csdev);
	i_trace_id = source_ops(i_csdev)->trace_id(i_csdev);

	/* All you need is one */
	if (trace_id == i_trace_id)
		return 1;

	return 0;
}

static int coresight_source_is_unique(struct coresight_device *csdev)
{
	int trace_id = source_ops(csdev)->trace_id(csdev);

	/* this shouldn't happen */
	if (trace_id < 0)
		return 0;

	return !bus_for_each_dev(&coresight_bustype, NULL,
				 csdev, coresight_id_match);
}

static int coresight_reset_sink(struct device *dev, void *data)
{
	struct coresight_device *csdev = to_coresight_device(dev);

	if ((csdev->type == CORESIGHT_DEV_TYPE_SINK ||
	     csdev->type == CORESIGHT_DEV_TYPE_LINKSINK) &&
	     csdev->activated)
		csdev->activated = false;

	return 0;
}

static void coresight_reset_all_sink(void)
{
	bus_for_each_dev(&coresight_bustype, NULL, NULL, coresight_reset_sink);
}

static int coresight_find_link_inport(struct coresight_device *csdev,
				      struct coresight_device *parent)
{
	int i;
	struct coresight_connection *conn;

	for (i = 0; i < parent->nr_outport; i++) {
		conn = &parent->conns[i];
		if (conn->child_dev == csdev)
			return conn->child_port;
	}

	dev_err(&csdev->dev, "couldn't find inport, parent: %s, child: %s\n",
		dev_name(&parent->dev), dev_name(&csdev->dev));

	return 0;
}

static int coresight_find_link_outport(struct coresight_device *csdev,
				       struct coresight_device *child)
{
	int i;
	struct coresight_connection *conn;

	for (i = 0; i < csdev->nr_outport; i++) {
		conn = &csdev->conns[i];
		if (conn->child_dev == child)
			return conn->outport;
	}

	dev_err(&csdev->dev, "couldn't find outport, parent: %s, child: %s\n",
		dev_name(&csdev->dev), dev_name(&child->dev));

	return 0;
}

static int coresight_enable_sink(struct coresight_device *csdev, u32 mode)
{
	int ret;

	if (!csdev->enable) {
		if (sink_ops(csdev)->enable) {
			ret = sink_ops(csdev)->enable(csdev, mode);
			if (ret)
				return ret;
		}
		csdev->enable = true;
	}

	atomic_inc(csdev->refcnt);

	return 0;
}

static void coresight_disable_sink(struct coresight_device *csdev)
{
	if (atomic_dec_return(csdev->refcnt) == 0) {
		if (sink_ops(csdev)->disable) {
			sink_ops(csdev)->disable(csdev);
			csdev->enable = false;
			csdev->activated = false;
		}
	}
}

static int coresight_enable_link(struct coresight_device *csdev,
				 struct coresight_device *parent,
				 struct coresight_device *child)
{
	int ret;
	int link_subtype;
	int refport, inport, outport;

	if (!parent || !child)
		return -EINVAL;

	inport = coresight_find_link_inport(csdev, parent);
	outport = coresight_find_link_outport(csdev, child);
	link_subtype = csdev->subtype.link_subtype;

	if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_MERG)
		refport = inport;
	else if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_SPLIT)
		refport = outport;
	else
		refport = 0;

	if (atomic_inc_return(&csdev->refcnt[refport]) == 1) {
		if (link_ops(csdev)->enable) {
			ret = link_ops(csdev)->enable(csdev, inport, outport);
			if (ret)
				return ret;
		}
	}

	csdev->enable = true;

	return 0;
}

static void coresight_disable_link(struct coresight_device *csdev,
				   struct coresight_device *parent,
				   struct coresight_device *child)
{
	int i, nr_conns;
	int link_subtype;
	int refport, inport, outport;

	if (!parent || !child)
		return;

	inport = coresight_find_link_inport(csdev, parent);
	outport = coresight_find_link_outport(csdev, child);
	link_subtype = csdev->subtype.link_subtype;

	if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_MERG) {
		refport = inport;
		nr_conns = csdev->nr_inport;
	} else if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_SPLIT) {
		refport = outport;
		nr_conns = csdev->nr_outport;
	} else {
		refport = 0;
		nr_conns = 1;
	}

	if (atomic_dec_return(&csdev->refcnt[refport]) == 0) {
		if (link_ops(csdev)->disable)
			link_ops(csdev)->disable(csdev, inport, outport);
	}

	for (i = 0; i < nr_conns; i++)
		if (atomic_read(&csdev->refcnt[i]) != 0)
			return;

	csdev->enable = false;
}

static int coresight_enable_source(struct coresight_device *csdev, u32 mode)
{
	int ret;

	if (!coresight_source_is_unique(csdev)) {
		dev_warn(&csdev->dev, "traceID %d not unique\n",
			 source_ops(csdev)->trace_id(csdev));
		return -EINVAL;
	}

	if (!csdev->enable) {
		if (source_ops(csdev)->enable) {
			ret = source_ops(csdev)->enable(csdev, NULL, mode);
			if (ret)
				return ret;
		}
		csdev->enable = true;
	}

	atomic_inc(csdev->refcnt);

	return 0;
}

/**
 *  coresight_disable_source - Drop the reference count by 1 and disable
 *  the device if there are no users left.
 *
 *  @csdev - The coresight device to disable
 *
 *  Returns true if the device has been disabled.
 */
static bool coresight_disable_source(struct coresight_device *csdev)
{
	if (atomic_dec_return(csdev->refcnt) == 0) {
		if (source_ops(csdev)->disable)
			source_ops(csdev)->disable(csdev, NULL);
		csdev->enable = false;
	}
	return !csdev->enable;
}

void coresight_disable_path(struct list_head *path)
{
	u32 type;
	struct coresight_node *nd;
	struct coresight_device *csdev, *parent, *child;

	list_for_each_entry(nd, path, link) {
		csdev = nd->csdev;
		type = csdev->type;

		/*
		 * ETF devices are tricky... They can be a link or a sink,
		 * depending on how they are configured.  If an ETF has been
		 * "activated" it will be configured as a sink, otherwise
		 * go ahead with the link configuration.
		 */
		if (type == CORESIGHT_DEV_TYPE_LINKSINK)
			type = (csdev == coresight_get_sink(path)) ?
						CORESIGHT_DEV_TYPE_SINK :
						CORESIGHT_DEV_TYPE_LINK;

		switch (type) {
		case CORESIGHT_DEV_TYPE_SINK:
			coresight_disable_sink(csdev);
			break;
		case CORESIGHT_DEV_TYPE_SOURCE:
			/* sources are disabled from either sysFS or Perf */
			break;
		case CORESIGHT_DEV_TYPE_LINK:
			parent = list_prev_entry(nd, link)->csdev;
			child = list_next_entry(nd, link)->csdev;
			coresight_disable_link(csdev, parent, child);
			break;
		default:
			break;
		}
	}
}

int coresight_enable_path(struct list_head *path, u32 mode)
{

	int ret = 0;
	u32 type;
	struct coresight_node *nd;
	struct coresight_device *csdev, *parent, *child;

	list_for_each_entry_reverse(nd, path, link) {
		csdev = nd->csdev;
		type = csdev->type;

		/*
		 * ETF devices are tricky... They can be a link or a sink,
		 * depending on how they are configured.  If an ETF has been
		 * "activated" it will be configured as a sink, otherwise
		 * go ahead with the link configuration.
		 */
		if (type == CORESIGHT_DEV_TYPE_LINKSINK)
			type = (csdev == coresight_get_sink(path)) ?
						CORESIGHT_DEV_TYPE_SINK :
						CORESIGHT_DEV_TYPE_LINK;

		switch (type) {
		case CORESIGHT_DEV_TYPE_SINK:
			ret = coresight_enable_sink(csdev, mode);
			if (ret)
				goto err;
			break;
		case CORESIGHT_DEV_TYPE_SOURCE:
			/* sources are enabled from either sysFS or Perf */
			break;
		case CORESIGHT_DEV_TYPE_LINK:
			parent = list_prev_entry(nd, link)->csdev;
			child = list_next_entry(nd, link)->csdev;
			ret = coresight_enable_link(csdev, parent, child);
			if (ret)
				goto err;
			break;
		default:
			goto err;
		}
	}

out:
	return ret;
err:
	coresight_disable_path(path);
	goto out;
}

struct coresight_device *coresight_get_source(struct list_head *path)
{
	struct coresight_device *csdev;

	if (!path)
		return NULL;

	csdev = list_first_entry(path, struct coresight_node, link)->csdev;
	if (csdev->type != CORESIGHT_DEV_TYPE_SOURCE)
		return NULL;

	return csdev;
}

struct coresight_device *coresight_get_sink(struct list_head *path)
{
	struct coresight_device *csdev;

	if (!path)
		return NULL;

	csdev = list_last_entry(path, struct coresight_node, link)->csdev;
	if (csdev->type != CORESIGHT_DEV_TYPE_SINK &&
	    csdev->type != CORESIGHT_DEV_TYPE_LINKSINK)
		return NULL;

	return csdev;
}

static int coresight_enabled_sink(struct device *dev, void *data)
{
	bool *reset = data;
	struct coresight_device *csdev = to_coresight_device(dev);

	if ((csdev->type == CORESIGHT_DEV_TYPE_SINK ||
	     csdev->type == CORESIGHT_DEV_TYPE_LINKSINK) &&
	     csdev->activated) {
		/*
		 * Now that we have a handle on the sink for this session,
		 * disable the sysFS "enable_sink" flag so that possible
		 * concurrent perf session that wish to use another sink don't
		 * trip on it.  Doing so has no ramification for the current
		 * session.
		 */
		if (*reset)
			csdev->activated = false;

		return 1;
	}

	return 0;
}

/**
 * coresight_get_enabled_sink - returns the first enabled sink found on the bus
 * @deactivate:	Whether the 'enable_sink' flag should be reset
 *
 * When operated from perf the deactivate parameter should be set to 'true'.
 * That way the "enabled_sink" flag of the sink that was selected can be reset,
 * allowing for other concurrent perf sessions to choose a different sink.
 *
 * When operated from sysFS users have full control and as such the deactivate
 * parameter should be set to 'false', hence mandating users to explicitly
 * clear the flag.
 */
struct coresight_device *coresight_get_enabled_sink(bool deactivate)
{
	struct device *dev = NULL;

	dev = bus_find_device(&coresight_bustype, NULL, &deactivate,
			      coresight_enabled_sink);

	return dev ? to_coresight_device(dev) : NULL;
}

/**
 * _coresight_build_path - recursively build a path from a @csdev to a sink.
 * @csdev:	The device to start from.
 * @path:	The list to add devices to.
 *
 * The tree of Coresight device is traversed until an activated sink is
 * found.  From there the sink is added to the list along with all the
 * devices that led to that point - the end result is a list from source
 * to sink. In that list the source is the first device and the sink the
 * last one.
 */
static int _coresight_build_path(struct coresight_device *csdev,
				 struct coresight_device *sink,
				 struct list_head *path)
{
	int i;
	bool found = false;
	struct coresight_node *node;

	/* An activated sink has been found.  Enqueue the element */
	if (csdev == sink)
		goto out;

	/* Not a sink - recursively explore each port found on this element */
	for (i = 0; i < csdev->nr_outport; i++) {
		struct coresight_device *child_dev = csdev->conns[i].child_dev;

		if (child_dev &&
		    _coresight_build_path(child_dev, sink, path) == 0) {
			found = true;
			break;
		}
	}

	if (!found)
		return -ENODEV;

out:
	/*
	 * A path from this element to a sink has been found.  The elements
	 * leading to the sink are already enqueued, all that is left to do
	 * is tell the PM runtime core we need this element and add a node
	 * for it.
	 */
	node = kzalloc(sizeof(struct coresight_node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->csdev = csdev;
	list_add(&node->link, path);
	pm_runtime_get_sync(csdev->dev.parent);

	return 0;
}

struct list_head *coresight_build_path(struct coresight_device *source,
				       struct coresight_device *sink)
{
	struct list_head *path;
	int rc;

	if (!sink)
		return ERR_PTR(-EINVAL);

	path = kzalloc(sizeof(struct list_head), GFP_KERNEL);
	if (!path)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(path);

	rc = _coresight_build_path(source, sink, path);
	if (rc) {
		kfree(path);
		return ERR_PTR(rc);
	}

	return path;
}

/**
 * coresight_release_path - release a previously built path.
 * @path:	the path to release.
 * Remove coresight path entry from source device
 *
 * Go through all the elements of a path and 1) removed it from the list and
 * 2) free the memory allocated for each node.
 */
void coresight_release_path(struct coresight_device *csdev,
			    struct list_head *path)
{
	struct coresight_node *nd, *next;

	if (csdev != NULL && csdev->node != NULL) {
		/* Remove path entry from source device */
		list_del(&csdev->node->link);
		kfree(csdev->node);
		csdev->node = NULL;
	}

	/* Free the path */
	list_for_each_entry_safe(nd, next, path, link) {
		csdev = nd->csdev;

		pm_runtime_put_sync(csdev->dev.parent);
		list_del(&nd->link);
		kfree(nd);
	}

	kfree(path);
	path = NULL;
}

/** coresight_validate_source - make sure a source has the right credentials
 *  @csdev:	the device structure for a source.
 *  @function:	the function this was called from.
 *
 * Assumes the coresight_mutex is held.
 */
static int coresight_validate_source(struct coresight_device *csdev,
				     const char *function)
{
	u32 type, subtype;

	type = csdev->type;
	subtype = csdev->subtype.source_subtype;

	if (type != CORESIGHT_DEV_TYPE_SOURCE) {
		dev_err(&csdev->dev, "wrong device type in %s\n", function);
		return -EINVAL;
	}

	if (subtype != CORESIGHT_DEV_SUBTYPE_SOURCE_PROC &&
	    subtype != CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE) {
		dev_err(&csdev->dev, "wrong device subtype in %s\n", function);
		return -EINVAL;
	}

	return 0;
}

int coresight_store_path(struct coresight_device *csdev, struct list_head *path)
{
	struct coresight_path *node;

	node = kzalloc(sizeof(struct coresight_path), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->path = path;
	list_add(&node->link, &cs_active_paths);

	csdev->node = node;

	return 0;
}

int coresight_enable(struct coresight_device *csdev)
{
	int ret = 0;
	struct coresight_device *sink;
	struct list_head *path;
	enum coresight_dev_subtype_source subtype;

	subtype = csdev->subtype.source_subtype;

	mutex_lock(&coresight_mutex);

	ret = coresight_validate_source(csdev, __func__);
	if (ret)
		goto out;

	if (csdev->enable) {
		/*
		 * There could be multiple applications driving the software
		 * source. So keep the refcount for each such user when the
		 * source is already enabled.
		 */
		if (subtype == CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE)
			atomic_inc(csdev->refcnt);
		goto out;
	}

	/*
	 * Search for a valid sink for this session but don't reset the
	 * "enable_sink" flag in sysFS.  Users get to do that explicitly.
	 */
	sink = coresight_get_enabled_sink(false);
	if (!sink) {
		ret = -EINVAL;
		goto out;
	}

	path = coresight_build_path(csdev, sink);
	if (IS_ERR(path)) {
		pr_err("building path(s) failed\n");
		ret = PTR_ERR(path);
		goto out;
	}

	ret = coresight_enable_path(path, CS_MODE_SYSFS);
	if (ret)
		goto err_path;

	ret = coresight_enable_source(csdev, CS_MODE_SYSFS);
	if (ret)
		goto err_source;

	ret = coresight_store_path(csdev, path);
	if (ret)
		goto err_source;

out:
	mutex_unlock(&coresight_mutex);
	return ret;

err_source:
	coresight_disable_path(path);

err_path:
	coresight_release_path(csdev, path);
	goto out;
}
EXPORT_SYMBOL(coresight_enable);

static void __coresight_disable(struct coresight_device *csdev)
{
	int  ret;

	ret = coresight_validate_source(csdev, __func__);
	if (ret)
		return;

	if (!csdev->enable || !coresight_disable_source(csdev))
		return;

	if (csdev->node == NULL)
		return;

	coresight_disable_path(csdev->node->path);
	coresight_release_path(csdev, csdev->node->path);
}

void coresight_disable(struct coresight_device *csdev)
{
	mutex_lock(&coresight_mutex);
	__coresight_disable(csdev);
	mutex_unlock(&coresight_mutex);
}
EXPORT_SYMBOL(coresight_disable);

void coresight_abort(void)
{
	if (!mutex_trylock(&coresight_mutex)) {
		pr_err("coresight: abort could not be processed\n");
		return;
	}
	if (!curr_sink)
		goto out;

	if (curr_sink->enable && sink_ops(curr_sink)->abort) {
		sink_ops(curr_sink)->abort(curr_sink);
		curr_sink->enable = false;
	}

out:
	mutex_unlock(&coresight_mutex);
}
EXPORT_SYMBOL(coresight_abort);


static ssize_t enable_sink_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct coresight_device *csdev = to_coresight_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", csdev->activated);
}

static ssize_t enable_sink_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct coresight_device *csdev = to_coresight_device(dev);

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val)
		csdev->activated = true;
	else
		csdev->activated = false;

	return size;

}
static DEVICE_ATTR_RW(enable_sink);

static ssize_t enable_source_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct coresight_device *csdev = to_coresight_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", csdev->enable);
}

static ssize_t enable_source_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int ret = 0;
	unsigned long val;
	struct coresight_device *csdev = to_coresight_device(dev);

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val) {
		ret = coresight_enable(csdev);
		if (ret)
			return ret;
	} else {
		coresight_disable(csdev);
	}

	return size;
}
static DEVICE_ATTR_RW(enable_source);

static struct attribute *coresight_sink_attrs[] = {
	&dev_attr_enable_sink.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_sink);

static struct attribute *coresight_source_attrs[] = {
	&dev_attr_enable_source.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_source);

static struct device_type coresight_dev_type[] = {
	{
		.name = "none",
	},
	{
		.name = "sink",
		.groups = coresight_sink_groups,
	},
	{
		.name = "link",
	},
	{
		.name = "linksink",
		.groups = coresight_sink_groups,
	},
	{
		.name = "source",
		.groups = coresight_source_groups,
	},
};

static void coresight_device_release(struct device *dev)
{
	struct coresight_device *csdev = to_coresight_device(dev);

	kfree(csdev->conns);
	kfree(csdev->refcnt);
	kfree(csdev);
}

static int coresight_orphan_match(struct device *dev, void *data)
{
	int i;
	bool still_orphan = false;
	struct coresight_device *csdev, *i_csdev;
	struct coresight_connection *conn;

	csdev = data;
	i_csdev = to_coresight_device(dev);

	/* No need to check oneself */
	if (csdev == i_csdev)
		return 0;

	/* Move on to another component if no connection is orphan */
	if (!i_csdev->orphan)
		return 0;
	/*
	 * Circle throuch all the connection of that component.  If we find
	 * an orphan connection whose name matches @csdev, link it.
	 */
	for (i = 0; i < i_csdev->nr_outport; i++) {
		conn = &i_csdev->conns[i];

		/* We have found at least one orphan connection */
		if (conn->child_dev == NULL) {
			/* Does it match this newly added device? */
			if (conn->child_name &&
			    !strcmp(dev_name(&csdev->dev), conn->child_name)) {
				conn->child_dev = csdev;
			} else {
				/* This component still has an orphan */
				still_orphan = true;
			}
		}
	}

	i_csdev->orphan = still_orphan;

	/*
	 * Returning '0' ensures that all known component on the
	 * bus will be checked.
	 */
	return 0;
}

static void coresight_fixup_orphan_conns(struct coresight_device *csdev)
{
	/*
	 * No need to check for a return value as orphan connection(s)
	 * are hooked-up with each newly added component.
	 */
	bus_for_each_dev(&coresight_bustype, NULL,
			 csdev, coresight_orphan_match);
}


static int coresight_name_match(struct device *dev, void *data)
{
	char *to_match;
	struct coresight_device *i_csdev;

	to_match = data;
	i_csdev = to_coresight_device(dev);

	if (to_match && !strcmp(to_match, dev_name(&i_csdev->dev)))
		return 1;

	return 0;
}

static void coresight_fixup_device_conns(struct coresight_device *csdev)
{
	int i;
	struct device *dev = NULL;
	struct coresight_connection *conn;

	for (i = 0; i < csdev->nr_outport; i++) {
		conn = &csdev->conns[i];
		dev = bus_find_device(&coresight_bustype, NULL,
				      (void *)conn->child_name,
				      coresight_name_match);

		if (dev) {
			conn->child_dev = to_coresight_device(dev);
			/* and put reference from 'bus_find_device()' */
			put_device(dev);
		} else {
			csdev->orphan = true;
			conn->child_dev = NULL;
		}
	}
}

static int coresight_remove_match(struct device *dev, void *data)
{
	int i;
	struct coresight_device *csdev, *iterator;
	struct coresight_connection *conn;

	csdev = data;
	iterator = to_coresight_device(dev);

	/* No need to check oneself */
	if (csdev == iterator)
		return 0;

	/*
	 * Circle throuch all the connection of that component.  If we find
	 * a connection whose name matches @csdev, remove it.
	 */
	for (i = 0; i < iterator->nr_outport; i++) {
		conn = &iterator->conns[i];

		if (conn->child_dev == NULL)
			continue;

		if (!strcmp(dev_name(&csdev->dev), conn->child_name)) {
			iterator->orphan = true;
			conn->child_dev = NULL;
			/* No need to continue */
			break;
		}
	}

	/*
	 * Returning '0' ensures that all known component on the
	 * bus will be checked.
	 */
	return 0;
}

static void coresight_remove_conns(struct coresight_device *csdev)
{
	bus_for_each_dev(&coresight_bustype, NULL,
			 csdev, coresight_remove_match);
}

/**
 * coresight_timeout - loop until a bit has changed to a specific state.
 * @addr: base address of the area of interest.
 * @offset: address of a register, starting from @addr.
 * @position: the position of the bit of interest.
 * @value: the value the bit should have.
 *
 * Return: 0 as soon as the bit has taken the desired state or -EAGAIN if
 * TIMEOUT_US has elapsed, which ever happens first.
 */

int coresight_timeout(void __iomem *addr, u32 offset, int position, int value)
{
	int i;
	u32 val;

	for (i = TIMEOUT_US; i > 0; i--) {
		val = __raw_readl(addr + offset);
		/* waiting on the bit to go from 0 to 1 */
		if (value) {
			if (val & BIT(position))
				return 0;
		/* waiting on the bit to go from 1 to 0 */
		} else {
			if (!(val & BIT(position)))
				return 0;
		}

		/*
		 * Delay is arbitrary - the specification doesn't say how long
		 * we are expected to wait.  Extra check required to make sure
		 * we don't wait needlessly on the last iteration.
		 */
		if (i - 1)
			udelay(1);
	}

	return -EAGAIN;
}

static ssize_t reset_source_sink_store(struct bus_type *bus,
				       const char *buf, size_t size)
{
	int ret = 0;
	unsigned long val;
	struct coresight_path *cspath = NULL;
	struct coresight_path *cspath_next = NULL;
	struct coresight_device *csdev;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	mutex_lock(&coresight_mutex);

	list_for_each_entry_safe(cspath, cspath_next, &cs_active_paths, link) {
		csdev = coresight_get_source(cspath->path);
		if (!csdev)
			continue;
		__coresight_disable(csdev);
	}

	/* Reset all activated sinks */
	coresight_reset_all_sink();

	mutex_unlock(&coresight_mutex);
	return size;
}
static BUS_ATTR_WO(reset_source_sink);

static struct attribute *coresight_reset_source_sink_attrs[] = {
	&bus_attr_reset_source_sink.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_reset_source_sink);

struct bus_type coresight_bustype = {
	.name		= "coresight",
	.bus_groups	= coresight_reset_source_sink_groups,
};

static int __init coresight_init(void)
{
	return bus_register(&coresight_bustype);
}
postcore_initcall(coresight_init);

struct coresight_device *coresight_register(struct coresight_desc *desc)
{
	int i;
	int ret;
	int link_subtype;
	int nr_refcnts = 1;
	atomic_t *refcnts = NULL;
	struct coresight_device *csdev;
	struct coresight_connection *conns = NULL;
	struct clk *pclk;

	pclk = clk_get(desc->dev, "apb_pclk");
	if (!IS_ERR(pclk)) {
		ret = clk_set_rate(pclk, QDSS_CLK_LEVEL_DYNAMIC);
		if (ret)
			dev_err(desc->dev, "clk set rate failed\n");
	}

	csdev = kzalloc(sizeof(*csdev), GFP_KERNEL);
	if (!csdev) {
		ret = -ENOMEM;
		goto err_kzalloc_csdev;
	}

	if (desc->type == CORESIGHT_DEV_TYPE_LINK ||
	    desc->type == CORESIGHT_DEV_TYPE_LINKSINK) {
		link_subtype = desc->subtype.link_subtype;

		if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_MERG)
			nr_refcnts = desc->pdata->nr_inport;
		else if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_SPLIT)
			nr_refcnts = desc->pdata->nr_outport;
	}

	refcnts = kcalloc(nr_refcnts, sizeof(*refcnts), GFP_KERNEL);
	if (!refcnts) {
		ret = -ENOMEM;
		goto err_kzalloc_refcnts;
	}

	csdev->refcnt = refcnts;

	csdev->nr_inport = desc->pdata->nr_inport;
	csdev->nr_outport = desc->pdata->nr_outport;

	/* Initialise connections if there is at least one outport */
	if (csdev->nr_outport) {
		conns = kcalloc(csdev->nr_outport, sizeof(*conns), GFP_KERNEL);
		if (!conns) {
			ret = -ENOMEM;
			goto err_kzalloc_conns;
		}

		for (i = 0; i < csdev->nr_outport; i++) {
			conns[i].outport = desc->pdata->outports[i];
			conns[i].child_name = desc->pdata->child_names[i];
			conns[i].child_port = desc->pdata->child_ports[i];
		}
	}

	csdev->conns = conns;

	csdev->type = desc->type;
	csdev->subtype = desc->subtype;
	csdev->ops = desc->ops;
	csdev->orphan = false;

	csdev->dev.type = &coresight_dev_type[desc->type];
	csdev->dev.groups = desc->groups;
	csdev->dev.parent = desc->dev;
	csdev->dev.release = coresight_device_release;
	csdev->dev.bus = &coresight_bustype;
	dev_set_name(&csdev->dev, "%s", desc->pdata->name);

	ret = device_register(&csdev->dev);
	if (ret)
		goto err_device_register;

	mutex_lock(&coresight_mutex);

	coresight_fixup_device_conns(csdev);
	coresight_fixup_orphan_conns(csdev);

	mutex_unlock(&coresight_mutex);

	return csdev;

err_device_register:
	kfree(conns);
err_kzalloc_conns:
	kfree(refcnts);
err_kzalloc_refcnts:
	kfree(csdev);
err_kzalloc_csdev:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(coresight_register);

void coresight_unregister(struct coresight_device *csdev)
{
	/* Remove references of that device in the topology */
	coresight_remove_conns(csdev);
	device_unregister(&csdev->dev);
}
EXPORT_SYMBOL_GPL(coresight_unregister);
