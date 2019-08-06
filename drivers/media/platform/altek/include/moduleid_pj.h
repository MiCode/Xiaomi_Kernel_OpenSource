/*
 * File: Moduleid_PJ.h
 * Description: Define module id  in project
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


#ifndef _MODULEID_PJ_H_
#define _MODULEID_PJ_H_

#include "moduleid.h"

/* Project-dependent module starts from MODULEID_PROJECT (0x400)*/
#define MODULEID_PJ_MINIISP_STATE	 (MODULEID_PROJECT + 0x2C)
#define MODULEID_PJ_ISPCTRLIF_SLAVE	 (MODULEID_PROJECT + 0x2D)
#define MODULEID_PJ_ISPCTRLIF_MASTER	(MODULEID_PROJECT + 0x2E)
#define MODULEID_PJ_MINIISP			 (MODULEID_PROJECT + 0x2F)
#endif




