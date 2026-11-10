#include "clearing.h"
#include "raymath.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static Clearing Room(void) {
    Clearing map={0};map.cemetery=true;
    map.background.width=1000;map.background.height=1000;
    map.boundaryCount=4;
    map.boundary[0]=(Vector2){0,0};map.boundary[1]=(Vector2){1000,0};
    map.boundary[2]=(Vector2){1000,1000};map.boundary[3]=(Vector2){0,1000};
    return map;
}
int main(void) {
    Clearing map=Room();Vector2 point={992,400};
    MoveMapPlayer(&map,&point,(Vector2){100,60});
    assert(point.x<=993 && point.y>459 && !IsMapActorBlocked(&map,point));
    float y=point.y;
    MoveMapPlayer(&map,&point,(Vector2){100,0});
    assert(fabsf(point.y-y)<.01f); /* No sideways drift from head-on input. */
    point=(Vector2){400,400};
    MoveMapPlayer(&map,&point,(Vector2){1200,0});
    assert(point.x<=993 && !IsMapActorBlocked(&map,point));
    point=(Vector2){995,400};Vector2 embedded=point;
    MoveMapPlayer(&map,&point,(Vector2){0,0});
    assert(!IsMapActorBlocked(&map,point) && Vector2Distance(point,embedded)<=24);
    map.boundary[2]=(Vector2){1000,500};
    point=(Vector2){400,790};
    MoveMapPlayer(&map,&point,(Vector2){100,0});
    assert(point.x>450 && point.y<780 && !IsMapActorBlocked(&map,point));
    for(int which=0;which<2;which++) {
        Clearing arena={0};arena.cemetery=which;
        arena.background.width=which?1670:1672;arena.background.height=which?942:941;
        if(which) {ConfigureCemeteryBoundary(&arena);arena.spawn=(Vector2){835,207};}
        else ConfigureClearingBoundary(&arena);
        point=arena.spawn;
        for(int phase=0;phase<8;phase++) for(int frame=0;frame<220;frame++) {
            Vector2 delta={cosf(phase*PI/4)*8,sinf(phase*PI/4)*8};
            Vector2 previous=point;
            MoveMapPlayer(&arena,&point,delta);
            assert(!IsMapActorBlocked(&arena,point));
            assert(Vector2Distance(previous,point)<=8.1f);
        }
    }
    puts("Player: wall sliding, slanted edge, bounded recovery, dodge and map sweeps passed");
    return 0;
}
