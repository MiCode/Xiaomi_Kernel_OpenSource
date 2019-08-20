/**
 * File: ispctrl_if_master_err.h
 * Description: ISP Ctrl IF Master error definition
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2013/10/14; Aaron Chuang; Initial version
 *  2013/12/05; Bruce Chung; 2nd version
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


#ifndef	_ISPCTRLIFMASTER_ERR_H_
#define	_ISPCTRLIFMASTER_ERR_H_

/******Include File******/


#include "../error_pj.h"


/******Public Define******/
/* command Size mismatch*/
#define ERR_MASTERCMDSIZE_MISMATCH (ERR_BASE_PJ_ISPCTRLIF_MASTER + 0x000)
/* null buffer*/
#define ERR_MASTERCMDBUF_NULL (ERR_BASE_PJ_ISPCTRLIF_MASTER + 0x001)
/* command options unsupport*/
#define ERR_MASTERCMDOPTION_UNSUPPORT (ERR_BASE_PJ_ISPCTRLIF_MASTER + 0x002)
/* open file error*/
#define ERR_MASTERCMDCKSM_INVALID (ERR_BASE_PJ_ISPCTRLIF_MASTER + 0x003)
/* open file error*/
#define ERR_MASTEROPENFILE_FAILED (ERR_BASE_PJ_ISPCTRLIF_MASTER + 0x004)
/* open file error*/
#define ERR_MASTEROPENFILE_FILENAME_INVALID (ERR_BASE_PJ_ISPCTRLIF_MASTER+\
						0x005)
/* event timeout*/
#define ERR_MASTER_EVENT_TIMEOUT (ERR_BASE_PJ_ISPCTRLIF_MASTER + 0x006)

/* opcode not supported*/
#define ERR_MASTER_OPCODE_UNSUPPORT (ERR_BASE_PJ_ISPCTRLIF_MASTER + 0x007)

#endif
