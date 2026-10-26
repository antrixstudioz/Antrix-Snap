#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
class SnapEffects {
 juce::Reverb reverb;std::vector<float> left,right;size_t position=0,delaySamples=1;double rate=48000;
 float delayMix=0,feedback=.35f,reverbMix=0,reverbWidth=1;
public:
 void prepare(double sr){
  rate=sr;left.assign(size_t(sr*1.1)+1,0);right=left;position=0;
  // Initialise the wet-only tank before resetting JUCE's gain smoothers.
  // The dry signal stays outside the reverb, including its very first sample.
  juce::Reverb::Parameters p;p.roomSize=.58f;p.damping=.58f;p.width=1;p.freezeMode=0;p.wetLevel=.42f;p.dryLevel=0;
  reverb.setParameters(p);reverb.setSampleRate(sr);reverb.reset();
 }
 void reset(){std::fill(left.begin(),left.end(),0);std::fill(right.begin(),right.end(),0);position=0;reverb.reset();}
 void configure(const snap::Settings& s){
  delaySamples=std::clamp(size_t(s.delayTime*rate),size_t(1),left.size()-1);delayMix=s.delay;feedback=std::clamp(s.feedback,0.f,.55f);
  reverbMix=s.space;reverbWidth=std::min(1.f,s.width*2);
 }
 std::array<float,2> process(std::array<float,2> x){
  const size_t index=(position+left.size()-delaySamples)%left.size();const float dl=left[index],dr=right[index];
  left[position]=x[0]+dr*feedback;right[position]=x[1]+dl*feedback;position=(position+1)%left.size();
  x[0]+=dl*delayMix*.7f;x[1]+=dr*delayMix*.7f;
  auto wet=x;reverb.processStereo(&wet[0],&wet[1],1);
  const float mid=(wet[0]+wet[1])*.5f,side=(wet[0]-wet[1])*.5f*reverbWidth;
  x[0]+=(mid+side)*reverbMix;x[1]+=(mid-side)*reverbMix;
  return {snap::Engine::limit(x[0]),snap::Engine::limit(x[1])};
 }
};
