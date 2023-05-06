/*
 * netlink interface
 *
 * Copyright (c) 2017 Goodix
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/types.h>
#include <net/sock.h>
#include <net/netlink.h>

#define NETLINK_TEST 25
#define MAX_MSGSIZE 32
int stringlength(char *s);
void sendnlmsg(char *message);
static int pid = -1;
struct sock *nl_sk;

void sendnlmsg(char *message)
{
	struct sk_buff *skb_1;
	struct nlmsghdr *nlh;
	int len = NLMSG_SPACE(MAX_MSGSIZE);
	int slen = 0;
	int ret = 0;

	if (!message || !nl_sk || !pid) {
		return;
	}

	skb_1 = alloc_skb(len, GFP_KERNEL);

	if (!skb_1) {
		pr_err("alloc_skb error\n");
		return;
	}

	slen = strlen(message);
	nlh = nlmsg_put(skb_1, 0, 0, 0, MAX_MSGSIZE, 0);
	NETLINK_CB(skb_1).portid = 0;
	NETLINK_CB(skb_1).dst_group = 0;
	message[slen] = '\0';
	memcpy(NLMSG_DATA(nlh), message, slen + 1);
	ret = netlink_unicast(nl_sk, skb_1, pid, MSG_DONTWAIT);

	if (!ret) {
		/*kfree_skb(skb_1); */
		pr_err("send msg from kernel to usespace failed ret 0x%x\n",
		       ret);
	}
}

void nl_data_ready(struct sk_buff *__skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	char str[100];
	skb = skb_get(__skb);

	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;
		kfree_skb(skb);
	}
}

int netlink_init(void)
{
	struct netlink_kernel_cfg netlink_cfg;
	memset(&netlink_cfg, 0, sizeof(struct netlink_kernel_cfg));
	netlink_cfg.groups = 0;
	netlink_cfg.flags = 0;
	netlink_cfg.input = nl_data_ready;
	netlink_cfg.cb_mutex = NULL;
	nl_sk = netlink_kernel_create(&init_net, NETLINK_TEST, &netlink_cfg);

	if (!nl_sk) {
		pr_err("create netlink socket error\n");
		return 1;
	}

	return 0;
}

void netlink_exit(void)
{
	if (nl_sk != NULL) {
		netlink_kernel_release(nl_sk);
		nl_sk = NULL;
	}

	pr_info("self module exited\n");
}
