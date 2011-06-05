/*
   File proc.c - /proc routines for cdfs


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

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*
 * Robert W. Boone (November, 2002)                                           *
 * 	Rewrote using kernel sequential synthetic file support.  This was     *
 * 	done to fix a kernel segmentation fault caused by trying to shove too *
 * 	much data into a kernel buffer when looking at a CD with more than a  *
 * 	few sessions on it.  I tried modifying existing code to gracefully    *
 *	carve up output into 4K pieces but it quickly became a nightmare to   *
 *	manage the file position to keep proc_file_read() from changing it    *
 *	unexpectedly.  To me, it seemed most logical to break up the output   *
 *	into manageable chunks, with each chunk being a CD track.  Thus, file *
 *	offset 0 would be track 0, file offset 1 would be track 1, etc.       *
 *	Since the kernel already provides this capability in the form of      *
 *	sequential synthetic file support, that's what I settled on using.    *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#include "cdfs.h"
#include <linux/seq_file.h>

struct proc_dir_entry * cdfs_proc_entry;

cd * cdfs_proc_cd;

/******************************************************************************
 * Pointer to dummy track; used to tell cdfs_show() there is no CD mounted    *
 ******************************************************************************/

struct _track_info *dummy_track_p = NULL;


/*============================================================================*
 *  cdfs_start()                                                              *
 *	Purpose:                                                              *
 *	    Given a track number, find its track[] entry.                     *
 *	Inputs:                                                               *
 *	    file_p ===>	Pointer to seq_file descriptor; unused                *
 *	    offset_p =>	Pointer to offset (track number) within /proc/cdfs    *
 *	Returns:                                                              *
 *	    If no CD is mounted, dummy_track_p is returned as long as we      *
 *	    don't try to go past the first track, otherwise NULL is returned. *
 *	    If there are no more tracks left, NULL is returned.  Otherwise,   *
 *	    the address of the track's track[] entry is returned.             *
 *============================================================================*/

static void *
cdfs_start(struct seq_file *file_p, loff_t *offset_p) {

    /*
     *	If there is no CD
     *	    If we're not trying to go past the first track
     *		Allocate a dummy track pointer if needed
     *	    	Tell cdfs_show() to print "No CD mounted" message
     *	    Else
     *		Indicate end of file
     */

    if (cdfs_proc_cd == NULL) {
	if (*offset_p < 1) {
	    if (dummy_track_p == NULL) {
		dummy_track_p = (struct _track_info *)kmalloc(sizeof(struct _track_info), GFP_KERNEL);
		if (dummy_track_p == NULL) {
                   printk(FSNAME ": kmalloc failed in cdfs_start !\n"); 
                   return NULL;
		}
	    }
            return dummy_track_p;
	} else {
	    return NULL;
	}
    }

    /*
     *	If there are no more tracks left
     *	    Indicate end of file
     */

    if (*offset_p >= cdfs_proc_cd->tracks) {
	return NULL;
    }

    /*
     *	Return track[] entry address for track, converting track to inode
     */

    return &cdfs_proc_cd->track[T2I(*offset_p)];
}


/*============================================================================*
 *  cdfs_next()                                                               *
 *	Purpose:                                                              *
 *	    Move to next track number.                                        *
 *	Inputs:                                                               *
 *	    file_p ===>	Pointer to seq_file descriptor; unused                *
 *	    data_p ===>	Pointer to some bit of data; unused                   *
 *	    offset_p =>	Pointer to offset (track number) within /proc/cdfs    *
 *	Returns:                                                              *
 *	    cdfs_start() result.                                              *
 *============================================================================*/

static void *
cdfs_next(struct seq_file *file_p, void *data_p, loff_t *offset_p) {

    /*
     *	Go to next track and get its track[] entry address
     */

    (*offset_p)++;
    return cdfs_start(file_p, offset_p);
}


/*============================================================================*
 *  cdfs_stop()                                                               *
 *	Purpose:                                                              *
 *	    Placeholder function; does nothing.                               *
 *	Inputs:                                                               *
 *	    file_p ===>	Pointer to seq_file descriptor; unused                *
 *	    data_p ===>	Pointer to some bit of data; unused                   *
 *	Returns:                                                              *
 *	    Nothing.                                                          *
 *============================================================================*/

static void
cdfs_stop(struct seq_file *file_p, void *data_p) {
}


/*============================================================================*
 *  remove_trailing_blanks()                                                  *
 *	Purpose:                                                              *
 *	    Remove trailing blanks from a string, placing a null character    *
 *	    (\0) immediately after first non-blank character found during     *
 *	    right to left scan.                                               *
 *	Inputs:                                                               *
 *	    string_p => Pointer to string to remove blanks from               *
 *	    bytes ====> Maximum number of bytes in string                     *
 *	Returns:                                                              *
 *	    Address of 
 *============================================================================*/

static char temp_buffer[4096];

static char *
remove_trailing_blanks(
    char *string_p,
    int bytes
) {
    char	*temp_p;

    /*
     *	Make copy of string in temporary buffer
     *	Get address of last character in string
     */

    (void) memcpy(temp_buffer, string_p, bytes);
    temp_p = temp_buffer + bytes - 1;

    /*
     *	While string not exhausted and current character is a blank
     *	    Move one character to the left in the string
     */
 
    while ((temp_p >= temp_buffer) && (*temp_p == ' ')) {
	temp_p -= 1;
    }

    /*
     *	We went too far left, move one character back to right
     *	Insert null character to terminate string
     */

    temp_p += 1;
    *temp_p = '\0';
    return &temp_buffer[0];
}


/*============================================================================*
 *  cdfs_show()                                                               *
 *	Purpose:                                                              *
 *	    Print information for a CD track.                                 *
 *	Inputs:                                                               *
 *	    file_p ===>	Pointer to seq_file descriptor                        *
 *	    data_p ===>	Pointer to address of track's track[] entry           *
 *	Returns:                                                              *
 *	    0 on success.  Negative number on failure.                        *
 *============================================================================*/

#define TRACK_POINTER_TO_TRACK_NUMBER(pointer) \
    ((pointer - &cdfs_proc_cd->track[0]) - 3) + 1

static int
cdfs_show(struct seq_file *file_p, void *data_p) {
    struct _track_info	*track_p;

    track_p = data_p;

    /*
     *	If no CD mounted
     *	    Print that message but nothing else
     */

    if (track_p == dummy_track_p) {
	seq_printf(
	    file_p,
	    "[%s\t%s]\n\tNo CD mounted\n",
	    FSNAME,
	    VERSION
	);
	return 0;
    }

    /*
     *	If current track is track 1 (inode 3)
     *	    Print disc ID and number of tracks before anything else
     */

    if (track_p == &(cdfs_proc_cd->track[3])) {
	seq_printf(
	    file_p,
	    "[%s\t%s%s]\n\nCD (discid=%08X) contains %d track%s:\n\n",
	    FSNAME,
	    VERSION,
	    cdfs_proc_cd->toc_scsi?", with new (SCSI) TOC support":"",
	    cdfs_proc_cd->discid,
	    cdfs_proc_cd->tracks,
	    cdfs_proc_cd->tracks - 1 ? "s" : ""
	);
    }

    seq_printf(file_p, "\n");

    /*
     *	Now print information for current track
     */

    if (track_p->type == DATA) {
	if (track_p->iso_size) {
	    seq_printf(
		file_p,
		"Track %2d: data track (%s), [%d-%d/%d], length=%d MB\n",
		TRACK_POINTER_TO_TRACK_NUMBER(track_p),
		track_p->name,
		track_p->start_lba,
		track_p->iso_size / 2048,
		track_p->stop_lba,
		track_p->track_size / 1024 / 1024
	    );
	    seq_printf(
		file_p,
		"\ttype: %c info: %.5s version: %c\n"
		"\tdate: %.2s/%.2s/%.4s time: %.2s:%.2s:%.2s\n"
		"\tsystem: %.32s\n\tvolume: %.32s\n",
		track_p->iso_info->type[0] + 48,
		track_p->iso_info->id,
		track_p->iso_info->version[0] + 48,
		track_p->iso_info->creation_date + 6,
		track_p->iso_info->creation_date + 4,
		track_p->iso_info->creation_date,
		track_p->iso_info->creation_date + 8,
		track_p->iso_info->creation_date + 10,
		track_p->iso_info->creation_date + 12,
		track_p->iso_info->system_id,
		track_p->iso_info->volume_id
	    );
	    seq_printf(
		file_p,
		"\tpublisher: %.128s\n",
		remove_trailing_blanks(track_p->iso_info->publisher_id, 128)
	    );
	    seq_printf(
		file_p,
		"\tpreparer: %.128s\n",
		remove_trailing_blanks(track_p->iso_info->preparer_id, 128)
	    );
	    seq_printf(
		file_p,
		"\tapplication: %.128s\n",
		remove_trailing_blanks(track_p->iso_info->application_id, 128)
	    );
	    seq_printf(
		file_p,
		"\tlength: %d MB / %d MB / %d MB / %d MB\n",
		(track_p->iso_size - track_p->start_lba * CD_FRAMESIZE) / 1024 / 1024,
		track_p->track_size / 1024 / 1024,
		track_p->iso_size / 1024 / 1024,
		track_p->size / 1024 / 1024
	    );
	} else {
	    seq_printf(
		file_p,
		"Track %2d: data track (%s), [%d-%d], length=%d kB\n",
		TRACK_POINTER_TO_TRACK_NUMBER(track_p),
		track_p->name,
		track_p->start_lba,
		track_p->stop_lba,
		track_p->track_size / 1024
	    );
	    seq_printf(
		file_p,
		"\ttype:  %s\n",
		cdfs_proc_cd->videocd_type
	    );
	    seq_printf(
		file_p,
		"\ttitle:  %s\n",
		cdfs_proc_cd->videocd_title
	    );
	    seq_printf(
		file_p,
		"\tframesize:  %d B\n",
		track_p->xa_data_size
	    );
	}
    } else if (track_p->type == BOOT) {
	seq_printf(
	    file_p,
	    "Bootimage (%s), [%d-%d], length=%d kB\n\tID string:%s\n",
	    track_p->name,
	    track_p->start_lba,
	    track_p->stop_lba,
	    track_p->size / 1024,
	    track_p->bootID
	);
    } else if (track_p->type == HFS) {
	seq_printf(
	    file_p,
	    "Apple HFS (%s), [%d-%d], length=%d MB\n\tID string:%s\n",
	    track_p->name,
	    track_p->start_lba,
	    track_p->stop_lba,
	    track_p->size / 1024 / 1024,
	    track_p->bootID
	);
    } else if (track_p->type == AUDIO) {
	char	MSFsize[10];

	cdfs_constructMSFsize(MSFsize, track_p->size);
	seq_printf(
	    file_p,
	    "Track %2d: audio track (%s), [%8d -%8d], length=%s\n",
	    TRACK_POINTER_TO_TRACK_NUMBER(track_p),
	    track_p->name,
	    track_p->start_lba,
	    track_p->stop_lba,
	    MSFsize
	);
    }

    return 0;
}


/******************************************************************************
 * Table of sequential file operations implemented for CDFS                   *
 ******************************************************************************/

struct seq_operations cdfs_operations = {
    .start = cdfs_start,
    .next  = cdfs_next,
    .stop  = cdfs_stop,
    .show  = cdfs_show,
};
