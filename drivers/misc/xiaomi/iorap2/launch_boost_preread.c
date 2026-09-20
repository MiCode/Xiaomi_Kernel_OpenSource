#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rbtree.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/rwlock.h>
#include <linux/atomic.h>
#include <linux/cred.h>
#include <linux/fadvise.h>
#include <linux/module.h>

#include "mi_lb_def.h"
#include "launch_boost_preread.h"
#include "launch_boost_com.h"

static bool get_app_pre_filesinfo(struct package_info *package,
					struct lb_app_preread_info *app_preread_info)
{
	struct package_info *active_package = package;
	struct list_head *p = NULL;
	struct file_info *file_list = NULL;
	unsigned long wr_size = sizeof(struct lb_app_preread_info);
	struct lb_app_preread_file_info *preread_file_info;
	unsigned long bitmap_bytes = 0;
	unsigned int name_len = 0;
	app_preread_info->files = 0;

	list_for_each_prev(p, &active_package->file_infos) {
		file_list = list_entry(p, struct file_info, list);
		if (!file_list->is_collected)
			continue;
		bitmap_bytes = file_list->bitmap_longs * sizeof(unsigned long);
		if (wr_size + bitmap_bytes + sizeof(struct lb_app_preread_file_info)
				> app_preread_info->file_info_buf_size) {
			MI_LB_ERR("Exceeded preread_info memory size %zu", app_preread_info->file_info_buf_size);
			break;
		}
		preread_file_info =
			(struct lb_app_preread_file_info *) ((u8 *)app_preread_info + wr_size);
		preread_file_info->i_mtime = file_list->i_mtime;
		preread_file_info->i_ino = file_list->i_ino;
		preread_file_info->i_size = file_list->i_size;
		name_len = strlen(file_list->path);

		if(name_len < 256) {
			strncpy(preread_file_info->file_path, file_list->path, name_len);
			preread_file_info->file_path[name_len] = '\0';
		}
		else{
			MI_LB_ERR("file path is too long %s", file_list->path);
			preread_file_info->file_path[0] = '\0';
		}

		preread_file_info->bitmap_longs = file_list->bitmap_longs;
		memcpy(preread_file_info->bitmaps, file_list->bitmaps, bitmap_bytes);
		app_preread_info->files++;
		wr_size += sizeof(struct lb_app_preread_file_info) + bitmap_bytes;
	}

	return 0;
}

static unsigned long get_app_filesinfo_size(struct package_info *package)
{
	struct package_info *active_package = package;
	struct list_head *p = NULL;
	struct file_info *file_list = NULL;
	unsigned long filesinfo_size = 0;

	if(list_empty(&active_package->file_infos))
		return 0;
	filesinfo_size = sizeof(struct lb_app_preread_info);
	list_for_each(p, &active_package->file_infos) {
		file_list = list_entry(p, struct file_info, list);
		if (file_list->is_collected)
			filesinfo_size += sizeof(struct lb_app_preread_file_info) +
						file_list->bitmap_longs * sizeof(unsigned long);
	}
	return filesinfo_size;
}

int mi_lb_dealwith_preread_size_cmd(struct lb_app_preread_info_size *app_preread_info_size)
{
	struct package_info *package = NULL;
	unsigned long filesinfo_size = 0;
	int ret;

	mutex_lock(&g_manager->compiler_lock);

	package = mi_lb_get_package_info(&app_preread_info_size->owner, false);
	if (!package) {
		MI_LB_DBG("app first cold start, No pre-reading");
		ret = -1;
		goto err;
	}

	filesinfo_size = get_app_filesinfo_size(package);
	if (filesinfo_size <= 0) {
		MI_LB_DBG("app has collected, but no pre-reading files");
		ret = -2;
		goto err;
	}
	app_preread_info_size->info_size = sizeof(struct lb_app_preread_info_size) + filesinfo_size;
	package_info_put(package);
	mutex_unlock(&g_manager->compiler_lock);
	return 0;
err:
	mutex_unlock(&g_manager->compiler_lock);
	return ret;

}

int mi_lb_dealwith_preread_info_cmd(struct lb_app_preread_info *app_preread_info)
{
	struct package_info *package = NULL;

	mutex_lock(&g_manager->compiler_lock);
	package = mi_lb_get_package_info(&app_preread_info->owner, false);
	if (!package) {
		mutex_unlock(&g_manager->compiler_lock);
		MI_LB_DBG("app first cold start, No pre-reading");
		return -1;
	}

	get_app_pre_filesinfo(package, app_preread_info);
	package_info_put(package);
	mutex_unlock(&g_manager->compiler_lock);

	return 0;
}


#ifdef MI_LB_PREREAD

static struct package_info *current_preread_package;

static bool open_preread_files(struct package_info *package)
{
	struct list_head *p = NULL;
	struct file_info *file_list = NULL;

	list_for_each(p, &package->file_infos) {
		file_list = list_entry(p, struct file_info, list);
		file_list->pre_filp = filp_open(file_list->path, O_RDONLY, 0);
		if(IS_ERR(file_list->pre_filp)) {
			MI_LB_ERR("open file %s failed", file_list->path);
			file_list->pre_filp = NULL;
		}
	}
	return true;
}

static bool close_preread_files(struct package_info *package)
{
	struct list_head *p = NULL;
	struct file_info *file_list = NULL;

	list_for_each(p, &package->file_infos) {
		file_list = list_entry(p, struct file_info, list);
		if(file_list->pre_filp){
			filp_close(file_list->pre_filp, NULL);
			file_list->pre_filp = NULL;
		}
	}
	return true;
}


static void mi_lb_preread_do_work(struct package_info *package)
{
	struct list_head *p = NULL;
	struct file_info *file_list = NULL;
	unsigned long start = 0, end = 0, last = 0;
	struct timespec64 mtime;
	int ret = 0;

	list_for_each(p, &package->file_infos) {
		file_list = list_entry(p, struct file_info, list);
		MI_LB_DBG("path: %s, mtime: %lld, bitmap bytes %lu",
					file_list->path, file_list->i_mtime.tv_sec,
					file_list->bitmap_longs * sizeof(unsigned long));

		if(file_list->pre_filp) {
			mtime = inode_get_mtime(file_inode(file_list->pre_filp));
			if(file_list->i_mtime.tv_sec == mtime.tv_sec) {
				last = file_list->bitmap_longs * sizeof(unsigned long) * 8;
				start = 0;
				end = 0;
				for (;;) {
					start = find_next_bit(file_list->bitmaps, last, end);
					if (start >= last)
						break;
					end = find_next_zero_bit(file_list->bitmaps, last, start);
					MI_LB_DBG("bitmapfind start %ld  end %ld", start, end);
					ret= vfs_fadvise(file_list->pre_filp, start << PAGE_SHIFT,
						(end - start) << PAGE_SHIFT, POSIX_FADV_WILLNEED);
					if (ret){
						MI_LB_ERR("vfs_fadvise failed, ret %d", ret);
						goto out;
					}
				}
			}
		}
	}
out:
	close_preread_files(package);
}

static int mi_lb_preread_thread(void *data)
{
	struct lb_preread *preread = data;

	MI_LB_DBG("enter");
	if (!data) {
		MI_LB_ERR("data is null");
		return -EINVAL;
	}

	init_waitqueue_head(&preread->preread_wait);

	MI_LB_DBG("collecting_: %d",  atomic_read(&collecting_));

	while (!kthread_should_stop()) {
		wait_event(preread->preread_wait, atomic_read(&collecting_) > 0);
		mi_lb_preread_do_work(current_preread_package);
	}

	MI_LB_DBG("exit");
	return 0;
}

int mi_lb_dealwith_preread_cmd(struct lb_owner *owner)
{
	struct lb_preread *preread;
	MI_LB_DBG("preread cmd : package uid: %d, pid: %d, packagename: %s",
				owner->uid, owner->pid, owner->package_name);

	if (!atomic_add_unless(&collecting_, 1, 1)) {
		MI_LB_DBG("preread: app[%s] is collecting or preread", owner->package_name);
		return -1;
	}
	current_preread_package = mi_lb_get_package_info(owner, false);
	if (!current_preread_package) {
		MI_LB_DBG("app first cold start, No pre-reading");
		goto err;
	}
	open_preread_files(current_preread_package);
	for (int i = 0; i < PREREAD_THREAD_CNT; i++) {
		preread = &g_manager->preread[i];
		wake_up_all(&preread->preread_wait);
	}
	atomic_set(&collecting_, 0);
	return 0;
err:
	atomic_set(&collecting_, 0);
	return -1;
}

int mi_lb_init_preread(void)
{
	struct lb_preread *preread;
	int ret = 0;

	for (int i = 0; i < PREREAD_THREAD_CNT; i++) {
		preread = &g_manager->preread[i];
		preread->preread_thread = kthread_run(
			mi_lb_preread_thread, preread, "lb::preread_thread");
		if (IS_ERR(preread->preread_thread)) {
			ret = PTR_ERR(preread->preread_thread);
			MI_LB_ERR("create preread_thread fail, ret: %d", ret);
			return ret;
		}
	}
	return 0;
}
#endif


























