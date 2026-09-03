/* SPDX-License-Identifier: GPL-2.0 */


#ifndef __LINUX_LC_CHARGER_CLASS_H__
#define __LINUX_LC_CHARGER_CLASS_H__

struct charger_dev;
struct charger_ops {
	int (*is_present)(struct charger_dev *charger, int *present);
	int (*get_vindpm_state)(struct charger_dev *charger, bool *state);
	int (*set_shipmode)(struct charger_dev *charger, bool en);
	int (*first_bc1p2)(struct charger_dev *charger, int *bc1p2_result);
	int (*retry_bc1p2)(struct charger_dev *charger, int *bc1p2_result);
	int (*get_vbus_type)(struct charger_dev *charger, int *type);
	int (*set_vindpm)(struct charger_dev *charger, int vindpm);
	int (*get_iindpm)(struct charger_dev *charger, int *iindpm);
	int (*set_iterm)(struct charger_dev *charger, int iterm);
	int (*set_pfm)(struct charger_dev *charger, bool en);
};

struct charger_dev {
	struct device dev;
	char *name;
	void *private;
	struct charger_ops *ops;
};

struct charger_dev *charger_find_dev_by_name(const char *name);
struct charger_dev *charger_register(char *name, struct device *parent,
							struct charger_ops *ops, void *private);
int charger_unregister(struct charger_dev *charger);
void *charger_get_private(struct charger_dev *charger);

int charger_get_chg_present(struct charger_dev *charger, int *state);
int charger_get_vindpm_state(struct charger_dev *charger, bool *state);
int charger_set_shipmode(struct charger_dev *charger, bool en);
int charger_first_bc1p2(struct charger_dev *charger, int *bc1p2_result);
int charger_retry_bc1p2(struct charger_dev *charger, int *bc1p2_result);
int charger_get_vbus_type(struct charger_dev *charger, int *type);
int charger_set_vindpm(struct charger_dev *charger, int vindpm);
int charger_get_iindpm(struct charger_dev *charger, int *iindpm);
int charger_set_iterm(struct charger_dev *charger, int iterm);
int charger_set_pfm(struct charger_dev *charger, bool en);
#endif /* __LINUX_LC_CHARGER_CLASS_H__ */
