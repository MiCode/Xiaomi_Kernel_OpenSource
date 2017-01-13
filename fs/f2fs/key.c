/* fill fi->keyring_key */
int f2fs_validate_key(struct inode *inode)
{
	u8 full_key_descriptor[FS_KEY_DESC_PREFIX_SIZE +
				(FS_KEY_DESCRIPTOR_SIZE * 2) + 1];
	struct key *keyring_key = NULL;
	u8 key[F2FS_SET_KEY_SIZE];
	int ret;

	ret = f2fs_getxattr(inode, F2FS_XATTR_INDEX_KEY,
				F2FS_XATTR_NAME_ENCRYPTION_CONTEXT,
				key, F2FS_SET_KEY_SIZE, NULL);
	if (ret)
		return ret;

	memcpy(full_key_descriptor, F2FS_KEY_DESC_PREFIX,
					F2FS_KEY_DESC_PREFIX_SIZE);
	sprintf(full_key_descriptor + F2FS_KEY_DESC_PREFIX_SIZE,
					"%*phN", F2FS_KEY_DESCRIPTOR_SIZE, key);
	full_key_descriptor[F2FS_KEY_DESC_PREFIX_SIZE +
					(2 * F2FS_KEY_DESCRIPTOR_SIZE)] = '\0';
	keyring_key = request_key(&key_type_logon, full_key_descriptor, NULL);
	if (IS_ERR(keyring_key))
		return PTR_ERR(keyring_key);

	if (keyring_key->type != &key_type_logon) {
		printk_once(KERN_WARNING
				"%s: key type must be logon\n", __func__);
		key_put(keyring_key);
		return -ENOKEY;
	}
	key_put(keyring_key);
	return 0;
}
