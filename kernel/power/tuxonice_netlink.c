/*
 * kernel/power/tuxonice_netlink.c
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Functions for communicating with a userspace helper via netlink.
 */


#include <linux/suspend.h>
#include <linux/sched.h>
#include "tuxonice_netlink.h"
#include "tuxonice.h"
#include "tuxonice_modules.h"
#include "tuxonice_alloc.h"
#include "tuxonice_builtin.h"

static struct user_helper_data *uhd_list;

/*
 * Refill our pool of SKBs for use in emergencies (eg, when eating memory and
 * none can be allocated).
 */
static void toi_fill_skb_pool(struct user_helper_data *uhd)
{
	while (uhd->pool_level < uhd->pool_limit) {
		struct sk_buff *new_skb = alloc_skb(NLMSG_SPACE(uhd->skb_size), TOI_ATOMIC_GFP);

		if (!new_skb)
			break;

		new_skb->next = uhd->emerg_skbs;
		uhd->emerg_skbs = new_skb;
		uhd->pool_level++;
	}
}

/*
 * Try to allocate a single skb. If we can't get one, try to use one from
 * our pool.
 */
static struct sk_buff *toi_get_skb(struct user_helper_data *uhd)
{
	struct sk_buff *skb = alloc_skb(NLMSG_SPACE(uhd->skb_size), TOI_ATOMIC_GFP);

	if (skb)
		return skb;

	skb = uhd->emerg_skbs;
	if (skb) {
		uhd->pool_level--;
		uhd->emerg_skbs = skb->next;
		skb->next = NULL;
	}

	return skb;
}

void toi_send_netlink_message(struct user_helper_data *uhd, int type, void *params, size_t len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	void *dest;
	struct task_struct *t;

	if (uhd->pid == -1)
		return;

	if (uhd->debug)
		printk(KERN_ERR "toi_send_netlink_message: Send " "message type %d.\n", type);

	skb = toi_get_skb(uhd);
	if (!skb) {
		printk(KERN_INFO "toi_netlink: Can't allocate skb!\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, uhd->sock_seq, type, len, 0);
	uhd->sock_seq++;

	dest = NLMSG_DATA(nlh);
	if (params && len > 0)
		memcpy(dest, params, len);

	netlink_unicast(uhd->nl, skb, uhd->pid, 0);

	toi_read_lock_tasklist();
	t = find_task_by_pid_ns(uhd->pid, &init_pid_ns);
	if (!t) {
		toi_read_unlock_tasklist();
		if (uhd->pid > -1)
			printk(KERN_INFO "Hmm. Can't find the userspace task" " %d.\n", uhd->pid);
		return;
	}
	wake_up_process(t);
	toi_read_unlock_tasklist();

	yield();
}
EXPORT_SYMBOL_GPL(toi_send_netlink_message);

static void send_whether_debugging(struct user_helper_data *uhd)
{
	static u8 is_debugging = 1;

	toi_send_netlink_message(uhd, NETLINK_MSG_IS_DEBUGGING, &is_debugging, sizeof(u8));
}

/*
 * Set the PF_NOFREEZE flag on the given process to ensure it can run whilst we
 * are hibernating.
 */
static int nl_set_nofreeze(struct user_helper_data *uhd, __u32 pid)
{
	struct task_struct *t;

	if (uhd->debug)
		printk(KERN_ERR "nl_set_nofreeze for pid %d.\n", pid);

	toi_read_lock_tasklist();
	t = find_task_by_pid_ns(pid, &init_pid_ns);
	if (!t) {
		toi_read_unlock_tasklist();
		printk(KERN_INFO "Strange. Can't find the userspace task %d.\n", pid);
		return -EINVAL;
	}

	t->flags |= PF_NOFREEZE;

	toi_read_unlock_tasklist();
	uhd->pid = pid;

	toi_send_netlink_message(uhd, NETLINK_MSG_NOFREEZE_ACK, NULL, 0);

	return 0;
}

/*
 * Called when the userspace process has informed us that it's ready to roll.
 */
static int nl_ready(struct user_helper_data *uhd, u32 version)
{
	if (version != uhd->interface_version) {
		printk(KERN_INFO "%s userspace process using invalid interface"
		       " version (%d - kernel wants %d). Trying to "
		       "continue without it.\n", uhd->name, version, uhd->interface_version);
		if (uhd->not_ready)
			uhd->not_ready();
		return -EINVAL;
	}

	complete(&uhd->wait_for_process);

	return 0;
}

void toi_netlink_close_complete(struct user_helper_data *uhd)
{
	if (uhd->nl) {
		netlink_kernel_release(uhd->nl);
		uhd->nl = NULL;
	}

	while (uhd->emerg_skbs) {
		struct sk_buff *next = uhd->emerg_skbs->next;
		kfree_skb(uhd->emerg_skbs);
		uhd->emerg_skbs = next;
	}

	uhd->pid = -1;
}
EXPORT_SYMBOL_GPL(toi_netlink_close_complete);

static int toi_nl_gen_rcv_msg(struct user_helper_data *uhd,
			      struct sk_buff *skb, struct nlmsghdr *nlh)
{
	int type = nlh->nlmsg_type;
	int *data;
	int err;

	if (uhd->debug)
		printk(KERN_ERR "toi_user_rcv_skb: Received message %d.\n", type);

	/* Let the more specific handler go first. It returns
	 * 1 for valid messages that it doesn't know. */
	err = uhd->rcv_msg(skb, nlh);
	if (err != 1)
		return err;

	/* Only allow one task to receive NOFREEZE privileges */
	if (type == NETLINK_MSG_NOFREEZE_ME && uhd->pid != -1) {
		printk(KERN_INFO "Received extra nofreeze me requests.\n");
		return -EBUSY;
	}

	data = NLMSG_DATA(nlh);

	switch (type) {
	case NETLINK_MSG_NOFREEZE_ME:
		return nl_set_nofreeze(uhd, nlh->nlmsg_pid);
	case NETLINK_MSG_GET_DEBUGGING:
		send_whether_debugging(uhd);
		return 0;
	case NETLINK_MSG_READY:
		if (nlh->nlmsg_len != NLMSG_LENGTH(sizeof(u32))) {
			printk(KERN_INFO "Invalid ready mesage.\n");
			if (uhd->not_ready)
				uhd->not_ready();
			return -EINVAL;
		}
		return nl_ready(uhd, (u32) *data);
	case NETLINK_MSG_CLEANUP:
		toi_netlink_close_complete(uhd);
		return 0;
	}

	return -EINVAL;
}

static void toi_user_rcv_skb(struct sk_buff *skb)
{
	int err;
	struct nlmsghdr *nlh;
	struct user_helper_data *uhd = uhd_list;

	while (uhd && uhd->netlink_id != skb->sk->sk_protocol)
		uhd = uhd->next;

	if (!uhd)
		return;

	while (skb->len >= NLMSG_SPACE(0)) {
		u32 rlen;

		nlh = (struct nlmsghdr *)skb->data;
		if (nlh->nlmsg_len < sizeof(*nlh) || skb->len < nlh->nlmsg_len)
			return;

		rlen = NLMSG_ALIGN(nlh->nlmsg_len);
		if (rlen > skb->len)
			rlen = skb->len;

		err = toi_nl_gen_rcv_msg(uhd, skb, nlh);
		if (err)
			netlink_ack(skb, nlh, err);
		else if (nlh->nlmsg_flags & NLM_F_ACK)
			netlink_ack(skb, nlh, 0);
		skb_pull(skb, rlen);
	}
}

static int netlink_prepare(struct user_helper_data *uhd)
{
	struct netlink_kernel_cfg cfg = {
		.groups = 0,
		.input = toi_user_rcv_skb,
	};

	uhd->next = uhd_list;
	uhd_list = uhd;

	uhd->sock_seq = 0x42c0ffee;
	uhd->nl = netlink_kernel_create(&init_net, uhd->netlink_id, &cfg);
	if (!uhd->nl) {
		printk(KERN_INFO "Failed to allocate netlink socket for %s.\n", uhd->name);
		return -ENOMEM;
	}

	toi_fill_skb_pool(uhd);

	return 0;
}

void toi_netlink_close(struct user_helper_data *uhd)
{
	struct task_struct *t;

	toi_read_lock_tasklist();
	t = find_task_by_pid_ns(uhd->pid, &init_pid_ns);
	if (t)
		t->flags &= ~PF_NOFREEZE;
	toi_read_unlock_tasklist();

	toi_send_netlink_message(uhd, NETLINK_MSG_CLEANUP, NULL, 0);
}
EXPORT_SYMBOL_GPL(toi_netlink_close);

int toi_netlink_setup(struct user_helper_data *uhd)
{
	/* In case userui didn't cleanup properly on us */
	toi_netlink_close_complete(uhd);

	if (netlink_prepare(uhd) < 0) {
		printk(KERN_INFO "Netlink prepare failed.\n");
		return 1;
	}

	if (toi_launch_userspace_program(uhd->program, uhd->netlink_id,
					 UMH_WAIT_EXEC, uhd->debug) < 0) {
		printk(KERN_INFO "Launch userspace program failed.\n");
		toi_netlink_close_complete(uhd);
		return 1;
	}

	/* Wait 2 seconds for the userspace process to make contact */
	wait_for_completion_timeout(&uhd->wait_for_process, 2 * HZ);

	if (uhd->pid == -1) {
		printk(KERN_INFO "%s: Failed to contact userspace process.\n", uhd->name);
		toi_netlink_close_complete(uhd);
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(toi_netlink_setup);
