// MIUI ADD: MiSight_LogEnhance
#ifndef _EVENT_MIEVENT_H_
#define _EVENT_MIEVENT_H_

#include <linux/module.h>
#include <linux/kernel.h>

#define FORMAT_COMMA_SIZE 1
#define FORMAT_COLON_SIZE 1
#define FORMAT_QUOTES_SIZE 2


enum DATA_TYPE { INT_T = 0, STR_T };
extern int write_kbuf(char __kernel *kbuf, int size);

/* mievent struct */
struct misight_mievent {
	unsigned int eventid;
	long long time;
	/* payload linked list */
	struct mievent_payload *head;
};

struct mievent_payload {
	int type;
	char *key;
	char *value;
	struct mievent_payload *next;
};

struct misight_mievent *cdev_tevent_alloc(unsigned int eventid)
{

	return NULL; 
}
int cdev_tevent_add_int(struct misight_mievent *event, const char *key,
                        long value)
{
	return 0;
}
int cdev_tevent_add_str(struct misight_mievent *event, const char *key,
                        const char *value)
{
	return 0;
}
int cdev_tevent_write(struct misight_mievent *event)
{
	return 0;
}
void cdev_tevent_destroy(struct misight_mievent *event)
{
    return;
}
#endif // _EVENT_MIEVENT_H_
// END MiSight_LogEnhance
