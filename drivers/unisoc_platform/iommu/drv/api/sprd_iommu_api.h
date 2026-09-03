/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_IOMMU_API_H_
#define _SPRD_IOMMU_API_H_

#include "../inc/sprd_defs.h"
#include "../osal/sprd_osal.h"
#include "../com/sprd_com.h"

/*Declares a handle to the iommu object*/
SPRD_DECLARE_HANDLE(sprd_iommu_hdl);

extern struct sprd_iommu_func_tbl iommuex_func_tbl;
extern struct sprd_iommu_func_tbl iommuvau_func_tbl;

/********************************************************
* @enum SPRDChannelType
* @brief Define channel type,
*
* The structure includes a list for Iommu channel .
*********************************************************/
enum sprd_iommu_ch_type {
	PF_CH_READ = 0x100,/*prefetch channel only support read*/
	PF_CH_WRITE,/* prefetch channel only support write*/
	FM_CH_RW,/*ullmode channel support write/read in one channel*/
	EX_CH_READ,/*channel only support read, only ISP use now*/
	EX_CH_WRITE,/*channel only support read, only ISP use now*/
	CH_TYPE_INVALID, /*unsupported channel type*/
};

enum sprd_iommu_type {
	SPRD_IOMMUEX_SHARKLE,
	SPRD_IOMMUEX_PIKE2,
	SPRD_IOMMUEX_SHARKL3,
	SPRD_IOMMUEX_SHARKL5,
	SPRD_IOMMUEX_ROC1,
	SPRD_IOMMUVAU_SHARKL5P,
	SPRD_IOMMUVAU_SHARKL6,
	SPRD_IOMMUVAU_SHARKL6P,
	SPRD_IOMMUVAU_QOGIRN6L,
	SPRD_IOMMU_NOT_SUPPORT,
};


/*******************************************************
* @struct SPRDIommuInitParam
* @brief Initialize parameter for Iommu
*
* The structure includes Initialize parameter.
********************************************************/
struct sprd_iommu_init_param {
	enum sprd_iommu_type iommu_type;
	enum IOMMU_ID iommu_id;
	ulong master_reg_addr;/*master base register address */
	ulong base_reg_addr;
	u32 pgt_size;
	ulong ctrl_reg_addr;
	u32 phys_offset;

	ulong fm_base_addr;/*fullmode virtual pool base address*/
	u32 fm_ram_size;
	u64 faultpage_addr;/* Enabel fault page function */
	u8 ram_clk_div;/*Clock divisor*/
	unsigned long pagt_base_ddr;
	unsigned long pagt_base_virt;
	unsigned int pagt_ddr_size;

	/*for sharkl2/isharkl2*/
	u64 mini_ppn1;
	u64 ppn1_range;
	u64 mini_ppn2;
	u64 ppn2_range;
	int chip;
};

/******************************************************
* @struct SPRDIommuMapParam
* @brief Configure maping parameter for Iommu, including physical
	map or others , mapping use which channel
*
* The structure includes maping parameter.
******************************************************/
struct sprd_iommu_map_param {
	enum sprd_iommu_ch_type channel_type;/*which kinds of channel would
				used to map , dynamic alloc from channel pool*/
	u8 channel_bypass;/*whether setting channel bypass
				0:Normal ; 1: channel  in  bypass mode*/

	u64 start_virt_addr;
	u32 total_map_size;
	struct sg_table *p_sg_table;
	u32 sg_offset;
};


/*********************************************************
* @struct SPRDIommuMapParam
* @brief Configure maping parameter for Iommu, including physical
	map or others , mapping use which channel
*
* The structure includes maping parameter.
**********************************************************/
struct sprd_iommu_unmap_param {
	u64 start_virt_addr;
	u32 total_map_size;
	enum sprd_iommu_ch_type ch_type;
	u32 ch_id;
};


/******************************************************
* @struct SPRDIommuFuncTbl
* @brief Iommu was built-in several hardware IP such as VSP/DCAM/GSP etc,
	choose setup abstract common layer for operate iommu
*
* The structure includes operations for each iommu.
*******************************************************/
struct sprd_iommu_func_tbl {
	u32 (*init)(struct sprd_iommu_init_param *p_init_param, sprd_iommu_hdl p_iommu_hdl);
	u32  (*uninit)(sprd_iommu_hdl p_iommu_hdl);

	u32  (*map)(sprd_iommu_hdl p_iommu_hdl, struct sprd_iommu_map_param *p_map_param);
	u32  (*unmap)(sprd_iommu_hdl p_iommu_hdl, struct sprd_iommu_unmap_param *p_unmap_param);

	u32  (*enable)(sprd_iommu_hdl p_iommu_hdl);
	u32  (*disable)(sprd_iommu_hdl p_iommu_hdl);

	u32  (*suspend)(sprd_iommu_hdl p_iommu_hdl);
	u32  (*resume)(sprd_iommu_hdl p_iommu_hdl);
	u32  (*release)(sprd_iommu_hdl p_iommu_hdl);

	u32  (*reset)(sprd_iommu_hdl p_iommu_hdl, u32 channel_num);
	u32  (*set_bypass)(sprd_iommu_hdl p_iommu_hdl, bool vaor_bp_en);
	u32  (*virttophy)(sprd_iommu_hdl p_iommu_hdl, u64 virt_addr, u64 *dest_addr);

	u32  (*unmap_orphaned)(sprd_iommu_hdl p_iommu_hdl, struct sprd_iommu_unmap_param *p_unmap_param);
};


/*******************************************************
* @struct SPRDIommuWidget
* @brief sprd_iommu.h/c was upper on hardware iommu driver
*
* The structure include detaild iommu private data structure.
*******************************************************/
struct sprd_iommu_widget {
	void *p_priv;
	struct sprd_iommu_func_tbl *p_iommu_tbl;
};

/*-----------------------------------------------------------------------*/
/*                          FUNCTIONS HEADERS                            */
/*-----------------------------------------------------------------------*/


/**********************************************************
 * @name SPRDIommuInit
 * ---------------------------------------------------------
 * @brief Init Iommu resource and hardware setting.
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * ---------------------------------------------------------
 * Arguments:
 * @param[in]		pInitParam - Initialized parameters.
 * @param[out]	p_iommu_hdl - Pointer to Iommu handle.
 **********************************************************/
u32 sprd_iommudrv_init(struct sprd_iommu_init_param *p_init_param,
		  sprd_iommu_hdl *p_iommu_hdl);


/*********************************************************
 * @name SPRDIommuUninit
 * --------------------------------------------------------
 * @brief Unnit Iommu resource and hardware setting.
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * --------------------------------------------------------
 * Arguments:
 * @param[in]		p_iommu_hdl - Pointer to Iommu handle.
 *********************************************************/
u32 sprd_iommudrv_uninit(sprd_iommu_hdl p_iommu_hdl);

/********************************************************
 * @name SPRDIommuMap
 * ------------------------------------------------------
 * @brief Setup Iommu Map
 *
 * @return u32
 * @retval ch_num - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * ------------------------------------------------------
 * Arguments:
 * @param[in]	p_iommu_hdl - Pointer to Iommu handle.
 *******************************************************/
u32 sprd_iommudrv_map(sprd_iommu_hdl p_iommu_hdl,
		  struct sprd_iommu_map_param *p_map_param);


/********************************************************
 * @name SPRDIommuUnmap
 * ------------------------------------------------------
 * @brief Unmap the virtual address with physical pages, clear page
	table , return channel num been unmaped
 *
 * @return u32
 * @retval ch_num - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * -----------------------------------------------------
 * Arguments:
 * @param[in]pIommupfHdl - IommuPf Handle.
 * @param[in]pMapParam - Configure parameters.
 *******************************************************/
u32 sprd_iommudrv_unmap(sprd_iommu_hdl p_iommu_hdl,
		  struct sprd_iommu_unmap_param *p_unmap_param);

/********************************************************
 * @name SPRDIommuUnmap orphaned
 * ------------------------------------------------------
 * @brief Unmap the virtual address with physical pages, clear page
	table , return channel num been unmaped
 *
 * @return u32
 * @retval ch_num - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * -----------------------------------------------------
 * Arguments:
 * @param[in]pIommupfHdl - IommuPf Handle.
 * @param[in]pMapParam - Configure parameters.
 *******************************************************/
u32 sprd_iommudrv_unmap_orphaned(sprd_iommu_hdl p_iommu_hdl,
			struct sprd_iommu_unmap_param *p_unmap_param);

/******************************************************
 * @name SPRDIommuStartCh
 * -----------------------------------------------------
 * @brief Start Iommu. channel
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * -----------------------------------------------------
 * Arguments:
 * @param[in]p_iommu_hdl - Pointer to Iommu handle.
 *******************************************************/
u32 sprd_iommudrv_enable(sprd_iommu_hdl p_iommu_hdl);

/*******************************************************
 * @name SPRDIommuStopCh
 * ------------------------------------------------------
 * @brief Stop Iommu specific channel.
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * ------------------------------------------------------
 * Arguments:
 * @param[in]		p_iommu_hdl - Pointer to Iommu handle.
 ********************************************************/
u32 sprd_iommudrv_disable(sprd_iommu_hdl p_iommu_hdl);


/********************************************************
 * @name SPRDIommuStartCh
 * -------------------------------------------------------
 * @brief Start Iommu. channel
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * ------------------------------------------------------
 * Arguments:
 * @param[in]		p_iommu_hdl - Pointer to Iommu handle.
 ********************************************************/
u32 sprd_iommudrv_suspend(sprd_iommu_hdl p_iommu_hdl);

/********************************************************
 * @name SPRDIommuStopCh
 * -------------------------------------------------------
 * @brief Stop Iommu specific channel.
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * --------------------------------------------------------
 * Arguments:
 * @param[in]		p_iommu_hdl - Pointer to Iommu handle.
 *********************************************************/
u32 sprd_iommudrv_resume(sprd_iommu_hdl p_iommu_hdl);

u32 sprd_iommudrv_release(sprd_iommu_hdl p_iommu_hdl);

/********************************************************
 * @name SPRDIommuReset
 * -------------------------------------------------------
 * @brief Stop Iommu specific channel.
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * --------------------------------------------------------
 * Arguments:
 * @param[in]		p_iommu_hdl - Pointer to Iommu handle.
 *********************************************************/
u32 sprd_iommudrv_reset(sprd_iommu_hdl p_iommu_hdl, u32 channel_num);

u32 sprd_iommudrv_set_bypass(sprd_iommu_hdl  p_iommu_hdl, bool vaor_bp_en);

/**********************************************************
 * @name SPRDIommuVirtToPhy
 * ---------------------------------------------------------
 * @brief Translate one virtual address to physical address.
 *
 * @return u32
 * @retval SPRD_NO_ERR - No error.
 * @retval SPRD_ERR_INVALID_PARAM - Parameters error.
 * ----------------------------------------------------------
 * Arguments:
 * @param[in]		p_iommu_hdl - Iommu Handle.
 * @param[in]		VirtAddr -  Virtual address need to translate.
 **********************************************************/
u32 sprd_iommudrv_virt_to_phy(sprd_iommu_hdl p_iommu_hdl,
			  u64 virt_addr, u64 *dest_addr);

#endif  /*END OF : define  _SPRD_IOMMU_API_H_ */
