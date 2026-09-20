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
#include "launch_boost_flush.h"


int mi_lb_dealwith_get_app_names(struct lb_app_list *app_list)
{

	struct list_head *pos = NULL;
	struct package_info *package_list = NULL;
	struct list_head *head = &g_manager->package_infos;
	int i = 0;
	struct lb_owner *owner = NULL;

	mutex_lock(&g_manager->package_list_lock);
	list_for_each(pos, head) {
		package_list = list_entry(pos, struct package_info, app_list);
		owner = &app_list->owner[i++];
		memcpy(owner, &package_list->owner, sizeof(struct lb_owner));
		if (i >= MAX_APPS_LIST) {
            break;
        }
	}
	app_list->app_nums = i;
	mutex_unlock(&g_manager->package_list_lock);

	return 0;
}















