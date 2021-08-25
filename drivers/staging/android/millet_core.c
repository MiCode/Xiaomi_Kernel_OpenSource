/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * File name: millet.c
 * Description: smart frozen control
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2019/11/27
 */

#define pr_fmt(fmt) "millet: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/millet.h>
#include <linux/freezer.h>
#include <net/sock.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>

int frozen_uid_min = 10000;
unsigned long binder_warn_ahead_space = WARN_AHEAD_SPACE;
static struct millet_sock millet_sk;
struct proc_dir_entry *millet_rootdir;
static unsigned int millet_debug;
module_param(millet_debug, uint, 0644);
module_param(frozen_uid_min, uint, 0644);
module_param(binder_warn_ahead_space, ulong, 0644);
enum MILLET_VERSION millet_v= VERSION_1_0;

static void dump_send_msg(struct millet_data *msg)
{
	if (!millet_debug)
		return;

	pr_info("-----up-report msg-head dump-----\n");
	if (!msg) {
		pr_err("msg is NULL");
		return;
	}

	pr_info("msg: %d\n", msg->msg_type);
	pr_info("type: %d\n", msg->owner);
	pr_info("src_port: 0x%x\n", msg->src_port);
	pr_info("dest_port: 0x%x\n", msg->dst_port);
	pr_info("uid: %d\n", msg->uid);
}

static void dump_recv_msg(struct millet_userconf *msg)
{
	if (!millet_debug)
		return;

	pr_info("-----recv msg from user-----\n");
	if (!msg) {
		pr_err("msg is NULL");
		return;
	}

	pr_info("msg: %d\n", msg->msg_type);
	pr_info("type: %d\n", msg->owner);
	pr_info("src_port: 0x%x\n", msg->src_port);
	pr_info("dest_port: 0x%x\n", msg->dst_port);
}


int millet_can_attach(struct cgroup_taskset *tset)
{
	const struct cred *cred = current_cred(), *tcred;
	struct task_struct *task;
	struct cgroup_subsys_state *css;

	cgroup_taskset_for_each(task, css, tset)
	{
		tcred = __task_cred(task);

		if ((current != task) &&
		    !(cred->euid.val == 1000
			    || capable(CAP_SYS_ADMIN))) {
			pr_err("Permission problem\n");
			return 1; // >0 means can't attach
		}
	}

	return 0;
}

int millet_sendto_user(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk)
{
	int ret, msg_len = 0;
	int monitor, monitor_port = 0;
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	struct millet_data *payload = NULL;
	struct timespec64 ts;

	if (!atomic_read(&sk->has_init))
		return RET_ERR;

	if (!data || !MSG_VALID(data->msg_type)) {
		pr_err("%s:msg or  msg type is invalid! %d\n",
		       __func__, data->msg_type);
		return RET_ERR;
	}

	msg_len = sizeof(struct millet_data);
	skb = nlmsg_new(msg_len, GFP_ATOMIC);
	if (!skb) {
		pr_err("%s alloc_skb failed! %d\n",
		       __func__, data->owner);
		return RET_ERR;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, msg_len, 0);
	if (!nlh) {
		kfree_skb(skb);
		return RET_ERR;
	}

	payload = nlmsg_data(nlh);
	memset(payload, 0, msg_len);
	memcpy(payload, data, msg_len);
	payload->src_port = MILLET_KERNEL_ID;
	payload->dst_port = MILLET_USER_ID;
	monitor = sk->mod[payload->owner].monitor;
	if (!TYPE_VALID(monitor))
		monitor = payload->owner;

	payload->monitor = monitor;
	ktime_get_ts64(&ts);
	payload->tm.sec  = ts.tv_sec;
	payload->tm.nsec = ts.tv_nsec;
	monitor_port = atomic_read(&sk->mod[monitor].port);
	if (millet_debug)
		dump_send_msg(payload);

	ret = nlmsg_unicast(sk->sock, skb, monitor_port);
	if (ret < 0) {
		pr_err("nlmsg_unicast failed! %s errno %d\n",
				__func__, ret);
		return RET_ERR;
	} else {
		if (millet_debug)
			pr_info("nlmsg_unicast snd msg success\n");
	}

	return RET_OK;
}

int millet_sendmsg(enum MILLET_TYPE type, struct task_struct *tsk,
		struct millet_data *data)
{
	u64 walltime, timecost;
	unsigned long flags;
	int ret = RET_OK;

	if (!TYPE_VALID(type)) {
		pr_err("wrong type valid %d\n", type);
		return RET_ERR;
	}

	if (!millet_sk.mod[type].send_to) {
		pr_err("mod %d send_to interface is NULL");
		return RET_ERR;
	}

	walltime = ktime_to_us(ktime_get());
	ret = millet_sk.mod[type].send_to(tsk, data, &millet_sk);
	spin_lock_irqsave(&millet_sk.mod[type].lock, flags);

	if (ret < 0) {
		millet_sk.mod[type].stat.send_fail++;
	} else if (ret > 0)
		millet_sk.mod[type].stat.send_suc++;

	timecost = ktime_to_us(ktime_get()) - walltime;
	millet_sk.mod[type].stat.runtime += timecost;
	spin_unlock_irqrestore(&millet_sk.mod[type].lock, flags);

	return ret;
}

static void recv_handler(struct sk_buff *skb)
{
	struct millet_userconf *payload = NULL;
	struct nlmsghdr *nlh = NULL;
	unsigned int msglen = sizeof(struct millet_userconf);
	uid_t uid = 0;
	int from = -1;

	if (!skb) {
		pr_err("recv_handler %s: skb is	NULL!\n", __func__);
		return;
	}

	uid = (*NETLINK_CREDS(skb)).uid.val;
	if (uid > 1000) {
		pr_err("uid: %d, permission denied\n", uid);
		return;
	}

	if (millet_debug)
		pr_info("kernel millet receive msg now\n");

	if (unlikely(skb->len < NLMSG_SPACE(0))) {
		pr_err("msg len is invalid\n");
		return;
	}

	nlh = nlmsg_hdr(skb);
	if (nlh->nlmsg_len < NLMSG_SPACE(msglen)) {
		pr_err("msg len err %d need %d\n",
		       nlh->nlmsg_len, NLMSG_SPACE(msglen));
		return;
	}

	from = nlh->nlmsg_pid;
	payload = (struct millet_userconf *) NLMSG_DATA(nlh);
	if (payload->src_port != MILLET_USER_ID) {
		pr_err("src_port %x is not valid!\n",
		       payload->src_port);
		return;
	}

	if (payload->dst_port != MILLET_KERNEL_ID) {
		pr_err("dst_port is %x not kernel!\n",
		       payload->dst_port);
		return;
	}

	if (!TYPE_VALID(payload->owner)) {
		pr_err("mod %d is not valid!\n",
		       payload->owner);
		return;
	}

	switch (payload->msg_type) {
	case LOOPBACK_MSG: {
		struct millet_data data;
		data.msg_type = LOOPBACK_MSG;
		data.owner = payload->owner;
		atomic_set(&millet_sk.mod[payload->owner].port, from);
		dump_recv_msg(payload);
		millet_sendto_user(current, &data, &millet_sk);
		break;
	}

	case MSG_TO_KERN: {
		if (millet_sk.mod[payload->owner].recv_from)
			millet_sk.mod[payload->owner].recv_from(
				payload, sizeof(struct millet_userconf));

		if (millet_debug) {
			pr_err("recv mesg form %d\n", from);
			dump_recv_msg(payload);
		}
		break;
	}

	default:
		pr_err("msg type is valid %d\n", payload->msg_type);
		break;
	}
}

static int millet_sock_show(struct seq_file *m, void *v)
{
	int i;
	unsigned long flags;
	u64 send_suc[MILLET_TYPES_NUM];
	u64 send_fail[MILLET_TYPES_NUM];
	u64 runtime[MILLET_TYPES_NUM];
	u64 total_send_suc, total_send_fail, total_runtime;

	memset(send_suc, 0, sizeof(u64) * MILLET_TYPES_NUM);
	memset(send_fail, 0, sizeof(u64) * MILLET_TYPES_NUM);
	memset(runtime, 0, sizeof(u64) * MILLET_TYPES_NUM);

	for (i = O_TYPE + 1; i < MILLET_TYPES_NUM; i++) {
		spin_lock_irqsave(&millet_sk.mod[i].lock, flags);
		send_suc[i] = millet_sk.mod[i].stat.send_suc;
		send_fail[i] = millet_sk.mod[i].stat.send_fail;
		runtime[i] = millet_sk.mod[i].stat.runtime;
		spin_unlock_irqrestore(&millet_sk.mod[i].lock, flags);
	}

	total_send_suc = total_send_fail = total_runtime = 0;
	seq_printf(m, "-----------------------------\n\n");
	for (i = O_TYPE + 1; i < MILLET_TYPES_NUM; i++) {
		if (!send_suc[i] && !send_fail[i])
			continue;

		total_send_suc += send_suc[i];
		total_send_fail += send_fail[i];
		total_runtime += runtime[i];

		seq_printf(m, "name: %s mod id %d:\n",
		           millet_sk.mod[i].name, i);
		seq_printf(m, "send suc: %llu\n", send_suc[i]);
		seq_printf(m, "send fail: %llu\n", send_fail[i]);
		seq_printf(m, "runtime: %llu us\n\n", runtime[i]);

	}

	seq_printf(m, "---------total info--------------------\n");
	seq_printf(m, "send_suc: %llu\n", total_send_suc);
	seq_printf(m, "send_fail: %llu\n", total_send_fail);
	seq_printf(m, "runtime: %llu us\n", total_runtime);
	return 0;
}

static int millet_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, millet_sock_show, NULL);
}

static void stat_reset(void)
{
	int i;
	unsigned long flags;

	for (i = O_TYPE + 1; i < MILLET_TYPES_NUM; i++) {
		spin_lock_irqsave(&millet_sk.mod[i].lock, flags);
		millet_sk.mod[i].stat.send_suc = 0;
		millet_sk.mod[i].stat.send_fail = 0;
		millet_sk.mod[i].stat.runtime = 0;
		spin_unlock_irqrestore(&millet_sk.mod[i].lock, flags);
	}
}

static ssize_t millet_stat_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	unsigned char tmp = 0;
	int value = 0;

	get_user(tmp, buf);
	value = simple_strtol(&tmp, NULL, 10);
	pr_info("input value number: %d\n", value);
	if (value == 1)
		stat_reset();

	printk(KERN_WARNING "stat reset now\n");
	return count;
}

static const struct file_operations millet_proc_fops = {
	.open   = millet_stat_open,
	.read   = seq_read,
	.write   = millet_stat_write,
	.llseek   = seq_lseek,
	.release   = single_release,
	.owner   = THIS_MODULE,
};

static int millet_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", millet_v);
	return 0;
}

static int millet_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, millet_version_show, NULL);
}

static const struct file_operations millet_version_fops = {
	.open   = millet_version_open,
	.read   = seq_read,
	.llseek   = seq_lseek,
	.release   = single_release,
	.owner   = THIS_MODULE,
};

int register_millet_hook(int type, recv_hook recv_from,
		send_hook send_to, init_hook init)
{
	if (!TYPE_VALID(type)) {
		pr_err("%s: type is invalid! %d\n",
				__func__, type);
		return RET_ERR;
	}

	if (millet_sk.mod[type].init) {
		pr_err("%s: init has been registered %d\n",
		       __func__, type);
		return RET_ERR;
	} else
		millet_sk.mod[type].init = init;

	if (millet_sk.mod[type].recv_from) {
		pr_err("%s: recv_from has been registered %d\n",
		       __func__, type);
		return RET_ERR;
	} else
		millet_sk.mod[type].recv_from = recv_from;

	if (millet_sk.mod[type].send_to) {
		pr_err("%s: send_to has been registered %d\n",
		       __func__, type);
		return RET_ERR;
	} else
		millet_sk.mod[type].send_to = send_to;

	if (millet_debug) {
		printk("type %d register hook\n", type);
		dump_stack();
	}

	return RET_OK;
}

int unregister_millet_hook(int type)
{
	if (!TYPE_VALID(type)) {
		pr_err("%s: type is invalid! %d\n",
		       __func__, type);
		return RET_ERR;
	}

	millet_sk.mod[type].recv_from = NULL;
	millet_sk.mod[type].send_to = NULL;
	millet_sk.mod[type].init = NULL;
	return RET_OK;
}

static int __init millet_init(void)
{
	int ret = RET_ERR;
	struct proc_dir_entry *millet_stat_entry = NULL;
	struct proc_dir_entry *millet_version_entry = NULL;
	int i;

	struct netlink_kernel_cfg cfg = {
		.input = recv_handler,
	};

	millet_sk.sock =
		netlink_kernel_create(&init_net, NETLINK_MILLET, &cfg);
	if (!millet_sk.sock) {
		pr_err("%s: create socket error!\n", __func__);
		return ret;
	}

	for (i = O_TYPE + 1; i < MILLET_TYPES_NUM; i++) {
		atomic_set(&millet_sk.mod[i].port, 0);
		spin_lock_init(&millet_sk.mod[i].lock);
		if (millet_sk.mod[i].init)
			millet_sk.mod[i].init(&millet_sk);

		strlcpy(millet_sk.mod[i].name, NAME_ARRAY[i], NAME_MAXLEN);
	}

	millet_rootdir = proc_mkdir("millet", NULL);
	if (!millet_rootdir)
		pr_err("create /proc/millet failed\n");
	else {
		millet_stat_entry = proc_create("millet_stat",
			0644, millet_rootdir, &millet_proc_fops);
		if (!millet_stat_entry)
			pr_err("create millet stat failed\n");

		millet_version_entry = proc_create("version",
			0644, millet_rootdir, &millet_version_fops);
		if (!millet_version_entry)
			pr_err("create millet version failed\n");
	}

	atomic_set(&millet_sk.has_init, 1);
	return RET_OK;
}

late_initcall(millet_init);

MODULE_LICENSE("GPL");
