/*
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

#ifndef _LINUX_MIPI_BIF_H
#define _LINUX_MIPI_BIF_H

#include <linux/types.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>	/* for struct device */
#include <linux/sched.h>	/* for completion */
#include <linux/mutex.h>

/* Below represent 2 bit (9:8) prefixes for commands */
#define MIPI_BIF_WD		(0x0<<8)	/* Bits 7:0 are meant for data */
#define MIPI_BIF_ERA		(0x1<<8)	/* Bits 7:0 are meant for reg add (high) */
#define MIPI_BIF_WRA		(0x2<<8)	/* Bits 7:0 are meant for reg add (low) */
#define MIPI_BIF_RRA		(0x3<<8)	/* Bits 7:0 are meant for reg add (low) */

#define MIPI_BIF_BUS_COMMAND	(0x4<<8)	/* Bits 7:0 are meant to indicate the type of bus command */
#define MIPI_BIF_EDA		(0x5<<8)	/* Bits 7:0 are meant for device add (high) */
#define MIPI_BIF_SDA		(0x6<<8)	/* Bits 7:0 are meant for device add (low) */

/* Below are 10 bit (9:0) bus commands */
#define MIPI_BIF_BUS_COMMAND_RESET	(0x0 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_PWDN	(0x2 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_STBY	(0x3 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_EINT	(0x10 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_ISTS	(0x11 | MIPI_BIF_BUS_COMMAND)

#define MIPI_BIF_BUS_COMMAND_RBL0	((0x2 << 4) | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_RBE0	((0x3 << 4) | MIPI_BIF_BUS_COMMAND)

#define MIPI_BIF_BUS_COMMAND_DASM	(0x40 | MIPI_BIF_BUS_COMMAND)

#define MIPI_BIF_BUS_COMMAND_BRES	(0x00 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DISS	(0x80 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DILC	(0x81 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIE0	(0x84 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIE1	(0x85 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIP0	(0x86 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIP1	(0x87 | MIPI_BIF_BUS_COMMAND)

#define MIPI_BIF_BUS_COMMAND_DRES	(0xc0 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_TQ		(0xc2 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_AIO	(0xc4 | MIPI_BIF_BUS_COMMAND)

/* Below are command codes to be used from client side */
#define MIPI_BIF_WRITE		0x0001
#define MIPI_BIF_READDATA	0x0002
#define MIPI_BIF_INT_READ	0x0003
#define MIPI_BIF_STDBY		0x0004
#define MIPI_BIF_PWRDOWN	0x0005
#define MIPI_BIF_ACTIVATE	0x0006
#define MIPI_BIF_INT_EXIT	0x0007
#define MIPI_BIF_HARD_RESET	0x0008

/* Error codes for READ */
#define MIPI_BIF_RD_ACK_BIT			(1<<9)
#define MIPI_BIF_RD_PARITY_ERR			0x11
#define MIPI_BIF_RD_INVERSION_ERR		0x12
#define	MIPI_BIF_RD_INVALID_WORD_LEN_ERR	0x13
#define MIPI_BIF_RD_TIMING_ERR			0x14
#define MIPI_BIF_RD_UNKNOWN_CMD_ERR		0x15
#define MIPI_BIF_RD_WRONG_CMD_SEQ		0x16
#define MIPI_BIF_RD_BUS_COLLISION_ERR		0x1F
#define	MIPI_BIF_RD_SLAVE_BUSY_ERR		0x20

extern struct bus_type mipi_bif_bus_type;
extern struct device_type mipi_bif_adapter_type;

struct mipi_bif_msg;
struct mipi_bif_algorithm;
struct mipi_bif_adapter;
struct mipi_bif_client;
struct mipi_bif_driver;

extern int mipi_bif_transfer(struct mipi_bif_adapter *adap, struct mipi_bif_msg *msg);

struct mipi_bif_driver {

	unsigned int class;

	int (*attach_adapter)(struct mipi_bif_adapter *) __deprecated;
	int (*detach_adapter)(struct mipi_bif_adapter *) __deprecated;

	int (*probe)(struct mipi_bif_client *, const struct mipi_bif_device_id *);
	int (*remove)(struct mipi_bif_client *);
	int (*shutdown)(struct mipi_bif_client *);
	int (*suspend)(struct mipi_bif_client *, pm_message_t mesg);
	int (*resume)(struct mipi_bif_client *);

	struct device_driver driver;
	const struct mipi_bif_device_id *id_table;

	struct list_head clients;

};
#define to_mipi_bif_driver(d) container_of(d, struct mipi_bif_driver, driver)

extern int mipi_bif_register_driver(struct module *owner,
	struct mipi_bif_driver *driver);
#define mipi_bif_add_driver(driver) \
	mipi_bif_register_driver(THIS_MODULE, driver)

extern void mipi_bif_del_driver(struct mipi_bif_driver *driver);

struct mipi_bif_client {
	unsigned short addr;
	char name[MIPI_BIF_NAME_SIZE];
	struct mipi_bif_adapter *adapter;	/* the adapter we sit on	*/
	struct mipi_bif_driver *driver;	/* and our access routines	*/
	struct device dev;		/* the device structure		*/

};
#define to_mipi_bif_client(d) container_of(d, struct mipi_bif_client, dev)

struct mipi_bif_board_info {
	char		type[MIPI_BIF_NAME_SIZE];
	void		*platform_data;
	struct dev_archdata	*archdata;
	unsigned short addr;
};

static inline void *mipi_bif_get_clientdata(const struct mipi_bif_client *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void mipi_bif_set_clientdata(struct mipi_bif_client *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

struct mipi_bif_algorithm {
	int (*master_xfer)(struct mipi_bif_adapter *adap, struct mipi_bif_msg *msg);
};

struct mipi_bif_adapter {

	struct module *owner;
	unsigned int class;		  /* classes to allow probing for */
	const struct mipi_bif_algorithm *algo; /* the algorithm to access the bus */
	void *algo_data;

	struct rt_mutex bus_lock;

	int timeout;			/* in jiffies */
	int retries;
	struct device dev;		/* the adapter device */

	int nr;
	char name[48];
	struct completion dev_released;
};
#define to_mipi_bif_adapter(d) container_of(d, struct mipi_bif_adapter, dev)

extern struct mipi_bif_adapter *mipi_bif_get_adapter(int nr);

extern struct mipi_bif_client *
	mipi_bif_new_device(struct mipi_bif_adapter *adap,
	struct mipi_bif_board_info const *info);

extern void mipi_bif_unregister_device(struct mipi_bif_client *client);

static inline void *mipi_bif_get_adapdata(const struct mipi_bif_adapter *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void mipi_bif_set_adapdata(struct mipi_bif_adapter *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

static inline int mipi_bif_adapter_id(struct mipi_bif_adapter *adap)
{
	return adap->nr;
}

static inline struct mipi_bif_adapter *
mipi_bif_parent_is_mipi_bif_adapter(const struct mipi_bif_adapter *adapter)
{
	struct device *parent = adapter->dev.parent;

	if (parent != NULL && parent->type == &mipi_bif_adapter_type)
		return to_mipi_bif_adapter(parent);
	else
		return NULL;
}
extern int mipi_bif_add_numbered_adapter(struct mipi_bif_adapter *adap);

struct mipi_bif_msg {

	__u16 device_addr;	/* slave device address			*/
	__u16 reg_addr;		/* register address			*/
	__u16 commands;
	__u16 len;		/* msg length				*/
	__u8 *buf;		/* pointer to msg data			*/

};

#endif /* _LINUX_MIPI_BIF_H */
