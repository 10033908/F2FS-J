/**
 * @file j_epoch.c
 * @author leslie.cui (10033908@github.com)
 * @brief operate epoch
 * @version 0.1
 * @date 2023-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "j_epoch.h"
#include <linux/slab.h>
#include "f2fs.h"

static uint64_t g_epoch_seq = 0;
static uint8_t g_running_ep = 0; // TODO, This value should be controled by journal process thread
static global_epoch_t global_epoch[MAX_GLOBAL_EP_NUM];
static spinlock_t epoch_switch_spin_lock;
static spinlock_t global_epoch_ino_register_lock[MAX_GLOBAL_EP_NUM];


///< for epoch commit and checkpoint
static global_epoch_t g_epoch_head;

int init_global_epoch()
{
    int i = 0;
    for (i = 0; i < MAX_GLOBAL_EP_NUM; i ++)
    {
        global_epoch[i].g_epoch_type   = i;
        global_epoch[i].g_epoch_status = EPOCH_IDLE;
        global_epoch[i].total_checkin_ino = 0;
        global_epoch[i].epoch_seq = 0;

        INIT_LIST_HEAD(&global_epoch[i].global_ino_epoch_list);
        INIT_LIST_HEAD(&global_epoch[i].to_be_commit_global_epoch_list);

        spin_lock_init(&global_epoch_ino_register_lock[i]);
    }

    //Init epoch switch spin lock
    spin_lock_init(&epoch_switch_spin_lock);

    //Init global commit/checkpoint epoch list
    INIT_LIST_HEAD(&g_epoch_head.to_be_commit_global_epoch_list);

    INFO_REPORT("init global epoch end\n");
    return 0;
}


int is_valid_running_ep()
{
    if (g_running_ep >= MAX_GLOBAL_EP_NUM)
    {
        STATUS_LOG(STATUS_ERROR, "invlid running ep number\n");
        return -1;
    }

    return 1;
}

int ino_register_lock(uint8_t global_epoch_idx)
{
    spin_lock(&global_epoch_ino_register_lock[global_epoch_idx]);
}

int ino_register_unlock(uint8_t global_epoch_idx)
{
    spin_unlock(&global_epoch_ino_register_lock[global_epoch_idx]);
}


int get_global_epoch(uint8_t * ep_no, struct list_head ** g_ep_head)
{
    spin_lock(&epoch_switch_spin_lock);
    if (is_valid_running_ep())
    {
        *ep_no = g_running_ep;
        *g_ep_head = &global_epoch[g_running_ep].global_ino_epoch_list;
        spin_unlock(&epoch_switch_spin_lock);
        //INFO_REPORT("get current global epoch info success, idx %d\n", g_running_ep);
        return F2FSJ_OK;
    }
    else
    {
        spin_unlock(&epoch_switch_spin_lock);
        STATUS_LOG(STATUS_ERROR, "invalid running epoch number, please check\n");
        return F2FSJ_ERROR;
    }
}

int update_ino_epoch_status(j_ino_local_epoch_t * j_ino_log_list, epoch_status_e set_status)
{
    if (!j_ino_log_list)
    {
        INFO_REPORT("invalid epoch addr\n");
    }

    j_ino_log_list->log_list_status = set_status;
    return 0;
}

//TOBE rewrite
int alloc_log_entry_memory(delta_log_t **log_entry)
{
#if ENABLE_F2FSJ
    *log_entry = kmem_cache_alloc(kmem_delta_log_slab_cache_heap, GFP_NOIO);
    if (!(*log_entry))
    {
        STATUS_LOG(STATUS_ERROR, "allocate delta log memory failed\n");
        return F2FSJ_ERROR;
    }
#else
    *log_entry = NULL;
#endif
    return F2FSJ_OK;
}

global_epoch_t *get_cur_g_running_epoch()
{
    return &global_epoch[g_running_ep];
}

struct list_head  *get_g_to_be_commited_epoch_list_head()
{
    return &g_epoch_head.to_be_commit_global_epoch_list;
}


int iterate_2_next_ep()
{
    g_epoch_seq ++;
    g_running_ep ++;
    g_running_ep = g_running_ep % MAX_GLOBAL_EP_NUM;
    if (global_epoch[g_running_ep].g_epoch_status == EPOCH_IDLE)
    {
        global_epoch[g_running_ep].g_epoch_status = EPOCH_RUNNING;
        global_epoch[g_running_ep].epoch_seq      = g_running_ep;
        //INFO_REPORT("Switch to next epoch\n");
    }
    else
    {
        INFO_REPORT("no IDLE epoch, need to wait...\n");
    }
}

int ep_switch_spin_lock()
{
    spin_lock(&epoch_switch_spin_lock);
}

int ep_switch_spin_unlock()
{
    spin_unlock(&epoch_switch_spin_lock);
}

#define J_TIMEOUT_MS (5000)
#define STEP (32)
#define J_FILE_SIZE (8192 * 16) // real size *= 4, the Nr of pages
#define J_COMMIT_SIZE (J_FILE_SIZE) 
static struct page * page_array[J_FILE_SIZE] = {NULL};
uint8_t *page_buf_array[J_FILE_SIZE] = {NULL};

int alloc_log_memory()
{
    int i = 0;
    static int is_init = 0;
    static uint64_t cnt = 0;
    static unsigned long time_stamp1 = 0;
    static unsigned long time_stamp2 = 0;
    static void * mem_ptr = NULL;
    if (!is_init)
    {
        printk("memory test init\n");
        is_init = 1;
        time_stamp1 = get_current_time_ms();
        printk("time test %lld\n", time_stamp1);
    }

    time_stamp2 = get_current_time_ms();

    // timeout Or journal file full
    if (time_stamp2 - time_stamp1 >= J_TIMEOUT_MS || ((cnt / STEP) + 1 >= J_COMMIT_SIZE))
    {
        printk("time2 - time2 %lld, used page for journal is %lld\n", time_stamp2 - time_stamp1, cnt / STEP);
        for (i = 0; i < (cnt / STEP); i++)
        {
            //kunmap_atomic(page_buf_array[i]);
            __free_page(page_array[i]);
            page_array[i] = NULL;
        }
        cnt = 0;
        time_stamp1 = get_current_time_ms();
    }
    else // others, allocate memory every (STEP) pages
    {
        cnt ++;
        if (cnt % STEP == 0)
        {
            page_array[cnt/STEP - 1] = alloc_page(GFP_KERNEL);
            //page_buf_array[cnt/STEP -1] = kmap_atomic(page_array[cnt/STEP - 1]);
        }
    }
    return 0;
}