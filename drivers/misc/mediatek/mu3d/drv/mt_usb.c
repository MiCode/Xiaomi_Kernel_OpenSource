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
#include <linux/workqueue.h>
#include <linux/usb/gadget.h>
#include "mach/emi_mpu.h"

#include <linux/mu3d/hal/mu3d_hal_osal.h>
#include "musb_core.h"
#ifdef CONFIG_MTK_UART_USB_SWITCH
#include <linux/mu3phy/mtk-phy-asic.h>
#include <mach/mt_typedefs.h>
#endif

extern struct musb *_mu3d_musb;

#ifdef CONFIG_MTK_UART_USB_SWITCH
typedef enum
{
    PORT_MODE_USB = 0,
    PORT_MODE_UART,

    PORT_MODE_MAX
} PORT_MODE;

extern bool in_uart_mode;
extern void uart_usb_switch_dump_register(void);
extern bool usb_phy_check_in_uart_mode(void);
extern void usb_phy_switch_to_usb(void);
extern void usb_phy_switch_to_uart(void);
extern void __iomem *ap_uart0_base;
#endif

#ifndef FOR_BRING_UP
#ifndef CONFIG_MTK_FPGA
extern void wake_up_bat(void);
#endif
#endif

#ifdef FOR_BRING_UP

static inline void BATTERY_SetUSBState(int usb_state) {};
static inline CHARGER_TYPE mt_charger_type_detection(void) { return STANDARD_HOST; };
static inline bool upmu_is_chr_det(void) { return true; };
static inline u32 upmu_get_rgs_chrdet(void) { return 1; };

#else /* NOT CONFIG_ARM64 */

extern CHARGER_TYPE mt_charger_type_detection(void);
extern bool upmu_is_chr_det(void);
extern void BATTERY_SetUSBState(int usb_state);
extern u32 upmu_get_rgs_chrdet(void);

#endif

#ifdef CONFIG_MTK_XHCI
extern bool mtk_is_host_mode(void);
#else
int mtk_is_host_mode(void){return 0;}
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
#ifndef CONFIG_MTK_FPGA
	bool tmp = mtk_is_host_mode();
	os_printk(K_INFO, "%s mode\n", tmp?"HOST":"DEV");

	return !tmp;
#else
	return true;
#endif
}

enum status{INIT, ON, OFF};
#ifdef CONFIG_USBIF_COMPLIANCE
static enum status connection_work_dev_status = INIT;
void init_connection_work(void){
	connection_work_dev_status = INIT;
}
#endif

#ifndef CONFIG_USBIF_COMPLIANCE

struct timespec connect_timestamp = { 0, 0};

void set_connect_timestamp(void)
{
	connect_timestamp = CURRENT_TIME;
	pr_debug( "set timestamp = %llu\n", timespec_to_ns(&connect_timestamp));
}

void clr_connect_timestamp(void)
{
	connect_timestamp.tv_sec = 0;
	connect_timestamp.tv_nsec = 0;
	pr_debug( "clr timestamp = %llu\n", timespec_to_ns(&connect_timestamp));
}

struct timespec get_connect_timestamp(void)
{
	pr_debug( "get timestamp = %llu\n", timespec_to_ns(&connect_timestamp));
	return connect_timestamp;
}
#endif

void connection_work(struct work_struct *data)
{
	struct musb *musb = container_of(to_delayed_work(data), struct musb, connection_work);
#ifndef CONFIG_USBIF_COMPLIANCE
	static enum status connection_work_dev_status = INIT;
#endif
#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!usb_phy_check_in_uart_mode()) {
#endif
	bool is_usb_cable = usb_cable_connected();

#ifndef CONFIG_MTK_FPGA
	if(!mt_usb_is_device()){
		connection_work_dev_status = OFF;
		usb_fake_powerdown(musb->is_clk_on);
		musb->is_clk_on = 0;
		os_printk(K_INFO, "%s, Host mode. directly return\n", __func__);
		return;
	}
#endif

	os_printk(K_INFO, "%s musb %s, cable %s\n", __func__, ((connection_work_dev_status==0)?"INIT":((connection_work_dev_status==1)?"ON":"OFF")), (is_usb_cable?"IN":"OUT"));

	if ((is_usb_cable == true) && (connection_work_dev_status != ON) && (musb->usb_mode == CABLE_MODE_NORMAL)) {

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
	} else if (((is_usb_cable == false) && (connection_work_dev_status != OFF)) || (musb->usb_mode != CABLE_MODE_NORMAL)) {

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
		//if( (is_usb_cable == true) && !wake_lock_active(&musb->usb_wakelock)) {
		//	os_printk(K_INFO, "%s Boot wakelock\n", __func__);
		//	wake_lock(&musb->usb_wakelock);
		//} else if( (is_usb_cable == false) && wake_lock_active(&musb->usb_wakelock)) {
		//	os_printk(K_INFO, "%s Boot unwakelock\n", __func__);
		//	wake_unlock(&musb->usb_wakelock);
		//}

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
	if(!mtk_musb || !mtk_musb->is_ready)
		return false;
	else
		return true;
#endif /* NEVER */
	return true;
}

void mt_usb_connect(void)
{
	os_printk(K_INFO, "%s+\n", __func__);
	if(_mu3d_musb) {
		struct delayed_work *work;

		work = &_mu3d_musb->connection_work;

		schedule_delayed_work_on(0, work, 0);
	} else {
		os_printk(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	os_printk(K_INFO, "%s-\n", __func__);
}
EXPORT_SYMBOL_GPL(mt_usb_connect);

void mt_usb_disconnect(void)
{
	os_printk(K_INFO, "%s+\n", __func__);

	if(_mu3d_musb) {
		struct delayed_work *work;

		work = &_mu3d_musb->connection_work;

		schedule_delayed_work_on(0, work, 0);
	} else {
		os_printk(K_INFO, "%s musb_musb not ready\n", __func__);
	}
	os_printk(K_INFO, "%s-\n", __func__);
}
EXPORT_SYMBOL_GPL(mt_usb_disconnect);

bool usb_cable_connected(void)
{
#ifndef CONFIG_MTK_FPGA
#ifdef CONFIG_POWER_EXT
	CHARGER_TYPE chg_type = mt_charger_type_detection();
	os_printk(K_INFO, "%s ext-chrdet=%d type=%d\n", __func__, upmu_get_rgs_chrdet(), chg_type);
	if (upmu_get_rgs_chrdet() && (chg_type == STANDARD_HOST))
	{
		return true;
	}
#else
	if (upmu_is_chr_det())
	{
		CHARGER_TYPE chg_type = mt_charger_type_detection();
		os_printk(K_INFO, "%s type=%d\n", __func__, chg_type);
		if (chg_type == STANDARD_HOST)
			return true;
	}
#endif
	os_printk(K_INFO, "%s no USB Host detect!\n", __func__);

	return false;
#else
	os_printk(K_INFO, "%s [FPGA] always true\n", __func__);

	return true;
#endif
}
EXPORT_SYMBOL_GPL(usb_cable_connected);

#ifdef NEVER
void musb_platform_reset(struct musb *musb)
{
	u16 swrst = 0;
	void __iomem	*mbase = musb->mregs;
	swrst = musb_readw(mbase,MUSB_SWRST);
	swrst |= (MUSB_SWRST_DISUSBRESET | MUSB_SWRST_SWRST);
	musb_writew(mbase, MUSB_SWRST,swrst);
}
#endif /* NEVER */

void usb_check_connect(void)
{
	os_printk(K_INFO, "usb_check_connect\n");

#ifndef CONFIG_MTK_FPGA
	if (usb_cable_connected())
		mt_usb_connect();
#endif

}

void musb_sync_with_bat(struct musb *musb, int usb_state)
{
	os_printk(K_INFO, "musb_sync_with_bat\n");

#ifndef CONFIG_MTK_FPGA
	BATTERY_SetUSBState(usb_state);
#ifndef FOR_BRING_UP
	wake_up_bat();
#endif
#endif

}
EXPORT_SYMBOL_GPL(musb_sync_with_bat);

/*--FOR INSTANT POWER ON USAGE--------------------------------------------------*/
static inline struct musb *dev_to_musb(struct device *dev)
{
	return dev_get_drvdata(dev);
}

const char* const usb_mode_str[CABLE_MODE_MAX] = {"CHRG_ONLY", "NORMAL", "HOST_ONLY"};

ssize_t musb_cmode_show(struct device* dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		os_printk(K_ERR, "dev is null!!\n");
		return 0;
	}
	return sprintf(buf, "%d\n", cable_mode);
}

ssize_t musb_cmode_store(struct device* dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int cmode;
	struct musb	*musb = dev_to_musb(dev);

	if (!dev) {
		os_printk(K_ERR, "dev is null!!\n");
		return count;
	} else if (1 == sscanf(buf, "%d", &cmode)) {
		os_printk(K_INFO, "%s %s --> %s\n", __func__, usb_mode_str[cable_mode], usb_mode_str[cmode]);

		if (cmode >= CABLE_MODE_MAX)
			cmode = CABLE_MODE_NORMAL;

		if (cable_mode != cmode) {
			if (cmode == CABLE_MODE_CHRG_ONLY) { // IPO shutdown, disable USB
				if(musb) {
					musb->usb_mode = CABLE_MODE_CHRG_ONLY;
					mt_usb_disconnect();
				}
			} else if (cmode == CABLE_MODE_HOST_ONLY) {
				if(musb) {
					musb->usb_mode = CABLE_MODE_HOST_ONLY;
					mt_usb_disconnect();
				}
			} else { // IPO bootup, enable USB
				if(musb) {
					musb->usb_mode = CABLE_MODE_NORMAL;
					mt_usb_connect();
				}
			}
			cable_mode = cmode;
		}
	}
	return count;
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
ssize_t musb_portmode_show(struct device* dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		printk("dev is null!!\n");
		return 0;
	}

	if (usb_phy_check_in_uart_mode())
		port_mode = PORT_MODE_UART;
	else
		port_mode = PORT_MODE_USB;

	if (port_mode == PORT_MODE_USB) {
		printk("\nUSB Port mode -> USB\n");
	} else if (port_mode == PORT_MODE_UART) {
		printk("\nUSB Port mode -> UART\n");
	}
	uart_usb_switch_dump_register();

	return scnprintf(buf, PAGE_SIZE, "%d\n", port_mode);
}

ssize_t musb_portmode_store(struct device* dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int portmode;

	if (!dev) {
		printk("dev is null!!\n");
		return count;
	} else if (1 == sscanf(buf, "%d", &portmode)) {
		printk("\nUSB Port mode: current => %d (port_mode), change to => %d (portmode)\n", port_mode, portmode);
		if (portmode >= PORT_MODE_MAX)
			portmode = PORT_MODE_USB;

		if (port_mode != portmode) {
			if(portmode == PORT_MODE_USB) { // Changing to USB Mode
				printk("USB Port mode -> USB\n");
				usb_phy_switch_to_usb();
			} else if(portmode == PORT_MODE_UART) { // Changing to UART Mode
				printk("USB Port mode -> UART\n");
				usb_phy_switch_to_uart();
			}
			uart_usb_switch_dump_register();
			port_mode = portmode;
		}
	}
	return count;
}

ssize_t musb_tx_show(struct device* dev, struct device_attribute *attr, char *buf)
{
	u8 var;
	u8 var2;

	if (!dev) {
		printk("dev is null!!\n");
		return 0;
	}

	var = U3PhyReadReg8(U3D_U2PHYDTM1+0x2);
	var2 = (var >> 3) & ~0xFE;
	printk("[MUSB]addr: 0x6E (TX), value: %x - %x\n", var, var2);

	sw_tx = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var2);
}

ssize_t musb_tx_store(struct device* dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int val;
	u8 var;
	u8 var2;

	if (!dev) {
		printk("dev is null!!\n");
		return count;
	} else if (1 == sscanf(buf, "%d", &val)) {
		printk("\n Write TX : %d\n", val);

#ifdef CONFIG_MTK_FPGA
		var = USB_PHY_Read_Register8(U3D_U2PHYDTM1+0x2);
#else
        var = U3PhyReadReg8(U3D_U2PHYDTM1+0x2);
#endif

		if (val == 0) {
			var2 = var & ~(1 << 3);
		} else {
			var2 = var | (1 << 3);
		}

#ifdef CONFIG_MTK_FPGA
		USB_PHY_Write_Register8(var2, U3D_U2PHYDTM1+0x2);
		var = USB_PHY_Read_Register8(U3D_U2PHYDTM1+0x2);
#else
        //U3PhyWriteField32(U3D_USBPHYDTM1+0x2, E60802_RG_USB20_BC11_SW_EN_OFST, E60802_RG_USB20_BC11_SW_EN, 0);
        //Jeremy TODO 0320
		var = U3PhyReadReg8(U3D_U2PHYDTM1+0x2);
#endif

		var2 = (var >> 3) & ~0xFE;

		printk("[MUSB]addr: U3D_U2PHYDTM1 (0x6E) TX [AFTER WRITE], value after: %x - %x\n", var, var2);
		sw_tx = var;
	}
	return count;
}

ssize_t musb_rx_show(struct device* dev, struct device_attribute *attr, char *buf)
{
	u8 var;
	u8 var2;

	if (!dev) {
		printk("dev is null!!\n");
		return 0;
	}

#ifdef CONFIG_MTK_FPGA
	var = USB_PHY_Read_Register8(U3D_U2PHYDMON1+0x3);
#else
    var = U3PhyReadReg8(U3D_U2PHYDMON1+0x3);
#endif
	var2 = (var >> 7) & ~0xFE;
	printk("[MUSB]addr: U3D_U2PHYDMON1 (0x77) (RX), value: %x - %x\n", var, var2);
	sw_rx = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var2);
}

ssize_t musb_uart_path_show(struct device* dev, struct device_attribute *attr, char *buf)
{
	u8 var=0;

	if (!dev) {
		printk("dev is null!!\n");
		return 0;
	}

	var = DRV_Reg8(ap_uart0_base + 0xB0);
	printk("[MUSB]addr: (UART0) 0xB0, value: %x\n\n", DRV_Reg8(ap_uart0_base + 0xB0));
	sw_uart_path = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var);
}
#endif

#ifdef NEVER
#ifdef CONFIG_MTK_FPGA
static struct i2c_client *usb_i2c_client = NULL;
static const struct i2c_device_id usb_i2c_id[] = {{"mtk-usb",0},{}};

static struct i2c_board_info __initdata usb_i2c_dev = { I2C_BOARD_INFO("mtk-usb", 0x60)};


void USB_PHY_Write_Register8(UINT8 var,  UINT8 addr)
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

	printk("[MUSB]usb_i2c_probe, start\n");

	usb_i2c_client = client;

	//disable usb mac suspend
	DRV_WriteReg8(USB_SIF_BASE + 0x86a, 0x00);

	//usb phy initial sequence
	USB_PHY_Write_Register8(0x00, 0xFF);
	USB_PHY_Write_Register8(0x04, 0x61);
	USB_PHY_Write_Register8(0x00, 0x68);
	USB_PHY_Write_Register8(0x00, 0x6a);
	USB_PHY_Write_Register8(0x6e, 0x00);
	USB_PHY_Write_Register8(0x0c, 0x1b);
	USB_PHY_Write_Register8(0x44, 0x08);
	USB_PHY_Write_Register8(0x55, 0x11);
	USB_PHY_Write_Register8(0x68, 0x1a);


	printk("[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	printk("[MUSB]addr: 0x61, value: %x\n", USB_PHY_Read_Register8(0x61));
	printk("[MUSB]addr: 0x68, value: %x\n", USB_PHY_Read_Register8(0x68));
	printk("[MUSB]addr: 0x6a, value: %x\n", USB_PHY_Read_Register8(0x6a));
	printk("[MUSB]addr: 0x00, value: %x\n", USB_PHY_Read_Register8(0x00));
	printk("[MUSB]addr: 0x1b, value: %x\n", USB_PHY_Read_Register8(0x1b));
	printk("[MUSB]addr: 0x08, value: %x\n", USB_PHY_Read_Register8(0x08));
	printk("[MUSB]addr: 0x11, value: %x\n", USB_PHY_Read_Register8(0x11));
	printk("[MUSB]addr: 0x1a, value: %x\n", USB_PHY_Read_Register8(0x1a));


	printk("[MUSB]usb_i2c_probe, end\n");
    return 0;

}

static int usb_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, "mtk-usb");
    return 0;
}

static int usb_i2c_remove(struct i2c_client *client) {return 0;}


struct i2c_driver usb_i2c_driver = {
    .probe = usb_i2c_probe,
    .remove = usb_i2c_remove,
    .detect = usb_i2c_detect,
    .driver = {
    	.name = "mtk-usb",
    },
    .id_table = usb_i2c_id,
};

int add_usb_i2c_driver()
{
	i2c_register_board_info(0, &usb_i2c_dev, 1);
	if(i2c_add_driver(&usb_i2c_driver)!=0)
	{
		printk("[MUSB]usb_i2c_driver initialization failed!!\n");
		return -1;
	}
	else
	{
		printk("[MUSB]usb_i2c_driver initialization succeed!!\n");
	}
	return 0;
}
#endif //End of CONFIG_MTK_FPGA
#endif /* NEVER */
