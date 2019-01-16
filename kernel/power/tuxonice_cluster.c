/*
 * kernel/power/tuxonice_cluster.c
 *
 * Copyright (C) 2006-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * This file contains routines for cluster hibernation support.
 *
 * Based on ip autoconfiguration code in net/ipv4/ipconfig.c.
 *
 * How does it work?
 *
 * There is no 'master' node that tells everyone else what to do. All nodes
 * send messages to the broadcast address/port, maintain a list of peers
 * and figure out when to progress to the next step in hibernating or resuming.
 * This makes us more fault tolerant when it comes to nodes coming and going
 * (which may be more of an issue if we're hibernating when power supplies
 * are being unreliable).
 *
 * At boot time, we start a ktuxonice thread that handles communication with
 * other nodes. This node maintains a state machine that controls our progress
 * through hibernating and resuming, keeping us in step with other nodes. Nodes
 * are identified by their hw address.
 *
 * On startup, the node sends CLUSTER_PING on the configured interface's
 * broadcast address, port $toi_cluster_port (see below) and begins to listen
 * for other broadcast messages. CLUSTER_PING messages are repeated at
 * intervals of 5 minutes, with a random offset to spread traffic out.
 *
 * A hibernation cycle is initiated from any node via
 *
 * echo > /sys/power/tuxonice/do_hibernate
 *
 * and (possibily) the hibernate script. At each step of the process, the node
 * completes its work, and waits for all other nodes to signal completion of
 * their work (or timeout) before progressing to the next step.
 *
 * Request/state  Action before reply	Possible reply	Next state
 * HIBERNATE	  capable, pre-script	HIBERNATE|ACK	NODE_PREP
 *					HIBERNATE|NACK	INIT_0
 *
 * PREP		  prepare_image		PREP|ACK	IMAGE_WRITE
 *					PREP|NACK	INIT_0
 *					ABORT		RUNNING
 *
 * IO		  write image		IO|ACK		power off
 *					ABORT		POST_RESUME
 *
 * (Boot time)	  check for image	IMAGE|ACK	RESUME_PREP
 *					(Note 1)
 *					IMAGE|NACK	(Note 2)
 *
 * PREP		  prepare read image	PREP|ACK	IMAGE_READ
 *					PREP|NACK	(As NACK_IMAGE)
 *
 * IO		  read image		IO|ACK		POST_RESUME
 *
 * POST_RESUME	  thaw, post-script			RUNNING
 *
 * INIT_0	  init 0
 *
 * Other messages:
 *
 * - PING: Request for all other live nodes to send a PONG. Used at startup to
 *   announce presence, when a node is suspected dead and periodically, in case
 *   segments of the network are [un]plugged.
 *
 * - PONG: Response to a PING.
 *
 * - ABORT: Request to cancel writing an image.
 *
 * - BYE: Notification that this node is shutting down.
 *
 * Note 1: Repeated at 3s intervals until we continue to boot/resume, so that
 * nodes which are slower to start up can get state synchronised. If a node
 * starting up sees other nodes sending RESUME_PREP or IMAGE_READ, it may send
 * ACK_IMAGE and they will wait for it to catch up. If it sees ACK_READ, it
 * must invalidate its image (if any) and boot normally.
 *
 * Note 2: May occur when one node lost power or powered off while others
 * hibernated. This node waits for others to complete resuming (ACK_READ)
 * before completing its boot, so that it appears as a fail node restarting.
 *
 * If any node has an image, then it also has a list of nodes that hibernated
 * in synchronisation with it. The node will wait for other nodes to appear
 * or timeout before beginning its restoration.
 *
 * If a node has no image, it needs to wait, in case other nodes which do have
 * an image are going to resume, but are taking longer to announce their
 * presence. For this reason, the user can specify a timeout value and a number
 * of nodes detected before we just continue. (We might want to assume in a
 * cluster of, say, 15 nodes, if 8 others have booted without finding an image,
 * the remaining nodes will too. This might help in situations where some nodes
 * are much slower to boot, or more subject to hardware failures or such like).
 */

#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/if.h>
#include <linux/rtnetlink.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/if_arp.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/netdevice.h>
#include <net/ip.h>

#include "tuxonice.h"
#include "tuxonice_modules.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_alloc.h"
#include "tuxonice_io.h"

#if 1
#define PRINTK(a, b...) do { printk(a, ##b); } while (0)
#else
#define PRINTK(a, b...) do { } while (0)
#endif

static int loopback_mode;
static int num_local_nodes = 1;
#define MAX_LOCAL_NODES 8
#define SADDR (loopback_mode ? b->sid : h->saddr)

#define MYNAME "TuxOnIce Clustering"

enum cluster_message {
	MSG_ACK = 1,
	MSG_NACK = 2,
	MSG_PING = 4,
	MSG_ABORT = 8,
	MSG_BYE = 16,
	MSG_HIBERNATE = 32,
	MSG_IMAGE = 64,
	MSG_IO = 128,
	MSG_RUNNING = 256
};

static char *str_message(int message)
{
	switch (message) {
	case 4:
		return "Ping";
	case 8:
		return "Abort";
	case 9:
		return "Abort acked";
	case 10:
		return "Abort nacked";
	case 16:
		return "Bye";
	case 17:
		return "Bye acked";
	case 18:
		return "Bye nacked";
	case 32:
		return "Hibernate request";
	case 33:
		return "Hibernate ack";
	case 34:
		return "Hibernate nack";
	case 64:
		return "Image exists?";
	case 65:
		return "Image does exist";
	case 66:
		return "No image here";
	case 128:
		return "I/O";
	case 129:
		return "I/O okay";
	case 130:
		return "I/O failed";
	case 256:
		return "Running";
	default:
		printk(KERN_ERR "Unrecognised message %d.\n", message);
		return "Unrecognised message (see dmesg)";
	}
}

#define MSG_ACK_MASK (MSG_ACK | MSG_NACK)
#define MSG_STATE_MASK (~MSG_ACK_MASK)

struct node_info {
	struct list_head member_list;
	wait_queue_head_t member_events;
	spinlock_t member_list_lock;
	spinlock_t receive_lock;
	int peer_count, ignored_peer_count;
	struct toi_sysfs_data sysfs_data;
	enum cluster_message current_message;
};

struct node_info node_array[MAX_LOCAL_NODES];

struct cluster_member {
	__be32 addr;
	enum cluster_message message;
	struct list_head list;
	int ignore;
};

#define toi_cluster_port_send 3501
#define toi_cluster_port_recv 3502

static struct net_device *net_dev;
static struct toi_module_ops toi_cluster_ops;

static int toi_recv(struct sk_buff *skb, struct net_device *dev,
		    struct packet_type *pt, struct net_device *orig_dev);

static struct packet_type toi_cluster_packet_type = {
	.type = __constant_htons(ETH_P_IP),
	.func = toi_recv,
};

struct toi_pkt {		/* BOOTP packet format */
	struct iphdr iph;	/* IP header */
	struct udphdr udph;	/* UDP header */
	u8 htype;		/* HW address type */
	u8 hlen;		/* HW address length */
	__be32 xid;		/* Transaction ID */
	__be16 secs;		/* Seconds since we started */
	__be16 flags;		/* Just what it says */
	u8 hw_addr[16];		/* Sender's HW address */
	u16 message;		/* Message */
	unsigned long sid;	/* Source ID for loopback testing */
};

static char toi_cluster_iface[IFNAMSIZ] = CONFIG_TOI_DEFAULT_CLUSTER_INTERFACE;

static int added_pack;

static int others_have_image;

/* Key used to allow multiple clusters on the same lan */
static char toi_cluster_key[32] = CONFIG_TOI_DEFAULT_CLUSTER_KEY;
static char pre_hibernate_script[255] = CONFIG_TOI_DEFAULT_CLUSTER_PRE_HIBERNATE;
static char post_hibernate_script[255] = CONFIG_TOI_DEFAULT_CLUSTER_POST_HIBERNATE;

/*			List of cluster members			*/
static unsigned long continue_delay = 5 * HZ;
static unsigned long cluster_message_timeout = 3 * HZ;

/*		=== Membership list ===	*/

static void print_member_info(int index)
{
	struct cluster_member *this;

	printk(KERN_INFO "==> Dumping node %d.\n", index);

	list_for_each_entry(this, &node_array[index].member_list, list)
	    printk(KERN_INFO "%d.%d.%d.%d last message %s. %s\n",
		   NIPQUAD(this->addr),
		   str_message(this->message), this->ignore ? "(Ignored)" : "");
	printk(KERN_INFO "== Done ==\n");
}

static struct cluster_member *__find_member(int index, __be32 addr)
{
	struct cluster_member *this;

	list_for_each_entry(this, &node_array[index].member_list, list) {
		if (this->addr != addr)
			continue;

		return this;
	}

	return NULL;
}

static void set_ignore(int index, __be32 addr, struct cluster_member *this)
{
	if (this->ignore) {
		PRINTK("Node %d already ignoring %d.%d.%d.%d.\n", index, NIPQUAD(addr));
		return;
	}

	PRINTK("Node %d sees node %d.%d.%d.%d now being ignored.\n", index, NIPQUAD(addr));
	this->ignore = 1;
	node_array[index].ignored_peer_count++;
}

static int __add_update_member(int index, __be32 addr, int message)
{
	struct cluster_member *this;

	this = __find_member(index, addr);
	if (this) {
		if (this->message != message) {
			this->message = message;
			if ((message & MSG_NACK) &&
			    (message & (MSG_HIBERNATE | MSG_IMAGE | MSG_IO)))
				set_ignore(index, addr, this);
			PRINTK("Node %d sees node %d.%d.%d.%d now sending "
			       "%s.\n", index, NIPQUAD(addr), str_message(message));
			wake_up(&node_array[index].member_events);
		}
		return 0;
	}

	this = (struct cluster_member *)toi_kzalloc(36, sizeof(struct cluster_member), GFP_KERNEL);

	if (!this)
		return -1;

	this->addr = addr;
	this->message = message;
	this->ignore = 0;
	INIT_LIST_HEAD(&this->list);

	node_array[index].peer_count++;

	PRINTK("Node %d sees node %d.%d.%d.%d sending %s.\n", index,
	       NIPQUAD(addr), str_message(message));

	if ((message & MSG_NACK) && (message & (MSG_HIBERNATE | MSG_IMAGE | MSG_IO)))
		set_ignore(index, addr, this);
	list_add_tail(&this->list, &node_array[index].member_list);
	return 1;
}

static int add_update_member(int index, __be32 addr, int message)
{
	int result;
	unsigned long flags;
	spin_lock_irqsave(&node_array[index].member_list_lock, flags);
	result = __add_update_member(index, addr, message);
	spin_unlock_irqrestore(&node_array[index].member_list_lock, flags);

	print_member_info(index);

	wake_up(&node_array[index].member_events);

	return result;
}

static void del_member(int index, __be32 addr)
{
	struct cluster_member *this;
	unsigned long flags;

	spin_lock_irqsave(&node_array[index].member_list_lock, flags);
	this = __find_member(index, addr);

	if (this) {
		list_del_init(&this->list);
		toi_kfree(36, this, sizeof(*this));
		node_array[index].peer_count--;
	}

	spin_unlock_irqrestore(&node_array[index].member_list_lock, flags);
}

/*		=== Message transmission ===	*/

static void toi_send_if(int message, unsigned long my_id);

/*
 *  Process received TOI packet.
 */
static int toi_recv(struct sk_buff *skb, struct net_device *dev,
		    struct packet_type *pt, struct net_device *orig_dev)
{
	struct toi_pkt *b;
	struct iphdr *h;
	int len, result, index;
	unsigned long addr, message, ack;

	/* Perform verifications before taking the lock.  */
	if (skb->pkt_type == PACKET_OTHERHOST)
		goto drop;

	if (dev != net_dev)
		goto drop;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return NET_RX_DROP;

	if (!pskb_may_pull(skb, sizeof(struct iphdr) + sizeof(struct udphdr)))
		goto drop;

	b = (struct toi_pkt *)skb_network_header(skb);
	h = &b->iph;

	if (h->ihl != 5 || h->version != 4 || h->protocol != IPPROTO_UDP)
		goto drop;

	/* Fragments are not supported */
	if (h->frag_off & htons(IP_OFFSET | IP_MF)) {
		if (net_ratelimit())
			printk(KERN_ERR "TuxOnIce: Ignoring fragmented " "cluster message.\n");
		goto drop;
	}

	if (skb->len < ntohs(h->tot_len))
		goto drop;

	if (ip_fast_csum((char *)h, h->ihl))
		goto drop;

	if (b->udph.source != htons(toi_cluster_port_send) ||
	    b->udph.dest != htons(toi_cluster_port_recv))
		goto drop;

	if (ntohs(h->tot_len) < ntohs(b->udph.len) + sizeof(struct iphdr))
		goto drop;

	len = ntohs(b->udph.len) - sizeof(struct udphdr);

	/* Ok the front looks good, make sure we can get at the rest.  */
	if (!pskb_may_pull(skb, skb->len))
		goto drop;

	b = (struct toi_pkt *)skb_network_header(skb);
	h = &b->iph;

	addr = SADDR;
	PRINTK(">>> Message %s received from " NIPQUAD_FMT ".\n",
	       str_message(b->message), NIPQUAD(addr));

	message = b->message & MSG_STATE_MASK;
	ack = b->message & MSG_ACK_MASK;

	for (index = 0; index < num_local_nodes; index++) {
		int new_message = node_array[index].current_message, old_message = new_message;

		if (index == SADDR || !old_message) {
			PRINTK("Ignoring node %d (offline or self).\n", index);
			continue;
		}

		/* One message at a time, please. */
		spin_lock(&node_array[index].receive_lock);

		result = add_update_member(index, SADDR, b->message);
		if (result == -1) {
			printk(KERN_INFO "Failed to add new cluster member "
			       NIPQUAD_FMT ".\n", NIPQUAD(addr));
			goto drop_unlock;
		}

		switch (b->message & MSG_STATE_MASK) {
		case MSG_PING:
			break;
		case MSG_ABORT:
			break;
		case MSG_BYE:
			break;
		case MSG_HIBERNATE:
			/* Can I hibernate? */
			new_message = MSG_HIBERNATE | ((index & 1) ? MSG_NACK : MSG_ACK);
			break;
		case MSG_IMAGE:
			/* Can I resume? */
			new_message = MSG_IMAGE | ((index & 1) ? MSG_NACK : MSG_ACK);
			if (new_message != old_message)
				printk(KERN_ERR "Setting whether I can resume "
				       "to %d.\n", new_message);
			break;
		case MSG_IO:
			new_message = MSG_IO | MSG_ACK;
			break;
		case MSG_RUNNING:
			break;
		default:
			if (net_ratelimit())
				printk(KERN_ERR "Unrecognised TuxOnIce cluster"
				       " message %d from " NIPQUAD_FMT ".\n",
				       b->message, NIPQUAD(addr));
		};

		if (old_message != new_message) {
			node_array[index].current_message = new_message;
			printk(KERN_INFO ">>> Sending new message for node " "%d.\n", index);
			toi_send_if(new_message, index);
		} else if (!ack) {
			printk(KERN_INFO ">>> Resending message for node %d.\n", index);
			toi_send_if(new_message, index);
		}
 drop_unlock:
		spin_unlock(&node_array[index].receive_lock);
	};

 drop:
	/* Throw the packet out. */
	kfree_skb(skb);

	return 0;
}

/*
 *  Send cluster message to single interface.
 */
static void toi_send_if(int message, unsigned long my_id)
{
	struct sk_buff *skb;
	struct toi_pkt *b;
	int hh_len = LL_RESERVED_SPACE(net_dev);
	struct iphdr *h;

	/* Allocate packet */
	skb = alloc_skb(sizeof(struct toi_pkt) + hh_len + 15, GFP_KERNEL);
	if (!skb)
		return;
	skb_reserve(skb, hh_len);
	b = (struct toi_pkt *)skb_put(skb, sizeof(struct toi_pkt));
	memset(b, 0, sizeof(struct toi_pkt));

	/* Construct IP header */
	skb_reset_network_header(skb);
	h = ip_hdr(skb);
	h->version = 4;
	h->ihl = 5;
	h->tot_len = htons(sizeof(struct toi_pkt));
	h->frag_off = htons(IP_DF);
	h->ttl = 64;
	h->protocol = IPPROTO_UDP;
	h->daddr = htonl(INADDR_BROADCAST);
	h->check = ip_fast_csum((unsigned char *)h, h->ihl);

	/* Construct UDP header */
	b->udph.source = htons(toi_cluster_port_send);
	b->udph.dest = htons(toi_cluster_port_recv);
	b->udph.len = htons(sizeof(struct toi_pkt) - sizeof(struct iphdr));
	/* UDP checksum not calculated -- explicitly allowed in BOOTP RFC */

	/* Construct message */
	b->message = message;
	b->sid = my_id;
	b->htype = net_dev->type;	/* can cause undefined behavior */
	b->hlen = net_dev->addr_len;
	memcpy(b->hw_addr, net_dev->dev_addr, net_dev->addr_len);
	b->secs = htons(3);	/* 3 seconds */

	/* Chain packet down the line... */
	skb->dev = net_dev;
	skb->protocol = htons(ETH_P_IP);
	if ((dev_hard_header(skb, net_dev, ntohs(skb->protocol),
			     net_dev->broadcast, net_dev->dev_addr, skb->len) < 0) ||
	    dev_queue_xmit(skb) < 0)
		printk(KERN_INFO "E");
}

/*	=========================================		*/

/*			kTOICluster			*/

static atomic_t num_cluster_threads;
static DECLARE_WAIT_QUEUE_HEAD(clusterd_events);

static int kTOICluster(void *data)
{
	unsigned long my_id;

	my_id = atomic_add_return(1, &num_cluster_threads) - 1;
	node_array[my_id].current_message = (unsigned long)data;

	PRINTK("kTOICluster daemon %lu starting.\n", my_id);

	current->flags |= PF_NOFREEZE;

	while (node_array[my_id].current_message) {
		toi_send_if(node_array[my_id].current_message, my_id);
		sleep_on_timeout(&clusterd_events, cluster_message_timeout);
		PRINTK("Link state %lu is %d.\n", my_id, node_array[my_id].current_message);
	}

	toi_send_if(MSG_BYE, my_id);
	atomic_dec(&num_cluster_threads);
	wake_up(&clusterd_events);

	PRINTK("kTOICluster daemon %lu exiting.\n", my_id);
	__set_current_state(TASK_RUNNING);
	return 0;
}

static void kill_clusterd(void)
{
	int i;

	for (i = 0; i < num_local_nodes; i++) {
		if (node_array[i].current_message) {
			PRINTK("Seeking to kill clusterd %d.\n", i);
			node_array[i].current_message = 0;
		}
	}
	wait_event(clusterd_events, !atomic_read(&num_cluster_threads));
	PRINTK("All cluster daemons have exited.\n");
}

static int peers_not_in_message(int index, int message, int precise)
{
	struct cluster_member *this;
	unsigned long flags;
	int result = 0;

	spin_lock_irqsave(&node_array[index].member_list_lock, flags);
	list_for_each_entry(this, &node_array[index].member_list, list) {
		if (this->ignore)
			continue;

		PRINTK("Peer %d.%d.%d.%d sending %s. "
		       "Seeking %s.\n",
		       NIPQUAD(this->addr), str_message(this->message), str_message(message));
		if ((precise ? this->message : this->message & MSG_STATE_MASK) != message)
			result++;
	}
	spin_unlock_irqrestore(&node_array[index].member_list_lock, flags);
	PRINTK("%d peers in sought message.\n", result);
	return result;
}

static void reset_ignored(int index)
{
	struct cluster_member *this;
	unsigned long flags;

	spin_lock_irqsave(&node_array[index].member_list_lock, flags);
	list_for_each_entry(this, &node_array[index].member_list, list)
	    this->ignore = 0;
	node_array[index].ignored_peer_count = 0;
	spin_unlock_irqrestore(&node_array[index].member_list_lock, flags);
}

static int peers_in_message(int index, int message, int precise)
{
	return node_array[index].peer_count -
	    node_array[index].ignored_peer_count - peers_not_in_message(index, message, precise);
}

static int time_to_continue(int index, unsigned long start, int message)
{
	int first = peers_not_in_message(index, message, 0);
	int second = peers_in_message(index, message, 1);

	PRINTK("First part returns %d, second returns %d.\n", first, second);

	if (!first && !second) {
		PRINTK("All peers answered message %d.\n", message);
		return 1;
	}

	if (time_after(jiffies, start + continue_delay)) {
		PRINTK("Timeout reached.\n");
		return 1;
	}

	PRINTK("Not time to continue yet (%lu < %lu).\n", jiffies, start + continue_delay);
	return 0;
}

void toi_initiate_cluster_hibernate(void)
{
	int result;
	unsigned long start;

	result = do_toi_step(STEP_HIBERNATE_PREPARE_IMAGE);
	if (result)
		return;

	toi_send_if(MSG_HIBERNATE, 0);

	start = jiffies;
	wait_event(node_array[0].member_events, time_to_continue(0, start, MSG_HIBERNATE));

	if (test_action_state(TOI_FREEZER_TEST)) {
		toi_send_if(MSG_ABORT, 0);

		start = jiffies;
		wait_event(node_array[0].member_events, time_to_continue(0, start, MSG_RUNNING));

		do_toi_step(STEP_QUIET_CLEANUP);
		return;
	}

	toi_send_if(MSG_IO, 0);

	result = do_toi_step(STEP_HIBERNATE_SAVE_IMAGE);
	if (result)
		return;

	/* This code runs at resume time too! */
	if (toi_in_hibernate)
		result = do_toi_step(STEP_HIBERNATE_POWERDOWN);
}
EXPORT_SYMBOL_GPL(toi_initiate_cluster_hibernate);

/* toi_cluster_print_debug_stats
 *
 * Description:	Print information to be recorded for debugging purposes into a
 *		buffer.
 * Arguments:	buffer: Pointer to a buffer into which the debug info will be
 *			printed.
 *		size:	Size of the buffer.
 * Returns:	Number of characters written to the buffer.
 */
static int toi_cluster_print_debug_stats(char *buffer, int size)
{
	int len;

	if (strlen(toi_cluster_iface))
		len = scnprintf(buffer, size, "- Cluster interface is '%s'.\n", toi_cluster_iface);
	else
		len = scnprintf(buffer, size, "- Cluster support is disabled.\n");
	return len;
}

/* cluster_memory_needed
 *
 * Description:	Tell the caller how much memory we need to operate during
 *		hibernate/resume.
 * Returns:	Unsigned long. Maximum number of bytes of memory required for
 *		operation.
 */
static int toi_cluster_memory_needed(void)
{
	return 0;
}

static int toi_cluster_storage_needed(void)
{
	return 1 + strlen(toi_cluster_iface);
}

/* toi_cluster_save_config_info
 *
 * Description:	Save informaton needed when reloading the image at resume time.
 * Arguments:	Buffer:		Pointer to a buffer of size PAGE_SIZE.
 * Returns:	Number of bytes used for saving our data.
 */
static int toi_cluster_save_config_info(char *buffer)
{
	strcpy(buffer, toi_cluster_iface);
	return strlen(toi_cluster_iface + 1);
}

/* toi_cluster_load_config_info
 *
 * Description:	Reload information needed for declustering the image at
 *		resume time.
 * Arguments:	Buffer:		Pointer to the start of the data.
 *		Size:		Number of bytes that were saved.
 */
static void toi_cluster_load_config_info(char *buffer, int size)
{
	strncpy(toi_cluster_iface, buffer, size);
	return;
}

static void cluster_startup(void)
{
	int have_image = do_check_can_resume(), i;
	unsigned long start = jiffies, initial_message;
	struct task_struct *p;

	initial_message = MSG_IMAGE;

	have_image = 1;

	for (i = 0; i < num_local_nodes; i++) {
		PRINTK("Starting ktoiclusterd %d.\n", i);
		p = kthread_create(kTOICluster, (void *)initial_message, "ktoiclusterd/%d", i);
		if (IS_ERR(p)) {
			printk(KERN_ERR "Failed to start ktoiclusterd.\n");
			return;
		}

		wake_up_process(p);
	}

	/* Wait for delay or someone else sending first message */
	wait_event(node_array[0].member_events, time_to_continue(0, start, MSG_IMAGE));

	others_have_image = peers_in_message(0, MSG_IMAGE | MSG_ACK, 1);

	printk(KERN_INFO "Continuing. I %shave an image. Peers with image:"
	       " %d.\n", have_image ? "" : "don't ", others_have_image);

	if (have_image) {
		int result;

		/* Start to resume */
		printk(KERN_INFO "  === Starting to resume === \n");
		node_array[0].current_message = MSG_IO;
		toi_send_if(MSG_IO, 0);

		/* result = do_toi_step(STEP_RESUME_LOAD_PS1); */
		result = 0;

		if (!result) {
			/*
			 * Atomic restore - we'll come back in the hibernation
			 * path.
			 */

			/* result = do_toi_step(STEP_RESUME_DO_RESTORE); */
			result = 0;

			/* do_toi_step(STEP_QUIET_CLEANUP); */
		}

		node_array[0].current_message |= MSG_NACK;

		/* For debugging - disable for real life? */
		wait_event(node_array[0].member_events, time_to_continue(0, start, MSG_IO));
	}

	if (others_have_image) {
		/* Wait for them to resume */
		printk(KERN_INFO "Waiting for other nodes to resume.\n");
		start = jiffies;
		wait_event(node_array[0].member_events, time_to_continue(0, start, MSG_RUNNING));
		if (peers_not_in_message(0, MSG_RUNNING, 0))
			printk(KERN_INFO "Timed out while waiting for other " "nodes to resume.\n");
	}

	/* Find out whether an image exists here. Send ACK_IMAGE or NACK_IMAGE
	 * as appropriate.
	 *
	 * If we don't have an image:
	 * - Wait until someone else says they have one, or conditions are met
	 *   for continuing to boot (n machines or t seconds).
	 * - If anyone has an image, wait for them to resume before continuing
	 *   to boot.
	 *
	 * If we have an image:
	 * - Wait until conditions are met before continuing to resume (n
	 *   machines or t seconds). Send RESUME_PREP and freeze processes.
	 *   NACK_PREP if freezing fails (shouldn't) and follow logic for
	 *   us having no image above. On success, wait for [N]ACK_PREP from
	 *   other machines. Read image (including atomic restore) until done.
	 *   Wait for ACK_READ from others (should never fail). Thaw processes
	 *   and do post-resume. (The section after the atomic restore is done
	 *   via the code for hibernating).
	 */

	node_array[0].current_message = MSG_RUNNING;
}

/* toi_cluster_open_iface
 *
 * Description:	Prepare to use an interface.
 */

static int toi_cluster_open_iface(void)
{
	struct net_device *dev;

	rtnl_lock();

	for_each_netdev(&init_net, dev) {
		if (/* dev == &init_net.loopback_dev || */
			   strcmp(dev->name, toi_cluster_iface))
			continue;

		net_dev = dev;
		break;
	}

	rtnl_unlock();

	if (!net_dev) {
		printk(KERN_ERR MYNAME ": Device %s not found.\n", toi_cluster_iface);
		return -ENODEV;
	}

	dev_add_pack(&toi_cluster_packet_type);
	added_pack = 1;

	loopback_mode = (net_dev == init_net.loopback_dev);
	num_local_nodes = loopback_mode ? 8 : 1;

	PRINTK("Loopback mode is %s. Number of local nodes is %d.\n",
	       loopback_mode ? "on" : "off", num_local_nodes);

	cluster_startup();
	return 0;
}

/* toi_cluster_close_iface
 *
 * Description: Stop using an interface.
 */

static int toi_cluster_close_iface(void)
{
	kill_clusterd();
	if (added_pack) {
		dev_remove_pack(&toi_cluster_packet_type);
		added_pack = 0;
	}
	return 0;
}

static void write_side_effect(void)
{
	if (toi_cluster_ops.enabled) {
		toi_cluster_open_iface();
		set_toi_state(TOI_CLUSTER_MODE);
	} else {
		toi_cluster_close_iface();
		clear_toi_state(TOI_CLUSTER_MODE);
	}
}

static void node_write_side_effect(void)
{
}

/*
 * data for our sysfs entries.
 */
static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_STRING("interface", SYSFS_RW, toi_cluster_iface, IFNAMSIZ, 0,
		     NULL),
	SYSFS_INT("enabled", SYSFS_RW, &toi_cluster_ops.enabled, 0, 1, 0,
		  write_side_effect),
	SYSFS_STRING("cluster_name", SYSFS_RW, toi_cluster_key, 32, 0, NULL),
	SYSFS_STRING("pre-hibernate-script", SYSFS_RW, pre_hibernate_script,
		     256, 0, NULL),
	SYSFS_STRING("post-hibernate-script", SYSFS_RW, post_hibernate_script,
		     256, 0, STRING),
	SYSFS_UL("continue_delay", SYSFS_RW, &continue_delay, HZ / 2, 60 * HZ,
		 0)
};

/*
 * Ops structure.
 */

static struct toi_module_ops toi_cluster_ops = {
	.type = FILTER_MODULE,
	.name = "Cluster",
	.directory = "cluster",
	.module = THIS_MODULE,
	.memory_needed = toi_cluster_memory_needed,
	.print_debug_info = toi_cluster_print_debug_stats,
	.save_config_info = toi_cluster_save_config_info,
	.load_config_info = toi_cluster_load_config_info,
	.storage_needed = toi_cluster_storage_needed,

	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

/* ---- Registration ---- */

#ifdef MODULE
#define INIT static __init
#define EXIT static __exit
#else
#define INIT
#define EXIT
#endif

INIT int toi_cluster_init(void)
{
	int temp = toi_register_module(&toi_cluster_ops), i;
	struct kobject *kobj = toi_cluster_ops.dir_kobj;

	for (i = 0; i < MAX_LOCAL_NODES; i++) {
		node_array[i].current_message = 0;
		INIT_LIST_HEAD(&node_array[i].member_list);
		init_waitqueue_head(&node_array[i].member_events);
		spin_lock_init(&node_array[i].member_list_lock);
		spin_lock_init(&node_array[i].receive_lock);

		/* Set up sysfs entry */
		node_array[i].sysfs_data.attr.name = toi_kzalloc(8,
								 sizeof(node_array[i].sysfs_data.
									attr.name), GFP_KERNEL);
		sprintf((char *)node_array[i].sysfs_data.attr.name, "node_%d", i);
		node_array[i].sysfs_data.attr.mode = SYSFS_RW;
		node_array[i].sysfs_data.type = TOI_SYSFS_DATA_INTEGER;
		node_array[i].sysfs_data.flags = 0;
		node_array[i].sysfs_data.data.integer.variable =
		    (int *)&node_array[i].current_message;
		node_array[i].sysfs_data.data.integer.minimum = 0;
		node_array[i].sysfs_data.data.integer.maximum = INT_MAX;
		node_array[i].sysfs_data.write_side_effect = node_write_side_effect;
		toi_register_sysfs_file(kobj, &node_array[i].sysfs_data);
	}

	toi_cluster_ops.enabled = (strlen(toi_cluster_iface) > 0);

	if (toi_cluster_ops.enabled)
		toi_cluster_open_iface();

	return temp;
}

EXIT void toi_cluster_exit(void)
{
	int i;
	toi_cluster_close_iface();

	for (i = 0; i < MAX_LOCAL_NODES; i++)
		toi_unregister_sysfs_file(toi_cluster_ops.dir_kobj, &node_array[i].sysfs_data);
	toi_unregister_module(&toi_cluster_ops);
}

static int __init toi_cluster_iface_setup(char *iface)
{
	toi_cluster_ops.enabled = (*iface && strcmp(iface, "off"));

	if (toi_cluster_ops.enabled)
		strncpy(toi_cluster_iface, iface, strlen(iface));
}

__setup("toi_cluster=", toi_cluster_iface_setup);

#ifdef MODULE
MODULE_LICENSE("GPL");
module_init(toi_cluster_init);
module_exit(toi_cluster_exit);
MODULE_AUTHOR("Nigel Cunningham");
MODULE_DESCRIPTION("Cluster Support for TuxOnIce");
#endif
