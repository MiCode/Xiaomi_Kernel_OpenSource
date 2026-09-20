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

bool package_info_put(struct package_info *package)
{
	if (atomic_dec_and_test(&package->users)) {
		package_info_do_destroy(package);
		return true;
	}
	return false;
}

bool mi_lb_is_in_package_list(struct package_info *package)
{
	struct package_info *package_list = NULL;

	mutex_lock(&g_manager->package_list_lock);
	package_list = list_first_entry(&g_manager->package_infos, struct package_info, app_list);
	mutex_unlock(&g_manager->package_list_lock);

	return package_list == package;
}

int mi_lb_add_package_info(struct package_info *package)
{
	struct package_info *package_list = NULL;

	mutex_lock(&g_manager->package_list_lock);
	if (list_empty(&g_manager->package_infos)) {
		list_add(&package->app_list, &g_manager->package_infos);
		package_info_get(package);
		g_manager->app_num++;
		goto unlock;
	}
	package_list = list_first_entry(&g_manager->package_infos, struct package_info, app_list);
	if ((package_list->owner.uid == package->owner.uid)
		&& !strcmp(package_list->owner.package_name, package->owner.package_name)) {
		goto unlock;
	}

	list_add(&package->app_list, &g_manager->package_infos);
	package_info_get(package);
	g_manager->app_num++;
	mutex_unlock(&g_manager->package_list_lock);
	return 0;
unlock:
	mutex_unlock(&g_manager->package_list_lock);
	return 0;
}

struct package_info *mi_lb_get_package_info(struct lb_owner *owner, bool move_to_head)
{
	struct list_head *pos = NULL;
	struct package_info *package_list = NULL;
	struct list_head *head = &g_manager->package_infos;

	mutex_lock(&g_manager->package_list_lock);
	list_for_each(pos, head) {
		package_list = list_entry(pos, struct package_info, app_list);
		if ((package_list->owner.uid == owner->uid) &&
			!strcmp(package_list->owner.package_name, owner->package_name))
			break;
	}
	if (pos != head) {
		if (move_to_head)
			list_move(pos, head);
		package_info_get(package_list);
	}
	else
		package_list = NULL;
	mutex_unlock(&g_manager->package_list_lock);

	return package_list;
}

struct package_info *mi_lb_get_oldest_package_info(void)
{
	struct package_info *package_list = NULL;

	mutex_lock(&g_manager->package_list_lock);
	package_list = list_last_entry(&g_manager->package_infos, struct package_info, app_list);
	mutex_unlock(&g_manager->package_list_lock);

	return package_list;
}














