#ifndef __MT6575_PANICLOG_H__
#define __MT6575_PANICLOG_H__

#define PANICLOG_BUF_LEN 16384
#define PANICLOG_HEADER_SIZE 512
#define PANICLOG_SECTION_SIZE (PANICLOG_BUF_LEN) + (PANICLOG_HEADER_SIZE)

#ifndef __ASSEMBLY__

#define PANICLOG_VALID_PATTERN 0x90EFABCD
#define PANICLOG_BACKTRACE_NUM 4
#define PANICLOG_PROCESS_NAME_LENGTH 256

struct paniclog {
	int valid;		/* log is valid if valid == PANICLOG_VALID_PATTERN */
	int crc;

	struct {
		unsigned long bt_where[PANICLOG_BACKTRACE_NUM];
		unsigned long bt_from[PANICLOG_BACKTRACE_NUM];
		char process_path[PANICLOG_PROCESS_NAME_LENGTH];

		char buf[PANICLOG_BUF_LEN];
		int buf_len;
	} c;
};

extern struct paniclog *paniclog;

#ifdef CONFIG_PANICLOG

/* Begin starting panic record */
void paniclog_start(void);

void paniclog_end(void);

/* Record the task acenstor into current paniclog */
void paniclog_ptree_store(struct task_struct *tsk);

/* Record stack trace info into current paniclog */
void paniclog_stack_store(unsigned long where, unsigned long from);

/* Check if panic log available */
int paniclog_is_available(void);

/* Dump current paniclog to kernel log buffer */
void paniclog_dump(void);

/* Copy the current panic log and clear the panic log before return */
void paniclog_copy_and_clear(struct paniclog *log);

#else

#define paniclog_start(a)

#define  paniclog_end()

#define paniclog_ptree_store(tsk)

#define paniclog_stack_store(where, from)

#define paniclog_is_available() 0

#define paniclog_copy_and_clear(log)

#endif /* CONFIG_PANALOG_LOG */ 

#endif /* __ASSEMBLY */

#endif  /* !__MT6575_PANICLOG_H__ */

