#ifndef MILLET_H
#define MILLET_H

#include <linux/freezer.h>
#include <net/sock.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <uapi/linux/millet.h>

#define RET_OK 0
#define RET_ERR -1
#define WARN_AHEAD_MSGS		3
#define RESERVE_ORDER		17
#define WARN_AHEAD_SPACE	(1 << RESERVE_ORDER)

struct millet_sock;
typedef void (*recv_hook)(void *data, unsigned int len);
typedef int (*send_hook)(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk);
typedef void (*init_hook)(struct millet_sock *sk);

struct millet_sock {
	atomic_t has_init;
	struct sock *sock;

	struct mod_info {
		int id;
		enum MILLET_TYPE monitor;
		atomic_t port;
		recv_hook recv_from;
		send_hook send_to;
		init_hook init;
		spinlock_t lock;
		struct {
			u64 send_suc;
			u64 send_fail;
			u64 runtime;
		} stat;
		char name[NAME_MAXLEN];
		void *priv;
	} mod[MILLET_TYPES_NUM];
};

#ifdef CONFIG_MILLET
int millet_sendmsg(enum MILLET_TYPE type, struct task_struct *t,
		struct millet_data *data);
int millet_sendto_user(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk);
int register_millet_hook(int type, recv_hook recv_from, send_hook send_to,
		init_hook init);
int unregister_millet_hook(int type);
extern int frozen_uid_min;
extern unsigned long binder_warn_ahead_space;
#else
static int frozen_uid_min = 10000;
static unsigned long binder_warn_ahead_space = WARN_AHEAD_SPACE;
static inline int millet_sendmsg(enum MILLET_TYPE type, struct task_struct *t,
		struct millet_data *data) { return 0; }
static inline int millet_sendto_user(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk) { return 0; }
static inline int register_millet_hook(int type, recv_hook recv_from,
		send_hook send_to, init_hook init) { return 0; }
static inline int unregister_millet_hook(int type) { return 0; }
#endif
static inline int frozen_task_group(struct task_struct *task)
{
	return (freezing(task) || frozen(task));
}
#endif
