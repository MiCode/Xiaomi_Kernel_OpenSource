#define DEBUG 1
#define pr_fmt(fmt) "miev: " fmt

#include "mievent.h"
#include <linux/slab.h>
#include <linux/ktime.h>

static int jsonstr_size = strlen("{}");
static int format_size = strlen("EventId  -t  -paraList ");

struct misight_mievent *cdev_tevent_alloc(unsigned int eventid)
{
	struct misight_mievent *mievent =
		kmalloc(sizeof(struct misight_mievent), GFP_KERNEL);
	if (!mievent) {
		pr_err("mievent create error");
		goto err;
	}
	mievent->eventid = eventid;
	mievent->time = ktime_get_real_seconds();
	mievent->head = (struct mievent_payload *)kmalloc(
		sizeof(struct mievent_payload), GFP_KERNEL);
	if (!mievent->head) {
		pr_err("Fail to create mievent->head");
		goto err_head_nomem;
	}
	mievent->head->next = NULL;
	jsonstr_size = strlen("{}");
	pr_devel("create mievent success!");
	return mievent;

err_head_nomem:
	kfree(mievent);
err:
	return NULL;
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
	struct mievent_payload *p;
	struct mievent_payload *mievent_payload;
	int key_size = strlen(key);
	int value_size = get_integer_size(value);
	int payload_size = sizeof(struct mievent_payload);
	void *mem_blk = kmalloc(payload_size + key_size + 1 + value_size + 1,
				GFP_KERNEL);
	mievent_payload = (struct mievent_payload *)mem_blk;
	if (!mem_blk) {
		pr_err("mievent_payload create error");
		return -ENOMEM;
	}
	mievent_payload->key = (char *)(mem_blk + payload_size);
	mievent_payload->value =
		(char *)(mem_blk + payload_size + key_size + 1);
	mievent_payload->type = INT_T;
	mievent_payload->next = NULL;
	snprintf(mievent_payload->key, key_size + 1, "%s", key);
	snprintf(mievent_payload->value, value_size + 1, "%ld", value);

	p = event->head;
	mievent_payload->next = p->next;
	p->next = mievent_payload;
	jsonstr_size += key_size + FORMAT_QUOTES_SIZE + FORMAT_COLON_SIZE;
	jsonstr_size += value_size + FORMAT_COMMA_SIZE;
	pr_devel("add mievent payload success!");
	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_add_int);

int cdev_tevent_add_str(struct misight_mievent *event, const char *key,
			const char *value)
{
	struct mievent_payload *p;
	struct mievent_payload *mievent_payload;
	int key_size = strlen(key);
	int value_size = strlen(value);
	int payload_size = sizeof(struct mievent_payload);
	void *mem_blk = kmalloc(payload_size + key_size + 1 + value_size + 1,
				GFP_KERNEL);
	if (!mem_blk) {
		pr_err("mievent_payload create error");
		return -ENOMEM;
	}
	mievent_payload = (struct mievent_payload *)mem_blk;
	mievent_payload->key = (char *)(mem_blk + payload_size);
	mievent_payload->value =
		(char *)(mem_blk + payload_size + key_size + 1);
	mievent_payload->type = STR_T;
	mievent_payload->next = NULL;
	snprintf(mievent_payload->key, key_size + 1, "%s", key);
	snprintf(mievent_payload->value, value_size + 1, "%s", value);

	p = event->head;
	mievent_payload->next = p->next;
	p->next = mievent_payload;
	jsonstr_size += key_size + FORMAT_QUOTES_SIZE + FORMAT_COLON_SIZE;
	jsonstr_size += value_size + FORMAT_QUOTES_SIZE + FORMAT_COMMA_SIZE;
	pr_devel("add mievent payload success!");
	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_add_str);

int to_format(struct misight_mievent *event, char *jsonstr)
{
	struct mievent_payload *p;
	sprintf(jsonstr, "EventId %d -t %lld -paraList ", event->eventid,
		event->time);
	p = event->head->next;
	if (p == NULL) {
		strcat(jsonstr, "{}");
		return 0;
	}
	strcat(jsonstr, "{");
	while (p != NULL) {
		strcat(jsonstr, "\"");
		strcat(jsonstr, p->key);
		strcat(jsonstr, "\":");
		if (p->type == STR_T) {
			strcat(jsonstr, "\"");
		}
		strcat(jsonstr, p->value);
		if (p->type == STR_T) {
			strcat(jsonstr, "\"");
		}
		p = p->next;
		if (p != NULL) {
			strcat(jsonstr, ",");
		} else {
			jsonstr_size -= FORMAT_COMMA_SIZE;
			strcat(jsonstr, "}");
		}
	}
	pr_devel("jsonstr:%s", jsonstr);
	return 0;
}
void free_list(struct mievent_payload *head)
{
	struct mievent_payload *p;
	struct mievent_payload *q;
	p = head->next;
	while (p != NULL) {
		q = p->next;
		kfree(p);
		p = q;
	}
	head->next = NULL;
}

int cdev_tevent_write(struct misight_mievent *event)
{
	int buffer_size = format_size + get_integer_size(event->eventid) +
			  get_integer_size(event->time) + jsonstr_size;
	char *buffer = (char *)kmalloc(buffer_size, GFP_KERNEL);
	if (!buffer) {
		pr_err("buffer create error");
		return -ENOMEM;
	}

	to_format(event, buffer);
	pr_devel("buffer_size:%d,buffer len:%d", buffer_size, strlen(buffer));
	write_kbuf(buffer, strlen(buffer));

	kfree(buffer);
	free_list(event->head);
	jsonstr_size = strlen("{}");
	pr_devel("mievent report success!");
	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_write);

void cdev_tevent_destroy(struct misight_mievent *event)
{
	if (event != NULL) {
		free_list(event->head);
		kfree(event->head);
		kfree(event);
		pr_devel("kfree event");
	}
}
EXPORT_SYMBOL_GPL(cdev_tevent_destroy);