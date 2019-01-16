#ifndef __CMDQ_DEVICE_H__
#define __CMDQ_DEVICE_H__

#include <linux/platform_device.h>
#include <linux/device.h>

struct device *cmdq_dev_get(void);
const uint32_t cmdq_dev_get_irq_id(void);
const uint32_t cmdq_dev_get_irq_secure_id(void);

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name);
void cmdq_dev_free_module_base_VA(const long VA);

void cmdq_dev_init(struct platform_device *pDevice);
void cmdq_dev_deinit(void);

const uint32_t cmdq_dev_get_irq_id(void);
const uint32_t cmdq_dev_get_irq_secure_id(void);
const long cmdq_dev_get_module_base_VA_GCE(void);
const long cmdq_dev_get_module_base_PA_GCE(void);

#define DEFINE_MODULE_BASE_VA(BASE_SYMBOL) const long cmdq_dev_get_module_base_VA_##BASE_SYMBOL(void);
DEFINE_MODULE_BASE_VA(MMSYS_CONFIG)
DEFINE_MODULE_BASE_VA(MDP_RDMA0)
DEFINE_MODULE_BASE_VA(MDP_RDMA1)
DEFINE_MODULE_BASE_VA(MDP_RSZ0)
DEFINE_MODULE_BASE_VA(MDP_RSZ1)
DEFINE_MODULE_BASE_VA(MDP_RSZ2)
DEFINE_MODULE_BASE_VA(MDP_TDSHP0)
DEFINE_MODULE_BASE_VA(MDP_TDSHP1)
DEFINE_MODULE_BASE_VA(MDP_MOUT0)
DEFINE_MODULE_BASE_VA(MDP_MOUT1)
DEFINE_MODULE_BASE_VA(MDP_WROT0)
DEFINE_MODULE_BASE_VA(MDP_WROT1)
DEFINE_MODULE_BASE_VA(MDP_WDMA)
DEFINE_MODULE_BASE_VA(MM_MUTEX)
DEFINE_MODULE_BASE_VA(VENC)
#undef DEFINE_MODULE_BASE_VA

#endif				/* __CMDQ_DEVICE_H__ */
