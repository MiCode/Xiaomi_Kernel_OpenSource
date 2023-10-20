#include "async-setting.h"
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>


void do_async_setting(void *param)
{
	struct setting_work *setting_work = (struct setting_work *)param;
	pr_info("do_async_setting : %d-%s + \n", setting_work->type, setting_work->name);
	setting_work->write_setting(setting_work->ctx, setting_work->setting, setting_work->len);
	pr_info("do_async_setting : %d-%s - \n", setting_work->type, setting_work->name);

	mutex_lock(&setting_work->setting_workqueue->mutex);
	setting_work->setting_workqueue->count--;
	mutex_unlock(&setting_work->setting_workqueue->mutex);

	if (setting_work->setting_workqueue->count == 0) {
		wake_up(&setting_work->setting_workqueue->wq);
		pr_info("wake up!");
	}
	kfree(setting_work);
}

struct setting_work* create_and_queue_setting_work(
			struct setting_workqueue *setting_workqueue,
			char *name,
			SETTING_TYPE type,
			int (*write_setting)(struct subdrv_ctx *ctx, unsigned short *setting, unsigned int len),
			struct subdrv_ctx *ctx,
			unsigned short *setting,
			unsigned int len)
{
	struct setting_work *setting_work = NULL;

	if (!name || !ctx || !setting || !write_setting || !setting_workqueue) {
		pr_err("Failed to create_setting_work\n");
		return NULL;
	}


	setting_work = kmalloc(sizeof(struct setting_work), GFP_KERNEL);
	if (!setting_work) {
		return NULL;
	}

	strcpy(setting_work->name, name);
	setting_work->type              = type;
	setting_work->len               = len;
	setting_work->setting           = setting;
	setting_work->ctx               = ctx;
	setting_work->write_setting     = write_setting;
	setting_work->setting_workqueue = setting_workqueue;
	setting_work->work.run          = do_async_setting;
	setting_work->work.param        = setting_work;

	queue_setting_work(setting_work);
	return setting_work;
}

struct setting_workqueue* create_setting_workqueue(char *name)
{
	struct setting_workqueue *setting_workqueue = NULL;

	setting_workqueue = kmalloc(sizeof(struct setting_workqueue), GFP_KERNEL);
	if (!setting_workqueue) {
		pr_err("Failed to create_setting_workqueue : %s\n", name);
		return NULL;
	}

	init_waitqueue_head(&setting_workqueue->wq);
	mutex_init(&setting_workqueue->mutex);
	setting_workqueue->count = 0;
	setting_workqueue->queue = queue_create(name);
	if (!setting_workqueue->queue) {
		pr_err("Failed to create_setting_workqueue : %s\n", name);
		kfree(setting_workqueue);
		return NULL;
	}

	return setting_workqueue;
}

void __destroy_setting_workqueue(struct setting_workqueue *setting_workqueue)
{
	if (setting_workqueue) {
		if (setting_workqueue->queue) {
			queue_free(setting_workqueue->queue);
		}

		kfree(setting_workqueue);
	}
}

void queue_setting_work(struct setting_work *setting_work)
{
	if (setting_work && setting_work->setting_workqueue) {
		mutex_lock(&setting_work->setting_workqueue->mutex);
		setting_work->setting_workqueue->count++;
		mutex_unlock(&setting_work->setting_workqueue->mutex);

		queue_push_tail(setting_work->setting_workqueue->queue, &setting_work->work);
	} else {
		pr_err("Failed to queue_setting_work\n");
	}
}

void wait_workqueue_done(struct setting_workqueue *setting_workqueue)
{
	if (setting_workqueue) {
		wait_event(setting_workqueue->wq, setting_workqueue->count == 0);
	}
}

void lock_setting_work(struct setting_workqueue *setting_workqueue)
{
	if (setting_workqueue) {
		queue_lock(setting_workqueue->queue);
	} else {
		pr_err("Failed to queue_setting_work\n");
	}
}

void unlock_setting_work(struct setting_workqueue *setting_workqueue)
{
	if (setting_workqueue) {
		queue_unlock(setting_workqueue->queue);
	} else {
		pr_err("Failed to queue_setting_work\n");
	}
}

