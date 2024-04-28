#include "fp_driver.h"
/* -------------------------------------------------------------------- */
/* netlink functions                                                    */
/* -------------------------------------------------------------------- */

/* for netlink use */
#define MAX_NL_MSG_LEN 16
int pid = -1;
static struct sock *nl_sk;
void fp_netlink_send(struct fp_device *fp_dev, const int command)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int ret;

	pr_debug( "enter, send command %d\n", command);
	if (NULL == nl_sk) {
		pr_debug( "invalid socket\n");
		return;
	}

	if (0 == pid) {
		pr_debug( "invalid native process pid\n");
		return;
	}

	/*alloc data buffer for sending to native */
	/*malloc data space at least 1500 bytes, which is ethernet data length */
	skb = alloc_skb(MAX_NL_MSG_LEN, GFP_ATOMIC);
	if (skb == NULL) {
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, MAX_NL_MSG_LEN, 0);
	if (!nlh) {
		pr_debug("nlmsg_put failed\n");
		kfree_skb(skb);
		return;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	*(char *)NLMSG_DATA(nlh) = command;
	ret = netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret == 0) {
		pr_debug("send failed\n");
		return;
	}
}

void fp_netlink_recv(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	char str[128];
	FUNC_ENTRY();
	skb = skb_get(__skb);
	if (skb == NULL) {
		pr_debug("skb_get return NULL\n");
		return;
	}

	/* presume there is 5byte payload at leaset */
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;
		pr_debug( "pid: %d, msg: %s\n", pid,str);
	} else {
		pr_debug("[not enough data length\n");
	}
	kfree_skb(skb);
}

int fp_netlink_init(struct fp_device *fp_dev)
{
	struct netlink_kernel_cfg cfg;
	memset(&cfg, 0, sizeof(struct netlink_kernel_cfg));
	cfg.input = fp_netlink_recv;
	pr_debug("netlink num is %d",fp_dev->fp_netlink_num);
	nl_sk = netlink_kernel_create(
			&init_net, fp_dev->fp_netlink_num, &cfg);
	if (nl_sk == NULL) {
		pr_debug("netlink create failed\n");
		return -1;
	}
	pr_debug( "netlink create success\n");
	return 0;
}

int fp_netlink_destroy(struct fp_device *fp_dev)
{
	FUNC_ENTRY();
	if (nl_sk != NULL) {
		netlink_kernel_release(nl_sk);
		nl_sk = NULL;
		return 0;
	}
	return -1;
}
