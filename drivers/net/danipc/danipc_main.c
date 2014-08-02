/*
	All files except if stated otherwise in the begining of the file
	are under the ISC license:
	----------------------------------------------------------------------

	Copyright (c) 2010-2012 Design Art Networks Ltd.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <net/arp.h>

#include "danipc_k.h"
#include "ipc_api.h"
#include "danipc_lowlevel.h"

struct net_device		*danipc_dev;

#define DANIPC_VERSION		"v1.0"

#define TIMER_INTERVAL		10


static void
danipc_timer(unsigned long data)
{
	struct net_device	*dev = (struct net_device *)data;
	struct danipc_priv	*priv = netdev_priv(dev);

	danipc_poll(dev);

	mod_timer(&priv->timer, jiffies + TIMER_INTERVAL);
}


static void danipc_init_timer(struct net_device *dev, struct danipc_priv *priv)
{
	init_timer(&priv->timer);
	priv->timer.expires =	jiffies +  TIMER_INTERVAL;
	priv->timer.data =	(unsigned long)dev;
	priv->timer.function =	danipc_timer;
	add_timer(&priv->timer);
}

static int danipc_open(struct net_device *dev)
{
	struct danipc_priv	*priv = netdev_priv(dev);
	int			rc;

	danipc_init_irq(dev, priv);
	rc = request_irq(dev->irq, danipc_interrupt, 0, dev->name, dev);
	if (rc == 0) {
		danipc_init_timer(dev, priv);
		tasklet_init(&priv->rx_task, high_prio_rx, (unsigned long)dev);

		netif_start_queue(dev);
	}
	return rc;
}

static int danipc_close(struct net_device *dev)
{
	struct danipc_priv	*priv = netdev_priv(dev);

	netif_stop_queue(dev);

	tasklet_kill(&priv->rx_task);
	del_timer_sync(&priv->timer);
	free_irq(dev->irq, dev);
	return 0;
}

static int danipc_set_mac_addr(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	if (!(dev->priv_flags & IFF_LIVE_ADDR_CHANGE) && netif_running(dev))
		return -EBUSY;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	return 0;
}

static const struct net_device_ops danipc_netdev_ops = {
	.ndo_open		= danipc_open,
	.ndo_stop		= danipc_close,
	.ndo_start_xmit		= danipc_hard_start_xmit,
	.ndo_do_ioctl		= danipc_ioctl,
	.ndo_change_mtu		= danipc_change_mtu,
	.ndo_set_mac_address	= danipc_set_mac_addr,
};

static void danipc_setup(struct net_device *dev)
{
	dev->netdev_ops         = &danipc_netdev_ops;

	dev->type		= ARPHRD_VOID;
	dev->hard_header_len    = sizeof(struct ipc_msg_hdr);
	dev->addr_len           = sizeof(danipc_addr_t);
	dev->tx_queue_len       = 1000;

	/* New-style flags. */
	dev->flags              = IFF_NOARP;
}

static void __init
danipc_dev_priv_init(struct net_device *dev)
{
	struct danipc_priv	*priv = netdev_priv(dev);
	mutex_init(&priv->lock);
}

static void
danipc_dev_priv_cleanup(struct net_device *dev)
{
	struct danipc_priv	*priv = netdev_priv(dev);
	mutex_destroy(&priv->lock);
}

static int danipc_remove(struct platform_device *pdev)
{
	(void)pdev;
	if (danipc_dev) {
		if (danipc_dev->reg_state == NETREG_REGISTERED) {
			danipc_dev_priv_cleanup(danipc_dev);
			unregister_netdev(danipc_dev);
		}
		free_netdev(danipc_dev);
	}
	danipc_ll_cleanup();

	pr_info("DANIPC driver " DANIPC_VERSION " unregistered.\n");
	return 0;
}

/* Our vision of L2 header: it is of type struct danipc_pair
 * it is stored at address skb->cb[HADDR_CB_OFFSET].
 */

static int danipc_header_parse(const struct sk_buff *skb, unsigned char *haddr)
{
	struct danipc_pair	*pair = (struct danipc_pair *)
						&(skb->cb[HADDR_CB_OFFSET]);
	memcpy(haddr, &pair->src, sizeof(danipc_addr_t));
	return sizeof(danipc_addr_t);
}

int danipc_header(struct sk_buff *skb, struct net_device *dev,
			unsigned short type,
			const void *daddr, const void *saddr, unsigned len)
{
	struct danipc_pair	*pair = (struct danipc_pair *)
						&(skb->cb[HADDR_CB_OFFSET]);
	const uint8_t	*addr = daddr;

	pair->src = COOKIE_TO_AGENTID(type);
	pair->prio = COOKIE_TO_PRIO(type);
	if (addr)
		pair->dst = *addr;
	return 0;
}

static const struct header_ops danipc_header_ops ____cacheline_aligned = {
	.create		= danipc_header,
	.parse		= danipc_header_parse,
};


static int parse_resources(struct platform_device *pdev,
				struct danipc_priv *priv)
{
	const char		*regs[PLATFORM_MAX_NUM_OF_NODES] = {
		"cpu0_ipc", "cpu1_ipc", "cpu2_ipc", "cpu3_ipc",
		"dsp0_ipc", "dsp1_ipc", "dsp2_ipc", NULL,
		"krait_ipc", "qdsp6_0_ipc", "qdsp6_1_ipc", "qdsp6_2_ipc",
		"qdsp6_3_ipc", NULL, NULL, NULL
	};
	const char		*resource[RESOURCE_NUM] = {
		"ipc_bufs", "agent_table", "krait_ipc_intr_en"
	};
	const char		*shm_sizes[PLATFORM_MAX_NUM_OF_NODES] = {
		"qcom,cpu0-shm-size", "qcom,cpu1-shm-size",
		"qcom,cpu2-shm-size", "qcom,cpu3-shm-size",
		"qcom,dsp0-shm-size", "qcom,dsp1-shm-size",
		"qcom,dsp2-shm-size", NULL, "qcom,krait-shm-size",
		"qcom,qdsp6-0-shm-size", "qcom,qdsp6-1-shm-size",
		"qcom,qdsp6-2-shm-size", "qcom,qdsp6-3-shm-size",
		NULL, NULL, NULL
	};
	struct device_node	*node = pdev->dev.of_node;
	bool			parse_err = false;
	int			rc = -ENODEV;

	if (node) {
		struct resource	*res;
		int		shm_size;
		int		r;
		priv->irq = irq_of_parse_and_map(node, 0);
		if (!priv->irq || priv->irq == NO_IRQ) {
			pr_err("cannot get IRQ from DT\n");
			parse_err = true;
		}

		for (r = 0; r < RESOURCE_NUM && !parse_err; r++) {
			res = platform_get_resource_byname(pdev,
								IORESOURCE_MEM,
								resource[r]);
			if (res) {
				priv->res_start[r] = res->start;
				priv->res_len[r] = resource_size(res);
			} else {
				pr_err("cannot get resource %s\n", resource[r]);
				parse_err = true;
			}
		}

		for (r = 0; r < PLATFORM_MAX_NUM_OF_NODES && !parse_err; r++) {
			if (!regs[r])
				continue;
			res = platform_get_resource_byname(pdev,
								IORESOURCE_MEM,
								regs[r]);
			if (res) {
				ipc_regs_phys[r] = res->start;
				ipc_regs_len[r] = resource_size(res);
			} else {
				pr_err("cannot get resource %s\n", regs[r]);
				parse_err = true;
			}

			if (of_property_read_u32((&pdev->dev)->of_node,
					shm_sizes[r],
					&shm_size))
				ipc_shared_mem_sizes[r] = 0;
			else
				ipc_shared_mem_sizes[r] = shm_size;
		}

		rc = (!parse_err) ? 0 : -ENOMEM;
	}

	return rc;
}

static int danipc_probe(struct platform_device *pdev)
{
	struct net_device	*dev = alloc_netdev(sizeof(struct danipc_priv),
					 "danipc", danipc_setup);
	int			rc;

	if (dev) {
		struct danipc_priv *priv = netdev_priv(dev);
		danipc_dev = dev;
		strlcpy(dev->name, "danipc", sizeof(dev->name));
		dev->header_ops		= &danipc_header_ops;

		rc = parse_resources(pdev, priv);
		if (rc == 0) {
			rc = danipc_ll_init(priv);
			if (rc == 0) {
				dev->irq = priv->irq;
				dev->dev_addr[0] = LOCAL_IPC_ID;

				rc = register_netdev(dev);
				if (rc == 0) {
					danipc_dev_priv_init(dev);
					pr_info("DANIPC driver " DANIPC_VERSION
						" registered.\n");
				} else
					pr_err("%s: register_netdev failed\n",
						__func__);
			} else
				pr_err("%s: cannot init DAN IPC\n", __func__);
		} else
			pr_err("%s: cannot parse resources\n", __func__);

	} else
		rc = -ENOMEM;

	if (rc)
		danipc_remove(pdev);

	return rc;
}

static struct of_device_id danipc_ids[] = {
	{
		.compatible = "qcom,danipc",
	},
	{}
};

static struct platform_driver danipc_platform_driver = {
	.probe   = danipc_probe,
	.remove  = danipc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name	= "danipc",
		.of_match_table = danipc_ids,
	},
};

static int __init danipc_init_module(void)
{
	return platform_driver_register(&danipc_platform_driver);
}

static void __exit danipc_exit_module(void)
{
	platform_driver_unregister(&danipc_platform_driver);
}

module_init(danipc_init_module);
module_exit(danipc_exit_module);
