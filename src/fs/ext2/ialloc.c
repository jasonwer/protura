/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/mutex.h>
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/block/bcache.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/ext2.h>
#include "ext2_internal.h"

static ino_t __ext2_check_block_group(struct ext2_super_block *sb, int group_no)
{
    struct block *b;
    struct ext2_disk_block_group *group;
    int loc;
    ino_t ino;
    int inode_start = 0;
    ino_t highest_inode = sb->disksb.inodes_per_block_group;

    group = sb->groups + group_no;

    if (group_no == sb->block_group_count - 1)
        highest_inode = sb->disksb.inode_total - sb->disksb.inodes_per_block_group * group_no;

    if (group->inode_unused_total == 0)
        return 0;

    /* Group 0 includes the reserved inodes */
    if (group_no == 0)
        inode_start = sb->disksb.first_inode;

    using_block_locked(sb->sb.bdev, group->block_nr_inode_bitmap, b) {
        loc = bit_find_next_zero(b->data, sb->block_size, inode_start);

        if (loc > highest_inode)
            return 0;

        bit_set(b->data, loc);
        group->inode_unused_total--;

        ino = loc + group_no * sb->disksb.inodes_per_block_group + 1;

        kp_ext2_trace(sb, "inode bitmap: %d\n", group->block_nr_inode_bitmap);
        kp_ext2_trace(sb, "Free inode: %d, loc: %d\n", ino, loc);

        block_mark_dirty(b);
    }

    return ino;
}

static void __ext2_unset_inode_number(struct ext2_super_block *sb, ino_t ino)
{
    struct block *b;
    int inode_group = (ino - 1) / sb->disksb.inodes_per_block_group;
    int inode_entry = (ino - 1) % sb->disksb.inodes_per_block_group;

    using_block_locked(sb->sb.bdev, sb->groups[inode_group].block_nr_inode_bitmap, b) {
        if (bit_test(b->data, inode_entry) == 0)
            kp_ext2_warning(sb, "Attempted to unset inode with ino(%d) not currently used!\n", ino);

        bit_clear(b->data, inode_entry);
        block_mark_dirty(b);
    }

    sb->groups[inode_group].inode_unused_total++;
    sb->disksb.inode_unused_total++;
}

int ext2_inode_new(struct super_block *sb, struct inode **result)
{
    struct ext2_super_block *ext2sb = container_of(sb, struct ext2_super_block, sb);
    struct ext2_inode *ext2_inode;
    int i, ret = 0;
    ino_t ino = 0;
    struct inode *inode;
    sector_t inode_group_blk = 0;
    int inode_group_blk_offset = 0;

    using_ext2_super_block(ext2sb) {
        for (i = 0; i < ext2sb->block_group_count && !ino; i++)
            if ((ino = __ext2_check_block_group(ext2sb, i)) != 0)
                break;

        if (ino) {
            int entry;

            entry = (ino - 1) % ext2sb->disksb.inodes_per_block_group;
            kp_ext2_trace(sb, "ialloc: entry: %d\n", entry);

            inode_group_blk = ext2sb->groups[i].block_nr_inode_table;
            kp_ext2_trace(sb, "ialloc: group start: %d\n", inode_group_blk);

            inode_group_blk += (entry * ext2sb->disksb.inode_entry_size) / ext2sb->block_size;
            kp_ext2_trace(sb, "ialloc: inode group block: %d\n", inode_group_blk);

            inode_group_blk_offset = entry % (ext2sb->block_size / ext2sb->disksb.inode_entry_size);
            kp_ext2_trace(sb, "ialloc: inode group block offset: %d\n", inode_group_blk_offset);
        }

        kp_ext2_trace(sb, "ialloc: Found new inode: %d\n", ino);
        kp_ext2_trace(sb, "ialloc: group: %d\n", i);

        if (ino)
            ext2sb->disksb.inode_unused_total--;
        else
            ret = -ENOSPC;
    }

    if (ret)
        return ret;

    inode = inode_get_invalid(sb, ino);
    kp_ext2_trace(sb, "ialloc: inode_alloc: %p\n", inode);

    if (!inode) {
        using_ext2_super_block(ext2sb)
            __ext2_unset_inode_number(ext2sb, ino);

        return -ENOMEM;
    }

    ext2_inode = container_of(inode, struct ext2_inode, i);

    ext2_inode->inode_group_blk_nr = inode_group_blk;
    ext2_inode->inode_group_blk_offset = inode_group_blk_offset;

    *result = &ext2_inode->i;
    return 0;
}

