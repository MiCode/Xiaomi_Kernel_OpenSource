/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "cmdq_device.h"
#include "cmdq_core.h"
#include "cmdq_mdp.h"
/* #include <mach/mt_irq.h> */

/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>

typedef struct CmdqDeviceStruct {
	struct device *pDev;
	long regBaseVA;		/* considering 64 bit kernel, use long */
	long regBasePA;
	uint32_t irqId;
	uint32_t irqSecId;
} CmdqDeviceStruct;

static CmdqDeviceStruct gCmdqDev;
static long gMMSYS_CONFIG_Base_VA;

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

const long cmdq_dev_get_module_base_VA_MMSYS_CONFIG(void)
{
	return gMMSYS_CONFIG_Base_VA;
}

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, name);
	VA = (unsigned long)of_iomap(node, 0);
	CMDQ_LOG("DEV: VA(%s): 0x%lx\n", name, VA);

	return VA;
}

void cmdq_dev_free_module_base_VA(const long VA)
{
	iounmap((void *)VA);
}

void cmdq_dev_init_module_base_VA(void)
{
#ifdef CMDQ_OF_SUPPORT
	gMMSYS_CONFIG_Base_VA = 0;
	gMMSYS_CONFIG_Base_VA =
	    cmdq_dev_alloc_module_base_VA_by_name("mediatek,mt8173-mmsys");
	if (0 == gMMSYS_CONFIG_Base_VA) {
		gMMSYS_CONFIG_Base_VA =
			cmdq_dev_alloc_module_base_VA_by_name("mediatek,mt8163-mmsys");
	}
#endif

	cmdq_mdp_init_module_base_VA();

}

void cmdq_dev_deinit_module_base_VA(void)
{
#ifdef CMDQ_OF_SUPPORT
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MMSYS_CONFIG());
#endif

	cmdq_mdp_deinit_module_base_VA();

}

const long of_getPA(struct device_node *dev, int index)
{
	struct resource res;

	if (of_address_to_resource(dev, index, &res))
		return 0;
	return res.start;

}

void cmdq_dev_init(struct platform_device *pDevice)
{
#ifdef CMDQ_OF_SUPPORT
	struct device_node *node = pDevice->dev.of_node;
#endif
	/* init cmdq device dependent data */
	do {
		memset(&gCmdqDev, 0x0, sizeof(struct CmdqDeviceStruct));

		gCmdqDev.pDev = &pDevice->dev;
#ifdef CMDQ_OF_SUPPORT
		gCmdqDev.regBaseVA = (unsigned long)of_iomap(node, 0);
		/* gCmdqDev.regBasePA = GCE_BASE_PA; */
		gCmdqDev.regBasePA = of_getPA(node, 0);
		if (0 == gCmdqDev.regBasePA)
			CMDQ_ERR("ERROR!!! get GCE PA from device tree error. PA:%ld",
				 gCmdqDev.regBasePA);

		gCmdqDev.irqId = irq_of_parse_and_map(node, 0);
		gCmdqDev.irqSecId = irq_of_parse_and_map(node, 1);
#else
		gCmdqDev.regBaseVA = (long)ioremap(GCE_BASE_PA, 0x1000);
		gCmdqDev.regBasePA = GCE_BASE_PA;
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
