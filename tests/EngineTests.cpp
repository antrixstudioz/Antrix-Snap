#include "Engine.h"
#include <iostream>
#include <cstdlib>
void check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<"\n";std::exit(1);}}
int main(){
 for(double rate:{44100.,48000.,96000.}){
  snap::Engine e;e.prepare(rate);snap::Settings s;
  auto quiet=e.sample(s);check(quiet[0]==0&&quiet[1]==0,"Must start silent");
  for(int style=0;style<7;++style)for(int burst=0;burst<4;++burst){
   s.style=style;s.burst=burst;s.pitch=style%2?-12:12;s.tone=style%2?0:1;s.space=1;s.width=1;
   e.trigger(s);double energy=0,tail=0;
   for(int i=0;i<int(rate*2);++i){auto v=e.sample(s);for(float x:v){
    check(std::isfinite(x)&&std::abs(x)<=1,"Invalid/unsafe output");energy+=x*x;if(i>rate*1.8)tail+=x*x;}}
   check(energy>.001,"Snap must be audible");check(tail<.00001,"Tail must decay");
  }
  for(int n=0;n<100;++n)e.trigger(s);
  for(int i=0;i<5000;++i){auto v=e.sample(s);check(std::isfinite(v[0])&&std::abs(v[0])<=1,"Rapid retrigger");}
  e.stop();check(e.sample(s)[0]==0,"Stop must silence");
 }
 std::cout<<"Snap DSP checks passed at 44.1, 48 and 96 kHz\n";
}

