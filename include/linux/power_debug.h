#include<linux/list.h>
#include<linux/seq_file.h>

struct wakeup_device {
	struct list_head list;
	char *name;
	void *data;
	unsigned int count;
	bool (*check_wakeup_event)(void *data);
};

struct system_event_file_ops {
	int (*event_show)(struct seq_file *seq, void *v);
};

enum system_event_type {
	SYSTEM_EVENT_ALARM,
	SYSTEM_EVENT_UEVENT,
	SYSTEM_EVENT_MAX,
};

struct system_event {
	char creator_info[100];
	struct timespec set_time;
};

struct system_event_recorder {
	struct list_head list;
	char *name;
	enum system_event_type type;
	struct system_event_file_ops *ops;
	void (*system_event_record)(struct system_event_recorder *recorder, void *data);
	void (*system_event_print_records)(struct system_event_recorder *recorder);
	u32 max_num;
	void *buff;
	u32 count;
	u32 index_head;
	u32 index_tail;
};

int pm_register_wakeup_device(struct wakeup_device *dev);
int pm_register_system_event_recorder(struct system_event_recorder *rec);
void pm_trigger_system_event_record(enum system_event_type type, void *data);
