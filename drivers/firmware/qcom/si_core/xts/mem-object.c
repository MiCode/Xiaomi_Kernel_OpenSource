// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "si-mo: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#include <linux/mem-buf.h>
#include <linux/of_platform.h>
#include <linux/qtee_shmbridge.h>
#include <linux/firmware/qcom/si_core_xts.h>

/* Memory object operations. */
/* ... */

/* 'Primordial Object' operations related to memory object. */
#define OBJECT_OP_MAP_REGION	0

/* Auto mapping operation. */
#define OBJECT_OP_AUTO_MAP 0x00000003UL

#define SMCINVOKE_ASYNC_VERSION 0x00010002U

static struct platform_device *mem_object_pdev;

static struct si_object primordial_object;

/* **/
/* Memory object reference counting details:
 * There is one reference counter in memory object, i.e. 'object'.
 * 'object' counts number of times this object has been exported to TZ plus
 * total number of mappings plus one (for ownership reference).
 *
 * HOW IT WORKS
 *
 * Client obtains an instance of 'si_object' by calling 'init_si_mem_object_user'
 * with an instance of 'struct dma_buf' to initialize a memory object. It can
 * immediately use this instance of 'si_object' to share memory with TZ.
 * However, by transferring this object to TZ, client will lose it's ownership.
 * To retain the ownership it should call 'get_si_object' and send a second
 * instance of this object to TZ while keeping the initial 'si_object' instance
 * (hence plus one for ownership).
 *
 * Every time TZ request mapping of the memory object, the driver issues
 * 'get_si_object' on 'object'.
 *
 **/

struct mem_object {
	struct si_object object;

	struct dma_buf *dma_buf;

	union {
		struct {

			/* SHMBridge information. */
			/* Select with 'qcom,shmbridge'. */

			struct map {
				struct dma_buf_attachment *buf_attach;
				struct sg_table *sgt;

				/* 'lock' to protect concurrent request from QTEE. */
				struct mutex lock;
				int early_mapped;
			} map;

			/* Use SHMBridge, hence the handle. */
			u64 shm_bridge_handle;

			struct mapping_info {
				phys_addr_t p_addr;
				size_t p_addr_len;
			} mapping_info;
		};

		/* XXX information. */
		/* struct { ... } */
	};

	struct list_head node;

	/* Private pointer passed for callbacks. */

	void *private;

	void (*release)(void *private);
};

#define to_mem_object(o) container_of((o), struct mem_object, object)

/* List of memory objects. Only used for sysfs. */

static LIST_HEAD(mo_list);
static DEFINE_MUTEX(mo_list_mutex);

/* 'mo_notify' and 'mo_dispatch' are shared by all types of memory objects. */

static void mo_notify(unsigned int context_id, struct si_object *object, int status)
{

}

static int mo_dispatch(unsigned int context_id,
	struct si_object *object, unsigned long op, struct si_arg args[])
{
	return 0;
}

static struct si_object_operations mem_ops = {
	.notify = mo_notify,
	.dispatch = mo_dispatch
};

int op_supported(unsigned long op)
{
	switch (op) {
	case OBJECT_OP_MAP_REGION:
		return 1;
	default:
		return 0;
	}
}

/** Support for 'SHMBridge'. **/

/* 'make_shm_bridge_single' only support single continuous memory. */

static int make_shm_bridge_single(struct mem_object *mo)
{
	int ret;

	u32 *vmid_list, *perms_list, nelems;

	/* 'sgt' should have one mapped entry. **/

	if (mo->map.sgt->nents != 1)
		return -EINVAL;

	ret = mem_buf_dma_buf_copy_vmperm(mo->dma_buf,
		(int **)(&vmid_list),
		(int **)(&perms_list),
		(int *)(&nelems));

	if (ret)
		return ret;

	if (mem_buf_dma_buf_exclusive_owner(mo->dma_buf))
		perms_list[0] = PERM_READ | PERM_WRITE;

	mo->mapping_info.p_addr = sg_dma_address(mo->map.sgt->sgl);
	mo->mapping_info.p_addr_len = sg_dma_len(mo->map.sgt->sgl);

	ret = qtee_shmbridge_register(mo->mapping_info.p_addr, mo->mapping_info.p_addr_len,
		vmid_list, perms_list, nelems, PERM_READ | PERM_WRITE,
		&mo->shm_bridge_handle);

	kfree(perms_list);
	kfree(vmid_list);

	if (ret) {

		/* If 'p_addr' is not zero, then the memory object is already mapped. */

		mo->mapping_info.p_addr = 0;
		mo->mapping_info.p_addr_len = 0;
		// SCM driver touch this value even an failure so set to 0
		mo->shm_bridge_handle = 0;
	}

	return ret;
}

static void rm_shm_bridge(struct mem_object *mo)
{
	if (mo->shm_bridge_handle)
		qtee_shmbridge_deregister(mo->shm_bridge_handle);
	mo->shm_bridge_handle = 0;
}

static void detach_dma_buf(struct mem_object *mo)
{
	if (mo->map.sgt) {
		dma_buf_unmap_attachment_unlocked(mo->map.buf_attach,
			mo->map.sgt, DMA_BIDIRECTIONAL);
	}

	if (mo->map.buf_attach)
		dma_buf_detach(mo->dma_buf, mo->map.buf_attach);

	mo->map.buf_attach = NULL;
	mo->map.sgt = NULL;
}

/* 'init_tz_shared_memory' is called while holding the 'map.lock' mutex. */

static int init_tz_shared_memory(struct mem_object *mo)
{
	int ret;
	struct dma_buf_attachment *buf_attach;
	struct sg_table *sgt;

	mo->map.buf_attach = NULL;
	mo->map.sgt = NULL;
	mo->shm_bridge_handle = 0;

	buf_attach = dma_buf_attach(mo->dma_buf, &mem_object_pdev->dev);
	if (IS_ERR(buf_attach))
		return PTR_ERR(buf_attach);

	mo->map.buf_attach = buf_attach;

	sgt = dma_buf_map_attachment_unlocked(buf_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);

		goto out_failed;
	}

	mo->map.sgt = sgt;

	ret = make_shm_bridge_single(mo);
	if (ret)
		goto out_failed;

	return 0;

out_failed:
	detach_dma_buf(mo);

	return ret;
}

static int map_memory_obj(struct mem_object *mo, int advisory)
{
	int ret;

	if (mo->map.early_mapped)
		pr_debug("%s auto-mapped. Memory optimization unavailable.\n",
			si_object_name(&mo->object));

	mutex_lock(&mo->map.lock);
	if (mo->mapping_info.p_addr == 0) {

		/* 'mo' has not been mapped before. Do it now. */

		ret = init_tz_shared_memory(mo);

	} else {

		/* 'mo' is already mapped. Just return. */

		ret = advisory;
	}

	mutex_unlock(&mo->map.lock);

	return ret;
}

static void release_memory_obj(struct mem_object *mo)
{
	rm_shm_bridge(mo);

	detach_dma_buf(mo);
}

static unsigned long mo_shm_bridge_prepare(struct si_object *object, struct si_arg args[])
{
	struct mem_object *mo = to_mem_object(object);

	struct {
		u64 p_addr;
		u64 len;
		u32 perms;
	} *mi;

	if (get_async_proto_version() != SMCINVOKE_ASYNC_VERSION)
		return SI_OBJECT_OP_NO_OP;

	if (args[0].b.size < sizeof(*mi))
		return SI_OBJECT_OP_NO_OP;

	if (!map_memory_obj(mo, 1)) {
		mo->map.early_mapped = 1;

		/* 'object' has been mapped. Share it. */

		get_si_object(object);

		mi = (typeof(mi)) (args[0].b.addr);
		mi->p_addr = mo->mapping_info.p_addr;
		mi->len = mo->mapping_info.p_addr_len;
		mi->perms = 6; /* RW Permission. */
		args[0].b.size = sizeof(*mi);

		args[1].o = object;

		return OBJECT_OP_AUTO_MAP;
	}

	return SI_OBJECT_OP_NO_OP;
}

static void mo_shm_bridge_release(struct si_object *object)
{
	struct mem_object *mo = to_mem_object(object);

	release_memory_obj(mo);

	if (mo->release)
		mo->release(mo->private);

	/* Put a dam-buf copy obtained in 'init_si_mem_object_user'.*/

	dma_buf_put(mo->dma_buf);

	mutex_lock(&mo_list_mutex);
	list_del(&mo->node);
	mutex_unlock(&mo_list_mutex);

	pr_info("%s unmapped.\n", si_object_name(object));

	kfree(mo);
}

/* Primordial object for 'SHMBridge'. */

static int shm_bridge__po_dispatch(unsigned int context_id,
	struct si_object *unused, unsigned long op, struct si_arg args[])
{
	int ret;

	struct si_object *object;
	struct mem_object *mo;

	switch (op) {
	case OBJECT_OP_MAP_REGION: {

		/* Format of response as expected by TZ. */

		struct {
			u64 p_addr;
			u64 len;
			u32 perms;
		} *mi;

		if (size_of_arg(args) != 3 ||
			args[0].type != SI_AT_OB  ||
			args[1].type != SI_AT_IO  ||
			args[2].type != SI_AT_OO) {

			pr_err("mapping of a memory object with invalid message format.\n");

			return -EINVAL;
		}

		object = args[1].o;

		if (!is_mem_object(object)) {
			pr_err("mapping of a non-memory object.\n");

			put_si_object(object);
			return -EINVAL;
		}

		mo = to_mem_object(object);

		ret = map_memory_obj(mo, 0);
		if (!ret) {

			/* 'object' has been mapped. Share it. */

			args[2].o = object;

			mi = (typeof(mi)) (args[0].b.addr);
			mi->p_addr = mo->mapping_info.p_addr;
			mi->len = mo->mapping_info.p_addr_len;
			mi->perms = 6; /* RW Permission. */

			pr_info("%s mapped %llx %llx\n",
				si_object_name(object), mi->p_addr, mi->len);

		} else {
			pr_err("mapping memory object %s failed.\n",
				si_object_name(object));

			put_si_object(object);
		}
	}

		break;
	default:

		/* The operation is not supported! */

		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct si_object_operations shm_bridge__po_ops = {
	.op_supported = op_supported,
	.dispatch = shm_bridge__po_dispatch
};

/* Memory Object Extension. */

struct si_object *init_si_mem_object_user(struct dma_buf *dma_buf,
	void (*release)(void *), void *private)
{
	struct mem_object *mo;

	if (!mem_ops.release) {
		pr_err("memory object type is unknown.\n");

		return NULL_SI_OBJECT;
	}

	mo = kzalloc(sizeof(*mo), GFP_KERNEL);
	if (!mo)
		return NULL_SI_OBJECT;

	mutex_init(&mo->map.lock);

	/* Get a copy of dma-buf. */
	get_dma_buf(dma_buf);

	mo->dma_buf = dma_buf;
	mo->private = private;
	mo->release = release;

	init_si_object_user(&mo->object, SI_OT_CB_OBJECT, &mem_ops, "mem-object-%p", dma_buf);

	mutex_lock(&mo_list_mutex);
	list_add_tail(&mo->node, &mo_list);
	mutex_unlock(&mo_list_mutex);

	return &mo->object;
}
EXPORT_SYMBOL_GPL(init_si_mem_object_user);

struct dma_buf *mem_object_to_dma_buf(struct si_object *object)
{
	if (is_mem_object(object))
		return to_mem_object(object)->dma_buf;

	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL_GPL(mem_object_to_dma_buf);

int is_mem_object(struct si_object *object)
{
	/* Check 'typeof_si_object' to make sure 'object''s 'ops' has been
	 * initialized before checking it.
	 */

	return (typeof_si_object(object) == SI_OT_CB_OBJECT) &&
		(object->ops == &mem_ops);
}
EXPORT_SYMBOL_GPL(is_mem_object);

static ssize_t mem_objects_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	size_t len = 0;
	struct mem_object *mo;

	mutex_lock(&mo_list_mutex);
	list_for_each_entry(mo, &mo_list, node) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s %u (%llx %zx) %d\n",
			si_object_name(&mo->object), kref_read(&mo->object.refcount),
			mo->mapping_info.p_addr, mo->mapping_info.p_addr_len, mo->map.early_mapped);
	}

	mutex_unlock(&mo_list_mutex);

	return len;
}

/* 'struct device_attribute dev_attr_mem_objects'. */
/* Use device attribute rather than driver attribute in case we want to support
 * multiple types of memory objects as different devices.
 */

static DEVICE_ATTR_RO(mem_objects);

static struct attribute *attrs[] = {
	&dev_attr_mem_objects.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static const struct attribute_group *attr_groups[] = {
	&attr_group,
	NULL
};

static int mem_object_probe(struct platform_device *pdev)
{
	int ret;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret)
		return ret;

	/* Select memory object type: default to SHMBridge. */
	mem_ops.release = mo_shm_bridge_release;
	mem_ops.prepare = mo_shm_bridge_prepare;

	init_si_object_user(&primordial_object,
		SI_OT_ROOT, &shm_bridge__po_ops, "po_in_mem_object");

	mem_object_pdev = pdev;

	return 0;
}

static const struct of_device_id mem_object_match[] = {
	{ .compatible = "qcom,mem-object", }, {}
};

static struct platform_driver mem_object_plat_driver = {
	.probe = mem_object_probe,
	.driver = {
		.name = "mem-object",
		.dev_groups = attr_groups,
		.of_match_table = mem_object_match,
	},
};

module_platform_driver(mem_object_plat_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Memory object driver");
MODULE_IMPORT_NS(DMA_BUF);
