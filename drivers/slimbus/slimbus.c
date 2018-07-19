/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/idr.h>
#include <linux/pm_runtime.h>
#include <linux/slimbus/slimbus.h>

#define SLIM_PORT_HDL(la, f, p) ((la)<<24 | (f) << 16 | (p))

#define SLIM_HDL_TO_LA(hdl)	((u32)((hdl) & 0xFF000000) >> 24)
#define SLIM_HDL_TO_FLOW(hdl)	(((u32)(hdl) & 0xFF0000) >> 16)
#define SLIM_HDL_TO_PORT(hdl)	((u32)(hdl) & 0xFF)

#define SLIM_HDL_TO_CHIDX(hdl)	((u16)(hdl) & 0xFF)
#define SLIM_GRP_TO_NCHAN(hdl)	((u16)(hdl >> 8) & 0xFF)

#define SLIM_SLAVE_PORT(p, la)	(((la)<<16) | (p))
#define SLIM_MGR_PORT(p)	((0xFF << 16) | (p))
#define SLIM_LA_MANAGER		0xFF

#define SLIM_START_GRP		(1 << 8)
#define SLIM_END_GRP		(1 << 9)

#define SLIM_MAX_INTR_COEFF_3	(SLIM_SL_PER_SUPERFRAME/3)
#define SLIM_MAX_INTR_COEFF_1	SLIM_SL_PER_SUPERFRAME

static DEFINE_MUTEX(slim_lock);
static DEFINE_IDR(ctrl_idr);
static struct device_type slim_dev_type;
static struct device_type slim_ctrl_type;

#define DEFINE_SLIM_LDEST_TXN(name, mc, len, rl, rbuf, wbuf, la) \
	struct slim_msg_txn name = { rl, 0, mc, SLIM_MSG_DEST_LOGICALADDR, 0,\
					len, 0, la, false, rbuf, wbuf, NULL, }

#define DEFINE_SLIM_BCAST_TXN(name, mc, len, rl, rbuf, wbuf, la) \
	struct slim_msg_txn name = { rl, 0, mc, SLIM_MSG_DEST_BROADCAST, 0,\
					len, 0, la, false, rbuf, wbuf, NULL, }

static const struct slim_device_id *slim_match(const struct slim_device_id *id,
					const struct slim_device *slim_dev)
{
	while (id->name[0]) {
		if (strcmp(slim_dev->name, id->name) == 0)
			return id;
		id++;
	}
	return NULL;
}

const struct slim_device_id *slim_get_device_id(const struct slim_device *sdev)
{
	const struct slim_driver *sdrv = to_slim_driver(sdev->dev.driver);

	return slim_match(sdrv->id_table, sdev);
}
EXPORT_SYMBOL(slim_get_device_id);

static int slim_device_match(struct device *dev, struct device_driver *driver)
{
	struct slim_device *slim_dev;
	struct slim_driver *drv = to_slim_driver(driver);

	if (dev->type == &slim_dev_type)
		slim_dev = to_slim_device(dev);
	else
		return 0;
	if (drv->id_table)
		return slim_match(drv->id_table, slim_dev) != NULL;

	if (driver->name)
		return strcmp(slim_dev->name, driver->name) == 0;
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int slim_legacy_suspend(struct device *dev, pm_message_t mesg)
{
	struct slim_device *slim_dev = NULL;
	struct slim_driver *driver;

	if (dev->type == &slim_dev_type)
		slim_dev = to_slim_device(dev);

	if (!slim_dev || !dev->driver)
		return 0;

	driver = to_slim_driver(dev->driver);
	if (!driver->suspend)
		return 0;

	return driver->suspend(slim_dev, mesg);
}

static int slim_legacy_resume(struct device *dev)
{
	struct slim_device *slim_dev = NULL;
	struct slim_driver *driver;

	if (dev->type == &slim_dev_type)
		slim_dev = to_slim_device(dev);

	if (!slim_dev || !dev->driver)
		return 0;

	driver = to_slim_driver(dev->driver);
	if (!driver->resume)
		return 0;

	return driver->resume(slim_dev);
}

static int slim_pm_suspend(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_suspend(dev);
	else
		return slim_legacy_suspend(dev, PMSG_SUSPEND);
}

static int slim_pm_resume(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_resume(dev);
	else
		return slim_legacy_resume(dev);
}

#else
#define slim_pm_suspend		NULL
#define slim_pm_resume		NULL
#endif

static const struct dev_pm_ops slimbus_pm = {
	.suspend = slim_pm_suspend,
	.resume = slim_pm_resume,
	SET_RUNTIME_PM_OPS(
		pm_generic_suspend,
		pm_generic_resume,
		NULL
		)
};
struct bus_type slimbus_type = {
	.name		= "slimbus",
	.match		= slim_device_match,
	.pm		= &slimbus_pm,
};
EXPORT_SYMBOL(slimbus_type);

struct device slimbus_dev = {
	.init_name = "slimbus",
};

static void __exit slimbus_exit(void)
{
	device_unregister(&slimbus_dev);
	bus_unregister(&slimbus_type);
}

static int __init slimbus_init(void)
{
	int retval;

	retval = bus_register(&slimbus_type);
	if (!retval)
		retval = device_register(&slimbus_dev);

	if (retval)
		bus_unregister(&slimbus_type);

	return retval;
}
postcore_initcall(slimbus_init);
module_exit(slimbus_exit);

static int slim_drv_probe(struct device *dev)
{
	const struct slim_driver *sdrv = to_slim_driver(dev->driver);
	struct slim_device *sbdev = to_slim_device(dev);
	struct slim_controller *ctrl = sbdev->ctrl;

	if (sdrv->probe) {
		int ret;

		ret = sdrv->probe(sbdev);
		if (ret)
			return ret;
		if (sdrv->device_up)
			queue_work(ctrl->wq, &sbdev->wd);
		return 0;
	}
	return -ENODEV;
}

static int slim_drv_remove(struct device *dev)
{
	const struct slim_driver *sdrv = to_slim_driver(dev->driver);
	struct slim_device *sbdev = to_slim_device(dev);

	sbdev->notified = false;
	if (sdrv->remove)
		return sdrv->remove(to_slim_device(dev));
	return -ENODEV;
}

static void slim_drv_shutdown(struct device *dev)
{
	const struct slim_driver *sdrv = to_slim_driver(dev->driver);

	if (sdrv->shutdown)
		sdrv->shutdown(to_slim_device(dev));
}

/*
 * slim_driver_register: Client driver registration with slimbus
 * @drv:Client driver to be associated with client-device.
 * This API will register the client driver with the slimbus
 * It is called from the driver's module-init function.
 */
int slim_driver_register(struct slim_driver *drv)
{
	drv->driver.bus = &slimbus_type;
	if (drv->probe)
		drv->driver.probe = slim_drv_probe;

	if (drv->remove)
		drv->driver.remove = slim_drv_remove;

	if (drv->shutdown)
		drv->driver.shutdown = slim_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(slim_driver_register);

/*
 * slim_driver_unregister: Undo effects of slim_driver_register
 * @drv: Client driver to be unregistered
 */
void slim_driver_unregister(struct slim_driver *drv)
{
	if (drv)
		driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(slim_driver_unregister);

#define slim_ctrl_attr_gr NULL

static void slim_ctrl_release(struct device *dev)
{
	struct slim_controller *ctrl = to_slim_controller(dev);

	complete(&ctrl->dev_released);
}

static struct device_type slim_ctrl_type = {
	.groups		= slim_ctrl_attr_gr,
	.release	= slim_ctrl_release,
};

static struct slim_controller *slim_ctrl_get(struct slim_controller *ctrl)
{
	if (!ctrl || !get_device(&ctrl->dev))
		return NULL;

	return ctrl;
}

static void slim_ctrl_put(struct slim_controller *ctrl)
{
	if (ctrl)
		put_device(&ctrl->dev);
}

#define slim_device_attr_gr NULL
#define slim_device_uevent NULL
static void slim_dev_release(struct device *dev)
{
	struct slim_device *sbdev = to_slim_device(dev);

	slim_ctrl_put(sbdev->ctrl);
}

static struct device_type slim_dev_type = {
	.groups		= slim_device_attr_gr,
	.uevent		= slim_device_uevent,
	.release	= slim_dev_release,
};

static void slim_report(struct work_struct *work)
{
	struct slim_driver *sbdrv;
	struct slim_device *sbdev =
			container_of(work, struct slim_device, wd);
	if (!sbdev->dev.driver)
		return;
	/* check if device-up or down needs to be called */
	if ((!sbdev->reported && !sbdev->notified) ||
			(sbdev->reported && sbdev->notified))
		return;

	sbdrv = to_slim_driver(sbdev->dev.driver);
	/*
	 * address no longer valid, means device reported absent, whereas
	 * address valid, means device reported present
	 */
	if (sbdev->notified && !sbdev->reported) {
		sbdev->notified = false;
		if (sbdrv->device_down)
			sbdrv->device_down(sbdev);
	} else if (!sbdev->notified && sbdev->reported) {
		sbdev->notified = true;
		if (sbdrv->device_up)
			sbdrv->device_up(sbdev);
	}
}

static void slim_device_reset(struct work_struct *work)
{
	struct slim_driver *sbdrv;
	struct slim_device *sbdev =
			container_of(work, struct slim_device, device_reset);

	if (!sbdev->dev.driver)
		return;

	sbdrv = to_slim_driver(sbdev->dev.driver);
	if (sbdrv && sbdrv->reset_device)
		sbdrv->reset_device(sbdev);
}

/*
 * slim_add_device: Add a new device without register board info.
 * @ctrl: Controller to which this device is to be added to.
 * Called when device doesn't have an explicit client-driver to be probed, or
 * the client-driver is a module installed dynamically.
 */
int slim_add_device(struct slim_controller *ctrl, struct slim_device *sbdev)
{
	sbdev->dev.bus = &slimbus_type;
	sbdev->dev.parent = ctrl->dev.parent;
	sbdev->dev.type = &slim_dev_type;
	sbdev->dev.driver = NULL;
	sbdev->ctrl = ctrl;
	slim_ctrl_get(ctrl);
	dev_set_name(&sbdev->dev, "%s", sbdev->name);
	mutex_init(&sbdev->sldev_reconf);
	INIT_LIST_HEAD(&sbdev->mark_define);
	INIT_LIST_HEAD(&sbdev->mark_suspend);
	INIT_LIST_HEAD(&sbdev->mark_removal);
	INIT_WORK(&sbdev->wd, slim_report);
	INIT_WORK(&sbdev->device_reset, slim_device_reset);
	mutex_lock(&ctrl->m_ctrl);
	list_add_tail(&sbdev->dev_list, &ctrl->devs);
	mutex_unlock(&ctrl->m_ctrl);
	/* probe slave on this controller */
	return device_register(&sbdev->dev);
}
EXPORT_SYMBOL(slim_add_device);

struct sbi_boardinfo {
	struct list_head	list;
	struct slim_boardinfo	board_info;
};

static LIST_HEAD(board_list);
static LIST_HEAD(slim_ctrl_list);
static DEFINE_MUTEX(board_lock);

/* If controller is not present, only add to boards list */
static void slim_match_ctrl_to_boardinfo(struct slim_controller *ctrl,
				struct slim_boardinfo *bi)
{
	int ret;

	if (ctrl->nr != bi->bus_num)
		return;

	ret = slim_add_device(ctrl, bi->slim_slave);
	if (ret != 0)
		dev_err(ctrl->dev.parent, "can't create new device for %s\n",
			bi->slim_slave->name);
}

/*
 * slim_register_board_info: Board-initialization routine.
 * @info: List of all devices on all controllers present on the board.
 * @n: number of entries.
 * API enumerates respective devices on corresponding controller.
 * Called from board-init function.
 */
int slim_register_board_info(struct slim_boardinfo const *info, unsigned int n)
{
	struct sbi_boardinfo *bi;
	int i;

	bi = kcalloc(n, sizeof(*bi), GFP_KERNEL);
	if (!bi)
		return -ENOMEM;

	for (i = 0; i < n; i++, bi++, info++) {
		struct slim_controller *ctrl;

		memcpy(&bi->board_info, info, sizeof(*info));
		mutex_lock(&board_lock);
		list_add_tail(&bi->list, &board_list);
		list_for_each_entry(ctrl, &slim_ctrl_list, list)
			slim_match_ctrl_to_boardinfo(ctrl, &bi->board_info);
		mutex_unlock(&board_lock);
	}
	return 0;
}
EXPORT_SYMBOL(slim_register_board_info);

/*
 * slim_ctrl_add_boarddevs: Add devices registered by board-info
 * @ctrl: Controller to which these devices are to be added to.
 * This API is called by controller when it is up and running.
 * If devices on a controller were registered before controller,
 * this will make sure that they get probed when controller is up.
 */
void slim_ctrl_add_boarddevs(struct slim_controller *ctrl)
{
	struct sbi_boardinfo *bi;

	mutex_lock(&board_lock);
	list_add_tail(&ctrl->list, &slim_ctrl_list);
	list_for_each_entry(bi, &board_list, list)
		slim_match_ctrl_to_boardinfo(ctrl, &bi->board_info);
	mutex_unlock(&board_lock);
}
EXPORT_SYMBOL(slim_ctrl_add_boarddevs);

/*
 * slim_busnum_to_ctrl: Map bus number to controller
 * @busnum: Bus number
 * Returns controller representing this bus number
 */
struct slim_controller *slim_busnum_to_ctrl(u32 bus_num)
{
	struct slim_controller *ctrl;

	mutex_lock(&board_lock);
	list_for_each_entry(ctrl, &slim_ctrl_list, list)
		if (bus_num == ctrl->nr) {
			mutex_unlock(&board_lock);
			return ctrl;
		}
	mutex_unlock(&board_lock);
	return NULL;
}
EXPORT_SYMBOL(slim_busnum_to_ctrl);

static int slim_register_controller(struct slim_controller *ctrl)
{
	int ret = 0;

	/* Can't register until after driver model init */
	if (WARN_ON(!slimbus_type.p)) {
		ret = -EPROBE_DEFER;
		goto out_list;
	}

	dev_set_name(&ctrl->dev, "sb-%d", ctrl->nr);
	ctrl->dev.bus = &slimbus_type;
	ctrl->dev.type = &slim_ctrl_type;
	ctrl->num_dev = 0;
	if (!ctrl->min_cg)
		ctrl->min_cg = SLIM_MIN_CLK_GEAR;
	if (!ctrl->max_cg)
		ctrl->max_cg = SLIM_MAX_CLK_GEAR;
	spin_lock_init(&ctrl->txn_lock);
	mutex_init(&ctrl->m_ctrl);
	mutex_init(&ctrl->sched.m_reconf);
	ret = device_register(&ctrl->dev);
	if (ret)
		goto out_list;

	dev_dbg(&ctrl->dev, "Bus [%s] registered:dev:%p\n", ctrl->name,
							&ctrl->dev);

	if (ctrl->nports) {
		ctrl->ports = kcalloc(ctrl->nports, sizeof(struct slim_port),
					GFP_KERNEL);
		if (!ctrl->ports) {
			ret = -ENOMEM;
			goto err_port_failed;
		}
	}
	if (ctrl->nchans) {
		ctrl->chans = kcalloc(ctrl->nchans, sizeof(struct slim_ich),
					GFP_KERNEL);
		if (!ctrl->chans) {
			ret = -ENOMEM;
			goto err_chan_failed;
		}

		ctrl->sched.chc1 = kcalloc(ctrl->nchans,
					sizeof(struct slim_ich *), GFP_KERNEL);
		if (!ctrl->sched.chc1) {
			kfree(ctrl->chans);
			ret = -ENOMEM;
			goto err_chan_failed;
		}
		ctrl->sched.chc3 = kcalloc(ctrl->nchans,
					sizeof(struct slim_ich *), GFP_KERNEL);
		if (!ctrl->sched.chc3) {
			kfree(ctrl->sched.chc1);
			kfree(ctrl->chans);
			ret = -ENOMEM;
			goto err_chan_failed;
		}
	}
#ifdef DEBUG
	ctrl->sched.slots = kzalloc(SLIM_SL_PER_SUPERFRAME, GFP_KERNEL);
#endif
	init_completion(&ctrl->pause_comp);

	INIT_LIST_HEAD(&ctrl->devs);
	ctrl->wq = create_singlethread_workqueue(dev_name(&ctrl->dev));
	if (!ctrl->wq)
		goto err_workq_failed;

	return 0;

err_workq_failed:
	kfree(ctrl->sched.chc3);
	kfree(ctrl->sched.chc1);
	kfree(ctrl->chans);
err_chan_failed:
	kfree(ctrl->ports);
err_port_failed:
	device_unregister(&ctrl->dev);
out_list:
	mutex_lock(&slim_lock);
	idr_remove(&ctrl_idr, ctrl->nr);
	mutex_unlock(&slim_lock);
	return ret;
}

/* slim_remove_device: Remove the effect of slim_add_device() */
void slim_remove_device(struct slim_device *sbdev)
{
	struct slim_controller *ctrl = sbdev->ctrl;

	mutex_lock(&ctrl->m_ctrl);
	list_del_init(&sbdev->dev_list);
	mutex_unlock(&ctrl->m_ctrl);
	device_unregister(&sbdev->dev);
}
EXPORT_SYMBOL(slim_remove_device);

static void slim_ctrl_remove_device(struct slim_controller *ctrl,
				struct slim_boardinfo *bi)
{
	if (ctrl->nr == bi->bus_num)
		slim_remove_device(bi->slim_slave);
}

/*
 * slim_del_controller: Controller tear-down.
 * Controller added with the above API is teared down using this API.
 */
int slim_del_controller(struct slim_controller *ctrl)
{
	struct slim_controller *found;
	struct sbi_boardinfo *bi;

	/* First make sure that this bus was added */
	mutex_lock(&slim_lock);
	found = idr_find(&ctrl_idr, ctrl->nr);
	mutex_unlock(&slim_lock);
	if (found != ctrl)
		return -EINVAL;

	/* Remove all clients */
	mutex_lock(&board_lock);
	list_for_each_entry(bi, &board_list, list)
		slim_ctrl_remove_device(ctrl, &bi->board_info);
	mutex_unlock(&board_lock);

	init_completion(&ctrl->dev_released);
	device_unregister(&ctrl->dev);

	wait_for_completion(&ctrl->dev_released);
	list_del(&ctrl->list);
	destroy_workqueue(ctrl->wq);
	/* free bus id */
	mutex_lock(&slim_lock);
	idr_remove(&ctrl_idr, ctrl->nr);
	mutex_unlock(&slim_lock);

	kfree(ctrl->sched.chc1);
	kfree(ctrl->sched.chc3);
#ifdef DEBUG
	kfree(ctrl->sched.slots);
#endif
	kfree(ctrl->chans);
	kfree(ctrl->ports);

	return 0;
}
EXPORT_SYMBOL(slim_del_controller);

/*
 * slim_add_numbered_controller: Controller bring-up.
 * @ctrl: Controller to be registered.
 * A controller is registered with the framework using this API. ctrl->nr is the
 * desired number with which slimbus framework registers the controller.
 * Function will return -EBUSY if the number is in use.
 */
int slim_add_numbered_controller(struct slim_controller *ctrl)
{
	int	id;

	mutex_lock(&slim_lock);
	id = idr_alloc(&ctrl_idr, ctrl, ctrl->nr, ctrl->nr + 1, GFP_KERNEL);
	mutex_unlock(&slim_lock);

	if (id < 0)
		return id;

	ctrl->nr = id;
	return slim_register_controller(ctrl);
}
EXPORT_SYMBOL(slim_add_numbered_controller);

/*
 * slim_report_absent: Controller calls this function when a device
 *	reports absent, OR when the device cannot be communicated with
 * @sbdev: Device that cannot be reached, or sent report absent
 */
void slim_report_absent(struct slim_device *sbdev)
{
	struct slim_controller *ctrl;
	int i;

	if (!sbdev)
		return;
	ctrl = sbdev->ctrl;
	if (!ctrl)
		return;
	/* invalidate logical addresses */
	mutex_lock(&ctrl->m_ctrl);
	for (i = 0; i < ctrl->num_dev; i++) {
		if (sbdev->laddr == ctrl->addrt[i].laddr)
			ctrl->addrt[i].valid = false;
	}
	mutex_unlock(&ctrl->m_ctrl);
	sbdev->reported = false;
	queue_work(ctrl->wq, &sbdev->wd);
}
EXPORT_SYMBOL(slim_report_absent);

static int slim_remove_ch(struct slim_controller *ctrl, struct slim_ich *slc);
/*
 * slim_framer_booted: This function is called by controller after the active
 * framer has booted (using Bus Reset sequence, or after it has shutdown and has
 * come back up). Components, devices on the bus may be in undefined state,
 * and this function triggers their drivers to do the needful
 * to bring them back in Reset state so that they can acquire sync, report
 * present and be operational again.
 */
void slim_framer_booted(struct slim_controller *ctrl)
{
	struct slim_device *sbdev;
	struct list_head *pos, *next;
	int i;

	if (!ctrl)
		return;

	/* Since framer has rebooted, reset all data channels */
	mutex_lock(&ctrl->sched.m_reconf);
	for (i = 0; i < ctrl->nchans; i++) {
		struct slim_ich *slc = &ctrl->chans[i];

		if (slc->state > SLIM_CH_DEFINED)
			slim_remove_ch(ctrl, slc);
	}
	mutex_unlock(&ctrl->sched.m_reconf);
	mutex_lock(&ctrl->m_ctrl);
	list_for_each_safe(pos, next, &ctrl->devs) {
		sbdev = list_entry(pos, struct slim_device, dev_list);
		if (sbdev)
			queue_work(ctrl->wq, &sbdev->device_reset);
	}
	mutex_unlock(&ctrl->m_ctrl);
}
EXPORT_SYMBOL(slim_framer_booted);

/*
 * slim_msg_response: Deliver Message response received from a device to the
 *	framework.
 * @ctrl: Controller handle
 * @reply: Reply received from the device
 * @len: Length of the reply
 * @tid: Transaction ID received with which framework can associate reply.
 * Called by controller to inform framework about the response received.
 * This helps in making the API asynchronous, and controller-driver doesn't need
 * to manage 1 more table other than the one managed by framework mapping TID
 * with buffers
 */
void slim_msg_response(struct slim_controller *ctrl, u8 *reply, u8 tid, u8 len)
{
	int i;
	unsigned long flags;
	bool async;
	struct slim_msg_txn *txn;

	spin_lock_irqsave(&ctrl->txn_lock, flags);
	txn = ctrl->txnt[tid];
	if (txn == NULL || txn->rbuf == NULL) {
		spin_unlock_irqrestore(&ctrl->txn_lock, flags);
		if (txn == NULL)
			dev_err(&ctrl->dev, "Got response to invalid TID:%d, len:%d",
				tid, len);
		else
			dev_err(&ctrl->dev, "Invalid client buffer passed\n");
		return;
	}
	async = txn->async;
	for (i = 0; i < len; i++)
		txn->rbuf[i] = reply[i];
	if (txn->comp)
		complete(txn->comp);
	ctrl->txnt[tid] = NULL;
	spin_unlock_irqrestore(&ctrl->txn_lock, flags);
	if (async)
		kfree(txn);
}
EXPORT_SYMBOL(slim_msg_response);

static int slim_processtxn(struct slim_controller *ctrl,
				struct slim_msg_txn *txn, bool need_tid)
{
	unsigned int i = 0;
	int ret = 0;
	unsigned long flags;

	if (need_tid) {
		spin_lock_irqsave(&ctrl->txn_lock, flags);
		for (i = 0; i < ctrl->last_tid; i++) {
			if (ctrl->txnt[i] == NULL)
				break;
		}
		if (i >= ctrl->last_tid) {
			if (ctrl->last_tid == SLIM_MAX_TXNS) {
				spin_unlock_irqrestore(&ctrl->txn_lock, flags);
				return -ENOMEM;
			}
			ctrl->last_tid++;
		}
		ctrl->txnt[i] = txn;
		txn->tid = i;
		spin_unlock_irqrestore(&ctrl->txn_lock, flags);
	}

	ret = ctrl->xfer_msg(ctrl, txn);
	return ret;
}

static int ctrl_getlogical_addr(struct slim_controller *ctrl, const u8 *eaddr,
				u8 e_len, u8 *entry)
{
	u8 i;

	for (i = 0; i < ctrl->num_dev; i++) {
		if (ctrl->addrt[i].valid &&
			memcmp(ctrl->addrt[i].eaddr, eaddr, e_len) == 0) {
			*entry = i;
			return 0;
		}
	}
	return -ENXIO;
}

/*
 * slim_assign_laddr: Assign logical address to a device enumerated.
 * @ctrl: Controller with which device is enumerated.
 * @e_addr: 6-byte elemental address of the device.
 * @e_len: buffer length for e_addr
 * @laddr: Return logical address (if valid flag is false)
 * @valid: true if laddr holds a valid address that controller wants to
 *	set for this enumeration address. Otherwise framework sets index into
 *	address table as logical address.
 * Called by controller in response to REPORT_PRESENT. Framework will assign
 * a logical address to this enumeration address.
 * Function returns -EXFULL to indicate that all logical addresses are already
 * taken.
 */
int slim_assign_laddr(struct slim_controller *ctrl, const u8 *e_addr,
				u8 e_len, u8 *laddr, bool valid)
{
	int ret;
	u8 i = 0;
	bool exists = false;
	struct slim_device *sbdev;
	struct list_head *pos, *next;
	void *new_addrt = NULL;

	mutex_lock(&ctrl->m_ctrl);
	/* already assigned */
	if (ctrl_getlogical_addr(ctrl, e_addr, e_len, &i) == 0) {
		*laddr = ctrl->addrt[i].laddr;
		exists = true;
	} else {
		if (ctrl->num_dev >= 254) {
			ret = -EXFULL;
			goto ret_assigned_laddr;
		}
		for (i = 0; i < ctrl->num_dev; i++) {
			if (ctrl->addrt[i].valid == false)
				break;
		}
		if (i == ctrl->num_dev) {
			new_addrt = krealloc(ctrl->addrt,
					(ctrl->num_dev + 1) *
					sizeof(struct slim_addrt),
					GFP_KERNEL);
			if (!new_addrt) {
				ret = -ENOMEM;
				goto ret_assigned_laddr;
			}
			ctrl->addrt = new_addrt;
			ctrl->num_dev++;
		}
		memcpy(ctrl->addrt[i].eaddr, e_addr, e_len);
		ctrl->addrt[i].valid = true;
		/* Preferred address is index into table */
		if (!valid)
			*laddr = i;
	}

	ret = ctrl->set_laddr(ctrl, (const u8 *)&ctrl->addrt[i].eaddr, 6,
				*laddr);
	if (ret) {
		ctrl->addrt[i].valid = false;
		goto ret_assigned_laddr;
	}
	ctrl->addrt[i].laddr = *laddr;

	dev_dbg(&ctrl->dev, "setting slimbus l-addr:%x\n", *laddr);
ret_assigned_laddr:
	mutex_unlock(&ctrl->m_ctrl);
	if (exists || ret)
		return ret;

	pr_info("slimbus:%d laddr:0x%x, EAPC:0x%x:0x%x", ctrl->nr, *laddr,
				e_addr[1], e_addr[2]);
	mutex_lock(&ctrl->m_ctrl);
	list_for_each_safe(pos, next, &ctrl->devs) {
		sbdev = list_entry(pos, struct slim_device, dev_list);
		if (memcmp(sbdev->e_addr, e_addr, 6) == 0) {
			struct slim_driver *sbdrv;

			sbdev->laddr = *laddr;
			sbdev->reported = true;
			if (sbdev->dev.driver) {
				sbdrv = to_slim_driver(sbdev->dev.driver);
				if (sbdrv->device_up)
					queue_work(ctrl->wq, &sbdev->wd);
			}
			break;
		}
	}
	mutex_unlock(&ctrl->m_ctrl);
	return 0;
}
EXPORT_SYMBOL(slim_assign_laddr);

/*
 * slim_get_logical_addr: Return the logical address of a slimbus device.
 * @sb: client handle requesting the address.
 * @e_addr: Elemental address of the device.
 * @e_len: Length of e_addr
 * @laddr: output buffer to store the address
 * context: can sleep
 * -EINVAL is returned in case of invalid parameters, and -ENXIO is returned if
 *  the device with this elemental address is not found.
 */
int slim_get_logical_addr(struct slim_device *sb, const u8 *e_addr,
				u8 e_len, u8 *laddr)
{
	int ret = 0;
	u8 entry;
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl || !laddr || !e_addr || e_len != 6)
		return -EINVAL;
	mutex_lock(&ctrl->m_ctrl);
	ret = ctrl_getlogical_addr(ctrl, e_addr, e_len, &entry);
	if (!ret)
		*laddr = ctrl->addrt[entry].laddr;
	mutex_unlock(&ctrl->m_ctrl);
	if (ret == -ENXIO && ctrl->get_laddr) {
		ret = ctrl->get_laddr(ctrl, e_addr, e_len, laddr);
		if (!ret)
			ret = slim_assign_laddr(ctrl, e_addr, e_len, laddr,
						true);
	}
	return ret;
}
EXPORT_SYMBOL(slim_get_logical_addr);

static int slim_ele_access_sanity(struct slim_ele_access *msg, int oper,
				u8 *rbuf, const u8 *wbuf, u8 len)
{
	if (!msg || msg->num_bytes > 16 || msg->start_offset + len > 0xC00)
		return -EINVAL;
	switch (oper) {
	case SLIM_MSG_MC_REQUEST_VALUE:
	case SLIM_MSG_MC_REQUEST_INFORMATION:
		if (rbuf == NULL)
			return -EINVAL;
		return 0;
	case SLIM_MSG_MC_CHANGE_VALUE:
	case SLIM_MSG_MC_CLEAR_INFORMATION:
		if (wbuf == NULL)
			return -EINVAL;
		return 0;
	case SLIM_MSG_MC_REQUEST_CHANGE_VALUE:
	case SLIM_MSG_MC_REQUEST_CLEAR_INFORMATION:
		if (rbuf == NULL || wbuf == NULL)
			return -EINVAL;
		return 0;
	default:
		return -EINVAL;
	}
}

static u16 slim_slicecodefromsize(u32 req)
{
	u8 codetosize[8] = {1, 2, 3, 4, 6, 8, 12, 16};

	if (req >= 8)
		return 0;
	else
		return codetosize[req];
}

static u16 slim_slicesize(u32 code)
{
	u8 sizetocode[16] = {0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7};

	if (code == 0)
		code = 1;
	if (code > 16)
		code = 16;
	return sizetocode[code - 1];
}


/* Message APIs Unicast message APIs used by slimbus slave drivers */

/*
 * Message API access routines.
 * @sb: client handle requesting elemental message reads, writes.
 * @msg: Input structure for start-offset, number of bytes to read.
 * @rbuf: data buffer to be filled with values read.
 * @len: data buffer size
 * @wbuf: data buffer containing value/information to be written
 * context: can sleep
 * Returns:
 * -EINVAL: Invalid parameters
 * -ETIMEDOUT: If controller could not complete the request. This may happen if
 *  the bus lines are not clocked, controller is not powered-on, slave with
 *  given address is not enumerated/responding.
 */
int slim_request_val_element(struct slim_device *sb,
				struct slim_ele_access *msg, u8 *buf, u8 len)
{
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl)
		return -EINVAL;
	return slim_xfer_msg(ctrl, sb, msg, SLIM_MSG_MC_REQUEST_VALUE, buf,
			NULL, len);
}
EXPORT_SYMBOL(slim_request_val_element);

int slim_request_inf_element(struct slim_device *sb,
				struct slim_ele_access *msg, u8 *buf, u8 len)
{
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl)
		return -EINVAL;
	return slim_xfer_msg(ctrl, sb, msg, SLIM_MSG_MC_REQUEST_INFORMATION,
			buf, NULL, len);
}
EXPORT_SYMBOL(slim_request_inf_element);

int slim_change_val_element(struct slim_device *sb, struct slim_ele_access *msg,
				const u8 *buf, u8 len)
{
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl)
		return -EINVAL;
	return slim_xfer_msg(ctrl, sb, msg, SLIM_MSG_MC_CHANGE_VALUE, NULL, buf,
					len);
}
EXPORT_SYMBOL(slim_change_val_element);

int slim_clear_inf_element(struct slim_device *sb, struct slim_ele_access *msg,
				u8 *buf, u8 len)
{
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl)
		return -EINVAL;
	return slim_xfer_msg(ctrl, sb, msg, SLIM_MSG_MC_CLEAR_INFORMATION, NULL,
					buf, len);
}
EXPORT_SYMBOL(slim_clear_inf_element);

int slim_request_change_val_element(struct slim_device *sb,
					struct slim_ele_access *msg, u8 *rbuf,
					const u8 *wbuf, u8 len)
{
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl)
		return -EINVAL;
	return slim_xfer_msg(ctrl, sb, msg, SLIM_MSG_MC_REQUEST_CHANGE_VALUE,
					rbuf, wbuf, len);
}
EXPORT_SYMBOL(slim_request_change_val_element);

int slim_request_clear_inf_element(struct slim_device *sb,
					struct slim_ele_access *msg, u8 *rbuf,
					const u8 *wbuf, u8 len)
{
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl)
		return -EINVAL;
	return slim_xfer_msg(ctrl, sb, msg,
					SLIM_MSG_MC_REQUEST_CLEAR_INFORMATION,
					rbuf, wbuf, len);
}
EXPORT_SYMBOL(slim_request_clear_inf_element);

/*
 * Broadcast message API:
 * call this API directly with sbdev = NULL.
 * For broadcast reads, make sure that buffers are big-enough to incorporate
 * replies from all logical addresses.
 * All controllers may not support broadcast
 */
int slim_xfer_msg(struct slim_controller *ctrl, struct slim_device *sbdev,
			struct slim_ele_access *msg, u16 mc, u8 *rbuf,
			const u8 *wbuf, u8 len)
{
	DECLARE_COMPLETION_ONSTACK(complete);
	DEFINE_SLIM_LDEST_TXN(txn_stack, mc, len, 6, rbuf, wbuf, sbdev->laddr);
	struct slim_msg_txn *txn;
	int ret;
	u16 sl, cur;

	if (msg->comp && rbuf) {
		txn = kmalloc(sizeof(struct slim_msg_txn),
						GFP_KERNEL);
		if (IS_ERR_OR_NULL(txn))
			return PTR_ERR(txn);
		*txn = txn_stack;
		txn->async = true;
		txn->comp = msg->comp;
	} else {
		txn = &txn_stack;
		if (rbuf)
			txn->comp = &complete;
	}

	ret = slim_ele_access_sanity(msg, mc, rbuf, wbuf, len);
	if (ret)
		goto xfer_err;

	sl = slim_slicesize(len);
	dev_dbg(&ctrl->dev, "SB xfer msg:os:%x, len:%d, MC:%x, sl:%x\n",
				msg->start_offset, len, mc, sl);

	cur = slim_slicecodefromsize(sl);
	txn->ec = ((sl | (1 << 3)) | ((msg->start_offset & 0xFFF) << 4));

	if (wbuf)
		txn->rl += len;
	if (rbuf) {
		unsigned long flags;

		txn->rl++;
		ret = slim_processtxn(ctrl, txn, true);

		/* sync read */
		if (!ret && !msg->comp) {
			ret = wait_for_completion_timeout(&complete, HZ);
			if (!ret) {
				dev_err(&ctrl->dev, "slimbus Read timed out");
				spin_lock_irqsave(&ctrl->txn_lock, flags);
				/* Invalidate the transaction */
				ctrl->txnt[txn->tid] = NULL;
				spin_unlock_irqrestore(&ctrl->txn_lock, flags);
				ret = -ETIMEDOUT;
			} else
				ret = 0;
		} else if (ret < 0 && !msg->comp) {
			dev_err(&ctrl->dev, "slimbus Read error");
			spin_lock_irqsave(&ctrl->txn_lock, flags);
			/* Invalidate the transaction */
			ctrl->txnt[txn->tid] = NULL;
			spin_unlock_irqrestore(&ctrl->txn_lock, flags);
		}

	} else
		ret = slim_processtxn(ctrl, txn, false);
xfer_err:
	return ret;
}
EXPORT_SYMBOL(slim_xfer_msg);

/*
 * User message:
 * slim_user_msg: Send user message that is interpreted by destination device
 * @sb: Client handle sending the message
 * @la: Destination device for this user message
 * @mt: Message Type (Soruce-referred, or Destination-referred)
 * @mc: Message Code
 * @msg: Message structure (start offset, number of bytes) to be sent
 * @buf: data buffer to be sent
 * @len: data buffer size in bytes
 */
int slim_user_msg(struct slim_device *sb, u8 la, u8 mt, u8 mc,
				struct slim_ele_access *msg, u8 *buf, u8 len)
{
	if (!sb || !sb->ctrl || !msg || mt == SLIM_MSG_MT_CORE)
		return -EINVAL;
	if (!sb->ctrl->xfer_user_msg)
		return -EPROTONOSUPPORT;
	return sb->ctrl->xfer_user_msg(sb->ctrl, la, mt, mc, msg, buf, len);
}
EXPORT_SYMBOL(slim_user_msg);

/*
 * Queue bulk of message writes:
 * slim_bulk_msg_write: Write bulk of messages (e.g. downloading FW)
 * @sb: Client handle sending these messages
 * @la: Destination device for these messages
 * @mt: Message Type
 * @mc: Message Code
 * @msgs: List of messages to be written in bulk
 * @n: Number of messages in the list
 * @cb: Callback if client needs this to be non-blocking
 * @ctx: Context for this callback
 * If supported by controller, this message list will be sent in bulk to the HW
 * If the client specifies this to be non-blocking, the callback will be
 * called from atomic context.
 */
int slim_bulk_msg_write(struct slim_device *sb, u8 mt, u8 mc,
			struct slim_val_inf msgs[], int n,
			int (*comp_cb)(void *ctx, int err), void *ctx)
{
	int i, ret = 0;

	if (!sb || !sb->ctrl || !msgs || n <= 0)
		return -EINVAL;
	if (!sb->ctrl->xfer_bulk_wr) {
		pr_warn("controller does not support bulk WR, serializing");
		for (i = 0; i < n; i++) {
			struct slim_ele_access ele;

			ele.comp = NULL;
			ele.start_offset = msgs[i].start_offset;
			ele.num_bytes = msgs[i].num_bytes;
			ret = slim_xfer_msg(sb->ctrl, sb, &ele, mc,
					msgs[i].rbuf, msgs[i].wbuf,
					ele.num_bytes);
			if (ret)
				return ret;
		}
		return ret;
	}
	return sb->ctrl->xfer_bulk_wr(sb->ctrl, sb->laddr, mt, mc, msgs, n,
					comp_cb, ctx);
}
EXPORT_SYMBOL(slim_bulk_msg_write);

/*
 * slim_alloc_mgrports: Allocate port on manager side.
 * @sb: device/client handle.
 * @req: Port request type.
 * @nports: Number of ports requested
 * @rh: output buffer to store the port handles
 * @hsz: size of buffer storing handles
 * context: can sleep
 * This port will be typically used by SW. e.g. client driver wants to receive
 * some data from audio codec HW using a data channel.
 * Port allocated using this API will be used to receive the data.
 * If half-duplex ports are requested, two adjacent ports are allocated for
 * 1 half-duplex port. So the handle-buffer size should be twice the number
 * of half-duplex ports to be allocated.
 * -EDQUOT is returned if all ports are in use.
 */
int slim_alloc_mgrports(struct slim_device *sb, enum slim_port_req req,
				int nports, u32 *rh, int hsz)
{
	int i, j;
	int ret = -EINVAL;
	int nphysp = nports;
	struct slim_controller *ctrl = sb->ctrl;

	if (!rh || !ctrl)
		return -EINVAL;
	if (req == SLIM_REQ_HALF_DUP)
		nphysp *= 2;
	if (hsz/sizeof(u32) < nphysp)
		return -EINVAL;
	mutex_lock(&ctrl->m_ctrl);

	for (i = 0; i < ctrl->nports; i++) {
		bool multiok = true;

		if (ctrl->ports[i].state != SLIM_P_FREE)
			continue;
		/* Start half duplex channel at even port */
		if (req == SLIM_REQ_HALF_DUP && (i % 2))
			continue;
		/* Allocate ports contiguously for multi-ch */
		if (ctrl->nports < (i + nphysp)) {
			i = ctrl->nports;
			break;
		}
		if (req == SLIM_REQ_MULTI_CH) {
			multiok = true;
			for (j = i; j < i + nphysp; j++) {
				if (ctrl->ports[j].state != SLIM_P_FREE) {
					multiok = false;
					break;
				}
			}
			if (!multiok)
				continue;
		}
		break;
	}
	if (i >= ctrl->nports) {
		ret = -EDQUOT;
		goto alloc_err;
	}
	ret = 0;
	for (j = i; j < i + nphysp; j++) {
		ctrl->ports[j].state = SLIM_P_UNCFG;
		ctrl->ports[j].req = req;
		if (req == SLIM_REQ_HALF_DUP && (j % 2))
			ctrl->ports[j].flow = SLIM_SINK;
		else
			ctrl->ports[j].flow = SLIM_SRC;
		if (ctrl->alloc_port)
			ret = ctrl->alloc_port(ctrl, j);
		if (ret) {
			for (; j >= i; j--)
				ctrl->ports[j].state = SLIM_P_FREE;
			goto alloc_err;
		}
		*rh++ = SLIM_PORT_HDL(SLIM_LA_MANAGER, 0, j);
	}
alloc_err:
	mutex_unlock(&ctrl->m_ctrl);
	return ret;
}
EXPORT_SYMBOL(slim_alloc_mgrports);

/* Deallocate the port(s) allocated using the API above */
int slim_dealloc_mgrports(struct slim_device *sb, u32 *hdl, int nports)
{
	int i;
	struct slim_controller *ctrl = sb->ctrl;

	if (!ctrl || !hdl)
		return -EINVAL;

	mutex_lock(&ctrl->m_ctrl);

	for (i = 0; i < nports; i++) {
		u8 pn;

		pn = SLIM_HDL_TO_PORT(hdl[i]);

		if (pn >= ctrl->nports || ctrl->ports[pn].state == SLIM_P_CFG) {
			int j, ret;

			if (pn >= ctrl->nports) {
				dev_err(&ctrl->dev, "invalid port number");
				ret = -EINVAL;
			} else {
				dev_err(&ctrl->dev,
					"Can't dealloc connected port:%d", i);
				ret = -EISCONN;
			}
			for (j = i - 1; j >= 0; j--) {
				pn = SLIM_HDL_TO_PORT(hdl[j]);
				ctrl->ports[pn].state = SLIM_P_UNCFG;
			}
			mutex_unlock(&ctrl->m_ctrl);
			return ret;
		}
		if (ctrl->dealloc_port)
			ctrl->dealloc_port(ctrl, pn);
		ctrl->ports[pn].state = SLIM_P_FREE;
	}
	mutex_unlock(&ctrl->m_ctrl);
	return 0;
}
EXPORT_SYMBOL(slim_dealloc_mgrports);

/*
 * slim_config_mgrports: Configure manager side ports
 * @sb: device/client handle.
 * @ph: array of port handles for which this configuration is valid
 * @nports: Number of ports in ph
 * @cfg: configuration requested for port(s)
 * Configure port settings if they are different than the default ones.
 * Returns success if the config could be applied. Returns -EISCONN if the
 * port is in use
 */
int slim_config_mgrports(struct slim_device *sb, u32 *ph, int nports,
				struct slim_port_cfg *cfg)
{
	int i;
	struct slim_controller *ctrl;

	if (!sb || !ph || !nports || !sb->ctrl || !cfg)
		return -EINVAL;

	ctrl = sb->ctrl;
	mutex_lock(&ctrl->sched.m_reconf);
	for (i = 0; i < nports; i++) {
		u8 pn = SLIM_HDL_TO_PORT(ph[i]);

		if (ctrl->ports[pn].state == SLIM_P_CFG)
			return -EISCONN;
		ctrl->ports[pn].cfg = *cfg;
	}
	mutex_unlock(&ctrl->sched.m_reconf);
	return 0;
}
EXPORT_SYMBOL(slim_config_mgrports);

/*
 * slim_get_slaveport: Get slave port handle
 * @la: slave device logical address.
 * @idx: port index at slave
 * @rh: return handle
 * @flw: Flow type (source or destination)
 * This API only returns a slave port's representation as expected by slimbus
 * driver. This port is not managed by the slimbus driver. Caller is expected
 * to have visibility of this port since it's a device-port.
 */
int slim_get_slaveport(u8 la, int idx, u32 *rh, enum slim_port_flow flw)
{
	if (rh == NULL)
		return -EINVAL;
	*rh = SLIM_PORT_HDL(la, flw, idx);
	return 0;
}
EXPORT_SYMBOL(slim_get_slaveport);

static int connect_port_ch(struct slim_controller *ctrl, u8 ch, u32 ph,
				enum slim_port_flow flow)
{
	int ret;
	u8 buf[2];
	u32 la = SLIM_HDL_TO_LA(ph);
	u8 pn = (u8)SLIM_HDL_TO_PORT(ph);
	DEFINE_SLIM_LDEST_TXN(txn, 0, 2, 6, NULL, buf, la);

	if (flow == SLIM_SRC)
		txn.mc = SLIM_MSG_MC_CONNECT_SOURCE;
	else
		txn.mc = SLIM_MSG_MC_CONNECT_SINK;
	buf[0] = pn;
	buf[1] = ctrl->chans[ch].chan;
	if (la == SLIM_LA_MANAGER) {
		ctrl->ports[pn].flow = flow;
		ctrl->ports[pn].ch = &ctrl->chans[ch].prop;
	}
	ret = slim_processtxn(ctrl, &txn, false);
	if (!ret && la == SLIM_LA_MANAGER)
		ctrl->ports[pn].state = SLIM_P_CFG;
	return ret;
}

static int disconnect_port_ch(struct slim_controller *ctrl, u32 ph)
{
	int ret;
	u32 la = SLIM_HDL_TO_LA(ph);
	u8 pn = (u8)SLIM_HDL_TO_PORT(ph);
	DEFINE_SLIM_LDEST_TXN(txn, 0, 1, 5, NULL, &pn, la);

	txn.mc = SLIM_MSG_MC_DISCONNECT_PORT;
	ret = slim_processtxn(ctrl, &txn, false);
	if (ret)
		return ret;
	if (la == SLIM_LA_MANAGER) {
		ctrl->ports[pn].state = SLIM_P_UNCFG;
		ctrl->ports[pn].cfg.watermark = 0;
		ctrl->ports[pn].cfg.port_opts = 0;
		ctrl->ports[pn].ch = NULL;
	}
	return 0;
}

/*
 * slim_connect_src: Connect source port to channel.
 * @sb: client handle
 * @srch: source handle to be connected to this channel
 * @chanh: Channel with which the ports need to be associated with.
 * Per slimbus specification, a channel may have 1 source port.
 * Channel specified in chanh needs to be allocated first.
 * Returns -EALREADY if source is already configured for this channel.
 * Returns -ENOTCONN if channel is not allocated
 * Returns -EINVAL if invalid direction is specified for non-manager port,
 * or if the manager side port number is out of bounds, or in incorrect state
 */
int slim_connect_src(struct slim_device *sb, u32 srch, u16 chanh)
{
	struct slim_controller *ctrl = sb->ctrl;
	int ret;
	u8 chan = SLIM_HDL_TO_CHIDX(chanh);
	struct slim_ich *slc = &ctrl->chans[chan];
	enum slim_port_flow flow = SLIM_HDL_TO_FLOW(srch);
	u8 la = SLIM_HDL_TO_LA(srch);
	u8 pn = SLIM_HDL_TO_PORT(srch);

	/* manager ports don't have direction when they are allocated */
	if (la != SLIM_LA_MANAGER && flow != SLIM_SRC)
		return -EINVAL;

	mutex_lock(&ctrl->sched.m_reconf);

	if (la == SLIM_LA_MANAGER) {
		if (pn >= ctrl->nports ||
			ctrl->ports[pn].state != SLIM_P_UNCFG) {
			ret = -EINVAL;
			goto connect_src_err;
		}
	}

	if (slc->state == SLIM_CH_FREE) {
		ret = -ENOTCONN;
		goto connect_src_err;
	}
	/*
	 * Once channel is removed, its ports can be considered disconnected
	 * So its ports can be reassigned. Source port is zeroed
	 * when channel is deallocated.
	 */
	if (slc->srch) {
		ret = -EALREADY;
		goto connect_src_err;
	}
	ret = connect_port_ch(ctrl, chan, srch, SLIM_SRC);

	if (!ret)
		slc->srch = srch;

connect_src_err:
	mutex_unlock(&ctrl->sched.m_reconf);
	return ret;
}
EXPORT_SYMBOL(slim_connect_src);

/*
 * slim_connect_sink: Connect sink port(s) to channel.
 * @sb: client handle
 * @sinkh: sink handle(s) to be connected to this channel
 * @nsink: number of sinks
 * @chanh: Channel with which the ports need to be associated with.
 * Per slimbus specification, a channel may have multiple sink-ports.
 * Channel specified in chanh needs to be allocated first.
 * Returns -EALREADY if sink is already configured for this channel.
 * Returns -ENOTCONN if channel is not allocated
 * Returns -EINVAL if invalid parameters are passed, or invalid direction is
 * specified for non-manager port, or if the manager side port number is out of
 * bounds, or in incorrect state
 */
int slim_connect_sink(struct slim_device *sb, u32 *sinkh, int nsink, u16 chanh)
{
	struct slim_controller *ctrl = sb->ctrl;
	int j;
	int ret = 0;
	u8 chan = SLIM_HDL_TO_CHIDX(chanh);
	struct slim_ich *slc = &ctrl->chans[chan];
	void *new_sinkh = NULL;

	if (!sinkh || !nsink)
		return -EINVAL;

	mutex_lock(&ctrl->sched.m_reconf);

	/*
	 * Once channel is removed, its ports can be considered disconnected
	 * So its ports can be reassigned. Sink ports are freed when channel
	 * is deallocated.
	 */
	if (slc->state == SLIM_CH_FREE) {
		ret = -ENOTCONN;
		goto connect_sink_err;
	}

	for (j = 0; j < nsink; j++) {
		enum slim_port_flow flow = SLIM_HDL_TO_FLOW(sinkh[j]);
		u8 la = SLIM_HDL_TO_LA(sinkh[j]);
		u8 pn = SLIM_HDL_TO_PORT(sinkh[j]);

		if (la != SLIM_LA_MANAGER && flow != SLIM_SINK)
			ret = -EINVAL;
		else if (la == SLIM_LA_MANAGER &&
			   (pn >= ctrl->nports ||
			    ctrl->ports[pn].state != SLIM_P_UNCFG))
			ret = -EINVAL;
		else
			ret = connect_port_ch(ctrl, chan, sinkh[j], SLIM_SINK);

		if (ret) {
			for (j = j - 1; j >= 0; j--)
				disconnect_port_ch(ctrl, sinkh[j]);
			goto connect_sink_err;
		}
	}

	new_sinkh = krealloc(slc->sinkh, (sizeof(u32) * (slc->nsink + nsink)),
				GFP_KERNEL);
	if (!new_sinkh) {
		ret = -ENOMEM;
		for (j = 0; j < nsink; j++)
			disconnect_port_ch(ctrl, sinkh[j]);
		goto connect_sink_err;
	}

	slc->sinkh = new_sinkh;
	memcpy(slc->sinkh + slc->nsink, sinkh, (sizeof(u32) * nsink));
	slc->nsink += nsink;

connect_sink_err:
	mutex_unlock(&ctrl->sched.m_reconf);
	return ret;
}
EXPORT_SYMBOL(slim_connect_sink);

/*
 * slim_disconnect_ports: Disconnect port(s) from channel
 * @sb: client handle
 * @ph: ports to be disconnected
 * @nph: number of ports.
 * Disconnects ports from a channel.
 */
int slim_disconnect_ports(struct slim_device *sb, u32 *ph, int nph)
{
	struct slim_controller *ctrl = sb->ctrl;
	int i;

	mutex_lock(&ctrl->sched.m_reconf);

	for (i = 0; i < nph; i++)
		disconnect_port_ch(ctrl, ph[i]);
	mutex_unlock(&ctrl->sched.m_reconf);
	return 0;
}
EXPORT_SYMBOL(slim_disconnect_ports);

/*
 * slim_port_xfer: Schedule buffer to be transferred/received using port-handle.
 * @sb: client handle
 * @ph: port-handle
 * @iobuf: buffer to be transferred or populated
 * @len: buffer size.
 * @comp: completion signal to indicate transfer done or error.
 * context: can sleep
 * Returns number of bytes transferred/received if used synchronously.
 * Will return 0 if used asynchronously.
 * Client will call slim_port_get_xfer_status to get error and/or number of
 * bytes transferred if used asynchronously.
 */
int slim_port_xfer(struct slim_device *sb, u32 ph, void *buf, u32 len,
				struct completion *comp)
{
	struct slim_controller *ctrl = sb->ctrl;
	u8 pn = SLIM_HDL_TO_PORT(ph);

	dev_dbg(&ctrl->dev, "port xfer: num:%d", pn);
	return ctrl->port_xfer(ctrl, pn, buf, len, comp);
}
EXPORT_SYMBOL(slim_port_xfer);

/*
 * slim_port_get_xfer_status: Poll for port transfers, or get transfer status
 *	after completion is done.
 * @sb: client handle
 * @ph: port-handle
 * @done_buf: return pointer (iobuf from slim_port_xfer) which is processed.
 * @done_len: Number of bytes transferred.
 * This can be called when port_xfer complition is signalled.
 * The API will return port transfer error (underflow/overflow/disconnect)
 * and/or done_len will reflect number of bytes transferred. Note that
 * done_len may be valid even if port error (overflow/underflow) has happened.
 * e.g. If the transfer was scheduled with a few bytes to be transferred and
 * client has not supplied more data to be transferred, done_len will indicate
 * number of bytes transferred with underflow error. To avoid frequent underflow
 * errors, multiple transfers can be queued (e.g. ping-pong buffers) so that
 * channel has data to be transferred even if client is not ready to transfer
 * data all the time. done_buf will indicate address of the last buffer
 * processed from the multiple transfers.
 */
enum slim_port_err slim_port_get_xfer_status(struct slim_device *sb, u32 ph,
			phys_addr_t *done_buf, u32 *done_len)
{
	struct slim_controller *ctrl = sb->ctrl;
	u8 pn = SLIM_HDL_TO_PORT(ph);
	u32 la = SLIM_HDL_TO_LA(ph);
	enum slim_port_err err;

	dev_dbg(&ctrl->dev, "get status port num:%d", pn);
	/*
	 * Framework only has insight into ports managed by ported device
	 * used by the manager and not slave
	 */
	if (la != SLIM_LA_MANAGER) {
		if (done_buf)
			*done_buf = 0;
		if (done_len)
			*done_len = 0;
		return SLIM_P_NOT_OWNED;
	}
	err = ctrl->port_xfer_status(ctrl, pn, done_buf, done_len);
	if (err == SLIM_P_INPROGRESS)
		err = ctrl->ports[pn].err;
	return err;
}
EXPORT_SYMBOL(slim_port_get_xfer_status);

static void slim_add_ch(struct slim_controller *ctrl, struct slim_ich *slc)
{
	struct slim_ich **arr;
	int i, j;
	int *len;
	int sl = slc->seglen << slc->rootexp;
	/* Channel is already active and other end is transmitting data */
	if (slc->state >= SLIM_CH_ACTIVE)
		return;
	if (slc->coeff == SLIM_COEFF_1) {
		arr = ctrl->sched.chc1;
		len = &ctrl->sched.num_cc1;
	} else {
		arr = ctrl->sched.chc3;
		len = &ctrl->sched.num_cc3;
		sl *= 3;
	}

	*len += 1;

	/* Insert the channel based on rootexp and seglen */
	for (i = 0; i < *len - 1; i++) {
		/*
		 * Primary key: exp low to high.
		 * Secondary key: seglen: high to low
		 */
		if ((slc->rootexp > arr[i]->rootexp) ||
			((slc->rootexp == arr[i]->rootexp) &&
			(slc->seglen < arr[i]->seglen)))
			continue;
		else
			break;
	}
	for (j = *len - 1; j > i; j--)
		arr[j] = arr[j - 1];
	arr[i] = slc;
	if (!ctrl->allocbw)
		ctrl->sched.usedslots += sl;
}

static int slim_remove_ch(struct slim_controller *ctrl, struct slim_ich *slc)
{
	struct slim_ich **arr;
	int i;
	u32 la, ph;
	int *len;

	if (slc->coeff == SLIM_COEFF_1) {
		arr = ctrl->sched.chc1;
		len = &ctrl->sched.num_cc1;
	} else {
		arr = ctrl->sched.chc3;
		len = &ctrl->sched.num_cc3;
	}

	for (i = 0; i < *len; i++) {
		if (arr[i] == slc)
			break;
	}
	if (i >= *len)
		return -EXFULL;
	for (; i < *len - 1; i++)
		arr[i] = arr[i + 1];
	*len -= 1;
	arr[*len] = NULL;

	slc->state = SLIM_CH_ALLOCATED;
	slc->def = 0;
	slc->newintr = 0;
	slc->newoff = 0;
	for (i = 0; i < slc->nsink; i++) {
		ph = slc->sinkh[i];
		la = SLIM_HDL_TO_LA(ph);
		/*
		 * For ports managed by manager's ported device, no need to send
		 * disconnect. It is client's responsibility to call disconnect
		 * on ports owned by the slave device
		 */
		if (la == SLIM_LA_MANAGER) {
			ctrl->ports[SLIM_HDL_TO_PORT(ph)].state = SLIM_P_UNCFG;
			ctrl->ports[SLIM_HDL_TO_PORT(ph)].ch = NULL;
		}
	}

	ph = slc->srch;
	la = SLIM_HDL_TO_LA(ph);
	if (la == SLIM_LA_MANAGER) {
		u8 pn = SLIM_HDL_TO_PORT(ph);

		ctrl->ports[pn].state = SLIM_P_UNCFG;
		ctrl->ports[pn].cfg.watermark = 0;
		ctrl->ports[pn].cfg.port_opts = 0;
	}

	kfree(slc->sinkh);
	slc->sinkh = NULL;
	slc->srch = 0;
	slc->nsink = 0;
	return 0;
}

static u32 slim_calc_prrate(struct slim_controller *ctrl, struct slim_ch *prop)
{
	u32 rate = 0, rate4k = 0, rate11k = 0;
	u32 exp = 0;
	u32 pr = 0;
	bool exact = true;
	bool done = false;
	enum slim_ch_rate ratefam;

	if (prop->prot >= SLIM_ASYNC_SMPLX)
		return 0;
	if (prop->baser == SLIM_RATE_1HZ) {
		rate = prop->ratem / 4000;
		rate4k = rate;
		if (rate * 4000 == prop->ratem)
			ratefam = SLIM_RATE_4000HZ;
		else {
			rate = prop->ratem / 11025;
			rate11k = rate;
			if (rate * 11025 == prop->ratem)
				ratefam = SLIM_RATE_11025HZ;
			else
				ratefam = SLIM_RATE_1HZ;
		}
	} else {
		ratefam = prop->baser;
		rate = prop->ratem;
	}
	if (ratefam == SLIM_RATE_1HZ) {
		exact = false;
		if ((rate4k + 1) * 4000 < (rate11k + 1) * 11025) {
			rate = rate4k + 1;
			ratefam = SLIM_RATE_4000HZ;
		} else {
			rate = rate11k + 1;
			ratefam = SLIM_RATE_11025HZ;
		}
	}
	/* covert rate to coeff-exp */
	while (!done) {
		while ((rate & 0x1) != 0x1) {
			rate >>= 1;
			exp++;
		}
		if (rate > 3) {
			/* roundup if not exact */
			rate++;
			exact = false;
		} else
			done = true;
	}
	if (ratefam == SLIM_RATE_4000HZ) {
		if (rate == 1)
			pr = 0x10;
		else {
			pr = 0;
			exp++;
		}
	} else {
		pr = 8;
		exp++;
	}
	if (exp <= 7) {
		pr |= exp;
		if (exact)
			pr |= 0x80;
	} else
		pr = 0;
	return pr;
}

static int slim_nextdefine_ch(struct slim_device *sb, u8 chan)
{
	struct slim_controller *ctrl = sb->ctrl;
	u32 chrate = 0;
	u32 exp = 0;
	u32 coeff = 0;
	bool exact = true;
	bool done = false;
	int ret = 0;
	struct slim_ich *slc = &ctrl->chans[chan];
	struct slim_ch *prop = &slc->prop;

	slc->prrate = slim_calc_prrate(ctrl, prop);
	dev_dbg(&ctrl->dev, "ch:%d, chan PR rate:%x\n", chan, slc->prrate);
	if (prop->baser == SLIM_RATE_4000HZ)
		chrate = 4000 * prop->ratem;
	else if (prop->baser == SLIM_RATE_11025HZ)
		chrate = 11025 * prop->ratem;
	else
		chrate = prop->ratem;
	/* max allowed sample freq = 768 seg/frame */
	if (chrate > 3600000)
		return -EDQUOT;
	if (prop->baser == SLIM_RATE_4000HZ &&
			ctrl->a_framer->superfreq == 4000)
		coeff = prop->ratem;
	else if (prop->baser == SLIM_RATE_11025HZ &&
			ctrl->a_framer->superfreq == 3675)
		coeff = 3 * prop->ratem;
	else {
		u32 tempr = 0;

		tempr = chrate * SLIM_CL_PER_SUPERFRAME_DIV8;
		coeff = tempr / ctrl->a_framer->rootfreq;
		if (coeff * ctrl->a_framer->rootfreq != tempr) {
			coeff++;
			exact = false;
		}
	}

	/* convert coeff to coeff-exponent */
	exp = 0;
	while (!done) {
		while ((coeff & 0x1) != 0x1) {
			coeff >>= 1;
			exp++;
		}
		if (coeff > 3) {
			coeff++;
			exact = false;
		} else
			done = true;
	}
	if (prop->prot == SLIM_HARD_ISO && !exact)
		return -EPROTONOSUPPORT;
	else if (prop->prot == SLIM_AUTO_ISO) {
		if (exact)
			prop->prot = SLIM_HARD_ISO;
		else
			prop->prot = SLIM_PUSH;
	}
	slc->rootexp = exp;
	slc->seglen = prop->sampleszbits/SLIM_CL_PER_SL;
	if (prop->prot != SLIM_HARD_ISO)
		slc->seglen++;
	if (prop->prot >= SLIM_EXT_SMPLX)
		slc->seglen++;
	/* convert coeff to enum */
	if (coeff == 1) {
		if (exp > 9)
			ret = -EIO;
		coeff = SLIM_COEFF_1;
	} else {
		if (exp > 8)
			ret = -EIO;
		coeff = SLIM_COEFF_3;
	}
	slc->coeff = coeff;

	return ret;
}

/*
 * slim_alloc_ch: Allocate a slimbus channel and return its handle.
 * @sb: client handle.
 * @chanh: return channel handle
 * Slimbus channels are limited to 256 per specification.
 * -EXFULL is returned if all channels are in use.
 * Although slimbus specification supports 256 channels, a controller may not
 * support that many channels.
 */
int slim_alloc_ch(struct slim_device *sb, u16 *chanh)
{
	struct slim_controller *ctrl = sb->ctrl;
	u16 i;

	if (!ctrl)
		return -EINVAL;
	mutex_lock(&ctrl->sched.m_reconf);
	for (i = 0; i < ctrl->nchans; i++) {
		if (ctrl->chans[i].state == SLIM_CH_FREE)
			break;
	}
	if (i >= ctrl->nchans) {
		mutex_unlock(&ctrl->sched.m_reconf);
		return -EXFULL;
	}
	*chanh = i;
	ctrl->chans[i].nextgrp = 0;
	ctrl->chans[i].state = SLIM_CH_ALLOCATED;
	ctrl->chans[i].chan = (u8)(ctrl->reserved + i);

	mutex_unlock(&ctrl->sched.m_reconf);
	return 0;
}
EXPORT_SYMBOL(slim_alloc_ch);

/*
 * slim_query_ch: Get reference-counted handle for a channel number. Every
 * channel is reference counted by upto one as producer and the others as
 * consumer)
 * @sb: client handle
 * @chan: slimbus channel number
 * @chanh: return channel handle
 * If request channel number is not in use, it is allocated, and reference
 * count is set to one. If the channel was was already allocated, this API
 * will return handle to that channel and reference count is incremented.
 * -EXFULL is returned if all channels are in use
 */
int slim_query_ch(struct slim_device *sb, u8 ch, u16 *chanh)
{
	struct slim_controller *ctrl = sb->ctrl;
	u16 i, j;
	int ret = 0;

	if (!ctrl || !chanh)
		return -EINVAL;
	mutex_lock(&ctrl->sched.m_reconf);
	/* start with modulo number */
	i = ch % ctrl->nchans;

	for (j = 0; j < ctrl->nchans; j++) {
		if (ctrl->chans[i].chan == ch) {
			*chanh = i;
			ctrl->chans[i].ref++;
			if (ctrl->chans[i].state == SLIM_CH_FREE)
				ctrl->chans[i].state = SLIM_CH_ALLOCATED;
			goto query_out;
		}
		i = (i + 1) % ctrl->nchans;
	}

	/* Channel not in table yet */
	ret = -EXFULL;
	for (j = 0; j < ctrl->nchans; j++) {
		if (ctrl->chans[i].state == SLIM_CH_FREE) {
			ctrl->chans[i].state =
				SLIM_CH_ALLOCATED;
			*chanh = i;
			ctrl->chans[i].ref++;
			ctrl->chans[i].chan = ch;
			ctrl->chans[i].nextgrp = 0;
			ret = 0;
			break;
		}
		i = (i + 1) % ctrl->nchans;
	}
query_out:
	mutex_unlock(&ctrl->sched.m_reconf);
	dev_dbg(&ctrl->dev, "query ch:%d,hdl:%d,ref:%d,ret:%d",
				ch, i, ctrl->chans[i].ref, ret);
	return ret;
}
EXPORT_SYMBOL(slim_query_ch);

/*
 * slim_dealloc_ch: Deallocate channel allocated using the API above
 * -EISCONN is returned if the channel is tried to be deallocated without
 *  being removed first.
 *  -ENOTCONN is returned if deallocation is tried on a channel that's not
 *  allocated.
 */
int slim_dealloc_ch(struct slim_device *sb, u16 chanh)
{
	struct slim_controller *ctrl = sb->ctrl;
	u8 chan = SLIM_HDL_TO_CHIDX(chanh);
	struct slim_ich *slc = &ctrl->chans[chan];

	if (!ctrl)
		return -EINVAL;

	mutex_lock(&ctrl->sched.m_reconf);
	if (slc->state == SLIM_CH_FREE) {
		mutex_unlock(&ctrl->sched.m_reconf);
		return -ENOTCONN;
	}
	if (slc->ref > 1) {
		slc->ref--;
		mutex_unlock(&ctrl->sched.m_reconf);
		dev_dbg(&ctrl->dev, "remove chan:%d,hdl:%d,ref:%d",
					slc->chan, chanh, slc->ref);
		return 0;
	}
	if (slc->state >= SLIM_CH_PENDING_ACTIVE) {
		dev_err(&ctrl->dev, "Channel:%d should be removed first", chan);
		mutex_unlock(&ctrl->sched.m_reconf);
		return -EISCONN;
	}
	slc->ref--;
	slc->state = SLIM_CH_FREE;
	mutex_unlock(&ctrl->sched.m_reconf);
	dev_dbg(&ctrl->dev, "remove chan:%d,hdl:%d,ref:%d",
				slc->chan, chanh, slc->ref);
	return 0;
}
EXPORT_SYMBOL(slim_dealloc_ch);

/*
 * slim_get_ch_state: Channel state.
 * This API returns the channel's state (active, suspended, inactive etc)
 */
enum slim_ch_state slim_get_ch_state(struct slim_device *sb, u16 chanh)
{
	u8 chan = SLIM_HDL_TO_CHIDX(chanh);
	struct slim_ich *slc = &sb->ctrl->chans[chan];

	return slc->state;
}
EXPORT_SYMBOL(slim_get_ch_state);

/*
 * slim_define_ch: Define a channel.This API defines channel parameters for a
 *	given channel.
 * @sb: client handle.
 * @prop: slim_ch structure with channel parameters desired to be used.
 * @chanh: list of channels to be defined.
 * @nchan: number of channels in a group (1 if grp is false)
 * @grp: Are the channels grouped
 * @grph: return group handle if grouping of channels is desired.
 * Channels can be grouped if multiple channels use same parameters
 * (e.g. 5.1 audio has 6 channels with same parameters. They will all be grouped
 * and given 1 handle for simplicity and avoid repeatedly calling the API)
 * -EISCONN is returned if channel is already used with different parameters.
 * -ENXIO is returned if the channel is not yet allocated.
 */
int slim_define_ch(struct slim_device *sb, struct slim_ch *prop, u16 *chanh,
			u8 nchan, bool grp, u16 *grph)
{
	struct slim_controller *ctrl = sb->ctrl;
	int i, ret = 0;

	if (!ctrl || !chanh || !prop || !nchan)
		return -EINVAL;
	mutex_lock(&ctrl->sched.m_reconf);
	for (i = 0; i < nchan; i++) {
		u8 chan = SLIM_HDL_TO_CHIDX(chanh[i]);
		struct slim_ich *slc = &ctrl->chans[chan];

		dev_dbg(&ctrl->dev, "define_ch: ch:%d, state:%d", chan,
				(int)ctrl->chans[chan].state);
		if (slc->state < SLIM_CH_ALLOCATED) {
			ret = -ENXIO;
			goto err_define_ch;
		}
		if (slc->state >= SLIM_CH_DEFINED && slc->ref >= 2) {
			if (prop->ratem != slc->prop.ratem ||
			prop->sampleszbits != slc->prop.sampleszbits ||
			prop->baser != slc->prop.baser) {
				ret = -EISCONN;
				goto err_define_ch;
			}
		} else if (slc->state > SLIM_CH_DEFINED) {
			ret = -EISCONN;
			goto err_define_ch;
		} else {
			ctrl->chans[chan].prop = *prop;
			ret = slim_nextdefine_ch(sb, chan);
			if (ret)
				goto err_define_ch;
		}
		if (i < (nchan - 1))
			ctrl->chans[chan].nextgrp = chanh[i + 1];
		if (i == 0)
			ctrl->chans[chan].nextgrp |= SLIM_START_GRP;
		if (i == (nchan - 1))
			ctrl->chans[chan].nextgrp |= SLIM_END_GRP;
	}

	if (grp)
		*grph = ((nchan << 8) | SLIM_HDL_TO_CHIDX(chanh[0]));
	for (i = 0; i < nchan; i++) {
		u8 chan = SLIM_HDL_TO_CHIDX(chanh[i]);
		struct slim_ich *slc = &ctrl->chans[chan];

		if (slc->state == SLIM_CH_ALLOCATED)
			slc->state = SLIM_CH_DEFINED;
	}
err_define_ch:
	dev_dbg(&ctrl->dev, "define_ch: ch:%d, ret:%d", *chanh, ret);
	mutex_unlock(&ctrl->sched.m_reconf);
	return ret;
}
EXPORT_SYMBOL(slim_define_ch);

static u32 getsubfrmcoding(u32 *ctrlw, u32 *subfrml, u32 *msgsl)
{
	u32 code = 0;

	if (*ctrlw == *subfrml) {
		*ctrlw = 8;
		*subfrml = 8;
		*msgsl = SLIM_SL_PER_SUPERFRAME - SLIM_FRM_SLOTS_PER_SUPERFRAME
				- SLIM_GDE_SLOTS_PER_SUPERFRAME;
		return 0;
	}
	if (*subfrml == 6) {
		code = 0;
		*msgsl = 256;
	} else if (*subfrml == 8) {
		code = 1;
		*msgsl = 192;
	} else if (*subfrml == 24) {
		code = 2;
		*msgsl = 64;
	} else { /* 32 */
		code = 3;
		*msgsl = 48;
	}

	if (*ctrlw < 8) {
		if (*ctrlw >= 6) {
			*ctrlw = 6;
			code |= 0x14;
		} else {
			if (*ctrlw == 5)
				*ctrlw = 4;
			code |= (*ctrlw << 2);
		}
	} else {
		code -= 2;
		if (*ctrlw >= 24) {
			*ctrlw = 24;
			code |= 0x1e;
		} else if (*ctrlw >= 16) {
			*ctrlw = 16;
			code |= 0x1c;
		} else if (*ctrlw >= 12) {
			*ctrlw = 12;
			code |= 0x1a;
		} else {
			*ctrlw = 8;
			code |= 0x18;
		}
	}

	*msgsl = (*msgsl * *ctrlw) - SLIM_FRM_SLOTS_PER_SUPERFRAME -
				SLIM_GDE_SLOTS_PER_SUPERFRAME;
	return code;
}

static void shiftsegoffsets(struct slim_controller *ctrl, struct slim_ich **ach,
				int sz, u32 shft)
{
	int i;
	u32 oldoff;

	for (i = 0; i < sz; i++) {
		struct slim_ich *slc;

		if (ach[i] == NULL)
			continue;
		slc = ach[i];
		if (slc->state == SLIM_CH_PENDING_REMOVAL)
			continue;
		oldoff = slc->newoff;
		slc->newoff += shft;
		/* seg. offset must be <= interval */
		if (slc->newoff >= slc->newintr)
			slc->newoff -= slc->newintr;
	}
}

static inline int slim_sched_4k_coeff1_chans(struct slim_controller *ctrl,
			struct slim_ich **slc, int *coeff, int *opensl1,
			u32 expshft, u32 curintr, u32 curmaxsl,
			int curexp, int finalexp)
{
	int coeff1;
	struct slim_ich *slc1;

	if (unlikely(!coeff || !slc || !ctrl || !opensl1))
		return -EINVAL;

	coeff1 = *coeff;
	slc1 = *slc;
	while ((coeff1 < ctrl->sched.num_cc1) &&
	       (curexp == (int)(slc1->rootexp + expshft))) {
		if (slc1->state == SLIM_CH_PENDING_REMOVAL) {
			coeff1++;
			slc1 = ctrl->sched.chc1[coeff1];
			continue;
		}
		if (opensl1[1] >= opensl1[0] ||
		    (finalexp == (int)slc1->rootexp &&
		     curintr <= 24 && opensl1[0] == curmaxsl)) {
			opensl1[1] -= slc1->seglen;
			slc1->newoff = curmaxsl + opensl1[1];
			if (opensl1[1] < 0 && opensl1[0] == curmaxsl) {
				opensl1[0] += opensl1[1];
				opensl1[1] = 0;
				if (opensl1[0] < 0) {
					dev_dbg(&ctrl->dev,
						"reconfig failed:%d\n",
						__LINE__);
					return -EXFULL;
				}
			}
		} else {
			if (slc1->seglen > opensl1[0]) {
				dev_dbg(&ctrl->dev,
					"reconfig failed:%d\n",	__LINE__);
				return -EXFULL;
			}
			slc1->newoff = opensl1[0] - slc1->seglen;
			opensl1[0] = slc1->newoff;
		}
		slc1->newintr = curintr;
		coeff1++;
		slc1 = ctrl->sched.chc1[coeff1];
	}
	*coeff = coeff1;
	*slc = slc1;
	return 0;
}

static int slim_sched_chans(struct slim_device *sb, u32 clkgear,
			u32 *ctrlw, u32 *subfrml)
{
	int coeff1, coeff3;
	enum slim_ch_coeff bias;
	struct slim_controller *ctrl = sb->ctrl;
	int last1 = ctrl->sched.num_cc1 - 1;
	int last3 = ctrl->sched.num_cc3 - 1;

	/*
	 * Find first channels with coeff 1 & 3 as starting points for
	 * scheduling
	 */
	for (coeff3 = 0; coeff3 < ctrl->sched.num_cc3; coeff3++) {
		struct slim_ich *slc = ctrl->sched.chc3[coeff3];

		if (slc->state == SLIM_CH_PENDING_REMOVAL)
			continue;
		else
			break;
	}
	for (coeff1 = 0; coeff1 < ctrl->sched.num_cc1; coeff1++) {
		struct slim_ich *slc = ctrl->sched.chc1[coeff1];

		if (slc->state == SLIM_CH_PENDING_REMOVAL)
			continue;
		else
			break;
	}
	if (coeff3 == ctrl->sched.num_cc3 && coeff1 == ctrl->sched.num_cc1) {
		*ctrlw = 8;
		*subfrml = 8;
		return 0;
	} else if (coeff3 == ctrl->sched.num_cc3)
		bias = SLIM_COEFF_1;
	else
		bias = SLIM_COEFF_3;

	/*
	 * Find last chan in coeff1, 3 list, we will use to know when we
	 * have done scheduling all coeff1 channels
	 */
	while (last1 >= 0) {
		if (ctrl->sched.chc1[last1] != NULL &&
			(ctrl->sched.chc1[last1])->state !=
			SLIM_CH_PENDING_REMOVAL)
			break;
		last1--;
	}
	while (last3 >= 0) {
		if (ctrl->sched.chc3[last3] != NULL &&
			(ctrl->sched.chc3[last3])->state !=
			SLIM_CH_PENDING_REMOVAL)
			break;
		last3--;
	}

	if (bias == SLIM_COEFF_1) {
		struct slim_ich *slc1 = ctrl->sched.chc1[coeff1];
		u32 expshft = SLIM_MAX_CLK_GEAR - clkgear;
		int curexp, finalexp;
		u32 curintr, curmaxsl;
		int opensl1[2];
		int maxctrlw1;
		int ret;

		finalexp = (ctrl->sched.chc1[last1])->rootexp;
		curexp = (int)expshft - 1;

		curintr = (SLIM_MAX_INTR_COEFF_1 * 2) >> (curexp + 1);
		curmaxsl = curintr >> 1;
		opensl1[0] = opensl1[1] = curmaxsl;

		while ((coeff1 < ctrl->sched.num_cc1) || (curintr > 24)) {
			curintr >>= 1;
			curmaxsl >>= 1;

			/* update 4K family open slot records */
			if (opensl1[1] < opensl1[0])
				opensl1[1] -= curmaxsl;
			else
				opensl1[1] = opensl1[0] - curmaxsl;
			opensl1[0] = curmaxsl;
			if (opensl1[1] < 0) {
				opensl1[0] += opensl1[1];
				opensl1[1] = 0;
			}
			if (opensl1[0] <= 0) {
				dev_dbg(&ctrl->dev, "reconfig failed:%d\n",
					__LINE__);
				return -EXFULL;
			}
			curexp++;
			/* schedule 4k family channels */
			ret = slim_sched_4k_coeff1_chans(ctrl, &slc1, &coeff1,
					opensl1, expshft, curintr, curmaxsl,
					curexp, finalexp);
			if (ret)
				return ret;
		}
		/* Leave some slots for messaging space */
		if (opensl1[1] <= 0 && opensl1[0] <= 0)
			return -EXFULL;
		if (opensl1[1] > opensl1[0]) {
			int temp = opensl1[0];

			opensl1[0] = opensl1[1];
			opensl1[1] = temp;
			shiftsegoffsets(ctrl, ctrl->sched.chc1,
					ctrl->sched.num_cc1, curmaxsl);
		}
		/* choose subframe mode to maximize bw */
		maxctrlw1 = opensl1[0];
		if (opensl1[0] == curmaxsl)
			maxctrlw1 += opensl1[1];
		if (curintr >= 24) {
			*subfrml = 24;
			*ctrlw = maxctrlw1;
		} else if (curintr == 12) {
			if (maxctrlw1 > opensl1[1] * 4) {
				*subfrml = 24;
				*ctrlw = maxctrlw1;
			} else {
				*subfrml = 6;
				*ctrlw = opensl1[1];
			}
		} else {
			*subfrml = 6;
			*ctrlw = maxctrlw1;
		}
	} else {
		struct slim_ich *slc1 = NULL;
		struct slim_ich *slc3 = ctrl->sched.chc3[coeff3];
		u32 expshft = SLIM_MAX_CLK_GEAR - clkgear;
		int curexp, finalexp, exp1;
		u32 curintr, curmaxsl;
		int opensl3[2];
		int opensl1[6];
		bool opensl1valid = false;
		int maxctrlw1, maxctrlw3, i;

		/* intitalize array to zero */
		memset(opensl1, 0x0, sizeof(opensl1));
		finalexp = (ctrl->sched.chc3[last3])->rootexp;
		if (last1 >= 0) {
			slc1 = ctrl->sched.chc1[coeff1];
			exp1 = (ctrl->sched.chc1[last1])->rootexp;
			if (exp1 > finalexp)
				finalexp = exp1;
		}
		curexp = (int)expshft - 1;

		curintr = (SLIM_MAX_INTR_COEFF_3 * 2) >> (curexp + 1);
		curmaxsl = curintr >> 1;
		opensl3[0] = opensl3[1] = curmaxsl;

		while (coeff1 < ctrl->sched.num_cc1 ||
			coeff3 < ctrl->sched.num_cc3 ||
			curintr > 32) {
			curintr >>= 1;
			curmaxsl >>= 1;

			/* update 12k family open slot records */
			if (opensl3[1] < opensl3[0])
				opensl3[1] -= curmaxsl;
			else
				opensl3[1] = opensl3[0] - curmaxsl;
			opensl3[0] = curmaxsl;
			if (opensl3[1] < 0) {
				opensl3[0] += opensl3[1];
				opensl3[1] = 0;
			}
			if (opensl3[0] <= 0) {
				dev_dbg(&ctrl->dev, "reconfig failed:%d\n",
						__LINE__);
				return -EXFULL;
			}
			curexp++;

			/* schedule 12k family channels */
			while (coeff3 < ctrl->sched.num_cc3 &&
				curexp == (int)slc3->rootexp + expshft) {
				if (slc3->state == SLIM_CH_PENDING_REMOVAL) {
					coeff3++;
					slc3 = ctrl->sched.chc3[coeff3];
					continue;
				}
				opensl1valid = false;
				if (opensl3[1] >= opensl3[0] ||
					(finalexp == (int)slc3->rootexp &&
					 curintr <= 32 &&
					 opensl3[0] == curmaxsl &&
					 last1 < 0)) {
					opensl3[1] -= slc3->seglen;
					slc3->newoff = curmaxsl + opensl3[1];
					if (opensl3[1] < 0 &&
						opensl3[0] == curmaxsl) {
						opensl3[0] += opensl3[1];
						opensl3[1] = 0;
					}
					if (opensl3[0] < 0) {
						dev_dbg(&ctrl->dev,
						"reconfig failed:%d\n",
						__LINE__);
						return -EXFULL;
					}
				} else {
					if (slc3->seglen > opensl3[0]) {
						dev_dbg(&ctrl->dev,
						"reconfig failed:%d\n",
						__LINE__);
						return -EXFULL;
					}
					slc3->newoff = opensl3[0] -
							slc3->seglen;
					opensl3[0] = slc3->newoff;
				}
				slc3->newintr = curintr;
				coeff3++;
				slc3 = ctrl->sched.chc3[coeff3];
			}
			/* update 4k openslot records */
			if (opensl1valid == false) {
				for (i = 0; i < 3; i++) {
					opensl1[i * 2] = opensl3[0];
					opensl1[(i * 2) + 1] = opensl3[1];
				}
			} else {
				int opensl1p[6];

				memcpy(opensl1p, opensl1, sizeof(opensl1));
				for (i = 0; i < 3; i++) {
					if (opensl1p[i] < opensl1p[i + 3])
						opensl1[(i * 2) + 1] =
							opensl1p[i];
					else
						opensl1[(i * 2) + 1] =
							opensl1p[i + 3];
				}
				for (i = 0; i < 3; i++) {
					opensl1[(i * 2) + 1] -= curmaxsl;
					opensl1[i * 2] = curmaxsl;
					if (opensl1[(i * 2) + 1] < 0) {
						opensl1[i * 2] +=
							opensl1[(i * 2) + 1];
						opensl1[(i * 2) + 1] = 0;
					}
					if (opensl1[i * 2] < 0) {
						dev_dbg(&ctrl->dev,
						"reconfig failed:%d\n",
						__LINE__);
						return -EXFULL;
					}
				}
			}
			/* schedule 4k family channels */
			while (coeff1 < ctrl->sched.num_cc1 &&
				curexp == (int)slc1->rootexp + expshft) {
				/* searchorder effective when opensl valid */
				static const int srcho[] = { 5, 2, 4, 1, 3, 0 };
				int maxopensl = 0;
				int maxi = 0;

				if (slc1->state == SLIM_CH_PENDING_REMOVAL) {
					coeff1++;
					slc1 = ctrl->sched.chc1[coeff1];
					continue;
				}
				opensl1valid = true;
				for (i = 0; i < 6; i++) {
					if (opensl1[srcho[i]] > maxopensl) {
						maxopensl = opensl1[srcho[i]];
						maxi = srcho[i];
					}
				}
				opensl1[maxi] -= slc1->seglen;
				slc1->newoff = (curmaxsl * maxi) +
						opensl1[maxi];
				if (opensl1[maxi] < 0 && (maxi & 1) == 1 &&
				    opensl1[maxi - 1] == curmaxsl) {
					opensl1[maxi - 1] += opensl1[maxi];
					if (opensl3[0] > opensl1[maxi - 1])
						opensl3[0] = opensl1[maxi - 1];
					opensl3[1] = 0;
					opensl1[maxi] = 0;
					if (opensl1[maxi - 1] < 0) {
						dev_dbg(&ctrl->dev,
							"reconfig failed:%d\n",
							__LINE__);
						return -EXFULL;
					}
				} else if (opensl1[maxi] < 0) {
					dev_dbg(&ctrl->dev,
						"reconfig failed:%d\n",
						__LINE__);
					return -EXFULL;
				} else if (opensl3[maxi & 1] > opensl1[maxi]) {
					opensl3[maxi & 1] = opensl1[maxi];
				}
				slc1->newintr = curintr * 3;
				coeff1++;
				slc1 = ctrl->sched.chc1[coeff1];
			}
		}
		/* Leave some slots for messaging space */
		if (opensl3[1] <= 0 && opensl3[0] <= 0)
			return -EXFULL;
		/* swap 1st and 2nd bucket if 2nd bucket has more open slots */
		if (opensl3[1] > opensl3[0]) {
			int temp = opensl3[0];

			opensl3[0] = opensl3[1];
			opensl3[1] = temp;
			temp = opensl1[5];
			opensl1[5] = opensl1[4];
			opensl1[4] = opensl1[3];
			opensl1[3] = opensl1[2];
			opensl1[2] = opensl1[1];
			opensl1[1] = opensl1[0];
			opensl1[0] = temp;
			shiftsegoffsets(ctrl, ctrl->sched.chc1,
					ctrl->sched.num_cc1, curmaxsl);
			shiftsegoffsets(ctrl, ctrl->sched.chc3,
					ctrl->sched.num_cc3, curmaxsl);
		}
		/* subframe mode to maximize BW */
		maxctrlw3 = opensl3[0];
		maxctrlw1 = opensl1[0];
		if (opensl3[0] == curmaxsl)
			maxctrlw3 += opensl3[1];
		for (i = 0; i < 5 && opensl1[i] == curmaxsl; i++)
			maxctrlw1 += opensl1[i + 1];
		if (curintr >= 32) {
			*subfrml = 32;
			*ctrlw = maxctrlw3;
		} else if (curintr == 16) {
			if (maxctrlw3 > (opensl3[1] * 4)) {
				*subfrml = 32;
				*ctrlw = maxctrlw3;
			} else {
				*subfrml = 8;
				*ctrlw = opensl3[1];
			}
		} else {
			if ((maxctrlw1 * 8) >= (maxctrlw3 * 24)) {
				*subfrml = 24;
				*ctrlw = maxctrlw1;
			} else {
				*subfrml = 8;
				*ctrlw = maxctrlw3;
			}
		}
	}
	return 0;
}

#ifdef DEBUG
static int slim_verifychansched(struct slim_controller *ctrl, u32 ctrlw,
				u32 subfrml, u32 clkgear)
{
	int sl, i;
	int cc1 = 0;
	int cc3 = 0;
	struct slim_ich *slc = NULL;

	if (!ctrl->sched.slots)
		return 0;
	memset(ctrl->sched.slots, 0, SLIM_SL_PER_SUPERFRAME);
	dev_dbg(&ctrl->dev, "Clock gear is:%d\n", clkgear);
	for (sl = 0; sl < SLIM_SL_PER_SUPERFRAME; sl += subfrml) {
		for (i = 0; i < ctrlw; i++)
			ctrl->sched.slots[sl + i] = 33;
	}
	while (cc1 < ctrl->sched.num_cc1) {
		slc = ctrl->sched.chc1[cc1];
		if (slc == NULL) {
			dev_err(&ctrl->dev, "SLC1 null in verify: chan%d\n",
				cc1);
			return -EIO;
		}
		dev_dbg(&ctrl->dev, "chan:%d, offset:%d, intr:%d, seglen:%d\n",
				(slc - ctrl->chans), slc->newoff,
				slc->newintr, slc->seglen);

		if (slc->state != SLIM_CH_PENDING_REMOVAL) {
			for (sl = slc->newoff;
				sl < SLIM_SL_PER_SUPERFRAME;
				sl += slc->newintr) {
				for (i = 0; i < slc->seglen; i++) {
					if (ctrl->sched.slots[sl + i])
						return -EXFULL;
					ctrl->sched.slots[sl + i] = cc1 + 1;
				}
			}
		}
		cc1++;
	}
	while (cc3 < ctrl->sched.num_cc3) {
		slc = ctrl->sched.chc3[cc3];
		if (slc == NULL) {
			dev_err(&ctrl->dev, "SLC3 null in verify: chan%d\n",
				cc3);
			return -EIO;
		}
		dev_dbg(&ctrl->dev, "chan:%d, offset:%d, intr:%d, seglen:%d\n",
				(slc - ctrl->chans), slc->newoff,
				slc->newintr, slc->seglen);
		if (slc->state != SLIM_CH_PENDING_REMOVAL) {
			for (sl = slc->newoff;
				sl < SLIM_SL_PER_SUPERFRAME;
				sl += slc->newintr) {
				for (i = 0; i < slc->seglen; i++) {
					if (ctrl->sched.slots[sl + i])
						return -EXFULL;
					ctrl->sched.slots[sl + i] = cc3 + 1;
				}
			}
		}
		cc3++;
	}

	return 0;
}
#else
static int slim_verifychansched(struct slim_controller *ctrl, u32 ctrlw,
				u32 subfrml, u32 clkgear)
{
	return 0;
}
#endif

static void slim_sort_chan_grp(struct slim_controller *ctrl,
				struct slim_ich *slc)
{
	u8  last = (u8)-1;
	u8 second = 0;

	for (; last > 0; last--) {
		struct slim_ich *slc1 = slc;
		struct slim_ich *slc2;
		u8 next = SLIM_HDL_TO_CHIDX(slc1->nextgrp);

		slc2 = &ctrl->chans[next];
		for (second = 1; second <= last && slc2 &&
			(slc2->state == SLIM_CH_ACTIVE ||
			 slc2->state == SLIM_CH_PENDING_ACTIVE); second++) {
			if (slc1->newoff > slc2->newoff) {
				u32 temp = slc2->newoff;

				slc2->newoff = slc1->newoff;
				slc1->newoff = temp;
			}
			if (slc2->nextgrp & SLIM_END_GRP) {
				last = second;
				break;
			}
			slc1 = slc2;
			next = SLIM_HDL_TO_CHIDX(slc1->nextgrp);
			slc2 = &ctrl->chans[next];
		}
		if (slc2 == NULL)
			last = second - 1;
	}
}


static int slim_allocbw(struct slim_device *sb, int *subfrmc, int *clkgear)
{
	u32 msgsl = 0;
	u32 ctrlw = 0;
	u32 subfrml = 0;
	int ret = -EIO;
	struct slim_controller *ctrl = sb->ctrl;
	u32 usedsl = ctrl->sched.usedslots + ctrl->sched.pending_msgsl;
	u32 availsl = SLIM_SL_PER_SUPERFRAME - SLIM_FRM_SLOTS_PER_SUPERFRAME -
			SLIM_GDE_SLOTS_PER_SUPERFRAME;
	*clkgear = SLIM_MAX_CLK_GEAR;

	dev_dbg(&ctrl->dev, "used sl:%u, availlable sl:%u\n", usedsl, availsl);
	dev_dbg(&ctrl->dev, "pending:chan sl:%u, :msg sl:%u, clkgear:%u\n",
				ctrl->sched.usedslots,
				ctrl->sched.pending_msgsl, *clkgear);
	/*
	 * If number of slots are 0, that means channels are inactive.
	 * It is very likely that the manager will call clock pause very soon.
	 * By making sure that bus is in MAX_GEAR, clk pause sequence will take
	 * minimum amount of time.
	 */
	if (ctrl->sched.usedslots != 0) {
		while ((usedsl * 2 <= availsl) && (*clkgear > ctrl->min_cg)) {
			*clkgear -= 1;
			usedsl *= 2;
		}
	}

	/*
	 * Try scheduling data channels at current clock gear, if all channels
	 * can be scheduled, or reserved BW can't be satisfied, increase clock
	 * gear and try again
	 */
	for (; *clkgear <= ctrl->max_cg; (*clkgear)++) {
		ret = slim_sched_chans(sb, *clkgear, &ctrlw, &subfrml);

		if (ret == 0) {
			*subfrmc = getsubfrmcoding(&ctrlw, &subfrml, &msgsl);
			if ((msgsl >> (ctrl->max_cg - *clkgear) <
				ctrl->sched.pending_msgsl) &&
				(*clkgear < ctrl->max_cg))
				continue;
			else
				break;
		}
	}
	if (ret == 0) {
		int i;
		/* Sort channel-groups */
		for (i = 0; i < ctrl->sched.num_cc1; i++) {
			struct slim_ich *slc = ctrl->sched.chc1[i];

			if (slc->state == SLIM_CH_PENDING_REMOVAL)
				continue;
			if ((slc->nextgrp & SLIM_START_GRP) &&
				!(slc->nextgrp & SLIM_END_GRP)) {
				slim_sort_chan_grp(ctrl, slc);
			}
		}
		for (i = 0; i < ctrl->sched.num_cc3; i++) {
			struct slim_ich *slc = ctrl->sched.chc3[i];

			if (slc->state == SLIM_CH_PENDING_REMOVAL)
				continue;
			if ((slc->nextgrp & SLIM_START_GRP) &&
				!(slc->nextgrp & SLIM_END_GRP)) {
				slim_sort_chan_grp(ctrl, slc);
			}
		}

		ret = slim_verifychansched(ctrl, ctrlw, subfrml, *clkgear);
	}

	return ret;
}

static void slim_change_existing_chans(struct slim_controller *ctrl, int coeff)
{
	struct slim_ich **arr;
	int len, i;

	if (coeff == SLIM_COEFF_1) {
		arr = ctrl->sched.chc1;
		len = ctrl->sched.num_cc1;
	} else {
		arr = ctrl->sched.chc3;
		len = ctrl->sched.num_cc3;
	}
	for (i = 0; i < len; i++) {
		struct slim_ich *slc = arr[i];

		if (slc->state == SLIM_CH_ACTIVE ||
			slc->state == SLIM_CH_SUSPENDED) {
			slc->offset = slc->newoff;
			slc->interval = slc->newintr;
		}
	}
}
static void slim_chan_changes(struct slim_device *sb, bool revert)
{
	struct slim_controller *ctrl = sb->ctrl;

	while (!list_empty(&sb->mark_define)) {
		struct slim_ich *slc;
		struct slim_pending_ch *pch =
				list_entry(sb->mark_define.next,
					struct slim_pending_ch, pending);
		slc = &ctrl->chans[pch->chan];
		if (revert) {
			if (slc->state == SLIM_CH_PENDING_ACTIVE) {
				u32 sl = slc->seglen << slc->rootexp;

				if (slc->coeff == SLIM_COEFF_3)
					sl *= 3;
				if (!ctrl->allocbw)
					ctrl->sched.usedslots -= sl;
				slim_remove_ch(ctrl, slc);
				slc->state = SLIM_CH_DEFINED;
			}
		} else {
			slc->state = SLIM_CH_ACTIVE;
			slc->def++;
		}
		list_del_init(&pch->pending);
		kfree(pch);
	}

	while (!list_empty(&sb->mark_removal)) {
		struct slim_pending_ch *pch =
				list_entry(sb->mark_removal.next,
					struct slim_pending_ch, pending);
		struct slim_ich *slc = &ctrl->chans[pch->chan];
		u32 sl = slc->seglen << slc->rootexp;

		if (revert || slc->def > 0) {
			if (slc->coeff == SLIM_COEFF_3)
				sl *= 3;
			if (!ctrl->allocbw)
				ctrl->sched.usedslots += sl;
			if (revert)
				slc->def++;
			slc->state = SLIM_CH_ACTIVE;
		} else
			slim_remove_ch(ctrl, slc);
		list_del_init(&pch->pending);
		kfree(pch);
	}

	while (!list_empty(&sb->mark_suspend)) {
		struct slim_pending_ch *pch =
				list_entry(sb->mark_suspend.next,
					struct slim_pending_ch, pending);
		struct slim_ich *slc = &ctrl->chans[pch->chan];

		if (revert)
			slc->state = SLIM_CH_ACTIVE;
		list_del_init(&pch->pending);
		kfree(pch);
	}
	/* Change already active channel if reconfig succeeded */
	if (!revert) {
		slim_change_existing_chans(ctrl, SLIM_COEFF_1);
		slim_change_existing_chans(ctrl, SLIM_COEFF_3);
	}
}

/*
 * slim_reconfigure_now: Request reconfiguration now.
 * @sb: client handle
 * This API does what commit flag in other scheduling APIs do.
 * -EXFULL is returned if there is no space in TDM to reserve the
 * bandwidth. -EBUSY is returned if reconfiguration request is already in
 * progress.
 */
int slim_reconfigure_now(struct slim_device *sb)
{
	u8 i;
	u8 wbuf[4];
	u32 clkgear, subframe;
	u32 curexp;
	int ret;
	struct slim_controller *ctrl = sb->ctrl;
	u32 expshft;
	u32 segdist;
	struct slim_pending_ch *pch;
	DEFINE_SLIM_BCAST_TXN(txn, SLIM_MSG_MC_BEGIN_RECONFIGURATION, 0, 3,
				NULL, NULL, sb->laddr);

	mutex_lock(&ctrl->sched.m_reconf);
	/*
	 * If there are no pending changes from this client, avoid sending
	 * the reconfiguration sequence
	 */
	if (sb->pending_msgsl == sb->cur_msgsl &&
		list_empty(&sb->mark_define) &&
		list_empty(&sb->mark_suspend)) {
		struct list_head *pos, *next;

		list_for_each_safe(pos, next, &sb->mark_removal) {
			struct slim_ich *slc;

			pch = list_entry(pos, struct slim_pending_ch, pending);
			slc = &ctrl->chans[pch->chan];
			if (slc->def > 0)
				slc->def--;
			/* Disconnect source port to free it up */
			if (SLIM_HDL_TO_LA(slc->srch) == sb->laddr)
				slc->srch = 0;
			/*
			 * If controller overrides BW allocation,
			 * delete this in remove channel itself
			 */
			if (slc->def != 0 && !ctrl->allocbw) {
				list_del(&pch->pending);
				kfree(pch);
			}
		}
		if (list_empty(&sb->mark_removal)) {
			mutex_unlock(&ctrl->sched.m_reconf);
			pr_info("SLIM_CL: skip reconfig sequence");
			return 0;
		}
	}

	ctrl->sched.pending_msgsl += sb->pending_msgsl - sb->cur_msgsl;
	list_for_each_entry(pch, &sb->mark_define, pending) {
		struct slim_ich *slc = &ctrl->chans[pch->chan];

		slim_add_ch(ctrl, slc);
		if (slc->state < SLIM_CH_ACTIVE)
			slc->state = SLIM_CH_PENDING_ACTIVE;
	}

	list_for_each_entry(pch, &sb->mark_removal, pending) {
		struct slim_ich *slc = &ctrl->chans[pch->chan];
		u32 sl = slc->seglen << slc->rootexp;

		if (slc->coeff == SLIM_COEFF_3)
			sl *= 3;
		if (!ctrl->allocbw)
			ctrl->sched.usedslots -= sl;
		slc->state = SLIM_CH_PENDING_REMOVAL;
	}
	list_for_each_entry(pch, &sb->mark_suspend, pending) {
		struct slim_ich *slc = &ctrl->chans[pch->chan];

		slc->state = SLIM_CH_SUSPENDED;
	}

	/*
	 * Controller can override default channel scheduling algorithm.
	 * (e.g. if controller needs to use fixed channel scheduling based
	 * on number of channels)
	 */
	if (ctrl->allocbw)
		ret = ctrl->allocbw(sb, &subframe, &clkgear);
	else
		ret = slim_allocbw(sb, &subframe, &clkgear);

	if (!ret) {
		ret = slim_processtxn(ctrl, &txn, false);
		dev_dbg(&ctrl->dev, "sending begin_reconfig:ret:%d\n", ret);
	}

	if (!ret && subframe != ctrl->sched.subfrmcode) {
		wbuf[0] = (u8)(subframe & 0xFF);
		txn.mc = SLIM_MSG_MC_NEXT_SUBFRAME_MODE;
		txn.len = 1;
		txn.rl = 4;
		txn.wbuf = wbuf;
		ret = slim_processtxn(ctrl, &txn, false);
		dev_dbg(&ctrl->dev, "sending subframe:%d,ret:%d\n",
				(int)wbuf[0], ret);
	}
	if (!ret && clkgear != ctrl->clkgear) {
		wbuf[0] = (u8)(clkgear & 0xFF);
		txn.mc = SLIM_MSG_MC_NEXT_CLOCK_GEAR;
		txn.len = 1;
		txn.rl = 4;
		txn.wbuf = wbuf;
		ret = slim_processtxn(ctrl, &txn, false);
		dev_dbg(&ctrl->dev, "sending clkgear:%d,ret:%d\n",
				(int)wbuf[0], ret);
	}
	if (ret)
		goto revert_reconfig;

	expshft = SLIM_MAX_CLK_GEAR - clkgear;
	/* activate/remove channel */
	list_for_each_entry(pch, &sb->mark_define, pending) {
		struct slim_ich *slc = &ctrl->chans[pch->chan];
		/* Define content */
		wbuf[0] = slc->chan;
		wbuf[1] = slc->prrate;
		wbuf[2] = slc->prop.dataf | (slc->prop.auxf << 4);
		wbuf[3] = slc->prop.sampleszbits / SLIM_CL_PER_SL;
		txn.mc = SLIM_MSG_MC_NEXT_DEFINE_CONTENT;
		txn.len = 4;
		txn.rl = 7;
		txn.wbuf = wbuf;
		dev_dbg(&ctrl->dev, "define content, activate:%x, %x, %x, %x\n",
				wbuf[0], wbuf[1], wbuf[2], wbuf[3]);
		/* Right now, channel link bit is not supported */
		ret = slim_processtxn(ctrl, &txn, false);
		if (ret)
			goto revert_reconfig;

		txn.mc = SLIM_MSG_MC_NEXT_ACTIVATE_CHANNEL;
		txn.len = 1;
		txn.rl = 4;
		ret = slim_processtxn(ctrl, &txn, false);
		if (ret)
			goto revert_reconfig;
	}

	list_for_each_entry(pch, &sb->mark_removal, pending) {
		struct slim_ich *slc = &ctrl->chans[pch->chan];

		dev_dbg(&ctrl->dev, "remove chan:%x\n", pch->chan);
		wbuf[0] = slc->chan;
		txn.mc = SLIM_MSG_MC_NEXT_REMOVE_CHANNEL;
		txn.len = 1;
		txn.rl = 4;
		txn.wbuf = wbuf;
		ret = slim_processtxn(ctrl, &txn, false);
		if (ret)
			goto revert_reconfig;
	}
	list_for_each_entry(pch, &sb->mark_suspend, pending) {
		struct slim_ich *slc = &ctrl->chans[pch->chan];

		dev_dbg(&ctrl->dev, "suspend chan:%x\n", pch->chan);
		wbuf[0] = slc->chan;
		txn.mc = SLIM_MSG_MC_NEXT_DEACTIVATE_CHANNEL;
		txn.len = 1;
		txn.rl = 4;
		txn.wbuf = wbuf;
		ret = slim_processtxn(ctrl, &txn, false);
		if (ret)
			goto revert_reconfig;
	}

	/* Define CC1 channel */
	for (i = 0; i < ctrl->sched.num_cc1; i++) {
		struct slim_ich *slc = ctrl->sched.chc1[i];

		if (slc->state == SLIM_CH_PENDING_REMOVAL)
			continue;
		curexp = slc->rootexp + expshft;
		segdist = (slc->newoff << curexp) & 0x1FF;
		expshft = SLIM_MAX_CLK_GEAR - clkgear;
		dev_dbg(&ctrl->dev, "new-intr:%d, old-intr:%d, dist:%d\n",
				slc->newintr, slc->interval, segdist);
		dev_dbg(&ctrl->dev, "new-off:%d, old-off:%d\n",
				slc->newoff, slc->offset);

		if (slc->state < SLIM_CH_ACTIVE || slc->def < slc->ref ||
			slc->newintr != slc->interval ||
			slc->newoff != slc->offset) {
			segdist |= 0x200;
			segdist >>= curexp;
			segdist |= (slc->newoff << (curexp + 1)) & 0xC00;
			wbuf[0] = slc->chan;
			wbuf[1] = (u8)(segdist & 0xFF);
			wbuf[2] = (u8)((segdist & 0xF00) >> 8) |
					(slc->prop.prot << 4);
			wbuf[3] = slc->seglen;
			txn.mc = SLIM_MSG_MC_NEXT_DEFINE_CHANNEL;
			txn.len = 4;
			txn.rl = 7;
			txn.wbuf = wbuf;
			ret = slim_processtxn(ctrl, &txn, false);
			if (ret)
				goto revert_reconfig;
		}
	}

	/* Define CC3 channels */
	for (i = 0; i < ctrl->sched.num_cc3; i++) {
		struct slim_ich *slc = ctrl->sched.chc3[i];

		if (slc->state == SLIM_CH_PENDING_REMOVAL)
			continue;
		curexp = slc->rootexp + expshft;
		segdist = (slc->newoff << curexp) & 0x1FF;
		expshft = SLIM_MAX_CLK_GEAR - clkgear;
		dev_dbg(&ctrl->dev, "new-intr:%d, old-intr:%d, dist:%d\n",
				slc->newintr, slc->interval, segdist);
		dev_dbg(&ctrl->dev, "new-off:%d, old-off:%d\n",
				slc->newoff, slc->offset);

		if (slc->state < SLIM_CH_ACTIVE || slc->def < slc->ref ||
			slc->newintr != slc->interval ||
			slc->newoff != slc->offset) {
			segdist |= 0x200;
			segdist >>= curexp;
			segdist |= 0xC00;
			wbuf[0] = slc->chan;
			wbuf[1] = (u8)(segdist & 0xFF);
			wbuf[2] = (u8)((segdist & 0xF00) >> 8) |
					(slc->prop.prot << 4);
			wbuf[3] = (u8)(slc->seglen);
			txn.mc = SLIM_MSG_MC_NEXT_DEFINE_CHANNEL;
			txn.len = 4;
			txn.rl = 7;
			txn.wbuf = wbuf;
			ret = slim_processtxn(ctrl, &txn, false);
			if (ret)
				goto revert_reconfig;
		}
	}
	txn.mc = SLIM_MSG_MC_RECONFIGURE_NOW;
	txn.len = 0;
	txn.rl = 3;
	txn.wbuf = NULL;
	ret = slim_processtxn(ctrl, &txn, false);
	dev_dbg(&ctrl->dev, "reconfig now:ret:%d\n", ret);
	if (!ret) {
		ctrl->sched.subfrmcode = subframe;
		ctrl->clkgear = clkgear;
		ctrl->sched.msgsl = ctrl->sched.pending_msgsl;
		sb->cur_msgsl = sb->pending_msgsl;
		slim_chan_changes(sb, false);
		mutex_unlock(&ctrl->sched.m_reconf);
		return 0;
	}

revert_reconfig:
	/* Revert channel changes */
	slim_chan_changes(sb, true);
	mutex_unlock(&ctrl->sched.m_reconf);
	return ret;
}
EXPORT_SYMBOL(slim_reconfigure_now);

static int add_pending_ch(struct list_head *listh, u8 chan)
{
	struct slim_pending_ch *pch;

	pch = kmalloc(sizeof(struct slim_pending_ch), GFP_KERNEL);
	if (!pch)
		return -ENOMEM;
	pch->chan = chan;
	list_add_tail(&pch->pending, listh);
	return 0;
}

/*
 * slim_control_ch: Channel control API.
 * @sb: client handle
 * @chanh: group or channel handle to be controlled
 * @chctrl: Control command (activate/suspend/remove)
 * @commit: flag to indicate whether the control should take effect right-away.
 * This API activates, removes or suspends a channel (or group of channels)
 * chanh indicates the channel or group handle (returned by the define_ch API).
 * Reconfiguration may be time-consuming since it can change all other active
 * channel allocations on the bus, change in clock gear used by the slimbus,
 * and change in the control space width used for messaging.
 * commit makes sure that multiple channels can be activated/deactivated before
 * reconfiguration is started.
 * -EXFULL is returned if there is no space in TDM to reserve the bandwidth.
 * -EISCONN/-ENOTCONN is returned if the channel is already connected or not
 * yet defined.
 * -EINVAL is returned if individual control of a grouped-channel is attempted.
 */
int slim_control_ch(struct slim_device *sb, u16 chanh,
			enum slim_ch_control chctrl, bool commit)
{
	struct slim_controller *ctrl = sb->ctrl;
	int ret = 0;
	/* Get rid of the group flag in MSB if any */
	u8 chan = SLIM_HDL_TO_CHIDX(chanh);
	u8 nchan = 0;
	struct slim_ich *slc = &ctrl->chans[chan];

	if (!(slc->nextgrp & SLIM_START_GRP))
		return -EINVAL;

	mutex_lock(&sb->sldev_reconf);
	mutex_lock(&ctrl->sched.m_reconf);
	do {
		struct slim_pending_ch *pch;
		u8 add_mark_removal  = true;

		slc = &ctrl->chans[chan];
		dev_dbg(&ctrl->dev, "chan:%d,ctrl:%d,def:%d", chan, chctrl,
					slc->def);
		if (slc->state < SLIM_CH_DEFINED) {
			ret = -ENOTCONN;
			break;
		}
		if (chctrl == SLIM_CH_SUSPEND) {
			ret = add_pending_ch(&sb->mark_suspend, chan);
			if (ret)
				break;
		} else if (chctrl == SLIM_CH_ACTIVATE) {
			if (slc->state > SLIM_CH_ACTIVE) {
				ret = -EISCONN;
				break;
			}
			ret = add_pending_ch(&sb->mark_define, chan);
			if (ret)
				break;
		} else {
			if (slc->state < SLIM_CH_ACTIVE) {
				ret = -ENOTCONN;
				break;
			}
			/* If channel removal request comes when pending
			 * in the mark_define, remove it from the define
			 * list instead of adding it to removal list
			 */
			if (!list_empty(&sb->mark_define)) {
				struct list_head *pos, *next;

				list_for_each_safe(pos, next,
						  &sb->mark_define) {
					pch = list_entry(pos,
						struct slim_pending_ch,
						pending);
					if (pch->chan == chan) {
						list_del(&pch->pending);
						kfree(pch);
						add_mark_removal = false;
						break;
					}
				}
			}
			if (add_mark_removal == true) {
				ret = add_pending_ch(&sb->mark_removal, chan);
				if (ret)
					break;
			}
		}

		nchan++;
		if (nchan < SLIM_GRP_TO_NCHAN(chanh))
			chan = SLIM_HDL_TO_CHIDX(slc->nextgrp);
	} while (nchan < SLIM_GRP_TO_NCHAN(chanh));
	mutex_unlock(&ctrl->sched.m_reconf);
	if (!ret && commit == true)
		ret = slim_reconfigure_now(sb);
	mutex_unlock(&sb->sldev_reconf);
	return ret;
}
EXPORT_SYMBOL(slim_control_ch);

/*
 * slim_reservemsg_bw: Request to reserve bandwidth for messages.
 * @sb: client handle
 * @bw_bps: message bandwidth in bits per second to be requested
 * @commit: indicates whether the reconfiguration needs to be acted upon.
 * This API call can be grouped with slim_control_ch API call with only one of
 * the APIs specifying the commit flag to avoid reconfiguration being called too
 * frequently. -EXFULL is returned if there is no space in TDM to reserve the
 * bandwidth. -EBUSY is returned if reconfiguration is requested, but a request
 * is already in progress.
 */
int slim_reservemsg_bw(struct slim_device *sb, u32 bw_bps, bool commit)
{
	struct slim_controller *ctrl = sb->ctrl;
	int ret = 0;
	int sl;

	mutex_lock(&sb->sldev_reconf);
	if ((bw_bps >> 3) >= ctrl->a_framer->rootfreq)
		sl = SLIM_SL_PER_SUPERFRAME;
	else {
		sl = (bw_bps * (SLIM_CL_PER_SUPERFRAME_DIV8/SLIM_CL_PER_SL/2) +
			(ctrl->a_framer->rootfreq/2 - 1)) /
			(ctrl->a_framer->rootfreq/2);
	}
	dev_dbg(&ctrl->dev, "request:bw:%d, slots:%d, current:%d\n", bw_bps, sl,
						sb->cur_msgsl);
	sb->pending_msgsl = sl;
	if (commit == true)
		ret = slim_reconfigure_now(sb);
	mutex_unlock(&sb->sldev_reconf);
	return ret;
}
EXPORT_SYMBOL(slim_reservemsg_bw);

/*
 * slim_ctrl_clk_pause: Called by slimbus controller to request clock to be
 *	paused or woken up out of clock pause
 * or woken up from clock pause
 * @ctrl: controller requesting bus to be paused or woken up
 * @wakeup: Wakeup this controller from clock pause.
 * @restart: Restart time value per spec used for clock pause. This value
 *	isn't used when controller is to be woken up.
 * This API executes clock pause reconfiguration sequence if wakeup is false.
 * If wakeup is true, controller's wakeup is called
 * Slimbus clock is idle and can be disabled by the controller later.
 */
int slim_ctrl_clk_pause(struct slim_controller *ctrl, bool wakeup, u8 restart)
{
	int ret = 0;
	int i;
	DEFINE_SLIM_BCAST_TXN(txn, SLIM_MSG_CLK_PAUSE_SEQ_FLG |
				SLIM_MSG_MC_BEGIN_RECONFIGURATION, 0, 3,
				NULL, NULL, 0);

	if (wakeup == false && restart > SLIM_CLK_UNSPECIFIED)
		return -EINVAL;
	mutex_lock(&ctrl->m_ctrl);
	if (wakeup) {
		if (ctrl->clk_state == SLIM_CLK_ACTIVE) {
			mutex_unlock(&ctrl->m_ctrl);
			return 0;
		}
		wait_for_completion(&ctrl->pause_comp);
		/*
		 * Slimbus framework will call controller wakeup
		 * Controller should make sure that it sets active framer
		 * out of clock pause by doing appropriate setting
		 */
		if (ctrl->clk_state == SLIM_CLK_PAUSED && ctrl->wakeup)
			ret = ctrl->wakeup(ctrl);
		/*
		 * If wakeup fails, make sure that next attempt can succeed.
		 * Since we already consumed pause_comp, complete it so
		 * that next wakeup isn't blocked forever
		 */
		if (!ret)
			ctrl->clk_state = SLIM_CLK_ACTIVE;
		else
			complete(&ctrl->pause_comp);
		mutex_unlock(&ctrl->m_ctrl);
		return ret;
	}

	switch (ctrl->clk_state) {
	case SLIM_CLK_ENTERING_PAUSE:
	case SLIM_CLK_PAUSE_FAILED:
		/*
		 * If controller is already trying to enter clock pause,
		 * let it finish.
		 * In case of error, retry
		 * In both cases, previous clock pause has signalled
		 * completion.
		 */
		wait_for_completion(&ctrl->pause_comp);
		/* retry upon failure */
		if (ctrl->clk_state == SLIM_CLK_PAUSE_FAILED) {
			ctrl->clk_state = SLIM_CLK_ACTIVE;
		} else {
			mutex_unlock(&ctrl->m_ctrl);
			/*
			 * Signal completion so that wakeup can wait on
			 * it.
			 */
			complete(&ctrl->pause_comp);
			return 0;
		}
		break;
	case SLIM_CLK_PAUSED:
		/* already paused */
		mutex_unlock(&ctrl->m_ctrl);
		return 0;
	case SLIM_CLK_ACTIVE:
	default:
		break;
	}
	/* Pending response for a message */
	for (i = 0; i < ctrl->last_tid; i++) {
		if (ctrl->txnt[i]) {
			ret = -EBUSY;
			pr_info("slim_clk_pause: txn-rsp for %d pending", i);
			mutex_unlock(&ctrl->m_ctrl);
			return -EBUSY;
		}
	}
	ctrl->clk_state = SLIM_CLK_ENTERING_PAUSE;
	mutex_unlock(&ctrl->m_ctrl);

	mutex_lock(&ctrl->sched.m_reconf);
	/* Data channels active */
	if (ctrl->sched.usedslots) {
		pr_info("slim_clk_pause: data channel active");
		ret = -EBUSY;
		goto clk_pause_ret;
	}

	ret = slim_processtxn(ctrl, &txn, false);
	if (ret)
		goto clk_pause_ret;

	txn.mc = SLIM_MSG_CLK_PAUSE_SEQ_FLG | SLIM_MSG_MC_NEXT_PAUSE_CLOCK;
	txn.len = 1;
	txn.rl = 4;
	txn.wbuf = &restart;
	ret = slim_processtxn(ctrl, &txn, false);
	if (ret)
		goto clk_pause_ret;

	txn.mc = SLIM_MSG_CLK_PAUSE_SEQ_FLG | SLIM_MSG_MC_RECONFIGURE_NOW;
	txn.len = 0;
	txn.rl = 3;
	txn.wbuf = NULL;
	ret = slim_processtxn(ctrl, &txn, false);
	if (ret)
		goto clk_pause_ret;

clk_pause_ret:
	if (ret)
		ctrl->clk_state = SLIM_CLK_PAUSE_FAILED;
	else
		ctrl->clk_state = SLIM_CLK_PAUSED;
	complete(&ctrl->pause_comp);
	mutex_unlock(&ctrl->sched.m_reconf);
	return ret;
}
EXPORT_SYMBOL(slim_ctrl_clk_pause);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Slimbus module");
MODULE_ALIAS("platform:slimbus");
