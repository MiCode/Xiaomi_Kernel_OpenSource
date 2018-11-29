#ifndef __CHARGE_LOG_H__
#define __CHARGE_LOG_H__

/* usb log tag and print */
#define usb_logs_err(fmt, x...) \
do { \
	printk(KERN_ERR "USB_ERR:" pr_fmt(fmt), ##x); \
} while (0)

#define usb_logs_info(fmt, x...) \
do { \
	printk(KERN_ERR "USB_INFO:" pr_fmt(fmt), ##x); \
} while (0)

#define usb_dev_err(dev, fmt, x...) \
do { \
	dev_printk(KERN_ERR, dev, "USB_ERR:" fmt, ##x); \
} while (0)

#define usb_dev_info(dev, fmt, x...) \
do { \
	dev_printk(KERN_ERR, dev, "USB_INFO:" fmt, ##x); \
} while (0)

/* usb power delivery log tag and print */
#define usb_pd_logs_err(fmt, x...) \
do { \
	printk(KERN_ERR "USB_PD_ERR:" pr_fmt(fmt), ##x); \
} while (0)

#define usb_pd_logs_info(fmt, x...) \
do { \
	printk(KERN_ERR "USB_PD_INFO:" pr_fmt(fmt), ##x); \
} while (0)

#define usb_pd_dev_err(dev, fmt, x...) \
do { \
	dev_printk(KERN_ERR, dev, "USB_PD_ERR:" fmt, ##x); \
} while (0)

#define usb_pd_dev_info(dev, fmt, x...) \
do { \
	dev_printk(KERN_ERR, dev, "USB_PD_INFO:" fmt, ##x); \
} while (0)

/* charge log tag and print */
#define charge_logs_err(fmt, x...) \
do { \
	printk(KERN_ERR "CHARGE_ERR:" pr_fmt(fmt), ##x); \
} while (0)

#define charge_logs_info(fmt, x...) \
do { \
	printk(KERN_ERR "CHARGE_INFO:" pr_fmt(fmt), ##x); \
} while (0)

#define charge_dev_err(dev, fmt, x...) \
do { \
	dev_printk(KERN_ERR, dev, "CHARGE_ERR:" fmt, ##x); \
} while (0)

/* fuel gauge log tag and print */
#define fg_logs_err(fmt, x...) \
do { \
	printk(KERN_ERR "FG_ERR:" pr_fmt(fmt), ##x); \
} while (0)

#define fg_logs_info(fmt, x...) \
do { \
	printk(KERN_ERR "FG_INFO:" pr_fmt(fmt), ##x); \
} while (0)

#define fg_dev_err(dev, fmt, x...) \
do { \
	dev_printk(KERN_ERR, dev, "FG_ERR:" fmt, ##x); \
} while (0)

#endif  /* __CHARGE_LOG__ */

