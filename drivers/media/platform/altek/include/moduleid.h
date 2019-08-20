/*
 * File: moduleid.h
 * Description: Define module id
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


/**
 *@file moduleid.h
 *@author Gene Hung
 *@version 2005/08/22; Gene; Add Doxygen remark
 *@defgroup ModuleID Module ID definition
 *@brief TBirdOS 2.0 and later module ID definition.
 *code number definition:
 *Bits 31~20: Module id
 *Bits 19~12: Reserved
 *Bits 11~0: Code defined in each module
 */

#ifndef _MODULEID_H_
#define _MODULEID_H_

/**
 *@ingroup ModuleID
 *@{
 */

/**
 *@def MODULEID_SHIFTBITS
 *@brief Module ID MARCO definition
 */
#define MODULEID_SHIFTBITS 20
/**
 *@def MODULEID_ModuleID
 *@brief Get ID number from a CODE
 */
#define MODULEID_ModuleID(code) (code >> MODULEID_SHIFTBITS)
/**
 *@def MODULEID_ModuleBase
 *@brief Get CODE BASE from a module ID
 */
#define MODULEID_ModuleBase(id) (id << MODULEID_SHIFTBITS)

/* Project-dependent module starts from this ID*/
#define MODULEID_PROJECT 0x400


/* Let Project use MODULEID_PROJECT to extend. */
/*Don't define module ID 0x401 0x402... here.*/

/**
 *@}
 */
#endif /*_MODULEID_H_*/
