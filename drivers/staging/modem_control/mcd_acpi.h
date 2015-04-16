#include <linux/mdm_ctrl_board.h>
#include <linux/mdm_ctrl.h>

#define NAME_LEN	16

#define INVALID_GPIO -1

#define MODEM_DATA_INDEX_UNSUP			0
#define MODEM_DATA_INDEX_GENERIC		1
#define MODEM_DATA_INDEX_2230			2
#define MODEM_DATA_INDEX_6360			3


/* Retrieve modem parameters on ACPI framework */
int get_modem_acpi_data(struct platform_device *pdev);
void put_modem_acpi_data(struct platform_device *pdev);