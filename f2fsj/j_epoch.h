/**
 * @file j_epoch.h
 * @author leslie.cui (10033908@github.com)
 * @brief implement epoch
 * @version 0.1
 * @date 2023-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __J_EPOCH_H__
#define __J_EPOCH_H__

#include "j_log.h"

#ifndef MAX_GLOBAL_EP_NUM
#define MAX_GLOBAL_EP_NUM (8)
#endif // !MAX_GLOBAL_EP

typedef enum __epoch_type_e
{
    /**
     * Now, we maintain MAX_GLOBAL_EP_NUM global epochs in round-robin way (One is running/ others are committing Or idle).
     * If users invoke fsync() frequently, it will lead to a situation that both epochs are get into committing
     * To avoid this, we support up-to MAX_GLOBAL_EP_NUM g_epochs and would discuss
     */
    GLOBAL_EPOCH_0 = 0,
    GLOBAL_EPOCH_1 = 1,   // global epoch 0-7 for inode check-in
    GLOBAL_EPOCH_2 = 2,
    GLOBAL_EPOCH_3 = 3,
    GLOBAL_EPOCH_4 = 4,
    GLOBAL_EPOCH_5 = 5,
    GLOBAL_EPOCH_6 = 6,
    GLOBAL_EPOCH_7 = 7, 

    LOCAL_EPOCH_0  = 0,  // inode local epoch
    LOCAL_EPOCH_1  = 1,
    LOCAL_EPOCH_2  = 2,
    LOCAL_EPOCH_3  = 3,
    LOCAL_EPOCH_4  = 4,
    LOCAL_EPOCH_5  = 5,
    LOCAL_EPOCH_6  = 6,
    LOCAL_EPOCH_7  = 7,

    NONE_EPOCH     = MAX_GLOBAL_EP_NUM,    // invalid epoch number
}epoch_type_e;

typedef enum __epoch_status_enum_e
{
    //EPOCH_INIT = 0,
    EPOCH_IDLE = 0,
    EPOCH_RUNNING,
    EPOCH_TOBE_COMMIT,
    EPOCH_COMMITING,
    ///< after committing, epoch becomes to IDLE and can be writeback by checkpoint
    //EPOCH_TOBE_CHECKPOINT,
    //EPOCH_CHECKPOINT
} epoch_status_e;

typedef enum __j_inode_log_list_status_e
{
    LOG_LIST_IDLE  = 0,
    LOG_LIST_INUSE = 1
}j_inode_log_list_status_e;

typedef struct __j_ino_epoch
{
    // epoch number
    uint8_t epoch_number;

    // epoch status
    j_inode_log_list_status_e log_list_status;

    // inode log list head
    struct list_head inode_log_list_head;
}j_ino_local_epoch_t;

typedef struct __global_epoch
{
    uint32_t total_checkin_ino;

    // by this value, we can know how many space is needed for this epoch
    uint32_t total_logs_size;

    uint64_t epoch_seq;

    epoch_type_e g_epoch_type;

    epoch_status_e g_epoch_status;

    struct list_head global_ino_epoch_list;

    // Added this epoch to global commit list -> this ep is waiting for committ
    struct list_head to_be_commit_global_epoch_list;
}global_epoch_t;

int is_valid_running_ep();

int get_global_epoch(uint8_t * ep_no, struct list_head ** g_ep_head);

int init_global_epoch();

int update_ino_epoch_status(j_ino_local_epoch_t * j_ino_log_list, epoch_status_e set_status);

///< @brief Should be protected by ep switch lock 
global_epoch_t *get_cur_g_running_epoch();

struct list_head *get_g_to_be_commited_epoch_list_head();

int ino_register_lock(uint8_t global_epoch_idx);

int ino_register_unlock(uint8_t global_epoch_idx);

///< @brief Should be protected by ep switch lock
int iterate_2_next_ep();

int ep_switch_spin_lock();

int ep_switch_spin_unlock();

/** API for file operations to allocate memory for log entry*/
int alloc_log_entry_memory(delta_log_t ** log_entry);


int alloc_log_memory();
#endif // !__J_EPOCH_H__