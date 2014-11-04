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

#ifndef __IA_CSS_PRBS_H
#define __IA_CSS_PRBS_H

/** @file
 * This file contains support for Pseudo Random Bit Sequence (PRBS) inputs
 */

/** Enumerate the PRBS IDs.
 */
enum ia_css_prbs_id {
	IA_CSS_PRBS_ID0,
	IA_CSS_PRBS_ID1,
	IA_CSS_PRBS_ID2
};

/**
 * PRBS configuration structure.
 *
 * Seed the for the Pseudo Random Bit Sequence.
 *
 * @deprecated{This interface is deprecated, it is not portable -> move to input system API}
 */
struct ia_css_prbs_config {
	enum ia_css_prbs_id	id;
	unsigned int		h_blank;	/**< horizontal blank */
	unsigned int		v_blank;	/**< vertical blank */
	int			seed;	/**< random seed for the 1st 2-pixel-components/clock */
	int			seed1;	/**< random seed for the 2nd 2-pixel-components/clock */
};

#endif /* __IA_CSS_PRBS_H */
