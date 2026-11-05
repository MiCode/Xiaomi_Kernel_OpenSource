//#include <securec.h>

//#include <cetc_platform/log/cetc_log.h>
#include "rsmc_stdlib_s.h"
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/semaphore.h>
#include <linux/skbuff.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <uapi/linux/netlink.h>
#include <linux/genetlink.h>
#include <net/genetlink.h>

#include "../module_type.h"
#include "rsmc_msg_loop_x801.h"
#include "rsmc_spi_x801.h"
#include "rsmc_device_x801.h"

#ifdef CETCLOG_TAG
#undef CETCLOG_TAG
#endif
#define CETCLOG_TAG RSMC_MSG_LOOP

MODULE_LICENSE("GPL");

int rsmc_netlink_recv(struct sk_buff *__skb, struct genl_info *info);

enum channel_status {
	NETLINK_MSG_LOOP_EXIT = 0,
	NETLINK_MSG_LOOP_INIT,
};

#ifdef NETLINK_RSMC
#undef NETLINK_RSMC
#endif
#define NETLINK_RSMC 43

#define RSMC_HEART_BEAT_TIMEOUT msecs_to_jiffies(5000)
#define RSMC_SOC_HEART_BEAT_TIMEOUT msecs_to_jiffies(5000)
#define RSMC_SOC_SEND_HEART_INTERVAL msecs_to_jiffies(2000)
#define MAX_BIN_LEN 100

#define DTS_NODE_CETC_RSMC "xiaomi_rsmc"
#define DTS_PROP_RSMC_ENABLE "rsmc_enable"

static DEFINE_MUTEX(g_recv_mtx);
static DEFINE_MUTEX(g_send_mtx);

/* module IDs defined for each module */
enum install_model {
	RSMC_DEVICE = 0,
	MODEL_NUM,
};

#define MAX_NOR_MSG_LEN 100
#define MAX_FST_MSG_LEN 50
#define MAX_SPI_TRANS_MSG_LEN 50

/* The context of the message handle */
struct handle_ctx {
	/* Netlink socket fd */
	struct sock *nlfd;

	/* Save user space progress pid when user space netlink registering. */
	unsigned int native_pid;

	/* Tasks for send messages. */
	struct task_struct *up_task;

	/* Tasks for receive messages. */
	struct task_struct *dn_task;

	/* Channel status */
	int chan_state;

	/* Semaphore of the message sent */
	struct semaphore up_sema;

	spinlock_t up_lock;

	struct semaphore dn_inter_sema;

	spinlock_t dn_inter_lock;

	spinlock_t dn_spi_lock;

	/* Message processing callback functions */
	msg_process *mod_cb[MODEL_NUM];

	struct timer_list heartbeat_timer;

	volatile unsigned long native_jiffies;

	volatile unsigned long soc_rx_jiffies;

	volatile unsigned long soc_tx_jiffies;

	volatile bool heartbeat_running;

	volatile struct list_head *dn_inter_list[MAX_NOR_MSG_LEN + 4]; // add buffer 4

	volatile int dn_inter_end;

	volatile int dn_inter_start;

	/* Message send to x801 */
	volatile struct list_head *dn_spi_list[MAX_NOR_MSG_LEN + 4]; // add buffer 4

	volatile int dn_spi_end;

	volatile int dn_spi_start;

	volatile struct list_head *up_list[MAX_NOR_MSG_LEN + 4]; // add buffer 4

	volatile int up_end;

	volatile int up_start;
};

static struct handle_ctx g_nl_ctx = {0};

/* mesage map entry index */
enum map_index {
	MAP_KEY_INDEX = 0,
	MAP_VALUE_INDEX,
	MAP_ENTITY_NUM,
};

struct model_map {
	enum install_model model;
	model_reg *reg;
	model_unreg *unreg;
};

const static struct model_map module_init_map[MODEL_NUM] = {
	{RSMC_DEVICE, rsmc_device_reg_x801, rsmc_device_unreg_x801},
};

/* Message mapping table for external modules */
const static u16 cmd_module_map[][MAP_ENTITY_NUM] = {
	{CMD_DN_INIT_REQ, RSMC_DEVICE},
	{CMD_DN_ANY_REQ, RSMC_DEVICE},
	{CMD_DN_DTX_REQ, RSMC_DEVICE},
	{CMD_INTER_INIT_REQ, RSMC_DEVICE},
	{CMD_INTER_HB_TIMER_REQ, RSMC_DEVICE},
	{CMD_INTER_START_SPI_TIMER, RSMC_DEVICE},
	{CMD_INTER_STOP_SPI_TIMER, RSMC_DEVICE},
	{CMD_INTER_WAIT_STA_TIMEOUT, RSMC_DEVICE},
};

struct genl_ops rsmc_genl_ops = {
	.cmd = 1, //generic netlink对应command
	.flags = 0,
	.doit = rsmc_netlink_recv,
	.dumpit = NULL,
};

static struct genl_family rsmc_genl_family = {
	.id = 0, //指定由系统分配id
	.hdrsize = 0,
	.name = "rsmcgenl", //指定family name
	.version = 1,
	.maxattr = 4, //指定attibute最大数量
	.ops = &rsmc_genl_ops,
	.n_ops = 1,
        .resv_start_op = 2
};

u32 get_rsmc_spi_status(void)
{
	u32 status = get_spi_status();
	return status;
}

void rsmc_nl_up_msg_notify(struct msg_head *msg)
{
	struct msg_entity *p = NULL;
	u32 msg_len;
	int msg_idx, ret;

	if (msg == NULL)
		return;
	printk(KERN_INFO"%s: enter, type:%d,module:%d,len:%d",
			__func__, msg->type, msg->module, msg->len);
	if (g_nl_ctx.chan_state != NETLINK_MSG_LOOP_INIT) {
		printk(KERN_ERR"%s: module not inited", __func__);
		return;
	}
	msg_len = sizeof(struct list_head) + msg->len;
	p = kmalloc(msg_len, GFP_ATOMIC);
	if (p == NULL) {
		printk(KERN_ERR"%s: kmalloc failed", __func__);
		return;
	}
	ret = memcpy_s(&p->msg, msg_len, msg, msg->len);
	if (ret != EOK) {
		printk(KERN_ERR"%s: memcpy_s fail", __func__);
		return;
	}
	spin_lock_bh(&g_nl_ctx.up_lock);
	msg_idx = g_nl_ctx.up_end;
	if (msg_idx >= MAX_NOR_MSG_LEN || msg_idx < 0) {
		spin_unlock_bh(&g_nl_ctx.up_lock);
		printk(KERN_ERR"%s: idx err", __func__);
		return;
	}
	if ((g_nl_ctx.up_start + MAX_NOR_MSG_LEN - msg_idx) % MAX_NOR_MSG_LEN == 1) {
		spin_unlock_bh(&g_nl_ctx.up_lock);
		printk(KERN_ERR"%s: msg overlap", __func__);
		return;
	}
	g_nl_ctx.up_list[msg_idx] = (struct list_head *)p;
	msg_idx = (msg_idx + 1) % MAX_NOR_MSG_LEN;
	g_nl_ctx.up_end = msg_idx;
	barrier();
	if (g_nl_ctx.up_end != msg_idx)
		printk(KERN_ERR"%s: idx not new", __func__);
	spin_unlock_bh(&g_nl_ctx.up_lock);

	up(&g_nl_ctx.up_sema);

	printk(KERN_INFO"%s: exit", __func__);
}

void rsmc_nl_dn_msg_notify_spi(struct msg_head *msg)
{
	struct msg_entity *p = NULL;
	u32 msg_len;
	int msg_idx;
	int ret;

	if (msg == NULL)
		return;
	printk(KERN_INFO"%s: enter, type:%d,module:%d,len:%d",
		__func__, msg->type, msg->module, msg->len);
	if (g_nl_ctx.chan_state != NETLINK_MSG_LOOP_INIT) {
		printk(KERN_ERR"%s: module not inited", __func__);
		return;
	}
	msg_len = sizeof(struct list_head) + msg->len;
	p = kmalloc(msg_len, GFP_ATOMIC);
	if (p == NULL) {
		printk(KERN_ERR"%s: kmalloc failed", __func__);
		return;
	}
	ret = memcpy_s(&p->msg, msg_len, msg, msg->len);
	if (ret != EOK)
		return;

	spin_lock(&g_nl_ctx.dn_spi_lock);
	msg_idx = g_nl_ctx.dn_spi_end;
	if (msg_idx >= MAX_NOR_MSG_LEN || msg_idx < 0) {
		spin_unlock(&g_nl_ctx.dn_spi_lock);
		printk(KERN_ERR"%s: idx err", __func__);
		return;
	}
	if ((g_nl_ctx.dn_spi_start + MAX_NOR_MSG_LEN - msg_idx) % MAX_NOR_MSG_LEN == 1) {
		rsmc_clear_dn_spi_msg_list();
		spin_unlock(&g_nl_ctx.dn_spi_lock);
		printk(KERN_ERR"%s: msg overlap", __func__);
		return;
	}
	g_nl_ctx.dn_spi_list[msg_idx] = (struct list_head *)p;
	msg_idx = (msg_idx + 1) % MAX_NOR_MSG_LEN;
	g_nl_ctx.dn_spi_end = msg_idx;
	barrier();
	if (g_nl_ctx.dn_spi_end != msg_idx)
		printk(KERN_ERR"%s: idx not new", __func__);
	spin_unlock(&g_nl_ctx.dn_spi_lock);
	up_spi_sema();

	printk(KERN_INFO"%s: exit", __func__);
}

void rsmc_nl_dn_msg_notify_inter(struct msg_head *msg)
{
	struct msg_entity *p = NULL;
	u32 msg_len;
	int msg_idx;
	int ret;

	if (msg == NULL)
		return;
	printk(KERN_INFO"%s: enter, type:%d,module:%d,len:%d",
		__func__, msg->type, msg->module, msg->len);
	if (g_nl_ctx.chan_state != NETLINK_MSG_LOOP_INIT) {
		printk(KERN_ERR"%s: module not inited", __func__);
		return;
	}
	msg_len = sizeof(struct list_head) + msg->len;
	p = kmalloc(msg_len, GFP_ATOMIC);
	if (p == NULL) {
		printk(KERN_ERR"%s: kmalloc failed", __func__);
		return;
	}
	ret = memcpy_s(&p->msg, msg_len, msg, msg->len);
	if (ret != EOK)
		return;

	spin_lock(&g_nl_ctx.dn_inter_lock);
	msg_idx = g_nl_ctx.dn_inter_end;
	if (msg_idx >= MAX_NOR_MSG_LEN || msg_idx < 0) {
		spin_unlock(&g_nl_ctx.dn_inter_lock);
		printk(KERN_ERR"%s: idx err", __func__);
		return;
	}
	if ((g_nl_ctx.dn_inter_start + MAX_NOR_MSG_LEN - msg_idx) % MAX_NOR_MSG_LEN == 1) {
		rsmc_clear_dn_inter_msg_list();
		spin_unlock(&g_nl_ctx.dn_inter_lock);
		printk(KERN_ERR"%s: msg overlap", __func__);
		return;
	}
	g_nl_ctx.dn_inter_list[msg_idx] = (struct list_head *)p;
	msg_idx = (msg_idx + 1) % MAX_NOR_MSG_LEN;
	g_nl_ctx.dn_inter_end = msg_idx;
	barrier();
	if (g_nl_ctx.dn_inter_end != msg_idx)
		printk(KERN_ERR"%s: idx not new", __func__);
	spin_unlock(&g_nl_ctx.dn_inter_lock);
	up(&g_nl_ctx.dn_inter_sema);

	printk(KERN_INFO"%s: exit", __func__);
}

void rsmc_nl_dn_msg_notify_inter_nonblock(struct msg_head *msg)
{
	struct msg_entity *p = NULL;
	u32 msg_len;
	int msg_idx;
	int ret;

	if (msg == NULL)
		return;
	printk(KERN_INFO"%s: enter, type:%d,module:%d,len:%d",
		__func__, msg->type, msg->module, msg->len);
	if (g_nl_ctx.chan_state != NETLINK_MSG_LOOP_INIT) {
		printk(KERN_ERR"%s: module not inited", __func__);
		return;
	}
	msg_len = sizeof(struct list_head) + msg->len;
	p = kmalloc(msg_len, GFP_ATOMIC);
	if (p == NULL) {
		printk(KERN_ERR"%s: kmalloc failed", __func__);
		return;
	}
	ret = memcpy_s(&p->msg, msg_len, msg, msg->len);
	if (ret != EOK)
		return;

	msg_idx = g_nl_ctx.dn_inter_end;
	if (msg_idx >= MAX_NOR_MSG_LEN || msg_idx < 0) {
		printk(KERN_ERR"%s: idx err", __func__);
		return;
	}
	if ((g_nl_ctx.dn_inter_start + MAX_NOR_MSG_LEN - msg_idx) % MAX_NOR_MSG_LEN == 1) {
		rsmc_clear_dn_inter_msg_list();
		printk(KERN_ERR"%s: msg overlap", __func__);
		return;
	}
	g_nl_ctx.dn_inter_list[msg_idx] = (struct list_head *)p;
	msg_idx = (msg_idx + 1) % MAX_NOR_MSG_LEN;
	g_nl_ctx.dn_inter_end = msg_idx;
	barrier();
	if (g_nl_ctx.dn_inter_end != msg_idx)
		printk(KERN_ERR"%s: idx not new", __func__);
	up(&g_nl_ctx.dn_inter_sema);

	printk(KERN_INFO"%s: exit", __func__);
}

void rsmc_nl_dn_msg_dipatch(struct msg_head *msg)
{
	if (msg == NULL) {
		printk(KERN_ERR"%s: nl msg null", __func__);
		return;
	}
	if ((msg->type == CMD_DN_ANY_REQ) || (msg->type == CMD_DN_DTX_REQ))
		rsmc_nl_dn_msg_notify_spi(msg);
	else
		rsmc_nl_dn_msg_notify_inter(msg);
}

void rsmc_clear_dn_inter_msg_list(void)
{
	int ret, i;
	for (i = 0; i < MAX_NOR_MSG_LEN; i++) {
		struct msg_entity *msg = (struct msg_entity *)g_nl_ctx.dn_inter_list[i];
		if (msg != NULL)
			kfree(msg);
	}
	ret = memset_s(g_nl_ctx.dn_inter_list, sizeof(struct list_head *) * (MAX_NOR_MSG_LEN + 4),
		0, sizeof(struct list_head *) * MAX_NOR_MSG_LEN);
	if (ret != EOK)
		printk(KERN_ERR"%s: dn_list memset_s fail", __func__);
	g_nl_ctx.dn_inter_start = 0;
	g_nl_ctx.dn_inter_end = 0;
}

void rsmc_clear_dn_spi_msg_list(void)
{
	int ret, i;
	for (i = 0; i < MAX_NOR_MSG_LEN; i++) {
		struct msg_entity *spi_msg = (struct msg_entity *)g_nl_ctx.dn_spi_list[i];
		if (spi_msg != NULL)
			kfree(spi_msg);
	}
	ret = memset_s(g_nl_ctx.dn_spi_list, sizeof(struct list_head *) * (MAX_NOR_MSG_LEN + 4),
		0, sizeof(struct list_head *) * MAX_NOR_MSG_LEN);
	if (ret != EOK)
		printk(KERN_ERR"%s: dn_spi_list memset_s fail", __func__);
	g_nl_ctx.dn_spi_start = 0;
	g_nl_ctx.dn_spi_end = 0;
}

void rsmc_process_cmd_x801(struct msg_head *cmd)
{
	int i;

	if (cmd == NULL)
		return;
	printk(KERN_INFO"%s: enter, type:%d,module:%d",
		__func__, cmd->type, cmd->module);

	if (cmd->module != MODULE_TYPE_KNL) {
		printk(KERN_ERR"%s: module not kernel %d", __func__, cmd->module);
		return;
	}
	for (i = 0; i < sizeof(cmd_module_map) / (sizeof(u16) * MAP_ENTITY_NUM); i++) {
		if (cmd_module_map[i][MAP_KEY_INDEX] != cmd->type)
			continue;
		if (g_nl_ctx.mod_cb[cmd_module_map[i][MAP_VALUE_INDEX]] == NULL)
			break;
		g_nl_ctx.mod_cb[cmd_module_map[i][MAP_VALUE_INDEX]](cmd);
		break;
	}

	printk(KERN_INFO"%s: exit", __func__);
}

static char get_crc(char *data, int len)
{
	int i;
	char crc = 0;
	for (i = 0; i < len; i++)
		crc ^= data[i];
	return crc;
}

static void send_heartbeat_to_chip(void)
{
	char cmd[MAX_BIN_LEN] = {0};
	struct any_msg hrt_msg = {0};
	int cmd_len, len;

	printk(KERN_INFO"%s: enter", __func__);
	hrt_msg.head.type = CMD_DN_ANY_REQ;
	hrt_msg.head.module = MODULE_TYPE_KNL;
	hrt_msg.head.len = sizeof(struct any_msg);
	cmd_len =sprintf(cmd, "HRT,%u", jiffies_to_msecs(jiffies));
	len = sprintf(hrt_msg.data, "$%s*%.2x\r\n", cmd, get_crc(cmd, cmd_len));
	if (len <= 0) {
		printk(KERN_INFO"%s: sprintf fail", __func__);
		return;
	}
	rsmc_nl_dn_msg_notify_spi((struct msg_head *)&hrt_msg);
}

void send_up_soc_err(void)
{
	struct any_msg up_msg = {0};
	up_msg.head.type = CMD_UP_SOC_ERR_IND;
	up_msg.head.module = MODULE_TYPE_CTRL;
	up_msg.head.len = sizeof(struct any_msg);
	printk(KERN_INFO"%s: data len: %lu,data: %s", __func__, strlen(up_msg.data), up_msg.data);
	send_msg_to_ctrl_x801((struct msg_head *)&up_msg);
}

static void rsmc_heartbeat_process(void)
{
	printk(KERN_INFO"%s: heartbeat", __func__);
	g_nl_ctx.native_jiffies = (unsigned long)jiffies;
}

static void rsmc_heartbeat_callback(struct timer_list *t)
{
	if (!g_nl_ctx.heartbeat_running)
		return;
	if (jiffies - g_nl_ctx.native_jiffies > RSMC_HEART_BEAT_TIMEOUT) {
		struct enable_msg msg = {0};
		msg.head.type = CMD_INTER_INIT_REQ;
		msg.head.module = MODULE_TYPE_KNL;
		msg.head.len = sizeof(struct enable_msg);
		msg.status = 0;
		printk(KERN_INFO"%s: native heartbeat lost disable device", __func__);
		rsmc_clear_dn_inter_msg_list();
		rsmc_clear_dn_spi_msg_list();
		rsmc_nl_dn_msg_notify_inter_nonblock((struct msg_head *)&msg);
	} else if (jiffies - g_nl_ctx.soc_rx_jiffies > RSMC_SOC_HEART_BEAT_TIMEOUT) {
		printk(KERN_INFO"%s: x801 heartbeat lost report soc err", __func__);
		send_up_soc_err();
	} else {
		struct msg_head msg = {0};
		msg.type = CMD_INTER_HB_TIMER_REQ;
		msg.module = MODULE_TYPE_KNL;
		msg.len = sizeof(struct msg_head);
		rsmc_nl_dn_msg_notify_inter_nonblock((struct msg_head *)&msg);
	}
}

void rsmc_start_heartbeat_x801(void)
{
	if (g_nl_ctx.heartbeat_running) {
		del_timer_sync(&g_nl_ctx.heartbeat_timer);
	} else {
		g_nl_ctx.heartbeat_running = true;
		rsmc_heartbeat_process();
	}
	timer_setup(&g_nl_ctx.heartbeat_timer,
		rsmc_heartbeat_callback,
		TIMER_IRQSAFE);
	g_nl_ctx.heartbeat_timer.expires = jiffies + HZ;
	add_timer(&g_nl_ctx.heartbeat_timer);
	printk(KERN_INFO"%s", __func__);
	if (jiffies - g_nl_ctx.soc_tx_jiffies > RSMC_SOC_SEND_HEART_INTERVAL)
		send_heartbeat_to_chip();
}

void rsmc_stop_heartbeat_x801(void)
{
	if (g_nl_ctx.heartbeat_running) {
		del_timer_sync(&g_nl_ctx.heartbeat_timer);
		g_nl_ctx.heartbeat_running = false;
	}
	printk(KERN_INFO"%s", __func__);
}

void rsmc_update_soc_rx_jiffies(void)
{
	printk(KERN_INFO"%s", __func__);
	g_nl_ctx.soc_rx_jiffies = (unsigned long)jiffies;
}

void rsmc_update_soc_tx_jiffies(void)
{
	printk(KERN_INFO"%s", __func__);
	g_nl_ctx.soc_tx_jiffies = (unsigned long)jiffies;
}

int rsmc_netlink_recv(struct sk_buff *__skb, struct genl_info *info)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
        struct genlmsghdr *genlhdr = NULL;
        struct nlattr *nlattr = NULL;

	if (g_nl_ctx.chan_state != NETLINK_MSG_LOOP_INIT) {
		printk(KERN_ERR"%s: module not inited", __func__);
		return 0;
	}
	if (__skb == NULL) {
		printk(KERN_ERR"%s: __skb null", __func__);
		return 0;
	}
	skb = skb_get(__skb);
	if (skb == NULL) {
		printk(KERN_ERR"%s: skb null", __func__);
		return 0;
	}
	mutex_lock(&g_recv_mtx);
	if (skb->len < NLMSG_HDRLEN) {
		printk(KERN_ERR"%s: skb len error", __func__);
		goto skb_free;
	}
	nlh = nlmsg_hdr(skb);
        printk(KERN_INFO"%s: nlh.nlmsg_len = %d,nlh.nlmsg_type = %d,nlh.nlmsg_flags = %d,nlh.nlmsg_seq = %d,nlh.nlmsg_pid = %d", __func__,nlh->nlmsg_len,nlh->nlmsg_type,nlh->nlmsg_flags,nlh->nlmsg_seq,nlh->nlmsg_pid);
	if (nlh == NULL) {
		printk(KERN_ERR"%s: nlh = NULL", __func__);
		goto skb_free;
	}
	if ((nlh->nlmsg_len < sizeof(struct nlmsghdr)) ||
		(skb->len < nlh->nlmsg_len)) {
		printk(KERN_ERR"%s: nlmsg len error", __func__);
		goto skb_free;
	}
        genlhdr=nlmsg_data(nlh);
        printk(KERN_INFO"%s: genlhdr.cmd = %d,genlhdr.version = %d,genlhdr.reserved = %d", __func__,genlhdr->cmd,genlhdr->version,genlhdr->reserved);
        nlattr=genlmsg_data(genlhdr);
        printk(KERN_INFO"%s: nlattr.nla_len = %d,nlattr.nla_type = %d", __func__,nlattr->nla_len,nlattr->nla_type);
	switch (nlattr->nla_type) {
        //switch(nlh->nlmsg_type) {
	case NL_MSG_REG:
		g_nl_ctx.native_pid = nlh->nlmsg_pid;
		break;
	case NL_MSG_HEARTBEAT:
		rsmc_heartbeat_process();
		break;
	case NL_MSG_REQ:
		//rsmc_nl_dn_msg_dipatch((struct msg_head *)NLMSG_DATA(nlh));
                //rsmc_nl_dn_msg_dipatch((struct msg_head *)genlhdr);
                rsmc_nl_dn_msg_dipatch((struct msg_head *)nla_data(nlattr));
		break;
	default:
		break;
	}

skb_free:
	kfree_skb(skb);
	mutex_unlock(&g_recv_mtx);
	return 0;
}

static struct msg_entity *rsmc_nl_dn_get_inter_msg(void)
{
	struct msg_entity *msg = NULL;
	int idx;

	spin_lock_bh(&g_nl_ctx.dn_inter_lock);
	if (g_nl_ctx.dn_inter_end == g_nl_ctx.dn_inter_start) {
		spin_unlock_bh(&g_nl_ctx.dn_inter_lock);
		return NULL;
	}
	idx = g_nl_ctx.dn_inter_start;
	if (idx >= MAX_NOR_MSG_LEN || idx < 0) {
		spin_unlock_bh(&g_nl_ctx.dn_inter_lock);
		printk(KERN_ERR"%s: idx error", __func__);
		return NULL;
	}
	if (g_nl_ctx.dn_inter_list[idx] == NULL) {
		printk(KERN_ERR"%s: list is null", __func__);
		msg = NULL;
	} else {
		msg = (struct msg_entity *)g_nl_ctx.dn_inter_list[idx];
		g_nl_ctx.dn_inter_list[idx] = NULL;
	}
	g_nl_ctx.dn_inter_start = (idx + 1) % MAX_NOR_MSG_LEN;
	spin_unlock_bh(&g_nl_ctx.dn_inter_lock);
	if (msg != NULL)
		printk(KERN_INFO"%s: msg id=%d", __func__, msg->msg.type);
	return msg;
}

struct msg_entity *rsmc_nl_dn_get_spi_msg(void)
{
	struct msg_entity *msg = NULL;
	int idx;

	spin_lock_bh(&g_nl_ctx.dn_spi_lock);
	if (g_nl_ctx.dn_spi_end == g_nl_ctx.dn_spi_start) {
		spin_unlock_bh(&g_nl_ctx.dn_spi_lock);
		return NULL;
	}
	idx = g_nl_ctx.dn_spi_start;
	if (idx >= MAX_NOR_MSG_LEN || idx < 0) {
		spin_unlock_bh(&g_nl_ctx.dn_spi_lock);
		printk(KERN_ERR"%s: idx error", __func__);
		return NULL;
	}
	if (g_nl_ctx.dn_spi_list[idx] == NULL) {
		printk(KERN_ERR"%s: list is null", __func__);
		msg = NULL;
	} else {
		msg = (struct msg_entity *)g_nl_ctx.dn_spi_list[idx];
		g_nl_ctx.dn_spi_list[idx] = NULL;
	}
	g_nl_ctx.dn_spi_start = (idx + 1) % MAX_NOR_MSG_LEN;
	spin_unlock_bh(&g_nl_ctx.dn_spi_lock);
	if (msg != NULL)
		printk(KERN_INFO"%s: msg id=%d", __func__, msg->msg.type);
	return msg;
}

static struct msg_entity *rsmc_nl_up_get_msg(void)
{
	struct msg_entity *msg = NULL;
	int idx;

	spin_lock_bh(&g_nl_ctx.up_lock);
	if (g_nl_ctx.up_end == g_nl_ctx.up_start) {
		spin_unlock_bh(&g_nl_ctx.up_lock);
		return NULL;
	}
	idx = g_nl_ctx.up_start;
	if (idx >= MAX_NOR_MSG_LEN || idx < 0) {
		spin_unlock_bh(&g_nl_ctx.up_lock);
		printk(KERN_ERR"%s: idx error", __func__);
		return NULL;
	}
	if (g_nl_ctx.up_list[idx] == NULL) {
		printk(KERN_ERR"%s: list is null", __func__);
		msg = NULL;
	} else {
		msg = (struct msg_entity *)g_nl_ctx.up_list[idx];
		g_nl_ctx.up_list[idx] = NULL;
	}
	g_nl_ctx.up_start = (idx + 1) % MAX_NOR_MSG_LEN;
	spin_unlock_bh(&g_nl_ctx.up_lock);
	if (msg != NULL)
		printk(KERN_INFO"%s: msg id=%d", __func__, msg->msg.type);
	return msg;
}

/* send a message to user space */
int rsmc_nl_up_notify_event_x801(struct msg_head *msg)
{
	int ret;
	struct sk_buff *skb = NULL;
        //struct nlmsghdr *nlh = NULL;
        //void *pdata = NULL;
        void *hdr = NULL;

	if (msg == NULL)
		return -1;

	mutex_lock(&g_send_mtx);
	if (!g_nl_ctx.native_pid) {// || (g_nl_ctx.nlfd == NULL)
		printk(KERN_ERR"%s: err pid = %d\n",
			__func__, g_nl_ctx.native_pid);
		ret = -1;
		goto nty_end;
	}

	//skb = nlmsg_new(msg->len, GFP_ATOMIC);
        skb = genlmsg_new(msg->len, GFP_ATOMIC);
	if (skb == NULL) {
		printk(KERN_ERR"%s: alloc skb fail\n", __func__);
		ret = -1;
		goto nty_end;
	}
  
        hdr = genlmsg_put(skb, 0, 0, &rsmc_genl_family, 0, 1);

	/*nlh = nlmsg_put(skb, 0, 0, msg->type, msg->len, 0);
	if (nlh == NULL) {
		kfree_skb(skb);
		skb = NULL;
		ret = -1;
		goto nty_end;
	}*/
  
        nla_put(skb,msg->type,msg->len,msg);
	/*pdata = nlmsg_data(nlh);
	ret = memcpy_s(pdata, msg->len, msg, msg->len);
	if (ret != EOK) {
		kfree_skb(skb);
		skb = NULL;
		ret = -1;
		goto nty_end;
	}*/
        genlmsg_end(skb,hdr);

	/* skb will be freed in netlink_unicast */
        ret = genlmsg_unicast(&init_net,skb,g_nl_ctx.native_pid);
  
	//ret = netlink_unicast(g_nl_ctx.nlfd, skb,
	//	g_nl_ctx.native_pid, MSG_DONTWAIT);

nty_end:
	mutex_unlock(&g_send_mtx);
	return ret;
}

int rsmc_up_msg_thread(void *data)
{
	struct msg_entity *msg = NULL;

	printk(KERN_INFO"%s: enter", __func__);

	while (!kthread_should_stop()) {
		down(&g_nl_ctx.up_sema);
		if (g_nl_ctx.native_pid == 0)
			continue;
		msg = rsmc_nl_up_get_msg();
		while (msg != NULL) {
			rsmc_nl_up_notify_event_x801(&msg->msg);
			kfree(msg);
			msg = rsmc_nl_up_get_msg();
		}
	}
	return 0;
}

int rsmc_dn_msg_thread(void *data)
{
	struct msg_entity *msg = NULL;
	printk(KERN_INFO"%s: enter", __func__);
	while (!kthread_should_stop()) {
		down(&g_nl_ctx.dn_inter_sema);
		if (g_nl_ctx.native_pid == 0)
			continue;
		msg = rsmc_nl_dn_get_inter_msg();
		while (msg != NULL) {
			rsmc_process_cmd_x801(&msg->msg);
			kfree(msg);
			msg = rsmc_nl_dn_get_inter_msg();
		}
	}
	return 0;
}

/* netlink init function */
int rsmc_netlink_init(void)
{
	struct sched_param param;
	/*struct netlink_kernel_cfg nb_nl_cfg = {
		.input = rsmc_netlink_recv,
	};

	g_nl_ctx.nlfd = netlink_kernel_create(&init_net,
		NETLINK_RSMC, &nb_nl_cfg);

	if (g_nl_ctx.nlfd == NULL) {
		printk(KERN_ERR"%s: netlink_handle_init failed", __func__);
		return -EINVAL;
	}*/
  
        genl_register_family(&rsmc_genl_family);

	sema_init(&g_nl_ctx.up_sema, 0);
	sema_init(&g_nl_ctx.dn_inter_sema, 0);
	spin_lock_init(&g_nl_ctx.up_lock);
	spin_lock_init(&g_nl_ctx.dn_inter_lock);
	spin_lock_init(&g_nl_ctx.dn_spi_lock);

	g_nl_ctx.up_task = kthread_run(
		rsmc_up_msg_thread,
		NULL,
		"rsmc_nl_up_thread");
	if (IS_ERR(g_nl_ctx.up_task)) {
		printk(KERN_ERR"%s: failed to create thread", __func__);
		g_nl_ctx.up_task = NULL;
		return -EINVAL;
	}
	param.sched_priority = MAX_RT_PRIO - 1;
	sched_setscheduler(g_nl_ctx.up_task, SCHED_FIFO, &param);

	g_nl_ctx.dn_task = kthread_run(
		rsmc_dn_msg_thread,
		NULL,
		"rsmc_nl_dn_thread");
	if (IS_ERR(g_nl_ctx.dn_task)) {
		printk(KERN_ERR"%s: failed to create thread", __func__);
		g_nl_ctx.dn_task = NULL;
		return -EINVAL;
	}
	param.sched_priority = MAX_RT_PRIO - 1;
	sched_setscheduler(g_nl_ctx.dn_task, SCHED_FIFO, &param);

	g_nl_ctx.chan_state = NETLINK_MSG_LOOP_INIT;

	return 0;
}

/* netlink deinit function */
void rsmc_netlink_exit(void)
{
	if (g_nl_ctx.nlfd && g_nl_ctx.nlfd->sk_socket) {
		sock_release(g_nl_ctx.nlfd->sk_socket);
		g_nl_ctx.nlfd = NULL;
	}

	if (g_nl_ctx.up_task != NULL) {
		kthread_stop(g_nl_ctx.up_task);
		g_nl_ctx.up_task = NULL;
	}
	if (g_nl_ctx.dn_task != NULL) {
		kthread_stop(g_nl_ctx.dn_task);
		g_nl_ctx.dn_task = NULL;
	}
}

int regist_model_x801(model_reg *fun, enum install_model model)
{
	msg_process *fn = NULL;

	if (fun == NULL)
		return -EINVAL;
	if (model > MODEL_NUM || model < 0) {
		printk(KERN_ERR"%s: invalid model, model is :%d", __func__, model);
		return -EINVAL;
	}
	fn = fun(rsmc_nl_up_msg_notify);
	if (fn == NULL) {
		printk(KERN_ERR"%s: fn null:%d", __func__, model);
		return -EINVAL;
	}

	g_nl_ctx.mod_cb[model] = fn;
	return 0;
}

int rsmc_main_thread_x801(void *data)
{
	model_reg *reg_fn = NULL;
	int rtn = 0;
	int model;

	printk(KERN_INFO"%s: enter", __func__);
	if (rsmc_netlink_init()) {
		printk(KERN_ERR"%s: init netlink_handle module failed", __func__);
		g_nl_ctx.chan_state = NETLINK_MSG_LOOP_EXIT;
		return -EINVAL;
	}

	for (model = 0; model < MODEL_NUM; model++) {
		if (module_init_map[model].model != model) {
			printk(KERN_ERR"%s: model init map error", __func__);
			return -EINVAL;
		}
		reg_fn = module_init_map[model].reg;
		if (reg_fn)
			rtn += regist_model_x801(reg_fn, model);
	}
	if (rtn < 0)
		return rtn;
	g_nl_ctx.up_start = 0;
	g_nl_ctx.up_end = 0;
	rtn = memset_s(g_nl_ctx.up_list, sizeof(struct list_head *) * (MAX_NOR_MSG_LEN + 4),
		0, sizeof(struct list_head *) * MAX_NOR_MSG_LEN);
	if (rtn != EOK)
		return -EINVAL;
	rsmc_clear_dn_inter_msg_list();
	rsmc_clear_dn_spi_msg_list();
	g_nl_ctx.heartbeat_running = false;
	printk(KERN_INFO"%s: netlink_handle module inited", __func__);
	return 0;
}

int rsmc_init_x801(void)
{
	struct task_struct *task = kthread_run(rsmc_main_thread_x801, NULL, "rsmc_main_thread_x801");
	if (IS_ERR(task)) {
		printk(KERN_ERR"%s: failed to create thread", __func__);
		task = NULL;
		return -EINVAL;
	}
	return 0;
}

void rsmc_exit_x801(void)
{
	model_unreg *unreg_fn = NULL;
	int model;
	for (model = 0; model < MODEL_NUM; model++) {
		if (module_init_map[model].model != model) {
			printk(KERN_ERR"%s: model init map error", __func__);
			continue;
		}
		unreg_fn = module_init_map[model].unreg;
		if (unreg_fn != NULL)
			unreg_fn(0);
	}
	g_nl_ctx.chan_state = NETLINK_MSG_LOOP_EXIT;
	rsmc_netlink_exit();
}

