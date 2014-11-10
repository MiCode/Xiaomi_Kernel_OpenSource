#ifndef _SI_MHL_CALLBACK_API_H_
#define _SI_MHL_CALLBACK_API_H_

union __attribute__((__packed__)) avif_or_cea_861_dtd_u
{
	struct detailed_timing_descriptor_t cea_861_dtd;
	struct avi_info_frame_t avif;
};

enum hpd_high_callback_status {
	/* successful return values for hpd_driven_high(): */

	/* a DTD has been written to the p_avif_or_dtd buffer instead of an AVI
	 * infoframe
	 */
	HH_FMT_DVI = 0x00000000,
	/* No Vendor Specific InfoFrame provided */
	HH_FMT_HDMI_VSIF_NONE = 0x00000001,
	/* HDMI vsif has been filled into p_vsif */
	HH_FMT_HDMI_VSIF_HDMI = 0x00000002,
	/* MHL3 vsif has been filled into p_vsif */
	HH_FMT_HDMI_VSIF_MHL3 = 0x00000003,
	/* a DTD has been written to the DTD buffer instead of an AVI infoframe
	 * and 8620 shall expect HDCP enabled on its HDMI input
	 */
	HH_FMT_DVI_HDCP_ON = 0x00000004,
	/* No Vendor Specific InfoFrame provided and 8620 shall expect HDCP
	 * enabled on its HDMI input
	 */
	HH_FMT_HDMI_VSIF_NONE_HDCP_ON = 0x00000005,
	/* HDMI vsif has been filled into p_vsif and 8620 shall expect HDCP
	 * enabled on its HDMI input
	 */
	HH_FMT_HDMI_VSIF_HDMI_HDCP_ON = 0x00000006,
	/* MHL3 vsif has been filled into p_vsif and 8620 shall expect HDCP
	 * enabled on its HDMI input
	 */
	HH_FMT_HDMI_VSIF_MHL3_HDCP_ON = 0x00000007,
	/* a DTD has been written to the DTD buffer instead of an AVI
	 * infoframe
	 */
	HH_FMT_DVI_NOT_RPT = 0x00000008,
	/* No Vendor Specific InfoFrame provided */
	HH_FMT_HDMI_VSIF_NONE_NOT_RPT = 0x00000009,
	/*  HDMI vsif has been filled into p_vsif */
	HH_FMT_HDMI_VSIF_HDMI_NOT_RPT = 0x0000000A,
	/* MHL3 vsif has been filled into p_vsif */
	HH_FMT_HDMI_VSIF_MHL3_NOT_RPT = 0x0000000B,
	/* a DTD has been written to the DTD buffer instead of an AVI infoframe
	 * and 8620 shall expect HDCP enabled on its HDMI input
	 */
	HH_FMT_DVI_HDCP_ON_NOT_RPT = 0x0000000C,
	/* No Vendor Specific InfoFrame provided and 8620 shall expect HDCP
	 * enabled on its HDMI input
	 */
	HH_FMT_HDMI_VSIF_NONE_HDCP_ON_NOT_RPT = 0x0000000D,
	/* HDMI vsif has been filled into p_vsif and 8620 shall expect HDCP
	 * enabled on its HDMI input
	 */
	HH_FMT_HDMI_VSIF_HDMI_HDCP_ON_NOT_RPT = 0x0000000E,
	/* MHL3 vsif has been filled into p_vsif and 8620 shall expect HDCP
	 * enabled on its HDMI input
	 */
	HH_FMT_HDMI_VSIF_MHL3_HDCP_ON_NOT_RPT = 0x0000000F,

	/* failure return values for hpd_driven_high(): */

	/* avi_max_length not large enough for AVI info frame data. */
	HH_AVI_BUFFER_TOO_SMALL = 0x80000001,
	/* vsif_max_length not large enough for info frame data. */
	HH_VSIF_BUFFER_TOO_SMALL = 0x80000002,
	/* The callee is not ready to start video */
	HH_VIDEO_NOT_RDY = 0x80000004
};

struct __attribute__((__packed__)) si_mhl_callback_api_t {
	void *context;
#if 1
	int (*display_timing_enum_begin) (void *context);
	int (*display_timing_enum_item) (void *context, uint16_t columns,
		uint16_t rows, uint8_t bits_per_pixel,
		uint32_t vertical_refresh_rate_in_milliHz, uint16_t burst_id,
		union video_burst_descriptor_u *p_descriptor);
	int (*display_timing_enum_end) (void *context);
#endif
	/*
	   hpd_driven_low:
	   This gets called in response to CLR_HPD messages from the MHL sink.
	   The upstream client that registers this callback should disable
	   video and all DDC access before returning.
	 */
	void (*hpd_driven_low) (void *context);

	/*
	   hpd_driven_high:
	   This gets called when the driver is ready for upstream video
	   activity. The upstream client that registers this callback should
	   respond in the same way in which it would respond to a rising HPD
	   signal (which happens prior to the call).
	   Parameters:
	   *p_edid:

	   The processed EDID block(s) that the MHL driver has derived
	   from the downstream EDID and WRITE_BURST info associated with the
	   sink's responses to 3D_REQ (MHL 2.x) or FEAT_REQ (MHL 3.x and newer).

	   edid_length:

	  *p_emsc_edid:
		The unprocessed EDID transferred via eMSC BLOCK
			using SILICON_IMAGE_ADOPTER_ID

	   emsc_edid_length:
		The length, in bytes of the data in *p_emsc_edid

	   The length, in bytes, of the data in *p_edid

	   *p_hev_dtd:
	   The processed result of all the HEV_DTDA/HEV_DTDB WRITE_BURSTs
	   including the associated 3D_DTD VDI for each HEV_DTD pair.

	   num_hev_dtds:
	   The number of MHL3_hev_dtd_t elements in *p_hev_dtd.

	   p_hev_vic:
	   The processed result of all the HEV_VIC WRITE_BURSTs including
	   the associated 3D_VIC VDI for each HEV_DTD pair.

	   num_hev_vic_items:
	   The number of MHL3_hev_vic_item_t elements in p_hev_vic.

	   *p_3d_dtd_items:
	   The processed result of all the 3D_DTD WRITE_BURSTs including
	   the associated DTD from the EDID when VDI_H.HEV_FMT is zero.

	   num_3d_dtd_items:
	   The number of MHL3_3d_dtd_item_t elements in p_3d_dtd_items;

	   *p_3d_vic:
	   The processed result of all the 3D_VIC WRITE_BURSTs including
	   the associated VIC code from the EDID.

	   num_3d_vic_items:
	   The number of MHL3_3d_vic_item_t elements in p_3d_vic.

	   p_avif_or_dtd:

	   If the callee sends HDMI content, it shall fill in *p_avif_or_dtd
	   with the contents of its outgoing AVI info frame, including the
	   checksum byte, and return one of the values described under the
	   parameter p_vsif.

	   If the callee, sends DVI content, is shall fill in *p_avif_or_dtd
	   with a Detailed Timing Descriptor (defined in CEA-861D) that
	   accurately describes the timing parameters of the video which is
	   presented at the 8620's HDMI input and return one of:
	   HH_FMT_DVI
	   HH_FMT_DVI_HDCP_ON
	   HH_FMT_DVI_NOT_REPEATABLE
	   HH_FMT_DVI_HDCP_ON_NOT_REPEATABLE.

	   This buffer will be pre-initialized to zeroes prior to the call.

	   avi_max_length:

	   The length of the buffer pointed to by p_avif_or_dtd.

	   p_vsif:

	   A buffer into which the upstream driver should
	   write the contents of its outgoing vendor specific
	   info frame, if any, including the checksum byte.  This
	   buffer will be pre-initialized to zeroes prior to the call.

	   If the callee chooses to write an HDMI vendor specific info frame
	   into p_vsif, it shall return one of:
	   HH_FMT_HDMI_VSIF_HDMI
	   HH_FMT_HDMI_VSIF_HDMI_HDCP_ON
	   HH_FMT_HDMI_VSIF_HDMI_NOT_REPEATABLE
	   HH_FMT_HDMI_VSIF_HDMI_HDCP_ON_NOT_REPEATABLE.

	   If the callee chooses to write an MHL3 vendor specific info frame
	   into p_vsif, it shall return one of:
	   HH_FMT_HDMI_VSIF_MHL3
	   HH_FMT_HDMI_VSIF_MHL3_HDCP_ON
	   HH_FMT_HDMI_VSIF_MHL3_NOT_REPEATABLE
	   HH_FMT_HDMI_VSIF_MHL3_HDCP_ON_NOT_REPEATABLE.

	   If the callee does not write a vendor specific
	   info frame into this buffer, the callee shall return one of:
	   HH_FMT_HDMI_VSIF_NONE
	   HH_FMT_HDMI_VSIF_NONE_HDCP_ON
	   HH_FMT_HDMI_VSIF_NONE_NOT_RPT
	   HH_FMT_HDMI_VSIF_NONE_HDCP_ON_NOT_RPT
	   and the 8620 will infer the contents of the outgoing MHL3 VSIF from
	   the contents of the HDMI VSIF (if any) presented at the 8620's HDMI
	   input.

	   vsif_max_length:

	   The length of the buffer pointed to by p_vsif.

	   Return values for hpd_driven_high():

	   If the callee enabled video during the duration of this call, then
	   the callee shall return one of the values in
	   hpd_high_callback_status that do not have the sign bit set,
	   indicating the usage of parameters.

	   If the callee did not enable video during the duration of this call,
	   then the callee shall indicate specific reasons for not starting
	   video by returning the bitwise OR of the values in
	   hpd_high_callback_status that do have the sign bit set.

	 */
	enum hpd_high_callback_status(*hpd_driven_high) (void *context,
		uint8_t *p_edid, size_t edid_length,
		uint8_t *p_emsc_edid, size_t emsc_edid_length,
		struct MHL3_hev_dtd_item_t *p_hev_dtd, size_t num_hev_dtds,
		struct MHL3_hev_vic_item_t *p_hev_vic, size_t num_hev_vic_items,
		struct MHL3_3d_dtd_item_t *p_3d_dtd_items,
		size_t num_3d_dtd_items,
		struct MHL3_3d_vic_item_t *p_3d_vic, size_t num_3d_vic_items,
		union avif_or_cea_861_dtd_u *p_avif_or_dtd,
		size_t avif_or_dtd_max_length,
		union vsif_mhl3_or_hdmi_u *p_vsif,
		size_t vsif_max_length);
};

/* call this function to register the callback structure */
int si_8620_register_callbacks(struct si_mhl_callback_api_t *p_callbacks);

/* call this function to change video modes */
int si_8620_info_frame_change(enum hpd_high_callback_status status,
	union avif_or_cea_861_dtd_u *p_avif_or_dtd,
	size_t avif_or_dtd_max_length,
	union vsif_mhl3_or_hdmi_u *p_vsif,
	size_t vsif_max_length);

/* call this function to query downstream HPD status */
int si_8620_get_hpd_status(int *hpd_status);
int si_8620_get_hdcp2_status(uint32_t *hdcp2_status);
#endif
