/**
 * @file j_epoch_operate.h
 * @author leslie.cui (10033908@github.com)
 * @brief 
 * @version 0.1
 * @date 2023-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef _J_EPOCH_OPERATE_H
#define _J_EPOCH_OPERATE_H

#include "f2fs.h"

/**
 * @brief Current running epoch will be changed to to_be_committed status
 *        when this function invoked
 *        .fsync() will invoke this function
 * 
 * @return int trigger successful or not
 */
int trigger_epoch_commit();

/**
 * @brief This function is to 1) iterate g_commit_ep list to find checked-in inode;
 *        2) iterate each inode's local log list
 *        This function shoule be invoked by j_commit_thread
 * @return int 
 */
int epoch_commit(struct f2fs_sb_info *sbi);

/**
 * @brief Whether global to_be committed epoch is empty
 * 
 * @return 1 is empty, 0 is none empty
 */
int is_g_commit_ep_empty();

#endif // !_J_EPOCH_OPERATE_H