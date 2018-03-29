#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "bus_tracer_interface.h"

static const struct of_device_id bus_tracer_of_ids[] = {
	{}
};

static int bus_tracer_probe(struct platform_device *pdev);
static int bus_tracer_remove(struct platform_device *pdev);
static int bus_tracer_suspend(struct platform_device *pdev, pm_message_t state);
static int bus_tracer_resume(struct platform_device *pdev);

static char *bus_tracer_dump_buf;

static struct bus_tracer bus_tracer_drv = {
	.plt_drv = {
		.driver = {
			.name = "bus_tracer",
			.bus = &platform_bus_type,
			.owner = THIS_MODULE,
			.of_match_table = bus_tracer_of_ids,
		},
		.probe = bus_tracer_probe,
		.remove = bus_tracer_remove,
		.suspend = bus_tracer_suspend,
		.resume = bus_tracer_resume,
	},
};


static int bus_tracer_probe(struct platform_device *pdev)
{
	struct bus_tracer_plt *plt = NULL;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	plt = bus_tracer_drv.cur_plt;

	if (plt && plt->ops && plt->ops->probe)
		return plt->ops->probe(plt, pdev);

	return 0;
}

static int bus_tracer_remove(struct platform_device *pdev)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->remove)
		return plt->ops->remove(plt, pdev);

	return 0;
}

static int bus_tracer_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->suspend)
		return plt->ops->suspend(plt, pdev, state);

	return 0;
}

static int bus_tracer_resume(struct platform_device *pdev)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->resume)
		return plt->ops->resume(plt, pdev);

	return 0;
}

int bus_tracer_register(struct bus_tracer_plt *plt)
{
	if (!plt) {
		pr_warn("%s%d: plt is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	plt->common = &bus_tracer_drv;
	bus_tracer_drv.cur_plt = plt;

	return 0;
}

int bus_tracer_dump(char *buf, int len)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!buf) {
		pr_warn("%s:%d: buf is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!plt->common) {
		pr_warn("%s:%d: plt->common is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->dump)
		return plt->ops->dump(plt, buf, len);

	pr_warn("no dump function implemented\n");

	return 0;
}

int bus_tracer_enable(unsigned char force_enable, unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->enable)
		return plt->ops->enable(plt, force_enable, tracer_id);

	pr_warn("no enable function implemented\n");

	return 0;
}

int bus_tracer_disable(void)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->disable)
		return plt->ops->disable(plt);

	pr_warn("no disable function implemented\n");

	return 0;
}

int bus_tracer_set_watchpoint_filter(struct watchpoint_filter f, unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_watchpoint_filter)
		return plt->ops->set_watchpoint_filter(plt, f, tracer_id);

	pr_warn("no set_watchpoint_filter function implemented\n");

	return 0;
}

int bus_tracer_set_bypass_filter(struct bypass_filter f, unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_bypass_filter)
		return plt->ops->set_bypass_filter(plt, f, tracer_id);

	pr_warn("no set_bypass_filter function implemented\n");

	return 0;
}

int bus_tracer_set_id_filter(struct id_filter f, unsigned int tracer_id, unsigned int idf_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_id_filter)
		return plt->ops->set_id_filter(plt, f, tracer_id, idf_id);

	pr_warn("no set_id_filter function implemented\n");

	return 0;
}

int bus_tracer_set_rw_filter(struct rw_filter f, unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_rw_filter)
		return plt->ops->set_rw_filter(plt, f, tracer_id);

	pr_warn("no set_rw_filter function implemented\n");

	return 0;
}

int bus_tracer_dump_setting(char *buf, int len)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_err("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->dump_setting)
		return plt->ops->dump_setting(plt, buf, len);

	pr_warn("no dump_setting function implemented\n");

	return 0;
}

int bus_tracer_dump_min_len(void)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!plt->min_buf_len)
		pr_warn("%s:%d: min_buf_len is 0\n", __func__, __LINE__);

	return plt->min_buf_len;
}

int mt_bus_tracer_dump(char *buf)
{
	strncpy(buf, bus_tracer_dump_buf, strlen(bus_tracer_dump_buf)+1);
	return 0;
}

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/dump_to_buf
 * dump traces to internal buffer
 */
static ssize_t bus_tracer_dump_to_buf_show(struct device_driver *driver, char *buf)
{
	if (!bus_tracer_dump_buf)
		bus_tracer_dump_buf = kzalloc(bus_tracer_drv.cur_plt->min_buf_len, GFP_KERNEL);

	if (!bus_tracer_dump_buf)
		return -ENOMEM;

	if (bus_tracer_drv.cur_plt)
		bus_tracer_dump(bus_tracer_dump_buf, bus_tracer_drv.cur_plt->min_buf_len);

	return snprintf(buf, PAGE_SIZE, "copy trace to internal buffer..\n"
			"using command \"cat /proc/bus_tracer/dump_db\" to see the trace\n");
}

static ssize_t bus_tracer_dump_to_buf_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

static DRIVER_ATTR(dump_to_buf, 0664, bus_tracer_dump_to_buf_show, bus_tracer_dump_to_buf_store);

/*
 * interface: /proc/bus_trcaer/dump_db
 * AEE read this seq_file to get the content of internal buffer and generate db
 */
static int bus_tracer_dump_db_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", bus_tracer_dump_buf);
	return 0;
}

static int bus_tracer_dump_db_open(struct inode *inode, struct file *file)
{
	return single_open(file, bus_tracer_dump_db_show, PDE_DATA(inode));
}

static int bus_tracer_dump_db_release(struct inode *inode, struct file *file)
{
	/* release buffer after dumping to db */
	kfree(bus_tracer_dump_buf);
	bus_tracer_dump_buf = NULL;

	return single_release(inode, file);
}

static const struct file_operations bus_tracer_dump_db_proc_fops = {
	.open		= bus_tracer_dump_db_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= bus_tracer_dump_db_release,
};


/*
 * interface: /sys/bus/platform/drivers/bus_tracer/tracer_enable
 * enable tracer from sysfs in runtime
 */
static ssize_t bus_tracer_enable_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\n"
			"echo $tracer_id $enabled  > /sys/bus/platform/drivers/bus_tracer/tracer_enable\n");
}

static ssize_t bus_tracer_enable_store(struct device_driver *driver, const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	unsigned int tracer_id = 0, enable = 0;
	unsigned long input;
	char *p = (char *)buf, *arg;
	int ret = 0, i = 0;

	while ((arg = strsep(&p, " ")) && (i <= 1)) {
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_err("%s:%d: kstrtoul fail for %s\n", __func__, __LINE__, p);
			return 0;
		}
		switch (i) {
		case 0:
			tracer_id = (unsigned int) input & 1;
			break;
		case 1:
			enable = (unsigned char) input & 1;
			break;
		default:
			break;
		}
		i++;
	}

	if (plt && plt->ops) {
		if (enable)
			ret = plt->ops->enable(plt, 1, tracer_id);
		else
			pr_err("%s:%d: not support for runtime disabling\n", __func__, __LINE__);

		if (ret)
			pr_err("%s:%d: enable failed\n", __func__, __LINE__);
	}

	return count;
}

static DRIVER_ATTR(tracer_enable, 0664, bus_tracer_enable_show, bus_tracer_enable_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/trace_recording
 * pause/resume trace recording from sysfs in runtime
 */
static ssize_t bus_tracer_recording_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\ninput 0 to pause recording,\n"
					"input 1 to resume.\n");
}

static ssize_t bus_tracer_recording_store(struct device_driver *driver, const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	char *p = (char *)buf;
	unsigned long arg = 0;
	int ret = -1;

	if (kstrtoul(p, 10, &arg) != 0) {
		pr_err("%s:%d: kstrtoul fail for %s\n", __func__, __LINE__, p);
		return 0;
	}

	/* 0 to pause recording, 1 or other ints to resume */
	if (plt && plt->ops) {
		if (plt->ops->set_recording)
			ret = plt->ops->set_recording(plt, !(arg & 1));

		if (ret)
			pr_err("%s:%d: recording failed\n", __func__, __LINE__);
	}

	return count;
}

static DRIVER_ATTR(tracer_recording, 0664, bus_tracer_recording_show, bus_tracer_recording_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/watchpoint_filter
 * setup watchpoint filter
 */
static ssize_t bus_tracer_watchpoint_filter_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\n"
			"echo $tracer_id $enabled $addr $addr_h $mask > /sys/bus/platform/drivers/bus_tracer/watchpoint_filter\n");
}

static ssize_t bus_tracer_watchpoint_filter_store(struct device_driver *driver, const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	unsigned long input = 0;
	int ret = -1, i = 0, tracer_id = 0;
	char *p = (char *)buf, *arg;
	struct watchpoint_filter f = {
		.enabled = 0,
		.addr_h = 0,
		.addr = 0,
		.mask = 0,
	};

	while ((arg = strsep(&p, " ")) && (i <= 4)) {
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_err("%s:%d: kstrtoul fail for %s\n", __func__, __LINE__, p);
			return 0;
		}
		switch (i) {
		case 0:
			tracer_id = (unsigned int) input & 1;
			break;
		case 1:
			f.enabled = (unsigned char) input & 1;
			break;
		case 2:
			f.addr_h = (unsigned int) input;
			break;
		case 3:
			f.addr = (unsigned int) input;
			break;
		case 4:
			f.mask = (unsigned int) input;
			break;
		default:
			break;
		}
		i++;
	}

	if (i <= 4) {
		pr_err("%s:%d: too few arguments\n", __func__, __LINE__);
		return count;
	}

	if (plt && plt->ops) {
		ret = plt->ops->set_watchpoint_filter(plt, f, tracer_id);

		if (ret)
			pr_err("%s:%d: recording failed\n", __func__, __LINE__);
	}

	return count;
}

static DRIVER_ATTR(watchpoint_filter, 0664, bus_tracer_watchpoint_filter_show, bus_tracer_watchpoint_filter_store);


/*
 * interface: /sys/bus/platform/drivers/bus_tracer/bypass_filter
 * setup bypass filter
 */
static ssize_t bus_tracer_bypass_filter_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\n"
			"echo $tracer_id $enabled $addr $mask > /sys/bus/platform/drivers/bus_tracer/bypass_filter\n");
}

static ssize_t bus_tracer_bypass_filter_store(struct device_driver *driver, const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	char *p = (char *)buf, *arg;
	unsigned long input = 0;
	int ret = -1, i = 0, tracer_id = 0;
	struct bypass_filter f = {
		.enabled = 0,
		.addr = 0,
		.mask = 0,
	};

	while ((arg = strsep(&p, " ")) && (i <= 3)) {
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_err("%s:%d: kstrtoul fail for %s\n", __func__, __LINE__, p);
			return 0;
		}
		switch (i) {
		case 0:
			tracer_id = (unsigned int) input & 1;
			break;
		case 1:
			f.enabled = (unsigned char) input & 1;
			break;
		case 2:
			f.addr = (unsigned int) input;
			break;
		case 3:
			f.mask = (unsigned int) input;
			break;
		default:
			break;
		}
		i++;
	}

	if (i <= 3) {
		pr_err("%s:%d: too few arguments\n", __func__, __LINE__);
		return count;
	}

	if (plt && plt->ops) {
		ret = plt->ops->set_bypass_filter(plt, f, tracer_id);

		if (ret)
			pr_err("%s:%d: recording failed\n", __func__, __LINE__);
	}

	return count;
}

static DRIVER_ATTR(bypass_filter, 0664, bus_tracer_bypass_filter_show, bus_tracer_bypass_filter_store);


/*
 * interface: /sys/bus/platform/drivers/bus_tracer/id_filter
 * setup id filter
 */
static ssize_t bus_tracer_id_filter_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\n"
			"echo $tracer_id $idf_id $enabled $ID > /sys/bus/platform/drivers/bus_tracer/id_filter\n");
}

static ssize_t bus_tracer_id_filter_store(struct device_driver *driver, const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	char *p = (char *)buf, *arg;
	unsigned long input = 0;
	int ret = -1, i = 0, tracer_id = 0, idf_id;
	struct id_filter f = {
		.enabled = 0,
		.id = 0,
	};

	while ((arg = strsep(&p, " ")) && (i <= 3)) {
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_err("%s:%d: kstrtoul fail for %s\n", __func__, __LINE__, p);
			return 0;
		}
		switch (i) {
		case 0:
			tracer_id = (unsigned int) input & 1;
			break;
		case 1:
			idf_id = (unsigned int) input;
			break;
		case 2:
			f.enabled = (unsigned char) input & 1;
			break;
		case 3:
			f.id = (unsigned int) input;
			break;
		default:
			break;
		}
		i++;
	}

	if (i <= 3) {
		pr_err("%s:%d: too few arguments\n", __func__, __LINE__);
		return count;
	}

	if (plt && plt->ops) {
		ret = plt->ops->set_id_filter(plt, f, tracer_id, idf_id);

		if (ret)
			pr_err("%s:%d: recording failed\n", __func__, __LINE__);
	}

	return count;
}

static DRIVER_ATTR(id_filter, 0664, bus_tracer_id_filter_show, bus_tracer_id_filter_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/rw_filter
 * setup read/write filter
 */
static ssize_t bus_tracer_rw_filter_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\n"
			"echo $tracer_id $read $write > /sys/bus/platform/drivers/bus_tracer/rw_filter\n");
}

static ssize_t bus_tracer_rw_filter_store(struct device_driver *driver, const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	char *p = (char *)buf, *arg;
	unsigned long input = 0;
	int ret = -1, i = 0, tracer_id = 0;
	struct rw_filter f = {
		.read = 0,
		.write = 0,
	};

	while ((arg = strsep(&p, " ")) && (i <= 2)) {
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_err("%s:%d: kstrtoul fail for %s\n", __func__, __LINE__, p);
			return 0;
		}
		switch (i) {
		case 0:
			tracer_id = (unsigned int) input & 1;
			break;
		case 1:
			f.read = (unsigned char) input & 1;
			break;
		case 2:
			f.write = (unsigned char) input & 1;
			break;
		default:
			break;
		}
		i++;
	}

	if (i <= 2) {
		pr_err("%s:%d: too few arguments\n", __func__, __LINE__);
		return count;
	}

	if (plt && plt->ops) {
		ret = plt->ops->set_rw_filter(plt, f, tracer_id);

		if (ret)
			pr_err("%s:%d: recording failed\n", __func__, __LINE__);
	}

	return count;
}

static DRIVER_ATTR(rw_filter, 0664, bus_tracer_rw_filter_show, bus_tracer_rw_filter_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/dump_setting
 * dump current setting of bus tracers
 */
static ssize_t bus_dump_setting_show(struct device_driver *driver, char *buf)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	int ret = 0;

	ret = plt->ops->dump_setting(plt, buf, -1);
	if (ret)
		pr_err("%s:%d: dump failed\n", __func__, __LINE__);

	return strlen(buf);
}

static ssize_t bus_dump_setting_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

static DRIVER_ATTR(dump_setting, 0664, bus_dump_setting_show, bus_dump_setting_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/last_status
 * read this status from ram-console to check whether the bus tracer
 * was recording before system resets
 */
static ssize_t last_status_show(struct device_driver *driver, char *buf)
{
	/* FIXME: get the last status from ram console after integration with aee */
	snprintf(buf, PAGE_SIZE, "1\n");

	return strlen(buf);
}

static ssize_t last_status_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

static DRIVER_ATTR(last_status, 0664, last_status_show, last_status_store);



static int bus_tracer_start(void)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt)
		return -ENODEV;

	if (!plt->ops) {
		pr_err("%s:%d: ops not installed\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops->start)
		return plt->ops->start(plt);

	return 0;
}

static int __init bus_tracer_init(void)
{
	int ret;
	static struct proc_dir_entry *root_dir;

	ret = bus_tracer_start();
	if (ret) {
		pr_err("%s:%d: bus_tracer_start failed\n", __func__, __LINE__);
		return -ENODEV;
	}

	/* since kernel already populates dts, our probe would be callback after this registration */
	ret = platform_driver_register(&bus_tracer_drv.plt_drv);
	if (ret) {
		pr_err("%s:%d: platform_driver_register failed\n", __func__, __LINE__);
		return -ENODEV;
	}

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_dump_to_buf);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_tracer_enable);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_tracer_recording);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_watchpoint_filter);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_bypass_filter);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_id_filter);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_rw_filter);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_dump_setting);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver, &driver_attr_last_status);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	/* create /proc/bus_tracer */
	root_dir = proc_mkdir("bus_tracer", NULL);
	if (!root_dir)
		return -EINVAL;

	/* create /proc/bus_tracer/dump_db */
	proc_create("dump_db", 0644, root_dir, &bus_tracer_dump_db_proc_fops);

	bus_tracer_dump_buf = kzalloc(bus_tracer_drv.cur_plt->min_buf_len, GFP_KERNEL);
	if (!bus_tracer_dump_buf)
		return -ENOMEM;

	bus_tracer_dump(bus_tracer_dump_buf, bus_tracer_drv.cur_plt->min_buf_len);
	/* force_enable=0 to enable all the tracers with enabled=1 */
	bus_tracer_enable(0, -1);

	return 0;
}

postcore_initcall(bus_tracer_init);
