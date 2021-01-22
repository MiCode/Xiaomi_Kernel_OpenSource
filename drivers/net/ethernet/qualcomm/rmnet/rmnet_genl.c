// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * RMNET Data Generic Netlink
 *
 */

#include "rmnet_genl.h"
#include <net/sock.h>
#include <linux/skbuff.h>

/* Static Functions and Definitions */
static struct nla_policy rmnet_genl_attr_policy[RMNET_CORE_GENL_ATTR_MAX +
						1] = {
	[RMNET_CORE_GENL_ATTR_INT]  = { .type = NLA_S32 },
	[RMNET_CORE_GENL_ATTR_PID_BPS] = { .len =
				sizeof(struct rmnet_core_pid_bps_resp) },
	[RMNET_CORE_GENL_ATTR_PID_BOOST] = { .len =
				sizeof(struct rmnet_core_pid_boost_req) },
	[RMNET_CORE_GENL_ATTR_STR]  = { .type = NLA_NUL_STRING },
};

#define RMNET_CORE_GENL_OP(_cmd, _func)			\
	{						\
		.cmd	= _cmd,				\
		.policy	= rmnet_genl_attr_policy,	\
		.doit	= _func,			\
		.dumpit	= NULL,				\
		.flags	= 0,				\
	}

static const struct genl_ops rmnet_core_genl_ops[] = {
	RMNET_CORE_GENL_OP(RMNET_CORE_GENL_CMD_PID_BPS_REQ,
			   rmnet_core_genl_pid_bps_req_hdlr),
	RMNET_CORE_GENL_OP(RMNET_CORE_GENL_CMD_PID_BOOST_REQ,
			   rmnet_core_genl_pid_boost_req_hdlr),
};

struct genl_family rmnet_core_genl_family = {
	.hdrsize = 0,
	.name    = RMNET_CORE_GENL_FAMILY_NAME,
	.version = RMNET_CORE_GENL_VERSION,
	.maxattr = RMNET_CORE_GENL_ATTR_MAX,
	.ops     = rmnet_core_genl_ops,
	.n_ops   = ARRAY_SIZE(rmnet_core_genl_ops),
};

#define RMNET_PID_STATS_HT_SIZE (8)
#define RMNET_PID_STATS_HT rmnet_pid_ht
DEFINE_HASHTABLE(rmnet_pid_ht, RMNET_PID_STATS_HT_SIZE);

/* Spinlock definition for pid hash table */
static DEFINE_SPINLOCK(rmnet_pid_ht_splock);

#define RMNET_GENL_SEC_TO_MSEC(x)   ((x) * 1000)
#define RMNET_GENL_SEC_TO_NSEC(x)   ((x) * 1000000000)
#define RMNET_GENL_BYTES_TO_BITS(x) ((x) * 8)
#define RMNET_GENL_NSEC_TO_SEC(x) ({\
	u64 __quotient = (x); \
	do_div(__quotient, 1000000000); \
	__quotient; \
})

int rmnet_core_userspace_connected;
#define RMNET_QUERY_PERIOD_SEC (1) /* Period of pid/bps queries */

struct rmnet_pid_node_s {
	struct hlist_node list;
	time_t timstamp_last_query;
	u64 tx_bytes;
	u64 tx_bytes_last_query;
	u64 tx_bps;
	u64 sched_boost_period_ms;
	int sched_boost_remaining_ms;
	int sched_boost_enable;
	pid_t pid;
};

void rmnet_update_pid_and_check_boost(pid_t pid, unsigned int len,
				      int *boost_enable, u64 *boost_period)
{
	struct hlist_node *tmp;
	struct rmnet_pid_node_s *node_p;
	unsigned long ht_flags;
	u8 is_match_found = 0;
	u64 tx_bytes = 0;

	*boost_enable = 0;
	*boost_period = 0;

	/*  Using do while to spin lock and unlock only once */
	spin_lock_irqsave(&rmnet_pid_ht_splock, ht_flags);
	do {
		hash_for_each_possible_safe(RMNET_PID_STATS_HT, node_p, tmp,
					    list, pid) {
			if (pid != node_p->pid)
				continue;

			/* PID Match found */
			is_match_found = 1;
			node_p->tx_bytes += len;
			tx_bytes = node_p->tx_bytes;

			if (node_p->sched_boost_enable) {
				rm_err("boost triggered for pid %d",
				       pid);
				/* Just triggered boost, dont re-trigger */
				node_p->sched_boost_enable = 0;
				*boost_enable = 1;
				*boost_period = node_p->sched_boost_period_ms;
				node_p->sched_boost_remaining_ms =
							(int)*boost_period;
			}

			break;
		}

		if (is_match_found)
			break;

		/* No PID match */
		node_p = kzalloc(sizeof(*node_p), GFP_ATOMIC);
		if (!node_p)
			break;

		node_p->pid = pid;
		node_p->tx_bytes = len;
		node_p->sched_boost_enable = 0;
		node_p->sched_boost_period_ms = 0;
		node_p->sched_boost_remaining_ms = 0;
		hash_add_rcu(RMNET_PID_STATS_HT, &node_p->list, pid);
		break;
	} while (0);
	spin_unlock_irqrestore(&rmnet_pid_ht_splock, ht_flags);
}

void rmnet_boost_for_pid(pid_t pid, int boost_enable,
			 u64 boost_period)
{
	struct hlist_node *tmp;
	struct rmnet_pid_node_s *node_p;
	unsigned long ht_flags;

	/*  Using do while to spin lock and unlock only once */
	spin_lock_irqsave(&rmnet_pid_ht_splock, ht_flags);
	do {
		hash_for_each_possible_safe(RMNET_PID_STATS_HT, node_p, tmp,
					    list, pid) {
			if (pid != node_p->pid)
				continue;

			/* PID Match found */
			rm_err("CORE_BOOST: enable boost for pid %d for %llu ms",
			       pid, boost_period);
			node_p->sched_boost_enable = boost_enable;
			node_p->sched_boost_period_ms = boost_period;
			break;
		}

		break;
	} while (0);
	spin_unlock_irqrestore(&rmnet_pid_ht_splock, ht_flags);
}

static void rmnet_create_pid_bps_resp(struct rmnet_core_pid_bps_resp
				      *pid_bps_resp_ptr)
{
	struct timespec time;
	struct hlist_node *tmp;
	struct rmnet_pid_node_s *node_p;
	unsigned long ht_flags;
	u64 tx_bytes_cur, byte_diff, time_diff_ns, tmp_bits;
	int i;
	u16 bkt;

	(void)getnstimeofday(&time);
	pid_bps_resp_ptr->timestamp = RMNET_GENL_SEC_TO_NSEC(time.tv_sec) +
		   time.tv_nsec;

	/*  Using do while to spin lock and unlock only once */
	spin_lock_irqsave(&rmnet_pid_ht_splock, ht_flags);
	do {
		i = 0;

		hash_for_each_safe(RMNET_PID_STATS_HT, bkt, tmp,
				   node_p, list) {
			tx_bytes_cur = node_p->tx_bytes;
			if (tx_bytes_cur <= node_p->tx_bytes_last_query) {
				/* Dont send inactive pids to userspace */
				hash_del(&node_p->list);
				kfree(node_p);
				continue;
			}

			/* Compute bits per second */
			byte_diff = (node_p->tx_bytes -
				     node_p->tx_bytes_last_query);
			time_diff_ns = (pid_bps_resp_ptr->timestamp -
					node_p->timstamp_last_query);

			tmp_bits = RMNET_GENL_BYTES_TO_BITS(byte_diff);
			/* Note that do_div returns remainder and the */
			/* numerator gets assigned the quotient */
			/* Since do_div takes the numerator as a reference, */
			/* a tmp_bits is used*/
			do_div(tmp_bits, RMNET_GENL_NSEC_TO_SEC(time_diff_ns));
			node_p->tx_bps = tmp_bits;

			if (node_p->sched_boost_remaining_ms >=
			    RMNET_GENL_SEC_TO_MSEC(RMNET_QUERY_PERIOD_SEC)) {
				node_p->sched_boost_remaining_ms -=
				RMNET_GENL_SEC_TO_MSEC(RMNET_QUERY_PERIOD_SEC);

				rm_err("CORE_BOOST: enabling boost for pid %d\n"
				       "sched boost remaining = %d ms",
				       node_p->pid,
				       node_p->sched_boost_remaining_ms);
			} else {
				node_p->sched_boost_remaining_ms = 0;
			}

			pid_bps_resp_ptr->list[i].pid = node_p->pid;
			pid_bps_resp_ptr->list[i].tx_bps = node_p->tx_bps;
			pid_bps_resp_ptr->list[i].boost_remaining_ms =
					node_p->sched_boost_remaining_ms;

			node_p->timstamp_last_query =
				pid_bps_resp_ptr->timestamp;
			node_p->tx_bytes_last_query = tx_bytes_cur;
			i++;

			/* Support copying up to 32 active pids */
			if (i >= RMNET_CORE_GENL_MAX_PIDS)
				break;
		}
		break;
	} while (0);
	spin_unlock_irqrestore(&rmnet_pid_ht_splock, ht_flags);

	pid_bps_resp_ptr->list_len = i;
}

int rmnet_core_genl_send_resp(struct genl_info *info,
			      struct rmnet_core_pid_bps_resp *pid_bps_resp)
{
	struct sk_buff *skb;
	void *msg_head;
	int rc;

	if (!info || !pid_bps_resp) {
		rm_err("%s", "SHS_GNL: Invalid params\n");
		goto out;
	}

	skb = genlmsg_new(sizeof(struct rmnet_core_pid_bps_resp), GFP_KERNEL);
	if (!skb)
		goto out;

	msg_head = genlmsg_put(skb, 0, info->snd_seq + 1,
			       &rmnet_core_genl_family,
			       0, RMNET_CORE_GENL_CMD_PID_BPS_REQ);
	if (!msg_head) {
		rc = -ENOMEM;
		goto out;
	}
	rc = nla_put(skb, RMNET_CORE_GENL_ATTR_PID_BPS,
		     sizeof(struct rmnet_core_pid_bps_resp),
		     pid_bps_resp);
	if (rc != 0)
		goto out;

	genlmsg_end(skb, msg_head);

	rc = genlmsg_unicast(genl_info_net(info), skb, info->snd_portid);
	if (rc != 0)
		goto out;

	rm_err("%s", "SHS_GNL: Successfully sent pid/bytes info\n");
	return RMNET_GENL_SUCCESS;

out:
	/* TODO: Need to free skb?? */
	rm_err("%s", "SHS_GNL: FAILED to send pid/bytes info\n");
	rmnet_core_userspace_connected = 0;
	return RMNET_GENL_FAILURE;
}

int rmnet_core_genl_pid_bps_req_hdlr(struct sk_buff *skb_2,
				     struct genl_info *info)
{
	struct nlattr *na;
	struct rmnet_core_pid_bps_req  pid_bps_req;
	struct rmnet_core_pid_bps_resp pid_bps_resp;
	int is_req_valid = 0;

	rm_err("CORE_GNL: %s connected = %d", __func__,
	       rmnet_core_userspace_connected);

	if (!info) {
		rm_err("%s", "CORE_GNL: error - info is null");
		pid_bps_resp.valid = 0;
	} else {
		na = info->attrs[RMNET_CORE_GENL_ATTR_PID_BPS];
		if (na) {
			if (nla_memcpy(&pid_bps_req, na,
				       sizeof(pid_bps_req)) > 0) {
				is_req_valid = 1;
			} else {
				rm_err("CORE_GNL: nla_memcpy failed %d\n",
				       RMNET_CORE_GENL_ATTR_PID_BPS);
			}
		} else {
			rm_err("CORE_GNL: no info->attrs %d\n",
			       RMNET_CORE_GENL_ATTR_PID_BPS);
		}
	}

	if (!rmnet_core_userspace_connected)
		rmnet_core_userspace_connected = 1;

	/* Copy to pid/byte list to the payload */
	memset(&pid_bps_resp, 0x0,
	       sizeof(pid_bps_resp));
	if (is_req_valid) {
		rmnet_create_pid_bps_resp(&pid_bps_resp);
	}
	pid_bps_resp.valid = 1;

	rmnet_core_genl_send_resp(info, &pid_bps_resp);

	return RMNET_GENL_SUCCESS;
}

int rmnet_core_genl_pid_boost_req_hdlr(struct sk_buff *skb_2,
				       struct genl_info *info)
{
	struct nlattr *na;
	struct rmnet_core_pid_boost_req pid_boost_req;
	int is_req_valid = 0;
	u16 boost_pid_cnt = RMNET_CORE_GENL_MAX_PIDS;
	u16 i = 0;

	rm_err("CORE_GNL: %s", __func__);

	if (!info) {
		rm_err("%s", "CORE_GNL: error - info is null");
		return RMNET_GENL_FAILURE;
	}

	na = info->attrs[RMNET_CORE_GENL_ATTR_PID_BOOST];
	if (na) {
		if (nla_memcpy(&pid_boost_req, na, sizeof(pid_boost_req)) > 0) {
			is_req_valid = 1;
		} else {
			rm_err("CORE_GNL: nla_memcpy failed %d\n",
			       RMNET_CORE_GENL_ATTR_PID_BOOST);
			return RMNET_GENL_FAILURE;
		}
	} else {
		rm_err("CORE_GNL: no info->attrs %d\n",
		       RMNET_CORE_GENL_ATTR_PID_BOOST);
		return RMNET_GENL_FAILURE;
	}

	if (pid_boost_req.list_len < RMNET_CORE_GENL_MAX_PIDS)
		boost_pid_cnt = pid_boost_req.list_len;

	if (!pid_boost_req.valid)
		boost_pid_cnt = 0;

	for (i = 0; i < boost_pid_cnt; i++) {
		if (pid_boost_req.list[i].boost_enabled) {
			rmnet_boost_for_pid(pid_boost_req.list[i].pid, 1,
					    pid_boost_req.list[i].boost_period);
		}
	}

	return RMNET_GENL_SUCCESS;
}

/* register new rmnet core driver generic netlink family */
int rmnet_core_genl_init(void)
{
	int ret;

	ret = genl_register_family(&rmnet_core_genl_family);
	if (ret != 0) {
		rm_err("CORE_GNL: register family failed: %i", ret);
		genl_unregister_family(&rmnet_core_genl_family);
		return RMNET_GENL_FAILURE;
	}

	rm_err("CORE_GNL: successfully registered generic netlink family: %s",
	       RMNET_CORE_GENL_FAMILY_NAME);

	return RMNET_GENL_SUCCESS;
}

/* Unregister the generic netlink family */
int rmnet_core_genl_deinit(void)
{
	int ret;

	ret = genl_unregister_family(&rmnet_core_genl_family);
	if (ret != 0)
		rm_err("CORE_GNL: unregister family failed: %i\n", ret);

	return RMNET_GENL_SUCCESS;
}
