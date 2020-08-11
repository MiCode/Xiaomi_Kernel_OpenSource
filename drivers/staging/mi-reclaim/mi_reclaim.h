#ifndef _MI_RECLAIM_MODULE_H_
#define _MI_RECLAIM_MODULE_H_

#define ONCE_RECLAIM_PAGES 38400  //150MB, unit page
#define MIN_RECLAIM_PAGES  32

#define DEFAULT_ANON_PAGE_UP_THRESHOLD_FOR_EIGHTGB  419430 //1.6GB, unit page
#define DEFAULT_ANON_PAGE_UP_THRESHOLD_FOR_TWELVEGB 917504 //3.5GB, unit page
#define FREE_SWAP_LIMIT 230400 //900MB, unit page
#define RECLAIM_INTERVAL_TIME  1000  //ms
#define RECLAIM_SWAPPINESS 120

#define RAM_EIGHTGB_SIZE 2097152 //8GB unit page

#define RET_OK 0
#define RET_FAIL -1

#define KB(pages) ((pages) << (PAGE_SHIFT - 10))
#define PAGES(mb) ((mb * 1024) >> (PAGE_SHIFT - 10))

#define MI_RECLAIM_MODE_RO  0440
#define MI_RECLAIM_MODE_RW  0660

enum reclaim_index {
	UNMAP_PAGE = 1,
	SWAP_PAGE = 2,
	WRITE_PAGE = 4,
	PAGE_TYPE_MAX,
};

enum event_type {
	CANCEL_ST = 0,
	RECLAIM_MODE = 1,
	ST_MODE = 2,
	EVENT_MAX,
};

typedef struct global_reclaim_page {
	int           reclaim_swappiness;
	int           reclaim_type;
	int           event_type;
	unsigned long last_reclaim_time;
	u32           nr_reclaim;
	u32           anon_up_threshold;
	u64           total_spent_time;
	u64           total_reclaim_pages;
	u64           total_reclaim_times;
	spinlock_t    reclaim_lock;
} global_reclaim_page_t;

typedef struct sys {
	u64 totalram;
} sys_t;

typedef struct mi_reclaim {
	bool                      switch_on;
	bool                      debug;
	bool                      need_reclaim;
	wait_queue_head_t         wait;
	struct kobject            *kobj;
	struct task_struct        *task;
	global_reclaim_page_t     page_reclaim;
	sys_t                     sysinfo;
} mi_reclaim_t;

#endif
