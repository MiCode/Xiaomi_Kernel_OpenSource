/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/usb/gadget.h>
/*#include "mach/emi_mpu.h"*/

#include "mu3d_hal_osal.h"
#include "musb_core.h"
#if defined(CONFIG_MTK_UART_USB_SWITCH) || defined(CONFIG_MTK_SIB_USB_SWITCH)
#include "mtk-phy-asic.h"
/*#include <mach/mt_typedefs.h>*/
#endif


unsigned int cable_mode = CABLE_MODE_NORMAL;
#ifdef CONFIG_MTK_UART_USB_SWITCH
u32 port_mode = PORT_MODE_USB;
u32 sw_tx = 0;
u32 sw_rx = 0;
u32 sw_uart_path = 0;
#endif

/* ================================ */
/* connect and disconnect functions */
/* ================================ */
bool mt_usb_is_device(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_USB_XHCI_MTK)
	bool tmp = mtk_is_host_mode();

	os_printk(K_INFO, "%s mode\n", tmp ? "HOST" : "DEV");
	return !tmp;
#else
	return true;
#endif
}

enum status { INIT, ON, OFF };
#ifdef CONFIG_USBIF_COMPLIANCE
static enum status connection_work_dev_status = INIT;
void init_connection_work(void)
{
	connection_work_dev_status = INIT;
}
#endif

#ifndef CONFIG_USBIF_COMPLIANCE

struct timespec connect_timestamp = { 0, 0 };

void set_connect_timestamp(void)
{
	connect_timestamp = CURRENT_TIME;
	pr_debug("set timestamp = %llu\n", timespec_to_ns(&connect_timestamp));
}

void clr_connect_timestamp(void)
{
	connect_timestamp.tv_sec = 0;
	connect_timestamp.tv_nsec = 0;
	pr_debug("clr timestamp = %llu\n", timespec_to_ns(&connect_timestamp));
}

struct timespec get_connect_timestamp(void)
{
	pr_debug("get timestamp = %llu\n", timespec_to_ns(&connect_timestamp));
	return connect_timestamp;
}
#endif

void connection_work(struct work_struct *data)
{
	struct musb *musb = container_of(to_delayed_work(data), struct musb, connection_work);
#ifndef CONFIG_USBIF_COMPLIANCE
	static enum status connection_work_dev_status = INIT;
#endif
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);


#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!usb_phy_check_in_uart_mode()) {
#endif
		bool is_usb_cable;

#ifndef CONFIG_FPGA_EARLY_PORTING

		/* delay 100ms if user space is not ready to set usb function */
		if (!is_usb_rdy()) {
			if (__ratelimit(&ratelimit))
				os_printk(K_INFO, "%s, !is_usb_rdy, delay 100ms\n", __func__);
			schedule_delayed_work(&musb->connection_work,
								 msecs_to_jiffies(100));
			return;
		}

		if (!mt_usb_is_device()) {
			connection_work_dev_status = OFF;
			usb_fake_powerdown(musb->is_clk_on);
			musb->is_clk_on = 0;
			os_printk(K_INFO, "%s, Host mode. directly return\n", __func__);
			return;
		}
#endif

		is_usb_cable = usb_cable_connected();
		os_printk(K_INFO, "%s musb %s, cable %s\n", __func__,
			  ((connection_work_dev_status ==
			    0) ? "INIT" : ((connection_work_dev_status == 1) ? "ON" : "OFF")),
			  (is_usb_cable ? "IN" : "OUT"));

		if ((is_usb_cable == true) && (connection_work_dev_status != ON)) {

			connection_work_dev_status = ON;
#ifndef CONFIG_USBIF_COMPLIANCE
			set_connect_timestamp();
#endif

			if (!wake_lock_active(&musb->usb_wakelock))
				wake_lock(&musb->usb_wakelock);

			/* FIXME: Should use usb_udc_start() & usb_gadget_connect(), like usb_udc_softconn_store().
			 * But have no time to think how to handle. However i think it is the correct way.*/
			musb_start(musb);

			os_printk(K_INFO, "%s ----Connect----\n", __func__);
		} else if ((is_usb_cable == false) && (connection_work_dev_status != OFF)) {

			connection_work_dev_status = OFF;
#ifndef CONFIG_USBIF_COMPLIANCE
			clr_connect_timestamp();
#endif

			/*FIXME: we should use usb_gadget_disconnect() & usb_udc_stop().  like usb_udc_softconn_store().
			 * But have no time to think how to handle. However i think it is the correct way.*/
			musb_stop(musb);

			if (wake_lock_active(&musb->usb_wakelock))
				wake_unlock(&musb->usb_wakelock);

			os_printk(K_INFO, "%s ----Disconnect----\n", __func__);
		} else {
			/* This if-elseif is to set wakelock when booting with USB cable.
			 * Because battery driver does _NOT_ notify at this codition.*/
			/* if( (is_usb_cable == true) && !wake_lock_active(&musb->usb_wakelock)) { */
			/* os_printk(K_INFO, "%s Boot wakelock\n", __func__); */
			/* wake_lock(&musb->usb_wakelock); */
			/* } else if( (is_usb_cable == false) && wake_lock_active(&musb->usb_wakelock)) { */
			/* os_printk(K_INFO, "%s Boot unwakelock\n", __func__); */
			/* wake_unlock(&musb->usb_wakelock); */
			/* } */

			os_printk(K_INFO, "%s directly return\n", __func__);
		}
#ifdef CONFIG_MTK_UART_USB_SWITCH
	} else {
#if 0
		usb_fake_powerdown(musb->is_clk_on);
		musb->is_clk_on = 0;
#else
		os_printk(K_INFO, "%s, in UART MODE!!!\n", __func__);
#endif
	}
#endif
}

bool mt_usb_is_ready(void)
{
	os_printk(K_INFO, "USB is ready or not\n");
#ifdef NEVER
	if (!mtk_musb || !mtk_musb->is_ready)
		return false;
	else
		return true;
#endif				/* NEVER */
	return true;
}

void mt_usb_connect(void)
{
	os_printk(K_INFO, "%s+\n", __func__);
	if (_mu3d_musb) {
		struct delayed_work *work;

		work = &_mu3d_musb->connection_work;

		schedule_delayed_work(work, 0);
	} else {
		os_printk(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	os_printk(K_INFO, "%s-\n", __func__);
}
EXPORT_SYMBOL_GPL(mt_usb_connect);

void mt_usb_disconnect(void)
{
	os_printk(K_INFO, "%s+\n", __func__);

	if (_mu3d_musb) {
		struct delayed_work *work;

		work = &_mu3d_musb->connection_work;

		schedule_delayed_work(work, 0);
	} else {
		os_printk(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	os_printk(K_INFO, "%s-\n", __func__);
}
EXPORT_SYMBOL_GPL(mt_usb_disconnect);

/* #define BYPASS_PMIC_LINKAGE */
static CHARGER_TYPE mu3d_hal_get_charger_type(void)
{
	CHARGER_TYPE chg_type;
#ifdef BYPASS_PMIC_LINKAGE
	DBG(0, "force on");
	chg_type = STANDARD_HOST;
#else
	chg_type = mt_get_charger_type();
#endif

	return chg_type;
}
static bool mu3d_hal_is_vbus_exist(void)
{
	bool vbus_exist;

#ifdef BYPASS_PMIC_LINKAGE
	DBG(0, "force on");
	vbus_exist = true;
#else
#ifdef CONFIG_POWER_EXT
	vbus_exist = upmu_get_rgs_chrdet();
#else
	vbus_exist = upmu_is_chr_det();
#endif
#endif

	return vbus_exist;

}

bool usb_cable_connected(void)
{
	CHARGER_TYPE chg_type = CHARGER_UNKNOWN;
	bool connected = false, vbus_exist = false;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
			|| get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
		os_printk(K_INFO, "%s, in KPOC, force USB on\n", __func__);
		return true;
	}
#endif

	if (mu3d_force_on) {
		/* FORCE USB ON */
		chg_type = _mu3d_musb->charger_mode = STANDARD_HOST;
		vbus_exist = true;
		connected = true;
		os_printk(K_INFO, "%s type force to STANDARD_HOST\n", __func__);
	} else {
		/* TYPE CHECK*/
		chg_type = _mu3d_musb->charger_mode = mu3d_hal_get_charger_type();
		if (fake_CDP && chg_type == STANDARD_HOST) {
			os_printk(K_INFO, "%s, fake to type 2\n", __func__);
			chg_type = CHARGING_HOST;
		}

		if (chg_type == STANDARD_HOST || chg_type == CHARGING_HOST)
			connected = true;

		/* VBUS CHECK to avoid type miss-judge */
		vbus_exist = mu3d_hal_is_vbus_exist();
		os_printk(K_INFO, "%s vbus_exist=%d type=%d\n", __func__, vbus_exist, chg_type);
		if (!vbus_exist)
			connected = false;
	}

	/* CMODE CHECK */
	if (cable_mode == CABLE_MODE_CHRG_ONLY || (cable_mode == CABLE_MODE_HOST_ONLY && chg_type != CHARGING_HOST))
		connected = false;

	os_printk(K_INFO, "%s, connected:%d, cable_mode:%d\n", __func__, connected, cable_mode);
	return connected;
}
EXPORT_SYMBOL_GPL(usb_cable_connected);

#ifdef CONFIG_USB_C_SWITCH
int typec_switch_usb_connect(void *data)
{
	struct musb *musb = data;

	os_printk(K_INFO, "%s+\n", __func__);

	if (musb && musb->gadget_driver) {
		struct delayed_work *work;

		work = &musb->connection_work;

		schedule_delayed_work(work, 0);
	} else {
		os_printk(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	os_printk(K_INFO, "%s-\n", __func__);

	return 0;
}

int typec_switch_usb_disconnect(void *data)
{
	struct musb *musb = data;

	os_printk(K_INFO, "%s+\n", __func__);

	if (musb && musb->gadget_driver) {
		struct delayed_work *work;

		work = &musb->connection_work;

		schedule_delayed_work(work, 0);
	} else {
		os_printk(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	os_printk(K_INFO, "%s-\n", __func__);

	return 0;
}
#endif

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


void musb_sync_with_bat(struct musb *musb, int usb_state)
{
	os_printk(K_DEBUG, "musb_sync_with_bat\n");

#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_SMART_BATTERY)
	BATTERY_SetUSBState(usb_state);
	wake_up_bat();
#endif
#endif

}
EXPORT_SYMBOL_GPL(musb_sync_with_bat);


#ifdef CONFIG_USB_MTK_DUALMODE
bool musb_check_ipo_state(void)
{
	bool ipo_off;

	down(&_mu3d_musb->musb_lock);
	ipo_off = _mu3d_musb->in_ipo_off;
	os_printk(K_INFO, "IPO State is %s\n", (ipo_off ? "true" : "false"));
	up(&_mu3d_musb->musb_lock);
	return ipo_off;
}
#endif

/*--FOR INSTANT POWER ON USAGE--------------------------------------------------*/
static inline struct musb *dev_to_musb(struct device *dev)
{
	return dev_get_drvdata(dev);
}

const char *const usb_mode_str[CABLE_MODE_MAX] = { "CHRG_ONLY", "NORMAL", "HOST_ONLY" };

ssize_t musb_cmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		os_printk(K_ERR, "dev is null!!\n");
		return 0;
	}
	return sprintf(buf, "%d\n", cable_mode);
}

ssize_t musb_cmode_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int cmode;
	struct musb *musb;

	if (!dev) {
		os_printk(K_ERR, "dev is null!!\n");
		return count;
	}

	musb = dev_to_musb(dev);

	if (1 == sscanf(buf, "%ud", &cmode)) {
		os_printk(K_INFO, "%s %s --> %s\n", __func__, usb_mode_str[cable_mode],
			  usb_mode_str[cmode]);

		if (cmode >= CABLE_MODE_MAX)
			cmode = CABLE_MODE_NORMAL;

		if (cable_mode != cmode) {

			cable_mode = cmode;

			if (_mu3d_musb) {
				if (down_interruptible(&_mu3d_musb->musb_lock))
					os_printk(K_INFO, "%s: busy, Couldn't get musb_lock\n", __func__);
			}
			if (cmode == CABLE_MODE_CHRG_ONLY) {	/* IPO shutdown, disable USB */
				if (_mu3d_musb)
					_mu3d_musb->in_ipo_off = true;
			} else {	/* IPO bootup, enable USB */
				if (_mu3d_musb)
					_mu3d_musb->in_ipo_off = false;
			}

			if (cmode == CABLE_MODE_CHRG_ONLY) {	/* IPO shutdown, disable USB */
				if (musb) {
					musb->usb_mode = CABLE_MODE_CHRG_ONLY;
					mt_usb_disconnect();
				}
			} else if (cmode == CABLE_MODE_HOST_ONLY) {
				if (musb) {
					musb->usb_mode = CABLE_MODE_HOST_ONLY;
					mt_usb_disconnect();
				}
			} else {	/* IPO bootup, enable USB */
				if (musb) {
					musb->usb_mode = CABLE_MODE_NORMAL;
#ifndef CONFIG_USB_C_SWITCH
					mt_usb_connect();
#else
					typec_switch_usb_connect(musb);
#endif
				}
			}
#ifdef CONFIG_USB_MTK_DUALMODE
			if (cmode == CABLE_MODE_CHRG_ONLY) {
#ifdef CONFIG_USB_C_SWITCH
				;
#else
				/* mask ID pin interrupt even if A-cable is not plugged in */
				switch_int_to_host_and_mask();
				if (mtk_is_host_mode() == true)
					mtk_unload_xhci_on_ipo();
#endif
			} else {
#ifdef CONFIG_USB_C_SWITCH
				;
#else
				switch_int_to_host();	/* resotre ID pin interrupt */
#endif
			}
#endif
			if (_mu3d_musb)
				up(&_mu3d_musb->musb_lock);
		}
	}
	return count;
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
ssize_t musb_portmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		pr_debug("dev is null!!\n");
		return 0;
	}

	if (usb_phy_check_in_uart_mode())
		port_mode = PORT_MODE_UART;
	else
		port_mode = PORT_MODE_USB;

	if (port_mode == PORT_MODE_USB)
		pr_debug("\nUSB Port mode -> USB\n");
	else if (port_mode == PORT_MODE_UART)
		pr_debug("\nUSB Port mode -> UART\n");

	uart_usb_switch_dump_register();

	return scnprintf(buf, PAGE_SIZE, "%d\n", port_mode);
}

ssize_t musb_portmode_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int portmode;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return count;
	} else if (1 == sscanf(buf, "%ud", &portmode)) {
		pr_debug("\nUSB Port mode: current => %d (port_mode), change to => %d (portmode)\n",
			 port_mode, portmode);
		if (portmode >= PORT_MODE_MAX)
			portmode = PORT_MODE_USB;

		if (port_mode != portmode) {
			if (portmode == PORT_MODE_USB) {	/* Changing to USB Mode */
				pr_debug("USB Port mode -> USB\n");
				usb_phy_switch_to_usb();
			} else if (portmode == PORT_MODE_UART) {	/* Changing to UART Mode */
				pr_debug("USB Port mode -> UART\n");
				usb_phy_switch_to_uart();
			}
			uart_usb_switch_dump_register();
			port_mode = portmode;
		}
	}
	return count;
}

ssize_t musb_tx_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 var;
	u8 var2;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return 0;
	}

	var = U3PhyReadReg8((u3phy_addr_t) (U3D_U2PHYDTM1 + 0x2));
	var2 = (var >> 3) & ~0xFE;
	pr_debug("[MUSB]addr: 0x6E (TX), value: %x - %x\n", var, var2);

	sw_tx = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var2);
}

ssize_t musb_tx_store(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	unsigned int val;
	u8 var;
	u8 var2;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return count;
	} else if (1 == sscanf(buf, "%ud", &val)) {
		pr_debug("\n Write TX : %d\n", val);

#ifdef CONFIG_FPGA_EARLY_PORTING
		var = USB_PHY_Read_Register8(U3D_U2PHYDTM1 + 0x2);
#else
		var = U3PhyReadReg8((u3phy_addr_t) (U3D_U2PHYDTM1 + 0x2));
#endif

		if (val == 0)
			var2 = var & ~(1 << 3);
		else
			var2 = var | (1 << 3);

#ifdef CONFIG_FPGA_EARLY_PORTING
		USB_PHY_Write_Register8(var2, U3D_U2PHYDTM1 + 0x2);
		var = USB_PHY_Read_Register8(U3D_U2PHYDTM1 + 0x2);
#else
		/* U3PhyWriteField32(U3D_USBPHYDTM1+0x2,
		   E60802_RG_USB20_BC11_SW_EN_OFST, E60802_RG_USB20_BC11_SW_EN, 0); */
		/* Jeremy TODO 0320 */
		var = U3PhyReadReg8((u3phy_addr_t) (U3D_U2PHYDTM1 + 0x2));
#endif

		var2 = (var >> 3) & ~0xFE;

		pr_debug
		    ("[MUSB]addr: U3D_U2PHYDTM1 (0x6E) TX [AFTER WRITE], value after: %x - %x\n",
		     var, var2);
		sw_tx = var;
	}
	return count;
}

ssize_t musb_rx_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 var;
	u8 var2;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return 0;
	}
#ifdef CONFIG_FPGA_EARLY_PORTING
	var = USB_PHY_Read_Register8(U3D_U2PHYDMON1 + 0x3);
#else
	var = U3PhyReadReg8((u3phy_addr_t) (U3D_U2PHYDMON1 + 0x3));
#endif
	var2 = (var >> 7) & ~0xFE;
	pr_debug("[MUSB]addr: U3D_U2PHYDMON1 (0x77) (RX), value: %x - %x\n", var, var2);
	sw_rx = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var2);
}
ssize_t musb_uart_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 var = 0;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return 0;
	}

	var = DRV_Reg32(ap_uart0_base + 0x600);
	pr_debug("[MUSB]addr: (GPIO Misc) 0x600, value: %x\n\n", DRV_Reg32(ap_uart0_base + 0x600));
	sw_uart_path = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var);
}
#endif

#ifdef CONFIG_MTK_SIB_USB_SWITCH
ssize_t musb_sib_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return 0;
	}
#ifndef CONFIG_FPGA_EARLY_PORTING
	ret = usb_phy_sib_enable_switch_status();
#else
	pr_err("FPGA not support SIB switch!!\n");
	ret = false;
#endif
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

ssize_t musb_sib_enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int mode;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return count;
	} else if (!kstrtouint(buf, 0, &mode)) {
#ifndef CONFIG_FPGA_EARLY_PORTING
		pr_debug("USB sib_enable: %d\n", mode);
		usb_phy_sib_enable_switch(mode);
#else
		pr_err("FPGA not support SIB switch!!\n");
#endif
	}
	return count;
}
#endif

#ifdef NEVER
#ifdef CONFIG_FPGA_EARLY_PORTING
static struct i2c_client *usb_i2c_client;
static const struct i2c_device_id usb_i2c_id[] = { {"mtk-usb", 0}, {} };

static struct i2c_board_info usb_i2c_dev __initdata = { I2C_BOARD_INFO("mtk-usb", 0x60) };


void USB_PHY_Write_Register8(UINT8 var, UINT8 addr)
{
	char buffer[2];

	buffer[0] = addr;
	buffer[1] = var;
	i2c_master_send(usb_i2c_client, &buffer, 2);
}

UINT8 USB_PHY_Read_Register8(UINT8 addr)
{
	UINT8 var;

	i2c_master_send(usb_i2c_client, &addr, 1);
	i2c_master_recv(usb_i2c_client, &var, 1);
	return var;
}

static int usb_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	pr_debug("[MUSB]usb_i2c_probe, start\n");

	usb_i2c_client = client;

	/* disable usb mac suspend */
	DRV_WriteReg8(USB_SIF_BASE + 0x86a, 0x00);

	/* usb phy initial sequence */
	USB_PHY_Write_Register8(0x00, 0xFF);
	USB_PHY_Write_Register8(0x04, 0x61);
	USB_PHY_Write_Register8(0x00, 0x68);
	USB_PHY_Write_Register8(0x00, 0x6a);
	USB_PHY_Write_Register8(0x6e, 0x00);
	USB_PHY_Write_Register8(0x0c, 0x1b);
	USB_PHY_Write_Register8(0x44, 0x08);
	USB_PHY_Write_Register8(0x55, 0x11);
	USB_PHY_Write_Register8(0x68, 0x1a);


	pr_debug("[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	pr_debug("[MUSB]addr: 0x61, value: %x\n", USB_PHY_Read_Register8(0x61));
	pr_debug("[MUSB]addr: 0x68, value: %x\n", USB_PHY_Read_Register8(0x68));
	pr_debug("[MUSB]addr: 0x6a, value: %x\n", USB_PHY_Read_Register8(0x6a));
	pr_debug("[MUSB]addr: 0x00, value: %x\n", USB_PHY_Read_Register8(0x00));
	pr_debug("[MUSB]addr: 0x1b, value: %x\n", USB_PHY_Read_Register8(0x1b));
	pr_debug("[MUSB]addr: 0x08, value: %x\n", USB_PHY_Read_Register8(0x08));
	pr_debug("[MUSB]addr: 0x11, value: %x\n", USB_PHY_Read_Register8(0x11));
	pr_debug("[MUSB]addr: 0x1a, value: %x\n", USB_PHY_Read_Register8(0x1a));


	pr_debug("[MUSB]usb_i2c_probe, end\n");
	return 0;

}

static int usb_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, "mtk-usb");
	return 0;
}

static int usb_i2c_remove(struct i2c_client *client)
{
	return 0;
}


struct i2c_driver usb_i2c_driver = {
	.probe = usb_i2c_probe,
	.remove = usb_i2c_remove,
	.detect = usb_i2c_detect,
	.driver = {
		   .name = "mtk-usb",
		   },
	.id_table = usb_i2c_id,
};

int add_usb_i2c_driver(void)
{
	int ret = 0;

	i2c_register_board_info(0, &usb_i2c_dev, 1);
	if (i2c_add_driver(&usb_i2c_driver) != 0) {
		pr_debug("[MUSB]usb_i2c_driver initialization failed!!\n");
		ret = -1;
	} else
		pr_debug("[MUSB]usb_i2c_driver initialization succeed!!\n");

	return ret;
}
#endif				/* End of CONFIG_FPGA_EARLY_PORTING */
#endif				/* NEVER */
