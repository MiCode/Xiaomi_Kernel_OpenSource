#include "utdriver_macro.h"

struct work_entry {
	int call_no;
	struct work_struct work;
};

struct service_handler {
	unsigned int sysno;
	void *param_buf;
	unsigned size;
	long (*init)(struct service_handler *handler);
	void (*deinit)(struct service_handler *handler);
	int (*handle)(struct service_handler *handler);
};

struct NQ_entry {
	unsigned int valid_flag;
	unsigned int length;
	unsigned int buffer_addr;
	unsigned char reserve[20];
};

struct load_soter_entry {
	unsigned long vfs_addr;
	struct work_struct work;
};


struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};


extern irqreturn_t tlog_handler(void);
extern int vfs_thread_function(unsigned long virt_addr, unsigned long para_vaddr, unsigned long buff_vaddr);
extern int get_current_cpuid(void);
extern unsigned char *get_nq_entry(unsigned char *buffer_addr);
extern unsigned long t_nt_buffer;
extern unsigned long message_buff;
extern unsigned long boot_vfs_addr;
extern unsigned long boot_soter_flag;

static struct load_soter_entry load_ent;

extern struct service_handler reetime;
extern struct service_handler socket;
extern struct service_handler vfs_handler;
extern struct service_handler printer_driver;

extern unsigned int forward_call_flag;
extern unsigned int soter_error_flag;
extern struct semaphore smc_lock;
extern struct completion global_down_lock;
extern unsigned long teei_config_flag;

extern unsigned long bdrv_message_buff;
extern void load_func(struct work_struct *entry);
extern void work_func(struct work_struct *entry);
extern int irq_call_flag;
extern struct semaphore boot_sema;
extern struct semaphore fdrv_sema;
extern int fp_call_flag;
extern int keymaster_call_flag;
static struct work_entry work_ent;
extern struct work_queue *secure_wq;
extern int teei_vfs_flag;

