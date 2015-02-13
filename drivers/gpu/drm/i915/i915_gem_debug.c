/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Packard <keithp@keithp.com>
 *
 */

#include <linux/pid.h>
#include <linux/shmem_fs.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include <linux/async.h>
#include "i915_drv.h"

#if WATCH_LISTS
int
i915_verify_lists(struct drm_device *dev)
{
	static int warned;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	int err = 0;

	if (warned)
		return 0;

	list_for_each_entry(obj, &dev_priv->render_ring.active_list, list) {
		if (obj->base.dev != dev ||
		    !atomic_read(&obj->base.refcount.refcount)) {
			DRM_ERROR("freed render active %p\n", obj);
			err++;
			break;
		} else if (!obj->active ||
			   (obj->base.read_domains & I915_GEM_GPU_DOMAINS) == 0) {
			DRM_ERROR("invalid render active %p (a %d r %x)\n",
				  obj,
				  obj->active,
				  obj->base.read_domains);
			err++;
		} else if (obj->base.write_domain && list_empty(&obj->gpu_write_list)) {
			DRM_ERROR("invalid render active %p (w %x, gwl %d)\n",
				  obj,
				  obj->base.write_domain,
				  !list_empty(&obj->gpu_write_list));
			err++;
		}
	}

	list_for_each_entry(obj, &dev_priv->mm.flushing_list, list) {
		if (obj->base.dev != dev ||
		    !atomic_read(&obj->base.refcount.refcount)) {
			DRM_ERROR("freed flushing %p\n", obj);
			err++;
			break;
		} else if (!obj->active ||
			   (obj->base.write_domain & I915_GEM_GPU_DOMAINS) == 0 ||
			   list_empty(&obj->gpu_write_list)) {
			DRM_ERROR("invalid flushing %p (a %d w %x gwl %d)\n",
				  obj,
				  obj->active,
				  obj->base.write_domain,
				  !list_empty(&obj->gpu_write_list));
			err++;
		}
	}

	list_for_each_entry(obj, &dev_priv->mm.gpu_write_list, gpu_write_list) {
		if (obj->base.dev != dev ||
		    !atomic_read(&obj->base.refcount.refcount)) {
			DRM_ERROR("freed gpu write %p\n", obj);
			err++;
			break;
		} else if (!obj->active ||
			   (obj->base.write_domain & I915_GEM_GPU_DOMAINS) == 0) {
			DRM_ERROR("invalid gpu write %p (a %d w %x)\n",
				  obj,
				  obj->active,
				  obj->base.write_domain);
			err++;
		}
	}

	list_for_each_entry(obj, &i915_gtt_vm->inactive_list, list) {
		if (obj->base.dev != dev ||
		    !atomic_read(&obj->base.refcount.refcount)) {
			DRM_ERROR("freed inactive %p\n", obj);
			err++;
			break;
		} else if (obj->pin_count || obj->active ||
			   (obj->base.write_domain & I915_GEM_GPU_DOMAINS)) {
			DRM_ERROR("invalid inactive %p (p %d a %d w %x)\n",
				  obj,
				  obj->pin_count, obj->active,
				  obj->base.write_domain);
			err++;
		}
	}

	return warned = err;
}
#endif /* WATCH_LIST */

struct per_file_obj_mem_info {
	int num_obj;
	int num_obj_shared;
	int num_obj_private;
	int num_obj_gtt_bound;
	int num_obj_purged;
	int num_obj_purgeable;
	int num_obj_allocated;
	int num_obj_fault_mappable;
	int num_obj_stolen;
	size_t gtt_space_allocated_shared;
	size_t gtt_space_allocated_priv;
	size_t phys_space_allocated_shared;
	size_t phys_space_allocated_priv;
	size_t phys_space_purgeable;
	size_t phys_space_shared_proportion;
	size_t fault_mappable_size;
	size_t stolen_space_allocated;
	char *process_name;
};

struct name_entry {
	struct list_head head;
	struct drm_hash_item hash_item;
};

struct pid_stat_entry {
	struct list_head head;
	struct list_head namefree;
	struct drm_open_hash namelist;
	struct per_file_obj_mem_info stats;
	struct pid *tgid;
	int pid_num;
};

struct drm_i915_obj_virt_addr {
	struct list_head head;
	unsigned long user_virt_addr;
};

struct drm_i915_obj_pid_info {
	struct list_head head;
	pid_t tgid;
	int open_handle_count;
	struct list_head virt_addr_head;
};

struct get_obj_stats_buf {
	struct pid_stat_entry *entry;
	struct drm_i915_error_state_buf *m;
};

#define err_printf(e, ...) i915_error_printf(e, __VA_ARGS__)
#define err_puts(e, s) i915_error_puts(e, s)

static const char *get_pin_flag(struct drm_i915_gem_object *obj)
{
	if (obj->user_pin_count > 0)
		return "P";
	else if (i915_gem_obj_is_pinned(obj))
		return "p";
	else
		return " ";
}

static const char *get_tiling_flag(struct drm_i915_gem_object *obj)
{
	switch (obj->tiling_mode) {
	default:
	case I915_TILING_NONE: return " ";
	case I915_TILING_X: return "X";
	case I915_TILING_Y: return "Y";
	}
}

/*
 * If this mmput call is the last one, it will tear down the mmaps of the
 * process and calls drm_gem_vm_close(), which leads deadlock on i915 mutex.
 * Instead, asynchronously schedule mmput function here, to avoid recursive
 * calls to acquire i915_mutex.
 */
static void async_mmput_func(void *data, async_cookie_t cookie)
{
	struct mm_struct *mm = data;
	mmput(mm);
}

static void async_mmput(struct mm_struct *mm)
{
	async_schedule(async_mmput_func, mm);
}

int i915_get_pid_cmdline(struct task_struct *task, char *buffer)
{
	int res = 0;
	unsigned int len;
	struct mm_struct *mm = get_task_mm(task);

	if (!mm)
		goto out;
	if (!mm->arg_end)
		goto out_mm;

	len = mm->arg_end - mm->arg_start;

	if (len > PAGE_SIZE)
		len = PAGE_SIZE;

	res = access_process_vm(task, mm->arg_start, buffer, len, 0);
	if (res < 0) {
		async_mmput(mm);
		return res;
	}

	if (res > 0 && buffer[res-1] != '\0' && len < PAGE_SIZE)
		buffer[res-1] = '\0';
out_mm:
	async_mmput(mm);
out:
	return 0;
}

static int i915_obj_get_shmem_pages_alloced(struct drm_i915_gem_object *obj)
{
	int ret;

	if (obj->base.filp) {
		struct inode *inode = file_inode(obj->base.filp);
		struct shmem_inode_info *info = SHMEM_I(inode);

		if (!inode)
			return 0;
		spin_lock(&info->lock);
		ret = inode->i_mapping->nrpages;
		spin_unlock(&info->lock);
		return ret;
	}
	return 0;
}

int i915_gem_obj_insert_pid(struct drm_i915_gem_object *obj)
{
	int ret = 0, found = 0;
	struct drm_i915_obj_pid_info *entry;
	pid_t current_tgid = task_tgid_nr(current);

	if (!i915.memtrack_debug)
		return 0;

	list_for_each_entry(entry, &obj->pid_info, head) {
		if (entry->tgid == current_tgid) {
			entry->open_handle_count++;
			found = 1;
			break;
		}
	}
	if (found == 0) {
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (entry == NULL) {
			DRM_ERROR("alloc failed\n");
			return -ENOMEM;
		}
		entry->tgid = current_tgid;
		entry->open_handle_count = 1;
		INIT_LIST_HEAD(&entry->virt_addr_head);
		list_add_tail(&entry->head, &obj->pid_info);
	}

	return ret;
}

void i915_gem_obj_remove_pid(struct drm_i915_gem_object *obj)
{
	pid_t current_tgid = task_tgid_nr(current);
	struct drm_i915_obj_pid_info *pid_entry, *pid_next;
	struct drm_i915_obj_virt_addr *virt_entry, *virt_next;
	int found = 0;

	if (!i915.memtrack_debug)
		return;

	list_for_each_entry_safe(pid_entry, pid_next, &obj->pid_info, head) {
		if (pid_entry->tgid == current_tgid) {
			pid_entry->open_handle_count--;
			found = 1;
			if (pid_entry->open_handle_count == 0) {
				list_for_each_entry_safe(virt_entry,
						virt_next,
						&pid_entry->virt_addr_head,
						head) {
					list_del(&virt_entry->head);
					kfree(virt_entry);
				}
				list_del(&pid_entry->head);
				kfree(pid_entry);
			}
			break;
		}
	}

	if (found == 0)
		DRM_DEBUG("Couldn't find matching tgid %d for obj %p\n",
				current_tgid, obj);
}

void i915_gem_obj_remove_all_pids(struct drm_i915_gem_object *obj)
{
	struct drm_i915_obj_pid_info *pid_entry, *pid_next;
	struct drm_i915_obj_virt_addr *virt_entry, *virt_next;

	list_for_each_entry_safe(pid_entry, pid_next, &obj->pid_info, head) {
		list_for_each_entry_safe(virt_entry,
					 virt_next,
					 &pid_entry->virt_addr_head,
					 head) {
			list_del(&virt_entry->head);
			kfree(virt_entry);
		}
		list_del(&pid_entry->head);
		kfree(pid_entry);
	}
}

int
i915_obj_insert_virt_addr(struct drm_i915_gem_object *obj,
				unsigned long addr,
				bool is_map_gtt,
				bool is_mutex_locked)
{
	struct drm_i915_obj_pid_info *pid_entry;
	pid_t current_tgid = task_tgid_nr(current);
	int ret = 0, found = 0;

	if (!i915.memtrack_debug)
		return 0;

	if (is_map_gtt)
		addr |= 1;

	if (!is_mutex_locked) {
		ret = i915_mutex_lock_interruptible(obj->base.dev);
		if (ret)
			return ret;
	}

	list_for_each_entry(pid_entry, &obj->pid_info, head) {
		if (pid_entry->tgid == current_tgid) {
			struct drm_i915_obj_virt_addr *virt_entry, *new_entry;

			list_for_each_entry(virt_entry,
					    &pid_entry->virt_addr_head,
					    head) {
				if (virt_entry->user_virt_addr == addr) {
					found = 1;
					break;
				}
			}
			if (found)
				break;
			new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
			if (new_entry == NULL) {
				DRM_ERROR("alloc failed\n");
				ret = -ENOMEM;
				goto out;
			}
			new_entry->user_virt_addr = addr;
			list_add_tail(&new_entry->head,
				&pid_entry->virt_addr_head);
			break;
		}
	}

out:
	if (!is_mutex_locked)
		mutex_unlock(&obj->base.dev->struct_mutex);

	return ret;
}

static int i915_obj_virt_addr_is_invalid(struct drm_gem_object *obj,
				struct pid *tgid, unsigned long addr)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	int locked, ret = 0;

	task = get_pid_task(tgid, PIDTYPE_PID);
	if (task == NULL) {
		DRM_DEBUG("null task for tgid=%d\n", pid_nr(tgid));
		return -EINVAL;
	}

	mm = get_task_mm(task);
	if (mm == NULL) {
		DRM_DEBUG("null mm for tgid=%d\n", pid_nr(tgid));
		ret = -EINVAL;
		goto out_task;
	}

	locked = down_read_trylock(&mm->mmap_sem);
	if (!locked)
		goto out_mm;

	vma = find_vma(mm, addr);
	if (vma) {
		if (addr & 1) { /* mmap_gtt case */
			if (vma->vm_pgoff*PAGE_SIZE == (unsigned long)
				drm_vma_node_offset_addr(&obj->vma_node))
				ret = 0;
			else
				ret = -EINVAL;
		} else { /* mmap case */
			if (vma->vm_file == obj->filp)
				ret = 0;
			else
				ret = -EINVAL;
		}
	} else
		ret = -EINVAL;

	up_read(&mm->mmap_sem);

out_mm:
	async_mmput(mm);
out_task:
	put_task_struct(task);
	return ret;
}

static void i915_obj_pidarray_validate(struct drm_gem_object *gem_obj)
{
	struct drm_i915_gem_object *obj = to_intel_bo(gem_obj);
	struct drm_device *dev = gem_obj->dev;
	struct drm_i915_obj_virt_addr *virt_entry, *virt_next;
	struct drm_i915_obj_pid_info *pid_entry, *pid_next;
	struct drm_file *file;
	struct drm_i915_file_private *file_priv;
	struct pid *tgid;
	int pid_num, present;

	/*
	 * Run a sanity check on pid_array. All entries in pid_array should
	 * be subset of the the drm filelist pid entries.
	 */
	list_for_each_entry_safe(pid_entry, pid_next, &obj->pid_info, head) {
		if (pid_next == NULL) {
			DRM_ERROR(
				  "Invalid pid info. obj:%p, size:%zdK, pin flag:%s, tiling:%s, userptr=%s, stolen:%s, name:%d, handle_count=%d\n",
				  &obj->base, obj->base.size/1024,
				  get_pin_flag(obj), get_tiling_flag(obj),
				  (obj->userptr.mm != 0) ? "Y" : "N",
				  obj->stolen ? "Y" : "N", obj->base.name,
				  obj->base.handle_count);
			break;
		}

		present = 0;
		list_for_each_entry(file, &dev->filelist, lhead) {
			file_priv = file->driver_priv;
			tgid = file_priv->tgid;
			pid_num = pid_nr(tgid);

			if (pid_num == pid_entry->tgid) {
				present = 1;
				break;
			}
		}
		if (present == 0) {
			DRM_DEBUG("stale_tgid=%d\n", pid_entry->tgid);
			list_for_each_entry_safe(virt_entry, virt_next,
					&pid_entry->virt_addr_head,
					head) {
				list_del(&virt_entry->head);
				kfree(virt_entry);
			}
			list_del(&pid_entry->head);
			kfree(pid_entry);
		} else {
			/* Validate the virtual address list */
			struct task_struct *task =
				get_pid_task(tgid, PIDTYPE_PID);
			if (task == NULL)
				continue;

			list_for_each_entry_safe(virt_entry, virt_next,
					&pid_entry->virt_addr_head,
					head) {
				if (i915_obj_virt_addr_is_invalid(gem_obj, tgid,
					virt_entry->user_virt_addr)) {
					DRM_DEBUG("stale_addr=%ld\n",
					virt_entry->user_virt_addr);
					list_del(&virt_entry->head);
					kfree(virt_entry);
				}
			}
			put_task_struct(task);
		}
	}
}

static int
i915_describe_obj(struct get_obj_stats_buf *obj_stat_buf,
		struct drm_i915_gem_object *obj)
{
	struct i915_vma *vma;
	struct drm_i915_obj_pid_info *pid_info_entry;
	struct drm_i915_obj_virt_addr *virt_entry;
	struct drm_i915_error_state_buf *m = obj_stat_buf->m;
	struct pid_stat_entry *pid_entry = obj_stat_buf->entry;
	struct per_file_obj_mem_info *stats = &pid_entry->stats;
	struct drm_hash_item *hash_item;
	int obj_shared_count = 0;
	bool duplicate_obj = false;

	if (obj->base.name) {
		if (drm_ht_find_item(&pid_entry->namelist,
				(unsigned long)obj->base.name, &hash_item)) {
			struct name_entry *entry =
				kzalloc(sizeof(*entry), GFP_NOWAIT);
			if (entry == NULL) {
				DRM_ERROR("alloc failed\n");
				return -ENOMEM;
			}
			entry->hash_item.key = obj->base.name;
			drm_ht_insert_item(&pid_entry->namelist,
					   &entry->hash_item);
			list_add_tail(&entry->head, &pid_entry->namefree);
			list_for_each_entry(pid_info_entry, &obj->pid_info,
					head)
				obj_shared_count++;

			if (WARN_ON(obj_shared_count == 0))
				return -EINVAL;
		} else
			duplicate_obj = true;
	} else
		obj_shared_count = 1;

	if (!duplicate_obj && !obj->stolen &&
			(obj->madv != __I915_MADV_PURGED) &&
			(i915_obj_get_shmem_pages_alloced(obj) != 0)) {
		if (obj_shared_count > 1)
			stats->phys_space_shared_proportion +=
				obj->base.size/obj_shared_count;
		else
			stats->phys_space_allocated_priv +=
				obj->base.size;
	}

	err_printf(m,
		"%p: %7zdK  %s    %s     %s      %s     %s      %s       %s     ",
		   &obj->base,
		   obj->base.size / 1024,
		   get_pin_flag(obj),
		   get_tiling_flag(obj),
		   obj->dirty ? "Y" : "N",
		   obj->base.name ? "Y" : "N",
		   (obj->userptr.mm != 0) ? "Y" : "N",
		   obj->stolen ? "Y" : "N",
		   (obj->pin_mappable || obj->fault_mappable) ? "Y" : "N");

	if (obj->madv == __I915_MADV_PURGED)
		err_puts(m, " purged    ");
	else if (obj->madv == I915_MADV_DONTNEED)
		err_puts(m, " purgeable   ");
	else if (i915_obj_get_shmem_pages_alloced(obj) != 0)
		err_puts(m, " allocated   ");
	else
		err_puts(m, "             ");

	list_for_each_entry(vma, &obj->vma_list, vma_link) {
		if (!i915_is_ggtt(vma->vm))
			err_puts(m, " PP    ");
		else
			err_puts(m, " G     ");
		err_printf(m, "  %08lx ", vma->node.start);
	}
	if (list_empty(&obj->vma_list))
		err_puts(m, "                  ");

	list_for_each_entry(pid_info_entry, &obj->pid_info, head) {
		err_printf(m, " (%d: %d:",
			   pid_info_entry->tgid,
			   pid_info_entry->open_handle_count);
		list_for_each_entry(virt_entry,
				    &pid_info_entry->virt_addr_head, head) {
			if (virt_entry->user_virt_addr & 1)
				err_printf(m, " %p",
				(void *)(virt_entry->user_virt_addr & ~1));
			else
				err_printf(m, " %p*",
				(void *)virt_entry->user_virt_addr);
		}
		err_puts(m, ") ");
	}

	err_puts(m, "\n");

	if (m->bytes == 0 && m->err)
		return m->err;

	return 0;
}

static int
i915_drm_gem_obj_info(int id, void *ptr, void *data)
{
	struct drm_i915_gem_object *obj = ptr;
	struct get_obj_stats_buf *obj_stat_buf = data;
	int ret;

	if (obj->pid_info.next == NULL) {
		DRM_ERROR(
			"Invalid pid info. obj:%p, size:%zdK, pin flag:%s, tiling:%s, userptr=%s, stolen:%s, name:%d, handle_count=%d\n",
			&obj->base, obj->base.size/1024,
			get_pin_flag(obj), get_tiling_flag(obj),
			(obj->userptr.mm != 0) ? "Y" : "N",
			obj->stolen ? "Y" : "N", obj->base.name,
			obj->base.handle_count);
		return 0;
	}
	i915_obj_pidarray_validate(&obj->base);
	ret = i915_describe_obj(obj_stat_buf, obj);

	return ret;
}

static int
i915_drm_gem_object_per_file_summary(int id, void *ptr, void *data)
{
	struct pid_stat_entry *pid_entry = data;
	struct drm_i915_gem_object *obj = ptr;
	struct per_file_obj_mem_info *stats = &pid_entry->stats;
	struct drm_i915_obj_pid_info *pid_info_entry;
	struct drm_hash_item *hash_item;
	int obj_shared_count = 0;

	if (obj->pid_info.next == NULL) {
		DRM_ERROR(
			"Invalid pid info. obj:%p, size:%zdK, pin flag:%s, tiling:%s, userptr=%s, stolen:%s, name:%d, handle_count=%d\n",
			&obj->base, obj->base.size/1024,
			get_pin_flag(obj), get_tiling_flag(obj),
			(obj->userptr.mm != 0) ? "Y" : "N",
			obj->stolen ? "Y" : "N", obj->base.name,
			obj->base.handle_count);
		return 0;
	}

	i915_obj_pidarray_validate(&obj->base);

	stats->num_obj++;

	if (obj->base.name) {

		if (drm_ht_find_item(&pid_entry->namelist,
				(unsigned long)obj->base.name, &hash_item)) {
			struct name_entry *entry =
				kzalloc(sizeof(*entry), GFP_NOWAIT);
			if (entry == NULL) {
				DRM_ERROR("alloc failed\n");
				return -ENOMEM;
			}
			entry->hash_item.key = obj->base.name;
			drm_ht_insert_item(&pid_entry->namelist,
					&entry->hash_item);
			list_add_tail(&entry->head, &pid_entry->namefree);

			list_for_each_entry(pid_info_entry, &obj->pid_info,
					head)
				obj_shared_count++;
			if (WARN_ON(obj_shared_count == 0))
				return -EINVAL;
		} else {
			DRM_DEBUG("Duplicate obj with name %d for process %s\n",
				obj->base.name, stats->process_name);
			return 0;
		}

		DRM_DEBUG("Obj: %p, shared count =%d\n",
			&obj->base, obj_shared_count);

		if (obj_shared_count > 1)
			stats->num_obj_shared++;
		else
			stats->num_obj_private++;
	} else {
		obj_shared_count = 1;
		stats->num_obj_private++;
	}

	if (i915_gem_obj_bound_any(obj)) {
		stats->num_obj_gtt_bound++;
		if (obj_shared_count > 1)
			stats->gtt_space_allocated_shared += obj->base.size;
		else
			stats->gtt_space_allocated_priv += obj->base.size;
	}

	if (obj->stolen) {
		stats->num_obj_stolen++;
		stats->stolen_space_allocated += obj->base.size;
	} else if (obj->madv == __I915_MADV_PURGED) {
		stats->num_obj_purged++;
	} else if (obj->madv == I915_MADV_DONTNEED) {
		stats->num_obj_purgeable++;
		stats->num_obj_allocated++;
		if (i915_obj_get_shmem_pages_alloced(obj) != 0) {
			stats->phys_space_purgeable += obj->base.size;
			if (obj_shared_count > 1) {
				stats->phys_space_allocated_shared +=
					obj->base.size;
				stats->phys_space_shared_proportion +=
					obj->base.size/obj_shared_count;
			} else
				stats->phys_space_allocated_priv +=
					obj->base.size;
		} else
			WARN_ON(1);
	} else if (i915_obj_get_shmem_pages_alloced(obj) != 0) {
		stats->num_obj_allocated++;
			if (obj_shared_count > 1) {
				stats->phys_space_allocated_shared +=
					obj->base.size;
				stats->phys_space_shared_proportion +=
					obj->base.size/obj_shared_count;
			}
		else
			stats->phys_space_allocated_priv += obj->base.size;
	}
	if (obj->fault_mappable) {
		stats->num_obj_fault_mappable++;
		stats->fault_mappable_size += obj->base.size;
	}
	return 0;
}

static int
__i915_get_drm_clients_info(struct drm_i915_error_state_buf *m,
			struct drm_device *dev)
{
	struct drm_file *file;
	struct drm_i915_private *dev_priv = dev->dev_private;

	struct name_entry *entry, *next;
	struct pid_stat_entry *pid_entry, *temp_entry;
	struct pid_stat_entry *new_pid_entry, *new_temp_entry;
	struct list_head per_pid_stats, sorted_pid_stats;
	int ret = 0;
	size_t total_shared_prop_space = 0, total_priv_space = 0;

	INIT_LIST_HEAD(&per_pid_stats);
	INIT_LIST_HEAD(&sorted_pid_stats);

	err_puts(m,
		"\n\n  pid   Total  Shared  Priv   Purgeable  Alloced  SharedPHYsize   SharedPHYprop    PrivPHYsize   PurgeablePHYsize   process\n");

	list_for_each_entry(file, &dev->filelist, lhead) {
		struct pid *tgid;
		struct drm_i915_file_private *file_priv = file->driver_priv;
		int pid_num, found = 0;

		tgid = file_priv->tgid;
		pid_num = pid_nr(tgid);

		list_for_each_entry(pid_entry, &per_pid_stats, head) {
			if (pid_entry->pid_num == pid_num) {
				found = 1;
				break;
			}
		}

		if (!found) {
			struct pid_stat_entry *new_entry =
				kzalloc(sizeof(*new_entry), GFP_KERNEL);
			if (new_entry == NULL) {
				DRM_ERROR("alloc failed\n");
				ret = -ENOMEM;
				break;
			}
			new_entry->tgid = tgid;
			new_entry->pid_num = pid_num;
			ret = drm_ht_create(&new_entry->namelist,
				      DRM_MAGIC_HASH_ORDER);
			if (ret) {
				kfree(new_entry);
				break;
			}

			list_add_tail(&new_entry->head, &per_pid_stats);
			INIT_LIST_HEAD(&new_entry->namefree);
			new_entry->stats.process_name = file_priv->process_name;
			pid_entry = new_entry;
		}

		spin_lock(&file->table_lock);
		ret = idr_for_each(&file->object_idr,
			&i915_drm_gem_object_per_file_summary, pid_entry);
		spin_unlock(&file->table_lock);
		if (ret)
			break;
	}

	list_for_each_entry_safe(pid_entry, temp_entry, &per_pid_stats, head) {
		if (list_empty(&sorted_pid_stats)) {
			list_del(&pid_entry->head);
			list_add_tail(&pid_entry->head, &sorted_pid_stats);
			continue;
		}

		list_for_each_entry_safe(new_pid_entry, new_temp_entry,
			&sorted_pid_stats, head) {
			int prev_space =
				pid_entry->stats.phys_space_shared_proportion +
				pid_entry->stats.phys_space_allocated_priv;
			int new_space =
				new_pid_entry->
				stats.phys_space_shared_proportion +
				new_pid_entry->stats.phys_space_allocated_priv;
			if (prev_space > new_space) {
				list_del(&pid_entry->head);
				list_add_tail(&pid_entry->head,
					&new_pid_entry->head);
				break;
			}
			if (list_is_last(&new_pid_entry->head,
				&sorted_pid_stats)) {
				list_del(&pid_entry->head);
				list_add_tail(&pid_entry->head,
						&sorted_pid_stats);
			}
		}
	}

	list_for_each_entry_safe(pid_entry, temp_entry,
				&sorted_pid_stats, head) {
		struct task_struct *task = get_pid_task(pid_entry->tgid,
							PIDTYPE_PID);
		err_printf(m,
			"%5d %6d %6d %6d %9d %8d %14zdK %14zdK %14zdK  %14zdK     %s",
			   pid_entry->pid_num,
			   pid_entry->stats.num_obj,
			   pid_entry->stats.num_obj_shared,
			   pid_entry->stats.num_obj_private,
			   pid_entry->stats.num_obj_purgeable,
			   pid_entry->stats.num_obj_allocated,
			   pid_entry->stats.phys_space_allocated_shared/1024,
			   pid_entry->stats.phys_space_shared_proportion/1024,
			   pid_entry->stats.phys_space_allocated_priv/1024,
			   pid_entry->stats.phys_space_purgeable/1024,
			   pid_entry->stats.process_name);

		if (task == NULL)
			err_puts(m, "*\n");
		else
			err_puts(m, "\n");

		total_shared_prop_space +=
			pid_entry->stats.phys_space_shared_proportion/1024;
		total_priv_space +=
			pid_entry->stats.phys_space_allocated_priv/1024;
		list_del(&pid_entry->head);

		list_for_each_entry_safe(entry, next,
					&pid_entry->namefree, head) {
			list_del(&entry->head);
			drm_ht_remove_item(&pid_entry->namelist,
					&entry->hash_item);
			kfree(entry);
		}
		drm_ht_remove(&pid_entry->namelist);
		kfree(pid_entry);
		if (task)
			put_task_struct(task);
	}

	err_puts(m,
		"\t\t\t\t\t\t\t\t--------------\t-------------\t--------\n");
	err_printf(m,
		"\t\t\t\t\t\t\t\t%13zdK\t%12zdK\tTotal\n",
			total_shared_prop_space, total_priv_space);

	err_printf(m, "\nTotal used GFX Shmem Physical space %8zdK\n",
		   dev_priv->mm.phys_mem_total/1024);

	if (ret)
		return ret;
	if (m->bytes == 0 && m->err)
		return m->err;

	return 0;
}

#define NUM_SPACES 100
#define INITIAL_SPACES_STR(x) #x
#define SPACES_STR(x) INITIAL_SPACES_STR(x)

static int
__i915_gem_get_obj_info(struct drm_i915_error_state_buf *m,
			struct drm_device *dev, struct pid *tgid)
{
	struct drm_file *file;
	struct drm_i915_file_private *file_priv_reqd = NULL;
	int bytes_copy, ret = 0;
	struct pid_stat_entry pid_entry;
	struct name_entry *entry, *next;

	pid_entry.stats.phys_space_shared_proportion = 0;
	pid_entry.stats.phys_space_allocated_priv = 0;
	pid_entry.tgid = tgid;
	pid_entry.pid_num = pid_nr(tgid);
	ret = drm_ht_create(&pid_entry.namelist, DRM_MAGIC_HASH_ORDER);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&pid_entry.namefree);

	/*
	 * Fill up initial few bytes with spaces, to insert summary data later
	 * on
	 */
	err_printf(m, "%"SPACES_STR(NUM_SPACES)"s\n", " ");

	list_for_each_entry(file, &dev->filelist, lhead) {
		struct drm_i915_file_private *file_priv = file->driver_priv;
		struct get_obj_stats_buf obj_stat_buf;

		obj_stat_buf.entry = &pid_entry;
		obj_stat_buf.m = m;

		if (file_priv->tgid != tgid)
			continue;

		file_priv_reqd = file_priv;
		err_puts(m,
			"\n Obj Identifier       Size Pin Tiling Dirty Shared Vmap Stolen Mappable  AllocState Global/PP  GttOffset (PID: handle count: user virt addrs)\n");
		spin_lock(&file->table_lock);
		ret = idr_for_each(&file->object_idr,
				&i915_drm_gem_obj_info, &obj_stat_buf);
		spin_unlock(&file->table_lock);
		if (ret)
			break;
	}

	if (file_priv_reqd) {
		int space_remaining;

		/* Reset the bytes counter to buffer beginning */
		bytes_copy = m->bytes;
		m->bytes = 0;

		err_printf(m, "\n  PID    GfxMem   Process\n");
		err_printf(m, "%5d %8zdK ", pid_nr(file_priv_reqd->tgid),
			   (pid_entry.stats.phys_space_shared_proportion +
			    pid_entry.stats.phys_space_allocated_priv)/1024);

		space_remaining = NUM_SPACES - m->bytes - 1;
		if (strlen(file_priv_reqd->process_name) > space_remaining)
			file_priv_reqd->process_name[space_remaining] = '\0';

		err_printf(m, "%s\n", file_priv_reqd->process_name);

		/* Reinstate the previous saved value of bytes counter */
		m->bytes = bytes_copy;
	} else
		WARN(1, "drm file corresponding to tgid:%d not found\n",
			pid_nr(tgid));

	list_for_each_entry_safe(entry, next,
				 &pid_entry.namefree, head) {
		list_del(&entry->head);
		drm_ht_remove_item(&pid_entry.namelist,
				   &entry->hash_item);
		kfree(entry);
	}
	drm_ht_remove(&pid_entry.namelist);

	if (ret)
		return ret;
	if (m->bytes == 0 && m->err)
		return m->err;
	return 0;
}

int i915_get_drm_clients_info(struct drm_i915_error_state_buf *m,
			struct drm_device *dev)
{
	int ret = 0;

	/*
	 * Protect the access to global drm resources such as filelist. Protect
	 * against their removal under our noses, while in use.
	 */
	mutex_lock(&drm_global_mutex);
	ret = i915_mutex_lock_interruptible(dev);
	if (ret) {
		mutex_unlock(&drm_global_mutex);
		return ret;
	}

	ret = __i915_get_drm_clients_info(m, dev);

	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&drm_global_mutex);

	return ret;
}

int i915_gem_get_obj_info(struct drm_i915_error_state_buf *m,
			struct drm_device *dev, struct pid *tgid)
{
	int ret = 0;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	ret = __i915_gem_get_obj_info(m, dev, tgid);

	mutex_unlock(&dev->struct_mutex);

	return ret;
}
