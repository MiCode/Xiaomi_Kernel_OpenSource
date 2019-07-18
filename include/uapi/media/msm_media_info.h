/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __MSM_MEDIA_INFO_H__
#define __MSM_MEDIA_INFO_H__

/* Width and Height should be multiple of 16 */
#define INTERLACE_WIDTH_MAX 1920
#define INTERLACE_HEIGHT_MAX 1920
#define INTERLACE_MB_PER_FRAME_MAX ((1920*1088)/256)

#ifndef MSM_MEDIA_ALIGN
#define MSM_MEDIA_ALIGN(__sz, __align) (((__align) & ((__align) - 1)) ?\
	((((__sz) + (__align) - 1) / (__align)) * (__align)) :\
	(((__sz) + (__align) - 1) & (~((__align) - 1))))
#endif

#ifndef MSM_MEDIA_ROUNDUP
#define MSM_MEDIA_ROUNDUP(__sz, __r) (((__sz) + ((__r) - 1)) / (__r))
#endif

enum color_fmts {
	/* Venus NV12:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved U/V plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              V
	 * U V U V U V U V U V U V . . . .  ^
	 * U V U V U V U V U V U V . . . .  |
	 * U V U V U V U V U V U V . . . .  |
	 * U V U V U V U V U V U V . . . .  UV_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  --> Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 512
	 * UV_Stride : Width aligned to 512
	 * Y_Scanlines: Height aligned to 512
	 * UV_Scanlines: Height/2 aligned to 256
	 * Total size = align(Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines, 4096)
	 */
	COLOR_FMT_NV12,
	/* Venus NV21:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved V/U plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              V
	 * V U V U V U V U V U V U . . . .  ^
	 * V U V U V U V U V U V U . . . .  |
	 * V U V U V U V U V U V U . . . .  |
	 * V U V U V U V U V U V U . . . .  UV_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  --> Padding & Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 512
	 * UV_Stride : Width aligned to 512
	 * Y_Scanlines: Height aligned to 512
	 * UV_Scanlines: Height/2 aligned to 256
	 * Total size = align(Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines, 4096)
	 */
	COLOR_FMT_NV21,
	/*
	 * The buffer can be of 2 types:
	 * (1) Venus NV12 UBWC Progressive
	 * (2) Venus NV12 UBWC Interlaced
	 *
	 * (1) Venus NV12 UBWC Progressive Buffer Format:
	 * Compressed Macro-tile format for NV12.
	 * Contains 4 planes in the following order -
	 * (A) Y_Meta_Plane
	 * (B) Y_UBWC_Plane
	 * (C) UV_Meta_Plane
	 * (D) UV_UBWC_Plane
	 *
	 * Y_Meta_Plane consists of meta information to decode compressed
	 * tile data in Y_UBWC_Plane.
	 * Y_UBWC_Plane consists of Y data in compressed macro-tile format.
	 * UBWC decoder block will use the Y_Meta_Plane data together with
	 * Y_UBWC_Plane data to produce loss-less uncompressed 8 bit Y samples.
	 *
	 * UV_Meta_Plane consists of meta information to decode compressed
	 * tile data in UV_UBWC_Plane.
	 * UV_UBWC_Plane consists of UV data in compressed macro-tile format.
	 * UBWC decoder block will use UV_Meta_Plane data together with
	 * UV_UBWC_Plane data to produce loss-less uncompressed 8 bit 2x2
	 * subsampled color difference samples.
	 *
	 * Each tile in Y_UBWC_Plane/UV_UBWC_Plane is independently decodable
	 * and randomly accessible. There is no dependency between tiles.
	 *
	 * <----- Y_Meta_Stride ---->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      Height      |
	 * M M M M M M M M M M M M . .      |         Meta_Y_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <--Compressed tile Y Stride--->
	 * <------- Width ------->
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  ^           ^
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  Height      |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |        Macro_tile_Y_Scanlines
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 * <----- UV_Meta_Stride ---->
	 * M M M M M M M M M M M M . .      ^
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      M_UV_Scanlines
	 * . . . . . . . . . . . . . .      |
	 * . . . . . . . . . . . . . .      V
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * <--Compressed tile UV Stride--->
	 * U* V* U* V* U* V* U* V* . . . .  ^
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  UV_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 *
	 * Y_Stride = align(Width, 128)
	 * UV_Stride = align(Width, 128)
	 * Y_Scanlines = align(Height, 32)
	 * UV_Scanlines = align(Height/2, 32)
	 * Y_UBWC_Plane_size = align(Y_Stride * Y_Scanlines, 4096)
	 * UV_UBWC_Plane_size = align(UV_Stride * UV_Scanlines, 4096)
	 * Y_Meta_Stride = align(roundup(Width, Y_TileWidth), 64)
	 * Y_Meta_Scanlines = align(roundup(Height, Y_TileHeight), 16)
	 * Y_Meta_Plane_size = align(Y_Meta_Stride * Y_Meta_Scanlines, 4096)
	 * UV_Meta_Stride = align(roundup(Width, UV_TileWidth), 64)
	 * UV_Meta_Scanlines = align(roundup(Height, UV_TileHeight), 16)
	 * UV_Meta_Plane_size = align(UV_Meta_Stride * UV_Meta_Scanlines, 4096)
	 *
	 * Total size = align( Y_UBWC_Plane_size + UV_UBWC_Plane_size +
	 *           Y_Meta_Plane_size + UV_Meta_Plane_size, 4096)
	 *
	 *
	 * (2) Venus NV12 UBWC Interlaced Buffer Format:
	 * Compressed Macro-tile format for NV12 interlaced.
	 * Contains 8 planes in the following order -
	 * (A) Y_Meta_Top_Field_Plane
	 * (B) Y_UBWC_Top_Field_Plane
	 * (C) UV_Meta_Top_Field_Plane
	 * (D) UV_UBWC_Top_Field_Plane
	 * (E) Y_Meta_Bottom_Field_Plane
	 * (F) Y_UBWC_Bottom_Field_Plane
	 * (G) UV_Meta_Bottom_Field_Plane
	 * (H) UV_UBWC_Bottom_Field_Plane
	 * Y_Meta_Top_Field_Plane consists of meta information to decode
	 * compressed tile data for Y_UBWC_Top_Field_Plane.
	 * Y_UBWC_Top_Field_Plane consists of Y data in compressed macro-tile
	 * format for top field of an interlaced frame.
	 * UBWC decoder block will use the Y_Meta_Top_Field_Plane data together
	 * with Y_UBWC_Top_Field_Plane data to produce loss-less uncompressed
	 * 8 bit Y samples for top field of an interlaced frame.
	 *
	 * UV_Meta_Top_Field_Plane consists of meta information to decode
	 * compressed tile data in UV_UBWC_Top_Field_Plane.
	 * UV_UBWC_Top_Field_Plane consists of UV data in compressed macro-tile
	 * format for top field of an interlaced frame.
	 * UBWC decoder block will use UV_Meta_Top_Field_Plane data together
	 * with UV_UBWC_Top_Field_Plane data to produce loss-less uncompressed
	 * 8 bit subsampled color difference samples for top field of an
	 * interlaced frame.
	 *
	 * Each tile in Y_UBWC_Top_Field_Plane/UV_UBWC_Top_Field_Plane is
	 * independently decodable and randomly accessible. There is no
	 * dependency between tiles.
	 *
	 * Y_Meta_Bottom_Field_Plane consists of meta information to decode
	 * compressed tile data for Y_UBWC_Bottom_Field_Plane.
	 * Y_UBWC_Bottom_Field_Plane consists of Y data in compressed macro-tile
	 * format for bottom field of an interlaced frame.
	 * UBWC decoder block will use the Y_Meta_Bottom_Field_Plane data
	 * together with Y_UBWC_Bottom_Field_Plane data to produce loss-less
	 * uncompressed 8 bit Y samples for bottom field of an interlaced frame.
	 *
	 * UV_Meta_Bottom_Field_Plane consists of meta information to decode
	 * compressed tile data in UV_UBWC_Bottom_Field_Plane.
	 * UV_UBWC_Bottom_Field_Plane consists of UV data in compressed
	 * macro-tile format for bottom field of an interlaced frame.
	 * UBWC decoder block will use UV_Meta_Bottom_Field_Plane data together
	 * with UV_UBWC_Bottom_Field_Plane data to produce loss-less
	 * uncompressed 8 bit subsampled color difference samples for bottom
	 * field of an interlaced frame.
	 *
	 * Each tile in Y_UBWC_Bottom_Field_Plane/UV_UBWC_Bottom_Field_Plane is
	 * independently decodable and randomly accessible. There is no
	 * dependency between tiles.
	 *
	 * <-----Y_TF_Meta_Stride---->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . . Half_height      |
	 * M M M M M M M M M M M M . .      |         Meta_Y_TF_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <-Compressed tile Y_TF Stride->
	 * <------- Width ------->
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  ^           ^
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . . Half_height  |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |        Macro_tile_Y_TF_Scanlines
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 * <----UV_TF_Meta_Stride---->
	 * M M M M M M M M M M M M . .      ^
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      M_UV_TF_Scanlines
	 * . . . . . . . . . . . . . .      |
	 * . . . . . . . . . . . . . .      V
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * <-Compressed tile UV_TF Stride->
	 * U* V* U* V* U* V* U* V* . . . .  ^
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  UV_TF_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 * <-----Y_BF_Meta_Stride---->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . . Half_height      |
	 * M M M M M M M M M M M M . .      |         Meta_Y_BF_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <-Compressed tile Y_BF Stride->
	 * <------- Width ------->
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  ^           ^
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . . Half_height  |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |        Macro_tile_Y_BF_Scanlines
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 * <----UV_BF_Meta_Stride---->
	 * M M M M M M M M M M M M . .      ^
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      M_UV_BF_Scanlines
	 * . . . . . . . . . . . . . .      |
	 * . . . . . . . . . . . . . .      V
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * <-Compressed tile UV_BF Stride->
	 * U* V* U* V* U* V* U* V* . . . .  ^
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  UV_BF_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 *
	 * Half_height = (Height+1)>>1
	 * Y_TF_Stride = align(Width, 128)
	 * UV_TF_Stride = align(Width, 128)
	 * Y_TF_Scanlines = align(Half_height, 32)
	 * UV_TF_Scanlines = align((Half_height+1)/2, 32)
	 * Y_UBWC_TF_Plane_size = align(Y_TF_Stride * Y_TF_Scanlines, 4096)
	 * UV_UBWC_TF_Plane_size = align(UV_TF_Stride * UV_TF_Scanlines, 4096)
	 * Y_TF_Meta_Stride = align(roundup(Width, Y_TileWidth), 64)
	 * Y_TF_Meta_Scanlines = align(roundup(Half_height, Y_TileHeight), 16)
	 * Y_TF_Meta_Plane_size =
	 *     align(Y_TF_Meta_Stride * Y_TF_Meta_Scanlines, 4096)
	 * UV_TF_Meta_Stride = align(roundup(Width, UV_TileWidth), 64)
	 * UV_TF_Meta_Scanlines = align(roundup(Half_height, UV_TileHeight), 16)
	 * UV_TF_Meta_Plane_size =
	 *     align(UV_TF_Meta_Stride * UV_TF_Meta_Scanlines, 4096)
	 * Y_BF_Stride = align(Width, 128)
	 * UV_BF_Stride = align(Width, 128)
	 * Y_BF_Scanlines = align(Half_height, 32)
	 * UV_BF_Scanlines = align((Half_height+1)/2, 32)
	 * Y_UBWC_BF_Plane_size = align(Y_BF_Stride * Y_BF_Scanlines, 4096)
	 * UV_UBWC_BF_Plane_size = align(UV_BF_Stride * UV_BF_Scanlines, 4096)
	 * Y_BF_Meta_Stride = align(roundup(Width, Y_TileWidth), 64)
	 * Y_BF_Meta_Scanlines = align(roundup(Half_height, Y_TileHeight), 16)
	 * Y_BF_Meta_Plane_size =
	 *     align(Y_BF_Meta_Stride * Y_BF_Meta_Scanlines, 4096)
	 * UV_BF_Meta_Stride = align(roundup(Width, UV_TileWidth), 64)
	 * UV_BF_Meta_Scanlines = align(roundup(Half_height, UV_TileHeight), 16)
	 * UV_BF_Meta_Plane_size =
	 *     align(UV_BF_Meta_Stride * UV_BF_Meta_Scanlines, 4096)
	 *
	 * Total size = align( Y_UBWC_TF_Plane_size + UV_UBWC_TF_Plane_size +
	 *           Y_TF_Meta_Plane_size + UV_TF_Meta_Plane_size +
	 *			 Y_UBWC_BF_Plane_size + UV_UBWC_BF_Plane_size +
	 *           Y_BF_Meta_Plane_size + UV_BF_Meta_Plane_size +, 4096)
	 */
	COLOR_FMT_NV12_UBWC,
	/* Venus NV12 10-bit UBWC:
	 * Compressed Macro-tile format for NV12.
	 * Contains 4 planes in the following order -
	 * (A) Y_Meta_Plane
	 * (B) Y_UBWC_Plane
	 * (C) UV_Meta_Plane
	 * (D) UV_UBWC_Plane
	 *
	 * Y_Meta_Plane consists of meta information to decode compressed
	 * tile data in Y_UBWC_Plane.
	 * Y_UBWC_Plane consists of Y data in compressed macro-tile format.
	 * UBWC decoder block will use the Y_Meta_Plane data together with
	 * Y_UBWC_Plane data to produce loss-less uncompressed 10 bit Y samples.
	 *
	 * UV_Meta_Plane consists of meta information to decode compressed
	 * tile data in UV_UBWC_Plane.
	 * UV_UBWC_Plane consists of UV data in compressed macro-tile format.
	 * UBWC decoder block will use UV_Meta_Plane data together with
	 * UV_UBWC_Plane data to produce loss-less uncompressed 10 bit 2x2
	 * subsampled color difference samples.
	 *
	 * Each tile in Y_UBWC_Plane/UV_UBWC_Plane is independently decodable
	 * and randomly accessible. There is no dependency between tiles.
	 *
	 * <----- Y_Meta_Stride ----->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      Height      |
	 * M M M M M M M M M M M M . .      |         Meta_Y_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <--Compressed tile Y Stride--->
	 * <------- Width ------->
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  ^           ^
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  Height      |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |        Macro_tile_Y_Scanlines
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 * <----- UV_Meta_Stride ---->
	 * M M M M M M M M M M M M . .      ^
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      M_UV_Scanlines
	 * . . . . . . . . . . . . . .      |
	 * . . . . . . . . . . . . . .      V
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * <--Compressed tile UV Stride--->
	 * U* V* U* V* U* V* U* V* . . . .  ^
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  UV_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 *
	 *
	 * Y_Stride = align(Width * 4/3, 256)
	 * UV_Stride = align(Width * 4/3, 256)
	 * Y_Scanlines = align(Height, 32)
	 * UV_Scanlines = align(Height/2, 16)
	 * Y_UBWC_Plane_Size = align(Y_Stride * Y_Scanlines, 4096)
	 * UV_UBWC_Plane_Size = align(UV_Stride * UV_Scanlines, 4096)
	 * Y_Meta_Stride = align(roundup(Width, Y_TileWidth), 64)
	 * Y_Meta_Scanlines = align(roundup(Height, Y_TileHeight), 16)
	 * Y_Meta_Plane_size = align(Y_Meta_Stride * Y_Meta_Scanlines, 4096)
	 * UV_Meta_Stride = align(roundup(Width, UV_TileWidth), 64)
	 * UV_Meta_Scanlines = align(roundup(Height, UV_TileHeight), 16)
	 * UV_Meta_Plane_size = align(UV_Meta_Stride * UV_Meta_Scanlines, 4096)
	 *
	 * Total size = align(Y_UBWC_Plane_size + UV_UBWC_Plane_size +
	 *           Y_Meta_Plane_size + UV_Meta_Plane_size, 4096)
	 */
	COLOR_FMT_NV12_BPP10_UBWC,
	/* Venus RGBA8888 format:
	 * Contains 1 plane in the following order -
	 * (A) RGBA plane
	 *
	 * <-------- RGB_Stride -------->
	 * <------- Width ------->
	 * R R R R R R R R R R R R . . . .  ^           ^
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  Height      |
	 * R R R R R R R R R R R R . . . .  |       RGB_Scanlines
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              V
	 *
	 * RGB_Stride = align(Width * 4, 128)
	 * RGB_Scanlines = align(Height, 32)
	 * RGB_Plane_size = align(RGB_Stride * RGB_Scanlines, 4096)
	 *
	 * Total size = align(RGB_Plane_size , 4096)
	 */
	COLOR_FMT_RGBA8888,
	/* Venus RGBA8888 UBWC format:
	 * Contains 2 planes in the following order -
	 * (A) Meta plane
	 * (B) RGBA plane
	 *
	 * <--- RGB_Meta_Stride ---->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      Height      |
	 * M M M M M M M M M M M M . .      |       Meta_RGB_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <-------- RGB_Stride -------->
	 * <------- Width ------->
	 * R R R R R R R R R R R R . . . .  ^           ^
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  Height      |
	 * R R R R R R R R R R R R . . . .  |       RGB_Scanlines
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .    -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 *
	 * RGB_Stride = align(Width * 4, 256)
	 * RGB_Scanlines = align(Height, 16)
	 * RGB_Plane_size = align(RGB_Stride * RGB_Scanlines, 4096)
	 * RGB_Meta_Stride = align(roundup(Width, RGB_TileWidth), 64)
	 * RGB_Meta_Scanline = align(roundup(Height, RGB_TileHeight), 16)
	 * RGB_Meta_Plane_size = align(RGB_Meta_Stride *
	 *		RGB_Meta_Scanlines, 4096)
	 *
	 * Total size = align(RGB_Meta_Plane_size + RGB_Plane_size, 4096)
	 */
	COLOR_FMT_RGBA8888_UBWC,
	/* Venus RGBA1010102 UBWC format:
	 * Contains 2 planes in the following order -
	 * (A) Meta plane
	 * (B) RGBA plane
	 *
	 * <--- RGB_Meta_Stride ---->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      Height      |
	 * M M M M M M M M M M M M . .      |       Meta_RGB_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <-------- RGB_Stride -------->
	 * <------- Width ------->
	 * R R R R R R R R R R R R . . . .  ^           ^
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  Height      |
	 * R R R R R R R R R R R R . . . .  |       RGB_Scanlines
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .    -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 *
	 * RGB_Stride = align(Width * 4, 256)
	 * RGB_Scanlines = align(Height, 16)
	 * RGB_Plane_size = align(RGB_Stride * RGB_Scanlines, 4096)
	 * RGB_Meta_Stride = align(roundup(Width, RGB_TileWidth), 64)
	 * RGB_Meta_Scanline = align(roundup(Height, RGB_TileHeight), 16)
	 * RGB_Meta_Plane_size = align(RGB_Meta_Stride *
	 *		RGB_Meta_Scanlines, 4096)
	 *
	 * Total size = align(RGB_Meta_Plane_size + RGB_Plane_size, 4096)
	 */
	COLOR_FMT_RGBA1010102_UBWC,
	/* Venus RGB565 UBWC format:
	 * Contains 2 planes in the following order -
	 * (A) Meta plane
	 * (B) RGB plane
	 *
	 * <--- RGB_Meta_Stride ---->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      Height      |
	 * M M M M M M M M M M M M . .      |       Meta_RGB_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <-------- RGB_Stride -------->
	 * <------- Width ------->
	 * R R R R R R R R R R R R . . . .  ^           ^
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  Height      |
	 * R R R R R R R R R R R R . . . .  |       RGB_Scanlines
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  |           |
	 * R R R R R R R R R R R R . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .    -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 *
	 * RGB_Stride = align(Width * 2, 256)
	 * RGB_Scanlines = align(Height, 16)
	 * RGB_Plane_size = align(RGB_Stride * RGB_Scanlines, 4096)
	 * RGB_Meta_Stride = align(roundup(Width, RGB_TileWidth), 64)
	 * RGB_Meta_Scanline = align(roundup(Height, RGB_TileHeight), 16)
	 * RGB_Meta_Plane_size = align(RGB_Meta_Stride *
	 *		RGB_Meta_Scanlines, 4096)
	 *
	 * Total size = align(RGB_Meta_Plane_size + RGB_Plane_size, 4096)
	 */
	COLOR_FMT_RGB565_UBWC,
	/* P010 UBWC:
	 * Compressed Macro-tile format for NV12.
	 * Contains 4 planes in the following order -
	 * (A) Y_Meta_Plane
	 * (B) Y_UBWC_Plane
	 * (C) UV_Meta_Plane
	 * (D) UV_UBWC_Plane
	 *
	 * Y_Meta_Plane consists of meta information to decode compressed
	 * tile data in Y_UBWC_Plane.
	 * Y_UBWC_Plane consists of Y data in compressed macro-tile format.
	 * UBWC decoder block will use the Y_Meta_Plane data together with
	 * Y_UBWC_Plane data to produce loss-less uncompressed 10 bit Y samples.
	 *
	 * UV_Meta_Plane consists of meta information to decode compressed
	 * tile data in UV_UBWC_Plane.
	 * UV_UBWC_Plane consists of UV data in compressed macro-tile format.
	 * UBWC decoder block will use UV_Meta_Plane data together with
	 * UV_UBWC_Plane data to produce loss-less uncompressed 10 bit 2x2
	 * subsampled color difference samples.
	 *
	 * Each tile in Y_UBWC_Plane/UV_UBWC_Plane is independently decodable
	 * and randomly accessible. There is no dependency between tiles.
	 *
	 * <----- Y_Meta_Stride ----->
	 * <-------- Width ------>
	 * M M M M M M M M M M M M . .      ^           ^
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      Height      |
	 * M M M M M M M M M M M M . .      |         Meta_Y_Scanlines
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      |           |
	 * M M M M M M M M M M M M . .      V           |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .                  |
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . .                  V
	 * <--Compressed tile Y Stride--->
	 * <------- Width ------->
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  ^           ^
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  Height      |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |        Macro_tile_Y_Scanlines
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  |           |
	 * Y* Y* Y* Y* Y* Y* Y* Y* . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 * . . . . . . . . . . . . . . . .              V
	 * <----- UV_Meta_Stride ---->
	 * M M M M M M M M M M M M . .      ^
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      |
	 * M M M M M M M M M M M M . .      M_UV_Scanlines
	 * . . . . . . . . . . . . . .      |
	 * . . . . . . . . . . . . . .      V
	 * . . . . . . . . . . . . . .      -------> Buffer size aligned to 4k
	 * <--Compressed tile UV Stride--->
	 * U* V* U* V* U* V* U* V* . . . .  ^
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  |
	 * U* V* U* V* U* V* U* V* . . . .  UV_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  -------> Buffer size aligned to 4k
	 *
	 *
	 * Y_Stride = align(Width * 2, 256)
	 * UV_Stride = align(Width * 2, 256)
	 * Y_Scanlines = align(Height, 16)
	 * UV_Scanlines = align(Height/2, 16)
	 * Y_UBWC_Plane_Size = align(Y_Stride * Y_Scanlines, 4096)
	 * UV_UBWC_Plane_Size = align(UV_Stride * UV_Scanlines, 4096)
	 * Y_Meta_Stride = align(roundup(Width, Y_TileWidth), 64)
	 * Y_Meta_Scanlines = align(roundup(Height, Y_TileHeight), 16)
	 * Y_Meta_Plane_size = align(Y_Meta_Stride * Y_Meta_Scanlines, 4096)
	 * UV_Meta_Stride = align(roundup(Width, UV_TileWidth), 64)
	 * UV_Meta_Scanlines = align(roundup(Height, UV_TileHeight), 16)
	 * UV_Meta_Plane_size = align(UV_Meta_Stride * UV_Meta_Scanlines, 4096)
	 *
	 * Total size = align(Y_UBWC_Plane_size + UV_UBWC_Plane_size +
	 *           Y_Meta_Plane_size + UV_Meta_Plane_size, 4096)
	 */
	COLOR_FMT_P010_UBWC,
	/* Venus P010:
	 * YUV 4:2:0 image with a plane of 10 bit Y samples followed
	 * by an interleaved U/V plane containing 10 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              V
	 * U V U V U V U V U V U V . . . .  ^
	 * U V U V U V U V U V U V . . . .  |
	 * U V U V U V U V U V U V . . . .  |
	 * U V U V U V U V U V U V . . . .  UV_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  --> Buffer size alignment
	 *
	 * Y_Stride : Width * 2 aligned to 256
	 * UV_Stride : Width * 2 aligned to 256
	 * Y_Scanlines: Height aligned to 32
	 * UV_Scanlines: Height/2 aligned to 16
	 * Total size = align(Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines, 4096)
	 */
	COLOR_FMT_P010,
	/* Venus NV12_512:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved U/V plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y . . . .  V           |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              |
	 * . . . . . . . . . . . . . . . .              V
	 * U V U V U V U V U V U V . . . .  ^
	 * U V U V U V U V U V U V . . . .  |
	 * U V U V U V U V U V U V . . . .  |
	 * U V U V U V U V U V U V . . . .  UV_Scanlines
	 * . . . . . . . . . . . . . . . .  |
	 * . . . . . . . . . . . . . . . .  V
	 * . . . . . . . . . . . . . . . .  --> Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 512
	 * UV_Stride : Width aligned to 512
	 * Y_Scanlines: Height aligned to 512
	 * UV_Scanlines: Height/2 aligned to 256
	 * Total size = align((Y_Stride * Y_Scanlines
	 *          + UV_Stride  * UV_Scanlines), 4096)
	 */
	COLOR_FMT_NV12_512,
};

/*
 * Function arguments:
 * @color_fmt
 * @width
 * Progressive: width
 * Interlaced: width
 */
static inline unsigned int VENUS_Y_STRIDE(unsigned int color_fmt,
	unsigned int width)
{
	unsigned int alignment, stride = 0;

	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12_512:
		alignment = 512;
		stride = MSM_MEDIA_ALIGN(width, alignment);
		break;
	case COLOR_FMT_NV12_UBWC:
		alignment = 128;
		stride = MSM_MEDIA_ALIGN(width, alignment);
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		alignment = 256;
		stride = MSM_MEDIA_ALIGN(width, 192);
		stride = MSM_MEDIA_ALIGN(stride * 4/3, alignment);
		break;
	case COLOR_FMT_P010_UBWC:
	case COLOR_FMT_P010:
		alignment = 256;
		stride = MSM_MEDIA_ALIGN(width * 2, alignment);
		break;
	default:
		break;
	}
invalid_input:
	return stride;
}

/*
 * Function arguments:
 * @color_fmt
 * @width
 * Progressive: width
 * Interlaced: width
 */
static inline unsigned int VENUS_UV_STRIDE(unsigned int color_fmt,
	unsigned int width)
{
	unsigned int alignment, stride = 0;

	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_512:
		alignment = 512;
		stride = MSM_MEDIA_ALIGN(width, alignment);
		break;
	case COLOR_FMT_NV12_UBWC:
		alignment = 128;
		stride = MSM_MEDIA_ALIGN(width, alignment);
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		alignment = 256;
		stride = MSM_MEDIA_ALIGN(width, 192);
		stride = MSM_MEDIA_ALIGN(stride * 4/3, alignment);
		break;
	case COLOR_FMT_P010_UBWC:
	case COLOR_FMT_P010:
		alignment = 256;
		stride = MSM_MEDIA_ALIGN(width * 2, alignment);
		break;
	default:
		break;
	}
invalid_input:
	return stride;
}

/*
 * Function arguments:
 * @color_fmt
 * @height
 * Progressive: height
 * Interlaced: (height+1)>>1
 */
static inline unsigned int VENUS_Y_SCANLINES(unsigned int color_fmt,
	unsigned int height)
{
	unsigned int alignment, sclines = 0;

	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12_512:
		alignment = 512;
		break;
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_P010:
		alignment = 32;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
		alignment = 16;
		break;
	default:
		return 0;
	}
	sclines = MSM_MEDIA_ALIGN(height, alignment);
invalid_input:
	return sclines;
}

/*
 * Function arguments:
 * @color_fmt
 * @height
 * Progressive: height
 * Interlaced: (height+1)>>1
 */
static inline unsigned int VENUS_UV_SCANLINES(unsigned int color_fmt,
	unsigned int height)
{
	unsigned int alignment, sclines = 0;

	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_512:
		alignment = 256;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
	case COLOR_FMT_P010:
		alignment = 16;
		break;
	case COLOR_FMT_NV12_UBWC:
		alignment = 32;
		break;
	default:
		goto invalid_input;
	}

	sclines = MSM_MEDIA_ALIGN((height+1)>>1, alignment);

invalid_input:
	return sclines;
}

/*
 * Function arguments:
 * @color_fmt
 * @width
 * Progressive: width
 * Interlaced: width
 */
static inline unsigned int VENUS_Y_META_STRIDE(unsigned int color_fmt,
	unsigned int width)
{
	int y_tile_width = 0, y_meta_stride = 0;

	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_P010_UBWC:
		y_tile_width = 32;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		y_tile_width = 48;
		break;
	default:
		goto invalid_input;
	}

	y_meta_stride = MSM_MEDIA_ROUNDUP(width, y_tile_width);
	y_meta_stride = MSM_MEDIA_ALIGN(y_meta_stride, 64);

invalid_input:
	return y_meta_stride;
}

/*
 * Function arguments:
 * @color_fmt
 * @height
 * Progressive: height
 * Interlaced: (height+1)>>1
 */
static inline unsigned int VENUS_Y_META_SCANLINES(unsigned int color_fmt,
	unsigned int height)
{
	int y_tile_height = 0, y_meta_scanlines = 0;

	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
		y_tile_height = 8;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
		y_tile_height = 4;
		break;
	default:
		goto invalid_input;
	}

	y_meta_scanlines = MSM_MEDIA_ROUNDUP(height, y_tile_height);
	y_meta_scanlines = MSM_MEDIA_ALIGN(y_meta_scanlines, 16);

invalid_input:
	return y_meta_scanlines;
}

/*
 * Function arguments:
 * @color_fmt
 * @width
 * Progressive: width
 * Interlaced: width
 */
static inline unsigned int VENUS_UV_META_STRIDE(unsigned int color_fmt,
	unsigned int width)
{
	int uv_tile_width = 0, uv_meta_stride = 0;

	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_P010_UBWC:
		uv_tile_width = 16;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		uv_tile_width = 24;
		break;
	default:
		goto invalid_input;
	}

	uv_meta_stride = MSM_MEDIA_ROUNDUP((width+1)>>1, uv_tile_width);
	uv_meta_stride = MSM_MEDIA_ALIGN(uv_meta_stride, 64);

invalid_input:
	return uv_meta_stride;
}

/*
 * Function arguments:
 * @color_fmt
 * @height
 * Progressive: height
 * Interlaced: (height+1)>>1
 */
static inline unsigned int VENUS_UV_META_SCANLINES(unsigned int color_fmt,
	unsigned int height)
{
	int uv_tile_height = 0, uv_meta_scanlines = 0;

	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
		uv_tile_height = 8;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
		uv_tile_height = 4;
		break;
	default:
		goto invalid_input;
	}

	uv_meta_scanlines = MSM_MEDIA_ROUNDUP((height+1)>>1, uv_tile_height);
	uv_meta_scanlines = MSM_MEDIA_ALIGN(uv_meta_scanlines, 16);

invalid_input:
	return uv_meta_scanlines;
}

static inline unsigned int VENUS_RGB_STRIDE(unsigned int color_fmt,
	unsigned int width)
{
	unsigned int alignment = 0, stride = 0, bpp = 4;

	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_RGBA8888:
		alignment = 128;
		break;
	case COLOR_FMT_RGB565_UBWC:
		alignment = 256;
		bpp = 2;
		break;
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
		alignment = 256;
		break;
	default:
		goto invalid_input;
	}

	stride = MSM_MEDIA_ALIGN(width * bpp, alignment);

invalid_input:
	return stride;
}

static inline unsigned int VENUS_RGB_SCANLINES(unsigned int color_fmt,
	unsigned int height)
{
	unsigned int alignment = 0, scanlines = 0;

	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_RGBA8888:
		alignment = 32;
		break;
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
	case COLOR_FMT_RGB565_UBWC:
		alignment = 16;
		break;
	default:
		goto invalid_input;
	}

	scanlines = MSM_MEDIA_ALIGN(height, alignment);

invalid_input:
	return scanlines;
}

static inline unsigned int VENUS_RGB_META_STRIDE(unsigned int color_fmt,
	unsigned int width)
{
	int rgb_tile_width = 0, rgb_meta_stride = 0;

	if (!width)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
	case COLOR_FMT_RGB565_UBWC:
		rgb_tile_width = 16;
		break;
	default:
		goto invalid_input;
	}

	rgb_meta_stride = MSM_MEDIA_ROUNDUP(width, rgb_tile_width);
	rgb_meta_stride = MSM_MEDIA_ALIGN(rgb_meta_stride, 64);

invalid_input:
	return rgb_meta_stride;
}

static inline unsigned int VENUS_RGB_META_SCANLINES(unsigned int color_fmt,
	unsigned int height)
{
	int rgb_tile_height = 0, rgb_meta_scanlines = 0;

	if (!height)
		goto invalid_input;

	switch (color_fmt) {
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
	case COLOR_FMT_RGB565_UBWC:
		rgb_tile_height = 4;
		break;
	default:
		goto invalid_input;
	}

	rgb_meta_scanlines = MSM_MEDIA_ROUNDUP(height, rgb_tile_height);
	rgb_meta_scanlines = MSM_MEDIA_ALIGN(rgb_meta_scanlines, 16);

invalid_input:
	return rgb_meta_scanlines;
}

/*
 * Function arguments:
 * @color_fmt
 * @width
 * Progressive: width
 * Interlaced: width
 * @height
 * Progressive: height
 * Interlaced: height
 */
static inline unsigned int VENUS_BUFFER_SIZE(unsigned int color_fmt,
	unsigned int width, unsigned int height)
{
	unsigned int size = 0;
	unsigned int y_plane, uv_plane, y_stride,
		uv_stride, y_sclines, uv_sclines;
	unsigned int y_ubwc_plane = 0, uv_ubwc_plane = 0;
	unsigned int y_meta_stride = 0, y_meta_scanlines = 0;
	unsigned int uv_meta_stride = 0, uv_meta_scanlines = 0;
	unsigned int y_meta_plane = 0, uv_meta_plane = 0;
	unsigned int rgb_stride = 0, rgb_scanlines = 0;
	unsigned int rgb_plane = 0, rgb_ubwc_plane = 0, rgb_meta_plane = 0;
	unsigned int rgb_meta_stride = 0, rgb_meta_scanlines = 0;

	if (!width || !height)
		goto invalid_input;

	y_stride = VENUS_Y_STRIDE(color_fmt, width);
	uv_stride = VENUS_UV_STRIDE(color_fmt, width);
	y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
	uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);
	rgb_stride = VENUS_RGB_STRIDE(color_fmt, width);
	rgb_scanlines = VENUS_RGB_SCANLINES(color_fmt, height);

	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_P010:
	case COLOR_FMT_NV12_512:
		y_plane = y_stride * y_sclines;
		uv_plane = uv_stride * uv_sclines;
		size = y_plane + uv_plane;
		break;
	case COLOR_FMT_NV12_UBWC:
		y_meta_stride = VENUS_Y_META_STRIDE(color_fmt, width);
		uv_meta_stride = VENUS_UV_META_STRIDE(color_fmt, width);
		if (width <= INTERLACE_WIDTH_MAX &&
			height <= INTERLACE_HEIGHT_MAX &&
			(height * width) / 256 <= INTERLACE_MB_PER_FRAME_MAX) {
			y_sclines =
				VENUS_Y_SCANLINES(color_fmt, (height+1)>>1);
			y_ubwc_plane =
				MSM_MEDIA_ALIGN(y_stride * y_sclines, 4096);
			uv_sclines =
				VENUS_UV_SCANLINES(color_fmt, (height+1)>>1);
			uv_ubwc_plane =
				MSM_MEDIA_ALIGN(uv_stride * uv_sclines, 4096);
			y_meta_scanlines =
			VENUS_Y_META_SCANLINES(color_fmt, (height+1)>>1);
			y_meta_plane = MSM_MEDIA_ALIGN(
				y_meta_stride * y_meta_scanlines, 4096);
			uv_meta_scanlines =
			VENUS_UV_META_SCANLINES(color_fmt, (height+1)>>1);
			uv_meta_plane = MSM_MEDIA_ALIGN(uv_meta_stride *
				uv_meta_scanlines, 4096);
			size = (y_ubwc_plane + uv_ubwc_plane + y_meta_plane +
				uv_meta_plane)*2;
		} else {
			y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
			y_ubwc_plane =
				MSM_MEDIA_ALIGN(y_stride * y_sclines, 4096);
			uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);
			uv_ubwc_plane =
				MSM_MEDIA_ALIGN(uv_stride * uv_sclines, 4096);
			y_meta_scanlines =
				VENUS_Y_META_SCANLINES(color_fmt, height);
			y_meta_plane = MSM_MEDIA_ALIGN(
				y_meta_stride * y_meta_scanlines, 4096);
			uv_meta_scanlines =
				VENUS_UV_META_SCANLINES(color_fmt, height);
			uv_meta_plane = MSM_MEDIA_ALIGN(uv_meta_stride *
				uv_meta_scanlines, 4096);
			size = (y_ubwc_plane + uv_ubwc_plane + y_meta_plane +
				uv_meta_plane);
		}
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		y_ubwc_plane = MSM_MEDIA_ALIGN(y_stride * y_sclines, 4096);
		uv_ubwc_plane = MSM_MEDIA_ALIGN(uv_stride * uv_sclines, 4096);
		y_meta_stride = VENUS_Y_META_STRIDE(color_fmt, width);
		y_meta_scanlines = VENUS_Y_META_SCANLINES(color_fmt, height);
		y_meta_plane = MSM_MEDIA_ALIGN(
				y_meta_stride * y_meta_scanlines, 4096);
		uv_meta_stride = VENUS_UV_META_STRIDE(color_fmt, width);
		uv_meta_scanlines = VENUS_UV_META_SCANLINES(color_fmt, height);
		uv_meta_plane = MSM_MEDIA_ALIGN(uv_meta_stride *
					uv_meta_scanlines, 4096);

		size = y_ubwc_plane + uv_ubwc_plane + y_meta_plane +
			uv_meta_plane;
		break;
	case COLOR_FMT_P010_UBWC:
		y_ubwc_plane = MSM_MEDIA_ALIGN(y_stride * y_sclines, 4096);
		uv_ubwc_plane = MSM_MEDIA_ALIGN(uv_stride * uv_sclines, 4096);
		y_meta_stride = VENUS_Y_META_STRIDE(color_fmt, width);
		y_meta_scanlines = VENUS_Y_META_SCANLINES(color_fmt, height);
		y_meta_plane = MSM_MEDIA_ALIGN(
				y_meta_stride * y_meta_scanlines, 4096);
		uv_meta_stride = VENUS_UV_META_STRIDE(color_fmt, width);
		uv_meta_scanlines = VENUS_UV_META_SCANLINES(color_fmt, height);
		uv_meta_plane = MSM_MEDIA_ALIGN(uv_meta_stride *
					uv_meta_scanlines, 4096);

		size = y_ubwc_plane + uv_ubwc_plane + y_meta_plane +
			uv_meta_plane;
		break;
	case COLOR_FMT_RGBA8888:
		rgb_plane = MSM_MEDIA_ALIGN(rgb_stride  * rgb_scanlines, 4096);
		size = rgb_plane;
		break;
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
	case COLOR_FMT_RGB565_UBWC:
		rgb_ubwc_plane = MSM_MEDIA_ALIGN(rgb_stride * rgb_scanlines,
							4096);
		rgb_meta_stride = VENUS_RGB_META_STRIDE(color_fmt, width);
		rgb_meta_scanlines = VENUS_RGB_META_SCANLINES(color_fmt,
					height);
		rgb_meta_plane = MSM_MEDIA_ALIGN(rgb_meta_stride *
					rgb_meta_scanlines, 4096);
		size = rgb_ubwc_plane + rgb_meta_plane;
		break;
	default:
		break;
	}
invalid_input:
	return MSM_MEDIA_ALIGN(size, 4096);
}

static inline unsigned int VENUS_BUFFER_SIZE_USED(unsigned int color_fmt,
	unsigned int width, unsigned int height, unsigned int interlace)
{
	unsigned int size = 0;
	unsigned int y_stride, uv_stride, y_sclines, uv_sclines;
	unsigned int y_ubwc_plane = 0, uv_ubwc_plane = 0;
	unsigned int y_meta_stride = 0, y_meta_scanlines = 0;
	unsigned int uv_meta_stride = 0, uv_meta_scanlines = 0;
	unsigned int y_meta_plane = 0, uv_meta_plane = 0;

	if (!width || !height)
		goto invalid_input;

	if (!interlace && color_fmt == COLOR_FMT_NV12_UBWC) {
		y_stride = VENUS_Y_STRIDE(color_fmt, width);
		uv_stride = VENUS_UV_STRIDE(color_fmt, width);
		y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
		y_ubwc_plane = MSM_MEDIA_ALIGN(y_stride * y_sclines, 4096);
		uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);
		uv_ubwc_plane = MSM_MEDIA_ALIGN(uv_stride * uv_sclines, 4096);
		y_meta_stride = VENUS_Y_META_STRIDE(color_fmt, width);
		y_meta_scanlines =
			VENUS_Y_META_SCANLINES(color_fmt, height);
		y_meta_plane = MSM_MEDIA_ALIGN(
			y_meta_stride * y_meta_scanlines, 4096);
		uv_meta_stride = VENUS_UV_META_STRIDE(color_fmt, width);
		uv_meta_scanlines =
			VENUS_UV_META_SCANLINES(color_fmt, height);
		uv_meta_plane = MSM_MEDIA_ALIGN(uv_meta_stride *
			uv_meta_scanlines, 4096);
		size = (y_ubwc_plane + uv_ubwc_plane + y_meta_plane +
			uv_meta_plane);
		size = MSM_MEDIA_ALIGN(size, 4096);
	} else {
		size = VENUS_BUFFER_SIZE(color_fmt, width, height);
	}
invalid_input:
	return size;
}

#endif
