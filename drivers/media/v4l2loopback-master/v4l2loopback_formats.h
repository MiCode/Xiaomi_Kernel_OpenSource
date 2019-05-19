
/* here come the packed formats */
{
	.name = "32 bpp RGB, le",
	.fourcc = V4L2_PIX_FMT_BGR32,
	.depth = 32,
	.flags = 0,
},
{
	.name = "32 bpp RGB, be",
	.fourcc = V4L2_PIX_FMT_RGB32,
	.depth = 32,
	.flags = 0,
},
{
	.name = "24 bpp RGB, le",
	.fourcc = V4L2_PIX_FMT_BGR24,
	.depth = 24,
	.flags = 0,
},
{
	.name = "24 bpp RGB, be",
	.fourcc = V4L2_PIX_FMT_RGB24,
	.depth = 24,
	.flags = 0,
},
#ifdef V4L2_PIX_FMT_RGB332
{
	.name = "8 bpp RGB-3-3-2",
	.fourcc = V4L2_PIX_FMT_RGB332,
	.depth = 8,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_RGB332 */
#ifdef V4L2_PIX_FMT_RGB444
{
	.name = "16 bpp RGB (xxxxrrrr ggggbbbb)",
	.fourcc = V4L2_PIX_FMT_RGB444,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_RGB444 */
#ifdef V4L2_PIX_FMT_RGB555
{
	.name = "16 bpp RGB-5-5-5",
	.fourcc = V4L2_PIX_FMT_RGB555,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_RGB555 */
#ifdef V4L2_PIX_FMT_RGB565
{
	.name = "16 bpp  RGB-5-6-5",
	.fourcc = V4L2_PIX_FMT_RGB565,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_RGB565 */
#ifdef V4L2_PIX_FMT_RGB555X
{
	.name = "16 bpp RGB-5-5-5 BE",
	.fourcc = V4L2_PIX_FMT_RGB555X,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_RGB555X */
#ifdef V4L2_PIX_FMT_RGB565X
{
	.name = "16 bpp RGB-5-6-5 BE",
	.fourcc = V4L2_PIX_FMT_RGB565X,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_RGB565X */
#ifdef V4L2_PIX_FMT_BGR666
{
	.name = "18 bpp BGR-6-6-6",
	.fourcc = V4L2_PIX_FMT_BGR666,
	.depth = 18,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_BGR666 */
{
	.name = "4:2:2, packed, YUYV",
	.fourcc = V4L2_PIX_FMT_YUYV,
	.depth = 16,
	.flags = 0,
},
{
	.name = "4:2:2, packed, UYVY",
	.fourcc = V4L2_PIX_FMT_UYVY,
	.depth = 16,
	.flags = 0,
},
#ifdef V4L2_PIX_FMT_YVYU
{
	.name = "4:2:2, packed YVYU",
	.fourcc = V4L2_PIX_FMT_YVYU,
	.depth = 16,
	.flags = 0,
},
#endif
#ifdef V4L2_PIX_FMT_VYUY
{
	.name = "4:2:2, packed VYUY",
	.fourcc = V4L2_PIX_FMT_VYUY,
	.depth = 16,
	.flags = 0,
},
#endif
{
	.name = "4:2:2, packed YYUV",
	.fourcc = V4L2_PIX_FMT_YYUV,
	.depth = 16,
	.flags = 0,
},
{
	.name = "YUV-8-8-8-8",
	.fourcc = V4L2_PIX_FMT_YUV32,
	.depth = 32,
	.flags = 0,
},
{
	.name = "8 bpp, Greyscale",
	.fourcc = V4L2_PIX_FMT_GREY,
	.depth = 8,
	.flags = 0,
},
#ifdef V4L2_PIX_FMT_Y4
{
	.name = "4 bpp Greyscale",
	.fourcc = V4L2_PIX_FMT_Y4,
	.depth = 4,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_Y4 */
#ifdef V4L2_PIX_FMT_Y6
{
	.name = "6 bpp Greyscale",
	.fourcc = V4L2_PIX_FMT_Y6,
	.depth = 6,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_Y6 */
#ifdef V4L2_PIX_FMT_Y10
{
	.name = "10 bpp Greyscale",
	.fourcc = V4L2_PIX_FMT_Y10,
	.depth = 10,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_Y10 */
#ifdef V4L2_PIX_FMT_Y12
{
	.name = "12 bpp Greyscale",
	.fourcc = V4L2_PIX_FMT_Y12,
	.depth = 12,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_Y12 */
{
	.name = "16 bpp, Greyscale",
	.fourcc = V4L2_PIX_FMT_Y16,
	.depth = 16,
	.flags = 0,
},
#ifdef V4L2_PIX_FMT_YUV444
{
	.name = "16 bpp xxxxyyyy uuuuvvvv",
	.fourcc = V4L2_PIX_FMT_YUV444,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_YUV444 */
#ifdef V4L2_PIX_FMT_YUV555
{
	.name = "16 bpp YUV-5-5-5",
	.fourcc = V4L2_PIX_FMT_YUV555,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_YUV555 */
#ifdef V4L2_PIX_FMT_YUV565
{
	.name = "16 bpp YUV-5-6-5",
	.fourcc = V4L2_PIX_FMT_YUV565,
	.depth = 16,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_YUV565 */

	/* bayer formats */
#ifdef V4L2_PIX_FMT_SRGGB8
{
	.name = "Bayer RGGB 8bit",
	.fourcc = V4L2_PIX_FMT_SRGGB8,
	.depth = 8,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_SRGGB8 */
#ifdef V4L2_PIX_FMT_SGRBG8
{
	.name = "Bayer GRBG 8bit",
	.fourcc = V4L2_PIX_FMT_SGRBG8,
	.depth = 8,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_SGRBG8 */
#ifdef V4L2_PIX_FMT_SGBRG8
{
	.name = "Bayer GBRG 8bit",
	.fourcc = V4L2_PIX_FMT_SGBRG8,
	.depth = 8,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_SGBRG8 */
#ifdef V4L2_PIX_FMT_SBGGR8
{
	.name = "Bayer BA81 8bit",
	.fourcc = V4L2_PIX_FMT_SBGGR8,
	.depth = 8,
	.flags = 0,
},
#endif /* V4L2_PIX_FMT_SBGGR8 */



	/* here come the planar formats */
{
	.name = "4:1:0, planar, Y-Cr-Cb",
	.fourcc = V4L2_PIX_FMT_YVU410,
	.depth = 9,
	.flags = FORMAT_FLAGS_PLANAR,
},
{
	.name = "4:2:0, planar, Y-Cr-Cb",
	.fourcc = V4L2_PIX_FMT_YVU420,
	.depth = 12,
	.flags = FORMAT_FLAGS_PLANAR,
},
{
	.name = "4:1:0, planar, Y-Cb-Cr",
	.fourcc = V4L2_PIX_FMT_YUV410,
	.depth = 9,
	.flags = FORMAT_FLAGS_PLANAR,
},
{
	.name = "4:2:0, planar, Y-Cb-Cr",
	.fourcc = V4L2_PIX_FMT_YUV420,
	.depth = 12,
	.flags = FORMAT_FLAGS_PLANAR,
},
#ifdef V4L2_PIX_FMT_YUV422P
{
	.name = "16 bpp YVU422 planar",
	.fourcc = V4L2_PIX_FMT_YUV422P,
	.depth = 16,
	.flags = FORMAT_FLAGS_PLANAR,
},
#endif /* V4L2_PIX_FMT_YUV422P */
#ifdef V4L2_PIX_FMT_YUV411P
{
	.name = "16 bpp YVU411 planar",
	.fourcc = V4L2_PIX_FMT_YUV411P,
	.depth = 16,
	.flags = FORMAT_FLAGS_PLANAR,
},
#endif /* V4L2_PIX_FMT_YUV411P */
#ifdef V4L2_PIX_FMT_Y41P
{
	.name = "12 bpp YUV 4:1:1",
	.fourcc = V4L2_PIX_FMT_Y41P,
	.depth = 12,
	.flags = FORMAT_FLAGS_PLANAR,
},
#endif /* V4L2_PIX_FMT_Y41P */
#ifdef V4L2_PIX_FMT_NV12
{
	.name = "12 bpp Y/CbCr 4:2:0 ",
	.fourcc = V4L2_PIX_FMT_NV12,
	.depth = 12,
	.flags = FORMAT_FLAGS_PLANAR,
},
#endif /* V4L2_PIX_FMT_NV12 */

	/* here come the compressed formats */

#ifdef V4L2_PIX_FMT_MJPEG
{
	.name = "Motion-JPEG",
	.fourcc = V4L2_PIX_FMT_MJPEG,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_MJPEG */
#ifdef V4L2_PIX_FMT_JPEG
{
	.name = "JFIF JPEG",
	.fourcc = V4L2_PIX_FMT_JPEG,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_JPEG */
#ifdef V4L2_PIX_FMT_DV
{
	.name = "DV1394",
	.fourcc = V4L2_PIX_FMT_DV,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_DV */
#ifdef V4L2_PIX_FMT_MPEG
{
	.name = "MPEG-1/2/4 Multiplexed",
	.fourcc = V4L2_PIX_FMT_MPEG,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_MPEG */
#ifdef V4L2_PIX_FMT_H264
{
	.name = "H264 with start codes",
	.fourcc = V4L2_PIX_FMT_H264,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_H264 */
#ifdef V4L2_PIX_FMT_H264_NO_SC
{
	.name = "H264 without start codes",
	.fourcc = V4L2_PIX_FMT_H264_NO_SC,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_H264_NO_SC */
#ifdef V4L2_PIX_FMT_H264_MVC
{
	.name = "H264 MVC",
	.fourcc = V4L2_PIX_FMT_H264_MVC,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_H264_MVC */
#ifdef V4L2_PIX_FMT_H263
{
	.name = "H263",
	.fourcc = V4L2_PIX_FMT_H263,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_H263 */
#ifdef V4L2_PIX_FMT_MPEG1
{
	.name = "MPEG-1 ES",
	.fourcc = V4L2_PIX_FMT_MPEG1,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_MPEG1 */
#ifdef V4L2_PIX_FMT_MPEG2
{
	.name = "MPEG-2 ES",
	.fourcc = V4L2_PIX_FMT_MPEG2,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_MPEG2 */
#ifdef V4L2_PIX_FMT_MPEG4
{
	.name = "MPEG-4 part 2 ES",
	.fourcc = V4L2_PIX_FMT_MPEG4,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_MPEG4 */
#ifdef V4L2_PIX_FMT_XVID
{
	.name = "Xvid",
	.fourcc = V4L2_PIX_FMT_XVID,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_XVID */
#ifdef V4L2_PIX_FMT_VC1_ANNEX_G
{
	.name = "SMPTE 421M Annex G compliant stream",
	.fourcc = V4L2_PIX_FMT_VC1_ANNEX_G,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_VC1_ANNEX_G */
#ifdef V4L2_PIX_FMT_VC1_ANNEX_L
{
	.name = "SMPTE 421M Annex L compliant stream",
	.fourcc = V4L2_PIX_FMT_VC1_ANNEX_L,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_VC1_ANNEX_L */
#ifdef V4L2_PIX_FMT_VP8
{
	.name = "VP8",
	.fourcc = V4L2_PIX_FMT_VP8,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
#endif /* V4L2_PIX_FMT_VP8 */
{
	.name = "VP9",
	.fourcc = V4L2_PIX_FMT_VP9,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
{
	.name = "HEVC",
	.fourcc = V4L2_PIX_FMT_HEVC,
	.depth = 32,
	.flags = FORMAT_FLAGS_COMPRESSED,
},
