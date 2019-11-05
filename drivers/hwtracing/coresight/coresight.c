// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2012, 2017-2019, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>

#include "coresight-priv.h"

static int coresight_source_filter(struct list_head *path,
			struct coresight_connection *conn);

static DEFINE_MUTEX(coresight_mutex);

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

struct coresight_link {
	struct coresight_device *csdev;
	int inport;
	int outport;
};

struct coresight_link_node {
	struct coresight_link *cs_link;
	struct list_head link;
};

static LIST_HEAD(cs_disabled_link);

static LIST_HEAD(cs_active_paths);

static struct coresight_device *activated_sink;

/*
 * When losing synchronisation a new barrier packet needs to be inserted at the
 * beginning of the data collected in a buffer.  That way the decoder knows that
 * it needs to look for another sync sequence.
 */
const u32 barrier_pkt[4] = {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};

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

int coresight_enable_reg_clk(struct coresight_device *csdev)
{
	struct coresight_reg_clk *reg_clk = csdev->reg_clk;
	int ret;
	int i, j;

	if (IS_ERR_OR_NULL(reg_clk))
		return -EINVAL;

	for (i = 0; i < reg_clk->nr_reg; i++) {
		ret = regulator_enable(reg_clk->reg[i]);
		if (ret)
			goto err_regs;
	}

	for (j = 0; j < reg_clk->nr_clk; j++) {
		ret = clk_prepare_enable(reg_clk->clk[j]);
		if (ret)
			goto err_clks;
	}

	return 0;
err_clks:
	for (j--; j >= 0; j--)
		clk_disable_unprepare(reg_clk->clk[j]);
err_regs:
	for (i--; i >= 0; i--)
		regulator_disable(reg_clk->reg[i]);

	return ret;
}
EXPORT_SYMBOL(coresight_enable_reg_clk);

void coresight_disable_reg_clk(struct coresight_device *csdev)
{
	struct coresight_reg_clk *reg_clk = csdev->reg_clk;
	int i;

	if (IS_ERR_OR_NULL(reg_clk))
		return;

	for (i = reg_clk->nr_clk - 1; i >= 0; i--)
		clk_disable_unprepare(reg_clk->clk[i]);
	for (i = reg_clk->nr_reg - 1; i >= 0; i--)
		regulator_disable(reg_clk->reg[i]);
}
EXPORT_SYMBOL(coresight_disable_reg_clk);

static int coresight_find_link_inport(struct coresight_device *csdev,
				      struct coresight_device *parent,
				      struct list_head *path)
{
	int i;
	struct coresight_connection *conn;

	for (i = 0; i < parent->nr_outport; i++) {
		conn = &parent->conns[i];
		if (coresight_source_filter(path, conn))
			continue;
		if (conn->child_dev == csdev)
			return conn->child_port;
	}

	dev_err(&csdev->dev, "couldn't find inport, parent: %s, child: %s\n",
		dev_name(&parent->dev), dev_name(&csdev->dev));

	return -ENODEV;
}

static int coresight_find_link_outport(struct coresight_device *csdev,
				       struct coresight_device *child,
				       struct list_head *path)
{
	int i;
	struct coresight_connection *conn;

	for (i = 0; i < csdev->nr_outport; i++) {
		conn = &csdev->conns[i];
		if (coresight_source_filter(path, conn))
			continue;
		if (conn->child_dev == child)
			return conn->outport;
	}

	dev_err(&csdev->dev, "couldn't find outport, parent: %s, child: %s\n",
		dev_name(&csdev->dev), dev_name(&child->dev));

	return -ENODEV;
}

static void coresight_disable_links(void)
{
	struct coresight_link_node *node;
	struct coresight_link_node *node_next;
	struct device *dev;
	struct coresight_device *csdev;

	list_for_each_entry_safe(node, node_next, &cs_disabled_link, link) {
		csdev = node->cs_link->csdev;
		if (link_ops(csdev)->disable) {
			link_ops(csdev)->disable(csdev, node->cs_link->inport,
					node->cs_link->outport);
			coresight_disable_reg_clk(csdev);
		}
		dev = &node->cs_link->csdev->dev;
		devm_kfree(dev, node->cs_link);
		node->cs_link = NULL;
		list_del(&node->link);
		devm_kfree(dev, node);
	}
}

static void coresight_add_disabled_link(struct coresight_device *csdev,
					int inport, int outport)
{
	struct coresight_link_node *node;
	struct coresight_link *cs_link;

	if (csdev) {
		cs_link = devm_kzalloc(&csdev->dev,
				sizeof(struct coresight_link), GFP_KERNEL);
		if (!cs_link)
			return;
		node = devm_kzalloc(&csdev->dev,
				sizeof(struct coresight_link_node), GFP_KERNEL);
		if (!node) {
			devm_kfree(&csdev->dev, cs_link);
			return;
		}
		cs_link->csdev = csdev;
		cs_link->inport = inport;
		cs_link->outport = outport;
		node->cs_link = cs_link;
		list_add(&node->link, &cs_disabled_link);
	}
}

static int coresight_enable_sink(struct coresight_device *csdev, u32 mode)
{
	int ret;

	if (!csdev->enable) {
		if (sink_ops(csdev)->enable) {
			ret = coresight_enable_reg_clk(csdev);
			if (ret)
				return ret;

			ret = sink_ops(csdev)->enable(csdev, mode);
			if (ret) {
				coresight_disable_reg_clk(csdev);
				return ret;
			}
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
			coresight_disable_reg_clk(csdev);
			if (coresight_link_late_disable())
				coresight_disable_links();
			csdev->enable = false;
			csdev->activated = false;
		}
	}
}

static int coresight_enable_link(struct coresight_device *csdev,
				 struct coresight_device *parent,
				 struct coresight_device *child,
				 struct list_head *path)
{
	int ret;
	int link_subtype;
	int refport, inport, outport;

	if (!parent || !child)
		return -EINVAL;

	inport = coresight_find_link_inport(csdev, parent, path);
	outport = coresight_find_link_outport(csdev, child, path);
	link_subtype = csdev->subtype.link_subtype;

	if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_MERG)
		refport = inport;
	else if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_SPLIT)
		refport = outport;
	else
		refport = 0;

	if (refport < 0)
		return refport;

	if (atomic_inc_return(&csdev->refcnt[refport]) == 1) {
		if (link_ops(csdev)->enable) {
			ret = coresight_enable_reg_clk(csdev);
			if (ret)
				return ret;

			ret = link_ops(csdev)->enable(csdev, inport, outport);
			if (ret) {
				coresight_disable_reg_clk(csdev);
				atomic_dec(&csdev->refcnt[refport]);
				return ret;
			}
		}
	}

	csdev->enable = true;

	return 0;
}

static void coresight_disable_link(struct coresight_device *csdev,
				   struct coresight_device *parent,
				   struct coresight_device *child,
				   struct list_head *path)
{
	int i, nr_conns;
	int link_subtype;
	int refport, inport, outport;

	if (!parent || !child)
		return;

	inport = coresight_find_link_inport(csdev, parent, path);
	outport = coresight_find_link_outport(csdev, child, path);
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
		if (coresight_link_late_disable()) {
			coresight_add_disabled_link(csdev, inport, outport);
		} else if (link_ops(csdev)->disable) {
			link_ops(csdev)->disable(csdev, inport, outport);
			coresight_disable_reg_clk(csdev);
		}
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
			ret = coresight_enable_reg_clk(csdev);
			if (ret)
				return ret;

			ret = source_ops(csdev)->enable(csdev, NULL, mode);
			if (ret) {
				coresight_disable_reg_clk(csdev);
				return ret;
			}
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
		if (source_ops(csdev)->disable) {
			source_ops(csdev)->disable(csdev, NULL);
			coresight_disable_reg_clk(csdev);
		}
		csdev->enable = false;
	}
	return !csdev->enable;
}

static void coresight_disable_list_node(struct list_head *path,
					struct coresight_node *nd)
{
	u32 type;
	struct coresight_device *csdev, *parent, *child;

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
		coresight_disable_link(csdev, parent, child, path);
		break;
	default:
		break;
	}
}

/**
 * During enabling path, if it is failed, then only those enabled
 * devices need to be disabled. This function is to disable devices
 * which is enabled before the failed device.
 *
 * @path the head of the list
 * @nd the failed device node
 */
static void coresight_disable_previous_devs(struct list_head *path,
					struct coresight_node *nd)
{

	list_for_each_entry_continue(nd, path, link) {
		coresight_disable_list_node(path, nd);
	}
}

void coresight_disable_path(struct list_head *path)
{
	struct coresight_node *nd;

	list_for_each_entry(nd, path, link) {
		coresight_disable_list_node(path, nd);
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
			ret = coresight_enable_link(csdev, parent, child, path);
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
	coresight_disable_previous_devs(path, nd);
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

/*
 * coresight_grab_device - Power up this device and any of the helper
 * devices connected to it for trace operation. Since the helper devices
 * don't appear on the trace path, they should be handled along with the
 * the master device.
 */
static void coresight_grab_device(struct coresight_device *csdev)
{
	int i;

	for (i = 0; i < csdev->nr_outport; i++) {
		struct coresight_device *child = csdev->conns[i].child_dev;

		if (child && child->type == CORESIGHT_DEV_TYPE_HELPER)
			pm_runtime_get_sync(child->dev.parent);
	}
	pm_runtime_get_sync(csdev->dev.parent);
}

/*
 * coresight_drop_device - Release this device and any of the helper
 * devices connected to it.
 */
static void coresight_drop_device(struct coresight_device *csdev)
{
	int i;

	pm_runtime_put(csdev->dev.parent);
	for (i = 0; i < csdev->nr_outport; i++) {
		struct coresight_device *child = csdev->conns[i].child_dev;

		if (child && child->type == CORESIGHT_DEV_TYPE_HELPER)
			pm_runtime_put(child->dev.parent);
	}
}

/**
 * coresight_source_filter - checks whether the connection matches the source
 * of path if connection is binded to specific source.
 * @path:	The list of devices
 * @conn:	The connection of one outport
 *
 * Return zero if the connection doesn't have a source binded or source of the
 * path matches the source binds to connection.
 */
static int coresight_source_filter(struct list_head *path,
			struct coresight_connection *conn)
{
	int ret = 0;
	struct coresight_device *source = NULL;

	if (conn->source_name == NULL)
		return ret;

	source = coresight_get_source(path);
	if (source == NULL)
		return ret;

	return strcmp(conn->source_name, dev_name(&source->dev));
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
				 struct list_head *path,
				 struct coresight_device *source)
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

		if (csdev->conns[i].source_name &&
		    strcmp(csdev->conns[i].source_name, dev_name(&source->dev)))
			continue;

		if (child_dev &&
		    _coresight_build_path(child_dev, sink, path, source) == 0) {
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

	coresight_grab_device(csdev);
	node->csdev = csdev;
	list_add(&node->link, path);

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

	rc = _coresight_build_path(source, sink, path, source);
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

		coresight_drop_device(csdev);
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

static void coresight_enable_source_link(struct list_head *path)
{
	u32 type;
	int ret;
	struct coresight_node *nd;
	struct coresight_device *csdev, *parent, *child;

	list_for_each_entry_reverse(nd, path, link) {
		csdev = nd->csdev;
		type = csdev->type;

		if (type == CORESIGHT_DEV_TYPE_LINKSINK)
			type = (csdev == coresight_get_sink(path)) ?
						CORESIGHT_DEV_TYPE_SINK :
						CORESIGHT_DEV_TYPE_LINK;

		switch (type) {
		case CORESIGHT_DEV_TYPE_SINK:
			break;
		case CORESIGHT_DEV_TYPE_SOURCE:
			if (source_ops(csdev)->enable) {
				ret = coresight_enable_reg_clk(csdev);
				if (ret)
					goto err;

				ret = source_ops(csdev)->enable(csdev,
					NULL, CS_MODE_SYSFS);
				if (ret) {
					coresight_disable_reg_clk(csdev);
					goto err;
				}
			}
			csdev->enable = true;
			break;
		case CORESIGHT_DEV_TYPE_LINK:
			parent = list_prev_entry(nd, link)->csdev;
			child = list_next_entry(nd, link)->csdev;
			ret = coresight_enable_link(csdev, parent, child, path);
			if (ret)
				goto err;
			break;
		default:
			break;
		}
	}

	return;
err:
	coresight_disable_previous_devs(path, nd);
	coresight_release_path(csdev, path);
}

static void coresight_disable_source_link(struct list_head *path)
{
	u32 type;
	struct coresight_node *nd;
	struct coresight_device *csdev, *parent, *child;

	list_for_each_entry(nd, path, link) {
		csdev = nd->csdev;
		type = csdev->type;

		if (type == CORESIGHT_DEV_TYPE_LINKSINK)
			type = (csdev == coresight_get_sink(path)) ?
						CORESIGHT_DEV_TYPE_SINK :
						CORESIGHT_DEV_TYPE_LINK;

		switch (type) {
		case CORESIGHT_DEV_TYPE_SINK:
			break;
		case CORESIGHT_DEV_TYPE_SOURCE:
			if (source_ops(csdev)->disable) {
				source_ops(csdev)->disable(csdev, NULL);
				coresight_disable_reg_clk(csdev);
			}
			csdev->enable = false;
			break;
		case CORESIGHT_DEV_TYPE_LINK:
			parent = list_prev_entry(nd, link)->csdev;
			child = list_next_entry(nd, link)->csdev;
			coresight_disable_link(csdev, parent, child, path);
			break;
		default:
			break;
		}
	}
}
void coresight_disable_all_source_link(void)
{
	struct coresight_path *cspath = NULL;
	struct coresight_path *cspath_next = NULL;

	mutex_lock(&coresight_mutex);

	list_for_each_entry_safe(cspath, cspath_next, &cs_active_paths, link) {
		coresight_disable_source_link(cspath->path);
	}

	activated_sink = coresight_get_enabled_sink(false);
	if (activated_sink)
		activated_sink->activated = false;

	mutex_unlock(&coresight_mutex);
}

void coresight_enable_all_source_link(void)
{
	struct coresight_path *cspath = NULL;
	struct coresight_path *cspath_next = NULL;

	mutex_lock(&coresight_mutex);

	list_for_each_entry_safe(cspath, cspath_next, &cs_active_paths, link) {
		coresight_enable_source_link(cspath->path);
	}

	if (activated_sink && activated_sink->enable)
		activated_sink->activated = true;

	activated_sink = NULL;
	mutex_unlock(&coresight_mutex);
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
EXPORT_SYMBOL_GPL(coresight_enable);

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
EXPORT_SYMBOL_GPL(coresight_disable);

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
	struct coresight_device *sink = NULL;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;
	mutex_lock(&coresight_mutex);

	if (val) {
		sink = activated_sink ? activated_sink :
			coresight_get_enabled_sink(false);
		if (sink && strcmp(dev_name(&sink->dev),
				dev_name(&csdev->dev)))
			goto err;
		csdev->activated = true;
	} else {
		if (csdev->enable)
			goto err;
		csdev->activated = false;
	}
	mutex_unlock(&coresight_mutex);

	return size;

err:
	mutex_unlock(&coresight_mutex);
	return -EINVAL;
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
		atomic_set(csdev->refcnt, 1);
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
	{
		.name = "helper",
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


static void coresight_fixup_device_conns(struct coresight_device *csdev)
{
	int i;

	for (i = 0; i < csdev->nr_outport; i++) {
		struct coresight_connection *conn = &csdev->conns[i];
		struct device *dev = NULL;

		if (conn->child_name)
			dev = bus_find_device_by_name(&coresight_bustype, NULL,
						      conn->child_name);
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
		atomic_set(csdev->refcnt, 1);
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
			conns[i].source_name = desc->pdata->source_names[i];
		}
	}

	csdev->conns = conns;

	csdev->type = desc->type;
	csdev->subtype = desc->subtype;
	csdev->ops = desc->ops;
	csdev->orphan = false;
	csdev->reg_clk = desc->pdata->reg_clk;

	csdev->dev.type = &coresight_dev_type[desc->type];
	csdev->dev.groups = desc->groups;
	csdev->dev.parent = desc->dev;
	csdev->dev.release = coresight_device_release;
	csdev->dev.bus = &coresight_bustype;
	dev_set_name(&csdev->dev, "%s", desc->pdata->name);

	ret = device_register(&csdev->dev);
	if (ret) {
		put_device(&csdev->dev);
		goto err_kzalloc_csdev;
	}

	mutex_lock(&coresight_mutex);

	coresight_fixup_device_conns(csdev);
	coresight_fixup_orphan_conns(csdev);

	mutex_unlock(&coresight_mutex);

	return csdev;

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
