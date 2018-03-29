/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/******************************************************************************
 *  INCLUDE LINUX HEADER
 ******************************************************************************/
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include "sec_clk.h"

int sec_clk_enable(struct platform_device *dev)
{
	pr_debug("[sec] Need not to get hacc clock\n");
	return 0;
}

