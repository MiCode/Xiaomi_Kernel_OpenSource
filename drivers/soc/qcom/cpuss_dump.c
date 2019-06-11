// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <soc/qcom/memory_dump.h>

#define REG_DUMP_ID		0xEF

#define INPUT_DATA_BY_HLOS		0x00C0FFEE
#define FORMAT_VERSION_1		0x1
#define CORE_REG_NUM_DEFAULT	0x1

#define MAGIC_INDEX				0
#define FORMAT_VERSION_INDEX	1
#define SYS_REG_INPUT_INDEX		2
#define OUTPUT_DUMP_INDEX		3
#define PERCORE_INDEX			4
#define SYSTEM_REGS_INPUT_INDEX	5

struct cpuss_dump_drvdata {
	void *dump_vaddr;
	u32 size;
	u32 core_reg_num;
	u32 core_reg_used_num;
	u32 core_reg_end_index;
	u32 sys_reg_size;
	u32 used_memory;
	struct mutex mutex;
};

struct reg_dump_data {
	uint32_t magic;
	uint32_t version;
	uint32_t system_regs_input_index;
	uint32_t regdump_output_byte_offset;
};

/**
 * update_reg_dump_table - update the register dump table
 * @core_reg_num: the number of per-core registers
 *
 * This function calculates system_regs_input_index and
 * regdump_output_byte_offset to store into the dump memory.
 * It also updates members of drvdata by the parameter core_reg_num.
 *
 * Returns 0 on success, or -ENOMEM on error of no enough memory.
 */
static int update_reg_dump_table(struct device *dev, u32 core_reg_num)
{
	int ret = 0;
	u32 system_regs_input_index = SYSTEM_REGS_INPUT_INDEX +
			core_reg_num * 2;
	u32 regdump_output_byte_offset = (system_regs_input_index + 1)
			* sizeof(uint32_t);
	struct reg_dump_data *p;
	struct cpuss_dump_drvdata *drvdata = dev_get_drvdata(dev);

	mutex_lock(&drvdata->mutex);

	if (regdump_output_byte_offset >= drvdata->size ||
			regdump_output_byte_offset / sizeof(uint32_t)
			< system_regs_input_index + 1) {
		ret = -ENOMEM;
		goto err;
	}

	drvdata->core_reg_num = core_reg_num;
	drvdata->core_reg_used_num = 0;
	drvdata->core_reg_end_index = PERCORE_INDEX;
	drvdata->sys_reg_size = 0;
	drvdata->used_memory = regdump_output_byte_offset;

	memset(drvdata->dump_vaddr, 0xDE, drvdata->size);
	p = (struct reg_dump_data *)drvdata->dump_vaddr;
	p->magic = INPUT_DATA_BY_HLOS;
	p->version = FORMAT_VERSION_1;
	p->system_regs_input_index = system_regs_input_index;
	p->regdump_output_byte_offset = regdump_output_byte_offset;
	memset((uint32_t *)drvdata->dump_vaddr + PERCORE_INDEX, 0x0,
			(system_regs_input_index - PERCORE_INDEX + 1)
			* sizeof(uint32_t));

err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static void init_register_dump(struct device *dev)
{
	update_reg_dump_table(dev, CORE_REG_NUM_DEFAULT);
}

static ssize_t core_reg_num_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct cpuss_dump_drvdata *drvdata = dev_get_drvdata(dev);

	mutex_lock(&drvdata->mutex);

	ret = scnprintf(buf, PAGE_SIZE, "%u\n", drvdata->core_reg_num);

	mutex_unlock(&drvdata->mutex);
	return ret;
}

static ssize_t core_reg_num_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	unsigned int val;
	struct cpuss_dump_drvdata *drvdata = dev_get_drvdata(dev);

	if (kstrtouint(buf, 16, &val))
		return -EINVAL;

	mutex_lock(&drvdata->mutex);

	if (drvdata->core_reg_used_num || drvdata->sys_reg_size) {
		dev_err(dev, "Couldn't set core_reg_num, register available in list\n");
		ret = -EPERM;
		goto err;
	}
	if (val == drvdata->core_reg_num) {
		ret = 0;
		goto err;
	}

	mutex_unlock(&drvdata->mutex);

	ret = update_reg_dump_table(dev, val);
	if (ret) {
		dev_err(dev, "Couldn't set core_reg_num, no enough memory\n");
		return ret;
	}

	return size;

err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
static DEVICE_ATTR_RW(core_reg_num);

/**
 * This function shows configs of per-core and system registers.
 */
static ssize_t register_config_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char local_buf[64];
	int len = 0, count = 0;
	int index, system_index_start, index_end;
	uint32_t register_offset, length_in_bytes;
	uint32_t length_in_words;
	uint32_t *p;
	struct cpuss_dump_drvdata *drvdata = dev_get_drvdata(dev);

	buf[0] = '\0';

	mutex_lock(&drvdata->mutex);

	p = (uint32_t *)drvdata->dump_vaddr;

	/* print per-core & system registers */
	len = snprintf(local_buf, 64, "per-core registers:\n");
	strlcat(buf, local_buf, PAGE_SIZE);
	count += len;

	system_index_start = *(p + SYS_REG_INPUT_INDEX);
	index_end = system_index_start +
			drvdata->sys_reg_size / sizeof(uint32_t) + 1;
	for (index = PERCORE_INDEX; index < index_end;) {
		if (index == system_index_start) {
			len = snprintf(local_buf, 64, "system registers:\n");
			if ((count + len) > PAGE_SIZE) {
				dev_err(dev, "Couldn't write complete config\n");
				break;
			}

			strlcat(buf, local_buf, PAGE_SIZE);
			count += len;
		}

		register_offset = *(p + index);
		if (register_offset == 0) {
			index++;
			continue;
		}

		if (register_offset & 0x3) {
			length_in_words = register_offset & 0x3;
			length_in_bytes = length_in_words << 2;
			len = snprintf(local_buf, 64,
				"Index: 0x%x, addr: 0x%x\n",
				index, register_offset);
			index++;
		} else {
			length_in_bytes = *(p + index + 1);
			len = snprintf(local_buf, 64,
				"Index: 0x%x, addr: 0x%x, length: 0x%x\n",
				index, register_offset, length_in_bytes);
			index += 2;
		}

		if ((count + len) > PAGE_SIZE) {
			dev_err(dev, "Couldn't write complete config\n");
			break;
		}

		strlcat(buf, local_buf, PAGE_SIZE);
		count += len;
	}

	mutex_unlock(&drvdata->mutex);
	return count;
}

/**
 * This function sets configs of per-core or system registers.
 */
static ssize_t register_config_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	uint32_t register_offset, length_in_bytes, per_core = 0;
	uint32_t length_in_words;
	int nval;
	uint32_t num_cores;
	u32 extra_memory;
	u32 used_memory;
	u32 system_reg_end_index;
	uint32_t *p;
	struct cpuss_dump_drvdata *drvdata = dev_get_drvdata(dev);

	nval = sscanf(buf, "%x %x %u", &register_offset,
				&length_in_bytes, &per_core);
	if (nval != 2 && nval != 3)
		return -EINVAL;
	if (per_core > 1)
		return -EINVAL;
	if (register_offset & 0x3) {
		dev_err(dev, "Invalid address, must be 4 byte aligned\n");
		return -EINVAL;
	}
	if (length_in_bytes & 0x3) {
		dev_err(dev, "Invalid length, must be 4 byte aligned\n");
		return -EINVAL;
	}
	if (length_in_bytes == 0) {
		dev_err(dev, "Invalid length of 0\n");
		return -EINVAL;
	}

	mutex_lock(&drvdata->mutex);

	p = (uint32_t *)drvdata->dump_vaddr;
	length_in_words = length_in_bytes >> 2;
	if (per_core) { /* per-core register */
		if (drvdata->core_reg_used_num == drvdata->core_reg_num) {
			dev_err(dev, "Couldn't add per-core config, out of range\n");
			ret = -EINVAL;
			goto err;
		}

		num_cores = num_possible_cpus();
		extra_memory = length_in_bytes * num_cores;
		used_memory = drvdata->used_memory + extra_memory;
		if (extra_memory / num_cores < length_in_bytes ||
				used_memory > drvdata->size ||
				used_memory < drvdata->used_memory) {
			dev_err(dev, "Couldn't add per-core reg config, no enough memory\n");
			ret = -ENOMEM;
			goto err;
		}

		if (length_in_words > 3) {
			*(p + drvdata->core_reg_end_index) = register_offset;
			*(p + drvdata->core_reg_end_index + 1) =
					length_in_bytes;
			drvdata->core_reg_end_index += 2;
		} else {
			*(p + drvdata->core_reg_end_index) = register_offset |
					length_in_words;
			drvdata->core_reg_end_index++;
		}

		drvdata->core_reg_used_num++;
		drvdata->used_memory = used_memory;
	} else { /* system register */
		system_reg_end_index = *(p + SYS_REG_INPUT_INDEX) +
				drvdata->sys_reg_size / sizeof(uint32_t);

		if (length_in_words > 3) {
			extra_memory = sizeof(uint32_t) * 2 + length_in_bytes;
			used_memory = drvdata->used_memory + extra_memory;
			if (extra_memory < length_in_bytes ||
					used_memory > drvdata->size ||
					used_memory < drvdata->used_memory) {
				dev_err(dev, "Couldn't add system reg config, no enough memory\n");
				ret = -ENOMEM;
				goto err;
			}

			*(p + system_reg_end_index) = register_offset;
			*(p + system_reg_end_index + 1) = length_in_bytes;
			system_reg_end_index += 2;
			drvdata->sys_reg_size += sizeof(uint32_t) * 2;
		} else {
			extra_memory = sizeof(uint32_t) + length_in_bytes;
			used_memory = drvdata->used_memory + extra_memory;
			if (extra_memory < length_in_bytes ||
					used_memory > drvdata->size ||
					used_memory < drvdata->used_memory) {
				dev_err(dev, "Couldn't add system reg config, no enough memory\n");
				ret = -ENOMEM;
				goto err;
			}

			*(p + system_reg_end_index) = register_offset |
					length_in_words;
			system_reg_end_index++;
			drvdata->sys_reg_size += sizeof(uint32_t);
		}

		drvdata->used_memory = used_memory;

		*(p + system_reg_end_index) = 0x0;
		*(p + OUTPUT_DUMP_INDEX) = (system_reg_end_index + 1)
				* sizeof(uint32_t);
	}

	ret = size;

err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
static DEVICE_ATTR_RW(register_config);

/**
 * This function resets the register dump table.
 */
static ssize_t register_reset_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	unsigned int val;

	if (kstrtouint(buf, 16, &val))
		return -EINVAL;
	if (val != 1)
		return -EINVAL;

	init_register_dump(dev);

	return size;
}
static DEVICE_ATTR_WO(register_reset);

static const struct device_attribute *register_dump_attrs[] = {
	&dev_attr_core_reg_num,
	&dev_attr_register_config,
	&dev_attr_register_reset,
	NULL,
};

static int register_dump_create_files(struct device *dev,
			const struct device_attribute **attrs)
{
	int ret = 0;
	int i, j;

	for (i = 0; attrs[i] != NULL; i++) {
		ret = device_create_file(dev, attrs[i]);
		if (ret) {
			dev_err(dev, "Couldn't create sysfs attribute: %s\n",
				attrs[i]->attr.name);
			for (j = 0; j < i; j++)
				device_remove_file(dev, attrs[j]);
			break;
		}
	}
	return ret;
}

static int cpuss_dump_probe(struct platform_device *pdev)
{
	struct device_node *child_node, *dump_node;
	const struct device_node *node = pdev->dev.of_node;
	static dma_addr_t dump_addr;
	static void *dump_vaddr;
	struct msm_dump_data *dump_data;
	struct msm_dump_entry dump_entry;
	int ret;
	u32 size, id;
	struct cpuss_dump_drvdata *drvdata;

	for_each_available_child_of_node(node, child_node) {
		dump_node = of_parse_phandle(child_node, "qcom,dump-node", 0);
		drvdata = NULL;

		if (!dump_node) {
			dev_err(&pdev->dev, "Unable to find node for %s\n",
				child_node->name);
			continue;
		}

		ret = of_property_read_u32(dump_node, "qcom,dump-size", &size);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find size for %s\n",
					dump_node->name);
			continue;
		}

		ret = of_property_read_u32(child_node, "qcom,dump-id", &id);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find id for %s\n",
					child_node->name);
			continue;
		}

		dump_vaddr = (void *) dma_alloc_coherent(&pdev->dev, size,
						&dump_addr, GFP_KERNEL);

		if (!dump_vaddr) {
			dev_err(&pdev->dev, "Couldn't get memory for dumping\n");
			continue;
		}

		if (id == REG_DUMP_ID) {
			drvdata = devm_kzalloc(&pdev->dev,
				sizeof(struct cpuss_dump_drvdata), GFP_KERNEL);
			if (!drvdata) {
				dma_free_coherent(&pdev->dev, size, dump_vaddr,
						dump_addr);
				continue;
			}

			drvdata->dump_vaddr = dump_vaddr;
			drvdata->size = size;

			ret = register_dump_create_files(&pdev->dev,
					register_dump_attrs);
			if (ret) {
				dma_free_coherent(&pdev->dev, size, dump_vaddr,
						dump_addr);
				devm_kfree(&pdev->dev, drvdata);
				continue;
			}

			mutex_init(&drvdata->mutex);
			platform_set_drvdata(pdev, drvdata);

			init_register_dump(&pdev->dev);
		} else {
			memset(dump_vaddr, 0x0, size);
		}

		dump_data = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!dump_data) {
			dma_free_coherent(&pdev->dev, size, dump_vaddr,
					dump_addr);
			if (drvdata) {
				devm_kfree(&pdev->dev, drvdata);
				platform_set_drvdata(pdev, NULL);
			}
			continue;
		}

		dump_data->addr = dump_addr;
		dump_data->len = size;
		scnprintf(dump_data->name, sizeof(dump_data->name),
			"KCPUSS%X", id);
		dump_entry.id = id;
		dump_entry.addr = virt_to_phys(dump_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			dev_err(&pdev->dev, "Data dump setup failed, id = %d\n",
				id);
			dma_free_coherent(&pdev->dev, size, dump_vaddr,
					dump_addr);
			if (drvdata) {
				devm_kfree(&pdev->dev, drvdata);
				platform_set_drvdata(pdev, NULL);
			}
			devm_kfree(&pdev->dev, dump_data);
		}

	}
	return 0;
}

static int cpuss_dump_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id cpuss_dump_match_table[] = {
	{	.compatible = "qcom,cpuss-dump",	},
	{}
};

static struct platform_driver cpuss_dump_driver = {
	.probe = cpuss_dump_probe,
	.remove = cpuss_dump_remove,
	.driver = {
		.name = "msm_cpuss_dump",
		.owner = THIS_MODULE,
		.of_match_table = cpuss_dump_match_table,
	},
};

static int __init cpuss_dump_init(void)
{
	return platform_driver_register(&cpuss_dump_driver);
}

static void __exit cpuss_dump_exit(void)
{
	platform_driver_unregister(&cpuss_dump_driver);
}

subsys_initcall(cpuss_dump_init);
module_exit(cpuss_dump_exit)
