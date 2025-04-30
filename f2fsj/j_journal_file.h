/**
 * @file j_journal_file.h
 * @author leslie.cui (10033908@github.com)
 * @brief This file is to define some functions to operate journal file. such as
 *        block allocation in journal file, journal file recording...
 *
 *        I.e., journal file infor is stored in struct f2fs_sb_info
 *
 * @version 0.1
 * @date 2023-10
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef _J_JOURNAL_FILE_H_
#define _J_JOURNAL_FILE_H_
#include "f2fs.h"
#include "j_log_content.h"

#define JOURNAL_BLOCK_SIZE (PAGE_SIZE)

// The last 256 MB is journal file
#define JOURNAL_FILE_SIZE (256 * 1024 * 1024)

// How many blocks does each journal file contains
#define JUORNAL_FILE_LENTH (JOURNAL_FILE_SIZE / JOURNAL_BLOCK_SIZE)

// The journal SB block, hardcode now
#define F2FSJ_SB_BLOCK1_ADDR (3866624) // 6232476, orginal 256GB SSD config

// Journal file start addr, TODO-also hardcode
#define JOURNAL_FILE_START_ADDR (F2FSJ_SB_BLOCK1_ADDR + 1)

// seperate journal file into 4 small files, each is 64MB
#define JOURNAL_BLK_PER_SMALL_FILE (256 * 1024 * 1024 / 4096)

#define NR_JOUNRAL_SMALL_FILE (1)
#define JORNAL_FILE_0_START_BLK (JOURNAL_FILE_START_ADDR + JOURNAL_BLK_PER_SMALL_FILE * 0)
#define JORNAL_FILE_1_START_BLK (JOURNAL_FILE_START_ADDR + JOURNAL_BLK_PER_SMALL_FILE * 1)
#define JORNAL_FILE_2_START_BLK (JOURNAL_FILE_START_ADDR + JOURNAL_BLK_PER_SMALL_FILE * 2)
#define JORNAL_FILE_3_START_BLK (JOURNAL_FILE_START_ADDR + JOURNAL_BLK_PER_SMALL_FILE * 3)

//#define J_LOG_ENTRY_SIZE ((128 + sizeof(uint32_t) + sizeof(struct list_head)))
#define J_LOG_ENTRY_SIZE (128)
#define J_LOG_ENTRY_PER_FILE (256 * 1024 * 1024 / J_LOG_ENTRY_SIZE)
#define J_LOG_ENTRY_PER_BLOCK (4096 / J_LOG_ENTRY_SIZE)

// log entry <-> blk LBA
#define J_LOG_ENTRY_TO_BLK(__log_entry_idx) \
    (((uint32_t)(__log_entry_idx)) / J_LOG_ENTRY_PER_BLOCK)

// log entry <-> offset in blk
#define J_LOG_ENTRY_TO_BLK_OFFSET(__log_entry_idx) \
    (((uint32_t)(__log_entry_idx)) % J_LOG_ENTRY_PER_BLOCK)

// log entry <-> blk LBA + offset
#define J_LOG_ENTRY_ADDR(__j_file_mapping, __log_entry_idx) \
    ((__j_file_mapping)->j_pages_buf[J_LOG_ENTRY_TO_BLK(__log_entry_idx)]) \
    + (J_LOG_ENTRY_TO_BLK_OFFSET(__log_entry_idx) * (J_LOG_ENTRY_SIZE))


// bi_sector is aligned with 512 Bytes, but the LBA is aligned with 4096 bytes
/* 3 --> 4096 / 512 = 2^3; sector >> 3 = LBA; LBA << 3 = sector
 * LBA 0 <-> sector 0  (0 << 3)
 * LBA 1 <-> sector 8  (1 << 3)
 * LBA 2 <-> sector 16 (2 << 3)
 * ...
*/
#define J_SECTOR_FROM_BLOCK(blk_addr)					\
	(((sector_t)blk_addr) << F2FS_LOG_SECTORS_PER_BLOCK)
#define J_SECTOR_TO_BLOCK(sectors)					\
	((sectors) >> F2FS_LOG_SECTORS_PER_BLOCK)


/** Journal file layout
    ************************************
    *  1st      *                      *
    *  journal  *  1st journal file    *
    *  SB       *                      *
    ************************************
 */

typedef enum __j_file_range_e
{
    F2FSJ_J_FILE_0 = 0,
    F2FSJ_J_FILE_1 = 1,
    F2FSJ_J_FILE_2 = 2,
    F2FSJ_J_FILE_3 = 3,

    NR_F2FSJ_J_FILE = 4
}j_file_range_e;

typedef enum __j_magic_e
{
    JOURNAL_FILE_MAGIC_NUMBER = 0xCDEF,
}j_magic_number_e;

typedef enum __j_file_e
{
    J_FILE_IDLE = 0,
    J_FILE_INUSE = 1,
    J_WHOLE_FILE_WAIT_COMMIT = 2,
    J_PARTIAL_FILE_WAIT_COMMIT = 3,
}j_file_state_e;

/**
 * @brief We make 2 journal files on disk as round-robin way, make sure that at least one journal file is valid
 *
 */
typedef struct __j_jsb_info
{
    ///< basic info of journal file
    uint32_t j_magic_num;
    uint32_t j_start_addr;
    uint32_t j_file_size;

    uint32_t j_current_small_file;  // 0
    uint32_t j_current_free_log_entry;  // 0-J_LOG_ENTRY_PER_FILE

    spinlock_t j_file_memap_lock;
}j_jsb_info_t;

typedef struct __j_file_mapping_t
{
    uint8_t j_file_state;
    uint32_t j_cur_file; ///< journal file index
    uint32_t j_cur_log_entry_idx; ///< current allocated log entry
                                  ///< current log entry / J_LOG_ENTRY_PER_BLOCK = journal block

    uint32_t j_cur_file_start_blk; ///< the start physical block address of one journal file
    uint32_t j_cur_file_end_blk;   ///< the end physical block address of one journal file
    uint32_t j_cur_file_first_inused_blk; ///< the first used block, write journal from this block

    struct page *j_pages[JOURNAL_BLK_PER_SMALL_FILE]; ///< journal file pages
    char *j_pages_buf[JOURNAL_BLK_PER_SMALL_FILE]; ///< virtual memory address of journal file pages
}j_file_mapping_t;

typedef struct __j_on_disk_file_info_t
{
    uint32_t total_file_size;
    uint32_t used_file_size;
}j_on_disk_file_into_t;

typedef struct __j_log_entry_t
{
    uint32_t log_entry_idx;
    struct list_head log_node;
    uint8_t *log_entry_addr;
}j_log_entry_t;


/**
 * @brief This function is invoked during f2fs_mount(), read journal file metadata from disk
 *        TODO, journal file structure
 * @param[in] jsb_in
 * @return
 */
int init_journal_file_info(struct super_block *sb);

/**
 * @brief Map the journal file
 * 
 * @return
 */
int mmap_journal_file();

int j_alloc_log_entry(log_type_e log_type, j_log_entry_t **log_entry);

int j_free_log_entry(j_log_entry_t * log_entry);

int alloc_log_entry_test(log_type_e log_type);

/**
 * @brief Get the current jouranl file object by comparing j_version
 * 
 * @return j_jsb_info_t* 
 */
j_jsb_info_t* get_current_jouranl_sb();

/**
 * @brief reserve continous free blocks for future journal logs (aggregated in page)
 *
 * @param[in] how many 4KB blocks are needed
 * @param[out] The first blk addr of 
 * @return
 */
int reserve_free_journal_space(uint32_t nr_reserve_blk, uint32_t *start_blk_addr);

/**
 * @brief allocate a bio struct
 * 
 * @param b_out
 * @return int
 */
int j_alloc_bio_write(struct f2fs_sb_info *sbi, struct bio **b_out, uint32_t start_blk_addr, int pre_alloc_iovecs);


int j_alloc_bio_read(struct f2fs_sb_info *sbi, struct bio **b_out, uint32_t start_blk_addr);

/**
 * @brief This function is to aggregate pages (logs are already in pages) into bio struct
 *
 *        Notice that one bio can contains up-to 256 continous pages
 * @return int 
 */

int add_journal_page_2_bio(struct page *p_log, struct bio *b);

/**
 * @brief This function need to iterate all DF logs and writeback data pages
 *        Now, I think we can invoke inode->writepages() just like jbd2 does
 *
 * @param f2fs_i
 * @return int
 */
int ep_commit_writeback_data_pages(struct f2fs_inode_info *f2fs_i);

/**
 * @brief Submit bio
 *
 * @param bio
 * @return int
 */
int write_current_mmap_j_file(struct f2fs_sb_info *sbi);


int get_on_disk_free_journal_space();

int reset_on_disk_journal_space_info();
/**
 * @brief Writeback inode page also the node page
 * 
 * @param sbi 
 * @param inode 
 * @return int 
 */
int j_apply_NODE(struct f2fs_sb_info *sbi, uint32_t nid);

/**
 * @brief Writeback META page
 * 
 * @return int
 */
int j_apply_META(struct f2fs_sb_info *sbi, uint32_t ino_num, uint32_t node_id, uint32_t segno);

/**
 * @brief before recovery, we firstly read journal file
 * 
 * @param sb
 * @return int 
 */
int recover_read_journal(struct super_block *sb);

/**
 * @brief iterate journal file and recover corresponding to log-type
 * 
 * @param sb 
 * @param j_file_idx 
 * @return int 
 */
int iterate_journal(struct super_block *sb, int j_file_idx);

int is_invalid_log_type(log_type_e log_type);

/**
 * @brief recover file system by journal, should be invoked in the critical path of f2fs_mount()
 * 
 * @param latest_j_file, the latest journal file
 * @return int
 */
int do_recover_from_journal(struct super_block *sb, log_type_e log_type, uint8_t * log_content);

int clear_journal_file_after_recovery(struct super_block *sb);

#endif

