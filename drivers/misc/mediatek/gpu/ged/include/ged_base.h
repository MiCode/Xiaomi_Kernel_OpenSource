#ifndef __GED_BASE_H__
#define __GED_BASE_H__

#include <linux/xlog.h>
#include "ged_type.h"

#ifdef GED_DEBUG
#define GED_LOGI(...)	xlog_printk(ANDROID_LOG_INFO, "GED", __VA_ARGS__)
#else
#define GED_LOGI(...)
#endif
#define GED_LOGE(...)	xlog_printk(ANDROID_LOG_ERROR, "GED", __VA_ARGS__)
#define GED_CONTAINER_OF(ptr, type, member) ((type *)( ((char *)ptr) - offsetof(type,member) ))

unsigned long ged_copy_to_user(void __user *pvTo, const void *pvFrom, unsigned long ulBytes);

unsigned long ged_copy_from_user(void *pvTo, const void __user *pvFrom, unsigned long ulBytes);

void* ged_alloc(int i32Size);

void ged_free(void* pvBuf, int i32Size);

long ged_get_pid(void);

#endif
