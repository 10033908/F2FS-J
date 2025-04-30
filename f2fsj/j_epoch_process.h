/**
 * @file j_epoch_process.h
 * @author leslie.cui (10033908@github.com)
 * @brief This file is to create thread to handle journal commit and journal checkpoint
 * @version 0.1
 * @date 2023-10
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef _J_EPOCH_PROCESS_H
#define _J_EPOCH_PROCESS_H

#include <linux/wait.h>
#include <linux/workqueue.h>
#include "f2fs.h"
#include <linux/sched.h>

#define J_EP_COMMIT_TIMEOUT (4 * 1e3)   ///< 2000ms <-> 2s
#define J_CHECKPOINT_TIMEOUT (4 * 1e3)

typedef struct __j_ep_commit_task
{
    struct task_struct *f2fsj_ep_commit_task;
    wait_queue_head_t  j_ep_commit_wait_queue;
}j_ep_commit_task_t;


typedef struct __j_ep_checkpoint_task
{
    struct task_struct *f2fsj_checkpoint_task;
    wait_queue_head_t  j_checkpoint_wait_queue;
}j_checkpoint_task_t;

#ifndef F2FSj_K_THREAD_SLEEP_MS                   
#define F2FSj_K_THREAD_SLEEP_MS(ms)             \
do                                              \
{                                               \
    long timeout = (ms) * HZ / 1000;            \
    while (timeout > 0)                         \
    {                                           \
        timeout = schedule_timeout(timeout);    \
    }                                           \
}while(0)
#endif

int j_ep_commit_kthread(void *param);

int create_j_ep_commit_kthread(struct f2fs_sb_info *sbi);

int j_checkpoint_kthread(void *param);

int create_j_checkpoint_kthread(struct f2fs_sb_info *sbi);

/**
 * @brief This function is invoked at f2fs_mount()
 * 
 * @param sbi 
 * @return int 
 */
int create_f2fsj_kthread(struct f2fs_sb_info *sbi);


int stop_f2fsj_kthread();

#endif // !_J_EPOCH_PROCESS_H