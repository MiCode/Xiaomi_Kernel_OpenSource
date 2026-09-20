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
#include <linux/file.h>
#include <linux/jiffies.h>

#include "mi_lb_def.h"
#include "launch_boost_collect.h"
#include "launch_boost_com.h"
#include "launch_boost_debug.h"
#include "launch_boost_clear.h"

#include "launch_boost_preread.h"

#define MAX_ICOUNT 3

struct lb_manager *g_manager;
atomic_t collecting_;
static struct active_package_info g_active_package;

static const char * file_type_list[] =
{
	"so",
	"apk",
	"dex",
	"vdex",
	"odex",
	"art",
	"jar",
	NULL
};

static inline void add_pid_to_pids(int pid)
{
	if (g_active_package.current_package && g_active_package.pids_num < MAX_COLLECT_PIDS) {
		g_active_package.pids[g_active_package.pids_num] = pid;
		g_active_package.pids_num++;
	}
}

static inline bool is_app_collec_pids(int pid)
{
	int i;

	for (i = 0; i < g_active_package.pids_num; i++) {
		if (g_active_package.pids[i] == pid) {
			return true;
		}
	}

	return false;
}

static void mi_lb_pagecache_collect(struct file *file,
				    pgoff_t start, unsigned count, bool is_fault)
{
	struct collect_info *info;
	int i;
	struct file *tmp_file;

	if (g_atrace_enable) {
		uid_t uid = current_uid().val;
		char *f_path;
		if (debug_owner.uid !=0 && uid == debug_owner.uid) {
			f_path =file_path(file, g_manager->file_path, PATH_MAX);
			MI_LB_DBG("collect file %s", f_path);
		}
	}

	if (g_active_package.current_package == NULL || !is_app_collec_pids(current->pid))
		return;

	if (time_after(jiffies, g_manager->start_jiffies + HZ * 2))
		return;

	if (file_inode(file)->i_sb == NULL
		|| (file_inode(file)->i_sb->s_magic != F2FS_SUPER_MAGIC
		&& file_inode(file)->i_sb->s_magic != EROFS_SUPER_MAGIC_V1
		&&  file_inode(file)->i_sb->s_magic != EXT4_SUPER_MAGIC))
		return;

	spin_lock(&g_manager->lock);
	for (i = 0; i < g_manager->collected_num; i++) {
		if (file_inode(file)->i_sb->s_dev == g_manager->tmp_file_infos[i].s_dev
			&& file_inode(file)->i_ino == g_manager->tmp_file_infos[i].i_ino) {
			tmp_file = g_manager->tmp_file_infos[i].file;
			break;
		}
	}

	if (i == g_manager->collected_num && i < MAX_COLLECT_FILES) {
		/* get for once, paired with fput */
		get_file(file);
		tmp_file = file;
		g_manager->tmp_file_infos[i].file = file;
		g_manager->tmp_file_infos[i].s_dev = file_inode(file)->i_sb->s_dev;
		g_manager->tmp_file_infos[i].i_ino = file_inode(file)->i_ino;
		g_manager->collected_num += 1;
	}

	if (i == MAX_COLLECT_FILES) {
		spin_unlock(&g_manager->lock);
		return;
	}

	if (g_manager->cur < RECORD_SIZE) {
		info = &((struct collect_info *)g_manager->record_buf)[g_manager->cur];
		info->file = tmp_file;
		info->start = start;
		info->len = count;
		g_manager->cur++;
	}

	spin_unlock(&g_manager->lock);

}

void mi_lb_page_collect_on_readfile(
                void *unused, struct file *file, loff_t pos, size_t size)
{
	if (size != 0)
		mi_lb_pagecache_collect(file, pos >> PAGE_SHIFT,
						PAGE_ALIGN(size) >> PAGE_SHIFT, 0);
}

void mi_lb_page_collect_on_pagefault(void *unused, struct file *file,
                                        pgoff_t first_pgoff,
                                        pgoff_t last_pgoff,
                                        vm_fault_t ret)
{
	if(first_pgoff != last_pgoff)
		mi_lb_pagecache_collect(file, first_pgoff,
                                last_pgoff - first_pgoff, 1);
}

static bool is_file_info_collected(struct file_info *file_entry)
{
	int i;

	for (i = 0; i < g_manager->collected_num; i++) {
		if(g_manager->tmp_file_infos[i].s_dev == file_entry->s_dev
			&& g_manager->tmp_file_infos[i].i_ino == file_entry->i_ino)
			return true;
	}
	return false;
}

static void clear_fileinfos_collected(struct package_info *package)
{
	struct file_info *file_entry, *tmp;

	list_for_each_entry_safe(file_entry, tmp, &package->file_infos, list)
		file_entry->is_collected = false;
}

static void mi_lb_del_lists_file_info(struct package_info *package)
{
	struct file_info *file_entry, *tmp;
	unsigned int io_size = 0;

	list_for_each_entry_safe(file_entry, tmp, &package->file_infos, list) {
		if (g_debug_verbose) {
			io_size = bitmap_weight(file_entry->bitmaps, file_entry->i_size);
			package->total_startup_bytes += io_size << PAGE_SHIFT;
		}
		file_entry->is_moved = false;
		if(!is_file_info_collected(file_entry))	{
			file_entry->i_count--;
			if(file_entry->i_count == 0) {
				free_one_fileinfo(package, file_entry);
			}
		}else {
			file_entry->i_count = MAX_ICOUNT;
		}
	}
}

static void mi_lb_record_buf_clear(bool atrace_print_enable)
{
	int i;

	for (i = 0; i < g_manager->collected_num; i++) {
		if (g_atrace_enable && atrace_print_enable) {
			if (g_manager->tmp_file_infos[i].file) {
				char *f_path =file_path(g_manager->tmp_file_infos[i].file, g_manager->file_path, PATH_MAX);
				MI_LB_DBG("collected file: %s", f_path);
			}
		}
		if (g_manager->tmp_file_infos[i].file) {
			fput(g_manager->tmp_file_infos[i].file);
			g_manager->tmp_file_infos[i].file = NULL;
		}
	}
	g_manager->collected_num = 0;
	g_manager->cur = 0;

}

static bool mb_lb_dealwith_collect_finish(struct lb_owner *owner)
{
	struct package_info *package = g_active_package.current_package;

	if (!package) {
		MI_LB_ERR("active package is NULL");
		return false;
	}

	if ((package->owner.uid == owner->uid) &&
		!strcmp(package->owner.package_name, owner->package_name)) {
		cancel_delayed_work_sync(&g_manager->compiler_work);
		wake_up_all(&g_manager->compiler_wait);
	} else {
		MI_LB_ERR("finish collecting app[%s] != active app[%s]",
					package->owner.package_name, owner->package_name);
	}

	return 0;
}

static bool mb_lb_dealwith_collect_abort(struct lb_owner *owner)
{
	struct package_info *package = g_active_package.current_package;

	if (!package) {
		MI_LB_ERR("active package is NULL");
		return false;
	}

	memset(&g_active_package, 0, sizeof(g_active_package));
	cancel_delayed_work_sync(&g_manager->compiler_work);
	if ((package->owner.uid == owner->uid) &&
		!strncmp(package->owner.package_name, owner->package_name,
					strlen(owner->package_name))) {
		mi_lb_record_buf_clear(false);
		if(!mi_lb_is_in_package_list(package)) {
			MI_LB_DBG("active package %s not in package list",
					package->owner.package_name);
			g_manager->total_memory -= package->mm_size;
			lb_kfree(package, sizeof(struct package_info));
		}else {
			package_info_put(package);
		}

		atomic_set(&collecting_, 0);
	}
	return 0;
}

static void mi_lb_do_compile(struct work_struct *work)
{
	wake_up_all(&g_manager->compiler_wait);
}

static bool is_pin(const char *f_path)
{
	if (!g_collect_pinfile)
		return !strncmp(f_path, "/system", 7)
					//|| !strncmp(f_path, "/apex", 5)
					|| !strncmp(f_path, "/system_ext", 11);
	return false;
}

static bool preread_fnmatch(const char *sub, const char *s)
{
	size_t slen = strlen(s);
	size_t sublen = strlen(sub);
	int i;

	/*
	* filename format of multimedia file should be defined as:
	* "filename + '.' + extension + (optional: '.' + temp extension)".
	*/
	if (slen < sublen + 2)
		return false;

	for (i = 1; i < slen - sublen; i++) {
		if (s[i] != '.')
			continue;
		if (!strncasecmp(s + i + 1, sub, sublen)) {
			return true;
		}
	}
	return false;
}

static bool is_preread_file_type(const char *f_path, const char *file_types[])
{
	const char *file_type;

	while ((file_type = *file_types++)) {
		if (preread_fnmatch(file_type, f_path))
			return true;
	}
	return false;
}


static bool is_bypass_preread_file(struct file *file, const char *f_path)
{
	if (is_pin(f_path))
		return true;
	if (!g_collect_specify_filetype)
		return false;
	if(file->f_path.dentry && file->f_path.dentry->d_name.name)
		return !is_preread_file_type(file->f_path.dentry->d_name.name, file_type_list);
	return true;
}

static bool mb_lb_dealwith_collect_start(struct lb_owner *owner)
{
	struct package_info *package = NULL;
	struct package_info  *oldest_package = NULL;

	mi_lb_record_buf_clear(false);
	package = mi_lb_get_package_info(owner, true);
	if (!package) {
		if (g_manager->app_num >= MAX_APPS_LIST) {
			oldest_package = mi_lb_get_oldest_package_info();
			if (!oldest_package) {
				MI_LB_ERR("no old package to free");
			}else {
				package_info_put(oldest_package);
			}
		}
		package = lb_zmalloc(sizeof(struct package_info), GFP_KERNEL);
		if (!package) {
			MI_LB_ERR("alloc package info fail");
			return -1;
		}
		INIT_LIST_HEAD(&package->file_infos);
		mutex_init(&package->package_lock);
		package->files = 0;
		package->mm_size = sizeof(struct package_info);
		package->is_cleared = false;
		package_info_get(package);
	}
	package->owner = *owner;
	package->app_startup_bytes = 0;
	package->total_startup_bytes = 0;
	g_active_package.current_package = package;
	g_manager->start_jiffies = jiffies;

	//During a cold start, if no finish or abort signal is received within 2 seconds, the 
	//system will automatically terminate the data collection process and initiate compilation.
	schedule_delayed_work(&g_manager->compiler_work, msecs_to_jiffies(2000));
	return 0;
}

int mi_lb_dealwith_collect_cmd(enum APP_LAUNCH_CMD cmd, struct lb_owner *owner)
{
	int ret = 0;

	switch (cmd) {
	case APP_COLLECT_START:
		if (atomic_add_unless(&collecting_, 1, 1)) {
			ret = mb_lb_dealwith_collect_start(owner);
			if (!ret)
				add_pid_to_pids(owner->pid);
		}else if(g_active_package.current_package) {
			struct package_info *package = g_active_package.current_package;
			if(package && !strcmp(package->owner.package_name, owner->package_name)) {
				add_pid_to_pids(owner->pid);
			}
		}
		break;
	case APP_LAUNCH_FINISH:
		if (atomic_read(&collecting_)) {
			ret = mb_lb_dealwith_collect_finish(owner);
		}else {
			MI_LB_DBG("finish: the app[%s] is not collecting", owner->package_name);
		}
		break;
	case APP_LAUNCH_ABORT:
		if (atomic_read(&collecting_)) {
			ret = mb_lb_dealwith_collect_abort(owner);
		}else {
			MI_LB_DBG("abort: the app[%s] is not collecting", owner->package_name);
		}
		break;
	default:
		MI_LB_ERR("unknown cold start cmd is %u", cmd);
        break;
	}
	return 0;
}

static bool lookup_app_fileinfo_list(struct package_info *active_package,
					struct file *file, struct file_info **file_info, char *f_path)
{
	struct list_head *p = NULL;
	struct file_info *file_list = NULL;
	struct list_head *head = &active_package->file_infos;

	if(list_empty(&active_package->file_infos))
		return 0;
	list_for_each(p, &active_package->file_infos) {
		file_list = list_entry(p, struct file_info, list);
		if (file_list->s_dev ==  file_inode(file)->i_sb->s_dev
			 && !strcmp(file_list->path, f_path)) {
				goto found;
		}
	}
	return 0;
found:
	if ((!file_list->is_moved) && (p != head)) {
		list_move(p, head);
		file_list->is_moved = true;
	}
	file_list->is_collected = true;
	*file_info = file_list;

	return 1;
}

static bool launch_reset_fileinfo(struct file *file, struct file_info *info)
{
	int nbits;
	loff_t max_size = (loff_t)PREREAD_FILE_SIZE;

	if(info->bitmaps)
		kfree(info->bitmaps);

	info->collect_num = 0;
	info->s_dev = file_inode(file)->i_sb->s_dev;
	info->i_ino = file_inode(file)->i_ino;
	info->i_mtime = file_inode(file)->i_mtime;
	info->i_count = MAX_ICOUNT;
	info->i_size = min(i_size_read(file_inode(file)), max_size);
	info->i_size = PAGE_ALIGN(info->i_size) >> PAGE_SHIFT;
	nbits = info->i_size;
	info->bitmap_longs = BITS_TO_LONGS(nbits);
	info->bitmaps = bitmap_zalloc(nbits, GFP_KERNEL);
	if (!info->bitmaps) {
		MI_LB_ERR("alloc file_info bitmaps err");
		if(info->path)
			kfree(info->path);
		list_del(&info->list);
		lb_kfree(info, sizeof(struct file_info));
		return false;
	}

	return true;
}

static int mi_lb_get_fileinfo(struct package_info *active_package,
					struct file *file, struct file_info **file_info)
{
	bool find;
	struct file_info *info;
	char *f_path =NULL;
	int path_len;
	int nbits;
	loff_t max_size = (loff_t)PREREAD_FILE_SIZE;
	unsigned int mm_size;

	f_path = file_path(file, g_manager->file_path, PATH_MAX);
	if (IS_ERR(f_path)) {
		MI_LB_ERR("find filepath error\n");
		goto out;
	}

	find = lookup_app_fileinfo_list(active_package, file, file_info, f_path);
	if (find) {
		info = *file_info;
		if(info->i_ino != file_inode(file)->i_ino
				|| file_inode(file)->i_mtime.tv_sec != info->i_mtime.tv_sec) {
			if (!launch_reset_fileinfo(file, info))
				return -1;
			return 0;
		}else {
			return 0;
		}
	}

	if (is_bypass_preread_file(file, f_path)) {
		goto out;
	}

	info = lb_zmalloc(sizeof(struct file_info), GFP_KERNEL);
	if (!info) {
		MI_LB_ERR("alloc file_info err");
		goto out;;
	}
	info->collect_num = 0;
	info->s_dev = file_inode(file)->i_sb->s_dev;
	info->i_ino = file_inode(file)->i_ino;
	info->i_mtime = inode_get_mtime(file_inode(file));
	info->is_moved = true;
	info->is_collected = true;
	info->i_count = MAX_ICOUNT;
	info->i_size = min(i_size_read(file_inode(file)), max_size);
	info->i_size = PAGE_ALIGN(info->i_size) >> PAGE_SHIFT;
	nbits = info->i_size;
	info->bitmap_longs = BITS_TO_LONGS(nbits);
	info->bitmaps = bitmap_zalloc(nbits, GFP_KERNEL);
	if (!info->bitmaps) {
		MI_LB_ERR("alloc file_info bitmaps err");
		goto allock_file_info_fail;
	}

	path_len = strlen(f_path) + 1;
	info->path = lb_zmalloc(path_len, GFP_KERNEL);
	if (!info->path) {
		MI_LB_ERR("info->path alloc fail\n");
		goto allock_bitmap_fail;
	}
	strncpy(info->path, f_path, path_len);
	*file_info = info;
	list_add(&info->list, &active_package->file_infos);
	active_package->files++;
	mm_size = sizeof(struct file_info) + path_len
				+ info->bitmap_longs * sizeof(unsigned long);
	info->mm_size = mm_size;
	active_package->mm_size +=mm_size;
	g_manager->total_memory += mm_size;

	return 0;

allock_bitmap_fail:
	bitmap_free(info->bitmaps);
allock_file_info_fail:
	lb_kfree(info, sizeof(struct file_info));
out:
	return -1;
}

static void mi_lb_app_compiler(struct package_info *package)
{
	struct collect_info *collect_info;
	int i;
	struct file_info *file_info = NULL;
	unsigned int blocks, start;

	for (i = 0; i < g_manager->cur; i++) {
		collect_info = &((struct collect_info *)g_manager->record_buf)[i];

		if (mi_lb_get_fileinfo(package, collect_info->file, &file_info) ==0) {
			if ((collect_info->start + collect_info->len) <= file_info->i_size) {
				start = collect_info->start;
				blocks = collect_info->len;

#ifdef LB_DEBUG_COLLECT
				MI_LB_DBG("bitmapset file %s start:%ld, end:%lu\n",
						file_info->path, collect_info->start, collect_info->len + collect_info->start);
#endif
				if (g_debug_verbose) {
					unsigned begin = 0;
					for (begin = start; begin < (start + blocks); begin++) {
						if (!test_bit(begin, file_info->bitmaps))
						package->app_startup_bytes += 1 << PAGE_SHIFT;
					}
				}
				bitmap_set(file_info->bitmaps, start, blocks);
			}
		}
	}
}

static void mi_lb_compiler_do_work(struct lb_manager *manager)
{
	struct package_info *package = g_active_package.current_package;

	if (!package) {
		MI_LB_ERR("package is null");
		return;
	}

	mutex_lock(&g_manager->compiler_lock);
	memset(&g_active_package, 0, sizeof(g_active_package));
	clear_fileinfos_collected(package);
	mi_lb_app_compiler(package);
	mi_lb_add_package_info(package);
	mi_lb_del_lists_file_info(package);
	mi_lb_record_buf_clear(true);
	package_info_put(package);
	atomic_set(&collecting_, 0);
	mutex_unlock(&g_manager->compiler_lock);

	if (g_debug_verbose) {
		MI_LB_DBG("compiler finish: total_memory %lu apps %u",
						g_manager->total_memory, g_manager->app_num);

		package->hit_rate = 100 - (unsigned long)package->app_startup_bytes* 100 / package->total_startup_bytes;

		MI_LB_DBG("current app %s, This app load io: %u bytes, total io: %u bytes, hit_rate: %d%%",
					package->owner.package_name, package->app_startup_bytes, package->total_startup_bytes, package->hit_rate);
	}
}

static int mi_lb_compiler_thread(void *data)
{
	struct lb_manager *manager = data;

	if (!data) {
		MI_LB_ERR("data is null");
		return -EINVAL;
	}

	MI_LB_DBG("collecting_: %d",  atomic_read(&collecting_));

	while (!kthread_should_stop()) {
		DEFINE_WAIT(entry);
		prepare_to_wait(&manager->compiler_wait, &entry, TASK_UNINTERRUPTIBLE);
		schedule();
		finish_wait(&manager->compiler_wait, &entry);
		if (atomic_read(&collecting_) == 0)
			continue;
		mi_lb_compiler_do_work(manager);
	}

	return 0;
}


int mi_lb_init_manager(void)
{
	int ret;

	g_manager = kzalloc(sizeof(struct lb_manager), GFP_KERNEL);
	if (!g_manager) {
		ret = -ENOMEM;
		return ret;
	}
	g_manager->record_buf = kvmalloc(RECORD_BUFFER_SIZE, GFP_KERNEL);
	if (!g_manager->record_buf) {
		ret = -ENOMEM;
		goto out_free_g_manager;
	}
	g_manager->total_memory = RECORD_BUFFER_SIZE + sizeof(struct lb_manager);
	spin_lock_init(&g_manager->lock);

	mutex_init(&g_manager->compiler_lock);
	mutex_init(&g_manager->package_list_lock);
	INIT_LIST_HEAD(&g_manager->package_infos);
	INIT_DELAYED_WORK(&g_manager->compiler_work, mi_lb_do_compile);
	g_manager->cur = 0;
	g_manager->collected_num = 0;
	init_waitqueue_head(&g_manager->compiler_wait);
	g_manager->compiler_thread = kthread_run(
		mi_lb_compiler_thread,
		g_manager, "lb:g_manager");
	if (IS_ERR(g_manager->compiler_thread)) {
		ret = PTR_ERR(g_manager->compiler_thread);
		MI_LB_ERR("create compiler_thread fail, ret: %d", ret);
		goto create_collect_thread_fail;
	}
	memset(&g_active_package, 0, sizeof(g_active_package));
#ifdef MI_LB_PREREAD
	mi_lb_init_preread();
#endif

	return 0;

create_collect_thread_fail:
	kvfree(g_manager->record_buf);
out_free_g_manager:
	lb_kfree(g_manager, sizeof(struct lb_manager));
	return ret;
}

int mi_lb_exit_manager(void)
{
	mi_lb_record_buf_clear(false);
	if (g_manager) {
		kthread_stop(g_manager->compiler_thread);
		wake_up(&g_manager->compiler_wait);
		if (g_manager->record_buf)
			kvfree(g_manager->record_buf);
		lb_kfree(g_manager, sizeof(struct lb_manager));
	}
	return 0;
}
