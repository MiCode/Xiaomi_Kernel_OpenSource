
#define pr_fmt(fmt) "migt-gtc: " fmt
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <linux/kthread.h>
#include <linux/sort.h>
#include <linux/cred.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>






MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("vip-task detected by David");

