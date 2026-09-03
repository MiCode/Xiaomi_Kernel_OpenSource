// MIUI ADD: MiSight_LogEnhance

#define pr_fmt(fmt) "miev: " fmt

#include "mievent.h"
#include <linux/slab.h>
#include <linux/ktime.h>

#include "common.h"

struct misight_mievent *cdev_tevent_alloc(unsigned int eventid)
{
	int buf_size = BUF_MAX_SIZE - sizeof(struct misight_mievent);
	struct misight_mievent *mievent = NULL;
	char ** p = miev_get_work_msg();
	char * para = NULL;
	int len = 0;

	if (p == NULL || *p == NULL) {
		pr_err("miev_alloc:mievent create error");
		return NULL;
	}
	mievent = (struct misight_mievent *)(*p);
	mievent->buf_ptr = p;
	mievent->eventid = eventid;
	mievent->time = ktime_get_real_seconds();
	mievent->para_cnt = 0;
	para = (char *)(*p) + sizeof(struct misight_mievent);
	// add event head
	len = snprintf(para, buf_size, "EventId %d -t %lld -paraList {",
			mievent->eventid, mievent->time);
	if (len <= 0) {
		pr_err("miev_alloc:add json head failed");
		return NULL;
	}
	mievent->used_size = len + sizeof(struct misight_mievent); // not include end str;
	return mievent;
}
EXPORT_SYMBOL_GPL(cdev_tevent_alloc);

int get_integer_size(long number)
{
	int num_count = 0;
	long tmp = 0;

	if (number < 0) {
		num_count++;
	}
	tmp = number;
	do {
		num_count++;
		tmp = tmp / 10;
	} while (tmp != 0);

	return num_count;
}

int cdev_tevent_add_int(struct misight_mievent *event, const char *key,
			long value)
{
	char *pa  = NULL;
	int len = 0;
	int last_len = 0;
	int value_len = get_integer_size(value);
	int json_sym = FORMAT_QUOTES_SIZE + FORMAT_COLON_SIZE;

	if (event == NULL || event->used_size >= BUF_MAX_SIZE -1) {
		pr_err("add_int:para over size, %p can not add %s", event, key);
		return -1;
	}
	last_len = BUF_MAX_SIZE - event->used_size;
	if (event->para_cnt != 0) {
		json_sym += FORMAT_COMMA_SIZE;
	}

	if (strlen(key) + value_len + json_sym >= last_len -1) {
		pr_err("add_int:new para over size, %u can not add %s", event->eventid, key);
		return -1;
	}

	pa = (char *)event + event->used_size;
	if (event->para_cnt == 0) {
		len = snprintf(pa, last_len, "\"%s\":%ld", key, value);
	} else {
		len = snprintf(pa, last_len, ",\"%s\":%ld", key, value);
	}
	if (len <= 0) {
		pr_err("add_int:%u add %s failed, len %d", event->eventid, key, len);
		return -1;
	}
	event->para_cnt += 1;
	event->used_size += len;// not include end str

	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_add_int);

int cdev_tevent_add_str(struct misight_mievent *event, const char *key,
			const char *value)
{
	char *pa  = NULL;
	int len = 0;
	int last_len = 0;
	int json_sym = FORMAT_QUOTES_SIZE * 2 + FORMAT_COLON_SIZE;

	if (event == NULL || event->used_size >= BUF_MAX_SIZE -1) {
		pr_err("add_str:para over size, %p can not add %s", event, key);
		return -1;
	}
	last_len = BUF_MAX_SIZE - event->used_size;
	if (event->para_cnt != 0) {
		json_sym += FORMAT_COMMA_SIZE;
	}
	if (strlen(key) + strlen(value) + json_sym >= last_len -1) {
		pr_err("add_str:new para over size, %p can not add %s", event, key);
		return -1;
	}

	pa = (char *)event + event->used_size;
	if (event->para_cnt == 0) {
		len = snprintf(pa, last_len, "\"%s\":\"%s\"", key, value);
	} else {
		len = snprintf(pa, last_len, ",\"%s\":\"%s\"", key, value);
	}
	if (len <= 0) {
		pr_err("add_str:%p add %s failed", event, key);
		return -1;
	}

	event->para_cnt += 1;
	event->used_size += len;// not include end str

	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_add_str);

int cdev_tevent_write(struct misight_mievent *event)
{
	char *pa = NULL;
	int len = 0;
	int last_len = 0;

	if (event == NULL) {
		pr_err("event_write:event is null %d", current->pid);
		return -1;
	}
	if (event->para_cnt == 0 || event->used_size >= BUF_MAX_SIZE -1) {
		pr_err("event_write:%p para cnt %d, used %d", event, event->para_cnt, event->used_size);
		return -1;
	}

	pa = (char *)event + event->used_size;
	last_len = BUF_MAX_SIZE - event->used_size;
	len = snprintf(pa, last_len, "}");
	if (len <= 0) {
		pr_err("event_write:add json end str failed %p", event);
		return -1;
	}
	event->used_size += len;
	pa = (char *)event + sizeof(struct misight_mievent);
	write_kbuf(event->buf_ptr, sizeof(struct misight_mievent), event->used_size - sizeof(struct misight_mievent));

	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_write);

void cdev_tevent_destroy(struct misight_mievent *event)
{
	if (event == NULL) {
		return;
	}
	miev_release_work_msg(event->buf_ptr);
}
EXPORT_SYMBOL_GPL(cdev_tevent_destroy);
// END MiSight_LogEnhance
