
#include "cdfs.h"

static unsigned int cddb_sum(int n);


unsigned long discid(cd * this_cd) {
  unsigned int i=0, t, n = 0, trks=0, l=0;

  for (i=0; i< this_cd->tracks; i++)
    if(this_cd->track[T2I(i)].type==AUDIO) {
      n += cddb_sum((this_cd->track[T2I(i)].start_lba+CD_MSF_OFFSET)/CD_FRAMES);
      trks++; l=i;
      }

  t = (this_cd->track[T2I(l+1)].start_lba-
       this_cd->track[T2I(0)].start_lba)/CD_FRAMES;

  return (((n % 0xFF) << 24) | (t << 8) | trks);
}


static unsigned int cddb_sum(int n) {
  unsigned int ret = 0;

  while (n > 0) {
    ret += (n % 10);
    n /= 10;
  }

  return ret;
}

