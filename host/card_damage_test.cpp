#include "../src/platform/CardDamage.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace pokeri;
int main(){
 unsigned cases=0;
 for(unsigned source:{0u,1u,607u,608u,16384u,180224u,700000u})
 for(int row:{-110,-99,-1,0,1,80,199,283,300})
 for(unsigned x:{0u,1u,24u,519u,520u,521u,600u,607u})
 for(unsigned height:{1u,40u,222u,283u}){
  int64_t first=int64_t(source)+int64_t(row)*608+x;
  if(first<0 || first>0x100000-99*608-88)continue;
  DamageBounds got;bool known=CardDamage::project(first,source,608,0,height,got);
  if(!known){assert(x>520);continue;}
  DamageBounds want;
  for(unsigned y=0;y<height;++y)for(unsigned px=0;px<608;++px){
   uint32_t address=source+y*608+px;
   if(address>=unsigned(first) && (address-first)/608<100 && (address-first)%608<88)
    want.include(DamageBounds(px,y,1,1));
  }
  assert(got.x==want.x && got.y==want.y && got.width==want.width && got.height==want.height);++cases;
 }
 DamageBounds r(5,7,8,9);assert(!CardDamage::project(0,0,0,0,283,r));
 assert(!CardDamage::project(0,0,608,282,2,r));assert(!CardDamage::project(0xfffff,0,608,0,283,r));
 assert(!CardDamage::project(0,0xfffff,608,0,1,r));
 CardDamage d;bool unknown=false;d.include(100,unknown);d.include(100,unknown);assert(d.marked&&!unknown);
 d.include(101,unknown);assert(unknown);d.clear();assert(!d.marked);
 DamageBounds buffers[2];buffers[0].include(DamageBounds(1,2,3,4));buffers[1]=buffers[0];
 buffers[0]=DamageBounds();buffers[1].include(DamageBounds(20,30,2,2));
 assert(!buffers[0].width && buffers[1].x==1 && buffers[1].y==2 && buffers[1].width==21 && buffers[1].height==30);
 std::printf("PASS: %u card projections against per-pixel oracle; conservative guards and separate buffer damage\n",cases);
}
