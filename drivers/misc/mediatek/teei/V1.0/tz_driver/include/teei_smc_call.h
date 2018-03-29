
#include "teei_smc_struct.h"
#include "sched_status.h"

#define NQ_VALID                                1

struct teei_contexts_head {
	u32 dev_file_cnt;
	struct list_head context_list;
	struct rw_semaphore teei_contexts_sem;
};

/******************************
 * Message header
 ******************************/

struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};


struct smc_call_struct {
	unsigned long local_cmd;
	u32 teei_cmd_type;
	u32 dev_file_id;
	u32 svc_id;
	u32 cmd_id;
	u32 context;
	u32 enc_id;
	void *cmd_buf;
	size_t cmd_len;
	void *resp_buf;
	size_t resp_len;
	void *meta_data;
	void *info_data;
	size_t info_len;
	int *ret_resp_len;
	int *error_code;
	struct semaphore *psema;
	int retVal;
};

extern struct semaphore smc_lock;
extern struct teei_contexts_head teei_contexts_head;
extern int forward_call_flag;
extern unsigned long t_nt_buffer;
extern struct smc_call_struct smc_call_entry;
extern unsigned long message_buff;
extern struct completion global_down_lock;
extern unsigned long teei_config_flag;

extern void tz_free_shared_mem(void *addr, size_t size);
extern void *tz_malloc_shared_mem(size_t size, int flags);
extern int get_current_cpuid(void);
extern int add_nq_entry(unsigned char *command_buff, int command_length, int valid_flag);
extern int add_work_entry(int work_type, unsigned long buff);
