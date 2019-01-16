
#ifndef __LZ4K_H__
#define __LZ4K_H__

#include <linux/types.h>
#include <linux/lzo.h>

#define LZ4K_TAG 1261722188	/* "LZ4K" */

#ifndef CONFIG_64BIT
#define LZ4K_MEM_COMPRESS LZO1X_MEM_COMPRESS
#else
#define LZ4K_MEM_COMPRESS (LZO1X_MEM_COMPRESS << 1)
#endif

int lz4k_compress(const unsigned char *src, size_t src_len,
		  unsigned char *dst, size_t *dst_len, void *wrkmem);

int lz4k_decompress_safe(const unsigned char *src, size_t src_len,
			 unsigned char *dst, size_t *dst_len);

#ifdef CONFIG_UBIFS_FS
int lz4k_decompress_ubifs(const unsigned char *src, size_t src_len,
			  unsigned char *dst, size_t *dst_len);
#endif				/* CONFIG_UBIFS_FS */

#endif				/* __LZ4K_H__ */
