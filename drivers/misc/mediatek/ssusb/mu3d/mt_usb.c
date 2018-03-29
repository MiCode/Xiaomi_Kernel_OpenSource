/*
 * MUSB OTG controller driver for Blackfin Processors
 *
 * Copyright 2006-2008 Analog Devices Inc.
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/usb/gadget.h>
#include <mt-plat/upmu_common.h>
#include "musb_core.h"


enum mu3d_status {
	MU3D_INIT,
	MU3D_ON,
	MU3D_OFF,
};







/* ================================ */
/* connect and disconnect functions */
/* ================================ */

bool mt_usb_is_device(void)
{
	bool tmp = mtk_is_host_mode();

	mu3d_dbg(K_DEBUG, "%s(%s)\n", __func__, tmp ? "host" : "device");
	return !tmp;
	/* return true; */
}

void connection_work(struct work_struct *data)
{
	struct musb *musb = container_of(to_delayed_work(data), struct musb, connection_work);
	static enum mu3d_status is_on = MU3D_INIT;
	bool is_usb_cable = usb_cable_connected();

	if (is_otg_enabled(musb) && !mt_usb_is_device()) {
#ifdef CONFIG_SSUSB_PROJECT_PHY
		/* usb_fake_powerdown(musb->is_clk_on); */
#endif
		musb->is_clk_on = 0;
		is_on = MU3D_OFF;
		mu3d_dbg(K_INFO, "%s, Host mode. directly return\n", __func__);
		return;
	}
	mu3d_dbg(K_INFO, "%s musb %s, cable %s\n", __func__,
		 (is_on == MU3D_INIT ? "INIT" : (is_on == MU3D_ON ? "ON" : "OFF")),
		 (is_usb_cable ? "IN" : "OUT"));

	if ((is_usb_cable == true) && (is_on != MU3D_ON) && (musb->usb_mode == CABLE_MODE_NORMAL)) {

		is_on = MU3D_ON;

		if (!wake_lock_active(&musb->usb_wakelock))
			wake_lock(&musb->usb_wakelock);

		/* FIXME: Should use usb_udc_start() & usb_gadget_connect(), like usb_udc_softconn_store().
		 * But have no time to think how to handle. However i think it is the correct way.*/
		musb_start(musb);

		mu3d_dbg(K_INFO, "%s ----Connect----\n", __func__);
	} else if (((is_usb_cable == false) && (is_on != MU3D_OFF))
		   || (musb->usb_mode != CABLE_MODE_NORMAL)) {

		is_on = MU3D_OFF;

		if (wake_lock_active(&musb->usb_wakelock))
			wake_unlock(&musb->usb_wakelock);

		/*FIXME: we should use usb_gadget_disconnect() & usb_udc_stop().  like usb_udc_softconn_store().
		 * But have no time to think how to handle. However i think it is the correct way.*/
		musb_stop(musb);

		mu3d_dbg(K_INFO, "%s ----Disconnect----\n", __func__);
	} else {
		/* This if-elseif is to set wakelock when booting with USB cable.
		 * Because battery driver does _NOT_ notify at this codition.*/
#if 0
		if ((is_usb_cable == true) && !wake_lock_active(&musb->usb_wakelock)) {
			mu3d_dbg(K_INFO, "%s Boot wakelock\n", __func__);
			wake_lock(&musb->usb_wakelock);
		} else if ((is_usb_cable == false) && wake_lock_active(&musb->usb_wakelock)) {
			mu3d_dbg(K_INFO, "%s Boot unwakelock\n", __func__);
			wake_unlock(&musb->usb_wakelock);
		}
#endif
		mu3d_dbg(K_INFO, "%s directly return\n", __func__);
	}
}

bool mt_usb_is_ready(void)
{
	if (!_mu3d_musb)
		return false;

	if (_mu3d_musb->usb_mode == CABLE_MODE_CHRG_ONLY ||
		_mu3d_musb->usb_mode == CABLE_MODE_HOST_ONLY)
		return true;

	return _mu3d_musb->softconnect ? true : false;
}

void mt_usb_connect(void)
{
	struct delayed_work *work;

	mu3d_dbg(K_INFO, "%s+\n", __func__);
	if (_mu3d_musb) {
		work = &_mu3d_musb->connection_work;

		/* if(!cancel_delayed_work(work)) */
		/* flush_workqueue(_mu3d_musb->wq); */

		queue_delayed_work(_mu3d_musb->wq, work, 0);
	} else {
		mu3d_dbg(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	mu3d_dbg(K_INFO, "%s-\n", __func__);
}
EXPORT_SYMBOL_GPL(mt_usb_connect);

void mt_usb_disconnect(void)
{
	struct delayed_work *work;

	mu3d_dbg(K_INFO, "%s+\n", __func__);

	if (_mu3d_musb) {
		work = &_mu3d_musb->connection_work;

		/* if(!cancel_delayed_work(work)) */
		/* flush_workqueue(_mu3d_musb->wq); */

		queue_delayed_work(_mu3d_musb->wq, work, 0);
	} else {
		mu3d_dbg(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	mu3d_dbg(K_INFO, "%s-\n", __func__);
}
EXPORT_SYMBOL_GPL(mt_usb_disconnect);

bool usb_cable_connected(void)
{
	u32 ret = false;
#ifndef CONFIG_MTK_FPGA
	u32 chrdet = 0;
#ifdef CONFIG_POWER_EXT
	chrdet = upmu_get_rgs_chrdet();
#else
	chrdet = upmu_is_chr_det();
#endif
	if (chrdet && ((bat_charger_type_detection() == 1) || (bat_charger_type_detection() == 2))) {
		ret = true;
	} else {
		if (_mu3d_musb && need_vbus_chg_int(_mu3d_musb)
		    && is_ssusb_connected_to_pc(_mu3d_musb->ssusb))
			ret = true;
		else
			ret = false;
	}

#endif
	return ret;
}
EXPORT_SYMBOL_GPL(usb_cable_connected);

#ifdef NEVER
void musb_platform_reset(struct musb *musb)
{
	u16 swrst = 0;
	void __iomem *mbase = musb->mregs;

	swrst = musb_readw(mbase, MUSB_SWRST);
	swrst |= (MUSB_SWRST_DISUSBRESET | MUSB_SWRST_SWRST);
	musb_writew(mbase, MUSB_SWRST, swrst);
}
#endif				/* NEVER */

void usb_check_connect(void)
{
	mu3d_dbg(K_INFO, "usb_check_connect\n");

#ifndef CONFIG_MTK_FPGA
	if (usb_cable_connected())
		mt_usb_connect();
#endif

}

void musb_sync_with_bat(struct musb *musb, int usb_state)
{

	bat_charger_update_usb_state(usb_state);

#if 0
	mu3d_dbg(K_INFO, "musb_sync_with_bat\n");

#ifndef CONFIG_MTK_FPGA
	BATTERY_SetUSBState(usb_state);
	wake_up_bat();
#endif
#endif
}
