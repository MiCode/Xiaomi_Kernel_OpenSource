/*
 * File: altek_state_err.h
 * Description: miniISPISP error definition
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */


#ifndef	_ALTEK_STATE_ERR_H_
#define	_ALTEK_STATE_ERR_H_

/******Include File******/

#include "../error_pj.h"

/******Public Define******/

#define   ERR_MINIISP_STATE_REDUNDANT_SEQUENCE \
		(ERR_BASE_PJ_MINIISP_STATE + 0x000)
#define   ERR_MINIISP_STATE_ERROR_SEQUENCE \
		(ERR_BASE_PJ_MINIISP_STATE + 0x001)

#endif
