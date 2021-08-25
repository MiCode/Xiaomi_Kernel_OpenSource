#define BWC_NUMS		6
#define BWC_MAX_INDEX		5
#define DEFAULT_BDC_INDEX	0
#define BWC_NAME_LEN		20

#define IO_PRESSURE_WARNING_PROCESS_NUMS 		12
#define IO_PRESSURE_WARNING_SCAN_PROCESS_LEVEL		16  /* ms*/
#define IO_PRESSURE_WARNING_WAKEUP_THREAD_LEVEL		50 /* ms*/
#define IO_PRESSURE_WARNING_BW_LEVEL			10000 /*10 MB*/
#define IO_PRESSURE_THREAD_LOG_INTERVAL			5000 /* ms */
#define THREAD_WAIT_FOREVER				0 /* ms */


enum io_type {
	NORMAL_READ,
	NORMAL_WRITE,
	DIRECT_READ,
	DIRECT_WRITE,
};

enum io_monitor_type {
	IO_WARNING,
	IO_LIMIT,
};

typedef struct bwc_stats {
	bool	enable;
	u64		nr_count; /* normal read count */
	u64		nw_count; /* normal write count */
	u64		dr_count; /* direct read count */
	u64		dw_count; /* direct write count */
	u64		dr_limit_count; /* direct read limit count */
	u64		dw_limit_count; /* direct write limit count */
	u64		nr_limit_count; /* normal read limit count */
	u64		nw_limit_count; /* normal write limit count */
} bwc_stats_t;

typedef struct io_monitor_info {
	pid_t	pid;
	char	comm[TASK_COMM_LEN];
	int		group;
	int		adj;
	int		type;
	u64		rbw;
	u64		wbw;
	u64		stime;
	u64		wtime;
	unsigned long	io_count;
	unsigned long	limit_time;
	loff_t	fsize;
} io_monitor_info_t;

typedef struct io_monitor {
	bool 					io_pressure_warning;
	bool					log_out;
	unsigned long 			last_out_log_time;
	io_monitor_info_t		io_monitor_info[IO_PRESSURE_WARNING_PROCESS_NUMS];
	spinlock_t				io_lock;
	struct semaphore		wait;
	struct task_struct		*task;
} io_monitor_t;

typedef struct token_throttle {
	bool				token_ok;
	unsigned int		token_remains;
	unsigned int		push_token_interval;
	unsigned long		token_current_capacity;
	unsigned long		last_token_dispatch_time;
	unsigned int		token_bucket_capacity;
	unsigned long		capacity_per_token;  /* how many bandwidth per token */
	wait_queue_head_t	token_waitqueue;
} token_throttle_t;

typedef struct bandwidth_control {
	bool				bwc_init_ok;
	bool				limit_switch;
	bool				debug;
	char				name[BWC_NAME_LEN];
	int					index;
	int					adj_limit_level;
	token_throttle_t	token_throttle;
	bwc_stats_t			stats;
	spinlock_t			bwc_lock;
} bandwidth_control_t;

typedef struct bwc_manager {
	bool			bwcm_switch;
	bool			bwcm_init_ok;
	bool			bwcm_lock_init_ok;
	bool			bwcm_debug;
	int				bwc_online; /* bwc bitmap */
	int				bwc_index; /* for configuration selection */
	u64				last_stime;
	u64				last_wtime;
	spinlock_t		bwcm_lock;
	bandwidth_control_t		bwc[BWC_NUMS];
	io_monitor_t			io_moniter;
	struct kobject			*p_kobj;
	struct kobject			*p_stats_kobj;
} bwc_manager_t;

/* API for external */
extern void unregister_bwc(bandwidth_control_t *p_bwc);
extern void mi_io_bwc(struct kiocb *iocb, struct inode *inode,
		pgoff_t page_index, size_t count, enum io_type type);
extern bandwidth_control_t *register_bwc(char *name, unsigned long bandwidth);
