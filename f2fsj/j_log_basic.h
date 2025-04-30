/**
 * @file j_log_basic.h
 * @author leslie.cui (10033908@github.com)
 * @brief 
 * @version 0.1
 * @date 2023-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef J_LOG_BASIC_H
#define J_LOG_BASIC_H

#include <linux/list.h>
#include <linux/types.h>
#include <linux/writeback.h>
#include "j_log_content.h"

typedef enum __j_page_extend_flag_e
{
    PG_Jdirty = PG_unevictable + 1,

}j_page_extend_flag_e;

/** Here is the step:
 *  When doing log collection
 *  1) clear_page_dirty_for_io(page)
 *  2) setJournalDirty(page)
 *  After committing:
 *  3) clearJournalDirty(page)
 *  4) SetPageDirty(page)
*/
static __always_inline void setPageJournalDirty(struct page *page)
{
    VM_BUG_ON_PAGE(PageTail(page), page);
    smp_wmb();
    set_bit(PG_Jdirty, &page->flags);
}

static __always_inline void clearPageJournalDirty(struct page *page)
{
    VM_BUG_ON_PAGE(PageTail(page), page);
    smp_wmb();
    clear_bit(PG_Jdirty, &page->flags);
}

///< define the type of journal log base on frequency
typedef enum __j_log
{
    META_DRIVED_FREQUENT    = 0, // MF
    META_DRIVED_IN_FREQUENT = 1, // MI
    DATA_DRIVED_FREQUENT    = 2, // DF
    DATA_DRIVED_IN_FREQUENT = 3  // No operations
}j_log_e;

typedef enum __j_log_priority
{
    PRIORITY_LOW  = 0,
    PRIORITY_MID  = 1,
    PRIORITY_HIGH = 2
}j_log_priority_e;

///< define log contents
typedef struct __j_log_contents
{
    union
    {
        ///< different log types
        data_write_log_t     log_data_write_df;
        read_stat_log_t      log_read_mf;
        create_log_t         log_create_mi;
        delete_log_t         log_delete_mi;
        symlink_log_t        log_symlink_mi;
        rename_log_t         log_rename_mi;
        chown_log_t          log_chown_mi;

        ///< related to directory file operations
        ///< like create()/mkdir()/symlink()/rmdir()/rename()/unlink()/link()
        j_dir_log_t          log_dir_mi;
    };
}j_log_contents_t;


#endif // !J_LOG_BASIC_H