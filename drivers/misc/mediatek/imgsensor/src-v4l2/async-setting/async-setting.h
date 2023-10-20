#ifndef __ASYNC_SETTING_H__
#define __ASYNC_SETTING_H__

#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include "task-queue.h"
#include "adaptor-subdrv.h"


typedef enum {
	INIT_SETTING,
	RES_SETTING,
	EXP_SETTING,
} SETTING_TYPE;

struct setting_workqueue {
	int count;
	struct mutex mutex;
	wait_queue_head_t wq;
	task_queue_t *queue;
};

struct setting_work {
	char name[64];
	bool used;
	SETTING_TYPE type;
	unsigned int len;
	unsigned short *setting;
	struct subdrv_ctx *ctx;
	int (*write_setting)(struct subdrv_ctx *ctx, unsigned short *setting, unsigned int len);
	struct setting_workqueue *setting_workqueue;
	task_t work;
};

#define destroy_setting_workqueue(setting_workqueue) \
({                                                   \
	__destroy_setting_workqueue(setting_workqueue);  \
	setting_workqueue = NULL;                        \
})

struct setting_work* create_and_queue_setting_work(
			struct setting_workqueue *setting_workqueue,
			char *name,
			SETTING_TYPE type,
			int (*write_setting)(struct subdrv_ctx *ctx, unsigned short *setting, unsigned int len),
			struct subdrv_ctx *ctx,
			unsigned short *setting,
			unsigned int len);

struct setting_workqueue* create_setting_workqueue(char *name);
void __destroy_setting_workqueue(struct setting_workqueue *setting_workqueue);

void queue_setting_work(struct setting_work *setting_work);
void wait_workqueue_done(struct setting_workqueue *setting_workqueue);

void lock_setting_work(struct setting_workqueue *setting_workqueue);
void unlock_setting_work(struct setting_workqueue *setting_workqueue);
#endif
