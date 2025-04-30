/**
 * @file j_epoch_process.c
 * @author leslie.cui (10033908@github.com)
 * @brief process per-inode logs that are registered in global epoch
 * @version 0.1
 * @date 2023-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "j_epoch_process.h"
#include "j_epoch_commit.h"
#include "j_checkpoint.h"
#include "node.h"
#include "segment.h"

static j_ep_commit_task_t     g_ep_commit_thread = {0};
static j_checkpoint_task_t    g_checkpoint_thread = {0};

static spinlock_t journal_commit_spin_lock;
static spinlock_t journal_checkpoint_spin_lock;

static uint8_t stop_flag = 0;
static uint8_t stop_flag2 = 0;

static bool is_clean_jfile = false;

#define J_COMMIT_INTERVAL (5)

int j_ep_commit_kthread(void *param)
{
    struct f2fs_sb_info *sbi = (struct f2fs_sb_info *)param;
    wait_queue_head_t *q = &g_ep_commit_thread.j_ep_commit_wait_queue;
    uint64_t commt_thread_cnt = 0;

    INFO_REPORT("Journal epoch commit thread begins to run\n");

    while (!kthread_should_stop())
    {
        commt_thread_cnt ++;
        set_current_state(TASK_INTERRUPTIBLE);

        // sleep
        F2FSj_K_THREAD_SLEEP_MS(1000);

        if (!is_clean_jfile)
        {
            // make journal clean by setting 0
            INFO_REPORT("clear journal file...\n");
            clear_journal_file_after_recovery(sbi->sb);
            INFO_REPORT("clear journal file end\n");
            is_clean_jfile = true;
        }

        if (commt_thread_cnt % J_COMMIT_INTERVAL == 0)
        //if (commt_thread_cnt == 1) // for debug
        {
            // Switch to next journal period
            trigger_epoch_commit();

            if (!is_g_commit_ep_empty())
            {
                epoch_commit(sbi);
            }
        }
        //INFO_REPORT("[%s] run one time\n", __FUNCTION__);
    }

    return F2FSJ_OK;
}

int j_checkpoint_kthread(void *param)
{
    struct f2fs_sb_info *sbi = (struct f2fs_sb_info *)param;
    wait_queue_head_t *q = &g_checkpoint_thread.j_checkpoint_wait_queue;

    INFO_REPORT("Journal checkpoint thread begins to run\n");

    static uint64_t cnt = 0;
    static uint64_t cnt_unlink = 0;
    int free_on_disk_journal_space = 0;
    uint64_t cp_thread_cnt = 0;

    while (!kthread_should_stop())
    {
        set_current_state(TASK_INTERRUPTIBLE);

        // sleep 1s
        F2FSj_K_THREAD_SLEEP_MS(1000);

        cp_thread_cnt ++;

        // if free journal space is less than 25%, should trigger checkpoint to apply logs and free journal file
        free_on_disk_journal_space = get_on_disk_free_journal_space();
        if (free_on_disk_journal_space)
        {
            // free space trigger journal apply (journal ckpt)
            if (free_on_disk_journal_space <= (0.05) * JOURNAL_FILE_SIZE)
            {
                // if global checkpoint list is not empty, we can do checkpoint
                if (!is_g_checkpoint_cp_list_empty())
                {
                    INFO_REPORT("Exceed free journal space ratio, trigger checkpoint\n");
                    epoch_checkpoint(sbi);
                }
            }

            // timeout to trigger checkpoint
            if (cp_thread_cnt % 3 == 0)
            {
                if (!is_g_checkpoint_cp_list_empty())
                {
                    INFO_REPORT("Exceed 30s, trigger checkpoint\n");
                    epoch_checkpoint(sbi);
                }
            }
        }
    }

    return F2FSJ_OK;
}

int create_j_ep_commit_kthread(struct f2fs_sb_info *sbi)
{
    //init_waitqueue_head(&g_ep_commit_thread.j_ep_commit_wait_queue);

    g_ep_commit_thread.f2fsj_ep_commit_task = (struct task_struct *)kthread_run(j_ep_commit_kthread, sbi, "j_commit_t");

    return F2FSJ_OK;
}

int create_j_checkpoint_kthread(struct f2fs_sb_info *sbi)
{
    //init_waitqueue_head(&g_checkpoint_thread.j_checkpoint_wait_queue);

    g_checkpoint_thread.f2fsj_checkpoint_task = (struct task_struct *)kthread_run(j_checkpoint_kthread, sbi, "j_checkpoint_t");

    return F2FSJ_OK;
}

int create_f2fsj_kthread(struct f2fs_sb_info *sbi)
{
    create_j_ep_commit_kthread(sbi);

    create_j_checkpoint_kthread(sbi);
}

int stop_f2fsj_kthread()
{
    if (g_checkpoint_thread.f2fsj_checkpoint_task)
    {
        kthread_stop(g_checkpoint_thread.f2fsj_checkpoint_task);
        INFO_REPORT("stop checkpoint thread\n");
    }
    else
    {
        STATUS_LOG(STATUS_WARNING, "f2fsj checkpoint task is null, no need to stop\n");
    }

    if (g_ep_commit_thread.f2fsj_ep_commit_task)
    {
        kthread_stop(g_ep_commit_thread.f2fsj_ep_commit_task);
        INFO_REPORT("stop journal commit thread\n");
    }
    else
    {
        STATUS_LOG(STATUS_WARNING, "f2fsj ep commit thread task is null, no need to stop\n");
    }

    return F2FSJ_OK;
}