/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
mrcmcr.h

DESCRIPTION: Convenience macros for access the cp registers in the arm.

REV/DATE: Fri Mar 18 16:34:44 EST 2005
*/

#ifndef __mrcmcr__h_
#define __mrcmcr__h_

/*
* Define some convenience macros to acccess the cp registers from c code
* Lots of macro trickery here.
*
* Takes the same format as the asm instructions and unfortunatly you cannot
* use variables to select the crn, crn or op fields...
*
* For those unfamiliar with the # and string stuff.
* # creates a string from the value and any two strings that are beside
*   are concatenated...thus these create one big asm string for the
*   inline asm code.
*
* When compiled these compile to single asm instructions (fast) but
* without all the hassel of __asm__ __volatile__ (...) =r
*
* Format is:
*
*    unsigned long reg;   // destination variable
*    MRC(reg, p15, 0, c1, c0, 0 );
*
*   MRC read control register
*   MCR control register write
*/

/*
* Some assembly macros so we can use the same macros as in the C version.
* Turns the ASM code a little C-ish but keeps the code consistent and in
* one location...
*/
#ifdef __ASSEMBLY__


#define MRC(reg, processor, op1, crn, crm, op2) \
(mrc      processor , op1 , reg,  crn , crm , op2)

#define MCR(reg, processor, op1, crn, crm, op2) \
(mcr      processor , op1 , reg,  crn , crm , op2)

/*
* C version of the macros.
*/
#else

#define MRC(reg, processor, op1, crn, crm, op2) \
__asm__ __volatile__ ( \
"   mrc   "   #processor "," #op1 ", %0,"  #crn "," #crm "," #op2 "\n" \
: "=r" (reg))

#define MCR(reg, processor, op1, crn, crm, op2) \
__asm__ __volatile__ ( \
"   mcr   "   #processor "," #op1 ", %0,"  #crn "," #crm "," #op2 "\n" \
: : "r" (reg))
#endif


/*
* Easy access convenience function to read CP15 registers from c code
*/
#define MRC15(reg, op1, crn, crm, op2) MRC(reg, p15, op1, crn, crm, op2)
#define MCR15(reg, op1, crn, crm, op2) MCR(reg, p15, op1, crn, crm, op2)

#endif
