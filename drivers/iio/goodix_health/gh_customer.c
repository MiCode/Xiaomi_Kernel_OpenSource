#include "gh_customer.h"

gh_power_cfg g_vdd_cfg = {
	.load_uA = 100000,
	.min_uV = 3000000,
	.max_uV = 3000000,
};

gh_power_cfg g_vdd_io_cfg = {
	.load_uA = 100000,
	.min_uV = 1620000,
	.max_uV = 1980000,
};