#ifndef _UAPI_QCEDEV__H
#define _UAPI_QCEDEV__H

#include <linux/types.h>
#include <linux/ioctl.h>
#include "fips_status.h"

#define QCEDEV_MAX_SHA_BLOCK_SIZE	64
#define QCEDEV_MAX_BEARER	31
#define QCEDEV_MAX_KEY_SIZE	64
#define QCEDEV_MAX_IV_SIZE	32

#define QCEDEV_MAX_BUFFERS      16
#define QCEDEV_MAX_SHA_DIGEST	32

#define QCEDEV_USE_PMEM		1
#define QCEDEV_NO_PMEM		0

#define QCEDEV_AES_KEY_128	16
#define QCEDEV_AES_KEY_192	24
#define QCEDEV_AES_KEY_256	32
/**
*qcedev_oper_enum: Operation types
* @QCEDEV_OPER_ENC:		Encrypt
* @QCEDEV_OPER_DEC:		Decrypt
* @QCEDEV_OPER_ENC_NO_KEY:	Encrypt. Do not need key to be specified by
*				user. Key already set by an external processor.
* @QCEDEV_OPER_DEC_NO_KEY:	Decrypt. Do not need the key to be specified by
*				user. Key already set by an external processor.
*/
enum qcedev_oper_enum {
	QCEDEV_OPER_DEC		= 0,
	QCEDEV_OPER_ENC		= 1,
	QCEDEV_OPER_DEC_NO_KEY	= 2,
	QCEDEV_OPER_ENC_NO_KEY	= 3,
	QCEDEV_OPER_LAST
};

/**
*qcedev_oper_enum: Cipher algorithm types
* @QCEDEV_ALG_DES:		DES
* @QCEDEV_ALG_3DES:		3DES
* @QCEDEV_ALG_AES:		AES
*/
enum qcedev_cipher_alg_enum {
	QCEDEV_ALG_DES		= 0,
	QCEDEV_ALG_3DES		= 1,
	QCEDEV_ALG_AES		= 2,
	QCEDEV_ALG_LAST
};

/**
*qcedev_cipher_mode_enum : AES mode
* @QCEDEV_AES_MODE_CBC:		CBC
* @QCEDEV_AES_MODE_ECB:		ECB
* @QCEDEV_AES_MODE_CTR:		CTR
* @QCEDEV_AES_MODE_XTS:		XTS
* @QCEDEV_AES_MODE_CCM:		CCM
* @QCEDEV_DES_MODE_CBC:		CBC
* @QCEDEV_DES_MODE_ECB:		ECB
*/
enum qcedev_cipher_mode_enum {
	QCEDEV_AES_MODE_CBC	= 0,
	QCEDEV_AES_MODE_ECB	= 1,
	QCEDEV_AES_MODE_CTR	= 2,
	QCEDEV_AES_MODE_XTS	= 3,
	QCEDEV_AES_MODE_CCM	= 4,
	QCEDEV_DES_MODE_CBC	= 5,
	QCEDEV_DES_MODE_ECB	= 6,
	QCEDEV_AES_DES_MODE_LAST
};

/**
*enum qcedev_sha_alg_enum : Secure Hashing Algorithm
* @QCEDEV_ALG_SHA1:		Digest returned: 20 bytes (160 bits)
* @QCEDEV_ALG_SHA256:		Digest returned: 32 bytes (256 bit)
* @QCEDEV_ALG_SHA1_HMAC:	HMAC returned 20 bytes (160 bits)
* @QCEDEV_ALG_SHA256_HMAC:	HMAC returned 32 bytes (256 bit)
* @QCEDEV_ALG_AES_CMAC:		Configurable MAC size
*/
enum qcedev_sha_alg_enum {
	QCEDEV_ALG_SHA1		= 0,
	QCEDEV_ALG_SHA256	= 1,
	QCEDEV_ALG_SHA1_HMAC	= 2,
	QCEDEV_ALG_SHA256_HMAC	= 3,
	QCEDEV_ALG_AES_CMAC	= 4,
	QCEDEV_ALG_SHA_ALG_LAST
};

/**
* struct buf_info - Buffer information
* @offset:			Offset from the base address of the buffer
*				(Used when buffer is allocated using PMEM)
* @vaddr:			Virtual buffer address pointer
* @len:				Size of the buffer
*/
struct	buf_info {
	union {
		uint32_t	offset;
		uint8_t		*vaddr;
	};
	uint32_t	len;
};

/**
* struct qcedev_vbuf_info - Source and destination Buffer information
* @src:				Array of buf_info for input/source
* @dst:				Array of buf_info for output/destination
*/
struct	qcedev_vbuf_info {
	struct buf_info	src[QCEDEV_MAX_BUFFERS];
	struct buf_info	dst[QCEDEV_MAX_BUFFERS];
};

/**
* struct qcedev_pmem_info - Stores PMEM buffer information
* @fd_src:			Handle to /dev/adsp_pmem used to allocate
*				memory for input/src buffer
* @src:				Array of buf_info for input/source
* @fd_dst:			Handle to /dev/adsp_pmem used to allocate
*				memory for output/dst buffer
* @dst:				Array of buf_info for output/destination
* @pmem_src_offset:		The offset from input/src buffer
*				(allocated by PMEM)
*/
struct	qcedev_pmem_info {
	int		fd_src;
	struct buf_info	src[QCEDEV_MAX_BUFFERS];
	int		fd_dst;
	struct buf_info	dst[QCEDEV_MAX_BUFFERS];
};

/**
* struct qcedev_cipher_op_req - Holds the ciphering request information
* @use_pmem (IN):	Flag to indicate if buffer source is PMEM
*			QCEDEV_USE_PMEM/QCEDEV_NO_PMEM
* @pmem (IN):		Stores PMEM buffer information.
*			Refer struct qcedev_pmem_info
* @vbuf (IN/OUT):	Stores Source and destination Buffer information
*			Refer to struct qcedev_vbuf_info
* @data_len (IN):	Total Length of input/src and output/dst in bytes
* @in_place_op (IN):	Indicates whether the operation is inplace where
*			source == destination
*			When using PMEM allocated memory, must set this to 1
* @enckey (IN):		128 bits of confidentiality key
*			enckey[0] bit 127-120, enckey[1] bit 119-112,..
*			enckey[15] bit 7-0
* @encklen (IN):	Length of the encryption key(set to 128  bits/16
*			bytes in the driver)
* @iv (IN/OUT):		Initialisation vector data
*			This is updated by the driver, incremented by
*			number of blocks encrypted/decrypted.
* @ivlen (IN):		Length of the IV
* @byteoffset (IN):	Offset in the Cipher BLOCK (applicable and to be set
*			for AES-128 CTR mode only)
* @alg (IN):		Type of ciphering algorithm: AES/DES/3DES
* @mode (IN):		Mode use when using AES algorithm: ECB/CBC/CTR
*			Apllicabel when using AES algorithm only
* @op (IN):		Type of operation: QCEDEV_OPER_DEC/QCEDEV_OPER_ENC or
*			QCEDEV_OPER_ENC_NO_KEY/QCEDEV_OPER_DEC_NO_KEY
*
*If use_pmem is set to 0, the driver assumes that memory was not allocated
* via PMEM, and kernel will need to allocate memory and copy data from user
* space buffer (data_src/dta_dst) and process accordingly and copy data back
* to the user space buffer
*
* If use_pmem is set to 1, the driver assumes that memory was allocated via
* PMEM.
* The kernel driver will use the fd_src to determine the kernel virtual address
* base that maps to the user space virtual address base for the  buffer
* allocated in user space.
* The final input/src and output/dst buffer pointer will be determined
* by adding the offsets to the kernel virtual addr.
*
* If use of hardware key is supported in the target, user can configure the
* key parameters (encklen, enckey) to use the hardware key.
* In order to use the hardware key, set encklen to 0 and set the enckey
* data array to 0.
*/
struct	qcedev_cipher_op_req {
	uint8_t				use_pmem;
	union {
		struct qcedev_pmem_info	pmem;
		struct qcedev_vbuf_info	vbuf;
	};
	uint32_t			entries;
	uint32_t			data_len;
	uint8_t				in_place_op;
	uint8_t				enckey[QCEDEV_MAX_KEY_SIZE];
	uint32_t			encklen;
	uint8_t				iv[QCEDEV_MAX_IV_SIZE];
	uint32_t			ivlen;
	uint32_t			byteoffset;
	enum qcedev_cipher_alg_enum	alg;
	enum qcedev_cipher_mode_enum	mode;
	enum qcedev_oper_enum		op;
};

/**
* struct qcedev_sha_op_req - Holds the hashing request information
* @data (IN):			Array of pointers to the data to be hashed
* @entries (IN):		Number of buf_info entries in the data array
* @data_len (IN):		Length of data to be hashed
* @digest (IN/OUT):		Returns the hashed data information
* @diglen (OUT):		Size of the hashed/digest data
* @authkey (IN):		Pointer to authentication key for HMAC
* @authklen (IN):		Size of the authentication key
* @alg (IN):			Secure Hash algorithm
*/
struct	qcedev_sha_op_req {
	struct buf_info			data[QCEDEV_MAX_BUFFERS];
	uint32_t			entries;
	uint32_t			data_len;
	uint8_t				digest[QCEDEV_MAX_SHA_DIGEST];
	uint32_t			diglen;
	uint8_t				*authkey;
	uint32_t			authklen;
	enum qcedev_sha_alg_enum	alg;
};

/**
* struct qfips_verify_t - Holds data for FIPS Integrity test
* @kernel_size  (IN):		Size of kernel Image
* @kernel       (IN):		pointer to buffer containing the kernel Image
*/
struct qfips_verify_t {
	unsigned int kernel_size;
	void *kernel;
};

/**
 * struct qcedev_map_buf_req - Holds the mapping request information
 * fd (IN):            Array of fds.
 * num_fds (IN):       Number of fds in fd[].
 * fd_size (IN):       Array of sizes corresponding to each fd in fd[].
 * fd_offset (IN):     Array of offset corresponding to each fd in fd[].
 * vaddr (OUT):        Array of mapped virtual address corresponding to
 *			each fd in fd[].
 */
struct qcedev_map_buf_req {
	int32_t         fd[QCEDEV_MAX_BUFFERS];
	uint32_t        num_fds;
	uint32_t        fd_size[QCEDEV_MAX_BUFFERS];
	uint32_t        fd_offset[QCEDEV_MAX_BUFFERS];
	uint64_t        buf_vaddr[QCEDEV_MAX_BUFFERS];
};

/**
 * struct qcedev_unmap_buf_req - Holds the hashing request information
 * fd (IN):            Array of fds to unmap
 * num_fds (IN):       Number of fds in fd[].
 */
struct  qcedev_unmap_buf_req {
	int32_t         fd[QCEDEV_MAX_BUFFERS];
	uint32_t        num_fds;
};

struct file;

#define QCEDEV_IOC_MAGIC	0x87

#define QCEDEV_IOCTL_ENC_REQ		\
	_IOWR(QCEDEV_IOC_MAGIC, 1, struct qcedev_cipher_op_req)
#define QCEDEV_IOCTL_DEC_REQ		\
	_IOWR(QCEDEV_IOC_MAGIC, 2, struct qcedev_cipher_op_req)
#define QCEDEV_IOCTL_SHA_INIT_REQ	\
	_IOWR(QCEDEV_IOC_MAGIC, 3, struct qcedev_sha_op_req)
#define QCEDEV_IOCTL_SHA_UPDATE_REQ	\
	_IOWR(QCEDEV_IOC_MAGIC, 4, struct qcedev_sha_op_req)
#define QCEDEV_IOCTL_SHA_FINAL_REQ	\
	_IOWR(QCEDEV_IOC_MAGIC, 5, struct qcedev_sha_op_req)
#define QCEDEV_IOCTL_GET_SHA_REQ	\
	_IOWR(QCEDEV_IOC_MAGIC, 6, struct qcedev_sha_op_req)
#define QCEDEV_IOCTL_LOCK_CE	\
	_IO(QCEDEV_IOC_MAGIC, 7)
#define QCEDEV_IOCTL_UNLOCK_CE	\
	_IO(QCEDEV_IOC_MAGIC, 8)
#define QCEDEV_IOCTL_GET_CMAC_REQ	\
	_IOWR(QCEDEV_IOC_MAGIC, 9, struct qcedev_sha_op_req)
#define QCEDEV_IOCTL_MAP_BUF_REQ	\
	_IOWR(QCEDEV_IOC_MAGIC, 10, struct qcedev_map_buf_req)
#define QCEDEV_IOCTL_UNMAP_BUF_REQ	\
	_IOWR(QCEDEV_IOC_MAGIC, 11, struct qcedev_unmap_buf_req)
#endif /* _UAPI_QCEDEV__H */
