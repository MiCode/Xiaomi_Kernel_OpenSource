#ifndef _CHRDEV_MIEV_H_
#define _CHRDEV_MIEV_H_
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/wait.h>

/* ioctl magic */
#define MIEV_IOC_MAGIC 'k'
/* ioctl command */
#define MIEV_IOC_NONE _IO(MIEV_IOC_MAGIC, 1)
#define MIEV_IOC_READ _IOR(MIEV_IOC_MAGIC, 2, int)
#define MIEV_IOC_WRITE _IOW(MIEV_IOC_MAGIC, 3, int)
/* maximum number of commands */
#define MIEV_IOC_MAXNR 3

/* device name */
#define DEV_NAME "miev"
/* major and minjor*/
#define DEV_MAJOR 520
#define DEV_MINOR 0

enum MIEV_IOCTL_CMD { IOCTL_READ_LOG_EVENT = 0, IOCTL_WRITE_LOG_EVENT };

struct miev_device {
	dev_t dev_no;
        struct class *my_class;
	struct cdev chrdev;
	struct kfifo fifo;
	struct mutex lock;
	wait_queue_head_t wait_queue;
};

struct miev_device *miev_dev;

int write_kbuf(char __kernel *kbuf, int size);

#endif //_CHRDEV_MIEV_H_