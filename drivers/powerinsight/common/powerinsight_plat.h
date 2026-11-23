#ifndef POWERINSIGHT_PLAT_H
#define POWERINSIGHT_PLAT_H

#include "powerinsight_gpu.h"

enum powerinsight_module_t {
	POWERINSIGHT_MODULE_GPU,
};

int powerinsight_register_module_ops(enum powerinsight_module_t module, void *mops);
int powerinsight_unregister_module_ops(enum powerinsight_module_t module);

#endif // POWERINSIGHT_PLAT_H
