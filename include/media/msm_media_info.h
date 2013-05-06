#ifndef __MEDIA_INFO_H__
#define __MEDIA_INFO_H__

#ifndef MSM_MEDIA_ALIGN
#define MSM_MEDIA_ALIGN(__sz, __align) (((__sz) + (__align-1)) & (~(__align-1)))
#endif

enum color_fmts {
	COLOR_FMT_NV12,
	COLOR_FMT_NV21,
};

static inline unsigned int VENUS_Y_STRIDE(int color_fmt, int width)
{
	unsigned int alignment, stride = 0;
	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
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
	unsigned int uv_alignment;
	unsigned int size = 0;
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
		size = y_plane + uv_plane;
		size = MSM_MEDIA_ALIGN(size, 4096);
		break;
	default:
		break;
	}
invalid_input:
	return size;
}

#endif
