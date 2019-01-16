#include <mach/mt_clkmgr.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/musb/mtk_musb.h>
#include <linux/musb/musb_core.h>
#include "usb20.h"

#define FRA (48)
#define PARA (28)

#ifdef FPGA_PLATFORM
bool usb_enable_clock(bool enable)
{
	return true;
}

void usb_phy_poweron(void)
{
}

void usb_phy_savecurrent(void)
{
}

void usb_phy_recover(void)
{
}

// BC1.2
void Charger_Detect_Init(void)
{
}

void Charger_Detect_Release(void)
{
}

void usb_phy_context_save(void)
{
}

void usb_phy_context_restore(void)
{
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
bool usb_phy_check_in_uart_mode()
{
	UINT8 usb_port_mode;
    usb_enable_clock(true);
    udelay(50);

	usb_port_mode = USB_PHY_Read_Register8(0x6B);
    usb_enable_clock(false);

	if ((usb_port_mode == 0x5C) ||
		(usb_port_mode == 0x5E))
		return true;
	else
		return false;
}

void usb_phy_switch_to_uart(void)
{
	int var;
#if 0
	//SW disconnect
	var = USB_PHY_Read_Register8(0x68);
	printk("[MUSB]addr: 0x68, value: %x\n", var);
	USB_PHY_Write_Register8(0x15, 0x68);
	printk("[MUSB]addr: 0x68, value after: %x\n", USB_PHY_Read_Register8(0x68));

	var = USB_PHY_Read_Register8(0x6A);
	printk("[MUSB]addr: 0x6A, value: %x\n", var);
	USB_PHY_Write_Register8(0x0, 0x6A);
	printk("[MUSB]addr: 0x6A, value after: %x\n", USB_PHY_Read_Register8(0x6A));
	//SW disconnect
#endif
	/* Set ru_uart_mode to 2'b01 */
	var = USB_PHY_Read_Register8(0x6B);
	printk("[MUSB]addr: 0x6B, value: %x\n", var);
	USB_PHY_Write_Register8(var | 0x7C, 0x6B);
	printk("[MUSB]addr: 0x6B, value after: %x\n", USB_PHY_Read_Register8(0x6B));

	/* Set RG_UART_EN to 1 */
	var = USB_PHY_Read_Register8(0x6E);
	printk("[MUSB]addr: 0x6E, value: %x\n", var);
	USB_PHY_Write_Register8(var | 0x07, 0x6E);
	printk("[MUSB]addr: 0x6E, value after: %x\n", USB_PHY_Read_Register8(0x6E));

	/* Set RG_USB20_DM_100K_EN to 1 */
	var = USB_PHY_Read_Register8(0x22);
	printk("[MUSB]addr: 0x22, value: %x\n", var);
	USB_PHY_Write_Register8(var | 0x02, 0x22);
	printk("[MUSB]addr: 0x22, value after: %x\n", USB_PHY_Read_Register8(0x22));

	var = DRV_Reg8(UART1_BASE + 0x90);
	printk("[MUSB]addr: 0x11002090 (UART1), value: %x\n", var);
	DRV_WriteReg8(UART1_BASE + 0x90, var | 0x01);
	printk("[MUSB]addr: 0x11002090 (UART1), value after: %x\n\n", DRV_Reg8(UART1_BASE + 0x90));

	//SW disconnect
	mt_usb_disconnect();
}

void usb_phy_switch_to_usb(void)
{
	int var;
	/* Set RG_UART_EN to 0 */
	var = USB_PHY_Read_Register8(0x6E);
	printk("[MUSB]addr: 0x6E, value: %x\n", var);
	USB_PHY_Write_Register8(var & ~0x01, 0x6E);
	printk("[MUSB]addr: 0x6E, value after: %x\n", USB_PHY_Read_Register8(0x6E));

	/* Set RG_USB20_DM_100K_EN to 0 */
	var = USB_PHY_Read_Register8(0x22);
	printk("[MUSB]addr: 0x22, value: %x\n", var);
	USB_PHY_Write_Register8(var & ~0x02, 0x22);
	printk("[MUSB]addr: 0x22, value after: %x\n", USB_PHY_Read_Register8(0x22));

	var = DRV_Reg8(UART1_BASE + 0x90);
	printk("[MUSB]addr: 0x11002090 (UART1), value: %x\n", var);
	DRV_WriteReg8(UART1_BASE + 0x90, var & ~0x01);
	printk("[MUSB]addr: 0x11002090 (UART1), value after: %x\n\n", DRV_Reg8(UART1_BASE + 0x90));
#if 0
	//SW connect
	var = USB_PHY_Read_Register8(0x68);
	printk("[MUSB]addr: 0x68, value: %x\n", var);
	USB_PHY_Write_Register8(0x0, 0x68);
	printk("[MUSB]addr: 0x68, value after: %x\n", USB_PHY_Read_Register8(0x68));

	var = USB_PHY_Read_Register8(0x6A);
	printk("[MUSB]addr: 0x6A, value: %x\n", var);
	USB_PHY_Write_Register8(0x0, 0x6A);
	printk("[MUSB]addr: 0x6A, value after: %x\n", USB_PHY_Read_Register8(0x6A));
	//SW connect
#endif
	//SW connect
	mt_usb_connect();
}
#endif

#else

#ifdef CONFIG_MTK_UART_USB_SWITCH
static bool in_uart_mode = false;
#endif

static DEFINE_SPINLOCK(musb_reg_clock_lock);

static void enable_phy_clock(bool enable)
{
    ///* USB phy 48M clock , UNIVPLL_CON0[26] */
    //if (enable) {
    //    writel(readl((void __iomem *)UNIVPLL_CON0)|(0x04000000),
		//	   (void __iomem *)UNIVPLL_CON0);
    //} else {
    //    writel(readl((void __iomem *)UNIVPLL_CON0)&~(0x04000000),
		//	   (void __iomem *)UNIVPLL_CON0);
    //}
}

bool usb_enable_clock(bool enable)
{
	static int count = 0;
	bool res1 = TRUE;
    bool res2 = TRUE;
	unsigned long flags;

	spin_lock_irqsave(&musb_reg_clock_lock, flags);

	if (enable && count == 0) {
		enable_phy_clock(true);
		res1 = enable_clock(MT_CG_INFRA_USB, "INFRA_USB");
        res2 = enable_clock(MT_CG_INFRA_USB_MCU, "INFRA_USB_MCU");
	} else if (!enable && count == 1) {
		res1 = disable_clock(MT_CG_INFRA_USB, "INFRA_USB");
        res2 = disable_clock(MT_CG_INFRA_USB_MCU, "INFRA_USB_MCU");
		enable_phy_clock(false);
	}

	if (enable)
		count++;
	else
		count = (count==0) ? 0 : (count-1);

	spin_unlock_irqrestore(&musb_reg_clock_lock, flags);

	printk(KERN_DEBUG "enable(%d), count(%d) res1=%d, res2=%d\n", enable, count, res1, res2);
	return 1;
}

static void hs_slew_rate_cal(void){
  unsigned long data;
  unsigned long x;
  unsigned char value;
  unsigned long start_time, timeout;
  unsigned int timeout_flag = 0;
  //4 s1:enable usb ring oscillator.
  USBPHY_WRITE8(0x15,0x80);

  //4 s2:wait 1us.
  udelay(1);

  //4 s3:enable free run clock
  USBPHY_WRITE8 (0xf00-0x800+0x11,0x01);
  //4 s4:setting cyclecnt.
  USBPHY_WRITE8 (0xf00-0x800+0x01,0x04);
  //4 s5:enable frequency meter
  USBPHY_SET8 (0xf00-0x800+0x03,0x01);

  //4 s6:wait for frequency valid.
  start_time = jiffies;
  timeout = jiffies + 3 * HZ;

  while(!(USBPHY_READ8(0xf00-0x800+0x10)&0x1)){
    if(time_after(jiffies, timeout)){
        timeout_flag = 1;
        break;
        }
    }

  //4 s7: read result.
  if(timeout_flag){
    printk("[USBPHY] Slew Rate Calibration: Timeout\n");
    value = 0x4;
    }
  else{
      data = USBPHY_READ32 (0xf00-0x800+0x0c);
      x = ((1024*FRA*PARA)/data);
      value = (unsigned char)(x/1000);
      if((x-value*1000)/100>=5)
        value += 1;
        printk("[USBPHY]slew calibration:FM_OUT =%lu,x=%lu,value=%d\n",data,x,value);
    }

  //4 s8: disable Frequency and run clock.
  USBPHY_CLR8 (0xf00-0x800+0x03,0x01);//disable frequency meter
  USBPHY_CLR8 (0xf00-0x800+0x11,0x01);//disable free run clock

  //4 s9:
  USBPHY_WRITE8(0x15,value<<4);

  //4 s10:disable usb ring oscillator.
  USBPHY_CLR8(0x15,0x80);
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
bool usb_phy_check_in_uart_mode()
{
	UINT8 usb_port_mode;

    usb_enable_clock(true);
    udelay(50);
	usb_port_mode = USBPHY_READ8(0x6B);
	//usb_port_mode = 1;
    usb_enable_clock(false);

	if ((usb_port_mode == 0x5C) ||
		(usb_port_mode == 0x5E))
		return true;
	else
		return false;
}

void usb_phy_switch_to_uart(void)
{
	if (usb_phy_check_in_uart_mode()) {
		return;
	}
    usb_enable_clock(true);
	udelay(50);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USBPHY_CLR8(0x1a, 0x80);

	/* Set RG_SUSPENDM to 1 */
	USBPHY_SET8(0x68, 0x08);

	/* force suspendm = 1 */
	USBPHY_SET8(0x6a, 0x04);

	/* Set ru_uart_mode to 2'b01 */
	USBPHY_SET8(0x6B, 0x5C);

	/* Set RG_UART_EN to 1 */
	USBPHY_SET8(0x6E, 0x07);

	/* Set RG_USB20_DM_100K_EN to 1 */
	USBPHY_SET8(0x22, 0x02);
    usb_enable_clock(false);

	/* GPIO Selection */
	DRV_WriteReg32(UART1_BASE + 0x90, 0x1);	//set
}


void usb_phy_switch_to_usb(void)
{
	/* GPIO Selection */
	DRV_WriteReg32(UART1_BASE + 0x90, 0x0);	//set

    usb_enable_clock(true);
    udelay(50);
	/* clear force_uart_en */
	USBPHY_WRITE8(0x6B, 0x00);
    usb_enable_clock(false);
	usb_phy_poweron();
	/* disable the USB clock turned on in usb_phy_poweron() */
    usb_enable_clock(false);
}
#endif

void usb_phy_poweron(void){

	#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (usb_phy_check_in_uart_mode())
		return;
	#endif

    //4 s1: enable USB MAC clock.
    usb_enable_clock(true);

    //4 s2: wait 50 usec for PHY3.3v/1.8v stable.
    udelay(50);

    //4 s3: swtich to USB function. (system register, force ip into usb mode.
    USBPHY_CLR8(0x6b, 0x04);
    USBPHY_CLR8(0x6e, 0x01);
    USBPHY_CLR8(0x21, 0x03);
    //USBPHY_CLR8(0x21, 0x01);

    //4 s4: RG_USB20_BC11_SW_EN 1'b0
    USBPHY_CLR8(0x1a, 0x80);

    //5 s5: RG_USB20_DP_100K_EN 1'b0, RG_USB20_DM_100K_EN 1'b0
    USBPHY_SET8(0x22, 0x04);
    USBPHY_CLR8(0x22, 0x03);

    /*OTG enable*/
    USBPHY_SET8(0x20, 0x10);
    //6 s6: release force suspendm.
    USBPHY_CLR8(0x6a, 0x04);

    //7 s7: wait for 800 usec.
    udelay(800);

    // force enter device mode, k2 need to modify?
    USBPHY_CLR8(0x6c, 0x10);
    USBPHY_SET8(0x6c, 0x2E);
    USBPHY_SET8(0x6d, 0x3E);

    printk("usb power on success\n");
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
static bool skipDisableUartMode = true;
#endif

static void usb_phy_savecurrent_internal(void){

    //4 1. swtich to USB function. (system register, force ip into usb mode.

	#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!usb_phy_check_in_uart_mode()) {
		//4 s1: enable USB MAC clock.
		usb_enable_clock(true);

		//4 s2: wait 50 usec for PHY3.3v/1.8v stable.
		udelay(50);

		USBPHY_CLR8(0x6b, 0x04);
		USBPHY_CLR8(0x6e, 0x01);

		//4 2. release force suspendm.
		USBPHY_CLR8(0x6a, 0x04);
		usb_enable_clock(false);
	} else {
		if (skipDisableUartMode)
			skipDisableUartMode = false;
		else
			return;
	}
	#else
    USBPHY_CLR8(0x6b, 0x04);
    USBPHY_CLR8(0x6e, 0x01);
    USBPHY_CLR8(0x21, 0x03);

    //4 2. release force suspendm.
   // USBPHY_CLR8(0x6a, 0x04);
    USBPHY_SET8(0x6a, 0x04);
    USBPHY_SET8(0x68, 0x04);
	#endif

    //4 3. RG_DPPULLDOWN./RG_DMPULLDOWN.
    USBPHY_SET8(0x68, 0xc0);
    //4 4. RG_XCVRSEL[1:0] =2'b01.
    USBPHY_CLR8(0x68, 0x30);
    USBPHY_SET8(0x68, 0x10);
    //4 5. RG_TERMSEL = 1'b1
	USBPHY_SET8(0x68, 0x04);
    //4 6. RG_DATAIN[3:0]=4'b0000
	USBPHY_CLR8(0x69, 0x3c);
    //4 7.force_dp_pulldown, force_dm_pulldown, force_xcversel,force_termsel.
    USBPHY_SET8(0x6a, 0xba);

    //4 8.RG_USB20_BC11_SW_EN 1'b0
    USBPHY_CLR8(0x1a, 0x80);
    //4 9.RG_USB20_OTG_VBUSSCMP_EN 1'b0
    USBPHY_CLR8(0x1a, 0x10);
    //4 10. delay 800us.
    udelay(800);
    //4 11. rg_usb20_pll_stable = 1
    USBPHY_CLR8(0x6a, 0x04);
    //USBPHY_SET8(0x63, 0x02);

//ALPS00427972, implement the analog register formula
   // printk("%s: USBPHY_READ8(0x05) = 0x%x \n", __func__, USBPHY_READ8(0x05));
   // printk("%s: USBPHY_READ8(0x07) = 0x%x \n", __func__, USBPHY_READ8(0x07));
//ALPS00427972, implement the analog register formula

    udelay(1);
    //4 12.  force suspendm = 1.
   // USBPHY_SET8(0x6a, 0x04);
    //4 13.  wait 1us
    udelay(1);

    // force enter device mode
   // USBPHY_CLR8(0x6c, 0x10);
   // USBPHY_SET8(0x6c, 0x2E);
   // USBPHY_SET8(0x6d, 0x3E);
}

void usb_phy_savecurrent(void){

    usb_phy_savecurrent_internal();
    //4 14. turn off internal 48Mhz PLL.
    usb_enable_clock(false);
    printk("usb save current success\n");
}

void usb_phy_recover(void){

    //4 1. turn on USB reference clock.
    usb_enable_clock(true);
    //4 2. wait 50 usec.
    udelay(50);

	#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!usb_phy_check_in_uart_mode()) {
    // clean PUPD_BIST_EN
    // PUPD_BIST_EN = 1'b0
    // PMIC will use it to detect charger type
    USBPHY_CLR8(0x1d, 0x10);

    //4 3. force_uart_en = 1'b0
    USBPHY_CLR8(0x6b, 0x04);
    //4 4. RG_UART_EN = 1'b0
    USBPHY_CLR8(0x6e, 0x1);
	//4 5. release force suspendm.
    USBPHY_CLR8(0x6a, 0x04);

	skipDisableUartMode = false;
	} else {
		if (!skipDisableUartMode)
			return;
	}
	#else
    // clean PUPD_BIST_EN
    // PUPD_BIST_EN = 1'b0
    // PMIC will use it to detect charger type
    USBPHY_CLR8(0x1d, 0x10);

    //4 3. force_uart_en = 1'b0
    USBPHY_CLR8(0x6b, 0x04);
    //4 4. RG_UART_EN = 1'b0
    USBPHY_CLR8(0x6e, 0x1);
    //4 5. force_uart_en = 1'b0
    USBPHY_CLR8(0x6a, 0x04);

    USBPHY_CLR8(0x21, 0x03);
	#endif

    //4 6. RG_DPPULLDOWN = 1'b0
    USBPHY_CLR8(0x68, 0x40);
    //4 7. RG_DMPULLDOWN = 1'b0
    USBPHY_CLR8(0x68, 0x80);
    //4 8. RG_XCVRSEL = 2'b00
    USBPHY_CLR8(0x68, 0x30);
    //4 9. RG_TERMSEL = 1'b0
    USBPHY_CLR8(0x68, 0x04);
    //4 10. RG_DATAIN[3:0] = 4'b0000
    USBPHY_CLR8(0x69, 0x3c);

   //4 11. force_dp_pulldown = 1b'0
    USBPHY_CLR8(0x6a, 0x10);
    //4 12. force_dm_pulldown = 1b'0
    USBPHY_CLR8(0x6a, 0x20);
    //4 13. force_xcversel = 1b'0
    USBPHY_CLR8(0x6a, 0x08);
    //4 14. force_termsel = 1b'0
    USBPHY_CLR8(0x6a, 0x02);
    //4 15. force_datain = 1b'0
    USBPHY_CLR8(0x6a, 0x80);

    //4 16. RG_USB20_BC11_SW_EN 1'b0
    USBPHY_CLR8(0x1a, 0x80);
    //4 17. RG_USB20_OTG_VBUSSCMP_EN 1'b1
    USBPHY_SET8(0x1a, 0x10);

    //4 18. wait 800 usec.
    udelay(800);

    // force enter device mode
    USBPHY_CLR8(0x6c, 0x10);
    USBPHY_SET8(0x6c, 0x2E);
    USBPHY_SET8(0x6d, 0x3E);

    #if defined(CONFIG_MTK_HDMI_SUPPORT)
    USBPHY_SET8(0x05, 0x05);
    USBPHY_SET8(0x05, 0x50);
    #endif
    hs_slew_rate_cal();

    printk("usb recovery success\n");
    return;
}

// BC1.2
void Charger_Detect_Init(void)
{
    //turn on USB reference clock.
    usb_enable_clock(true);
    //wait 50 usec.
    udelay(50);
    /* RG_USB20_BC11_SW_EN = 1'b1 */
    USBPHY_SET8(0x1a, 0x80);
    printk("Charger_Detect_Init\n");
}

void Charger_Detect_Release(void)
{
    /* RG_USB20_BC11_SW_EN = 1'b0 */
    USBPHY_CLR8(0x1a, 0x80);
    udelay(1);
    //4 14. turn off internal 48Mhz PLL.
    usb_enable_clock(false);
    printk("Charger_Detect_Release\n");
}

void usb_phy_context_save(void)
{
#ifdef CONFIG_MTK_UART_USB_SWITCH
	in_uart_mode = usb_phy_check_in_uart_mode();
#endif
}

void usb_phy_context_restore(void)
{
#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (in_uart_mode) {
		usb_phy_switch_to_uart();
	}
#endif
    usb_phy_savecurrent_internal();
}

#endif
