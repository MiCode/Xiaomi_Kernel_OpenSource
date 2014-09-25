#include <linux/mdm_ctrl_board.h>
#include <linux/mdm_ctrl.h>

#define NAME_LEN	16

/* Retrieve modem parameters on ACPI framework */
int get_modem_acpi_data(struct platform_device *pdev);
