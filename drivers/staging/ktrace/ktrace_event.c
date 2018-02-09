
#define pr_fmt(fmt)  "ktrace : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/ktrace.h>

struct event_entry {
	atomic64_t created_count;
	atomic64_t deleted_count;
};

struct ktrace_event {
	struct dentry *dir;

	struct event_entry entry[KTRACE_EVENT_NR];

};

static const char * const event_str[KTRACE_EVENT_NR] = {
	"event-1",
	"event-2",
	"event-3",
	"event-4",
	"event-5",
	"event-6",
	"event-7",
	"event-8",
	"event-9",
	"event-10",

	"event-11",
	"event-12",
	"event-13",
	"event-14",
	"event-15",
	"event-16",
	"event-17",
	"event-18",
	"event-19",
	"event-20",
};

static struct ktrace_event __event;

void ktrace_event_inc(unsigned char type)
{
	struct ktrace_event *event = &__event;
	struct event_entry *ee = &event->entry[type];

	atomic64_inc(&ee->created_count);
}

void ktrace_event_dec(unsigned char type)
{
	struct ktrace_event *event = &__event;
	struct event_entry *ee = &event->entry[type];

	atomic64_inc(&ee->deleted_count);
}

void ktrace_event_add(unsigned char type, int i)
{
	struct ktrace_event *event = &__event;
	struct event_entry *ee = &event->entry[type];

	atomic64_add_return(i, &ee->created_count);
}

void ktrace_event_sub(unsigned char type, int i)
{
	struct ktrace_event *event = &__event;
	struct event_entry *ee = &event->entry[type];

	atomic64_add_return(i, &ee->deleted_count);
}

static int event_stats_show(struct seq_file *m, void *v)
{
	struct ktrace_event *event = m->private;
	int i;

	seq_puts(m, "event stats:\n");

	for (i = 0; i < KTRACE_EVENT_NR; i++) {
		s64 created = atomic64_read(&event->entry[i].created_count);
		s64 deleted = atomic64_read(&event->entry[i].deleted_count);

		seq_printf(m, "  %-20s  %lld - %lld = %lld\n", event_str[i],
			   created, deleted, created - deleted);
	}

	return 0;
}

KTRACE_ENTRY_RO_SINGLE(event_stats);

int __init ktrace_event_init(struct dentry *dir)
{
	struct ktrace_event *event = &__event;

	memset(event, 0, sizeof(struct ktrace_event));

	event->dir = debugfs_create_dir("event", dir);

	if (event->dir) {
		debugfs_create_file("stats",
				    S_IFREG | S_IRUGO,
				    event->dir,
				    event,
				    &event_stats_fops);
	}

	pr_info("event ktrace init OK\n");

	return 0;
}
