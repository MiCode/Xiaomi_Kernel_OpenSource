/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/hif/ehpi/arm.c#1
*/

/*! \file   "colibri.c"
*    \brief  Brief description.
*
*    Detail description.
*/


/******************************************************************************
*                         C O M P I L E R   F L A G S
*******************************************************************************
*/
#if !defined(MCR_EHTCR)
#define MCR_EHTCR                           0x0054
#endif

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include "colibri.h"
#include "wlan_lib.h"

/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/
static void __iomem *mt5931_mcr_base;

/*******************************************************************************
*                             M A C R O S
********************************************************************************
*/
#if CFG_EHPI_FASTER_BUS_TIMING
#define EHPI_CONFIG     MSC_CS(4, MSC_RBUFF_SLOW | \
	    MSC_RRR(4) | \
	    MSC_RDN(8) | \
	    MSC_RDF(7) | \
	    MSC_RBW_16 | \
	    MSC_RT_VLIO)
#else
#define EHPI_CONFIG     MSC_CS(4, MSC_RBUFF_SLOW | \
	    MSC_RRR(7) | \
	    MSC_RDN(13) | \
	    MSC_RDF(12) | \
	    MSC_RBW_16 | \
	    MSC_RT_VLIO)
#endif /* CFG_EHPI_FASTER_BUS_TIMING */

/*******************************************************************************
*              F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static VOID collibri_ehpi_reg_init(VOID);

static VOID collibri_ehpi_reg_uninit(VOID);

static VOID mt5931_ehpi_reg_init(VOID);

static VOID mt5931_ehpi_reg_uninit(VOID);

static void busSetIrq(void);

static void busFreeIrq(void);

static irqreturn_t glEhpiInterruptHandler(int irq, void *dev_id);

#if DBG
static void initTrig(void);
#endif

/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will register sdio bus to the os
*
* \param[in] pfProbe    Function pointer to detect card
* \param[in] pfRemove   Function pointer to remove card
*
* \return The result of registering sdio bus
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	pr_info("mtk_sdio: MediaTek eHPI WLAN driver\n");
	pr_info("mtk_sdio: Copyright MediaTek Inc.\n");

	if (pfProbe(NULL) != WLAN_STATUS_SUCCESS) {
		pfRemove();
		return WLAN_STATUS_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}				/* end of glRegisterBus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will unregister sdio bus to the os
*
* \param[in] pfRemove   Function pointer to remove card
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glUnregisterBus(remove_card pfRemove)
{
	ASSERT(pfRemove);
	pfRemove();

	/* TODO: eHPI uninitialization */
}				/* end of glUnregisterBus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function stores hif related info, which is initialized before.
*
* \param[in] prGlueInfo Pointer to glue info structure
* \param[in] u4Cookie   Pointer to UINT_32 memory base variable for _HIF_HPI
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glSetHifInfo(P_GLUE_INFO_T prGlueInfo, ULONG ulCookie)
{
	P_GL_HIF_INFO_T prHif = NULL;

	ASSERT(prGlueInfo);

	prHif = &prGlueInfo->rHifInfo;

	/* fill some buffered information into prHif */
	prHif->mcr_addr_base = mt5931_mcr_base + EHPI_OFFSET_ADDR;
	prHif->mcr_data_base = mt5931_mcr_base + EHPI_OFFSET_DATA;
}				/* end of glSetHifInfo() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clears hif related info.
*
* \param[in] prGlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glClearHifInfo(P_GLUE_INFO_T prGlueInfo)
{
	P_GL_HIF_INFO_T prHif = NULL;

	ASSERT(prGlueInfo);

	prHif = &prGlueInfo->rHifInfo;

	/* do something */
	prHif->mcr_addr_base = 0;
	prHif->mcr_data_base = 0;
}				/* end of glClearHifInfo() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Initialize bus operation and hif related information, request resources.
*
* \param[out] pvData    A pointer to HIF-specific data type buffer.
*                       For eHPI, pvData is a pointer to UINT_32 type and stores a
*                       mapped base address.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOL glBusInit(PVOID pvData)
{
#if DBG
	initTrig();
#endif

	/* 1. initialize eHPI control registers */
	collibri_ehpi_reg_init();

	/* 2. memory remapping for MT5931 */
	mt5931_ehpi_reg_init();

	return TRUE;
};

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus operation and release resources.
*
* \param[in] pvData A pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glBusRelease(PVOID pvData)
{
	/* 1. memory unmapping for MT5931 */
	mt5931_ehpi_reg_uninit();

	/* 2. uninitialize eHPI control registers */
	collibri_ehpi_reg_uninit();
}				/* end of glBusRelease() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Setup bus interrupt operation and interrupt handler for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pfnIsr     A pointer to interrupt handler function.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \retval WLAN_STATUS_SUCCESS   if success
*         NEGATIVE_VALUE   if fail
*/
/*----------------------------------------------------------------------------*/
INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie)
{
	struct net_device *pDev = (struct net_device *)pvData;
	int i4Status = 0;

	/* 1. enable GPIO pin as IRQ */
	busSetIrq();

	/* 2. Specify IRQ number into net_device */
	pDev->irq = WLAN_STA_IRQ;

	/* 3. register ISR callback */

	i4Status = request_irq(pDev->irq,
			       glEhpiInterruptHandler,
			       IRQF_DISABLED | IRQF_SHARED | IRQF_TRIGGER_FALLING, pDev->name, pvCookie);

	if (i4Status < 0)
		pr_debug("request_irq(%d) failed\n", pDev->irq);
	else
		pr_info("request_irq(%d) success with dev_id(%x)\n", pDev->irq, (unsigned int)pvCookie);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus interrupt operation and disable interrupt handling for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie)
{
	struct net_device *prDev = (struct net_device *)pvData;

	if (!prDev) {
		pr_info("Invalid net_device context.\n");
		return;
	}

	if (prDev->irq) {
		disable_irq(prDev->irq);
		free_irq(prDev->irq, pvCookie);
		prDev->irq = 0;
	}

	busFreeIrq();
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Set power state
*
* \param[in] pvGlueInfo     A pointer to GLUE_INFO_T
* \param[in] ePowerMode     Power Mode Setting
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glSetPowerState(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 ePowerMode)
{
}

#if DBG
/*----------------------------------------------------------------------------*/
/*!
* \brief Setup the GPIO pin.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
void setTrig(void)
{
	GPSR1 = (0x1UL << 8);
}				/* end of setTrig() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Clear the GPIO pin.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
void clearTrig(void)
{
	GPCR1 = (0x1UL << 8);
}				/* end of clearTrig() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Set a specified GPIO pin to H or L.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
static void initTrig(void)
{
	set_GPIO_mode(GPIO40_FFDTR | GPIO_OUT);
	clearTrig();
}				/* end of initTrig() */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This function congifure platform-dependent interrupt triger type.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
void busSetIrq(void)
{
#if defined(WLAN_STA_IRQ_GPIO)
	pxa_gpio_mode(WLAN_STA_IRQ_GPIO | GPIO_IN);
	set_irq_type(WLAN_STA_IRQ, IRQT_FALLING);
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function should restore settings changed by busSetIrq().
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
void busFreeIrq(void)
{
#if defined(WLAN_STA_IRQ_GPIO)
	pxa_gpio_mode(WLAN_STA_IRQ_GPIO | GPIO_OUT);
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function configures colibri memory controller registers
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
static VOID collibri_ehpi_reg_init(VOID)
{
	UINT_32 u4RegValue;

	/* 1. enable nCS as memory controller */
	pxa_gpio_mode(GPIO80_nCS_4_MD);

	/* 2. nCS<4> configuration */
	u4RegValue = MSC2;
	u4RegValue &= ~MSC_CS(4, 0xFFFF);
	u4RegValue |= EHPI_CONFIG;
	MSC2 = u4RegValue;

	pr_info("EHPI new MSC2:0x%08x\n", MSC2);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function restores colibri memory controller registers
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
static VOID collibri_ehpi_reg_uninit(VOID)
{
	UINT_32 u4RegValue;

	/* 1. restore nCS<4> configuration */
	u4RegValue = MSC2;
	u4RegValue &= ~MSC_CS(4, 0xFFFF);
	MSC2 = u4RegValue;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function configures MT5931 mapped registers on colibri
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
static VOID mt5931_ehpi_reg_init(VOID)
{
	struct resource *reso = NULL;

	/* 1. request memory regioin */
	reso = request_mem_region((unsigned long)MEM_MAPPED_ADDR, (unsigned long)MEM_MAPPED_LEN, (char *)MODULE_PREFIX);
	if (!reso) {
		pr_err("request_mem_region(0x%08X) failed.\n", MEM_MAPPED_ADDR);
		return;
	}

	/* 2. memory regioin remapping */
	mt5931_mcr_base = ioremap_nocache(MEM_MAPPED_ADDR, MEM_MAPPED_LEN);
	if (!(mt5931_mcr_base)) {
		release_mem_region(MEM_MAPPED_ADDR, MEM_MAPPED_LEN);
		pr_err("ioremap_nocache(0x%08X) failed.\n", MEM_MAPPED_ADDR);
		return;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function releases MT5931 mapped registers on colibri
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
static VOID mt5931_ehpi_reg_uninit(VOID)
{
	iounmap(mt5931_mcr_base);
	mt5931_mcr_base = NULL;

	release_mem_region(MEM_MAPPED_ADDR, MEM_MAPPED_LEN);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Callback for interrupt coming from device
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
static irqreturn_t glEhpiInterruptHandler(int irq, void *dev_id)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) dev_id;

	ASSERT(prGlueInfo);

	if (!prGlueInfo)
		return IRQ_HANDLED;

	/* 1. Running for ISR */
	wlanISR(prGlueInfo->prAdapter, TRUE);

	/* 1.1 Halt flag Checking */
	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT)
		return IRQ_HANDLED;

	/* 2. Flag marking for interrupt */
	set_bit(GLUE_FLAG_INT_BIT, &prGlueInfo->ulFlag);

	/* 3. wake up tx service thread */
	wake_up_interruptible(&prGlueInfo->waitq);

	return IRQ_HANDLED;
}
