/**
 * @file j_epoch_operate.c
 * @author leslie.cui (10033908@github.com)
 * @brief operate Epoch's API
 * @version 0.1
 * @date 2023-10
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "j_epoch_commit.h"
#include "j_epoch.h"
#include "j_log_operate.h"
#include "j_journal_file.h"
#include "j_checkpoint.h"

int trigger_epoch_commit()
{
    global_epoch_t *g_to_be_committed_ep = NULL;

    uint8_t local_running_ep_no = MAX_GLOBAL_EP_NUM;
    struct list_head *g_epoch_inode_list_head        = NULL;
    struct list_head *g_to_be_committed_ep_list_head = NULL;
    struct f2fs_inode_info * f2fs_i      = NULL;
    struct f2fs_inode_info * f2fs_i_next = NULL;


    //Now, I use a spinlock to change the current running epoch to to_be_committed epoch
    ep_switch_spin_lock();

    /** Get current running epoch*/
    g_to_be_committed_ep = get_cur_g_running_epoch();

    /** Get to_be_committed_ep list_head*/
    g_to_be_committed_ep_list_head = get_g_to_be_commited_epoch_list_head();

    /** add current epoch to g_committed_ep_list*/
    list_add(&g_to_be_committed_ep->to_be_commit_global_epoch_list, g_to_be_committed_ep_list_head);

    /** change the epoch status to commit*/
    g_to_be_committed_ep->g_epoch_status = EPOCH_TOBE_COMMIT;

    /** iterate to next epoch, g_running_ep ++ and mod*/
    iterate_2_next_ep();

    ep_switch_spin_unlock();

    /** iterate the checkin inodes and set their local running epoch to EPOCH_TOBE_COMMIT*/
    /** But this step does not make sence, inode log list may not need record epoch status
     *  IDLE or INUSE for per-inode log list is enough 
    */
#if 0
    g_epoch_inode_list_head = &g_to_be_committed_ep->global_ino_epoch_list;
    list_for_each_entry_safe(f2fs_i, f2fs_i_next, g_epoch_inode_list_head, ino_regis_global_epoch_list)
    {
        local_running_ep_no = f2fs_i->j_local_active_epoch;
        f2fs_i->j_ino_log_list[local_running_ep_no].log_list_status = EPOCH_TOBE_COMMIT; // can delete
    }
#endif

    INFO_REPORT("epoch %d can be committed\n", g_to_be_committed_ep->epoch_seq);

    return F2FSJ_OK;
}

int epoch_commit(struct f2fs_sb_info *sbi)
{
    int ret = F2FSJ_OK;
    struct list_head *g_to_be_committed_ep_list_head = NULL;
    global_epoch_t *g_to_be_committed_ep = NULL;
    global_epoch_t *g_to_be_committed_ep_next = NULL;

    struct list_head *g_epoch_inode_list_head = NULL;
    struct f2fs_inode_info * f2fs_i      = NULL;
    struct f2fs_inode_info * f2fs_i_next = NULL;

    uint8_t local_ep_idx  = NONE_EPOCH;
    uint8_t global_ep_idx = NONE_EPOCH;

    struct page *p = NULL;
    struct bio  *b = NULL;
    uint32_t j_start_blk = 0;

    /** get global commit and checkpoint epoch list head*/
    g_to_be_committed_ep_list_head  = get_g_to_be_commited_epoch_list_head();

    j_checkpoint_list_t *cp_info_list_head_node = NULL;

    if (!list_empty(g_to_be_committed_ep_list_head))
    {
        /** iterate global epoch list and commit these epochs one by one*/
        list_for_each_entry_safe(g_to_be_committed_ep, g_to_be_committed_ep_next, 
                                        g_to_be_committed_ep_list_head, to_be_commit_global_epoch_list)
        {

            ///< get list where inode registered
            g_epoch_inode_list_head = &g_to_be_committed_ep->global_ino_epoch_list;
            if (list_empty(g_epoch_inode_list_head))
            {
                //This epoch is empty
                ///< delete this g_epoch from g_to_be_commited_ep_list
                //list_del(&g_to_be_committed_ep->to_be_commit_global_epoch_list);

                ///< change g_epoch status to EPOCH_IDLE, checkpoint is another policy
                //g_to_be_committed_ep->g_epoch_status = EPOCH_IDLE;

                INFO_REPORT("An empty RUNNING epoch and change to IDLE\n");
                //continue;
            }

            ///< change this global epoch to COMMITTING
            g_to_be_committed_ep->g_epoch_status = EPOCH_COMMITING;

            /** Commit journal become simple casue we memmap the journal file, and only need to flush the corresponding small
             * file to disk
             */

            ///< init cp_info list head
            alloc_log_cp_head_node_memory(&cp_info_list_head_node);
            if (!cp_info_list_head_node)
            {
                STATUS_LOG(STATUS_ERROR, "allocate memory for cp_info_list head node fail\n");
                return F2FSJ_ERROR;
            }

            ///< link this cp_info list head into g_checkpoint_list
            insert_cp_info_list_head_2_g_cp_list(cp_info_list_head_node);

            global_ep_idx = g_to_be_committed_ep->g_epoch_type;
            INFO_REPORT("global ep idx %d\n", global_ep_idx);
            /** iter each inode log list and aggregate logs into pages*/
            list_for_each_entry_safe(f2fs_i, f2fs_i_next, g_epoch_inode_list_head, ino_regis_global_epoch_list[global_ep_idx])
            {
                //spin_lock(&f2fs_i->ino_spin_lock_local_ep);
                if (f2fs_i)
                {
                    //INFO_REPORT("get ino [%d] from epoch\n", f2fs_i->vfs_inode.i_ino);
                    local_ep_idx  = f2fs_i->g2l_ep_map[global_ep_idx];

                    if (local_ep_idx >= MAX_GLOBAL_EP_NUM)
                    {
                        /* not enable log list*/
                        continue;
                    }

                    ///< change this inode local epoch to COMMITTING
                    f2fs_i->j_ino_log_list[local_ep_idx].log_list_status = EPOCH_COMMITING; // can delete
                    /** reset inode local active epoch
                     *  wait future file ops to make this inode register to new running g_epoch and also enable local epoch*/
                    f2fs_i->j_local_active_epoch = NONE_EPOCH;

                    //spin_unlock(&f2fs_i->ino_spin_lock_local_ep);

                    /** aggregates logs into page
                     *  In this function, log entries will be deleted from local log list
                     *  collec cp_info for journal apply
                    */
                    aggregate_per_ino_log(f2fs_i, cp_info_list_head_node, global_ep_idx, local_ep_idx);

                    /** reset local inode log list status*/
                    //local_ep_idx  = f2fs_i->g2l_ep_map[global_ep_idx];
                    f2fs_i->j_ino_log_list[local_ep_idx].log_list_status = LOG_LIST_IDLE;
                }
                else
                {
                    // null inode
                }
            }

            // Code at here means that we already aggragate information of a group of logs which comes from same global epoch
            // we can commit journal now
            write_current_mmap_j_file(sbi);

            // Then set NODE and META page to dirty and ready for checkpoint
            // TODO
            //set_page_dirty_and_ready_do_cp(sbi, cp_info_list_head_node);

            ///< when complete commit, delete this g_epoch from g_to_be_commited_ep_list
            list_del(&g_to_be_committed_ep->to_be_commit_global_epoch_list);

            ///< change g_epoch status to EPOCH_IDLE, checkpoint is another policy
            g_to_be_committed_ep->g_epoch_status = EPOCH_IDLE;

            // Iteration keep going, handle next g_epoch until g_to_be_commit list become empty
        }
    }
    else
    {
        INFO_REPORT("To be committed epoch list is empty, no register epoch\n");
    }

    return F2FSJ_OK;
}

int is_g_commit_ep_empty()
{
    struct list_head *g_to_be_committed_ep_list_head = NULL;
    g_to_be_committed_ep_list_head  = get_g_to_be_commited_epoch_list_head();

    if (list_empty(g_to_be_committed_ep_list_head))
    {
        //INFO_REPORT("commit ep is empty\n");
        return 1;
    }
    else
    {
        //INFO_REPORT("commit ep is not empty\n");
        return 0;
    }
}