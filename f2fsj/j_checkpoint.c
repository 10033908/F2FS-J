/**
 * @file j_checkpoint.c
 * @author leslie.cui (10033908@github.com)
 * @brief 
 * @version 0.1
 * @date 2023-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "j_checkpoint.h"
#include "j_epoch.h"
#include "j_journal_file.h"

static j_checkpoint_list_t g_checkpoint_list;
static struct kmem_cache *kmem_log_cp_info_slab_cache_heap = NULL;
static struct kmem_cache *kmem_log_cp_list_head_node_slab_cache_heap = NULL;
static uint8_t trigger_journal_apply = 0;

int init_g_checkpoint_list()
{
    INIT_LIST_HEAD(&g_checkpoint_list.g_checkpoint_list);
    INIT_LIST_HEAD(&g_checkpoint_list.ep_log_cp_info_list_head);

    g_checkpoint_list.g_ep_num = 0;
    g_checkpoint_list.g_ep_ver = 0;

    kmem_log_cp_info_slab_cache_heap = NULL;
    kmem_log_cp_info_slab_cache_heap = kmem_cache_create("f2fsj_log_cp_info_cache_heap", sizeof(j_log_cp_info_t), 0,
                                                        SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
    if (!kmem_log_cp_info_slab_cache_heap)
    {
        STATUS_LOG(STATUS_FATAL, "init log cp info cache heap fail, f2fsj service exit\n");
        return F2FSJ_FATAL;
    }

    kmem_log_cp_list_head_node_slab_cache_heap = NULL;
    kmem_log_cp_list_head_node_slab_cache_heap = kmem_cache_create("f2fsj_log_cp_list_head_cache_heap", sizeof(j_checkpoint_list_t), 0,
                                                        SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
    if (!kmem_log_cp_list_head_node_slab_cache_heap)
    {
        STATUS_LOG(STATUS_FATAL, "init log cp info cache heap fail, f2fsj service exit\n");
        return F2FSJ_FATAL;
    }

    return F2FSJ_OK;
}

int epoch_checkpoint(struct f2fs_sb_info *sbi)
{
    int err;
    struct list_head *g_checkpoint_list_head = NULL;
    j_checkpoint_list_t *g_to_be_checkpoint_ep = NULL;
    j_checkpoint_list_t *g_to_be_checkpoint_ep_next = NULL;

    struct list_head *g_epoch_cp_info_list_head = NULL;
    j_log_cp_info_t * cp_info      = NULL;
    j_log_cp_info_t * cp_info_next = NULL;

    struct cp_control cpc = {
        .reason = CP_FASTBOOT,
    };


    // get global to_be_checkpoint list_head
    g_checkpoint_list_head = &g_checkpoint_list.g_checkpoint_list;

    list_for_each_entry_safe(g_to_be_checkpoint_ep, g_to_be_checkpoint_ep_next,
                                    g_checkpoint_list_head, g_checkpoint_list)
    {
        g_epoch_cp_info_list_head = &g_to_be_checkpoint_ep->ep_log_cp_info_list_head;

        // use cp_info (#inode, #node, #NAT, #segnum) to locate in-memory metadata pages
        // With page state control, use bind writes by flushing Dirty in-mem FS metadata
        list_for_each_entry_safe(cp_info, cp_info_next, g_epoch_cp_info_list_head, log_cp_list)
        {
#if 0
            // apply inode page
            if (cp_info->log_inode_id != 0)
            {
                j_apply_NODE(sbi, cp_info->log_inode_id);
            }

            // apply Dirty FS metadata

            // apply node page
            if (cp_info->log_node_id != 0)
            {
                j_apply_NODE(sbi, cp_info->log_node_id);
            }

            // apply SIT
            if (cp_info->log_segno != 0)
            {
                j_apply_META(sbi, cp_info->log_inode_id, cp_info->log_node_id, cp_info->log_segno);
            }
#endif
            ///< delete cp_info from epoch_cp_info_list
            list_del(&cp_info->log_cp_list);
            free_log_cp_info_memory(cp_info);
        }


        ///< delete these cp_info_list of one global epoch from g_checkpoint_list
        list_del(&g_to_be_checkpoint_ep->g_checkpoint_list);
        free_log_cp_head_node_memory(g_to_be_checkpoint_ep);
    }

    //apply by ckpt
    INFO_REPORT("Apply in-mem metadata begin\n");
    //j_apply_flushing(sbi, &cpc);
    f2fs_write_checkpoint(sbi, &cpc);
    INFO_REPORT("Apply in-mem metadata end\n");

    // reset on-disk journal space info
    reset_on_disk_journal_space_info();

    return F2FSJ_OK;
}

/**
 * @brief Whether global to_be checkpoint log file is empty
 * 
 * @return int 
 */
int is_g_checkpoint_cp_list_empty()
{
    struct list_head *g_checkpoint_list_head = NULL;
    g_checkpoint_list_head  = &g_checkpoint_list.g_checkpoint_list;

    if (list_empty(g_checkpoint_list_head))
    {
        //INFO_REPORT("g_checkpoint list is empty\n");
        return 1;
    }
    else
    {
        //INFO_REPORT("g_checkpoint list is not empty\n");
        return 0;
    }
}

int alloc_log_cp_head_node_memory(j_checkpoint_list_t ** cp_head_node)
{
    *cp_head_node = kmem_cache_alloc(kmem_log_cp_list_head_node_slab_cache_heap, GFP_NOIO);
    if (!(*cp_head_node))
    {
        STATUS_LOG(STATUS_ERROR, "allocate cp_head_node memory failed\n");
        return F2FSJ_ERROR;
    }

    // init cp_info list head
    INIT_LIST_HEAD(&((*cp_head_node)->ep_log_cp_info_list_head));

    return F2FSJ_OK;
}

int alloc_log_cp_info_memory(j_log_cp_info_t ** log_cp_info)
{
    *log_cp_info = kmem_cache_alloc(kmem_log_cp_info_slab_cache_heap, GFP_NOIO);
    if (!(*log_cp_info))
    {
        STATUS_LOG(STATUS_ERROR, "allocate log_cp_info memory failed\n");
        return F2FSJ_ERROR;
    }

    /** value 0 means that there is no need to apply correspoind META or NODE*/
    (*log_cp_info)->log_inode_id = 0;
    (*log_cp_info)->log_node_id  = 0;
    (*log_cp_info)->log_segno    = 0;
    //INIT_LIST_HEAD(&((*log_cp_info)->log_cp_list));

    return F2FSJ_OK;
}

int free_log_cp_info_memory(j_log_cp_info_t* log_cp_info)
{
    if (log_cp_info)
    {
        kmem_cache_free(kmem_log_cp_info_slab_cache_heap, log_cp_info);
    }
    else
    {
        INFO_REPORT("NULL cp info entry, cannot free\n");
        return F2FSJ_ERROR;
    }
    return F2FSJ_OK;
}

int free_log_cp_head_node_memory(j_checkpoint_list_t* cp_head_node)
{
    if (cp_head_node)
    {
        kmem_cache_free(kmem_log_cp_list_head_node_slab_cache_heap, cp_head_node);
    }
    else
    {
        INFO_REPORT("NULL cp head entry, cannot free\n");
        return F2FSJ_ERROR;
    }
    return F2FSJ_OK;
}

int insert_cp_info_list_head_2_g_cp_list(j_checkpoint_list_t *cp_head_node)
{
    // for debug
    if (list_empty(&g_checkpoint_list.g_checkpoint_list))
    {
        INFO_REPORT("Empty global j_CP list, first insert log\n");
    }

    list_add_tail(&cp_head_node->g_checkpoint_list, &g_checkpoint_list.g_checkpoint_list);

    return F2FSJ_OK;
}


int get_cp_info_from_log(struct f2fs_inode_info *f2fs_i, j_checkpoint_list_t *cp_head_node, j_log_entry_t *log_entry)
{
    int ret = 0;
    j_log_cp_info_t * cp_info = NULL;
    j_log_head_t *log_header = NULL;
    log_header = (j_log_head_t*)log_entry->log_entry_addr;

    log_type_e log_type = log_header->log_type;

    alloc_log_cp_info_memory(&cp_info);
    if (!cp_info)
    {
        STATUS_LOG(STATUS_ERROR, "alloc mem for cp_info fail\n");
        return F2FSJ_ERROR;
    }

    // init cp_info
    cp_info->log_inode_id = 0;
    cp_info->log_node_id = 0;
    cp_info->log_segno = 0;  

    /**
     * Please notice that if the inode id or node id equals to 0, it means that no need to apply
     *
     */
    if (log_type == CREATE_LOG || log_type == MKDIR_LOG || log_type == SYMLINK_LOG)
    {
        create_log_t * create_log = (create_log_t *)log_entry->log_entry_addr;
        // Do not consider FS metadata for newly create files
        cp_info->log_inode_id = create_log->j_new_ino_log.i_ino;
        cp_info->log_node_id  = 0;
        cp_info->log_segno    = 0;   
    }
    else if (log_type == DIR_LOG)
    {
        j_dir_log_t *dir_log = (j_dir_log_t *)log_entry->log_entry_addr;
        cp_info->log_inode_id = dir_log->parent_ino;
    }
    else if (log_type == DATA_WRITE_LOG)
    {
        data_write_log_t * write_log = (data_write_log_t *)log_entry->log_entry_addr;
        cp_info->log_inode_id = write_log->ino_num;
        cp_info->log_node_id  = write_log->nat_en_log.j_nid;
        cp_info->log_segno    = write_log->sit_log.segno;
        /** Commit phase
         *  writeback() data pages (by DF logs)
        */
        ret = ep_commit_writeback_data_pages(f2fs_i);
        if (ret != F2FSJ_OK)
        {
            STATUS_LOG(STATUS_ERROR, "J_commit invoke writepages() happens err\n");
            return F2FSJ_ERROR;
        }
    }
    else if (log_type == CHOWN_LOG)
    {
        chown_log_t * chown_log = (chown_log_t *)log_entry->log_entry_addr;
        cp_info->log_inode_id = chown_log->ino_num;
        cp_info->log_node_id  = 0;
        cp_info->log_segno    = 0;     
    }
    else if (log_type == UNLINK_LOG)
    {
        delete_log_t * delete_log = (delete_log_t *)log_entry->log_entry_addr;
        // Do not consider FS metadata for newly create files
        cp_info->log_inode_id = delete_log->ino_num;
    }
    //TODO, rename and link
    else
    {
        STATUS_LOG(STATUS_INFO, "This type of log is unknown- [%d], log size is %d\n", log_type, log_header->log_size);
    }

    // insert this cp_info into cp_info_list
    list_add_tail(&(cp_info->log_cp_list), &(cp_head_node->ep_log_cp_info_list_head));

    return F2FSJ_OK;
}

int set_page_dirty_and_ready_do_cp(struct f2fs_sb_info *sbi, j_checkpoint_list_t *cp_head_node)
{
    j_log_cp_info_t * cp_info      = NULL;
    j_log_cp_info_t * cp_info_next = NULL;

    list_for_each_entry_safe(cp_info, cp_info_next, &cp_head_node->ep_log_cp_info_list_head, log_cp_list)
    {
            if (cp_info->log_inode_id != 0)
            {
                j_clear_NODE_page_j_dirty(sbi, cp_info->log_inode_id);
                j_set_NODE_page_dirty(sbi, cp_info->log_inode_id);
                j_clear_META_nat_page_j_dirty(sbi, cp_info->log_inode_id);
                j_set_META_nat_page_dirty(sbi, cp_info->log_inode_id);
            }

            if (cp_info->log_node_id != 0)
            {
                j_clear_NODE_page_j_dirty(sbi, cp_info->log_node_id);
                j_set_NODE_page_dirty(sbi, cp_info->log_node_id);
                j_clear_META_nat_page_j_dirty(sbi, cp_info->log_node_id);
                j_set_META_nat_page_dirty(sbi, cp_info->log_node_id);
            }

            if (cp_info->log_segno != 0)
            {
                j_clear_META_sit_page_j_dirty(sbi, cp_info->log_segno);
                j_set_META_sit_page_dirty(sbi, cp_info->log_segno);
                j_clear_META_ssa_page_j_dirty(sbi, cp_info->log_segno);
                j_set_META_ssa_page_dirty(sbi, cp_info->log_segno);
            }
    }
}