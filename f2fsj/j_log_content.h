/**
 * @file j_log_content.h
 * @author leslie.cui (10033908@github.com)
 * @brief 
 * @version 0.1
 * @date 2023-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef J_LOG_CONTENT_H
#define J_LOG_CONTENT_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/smp.h>
#include <linux/f2fs_fs.h>

typedef enum __return_value_e
{
    F2FSJ_OK  = 0,
    F2FSJ_ERROR = -1,
    F2FSJ_FATAL = -99,
}return_value_e;

typedef enum __print_log_status_e
{
    STATUS_TRACE   = 0,
    STATUS_INFO    = 1,
    STATUS_WARNING = 2,
    STATUS_ERROR   = 3,
    STATUS_FATAL   = 4,
}print_log_status_e;

#define DEFAULT_LOG_OUTPUT_LEVEL (STATUS_TRACE)    ///< Now, default log output level is Trace

#ifndef STATUS_LOG
#define STATUS_LOG(status_level, fmt, args...)                                        \
do                                                                                  \
{                                                                                   \
    /** TRACE and INFO LOG*/                                                        \
    if (status_level <= STATUS_INFO && status_level >= DEFAULT_LOG_OUTPUT_LEVEL)    \
        {pr_info("[f2fsj-cpu-%d]\t%s:%d,\t" fmt, get_cpu(), __FUNCTION__, __LINE__, ##args);}\
    /** WARNING LOG*/                                                               \
    if (status_level == STATUS_WARNING && status_level >= DEFAULT_LOG_OUTPUT_LEVEL) \
        {pr_warn("[f2fsj-cpu-%d]\t%s:%d,\t" fmt, get_cpu(), __FUNCTION__, __LINE__, ##args);}\
    /** ERROR LOG*/                                                                 \
    if (status_level == STATUS_ERROR && status_level >= DEFAULT_LOG_OUTPUT_LEVEL)   \
        {pr_err("[f2fsj-cpu-%d]\t%s:%d,\t" fmt, get_cpu(), __FUNCTION__, __LINE__, ##args);}\
    /** FATAL LOG*/                                                                 \
    if (status_level == STATUS_FATAL && status_level >= DEFAULT_LOG_OUTPUT_LEVEL)   \
        {pr_emerg("[f2fsj-cpu-%d]\t%s:%d,\t" fmt, get_cpu(), __FUNCTION__, __LINE__, ##args);}\
} while (0);                                                                        
#endif

#define DEBUG_INFO (0)

#if DEBUG_INFO
#ifndef INFO_REPORT
#define INFO_REPORT(fmt, args...)                                              \
do                                                                             \
{                                                                              \
    if (DEFAULT_LOG_OUTPUT_LEVEL <= STATUS_INFO)                               \
        {pr_info("[f2fsj-cpu-%d]\t%s:%d,\t" fmt, get_cpu(), __FUNCTION__, __LINE__, ##args);}\
} while (0);        
#endif
#else
#define INFO_REPORT(fmt, args...)
#endif

static inline unsigned long get_current_time_ms(void)
{
    unsigned long ns;
    unsigned long ms;

    struct timespec64 cur_time;

    ktime_get_ts64(&cur_time);

    ns = timespec64_to_ns(&cur_time);
    ms = ns / 1000000;

    return ms;
}

static inline unsigned long get_current_time_ns(void)
{
    unsigned long ns;

    struct timespec64 cur_time;

    ktime_get_ts64(&cur_time);

    ns = timespec64_to_ns(&cur_time);

    return ns;
}

static inline log2(int n)
{
    int cnt = 0;
    if (n == 1)
    {
        return 0;
    }

    return 1 + log2(n >> 1);
}

///< define log type base on file operations
typedef enum __log_type
{
    ///< meta drived in-frequent log
    CREATE_LOG  = 0,
    MKDIR_LOG   = 1,
    UNLINK_LOG  = 2,
    LINK_LOG    = 3,
    RENAME_LOG  = 4,
    SYMLINK_LOG = 5,
    CHOWN_LOG   = 6,
    DIR_LOG     = 7,

    ///< meta drived frequent log
    READ_FILE_DATA_LOG    = 8,
    READ_DIR_LOG          = 9,
    STAT_LOG              = 10,

    ///< data drived frequent log
    DATA_WRITE_LOG    = 11
}log_type_e;

///< define log head
typedef struct __j_log_head
{
    log_type_e       log_type;
    //uint32_t         checksum;
    uint32_t         log_size;
}j_log_head_t;


typedef struct __j_nat_entry
{
    uint32_t j_nid;     /** node id*/
    uint32_t j_ino;     /** inode number related to this node, if nid == ino, it means it's a inode page*/
    uint32_t j_node_lba;

    /** 
     *  To locate nat page: 
     *
     *  f2fs_get_meta_page(sbi, current_nat_addr(sbi, nid));
     */

}j_nat_entry_t;


///< sit log is for one segment (like nr of valid blocks and also bitmap)
typedef struct __sit_log
{
    /** 
     *  To locate sit page: 
     *
     *  f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
     */
    uint32_t j_sit_cur_seg;

    uint32_t segno;
    uint32_t j_nr_valid_block;
    uint32_t bitmap_size;     // we should copy this bitmap into log
    char     * j_bitmap;
}j_sit_log_t;

///< ssa: reverse mapping (LBA -> nid) compared with NAT (nid -> LBA)
///< Usage when GC, we move datas to new blk, so need to modify old LBA to new LBA. So need to use nid to locate
///< corresponding old LBA entry
typedef struct __ssa_entry_log
{
    /** 
     *  To locate ssa page: 
     *
     *  f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
     */
    uint32_t j_cur_seg_no;

    uint16_t blk_ofs_in_inode;
    uint32_t block_addr;
    uint32_t j_nid;
}j_ssa_log_entry_t;

typedef struct __j_new_inode_log
{
    __le16 i_mode;          /* file mode */
    __u8 i_advise;          /* file hints */
    __u8 i_inline;          /* file inline flags */
    __le32 i_uid;           /* user ID */
    __le32 i_gid;           /* group ID */
    __le32 i_links;         /* links count */
    __le64 i_size;          /* file size in bytes */
    __le64 i_blocks;        /* file size in blocks */
    __le64 i_atime;         /* access time */
    __le64 i_ctime;         /* change time */
    __le64 i_mtime;         /* modification time */
    __le32 i_atime_nsec;    /* access time in nano scale */
    __le32 i_ctime_nsec;    /* change time in nano scale */
    __le32 i_mtime_nsec;    /* modification time in nano scale */
    __le32 i_generation;    /* file version (for NFS) */
    union {
    __le32 i_current_depth;	/* only for directory depth */
    __le16 i_gc_failures;	/*
    * # of gc failures on pinned file.
    * only for regular files.
    */
    };
    __le32 i_xattr_nid;     /* nid to save xattr */
    __le32 i_flags;         /* file attributes */
    __le32 i_pino;          /* parent inode number */
    __le32 i_ino;           /* itself inode number, useful for apply*/
    __le32 i_namelen;       /* file name length */
    __u8 i_name[16]; /* file name for SPOR *//* This guy is 255 bytes but seems would not participant in journal fopen*/
    __u8 i_dir_level;       /* dentry_level for large dir */

    // New inode doesn't need extent
    //struct f2fs_extent i_ext;   /* caching a largest extent 12 bytes*/
}j_new_inode_log_t;

typedef struct __parent_inode_basic_attr
{
    //parent inode attr
    struct timespec64 c_time;
    struct timespec64 m_time;
    size_t   i_size;
}parent_inode_basic_attr_t;
/*******************************************FIle operation logs***********************************************/

typedef struct __data_write_log  ///< data writeback
{
    j_log_head_t log_header;

    ///< if inline data stored in inode
    uint8_t is_inline_data;
    ///< use ino num can find inode page by NAT
    uint32_t ino_num;
    ///< offset of current page
    uint32_t page_ofs;
    ///< current file size
    uint64_t file_size;

    ///< cause data block address is preallocated in f2fs_write_iter(), which means that nat/sit/ssa stores the newest version
    ///< data pages will be writeback firstly when committing logs, so it's necessary to record nat/sit/ssa entry
    ///< if we do not record nat/sit/ssa, we cannot recover the datas because of mapping lost of datas
    j_nat_entry_t     nat_en_log;
    j_sit_log_t       sit_log;
    j_ssa_log_entry_t ssa_en_log;

    // Thinking: whether need record LBA of data page?
    // I think there is no need to record dirty data pages cause when committing logs and writeback dirty pages
    // we just invoke writepage() and use page_ofs to find corresponding page 
}data_write_log_t;

typedef struct __read_stat_log  ///< read file or listdir directory
{
    j_log_head_t log_header;

    uint32_t ino_num;
    time64_t access_time_s;
    long     access_time_ns;
}read_stat_log_t;

typedef struct __create_log    ///< create file or directory
{
    j_log_head_t log_header;

    ///< refer struct f2fs_inode in f2fs_fs.h
    ///< And I think it is better to customize some attributes out
    j_new_inode_log_t j_new_ino_log;

    ///< Correct thinking (now)
    /**
     * Although new block address is allocated until checkpoint (by invoking writepages)
     * When we do recovery, we need to know the segment information at that time when crash happened
     * But for newly create file/directory, no on-disk FS metadata, so no need to record in-mem FS metadata
    */
#if 0
    j_nat_entry_t      nat_log;
    j_sit_log_t        sit_log;
    j_ssa_log_entry_t  ssa_en_log;
#endif
}create_log_t;

/**
 * @brief TODO, may not useful for recovering, creat_log is enough...
 * refer to struct f2fs_dir_entry and struct f2fs_dentry_block
 *        This log will be added into parent's inode local log list
 */
typedef struct __j_dir_log
{
    j_log_head_t log_header;

    ///< This flag identify where to recover new directory entry. 
    ///< we can utilize NAT (by nid) to find original inode page or regular data block when doing recovery
    uint8_t inline_dir_or_regular;

    // dir entry, refer struct f2fs_dir_entry
    f2fs_hash_t hash_code; //Can be recovered by file_name
    uint32_t ino;
    uint32_t parent_ino;
    uint16_t name_len;
    uint8_t  file_type;
    uint8_t  file_name[16];

    ///< Still need to record bitmap?
    ///< I think if we have dir entry log, we are able to recover by refering f2fs_update_dentry()
    ///< So, no need to record dir bitmap now
    // uint8_t  each_dentry_blk_bitmap[SIZE_OF_DENTRY_BITMAP];

    ///< new dir also change some parent inode attributes 
    parent_inode_basic_attr_t pino_basic_attr;

}j_dir_log_t;


/**
 * @brief This log should insert into parent ino log list
 *        if n_link == 0, the inode page and data blks will be deleted
 *        if n_link > 0, just one dir entry need to be deleted
 *        
 *        Need to delete inode and delete directory entry
 */
typedef struct __delete_log    ///< delete file or directory
{
    j_log_head_t log_header;

    uint32_t ino_num;
    uint32_t parent_ino_num;
    uint32_t ino_nlink;
    char     file_name[16]; //TODO, filename maybe not necessary cause we can find dir entry by inode number
    uint8_t  i_namelen;

    /**
     * Thinking: if no need to record nat/sit/ssa log?
     * When n_link == 0, it means
     * 1) one directory entry need to be deleted from parent inode
     *
     * 2) the nat entry of this inode page should be deleted and the corresponding sit/ssa (node and data) also need to be updated
     * --> recovery is enough based on log (ino, pino, ino_nlink):
     *     When crash happens before checkpoint finished (before successfully apply changes),
     *     we can find original inode page by NAT, and update sit/ssa. But if the nat entry is deleted but not the sit or ssa?
     *     So I think is may still need to record nat entry
     *
     * When n_link >= 1, it means the just one directory entry need to be deleted
     * --> recovery is enough based on log (ino, pino, ino_nlink), we just need to find original directory block
     *     and complete the deletion
     *
     */
    j_nat_entry_t     nat_log;
    // j_sit_log_t       sit_log;
    // j_ssa_log_entry_t ssa_en_log;

}delete_log_t;

/**
 * @brief for symlink, firstly a new inode will be created and write the symlink contents
 *        Then, a directory entry will be added into parent inode local log list
 *        ---> j_dir_log_t for parent log list
 */
typedef struct __symlink_log   ///< file symlink
{
    j_log_head_t log_header;

    ///< firstly, new inode contents should be recorded
    j_new_inode_log_t j_new_inode;

    ///< symlink contents
    uint16_t j_symlink_name_len;
    char j_symlink_name[32];

    ///< cause block address of new inode will be allocated during writepages(NODE)
    ///< so no need to record nat/sit/ssa (no apply means no need to record) 
    j_nat_entry_t     nat_en_log;
    j_sit_log_t       sit_log;
    j_ssa_log_entry_t ssa_en_log;
}symlink_log_t;

/**
 * @brief for rename, firstly an old directory entry should be deleted from old parent directory block
 *        -> a j_dir_log_t for old parent log list
 *        Then, a new directory entry would be added into new parent directory block
 *        -> a j_dir_log_t for new parent log list
 *        lastly, this inode also need to update some attributes (time)
 */
typedef struct __rename_log    ///< rename file or directory
{
    j_new_inode_log_t j_inode;

}rename_log_t;

typedef struct __chown_log    ///< change the owner
{
    uint32_t ino_num;
    kuid_t user_id;
    kgid_t group_id;
    loff_t i_size;
    struct timespec64   mtime;
    struct timespec64   ctime;
}chown_log_t;

#endif // !J_LOG_CONTENT_H