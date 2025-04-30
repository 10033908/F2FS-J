#ifndef __J_RECOVERY_H__
#define __J_RECOVERY_H__

#include "f2fs.h"
#include "j_log_content.h"


int j_recover_new_inode(struct super_block *sb, j_new_inode_log_t * new_inode_log);

int j_recover_unlink(struct super_block *sb, delete_log_t *delete_log);
#endif // !__J_RECOVERY_H__