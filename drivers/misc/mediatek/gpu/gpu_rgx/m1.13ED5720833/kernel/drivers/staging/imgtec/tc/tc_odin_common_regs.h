/*************************************************************************/ /*!
@File           odin_common_regs.h
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef __TC_ODIN_COMMON_REGS_H__
#define __TC_ODIN_COMMON_REGS_H__

#include <linux/types.h>
#include <linux/stringify.h>

struct tc_device;

enum odin_common_regs {
	CORE_REVISION = 0,
	CORE_CHANGE_SET,
	CORE_USER_ID,
	CORE_USER_BUILD,
	CORE_INTERRUPT_ENABLE,
	CORE_INTERRUPT_CLR,
	CORE_INTERRUPT_STATUS,
	REG_BANK_ODN_CLK_BLK,
};

#define ODIN_REGNAME(REG_NAME) "ODN_" __stringify(REG_NAME)
#define ORION_REGNAME(REG_NAME) "SRS_" __stringify(REG_NAME)

struct odin_orion_reg {
	u32 odin_offset;
	u32 orion_offset;
	const char *odin_name;
	const char *orion_name;
};

#define COMMON_REG_ENTRY(REG) \
	[REG] = {				  \
		.odin_offset = ODN_##REG,	  \
		.orion_offset = SRS_##REG,	  \
		.odin_name = ODIN_REGNAME(REG),	  \
		.orion_name = ORION_REGNAME(REG), \
	}

static const struct odin_orion_reg common_regs[] = {
	COMMON_REG_ENTRY(CORE_REVISION),
	COMMON_REG_ENTRY(CORE_CHANGE_SET),
	COMMON_REG_ENTRY(CORE_USER_ID),
	COMMON_REG_ENTRY(CORE_USER_BUILD),
	COMMON_REG_ENTRY(CORE_INTERRUPT_ENABLE),
	COMMON_REG_ENTRY(CORE_INTERRUPT_CLR),
	COMMON_REG_ENTRY(CORE_INTERRUPT_STATUS),
	COMMON_REG_ENTRY(REG_BANK_ODN_CLK_BLK),
};

static inline const u32 common_reg_offset(struct tc_device *tc, u32 reg)
{
	if (tc->odin)
		return common_regs[reg].odin_offset;
	else
		return common_regs[reg].orion_offset;
}

static inline const char *common_reg_name(struct tc_device *tc, u32 reg)
{
	if (tc->odin)
		return common_regs[reg].odin_name;
	else
		return common_regs[reg].orion_name;
}

#endif	/* __TC_ODIN_COMMON_REGS_H__ */
