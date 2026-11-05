#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "boot_monitor.h"
#include "boot_fail.h"

static int pid = 2230;
struct sock *nl_sk;

void bm_sendnlmsg(char *message)
{
	struct sk_buff *skb_1;
	struct nlmsghdr *nlh;
	int len = NLMSG_SPACE(MAX_MSGSIZE);
	int slen = 0;
	int ret = 0;

	if (!nl_sk || !pid) {
		return;
	}

	skb_1 = alloc_skb(len, GFP_KERNEL);

	if (!skb_1) {
		MTN_PRINT_ERR("%s-%d:alloc_skb error\n", __func__, __LINE__);
		return;
	}

	slen = strlen(message);
	nlh = nlmsg_put(skb_1, 0, 0, 0, MAX_MSGSIZE, 0);
	NETLINK_CB(skb_1).portid = 0;
	NETLINK_CB(skb_1).dst_group = 0;
	message[slen] = '\0';

	memcpy(NLMSG_DATA(nlh), message, slen);
	ret = netlink_unicast(nl_sk, skb_1, pid, MSG_DONTWAIT);
	MTN_PRINT_ERR("slen =%d, message = %s\n", slen, message);
	if (!ret) {
		/*kfree_skb(skb_1); */
		MTN_PRINT_ERR("%s-%d:send msg from kernel to usespace failed ret 0x%x\n",
		        __func__, __LINE__, ret);
	}
}

static void nl_data_ready(struct sk_buff *__skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	char str[100] = {'\0'};
	skb = skb_get(__skb);

	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;
		kfree_skb(skb);
	}
	MTN_PRINT_ERR("netlink socket, pid =%d, msg = %d\n", pid, str[0]);
}

int bm_netlink_init(void)
{
	struct netlink_kernel_cfg netlink_cfg;
	memset(&netlink_cfg, 0, sizeof(struct netlink_kernel_cfg));
	netlink_cfg.groups = 0;
	netlink_cfg.flags = 0;
	netlink_cfg.input = nl_data_ready;
	netlink_cfg.cb_mutex = NULL;
	nl_sk = netlink_kernel_create(&init_net, NETLINK_TEST, &netlink_cfg);

	if (!nl_sk) {
		MTN_PRINT_ERR("%s-%d:create netlink socket error\n", __func__, __LINE__);
		return 1;
	}

	return 0;
}

void bm_netlink_exit(void)
{
	if (nl_sk != NULL) {
		netlink_kernel_release(nl_sk);
		nl_sk = NULL;
	}

	MTN_PRINT_ERR("self module exited\n");
}
