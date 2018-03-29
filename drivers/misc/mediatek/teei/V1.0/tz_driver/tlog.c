#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/irq.h>
#include <linux/kthread.h>

#include "sched_status.h"
#include "tlog.h"
#include "teei_id.h"

/********************************************
		LOG IRQ handler
 ********************************************/

void init_tlog_entry(void)
{
	int i = 0;

	for (i = 0; i < TLOG_MAX_CNT; i++)
		tlog_ent[i].valid = TLOG_UNUSE;

	return;
}

int search_tlog_entry(void)
{
	int i = 0;

	for (i = 0; i < TLOG_MAX_CNT; i++) {
		if (tlog_ent[i].valid == TLOG_UNUSE) {
			tlog_ent[i].valid = TLOG_INUSE;
			return i;
		}
	}

	return -1;
}


void tlog_func(struct work_struct *entry)
{
	struct tlog_struct *ts = container_of(entry, struct tlog_struct, work);

	pr_info("TLOG %s", (char *)(ts->context));

	ts->valid = TLOG_UNUSE;
	return;
}


irqreturn_t tlog_handler(void)
{
	int pos = 0;

	pos = search_tlog_entry();

	if (-1 != pos) {
		memset(tlog_ent[pos].context, 0, TLOG_CONTEXT_LEN);
		memcpy(tlog_ent[pos].context, (char *)tlog_message_buff, TLOG_CONTEXT_LEN);
		Flush_Dcache_By_Area((unsigned long)tlog_message_buff, (unsigned long)tlog_message_buff + TLOG_CONTEXT_LEN);
		INIT_WORK(&(tlog_ent[pos].work), tlog_func);
		queue_work(secure_wq, &(tlog_ent[pos].work));
	}

	/* irq_call_flag = GLSCH_HIGH; */
	/* up(&smc_lock); */

	return IRQ_HANDLED;
}

/**************************************************
		LOG thread for printf
 **************************************************/

unsigned long tlog_thread_buff = 0;
unsigned long tlog_buf = NULL;
unsigned long tlog_pos = 0;
unsigned char tlog_line[256];
unsigned long tlog_line_len = 0;
struct task_struct *tlog_thread = NULL;

long init_tlog_buff_head(unsigned long tlog_virt_addr, unsigned long buff_size)
{
	long retVal = 0;
	struct ut_log_buf_head *tlog_head = NULL;

	if (tlog_virt_addr == NULL)
		return -EINVAL;

	if (buff_size < 0)
		return -EINVAL;

	tlog_thread_buff = tlog_virt_addr;
	tlog_buf = tlog_virt_addr;

	memset(tlog_virt_addr, 0, buff_size);
	tlog_head = (struct ut_log_buf_head *)tlog_virt_addr;

	tlog_head->version = UT_TLOG_VERSION;
	tlog_head->length = buff_size;
	tlog_head->write_pos = 0;

	Flush_Dcache_By_Area((unsigned long)tlog_virt_addr, (unsigned long)tlog_virt_addr + buff_size);

	return 0;
}

int tlog_print(unsigned long log_start)
{
	struct ut_log_entry *entry = NULL;

	entry = (struct ut_log_entry *)log_start;

	if (entry->type != UT_TYPE_STRING) {
		pr_err("[%s][%d]ERROR: tlog type is invaild!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (entry->context == '\n') {
		pr_info("[UT_LOG] %s\n", tlog_line);
		tlog_line_len = 0;
		tlog_line[0] = 0;
	} else {
		tlog_line[tlog_line_len] = entry->context;
		tlog_line[tlog_line_len + 1] = 0;
		tlog_line_len++;
	}

	tlog_pos = (tlog_pos + sizeof(struct ut_log_entry)) % (((struct ut_log_buf_head *)tlog_buf)->length - sizeof(struct ut_log_buf_head));

	return 0;
}

int handle_tlog(void)
{
	int retVal = 0;
	unsigned long shared_buff_write_pos = ((struct ut_log_buf_head *)tlog_buf)->write_pos;
	unsigned long tlog_cont_pos = (unsigned long)tlog_buf + sizeof(struct ut_log_buf_head);
	unsigned long last_log_pointer = tlog_cont_pos + shared_buff_write_pos;
	unsigned long start_log_pointer = tlog_cont_pos + tlog_pos;

	while (last_log_pointer != start_log_pointer) {
		retVal = tlog_print(start_log_pointer);

		if (retVal != 0) {
			pr_err("[%s][%d]fail to print tlog last_log_pointer = %x, start_log_pointer = %x!\n", __func__, __LINE__, last_log_pointer, start_log_pointer);
			tlog_pos = shared_buff_write_pos;
		}

		start_log_pointer = tlog_cont_pos + tlog_pos;
	}

	return 0;
}

int tlog_worker(void *p)
{
	int ret = 0;

	if (tlog_thread_buff == NULL) {
		pr_err("[%s][%d] tlog buff is NULL !\n", __func__, __LINE__);
		return -1;
	}

	while (!kthread_should_stop()) {
		if (((struct ut_log_buf_head *)tlog_buf)->write_pos == tlog_pos) {
			schedule_timeout_interruptible(1 * HZ);
			continue;
		}

		switch (((struct ut_log_buf_head *)tlog_buf)->version) {
		case UT_TLOG_VERSION:
			ret = handle_tlog();

			if (ret != 0)
				return ret;

			break;

		default:
			pr_err("[%s][%d] tlog VERSION is wrong !\n", __func__, __LINE__);
			tlog_pos = ((struct ut_log_buf_head *)tlog_buf)->write_pos;
			ret = -EFAULT;
		}
	}

	return ret;
}

long create_tlog_thread(unsigned long tlog_virt_addr, unsigned long buff_size)
{
	long retVal = 0;
	int ret = 0;

	struct sched_param param = { .sched_priority = 1 };

	if (tlog_virt_addr == NULL)
		return -EINVAL;

	if (buff_size < 0)
		return -EINVAL;

	retVal = init_tlog_buff_head(tlog_virt_addr, buff_size);

	if (retVal != 0) {
		pr_err("[%s][%d] fail to init tlog buff head !\n", __func__, __LINE__);
		return -1;
	}

	tlog_thread = kthread_create(tlog_worker, NULL, "ut_tlog");

	if (IS_ERR(tlog_thread)) {
		pr_err("[%s][%d] fail to create tlog thread !\n", __func__, __LINE__);
		return -1;
	}

	ret = sched_setscheduler(tlog_thread, SCHED_IDLE, &param);

	if (ret == -1) {
		pr_err("[%s][%d] fail to setscheduler tlog thread !\n", __func__, __LINE__);
		return -1;
	}

	wake_up_process(tlog_thread);
	return 0;
}

/**************************************************
		LOG thread for uTgate
 **************************************************/
struct task_struct *utgate_log_thread = NULL;
unsigned long utgate_log_buff = 0;
unsigned long utgate_log_pos = 0;
unsigned char utgate_log_line[256];
unsigned long utgate_log_len = 0;

long init_utgate_log_buff_head(unsigned long log_virt_addr, unsigned long buff_size)
{
	struct utgate_log_head *utgate_log_head = NULL;
	long retVal = 0;

	if (log_virt_addr == NULL)
		return -EINVAL;

	if (buff_size < 0)
		return -EINVAL;

	utgate_log_buff = log_virt_addr;

	memset(log_virt_addr, 0, buff_size);
	utgate_log_head = (struct ut_log_buf_head *)log_virt_addr;

	utgate_log_head->version = UT_TLOG_VERSION;
	utgate_log_head->length = buff_size;
	utgate_log_head->write_pos = 0;

	Flush_Dcache_By_Area((unsigned long)log_virt_addr, (unsigned long)log_virt_addr + buff_size);

	return 0;
}

int utgate_log_print(unsigned long log_start)
{
	if (*((char *)log_start) == '\n') {
		pr_info("[uTgate LOG] %s\n", utgate_log_line);
		utgate_log_len = 0;
		utgate_log_line[0] = 0;
	} else {
		utgate_log_line[utgate_log_len] = *((char *)log_start);
		utgate_log_line[utgate_log_len + 1] = 0;
		utgate_log_len++;
	}

	utgate_log_pos = (utgate_log_pos + 1) % (((struct utgate_log_head *)utgate_log_buff)->length - sizeof(struct utgate_log_head));

	return 0;
}

int handle_utgate_log(void)
{
	unsigned long utgate_log_cont_pos = (unsigned long)utgate_log_buff + sizeof(struct utgate_log_head);
	unsigned long utgate_last_log_pos = utgate_log_cont_pos + ((struct utgate_log_head *)utgate_log_buff)->write_pos;
	unsigned long utgate_start_log_pos = utgate_log_cont_pos + utgate_log_pos;
	int retVal = 0;

	while (utgate_last_log_pos != utgate_start_log_pos) {
		retVal = utgate_log_print(utgate_start_log_pos);

		if (retVal != 0)
			pr_err("[%s][%d]fail to print utgate tlog!\n", __func__, __LINE__);

		utgate_start_log_pos = utgate_log_cont_pos + utgate_log_pos;
	}

	return 0;
}

int utgate_log_worker(void *p)
{
	int ret = 0;

	if (utgate_log_buff == NULL) {
		pr_err("[%s][%d] utgate tlog buff is NULL !\n", __func__, __LINE__);
		return -1;
	}

	while (!kthread_should_stop()) {
		if (((struct utgate_log_head *)utgate_log_buff)->write_pos == utgate_log_pos) {
			schedule_timeout_interruptible(1 * HZ);
			continue;
		}

		switch (((struct utgate_log_head *)utgate_log_buff)->version) {
		case UT_TLOG_VERSION:
			ret = handle_utgate_log();

			if (ret != 0)
				return ret;

			break;

		default:
			pr_err("[%s][%d] utgate tlog VERSION is wrong !\n", __func__, __LINE__);
			utgate_log_pos = ((struct utgate_log_head *)utgate_log_buff)->write_pos;
			ret = -EFAULT;
		}
	}

	return ret;
}

long create_utgate_log_thread(unsigned long log_virt_addr, unsigned long buff_size)
{
	long retVal = 0;
	int ret = 0;

	struct sched_param param = { .sched_priority = 1 };

	if (log_virt_addr == NULL)
		return -EINVAL;

	if (buff_size < 0)
		return -EINVAL;

	retVal = init_utgate_log_buff_head(log_virt_addr, buff_size);

	if (retVal != 0) {
		pr_err("[%s][%d] fail to init uTgate tlog buff head !\n", __func__, __LINE__);
		return -1;
	}

	utgate_log_thread = kthread_create(utgate_log_worker, NULL, "utgate_tlog");

	if (IS_ERR(utgate_log_thread)) {
		pr_err("[%s][%d] fail to create utgate tlog thread !\n", __func__, __LINE__);
		return -1;
	}

	ret = sched_setscheduler(utgate_log_thread, SCHED_IDLE, &param);

	if (ret == -1) {
		pr_err("[%s][%d] fail to setscheduler tlog thread !\n", __func__, __LINE__);
		return -1;
	}

	wake_up_process(utgate_log_thread);
	return 0;
}
