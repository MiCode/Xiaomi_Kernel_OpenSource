#ifndef _LOW_FREE_MEMORY_KILL_MODULE_H_
#define _LOW_FREE_MEMORY_KILL_MODULE_H_

#define MIN_INTERVAL_KILL_PROCESS 100  //ms
#define SERVICE_B_ADJ 800
#define PERCEPTIBLE_LOW_APP_ADJ 250
#define FOREGROUND_APP_ADJ 0

#define NAME_LEN 12
#define USER_MIN_UID         10000

#define RET_OK 0
#define RET_FAIL -1


#define KB(pages) ((pages) << (PAGE_SHIFT - 10))
#define PAGES(mb) ((mb * 1024) >> (PAGE_SHIFT - 10))
#define MAX(a,b) ((a)>(b) ? (a):(b))

#define MODE_RO  0440
#define MODE_RW  0660

enum memory_event_type {
	LOW_FREE_KILL = 1,
	MEMORY_EVENT_MAX,
};

enum enable_type {
	FUNC_ON = 1,
	DEBUG_ON,
};

typedef struct mem_stat {
	atomic_t wakeup_throttle_count;
	atomic_t wakeup_lmkd_count;
} mem_stat_t;

typedef struct memory {
	int			enable;
	mem_stat_t		stat;
	struct kobject		*kobj;
} memory_t;

#endif
