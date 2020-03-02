/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k3m3_setting_v2.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor setting file
 *
 * Setting Release Date:
 * ------------
 *     2016.09.01
 *
 ****************************************************************************/
#ifndef _s5k3m3MIPI_SETTING_V2_H_
#define _s5k3m3MIPI_SETTING_V2_H_


static void sensor_init_v2(void)
{
#ifdef FrameAESync
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x30EC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0449);
	write_cmos_sensor(0x6F12, 0x0348);
	write_cmos_sensor(0x6F12, 0x044A);
	write_cmos_sensor(0x6F12, 0x0860);
	write_cmos_sensor(0x6F12, 0x101A);
	write_cmos_sensor(0x6F12, 0x8880);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x4EB8);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x3240);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1E80);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x4C00);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x2D4D);
	write_cmos_sensor(0x6F12, 0x0646);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x6968);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8FB2);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x5EF8);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x60F8);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x56F8);
	write_cmos_sensor(0x6F12, 0x2548);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x5800);
	write_cmos_sensor(0x6F12, 0x2860);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF081);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x204E);
	write_cmos_sensor(0x6F12, 0x8846);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0xB168);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x0D0C);
	write_cmos_sensor(0x6F12, 0x8FB2);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x43F8);
	write_cmos_sensor(0x6F12, 0x7088);
	write_cmos_sensor(0x6F12, 0x2080);
	write_cmos_sensor(0x6F12, 0x4146);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x47F8);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x36B8);
	write_cmos_sensor(0x6F12, 0x0179);
	write_cmos_sensor(0x6F12, 0x8079);
	write_cmos_sensor(0x6F12, 0x01B9);
	write_cmos_sensor(0x6F12, 0x48B1);
	write_cmos_sensor(0x6F12, 0x0121);
	write_cmos_sensor(0x6F12, 0x401E);
	write_cmos_sensor(0x6F12, 0x8140);
	write_cmos_sensor(0x6F12, 0x1F23);
	write_cmos_sensor(0x6F12, 0x8B43);
	write_cmos_sensor(0x6F12, 0x0422);
	write_cmos_sensor(0x6F12, 0x1149);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x37B8);
	write_cmos_sensor(0x6F12, 0x1F21);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x38B8);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x5F01);
	write_cmos_sensor(0x6F12, 0x0C48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x36F8);
	write_cmos_sensor(0x6F12, 0x084C);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x9F01);
	write_cmos_sensor(0x6F12, 0xA060);
	write_cmos_sensor(0x6F12, 0x0948);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x2EF8);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x4701);
	write_cmos_sensor(0x6F12, 0x6060);
	write_cmos_sensor(0x6F12, 0x0748);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x27F8);
	write_cmos_sensor(0x6F12, 0xE060);
	write_cmos_sensor(0x6F12, 0x10BD);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x3230);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1CC0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x5553);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x75C7);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x71B1);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0635);
	write_cmos_sensor(0x6F12, 0x40F6);
	write_cmos_sensor(0x6F12, 0x4F0C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x47F2);
	write_cmos_sensor(0x6F12, 0xB11C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x47F2);
	write_cmos_sensor(0x6F12, 0xC75C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0xCD2C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0x473C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4DF6);
	write_cmos_sensor(0x6F12, 0x4B2C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x30D3);
	write_cmos_sensor(0x6F12, 0x02AB);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0x3D70, 0x0002);
	write_cmos_sensor(0x3D6A, 0x0001);
	write_cmos_sensor(0x3D66, 0x0105);
	write_cmos_sensor(0x3D64, 0x0105);
	write_cmos_sensor(0x3D6C, 0x0080);
	write_cmos_sensor(0xF496, 0x0048);
	write_cmos_sensor(0xF470, 0x0008);
	write_cmos_sensor(0xF43A, 0x0015);
	write_cmos_sensor(0x3676, 0x0008);
	write_cmos_sensor(0x3678, 0x0008);
	write_cmos_sensor(0x32A8, 0x000C);
	write_cmos_sensor(0x3238, 0x000B);
	write_cmos_sensor(0x3230, 0x000C);
	write_cmos_sensor(0x3AC8, 0x0A04);
	write_cmos_sensor(0x322E, 0x000C);
	write_cmos_sensor(0x3236, 0x000B);
	write_cmos_sensor(0x32A6, 0x000C);
	write_cmos_sensor(0x362A, 0x0303);
	write_cmos_sensor(0xF442, 0x44C6);
	write_cmos_sensor(0xF408, 0xFFF7);
	write_cmos_sensor(0x3666, 0x030B);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0x32F8, 0x0003);
	write_cmos_sensor(0x32F0, 0x0001);
	write_cmos_sensor(0x3616, 0x0707);
	write_cmos_sensor(0x3622, 0x0808);
	write_cmos_sensor(0x3626, 0x0808);
	write_cmos_sensor(0x32EE, 0x0001);
	write_cmos_sensor(0x32F6, 0x0003);
	write_cmos_sensor(0x361E, 0x3030);
	write_cmos_sensor(0x3670, 0x0001);
	write_cmos_sensor(0x31B6, 0x0008);
	write_cmos_sensor(0xF4D0, 0x0034);
	write_cmos_sensor(0xF4D8, 0x0034);
	write_cmos_sensor(0xF636, 0x00D6);
	write_cmos_sensor(0xF638, 0x00DE);
	write_cmos_sensor(0xF63A, 0x00EE);
	write_cmos_sensor(0xF63C, 0x00F6);
	write_cmos_sensor(0xF63E, 0x0106);
	write_cmos_sensor(0xF640, 0x010E);
	write_cmos_sensor(0x3D34, 0x0010);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x021E, 0x0400);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x30A0, 0x0008);
	write_cmos_sensor(0x0112, 0x0A0A);
	write_cmos_sensor(0x3606, 0x0104);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x3070, 0x0100);
	write_cmos_sensor(0x0B04, 0x0101);
	write_cmos_sensor(0x0B08, 0x0000);
	write_cmos_sensor(0x3090, 0x0904);
	write_cmos_sensor(0x3058, 0x0001);
	write_cmos_sensor(0x3150, 0x1838);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x157C);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x15F0);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x1898);
	write_cmos_sensor(0x6F12, 0x0101);

#else
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0x3D70, 0x0002);
	write_cmos_sensor(0x3D6A, 0x0001);
	write_cmos_sensor(0x3D66, 0x0105);
	write_cmos_sensor(0x3D64, 0x0105);
	write_cmos_sensor(0x3D6C, 0x0080);
	write_cmos_sensor(0xF496, 0x0048);
	write_cmos_sensor(0xF470, 0x0008);
	write_cmos_sensor(0xF43A, 0x0015);
	write_cmos_sensor(0x3676, 0x0008);
	write_cmos_sensor(0x3678, 0x0008);
	write_cmos_sensor(0x32A8, 0x000C);
	write_cmos_sensor(0x3238, 0x000B);
	write_cmos_sensor(0x3230, 0x000C);
	write_cmos_sensor(0x3AC8, 0x0A04);
	write_cmos_sensor(0x322E, 0x000C);
	write_cmos_sensor(0x3236, 0x000B);
	write_cmos_sensor(0x32A6, 0x000C);
	write_cmos_sensor(0x362A, 0x0303);
	write_cmos_sensor(0xF442, 0x44C6);
	write_cmos_sensor(0xF408, 0xFFF7);
	write_cmos_sensor(0x3666, 0x030B);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0x32F8, 0x0003);
	write_cmos_sensor(0x32F0, 0x0001);
	write_cmos_sensor(0x3616, 0x0707);
	write_cmos_sensor(0x3622, 0x0808);
	write_cmos_sensor(0x3626, 0x0808);
	write_cmos_sensor(0x32EE, 0x0001);
	write_cmos_sensor(0x32F6, 0x0003);
	write_cmos_sensor(0x361E, 0x3030);
	write_cmos_sensor(0x3670, 0x0001);
	write_cmos_sensor(0x31B6, 0x0008);
	write_cmos_sensor(0xF4D0, 0x0034);
	write_cmos_sensor(0xF4D8, 0x0034);
	write_cmos_sensor(0xF636, 0x00D6);
	write_cmos_sensor(0xF638, 0x00DE);
	write_cmos_sensor(0xF63A, 0x00EE);
	write_cmos_sensor(0xF63C, 0x00F6);
	write_cmos_sensor(0xF63E, 0x0106);
	write_cmos_sensor(0xF640, 0x010E);
	write_cmos_sensor(0x3D34, 0x0010);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x021E, 0x0400);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x30A0, 0x0008);
	write_cmos_sensor(0x0112, 0x0A0A);
	write_cmos_sensor(0x3606, 0x0104);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x3070, 0x0100);
	write_cmos_sensor(0x0B04, 0x0101);
	write_cmos_sensor(0x0B08, 0x0000);
	write_cmos_sensor(0x3090, 0x0904);
	write_cmos_sensor(0x3058, 0x0001);
	write_cmos_sensor(0x3150, 0x1838);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x157C);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x15F0);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x1898);
	write_cmos_sensor(0x6F12, 0x0101);
#endif
}				/*      sensor_init  */


static void preview_setting_v2(kal_uint16 fps)
{
	write_cmos_sensor_byte(0x0100, 0x00);
	check_stremoff(fps);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x1077);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x0838);
	write_cmos_sensor(0x034E, 0x0618);
	write_cmos_sensor(0x0340, 0x0699);
	write_cmos_sensor(0x0342, 0x24C0);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x0900, 0x0112);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0003);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0020);
	write_cmos_sensor(0x3002, 0x0001);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0300, 0x0004);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0032);
	write_cmos_sensor(0x0202, 0x059D);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0206, 0x0020);
#ifdef VCPDAF
	write_cmos_sensor(0X0B0E, 0X0100);
#else
	write_cmos_sensor(0X0B0E, 0X0000);
#endif
	write_cmos_sensor(0x3D06, 0x0010);
#ifdef VCPDAF_PRE
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0000);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0001);
	write_cmos_sensor(0X317A, 0X0115);
#else
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0001);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0000);
	write_cmos_sensor(0X317A, 0X0007);
#endif
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2F34);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x1002);
	write_cmos_sensor(0xF440, 0x006F);
	write_cmos_sensor(0xF494, 0x0020);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x3604, 0x0000);

	write_cmos_sensor_byte(0x0100, 0x01);

}				/*      preview_setting  */



static void capture_setting_v2(kal_uint16 currefps)
{
	write_cmos_sensor_byte(0x0100, 0x00);
	check_stremoff(currefps);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x1077);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x1070);
	write_cmos_sensor(0x034E, 0x0C30);
	write_cmos_sensor(0x0340, 0x0D47);
	write_cmos_sensor(0x0342, 0x1260);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x3002, 0x0001);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0300, 0x0004);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0062);
	write_cmos_sensor(0x0202, 0x0D1F);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0206, 0x0020);
#ifdef VCPDAF
	write_cmos_sensor(0X0B0E, 0X0100);
#else
	write_cmos_sensor(0X0B0E, 0X0000);
#endif
	write_cmos_sensor(0x3D06, 0x0010);
#ifdef VCPDAF
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0000);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0001);
	write_cmos_sensor(0X317A, 0X0115);
#else
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0001);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0000);
	write_cmos_sensor(0X317A, 0X0007);
#endif
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2F34);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x1002);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0xF494, 0x0030);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x3604, 0x0000);


	write_cmos_sensor_byte(0x0100, 0x01);
}

static void hs_video_setting_v2(kal_uint16 fps)
{
	write_cmos_sensor_byte(0x0100, 0x00);
	check_stremoff(fps);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x00C0);
	write_cmos_sensor(0x0346, 0x01E8);
	write_cmos_sensor(0x0348, 0x0FBF);
	write_cmos_sensor(0x034A, 0x0A57);
	write_cmos_sensor(0x034C, 0x0500);
	write_cmos_sensor(0x034E, 0x02D0);
	write_cmos_sensor(0x0340, 0x0380);
	write_cmos_sensor(0x0342, 0x1168);
	write_cmos_sensor(0x3000, 0x0000);
	write_cmos_sensor(0x0900, 0x0113);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0005);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0030);
	write_cmos_sensor(0x3002, 0x0001);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0300, 0x0004);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0032);
	write_cmos_sensor(0x0202, 0x0369);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0206, 0x0020);
	write_cmos_sensor(0x0B0E, 0x0000);
	write_cmos_sensor(0x3D06, 0x0010);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x19E0);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x30E2, 0x0000);
	write_cmos_sensor(0x317A, 0x0007);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2F34);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x1002);
	write_cmos_sensor(0xF440, 0x00AF);
	write_cmos_sensor(0xF494, 0x0020);
	write_cmos_sensor(0xF4CC, 0x0028);
	write_cmos_sensor(0xF4CE, 0x0028);
	write_cmos_sensor(0xF4D2, 0x0034);
	write_cmos_sensor(0xF4D4, 0x0FFF);
	write_cmos_sensor(0xF4D6, 0x0FFF);
	write_cmos_sensor(0xF4DA, 0x0034);
	write_cmos_sensor(0xF4DC, 0x0FFF);
	write_cmos_sensor(0xF4DE, 0x0FFF);
	write_cmos_sensor(0x3604, 0x0001);


	write_cmos_sensor_byte(0x0100, 0x01);

}

static void slim_video_setting_v2(kal_uint16 fps)
{
	write_cmos_sensor_byte(0x0100, 0x00);
	check_stremoff(fps);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x00C0);
	write_cmos_sensor(0x0346, 0x01E8);
	write_cmos_sensor(0x0348, 0x0FBF);
	write_cmos_sensor(0x034A, 0x0A57);
	write_cmos_sensor(0x034C, 0x0500);
	write_cmos_sensor(0x034E, 0x02D0);
	write_cmos_sensor(0x0340, 0x0699);
	write_cmos_sensor(0x0342, 0x24C0);
	write_cmos_sensor(0x3000, 0x0000);
	write_cmos_sensor(0x0900, 0x0113);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0005);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0030);
	write_cmos_sensor(0x3002, 0x0001);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0300, 0x0004);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0032);
	write_cmos_sensor(0x0202, 0x0369);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0206, 0x0020);
	write_cmos_sensor(0x0B0E, 0x0000);
	write_cmos_sensor(0x3D06, 0x0010);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x19E0);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x30E2, 0x0000);
	write_cmos_sensor(0x317A, 0x0007);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2F34);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x1002);
	write_cmos_sensor(0xF440, 0x00AF);
	write_cmos_sensor(0xF494, 0x0020);
	write_cmos_sensor(0xF4CC, 0x0028);
	write_cmos_sensor(0xF4CE, 0x0028);
	write_cmos_sensor(0xF4D2, 0x0034);
	write_cmos_sensor(0xF4D4, 0x0FFF);
	write_cmos_sensor(0xF4D6, 0x0FFF);
	write_cmos_sensor(0xF4DA, 0x0034);
	write_cmos_sensor(0xF4DC, 0x0FFF);
	write_cmos_sensor(0xF4DE, 0x0FFF);
	write_cmos_sensor(0x3604, 0x0001);



	write_cmos_sensor_byte(0x0100, 0x01);
}

static void custom1_setting_v2(kal_uint16 fps)
{
	write_cmos_sensor_byte(0x0100, 0x00);
	check_stremoff(fps);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x1077);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x1070);
	write_cmos_sensor(0x034E, 0x0C30);
	write_cmos_sensor(0x0340, 0x0D2F);
	write_cmos_sensor(0x0342, 0x1716);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x3002, 0x0001);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0300, 0x0004);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x004e);
	write_cmos_sensor(0x0202, 0x0D1F);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0206, 0x0020);
#ifdef VCPDAF
	write_cmos_sensor(0X0B0E, 0X0100);
#else
	write_cmos_sensor(0X0B0E, 0X0000);
#endif
	write_cmos_sensor(0x3D06, 0x0010);
#ifdef VCPDAF
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0000);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0001);
	write_cmos_sensor(0X317A, 0X0115);
#else
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0001);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0000);
	write_cmos_sensor(0X317A, 0X0007);
#endif
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2F34);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x1002);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0xF494, 0x0030);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x3604, 0x0000);


	write_cmos_sensor_byte(0x0100, 0x01);
}


static void custom2_setting_v2(kal_uint16 fps)
{
	write_cmos_sensor_byte(0x0100, 0x00);
	check_stremoff(fps);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x1077);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x0838);
	write_cmos_sensor(0x034E, 0x0618);
	write_cmos_sensor(0x0340, 0x0840);
	write_cmos_sensor(0x0342, 0x24C0);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x0900, 0x0112);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0003);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0020);
	write_cmos_sensor(0x3002, 0x0001);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0300, 0x0004);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0032);
	write_cmos_sensor(0x0202, 0x059D);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0206, 0x0020);
#ifdef VCPDAF
	write_cmos_sensor(0X0B0E, 0X0100);
#else
	write_cmos_sensor(0X0B0E, 0X0000);
#endif
	write_cmos_sensor(0x3D06, 0x0010);
#ifdef VCPDAF_PRE
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0000);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0001);
	write_cmos_sensor(0X317A, 0X0115);
#else
	write_cmos_sensor(0X6028, 0X2000);
	write_cmos_sensor(0X602A, 0X19E0);
	write_cmos_sensor(0X6F12, 0X0001);
	write_cmos_sensor(0X6028, 0X4000);
	write_cmos_sensor(0X30E2, 0X0000);
	write_cmos_sensor(0X317A, 0X0007);
#endif
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2F34);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x1002);
	write_cmos_sensor(0xF440, 0x006F);
	write_cmos_sensor(0xF494, 0x0020);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x3604, 0x0000);

	write_cmos_sensor_byte(0x0100, 0x01);
}

#endif
