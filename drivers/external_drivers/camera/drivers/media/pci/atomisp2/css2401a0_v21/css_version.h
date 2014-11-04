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

/*  */
/* CSS version file */
/*  */

#ifndef __CSS_VERSION_H
#define __CSS_VERSION_H

#define VER	1
#define SUBVER	4

/*
Changelog
---------


VER 0.5, released 05/02/14:
	- Added versioning

VER 0.6, released 11/02/14:
	- Version release mainly for driver and AIC bug fixes

	Changes/Fixes:
		AIC bugs  : 1399,1588,1590,1589,1703 (Linearization hang)
		FW versioning added
		FW tracer for debug in DMEM2
		Removed A0 support from code
		Fixed corrupted fields of gird x/y_end for AWB & AE


VER 0.7, released 17/02/14:
	- Version release mainly for driver and AIC bug fixes

	Changes/Fixes:
		AIC bugs  : 1703 RGB gamma enable
		1st striping pipe in smoketest - if_striped

VER 0.8, released 24/02/14:
	- Version release mainly for validation IGK and SVOS run  (patch 1667)

	Changes/Fixes:
		added missing file to css_skycam_a0t_system.proj.mk
		Updated reset function in mini_vid.c
		added prepare function to mini_vid.c

VER 0.9, released 09/03/14:
	- Version release for : striding , new ATE refernces B0 ( based on ATE B0 official branch fixes )

	Changes/Fixes:
		added striding support
		new ATE refernces B0 official branch fixes


VER 1.0, released 19/03/14:
	- Version release for : striping

	Changes/Fixes:
		added striping
		BZ #1717 fix - raw8 input support


VER 1.1, released 03/04/14:
	- Version release for : sp1 disable and stop start WA

	Changes/Fixes:
		SP1 Implementation Enable\Disable  - set to Disable
		Stop Start WA
		Fix config change logic (set changed to true after setting to default) found in stop start debugging
		Code fixes for the win driver integration


VER 1.2, released 13/04/14:
	- Version release for : Trunk release

	Changes/Fixes:
		Stop start bug fix : shading problem fixed issue of char array accessing by int pointer and aligned the buffers for dma access.
		SP1 Enable\Disable  ( set to Disable in release )
		Input Feeder cropping calculation fix
		if_to_bds_rgbp_dvs_stats: new binary + vga test
		activate striping tests	added:	if_norm_lin_shd_awbfr_bnr_striped_vga	replaced in smoke test:	if_to_yuvp2_no_dpc_ob_stats_3840x16 with	if_to_yuvp2_no_dpc_ob_ae_3840x160
		Enabling 3A stats in if_to_dvs and if_to_tnr
		update B0 & C0 SDK These new versions includes a fix for str2vec issue (see bz1874)
		if_to_dvs 3a stats references:	updated af, awb_fr refs for frame 1,2	added 3a stats refs for frame-3	remaned 3a stats files
		stride fix on second frame fix a bug in second frame with stride > width
		shd_config support for higher reolution images added support for high-resolution images ( > 3840x16)
		added default BDS config based on SF
		yuv shift 12 to 8 bit kernel added to sc_video pipe- used yuv_shift new kernel as in ATE official pipe instead of yuv_down scale (which should be removed from all partial pipes at the end)
		binary for fullest stripe pipe  added binary for fullest supported striping pipe includes if_to_yuvp2_no_dpc_ob_ae
		New resolution (hd and 720p) tests are added for DVS statistics
		Code fixes for the driver integration
		test the fullest pipe w bds :  - add test for the fullest striped binary  test name: if_to_yuvp2_no_dpc_ob_stats_ds
		change fullest hd pipe in rls pkg : binary in the rls pkg changed to sc_video  accelerators are directly connected (not through the VLIW)
		ANR api workaround for striping & cleanup :  ANR transform set_params api seems to be buggy  workaround is to configure CALC registers manually
		DVS statistics support is added + combined DVS statistics API

	Verion build data :
		Merge "[Skycam | BUGFIX] GAC control operations loaded at frame level" 4e11164c6cb59c47676d9ef8acb3f11a9394b479 -> cherry pick unmerged : 4240/14 guy patch includes Nitsan added fixes problems found when start stop debug (I6e470944fdebb03ae27d31386f91cf199ea7872f)    ->cherry pick unmerged  Ia8462ef3e3d9c261912986cd84e2be35b191be3d dror timing patch

VER 1.3, released 29/04/14:
	- Version release for : Trunk release
	Changes/Fixes:
		Save/restore pipe pointers in suspend/resume – requested by driver team
		remove obsolete obrow and obcol kernels
		Enable y-ee-nr
		norm failed with raw8 input fixed
		Trace point enabled with SP1
		remove SQRT_LUT from ANR api
		vga OB test added and new configuration files
		error reporting in ddr_encode functions +cleanup fixed
		DPC minimal test ATE pipe update
		fix input cropping + striping
		Update release binary with fullset striped pipe
		Unwanted/unused includes removed from code
		Slimming the release package

	Verion build data : based on ae79bab (tag: irci_master_20140429_0550, origin/master, origin/HEAD) Merge "[CSI2+] Disable offline capture in ACC pipe"
			    and  added cherry pick unmerged "Measurement code"  5073/3


VER 1.4, released 01/06/14 :
	-Version release for : Trunk release
		Changes/Fixes :
		Added dvs/tnr to operational pipe
		BZ 2817 fix : Sky Cam CSS Host code 18_1.3 memory leak
		relax limits of supported resolutions in bins
		BZ 1426 fix :  Extend config for lin kerneltest
		fix bds default tables script and table
		Align to new SDK fixes
		Fixes for Driver compilation
		YUV downscale kernel bug fix
		BDS reapply assertion SF calc and lut select
		center crop in dvs

	Verion build data :
		irci_master_20140601_0200 no other manual patches



#endif /* __CSS_VERSION_H */
