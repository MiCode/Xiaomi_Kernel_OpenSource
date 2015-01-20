/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __IA_CSS_SDIS_COMMON_TYPES_H
#define __IA_CSS_SDIS_COMMON_TYPES_H

/** @file
* CSS-API header file for DVS statistics parameters.
*/

#include <type_support.h>

/** DVS statistics grid dimensions in number of cells.
 */

struct ia_css_dvs_grid_dim {
	uint32_t width;		/**< Width of DVS grid table in cells */
	uint32_t height;	/**< Height of DVS grid table in cells */
};

/** DVS statistics dimensions in number of cells for
 * grid, coeffieicient and projection.
 */

struct ia_css_sdis_info {
	struct {
		struct ia_css_dvs_grid_dim dim; /* Dimensions */
		struct ia_css_dvs_grid_dim pad; /* Padded dimensions */
	} grid, coef, proj;
	uint32_t deci_factor_log2;
};

#define IA_CSS_DEFAULT_SDIS_INFO \
	{	\
		{	{ 0, 0 },	/* dim */ \
			{ 0, 0 },	/* pad */ \
		},	/* grid */ \
		{	{ 0, 0 },	/* dim */ \
			{ 0, 0 },	/* pad */ \
		},	/* coef */ \
		{	{ 0, 0 },	/* dim */ \
			{ 0, 0 },	/* pad */ \
		},	/* proj */ \
		0,	/* dis_deci_factor_log2 */ \
	}

/** DVS statistics grid
 *
 *  ISP block: SDVS1 (DIS/DVS Support for DIS/DVS ver.1 (2-axes))
 *             SDVS2 (DVS Support for DVS ver.2 (6-axes))
 *  ISP1: SDVS1 is used.
 *  ISP2: SDVS2 is used.
 */
struct ia_css_dvs_grid_res {
	uint32_t width;	    	/**< Width of DVS grid table.
					(= Horizontal number of grid cells
					in table, which cells have effective
					statistics.)
					For DVS1, this is equal to
					 the number of vertical statistics. */
	uint32_t aligned_width; /**< Stride of each grid line.
					(= Horizontal number of grid cells
					in table, which means
					the allocated width.) */
	uint32_t height;	/**< Height of DVS grid table.
					(= Vertical number of grid cells
					in table, which cells have effective
					statistics.)
					For DVS1, This is equal to
					the number of horizontal statistics. */
	uint32_t aligned_height;/**< Stride of each grid column.
					(= Vertical number of grid cells
					in table, which means
					the allocated height.) */
};

/* TODO: use ia_css_dvs_grid_res in here.
 * However, that implies driver I/F changes
 */
struct ia_css_dvs_grid_info {
	uint32_t enable;        /**< DVS statistics enabled.
					0:disabled, 1:enabled */
	uint32_t width;	    	/**< Width of DVS grid table.
					(= Horizontal number of grid cells
					in table, which cells have effective
					statistics.)
					For DVS1, this is equal to
					 the number of vertical statistics. */
	uint32_t aligned_width; /**< Stride of each grid line.
					(= Horizontal number of grid cells
					in table, which means
					the allocated width.) */
	uint32_t height;	/**< Height of DVS grid table.
					(= Vertical number of grid cells
					in table, which cells have effective
					statistics.)
					For DVS1, This is equal to
					the number of horizontal statistics. */
	uint32_t aligned_height;/**< Stride of each grid column.
					(= Vertical number of grid cells
					in table, which means
					the allocated height.) */
	uint32_t bqs_per_grid_cell; /**< Grid cell size in BQ(Bayer Quad) unit.
					(1BQ means {Gr,R,B,Gb}(2x2 pixels).)
					For DVS1, valid value is 64.
					For DVS2, valid value is only 64,
					currently. */
	uint32_t num_hor_coefs;	/**< Number of horizontal coefficients. */
	uint32_t num_ver_coefs;	/**< Number of vertical coefficients. */
};

#define DEFAULT_DVS_GRID_INFO \
{ \
	0,				/* enable */ \
	0,				/* width */ \
	0,				/* aligned_width */ \
	0,				/* height */ \
	0,				/* aligned_height */ \
	0,				/* bqs_per_grid_cell */ \
	0,				/* num_hor_coefs */ \
	0,				/* num_ver_coefs */ \
}

#endif /* __IA_CSS_SDIS_COMMON_TYPES_H */
