#ifndef __MEDIA_INFO_H__
#define __MEDIA_INFO_H__

#ifndef MSM_MEDIA_ALIGN
#define MSM_MEDIA_ALIGN(__sz, __align) (((__sz) + (__align-1)) & (~(__align-1)))
#endif

enum color_fmts {
	/* Venus NV12:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved U/V plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  V           |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              V
	 * U V U V U V U V U V U V X X X X  ^
	 * U V U V U V U V U V U V X X X X  |
	 * U V U V U V U V U V U V X X X X  |
	 * U V U V U V U V U V U V X X X X  UV_Scanlines
	 * X X X X X X X X X X X X X X X X  |
	 * X X X X X X X X X X X X X X X X  V
	 * X X X X X X X X X X X X X X X X  --> Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 128
	 * UV_Stride : Width aligned to 128
	 * Y_Scanlines: Height aligned to 32
	 * UV_Scanlines: Height/2 aligned to 16
	 * Total size = align((Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines + 4096), 4096)
	 */
	COLOR_FMT_NV12,

	/* Venus NV21:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved V/U plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  V           |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              V
	 * V U V U V U V U V U V U X X X X  ^
	 * V U V U V U V U V U V U X X X X  |
	 * V U V U V U V U V U V U X X X X  |
	 * V U V U V U V U V U V U X X X X  UV_Scanlines
	 * X X X X X X X X X X X X X X X X  |
	 * X X X X X X X X X X X X X X X X  V
	 * X X X X X X X X X X X X X X X X  --> Padding & Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 128
	 * UV_Stride : Width aligned to 128
	 * Y_Scanlines: Height aligned to 32
	 * UV_Scanlines: Height/2 aligned to 16
	 * Total size = align((Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines + 4096), 4096)
	 */
	COLOR_FMT_NV21,
	/* Venus NV12_MVTB:
	 * Two YUV 4:2:0 images/views one after the other
	 * in a top-bottom layout, same as NV12
	 * with a plane of 8 bit Y samples followed
	 * by an interleaved U/V plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  ^           ^               ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  Height      |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |          Y_Scanlines      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  V           |               |
	 * X X X X X X X X X X X X X X X X              |             View_1
	 * X X X X X X X X X X X X X X X X              |               |
	 * X X X X X X X X X X X X X X X X              |               |
	 * X X X X X X X X X X X X X X X X              V               |
	 * U V U V U V U V U V U V X X X X  ^                           |
	 * U V U V U V U V U V U V X X X X  |                           |
	 * U V U V U V U V U V U V X X X X  |                           |
	 * U V U V U V U V U V U V X X X X  UV_Scanlines                |
	 * X X X X X X X X X X X X X X X X  |                           |
	 * X X X X X X X X X X X X X X X X  V                           V
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  ^           ^               ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  Height      |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |          Y_Scanlines      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |               |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  V           |               |
	 * X X X X X X X X X X X X X X X X              |             View_2
	 * X X X X X X X X X X X X X X X X              |               |
	 * X X X X X X X X X X X X X X X X              |               |
	 * X X X X X X X X X X X X X X X X              V               |
	 * U V U V U V U V U V U V X X X X  ^                           |
	 * U V U V U V U V U V U V X X X X  |                           |
	 * U V U V U V U V U V U V X X X X  |                           |
	 * U V U V U V U V U V U V X X X X  UV_Scanlines                |
	 * X X X X X X X X X X X X X X X X  |                           |
	 * X X X X X X X X X X X X X X X X  V                           V
	 * X X X X X X X X X X X X X X X X  --> Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 128
	 * UV_Stride : Width aligned to 128
	 * Y_Scanlines: Height aligned to 32
	 * UV_Scanlines: Height/2 aligned to 16
	 * View_1 begin at: 0 (zero)
	 * View_2 begin at: Y_Stride * Y_Scanlines + UV_Stride * UV_Scanlines
	 * Total size = align((2*(Y_Stride * Y_Scanlines)
	 *          + 2*(UV_Stride * UV_Scanlines) + 4096), 4096)
	 */
	COLOR_FMT_NV12_MVTB,
};

static inline unsigned int VENUS_Y_STRIDE(int color_fmt, int width)
{
	unsigned int alignment, stride = 0;
	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
		alignment = 128;
		stride = MSM_MEDIA_ALIGN(width, alignment);
		break;
	default:
		break;
	}
invalid_input:
	return stride;
}

static inline unsigned int VENUS_UV_STRIDE(int color_fmt, int width)
{
	unsigned int alignment, stride = 0;
	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
		alignment = 128;
		stride = MSM_MEDIA_ALIGN(width, alignment);
		break;
	default:
		break;
	}
invalid_input:
	return stride;
}

static inline unsigned int VENUS_Y_SCANLINES(int color_fmt, int height)
{
	unsigned int alignment, sclines = 0;
	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
		alignment = 32;
		sclines = MSM_MEDIA_ALIGN(height, alignment);
		break;
	default:
		break;
	}
invalid_input:
	return sclines;
}

static inline unsigned int VENUS_UV_SCANLINES(int color_fmt, int height)
{
	unsigned int alignment, sclines = 0;
	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
		alignment = 16;
		sclines = MSM_MEDIA_ALIGN(((height + 1) >> 1), alignment);
		break;
	default:
		break;
	}
invalid_input:
	return sclines;
}

static inline unsigned int VENUS_BUFFER_SIZE(
	int color_fmt, int width, int height)
{
	const unsigned int extra_size = 8*1024;
	unsigned int uv_alignment = 0, size = 0;
	unsigned int y_plane, uv_plane, y_stride,
		uv_stride, y_sclines, uv_sclines;
	if (!width || !height)
		goto invalid_input;

	y_stride = VENUS_Y_STRIDE(color_fmt, width);
	uv_stride = VENUS_UV_STRIDE(color_fmt, width);
	y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
	uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);
	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
		uv_alignment = 4096;
		y_plane = y_stride * y_sclines;
		uv_plane = uv_stride * uv_sclines + uv_alignment;
		size = y_plane + uv_plane + extra_size;
		size = MSM_MEDIA_ALIGN(size, 4096);
		break;
	case COLOR_FMT_NV12_MVTB:
		uv_alignment = 4096;
		y_plane = y_stride * y_sclines;
		uv_plane = uv_stride * uv_sclines + uv_alignment;
		size = y_plane + uv_plane;
		size = 2 * size + extra_size;
		size = MSM_MEDIA_ALIGN(size, 4096);
		break;
	default:
		break;
	}
invalid_input:
	return size;
}

static inline unsigned int VENUS_VIEW2_OFFSET(
	int color_fmt, int width, int height)
{
	unsigned int offset = 0;
	unsigned int y_plane, uv_plane, y_stride,
		uv_stride, y_sclines, uv_sclines;
	if (!width || !height)
		goto invalid_input;

	y_stride = VENUS_Y_STRIDE(color_fmt, width);
	uv_stride = VENUS_UV_STRIDE(color_fmt, width);
	y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
	uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);
	switch (color_fmt) {
	case COLOR_FMT_NV12_MVTB:
		y_plane = y_stride * y_sclines;
		uv_plane = uv_stride * uv_sclines;
		offset = y_plane + uv_plane;
		break;
	default:
		break;
	}
invalid_input:
	return offset;
}

#endif
