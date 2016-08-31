/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kconfig.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/rmi.h>
#include <linux/slab.h>

#define SMB_PROTOCOL_VERSION_ADDRESS	0xfd
#define RMI_PAGE_SELECT_REGISTER 0xff
#define RMI_SMB_PAGE(addr) (((addr) >> 8) & 0xff)
#define SMB_MAX_COUNT      32
#define RMI_SMB2_MAP_SIZE      8  /* 8 entry of 4 bytes each */
#define RMI_SMB2_MAP_FLAGS_WE      0x01

static char *smb_v1_proto_name = "smb1";   /* smbus old version */
static char *smb_v2_proto_name = "smb2";   /* smbus new version */

struct mapping_table_entry {
	union {
		struct {
			u16 rmiaddr;
			u8 readcount;
			u8 flags;
		};
		u8 entry[4];
	};
};

#define BUFFER_SIZE_INCREMENT 32

struct rmi_smb_data {
	struct mutex page_mutex;
	int page;
	int enabled;
	struct rmi_phys_device *phys;
	u8 table_index;
	struct mutex mappingtable_mutex;
	struct mapping_table_entry mapping_table[RMI_SMB2_MAP_SIZE];

	u8 *tx_buf;
	int tx_buf_size;

	u8 *debug_buf;
	int debug_buf_size;
	bool comms_debug;
#ifdef	CONFIG_RMI4_DEBUG
	struct dentry *debugfs_comms;
#endif
};

#ifdef CONFIG_RMI4_DEBUG

#include <linux/debugfs.h>
#include <linux/uaccess.h>

struct smbus_debugfs_data {
	bool done;
	struct rmi_smb_data *smbus_data;
};

static int debug_open(struct inode *inodep, struct file *filp)
{
	struct smbus_debugfs_data *data;

	data = kzalloc(sizeof(struct smbus_debugfs_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->smbus_data = inodep->i_private;
	filp->private_data = data;
	return 0;
}

static int debug_release(struct inode *inodep, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}

static ssize_t comms_debug_read(struct file *filp, char __user *buffer,
		size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	struct smbus_debugfs_data *dfs = filp->private_data;
	struct rmi_smb_data *data = dfs->smbus_data;

	if (dfs->done)
		return 0;
	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	dfs->done = 1;

	retval = snprintf(local_buf, PAGE_SIZE, "%u\n", data->comms_debug);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t comms_debug_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	unsigned int new_value;
	struct smbus_debugfs_data *dfs = filp->private_data;
	struct rmi_smb_data *data = dfs->smbus_data;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;
	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u", &new_value);
	kfree(local_buf);
	if (retval != 1 || new_value > 1)
		return -EINVAL;

	data->comms_debug = new_value;

	return size;
}

static const struct file_operations comms_debug_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = comms_debug_read,
	.write = comms_debug_write,
};

static int setup_debugfs(struct rmi_device *rmi_dev, struct rmi_smb_data *data)
{
	if (!rmi_dev->debugfs_root)
		return -ENODEV;

	data->debugfs_comms = debugfs_create_file("comms_debug", RMI_RW_ATTR,
			rmi_dev->debugfs_root, data, &comms_debug_fops);
	if (!data->debugfs_comms || IS_ERR(data->debugfs_comms)) {
		dev_warn(&rmi_dev->dev, "Failed to create debugfs comms_debug.\n");
		data->debugfs_comms = NULL;
	}

	return 0;
}

static void teardown_debugfs(struct rmi_smb_data *data)
{
	if (data->debugfs_comms)
		debugfs_remove(data->debugfs_comms);
}
#endif

#define COMMS_DEBUG(data) (IS_ENABLED(CONFIG_RMI4_DEBUG) && data->comms_debug)

/*
 * rmi_set_page - Set RMI page
 * @phys: The pointer to the rmi_phys_device struct
 * @page: The new page address.
 *
 * RMI devices have 16-bit addressing, but some of the physical
 * implementations (like SMBus) only have 8-bit addressing. So RMI implements
 * a page address at 0xff of every page so we can reliable page addresses
 * every 256 registers.
 *
 * The page_mutex lock must be held when this function is entered.
 *
 * Returns zero on success, non-zero on failure.
 */
static int rmi_set_page(struct rmi_phys_device *phys, u8 page)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	u8 txbuf[2] = {RMI_PAGE_SELECT_REGISTER, page};
	int retval;

	if (COMMS_DEBUG(data))
		dev_dbg(&client->dev, "writes 2 bytes: %02x %02x\n",
			txbuf[0], txbuf[1]);

	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval != sizeof(txbuf)) {
		phys->info.tx_errs++;
		dev_err(&client->dev,
			"%s: set page failed: %d.", __func__, retval);
		return (retval < 0) ? retval : -EIO;
	}
	data->page = page;
	return 0;
}

static int copy_to_debug_buf(struct device *dev, struct rmi_i2c_data *data,
			     u8 *buf, int len) {
	int i;
	int n = 0;
	char *temp;
	int dbg_size = 3 * len + 1;

	if (!data->debug_buf || data->debug_buf_size < dbg_size) {
		if (data->debug_buf)
			devm_kfree(dev, data->debug_buf);
		data->debug_buf_size = dbg_size + BUFFER_SIZE_INCREMENT;
		data->debug_buf = devm_kzalloc(dev, data->debug_buf_size,
					       GFP_KERNEL);
		if (!data->debug_buf) {
			data->debug_buf_size = 0;
			return -ENOMEM;
		}
	}
	temp = data->debug_buf;

	for (i = 0; i < len; i++) {
		n = sprintf(temp, " %02x", buf[i]);
		temp += n;
	}

	return 0;
}

static int rmi_smb_v1_write_block(struct rmi_phys_device *phys, u16 addr,
				  u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	int tx_size = len + 1;
	int retval;

	if (!data->tx_buf || data->tx_buf_size < tx_size) {
		if (data->tx_buf)
			devm_kfree(&client->dev, data->tx_buf);
		data->tx_buf_size = tx_size + BUFFER_SIZE_INCREMENT;
		data->tx_buf = devm_kzalloc(&client->dev, data->tx_buf_size,
					    GFP_KERNEL);
		if (!data->tx_buf) {
			data->tx_buf_size = 0;
			retval = -ENOMEM;
			goto exit;
		}
	}
	data->tx_buf[0] = addr & 0xff;
	memcpy(data->tx_buf + 1, buf, len);

	mutex_lock(&data->page_mutex);

	if (RMI_SMB_PAGE(addr) != data->page) {
		retval = rmi_set_page(phys, RMI_SMB_PAGE(addr));
		if (retval < 0)
			goto exit;
	}

	if (COMMS_DEBUG(data) &&
			!copy_to_debug_buf(&client->dev, data, buf, len))
		dev_dbg(&client->dev, "writes %d bytes at %#06x:%s\n",
			len, addr, data->debug_buf);

	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(tx_size);
	retval = i2c_master_send(client, data->tx_buf, tx_size);
	if (retval < 0)
		phys->info.tx_errs++;

exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

static int rmi_smb_v1_read_block(struct rmi_phys_device *phys, u16 addr,
			u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	u8 txbuf[1] = {addr & 0xff};
	int retval;

	mutex_lock(&data->page_mutex);

	if (RMI_SMB_PAGE(addr) != data->page) {
		retval = rmi_set_page(phys, RMI_SMB_PAGE(addr));
		if (retval < 0)
			goto exit;
	}

	if (COMMS_DEBUG(data))
		dev_dbg(&client->dev, "writes 1 bytes: %02x\n", txbuf[0]);

	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval != sizeof(txbuf)) {
		phys->info.tx_errs++;
		retval = (retval < 0) ? retval : -EIO;
		goto exit;
	}

	retval = i2c_master_recv(client, buf, len);

	phys->info.rx_count++;
	phys->info.rx_bytes += len;
	if (retval < 0)
		phys->info.rx_errs++;
	else if (COMMS_DEBUG(data) &&
			!copy_to_debug_buf(&client->dev, data, buf, len))
		dev_dbg(&client->dev, "read %d bytes at %#06x:%s\n",
			len, addr, data->debug_buf);

exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

/*SMB version 2 block write - wrapper over ic2_smb_write_block */
static int smb_v2_block_write(struct rmi_phys_device *phys,
			u8 commandcode, u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	int tx_size = len + 1;
	int retval;

	if (!data->tx_buf || data->tx_buf_size < tx_size) {
		if (data->tx_buf)
			devm_kfree(&client->dev, data->tx_buf);
		data->tx_buf_size = tx_size + BUFFER_SIZE_INCREMENT;
		data->tx_buf = devm_kzalloc(&client->dev, data->tx_buf_size,
					    GFP_KERNEL);
		if (!data->tx_buf) {
			data->tx_buf_size = 0;
			retval = -ENOMEM;
			goto exit;
		}
	}
	data->tx_buf[0] = commandcode & 0xff;
	memcpy(data->tx_buf + 1, buf, len);

	if (COMMS_DEBUG(data) &&
			!copy_to_debug_buf(&client->dev, data, buf, len))
		dev_dbg(&client->dev, "writes command %#04x %d bytes:%s\n",
			commandcode, len, data->debug_buf);

	phys->info.tx_count++;
	phys->info.tx_bytes += tx_size;

	retval = i2c_smbus_write_block_data(client, commandcode,
					    tx_size, data->tx_buf);
	if (retval < 0)
		phys->info.tx_errs++;

	return retval;
}

/* The function to get command code for smbus operations and keeps
records to the driver mapping table */
static int rmi_smb_v2_get_command_code(struct rmi_phys_device *phys,
		u16 rmiaddr, int bytecount, bool isread, u8 *commandcode)
{
	struct rmi_smb_data *data = phys->data;
	int i;
	int retval;
	struct mapping_table_entry *mapping_data;
	mutex_lock(&data->mappingtable_mutex);
	for (i = 0; i < RMI_SMB2_MAP_SIZE; i++) {
		if (data->mapping_table[i].rmiaddr == rmiaddr) {
			if (isread) {
				if (data->mapping_table[i].readcount
							== bytecount) {
					*commandcode = i;
					return 0;
				}
			} else {
				if (data->mapping_table[i].flags &
							RMI_SMB2_MAP_FLAGS_WE) {
					*commandcode = i;
					return 0;
				}
			}
		}
	}
	i = data->table_index;
	data->table_index = (i + 1) % RMI_SMB2_MAP_SIZE;

	mapping_data = kzalloc(sizeof(struct mapping_table_entry), GFP_KERNEL);
	if (!mapping_data) {
		retval = -ENOMEM;
		goto exit;
	}
	/* constructs mapping table data entry. 4 bytes each entry */
	mapping_data->rmiaddr = rmiaddr;
	mapping_data->readcount = bytecount;
	mapping_data->flags = RMI_SMB2_MAP_FLAGS_WE; /* enable write */

	retval = smb_v2_block_write(phys, i+0x80, (u8 *) mapping_data,
				    sizeof(mapping_data));
	if (retval < 0) {
		/* if not written to device mapping table */
		/* clear the driver mapping table records */
		data->mapping_table[i].rmiaddr = 0x0000;
		data->mapping_table[i].readcount = 0;
		data->mapping_table[i].flags = 0;
		goto exit;
	}
	/* save to the driver level mapping table */
	data->mapping_table[i].rmiaddr = rmiaddr;
	data->mapping_table[i].readcount = bytecount;
	data->mapping_table[i].flags = RMI_SMB2_MAP_FLAGS_WE;
	*commandcode = i;

exit:
	mutex_unlock(&data->mappingtable_mutex);
	return retval;
}

static int rmi_smb_v2_write_block(struct rmi_phys_device *phys, u16 rmiaddr,
		u8 *databuff, int len)
{
	int retval = 0;
	u8 commandcode;
	struct rmi_smb_data *data = phys->data;

	mutex_lock(&data->page_mutex);

	while (len > 0) {  /* while more than 32 bytes */
		/* break into 32 butes chunks to write */
		/* get command code */
		int block_len = min(len, SMB_MAX_COUNT);
		retval = rmi_smb_v2_get_command_code(phys, rmiaddr, block_len,
			false, &commandcode);
		if (retval < 0)
			goto exit;
		/* write to smb device */
		retval = smb_v2_block_write(phys, commandcode,
			databuff, block_len);
		if (retval < 0)
			goto exit;

		/* prepare to write next block of bytes */
		len -= SMB_MAX_COUNT;
		databuff += SMB_MAX_COUNT;
		rmiaddr += SMB_MAX_COUNT;
	}
exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

/*SMB version 2 block read - wrapper over ic2_smb_read_block */
static int smb_v2_block_read(struct rmi_phys_device *phys,
			u8 commandcode, u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	int retval;

/*	mutex_lock(&data->page_mutex); */

	retval = i2c_smbus_read_block_data(client, commandcode, buf);
	phys->info.rx_count++;
	phys->info.rx_bytes += len;
	if (retval < 0) {
		phys->info.rx_errs++;
		return retval;
	} else if (COMMS_DEBUG(data) &&
			!copy_to_debug_buf(&client->dev, data, buf, len))
		dev_dbg(&client->dev, "read %d bytes for command %#04x:%s\n",
			len, commandcode, data->debug_buf);

/*	mutex_unlock(&data->page_mutex);	*/
	return retval;
}

static int rmi_smb_v2_read_block(struct rmi_phys_device *phys, u16 rmiaddr,
					u8 *databuff, int len)
{
	struct rmi_smb_data *data = phys->data;
	int retval;
	u8 commandcode;

	mutex_lock(&data->page_mutex);

	while (len > 0) {
		/* break into 32 bytes chunks to write */
		/* get command code */
		int block_len = min(len, SMB_MAX_COUNT);

		retval = rmi_smb_v2_get_command_code(phys, rmiaddr, block_len,
			false, &commandcode);
		if (retval < 0)
			goto exit;
		/* read to smb device */
		retval = smb_v2_block_read(phys, commandcode,
			databuff, block_len);
		if (retval < 0)
			goto exit;

		/* prepare to read next block of bytes */
		len -= SMB_MAX_COUNT;
		databuff += SMB_MAX_COUNT;
		rmiaddr += SMB_MAX_COUNT;
	}

exit:
	mutex_unlock(&data->page_mutex);
	return 0;
}


static int rmi_smb_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct rmi_phys_device *rmi_phys;
	struct rmi_smb_data *data;
	struct rmi_device_platform_data *pdata = client->dev.platform_data;
	int retval;
	int smbus_version;
	if (!pdata) {
		dev_err(&client->dev, "no platform data\n");
		return -EINVAL;
	}
	pr_info("%s: Probing %s (IRQ %d).\n", __func__,
		pdata->sensor_name ? pdata->sensor_name : "-no name-",
		pdata->attn_gpio);

	retval = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!retval) {
		dev_err(&client->dev, "i2c_check_functionality error %d.\n",
				retval);
		return retval;
	}

	rmi_phys = devm_kzalloc(&client->dev, sizeof(struct rmi_phys_device),
			GFP_KERNEL);
	if (!rmi_phys)
		return -ENOMEM;

	data = devm_kzalloc(&client->dev, sizeof(struct rmi_smb_data),
				GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->enabled = true;	/* We plan to come up enabled. */
	data->phys = rmi_phys;

	rmi_phys->data = data;
	rmi_phys->dev = &client->dev;

	mutex_init(&data->page_mutex);
	mutex_init(&data->mappingtable_mutex);

	if (pdata->gpio_config) {
		retval = pdata->gpio_config(pdata->gpio_data, true);
		if (retval < 0) {
			dev_err(&client->dev, "failed to setup irq %d\n",
				pdata->attn_gpio);
			return retval;
		}
	}

	/* Check if for SMBus new version device by reading version byte. */
	retval = i2c_smbus_read_byte_data(client, SMB_PROTOCOL_VERSION_ADDRESS);
	if (retval < 0) {
		dev_err(&client->dev, "failed to get SMBus version number!\n");
		return retval;
	}
	smbus_version = retval + 1;
	dev_dbg(&client->dev, "Smbus version is %d", smbus_version);
	switch (smbus_version) {
	case 1:
		/* Setting the page to zero will (a) make sure the PSR is in a
		* known state, and (b) make sure we can talk to the device. */
		retval = rmi_set_page(rmi_phys, 0);
		if (retval) {
			dev_err(&client->dev, "Failed to set page select to 0.\n");
			return retval;
		}
		rmi_phys->write_block = rmi_smb_v1_write_block;
		rmi_phys->read_block = rmi_smb_v1_read_block;
		rmi_phys->info.proto = smb_v1_proto_name;
		break;
	case 2:
		/* SMBv2 */
		retval = i2c_check_functionality(client->adapter,
						I2C_FUNC_SMBUS_READ_BLOCK_DATA);
		if (retval < 0) {
			dev_err(&client->dev, "client's adapter does not support the I2C_FUNC_SMBUS_READ_BLOCK_DATA functionality.\n");
			return retval;
		}

		rmi_phys->write_block	= rmi_smb_v2_write_block;
		rmi_phys->read_block	= rmi_smb_v2_read_block;
		rmi_phys->info.proto	= smb_v2_proto_name;
		break;
	default:
		dev_err(&client->dev, "Unrecognized SMB version %d.\n",
				smbus_version);
		retval = -ENODEV;
		return retval;
	}
	/* End check if this is an SMBus device */

	retval = rmi_register_phys_device(rmi_phys);
	if (retval) {
		dev_err(&client->dev, "failed to register physical driver at 0x%.2X.\n",
			client->addr);
		return retval;
	}
	i2c_set_clientdata(client, rmi_phys);

	if (IS_ENABLED(CONFIG_RMI4_DEBUG))
		retval = setup_debugfs(rmi_phys->rmi_dev, data);

	dev_info(&client->dev, "registered rmi smb driver at 0x%.2X.\n",
			client->addr);
	return 0;
}

static int rmi_smb_remove(struct i2c_client *client)
{
	struct rmi_phys_device *phys = i2c_get_clientdata(client);
	struct rmi_device_platform_data *pd = client->dev.platform_data;

	if (IS_ENABLED(CONFIG_RMI4_DEBUG))
		teardown_debugfs(phys->data);

	rmi_unregister_phys_device(phys);

	if (pd->gpio_config)
		pd->gpio_config(&client->dev, false);

	return 0;
}

static const struct i2c_device_id rmi_id[] = {
	{ "rmi-smbus", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rmi_id);

static struct i2c_driver rmi_smb_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "rmi-smbus"
	},
	.id_table	= rmi_id,
	.probe		= rmi_smb_probe,
	.remove		= rmi_smb_remove,
};

static int __init rmi_smb_init(void)
{
	return i2c_add_driver(&rmi_smb_driver);
}

static void __exit rmi_smb_exit(void)
{
	i2c_del_driver(&rmi_smb_driver);
}

MODULE_AUTHOR("Allie Xiong <axiong@synaptics.com>");
MODULE_DESCRIPTION("RMI SMBus driver");
MODULE_LICENSE("GPL");

module_init(rmi_smb_init);
module_exit(rmi_smb_exit);
