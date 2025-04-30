/**
 * @file j_log_operate.h
 * @author leslie.cui (10033908@github.com)
 * @brief provide some interface to operate logs in inode
 * @version 0.1
 * @date 2023-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __J_LOG_OPERATE_H__
#define __J_LOG_OPERATE_H__
#include "f2fs.h"
#include "j_log_content.h"
#include "j_checkpoint.h"
#include "j_journal_file.h"

typedef enum __aggregate_logs_e
{
    STILL_REMAIN_SPACE = 0,
    FULL_PAGE_SPACE = 1,
}aggregate_logs_e;

/**
 * @brief This function is to verify if the inode is already registered into g_epoch
 *        If not, this inode should be registered into current running g_epoch and also
 *        enable local epoch
 * 
 * @param f2fs_i 
 * @return int 
 */
int is_already_checkin_g_epoch(struct f2fs_inode_info *f2fs_i, uint8_t * ep_no, struct list_head ** g_ep_head);

/**
 * @brief Get the idle local epoch object
 * 
 * @param f2fs_i 
 * @return the first idle local epoch number
 */
int get_idle_local_epoch(struct f2fs_inode_info *f2fs_i);

/**
 * @brief This function would be invoked when inserting logs into inode local log list.
 *        1)invoke is_already_checkin_g_epoch() to check if this inode checked in global inode list
 *          if not, register this inode into current running 
 *        2)invoke get_idle_local_epoch() to enable a local epoch
 * 
 * @param f2fs_i 
 * @return int 
 */
int ino_checkin_global_epoch(struct f2fs_inode_info *f2fs_i);

/************** Specific functions that is invoked to insert log into inode **************/

int insert_log_into_inode(struct f2fs_inode_info *f2fs_i, j_log_entry_t *j_log_entry);

/// @brief collect new inode log from vfs inode and f2fs_inode
/// @param[in] f2fs_i (contains vfs_inode)
/// @param[out] j_ino_log
/// @return
int get_inode_log_from_f2fs_inode(struct f2fs_sb_info *sbi,
                                      struct f2fs_inode_info *f2fs_i,
                                      char * fname,
                                      create_log_t *j_create_log);

int get_delete_log_from_f2fs_inode(struct f2fs_sb_info *sbi,
                                    struct f2fs_inode_info *f2fs_i,
                                    char * fname,
                                    delete_log_t *j_delete_log);
/*************** Specific functions that is invoked to insert log into inode **************/

/**
 * @brief aggregate one inode's logs into one page, start from offset
 *        except a new inode page, may one inode logs can exceed page-size
 *        So, when processing per-ino logs, plz use an iteration to aggregate logs
 * @param f2fs_i
 * @param j_page: copy_log_into_page
 * @param off: how many size are already used by log
 * @param cp_info_list_head: a list contains checkpoint informations to appy META and NODE
 * @param[out] new_off
 */
int aggregate_per_ino_log(struct f2fs_inode_info *f2fs_i, j_checkpoint_list_t *cp_info_list_head, uint8_t global_ep_idx, uint8_t local_log_list_idx);

/***************** Some critical functions that could get nat_entry/sit_entry/ssa_entry ****************/

/**
 * @brief refer to __lookup_nat_cache(), we get struct node_info from this interface.
 * Then, transfer it to j_nat_entry
 * @param[in], node id
 * @param[out], j_nat_entry
*/
int j_get_nat_log_entry(struct f2fs_sb_info *sbi, nid_t node_id, j_nat_entry_t *j_nat_e);

/**
 * @brief refer to get_sec_entry() and get_seg_entry() and current_sit_addr()
 *        we get number of valid blocks and also the bitmap of this segment
 *        For bitmap size, I refer to build_sit_info()
 */
int j_get_sit_log_entry(struct f2fs_sb_info *sbi, nid_t node_id, f2fs_current_seg_e seg_type, j_sit_log_t *j_sit_e);

/**
 * @brief refer to code set_summary(), __add_sum_entry()
 *
 */
int j_get_ssa_log_entry(struct f2fs_sb_info *sbi, nid_t node_id, uint32_t node_or_data_blk_addr, f2fs_current_seg_e seg_type, j_ssa_log_entry_t *j_ssa_e);
/***************** Some critical functions that could get nat_entry/sit_entry/ssa_entry ****************/


/**
 * As considering that We cannot directly get META and NODE page
 * So we can package one level by using nid and seg to get correspoing page and set flag
 * 
 *   f2fs_get_node_page(sbi, nid); 
 *   f2fs_get_meta_page(sbi, current_nat_addr(sbi, nid));
 *   f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
 *   f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
 * 
 *  When doing log collection
 *   1) clear_page_dirty_for_io(page)
 *   2) setJournalDirty(page)
 *   After committing:
 *   3) SetPageDirty(page)
 */

void j_clear_NODE_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid);
void j_set_NODE_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid);
void j_set_NODE_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid);
void j_clear_NODE_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid);

void j_clear_META_nat_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid);
void j_set_META_nat_page_dirty(struct f2fs_sb_info *sbi, uint32_t nid);
void j_set_META_nat_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid);
void j_clear_META_nat_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t nid);

void j_clear_META_sit_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno);
void j_set_META_sit_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno);
void j_set_META_sit_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno);
void j_clear_META_sit_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno);

void j_clear_META_ssa_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno);
void j_set_META_ssa_page_dirty(struct f2fs_sb_info *sbi, uint32_t segno);
void j_set_META_ssa_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno);
void j_clear_META_ssa_page_j_dirty(struct f2fs_sb_info *sbi, uint32_t segno);

#endif // !__J_LOG_OPERATE_H__