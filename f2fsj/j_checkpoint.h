/**
 * @file j_checkpoint.h
 * @author leslie.cui (10033908@github.com)
 * @brief 
 * @version 0.1
 * @date 2023-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "f2fs.h"
#include "j_log_basic.h"
#include "j_journal_file.h"

#ifndef J_CHECKPOINT_H
#define J_CHECKPOINT_H

typedef struct __j_checkpoint_list
{
    //chained to global checkpoint_list
    struct list_head g_checkpoint_list;

    //for cp_list head node, chain log_cp_info comes from same epoch 
    struct list_head ep_log_cp_info_list_head;
    uint32_t g_ep_num;
    uint32_t g_ep_ver;
}j_checkpoint_list_t;

typedef struct __j_log_cp_info
{
    /**
     * f2fs_get_node_page(sbi, nid);
     * 
     * f2fs_get_meta_page(sbi, current_nat_addr(sni, nid));
     * f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
     * f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
     */

    uint32_t log_inode_id;
    uint32_t log_node_id;
    uint32_t log_segno;

    struct list_head log_cp_list;
}j_log_cp_info_t;

int init_g_checkpoint_list();

int alloc_log_cp_info_memory(j_log_cp_info_t ** log_cp_info);

int alloc_log_cp_head_node_memory(j_checkpoint_list_t ** cp_head_node);

int free_log_cp_info_memory(j_log_cp_info_t* log_cp_info);

int free_log_cp_head_node_memory(j_checkpoint_list_t* cp_head_node);

int insert_cp_info_list_head_2_g_cp_list(j_checkpoint_list_t *cp_head_node);

int get_cp_info_from_log(struct f2fs_inode_info *f2fs_i, j_checkpoint_list_t *cp_head_node, j_log_entry_t *delta_log);

/**
 * @brief set the META and NODE to dirty, means journa logs are already on the disk
 *        and we can do checkpoint soon after
 * 
 * @param cp_head_node 
 * @return int 
 */
int set_page_dirty_and_ready_do_cp(struct f2fs_sb_info *sbi, j_checkpoint_list_t *cp_head_node);

/**
 * @brief This function will be invoked by j_checkpoint_thread
 *
 *        Checkpoint order: 1)apply NODE -> refer f2fs_sync_inode_meta() and f2fs_sync_node_pages()
 *                          2)apply META -> refer f2fs_sync_meta_pages(). maybe directly invoke f2fs_write_meta_pages()
 *
 * @param sbi
 * @return int
 */
int epoch_checkpoint(struct f2fs_sb_info *sbi);

/**
 * @brief Whether global to_be checkpoint log file is empty
 * 
 * @return int 
 */
int is_g_checkpoint_cp_list_empty();


#endif // !J_CHECKPOINT_H