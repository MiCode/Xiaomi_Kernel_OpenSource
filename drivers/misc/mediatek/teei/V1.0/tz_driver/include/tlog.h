
#define TLOG_CONTEXT_LEN		(300)
#define TLOG_MAX_CNT			(50)

#define TLOG_UNUSE				(0)
#define TLOG_INUSE				(1)

#define UT_TLOG_VERSION			(2)
#define UT_TYPE_STRING			(1)
#define TLOG_SIZE			(256 * 1024)
#define MAX_LOG_LEN			(256)

/********************************************
	structures for LOG IRQ handler
 ********************************************/
struct tlog_struct {
	int valid;
	struct work_struct work;
	char context[TLOG_CONTEXT_LEN];
};

/********************************************
	structures for utOS printf
 ********************************************/
struct ut_log_buf_head {
	int version;
	int length;
	int write_pos;
	int reserve;
};

struct ut_log_entry {
	int type;
	char context;
	char reserve;
};

/********************************************
	structures for uTgate LOG
 ********************************************/
struct utgate_log_head {
	int version;
	int length;
	int write_pos;
	int reserve;
};

/*********************************************
	variables for LOG IRQ handler
 *********************************************/
static struct tlog_struct tlog_ent[TLOG_MAX_CNT];
extern unsigned long tlog_message_buff;
extern struct work_queue *secure_wq;
extern int irq_call_flag;
extern struct semaphore smc_lock;
