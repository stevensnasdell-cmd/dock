/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Four largest engine states must fit the resident audio arena (Dock: each part's largest, DOCK_ENGINE_OK: the drummer
 * never holds PLUCK's). */
#define MELODEE_RAM_AUDIO 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
int main(void){
 host_tracks_init();memset(resource,0,sizeof resource);
 uint32_t total=0;
 for(uint32_t p=0;p<NPART;p++){
  uint32_t largest=0,engine=0;
  for(uint32_t k=0;k<NENG_SHOWN;k++){uint32_t e=eng_vis(k),bytes=eng_state_size(e);
   if(e==ENGI_DRUM){uint32_t a=sizeof(drum909_part_t),b=dsk_bytes(),c=sizeof(drum_lane_t)*DV_NLANE;bytes=a>b?(a>c?a:c):(b>c?b:c);}   /* its largest kit */
   if(DOCK_ENGINE_OK(p,e)&&bytes>largest){largest=bytes;engine=e;}}
  trk[p].engine=(uint8_t)engine;assert(resource_get(RES_ENGINE0+p,largest));total+=largest;
 }
 assert(resource_used()==total);assert(total<=48u*1024u);
 printf("Resident dry audio: four largest engines use %u B, fit 48 KiB: OK\n",total);
 return 0;
}
