/* Copyright (c) 2012, 2017-2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>
#include <linux/coresight.h>
#include <linux/cpumask.h>
#include <asm/smp_plat.h>
#include <linux/coresight-cti.h>

static int of_dev_node_match(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static struct device *
of_coresight_get_endpoint_device(struct device_node *endpoint)
{
	struct device *dev = NULL;

	/*
	 * If we have a non-configurable replicator, it will be found on the
	 * platform bus.
	 */
	dev = bus_find_device(&platform_bus_type, NULL,
			      endpoint, of_dev_node_match);
	if (dev)
		return dev;

	/*
	 * We have a configurable component - circle through the AMBA bus
	 * looking for the device that matches the endpoint node.
	 */
	return bus_find_device(&amba_bustype, NULL,
			       endpoint, of_dev_node_match);
}

static void of_coresight_get_ports(const struct device_node *node,
				   int *nr_inport, int *nr_outport)
{
	struct device_node *ep = NULL;
	int in = 0, out = 0;
	struct device_node *ports = NULL, *port = NULL;
	struct of_endpoint endpoint;

	ports = of_get_child_by_name(node, "ports");
	port = of_get_child_by_name(node, "port");

	if (!ports && !port)
		return;

	do {
		ep = of_graph_get_next_endpoint(node, ep);
		if (!ep)
			break;

		if (of_graph_parse_endpoint(ep, &endpoint))
			continue;

		if (of_property_read_bool(ep, "slave-mode"))
			in = (endpoint.port + 1 > in) ?
				endpoint.port + 1 : in;
		else
			out = (endpoint.port + 1) > out ?
				endpoint.port + 1 : out;

	} while (ep);

	*nr_inport = in;
	*nr_outport = out;
}

static int of_coresight_alloc_memory(struct device *dev,
			struct coresight_platform_data *pdata)
{
	/* List of output port on this component */
	pdata->outports = devm_kzalloc(dev, pdata->nr_outport *
				       sizeof(*pdata->outports),
				       GFP_KERNEL);
	if (!pdata->outports)
		return -ENOMEM;

	pdata->source_names = devm_kzalloc(dev, pdata->nr_outport *
					sizeof(*pdata->source_names),
					GFP_KERNEL);
	if (!pdata->source_names)
		return -ENOMEM;

	/* Children connected to this component via @outports */
	pdata->child_names = devm_kzalloc(dev, pdata->nr_outport *
					  sizeof(*pdata->child_names),
					  GFP_KERNEL);
	if (!pdata->child_names)
		return -ENOMEM;

	/* Port number on the child this component is connected to */
	pdata->child_ports = devm_kzalloc(dev, pdata->nr_outport *
					  sizeof(*pdata->child_ports),
					  GFP_KERNEL);
	if (!pdata->child_ports)
		return -ENOMEM;

	return 0;
}

int of_coresight_get_cpu(const struct device_node *node)
{
	int cpu;
	struct device_node *dn;

	dn = of_parse_phandle(node, "cpu", 0);
	/* Affinity defaults to invalid */
	if (!dn)
		return -ENODEV;
	cpu = of_cpu_node_to_id(dn);
	of_node_put(dn);

	return cpu;
}
EXPORT_SYMBOL_GPL(of_coresight_get_cpu);

static struct coresight_reg_clk *
of_coresight_get_reg_clk(struct device *dev, const struct device_node *node)
{
	struct coresight_reg_clk *reg_clk;
	const char *clk_name, *reg_name;
	int nr_reg, nr_clk, i, ret;

	nr_reg = of_property_count_strings(node, "qcom,proxy-regs");
	nr_clk = of_property_count_strings(node, "qcom,proxy-clks");
	if (!nr_reg && !nr_clk)
		return NULL;

	reg_clk = devm_kzalloc(dev, sizeof(*reg_clk), GFP_KERNEL);
	if (!reg_clk)
		return ERR_PTR(-ENOMEM);

	reg_clk->nr_reg = nr_reg;
	reg_clk->nr_clk = nr_clk;
	if (nr_reg > 0) {
		reg_clk->reg = devm_kzalloc(dev, nr_reg *
			sizeof(reg_clk->reg), GFP_KERNEL);
		if (!reg_clk->reg)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < nr_reg; i++) {
			ret = of_property_read_string_index(node,
				"qcom,proxy-regs", i, &reg_name);
			if (ret)
				return ERR_PTR(ret);
			reg_clk->reg[i] = devm_regulator_get(dev, reg_name);
			if (IS_ERR(reg_clk->reg[i]))
				return ERR_PTR(-EINVAL);
		}
	}
	if (nr_clk > 0) {
		reg_clk->clk = devm_kzalloc(dev, nr_clk *
			sizeof(reg_clk->clk), GFP_KERNEL);
		if (!reg_clk->clk)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < nr_clk; i++) {
			ret = of_property_read_string_index(node,
				"qcom,proxy-clks", i, &clk_name);
			if (ret)
				return ERR_PTR(ret);
			reg_clk->clk[i] = devm_clk_get(dev, clk_name);
			if (IS_ERR(reg_clk->clk[i]))
				return ERR_PTR(-EINVAL);
		}
	}
	return reg_clk;
}

/*
 * of_coresight_parse_endpoint : Parse the given output endpoint @ep
 * and fill the connection information in @pdata[@i].
 *
 * Parses the local port, remote device name and the remote port.
 *
 * Returns :
 *	 1	- If the parsing is successful and a connection record
 *		  was created for an output connection.
 *	 0	- If the parsing completed without any fatal errors.
 *	-Errno	- Fatal error, abort the scanning.
 */
static int of_coresight_parse_endpoint(struct device *dev,
				       struct device_node *ep,
				       struct coresight_platform_data *pdata,
				       int i)
{
	int ret = 0;
	struct of_endpoint endpoint, rendpoint;
	struct device_node *rparent = NULL;
	struct device_node *rep = NULL;
	struct device *rdev = NULL;
	struct device_node *sn = NULL;

	do {
		/* Parse the local port details */
		if (of_graph_parse_endpoint(ep, &endpoint))
			break;
		/*
		 * Get a handle on the remote endpoint and the device it is
		 * attached to.
		 */
		rep = of_graph_get_remote_endpoint(ep);
		if (!rep)
			break;
		rparent = of_graph_get_port_parent(rep);
		if (!rparent)
			break;
		if (of_graph_parse_endpoint(rep, &rendpoint))
			break;

		/* If the remote device is not available, defer probing */
		rdev = of_coresight_get_endpoint_device(rparent);
		if (!rdev) {
			ret = -EPROBE_DEFER;
			break;
		}

		pdata->outports[i] = endpoint.port;
		ret = of_property_read_string(rparent, "coresight-name",
					      &pdata->child_names[i]);
		if (ret)
			pdata->child_names[i] = devm_kstrdup(dev,
							     dev_name(rdev),
							     GFP_KERNEL);
		pdata->child_ports[i] = rendpoint.port;
		pdata->source_names[i] = NULL;
		sn = of_parse_phandle(ep, "source", 0);
		if (sn) {
			ret = of_property_read_string(sn, "coresight-name",
						&pdata->source_names[i]);
			of_node_put(sn);
		}
		/* Connection record updated */
		ret = 1;
	} while (0);

	if (rparent)
		of_node_put(rparent);
	if (rep)
		of_node_put(rep);
	if (rdev)
		put_device(rdev);

	return ret;
}

struct coresight_platform_data *
of_get_coresight_platform_data(struct device *dev,
			       const struct device_node *node)
{
	int i = 0, ret = 0;
	struct coresight_platform_data *pdata;
	struct device_node *ep = NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_string(node, "coresight-name", &pdata->name);
	if (ret) {
		/* Use device name as sysfs handle */
		pdata->name = dev_name(dev);
	}
	pdata->cpu = of_coresight_get_cpu(node);

	/* Get the number of input and output port for this component */
	of_coresight_get_ports(node, &pdata->nr_inport, &pdata->nr_outport);

	pdata->reg_clk = of_coresight_get_reg_clk(dev, node);
	if (IS_ERR(pdata->reg_clk))
		return (void *)(pdata->reg_clk);

	/* If there are no output connections, we are done */
	if (!pdata->nr_outport)
		return pdata;

	ret = of_coresight_alloc_memory(dev, pdata);
	if (ret)
		return ERR_PTR(ret);

	/* Iterate through each port to discover topology */
	while ((ep = of_graph_get_next_endpoint(node, ep))) {
		/*
		 * No need to deal with input ports, as processing the
		 * output ports connected to them will process the details.
		 */
		if (of_find_property(ep, "slave-mode", NULL))
			continue;

		ret = of_coresight_parse_endpoint(dev, ep, pdata, i);
		switch (ret) {
		case 1:
			i++;		/* Fall through */
		case 0:
			break;
		default:
			return ERR_PTR(ret);
		}
	}

	return pdata;
}
EXPORT_SYMBOL_GPL(of_get_coresight_platform_data);

struct coresight_cti_data *of_get_coresight_cti_data(
				struct device *dev, struct device_node *node)
{
	int i, ret;
	uint32_t ctis_len;
	struct device_node *child_node;
	struct coresight_cti_data *ctidata;

	ctidata = devm_kzalloc(dev, sizeof(*ctidata), GFP_KERNEL);
	if (!ctidata)
		return ERR_PTR(-ENOMEM);

	if (of_get_property(node, "coresight-ctis", &ctis_len))
		ctidata->nr_ctis = ctis_len/sizeof(uint32_t);
	else
		return ERR_PTR(-EINVAL);

	if (ctidata->nr_ctis) {
		ctidata->names = devm_kzalloc(dev, ctidata->nr_ctis *
					      sizeof(*ctidata->names),
					      GFP_KERNEL);
		if (!ctidata->names)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < ctidata->nr_ctis; i++) {
			child_node = of_parse_phandle(node, "coresight-ctis",
						      i);
			if (!child_node)
				return ERR_PTR(-EINVAL);

			ret = of_property_read_string(child_node,
						      "coresight-name",
						      &ctidata->names[i]);
			of_node_put(child_node);
			if (ret)
				return ERR_PTR(ret);
		}
	}
	return ctidata;
}
EXPORT_SYMBOL(of_get_coresight_cti_data);

int of_get_coresight_csr_name(struct device_node *node, const char **csr_name)
{
	int  ret;
	struct device_node *csr_node;

	csr_node = of_parse_phandle(node, "coresight-csr", 0);
	if (!csr_node)
		return -EINVAL;

	ret = of_property_read_string(csr_node, "coresight-name", csr_name);
	of_node_put(csr_node);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(of_get_coresight_csr_name);
