// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "cpufreq_hw_debug: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <soc/qcom/scm.h>

enum trace_type {
	XOR_PACKET,
	PERIODIC_PACKET,
};

enum clock_domain {
	CLKDOM0,
	CLKDOM1,
	CLKDOM2,
	CLKDOM3,

	CLK_DOMAIN_SIZE,
};

enum debug_trace_regs_data {
	GLOBAL_CTRL_OFFSET,
	CLKDOM_CTRL_OFFSET,
	PERIODIC_TIMER_CTRL_OFFSET,

	PERIODIC_TRACE_ENABLE_BIT,
	CPUFREQ_HW_TRACE_ENABLE_BIT,

	REG_PERF_STATE,
	REG_CYCLE_CNTR,
	REG_PSTATE_STATUS,

	REG_ARRAY_SIZE,
};

struct cpufreq_hwregs {
	void __iomem *base[REG_ARRAY_SIZE];
	int domain_cnt;
	struct dentry *debugfs_base;
};

struct cpufreq_register_data {
	char *name;
	u16 offset;
};

#define CLKDOM0_TRACE_PACKET_SHIFT	0
#define CLKDOM1_TRACE_PACKET_SHIFT	3
#define CLKDOM2_TRACE_PACKET_SHIFT	6
#define CLKDOM3_TRACE_PACKET_SHIFT	9
#define CLKDOM_TRACE_PACKET_WIDTH	2
#define MAX_DEBUG_BUF_LEN		50
#define MAX_PKT_SIZE			5
#define CLKDOMAIN_SET_VAL(val, packet_sel, shift) \
	((val & ~GENMASK(shift + CLKDOM_TRACE_PACKET_WIDTH, shift)) \
	| (packet_sel << shift))
#define CLKDOMAIN_CLEAR_VAL(val, packet_sel, shift) \
	((val & ~GENMASK(shift + CLKDOM_TRACE_PACKET_WIDTH, shift)))

static struct cpufreq_hwregs *hw_regs;
static char debug_buf[MAX_DEBUG_BUF_LEN];
static const u16 *offsets;

static const u16 cpufreq_qcom_std_data[REG_ARRAY_SIZE] = {
	[GLOBAL_CTRL_OFFSET]		= 0x10,
	[CLKDOM_CTRL_OFFSET]		= 0x14,
	[PERIODIC_TIMER_CTRL_OFFSET]	= 0x1C,
	[REG_PERF_STATE]		= 0x920,
	[REG_CYCLE_CNTR]		= 0x9c0,
	[REG_PSTATE_STATUS]		= 0x700,
	[CPUFREQ_HW_TRACE_ENABLE_BIT]	= 16,
	[PERIODIC_TRACE_ENABLE_BIT]	= 17,
};

static const u16 cpufreq_qcom_std_epss_data[REG_ARRAY_SIZE] = {
	[REG_PERF_STATE]		= 0x320,
	[REG_CYCLE_CNTR]		= 0x3c4,
	[REG_PSTATE_STATUS]		= 0x020,
};

static int clock_timer_set(void *data, u64 val)
{
	u32 base;
	int ret;

	if (!data)
		return -EINVAL;

	base = *((u32 *)data);

	ret = scm_io_write(base + offsets[PERIODIC_TIMER_CTRL_OFFSET], val);
	if (ret)
		pr_err("failed(0x%x) to set clk timer\n", ret);

	return ret;
}

static int clock_timer_get(void *data, u64 *val)
{
	u32 base;

	if (!data)
		return -EINVAL;

	base = *((u32 *)data);

	*val = scm_io_read(base + offsets[PERIODIC_TIMER_CTRL_OFFSET]);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(clock_timer_fops, clock_timer_get, clock_timer_set,
								"%llu\n");

static ssize_t trace_type_set(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	u32 regval, base;
	int ret;

	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	if (!file->private_data)
		return -EINVAL;

	base = *((u32 *)file->private_data);

	if (count <  MAX_DEBUG_BUF_LEN) {
		if (copy_from_user(debug_buf, (void __user *) buf,
						MAX_DEBUG_BUF_LEN))
			return -EFAULT;
	}
	debug_buf[count] = '\0';

	regval = scm_io_read(base + offsets[GLOBAL_CTRL_OFFSET]);
	if (!strcmp(debug_buf, "periodic\n")) {
		regval |= BIT(offsets[PERIODIC_TRACE_ENABLE_BIT]);
	} else if (!strcmp(debug_buf, "xor\n")) {
		regval &= ~BIT(offsets[PERIODIC_TRACE_ENABLE_BIT]);
	} else {
		pr_err("error, supported trace mode types: 'periodic' or 'xor'\n");
		return -EINVAL;
	}

	ret = scm_io_write(base + offsets[GLOBAL_CTRL_OFFSET], regval);
	if (ret)
		pr_err("failed(0x%x) to set cpufreq clk timer\n", ret);

	return count;
}

static ssize_t trace_type_get(struct file *file, char __user *buf, size_t count,
								loff_t *ppos)
{
	int len;
	u32 regval, base;

	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	if (!file->private_data)
		return -EINVAL;

	base = *((u32 *)file->private_data);

	regval = scm_io_read(base + offsets[GLOBAL_CTRL_OFFSET]);
	if (regval && offsets[PERIODIC_TRACE_ENABLE_BIT])
		len = snprintf(debug_buf, sizeof(debug_buf), "periodic\n");
	else
		len = snprintf(debug_buf, sizeof(debug_buf), "xor\n");

	return simple_read_from_buffer((void __user *) buf, count, ppos,
						(void *) debug_buf, len);
}

static int trace_type_open(struct inode *inode, struct file *file)
{
	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	file->private_data = inode->i_private;

	return 0;
}

static const struct file_operations clk_trace_type_fops = {
	.read = trace_type_get,
	.open = trace_type_open,
	.write = trace_type_set,
};

void __domain_packet_set(u32 *clkdom_regval, u32 *pktsel_regval, int clkdomain,
				int pktsel, int packet_sel_shift, int enable)
{
	if (!!enable) {
		*clkdom_regval |= BIT(clkdomain);
		*pktsel_regval = CLKDOMAIN_SET_VAL(*pktsel_regval, pktsel,
							packet_sel_shift);
	} else {
		*clkdom_regval &= ~BIT(clkdomain);
		*pktsel_regval &= CLKDOMAIN_CLEAR_VAL(*pktsel_regval, pktsel,
							packet_sel_shift);
	}
}

static ssize_t domain_packet_set(struct file *file,
			const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned int filled, clk_domain, packet_sel, enable = 1;
	u32 clkdom_regval, pktsel_regval, base;
	char buf[MAX_DEBUG_BUF_LEN];
	int ret;

	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	if (!file->private_data)
		return -EINVAL;

	base = *((u32 *)file->private_data);

	if (count <  MAX_DEBUG_BUF_LEN) {
		if (copy_from_user(buf, ubuf, count))
			return -EFAULT;
	}

	buf[count] = '\0';
	filled = sscanf(buf, "%u %u %u", &clk_domain, &packet_sel, &enable);
	if (clk_domain > CLK_DOMAIN_SIZE || packet_sel > MAX_PKT_SIZE) {
		pr_err("Clock domain and source selection not in range\n");
		return -EINVAL;
	}

	clkdom_regval = scm_io_read(base + offsets[GLOBAL_CTRL_OFFSET]);
	pktsel_regval = scm_io_read(base + offsets[CLKDOM_CTRL_OFFSET]);


	switch (clk_domain) {
	case CLKDOM0:
		__domain_packet_set(&clkdom_regval, &pktsel_regval, CLKDOM0,
				packet_sel, CLKDOM0_TRACE_PACKET_SHIFT, enable);
		break;
	case CLKDOM1:
		__domain_packet_set(&clkdom_regval, &pktsel_regval, CLKDOM1,
				packet_sel, CLKDOM1_TRACE_PACKET_SHIFT, enable);
		break;
	case CLKDOM2:
		__domain_packet_set(&clkdom_regval, &pktsel_regval, CLKDOM2,
				packet_sel, CLKDOM2_TRACE_PACKET_SHIFT, enable);
		break;
	case CLKDOM3:
		__domain_packet_set(&clkdom_regval, &pktsel_regval, CLKDOM3,
				packet_sel, CLKDOM3_TRACE_PACKET_SHIFT, enable);
		break;
	default:
		return -EINVAL;
	}

	ret = scm_io_write(base + offsets[GLOBAL_CTRL_OFFSET], clkdom_regval);
	if (ret)
		pr_err("failed(0x%x) to set cpufreq domain\n", ret);

	ret = scm_io_write(base + offsets[CLKDOM_CTRL_OFFSET], pktsel_regval);
	if (ret)
		pr_err("failed(0x%x) to set cpufreq trace packet\n", ret);

	return count;
}

static ssize_t domain_packet_get(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	u32 base, clkdom_regval, pktsel_regval;
	int len;

	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	if (!file->private_data)
		return -EINVAL;

	base = *((u32 *)file->private_data);

	clkdom_regval = scm_io_read(base + offsets[GLOBAL_CTRL_OFFSET]);
	pktsel_regval = scm_io_read(base + offsets[CLKDOM_CTRL_OFFSET]);

	len = snprintf(debug_buf, sizeof(debug_buf),
	"GLOBAL_TRACE_CTRL = 0x%x\nCLKDOM_TRACE_CTRL = 0x%x\n",
						clkdom_regval, pktsel_regval);

	return simple_read_from_buffer((void __user *) buf, count, ppos,
						(void *) debug_buf, len);
}

static int domain_packet_open(struct inode *inode, struct file *file)
{
	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	file->private_data = inode->i_private;

	return 0;
}

static const struct file_operations clock_domian_packet_set = {
	.write = domain_packet_set,
	.open = domain_packet_open,
	.read = domain_packet_get,
};

static int cpufreq_hw_trace_enable_set(void *data, u64 val)
{
	u32 regval, base;
	int ret;

	if (!data)
		return -EINVAL;

	base = *((u32 *)data);

	regval = scm_io_read(base + offsets[GLOBAL_CTRL_OFFSET]);
	if (!!val)
		regval |= BIT(offsets[CPUFREQ_HW_TRACE_ENABLE_BIT]);
	else
		regval &= ~BIT(offsets[CPUFREQ_HW_TRACE_ENABLE_BIT]);

	ret = scm_io_write(base + offsets[GLOBAL_CTRL_OFFSET], regval);
	if (ret)
		pr_err("failed(0x%x) to set cpufreq hw trace\n", ret);

	return ret;
}

static int cpufreq_hw_trace_enable_get(void *data, u64 *val)
{
	u32 regval, base;

	if (!data)
		return -EINVAL;

	base = *((u32 *)data);

	regval = scm_io_read(base + offsets[GLOBAL_CTRL_OFFSET]);
	*val = (regval & BIT(offsets[CPUFREQ_HW_TRACE_ENABLE_BIT])) >>
				offsets[CPUFREQ_HW_TRACE_ENABLE_BIT];

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(clock_trace_enable, cpufreq_hw_trace_enable_get,
					cpufreq_hw_trace_enable_set, "%llu\n");

static int print_cpufreq_hw_trace_regs(struct seq_file *s, void *unused)
{
	u32 base;
	int i;

	static struct cpufreq_register_data data[] = {
		 {"GLOBAL_TRACE_CTRL", GLOBAL_CTRL_OFFSET},
		 {"CLKDOM_TRACE_CTRL", CLKDOM_CTRL_OFFSET},
		 {"PERIODIC_TIMER_TIMER", PERIODIC_TIMER_CTRL_OFFSET},
	 };

	if (!s->private)
		return -EINVAL;

	base = *((u32 *)s->private);

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		seq_printf(s, "%25s: 0x%.8x\n", data[i].name,
				scm_io_read(base +  offsets[data[i].offset]));
	}

	return 0;
}

static int print_trace_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, print_cpufreq_hw_trace_regs, inode->i_private);
}

static const struct file_operations cpufreq_trace_register_fops = {
	.open = print_trace_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int print_cpufreq_hw_debug_regs(struct seq_file *s, void *unused)
{
	int i, j;
	u32 regval;

	static struct cpufreq_register_data data[] = {
		{"PERF_STATE_DESIRED", REG_PERF_STATE},
		{"CYCLE_CNTR_VAL", REG_CYCLE_CNTR},
		{"PSTATE_STATUS", REG_PSTATE_STATUS},
	};

	for (i = 0; i < hw_regs->domain_cnt; i++) {
		seq_printf(s, "FREQUENCY DOMAIN %d\n", i);
		for (j = 0; j < ARRAY_SIZE(data); j++) {
			regval = readl_relaxed(hw_regs->base[i] +
						offsets[data[j].offset]);
			seq_printf(s, "%25s: 0x%.8x\n", data[j].name, regval);
		}
	}

	return 0;
}

static int print_cpufreq_hw_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, print_cpufreq_hw_debug_regs, NULL);
}

static const struct file_operations cpufreq_debug_register_fops = {
	.open = print_cpufreq_hw_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int cpufreq_panic_callback(struct notifier_block *nfb,
					unsigned long event, void *unused)
{
	int i, j;
	u32 regval;

	static struct cpufreq_register_data data[] = {
		{"PERF_STATE_DESIRED", REG_PERF_STATE},
		{"CYCLE_CNTR_VAL", REG_CYCLE_CNTR},
		{"PSTATE_STATUS", REG_PSTATE_STATUS},
	};

	for (i = 0; i < hw_regs->domain_cnt; i++) {
		pr_err("FREQUENCY DOMAIN %d\n", i);
		for (j = 0; j < ARRAY_SIZE(data); j++) {
			regval = readl_relaxed(hw_regs->base[i] +
						offsets[data[j].offset]);
			pr_err("%25s: 0x%.8x\n", data[j].name, regval);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_panic_notifier = {
	.notifier_call = cpufreq_panic_callback,
	.priority = 1,
};

static int cpufreq_get_hwregs(struct platform_device *pdev)
{
	struct of_phandle_args args;
	struct property *prop;
	struct resource res;
	void __iomem *base;
	int i, ret;

	offsets = of_device_get_match_data(&pdev->dev);
	if (!offsets)
		return -EINVAL;

	hw_regs = devm_kzalloc(&pdev->dev, sizeof(*hw_regs), GFP_KERNEL);
	if (!hw_regs)
		return -ENOMEM;

	prop = of_find_property(pdev->dev.of_node, "qcom,freq-hw-domain", NULL);
	if (!prop)
		return -EINVAL;

	hw_regs->domain_cnt = prop->length / (2 * sizeof(prop->length));

	for (i = 0; i < hw_regs->domain_cnt; i++) {
		ret = of_parse_phandle_with_fixed_args(pdev->dev.of_node,
			"qcom,freq-hw-domain", 1, i, &args);
		of_node_put(pdev->dev.of_node);
		if (ret)
			return ret;

		ret = of_address_to_resource(args.np, args.args[0], &res);
		if (ret)
			return ret;

		base = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
		if (!base)
			return -ENOMEM;

		hw_regs->base[i] = base;
	}

	atomic_notifier_chain_register(&panic_notifier_list,
						&cpufreq_panic_notifier);

	return 0;
}

static int enable_cpufreq_hw_trace_debug(struct platform_device *pdev,
								bool is_secure)
{
	struct resource *res;
	void *base;
	int ret, debug_only, epss_debug_only;

	ret = cpufreq_get_hwregs(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to map cpufreq hw regs\n");
		return ret;
	}

	hw_regs->debugfs_base = debugfs_create_dir("qcom-cpufreq-hw", NULL);
	if (!hw_regs->debugfs_base) {
		dev_err(&pdev->dev, "Failed to create debugfs entry\n");
		return -ENODEV;
	}

	if (!debugfs_create_file("print_cpufreq_debug_regs", 0444,
		hw_regs->debugfs_base, NULL, &cpufreq_debug_register_fops))
		goto debugfs_fail;

	debug_only = of_device_is_compatible(pdev->dev.of_node,
				"qcom,cpufreq-hw-debug");
	epss_debug_only = of_device_is_compatible(pdev->dev.of_node,
				"qcom,cpufreq-hw-epss-debug");

	if (!is_secure || epss_debug_only || debug_only)
		return 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "domain-top");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get domain-top register base\n");
		return 0;
	}

	base = &(res->start);

	if (!debugfs_create_file("print_cpufreq_trace_regs", 0444,
		hw_regs->debugfs_base, base, &cpufreq_trace_register_fops))
		goto debugfs_fail;

	if (!debugfs_create_file("clock_domain_packet_sel", 0444,
		hw_regs->debugfs_base, base, &clock_domian_packet_set))
		goto debugfs_fail;

	if (!debugfs_create_file("trace_enable", 0444, hw_regs->debugfs_base,
				base, &clock_trace_enable))
		goto debugfs_fail;

	if (!debugfs_create_file("clock_timer", 0444,
			hw_regs->debugfs_base, base, &clock_timer_fops))
		goto debugfs_fail;

	if (!debugfs_create_file("trace_type", 0444,
		hw_regs->debugfs_base, base, &clk_trace_type_fops))
		goto debugfs_fail;

	return 0;

debugfs_fail:
	dev_err(&pdev->dev, "Failed to create debugfs entry so cleaning up\n");
	debugfs_remove_recursive(hw_regs->debugfs_base);
	return -ENODEV;
}

static int qcom_cpufreq_hw_debug_probe(struct platform_device *pdev)
{
	bool is_secure = scm_is_secure_device();

	return enable_cpufreq_hw_trace_debug(pdev, is_secure);
}

static int qcom_cpufreq_hw_debug_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(hw_regs->debugfs_base);
	return 0;
}

static const struct of_device_id qcom_cpufreq_hw_debug_trace_match[] = {
	{ .compatible = "qcom,cpufreq-hw-debug-trace",
					.data = &cpufreq_qcom_std_data },
	{ .compatible = "qcom,cpufreq-hw-debug",
					.data = &cpufreq_qcom_std_data },
	{ .compatible = "qcom,cpufreq-hw-epss-debug",
					.data = &cpufreq_qcom_std_epss_data },
	{}
};

static struct platform_driver qcom_cpufreq_hw_debug = {
	.probe = qcom_cpufreq_hw_debug_probe,
	.remove = qcom_cpufreq_hw_debug_remove,
	.driver = {
		.name = "qcom-cpufreq-hw-debug",
		.of_match_table = qcom_cpufreq_hw_debug_trace_match,
	},
};

static int __init qcom_cpufreq_hw_debug_trace_init(void)
{
	return platform_driver_register(&qcom_cpufreq_hw_debug);
}
fs_initcall(qcom_cpufreq_hw_debug_trace_init);

static void __exit qcom_cpufreq_hw_debug_trace_exit(void)
{
	return platform_driver_unregister(&qcom_cpufreq_hw_debug);
}
module_exit(qcom_cpufreq_hw_debug_trace_exit);

MODULE_DESCRIPTION("QTI clock driver for CPUFREQ HW debug");
MODULE_LICENSE("GPL v2");
