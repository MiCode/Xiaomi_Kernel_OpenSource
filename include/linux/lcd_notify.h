#ifndef __LINUX_LCD_NOTIFY_H
#define __LINUX_LCD_NOTIFY_H

#include <linux/notifier.h>

/* the display on process started */
#define LCD_EVENT_ON_START		0x01
/* the display on process end */
#define LCD_EVENT_ON_END		0x02
/* the display off process started */
#define LCD_EVENT_OFF_START		0x03
/* the display off process end */
#define LCD_EVENT_OFF_END		0x04

struct lcd_event {
	void *data;
};

#ifdef CONFIG_FB_MSM_MDSS
int lcd_register_client(struct notifier_block *nb);
int lcd_unregister_client(struct notifier_block *nb);
int lcd_notifier_call_chain(unsigned long val, void *v);
#else
static int inline lcd_register_client(struct notifier_block *nb)
{
	return -ENOENT;
}
static int inline lcd_unregister_client(struct notifier_block *nb)
{
	return -ENOENT;
}
static int inline lcd_notifier_call_chain(unsigned long val, void *v)
{
	return -ENOENT;
}
#endif
#endif /* _LINUX_LCD_NOTIFY_H */
