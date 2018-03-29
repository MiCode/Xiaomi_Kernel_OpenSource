#ifndef __USB2JTAG_H_
#define __USB2JTAG_H_
unsigned int usb2jtag_mode(void);
struct mt_usb2jtag_driver *get_mt_usb2jtag_drv(void);
struct mt_usb2jtag_driver {
	int	(*usb2jtag_init)(void);
	int	(*usb2jtag_resume)(void);
	int	(*usb2jtag_suspend)(void);
};

extern bool usb_enable_clock(bool enable);
extern struct clk *musb_clk;
#endif
