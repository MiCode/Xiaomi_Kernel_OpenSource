/* BEGIN PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
//add Touch driver for G610-T11
/* BEGIN PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* BEGIN PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* BEGIN PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/*
 * cyttsp4_bus.h
 * Cypress TrueTouch(TM) Standard Product V4 Bus Driver.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor to add device functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_BUS_H
#define _LINUX_CYTTSP4_BUS_H

#include <linux/device.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/limits.h>


extern struct bus_type cyttsp4_bus_type;

struct cyttsp4_driver;
struct cyttsp4_device;
struct cyttsp4_adapter;

enum cyttsp4_atten_type {
	CY_ATTEN_IRQ,
	CY_ATTEN_STARTUP,
	CY_ATTEN_EXCLUSIVE,
	CY_ATTEN_NUM_ATTEN,
};

typedef int (*cyttsp4_atten_func) (struct cyttsp4_device *);

struct cyttsp4_ops {
	int (*write)(struct cyttsp4_adapter *dev, u8 addr,
		const void *buf, int size);
	int (*read)(struct cyttsp4_adapter *dev, u8 addr, void *buf, int size);
};

struct cyttsp4_adapter {
	struct list_head node;
	char id[NAME_MAX];
	struct device *dev;
	int (*write)(struct cyttsp4_adapter *dev, u8 addr,
		const void *buf, int size);
	int (*read)(struct cyttsp4_adapter *dev, u8 addr, void *buf, int size);
};
#define to_cyttsp4_adapter(d) container_of(d, struct cyttsp4_adapter, dev)

struct cyttsp4_core_info {
	char const *name;
	char const *id;
	char const *adap_id;
	void *platform_data;
};

struct cyttsp4_core {
	struct list_head node;
	char const *name;
	char const *id;
	char const *adap_id;
	struct device dev;
	struct cyttsp4_adapter *adap;
};
#define to_cyttsp4_core(d) container_of(d, struct cyttsp4_core, dev)

struct cyttsp4_device_info {
	char const *name;
	char const *core_id;
	void *platform_data;
};

struct cyttsp4_device {
	struct list_head node;
	char const *name;
	char const *core_id;
	struct device dev;
	struct cyttsp4_core *core;
};
#define to_cyttsp4_device(d) container_of(d, struct cyttsp4_device, dev)

struct cyttsp4_core_driver {
	struct device_driver driver;
	int (*probe)(struct cyttsp4_core *core);
	int (*remove)(struct cyttsp4_core *core);
	int (*subscribe_attention)(struct cyttsp4_device *ttsp,
				enum cyttsp4_atten_type type,
				cyttsp4_atten_func func,
				int flags);
	int (*unsubscribe_attention)(struct cyttsp4_device *ttsp,
				enum cyttsp4_atten_type type,
				cyttsp4_atten_func func,
				int flags);
	int (*request_exclusive)(struct cyttsp4_device *ttsp, int timeout_ms);
	int (*release_exclusive)(struct cyttsp4_device *ttsp);
	int (*request_reset)(struct cyttsp4_device *ttsp);
	int (*request_restart)(struct cyttsp4_device *ttsp, bool wait);
	int (*request_set_mode)(struct cyttsp4_device *ttsp, int mode);
	struct cyttsp4_sysinfo *(*request_sysinfo)(struct cyttsp4_device *ttsp);
	struct cyttsp4_loader_platform_data
		*(*request_loader_pdata)(struct cyttsp4_device *ttsp);
	int (*request_handshake)(struct cyttsp4_device *ttsp, u8 mode);
	int (*request_exec_cmd)(struct cyttsp4_device *ttsp, u8 mode,
			u8 *cmd_buf, size_t cmd_size, u8 *return_buf,
			size_t return_buf_size, int timeout_ms);
	int (*request_stop_wd)(struct cyttsp4_device *ttsp);
	int (*request_toggle_lowpower)(struct cyttsp4_device *ttsp, u8 mode);
	int (*request_write_config)(struct cyttsp4_device *ttsp, u8 ebid,
			u16 offset, u8 *data, u16 length);
	int (*write)(struct cyttsp4_device *ttsp, int mode,
		u8 addr, const void *buf, int size);
	int (*read)(struct cyttsp4_device *ttsp, int mode,
		u8 addr, void *buf, int size);
};
#define to_cyttsp4_core_driver(d) \
	container_of(d, struct cyttsp4_core_driver, driver)

struct cyttsp4_driver {
	struct device_driver driver;
	int (*probe)(struct cyttsp4_device *dev);
	int (*remove)(struct cyttsp4_device *fev);
};
#define to_cyttsp4_driver(d) container_of(d, struct cyttsp4_driver, driver)

extern int cyttsp4_register_driver(struct cyttsp4_driver *drv);
extern void cyttsp4_unregister_driver(struct cyttsp4_driver *drv);

extern int cyttsp4_register_core_driver(struct cyttsp4_core_driver *drv);
extern void cyttsp4_unregister_core_driver(struct cyttsp4_core_driver *drv);

extern int cyttsp4_register_device(struct cyttsp4_device_info const *dev_info);
extern int cyttsp4_unregister_device(char const *name, char const *core_id);

extern int cyttsp4_register_core_device(
		struct cyttsp4_core_info const *core_info);

extern int cyttsp4_add_adapter(char const *id, struct cyttsp4_ops const *ops,
		struct device *parent);

extern int cyttsp4_del_adapter(char const *id);

static inline int cyttsp4_read(struct cyttsp4_device *ttsp, int mode, u8 addr,
		void *buf, int size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->read(ttsp, mode, addr, buf, size);
}

static inline int cyttsp4_write(struct cyttsp4_device *ttsp, int mode, u8 addr,
		const void *buf, int size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->write(ttsp, mode, addr, buf, size);
}

static inline int cyttsp4_adap_read(struct cyttsp4_adapter *adap, u8 addr,
		void *buf, int size)
{
	return adap->read(adap, addr, buf, size);
}

static inline int cyttsp4_adap_write(struct cyttsp4_adapter *adap, u8 addr,
		const void *buf, int size)
{
	return adap->write(adap, addr, buf, size);
}

static inline int cyttsp4_subscribe_attention(struct cyttsp4_device *ttsp,
		enum cyttsp4_atten_type type, cyttsp4_atten_func func,
		int flags)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->subscribe_attention(ttsp, type, func, flags);
}

static inline int cyttsp4_unsubscribe_attention(struct cyttsp4_device *ttsp,
		enum cyttsp4_atten_type type, cyttsp4_atten_func func,
		int flags)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->unsubscribe_attention(ttsp, type, func, flags);
}

static inline int cyttsp4_request_exclusive(struct cyttsp4_device *ttsp,
		int timeout_ms)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_exclusive(ttsp, timeout_ms);
}

static inline int cyttsp4_release_exclusive(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->release_exclusive(ttsp);
}

static inline int cyttsp4_request_reset(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_reset(ttsp);
}

static inline int cyttsp4_request_restart(struct cyttsp4_device *ttsp,
		bool wait)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_restart(ttsp, wait);
}

static inline int cyttsp4_request_set_mode(struct cyttsp4_device *ttsp,
		int mode)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_set_mode(ttsp, mode);
}

static inline struct cyttsp4_sysinfo *cyttsp4_request_sysinfo(
		struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_sysinfo(ttsp);
}

static inline struct cyttsp4_loader_platform_data *cyttsp4_request_loader_pdata(
		struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_loader_pdata(ttsp);
}

static inline int cyttsp4_request_handshake(struct cyttsp4_device *ttsp,
		u8 mode)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_handshake(ttsp, mode);
}

static inline int cyttsp4_request_exec_cmd(struct cyttsp4_device *ttsp,
		u8 mode, u8 *cmd_buf, size_t cmd_size, u8 *return_buf,
		size_t return_buf_size, int timeout_ms)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_exec_cmd(ttsp, mode, cmd_buf, cmd_size, return_buf,
			return_buf_size, timeout_ms);
}

static inline int cyttsp4_request_stop_wd(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_stop_wd(ttsp);
}

static inline int cyttsp4_request_toggle_lowpower(struct cyttsp4_device *ttsp,
		u8 mode)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_toggle_lowpower(ttsp, mode);
}

static inline int cyttsp4_request_write_config(struct cyttsp4_device *ttsp,
		u8 ebid, u16 offset, u8 *data, u16 length)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_write_config(ttsp, ebid, offset, data, length);
}
#endif /* _LINUX_CYTTSP4_BUS_H */
/* END PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/* END PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* END PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* END PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
