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

#include "mi_lb_def.h"
#include "launch_boost_collect.h"
#include "launch_boost_com.h"
#include "launch_boost_clear.h"

void free_one_fileinfo(struct package_info *package, struct file_info *file_entry)
{
	list_del(&file_entry->list);
	package->files--;
	package->mm_size -= file_entry->mm_size;
	g_manager->total_memory -= file_entry->mm_size;

	if(file_entry->path)
		kfree(file_entry->path);

	if(file_entry->bitmaps)
		kfree(file_entry->bitmaps);

	kfree(file_entry);
}

bool package_info_do_destroy(struct package_info *package)
{
	struct file_info *file_entry, *tmp;

	mutex_lock(&g_manager->package_list_lock);
	list_del(&package->app_list);
	g_manager->app_num--;
	g_manager->total_memory -= package->mm_size;
	mutex_unlock(&g_manager->package_list_lock);

	list_for_each_entry_safe(file_entry, tmp, &package->file_infos, list)
					free_one_fileinfo(package, file_entry);

	kfree(package);

	return true;
}

int mi_lb_dealwith_clear_cmd(struct lb_owner *owner)
{
	struct package_info *package = NULL;

	MI_LB_DBG("clear cmd : package uid: %d, pid: %d, packagename: %s",
   				owner->uid, owner->pid, owner->package_name);

	package = mi_lb_get_package_info(owner, false);
	if (!package) {
		MI_LB_DBG("app not found, don't clear");
		return -1;
	}
	mutex_lock(&package->package_lock);
	if (package->is_cleared) {
		mutex_unlock(&package->package_lock);
		MI_LB_DBG("app is cleared, don't clear");
		//Drop the usage of mi_lb_get_package_info
		package_info_put(package);
		return 0;
	}
	package->is_cleared = true;
	mutex_unlock(&package->package_lock);

	//Drop the usage of mi_lb_get_package_info
	package_info_put(package);
	//Release immediately if not collecting; else, ref count -1 and release after compiler
	package_info_put(package);
	return 0;
}















