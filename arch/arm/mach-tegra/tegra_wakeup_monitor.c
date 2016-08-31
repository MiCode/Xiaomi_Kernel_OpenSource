/*
 * arch/arm/mach-tegra/tegra_wakeup_monitor.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/irqchip/tegra.h>

#include <net/ip.h>
#include <linux/netfilter_ipv4.h>
#include <net/ipv6.h>
#include <linux/proc_fs.h>

#include <mach/tegra_wakeup_monitor.h>


#define MAX_PACKET_NUM 6
#define MONITOR_HTABLE_SIZE 256
#define TWM_TCP_IPV4 1
#define TWM_UDP_IPV4 2
#define TWM_TCP_IPV6 3
#define TWM_UDP_IPV6 4
#define CMDLINE_LENGTH 80

struct packet_info {
	struct hlist_nulls_node node;
	unsigned short		dport;
	atomic_t		*wakeup;
	atomic_t		unmonitored;
	unsigned int		valid_counter;
};

struct program_info {
	struct hlist_nulls_node	node;
	unsigned int		hash;
	char program[CMDLINE_LENGTH];
	atomic_t		wakeup;
	unsigned int		valid_counter;
};

struct twm_hslot {
	struct hlist_nulls_head	head;
	int			count;
	spinlock_t		lock;
};

struct tegra_wakeup_monitor {
		struct tegra_wakeup_monitor_platform_data *pdata;
		struct notifier_block pm_notifier;
		struct platform_device *pdev;
		bool monitor_enable;
		bool nf_enable;
		bool am_enable;
		int wakeup_source;
		struct completion suspend_prepare_done;
};

struct _monitor_table {
	struct twm_hslot		*program_hash;
	struct twm_hslot		*tcp_hash;
	struct twm_hslot		*udp_hash;
	unsigned short			mask;
	unsigned int			valid_counter;
	struct tegra_wakeup_monitor	*twm;
};

static struct _monitor_table monitor_table;

static bool nf_monitor;
static bool nf_valid_flag;
static int nf_counter;

static unsigned int string_hash(const char *str)
{
	unsigned int i, hash = 0;
	for (i = 0; str[i] && i < CMDLINE_LENGTH; i++)
		hash = 31*hash + str[i];

	return hash;
}

static ssize_t show_monitor_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", twm->monitor_enable ? 1 : 0);
}

static ssize_t store_monitor_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int enable;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &enable) != 1 || enable < 0 || enable > 1)
		return -EINVAL;

	dev_info(dev, "monitor enable = %d\n", enable);
	twm->monitor_enable = enable;

	return count;
}

/* MUST hold hslot->lock when call this func */
static struct program_info *get_program_info(struct device *dev,
			char *program,
			struct twm_hslot *hslot,
			bool auto_alloc)
{
	struct program_info *cmd_info;
	struct hlist_nulls_node *node;
	if (hslot->count == 0)
		goto alloc;

	hlist_nulls_for_each_entry(cmd_info, node, &hslot->head, node)
		if (!strncmp(cmd_info->program, program, CMDLINE_LENGTH))
			return cmd_info;
alloc:
	if (!auto_alloc)
		return NULL;

	cmd_info = devm_kzalloc(dev, sizeof(*cmd_info), GFP_ATOMIC);
	if (!cmd_info) {
		dev_err(dev, "could not allocate a program_info");
		return NULL;
	}

	strncpy(cmd_info->program, program, CMDLINE_LENGTH);
	cmd_info->hash = string_hash(program);
	atomic_set(&cmd_info->wakeup, 0);
	cmd_info->valid_counter = monitor_table.valid_counter;

	hlist_nulls_add_head_rcu(&cmd_info->node, &hslot->head);
	hslot->count++;
	return cmd_info;
}

/* if the wakeup monitor is enabled, it will receive a command before suspend */
static ssize_t store_cmd(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (strncmp(buf, "0", 1))
		return -EINVAL;

	dev_info(dev, "get done cmd\n");
	complete(&twm->suspend_prepare_done);

	return count;
}

static int tegra_wakeup_monitor_pm_notifier(struct notifier_block *notifier,
				   unsigned long pm_event, void *unused)
{
	struct tegra_wakeup_monitor *twm =
	    container_of(notifier, struct tegra_wakeup_monitor, pm_notifier);
	char *envp[2];
	unsigned long const timeout =
			msecs_to_jiffies(TEGRA_WAKEUP_MONITOR_CMD_TIMEOUT_MS);

	envp[1] = NULL;
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (twm->monitor_enable) {
			dev_info(&twm->pdev->dev, "enter suspend_prepare\n");
			switch (twm->wakeup_source) {
			case TEGRA_WAKEUP_SOURCE_WIFI:
				envp[0] = TEGRA_SUSPEND_PREPARE_UEVENT_WIFI;
				break;
			default:
				envp[0] = TEGRA_SUSPEND_PREPARE_UEVENT_OTHERS;
			}
			/* If we have more comletion, clean it */
			if (try_wait_for_completion(&twm->suspend_prepare_done))
				dev_warn(&twm->pdev->dev,
					"completion is not empty\n");
			/* send out a uevent to boardcast suspend prepare */
			kobject_uevent_env(&twm->pdev->dev.kobj, KOBJ_CHANGE,
						envp);
			/* clean the wakeup source flag */
			twm->wakeup_source = TEGRA_WAKEUP_SOURCE_OTHERS;

			/* waiting for cmd feedback */
			if (wait_for_completion_timeout(
				&twm->suspend_prepare_done, timeout) == 0)
				dev_err(&twm->pdev->dev, "cmd time out\n");
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		if (twm->monitor_enable) {
			dev_info(&twm->pdev->dev, "enter post_suspend\n");
			envp[0] = TEGRA_POST_SUSPEND_UEVENT;
			/* send out a uevent to boardcast post suspend*/
			kobject_uevent_env(&twm->pdev->dev.kobj, KOBJ_CHANGE,
						envp);
		}
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

/* MUST hold hslot->lock when call this func */
static struct packet_info *get_packet_info(struct device *dev,
			unsigned short port,
			struct twm_hslot *hslot,
			bool auto_alloc)
{
	struct packet_info *packet_info;
	struct hlist_nulls_node *node;
	if (hslot->count == 0)
		goto alloc;

	hlist_nulls_for_each_entry(packet_info, node, &hslot->head, node)
		if (packet_info->dport == port)
			return packet_info;

alloc:
	if (!auto_alloc)
		return NULL;

	packet_info = devm_kzalloc(dev, sizeof(*packet_info), GFP_ATOMIC);
	if (!packet_info) {
		dev_err(dev, "could not allocate a packet_info");
		return NULL;
	}

	packet_info->dport = port;

	hlist_nulls_add_head_rcu(&packet_info->node, &hslot->head);
	hslot->count++;
	return packet_info;
}

/* Note: this function must be SMP safe */
static unsigned int twm_nf_hook(unsigned int hook,
			struct sk_buff *skb,
			const struct net_device *indev,
			const struct net_device *outdev,
			int (*okfn) (struct sk_buff *))
{
	struct tcphdr *ptcphdr, _tcph;
	struct udphdr *pudphdr, _udph;
	struct timeval tv;
	unsigned short sport;
	unsigned short dport;
	unsigned char protocol;
	__le16 fragoff;
	int offset;
	struct ipv6hdr _ipv6h, *ip6;
	struct packet_info *packet_info;
	struct twm_hslot *hslot_table, *hslot;
	struct tegra_wakeup_monitor *twm = monitor_table.twm;

	if (nf_monitor == false)
		return NF_ACCEPT;

	if (!(skb) || !(ip_hdr(skb))) {
		dev_err(&twm->pdev->dev, "unexpected skb");
		return NF_ACCEPT;
	}

	if (nf_counter == 0)
		dev_dbg(&twm->pdev->dev, "a new begin of monitoring");

	if (nf_counter++ >= MAX_PACKET_NUM)
		nf_monitor = false;

	if (ip_hdr(skb)->version == 4) {
		protocol = ip_hdr(skb)->protocol;
		if (protocol == IPPROTO_TCP) {
			ptcphdr = (struct tcphdr *)
				((char *)ip_hdr(skb) + (ip_hdr(skb)->ihl << 2));
			sport = ntohs(ptcphdr->source);
			dport = ntohs(ptcphdr->dest);
			hslot_table = monitor_table.tcp_hash;
		} else if (protocol == IPPROTO_UDP) {
			pudphdr = (struct udphdr *)
				((char *)ip_hdr(skb) + (ip_hdr(skb)->ihl << 2));
			sport = ntohs(pudphdr->source);
			dport = ntohs(pudphdr->dest);
			hslot_table = monitor_table.udp_hash;
		} else {
			dev_dbg(&twm->pdev->dev, "unexpected transport layer trpacket");
			return NF_ACCEPT;
		}
	} else if (ip_hdr(skb)->version == 6) {
		offset = skb_network_offset(skb);
		ip6 = skb_header_pointer(skb, offset,
			sizeof(struct ipv6hdr), &_ipv6h);
		if (ip6 == NULL)
			return NF_ACCEPT;

		protocol = ip6->nexthdr;
		offset += sizeof(struct ipv6hdr);
		offset = ipv6_skip_exthdr(skb, offset, &protocol, &fragoff);
		if (offset < 0)
			return NF_ACCEPT;

		switch (protocol) {
		case IPPROTO_TCP: {
			ptcphdr = skb_header_pointer(skb, offset,
				sizeof(struct tcphdr), &_tcph);
			if (ptcphdr == NULL)
				return NF_ACCEPT;

			sport = ntohs(ptcphdr->source);
			dport = ntohs(ptcphdr->dest);
			hslot_table = monitor_table.tcp_hash;
			break;
		}
		case IPPROTO_UDP: {
			pudphdr = skb_header_pointer(skb, offset,
				sizeof(struct udphdr), &_udph);
			if (pudphdr == NULL)
				return NF_ACCEPT;

			sport = ntohs(pudphdr->source);
			dport = ntohs(pudphdr->dest);
			hslot_table = monitor_table.udp_hash;
			break;
		}
		default:
			dev_dbg(&twm->pdev->dev, "unexpected transport layer packet");
			return NF_ACCEPT;
		}

	} else {
		dev_dbg(&twm->pdev->dev, "unexpected not-IPV4/IPV6 packet");
		return NF_ACCEPT;
	}

	do_gettimeofday(&tv);

	if (nf_valid_flag == true && nf_counter == 1) {
		hslot = &hslot_table[dport & monitor_table.mask];
		spin_lock_bh(&hslot->lock);
		packet_info = get_packet_info(&twm->pdev->dev,
						dport, hslot, true);
		if (packet_info == NULL) {
			dev_err(&twm->pdev->dev,
				"%s: cannot get a packet_info", __func__);
			spin_unlock_bh(&hslot->lock);
			return NF_ACCEPT;
		} else if (packet_info->wakeup) {
			atomic_inc(packet_info->wakeup);
		} else {
			atomic_inc(&packet_info->unmonitored);
			packet_info->valid_counter =
					monitor_table.valid_counter;
		}

		spin_unlock_bh(&hslot->lock);
	}

	dev_dbg(&twm->pdev->dev, "%d packet received from %s",
		nf_counter, indev->name);
	dev_dbg(&twm->pdev->dev,
		"%s ip:%pI4 port:%u -> ip:%pI4 port:%u at time:%u,%u",
		(protocol == IPPROTO_TCP) ? "TCP" : "UDP",
		&ip_hdr(skb)->saddr, sport, &ip_hdr(skb)->daddr, dport,
		(unsigned int)tv.tv_sec, (unsigned int)tv.tv_usec);

	return NF_ACCEPT;
}

static struct nf_hook_ops twm_nf_ops __read_mostly = {
	.hook		= twm_nf_hook,
	.pf		= PF_INET,
	.hooknum	= NF_INET_PRE_ROUTING,
	.priority	= NF_IP_PRI_MANGLE,
};

static ssize_t show_nf_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", twm->nf_enable ? 1 : 0);
}

static ssize_t store_nf_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int err;
	int enable;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &enable) != 1 || enable < 0 || enable > 1)
		return -EINVAL;

	if (enable == 1 && twm->nf_enable == false) {
		err = nf_register_hook(&twm_nf_ops);
		if (err < 0) {
			dev_err(dev, "hook registration error\n");
			return err;
		}
	} else if (enable == 0 && twm->nf_enable == true) {
		nf_unregister_hook(&twm_nf_ops);
	}

	twm->nf_enable = enable;
	dev_info(dev, "netfilter enable = %d\n", enable);

	return count;
}

static ssize_t show_am_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", twm->am_enable ? 1 : 0);
}

static ssize_t store_am_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int enable;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &enable) != 1 || enable < 0 || enable > 1)
		return -EINVAL;

	dev_info(dev, "am_enable = %d\n", enable);
	twm->am_enable = enable;

	return count;
}

static ssize_t store_init_ports(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	char *program = NULL;
	unsigned int proto, port, off, hash;
	struct program_info *cmd_info;
	struct packet_info *packet_info;
	struct twm_hslot *program_hslot_table, *pkt_hslot_table;
	struct twm_hslot *program_hslot, *pkt_hslot;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (!twm || !monitor_table.program_hash) {
		dev_err(dev, "monitor_table is not initialized!");
		return 0;
	}

	off = strlen(buf);
	if (off >= PAGE_SIZE) {
		dev_err(dev, "buffer size is over %lu!", PAGE_SIZE);
		return 0;
	}

	program = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (program == NULL) {
		dev_err(dev, "can't not alloate buffer!");
		return 0;
	}

	monitor_table.valid_counter++;
	program_hslot_table = monitor_table.program_hash;
	while (3 == sscanf(buf, "%s %u %u;%n", program, &proto, &port, &off)) {
		if (off == 0)
			break;
		buf += off;

		switch (proto) {
		case TWM_TCP_IPV4:
		case TWM_TCP_IPV6:
			pkt_hslot_table = monitor_table.tcp_hash;
			break;
		case TWM_UDP_IPV4:
		case TWM_UDP_IPV6:
			pkt_hslot_table = monitor_table.udp_hash;
			break;
		default:
			dev_err(dev, "%s: invalid proto type", __func__);
			continue;
		}

		if (port > 0xffff) {
			dev_err(dev, "%s: invalid port number", __func__);
			continue;
		}

		hash = string_hash(program);
		program_hslot = &program_hslot_table[hash & monitor_table.mask];
		spin_lock_bh(&program_hslot->lock);
		cmd_info = get_program_info(dev, program, program_hslot, true);
		if (cmd_info == NULL) {
			dev_err(dev, "%s: cannot get a program_info", __func__);
			spin_unlock_bh(&program_hslot->lock);
			continue;
		}
		cmd_info->hash = hash;
		atomic_set(&cmd_info->wakeup, 0);
		cmd_info->valid_counter = monitor_table.valid_counter;

		pkt_hslot = &pkt_hslot_table[port & monitor_table.mask];
		spin_lock_bh(&pkt_hslot->lock);
		packet_info = get_packet_info(dev, port, pkt_hslot, true);
		if (packet_info == NULL) {
			dev_err(dev, "%s: cannot get a packet_info", __func__);
			spin_unlock_bh(&pkt_hslot->lock);
			spin_unlock_bh(&program_hslot->lock);
			continue;
		}
		packet_info->dport = port;
		packet_info->wakeup = &cmd_info->wakeup;
		atomic_set(&packet_info->unmonitored, 0);
		packet_info->valid_counter = monitor_table.valid_counter;

		spin_unlock_bh(&pkt_hslot->lock);
		spin_unlock_bh(&program_hslot->lock);
	}

	kfree(program);
	return count;
}

static ssize_t store_add_ports(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	char *program = NULL;
	unsigned int proto, port, off, hash;
	struct program_info *cmd_info;
	struct packet_info *packet_info;
	struct twm_hslot *program_hslot_table, *pkt_hslot_table;
	struct twm_hslot *program_hslot, *pkt_hslot;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (!twm || !monitor_table.program_hash) {
		dev_err(dev, "monitor_table is not initialized!");
		return 0;
	}

	off = strlen(buf);
	if (off >= PAGE_SIZE) {
		dev_err(dev, "buffer size is over %lu!", PAGE_SIZE);
		return 0;
	}

	program = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (program == NULL) {
		dev_err(dev, "can't not alloate buffer!");
		return 0;
	}

	program_hslot_table = monitor_table.program_hash;
	while (3 == sscanf(buf, "%s %u %u;%n", program, &proto, &port, &off)) {
		if (off == 0)
			break;
		buf += off;

		switch (proto) {
		case TWM_TCP_IPV4:
		case TWM_TCP_IPV6:
			pkt_hslot_table = monitor_table.tcp_hash;
			break;
		case TWM_UDP_IPV4:
		case TWM_UDP_IPV6:
			pkt_hslot_table = monitor_table.udp_hash;
			break;
		default:
			dev_err(dev, "%s: invalid proto type", __func__);
			continue;
		}

		if (port > 0xffff) {
			dev_err(dev, "%s: invalid port number", __func__);
			continue;
		}

		hash = string_hash(program);
		program_hslot = &program_hslot_table[hash & monitor_table.mask];
		spin_lock_bh(&program_hslot->lock);
		cmd_info = get_program_info(dev, program, program_hslot, true);
		if (cmd_info == NULL) {
			dev_err(dev, "%s: cannot get a program_info", __func__);
			spin_unlock_bh(&program_hslot->lock);
			continue;
		}

		pkt_hslot = &pkt_hslot_table[port & monitor_table.mask];
		spin_lock_bh(&pkt_hslot->lock);
		packet_info = get_packet_info(dev, port, pkt_hslot, true);
		if (packet_info == NULL) {
			dev_err(dev, "%s: cannot get a packet_info", __func__);
			spin_unlock_bh(&pkt_hslot->lock);
			spin_unlock_bh(&program_hslot->lock);
			continue;
		}
		packet_info->dport = port;
		packet_info->wakeup = &cmd_info->wakeup;
		atomic_set(&packet_info->unmonitored, 0);
		packet_info->valid_counter = cmd_info->valid_counter;

		spin_unlock_bh(&pkt_hslot->lock);
		spin_unlock_bh(&program_hslot->lock);
	}

	kfree(program);
	return count;
}

static ssize_t store_del_ports(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	char *program;
	unsigned int proto, port, off, hash;
	struct program_info *cmd_info, *target_program_info;
	struct packet_info *packet_info;
	struct twm_hslot *program_hslot_table, *pkt_hslot_table;
	struct twm_hslot *program_hslot, *pkt_hslot;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (!twm || !monitor_table.program_hash) {
		dev_err(dev, "monitor_table is not initialized!");
		return 0;
	}

	off = strlen(buf);
	if (off >= PAGE_SIZE) {
		dev_err(dev, "buffer size is over %lu!", PAGE_SIZE);
		return 0;
	}

	program = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (program == NULL) {
		dev_err(dev, "can't not alloate buffer!");
		return 0;
	}

	program_hslot_table = monitor_table.program_hash;
	while (3 == sscanf(buf, "%s %u %u;%n",
		program, &proto, &port, &off)) {
		if (off == 0)
			break;
		buf += off;

		switch (proto) {
		case TWM_TCP_IPV4:
		case TWM_TCP_IPV6:
			pkt_hslot_table = monitor_table.tcp_hash;
			break;
		case TWM_UDP_IPV4:
		case TWM_UDP_IPV6:
			pkt_hslot_table = monitor_table.udp_hash;
			break;
		default:
			dev_err(dev, "%s: invalid proto type", __func__);
			continue;
		}

		if (port > 0xffff) {
			dev_err(dev, "%s: invalid port number", __func__);
			continue;
		}

		hash = string_hash(program);
		program_hslot = &program_hslot_table[hash & monitor_table.mask];
		spin_lock_bh(&program_hslot->lock);
		cmd_info = get_program_info(dev, program, program_hslot, false);
		if (cmd_info == NULL) {
			dev_err(dev, "%s: cannot get a program_info", __func__);
			spin_unlock_bh(&program_hslot->lock);
			continue;
		}

		pkt_hslot = &pkt_hslot_table[port & monitor_table.mask];
		spin_lock_bh(&pkt_hslot->lock);
		packet_info = get_packet_info(dev, port, pkt_hslot, false);
		if (packet_info == NULL) {
			dev_err(dev, "%s: cannot get a packet_info", __func__);
			spin_unlock_bh(&pkt_hslot->lock);
			spin_unlock_bh(&program_hslot->lock);
			continue;
		}
		target_program_info = container_of(packet_info->wakeup,
					struct program_info, wakeup);
		if (!strncmp(target_program_info->program,
			program, CMDLINE_LENGTH)) {

			hlist_nulls_del(&packet_info->node);
			devm_kfree(dev, packet_info);
			pkt_hslot->count--;
		}

		spin_unlock_bh(&pkt_hslot->lock);
		spin_unlock_bh(&program_hslot->lock);
	}

	kfree(program);
	return count;
}

static struct device_attribute twm_attrs[] = {
	__ATTR(monitor_enable, S_IRUSR | S_IWUSR,
		show_monitor_enable, store_monitor_enable),
	__ATTR(cmd, S_IWUSR, NULL, store_cmd),
	__ATTR(nf_enable, S_IRUSR | S_IWUSR, show_nf_enable, store_nf_enable),
	__ATTR(am_enable, S_IRUSR | S_IWUSR, show_am_enable, store_am_enable),
	__ATTR(init_ports, S_IWUSR, NULL, store_init_ports),
	__ATTR(add_ports, S_IWUSR, NULL, store_add_ports),
	__ATTR(del_ports, S_IWUSR, NULL, store_del_ports),
};

static inline int monitor_table_init(struct _monitor_table *table,
				struct tegra_wakeup_monitor *twm)
{
	unsigned int i;

	table->program_hash = devm_kzalloc(&twm->pdev->dev,
		MONITOR_HTABLE_SIZE *
		3 * sizeof(struct twm_hslot), GFP_KERNEL);

	if (!table->program_hash)
		return -ENOMEM;

	table->twm = twm;

	table->mask = MONITOR_HTABLE_SIZE - 1;
	table->tcp_hash = table->program_hash + (table->mask + 1);
	table->udp_hash = table->tcp_hash + (table->mask + 1);
	for (i = 0; i <= table->mask; i++) {
		INIT_HLIST_NULLS_HEAD(&table->program_hash[i].head, i);
		table->program_hash[i].count = 0;
		spin_lock_init(&table->program_hash[i].lock);
	}
	for (i = 0; i <= table->mask; i++) {
		INIT_HLIST_NULLS_HEAD(&table->tcp_hash[i].head, i);
		table->tcp_hash[i].count = 0;
		spin_lock_init(&table->tcp_hash[i].lock);
	}
	for (i = 0; i <= table->mask; i++) {
		INIT_HLIST_NULLS_HEAD(&table->udp_hash[i].head, i);
		table->udp_hash[i].count = 0;
		spin_lock_init(&table->udp_hash[i].lock);
	}

	return 0;
}

static int twm_offender_stat_show(struct seq_file *m, void *v)
{
	int i = 0;
	struct twm_hslot *hslot;
	struct program_info *cmd_info;
	struct packet_info *packet_info;
	struct hlist_nulls_node *node;
	struct device *dev;

	if (!monitor_table.twm || !monitor_table.program_hash) {
		seq_printf(m, "monitor table not initialized\n");
		return 0;
	}

	dev = &monitor_table.twm->pdev->dev;

	/* make sure the len does not exceed PAGE_SIZE */
	for (i = 0; i <= monitor_table.mask; i++) {
		hslot = &monitor_table.program_hash[i];
		if (hslot->count == 0)
			continue;

		spin_lock_bh(&hslot->lock);
		hlist_nulls_for_each_entry(cmd_info, node,
					&hslot->head, node) {
			if (cmd_info->valid_counter !=
					monitor_table.valid_counter) {
				hlist_nulls_del(&cmd_info->node);
				devm_kfree(dev, cmd_info);
				hslot->count--;
				continue;
			}
			seq_printf(m, "program %s, wakeup times %u\n",
				cmd_info->program,
				atomic_read(&cmd_info->wakeup));
		}
		spin_unlock_bh(&hslot->lock);
	}
	seq_printf(m, "Unmonitored wakeups\nTCP statistics:\n");
	for (i = 0; i <= monitor_table.mask; i++) {
		hslot = &monitor_table.tcp_hash[i];
		if (hslot->count == 0)
			continue;

		spin_lock_bh(&hslot->lock);
		hlist_nulls_for_each_entry(packet_info, node,
					&hslot->head, node) {
			if (packet_info->valid_counter !=
					monitor_table.valid_counter) {
				hlist_nulls_del(&packet_info->node);
				devm_kfree(dev, packet_info);
				hslot->count--;
				continue;
			}
			if (packet_info->wakeup)
				continue;
			seq_printf(m, "port %u, wakeup times %u\n",
				packet_info->dport,
				atomic_read(&packet_info->unmonitored));
		}
		spin_unlock_bh(&hslot->lock);
	}
	seq_printf(m, "UDP statistics:\n");
	for (i = 0; i <= monitor_table.mask; i++) {
		hslot = &monitor_table.udp_hash[i];
		if (hslot->count == 0)
			continue;

		spin_lock_bh(&hslot->lock);
		hlist_nulls_for_each_entry(packet_info, node,
					&hslot->head, node) {
			if (packet_info->valid_counter !=
					monitor_table.valid_counter) {
				hlist_nulls_del(&packet_info->node);
				devm_kfree(dev, packet_info);
				hslot->count--;
				continue;
			}
			if (packet_info->wakeup)
				continue;
			seq_printf(m, "port %u, wakeup times %u\n",
				packet_info->dport,
				atomic_read(&packet_info->unmonitored));
		}
		spin_unlock_bh(&hslot->lock);
	}

	return 0;
}

static int twm_offender_stat_open(struct inode *inode, struct file *file) {
	return single_open(file, twm_offender_stat_show, PDE_DATA(inode));
}

static const struct file_operations twm_offender_stat_fops = {
	.open		= twm_offender_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static inline int twm_init(struct tegra_wakeup_monitor *twm,
					struct platform_device *pdev)
{
	unsigned int i;
	struct tegra_wakeup_monitor_platform_data *pdata =
		pdev->dev.platform_data;
	int ret = 0;

	twm->pdata = pdata;
	twm->pdev  = pdev;
	twm->monitor_enable = false;
	twm->nf_enable = false;
	twm->wakeup_source = TEGRA_WAKEUP_SOURCE_OTHERS;
	twm->pm_notifier.notifier_call = tegra_wakeup_monitor_pm_notifier;
	init_completion(&(twm->suspend_prepare_done));

	ret = register_pm_notifier(&twm->pm_notifier);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm notifier registration error\n");
		return ret;
	}

	if (!proc_create("twm_offender_stat", 0, init_net.proc_net,
			&twm_offender_stat_fops)) {
		dev_err(&pdev->dev, "proc entry creation error\n");
		return -ENOMEM;
	}

	/* create sysfs node */
	for (i = 0; i < ARRAY_SIZE(twm_attrs); i++) {
		ret = device_create_file(&pdev->dev, &twm_attrs[i]);
		if (ret)
			goto error;
	}

	return ret;

error:
	remove_proc_entry("twm_offender_stat", init_net.proc_net);
	for (i = 0; i < ARRAY_SIZE(twm_attrs); i++)
		device_remove_file(&pdev->dev, &twm_attrs[i]);

	return ret;
}

static int tegra_wakeup_monitor_probe(struct platform_device *pdev)
{
	struct tegra_wakeup_monitor_platform_data *pdata =
	    pdev->dev.platform_data;
	struct tegra_wakeup_monitor *twm;
	int ret = 0;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		return -EINVAL;
	}

	twm = devm_kzalloc(&pdev->dev,
		sizeof(struct tegra_wakeup_monitor), GFP_KERNEL);
	if (!twm) {
		dev_dbg(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = twm_init(twm, pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to init twm\n");
		goto error;
	}

	ret = dev_set_drvdata(&pdev->dev, twm);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set driver data\n");
		goto error;
	}

	ret = monitor_table_init(&monitor_table, twm);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to init monitor table\n");
		goto error;
	}

	return 0;

error:
	devm_kfree(&pdev->dev, twm);
	return ret;
}

static int __exit tegra_wakeup_monitor_remove(struct platform_device *pdev)
{
	unsigned int i;
	struct tegra_wakeup_monitor *twm = platform_get_drvdata(pdev);

	unregister_pm_notifier(&twm->pm_notifier);
	nf_unregister_hook(&twm_nf_ops);

	for (i = 0; i < ARRAY_SIZE(twm_attrs); i++)
		device_remove_file(&pdev->dev, &twm_attrs[i]);
	remove_proc_entry("twm_offender_stat", init_net.proc_net);

	kfree(twm); /* is it needed? */
	return 0;
}

static int tegra_wakeup_monitor_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct tegra_wakeup_monitor *twm = platform_get_drvdata(pdev);
	if (twm->nf_enable) {
		nf_monitor = true;
		dev_info(&pdev->dev, "start netfilter monitoring\n");
	} else {
		nf_monitor = false;
	}

	nf_counter = 0;

	return 0;
}

static int tegra_wakeup_monitor_resume(struct platform_device *pdev)
{
	struct tegra_wakeup_monitor *twm = platform_get_drvdata(pdev);

	/* read and save wake status */
	u64 wake_status = tegra_read_pmc_wake_status();
	nf_valid_flag = false;
	if (twm->pdata->wifi_wakeup_source != -1) {
		if (wake_status & BIT(twm->pdata->wifi_wakeup_source)) {
			twm->wakeup_source = TEGRA_WAKEUP_SOURCE_WIFI;
			nf_valid_flag = true;
		} else if (wake_status & BIT(twm->pdata->rtc_wakeup_source)) {
			twm->wakeup_source = TEGRA_WAKEUP_SOURCE_RTC;
			if (twm->am_enable)
				set_rtc_wakeup_src(1);
		} else {
			twm->wakeup_source = TEGRA_WAKEUP_SOURCE_OTHERS;
		}
	} else {
		twm->wakeup_source = TEGRA_WAKEUP_SOURCE_OTHERS;
	}

	dev_info(&pdev->dev, "wakeup source = %d\n", twm->wakeup_source);
	return 0;
}


static struct platform_driver tegra_wakeup_monitor_driver = {
	.driver = {
		   .name = "tegra_wakeup_monitor",
		   .owner = THIS_MODULE,
		   },
	.probe = tegra_wakeup_monitor_probe,
	.remove = __exit_p(tegra_wakeup_monitor_remove),
#ifdef CONFIG_PM
	.suspend = tegra_wakeup_monitor_suspend,
	.resume = tegra_wakeup_monitor_resume,
#endif
};

static int __init tegra_wakeup_monitor_init(void)
{
	return platform_driver_register(&tegra_wakeup_monitor_driver);
}

subsys_initcall(tegra_wakeup_monitor_init);

static void __exit tegra_wakeup_monitor_exit(void)
{
	platform_driver_unregister(&tegra_wakeup_monitor_driver);
}

module_exit(tegra_wakeup_monitor_exit);

MODULE_DESCRIPTION("Tegra Wakeup Monitor driver");
MODULE_LICENSE("GPL");
