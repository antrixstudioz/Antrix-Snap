#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <numeric>
namespace snap {
struct Settings {
 float tone=.55f,human=.5f,pitch=0,velocity=.85f,space=0,width=.5f,mix=.9f;
 float punch=.65f,delay=0,delayTime=.22f,feedback=.35f;
 int burst=0,style=0;
};
struct VoiceSpec {
 int first=0,second=0;double ratio=1,delaySeconds=0;
 float blend=.12f,gain=1,pan=0,tone=.55f,decay=0,attack=0,punch=.65f,width=1;
};
struct Recipe {Settings settings;std::array<VoiceSpec,3> voices{};int count=1,effectiveStyle=0;};
class Engine {
 struct Sample {std::vector<std::array<float,2>> frames;float brightness=0;};
 struct Voice {VoiceSpec spec;bool active=false;int delay=0;double position=0,step=1;std::array<float,2> low{};};
 struct Bag {std::array<int,128> pool{},order{};size_t size=0,cursor=0;};
 std::vector<Sample> bank;std::array<Voice,32> voices{};std::array<Bag,6> bags{};
 unsigned next=0;int lastPrimary=-1,lastSecondary=-1;uint32_t rng=0x12345678;double sr=48000,sourceRate=48000;
 float random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng&0xffffff)/16777216.f;}
 int choosePrimary(int style){
  auto& b=bags[size_t(style)];
  if(b.cursor>=b.size){b.order=b.pool;for(size_t n=b.size;n>1;--n)std::swap(b.order[n-1],b.order[size_t(random()*float(n))]);b.cursor=0;}
  if(b.size>1&&b.order[b.cursor]==lastPrimary){
   const auto other=b.cursor+1<b.size?b.cursor+1:size_t(0);std::swap(b.order[b.cursor],b.order[other]);
  }
  return b.order[b.cursor++];
 }
 float read(int index,double at,int c)const{
  const auto& f=bank[size_t(index)].frames;const auto i=size_t(at);if(i>=f.size())return 0;
  const float a=f[i][size_t(c)],b=i+1<f.size()?f[i+1][size_t(c)]:0;return a+(b-a)*float(at-double(i));
 }
public:
 static float limit(float x){const float a=std::abs(x);return a<=.92f?x:std::copysign(.92f+.08f*std::tanh((a-.92f)/.08f),x);}
 bool loadBank(const void* bytes,size_t size){
  if(!bytes||size<16)return false;const auto* data=static_cast<const uint8_t*>(bytes);constexpr char magic[]="SNAPBNK1";
  for(size_t i=0;i<8;++i)if(data[i]!=uint8_t(magic[i]))return false;
  size_t offset=8;auto u32=[&](){uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(data[offset++])<<(8*i);return n;};
  const auto count=u32(),rate=u32();if(count==0||count>128||rate<8000||rate>192000)return false;
  std::vector<Sample> decoded;decoded.reserve(count);
  for(uint32_t s=0;s<count;++s){
   if(size-offset<4)return false;const auto frames=u32();if(frames==0||frames>rate*2||uint64_t(frames)*4>size-offset)return false;
   Sample item;item.frames.resize(frames);float last=0;double energy=0,difference=0;
   for(auto& f:item.frames){for(auto& x:f){const int n=int(data[offset])|(int(data[offset+1])<<8);offset+=2;x=float(n>=32768?n-65536:n)/32768.f;}
    const float mid=(f[0]+f[1])*.5f;energy+=std::abs(mid);difference+=std::abs(mid-last);last=mid;}
   item.brightness=float(difference/(energy+1e-9));decoded.push_back(std::move(item));
  }
  if(offset!=size)return false;stop();bank=std::move(decoded);sourceRate=rate;lastPrimary=lastSecondary=-1;
  std::vector<int> order(count);std::iota(order.begin(),order.end(),0);
  auto fill=[&](int style,size_t n){auto& b=bags[size_t(style)];b.size=n;b.cursor=n;for(size_t i=0;i<n;++i)b.pool[i]=order[i];};
  fill(0,count);
  std::sort(order.begin(),order.end(),[&](int a,int b){return bank[size_t(a)].brightness<bank[size_t(b)].brightness;});
  fill(1,std::min(size_t(count),size_t(12)));fill(3,std::min(size_t(count),size_t(10)));
  std::reverse(order.begin(),order.end());fill(4,std::min(size_t(count),size_t(12)));
  std::sort(order.begin(),order.end(),[&](int a,int b){return bank[size_t(a)].frames.size()<bank[size_t(b)].frames.size();});
  fill(2,std::min(size_t(count),size_t(12)));std::reverse(order.begin(),order.end());fill(5,std::min(size_t(count),size_t(10)));
  return true;
 }
 size_t sampleCount()const{return bank.size();}int primaryIndex()const{return lastPrimary;}int secondaryIndex()const{return lastSecondary;}
 void setSeed(uint32_t seed){rng=seed?seed:1;for(auto& b:bags)b.cursor=b.size;lastPrimary=-1;}
 void prepare(double rate){sr=rate>0?rate:48000;stop();next=0;}void stop(){voices={};}
 bool isActive()const{for(const auto& v:voices)if(v.active)return true;return false;}
 Recipe makeRecipe(const Settings& s,float velocity=1){
  Recipe r;r.settings=s;r.count=bank.empty()?0:s.burst==1?2:s.burst==2?3:s.burst==3?2:1;
  const int style=s.style==6?int(random()*6):std::clamp(s.style,0,5);r.effectiveStyle=style;
  for(int i=0;i<r.count;++i){auto& v=r.voices[size_t(i)];v.first=choosePrimary(style);lastPrimary=v.first;
   v.second=bank.size()>1?(v.first+1+int(random()*float(bank.size()-1)))%int(bank.size()):v.first;lastSecondary=v.second;
   v.blend=.08f+random()*(.06f+.12f*s.human)+(style==5?.1f:0);
   const float detune=(random()-.5f)*.36f*s.human;
   v.ratio=std::pow(2.,double(s.pitch+detune+(style==3?-3.f:style==4?.8f:0.f))/12.);
   v.gain=1.22f*std::clamp(velocity,0.f,1.f)*s.velocity*s.mix*(1-random()*.08f*s.human)*(style==1?.72f:1.f)*(i>0?.82f:1.f);
   v.tone=std::clamp(s.tone+(style==1?-.28f:style==3?-.16f:style==4?.3f:style==5?.18f:0.f),0.f,1.f);
   v.decay=style==2?30.f:0;v.attack=style==1?.0012f:0;
   v.punch=std::clamp(s.punch+(style==2?.25f:style==1?-.4f:0.f),0.f,1.f);
   v.width=std::min(1.5f,s.width*2)*(style==5?1.2f:1.f);v.pan=(random()-.5f)*s.width*.2f;
   v.delaySeconds=i*(s.burst==3?.018:.065);
  }return r;
 }
 void playRecipe(const Recipe& r){for(int i=0;i<r.count;++i){const auto& spec=r.voices[size_t(i)];
  if(spec.first<0||size_t(spec.first)>=bank.size()||spec.second<0||size_t(spec.second)>=bank.size())continue;
  auto& v=voices[next++%voices.size()];v=Voice{};v.spec=spec;v.active=true;v.step=sourceRate/sr*spec.ratio;v.delay=int(spec.delaySeconds*sr);
 }}
 void trigger(const Settings& s,float velocity=1){playRecipe(makeRecipe(s,velocity));}
 std::array<float,2> sample(const Settings&){
  float l=0,r=0;const float alpha=1-std::exp(-6.2831853f*3500.f/float(sr));
  for(auto& v:voices){if(!v.active)continue;if(v.delay>0){--v.delay;continue;}const auto& q=v.spec;
   if(v.position>=double(std::max(bank[size_t(q.first)].frames.size(),bank[size_t(q.second)].frames.size()))){v.active=false;continue;}
   const float t=float(v.position/sourceRate/q.ratio);const float env=std::exp(-std::max(0.f,t-.014f)*q.decay)*(q.attack>0?std::min(1.f,t/q.attack):1.f)*(1+.48f*q.punch*std::exp(-t/.006f));
   std::array<float,2> y{};
   for(int c=0;c<2;++c){const float dry=read(q.first,v.position,c)*(1-q.blend)+read(q.second,v.position,c)*q.blend;v.low[size_t(c)]+=alpha*(dry-v.low[size_t(c)]);
    const float shaped=q.tone<.55f?dry+(v.low[size_t(c)]-dry)*(.55f-q.tone)*1.7f:dry+(dry-v.low[size_t(c)])*(q.tone-.55f)*1.1f;
    y[size_t(c)]=shaped*q.gain*env;}
   const float mid=(y[0]+y[1])*.5f,side=(y[0]-y[1])*.5f*q.width;
   l+=(mid+side)*(1-q.pan);r+=(mid-side)*(1+q.pan);v.position+=v.step;
  }return {limit(l),limit(r)};
 }
};
}
