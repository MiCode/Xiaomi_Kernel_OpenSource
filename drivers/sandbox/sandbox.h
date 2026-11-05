#ifndef SANDBOX_H
#define SANDBOX_H
/* Warning!!!
 * This header file shall be shared by Linux kernel and corresponding
 * syscall hacking module, and further user-mode programs would include this header file,
 * so please keep this file unchanged!
 *
 * This header file built here is to compile sandbox module.
 */

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#define SANDBOX_DEBUG 0
#define SANDBOX_AUDIT	324    /* sandbox init system call.*/

#define SANDBOX_MAX_SYSCALL	400
#define SANDBOX_MAX_PAGE	10  /* max size of each thread's policy. */

#define SANDBOX_AUDIT_LIMIT	64  /* max limit audited messages of each policy. */
#define SANDBOX_MAX_RT_THREADS	1024
//#define SANDBOX_MEMCACHE

#define NIPQUAD(addr)        \
        ((unsigned char *)&addr)[0],     \
        ((unsigned char *)&addr)[1],     \
        ((unsigned char *)&addr)[2],     \
        ((unsigned char *)&addr)[3]

#define NIPQUAD_FMT        "%u.%u.%u.%u"

struct sandbox_arg {
    struct list_head policy_list_head[SANDBOX_MAX_SYSCALL];
    rwlock_t policy_lock;
    atomic_t policy_refcnt;
    int audit_flag;
    char *evt_hdr;
};

struct sandbox_whitelist {
	char name[TASK_COMM_LEN];
	int maxAllowedNum;
	int num;
};

int sandbox_whitelist_audit(struct task_struct *p);
int sandbox_rt_priority_audit(struct task_struct *p);
void rtload_miscdev_exit(void);
void rtload_miscdev_init(void);
#endif
