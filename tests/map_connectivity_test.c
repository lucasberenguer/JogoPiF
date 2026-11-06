
#include "clearing.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#define STEP 8
int main(void) {
 for(int which=0;which<2;which++) {
  Clearing map={0};map.cemetery=which;
  map.background.width=which?1670:1672;map.background.height=which?942:941;
  if(which) { ConfigureCemeteryBoundary(&map);map.spawn=(Vector2){835,207}; }
  else ConfigureClearingBoundary(&map);
  int w=map.background.width/STEP+1,h=map.background.height/STEP+1,n=w*h;
  unsigned char *freeCells=calloc(n,1),*seen=calloc(n,1);
  int *q=malloc(n*sizeof(int)),mainComponent=0,freeTotal=0;
  assert(freeCells && seen && q);
  for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
   freeCells[y*w+x]=!IsMapActorBlocked(&map,(Vector2){x*STEP,y*STEP});freeTotal+=freeCells[y*w+x];
  }
  int start=(int)roundf(map.spawn.y/STEP)*w+(int)roundf(map.spawn.x/STEP);
  if(!freeCells[start]){printf("BLOCKED SPAWN %d\n",which);return 1;}
  int components=0,unreachable=0;
  for(int pass=-1;pass<n;pass++) {
   int seed=pass<0?start:pass;
   if(!freeCells[seed]||seen[seed])continue;
   int front=0,back=0;q[back++]=seed;seen[seed]=1;
   while(front<back) {
    int at=q[front++],x=at%w,y=at/w;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
     int nx=x+dx,ny=y+dy;
     if(nx<0||ny<0||nx>=w||ny>=h||(!dx&&!dy))continue;
     int next=ny*w+nx;if(seen[next]||!freeCells[next])continue;
     if(IsMapActorBlocked(&map,(Vector2){(x+dx*.5f)*STEP,(y+dy*.5f)*STEP}))continue;
     seen[next]=1;q[back++]=next;
    }
   }
   if(pass<0)mainComponent=back;else {
    unreachable+=back;
    printf("map=%d isolated_cells=%d sample=(%d,%d)\n",which,back,(seed%w)*STEP,(seed/w)*STEP);
   }
   components++;
  }
  printf("map=%d components=%d free=%d reachable=%d isolated=%d\n",which,components,freeTotal,mainComponent,unreachable);
  assert(components==1 && unreachable==0);
  free(freeCells);free(seen);free(q);
 }
 return 0;
}
