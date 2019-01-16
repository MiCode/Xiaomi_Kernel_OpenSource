#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

#include "mt8193_ctrl.h"
#include "mt8193hdmicmd.h"

static u32 pdMpegInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000085,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000dE,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdMpegInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000085,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000d4,	/* check sum */
	0x188, 0x00000041,
	0x188, 0x00000052,
	0x188, 0x00000003,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000004
};


static u32 pdSpdInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000083,	/* type */
	0x1A4, 0x00000019,	/* length, 25 bytes */
	0x188, 0x000000d1,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdSpdInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000083,	/* type */
	0x1A4, 0x00000019,	/* length, 25 bytes */
	0x188, 0x000000cf,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000002
};



static u32 pdAudioInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000084,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x0000006a,	/* check sum */
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000007,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdAudioInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000084,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x00000055,	/* check sum */
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000007,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000005,
	0x188, 0x00000006
};


static u32 pdVendorSpecInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000081,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000E2,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdVendorSpecInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000081,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000d2,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000006,
	0x188, 0x00000000
};


static u32 pdGenericInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000087,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000DC,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdGenericInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000087,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000BA,	/* check sum */
	0x188, 0x00000041,
	0x188, 0x00000051,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000010,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000006
};


static u32 pdACPInfoReg[] = {
	0x19c, 0x00000002,	/* version, acp type */
	0x1A0, 0x00000004,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static u32 pdACPInfoReg2[] = {
	0x19c, 0x00000002,	/* version, acp type */
	0x1A0, 0x00000004,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000001,
	0x188, 0x00000001
};


static u32 pdISRC1InfoReg[] = {
	0x19c, 0x00000002,	/* version, ISRC status */
	0x1A0, 0x00000005,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static u32 pdISRC1InfoReg2[] = {
	0x19c, 0x00000002,	/* version, ISRC status */
	0x1A0, 0x00000005,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000d0,	/* check sum, PB0 */
	0x188, 0x00000041,
	0x188, 0x00000051,
	0x188, 0x00000003,
	0x188, 0x00000001
};


static u32 pdISRC2InfoReg[] = {
	0x19c, 0x00000000,	/* version, ISRC status */
	0x1A0, 0x00000006,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static u32 pdISRC2InfoReg2[] = {
	0x19c, 0x00000000,	/* version, ISRC status */
	0x1A0, 0x00000006,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000d6,	/* check sum, PB0 */
	0x188, 0x00000042,
	0x188, 0x00000052,
	0x188, 0x00000002,
	0x188, 0x00000004
};


static u32 pdGamutInfoReg[] = {
	0x19c, 0x00000080,	/* HB1 version */
	0x1A0, 0x0000000a,	/* HB0 type */
	0x1A4, 0x00000030,	/* HB2 */
	0x188, 0x00000000	/* PB0 */
};

static u32 pdGamutInfoReg2[] = {
	0x19c, 0x00000080,	/* HB1 version */
	0x1A0, 0x0000000a,	/* HB0 type */
	0x1A4, 0x00000030,	/* HB2 */
	0x188, 0x00000001	/* PB0 */
};

void mt8193_InfoframeSetting(u8 i1typemode, u8 i1typeselect)
{
	u32 u4Ind;
	u8 bData;

	if ((i1typemode == 0xff) && (i1typeselect == 0xff)) {
		pr_debug("Arg1: Infoframe output type\n");
		pr_debug("1: AVi, 2: Mpeg, 3: SPD\n");
		pr_debug("4: Vendor, 5: Audio, 6: ACP\n");
		pr_debug("7: ISRC1, 8: ISRC2,  9:GENERIC\n");
		pr_debug("10:GAMUT\n");

		pr_debug("Arg2: Infoframe data select\n");
		pr_debug("0: old(default), 1: new;\n");
		return;
	}

	if (i1typeselect == 0) {
		switch (i1typemode) {
		case 1:	/* Avi */

			vSendAVIInfoFrame(HDMI_VIDEO_1280x720p_50Hz, 1);

			break;

		case 2:	/* Mpeg */
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdMpegInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdMpegInfoReg[u4Ind], pdMpegInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 4), (1 << 4));

			break;

		case 3:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdSpdInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdSpdInfoReg[u4Ind], pdSpdInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 3), (1 << 3));
			break;


		case 4:
			/* Vendor spec */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 0));
			for (u4Ind = 0; u4Ind < (sizeof(pdVendorSpecInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdVendorSpecInfoReg[u4Ind],
						  pdVendorSpecInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 0), (1 << 0));
			break;


		case 5:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 5));
			for (u4Ind = 0; u4Ind < (sizeof(pdAudioInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdAudioInfoReg[u4Ind], pdAudioInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 5), (1 << 5));

			break;

			/* ACP */
		case 6:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 1));
			for (u4Ind = 0; u4Ind < (sizeof(pdACPInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdACPInfoReg[u4Ind], pdACPInfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 1), (1 << 1));
			break;

			/* ISCR1 */
		case 7:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC1InfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC1InfoReg[u4Ind], pdISRC1InfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 2), (1 << 2));
			break;

		case 8:
			/* ISCR2 */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC2InfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC2InfoReg[u4Ind], pdISRC2InfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 3), (1 << 3));
			break;

		case 9:
			/* Generic spec */

			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdGenericInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGenericInfoReg[u4Ind],
						  pdGenericInfoReg[u4Ind + 1]);

			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 2), (1 << 2));
			break;

		case 10:

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdGamutInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGamutInfoReg[u4Ind], pdGamutInfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 4), (1 << 4));

			vSendAVIInfoFrame(HDMI_VIDEO_720x480p_60Hz, HDMI_XV_YCC);

			break;


		case 11:
			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;

		case 12:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 5);	/* disable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 5);	/* disable original mute */
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			break;

		case 13:

			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData &= ~(0x1 << 5);	/* enable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			break;
		case 14:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);
			bData &= ~(0x1 << 6);
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			break;
		case 15:
			/* vCMDHwNCTSOnOff(FALSE);// change to software NCTS; */
			/* vCMDHDMI_NCTS(0x03, 0x12); */

			break;
		default:
			break;

		}
	} else {
		switch (i1typemode) {
		case 1:	/* Avi */

			vSendAVIInfoFrame(HDMI_VIDEO_1280x720p_50Hz, 2);

			break;

		case 2:	/* Mpeg */
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdMpegInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdMpegInfoReg2[u4Ind], pdMpegInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 4), (1 << 4));

			break;

		case 3:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdSpdInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdSpdInfoReg2[u4Ind], pdSpdInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 3), (1 << 3));
			break;


		case 4:
			/* Vendor spec */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 0));
			for (u4Ind = 0; u4Ind < (sizeof(pdVendorSpecInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdVendorSpecInfoReg2[u4Ind],
						  pdVendorSpecInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 0), (1 << 0));
			break;


		case 5:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 5));
			for (u4Ind = 0; u4Ind < (sizeof(pdAudioInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdAudioInfoReg2[u4Ind],
						  pdAudioInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 5), (1 << 5));

			break;


			/* ACP */
		case 6:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 1));
			for (u4Ind = 0; u4Ind < (sizeof(pdACPInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdACPInfoReg2[u4Ind], pdACPInfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 1), (1 << 1));
			break;

			/* ISCR1 */
		case 7:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC1InfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC1InfoReg2[u4Ind],
						  pdISRC1InfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 2), (1 << 2));
			break;

		case 8:
			/* ISCR2 */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC2InfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC2InfoReg2[u4Ind],
						  pdISRC2InfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 3), (1 << 3));
			break;

		case 9:
			/* Generic spec */

			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdGenericInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGenericInfoReg2[u4Ind],
						  pdGenericInfoReg2[u4Ind + 1]);

			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 2), (1 << 2));
			break;

		case 10:

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdGamutInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGamutInfoReg2[u4Ind],
						  pdGamutInfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 4), (1 << 4));

			vSendAVIInfoFrame(HDMI_VIDEO_720x480p_60Hz, HDMI_XV_YCC);

			break;


		case 11:
			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;

		case 12:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 5);	/* disable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 5);	/* disable original mute */
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			break;

		case 13:

			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData &= ~(0x1 << 5);	/* enable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;
		case 14:
			bData = bReadByteHdmiGRL(GRL_CFG4);

			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);
			bData &= ~(0x1 << 6);
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			break;
		case 15:
			/* vCMDHwNCTSOnOff(FALSE);// change to software NCTS; */
			/* vCMDHDMI_NCTS(0x03, 0x12); */

			break;
		default:
			break;

		}

	}
}
#endif
