#include "mievent.h"

static struct misight_mievent miev_dummy_event;

struct misight_mievent *cdev_tevent_alloc(unsigned int eventid)
{
	miev_dummy_event.eventid = eventid;
	return &miev_dummy_event;
}
EXPORT_SYMBOL_GPL(cdev_tevent_alloc);

int cdev_tevent_add_int(struct misight_mievent *event, const char *key,
			long value)
{
	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_add_int);

int cdev_tevent_add_str(struct misight_mievent *event, const char *key,
			const char *value)
{
	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_add_str);

int cdev_tevent_write(struct misight_mievent *event)
{
	return 0;
}
EXPORT_SYMBOL_GPL(cdev_tevent_write);

void cdev_tevent_destroy(struct misight_mievent *event)
{
	return;
}
EXPORT_SYMBOL_GPL(cdev_tevent_destroy);
