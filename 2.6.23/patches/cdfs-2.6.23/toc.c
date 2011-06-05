/*

  File toc.c - superblock and module routines for cdfs

  
  Copyright (c) 2003 by Laurent Pinchart

  
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
#include <scsi/scsi.h>

/*
 * Send a SCSI command
 */
static int cdfs_scsi_send_command( struct super_block *sb,
    char *cmd, int dir, void *buffer, int buflen )
{
    struct cdrom_generic_command cd_cmd;
    struct request_sense sense;

    /* No need to zero the request sense as we don't use it anyway */
    memset( &cd_cmd, 0, sizeof( cd_cmd ) );
    memcpy( cd_cmd.cmd, cmd, CDROM_PACKET_SIZE );

    cd_cmd.data_direction = dir;
    cd_cmd.buffer = buffer;
    cd_cmd.buflen = buflen;
    cd_cmd.timeout = 20000;
    cd_cmd.sense = &sense;
    cd_cmd.quiet = 0;
    cd_cmd.stat = GOOD;

    if ( cdfs_ioctl( sb, CDROM_SEND_PACKET, (unsigned long)&cd_cmd ) < 0 || cd_cmd.stat != GOOD )
    {
        PRINT( "cdfs_scsi_send_command: CDROM_SEND_PACKET ioctl failed (status %d)\n", cd_cmd.stat );
        return -1;
    }

    return 0;
}

#define TIME2LBA(min, sec, frame) ((((min)*60)+(sec))*75+(frame))

int cdfs_toc_read_full( struct super_block *sb )
{
    unsigned char cmd[CDROM_PACKET_SIZE];
    unsigned char toc[12];
    int first_session, last_session;
    int sess, cur_track;
    int lead_out;
    cd * this_cd = cdfs_info( sb );

    /* Get the number of sessions */
    memset( cmd, 0, sizeof( cmd ) );
    memset( toc, 0, sizeof( toc ) );
    cmd[0] = READ_TOC;
    cmd[2] = 1;				/* Format : Session Info    */
    cmd[8] = sizeof( toc );
    if ( cdfs_scsi_send_command( sb, cmd, CGC_DATA_READ, toc, sizeof( toc ) ) < 0 )
        return -1;

    first_session = toc[2];
    last_session = toc[3];
    PRINT( "cdfs_toc_read_full: CD contains %d sessions\n", last_session - first_session + 1 ); 

    cur_track = T2I(0);
    this_cd->track[0].start_lba  = 0;
    this_cd->track[1].start_lba  = 0;
    this_cd->track[2].start_lba  = 0;

    /* Iterate through the sessions */
    cmd[1] = 2;				/* Addresses in TIME format */
    cmd[2] = 2;				/* Format : Full TOC        */
    lead_out = 0;
    for ( sess = first_session; sess <= last_session; ++sess )
    {
        unsigned char *stoc;
        int toc_size, tracks, j;

        /* Read the first 4 bytes of the full TOC to get the TOC length */
        cmd[6] = sess;			/* Session number           */
	cmd[7] = 0;			/* Allocation length MSB    */
	cmd[8] = 4;			/* Allocation length LSB    */

        if ( cdfs_scsi_send_command( sb, cmd, CGC_DATA_READ, toc, sizeof( toc ) ) < 0 )
	{
	    printk( "cdfs_read_toc: unable to read session %d toc header, skipping\n", sess );
	    continue;
	}

        /* Allocate memory for the session TOC */
        toc_size = ( toc[0] << 8 ) + toc[1] + 2;
	tracks = ( toc_size - 4 ) / 11;
        PRINT( "cdfs_toc_read_full: session %d contains %d track descriptors\n", sess, tracks ); 
	if ( tracks == 0 )
	    continue;

        if ( ! ( stoc = kmalloc( toc_size, GFP_KERNEL ) ) )
	{
	    printk( "cdfs_read_toc: unable to allocate memory for session %d toc, skipping\n", sess );
	    continue;
	}

        /* Read the full session TOC */
        cmd[7] = toc_size >> 8;
	cmd[8] = toc_size & 0xff;
        if ( cdfs_scsi_send_command( sb, cmd, CGC_DATA_READ, stoc, toc_size ) < 0 )
	{
	    printk( "cdfs_read_toc: unable to read session %d toc, skipping\n", sess );
	    kfree( stoc );
	    continue;
	}

        /* Iterate through the tracks descriptors */
	lead_out = 0;
        for ( j = 0; j < tracks; ++j )
	{
	    struct track_descriptor
	    {
	        unsigned char session;
		unsigned char adr_control;
		unsigned char tno;
		unsigned char point;
		unsigned char min;
		unsigned char sec;
		unsigned char frame;
		unsigned char zero;
		unsigned char pmin;
		unsigned char psec;
		unsigned char pframe;
	    } *track = (struct track_descriptor*)&stoc[11*j+4];

	    if ( ( track->adr_control >> 4 ) != 1 )
	        continue;

	    if ( track->point == 0xa2 )
	        /* Lead out start. Needed to compute the size of the last track */
		lead_out = TIME2LBA( track->pmin, track->psec, track->pframe ) - 150;
	    else if ( track->point <= 0x63 )
	    {
	        /* This is a true track descriptor */
		this_cd->track[cur_track].type = ( track->adr_control & 0x04 ) ? DATA : AUDIO;
		this_cd->track[cur_track].time = get_seconds();
		this_cd->track[cur_track].start_lba = TIME2LBA( track->pmin, track->psec, track->pframe ) - 150;
		this_cd->track[cur_track-1].stop_lba = this_cd->track[cur_track].start_lba - 1;
                PRINT("Start[%d]: %d\n", cur_track, this_cd->track[cur_track].start_lba);
		++cur_track;
	    }
	}
	this_cd->track[cur_track-1].stop_lba = lead_out - 1;

	kfree( stoc );
	break; ////////////
    }

    this_cd->track[cur_track].start_lba = lead_out;
    this_cd->tracks = cur_track-T2I(0);
    return 0;
}

int cdfs_toc_read( struct super_block *sb )
{
    int i, t;
    struct cdrom_tochdr hdr;
    struct cdrom_tocentry entry;   
    cd * this_cd = cdfs_info( sb );

    if ( cdfs_ioctl( sb, CDROMREADTOCHDR, (unsigned long)&hdr ) )
    {
        printk( "ioctl(CDROMREADTOCHDR) failed\n" );
        return -1;
    }

    this_cd->tracks = hdr.cdth_trk1 - hdr.cdth_trk0 + 1;
    PRINT( "CD contains %d tracks: %d-%d\n", this_cd->tracks, hdr.cdth_trk0, hdr.cdth_trk1 );

    /* Collect track info */
    entry.cdte_format = CDROM_LBA;

    this_cd->track[0].start_lba  = 0;
    this_cd->track[1].start_lba  = 0;
    this_cd->track[2].start_lba  = 0;
    for ( t = this_cd->tracks; t >= 0; --t )
    {
        i = T2I(t);

        entry.cdte_track = ( t == this_cd->tracks ) ? CDROM_LEADOUT : t+1;
        PRINT( "Read track %d/%d/%d\n", entry.cdte_track, t, i );

        if ( cdfs_ioctl( sb, CDROMREADTOCENTRY, (unsigned long)&entry ) )
	{
            printk( "ioctl(CDROMREADTOCENTRY) failed\n" );
            return -1;
        }

        this_cd->track[i].start_lba  = entry.cdte_addr.lba;
        this_cd->track[i].stop_lba   = this_cd->track[i+1].start_lba - 1;

        PRINT("Start[%d]: %d\n", i, this_cd->track[i].start_lba);

        if ( t != this_cd->tracks )	/* all tracks but the LEADOUT */
	{
            this_cd->track[i].type = ( entry.cdte_ctrl & CDROM_DATA_TRACK ) ? DATA : AUDIO;
            this_cd->track[i].time = get_seconds();
        }
    }

    return 0;
}
