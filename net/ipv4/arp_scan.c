#include <net/arp_scan.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/skbuff.h>
#include <linux/delay.h>


#define NETLINK_MI_EVENTS 31
#define NETLINK_MI_GROUP 2

static struct sock *nl_sk;
static int user_pid = -1;
extern struct net init_net;
static LIST_HEAD(arp_scan_list_head);
static LIST_HEAD(arp_scan_uid_list_head);
static raw_spinlock_t arp_scan_lock;

void arp_scan_create_neigh(u32 nexthop, u32 uid)
{
	u32 type = NIPQUAD_1(nexthop);
	arp_scan_list_t *tmp = NULL;
	arp_scan_uid_list_t *uid_tmp = NULL;
	bool found = false;
	if ((type == IPV4_ADDR_LOOPBACK) || ((type > IPV4_ADDR_MC_MIN) && (type < IPV4_ADDR_MC_MAX))) {
		pr_info("%s, loopback addr or mc addr, return!", __func__);
		return;
	}

	raw_spin_lock(&arp_scan_lock);
	list_for_each_entry(uid_tmp, &arp_scan_uid_list_head, list) {
		if (uid == uid_tmp->arp_scan_uid) {
			pr_info("%s, already save the app uid, return!", __func__);
			raw_spin_unlock(&arp_scan_lock);
			return;
		}
    }

	list_for_each_entry(tmp, &arp_scan_list_head, list) {
		if (uid == tmp->arp_scan_uid) {
			found = true;
			break;
		}
	}

	if (found) {
		arp_scan_list_update_map(tmp, nexthop);
	} else {
		tmp = arp_scan_list_entry_kmalloc(uid);
		list_add(&tmp->list, &arp_scan_list_head);
	}
	raw_spin_unlock(&arp_scan_lock);
}

void arp_scan_list_update_map(arp_scan_list_t *entry, u32 nexthop)
{
	milink_wlan_message_t *msg = NULL;
	arp_scan_uid_list_t *uid_tmp = NULL;
	//u32 *uid_t = NULL;
	size_t length = sizeof(uint32_t);
	u32 index = NIPQUAD_4(nexthop);

	if (entry->arp_scan_list_map[index] == 0) {
		entry->arp_scan_list_map[index] = 1;
		entry->arp_scan_list_size++;
	}

	if (entry->arp_scan_list_size > ARP_SCAN_THRESHOLD_COUNT) {
		//to do that use netlink to report the arp scan.
		pr_err("%s, app: %u doing arp scan, send to miui qos", __func__, entry->arp_scan_uid);

		uid_tmp = kmalloc(sizeof(arp_scan_uid_list_t), GFP_ATOMIC);
		uid_tmp->arp_scan_uid = entry->arp_scan_uid;
		list_add(&uid_tmp->list, &arp_scan_uid_list_head);

		msg = kmalloc(sizeof(milink_wlan_message_t), GFP_ATOMIC);
		msg->type = EVENT_MILINK_CONN_ARP_SCAN;
		msg->len = sizeof(uint32_t) + sizeof(uint32_t) + length;
		//msg->data = (char *)uid_t;
		memcpy(msg->data, (const char *)&entry->arp_scan_uid, sizeof(uint32_t));

		milink_srv_ucast(msg);

		list_del(&entry->list);
		kfree(entry);
		kfree(msg);
	}
}

arp_scan_list_t *arp_scan_list_entry_kmalloc(u32 uid)
{
	size_t len = sizeof(arp_scan_list_t);
	arp_scan_list_t *plist = kmalloc(len, GFP_ATOMIC);
	if (plist == NULL) {
		 pr_err("kmalloc arp_scan_list_entry failed!!!");
		return NULL;
	}

	memset(plist, 0, len);
	pr_info("%s, kmalloc arp_scan_list_entry for app's uid = %u!!!", __func__, uid);
	plist->arp_scan_uid = uid;

	return plist;
}

/*
 * message = type + len + data
 */
void milink_srv_ucast(milink_wlan_message_t *msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int msg_size = msg->len;
	int res;

	pr_info("entering: %s\n", __FUNCTION__);

	if (!nl_sk) {
		return;
	}

	skb = nlmsg_new(NLMSG_ALIGN(msg_size + 1), GFP_ATOMIC);
	if (!skb) {
		pr_err("Failed to allocate new skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NETLINK_MI_EVENTS, msg_size + 1, 0);
	if (nlh == NULL) {
		pr_err("nlmsg_put failauer \n");
		nlmsg_free(skb);
		return;
	}
	memcpy(nlmsg_data(nlh), (const char *)msg, msg_size);

	pr_info("Sending skb.\n");
	if (user_pid < 0) {
		pr_err("daemon not started, return!\n");
	}
	res = netlink_unicast(nl_sk, skb, user_pid, MSG_DONTWAIT);
	if (res < 0) {
		pr_err("Error while sending skb to user, err id: %d\n", res);
	} else {
		pr_info("Send success.\n");
    }
}

static void nl_data_ready(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	char *umsg = NULL;

	if (skb->len >= nlmsg_total_size(0)) {
		nlh = nlmsg_hdr(skb);
		umsg = NLMSG_DATA(nlh);
		if (umsg) {
			pr_info("nl_data_ready: %s\n", umsg);
			user_pid = nlh->nlmsg_pid;
		}
	}
}

struct netlink_kernel_cfg cfg = {
	.input = nl_data_ready, /* set recv callback */
};

void milink_srv_init(void)
{
	pr_info("entering: %s\n", __FUNCTION__);
	nl_sk = netlink_kernel_create(&init_net, NETLINK_MI_EVENTS, &cfg);
	raw_spin_lock_init(&arp_scan_lock);
	if (!nl_sk) {
		pr_err("Error creating socket.\n");
	}
}

void milink_srv_exit(void)
{
	pr_info("entering: %s\n", __FUNCTION__);
	sock_release(nl_sk->sk_socket);
	netlink_kernel_release(nl_sk);
}

