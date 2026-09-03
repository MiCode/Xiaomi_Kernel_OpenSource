// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "sprd_iommu_api.h"

u32 sprd_iommudrv_init(struct sprd_iommu_init_param *p_init_param,
		  sprd_iommu_hdl *p_iommu_hdl)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;
	struct sprd_iommu_func_tbl *func_tbl = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)sprd_malloc(
					sizeof(struct sprd_iommu_widget));

	if (p_init_param->iommu_type == SPRD_IOMMUEX_SHARKL3
		|| p_init_param->iommu_type == SPRD_IOMMUEX_SHARKLE
		|| p_init_param->iommu_type == SPRD_IOMMUEX_PIKE2
		|| p_init_param->iommu_type == SPRD_IOMMUEX_SHARKL5
		|| p_init_param->iommu_type == SPRD_IOMMUEX_ROC1) {
		func_tbl = (&iommuex_func_tbl);
	} else if (p_init_param->iommu_type == SPRD_IOMMUVAU_SHARKL5P ||
		   p_init_param->iommu_type == SPRD_IOMMUVAU_SHARKL6 ||
		   p_init_param->iommu_type == SPRD_IOMMUVAU_SHARKL6P ||
		   p_init_param->iommu_type == SPRD_IOMMUVAU_QOGIRN6L) {
		func_tbl = (&iommuvau_func_tbl);
	} else {
		sprd_free(p_iommu_data);
		return SPRD_ERR_INVALID_PARAM;
	}

	p_iommu_data->p_iommu_tbl = func_tbl;
	if (p_iommu_data->p_iommu_tbl != NULL)
		p_iommu_data->p_iommu_tbl->init(p_init_param,
					(sprd_iommu_hdl)p_iommu_data);

	*p_iommu_hdl = (sprd_iommu_hdl)p_iommu_data;
	return SPRD_NO_ERR;
}

u32 sprd_iommudrv_uninit(sprd_iommu_hdl p_iommu_hdl)
{
	struct sprd_iommu_widget *p_iommu_data;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl != NULL)
		p_iommu_data->p_iommu_tbl->uninit((sprd_iommu_hdl)p_iommu_data);

	sprd_free(p_iommu_data);

	return SPRD_NO_ERR;
}

u32 sprd_iommudrv_map(sprd_iommu_hdl p_iommu_hdl,
		  struct sprd_iommu_map_param *p_map_param)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if ((p_iommu_hdl == NULL) || (p_map_param == NULL))
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->map(p_iommu_hdl, p_map_param);
}

u32 sprd_iommudrv_unmap(sprd_iommu_hdl p_iommu_hdl,
		  struct sprd_iommu_unmap_param *p_unmap_param)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->unmap(p_iommu_hdl,
							p_unmap_param);
}

u32 sprd_iommudrv_unmap_orphaned(sprd_iommu_hdl p_iommu_hdl,
			struct sprd_iommu_unmap_param *p_unmap_param)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->unmap_orphaned(p_iommu_hdl,
							p_unmap_param);
}

u32 sprd_iommudrv_enable(sprd_iommu_hdl p_iommu_hdl)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->enable(p_iommu_hdl);
}


u32 sprd_iommudrv_disable(sprd_iommu_hdl p_iommu_hdl)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->disable(p_iommu_hdl);
}


u32 sprd_iommudrv_suspend(sprd_iommu_hdl p_iommu_hdl)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->suspend(p_iommu_hdl);
}


u32 sprd_iommudrv_resume(sprd_iommu_hdl p_iommu_hdl)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->resume(p_iommu_hdl);
}

u32 sprd_iommudrv_reset(sprd_iommu_hdl  p_iommu_hdl, u32 channel_num)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->reset(p_iommu_hdl,
							channel_num);
}

u32 sprd_iommudrv_set_bypass(sprd_iommu_hdl  p_iommu_hdl, bool vaor_bp_en)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->set_bypass(p_iommu_hdl,
							  vaor_bp_en);
}

u32 sprd_iommudrv_release(sprd_iommu_hdl p_iommu_hdl)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	if (p_iommu_data->p_iommu_tbl->release)
		p_iommu_data->p_iommu_tbl->release(p_iommu_hdl);

	return SPRD_NO_ERR;
}

u32 sprd_iommudrv_virt_to_phy(sprd_iommu_hdl p_iommu_hdl,
			  u64 virt_addr, u64 *dest_addr)
{
	struct sprd_iommu_widget *p_iommu_data = NULL;

	if (p_iommu_hdl == NULL)
		return SPRD_ERR_INVALID_PARAM;

	p_iommu_data = (struct sprd_iommu_widget *)p_iommu_hdl;

	if (p_iommu_data->p_iommu_tbl == NULL)
		return SPRD_ERR_INVALID_PARAM;
	else
		return p_iommu_data->p_iommu_tbl->virttophy(p_iommu_hdl,
						virt_addr,
						dest_addr);
}
