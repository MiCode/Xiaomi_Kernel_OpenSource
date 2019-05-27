#ifndef _FPC_IRQ_H_

#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>

#include <linux/regulator/consumer.h>

struct fpc_gpio_info;

struct fpc_data {
	struct device *dev;
	struct platform_device *pldev;

	int irq_gpio;
	int rst_gpio;
	int ldo_gpio;

	bool wakeup_enabled;


	struct wakeup_source ttw_ws;

	struct regulator *vdd_tx;

	bool power_enabled;
	bool use_regulator_for_bezel;
	const struct fpc_gpio_info *hwabs;
};

struct fpc_gpio_info {
	int (*init)(struct fpc_data *fpc);
	int (*configure)(struct fpc_data *fpc, int *irq_num, int *trigger_flags);
	int (*get_val)(unsigned gpio);
	void (*set_val)(unsigned gpio, int val);
	ssize_t (*clk_enable_set)(struct fpc_data *fpc, const char *buf, size_t count);
	void (*irq_handler)(int irq, struct fpc_data *fpc);
	void *priv;
};

extern int fpc_probe(struct platform_device *pldev,
			struct fpc_gpio_info *fpc_gpio_ops);

extern int fpc_remove(struct platform_device *pldev);

#endif
