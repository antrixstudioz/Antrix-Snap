#include <juce_audio_utils/juce_audio_utils.h>
#include "BinaryData.h"
#include "SnapSamples.h"
#include "Engine.h"
#include <atomic>
class SnapEditor;
class SnapProcessor final:public juce::AudioProcessor {
public:
 juce::AudioProcessorValueTreeState state;
 snap::Engine engine;
 std::atomic<unsigned> clicked{0},hits{0};
 std::array<std::atomic<float>,128> wave{};
 unsigned observed=0; int wavePos=128,waveDivider=0,waveStride=338; float wavePeak=0;
 static juce::AudioProcessorValueTreeState::ParameterLayout layout(){
  juce::AudioProcessorValueTreeState::ParameterLayout p;
  auto add=[&](const char* id,const char* name,float lo,float hi,float def){
   p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(lo,hi),def));};
  add("tone","Tone",0,1,.55f);add("human","Humanize",0,1,.5f);
  add("pitch","Pitch",-12,12,0);add("velocity","Velocity",0,1,.8f);
  add("space","Space",0,1,.03f);add("width","Width",0,1,.5f);add("mix","Level",0,1,1);
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"burst",1},"Snap Burst",juce::StringArray{"Single","Double","Triple","Flam"},0));
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"style",1},"Variation",juce::StringArray{"Natural","Soft","Sharp","Deep","Bright","Airy","Random"},0));
  return p;
 }
 SnapProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"Snap",layout()){
  for(auto& v:wave)v.store(0);
  const bool loaded=engine.loadBank(SnapSamples::snaps_bank,size_t(SnapSamples::snaps_bankSize));
  jassert(loaded);juce::ignoreUnused(loaded);
  engine.setSeed(static_cast<uint32_t>(juce::Random::getSystemRandom().nextInt()));
 }
 snap::Settings settings()const {
  auto v=[&](const char* id){return state.getRawParameterValue(id)->load();};
  snap::Settings s;s.tone=v("tone");s.human=v("human");s.pitch=v("pitch");s.velocity=v("velocity");
  s.space=v("space");s.width=v("width");s.mix=v("mix");s.burst=int(v("burst"));s.style=int(v("style"));return s;
 }
 void trigger(const snap::Settings& s,float v){engine.trigger(s,v);hits.fetch_add(1);wavePos=waveDivider=0;wavePeak=0;for(auto& bin:wave)bin.store(0);}
 void prepareToPlay(double r,int)override {engine.prepare(r);observed=clicked.load();waveStride=juce::jmax(1,int(r*.9/128));wavePos=128;}
 void releaseResources()override{}
 bool isBusesLayoutSupported(const BusesLayout& b)const override{
  return b.getMainInputChannelSet().isDisabled()&&(b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()||b.getMainOutputChannelSet()==juce::AudioChannelSet::mono());
 }
 void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)override{
  juce::ScopedNoDenormals guard;buffer.clear();const auto s=settings();
  const auto clicks=clicked.load();if(clicks!=observed){observed=clicks;trigger(s,1);}
  auto it=midi.cbegin();
  for(int i=0;i<buffer.getNumSamples();++i){
   while(it!=midi.cend()&&(*it).samplePosition<=i){
    const auto m=(*it).getMessage();
    if(m.isNoteOn())trigger(s,m.getFloatVelocity());
    if(m.isAllSoundOff()||m.isAllNotesOff())engine.stop();
    ++it;
   }
   const auto y=engine.sample(s);
   buffer.setSample(0,i,buffer.getNumChannels()==1?(y[0]+y[1])*.5f:y[0]);
   if(buffer.getNumChannels()>1)buffer.setSample(1,i,y[1]);
   if(wavePos<128){wavePeak=juce::jmax(wavePeak,std::abs(y[0]),std::abs(y[1]));if(++waveDivider>=waveStride){waveDivider=0;wave[size_t(wavePos++)].store(wavePeak);wavePeak=0;}}
  }
  midi.clear();
 }
 const juce::String getName()const override{return "Snap";}
 bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 4;}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return "Natural Random";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock& out)override{auto xml=state.copyState().createXml();copyXmlToBinary(*xml,out);}
 void setStateInformation(const void* data,int size)override{auto xml=getXmlFromBinary(data,size);if(xml&&xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
};
class GoldLook final:public juce::LookAndFeel_V4{
public:
 void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider&)override{
  auto area=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(10);
  const float diameter=juce::jmin(area.getWidth(),area.getHeight());
  auto b=area.withSizeKeepingCentre(diameter,diameter);
  const auto centre=b.getCentre();const float radius=juce::jmin(b.getWidth(),b.getHeight())*.5f;
  juce::Path arc;arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
  g.setColour(juce::Colour(0xff514530));g.strokePath(arc,juce::PathStrokeType(3));
  juce::Path active;active.addCentredArc(centre.x,centre.y,radius,radius,0,start,start+value*(end-start),true);
  g.setColour(juce::Colour(0xffffd787));g.strokePath(active,juce::PathStrokeType(4));
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3d372d),b.getX(),b.getY(),juce::Colour(0xff030303),b.getRight(),b.getBottom(),false));
  g.fillEllipse(b.reduced(6));g.setColour(juce::Colour(0xffb79a61));g.drawEllipse(b.reduced(6),1);
  const auto angle=start+value*(end-start);juce::Path needle;
  needle.startNewSubPath(centre.x+std::sin(angle)*radius*.55f,centre.y-std::cos(angle)*radius*.55f);
  needle.lineTo(centre.x+std::sin(angle)*radius*.85f,centre.y-std::cos(angle)*radius*.85f);
  g.setColour(juce::Colour(0xffffe3a5));g.strokePath(needle,juce::PathStrokeType(3));
 }
};
class HandTrigger final:public juce::Button{
public:
 HandTrigger():juce::Button("Snap hand"){}
 void paintButton(juce::Graphics&,bool,bool)override{} // Hit target only: no border, hover fill or focus box.
 bool hitTest(int x,int y)override{
  const float nx=float(x)/float(getWidth()),ny=float(y)/float(getHeight());
  return nx>.15f&&nx<.88f&&(ny<.59f||nx>.39f);
 }
};
class SnapEditor final:public juce::AudioProcessorEditor,private juce::Timer{
 SnapProcessor& p;GoldLook look;
 juce::Image background=juce::ImageCache::getFromMemory(BinaryData::background_png,BinaryData::background_pngSize);
 juce::Image sheet=juce::ImageCache::getFromMemory(BinaryData::hand_png,BinaryData::hand_pngSize);
 std::array<juce::Slider,7> knobs;std::array<juce::Label,7> labels;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
 std::array<juce::TextButton,4> bursts;std::array<juce::TextButton,7> styles;
 HandTrigger hand;int frame=0;double began=0,fireAt=0;bool pending=false;unsigned seen=0;
 const juce::Colour gold{0xffffd787};
 void select(const char* id,int index){
  auto* param=p.state.getParameter(id);param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(float(index)));param->endChangeGesture();
 }
 void timerCallback()override{
  const double now=juce::Time::getMillisecondCounterHiRes();
  if(pending&&now>=fireAt){p.clicked.fetch_add(1);pending=false;}
  const auto count=p.hits.load();
  if(count!=seen){seen=count;if(now-began>240)began=now-105;}
  const double elapsed=now-began;
  frame=elapsed<50?1:elapsed<80?2:elapsed<105?3:elapsed<155?4:elapsed<205?5:elapsed<270?6:0;
  for(int i=0;i<4;++i)bursts[size_t(i)].setToggleState(int(p.state.getRawParameterValue("burst")->load())==i,juce::dontSendNotification);
  for(int i=0;i<7;++i)styles[size_t(i)].setToggleState(int(p.state.getRawParameterValue("style")->load())==i,juce::dontSendNotification);
  repaint();
 }
 void snapClick(){began=juce::Time::getMillisecondCounterHiRes();fireAt=began+105;pending=true;}
public:
 explicit SnapEditor(SnapProcessor& processor):AudioProcessorEditor(processor),p(processor){
  setLookAndFeel(&look);setSize(1152,768);setResizable(true,true);getConstrainer()->setFixedAspectRatio(1.5);setResizeLimits(900,600,1536,1024);
  const char* ids[]={"tone","human","pitch","velocity","space","width","mix"};
  const char* names[]={"TONE","HUMANIZE","PITCH","VELOCITY","SPACE","WIDTH","LEVEL"};
  for(size_t i=0;i<knobs.size();++i){
   auto& k=knobs[i];k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,65,18);
   k.setColour(juce::Slider::textBoxTextColourId,gold);k.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
   addAndMakeVisible(k);attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],k));
   k.textFromValueFunction=[i](double value){return i==2?juce::String(value,1)+" st":juce::String(int(std::round(value*100)))+"%";};
   k.valueFromTextFunction=[i](const juce::String& text){return text.getDoubleValue()/(i==2?1.0:100.0);};
   k.updateText();
   labels[i].setText(names[i],juce::dontSendNotification);labels[i].setColour(juce::Label::textColourId,gold);labels[i].setJustificationType(juce::Justification::centred);addAndMakeVisible(labels[i]);
  }
  const char* bn[]={"SINGLE","DOUBLE","TRIPLE","FLAM"};
  const char* sn[]={"NATURAL","SOFT","SHARP","DEEP","BRIGHT","AIRY","RANDOM"};
  auto setup=[&](juce::TextButton& b,const char* name){b.setButtonText(name);b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff0a0907));b.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff72501c));b.setColour(juce::TextButton::textColourOffId,gold);b.setColour(juce::TextButton::textColourOnId,juce::Colours::white);addAndMakeVisible(b);};
  for(int i=0;i<4;++i){setup(bursts[size_t(i)],bn[i]);bursts[size_t(i)].onClick=[this,i]{select("burst",i);};}
  for(int i=0;i<7;++i){setup(styles[size_t(i)],sn[i]);styles[size_t(i)].onClick=[this,i]{select("style",i);};}
  hand.setMouseCursor(juce::MouseCursor::PointingHandCursor);hand.setTitle("Snap hand");hand.setDescription("Click to generate a new finger snap");hand.onClick=[this]{snapClick();};addAndMakeVisible(hand);
  setWantsKeyboardFocus(true);seen=p.hits.load();startTimerHz(60);
 }
 ~SnapEditor()override{stopTimer();setLookAndFeel(nullptr);}
 bool keyPressed(const juce::KeyPress& k)override{if(k.getKeyCode()==juce::KeyPress::spaceKey){snapClick();return true;}return false;}
 void resized()override{
  const float sx=float(getWidth())/1280,sy=float(getHeight())/853;
  auto rect=[&](int x,int y,int w,int h){return juce::Rectangle<int>(int(x*sx),int(y*sy),int(w*sx),int(h*sy));};
  const int xs[]={45,242,45,242,863,1000,1137};const int ys[]={185,185,382,382,395,395,395};
  for(size_t i=0;i<7;++i){knobs[i].setBounds(rect(xs[i],ys[i]+8,i<4?128:107,i<4?127:110));labels[i].setBounds(rect(xs[i]-8,ys[i]-23,i<4?144:123,28));}
  for(int i=0;i<4;++i)bursts[size_t(i)].setBounds(rect(855+i*100,198,91,109));
  for(int i=0;i<7;++i)styles[size_t(i)].setBounds(rect(33+i*77,599,71,79));
  hand.setBounds(rect(422,225,414,552));
 }
 void paint(juce::Graphics& original)override{
  juce::Graphics::ScopedSaveState save(original);original.addTransform(juce::AffineTransform::scale(float(getWidth())/1280,float(getHeight())/853));auto& g=original;
  g.drawImage(background,juce::Rectangle<float>(0,0,1280,853));
  g.setColour(juce::Colour(0xff080807));
  // Replace decorative controls and unsupported sample-count copy with working UI.
  for(auto b:{juce::Rectangle<float>(29,165,165,154),{225,165,153,154},{29,362,165,154},{225,362,153,154},{850,365,403,154},{25,587,543,116},{12,754,367,74},{273,17,255,42},{789,14,362,47},{1168,13,98,46}})
   g.fillRoundedRectangle(b,8);
  g.setColour(gold);g.setFont(15);g.drawText("REAL SNAP ENGINE",285,22,240,28,juce::Justification::centredLeft);
  g.drawText("MIDI: ANY NOTE  /  CLICK HAND",798,23,348,26,juce::Justification::centred);
  g.drawText("ANTRIX  /  SNAP",1172,23,99,26,juce::Justification::centred);
  g.setFont(13);g.drawText("29 REAL SNAP RECORDINGS",39,770,310,23,juce::Justification::centredLeft);g.drawText("LAYERED VARIATIONS / NO REPEAT",39,793,310,20,juce::Justification::centredLeft);
  const int cw=sheet.getWidth()/4,ch=sheet.getHeight()/2;
  g.drawImage(sheet,422,225,414,552,(frame%4)*cw,(frame/4)*ch,cw,ch);
  if(frame==4){g.setColour(gold.withAlpha(.8f));for(int i=0;i<12;++i){const float a=float(i)*juce::MathConstants<float>::twoPi/12;g.drawLine(641+std::cos(a)*29,395+std::sin(a)*29,641+std::cos(a)*65,395+std::sin(a)*65,2);}}
  g.setColour(juce::Colour(0xff080807));g.fillRoundedRectangle(871,593,355,104,6);
  g.setColour(gold);juce::Path waveform;waveform.startNewSubPath(878,647);
  for(size_t i=0;i<128;++i){const float x=878+float(i)*2.67f,a=p.wave[i].load()*60;waveform.startNewSubPath(x,647-a);waveform.lineTo(x,647+a);}
  g.strokePath(waveform,juce::PathStrokeType(1.3f));
 }
};
juce::AudioProcessorEditor* SnapProcessor::createEditor(){return new SnapEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SnapProcessor();}

