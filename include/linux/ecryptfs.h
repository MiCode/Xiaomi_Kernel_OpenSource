#ifndef _LINUX_ECRYPTFS_H
#define _LINUX_ECRYPTFS_H

struct inode;
struct page;

/* Version verification for shared data structures w/ userspace */
#define ECRYPTFS_VERSION_MAJOR 0x00
#define ECRYPTFS_VERSION_MINOR 0x04
#define ECRYPTFS_SUPPORTED_FILE_VERSION 0x03
/* These flags indicate which features are supported by the kernel
 * module; userspace tools such as the mount helper read the feature
 * bits from a sysfs handle in order to determine how to behave. */
#define ECRYPTFS_VERSIONING_PASSPHRASE            0x00000001
#define ECRYPTFS_VERSIONING_PUBKEY                0x00000002
#define ECRYPTFS_VERSIONING_PLAINTEXT_PASSTHROUGH 0x00000004
#define ECRYPTFS_VERSIONING_POLICY                0x00000008
#define ECRYPTFS_VERSIONING_XATTR                 0x00000010
#define ECRYPTFS_VERSIONING_MULTKEY               0x00000020
#define ECRYPTFS_VERSIONING_DEVMISC               0x00000040
#define ECRYPTFS_VERSIONING_HMAC                  0x00000080
#define ECRYPTFS_VERSIONING_FILENAME_ENCRYPTION   0x00000100
#define ECRYPTFS_VERSIONING_GCM                   0x00000200
#define ECRYPTFS_MAX_PASSWORD_LENGTH 64
#define ECRYPTFS_MAX_PASSPHRASE_BYTES ECRYPTFS_MAX_PASSWORD_LENGTH
#define ECRYPTFS_SALT_SIZE 8
#define ECRYPTFS_SALT_SIZE_HEX (ECRYPTFS_SALT_SIZE*2)
/* The original signature size is only for what is stored on disk; all
 * in-memory representations are expanded hex, so it better adapted to
 * be passed around or referenced on the command line */
#define ECRYPTFS_SIG_SIZE 8
#define ECRYPTFS_SIG_SIZE_HEX (ECRYPTFS_SIG_SIZE*2)
#define ECRYPTFS_PASSWORD_SIG_SIZE ECRYPTFS_SIG_SIZE_HEX
#define ECRYPTFS_MAX_KEY_BYTES 64
#define ECRYPTFS_MAX_ENCRYPTED_KEY_BYTES 512
#define ECRYPTFS_FILE_VERSION 0x03
#define ECRYPTFS_MAX_PKI_NAME_BYTES 16

#define RFC2440_CIPHER_DES3_EDE 0x02
#define RFC2440_CIPHER_CAST_5 0x03
#define RFC2440_CIPHER_BLOWFISH 0x04
#define RFC2440_CIPHER_AES_128 0x07
#define RFC2440_CIPHER_AES_192 0x08
#define RFC2440_CIPHER_AES_256 0x09
#define RFC2440_CIPHER_TWOFISH 0x0a
#define RFC2440_CIPHER_CAST_6 0x0b
#define RFC2440_CIPHER_AES_XTS_256 0x0c

#define RFC2440_CIPHER_RSA 0x01

/**
 * For convenience, we may need to pass around the encrypted session
 * key between kernel and userspace because the authentication token
 * may not be extractable.  For example, the TPM may not release the
 * private key, instead requiring the encrypted data and returning the
 * decrypted data.
 */
struct ecryptfs_session_key {
#define ECRYPTFS_USERSPACE_SHOULD_TRY_TO_DECRYPT 0x00000001
#define ECRYPTFS_USERSPACE_SHOULD_TRY_TO_ENCRYPT 0x00000002
#define ECRYPTFS_CONTAINS_DECRYPTED_KEY 0x00000004
#define ECRYPTFS_CONTAINS_ENCRYPTED_KEY 0x00000008
	u32 flags;
	u32 encrypted_key_size;
	u32 decrypted_key_size;
	u8 encrypted_key[ECRYPTFS_MAX_ENCRYPTED_KEY_BYTES];
	u8 decrypted_key[ECRYPTFS_MAX_KEY_BYTES];
};

struct ecryptfs_password {
	u32 password_bytes;
	s32 hash_algo;
	u32 hash_iterations;
	u32 session_key_encryption_key_bytes;
#define ECRYPTFS_PERSISTENT_PASSWORD 0x01
#define ECRYPTFS_SESSION_KEY_ENCRYPTION_KEY_SET 0x02
	u32 flags;
	/* Iterated-hash concatenation of salt and passphrase */
	u8 session_key_encryption_key[ECRYPTFS_MAX_KEY_BYTES];
	u8 signature[ECRYPTFS_PASSWORD_SIG_SIZE + 1];
	/* Always in expanded hex */
	u8 salt[ECRYPTFS_SALT_SIZE];
};

enum ecryptfs_token_types {ECRYPTFS_PASSWORD, ECRYPTFS_PRIVATE_KEY};

struct ecryptfs_private_key {
	u32 key_size;
	u32 data_len;
	u8 signature[ECRYPTFS_PASSWORD_SIG_SIZE + 1];
	char pki_type[ECRYPTFS_MAX_PKI_NAME_BYTES + 1];
	u8 data[];
};

/* May be a password or a private key */
struct ecryptfs_auth_tok {
	u16 version; /* 8-bit major and 8-bit minor */
	u16 token_type;
#define ECRYPTFS_ENCRYPT_ONLY 0x00000001
	u32 flags;
	struct ecryptfs_session_key session_key;
	u8 reserved[32];
	union {
		struct ecryptfs_password password;
		struct ecryptfs_private_key private_key;
	} token;
} __attribute__ ((packed));

#define ECRYPTFS_INVALID_EVENTS_HANDLE -1

/**
 * ecryptfs_events struct represents a partial interface
 * towards ecryptfs module. If registered to ecryptfs events,
 * one can receive push notifications.
 * A first callback received from ecryptfs will probably be
 * about file opening (open_cb),
 * in which ecryptfs passes its ecryptfs_data for future usage.
 * This data represents a file and must be passed in every query functions
 * such as ecryptfs_get_key_size(), ecryptfs_get_cipher() etc.
 */
struct ecryptfs_events {
	bool (*is_cipher_supported_cb)(const void *ecrytpfs_data);
	void (*open_cb)(struct inode *inode, void *ecrytpfs_data);
	void (*release_cb)(struct inode *inode);
	int (*encrypt_cb)(struct page *in_page, struct page *out_page,
		struct inode *inode, unsigned long extent_offset);
	int (*decrypt_cb)(struct page *in_page, struct page *out_page,
		struct inode *inode, unsigned long extent_offset);
	bool (*is_hw_crypt_cb)(void);
	size_t (*get_salt_key_size_cb)(const void *ecrytpfs_data);
};

#ifdef CONFIG_ECRYPT_FS
int ecryptfs_register_to_events(const struct ecryptfs_events *ops);

int ecryptfs_unregister_from_events(int user_handle);

const unsigned char *ecryptfs_get_key(const void *ecrytpfs_data);

size_t ecryptfs_get_key_size(const void *ecrytpfs_data);

const unsigned char *ecryptfs_get_salt(const void *ecrytpfs_data);

size_t ecryptfs_get_salt_size(const void *ecrytpfs_data);

bool ecryptfs_cipher_match(const void *ecrytpfs_data,
		const unsigned char *cipher, size_t cipher_size);

bool ecryptfs_is_page_in_metadata(const void *ecrytpfs_data, pgoff_t offset);

bool ecryptfs_is_data_equal(const void *ecrytpfs_data1,
		const void *ecrytpfs_data2);

#else
static inline int ecryptfs_register_to_events(
	const struct ecryptfs_events *ops)
{
	return 1; /* dummy handle */
}

static int ecryptfs_unregister_from_events(int user_handle)
{
	return 0;
}

static inline const unsigned char *ecryptfs_get_key(const void *ecrytpfs_data)
{
	return NULL;
}

static inline size_t ecryptfs_get_key_size(const void *ecrytpfs_data)
{
	return 0;
}

static inline const unsigned char *ecryptfs_get_salt(const void *ecrytpfs_data)
{
	return NULL;
}

static inline size_t ecryptfs_get_salt_size(const void *ecrytpfs_data)
{
	return 0;
}

static inline bool ecryptfs_cipher_match(const void *ecrytpfs_data,
		const unsigned char *cipher, size_t cipher_size)
{
	return false;
}

bool ecryptfs_is_page_in_metadata(const void *ecrytpfs_data, pgoff_t offset)
{
	return false;
}

bool ecryptfs_is_data_equal(const void *ecrytpfs_data1,
		const void *ecrytpfs_data2)
{
	return false;
}

#endif /* CONFIG_ECRYPT_FS */

#endif /* _LINUX_ECRYPTFS_H */
