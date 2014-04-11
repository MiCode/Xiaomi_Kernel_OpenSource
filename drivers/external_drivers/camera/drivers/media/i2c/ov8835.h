/*
 * Support for Omnivision OV8830 camera sensor.
 * Based on Aptina mt9e013 driver.
 *
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
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

#ifndef __OV8835_H__
#define __OV8835_H__

#include "ov8830.h"

static const struct ov8830_reg ov8835_basic_settings[] = {
	{ OV8830_8BIT, { 0x0103 }, 0x01 },
	{ OV8830_8BIT, { 0x0100 }, 0x00 },
	{ OV8830_8BIT, { 0x0102 }, 0x01 },
	{ OV8830_8BIT, { 0x3001 }, 0x2a },
	{ OV8830_8BIT, { 0x3002 }, 0x88 },
	{ OV8830_8BIT, { 0x3005 }, 0x00 },
	{ OV8830_8BIT, { 0x3011 }, 0x41 },
	{ OV8830_8BIT, { 0x3015 }, 0x08 },
	{ OV8830_8BIT, { 0x301b }, 0xb4 },
	{ OV8830_8BIT, { 0x301d }, 0x02 },
	{ OV8830_8BIT, { 0x3021 }, 0x00 },
	{ OV8830_8BIT, { 0x3022 }, 0x02 },
	{ OV8830_8BIT, { 0x3081 }, 0x02 },
	{ OV8830_8BIT, { 0x3083 }, 0x01 },
	{ OV8830_8BIT, { 0x3090 }, 0x02 }, /* PLL2 Settings 278.4MHz*/
	{ OV8830_8BIT, { 0x3091 }, 0x1d },
	{ OV8830_8BIT, { 0x3094 }, 0x00 },
	{ OV8830_8BIT, { 0x3092 }, 0x00 },
	{ OV8830_8BIT, { 0x3093 }, 0x00 },
	{ OV8830_8BIT, { 0x3098 }, 0x03 }, /* PLL3 Settings REF_CLK 256Mhz*/
	{ OV8830_8BIT, { 0x3099 }, 0x14 },
	{ OV8830_8BIT, { 0x309a }, 0x00 },
	{ OV8830_8BIT, { 0x309b }, 0x00 },
	{ OV8830_8BIT, { 0x309c }, 0x01 },
	{ OV8830_8BIT, { 0x30a2 }, 0x01 }, /* Ref Clk -> Manual mode enabled */
	{ OV8830_8BIT, { 0x30b0 }, 0x05 },
	{ OV8830_8BIT, { 0x30b2 }, 0x00 },
	{ OV8830_8BIT, { 0x30b3 }, 0x6b }, /* MIPI PLL1 Settings 684.4Mbps*/
	{ OV8830_8BIT, { 0x30b4 }, 0x03 },
	{ OV8830_8BIT, { 0x30b5 }, 0x04 },
	{ OV8830_8BIT, { 0x30b6 }, 0x01 },
	{ OV8830_8BIT, { 0x3104 }, 0xa1 },
	{ OV8830_8BIT, { 0x3106 }, 0x01 },
	{ OV8830_8BIT, { 0x3400 }, 0x04 },
	{ OV8830_8BIT, { 0x3401 }, 0x00 },
	{ OV8830_8BIT, { 0x3402 }, 0x04 },
	{ OV8830_8BIT, { 0x3403 }, 0x00 },
	{ OV8830_8BIT, { 0x3404 }, 0x04 },
	{ OV8830_8BIT, { 0x3405 }, 0x00 },
	{ OV8830_8BIT, { 0x3406 }, 0x01 },
	{ OV8830_8BIT, { 0x3503 }, 0x07 },
	{ OV8830_8BIT, { 0x3504 }, 0x00 },
	{ OV8830_8BIT, { 0x3505 }, 0x30 },
	{ OV8830_8BIT, { 0x3506 }, 0x00 },
	{ OV8830_8BIT, { 0x3507 }, 0x10 },
	{ OV8830_8BIT, { 0x3508 }, 0x80 },
	{ OV8830_8BIT, { 0x3509 }, 0x10 },
	{ OV8830_8BIT, { 0x350a }, 0x00 },
	{ OV8830_8BIT, { 0x350b }, 0x38 },
	{ OV8830_8BIT, { 0x3600 }, 0x98 },
	{ OV8830_8BIT, { 0x3601 }, 0x02 },
	{ OV8830_8BIT, { 0x3602 }, 0x7c },
	{ OV8830_8BIT, { 0x3604 }, 0x38 },
	{ OV8830_8BIT, { 0x3612 }, 0x80 },
	{ OV8830_8BIT, { 0x3620 }, 0x41 },
	{ OV8830_8BIT, { 0x3621 }, 0xa4 },
	{ OV8830_8BIT, { 0x3622 }, 0x0f },
	{ OV8830_8BIT, { 0x3625 }, 0x44 },
	{ OV8830_8BIT, { 0x3630 }, 0x55 },
	{ OV8830_8BIT, { 0x3631 }, 0xf2 },
	{ OV8830_8BIT, { 0x3632 }, 0x00 },
	{ OV8830_8BIT, { 0x3633 }, 0x34 },
	{ OV8830_8BIT, { 0x3634 }, 0x03 },
	{ OV8830_8BIT, { 0x364d }, 0x0d },
	{ OV8830_8BIT, { 0x364f }, 0x60 },
	{ OV8830_8BIT, { 0x3660 }, 0x80 },
	{ OV8830_8BIT, { 0x3662 }, 0x10 },
	{ OV8830_8BIT, { 0x3665 }, 0x00 },
	{ OV8830_8BIT, { 0x3666 }, 0x00 },
	{ OV8830_8BIT, { 0x3667 }, 0x00 },
	{ OV8830_8BIT, { 0x366a }, 0x80 },
	{ OV8830_8BIT, { 0x366c }, 0x00 },
	{ OV8830_8BIT, { 0x366d }, 0x00 },
	{ OV8830_8BIT, { 0x366e }, 0x00 },
	{ OV8830_8BIT, { 0x366f }, 0x20 },
	{ OV8830_8BIT, { 0x3680 }, 0xb5 },
	{ OV8830_8BIT, { 0x3681 }, 0x00 },
	{ OV8830_8BIT, { 0x3701 }, 0x14 },
	{ OV8830_8BIT, { 0x3702 }, 0x50 },
	{ OV8830_8BIT, { 0x3703 }, 0x8c },
	{ OV8830_8BIT, { 0x3704 }, 0x68 },
	{ OV8830_8BIT, { 0x3705 }, 0x02 },
	{ OV8830_8BIT, { 0x3709 }, 0x43 },
	{ OV8830_8BIT, { 0x370a }, 0x00 },
	{ OV8830_8BIT, { 0x370b }, 0x20 },
	{ OV8830_8BIT, { 0x370c }, 0x0c },
	{ OV8830_8BIT, { 0x370d }, 0x11 },
	{ OV8830_8BIT, { 0x370e }, 0x00 },
	{ OV8830_8BIT, { 0x370f }, 0x00 },
	{ OV8830_8BIT, { 0x3710 }, 0x00 },
	{ OV8830_8BIT, { 0x371c }, 0x01 },
	{ OV8830_8BIT, { 0x371f }, 0x0c },
	{ OV8830_8BIT, { 0x3721 }, 0x00 },
	{ OV8830_8BIT, { 0x3724 }, 0x10 },
	{ OV8830_8BIT, { 0x3726 }, 0x00 },
	{ OV8830_8BIT, { 0x372a }, 0x01 },
	{ OV8830_8BIT, { 0x3730 }, 0x18 },
	{ OV8830_8BIT, { 0x3738 }, 0x22 },
	{ OV8830_8BIT, { 0x3739 }, 0xd0 },
	{ OV8830_8BIT, { 0x373a }, 0x50 },
	{ OV8830_8BIT, { 0x373b }, 0x02 },
	{ OV8830_8BIT, { 0x373c }, 0x20 },
	{ OV8830_8BIT, { 0x373f }, 0x02 },
	{ OV8830_8BIT, { 0x3740 }, 0x42 },
	{ OV8830_8BIT, { 0x3741 }, 0x02 },
	{ OV8830_8BIT, { 0x3742 }, 0x18 },
	{ OV8830_8BIT, { 0x3743 }, 0x01 },
	{ OV8830_8BIT, { 0x3744 }, 0x02 },
	{ OV8830_8BIT, { 0x3747 }, 0x10 },
	{ OV8830_8BIT, { 0x374c }, 0x04 },
	{ OV8830_8BIT, { 0x3751 }, 0xf0 },
	{ OV8830_8BIT, { 0x3752 }, 0x00 },
	{ OV8830_8BIT, { 0x3753 }, 0x00 },
	{ OV8830_8BIT, { 0x3754 }, 0xc0 },
	{ OV8830_8BIT, { 0x3755 }, 0x00 },
	{ OV8830_8BIT, { 0x3756 }, 0x1a },
	{ OV8830_8BIT, { 0x3758 }, 0x00 },
	{ OV8830_8BIT, { 0x3759 }, 0x0f },
	{ OV8830_8BIT, { 0x375c }, 0x04 },
	{ OV8830_8BIT, { 0x3767 }, 0x01 },
	{ OV8830_8BIT, { 0x376b }, 0x44 },
	{ OV8830_8BIT, { 0x3774 }, 0x10 },
	{ OV8830_8BIT, { 0x3776 }, 0x00 },
	{ OV8830_8BIT, { 0x377f }, 0x08 },
	{ OV8830_8BIT, { 0x3780 }, 0x22 },
	{ OV8830_8BIT, { 0x3781 }, 0xcc },
	{ OV8830_8BIT, { 0x3784 }, 0x2c },
	{ OV8830_8BIT, { 0x3785 }, 0x08 },
	{ OV8830_8BIT, { 0x3786 }, 0x16 },
	{ OV8830_8BIT, { 0x378f }, 0xf5 },
	{ OV8830_8BIT, { 0x3791 }, 0xb0 },
	{ OV8830_8BIT, { 0x3795 }, 0x00 },
	{ OV8830_8BIT, { 0x3796 }, 0x94 },
	{ OV8830_8BIT, { 0x3797 }, 0x11 },
	{ OV8830_8BIT, { 0x3798 }, 0x30 },
	{ OV8830_8BIT, { 0x3799 }, 0x41 },
	{ OV8830_8BIT, { 0x379a }, 0x07 },
	{ OV8830_8BIT, { 0x379b }, 0xb0 },
	{ OV8830_8BIT, { 0x379c }, 0x0c },
	{ OV8830_8BIT, { 0x37c5 }, 0x00 },
	{ OV8830_8BIT, { 0x37c6 }, 0xa0 },
	{ OV8830_8BIT, { 0x37c7 }, 0x00 },
	{ OV8830_8BIT, { 0x37c9 }, 0x00 },
	{ OV8830_8BIT, { 0x37ca }, 0x00 },
	{ OV8830_8BIT, { 0x37cb }, 0x00 },
	{ OV8830_8BIT, { 0x37cc }, 0x00 },
	{ OV8830_8BIT, { 0x37cd }, 0x00 },
	{ OV8830_8BIT, { 0x37ce }, 0x01 },
	{ OV8830_8BIT, { 0x37cf }, 0x00 },
	{ OV8830_8BIT, { 0x37d1 }, 0x01 },
	{ OV8830_8BIT, { 0x37de }, 0x00 },
	{ OV8830_8BIT, { 0x37df }, 0x00 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3823 }, 0x00 },
	{ OV8830_8BIT, { 0x3824 }, 0x00 },
	{ OV8830_8BIT, { 0x3825 }, 0x00 },
	{ OV8830_8BIT, { 0x3826 }, 0x00 },
	{ OV8830_8BIT, { 0x3827 }, 0x00 },
	{ OV8830_8BIT, { 0x382a }, 0x04 },
	{ OV8830_8BIT, { 0x3a04 }, 0x09 },
	{ OV8830_8BIT, { 0x3a05 }, 0xa9 },
	{ OV8830_8BIT, { 0x3a06 }, 0x00 },
	{ OV8830_8BIT, { 0x3a07 }, 0xf8 },
	{ OV8830_8BIT, { 0x3b00 }, 0x00 },
	{ OV8830_8BIT, { 0x3b02 }, 0x00 },
	{ OV8830_8BIT, { 0x3b03 }, 0x00 },
	{ OV8830_8BIT, { 0x3b04 }, 0x00 },
	{ OV8830_8BIT, { 0x3b05 }, 0x00 },
	{ OV8830_8BIT, { 0x3d00 }, 0x00 },
	{ OV8830_8BIT, { 0x3d01 }, 0x00 },
	{ OV8830_8BIT, { 0x3d02 }, 0x00 },
	{ OV8830_8BIT, { 0x3d03 }, 0x00 },
	{ OV8830_8BIT, { 0x3d04 }, 0x00 },
	{ OV8830_8BIT, { 0x3d05 }, 0x00 },
	{ OV8830_8BIT, { 0x3d06 }, 0x00 },
	{ OV8830_8BIT, { 0x3d07 }, 0x00 },
	{ OV8830_8BIT, { 0x3d08 }, 0x00 },
	{ OV8830_8BIT, { 0x3d09 }, 0x00 },
	{ OV8830_8BIT, { 0x3d0a }, 0x00 },
	{ OV8830_8BIT, { 0x3d0b }, 0x00 },
	{ OV8830_8BIT, { 0x3d0c }, 0x00 },
	{ OV8830_8BIT, { 0x3d0d }, 0x00 },
	{ OV8830_8BIT, { 0x3d0e }, 0x00 },
	{ OV8830_8BIT, { 0x3d0f }, 0x00 },
	{ OV8830_8BIT, { 0x3d80 }, 0x00 },
	{ OV8830_8BIT, { 0x3d81 }, 0x00 },
	{ OV8830_8BIT, { 0x3d84 }, 0x00 },
	{ OV8830_8BIT, { 0x4000 }, 0x18 },
	{ OV8830_8BIT, { 0x4001 }, 0x04 },
	{ OV8830_8BIT, { 0x4002 }, 0x45 },
	{ OV8830_8BIT, { 0x4005 }, 0x18 },
	{ OV8830_8BIT, { 0x4006 }, 0x20 },
	{ OV8830_8BIT, { 0x4008 }, 0x24 },
	{ OV8830_8BIT, { 0x4009 }, 0x10 },
	{ OV8830_8BIT, { 0x4100 }, 0x17 },
	{ OV8830_8BIT, { 0x4101 }, 0x03 },
	{ OV8830_8BIT, { 0x4102 }, 0x04 },
	{ OV8830_8BIT, { 0x4103 }, 0x03 },
	{ OV8830_8BIT, { 0x4104 }, 0x5a },
	{ OV8830_8BIT, { 0x4307 }, 0x30 },
	{ OV8830_8BIT, { 0x4315 }, 0x00 },
	{ OV8830_8BIT, { 0x4511 }, 0x05 },
	{ OV8830_8BIT, { 0x4512 }, 0x01 }, /* Binning option Average */
	{ OV8830_8BIT, { 0x4805 }, 0x21 },
	{ OV8830_8BIT, { 0x4806 }, 0x00 },
	{ OV8830_8BIT, { 0x481f }, 0x36 },
	{ OV8830_8BIT, { 0x4831 }, 0x6c },
	{ OV8830_8BIT, { 0x4837 }, 0x0c }, /* MIPI Global timing */
	{ OV8830_8BIT, { 0x4a00 }, 0xaa },
	{ OV8830_8BIT, { 0x4a03 }, 0x01 },
	{ OV8830_8BIT, { 0x4a05 }, 0x08 },
	{ OV8830_8BIT, { 0x4a0a }, 0x88 },
	{ OV8830_8BIT, { 0x4d03 }, 0xbb },
	{ OV8830_8BIT, { 0x5000 }, 0x06 },
	{ OV8830_8BIT, { 0x5001 }, 0x01 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 },
	{ OV8830_8BIT, { 0x5003 }, 0x20 },
	{ OV8830_8BIT, { 0x5013 }, 0x00 },
	{ OV8830_8BIT, { 0x5046 }, 0x4a },
	{ OV8830_8BIT, { 0x5780 }, 0x1c },
	{ OV8830_8BIT, { 0x5786 }, 0x20 },
	{ OV8830_8BIT, { 0x5787 }, 0x10 },
	{ OV8830_8BIT, { 0x5788 }, 0x18 },
	{ OV8830_8BIT, { 0x578a }, 0x04 },
	{ OV8830_8BIT, { 0x578b }, 0x02 },
	{ OV8830_8BIT, { 0x578c }, 0x02 },
	{ OV8830_8BIT, { 0x578e }, 0x06 },
	{ OV8830_8BIT, { 0x578f }, 0x02 },
	{ OV8830_8BIT, { 0x5790 }, 0x02 },
	{ OV8830_8BIT, { 0x5791 }, 0xff },
	{ OV8830_8BIT, { 0x5a08 }, 0x02 },
	{ OV8830_8BIT, { 0x5e00 }, 0x00 },
	{ OV8830_8BIT, { 0x5e10 }, 0x0c },
	{ OV8830_TOK_TERM, {0}, 0}
};

/*****************************OV8835 STILL & PREVIEW**************************/

static const struct ov8830_reg ov8835_cif_mode[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 172, 20, 3123, 2459 2944x2432 */
	{ OV8830_8BIT, { 0x3801 }, 0xac },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x14 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0x33 },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0x9b },
	{ OV8830_8BIT, { 0x3808 }, 0x01 }, /* 368x304 O/p Binning+Scaling */
	{ OV8830_8BIT, { 0x3809 }, 0x70 },
	{ OV8830_8BIT, { 0x380a }, 0x01 },
	{ OV8830_8BIT, { 0x380b }, 0x30 },
	{ OV8830_8BIT, { 0x3814 }, 0x71 },
	{ OV8830_8BIT, { 0x3815 }, 0x71 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0xa0 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};


static struct ov8830_reg const ov8835_binning_4x3_mode[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 4, 3291, 2475, 3288X2472 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x06 }, /* Output size: 1632x1224 */
	{ OV8830_8BIT, { 0x3809 }, 0x60 },
	{ OV8830_8BIT, { 0x380a }, 0x04 },
	{ OV8830_8BIT, { 0x380b }, 0xc8 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Vertical Binning 0n */
	{ OV8830_8BIT, { 0x3821 }, 0x0f }, /* Horizontal Binning 0n */
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0xa0 },
	{ OV8830_8BIT, { 0x5002 }, 0x00 }, /* Scale disable */
	{ OV8830_8BIT, { 0x5041 }, 0x84 }, /* Set manual scale and disable */
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8835_binning_16x9_mode[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 310, 3291, 2169 3288x1860 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x36 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x79 },
	{ OV8830_8BIT, { 0x3808 }, 0x06 }, /* Output size 1632x 916 */
	{ OV8830_8BIT, { 0x3809 }, 0x60 },
	{ OV8830_8BIT, { 0x380a }, 0x03 },
	{ OV8830_8BIT, { 0x380b }, 0x94 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x08 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0x90 },
	{ OV8830_8BIT, { 0x5002 }, 0x00 }, /* Scale disable */
	{ OV8830_8BIT, { 0x5041 }, 0x84 }, /* Set manual scale and disable */
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8835_8M_mode[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe3 }, /* Binning Related */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 4, 3291, 2475, 3288X2472 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x0c }, /* Output size 3280x2464 */
	{ OV8830_8BIT, { 0x3809 }, 0xd0 },
	{ OV8830_8BIT, { 0x380a }, 0x09 },
	{ OV8830_8BIT, { 0x380b }, 0xa0 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 }, /* Binning off */
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 },
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_8BIT, { 0x4004 }, 0x08 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0xa0 },
	{ OV8830_8BIT, { 0x5002 }, 0x00 }, /* Scale disable */
	{ OV8830_8BIT, { 0x5041 }, 0x84 }, /* Set manual scale and disable */
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8835_6M_mode[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe3 }, /* Binning Related */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 310, 3291, 2169 3288x1860 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x36 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x79 },
	{ OV8830_8BIT, { 0x3808 }, 0x0c }, /* Output size 3280x1852 */
	{ OV8830_8BIT, { 0x3809 }, 0xd0 },
	{ OV8830_8BIT, { 0x380a }, 0x07 },
	{ OV8830_8BIT, { 0x380b }, 0x3c },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_8BIT, { 0x4004 }, 0x08 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0x90 },
	{ OV8830_8BIT, { 0x5002 }, 0x00 }, /* Scale disable */
	{ OV8830_8BIT, { 0x5041 }, 0x84 }, /* Set manual scale and disable */
	{ OV8830_TOK_TERM, {0}, 0}
};

/***************** OV8835 VIDEO ***************************************/

static const struct ov8830_reg ov8835_video_qcif[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 180, 16,3115, 2463 2928x2440 */
	{ OV8830_8BIT, { 0x3801 }, 0xb4 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x10 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0x2b },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0x9f },
	{ OV8830_8BIT, { 0x3808 }, 0x00 }, /* O/p Binning + Scaling 192x160 */
	{ OV8830_8BIT, { 0x3809 }, 0xc0 },
	{ OV8830_8BIT, { 0x380a }, 0x00 },
	{ OV8830_8BIT, { 0x380b }, 0xa0 },
	{ OV8830_8BIT, { 0x3814 }, 0x71 },
	{ OV8830_8BIT, { 0x3815 }, 0x71 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0xa0 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8835_video_qvga_dvs[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 4, 3291, 2475 3288x2472 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x01 }, /* 408x308 Binning+Scaling */
	{ OV8830_8BIT, { 0x3809 }, 0x98 },
	{ OV8830_8BIT, { 0x380a }, 0x01 },
	{ OV8830_8BIT, { 0x380b }, 0x34 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0xa0 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8835_video_vga_dvs[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 4, 3291, 2475 3288x2472 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x03 }, /* 820x616 Binning + Scaling */
	{ OV8830_8BIT, { 0x3809 }, 0x34 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0x68 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0xa0 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_reg const ov8835_video_480p_dvs[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x01 },
	{ OV8830_8BIT, { 0x3801 }, 0x08 },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x40 },
	{ OV8830_8BIT, { 0x3804 }, 0x0b },
	{ OV8830_8BIT, { 0x3805 }, 0xd5 },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x73 }, /* TODO! 2766 x 1844 */
	{ OV8830_8BIT, { 0x3808 }, 0x03 }, /* 936x602 Binning + Scaling */
	{ OV8830_8BIT, { 0x3809 }, 0xa8 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0x5a },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Binning on */
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0xa0 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_reg const ov8835_video_800x600_dvs[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x01 }, /* 414, 310, 2881, 2169 2468x1860 */
	{ OV8830_8BIT, { 0x3801 }, 0x9e },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x36 },
	{ OV8830_8BIT, { 0x3804 }, 0x0b },
	{ OV8830_8BIT, { 0x3805 }, 0x41 },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x79 },
	{ OV8830_8BIT, { 0x3808 }, 0x03 }, /* O/p 976x736 Bin+Scale */
	{ OV8830_8BIT, { 0x3809 }, 0xD0 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0xe0 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0x90 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_reg const ov8835_video_720p_dvs[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe6 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 310, 3291, 2169 3288x1860 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x36 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x79 },
	{ OV8830_8BIT, { 0x3808 }, 0x06 }, /* O/p 1568*880 Bin+Scale */
	{ OV8830_8BIT, { 0x3809 }, 0x20 },
	{ OV8830_8BIT, { 0x380a }, 0x03 },
	{ OV8830_8BIT, { 0x380b }, 0x70 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_8BIT, { 0x4004 }, 0x02 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0x90 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8835_video_1080p_dvs[] = {
	{ OV8830_8BIT, { 0x3708 }, 0xe3 }, /* Binning Related e6 : e3 */
	{ OV8830_8BIT, { 0x3800 }, 0x00 }, /* 4, 310, 3291, 2169 3288x1860 */
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x36 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x79 },
	{ OV8830_8BIT, { 0x3808 }, 0x09 }, /* 2336x1336 DVS O/p */
	{ OV8830_8BIT, { 0x3809 }, 0x20 },
	{ OV8830_8BIT, { 0x380a }, 0x05 },
	{ OV8830_8BIT, { 0x380b }, 0x38 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 },
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_8BIT, { 0x4004 }, 0x08 }, /* BLC No. of blacklines used. */
	{ OV8830_8BIT, { 0x404f }, 0x90 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 }, /* Scale enable */
	{ OV8830_8BIT, { 0x5041 }, 0x04 }, /* Auto scale */
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_resolution ov8835_res_preview[] = {
	{
		 .desc = "ov8835_cif_mode_for_preview",
		 .width = 368,
		 .height = 304,
		 .used = 0,
		 .regs = ov8835_cif_mode,
		 .bin_factor_x = 2,
		 .bin_factor_y = 2,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_binning_16x9_mode_for_preview",
		 .width = 1632,
		 .height = 916,
		 .used = 0,
		 .regs = ov8835_binning_16x9_mode,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4496,
				 .lines_per_frame = 2064,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_bnning_4x3_mode",
		 .width = 1632,
		 .height = 1224,
		 .used = 0,
		 .regs = ov8835_binning_4x3_mode,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4400,
				 .lines_per_frame = 2100,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_6M_mode_for_cont_capture",
		 .width = 3280,
		 .height = 1852,
		 .used = 0,
		 .regs = ov8835_6M_mode,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4496,
				 .lines_per_frame = 2064,
			},
			{
				 .fps = 24,
				 .pixels_per_line = 5496,
				 .lines_per_frame = 2064,
			},
			{
				 .fps = 19,
				 .pixels_per_line = 5800,
				 .lines_per_frame = 2500,
			}
		},
	},
	{
		.desc = "ov8835_8M_mode_for_cont_capture",
		.width = 3280,
		.height = 2464,
		.used = 0,
		.regs = ov8835_8M_mode,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 25,
				.pixels_per_line = 4450,
				.lines_per_frame = 2500,
			},
			{
				.fps = 19,
				.pixels_per_line = 5800,
				.lines_per_frame = 2500,
			},
			{
			}
		},
	},
};

static struct ov8830_resolution ov8835_res_still[] = {
	{
		 .desc = "ov8835_cif_mode_for_still",
		 .width = 368,
		 .height = 304,
		 .used = 0,
		 .regs = ov8835_cif_mode,
		 .bin_factor_x = 2,
		 .bin_factor_y = 2,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 6814,
				 .lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_binning_16x9_mode_for_still",
		 .width = 1632,
		 .height = 916,
		 .used = 0,
		 .regs = ov8835_binning_16x9_mode,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				.fps = 15,
				.pixels_per_line = 6814,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_bnning_4x3_mode_for_still",
		 .width = 1632,
		 .height = 1224,
		 .used = 0,
		 .regs = ov8835_binning_4x3_mode,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				.fps = 15,
				.pixels_per_line = 6474,
				.lines_per_frame = 2867,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_6M_mode_for_still",
		 .width = 3280,
		 .height = 1852,
		 .used = 0,
		 .regs = ov8835_6M_mode,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 1,
		 .fps_options = {
			{
				.fps = 15,
				.pixels_per_line = 6814,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		.desc = "ov8835_cont_cap_8M_for_still",
		.width = 3280,
		.height = 2464,
		.used = 0,
		.regs = ov8835_8M_mode,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 15,
				.pixels_per_line = 6474,
				.lines_per_frame = 2867,
			},
			{
			}
		},
	},
};

static struct ov8830_resolution ov8835_res_video[] = {
	{
		 .desc = "ov8835_video_qcif",
		 .width = 192,
		 .height = 160,
		 .used = 0,
		 .regs = ov8835_video_qcif,
		 .bin_factor_x = 2,
		 .bin_factor_y = 2,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_video_cif_dvs",
		 .width = 368,
		 .height = 304,
		 .used = 0,
		 .regs = ov8835_cif_mode,
		 .bin_factor_x = 2,
		 .bin_factor_y = 2,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_video_qvga_dvs",
		 .width = 408,
		 .height = 308,
		 .used = 0,
		 .regs = ov8835_video_qvga_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_video_vga_dvs",
		 .width = 820,
		 .height = 616,
		 .used = 0,
		 .regs = ov8835_video_vga_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		.desc = "ov8835_video_480p_dvs",
		.width = 936,
		.height = 602,
		.used = 0,
		.regs = ov8835_video_480p_dvs,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.skip_frames = 1,
		.fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_video_800x600_dvs",
		 .width = 976,
		 .height = 736,
		 .used = 0,
		 .regs = ov8835_video_800x600_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_video_720p_dvs",
		 .width = 1568,
		 .height = 880,
		 .used = 0,
		 .regs = ov8835_video_720p_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 5458,
				 .lines_per_frame = 1700,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8835_video_1080p_dvs",
		 .width = 2336,
		 .height = 1336,
		 .used = 0,
		 .regs = ov8835_video_1080p_dvs,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 2,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4300,
				 .lines_per_frame = 2158,
			},
			{
				 .fps = 24,
				 .pixels_per_line = 5300,
				 .lines_per_frame = 2158,
			},

			{
			}
		},
	},
};

#endif
