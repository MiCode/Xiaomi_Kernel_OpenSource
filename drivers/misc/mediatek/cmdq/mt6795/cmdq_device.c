#include "cmdq_device.h"
#include "cmdq_core.h"

#ifndef CMDQ_OF_SUPPORT
#include <mach/mt_irq.h>
#endif


/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>

typedef struct CmdqModuleBaseVA {
	long MMSYS_CONFIG;
	long MDP_RDMA0;
	long MDP_RDMA1;
	long MDP_RSZ0;
	long MDP_RSZ1;
	long MDP_RSZ2;
	long MDP_TDSHP0;
	long MDP_TDSHP1;
	long MDP_MOUT0;
	long MDP_MOUT1;
	long MDP_WROT0;
	long MDP_WROT1;
	long MDP_WDMA;
	long MM_MUTEX;
	long VENC;
} CmdqModuleBaseVA;

typedef struct CmdqDeviceStruct {
	struct device *pDev;
	long regBaseVA;		/* considering 64 bit kernel, use long */
	long regBasePA;
	uint32_t irqId;
	uint32_t irqSecId;
} CmdqDeviceStruct;

static CmdqModuleBaseVA gCmdqModuleBaseVA;
static CmdqDeviceStruct gCmdqDev;

struct device *cmdq_dev_get(void)
{
	return gCmdqDev.pDev;
}

const uint32_t cmdq_dev_get_irq_id(void)
{
	return gCmdqDev.irqId;
}

const uint32_t cmdq_dev_get_irq_secure_id(void)
{
	return gCmdqDev.irqSecId;
}

const long cmdq_dev_get_module_base_VA_GCE(void)
{
	return gCmdqDev.regBaseVA;
}

const long cmdq_dev_get_module_base_PA_GCE(void)
{
	return gCmdqDev.regBasePA;
}

#define DEFINE_MODULE_BASE_VA(BASE_SYMBOL) const long cmdq_dev_get_module_base_VA_##BASE_SYMBOL(void){\
												return gCmdqModuleBaseVA.BASE_SYMBOL;\
											}
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

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name)
{
	unsigned long VA = NULL;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, name);
	VA = (unsigned long)of_iomap(node, 0);
	CMDQ_LOG("DEV: VA(%s): 0x%lx\n", name, VA);

	return VA;
}

void cmdq_dev_free_module_base_VA(const long VA)
{
	iounmap((void*)VA);
}

void cmdq_dev_init_module_base_VA(void)
{
	memset(&gCmdqModuleBaseVA, 0, sizeof(CmdqModuleBaseVA));

#ifdef CMDQ_OF_SUPPORT
	#define ASSIGN_MODULE_BASE(SYMBOL) gCmdqModuleBaseVA.SYMBOL = cmdq_dev_alloc_module_base_VA_by_name("mediatek," #SYMBOL);
	ASSIGN_MODULE_BASE(MMSYS_CONFIG)
	ASSIGN_MODULE_BASE(MDP_RDMA0);
	ASSIGN_MODULE_BASE(MDP_RDMA1);
	ASSIGN_MODULE_BASE(MDP_RSZ0);
	ASSIGN_MODULE_BASE(MDP_RSZ1);
	ASSIGN_MODULE_BASE(MDP_RSZ2);
	ASSIGN_MODULE_BASE(MDP_TDSHP0);
	ASSIGN_MODULE_BASE(MDP_TDSHP1);
	ASSIGN_MODULE_BASE(MDP_MOUT0);
	ASSIGN_MODULE_BASE(MDP_MOUT1);
	ASSIGN_MODULE_BASE(MDP_WROT0);
	ASSIGN_MODULE_BASE(MDP_WROT1);
	ASSIGN_MODULE_BASE(MDP_WDMA);
	ASSIGN_MODULE_BASE(MM_MUTEX);
	ASSIGN_MODULE_BASE(VENC);
#else
	#define ASSIGN_MODULE_BASE(SYMBOL) gCmdqModuleBaseVA.SYMBOL = SYMBOL##_BASE;
	ASSIGN_MODULE_BASE(MMSYS_CONFIG)
	ASSIGN_MODULE_BASE(MDP_RDMA0);
	ASSIGN_MODULE_BASE(MDP_RDMA1);
	ASSIGN_MODULE_BASE(MDP_RSZ0);
	ASSIGN_MODULE_BASE(MDP_RSZ1);
	ASSIGN_MODULE_BASE(MDP_RSZ2);
	ASSIGN_MODULE_BASE(MDP_TDSHP0);
	ASSIGN_MODULE_BASE(MDP_TDSHP1);
	ASSIGN_MODULE_BASE(MDP_MOUT0);
	ASSIGN_MODULE_BASE(MDP_MOUT1);
	ASSIGN_MODULE_BASE(MDP_WROT0);
	ASSIGN_MODULE_BASE(MDP_WROT1);
	ASSIGN_MODULE_BASE(MDP_WDMA);
	ASSIGN_MODULE_BASE(MM_MUTEX);
	ASSIGN_MODULE_BASE(VENC);
#endif
}

void cmdq_dev_deinit_module_base_VA(void)
{
#ifdef CMDQ_OF_SUPPORT
	#define FREE_MODULE_BASE(SYMBOL) cmdq_dev_free_module_base_VA((cmdq_dev_get_module_base_VA_##SYMBOL()));
	FREE_MODULE_BASE(MMSYS_CONFIG);
	FREE_MODULE_BASE(MDP_RDMA0);
	FREE_MODULE_BASE(MDP_RDMA1);
	FREE_MODULE_BASE(MDP_RSZ0);
	FREE_MODULE_BASE(MDP_RSZ1);
	FREE_MODULE_BASE(MDP_RSZ2);
	FREE_MODULE_BASE(MDP_TDSHP0);
	FREE_MODULE_BASE(MDP_TDSHP1);
	FREE_MODULE_BASE(MDP_MOUT0);
	FREE_MODULE_BASE(MDP_MOUT1);
	FREE_MODULE_BASE(MDP_WROT0);
	FREE_MODULE_BASE(MDP_WROT1);
	FREE_MODULE_BASE(MDP_WDMA);
	FREE_MODULE_BASE(MM_MUTEX);
	FREE_MODULE_BASE(VENC);

	memset(&gCmdqModuleBaseVA, 0, sizeof(CmdqModuleBaseVA));
#else
	/* do nothing, registers' IOMAP will be destoryed by platform */
#endif
}

void cmdq_dev_init(struct platform_device *pDevice)
{
	struct device_node *node = pDevice->dev.of_node;

	/* init cmdq device dependent data */
	do {
		memset(&gCmdqDev, 0x0, sizeof(CmdqDeviceStruct));

		gCmdqDev.pDev = &pDevice->dev;
#ifdef CMDQ_OF_SUPPORT
		gCmdqDev.regBaseVA = (unsigned long)of_iomap(node, 0);
		gCmdqDev.regBasePA = (0L | 0x10212000);
		gCmdqDev.irqId = irq_of_parse_and_map(node, 0);
		gCmdqDev.irqSecId = irq_of_parse_and_map(node, 1);
#else
		gCmdqDev.regBaseVA = (0L | GCE_BASE);
		gCmdqDev.regBasePA = (0L | 0x10212000);
		gCmdqDev.irqId = CQ_DMA_IRQ_BIT_ID;
		gCmdqDev.irqSecId = CQ_DMA_SEC_IRQ_BIT_ID;
#endif

		CMDQ_LOG
		    ("[CMDQ] platform_dev: dev: %p, PA: %lx, VA: %lx, irqId: %d,  irqSecId:%d\n",
		     gCmdqDev.pDev, gCmdqDev.regBasePA, gCmdqDev.regBaseVA, gCmdqDev.irqId,
		     gCmdqDev.irqSecId);
	} while (0);

	/* init module VA */
	cmdq_dev_init_module_base_VA();
}

void cmdq_dev_deinit(void)
{
	cmdq_dev_deinit_module_base_VA();

	/* deinit cmdq device dependent data */
	do {
#ifdef CMDQ_OF_SUPPORT
		cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_GCE());
		gCmdqDev.regBaseVA = 0;
#else
		/* do nothing */
#endif
	} while (0);
}
