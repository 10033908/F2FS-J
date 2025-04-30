/**
 * @file j_log_operate.c
 * @author leslie.cui (10033908@github.com)
 * @brief This file contains some log list operations, such as insert, iterate...
 * @version 0.1
 * @date 2023-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <linux/pagemap.h>
#include "j_log_operate.h"
#include "j_epoch.h"
#include "node.h"
#include "segment.h"

int is_already_checkin_g_epoch(struct f2fs_inode_info *f2fs_i, uint8_t * ep_no, struct list_head ** g_ep_head)
{
    uint8_t g_active_ep_no = NONE_EPOCH;
    struct list_head * g_cur_active_epoch = NULL;

    uint8_t is_checkin = 0;
    uint8_t local_log_list_idx = NONE_EPOCH;

    get_global_epoch(&g_active_ep_no, &g_cur_active_epoch);
    * ep_no = g_active_ep_no;
    * g_ep_head = g_cur_active_epoch;
    if (!g_cur_active_epoch || g_active_ep_no >= NONE_EPOCH)
    {
        STATUS_LOG(STATUS_ERROR, "get global active epoch error\n");
        is_checkin = 0;
    }
    else
    {
        local_log_list_idx = f2fs_i->g2l_ep_map[g_active_ep_no];

        ///< Please notice that g_ep_no is from 0 - 7,
        ///< if >= NONE_EPOCH, it represents that this inode is not been checkin g_epoch
        if (local_log_list_idx >= NONE_EPOCH)
        {
            is_checkin = 0;
        }
        if (local_log_list_idx >= 0 && local_log_list_idx < NONE_EPOCH)
        {
            is_checkin = 1;
        }
    }

    return is_checkin;
}

int get_idle_local_epoch(struct f2fs_inode_info *f2fs_i)
{
    uint8_t i = 0;
    for (i = 0; i < MAX_GLOBAL_EP_NUM; i++)
    {
        if (f2fs_i->j_ino_log_list[i].log_list_status == LOG_LIST_IDLE)
        {
            break;
        }
    }

    if (i == MAX_GLOBAL_EP_NUM)
    {
        STATUS_LOG(STATUS_ERROR, "Every local epoch of ino[%d] is not idle, need to wait checkpoint finish\n");

        /** Find again*/
        for (i = 0; i < MAX_GLOBAL_EP_NUM; i++)
        {
            if (f2fs_i->j_ino_log_list[i].log_list_status == LOG_LIST_IDLE)
            {
                break;
            }
        }
    }

    return i;
}

int ino_checkin_global_epoch(struct f2fs_inode_info *f2fs_i)
{
    uint8_t g_active_ep_no = MAX_GLOBAL_EP_NUM;
    uint8_t idle_local_ep_no = MAX_GLOBAL_EP_NUM;
    struct list_head * g_cur_active_epoch = NULL;

    ///< if this inode has not been added into global_ep
    ///< Then need to add it into current running global epoch
    ///< And also enable a local epoch to running
    if (!is_already_checkin_g_epoch(f2fs_i, &g_active_ep_no, &g_cur_active_epoch))
    {
        ///< For debug
        //if (f2fs_i->vfs_inode.i_ino == 10){STATUS_LOG(STATUS_INFO, "This ino-[%d] is firstly added into global epoch\n", f2fs_i->vfs_inode.i_ino);}

        if (!g_cur_active_epoch || g_active_ep_no >= MAX_GLOBAL_EP_NUM)
        {
            STATUS_LOG(STATUS_ERROR, "get current active global epoch failed\n");
            goto out;
        }

        /** enable inode local epoch, find an idle local epoch*/
        idle_local_ep_no = get_idle_local_epoch(f2fs_i);
        if (idle_local_ep_no >= MAX_GLOBAL_EP_NUM)
        {
            STATUS_LOG(STATUS_ERROR, "Didn't find an idle local epoch\n");
        }
        else
        {
            spin_lock(&f2fs_i->ino_spin_lock_local_ep);

            f2fs_i->g2l_ep_map[g_active_ep_no] = idle_local_ep_no;
            f2fs_i->j_ino_log_list[idle_local_ep_no].log_list_status = LOG_LIST_INUSE;
            f2fs_i->j_local_active_epoch = idle_local_ep_no;
            //INFO_REPORT("active local log list %d\n", idle_local_ep_no);

            // insert inode into global epoch, should use [global_ep_idx] for register
            ino_register_lock(g_active_ep_no);
            list_add_tail(&f2fs_i->ino_regis_global_epoch_list[g_active_ep_no], g_cur_active_epoch);
            ino_register_unlock(g_active_ep_no);
            //INFO_REPORT("add ino %d into global epoch-[%d]\n", f2fs_i->vfs_inode.i_ino, g_active_ep_no);

            spin_unlock(&f2fs_i->ino_spin_lock_local_ep);
        }
    }
    else
    {
        ///< This inode is already in current running epoch, do nothing
    }

out:
    return g_active_ep_no;
}


int get_inode_log_from_f2fs_inode(struct f2fs_sb_info *sbi, 
                                      struct f2fs_inode_info *f2fs_i,
                                      char * fname,
                                      create_log_t *j_create_log)
{
    struct f2fs_inode *ri = NULL;
    struct inode *vfs_i = NULL;
    vfs_i = &f2fs_i->vfs_inode;
    struct extent_tree *et = f2fs_i->extent_tree;
    struct page *ipage = NULL;

    if (vfs_i)
    {
        //get inode log
        ///< basic
        j_create_log->j_new_ino_log.i_mode = cpu_to_le16(vfs_i->i_mode);
        j_create_log->j_new_ino_log.i_advise = F2FS_I(vfs_i)->i_advise;
        j_create_log->j_new_ino_log.i_uid = cpu_to_le32(i_uid_read(vfs_i));
        j_create_log->j_new_ino_log.i_gid = cpu_to_le32(i_gid_read(vfs_i));
        j_create_log->j_new_ino_log.i_links = cpu_to_le32(vfs_i->i_nlink);
        j_create_log->j_new_ino_log.i_size = cpu_to_le64(i_size_read(vfs_i));
        j_create_log->j_new_ino_log.i_blocks = cpu_to_le64(SECTOR_TO_BLOCK(vfs_i->i_blocks) + 1);

        j_create_log->j_new_ino_log.i_atime = cpu_to_le64(vfs_i->i_atime.tv_sec);
        j_create_log->j_new_ino_log.i_ctime = cpu_to_le64(vfs_i->i_ctime.tv_sec);
        j_create_log->j_new_ino_log.i_mtime = cpu_to_le64(vfs_i->i_mtime.tv_sec);
        j_create_log->j_new_ino_log.i_atime_nsec = cpu_to_le32(vfs_i->i_atime.tv_nsec);
        j_create_log->j_new_ino_log.i_ctime_nsec = cpu_to_le32(vfs_i->i_ctime.tv_nsec);
        j_create_log->j_new_ino_log.i_mtime_nsec = cpu_to_le32(vfs_i->i_mtime.tv_nsec);

        if (S_ISDIR(vfs_i->i_mode))
        {
            j_create_log->j_new_ino_log.i_current_depth = cpu_to_le32(F2FS_I(vfs_i)->i_current_depth);
        }
        else if (S_ISREG(vfs_i->i_mode))
        {
            j_create_log->j_new_ino_log.i_gc_failures = cpu_to_le16(F2FS_I(vfs_i)->i_gc_failures[GC_FAILURE_PIN]);
        }

        j_create_log->j_new_ino_log.i_xattr_nid = cpu_to_le32(F2FS_I(vfs_i)->i_xattr_nid);
        j_create_log->j_new_ino_log.i_flags = cpu_to_le32(F2FS_I(vfs_i)->i_flags);
        j_create_log->j_new_ino_log.i_pino = cpu_to_le32(F2FS_I(vfs_i)->i_pino);
        j_create_log->j_new_ino_log.i_ino = cpu_to_le32((vfs_i)->i_ino);
        j_create_log->j_new_ino_log.i_generation = cpu_to_le32(vfs_i->i_generation);
        j_create_log->j_new_ino_log.i_dir_level = F2FS_I(vfs_i)->i_dir_level;

        // file name
        snprintf(j_create_log->j_new_ino_log.i_name, 16, "%s", fname);

// Do not consider ext for new inode
#if 0
        ///< extent tree, maybe inode can ignore extent tree
        if (et)
        {
            read_lock(&et->lock);
            set_raw_extent(&et->largest, &j_new_ino_log->i_ext);
            read_unlock(&et->lock);
        }
        else
        {
            memset(&j_new_ino_log->i_ext, 0, sizeof(j_new_ino_log->i_ext));
        }
#endif

        ///<inline
        j_create_log->j_new_ino_log.i_inline = 0;
        if (is_inode_flag_set(vfs_i, FI_INLINE_XATTR))
        j_create_log->j_new_ino_log.i_inline |= F2FS_INLINE_XATTR;
        if (is_inode_flag_set(vfs_i, FI_INLINE_DATA))
        j_create_log->j_new_ino_log.i_inline |= F2FS_INLINE_DATA;
        if (is_inode_flag_set(vfs_i, FI_INLINE_DENTRY))
        j_create_log->j_new_ino_log.i_inline |= F2FS_INLINE_DENTRY;
        if (is_inode_flag_set(vfs_i, FI_DATA_EXIST))
        j_create_log->j_new_ino_log.i_inline |= F2FS_DATA_EXIST;
        if (is_inode_flag_set(vfs_i, FI_INLINE_DOTS))
        j_create_log->j_new_ino_log.i_inline |= F2FS_INLINE_DOTS;
        if (is_inode_flag_set(vfs_i, FI_EXTRA_ATTR))
        j_create_log->j_new_ino_log.i_inline |= F2FS_EXTRA_ATTR;
        if (is_inode_flag_set(vfs_i, FI_PIN_FILE))
        j_create_log->j_new_ino_log.i_inline |= F2FS_PIN_FILE;
        if (is_inode_flag_set(vfs_i, FI_COMPRESS_RELEASED))
        j_create_log->j_new_ino_log.i_inline |= F2FS_COMPRESS_RELEASED;
    }
    else
    {
        STATUS_LOG(STATUS_ERROR, "Get a NULL vfs_inode!\n");
        return F2FSJ_ERROR;
    }
    return F2FSJ_OK;
}

int get_delete_log_from_f2fs_inode(struct f2fs_sb_info *sbi, struct f2fs_inode_info *f2fs_i,
    char * fname, delete_log_t *j_delete_log)
{
    struct f2fs_inode *ri = NULL;
    struct inode *vfs_i = NULL;
    vfs_i = &f2fs_i->vfs_inode;

    if (vfs_i)
    {
        j_delete_log->ino_num =  cpu_to_le32((vfs_i)->i_ino);
        j_delete_log->parent_ino_num = cpu_to_le32(F2FS_I(vfs_i)->i_pino);
        j_delete_log->ino_nlink = cpu_to_le32(vfs_i->i_nlink);
        snprintf(j_delete_log->file_name, 16, "%s", fname);
        j_delete_log->i_namelen = strlen(fname);
    }
    else
    {
        STATUS_LOG(STATUS_ERROR, "Get a NULL vfs_inode!\n");
        return F2FSJ_ERROR;
    }
    return F2FSJ_OK;
}


int insert_log_into_inode(struct f2fs_inode_info *f2fs_i, j_log_entry_t *j_log_entry)
{
    struct list_head *inode_log_list = NULL;
    uint8_t ino_active_log_list_idx = MAX_GLOBAL_EP_NUM;

    /** Firstly, check this inode is already checkin current running epoch*/
    ino_checkin_global_epoch(f2fs_i);

    ino_active_log_list_idx = f2fs_i->j_local_active_epoch;
    if (ino_active_log_list_idx >= MAX_GLOBAL_EP_NUM)
    {
        STATUS_LOG(STATUS_ERROR, "invalid epoch number, fatal error, please check\n");
        return F2FSJ_ERROR;
    }

    if (f2fs_i->j_ino_log_list[ino_active_log_list_idx].log_list_status != LOG_LIST_INUSE)
    {
        STATUS_LOG(STATUS_ERROR, "inode local epoch status is not running, pls check ep status switch\n");
        return F2FSJ_ERROR;
    }

    //INFO_REPORT("local inode active log list is %d\n", ino_active_log_list_idx);

    inode_log_list = &(f2fs_i->j_ino_log_list[ino_active_log_list_idx].inode_log_list_head);
    if (inode_log_list == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "NULL list, fatal error");
        return F2FSJ_ERROR;
    }

    // for debug
#if 0
    if (list_empty(inode_log_list))
    {
        INFO_REPORT("Empty ino log list first insert log\n");
    }
#endif

    // insert log into inode
    list_add_tail(&j_log_entry->log_node, inode_log_list);
}


int aggregate_per_ino_log(struct f2fs_inode_info *f2fs_i, j_checkpoint_list_t *cp_info_list_head, uint8_t global_ep_idx, uint8_t local_log_list_idx)
{
    // head node
    struct list_head * log_list_head = NULL;

    // delta log
    j_log_entry_t * ino_log_entry = NULL;
    j_log_entry_t * ino_log_entry_next  = NULL;
    uint8_t ino_log_list_idx = 0;

    if (f2fs_i)
    {
        // find list head for each log list type
        if (local_log_list_idx >= MAX_GLOBAL_EP_NUM)
        {
            /* not enable log list*/
            INFO_REPORT("not enable log list\n");
            return 0;
        }
        log_list_head = &(f2fs_i->j_ino_log_list[local_log_list_idx].inode_log_list_head);
    }
    else
    {
        return 0;
    }


    // firstly, iterate log list
    if (list_empty(log_list_head))
    {
        INFO_REPORT("ino-[%d] log list-[%d] is empty, cur inode active log list is-[%d]\n",
                     f2fs_i->vfs_inode.i_ino, ino_log_list_idx, ino_log_list_idx, f2fs_i->j_local_active_epoch);
        goto out;
    }

    list_for_each_entry_safe(ino_log_entry, ino_log_entry_next, log_list_head, log_node)
    {
        if (ino_log_entry)
        {
            //INFO_REPORT("log entry in inode, addr is %p\n", ino_log_entry);

            // get cp_info here
            get_cp_info_from_log(f2fs_i, cp_info_list_head, ino_log_entry);

            // delte this log from ino_log_list
            list_del(&ino_log_entry->log_node);

            // free log_entry_info
            j_free_log_entry(ino_log_entry);
        }
        else
        {
            INFO_REPORT("log_entry_info in inode is NULL, pls check\n");
            break;
        }
    }

out:

    // delete this inode from epoch
    list_del(&f2fs_i->ino_regis_global_epoch_list[global_ep_idx]);

    return STILL_REMAIN_SPACE;
}

int j_get_nat_log_entry(struct f2fs_sb_info *sbi, nid_t node_id, j_nat_entry_t *j_nat_e)
{
    struct f2fs_nm_info *nm_i = NM_I(sbi);
    struct nat_entry *nat_e = NULL;
    //struct node_info ni;

    nat_e = radix_tree_lookup(&nm_i->nat_root, node_id);
    /** Notice that, in __lookup_nat_cache(), it will move recent accessed nat entry
     * to the tail of lru list. I ignore this step cause that it's duplicated
    */

    /**Where is nat entry offset inside a page? Here is the f2fs logic for nat entry and nat page
     * offset = nid - START_NID(nid)
     * nat_entry = nat_blk->entries[offset]
     * Specific code you can refer f2fs_get_node_info()
    */
    if (nat_e)
    {
        j_nat_e->j_nid = nat_e->ni.nid;
        j_nat_e->j_ino = nat_e->ni.ino;
        j_nat_e->j_node_lba = nat_e->ni.blk_addr;
    }
    else
    {
        STATUS_LOG(STATUS_ERROR, "get nat entry from nat_radix_tree failed, nid is %d\n", node_id);
    }

    /** page-flag-ctrl: clear page dirty here and set it to journal dirty*/
    j_clear_NODE_page_dirty(sbi, nat_e->ni.ino);
    j_set_NODE_page_j_dirty(sbi, nat_e->ni.ino);

    j_clear_NODE_page_dirty(sbi, nat_e->ni.nid);
    j_set_NODE_page_j_dirty(sbi, nat_e->ni.nid);

    j_clear_META_nat_page_dirty(sbi, nat_e->ni.nid);
    j_set_META_nat_page_j_dirty(sbi, nat_e->ni.nid);

    return F2FSJ_OK;
}

int j_get_sit_log_entry(struct f2fs_sb_info *sbi, nid_t node_id, f2fs_current_seg_e seg_type, j_sit_log_t *j_sit_e)
{
    /** Actually, I didn't use node id, but for each file ops which need to modify SIT, should record a sit log
     *  file ops that related to new inode page or data blk
     */
    struct curseg_info *curseg = CURSEG_I(sbi, seg_type);
    struct seg_entry   *seg_e  = NULL;

    uint32_t bitmap_size = 0;
    uint32_t discard_map = f2fs_block_unit_discard(sbi) ? 1 : 0;

    /** refer to build_sit_info()*/
#ifdef CONFIG_F2FS_CHECK_FS
    bitmap_size = MAIN_SEGS(sbi) * SIT_VBLOCK_MAP_SIZE * (3 + discard_map);
#else
    bitmap_size = MAIN_SEGS(sbi) * SIT_VBLOCK_MAP_SIZE * (2 + discard_map);
#endif

    INFO_REPORT("bitmap size is %d\n", bitmap_size);

    // allocate memory for bitmap
    // TODO-f2fsj, bitmap canbe stored at array
    //j_sit_e->j_bitmap = kmalloc(bitmap_size, GFP_KERNEL);

    // get seg info
    seg_e = get_sec_entry(sbi, curseg->segno);

    // get sit entry log
    //memcpy(j_sit_e->j_bitmap, seg_e->cur_valid_map, bitmap_size);
    j_sit_e->bitmap_size      = bitmap_size;
    j_sit_e->segno            = curseg->segno;
    j_sit_e->j_nr_valid_block = seg_e->valid_blocks;
    j_sit_e->j_sit_cur_seg    = curseg->segno;

    /** page-flag-ctrl */
    j_clear_META_sit_page_dirty(sbi, curseg->segno);
    j_set_META_sit_page_j_dirty(sbi, curseg->segno);

    return 0;
}

int j_get_ssa_log_entry(struct f2fs_sb_info *sbi, nid_t node_id, uint32_t node_or_data_blk_addr,
                                            f2fs_current_seg_e seg_type, j_ssa_log_entry_t *j_ssa_e)
{
    struct f2fs_summary *target_sum_e = NULL;

    struct f2fs_sm_info *sm_info = SM_I(sbi);
    // main area is sequential logging area
    uint32_t main_area_start_blk = sm_info->main_blkaddr;

    // each f2fs_summary_blk contains 512 ssa entry
    uint32_t cur_ssa_blk_no      = (node_or_data_blk_addr - main_area_start_blk) / 512;

    // the first blk recorded in this ssa blk
    uint32_t first_blk_addr      = cur_ssa_blk_no * 512 + main_area_start_blk;

    uint32_t offset_in_ssa_blk   = node_or_data_blk_addr - first_blk_addr;
    if (offset_in_ssa_blk >= 512)
    {
        STATUS_LOG(STATUS_ERROR, "invalid ssa entry ofs - %d\n", offset_in_ssa_blk);
        return F2FSJ_ERROR;
    }

    struct curseg_info *curseg = CURSEG_I(sbi, seg_type);
    uint32_t curssa_blk        = GET_SUM_BLOCK(sbi, curseg->segno);
    void  *sum_addr = curseg->sum_blk; // the address of current segment's summary block

    target_sum_e = (struct f2fs_summary *)sum_addr + (sizeof(struct f2fs_summary) * offset_in_ssa_blk);
    j_ssa_e->block_addr        = node_or_data_blk_addr;
    j_ssa_e->j_nid             = node_id;
    j_ssa_e->blk_ofs_in_inode  = target_sum_e->ofs_in_node;

    j_ssa_e->j_cur_seg_no      = curseg->segno;

    /** page-flag-ctrl */
    j_clear_META_ssa_page_dirty(sbi, curseg->segno);
    j_set_META_ssa_page_j_dirty(sbi, curseg->segno);

    return F2FSJ_OK;
}

void j_clear_NODE_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * NODE_page = NULL;
    NODE_page = f2fs_get_node_page(sbi, nid);
    if (NODE_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear node page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clear_page_dirty_for_io(NODE_page);
#endif
    }
}

void j_set_NODE_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * NODE_page = NULL;
    NODE_page = f2fs_get_node_page(sbi, nid);
    if (NODE_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, set node page journal dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        setPageJournalDirty(NODE_page);
#endif
    }
}

void j_clear_NODE_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * NODE_page = NULL;
    NODE_page = f2fs_get_node_page(sbi, nid);
    if (NODE_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, set node page journal dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clearPageJournalDirty(NODE_page);
#endif
    }
}

void j_set_NODE_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * NODE_page = NULL;
    NODE_page = f2fs_get_node_page(sbi, nid);
    if (NODE_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, set node page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        SetPageDirty(NODE_page);
#endif
    }
}

void j_clear_META_nat_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_nat_addr(sbi, nid));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear node page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clear_page_dirty_for_io(META_page);
#endif
    }
}

void j_set_META_nat_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_nat_addr(sbi, nid));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear node page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        setPageJournalDirty(META_page);
#endif
    }
}

void j_clear_META_nat_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_nat_addr(sbi, nid));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear node page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clearPageJournalDirty(META_page);
#endif
    }
}

void j_set_META_nat_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_nat_addr(sbi, nid));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear node page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        SetPageDirty(META_page);
#endif
    }
}

void j_clear_META_sit_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear meta sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clear_page_dirty_for_io(META_page);
#endif
    }
}

void j_set_META_sit_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear meta sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        setPageJournalDirty(META_page);
#endif
    }
}

void j_clear_META_sit_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear meta sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clearPageJournalDirty(META_page);
#endif
    }
}

void j_set_META_sit_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear META sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        SetPageDirty(META_page);
#endif
    }
}

void j_clear_META_ssa_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear meta sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clear_page_dirty_for_io(META_page);
#endif
    }
}

void j_set_META_ssa_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear meta sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        setPageJournalDirty(META_page);
#endif
    }
}

void j_clear_META_ssa_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear meta sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        clearPageJournalDirty(META_page);
#endif
    }
}

void j_set_META_ssa_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno)
{
    struct page * META_page = NULL;
    META_page = f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
    if (META_page == NULL)
    {
        STATUS_LOG(STATUS_ERROR, "get inode page err, clear META sit page dirty abort\n");
    }
    else
    {
#if ENABLE_F2FSJ
        SetPageDirty(META_page);
#endif
    }
}