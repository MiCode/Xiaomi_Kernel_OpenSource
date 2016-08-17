/*
 * Copyright (C) 2011 NVIDIA Corp.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include <linux/tegra_mediaserver.h>
#include "../avp/nvavp.h"
#include "../../../../video/tegra/nvmap/nvmap.h"

#define CHECK_STATUS(e, tag) \
	do { if (e < 0) goto tag; } while (0)

#define CHECK_NULL(ptr, tag) \
	do { if (!ptr) goto tag; } while (0)

#define CHECK_CONDITION(c, tag) \
	do { if (c) goto tag; } while (0)

struct tegra_mediasrv_block {
	struct list_head entry;
	struct tegra_mediaserver_block_info block;
};

struct tegra_mediasrv_iram {
	struct list_head entry;
	struct tegra_mediaserver_iram_info iram;
};

struct tegra_mediasrv_node {
	struct tegra_mediasrv_info *mediasrv;
	struct list_head blocks;
	int nr_iram_shared;
};

struct tegra_mediasrv_manager {
	struct tegra_avp_lib   lib;
	struct tegra_rpc_info  *rpc;
	struct tegra_sema_info *sema;
};

struct tegra_mediasrv_info {
	int minor;
	struct mutex lock;
	struct nvmap_client *nvmap;
	struct tegra_avp_info *avp;
	struct tegra_mediasrv_manager manager;
	int nr_nodes;
	int nr_blocks;
	struct tegra_mediaserver_iram_info iram; /* only one supported */
	int nr_iram_shared;
};

static struct tegra_mediasrv_info *mediasrv_info;


/*
 * File entry points
 */
static int mediasrv_open(struct inode *inode, struct file *file)
{
	struct tegra_mediasrv_info *mediasrv = mediasrv_info;
	struct tegra_mediasrv_node *node = NULL;
	struct tegra_mediasrv_manager *manager = &mediasrv->manager;
	struct tegra_avp_lib *lib = &manager->lib;
	int e;

	node = kzalloc(sizeof(struct tegra_mediasrv_node), GFP_KERNEL);
	CHECK_NULL(node, node_alloc_fail);
	INIT_LIST_HEAD(&node->blocks);
	node->mediasrv = mediasrv;

	mutex_lock(&mediasrv->lock);
	nonseekable_open(inode, file);

	if (!mediasrv->nr_nodes) {
		e = tegra_sema_open(&manager->sema);
		CHECK_STATUS(e, fail);

		e = tegra_rpc_open(&manager->rpc);
		CHECK_STATUS(e, fail);

		e = tegra_rpc_port_create(manager->rpc, "NVMM_MANAGER_SRV",
					  manager->sema);
		CHECK_STATUS(e, fail);

		e = tegra_avp_open(&mediasrv->avp);
		CHECK_STATUS(e, fail);

		memcpy(lib->name, "nvmm_manager.axf\0",
			   strlen("nvmm_manager.axf") + 1);
		lib->args = &mediasrv;
		lib->args_len = sizeof(unsigned long);
		e = tegra_avp_load_lib(mediasrv->avp, lib);
		CHECK_STATUS(e, fail);

		e = tegra_rpc_port_connect(manager->rpc, 50000);
		CHECK_STATUS(e, fail);
	}

	mediasrv->nr_nodes++;
	try_module_get(THIS_MODULE);

	mutex_unlock(&mediasrv->lock);

	file->private_data = node;

	return 0;

fail:
	if (lib->handle) {
		tegra_avp_unload_lib(mediasrv->avp, lib->handle);
		lib->handle = 0;
	}

	if (mediasrv->avp) {
		tegra_avp_release(mediasrv->avp);
		mediasrv->avp = NULL;
	}

	if (manager->rpc) {
		tegra_rpc_release(manager->rpc);
		manager->rpc = NULL;
	}
	if (manager->sema) {
		tegra_sema_release(manager->sema);
		manager->sema = NULL;
	}

	kfree(node);

	mutex_unlock(&mediasrv->lock);
	return e;

node_alloc_fail:
	e = -ENOMEM;
	return e;
}

static int mediasrv_release(struct inode *inode, struct file *file)
{
	struct tegra_mediasrv_info *mediasrv = mediasrv_info;
	struct tegra_mediasrv_node *node = file->private_data;
	struct tegra_mediasrv_block *block;
	struct list_head *entry;
	struct list_head *temp;
	u32 message[2];
	int e;

	mutex_lock(&mediasrv->lock);

	list_for_each_safe(entry, temp, &node->blocks) {
		block = list_entry(entry, struct tegra_mediasrv_block, entry);

		pr_info("Improperly closed block found!");
		pr_info("  NVMM Block Handle: 0x%08x\n",
			block->block.nvmm_block_handle);
		pr_info("  AVP Block Handle: 0x%08x\n",
			block->block.avp_block_handle);

		message[0] = 1; /* NvmmManagerMsgType_AbnormalTerm */
		message[1] = block->block.avp_block_handle;

		e = tegra_rpc_write(mediasrv->manager.rpc, (u8 *)message,
				    sizeof(u32) * 2);
		pr_info("Abnormal termination message result: %d\n", e);

		if (block->block.avp_block_library_handle) {
			e = tegra_avp_unload_lib(mediasrv->avp,
				block->block.avp_block_library_handle);
			pr_info("Unload block (0x%08x) result: %d\n",
				block->block.avp_block_library_handle, e);
		}

		if (block->block.service_library_handle) {
			e = tegra_avp_unload_lib(mediasrv->avp,
				block->block.service_library_handle);
			pr_info("Unload service (0x%08x) result: %d\n",
				block->block.service_library_handle, e);
		}

		mediasrv->nr_blocks--;
		list_del(entry);
		kfree(block);
	}

	mediasrv->nr_iram_shared -= node->nr_iram_shared;
	if (mediasrv->iram.rm_handle && !mediasrv->nr_iram_shared) {
		pr_info("Improperly freed shared iram found!");
		nvmap_unpin_ids(mediasrv->nvmap, 1, &mediasrv->iram.rm_handle);
		nvmap_free_handle_id(mediasrv->nvmap, mediasrv->iram.rm_handle);
		mediasrv->iram.rm_handle = 0;
		mediasrv->iram.physical_address = 0;
	}

	kfree(node);
	mediasrv->nr_nodes--;
	if (!mediasrv->nr_nodes) {
		struct tegra_mediasrv_manager *manager = &mediasrv->manager;

		tegra_avp_unload_lib(mediasrv->avp, manager->lib.handle);
		manager->lib.handle = 0;

		tegra_avp_release(mediasrv->avp);
		mediasrv->avp = NULL;

		tegra_rpc_release(manager->rpc);
		manager->rpc = NULL;

		tegra_sema_release(manager->sema);
		manager->sema = NULL;
	}

	mutex_unlock(&mediasrv->lock);
	module_put(THIS_MODULE);
	return 0;
}

static int mediasrv_alloc(struct tegra_mediasrv_node *node,
			  union tegra_mediaserver_alloc_info *in,
			  union tegra_mediaserver_alloc_info *out)
{
	struct tegra_mediasrv_info *mediasrv = node->mediasrv;
	int e;

	switch (in->in.tegra_mediaserver_resource_type) {
	case TEGRA_MEDIASERVER_RESOURCE_BLOCK:
	{
		struct tegra_mediasrv_block *block;

		block = kzalloc(sizeof(struct tegra_mediasrv_block),
				GFP_KERNEL);
		CHECK_NULL(block, block_alloc_fail);

		block->block = in->in.u.block;
		list_add(&block->entry, &node->blocks);
		goto block_done;

block_alloc_fail:
		e = -ENOMEM;
	    goto fail;

block_done:
		mediasrv->nr_blocks++;
		out->out.u.block.count = mediasrv->nr_blocks;
	}
	break;

	case TEGRA_MEDIASERVER_RESOURCE_IRAM:
	{
		if (in->in.u.iram.tegra_mediaserver_iram_type ==
		    TEGRA_MEDIASERVER_IRAM_SHARED) {
			if (!mediasrv->nr_iram_shared) {
				size_t align, size;
				struct nvmap_handle_ref *r = NULL;
				unsigned long id;
				int physical_address;

				size = PAGE_ALIGN(in->in.u.iram.size);
				r = nvmap_create_handle(mediasrv->nvmap, size);
				CHECK_CONDITION((r < 0),
						iram_shared_handle_fail);

				id = nvmap_ref_to_id(r);

				align = max_t(size_t, in->in.u.iram.alignment,
					      PAGE_SIZE);
				e = nvmap_alloc_handle_id(mediasrv->nvmap, id,
					NVMAP_HEAP_CARVEOUT_IRAM, align,
					NVMAP_HANDLE_WRITE_COMBINE);
				CHECK_STATUS(e, iram_shared_alloc_fail);

				physical_address =
					nvmap_pin_ids(mediasrv->nvmap, 1, &id);
				CHECK_CONDITION((physical_address < 0),
						iram_shared_pin_fail);

				mediasrv->iram.rm_handle = id;
				mediasrv->iram.physical_address =
					physical_address;
				goto iram_shared_done;

iram_shared_pin_fail:
				e = physical_address;
iram_shared_alloc_fail:
				nvmap_free_handle_id(mediasrv->nvmap, id);
iram_shared_handle_fail:
				goto fail;
			}

iram_shared_done:
			out->out.u.iram.rm_handle = mediasrv->iram.rm_handle;
			out->out.u.iram.physical_address =
				mediasrv->iram.physical_address;
			mediasrv->nr_iram_shared++;
			node->nr_iram_shared++;
		} else if (in->in.u.iram.tegra_mediaserver_iram_type ==
			   TEGRA_MEDIASERVER_IRAM_SCRATCH) {
			e = -EINVAL;
			goto fail;
		}
	}
	break;

	default:
	{
		e = -EINVAL;
		goto fail;
	}
	break;
	}

	return 0;

fail:
	return e;
}

static void mediasrv_free(struct tegra_mediasrv_node *node,
			  union tegra_mediaserver_free_info *in)
{
	struct tegra_mediasrv_info *mediasrv = node->mediasrv;

	switch (in->in.tegra_mediaserver_resource_type) {
	case TEGRA_MEDIASERVER_RESOURCE_BLOCK:
	{
		struct tegra_mediasrv_block *block = NULL;
		struct tegra_mediasrv_block *temp;
		struct list_head *entry;

		list_for_each(entry, &node->blocks) {
			temp = list_entry(entry, struct tegra_mediasrv_block,
					  entry);
			if (temp->block.nvmm_block_handle !=
			    in->in.u.nvmm_block_handle)
				continue;

			block = temp;
			break;
		}

		CHECK_NULL(block, done);
		list_del(&block->entry);
		kfree(block);
	}
	break;

	case TEGRA_MEDIASERVER_RESOURCE_IRAM:
	{
		if (in->in.u.iram_rm_handle == mediasrv->iram.rm_handle &&
		    node->nr_iram_shared) {
			node->nr_iram_shared--;
			mediasrv->nr_iram_shared--;

			if (!mediasrv->nr_iram_shared) {
				nvmap_unpin_ids(mediasrv->nvmap, 1,
						&mediasrv->iram.rm_handle);
				nvmap_free_handle_id(mediasrv->nvmap,
						     mediasrv->iram.rm_handle);
				mediasrv->iram.rm_handle = 0;
				mediasrv->iram.physical_address = 0;
			}
		}

		else
			goto done;
	}
	break;
	}

done:
	return;
}

static int mediasrv_update_block_info(
	struct tegra_mediasrv_node *node,
	union tegra_mediaserver_update_block_info *in
)
{
	struct tegra_mediasrv_block *entry = NULL;
	struct tegra_mediasrv_block *block = NULL;
	int e;

	list_for_each_entry(entry, &node->blocks, entry) {
		if (entry->block.nvmm_block_handle != in->in.nvmm_block_handle)
			continue;

		block = entry;
		break;
	}

	CHECK_NULL(block, fail);

	block->block = in->in;
	return 0;

fail:
	e = -EINVAL;
	return e;
}

static long mediasrv_unlocked_ioctl(struct file *file, unsigned int cmd,
				    unsigned long arg)
{
	struct tegra_mediasrv_info *mediasrv = mediasrv_info;
	struct tegra_mediasrv_node *node = file->private_data;
	int e = -ENODEV;

	mutex_lock(&mediasrv->lock);

	switch (cmd) {
	case TEGRA_MEDIASERVER_IOCTL_ALLOC:
	{
		union tegra_mediaserver_alloc_info in, out;
		e = copy_from_user(&in, (void __user *)arg, sizeof(in));
		CHECK_CONDITION(e, copy_fail);
		e = mediasrv_alloc(node, &in, &out);
		CHECK_STATUS(e, fail);
		e = copy_to_user((void __user *)arg, &out, sizeof(out));
		CHECK_CONDITION(e, copy_fail);
	}
	break;

	case TEGRA_MEDIASERVER_IOCTL_FREE:
	{
		union tegra_mediaserver_free_info in;
		e = copy_from_user(&in, (void __user *)arg, sizeof(in));
		CHECK_CONDITION(e, copy_fail);
		mediasrv_free(node, &in);
	}
	break;

	case TEGRA_MEDIASERVER_IOCTL_UPDATE_BLOCK_INFO:
	{
		union tegra_mediaserver_update_block_info in;
		e = copy_from_user(&in, (void __user *)arg, sizeof(in));
		CHECK_CONDITION(e, copy_fail);
		e = mediasrv_update_block_info(node, &in);
		CHECK_CONDITION(e, fail);
	}
	break;

	default:
	{
		e = -ENODEV;
		goto fail;
	}
	break;
	}

	mutex_unlock(&mediasrv->lock);
	return 0;

copy_fail:
	e = -EFAULT;
fail:
	mutex_unlock(&mediasrv->lock);
	return e;
}

/*
 * Kernel structures and entry points
 */
static const struct file_operations mediaserver_fops = {
	.owner			= THIS_MODULE,
	.open			= mediasrv_open,
	.release		= mediasrv_release,
	.unlocked_ioctl = mediasrv_unlocked_ioctl,
};

static struct miscdevice mediaserver_misc_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "tegra_mediaserver",
	.fops	= &mediaserver_fops,
};

static int __init tegra_mediaserver_init(void)
{
	struct tegra_mediasrv_info *mediasrv;
	int e = 0;

	CHECK_NULL(!mediasrv_info, busy);

	mediasrv = kzalloc(sizeof(struct tegra_mediasrv_info), GFP_KERNEL);
	CHECK_NULL(mediasrv, alloc_fail);

	mediasrv->nvmap = nvmap_create_client(nvmap_dev, "tegra_mediaserver");
	CHECK_NULL(mediasrv, nvmap_create_fail);

	e = misc_register(&mediaserver_misc_device);
	CHECK_STATUS(e, register_fail);

	mediasrv->nr_nodes = 0;
	mutex_init(&mediasrv->lock);

	mediasrv_info = mediasrv;
	goto done;

nvmap_create_fail:
	e = -ENOMEM;
	kfree(mediasrv);
	goto done;

register_fail:
	nvmap_client_put(mediasrv->nvmap);
	kfree(mediasrv);
	goto done;

alloc_fail:
	e = -ENOMEM;
	goto done;

busy:
	e = -EBUSY;
	goto done;

done:
	return e;
}

void __exit tegra_mediaserver_cleanup(void)
{
	struct tegra_mediasrv_info *mediasrv = mediasrv_info;
	int e;

	e = misc_deregister(&mediaserver_misc_device);
	CHECK_STATUS(e, fail);

	nvmap_client_put(mediasrv->nvmap);
	kfree(mediasrv);
	mediasrv_info = NULL;

fail:
	return;
}

module_init(tegra_mediaserver_init);
module_exit(tegra_mediaserver_cleanup);
MODULE_LICENSE("GPL");

