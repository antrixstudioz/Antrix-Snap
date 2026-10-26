#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "SnapSamples.h"
#include "History.h"
class SnapProcessor final:public juce::AudioProcessor,private juce::MidiInputCallback {
 snap::Engine engine;SnapEffects effects;juce::MidiMessageCollector collector;juce::MidiBuffer externalMidi;
 std::unique_ptr<juce::MidiInput> midiInput;juce::String midiIdentifier;
 std::atomic<unsigned> requests{0};SnapHistory::Preview* preview=nullptr;double previewPosition=0,rate=48000;
 void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& m)override{collector.addMessageToQueue(m);}
 void retirePreview(){if(preview&&history.canTakePreview()){history.retire(preview);preview=nullptr;replayProgress.store(-1);}}
 void generate(const snap::Settings& s,float velocity){const auto recipe=engine.makeRecipe(s,velocity);history.record(recipe);engine.playRecipe(recipe);hits.fetch_add(1);retirePreview();}
public:
 juce::AudioProcessorValueTreeState state;juce::MidiKeyboardState keyboard;SnapHistory history;
 std::atomic<unsigned> hits{0},midiHits{0};std::atomic<float> replayProgress{-1};
 static juce::AudioProcessorValueTreeState::ParameterLayout layout(){
  juce::AudioProcessorValueTreeState::ParameterLayout p;
  auto add=[&](const char* id,const char* name,float lo,float hi,float def){p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(lo,hi),def));};
  add("tone","Tone",0,1,.55f);add("human","Humanize",0,1,.5f);add("pitch","Pitch",-12,12,0);add("velocity","Velocity",0,1,.85f);
  add("space","Reverb",0,1,0);add("width","Width",0,1,.5f);add("mix","Level",0,1,.9f);add("punch","Punch",0,1,.65f);
  add("delay","Delay",0,1,0);add("delayTime","Delay Time",.06f,.6f,.22f);
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"burst",1},"Snap Burst",juce::StringArray{"Single","Double","Triple","Flam"},0));
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"style",1},"Variation",juce::StringArray{"Natural","Soft","Sharp","Deep","Bright","Airy","Random"},0));return p;
 }
 explicit SnapProcessor(juce::File folder=SnapHistory::defaultFolder()):AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"Snap",layout()),history(SnapSamples::snaps_bank,size_t(SnapSamples::snaps_bankSize),std::move(folder)){
  const bool loaded=engine.loadBank(SnapSamples::snaps_bank,size_t(SnapSamples::snaps_bankSize));jassert(loaded);juce::ignoreUnused(loaded);
  engine.setSeed(static_cast<uint32_t>(juce::Random::getSystemRandom().nextInt()));collector.reset(48000);
 }
 ~SnapProcessor()override{if(midiInput)midiInput->stop();retirePreview();}
 snap::Settings settings()const{auto v=[&](const char* id){return state.getRawParameterValue(id)->load();};snap::Settings s;
  s.tone=v("tone");s.human=v("human");s.pitch=v("pitch");s.velocity=v("velocity");s.space=v("space");s.width=v("width");s.mix=v("mix");
  s.punch=v("punch");s.delay=v("delay");s.delayTime=v("delayTime");s.burst=int(v("burst"));s.style=int(v("style"));return s;
 }
 void requestNew(){history.followLatest.store(true);requests.fetch_add(1);}
 void requestReplay(){if(history.display().file==juce::File{})requestNew();else history.replay();}
 bool standalone()const{return wrapperType==wrapperType_Standalone;}
 juce::String selectedMidi()const{return midiIdentifier;}
 bool selectMidi(const juce::String& identifier){
  if(!standalone())return false;if(midiInput)midiInput->stop();midiInput.reset();midiIdentifier={};
  if(identifier.isEmpty())return true;midiInput=juce::MidiInput::openDevice(identifier,this);
  if(!midiInput)return false;midiIdentifier=identifier;midiInput->start();return true;
 }
 void prepareToPlay(double sr,int)override{rate=sr>0?sr:48000;engine.prepare(rate);effects.prepare(rate);collector.reset(rate);externalMidi.ensureSize(8192);retirePreview();}
 void releaseResources()override{engine.stop();retirePreview();}
 bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainInputChannelSet().isDisabled()&&(b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()||b.getMainOutputChannelSet()==juce::AudioChannelSet::mono());}
 void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)override{
  juce::ScopedNoDenormals guard;buffer.clear();const int n=buffer.getNumSamples();if(n<=0||buffer.getNumChannels()==0)return;
  const auto s=settings();effects.configure(s);externalMidi.clear();collector.removeNextBlockOfMessages(externalMidi,n);midi.addEvents(externalMidi,0,n,0);keyboard.processNextMidiBuffer(midi,0,n,true);
  if(history.canTakePreview()){if(auto* clip=history.takePreview()){retirePreview();preview=clip;previewPosition=0;engine.stop();effects.reset();hits.fetch_add(1);}}
  const auto wanted=requests.exchange(0);for(unsigned i=0;i<wanted;++i)generate(s,1);
  auto it=midi.cbegin();
  for(int i=0;i<n;++i){while(it!=midi.cend()&&(*it).samplePosition<=i){const auto m=(*it).getMessage();if(m.isNoteOn()){generate(s,m.getFloatVelocity());midiHits.fetch_add(1);}if(m.isAllSoundOff()||m.isAllNotesOff()){engine.stop();effects.reset();retirePreview();}++it;}
   auto y=effects.process(engine.sample(s));
   if(preview){const int at=int(previewPosition),end=preview->audio.getNumSamples();
    if(at>=end){retirePreview();replayProgress.store(-1);}else{const float fraction=float(previewPosition-at);for(int c=0;c<2;++c){const float a=preview->audio.getSample(c,at),b=at+1<end?preview->audio.getSample(c,at+1):0;y[size_t(c)]+=a+(b-a)*fraction;}
     previewPosition+=preview->sampleRate/rate;replayProgress.store(float(previewPosition/end));}}
   buffer.setSample(0,i,snap::Engine::limit(buffer.getNumChannels()==1?(y[0]+y[1])*.5f:y[0]));if(buffer.getNumChannels()>1)buffer.setSample(1,i,snap::Engine::limit(y[1]));
  }midi.clear();
 }
 const juce::String getName()const override{return "Snap";}bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 8;}bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return "Snap";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock& out)override{auto tree=state.copyState();tree.setProperty("selectedClip",history.display().file.getFileName(),nullptr);auto xml=tree.createXml();copyXmlToBinary(*xml,out);}
 void setStateInformation(const void* data,int size)override{auto xml=getXmlFromBinary(data,size);if(xml&&xml->hasTagName(state.state.getType())){auto tree=juce::ValueTree::fromXml(*xml);state.replaceState(tree);history.restoreSelection(tree.getProperty("selectedClip").toString());}}
};
