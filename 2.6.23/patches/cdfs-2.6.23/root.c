/*

  File root.c - superblock and module routines for cdfs

  
  Copyright (c) 1999, 2000, 2001 by Michiel Ronsse 

  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
  
*/

#include "cdfs.h"

#include <linux/module.h>


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*
 * Robert W. Boone (November, 2002)                                           *
 *	Added code to transform /proc/cdfs into a sequential synthetic file.  *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#include <linux/seq_file.h>

#ifndef OLD_KERNEL
#include <linux/statfs.h>
#endif

extern struct seq_operations cdfs_operations;
extern struct _track_info *dummy_track_p;


/*============================================================================*
 *  cdfs_open()                                                               *
 *	Description:                                                          *
 *	    Open a CDFS /proc file as a sequential synthetic file.            *
 *	Input:                                                                *
 *	    inode_p => Pointer to /proc file inode; unused                    *
 *	    file_p ==> Pointer to sequential file descriptor                  *
 *	Return Value:                                                         *
 *	    seq_open() result.                                                *
 *============================================================================*/

static int
cdfs_open(struct inode *inode_p, struct file *file_p) {
    return seq_open(file_p, &cdfs_operations);
}


/*============================================================================*
 *  cdfs_release()                                                            *
 *	Description:                                                          *
 *	    Free any memory allocated when /proc/cdfs was opened or accessed  *
 *	    as a sequential synthetic file.                                   *
 *	Input:                                                                *
 *	    inode_p => Pointer to /proc file inode                            *
 *	    file_p ==> Pointer to sequential file descriptor                  *
 *	Return Value:                                                         *
 *	    seq_release() result.                                             *
 *============================================================================*/

int
cdfs_release(struct inode *inode_p, struct file *file_p) {

    /*
     *	Release any memory allocated to tell cdfs_show() there is no CD
     */

    if (dummy_track_p != NULL) {
	kfree(dummy_track_p);
	dummy_track_p = NULL;
    }

    return seq_release(inode_p, file_p);
}


/******************************************************************************
 * CDFS high-level file operations table                                      *
 ******************************************************************************/

struct file_operations proc_cdfs_operations = {
    .owner	= THIS_MODULE,
    .llseek	= seq_lseek,
    .read	= seq_read,
    .open	= cdfs_open,
    .release	= cdfs_release,
};


/********************************************************************/

static struct super_operations cdfs_ops;

void cdfs_parse_options(char *, cd *);

extern struct proc_dir_entry * cdfs_proc_entry;

extern cd * cdfs_proc_cd;

/********************************************************************/

#ifdef OLD_KERNEL
static struct super_block * cdfs_mount(struct super_block *sb, void *data, int silent){
  kdev_t dev = sb->s_dev;
  int i, j, t;
  struct cdrom_tochdr  hdr;
#else
static int cdfs_fill_super(struct super_block *sb, void *data, int silent){
  int i, t;
#endif
  struct cdrom_tocentry   entry;   
  int no_audio=0, no_data=0;
  cd * this_cd;

  PRINT("cdfs_mount\n");

#ifdef OLD_KERNEL
  MOD_INC_USE_COUNT;

  set_blocksize(dev, CD_FRAMESIZE);  // voor bread met ide-cd
#else
  sb_set_blocksize(sb, CD_FRAMESIZE);  // voor bread met ide-cd
#endif

  sb->s_blocksize = CD_FRAMESIZE;
  sb->s_blocksize_bits = 11;

  if (!(this_cd = cdfs_info(sb) = kmalloc(sizeof(cd), GFP_KERNEL))){
#ifdef OLD_KERNEL
    MOD_DEC_USE_COUNT;     
    return NULL;
#else
    return -ENOMEM;
#endif
  }

  this_cd->mode           = MODE;
  this_cd->gid            = GID;
  this_cd->uid            = UID;
  this_cd->single         = FALSE;
  this_cd->raw_audio      = 0;
  this_cd->toc_scsi       = FALSE;

  // Initialize cache for maximum sector size
  if (!(this_cd->cache = kmalloc(CD_FRAMESIZE_RAWER*CACHE_SIZE, GFP_KERNEL))) {
#ifdef OLD_KERNEL
    MOD_DEC_USE_COUNT;
    return NULL;
#else
    kfree(cdfs_info(sb));
    return -ENOMEM;
#endif
  }

  // Cache is still invalid
  this_cd->cache_sector = -CACHE_SIZE;

  cdfs_parse_options((char *) data, this_cd);

  /* Populate CD info with '.' and '..' */
  strcpy(this_cd->track[1].name, ".");  this_cd->track[1].start_lba=0;
  strcpy(this_cd->track[2].name, ".."); this_cd->track[2].start_lba=0;
  this_cd->nr_iso_sessions = 0;
  this_cd->size            = 0;

  if (this_cd->toc_scsi){
    if (cdfs_toc_read_full(sb)){
      printk("TOC read failed\n");
#ifdef OLD_KERNEL
      MOD_DEC_USE_COUNT;
      return NULL;
#else
      goto invalid;
#endif
    }
  } else {
    //if (cdfs_ioctl(sb, CDROMREADTOCHDR, (unsigned long)&hdr)){
    if (cdfs_toc_read(sb)){
      printk("cdfs_toc_read failed\n");
#ifdef OLD_KERNEL
      MOD_DEC_USE_COUNT;
      return NULL;
#else
      goto invalid;
#endif
    }
  }

  PRINT("CD contains %d tracks\n", this_cd->tracks);

  /* Collect track info */
  entry.cdte_format = CDROM_LBA;

  for (t=this_cd->tracks; t>=0; t--) {

    i = T2I(t);
//    j = this_cd->tracks-i;

 //   entry.cdte_track = (t==this_cd->tracks) ? CDROM_LEADOUT : t+1;
 //   PRINT("Read track %d/%d/%d\n", entry.cdte_track, t, i);

 //   if (cdfs_ioctl(sb, CDROMREADTOCENTRY, (unsigned long)&entry)){
   //   printk("ioctl(CDROMREADTOCENTRY) failed\n");
     // MOD_DEC_USE_COUNT;
  //    return NULL;
   // }

 //   this_cd->track[i].start_lba  = entry.cdte_addr.lba;
 //   this_cd->track[i].stop_lba   = this_cd->track[i+1].start_lba - 1;
    this_cd->track[i].track_size = this_cd->track[i+1].start_lba - this_cd->track[i].start_lba;  /* in sectors! */

    PRINT("Start[%d]: %d\n", i, this_cd->track[i].start_lba);

    if (t!=this_cd->tracks) {                 /* all tracks but the LEADOUT */
      if (this_cd->track[i].type==DATA) {
	//int track=i;
	no_data++;
	this_cd->track[i].iso_info  = cdfs_get_iso_info(sb, i);
	if (this_cd->track[i].iso_info) {
	  this_cd->track[i].time      = cdfs_constructtime((char*)&(this_cd->track[i].iso_info->creation_date));
	  this_cd->track[i].iso_size  = cdfs_constructsize((char*)&(this_cd->track[i].iso_info->volume_space_size)) * CD_FRAMESIZE;
	  if (!this_cd->single) this_cd->track[i].iso_size += this_cd->track[i].start_lba * CD_FRAMESIZE;
	  this_cd->track[i].track_size *= CD_FRAMESIZE;
	  this_cd->track[i].size = this_cd->track[i+1].start_lba * CD_FRAMESIZE;
	  sprintf(this_cd->track[i].name, this_cd->single ? DATA_NAME_SINGLE : DATA_NAME_ISO, t+1);
	  this_cd->lba_iso_sessions[this_cd->nr_iso_sessions].start = this_cd->track[i].start_lba;
	  this_cd->lba_iso_sessions[this_cd->nr_iso_sessions].stop  = this_cd->track[i].iso_size/CD_FRAMESIZE;
	  this_cd->nr_iso_sessions++;
	  cdfs_get_hfs_info(sb, i);  // possibly also a HFS
	} else {  // DATA, but no ISO -> either HFS or VideoCD
	  if (cdfs_get_hfs_info(sb, i)==-1){
	    printk("CHECKING VIDEOCD!!\n");
	    cdfs_get_XA_info(sb, i);
	    this_cd->track[i].time       = 0;
	    this_cd->track[i].iso_size   = 0;
	    this_cd->track[i].track_size = (this_cd->track[i].track_size-1) * this_cd->track[i].xa_data_size;
	    this_cd->track[i].size       = this_cd->track[i].track_size;
	    sprintf(this_cd->track[i].name, DATA_NAME_VCD, no_data);
	  } else { // HFS, no ISO, no VideoCD -> remove track
	    this_cd->track[i].iso_info  = NULL;
	    this_cd->track[i].type      = 0;
	  }
	}
      } else {
	no_audio++;
	this_cd->track[i].iso_info    = NULL;
	this_cd->track[i].type        = AUDIO;
	this_cd->track[i].time        = get_seconds();
	this_cd->track[i].iso_size    = 0;
	this_cd->track[i].track_size  = this_cd->track[i].track_size * CD_FRAMESIZE_RAW + ((this_cd->raw_audio==0)?WAV_HEADER_SIZE:0);
	this_cd->track[i].size        = this_cd->track[i].track_size;
	this_cd->track[i].avi         = 0;
	sprintf(this_cd->track[i].name, (this_cd->raw_audio)? RAW_AUDIO_NAME:AUDIO_NAME, t+1);
	if (this_cd->raw_audio) {
	  /* read the first sector. */
	  struct cdrom_read_audio cdda;
	  int status,k,j,prevk=0;
	  char* buf;
	  buf=kmalloc(CD_FRAMESIZE_RAW*2,GFP_KERNEL);
	  if(buf==NULL) {
		printk(FSNAME ": kmalloc failed in root.c !\n");
		return(-ENOMEM);
	  }
	  for (j=0;j<10;j++) {
	    cdda.addr_format = CDROM_LBA;
	    cdda.nframes     = 1;
	    cdda.buf         = buf+CD_FRAMESIZE_RAW;
	    cdda.addr.lba = this_cd->track[i].start_lba+j;
	    status = cdfs_ioctl(sb,CDROMREADAUDIO,(unsigned long)&cdda);
	    if (status) {
	      printk("cdfs_ioctl(CDROMREADAUDIO,%d) ioctl failed: %d\n", cdda.addr.lba, status);
	      goto out;
	    }
	    /* search the first non-zero byte */
	    for (k=0;k<CD_FRAMESIZE_RAW;k++)
	      if (buf[k+CD_FRAMESIZE_RAW]) break;
	    if (k<=CD_FRAMESIZE_RAW-4) break;
	    prevk=k;
	    if (k<CD_FRAMESIZE_RAW)
	      for (k=0;k<CD_FRAMESIZE_RAW;k++)
		buf[k]=buf[k+CD_FRAMESIZE_RAW];
	  }
	  if (j==10) goto out;
	  if ((j!=0)&&(prevk!=CD_FRAMESIZE_RAW)) {
	    k=prevk;
	    j--;
	  }
	  else k+=CD_FRAMESIZE_RAW;
	  this_cd->track[i].avi_offset = j*CD_FRAMESIZE_RAW+k-CD_FRAMESIZE_RAW;
	  if ((buf[k]=='R')&&(buf[k+1]=='I')&&
	      (buf[k+2]=='F')&&(buf[k+3]=='F')) {
	    this_cd->track[i].avi = 1;
	    this_cd->track[i].avi_swab = 0;
	  } else if ((buf[k]=='I')&&(buf[k+1]=='R')&&
	      (buf[k+2]=='F')&&(buf[k+3]=='F')) {
	    this_cd->track[i].avi = 1;
	    this_cd->track[i].avi_swab = 1;
	  }
	  if (this_cd->track[i].avi) {
	    if ((this_cd->track[i].avi_offset&1)!=0) {
	      printk("AVI offset is not even, error\n");
	      this_cd->track[i].avi=0;
	    } else {
	      this_cd->track[i].track_size -= this_cd->track[i].avi_offset;
	      sprintf(this_cd->track[i].name, AVI_AUDIO_NAME, t+1);
	    }
	  }
out:
    kfree(buf);
	}
      }
      // Calculate total CD size
      this_cd->size += this_cd->track[i].track_size;

      PRINT("Track %2d: (%dB)\n", t,  this_cd->track[i].size);

    } // else CDROM_LEADOUT

  }

  PRINT("CD ends at %d\n", this_cd->track[this_cd->tracks].start_lba);


  /* take care to get disc id after the toc has been read. JP, 29-12-2001 */
  this_cd->discid = discid(this_cd);

  ////////////////////////////////

  /* Check if CD is bootable */
  if (this_cd->track[T2I(0)].type==DATA) cdfs_check_bootable(sb);

  /* Check for an HFS partition in the first data track */
  /*if (no_data) {
    i=T2I(0);
    while (i<T2I(this_cd->tracks)) {
      if (this_cd->track[i].type==DATA)
        break;
      i++;
    }
    cdfs_get_hfs_info(sb, i);
  }
  */
  
  PRINT("%d audio tracks and %d data tracks => %dbytes\n", 
        no_audio, no_data, this_cd->size);
  
  sb->s_magic  = CDFS_MAGIC;
  sb->s_flags |= MS_RDONLY;
  sb->s_op     = &cdfs_ops;
  sb->s_root   = d_alloc_root(iget(sb, 0));
  
  cdfs_proc_cd = this_cd;

#ifdef OLD_KERNEL
  return sb;
#else
  return 0;

invalid:
  kfree(this_cd->cache);
  kfree(cdfs_info(sb));
  return -EINVAL;
#endif
}

/************************************************************************/

static void cdfs_umount(struct super_block *sb) {
  int t;
  cd * this_cd = cdfs_info(sb);

  PRINT("cdfs_umount\n");

  for (t=0; t<=this_cd->tracks; t++)
    if ((this_cd->track[T2I(t)].type == DATA) && this_cd->track[T2I(t)].iso_info)
      kfree(this_cd->track[T2I(t)].iso_info);
  
  // Free & invalidate cache
  kfree(this_cd->cache);
  this_cd->cache_sector = -CACHE_SIZE;

  // Remove /proc entry
  cdfs_proc_cd = NULL; 
  kfree(cdfs_info(sb));

#ifdef OLD_KERNEL
  MOD_DEC_USE_COUNT;
#endif

}

/************************************************************************/

#ifdef OLD_KERNEL
static int cdfs_statfs(struct super_block *sb, struct statfs *buf) {
  cd * this_cd = cdfs_info(sb);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
static int cdfs_statfs(struct dentry *d, struct kstatfs *buf) {
  cd * this_cd = cdfs_info(d->d_sb);
#else
static int cdfs_statfs(struct super_block *sb, struct kstatfs *buf) {
  cd * this_cd = cdfs_info(sb);
#endif
#endif
  PRINT("rmfs_statfs\n");

  buf->f_type    = CDFS_MAGIC;
  buf->f_bsize   = CD_FRAMESIZE;
  buf->f_blocks  = this_cd->size/CD_FRAMESIZE;
  buf->f_namelen = CDFS_MAXFN;
  buf->f_files   = this_cd->tracks;
  return 0;
}

/************************************************************************/

static int cdfs_readdir(struct file *filp, void *dirent, filldir_t filldir) {
  struct inode *inode = filp->f_dentry->d_inode;
  int i;
  cd * this_cd = cdfs_info(inode->i_sb);

  PRINT("cdfs_readdir ino=%ld f_pos=%u\n", inode->i_ino, (int)filp->f_pos);

  for(i=filp->f_pos; i<T2I(this_cd->tracks); i++) {
    if (filldir(dirent, this_cd->track[i].name, strlen(this_cd->track[i].name), 0, i, DT_UNKNOWN) < 0) 
      return 0;
    filp->f_pos++;
  }
  return 1;
}

/************************************************************************/

#ifdef OLD_KERNEL
static struct dentry * cdfs_lookup(struct inode *dir, struct dentry *dentry){
#else
static struct dentry * cdfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd){
#endif
  struct inode * inode;
  int i;
  cd * this_cd = cdfs_info(dir->i_sb);

  PRINT("cdfs_lookup %s ino=%ld -> ", dentry->d_name.name, dir->i_ino);

  for(i=0; i<T2I(this_cd->tracks); i++)
    if (!(strcmp(this_cd->track[i].name, dentry->d_name.name))) {
      if ((inode=iget(dir->i_sb, i))==NULL) {
        return ERR_PTR(-EACCES);
      } else {
        d_add(dentry, inode);
        return ERR_PTR(0);
      }
    }
  return ERR_PTR(-ENOENT);
}


/***************************************************************************/

static struct file_operations cdfs_dir_operations = {
  .read     = generic_read_dir,
  .readdir  = cdfs_readdir,
};

static struct inode_operations cdfs_inode_operations = {
  .lookup   = cdfs_lookup
};

/**************************************************************************/

static void cdfs_read_inode(struct inode *i) {
  cd * this_cd = cdfs_info(i->i_sb);

  //printk("this_cd = 0x%x\n\n\n", (unsigned)this_cd);

  PRINT("read inode %ld\n", i->i_ino);
  
  i->i_uid        = this_cd->uid;
  i->i_gid        = this_cd->gid;
  i->i_nlink      = 1;
  i->i_op         = &cdfs_inode_operations;
  i->i_fop        = NULL;
  i->i_data.a_ops = NULL;

  if (i->i_ino <= 2) {                               /* . and .. */
    i->i_size  = 0;                      /* Uuugh ?? */
    i->i_mtime = i->i_atime = i->i_ctime = CURRENT_TIME;
    i->i_mode  = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH;
    i->i_fop   = &cdfs_dir_operations;
  } else {                                          /* file */
    i->i_size  = this_cd->track[i->i_ino].size;
#ifdef OLD_KERNEL
    i->i_mtime = i->i_atime = i->i_ctime = this_cd->track[i->i_ino].time;
#else
    i->i_mtime.tv_sec = i->i_atime.tv_sec = i->i_ctime.tv_sec = this_cd->track[i->i_ino].time;
    i->i_mtime.tv_nsec = i->i_atime.tv_nsec = i->i_ctime.tv_nsec = 0;
#endif
    i->i_mode  = this_cd->mode;
    if ((this_cd->track[i->i_ino].type==DATA) && this_cd->track[i->i_ino].iso_size) {
      i->i_fop          = &cdfs_cddata_file_operations; 
      i->i_data.a_ops   = &cdfs_cddata_aops;
    } else if (this_cd->track[i->i_ino].type==AUDIO) {
      i->i_fop          = &cdfs_cdda_file_operations;
      if (this_cd->raw_audio)
	i->i_data.a_ops   = &cdfs_cdda_raw_aops;
      else
	i->i_data.a_ops   = &cdfs_cdda_aops;
    } else if (this_cd->track[i->i_ino].type==BOOT) {
      i->i_fop          = &cdfs_cddata_file_operations;
      i->i_data.a_ops   = &cdfs_cddata_aops;
    } else if (this_cd->track[i->i_ino].type==HFS) {
      if (this_cd->track[i->i_ino].hfs_offset) {
        i->i_fop        = &cdfs_cdhfs_file_operations; /* Bummer, this partition isn't properly aligned... */
        i->i_data.a_ops = &cdfs_cdhfs_aops;
      } else {
        i->i_fop        = &cdfs_cddata_file_operations;
        i->i_data.a_ops = &cdfs_cddata_aops;
      }
    } else {
      i->i_fop          = &cdfs_cdXA_file_operations;
      i->i_data.a_ops   = &cdfs_cdXA_aops;
    }
  }
}

/******************************************************************/

static struct super_operations cdfs_ops = {
  .read_inode = cdfs_read_inode,
  .put_super  = cdfs_umount,
  .statfs     = cdfs_statfs
};

#ifdef OLD_KERNEL
static DECLARE_FSTYPE_DEV(cdfs_fs_type, FSNAME, cdfs_mount);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
static int cdfs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data, struct vfsmount *mnt) {
  return get_sb_bdev(fs_type, flags, dev_name, data, cdfs_fill_super, mnt);
#else
static struct super_block *cdfs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
  return get_sb_bdev(fs_type, flags, dev_name, data, cdfs_fill_super);
#endif
}

static struct file_system_type cdfs_fs_type = {
  .owner    = THIS_MODULE,
  .name     = "cdfs",
  .get_sb   = cdfs_get_sb,
  .kill_sb  = kill_block_super,
  .fs_flags = FS_REQUIRES_DEV
};
#endif

/******************************************************/

MODULE_AUTHOR("Michiel Ronsse (ronsse@elis.UGent.be)");
MODULE_DESCRIPTION("CDfs: a CD filesystem");
MODULE_LICENSE("GPL"); 

#ifdef OLD_KERNEL
EXPORT_NO_SYMBOLS;
#endif

/******************************************************************/

static int __init cdfs_init(void) {
  int err;
  PRINT("init_module (insmod)\n");

  printk(FSNAME" "VERSION" loaded.\n");
 
  // register file system
  err = register_filesystem(&cdfs_fs_type);
  if (err < 0) return err;

  // register /proc entry
  if ((cdfs_proc_entry = create_proc_entry(FSNAME, 0, NULL )))
    cdfs_proc_entry->proc_fops = &proc_cdfs_operations;
  cdfs_proc_cd=NULL;

  // start kernel thread
  if ((kcdfsd_pid = kernel_thread(kcdfsd_thread, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND)) >0 ) {
    return 0;
  } else {
    printk(FSNAME" kernel_thread failed.\n");
    if (cdfs_proc_entry) remove_proc_entry(FSNAME, NULL);
    unregister_filesystem(&cdfs_fs_type);
    return -1;
  }
}

/******************************************************************/

static void __exit cdfs_exit(void) {
  PRINT("cleanup_module (rmmod)\n");
  kcdfsd_cleanup_thread();
  if (cdfs_proc_entry) remove_proc_entry(FSNAME, NULL);
  unregister_filesystem(&cdfs_fs_type);
}

/******************************************************************/

module_init(cdfs_init);
module_exit(cdfs_exit);

/******************************************************************/

void cdfs_parse_options(char *options, cd * this_cd) {  
  char *this_char,*value;
  
  /* from isofs */
  
  if (!options) return;
  
#ifdef OLD_KERNEL
  for (this_char = strtok(options,","); this_char; this_char = strtok(NULL,",")) {
#else
  while ((this_char = strsep(&options,",")) != NULL) {
#endif
    
    if (!strcmp(this_char,"single")) 
      this_cd->single=TRUE;
    else if (!strcmp(this_char,"raw")) 
      this_cd->raw_audio=TRUE;
    else if (!strcmp(this_char,"toc_scsi")) {
      printk("Using new TOC function!\n");
      this_cd->toc_scsi=TRUE;
    } else {
      if ((value = strchr(this_char,'=')) != NULL)
        *value++ = 0;     
      if (value &&
          (!strcmp(this_char,"mode") ||
           !strcmp(this_char,"uid") ||
           !strcmp(this_char,"gid"))) {
        char * vpnt = value;                                   
        unsigned int ivalue = simple_strtoul(vpnt, &vpnt, 0);
        if (*vpnt) return;
        switch(*this_char) {
        case 'u':  this_cd->uid = ivalue;              break;
        case 'g':  this_cd->gid = ivalue;              break;
        case 'm':  this_cd->mode = ivalue | S_IFREG ;  break;
        }
      } else 
        return;
    }
  }
}
