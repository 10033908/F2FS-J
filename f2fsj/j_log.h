/**
 * @file j_log.h
 * @author leslie.cui (10033908@github.com)
 * @brief
 * @version 0.1
 * @date 2023-07
 *
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef J_LOG_H
#define J_LOG_H

#include "j_log_basic.h"

///< define delta log struct
typedef struct __delta_log
{
    uint32_t nr_logs;

    ///< log head
    j_log_head_t delta_log_head;

    ///< log contents
    j_log_contents_t delta_log_contents;

    ///< log list is for collecting same type log
    struct list_head delta_log_list;
}delta_log_t;

///< define global log list
typedef struct __global_log_list
{
    ///< number of file which produce logs
    uint32_t nr_logged_file;

    ///< global list to link with inode
    struct list_head global_log_list;
}global_log_list_t;
#endif // !J_LOG_H