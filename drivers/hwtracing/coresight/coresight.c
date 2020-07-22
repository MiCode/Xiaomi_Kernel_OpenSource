// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2012, 2017-2020 The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/stringhash.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <dt-bindings/clock/qcom,aop-qmp.h>
#include <linux/coresight.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include "coresight-etm-perf.h"
#include "coresight-priv.h"

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

static LIST_HEAD(cs_active_paths);

static struct coresight_device *activated_sink;

/*
 * When losing synchronisation a new barrier packet needs to be inserted at the
 * beginning of the data collected in a buffer.  That way the decoder knows that
 * it needs to look for another sync sequence.
 */
const u32 barrier_pkt[4] = {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};

static struct coresight_device *coresight_get_source(struct list_head *path);

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

/**
 * coresight_source_filter - checks whether the connection matches the source
 * of path if connection is binded to specific source.
 * @path:	The list of devices
 * @conn:	The connection of one outport
 *
 * Return zero if the connection doesn't have a source binded or source of the
 * path matches the source binds to connection.
 */
#ifdef CONFIG_CORESIGHT_QGKI
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
#else
static int coresight_source_filter(struct list_head *path,
			struct coresight_connection *conn)
{
	return 0;
}
#endif

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

#ifdef CONFIG_CORESIGHT_QGKI
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
#else
int coresight_enable_reg_clk(struct coresight_device *csdev)
{ return 0; }
void coresight_disable_reg_clk(struct coresight_device *csdev) { }
#endif

static int coresight_find_link_inport(struct coresight_device *csdev,
				      struct coresight_device *parent,
				      struct list_head *path)
{
	int i;
	struct coresight_connection *conn;

	for (i = 0; i < parent->pdata->nr_outport; i++) {
		conn = &parent->pdata->conns[i];
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

	for (i = 0; i < csdev->pdata->nr_outport; i++) {
		conn = &csdev->pdata->conns[i];
		if (coresight_source_filter(path, conn))
			continue;
		if (conn->child_dev == child)
			return conn->outport;
	}

	dev_err(&csdev->dev, "couldn't find outport, parent: %s, child: %s\n",
		dev_name(&csdev->dev), dev_name(&child->dev));

	return -ENODEV;
}

static inline u32 coresight_read_claim_tags(void __iomem *base)
{
	return readl_relaxed(base + CORESIGHT_CLAIMCLR);
}

static inline bool coresight_is_claimed_self_hosted(void __iomem *base)
{
	return coresight_read_claim_tags(base) == CORESIGHT_CLAIM_SELF_HOSTED;
}

static inline bool coresight_is_claimed_any(void __iomem *base)
{
	return coresight_read_claim_tags(base) != 0;
}

static inline void coresight_set_claim_tags(void __iomem *base)
{
	writel_relaxed(CORESIGHT_CLAIM_SELF_HOSTED, base + CORESIGHT_CLAIMSET);
	isb();
}

static inline void coresight_clear_claim_tags(void __iomem *base)
{
	writel_relaxed(CORESIGHT_CLAIM_SELF_HOSTED, base + CORESIGHT_CLAIMCLR);
	isb();
}

/*
 * coresight_claim_device_unlocked : Claim the device for self-hosted usage
 * to prevent an external tool from touching this device. As per PSCI
 * standards, section "Preserving the execution context" => "Debug and Trace
 * save and Restore", DBGCLAIM[1] is reserved for Self-hosted debug/trace and
 * DBGCLAIM[0] is reserved for external tools.
 *
 * Called with CS_UNLOCKed for the component.
 * Returns : 0 on success
 */
int coresight_claim_device_unlocked(void __iomem *base)
{
	if (coresight_is_claimed_any(base))
		return -EBUSY;

	coresight_set_claim_tags(base);
	if (coresight_is_claimed_self_hosted(base))
		return 0;
	/* There was a race setting the tags, clean up and fail */
	coresight_clear_claim_tags(base);
	return -EBUSY;
}

int coresight_claim_device(void __iomem *base)
{
	int rc;

	CS_UNLOCK(base);
	rc = coresight_claim_device_unlocked(base);
	CS_LOCK(base);

	return rc;
}

/*
 * coresight_disclaim_device_unlocked : Clear the claim tags for the device.
 * Called with CS_UNLOCKed for the component.
 */
void coresight_disclaim_device_unlocked(void __iomem *base)
{

	if (coresight_is_claimed_self_hosted(base))
		coresight_clear_claim_tags(base);
	else
		/*
		 * The external agent may have not honoured our claim
		 * and has manipulated it. Or something else has seriously
		 * gone wrong in our driver.
		 */
		WARN_ON_ONCE(1);
}

void coresight_disclaim_device(void __iomem *base)
{
	CS_UNLOCK(base);
	coresight_disclaim_device_unlocked(base);
	CS_LOCK(base);
}

static int coresight_enable_sink(struct coresight_device *csdev,
				 u32 mode, void *data)
{
	int ret;

	/*
	 * We need to make sure the "new" session is compatible with the
	 * existing "mode" of operation.
	 */
	if (!sink_ops(csdev)->enable)
		return -EINVAL;

	coresight_enable_reg_clk(csdev);
	ret = sink_ops(csdev)->enable(csdev, mode, data);
	if (ret) {
		coresight_disable_reg_clk(csdev);
		return ret;
	}
	csdev->enable = true;

	return 0;
}

static void coresight_disable_sink(struct coresight_device *csdev)
{
	int ret;

	if (!sink_ops(csdev)->disable)
		return;

	ret = sink_ops(csdev)->disable(csdev);
	if (ret)
		return;
	coresight_disable_reg_clk(csdev);
	csdev->activated = false;
	csdev->enable = false;
}

static int coresight_enable_link(struct coresight_device *csdev,
				 struct coresight_device *parent,
				 struct coresight_device *child,
				 struct list_head *path)
{
	int ret = 0;
	int link_subtype;
	int inport, outport;

	if (!parent || !child)
		return -EINVAL;

	inport = coresight_find_link_inport(csdev, parent, path);
	outport = coresight_find_link_outport(csdev, child, path);
	link_subtype = csdev->subtype.link_subtype;

	if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_MERG && inport < 0)
		return inport;
	if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_SPLIT && outport < 0)
		return outport;

	if (link_ops(csdev)->enable) {
		coresight_enable_reg_clk(csdev);
		ret = link_ops(csdev)->enable(csdev, inport, outport);
	}
	if (!ret)
		csdev->enable = true;
	else
		coresight_disable_reg_clk(csdev);

	return ret;
}

static void coresight_disable_link(struct coresight_device *csdev,
				   struct coresight_device *parent,
				   struct coresight_device *child,
				   struct list_head *path)
{
	int i, nr_conns;
	int link_subtype;
	int inport, outport;

	if (!parent || !child)
		return;

	inport = coresight_find_link_inport(csdev, parent, path);
	outport = coresight_find_link_outport(csdev, child, path);
	link_subtype = csdev->subtype.link_subtype;

	if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_MERG) {
		nr_conns = csdev->pdata->nr_inport;
	} else if (link_subtype == CORESIGHT_DEV_SUBTYPE_LINK_SPLIT) {
		nr_conns = csdev->pdata->nr_outport;
	} else {
		nr_conns = 1;
	}

	if (link_ops(csdev)->disable) {
		link_ops(csdev)->disable(csdev, inport, outport);
		coresight_disable_reg_clk(csdev);
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
			coresight_enable_reg_clk(csdev);
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

/*
 * coresight_disable_path_from : Disable components in the given path beyond
 * @nd in the list. If @nd is NULL, all the components, except the SOURCE are
 * disabled.
 */
static void coresight_disable_path_from(struct list_head *path,
					struct coresight_node *nd)
{
	u32 type;
	struct coresight_device *csdev, *parent, *child;

	if (!nd)
		nd = list_first_entry(path, struct coresight_node, link);

	list_for_each_entry_continue(nd, path, link) {
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
			/*
			 * We skip the first node in the path assuming that it
			 * is the source. So we don't expect a source device in
			 * the middle of a path.
			 */
			WARN_ON(1);
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

void coresight_disable_path(struct list_head *path)
{
	coresight_disable_path_from(path, NULL);
}

int coresight_enable_path(struct list_head *path, u32 mode, void *sink_data)
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
			ret = coresight_enable_sink(csdev, mode, sink_data);
			/*
			 * Sink is the first component turned on. If we
			 * failed to enable the sink, there are no components
			 * that need disabling. Disabling the path here
			 * would mean we could disrupt an existing session.
			 */
			if (ret)
				goto out;
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
	coresight_disable_path_from(path, nd);
	goto out;
}

static struct coresight_device *coresight_get_source(struct list_head *path)
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

static int coresight_enabled_sink(struct device *dev, const void *data)
{
	const bool *reset = data;
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

static int coresight_sink_by_id(struct device *dev, const void *data)
{
	struct coresight_device *csdev = to_coresight_device(dev);
	unsigned long hash;

	if (csdev->type == CORESIGHT_DEV_TYPE_SINK ||
	     csdev->type == CORESIGHT_DEV_TYPE_LINKSINK) {

		if (!csdev->ea)
			return 0;
		/*
		 * See function etm_perf_add_symlink_sink() to know where
		 * this comes from.
		 */
		hash = (unsigned long)csdev->ea->var;

		if ((u32)hash == *(u32 *)data)
			return 1;
	}

	return 0;
}

/**
 * coresight_get_sink_by_id - returns the sink that matches the id
 * @id: Id of the sink to match
 *
 * The name of a sink is unique, whether it is found on the AMBA bus or
 * otherwise.  As such the hash of that name can easily be used to identify
 * a sink.
 */
struct coresight_device *coresight_get_sink_by_id(u32 id)
{
	struct device *dev = NULL;

	dev = bus_find_device(&coresight_bustype, NULL, &id,
			      coresight_sink_by_id);

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

	for (i = 0; i < csdev->pdata->nr_outport; i++) {
		struct coresight_device *child;

		child  = csdev->pdata->conns[i].child_dev;
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
	for (i = 0; i < csdev->pdata->nr_outport; i++) {
		struct coresight_device *child;

		child  = csdev->pdata->conns[i].child_dev;
		if (child && child->type == CORESIGHT_DEV_TYPE_HELPER)
			pm_runtime_put(child->dev.parent);
	}
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
	for (i = 0; i < csdev->pdata->nr_outport; i++) {
		struct coresight_device *child_dev;

		child_dev = csdev->pdata->conns[i].child_dev;
#ifdef CONFIG_CORESIGHT_QGKI
		if (csdev->pdata->conns[i].source_name &&
		    strcmp(csdev->pdata->conns[i].source_name,
				dev_name(&source->dev)))
			continue;
#endif
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
 *
 * Go through all the elements of a path and 1) removed it from the list and
 * 2) free the memory allocated for each node.
 */
void coresight_release_path(struct list_head *path)
{
	struct coresight_device *csdev;
	struct coresight_node *nd, *next;

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

static int coresight_store_path(struct list_head *path)
{
	struct coresight_path *node;

	node = kzalloc(sizeof(struct coresight_path), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->path = path;
	list_add(&node->link, &cs_active_paths);

	return 0;
}

static int coresight_enable_source_link(struct list_head *path)
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

	return 0;
err:
	coresight_disable_path_from(path, nd);
	return ret;
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
	struct list_head *path = NULL;
	struct coresight_path *cspath = NULL;
	struct coresight_path *cspath_next = NULL;
	int ret;


	mutex_lock(&coresight_mutex);

	list_for_each_entry_safe(cspath, cspath_next, &cs_active_paths, link) {
		ret = coresight_enable_source_link(cspath->path);
		if (ret) {
			path = cspath->path;
			list_del(&cspath->link);
			kfree(cspath);
			coresight_release_path(path);
		}
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

	ret = coresight_enable_path(path, CS_MODE_SYSFS, NULL);
	if (ret)
		goto err_path;

	ret = coresight_enable_source(csdev, CS_MODE_SYSFS);
	if (ret)
		goto err_source;

	ret = coresight_store_path(path);
	if (ret)
		goto err_source;

out:
	mutex_unlock(&coresight_mutex);
	return ret;

err_source:
	coresight_disable_path(path);

err_path:
	coresight_release_path(path);
	goto out;
}
EXPORT_SYMBOL_GPL(coresight_enable);

static void __coresight_disable(struct coresight_device *csdev)
{
	int  ret;
	struct list_head *path = NULL;
	struct coresight_path *cspath = NULL;
	struct coresight_path *cspath_next = NULL;
	struct coresight_device *src_csdev = NULL;

	ret = coresight_validate_source(csdev, __func__);
	if (ret)
		return;

	if (!csdev->enable || !coresight_disable_source(csdev))
		return;

	list_for_each_entry_safe(cspath, cspath_next, &cs_active_paths, link) {
		src_csdev = coresight_get_source(cspath->path);
		if (!src_csdev)
			continue;
		if (src_csdev == csdev) {
			path = cspath->path;
			list_del(&cspath->link);
			kfree(cspath);
		}
	}
	if (path == NULL)
		return;

	coresight_disable_path(path);
	coresight_release_path(path);
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

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;
	mutex_lock(&coresight_mutex);

	if (val) {
		if (activated_sink)
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

	fwnode_handle_put(csdev->dev.fwnode);
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
	for (i = 0; i < i_csdev->pdata->nr_outport; i++) {
		conn = &i_csdev->pdata->conns[i];

		/* Skip the port if FW doesn't describe it */
		if (!conn->child_fwnode)
			continue;

		/* We have found at least one orphan connection */
		if (conn->child_dev == NULL) {
			/* Does it match this newly added device? */
			if (conn->child_fwnode == csdev->dev.fwnode)
				conn->child_dev = csdev;
			else
				/* This component still has an orphan */
				still_orphan = true;
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

	for (i = 0; i < csdev->pdata->nr_outport; i++) {
		struct coresight_connection *conn = &csdev->pdata->conns[i];
		struct device *dev = NULL;

		if (!conn->child_fwnode)
			continue;
		dev = bus_find_device_by_fwnode(&coresight_bustype, conn->child_fwnode);
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
	for (i = 0; i < iterator->pdata->nr_outport; i++) {
		conn = &iterator->pdata->conns[i];

		if (conn->child_dev == NULL || conn->child_fwnode == NULL)
			continue;

		if (csdev->dev.fwnode == conn->child_fwnode) {
			iterator->orphan = true;
			conn->child_dev = NULL;
			/*
			 * Drop the reference to the handle for the remote
			 * device acquired in parsing the connections from
			 * platform data.
			 */
			fwnode_handle_put(conn->child_fwnode);
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

/*
 * coresight_remove_conns - Remove references to this given devices
 * from the connections of other devices.
 */
static void coresight_remove_conns(struct coresight_device *csdev)
{
	/*
	 * Another device will point to this device only if there is
	 * an output port connected to this one. i.e, if the device
	 * doesn't have at least one input port, there is no point
	 * in searching all the devices.
	 */
	if (csdev->pdata->nr_inport)
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

/*
 * coresight_release_platform_data: Release references to the devices connected
 * to the output port of this device.
 */
void coresight_release_platform_data(struct coresight_platform_data *pdata)
{
	int i;

	for (i = 0; i < pdata->nr_outport; i++) {
		if (pdata->conns[i].child_fwnode) {
			fwnode_handle_put(pdata->conns[i].child_fwnode);
			pdata->conns[i].child_fwnode = NULL;
		}
	}
}

struct coresight_device *coresight_register(struct coresight_desc *desc)
{
	int ret;
	int link_subtype;
	int nr_refcnts = 1;
	atomic_t *refcnts = NULL;
	struct coresight_device *csdev;
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
		goto err_out;
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
		goto err_free_csdev;
	}

	csdev->refcnt = refcnts;

	csdev->pdata = desc->pdata;

	csdev->type = desc->type;
	csdev->subtype = desc->subtype;
	csdev->ops = desc->ops;
	csdev->orphan = false;
#ifdef CONFIG_CORESIGHT_QGKI
	csdev->reg_clk = desc->pdata->reg_clk;
#endif

	csdev->dev.type = &coresight_dev_type[desc->type];
	csdev->dev.groups = desc->groups;
	csdev->dev.parent = desc->dev;
	csdev->dev.release = coresight_device_release;
	csdev->dev.bus = &coresight_bustype;
	/*
	 * Hold the reference to our parent device. This will be
	 * dropped only in coresight_device_release().
	 */
	csdev->dev.fwnode = fwnode_handle_get(dev_fwnode(desc->dev));
	dev_set_name(&csdev->dev, "%s", desc->name);

	ret = device_register(&csdev->dev);
	if (ret) {
		put_device(&csdev->dev);
		/*
		 * All resources are free'd explicitly via
		 * coresight_device_release(), triggered from put_device().
		 */
		goto err_out;
	}

	if (csdev->type == CORESIGHT_DEV_TYPE_SINK ||
	    csdev->type == CORESIGHT_DEV_TYPE_LINKSINK) {
		ret = etm_perf_add_symlink_sink(csdev);

		if (ret) {
			device_unregister(&csdev->dev);
			/*
			 * As with the above, all resources are free'd
			 * explicitly via coresight_device_release() triggered
			 * from put_device(), which is in turn called from
			 * function device_unregister().
			 */
			goto err_out;
		}
	}

	mutex_lock(&coresight_mutex);

	coresight_fixup_device_conns(csdev);
	coresight_fixup_orphan_conns(csdev);

	mutex_unlock(&coresight_mutex);

	return csdev;

err_free_csdev:
	kfree(csdev);
err_out:
	/* Cleanup the connection information */
	coresight_release_platform_data(desc->pdata);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(coresight_register);

void coresight_unregister(struct coresight_device *csdev)
{
	etm_perf_del_symlink_sink(csdev);
	/* Remove references of that device in the topology */
	coresight_remove_conns(csdev);
	coresight_release_platform_data(csdev->pdata);
	device_unregister(&csdev->dev);
}
EXPORT_SYMBOL_GPL(coresight_unregister);


/*
 * coresight_search_device_idx - Search the fwnode handle of a device
 * in the given dev_idx list. Must be called with the coresight_mutex held.
 *
 * Returns the index of the entry, when found. Otherwise, -ENOENT.
 */
static inline int coresight_search_device_idx(struct coresight_dev_list *dict,
					      struct fwnode_handle *fwnode)
{
	int i;

	for (i = 0; i < dict->nr_idx; i++)
		if (dict->fwnode_list[i] == fwnode)
			return i;
	return -ENOENT;
}

/*
 * coresight_alloc_device_name - Get an index for a given device in the
 * device index list specific to a driver. An index is allocated for a
 * device and is tracked with the fwnode_handle to prevent allocating
 * duplicate indices for the same device (e.g, if we defer probing of
 * a device due to dependencies), in case the index is requested again.
 */
const char *coresight_alloc_device_name(struct coresight_dev_list *dict,
				  struct device *dev)
{
	int idx;
	const char *name = NULL;
	struct fwnode_handle **list;
	struct device_node *node = dev->of_node;

	if (!of_property_read_string(node, "coresight-name", &name))
		return name;

	mutex_lock(&coresight_mutex);

	idx = coresight_search_device_idx(dict, dev_fwnode(dev));
	if (idx < 0) {
		/* Make space for the new entry */
		idx = dict->nr_idx;
		list = krealloc(dict->fwnode_list,
				(idx + 1) * sizeof(*dict->fwnode_list),
				GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(list)) {
			idx = -ENOMEM;
			goto done;
		}

		list[idx] = dev_fwnode(dev);
		dict->fwnode_list = list;
		dict->nr_idx = idx + 1;
	}

	name = devm_kasprintf(dev, GFP_KERNEL, "%s%d", dict->pfx, idx);
done:
	mutex_unlock(&coresight_mutex);
	return name;
}
EXPORT_SYMBOL_GPL(coresight_alloc_device_name);
