/**
 * @file j_journal_file.c
 * @author leslie.cui (10033908@github.com)
 * @brief implementation for journal file operations
 * @version 0.1
 * @date 2023-10
 *
 * @copyright Copyright (c) 2023
 */
#include "j_journal_file.h"
#include "node.h"
#include "segment.h"
#include "j_recovery.h"
#include <linux/stat.h>

// alloc memory for log_entry_info, the memory for log contents is allocated from mmaped journal file
static struct kmem_cache* j_log_entry_info_slab = NULL;
static j_file_mapping_t j_file_mmap[4] = {0};
static j_jsb_info_t g_jsb = {0};
static j_on_disk_file_into_t g_on_disk_j_file = {0};

int init_journal_file_info(struct super_block *sb)
{
    j_jsb_info_t * j_sb_blk_ptr = NULL;
    struct buffer_head *bh = NULL;
    bh = sb_bread(sb, F2FSJ_SB_BLOCK1_ADDR);
    if (!bh)
    {
        INFO_REPORT("sb_bread journal file superblock failed\n");
        return F2FSJ_ERROR;
    }
    else
    {
        j_sb_blk_ptr = (j_jsb_info_t *)bh->b_data;

        // journal superblock
        g_jsb.j_magic_num    = j_sb_blk_ptr->j_magic_num;
        g_jsb.j_start_addr   = j_sb_blk_ptr->j_start_addr;
        g_jsb.j_file_size    = j_sb_blk_ptr->j_file_size;
    }

    if (g_jsb.j_magic_num != JOURNAL_FILE_MAGIC_NUMBER)
    {
        // init journal file superblock
        INFO_REPORT("Init journal file superblock\n");
        j_sb_blk_ptr->j_magic_num    = JOURNAL_FILE_MAGIC_NUMBER;
        j_sb_blk_ptr->j_start_addr   = F2FSJ_SB_BLOCK1_ADDR;
        j_sb_blk_ptr->j_file_size    = JOURNAL_FILE_SIZE;
        j_sb_blk_ptr->j_current_small_file = F2FSJ_J_FILE_0;
        j_sb_blk_ptr->j_current_free_log_entry = JOURNAL_BLK_PER_SMALL_FILE;
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        INFO_REPORT("Init journal file superblock end");
    }
    else
    {
        INFO_REPORT("Journal file magic number is valid - %x\n", g_jsb.j_magic_num);
    }

    // init spin lock for log entry allocation
    spin_lock_init(&g_jsb.j_file_memap_lock);

    // init slab for log entry info
    j_log_entry_info_slab = kmem_cache_create("f2fsj_log_entry_info_cache_heap", sizeof(j_log_entry_t), 0,
                                                        SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
    if (!j_log_entry_info_slab)
    {
        STATUS_LOG(STATUS_FATAL, "init log entry info cache heap fail\n");
        return F2FSJ_ERROR;
    }

    // memory map journal file
    mmap_journal_file();
    INFO_REPORT("memory map journal file\n");


    // read logs from journal file
    INFO_REPORT("begin to read journal file\n");
    recover_read_journal(sb);
    INFO_REPORT("read journal file end\n");

    // init in-memory journal file info
    g_jsb.j_current_small_file     = F2FSJ_J_FILE_0;
    g_jsb.j_current_free_log_entry = JOURNAL_BLK_PER_SMALL_FILE;

    // init on-disk journal file info
    g_on_disk_j_file.total_file_size = JOURNAL_FILE_SIZE; // MB
    g_on_disk_j_file.used_file_size  = 0;

    return F2FSJ_OK;
}

int mmap_journal_file()
{
    int i, j;
    struct page *p = NULL;

    for (i = 0; i < NR_JOUNRAL_SMALL_FILE; i++)
    {
        j_file_mmap[i].j_file_state = J_FILE_IDLE;
        j_file_mmap[i].j_cur_file = i;
        j_file_mmap[i].j_cur_log_entry_idx = 0;
        for (j = 0; j < JOURNAL_BLK_PER_SMALL_FILE; j++)
        {
            p = alloc_page(GFP_KERNEL); //GFP_KERNEL
            if (p)
            {
                j_file_mmap[i].j_pages[j] = p;
                j_file_mmap[i].j_pages_buf[j] = page_address(p);
                if (j == 0)
                    INFO_REPORT("jfile-%d, %d page addr is %p\n", i, j, j_file_mmap[i].j_pages_buf[j]);
            }
            else
            {
                STATUS_LOG(STATUS_ERROR, "memmap journal file, alloc page fail\n");
            }
        }
    }

    j_file_mmap[0].j_cur_file_start_blk = JORNAL_FILE_0_START_BLK;

    j_file_mmap[0].j_cur_file_end_blk = JORNAL_FILE_0_START_BLK + JOURNAL_BLK_PER_SMALL_FILE;

    j_file_mmap[0].j_cur_file_first_inused_blk = JORNAL_FILE_0_START_BLK;

}

int j_alloc_log_entry(log_type_e log_type, j_log_entry_t **log_entry)
{
    j_file_mapping_t *j_f_mapping = NULL;
    uint32_t log_entry_idx = 0;

    *log_entry = NULL;
    *log_entry = kmem_cache_alloc(j_log_entry_info_slab, GFP_NOIO);
    if (*log_entry == NULL)
    {
        INFO_REPORT("allocate memory for log entry info failed\n");
        return F2FSJ_ERROR;
    }

    //INFO_REPORT("allocate log entry addr is %p\n", *log_entry );

    spin_lock(&g_jsb.j_file_memap_lock);

    if (j_file_mmap[g_jsb.j_current_small_file].j_file_state == J_FILE_IDLE)
    {
        INFO_REPORT("IDLE j_file[%d] is in-used\n", g_jsb.j_current_small_file);
        j_file_mmap[g_jsb.j_current_small_file].j_file_state = J_FILE_INUSE;
    }
    else if (j_file_mmap[g_jsb.j_current_small_file].j_file_state == J_WHOLE_FILE_WAIT_COMMIT)
    {
        INFO_REPORT("current j_file is whole wait for commit, cannot alloc log entry\n");
        return -1;
    }

    j_f_mapping = &j_file_mmap[g_jsb.j_current_small_file];
    log_entry_idx  = j_file_mmap[g_jsb.j_current_small_file].j_cur_log_entry_idx;

    //*log_entry = J_LOG_ENTRY_ADDR(j_f_mapping, log_entry_idx);
    (*log_entry)->log_entry_idx  = j_file_mmap[g_jsb.j_current_small_file].j_cur_log_entry_idx;
    //INIT_LIST_HEAD(&((*log_entry)->log_node));
    (*log_entry)->log_entry_addr = J_LOG_ENTRY_ADDR(j_f_mapping, log_entry_idx);


    j_file_mmap[g_jsb.j_current_small_file].j_cur_log_entry_idx++;
    if (j_file_mmap[g_jsb.j_current_small_file].j_cur_log_entry_idx == J_LOG_ENTRY_PER_FILE)
    {
        INFO_REPORT("No free entry on J_file[%d], need GC journal file\n", g_jsb.j_current_small_file);
        // current journal file change to WAIT_COMMIT
        j_file_mmap[g_jsb.j_current_small_file].j_file_state = J_WHOLE_FILE_WAIT_COMMIT;

        // Wait journal apply, invoke journal ckpt here; TODO

        // Then, journal file can reuse
        j_file_mmap[g_jsb.j_current_small_file].j_cur_log_entry_idx = 0;
        j_file_mmap[g_jsb.j_current_small_file].j_file_state = J_FILE_INUSE;
        j_file_mmap[g_jsb.j_current_small_file].j_cur_file_first_inused_blk = j_file_mmap[g_jsb.j_current_small_file].j_cur_file_start_blk;
        INFO_REPORT("reuse j_file[%d], start in-used\n", g_jsb.j_current_small_file);
    }
    else
    {
        j_file_mmap[g_jsb.j_current_small_file].j_file_state = J_PARTIAL_FILE_WAIT_COMMIT;
    }

    spin_unlock(&g_jsb.j_file_memap_lock);
    return F2FSJ_OK;
}

int j_free_log_entry(j_log_entry_t * log_entry)
{
    if (log_entry)
    {
        kmem_cache_free(j_log_entry_info_slab, log_entry);
    }
    else
    {
        INFO_REPORT("NULL log entry, cannot free\n");
        return F2FSJ_ERROR;
    }
    return F2FSJ_OK;
}


static g_whole_journal_size = 0;
int alloc_log_entry_test(log_type_e log_type)
{
    if (log_type == CREATE_LOG)
    {
        g_whole_journal_size += 128;
    }
    else if (log_type == MKDIR_LOG)
    {
        g_whole_journal_size += 128;
    }
    else if (log_type == UNLINK_LOG)
    {
        g_whole_journal_size += 64;
    }
    else if (log_type == DATA_WRITE_LOG)
    {
        g_whole_journal_size += 32;
    }

    return 0;
}


j_jsb_info_t* get_current_jouranl_sb()
{
    return &g_jsb;
}

int get_on_disk_free_journal_space()
{
    if (g_on_disk_j_file.total_file_size >= g_on_disk_j_file.used_file_size)
    {
        return g_on_disk_j_file.total_file_size - g_on_disk_j_file.used_file_size;
    }
    else
    {
        INFO_REPORT("Used j_file space > total j_file space, pls check\n");
        return -1;
    }
}

int reset_on_disk_journal_space_info()
{
    g_on_disk_j_file.used_file_size = 0;
    INFO_REPORT("journal checkpoint reset on-disk journal file size\n");
}

// This function need to be re-construct TODO!!!
int reserve_free_journal_space(uint32_t nr_reserve_blk, uint32_t *start_blk_addr)
{
    j_jsb_info_t *cur_jsb  = NULL;

    cur_jsb = get_current_jouranl_sb();
    if (cur_jsb)
    {
        if (cur_jsb->j_current_free_log_entry + nr_reserve_blk <= JOURNAL_BLK_PER_SMALL_FILE)
        {
            *start_blk_addr = cur_jsb->j_current_free_log_entry;
            cur_jsb->j_current_free_log_entry += nr_reserve_blk;
        }
    }
    else
    {
        STATUS_LOG(STATUS_ERROR, "get current journal sb fail\n");
    }

}

static void j_write_end_io(struct bio *bio)
{
    struct f2fs_sb_info *sbi;
    struct bio_vec *bvec;
    struct bvec_iter_all iter_all;

    sbi = bio->bi_private;

    // ring buffer, no need to free
#if 0
    bio_for_each_segment_all(bvec, bio, iter_all)
    {
        struct page *page = bvec->bv_page;

        // free pages
        __free_page(page);
        if (unlikely(bio->bi_status))
        {
            STATUS_LOG(STATUS_ERROR, "Journal IO happens err\n");
        }
    }
#endif
    static int io_cnt = 0;
    if (io_cnt ++ < 5)
    {
        INFO_REPORT("Journal file complete one bio\n");
    }


    bio_put(bio);
}

static void j_read_end_io(struct bio *bio)
{
    struct f2fs_sb_info *sbi;
    struct bio_vec *bvec;
    struct bvec_iter_all iter_all;

    sbi = bio->bi_private;

    // ring buffer, no need to free
#if 0
    bio_for_each_segment_all(bvec, bio, iter_all)
    {
        struct page *page = bvec->bv_page;

        // free pages
        __free_page(page);
        if (unlikely(bio->bi_status))
        {
            STATUS_LOG(STATUS_ERROR, "Journal IO happens err\n");
        }
    }
#endif
    static int io_cnt = 0;
    if (io_cnt ++ < 5)
    {
        INFO_REPORT("Journal file complete one bio\n");
    }


    bio_put(bio);
}



int j_alloc_bio_write(struct f2fs_sb_info *sbi, struct bio **b_out, uint32_t start_blk_addr, int pre_alloc_iovecs)
{
    struct bio *b = NULL;

    struct block_device *bdev = sbi->sb->s_bdev;

    b = bio_alloc(GFP_KERNEL, pre_alloc_iovecs); // The second parameter is num of iovecs to pre-allocated
    if (!b)
    {
        STATUS_LOG(STATUS_ERROR, "bio allocate fail\n");
        return F2FSJ_ERROR;
    }
    else
    {
        bio_set_dev(b, bdev);
        bio_set_op_attrs(b, REQ_OP_WRITE, REQ_SYNC);
        b->bi_end_io = j_write_end_io;
        b->bi_private = sbi;
        b->bi_iter.bi_sector = J_SECTOR_FROM_BLOCK(start_blk_addr);
        *b_out = b;
    }

    return F2FSJ_OK;
}

int j_alloc_bio_read(struct f2fs_sb_info *sbi, struct bio **b_out, uint32_t start_blk_addr)
{
    struct bio *b = NULL;

    struct block_device *bdev = sbi->sb->s_bdev;

    b = bio_alloc(GFP_KERNEL, 256); // The second parameter is num of iovecs to pre-allocated
    if (!b)
    {
        STATUS_LOG(STATUS_ERROR, "bio allocate fail\n");
        return F2FSJ_ERROR;
    }
    else
    {
        bio_set_dev(b, bdev);
        bio_set_op_attrs(b, REQ_OP_READ, REQ_SYNC);
        b->bi_end_io = j_read_end_io;
        b->bi_private = sbi;
        b->bi_iter.bi_sector = J_SECTOR_FROM_BLOCK(start_blk_addr);
        *b_out = b;
    }

    return F2FSJ_OK;
}

int add_journal_page_2_bio(struct page *p_log, struct bio *b)
{
    // add a page into bio
    if (bio_add_page(b, p_log, JOURNAL_BLOCK_SIZE, 0) != JOURNAL_BLOCK_SIZE)
    {
        STATUS_LOG(STATUS_ERROR, "add page into bio err\n");
        return F2FSJ_ERROR;
    }
    return F2FSJ_OK;
}

int write_current_mmap_j_file(struct f2fs_sb_info *sbi)
{
    struct bio *b = NULL;
    // find journal file tagged with J_WHOLE_FILE_WAIT_COMMIT
    int i = 0, j = 0, nr_page = 0;
    uint32_t start_blk = 0;
    uint32_t end_blk = 0;
    uint32_t total_pages = 0;
    uint32_t cur_log_entry_idx = 0;

    spin_lock(&g_jsb.j_file_memap_lock);
    cur_log_entry_idx = j_file_mmap[0].j_cur_log_entry_idx;
    spin_unlock(&g_jsb.j_file_memap_lock);

    for (i = 0; i < NR_JOUNRAL_SMALL_FILE; i++)
    {
        start_blk = j_file_mmap[i].j_cur_file_first_inused_blk; // first inused blk_lba
        end_blk = J_LOG_ENTRY_TO_BLK(cur_log_entry_idx) + j_file_mmap[i].j_cur_file_start_blk; // last inused blk_lba
        total_pages = end_blk - start_blk + 1;

        j_alloc_bio_write(sbi, &b, start_blk, 256);
        if (j_file_mmap[i].j_file_state == J_WHOLE_FILE_WAIT_COMMIT
         || j_file_mmap[i].j_file_state == J_PARTIAL_FILE_WAIT_COMMIT)
        {
            INFO_REPORT("Journal file %d is being written to disk, state is %d, pages num is %d, start-[%d], end-[%d]\n",
                         i, j_file_mmap[i].j_file_state, end_blk - start_blk + 1, start_blk, end_blk);

            for (j = 0; j <= end_blk - start_blk; j ++)
            {
                // all pages are added into bio
                if (nr_page == total_pages)
                {
                    // write the last pages
                    //submit_bio_wait(b);
                    submit_bio(b);
                    break;
                }

                //each bio can contain 256 pages
                if (nr_page % 256 == 0 && nr_page != 0)
                {
                    submit_bio(b); // write middle 256 pages and allocate a new bio
                    j_alloc_bio_write(sbi, &b, start_blk, 256);
                }

                // add page into bio
                add_journal_page_2_bio(j_file_mmap[i].j_pages[j], b);
                nr_page ++;
                start_blk ++;
            }

            // update first in-used journal block
            j_file_mmap[i].j_cur_file_first_inused_blk = end_blk;

            //update on-disk j_file info
            g_on_disk_j_file.used_file_size += (end_blk - start_blk + 1) * (PAGE_SIZE);
        }
    }

}

char zero_page[4096] = {0};
int clear_journal_file_after_recovery(struct super_block *sb)
{
    struct bio *b = NULL;
    struct page * j_p = NULL;
    uint8_t * j_p_b = NULL;


    int i = 0, j = 0, nr_page = 0;
    uint32_t start_blk = 0;
    uint32_t end_blk = 0;

    for (i = 0; i < NR_JOUNRAL_SMALL_FILE; i++)
    {
        // clear mmap journal page contents to 0
        start_blk = j_file_mmap[i].j_cur_file_start_blk;
        end_blk = j_file_mmap[i].j_cur_file_end_blk;
        nr_page = 0;
        j_alloc_bio_write(F2FS_SB(sb), &b, start_blk, 256);
        for (j = 0; j <= JOURNAL_BLK_PER_SMALL_FILE; j++)
        {
            if (nr_page % 256 == 0 && nr_page != 0)
            {
                if (nr_page != JOURNAL_BLK_PER_SMALL_FILE)
                {
                    submit_bio(b);
                }
                else
                {
                    submit_bio_wait(b);
                    break;
                }

                if (nr_page != JOURNAL_BLK_PER_SMALL_FILE)
                {
                    j_alloc_bio_write(F2FS_SB(sb), &b, start_blk, 256);
                }
            }
            j_p = j_file_mmap[i].j_pages[j];
            j_p_b = j_file_mmap[i].j_pages_buf[j];
            //INFO_REPORT("file-%d, %d page addr is %p, page_addr %p\n", i, j, j_p_b, page_address(j_p));
            memzero_page(j_p, 0, JOURNAL_BLOCK_SIZE);
            //memcpy(j_p_b, &zero_page, PAGE_SIZE);
            add_journal_page_2_bio(j_p, b);
            nr_page ++;
            start_blk ++;
        }
        INFO_REPORT("Clear %d-th j-file with %d pages\n", i, j);
    }
}

int recover_read_journal(struct super_block *sb)
{
    struct bio *b = NULL;
    // find journal file tagged with J_WHOLE_FILE_WAIT_COMMIT
    int i = 0, j = 0, nr_page = 0;
    uint32_t total_pages = 0;
    uint32_t start_blk = 0;
    uint32_t end_blk = 0;
    uint32_t start_page_idx = 0;
    int ret = F2FSJ_OK;

    // Only read and do not change mmaped journal file status
    //for (i = 0; i < NR_JOUNRAL_SMALL_FILE; i++)
    for (i = 0; i < 1; i++)
    {
        start_blk = j_file_mmap[i].j_cur_file_start_blk;
        end_blk = j_file_mmap[i].j_cur_file_end_blk;
        total_pages = end_blk - start_blk;
        start_page_idx = 0;
        nr_page = 0;

        j_alloc_bio_read(F2FS_SB(sb), &b, start_blk);
        for (j = start_page_idx; j <= total_pages; j ++)
        {
            if (nr_page % 256 == 0 && nr_page != 0)
            {
                if (nr_page == total_pages)
                {
                    submit_bio_wait(b);
                    break;
                }
                else
                {
                    submit_bio(b);
                }

                if (nr_page != total_pages)
                {
                    j_alloc_bio_read(F2FS_SB(sb), &b, start_blk);
                }
            }
            ret = add_journal_page_2_bio(j_file_mmap[i].j_pages[j], b);
            if (ret != F2FSJ_OK)
            {
                INFO_REPORT("add page-[%d] to bio err, bio_max_vec %d, read journal exit\n", j, b->bi_max_vecs);
                return F2FSJ_ERROR;
            }
            nr_page ++;
            start_blk ++;
        }
        INFO_REPORT("submit bio to read %d journal pages\n", total_pages);
    }

    if (b->bi_status)
    {
        STATUS_LOG(STATUS_ERROR, "Read journal file by bio happends ERR-%d\n",b->bi_status);
    }
    else
    {
        // read journal file contents and do recovery
        INFO_REPORT("read journal file by bio succuessfully\n");
        iterate_journal(sb, 0);
    }
}

int is_invalid_log_type(log_type_e log_type)
{
    if (log_type == CREATE_LOG || log_type == MKDIR_LOG || log_type == UNLINK_LOG
    || log_type == RENAME_LOG || log_type == DATA_WRITE_LOG)
    {
        return 0;
    }
    else
    {
        INFO_REPORT("Invalid log type, read journal teiminated\n");
        return 1;
    }
}


int iterate_journal(struct super_block *sb, int j_file_idx)
{
    int i = 0;
    int j = 0;
    uint8_t * p_addr = NULL;
    uint8_t * log_en = NULL;
    j_log_head_t *log_header = NULL;

    uint64_t time1, time2;
    time1 = get_current_time_ms();

    for (i = 0; i < JOURNAL_BLK_PER_SMALL_FILE; i ++)
    {
        p_addr = j_file_mmap[j_file_idx].j_pages_buf[i];

        for (j = 0; j < J_LOG_ENTRY_PER_BLOCK; j ++)
        {
            log_en = (uint8_t *)p_addr + (J_LOG_ENTRY_SIZE * j);
            log_header = (j_log_head_t *)log_en;
            if (log_header->log_size != 128 || is_invalid_log_type(log_header->log_type))
            {
                INFO_REPORT("read an invalid log, recover end\n");
                goto end;
            }
            INFO_REPORT("read one log, file op is %d\n", log_header->log_type);
            // do the recovery by log type
            do_recover_from_journal(sb, log_header->log_type, (uint8_t *) log_en);
        }
    }

end:
    time2 = get_current_time_ms();
    INFO_REPORT("recover %d files cost %llu ms\n", i, time2 - time1);
}

int ep_commit_writeback_data_pages(struct f2fs_inode_info *f2fs_i)
{
    struct inode *inode = NULL;
    struct address_space *mapping = NULL;

    if (f2fs_i)
    {
        inode = &f2fs_i->vfs_inode;
        if (inode)
        {
            mapping = inode->i_mapping;
            struct writeback_control wbc =
            {
                .sync_mode = WB_SYNC_ALL,
                .nr_to_write = mapping->nrpages,
                .range_start = 0,  // TODO, The first page num in log
                .range_end   = 0,  // TODO, The last page num is log
                .for_reclaim = 0,
            };
    
            return mapping->a_ops->writepages(mapping, &wbc);
        }
        else
        {
            STATUS_LOG(STATUS_ERROR, "get vfs inode from f2fs_inode_info happens err\n");
        }
    }

    // should never reach here
    return F2FSJ_ERROR;
}

int j_apply_NODE(struct f2fs_sb_info *sbi, uint32_t nid)
{
    int ret = F2FSJ_OK;
    struct page *NODE_page = NULL;
    struct address_space *node_mapping =  NODE_MAPPING(sbi);
    struct inode *inode = NULL;
    struct writeback_control wbc =
    {
        .sync_mode = WB_SYNC_NONE,
        .nr_to_write = 1,
        .for_reclaim = 0,
    };

	inode = f2fs_iget_retry(sbi->sb, nid);
	if (IS_ERR(inode)) {
		return PTR_ERR(inode);
	}

    // apply in-memory inode
    f2fs_write_inode(inode, NULL);

    // NODE_page = f2fs_get_node_page(sbi, nid);
    // if (NODE_page == NULL)
    // {
    //     STATUS_LOG(STATUS_ERROR, "get inode page err\n");
    //     return F2FSJ_ERROR;
    // }
    // else
    // {
    //     if (PageDirty(NODE_page))
    //     {
    //         ret = node_mapping->a_ops->writepage(NODE_page, &wbc);
    //     }
    // }

    return F2FSJ_OK;
}

int j_apply_META(struct f2fs_sb_info *sbi, uint32_t ino_num, uint32_t node_id, uint32_t segno)
{
    uint32_t ret = F2FSJ_OK;
    struct address_space *meta_mapping =  META_MAPPING(sbi);
    struct page * META_page = NULL;
    uint32_t meta_page_ofs = 0;

    struct writeback_control wbc =
    {
        .sync_mode = WB_SYNC_NONE,
        .nr_to_write = 1,
        .for_reclaim = 0,
    };

    // Apply NAT page for inode
    if (ino_num != 0 )
    {
        META_page = f2fs_get_meta_page(sbi, current_nat_addr(sbi, ino_num));
        if (META_page != NULL)
        {
            if (PageDirty(META_page))
            {
                ret = meta_mapping->a_ops->writepage(META_page, &wbc);
            }
        }
        else
        {
            STATUS_LOG(STATUS_INFO, "Get nat page for inode page fail\n");
        }
    }
    META_page = NULL;

    // Apply NAT page for node
    if (node_id != 0 )
    {
        META_page = f2fs_get_meta_page(sbi, current_nat_addr(sbi, node_id));
        if (META_page != NULL)
        {
            if (PageDirty(META_page))
            {
                ret = meta_mapping->a_ops->writepage(META_page, &wbc);
            }
        }
        else
        {
            STATUS_LOG(STATUS_INFO, "Get nat page for node page fail\n");
        }
    }
    META_page = NULL;

    // Apply sit and ssa page
    if (segno != 0)
    {
        META_page = f2fs_get_meta_page(sbi, current_sit_addr(sbi, segno));
        if (META_page != NULL)
        {
            if (PageDirty(META_page))
            {
                ret = meta_mapping->a_ops->writepage(META_page, &wbc);
            }
        }
        META_page = NULL;

        META_page = f2fs_get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
        if (META_page != NULL)
        {
            if (PageDirty(META_page))
            {
                ret = meta_mapping->a_ops->writepage(META_page, &wbc);
            }
        }
    }

    return ret;
}

int do_recover_from_journal(struct super_block *sb, log_type_e log_type, uint8_t *log_content)
{

    if (log_type == CREATE_LOG || log_type == MKDIR_LOG)
    {
        j_new_inode_log_t i_log;
        create_log_t * create_log = (create_log_t *)log_content;
        memcpy(&i_log, &(create_log->j_new_ino_log), sizeof(j_new_inode_log_t));
        INFO_REPORT("create log, p_ino is %d, file name is %s\n", i_log.i_pino, i_log.i_name);
        //j_recover_new_inode(sb, &i_log);
    }
    else if (log_type == UNLINK_LOG)
    {
        delete_log_t * delete_log = (delete_log_t *)log_content;
        INFO_REPORT("unlink log, file name is %s", delete_log->file_name);
        //j_recover_unlink(sb, delete_log);
    }

    return F2FSJ_OK;
}