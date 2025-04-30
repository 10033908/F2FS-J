#include <linux/f2fs_fs.h>
#include "j_recovery.h"
#include <trace/events/f2fs.h>
#include <asm/unaligned.h>

#ifdef CONFIG_UNICODE
extern struct kmem_cache *f2fs_cf_name_slab;
#endif

static struct inode *f2fs_new_inode_from_journal(struct inode *dir, j_new_inode_log_t *i_log)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(dir);
	nid_t ino;
	struct inode *inode;
	bool nid_free = false;
	bool encrypt = false;
	int xattr_size = 0;
	int err;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	f2fs_lock_op(sbi);
	if (!f2fs_alloc_nid(sbi, &ino)) {
		f2fs_unlock_op(sbi);
		err = -ENOSPC;
		goto fail;
	}
	f2fs_unlock_op(sbi);

	nid_free = true;

	//inode_init_owner(&init_user_ns, inode, dir, mode);
    inode->i_mode = i_log->i_mode;
    inode->i_uid.val  = i_log->i_uid;
    inode->i_gid.val  = i_log->i_gid;

	inode->i_ino = ino;
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	F2FS_I(inode)->i_crtime = inode->i_mtime;
	inode->i_generation = prandom_u32();

	if (S_ISDIR(inode->i_mode))
		F2FS_I(inode)->i_current_depth = 1;

    /** This func will lead ls failed...*/
	// err = insert_inode_locked(inode);
	// if (err) {
	// 	err = -EINVAL;
	// 	goto fail;
	// }

	if (f2fs_sb_has_project_quota(sbi) &&
		(F2FS_I(dir)->i_flags & F2FS_PROJINHERIT_FL))
		F2FS_I(inode)->i_projid = F2FS_I(dir)->i_projid;
	else
		F2FS_I(inode)->i_projid = make_kprojid(&init_user_ns,
							F2FS_DEF_PROJID);

	err = fscrypt_prepare_new_inode(dir, inode, &encrypt);
	if (err)
		goto fail_drop;

	err = dquot_initialize(inode);
	if (err)
		goto fail_drop;

	set_inode_flag(inode, FI_NEW_INODE);

	if (encrypt)
		f2fs_set_encrypted_inode(inode);

	if (f2fs_sb_has_extra_attr(sbi)) {
		set_inode_flag(inode, FI_EXTRA_ATTR);
		F2FS_I(inode)->i_extra_isize = F2FS_TOTAL_EXTRA_ATTR_SIZE;
	}

	if (test_opt(sbi, INLINE_XATTR))
		set_inode_flag(inode, FI_INLINE_XATTR);

	if (test_opt(sbi, INLINE_DATA) && f2fs_may_inline_data(inode))
		set_inode_flag(inode, FI_INLINE_DATA);
	if (f2fs_may_inline_dentry(inode))
		set_inode_flag(inode, FI_INLINE_DENTRY);

	if (f2fs_sb_has_flexible_inline_xattr(sbi)) {
		f2fs_bug_on(sbi, !f2fs_has_extra_attr(inode));
		if (f2fs_has_inline_xattr(inode))
			xattr_size = F2FS_OPTION(sbi).inline_xattr_size;
		/* Otherwise, will be 0 */
	} else if (f2fs_has_inline_xattr(inode) ||
				f2fs_has_inline_dentry(inode)) {
		xattr_size = DEFAULT_INLINE_XATTR_ADDRS;
	}
	F2FS_I(inode)->i_inline_xattr_size = xattr_size;

	f2fs_init_extent_tree(inode, NULL);

	stat_inc_inline_xattr(inode);
	stat_inc_inline_inode(inode);
	stat_inc_inline_dir(inode);

	F2FS_I(inode)->i_flags =
		f2fs_mask_flags(i_log->i_mode, F2FS_I(dir)->i_flags & F2FS_FL_INHERITED);

	if (S_ISDIR(inode->i_mode))
		F2FS_I(inode)->i_flags |= F2FS_INDEX_FL;

	if (F2FS_I(inode)->i_flags & F2FS_PROJINHERIT_FL)
		set_inode_flag(inode, FI_PROJ_INHERIT);

	if (f2fs_sb_has_compression(sbi)) {
		/* Inherit the compression flag in directory */
		if ((F2FS_I(dir)->i_flags & F2FS_COMPR_FL) &&
					f2fs_may_compress(inode))
			set_compress_context(inode);
	}

	f2fs_set_inode_flags(inode);

	trace_f2fs_new_inode(inode, 0);
	return inode;

fail:
	trace_f2fs_new_inode(inode, err);
	make_bad_inode(inode);
	if (nid_free)
		set_inode_flag(inode, FI_FREE_NID);
	iput(inode);
	return ERR_PTR(err);
fail_drop:
	trace_f2fs_new_inode(inode, err);
	dquot_drop(inode);
	inode->i_flags |= S_NOQUOTA;
	if (nid_free)
		set_inode_flag(inode, FI_FREE_NID);
	clear_nlink(inode);
	unlock_new_inode(inode);
	iput(inode);
	return ERR_PTR(err);
}

static int j_init_recovered_filename(const struct inode *dir,
				    j_new_inode_log_t *i_log,
				   struct f2fs_filename *fname,
				   struct qstr *usr_fname)
{
	int err;

	memset(fname, 0, sizeof(*fname));
	fname->disk_name.len = le32_to_cpu(i_log->i_namelen);
	fname->disk_name.name = i_log->i_name;

	if (WARN_ON(fname->disk_name.len > F2FS_NAME_LEN))
		return -ENAMETOOLONG;

	if (!IS_ENCRYPTED(dir)) {
		usr_fname->name = fname->disk_name.name;
		usr_fname->len = fname->disk_name.len;
		fname->usr_fname = usr_fname;
	}

	/* Compute the hash of the filename */
	if (IS_ENCRYPTED(dir) && IS_CASEFOLDED(dir)) {
		/*
		 * In this case the hash isn't computable without the key, so it
		 * was saved on-disk.
		 */
		if (fname->disk_name.len + sizeof(f2fs_hash_t) > F2FS_NAME_LEN)
			return -EINVAL;
		fname->hash = get_unaligned((f2fs_hash_t *)
				&i_log->i_name[fname->disk_name.len]);
	} else if (IS_CASEFOLDED(dir)) {
		err = f2fs_init_casefolded_name(dir, fname);
		if (err)
			return err;
		f2fs_hash_filename(dir, fname);
#ifdef CONFIG_UNICODE
		/* Case-sensitive match is fine for recovery */
		kmem_cache_free(f2fs_cf_name_slab, fname->cf_name.name);
		fname->cf_name.name = NULL;
#endif
	} else {
		f2fs_hash_filename(dir, fname);
	}
	return 0;
}

static int j_init_recovered_filename_delete(const struct inode *dir,
				    delete_log_t *delete_log,
				   struct f2fs_filename *fname,
				   struct qstr *usr_fname)
{
	int err;

	memset(fname, 0, sizeof(*fname));
	fname->disk_name.len = le32_to_cpu(delete_log->i_namelen);
	fname->disk_name.name = delete_log->file_name;

	if (WARN_ON(fname->disk_name.len > F2FS_NAME_LEN))
		return -ENAMETOOLONG;

	if (!IS_ENCRYPTED(dir)) {
		usr_fname->name = fname->disk_name.name;
		usr_fname->len = fname->disk_name.len;
		fname->usr_fname = usr_fname;
	}

	/* Compute the hash of the filename */
	if (IS_ENCRYPTED(dir) && IS_CASEFOLDED(dir)) {
		/*
		 * In this case the hash isn't computable without the key, so it
		 * was saved on-disk.
		 */
		if (fname->disk_name.len + sizeof(f2fs_hash_t) > F2FS_NAME_LEN)
			return -EINVAL;
		fname->hash = get_unaligned((f2fs_hash_t *)
				&delete_log->file_name[fname->disk_name.len]);
	} else if (IS_CASEFOLDED(dir)) {
		err = f2fs_init_casefolded_name(dir, fname);
		if (err)
			return err;
		f2fs_hash_filename(dir, fname);
#ifdef CONFIG_UNICODE
		/* Case-sensitive match is fine for recovery */
		kmem_cache_free(f2fs_cf_name_slab, fname->cf_name.name);
		fname->cf_name.name = NULL;
#endif
	} else {
		f2fs_hash_filename(dir, fname);
	}
	return 0;
}

int j_recover_new_inode(struct super_block *sb, j_new_inode_log_t * new_inode_log)
{
    int err;
    struct inode * p_dir = NULL;
    struct inode * inode   = NULL;

    struct f2fs_filename fname;
    struct qstr usr_fname;

    struct dentry *dentry = NULL;

    // get parent directory inode    
    nid_t p_ino = new_inode_log->i_pino;
    p_dir = f2fs_iget_retry(sb, p_ino);
    if (p_dir == NULL)
    {
        INFO_REPORT("get parent ino %d failed\n", p_ino);
    }

    //INFO_REPORT("get parent ino %d success\n", p_ino);

    nid_t ino = 0;
    struct f2fs_sb_info *sbi = F2FS_I_SB(p_dir);

    err = dquot_initialize(p_dir);
    if (err)
    {
        return err;
    }

    inode = f2fs_new_inode_from_journal(p_dir, new_inode_log);
    if (IS_ERR(inode)){
        return PTR_ERR(inode);
    }

    // if (!test_opt(sbi, DISABLE_EXT_IDENTIFY))
    //     set_file_temperature(sbi, inode, new_inode_log->i_name);

    // set_compress_inode(sbi, inode, new_inode_log->i_name);

    // recover directory inode
    if (S_ISDIR(inode->i_mode))
    {
        inode->i_op = &f2fs_dir_inode_operations;
	    inode->i_fop = &f2fs_dir_operations;

	    inode->i_mapping->a_ops = &f2fs_dblock_aops;
    	inode_nohighmem(inode);
        ino = inode->i_ino;
    }
    // recover file inode
    else
    {
        inode->i_op = &f2fs_file_inode_operations;
        inode->i_fop = &f2fs_file_operations;
        inode->i_mapping->a_ops = &f2fs_dblock_aops;
        ino = inode->i_ino;
    }

    //INFO_REPORT("recover, ino is %d, file is %s\n", ino, new_inode_log->i_name);

    j_init_recovered_filename(p_dir, new_inode_log, &fname, &usr_fname);

    //f2fs_lock_op(sbi);
    //err = f2fs_do_add_link(p_dir, &usr_fname, inode, ino, inode->i_mode);
    err = f2fs_add_dentry(p_dir, &fname, inode, ino, inode->i_mode);
    if (err){
        INFO_REPORT("recover add dentry fail\n");
        goto out;
    }
    //f2fs_unlock_op(sbi);

    f2fs_alloc_nid_done(sbi, ino);

    return 0;
out:
	f2fs_handle_failed_inode(inode);
	return err;

}

int j_recover_unlink(struct super_block *sb, delete_log_t *delete_log)
{
    int err;
    struct inode *p_dir = NULL;   // parent directory
    struct inode *inode = NULL; // self inode
    struct f2fs_dir_entry *de;
    struct page *page;

    struct f2fs_filename fname;
    struct qstr usr_fname;


    nid_t p_ino = delete_log->parent_ino_num;
    p_dir = f2fs_iget_retry(sb, p_ino);
    if (p_dir == NULL)
    {
        INFO_REPORT("get parent ino %d failed\n", p_ino);
    }

    struct f2fs_sb_info *sbi = F2FS_I_SB(p_dir);

    nid_t ino = delete_log->ino_num;
    inode = f2fs_iget_retry(sb, ino);
    if (inode == NULL)
    {
        INFO_REPORT("get deleted inode %d err\n", ino);
    }

    err = dquot_initialize(p_dir);
    if (err)
    {
        return err;
    }

    dquot_initialize(inode);
    if (err)
    {
        return err;
    }

    j_init_recovered_filename_delete(p_dir, delete_log, &fname, &usr_fname);

    de = f2fs_find_entry(p_dir, &usr_fname, &page);
    if (!de) {
        if (IS_ERR(page))
            err = PTR_ERR(page);
        INFO_REPORT("find de entry happens err\n");
        goto err;
    }

    f2fs_delete_entry(de, page, p_dir, inode);


    return F2FSJ_OK;

err:
    INFO_REPORT("recovery unlink log happens err\n");
    return err;

}