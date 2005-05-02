#ifndef __FILEKEY_H__
#define __FILEKEY_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/*
 * file.h
 *
 */

#define OC_File_Read      0
#define OC_File_Write     1
#define OC_File_Inode     2

#define OC_File_Map       3

#define RC_File_Length    1
#if 0
#define RC_FOFFSET        1
#define RC_FLEN           3
#endif

#ifndef __ASSEMBLER__
/*
 * based on inode structure from linux/include/linux/fs.h
 *
 * some elements are represented as domain key regs
 */

struct inode {
  uint64_t       i_sz;
  uint32_t	        i_mode;

  /* Yes, these DO need to be uint64_ts.  Think *microseconds* since
     some epoch! */
  uint64_t   	i_atime;
  uint64_t       i_mtime;
  uint64_t       i_ctime;
  
  uint32_t	        i_version;
  uint32_t	        i_nrpages;
  uint32_t          i_init;

  /*
    unsigned long	        i_blksize;
    unsigned long i_blocks;
    kdev_t	i_dev;
    unsigned long	i_ino;
    nlink_t	i_nlink;
    
    struct semaphore i_sem;
    struct inode_operations *i_op;
    struct super_block *i_sb;
    struct wait_queue *i_wait;
    struct file_lock *i_flock;
    struct vm_area_struct *i_mmap;
    struct page *i_pages;
    struct dquot *i_dquot[MAXQUOTAS];
    struct inode *i_next, *i_prev;
    struct inode *i_hash_next, *i_hash_prev;
    struct inode *i_bound_to, *i_bound_by;
    struct inode *i_mount;
    unsigned short i_count;
    unsigned short i_flags;
    unsigned char i_lock;
    unsigned char i_dirt;
    unsigned char i_pipe;
    unsigned char i_sock;
    unsigned char i_seek;
    unsigned char i_update;
    unsigned short i_writecount;
    union {
    struct pipe_inode_info pipe_i;
    struct minix_inode_info minix_i;
    struct ext_inode_info ext_i;
    struct ext2_inode_info ext2_i;
    struct hpfs_inode_info hpfs_i;
    struct msdos_inode_info msdos_i;
    struct umsdos_inode_info umsdos_i;
    struct iso_inode_info isofs_i;
    struct nfs_inode_info nfs_i;
    struct xiafs_inode_info xiafs_i;
    struct sysv_inode_info sysv_i;
    struct affs_inode_info affs_i;
    struct ufs_inode_info ufs_i;
    struct socket socket_i;
    void * generic_ip;
    } u;
    */
};

uint32_t file_read(uint32_t krFile, uint64_t offset, uint32_t len,
		   uint8_t *buf, uint32_t *outLen); 
uint32_t file_write(uint32_t krFile, uint64_t offset, uint32_t len,
		    const uint8_t *buf, uint32_t *outLen); 

#endif

#endif /* __FILEKEY_H__ */
