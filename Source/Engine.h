#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace snap {
struct Settings { float tone=.55f, human=.5f, pitch=0, velocity=.8f, space=.15f, width=.5f, mix=1; int burst=0, style=0; };
class Engine {
 struct Voice { bool active=false; int delay=0; float age=0, phase=0, low=0, freq=1000, decay=.035f, gain=.7f, pan=0, bright=.6f; };
 std::array<Voice,24> voices{};
 std::array<float,32768> dl{},dr{};
 unsigned pos=0, next=0; uint32_t rng=0x12345678; double sr=48000;
 float random() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return float(rng & 0xffffff)/float(0xffffff); }
public:
 void prepare(double rate) { sr=rate>0?rate:48000; voices={}; dl.fill(0);dr.fill(0);pos=next=0; }
 void stop() { voices={};dl.fill(0);dr.fill(0); }
 void trigger(const Settings& s, float velocity=1) {
  const int count=s.burst==1?2:s.burst==2?3:s.burst==3?2:1;
  for(int i=0;i<count;++i) {
   auto& v=voices[next++%voices.size()];v=Voice{};v.active=true;
   const int style=s.style==6?int(random()*5.999f):s.style;
   const float detune=(random()-.5f)*s.human*4;
   v.freq=(style==3?650.f:1150.f)*std::pow(2.f,(s.pitch+detune)/12.f);
   v.decay=(style==1?.047f:style==2?.018f:style==5?.065f:.03f)*(1+(random()-.5f)*s.human*.6f);
   v.bright=std::clamp(s.tone+(style==4?.22f:style==3?-.22f:0.f),.05f,.98f);
   v.gain=velocity*s.velocity*(.8f+random()*.2f)*(style==1?.7f:1.f)*(i>0?.8f:1.f);
   v.pan=(random()-.5f)*s.width;v.delay=int(i*(s.burst==3?.018:.065)*sr);
  }
 }
 std::array<float,2> sample(const Settings& s) {
  float l=0,r=0;
  for(auto& v:voices) {
   if(!v.active)continue;
   if(v.delay>0){--v.delay;continue;}
   const float t=v.age/float(sr);
   if(t>.5f){v.active=false;continue;}
   const float noise=random()*2-1;
   v.low+= (.08f+v.bright*.7f)*(noise-v.low);
   v.phase+=6.2831853f*v.freq/float(sr);
   const float env=std::exp(-t/v.decay);
   const float attack=std::min(1.f,t/.00025f);
   const float body=std::sin(v.phase)*std::exp(-t/.009f);
   const float tick=(noise-v.low)*std::exp(-t/.0025f);
   const float y=(v.low*env*.68f+body*.27f+tick*.55f)*attack*v.gain;
   l+=y*(1-v.pan);r+=y*(1+v.pan);++v.age;
  }
  const unsigned a=(pos+32768-std::min(32000u,unsigned(sr*.039)))%32768;
  const unsigned b=(pos+32768-std::min(32000u,unsigned(sr*.053)))%32768;
  const float wl=dl[a],wr=dr[b];dl[pos]=l+wr*.32f;dr[pos]=r+wl*.32f;pos=(pos+1)%32768;
  l=(l+wl*s.space*.7f)*s.mix;r=(r+wr*s.space*.7f)*s.mix;
  return {std::tanh(l*.85f),std::tanh(r*.85f)};
 }
};
}

