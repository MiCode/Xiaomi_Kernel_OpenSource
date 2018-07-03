/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "hab.h"
#include "hab_ghs.h"

static const char * const dt_gipc_path_name[] = {
	"testgipc1",
	"testgipc2",
	"testgipc3",
	"testgipc4",
	"testgipc5",
	"testgipc6",
	"testgipc7",
	"testgipc8",
	"testgipc9",
	"testgipc10",
	"testgipc11",
	"testgipc12",
	"testgipc13",
	"testgipc14",
	"testgipc15",
	"testgipc16",
	"testgipc17",
	"testgipc18",
	"testgipc19",
	"testgipc20",
	"testgipc21",
	"testgipc22",
};

static struct ghs_vmm_plugin_info_s {
	const char * const *dt_name;
	int curr;
	int probe_cnt;
} ghs_vmm_plugin_info = {
	dt_gipc_path_name,
	0,
	ARRAY_SIZE(dt_gipc_path_name),
};

static void ghs_irq_handler(void *cookie)
{
	struct physical_channel *pchan = cookie;
	struct ghs_vdev *dev =
		(struct ghs_vdev *) (pchan ? pchan->hyp_data : NULL);

	if (dev)
		tasklet_schedule(&dev->task);
}

/* static struct physical_channel *habhyp_commdev_alloc(int id) */
int habhyp_commdev_alloc(void **commdev, int is_be, char *name, int vmid_remote,
		struct hab_device *mmid_device)
{
	struct ghs_vdev *dev = NULL;
	struct physical_channel *pchan = NULL;
	struct physical_channel **ppchan = (struct physical_channel **)commdev;
	int ret = 0;

	if (ghs_vmm_plugin_info.curr > ghs_vmm_plugin_info.probe_cnt) {
		pr_err("too many commdev alloc %d, supported is %d\n",
			ghs_vmm_plugin_info.curr,
			ghs_vmm_plugin_info.probe_cnt);
		ret = -ENOENT;
		goto err;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		pr_err("allocate struct ghs_vdev failed %zu bytes on pchan %s\n",
			sizeof(*dev), name);
		goto err;
	}

	memset(dev, 0, sizeof(*dev));
	spin_lock_init(&dev->io_lock);

	/*
	 * TODO: ExtractEndpoint is in ghs_comm.c because it blocks.
	 *	Extrace and Request should be in roughly the same spot
	 */
	if (is_be) {
		/* role is backend */
		dev->be = 1;
	} else {
		/* role is FE */
		struct device_node *gvh_dn;

		gvh_dn = of_find_node_by_path("/aliases");
		if (gvh_dn) {
			const char *ep_path = NULL;
			struct device_node *ep_dn;

			ret = of_property_read_string(gvh_dn,
			ghs_vmm_plugin_info.dt_name[ghs_vmm_plugin_info.curr],
			&ep_path);
			if (ret)
				pr_err("failed to read endpoint string ret %d\n",
					ret);
			of_node_put(gvh_dn);

			ep_dn = of_find_node_by_path(ep_path);
			if (ep_dn) {
				dev->endpoint = kgipc_endpoint_alloc(ep_dn);
				of_node_put(ep_dn);
				if (IS_ERR(dev->endpoint)) {
					ret = PTR_ERR(dev->endpoint);
					pr_err("KGIPC alloc failed id: %d, ret: %d\n",
					   ghs_vmm_plugin_info.curr, ret);
					goto err;
				} else {
					pr_debug("gipc ep found for %d\n",
						ghs_vmm_plugin_info.curr);
				}
			} else {
				pr_err("of_parse_phandle failed id: %d\n",
					   ghs_vmm_plugin_info.curr);
				ret = -ENOENT;
				goto err;
			}
		} else {
			pr_err("of_find_compatible_node failed id: %d\n",
				   ghs_vmm_plugin_info.curr);
			ret = -ENOENT;
			goto err;
		}
	}
	/* add pchan into the mmid_device list */
	pchan = hab_pchan_alloc(mmid_device, vmid_remote);
	if (!pchan) {
		pr_err("hab_pchan_alloc failed for %s, cnt %d\n",
			   mmid_device->name, mmid_device->pchan_cnt);
		ret = -ENOMEM;
		goto err;
	}
	pchan->closed = 0;
	pchan->hyp_data = (void *)dev;
	pchan->is_be = is_be;
	strlcpy(dev->name, name, sizeof(dev->name));
	*ppchan = pchan;
	dev->read_data = kmalloc(GIPC_RECV_BUFF_SIZE_BYTES, GFP_KERNEL);
	if (!dev->read_data) {
		ret = -ENOMEM;
		goto err;
	}

	tasklet_init(&dev->task, physical_channel_rx_dispatch,
		(unsigned long) pchan);

	ret = kgipc_endpoint_start_with_irq_callback(dev->endpoint,
		ghs_irq_handler,
		pchan);
	if (ret) {
		pr_err("irq alloc failed id: %d %s, ret: %d\n",
				ghs_vmm_plugin_info.curr, name, ret);
		goto err;
	} else
		pr_debug("ep irq handler started for %d %s, ret %d\n",
				ghs_vmm_plugin_info.curr, name, ret);
	/* this value could be more than devp total */
	ghs_vmm_plugin_info.curr++;
	return 0;
err:
	hab_pchan_put(pchan);
	kfree(dev);
	return ret;
}

int habhyp_commdev_dealloc(void *commdev)
{
	struct physical_channel *pchan = (struct physical_channel *)commdev;
	struct ghs_vdev *dev = pchan->hyp_data;

	kgipc_endpoint_free(dev->endpoint);
	kfree(dev->read_data);
	kfree(dev);

	if (get_refcnt(pchan->refcount) > 1) {
		pr_warn("potential leak pchan %s vchans %d refcnt %d\n",
			pchan->name, pchan->vcnt, get_refcnt(pchan->refcount));
	}
	hab_pchan_put(pchan);
	return 0;
}

void hab_hypervisor_unregister(void)
{
	pr_debug("total %d\n", hab_driver.ndevices);

	hab_hypervisor_unregister_common();

	ghs_vmm_plugin_info.curr = 0;
}

int hab_hypervisor_register(void)
{
	int ret = 0;

	hab_driver.b_server_dom = 0;

	return ret;
}
