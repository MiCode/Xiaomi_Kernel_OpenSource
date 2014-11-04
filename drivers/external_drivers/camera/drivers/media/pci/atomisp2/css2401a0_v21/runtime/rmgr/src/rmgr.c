/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "ia_css_rmgr.h"

enum ia_css_err ia_css_rmgr_init(void)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	err = ia_css_rmgr_init_vbuf(vbuf_ref);
	if (err == IA_CSS_SUCCESS)
		err = ia_css_rmgr_init_vbuf(vbuf_write);
	if (err == IA_CSS_SUCCESS)
		err = ia_css_rmgr_init_vbuf(hmm_buffer_pool);
	if (err != IA_CSS_SUCCESS)
		ia_css_rmgr_uninit();
	return err;
}

/**
 * @brief Uninitialize resource pool (host)
 */
void ia_css_rmgr_uninit(void)
{
	ia_css_rmgr_uninit_vbuf(hmm_buffer_pool);
	ia_css_rmgr_uninit_vbuf(vbuf_write);
	ia_css_rmgr_uninit_vbuf(vbuf_ref);
}
