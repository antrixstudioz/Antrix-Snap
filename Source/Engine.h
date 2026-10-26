#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstddef>
namespace snap {
struct Settings { float tone=.55f, human=.5f, pitch=0, velocity=.8f, space=.03f, width=.5f, mix=1; int burst=0, style=0; };
class Engine {
 struct Sample { std::vector<std::array<float,2>> frames; };
 struct Voice {
  bool active=false; int delay=0, first=0, second=0;
  double position=0, step=1;
  float blend=.12f, gain=.7f, pan=0, tone=.55f, release=1;
  std::array<float,2> low{};
 };
 std::vector<Sample> bank;
 std::array<Voice,24> voices{};
 std::array<float,32768> dl{},dr{};
 std::array<int,128> bag{};
 unsigned pos=0,next=0; size_t bagPosition=0;
 int lastPrimary=-1,lastSecondary=-1;
 uint32_t rng=0x12345678; double sr=48000, sourceRate=48000;
 float random() { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng&0xffffff)/16777216.f; }
 int choosePrimary() {
  if(bagPosition>=bank.size()){
   for(size_t i=0;i<bank.size();++i)bag[i]=int(i);
   for(size_t n=bank.size();n>1;--n)std::swap(bag[n-1],bag[size_t(random()*float(n))]);
   if(bank.size()>1&&bag[0]==lastPrimary)std::swap(bag[0],bag[1]);
   bagPosition=0;
  }
  return bag[bagPosition++];
 }
 float read(int index,double position,int channel)const {
  const auto& frames=bank[size_t(index)].frames;
  const auto i=size_t(position);
  if(i>=frames.size())return 0;
  const float a=frames[i][size_t(channel)];
  const float b=i+1<frames.size()?frames[i+1][size_t(channel)]:0;
  return a+(b-a)*float(position-double(i));
 }
public:
 // Only call while audio is stopped. Decode and allocate once, never in sample/trigger.
 bool loadBank(const void* bytes,size_t size) {
  if(!bytes||size<16)return false;
  const auto* data=static_cast<const uint8_t*>(bytes);
  constexpr char magic[]="SNAPBNK1";
  for(size_t i=0;i<8;++i)if(data[i]!=uint8_t(magic[i]))return false;
  size_t offset=8;
  auto u32=[&](){uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(data[offset++])<<(8*i);return n;};
  const auto count=u32(),rate=u32();
  if(count==0||count>128||rate<8000||rate>192000)return false;
  std::vector<Sample> decoded;decoded.reserve(count);
  for(uint32_t s=0;s<count;++s){
   if(size-offset<4)return false;
   const auto frames=u32();
   if(frames==0||frames>rate*2||uint64_t(frames)*4>size-offset)return false;
   Sample item;item.frames.resize(frames);
   for(auto& frame:item.frames)for(auto& x:frame){
    const int n=int(data[offset])|(int(data[offset+1])<<8);offset+=2;
    x=float(n>=32768?n-65536:n)/32768.f;
   }
   decoded.push_back(std::move(item));
  }
  if(offset!=size)return false;
  stop();bank=std::move(decoded);sourceRate=rate;bagPosition=bank.size();lastPrimary=lastSecondary=-1;return true;
 }
 size_t sampleCount()const{return bank.size();}
 int primaryIndex()const{return lastPrimary;}
 int secondaryIndex()const{return lastSecondary;}
 void setSeed(uint32_t seed){rng=seed?seed:1;bagPosition=bank.size();}
 void prepare(double rate){sr=rate>0?rate:48000;stop();next=0;}
 void stop(){voices={};dl.fill(0);dr.fill(0);pos=0;}
 void trigger(const Settings& s,float velocity=1){
  if(bank.empty())return;
  const int count=s.burst==1?2:s.burst==2?3:s.burst==3?2:1;
  for(int i=0;i<count;++i){
   auto& v=voices[next++%voices.size()];v=Voice{};v.active=true;
   v.first=choosePrimary();lastPrimary=v.first;
   v.second=bank.size()>1?(v.first+1+int(random()*float(bank.size()-1)))%int(bank.size()):v.first;
   lastSecondary=v.second;
   const int style=s.style==6?int(random()*6):s.style;
   // Preserve a dominant real attack. A quieter aligned recording changes its texture.
   v.blend=.08f+random()*(.06f+.12f*s.human);
   const float detune=(random()-.5f)*.36f*s.human;
   const float colourPitch=style==3?-2.f:style==4?.5f:0.f;
   v.step=sourceRate/sr*std::pow(2.,double(s.pitch+detune+colourPitch)/12.);
   v.gain=std::clamp(velocity,0.f,1.f)*s.velocity*(1.f-random()*.12f*s.human)*(style==1?.78f:1.f)*(i>0?.82f:1.f);
   v.tone=std::clamp(s.tone+(style==1?-.12f:style==4?.12f:0.f),0.f,1.f);
   v.release=style==2?2.f:1.f;
   v.pan=(random()-.5f)*s.width*.25f;
   v.delay=int(i*(s.burst==3?.018:.065)*sr);
  }
 }
 std::array<float,2> sample(const Settings& s){
  float l=0,r=0;
  const float alpha=1.f-std::exp(-6.2831853f*3500.f/float(sr));
  for(auto& v:voices){
   if(!v.active)continue;
   if(v.delay>0){--v.delay;continue;}
   if(v.position>=double(std::max(bank[size_t(v.first)].frames.size(),bank[size_t(v.second)].frames.size()))){v.active=false;continue;}
   std::array<float,2> y{};
   for(int c=0;c<2;++c){
    const float dry=read(v.first,v.position,c)*(1-v.blend)+read(v.second,v.position,c)*v.blend;
    v.low[size_t(c)]+=alpha*(dry-v.low[size_t(c)]);
    // Neutral Tone is transparent; no oscillator/noise layer and no saturation.
    float shaped=dry;
    if(v.tone<.55f)shaped=dry+(v.low[size_t(c)]-dry)*(.55f-v.tone)*1.5f;
    else shaped=dry+(dry-v.low[size_t(c)])*(v.tone-.55f)*.65f;
    const float t=float(v.position/sourceRate);
    const float envelope=v.release>1?std::exp(-std::max(0.f,t-.012f)*12.f):1.f;
    y[size_t(c)]=shaped*v.gain*envelope;
   }
   const float mid=(y[0]+y[1])*.5f,side=(y[0]-y[1])*.5f*std::min(1.5f,s.width*2);
   l+=(mid+side)*(1-v.pan);r+=(mid-side)*(1+v.pan);
   v.position+=v.step;
  }
  const unsigned a=(pos+32768-std::min(32000u,unsigned(sr*.039)))%32768;
  const unsigned b=(pos+32768-std::min(32000u,unsigned(sr*.053)))%32768;
  const float wl=dl[a],wr=dr[b];dl[pos]=l+wr*.28f;dr[pos]=r+wl*.28f;pos=(pos+1)%32768;
  return {std::clamp((l+wl*s.space*.5f)*s.mix,-1.f,1.f),std::clamp((r+wr*s.space*.5f)*s.mix,-1.f,1.f)};
 }
};
}

