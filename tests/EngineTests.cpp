#include "Engine.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <set>
#include <memory>
#ifndef SNAP_BANK_PATH
#define SNAP_BANK_PATH "Assets/snaps.bank"
#endif
void check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<"\n";std::exit(1);}}
int main(){
 std::ifstream input(SNAP_BANK_PATH,std::ios::binary);
 std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
 check(!bytes.empty(),"Embedded sample bank missing");
 auto engine=std::make_unique<snap::Engine>();auto& e=*engine;
 check(e.loadBank(bytes.data(),bytes.size()),"Sample bank must decode");
 check(e.sampleCount()==29,"All 29 supplied recordings must be included");
 check(!e.loadBank(bytes.data(),bytes.size()-1),"Truncated bank must be rejected");
 check(e.sampleCount()==29,"Invalid load must preserve existing bank");
 snap::Settings s;s.space=0;s.human=0;e.setSeed(42);e.prepare(48000);
 int previous=-1;
 for(int cycle=0;cycle<4;++cycle){std::set<int> used;
  for(int n=0;n<29;++n){e.trigger(s);check(e.primaryIndex()!=previous,"Adjacent primary must not repeat");
   check(e.secondaryIndex()!=e.primaryIndex(),"Two layers must be different recordings");
   used.insert(e.primaryIndex());previous=e.primaryIndex();}
  check(used.size()==29,"Shuffle bag must cover every recording exactly once");
 }
 for(double rate:{44100.,48000.,96000.}){
  e.prepare(rate);check(e.sample(s)[0]==0,"Must start silent");
  for(int style=0;style<7;++style)for(int burst=0;burst<4;++burst){
   s.style=style;s.burst=burst;s.pitch=style%2?-12:12;s.tone=style%2?0:1;s.space=1;s.width=1;s.human=1;
   e.trigger(s);double energy=0,tail=0;
   for(int i=0;i<int(rate*4);++i){auto v=e.sample(s);for(float x:v){
    check(std::isfinite(x)&&std::abs(x)<=1,"Invalid or unbounded output");energy+=x*x;if(i>rate*3.8)tail+=x*x;}}
   check(energy>.001,"Actual sample playback must be audible");check(tail<.00001,"Release must decay");
  }
  for(int n=0;n<100;++n)e.trigger(s);
  for(int i=0;i<5000;++i){auto v=e.sample(s);check(std::isfinite(v[0])&&std::abs(v[0])<=1,"Rapid retrigger safety");}
  e.stop();check(e.sample(s)[0]==0,"Stop must silence");
 }
 // A silent PCM bank must remain silent: no synthesized rim/noise body may leak in.
 std::vector<uint8_t> silence(20+4000,0);
 const char magic[]="SNAPBNK1";std::copy(magic,magic+8,silence.begin());
 auto put=[&](int offset,uint32_t n){for(int i=0;i<4;++i)silence[size_t(offset+i)]=uint8_t(n>>(8*i));};
 put(8,1);put(12,48000);put(16,1000);
 check(e.loadBank(silence.data(),silence.size()),"Silent fixture must load");
 e.prepare(48000);e.trigger(s);
 for(int i=0;i<100000;++i){auto v=e.sample(s);check(v[0]==0&&v[1]==0,"Must use recordings only");}
 std::cout<<"29-sample bank, shuffle coverage, distinct layers, PCM-only audio, tails and output safety passed at 44.1/48/96 kHz\n";
}

