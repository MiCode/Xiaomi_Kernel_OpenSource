#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#ifndef _MI_EXCEPTION_LOG_H_
#define _MI_EXCEPTION_LOG_H_

#define MAXSIZE 100
#define DATASIZE 1024
#define MIN( x, y ) ( ((x) < (y)) ? (x) : (y) )

struct exception_log_dev {
	dev_t devt;
	char ais_data[MAXSIZE][DATASIZE];
	int data_front;
	int data_rear;
	wait_queue_head_t exception_log_is_not_empty;
};

#endif /* _MI_EXCEPTION_LOG_H_ */
