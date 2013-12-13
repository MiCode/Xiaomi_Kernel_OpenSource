#ifndef ACPI_BUTTON_H
#define ACPI_BUTTON_H

#include <linux/notifier.h>

#if defined(CONFIG_ACPI_BUTTON) || defined(CONFIG_ACPI_BUTTON_MODULE)
extern int acpi_lid_notifier_register(struct notifier_block *nb);
extern int acpi_lid_notifier_unregister(struct notifier_block *nb);
extern int acpi_lid_open(void);

struct acpi_pwrbtn_poll_dev {
	int (*poll)(struct acpi_pwrbtn_poll_dev *dev);
	struct timer_list timer;
	unsigned long started;
};
extern int acpi_pwrbtn_poll_register(struct acpi_pwrbtn_poll_dev *dev);
extern int acpi_pwrbtn_poll_unregister(struct acpi_pwrbtn_poll_dev *dev);

#else
static inline int acpi_lid_notifier_register(struct notifier_block *nb)
{
	return 0;
}
static inline int acpi_lid_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}
static inline int acpi_lid_open(void)
{
	return 1;
}
static inline int acpi_register_pwrbtn_poll(struct acpi_pwrbtn_poll_dev *dev)
{
	return 0;
}
static inline int acpi_unregister_pwrbtn_poll(struct acpi_pwrbtn_poll_dev *dev)
{
	return 0;
}
#endif /* defined(CONFIG_ACPI_BUTTON) || defined(CONFIG_ACPI_BUTTON_MODULE) */

#endif /* ACPI_BUTTON_H */
