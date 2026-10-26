#pragma once
#include "Processor.h"
#include "BinaryData.h"
class GoldLook final:public juce::LookAndFeel_V4 {
public:
 GoldLook(){setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff100e0b));setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff75603c));setColour(juce::ComboBox::textColourId,juce::Colour(0xfff1d294));setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff16120c));setColour(juce::PopupMenu::textColourId,juce::Colour(0xfff1d294));}
 juce::Font getTextButtonFont(juce::TextButton&,int h)override{return juce::Font(juce::FontOptions(float(juce::jmin(12,h-5)))) ;}
 juce::Font getComboBoxFont(juce::ComboBox&)override{return juce::Font(juce::FontOptions(11));}
 void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider&)override{
  auto area=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(4);const float d=juce::jmin(area.getWidth(),area.getHeight());auto b=area.withSizeKeepingCentre(d,d);auto c=b.getCentre();const float r=d*.5f;
  juce::Path arc;arc.addCentredArc(c.x,c.y,r,r,0,start,end,true);g.setColour(juce::Colour(0xff443a27));g.strokePath(arc,juce::PathStrokeType(2));
  juce::Path active;active.addCentredArc(c.x,c.y,r,r,0,start,start+value*(end-start),true);g.setColour(juce::Colour(0xffffd487));g.strokePath(active,juce::PathStrokeType(2.8f));
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff484035),b.getX(),b.getY(),juce::Colour(0xff060605),b.getRight(),b.getBottom(),false));g.fillEllipse(b.reduced(4));
  g.setColour(juce::Colour(0xff91794a));g.drawEllipse(b.reduced(4),.8f);const float angle=start+value*(end-start);
  g.setColour(juce::Colour(0xffffe3a5));g.drawLine(c.x+std::sin(angle)*r*.52f,c.y-std::cos(angle)*r*.52f,c.x+std::sin(angle)*r*.81f,c.y-std::cos(angle)*r*.81f,2);
 }
};
class HandTrigger final:public juce::Button {
public:
 HandTrigger():juce::Button("Snap hand"){}void paintButton(juce::Graphics&,bool,bool)override{}
 bool hitTest(int x,int y)override{const float nx=float(x)/float(getWidth()),ny=float(y)/float(getHeight());return nx>.16f&&nx<.9f&&ny>.01f;}
};
class SnapWaveform final:public juce::Component {
 SnapProcessor& p;bool dragged=false;double down=0;
public:
 SnapHistory::Display clip;
 explicit SnapWaveform(SnapProcessor& processor):p(processor){setName("Snap preview");setMouseCursor(juce::MouseCursor::PointingHandCursor);setTitle("Replay or drag current snap");}
 juce::File exportFile()const{return clip.file;}
 void paint(juce::Graphics& g)override{
  auto b=getLocalBounds().toFloat();g.setColour(juce::Colour(0xe8080705));g.fillRoundedRectangle(b,7);g.setColour(juce::Colour(0xff846c3e));g.drawRoundedRectangle(b.reduced(.5f),7,1);
  if(clip.file==juce::File{}){g.setColour(juce::Colour(0xffb69c69));g.setFont(11);g.drawText("Click the hand to create a snap",getLocalBounds(),juce::Justification::centred);return;}
  const float cx=b.getWidth()-16,mid=b.getHeight()*.43f;g.setColour(juce::Colour(0xffffd78b));
  for(size_t i=0;i<clip.peaks.size();++i){const float x=8+float(i)/float(clip.peaks.size()-1)*cx,a=std::max(.5f,clip.peaks[i]*mid*.85f);g.drawLine(x,mid-a,x,mid+a,1);}
  const float progress=p.replayProgress.load();if(progress>=0&&progress<=1){g.setColour(juce::Colours::white.withAlpha(.8f));g.drawLine(8+progress*cx,4,8+progress*cx,b.getHeight()-16,1);}
  g.setColour(juce::Colour(0xffb69c69));g.setFont(9.5f);g.drawText("CLICK: REPLAY   /   HOLD + DRAG: WAV",4,int(b.getHeight())-16,int(b.getWidth())-8,13,juce::Justification::centred);
 }
 void mouseDown(const juce::MouseEvent&)override{dragged=false;down=juce::Time::getMillisecondCounterHiRes();}
 void mouseDrag(const juce::MouseEvent& e)override{
  if(!dragged&&e.getDistanceFromDragStart()>6&&juce::Time::getMillisecondCounterHiRes()-down>120&&clip.file.existsAsFile()){
   dragged=juce::DragAndDropContainer::performExternalDragDropOfFiles({clip.file.getFullPathName()},false,this);
  }
 }
 void mouseUp(const juce::MouseEvent& e)override{if(!dragged&&getLocalBounds().contains(e.getPosition()))p.requestReplay();}
};
class SnapEditor final:public juce::AudioProcessorEditor,public juce::DragAndDropContainer,private juce::Timer,private juce::KeyListener {
 SnapProcessor& p;GoldLook look;juce::TooltipWindow tips{this,650};
 juce::Image background=juce::ImageCache::getFromMemory(BinaryData::scene_png,BinaryData::scene_pngSize);
 juce::Image sheet=juce::ImageCache::getFromMemory(BinaryData::hand_png,BinaryData::hand_pngSize);
 std::array<juce::Image,8> poses;
 std::array<juce::Slider,10> knobs;std::array<juce::Label,10> labels;std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
 std::array<juce::TextButton,4> bursts;std::array<juce::TextButton,7> styles;
 HandTrigger hand;SnapWaveform wave;juce::ComboBox historyBox,midiBox;juce::TextButton previous{"<"},next{">"},folder{"Folder"};
 juce::MidiKeyboardComponent piano;std::vector<SnapHistory::Entry> entries;juce::Array<juce::MidiDeviceInfo> devices;
 std::array<bool,13> held{};const juce::String keyMap="awsedftgyhujk";
 unsigned seenHits=0,seenHistory=~0u;int frames=0;double strike=-10000,scheduled=-1,lastManual=-10000;
 const juce::Colour gold{0xffffd487};
 void selectParameter(const char* id,int index){auto* param=p.state.getParameter(id);param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(float(index)));param->endChangeGesture();grabKeyboardFocus();}
 void setupButton(juce::TextButton& b){b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff110e09));b.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff725123));b.setColour(juce::TextButton::textColourOffId,gold);b.setColour(juce::TextButton::textColourOnId,juce::Colours::white);b.setWantsKeyboardFocus(false);addAndMakeVisible(b);b.addKeyListener(this);}
 void refreshHistory(){
  const auto revision=p.history.revision.load();if(revision==seenHistory)return;seenHistory=revision;
  auto updated=p.history.entries();if(updated.size()!=entries.size()){entries=std::move(updated);historyBox.clear(juce::dontSendNotification);for(size_t i=0;i<entries.size();++i)historyBox.addItem(entries[i].label,int(i+1));}
  wave.clip=p.history.display();for(size_t i=0;i<entries.size();++i)if(entries[i].file==wave.clip.file){historyBox.setSelectedId(int(i+1),juce::dontSendNotification);break;}wave.repaint();
 }
 void browse(int delta){const int target=std::clamp(historyBox.getSelectedId()-1+delta,0,juce::jmax(0,int(entries.size())-1));if(!entries.empty())historyBox.setSelectedId(target+1,juce::sendNotificationSync);}
 void refreshMidi(){
  if(!p.standalone())return;const auto available=juce::MidiInput::getAvailableDevices();if(available==devices)return;devices=available;
  midiBox.clear(juce::dontSendNotification);midiBox.addItem("No MIDI device",1);int selected=1;for(int i=0;i<devices.size();++i){midiBox.addItem(devices[i].name,i+2);if(devices[i].identifier==p.selectedMidi())selected=i+2;}midiBox.setSelectedId(selected,juce::dontSendNotification);
 }
 bool handleKey(const juce::KeyPress& k,juce::Component* origin){
  if(dynamic_cast<juce::TextEditor*>(origin)!=nullptr)return false;
  if(k.getModifiers().isCommandDown()||k.getModifiers().isCtrlDown()||k.getModifiers().isAltDown())return false;
  if(k.getKeyCode()==juce::KeyPress::spaceKey){p.requestReplay();return true;}
  const auto character=k.getTextCharacter()!=0?k.getTextCharacter():juce::juce_wchar(k.getKeyCode());
  const int index=keyMap.indexOfChar(juce::CharacterFunctions::toLowerCase(character));if(index<0)return false;
  if(!held[size_t(index)]){held[size_t(index)]=true;p.history.followLatest.store(true);p.keyboard.noteOn(1,60+index,.9f);}return true;
 }
 bool keyPressed(const juce::KeyPress& k,juce::Component* origin)override{return handleKey(k,origin);}
 bool keyStateChanged(bool,juce::Component*)override{bool changed=false;for(size_t i=0;i<held.size();++i)if(held[i]&&!juce::KeyPress::isKeyCurrentlyDown(int(juce::CharacterFunctions::toUpperCase(keyMap[int(i)])))){held[i]=false;p.keyboard.noteOff(1,60+int(i),0);changed=true;}return changed;}
 void timerCallback()override{
  const double now=juce::Time::getMillisecondCounterHiRes();if(scheduled>=0&&now>=scheduled){p.requestNew();scheduled=-1;strike=lastManual=now;}
  const auto count=p.hits.load();if(count!=seenHits){seenHits=count;if(now-lastManual>100)strike=now;}
  for(int i=0;i<4;++i)bursts[size_t(i)].setToggleState(int(p.state.getRawParameterValue("burst")->load())==i,juce::dontSendNotification);
  for(int i=0;i<7;++i)styles[size_t(i)].setToggleState(int(p.state.getRawParameterValue("style")->load())==i,juce::dontSendNotification);
  refreshHistory();if(++frames%120==0)refreshMidi();repaint();
 }
 void drawPose(juce::Graphics& g,int frame,float opacity,juce::Rectangle<float> dest,double time){
  if(opacity<.002f)return;juce::Graphics::ScopedSaveState save(g);g.setOpacity(opacity);
  const float sway=float(std::sin(time*1.3))*.018f;g.addTransform(juce::AffineTransform::rotation(sway,dest.getCentreX(),dest.getBottom()-20));
  g.drawImage(poses[size_t(frame)],dest,juce::RectanglePlacement::stretchToFit);

 }
public:
 explicit SnapEditor(SnapProcessor& processor):AudioProcessorEditor(processor),p(processor),wave(processor),piano(processor.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard){
  for(int i=0;i<8;++i)poses[size_t(i)]=sheet.getClippedImage({(i%4)*sheet.getWidth()/4,(i/4)*sheet.getHeight()/2,sheet.getWidth()/4,sheet.getHeight()/2});
  setLookAndFeel(&look);setResizable(true,true);getConstrainer()->setFixedAspectRatio(1.5);setResizeLimits(690,460,990,660);setSize(780,520);setWantsKeyboardFocus(true);
  const char* ids[]={"tone","human","pitch","punch","space","delay","delayTime","velocity","width","mix"};
  const char* titles[]={"TONE","HUMANIZE","PITCH","PUNCH","REVERB","DELAY","TIME","VELOCITY","WIDTH","LEVEL"};
  for(size_t i=0;i<knobs.size();++i){auto& k=knobs[i];k.setName(ids[i]);k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,15);k.setColour(juce::Slider::textBoxTextColourId,gold);k.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);addAndMakeVisible(k);k.addKeyListener(this);
   attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],k));
   k.textFromValueFunction=[i](double v){return i==2?juce::String(v,1)+" st":i==6?juce::String(int(v*1000))+" ms":juce::String(int(std::round(v*100)))+"%";};
   k.valueFromTextFunction=[i](const juce::String& t){return t.getDoubleValue()/(i==2?1.0:i==6?1000.0:100.0);};k.updateText();k.setTooltip(titles[i]);
   labels[i].setText(titles[i],juce::dontSendNotification);labels[i].setFont(juce::Font(juce::FontOptions(10.5f)));labels[i].setColour(juce::Label::textColourId,gold);labels[i].setJustificationType(juce::Justification::centred);addAndMakeVisible(labels[i]);}
  const char* bn[]={"SINGLE","DOUBLE","TRIPLE","FLAM"};const char* sn[]={"NATURAL","SOFT","SHARP","DEEP","BRIGHT","AIRY","RANDOM"};
  for(int i=0;i<4;++i){auto& b=bursts[size_t(i)];b.setButtonText(bn[i]);setupButton(b);b.onClick=[this,i]{selectParameter("burst",i);};}
  for(int i=0;i<7;++i){auto& b=styles[size_t(i)];b.setButtonText(sn[i]);setupButton(b);b.onClick=[this,i]{selectParameter("style",i);};}
  hand.setMouseCursor(juce::MouseCursor::PointingHandCursor);hand.setWantsKeyboardFocus(false);hand.setTooltip("Create a new snap and keep it in history");hand.onClick=[this]{scheduled=juce::Time::getMillisecondCounterHiRes()+65;grabKeyboardFocus();};addAndMakeVisible(hand);
  addAndMakeVisible(wave);wave.addKeyListener(this);addAndMakeVisible(historyBox);historyBox.setTextWhenNothingSelected("No snaps yet");historyBox.setTooltip("Every generated snap is kept as a WAV. Select one to replay.");historyBox.addKeyListener(this);
  historyBox.onChange=[this]{const int i=historyBox.getSelectedId()-1;if(i>=0&&i<int(entries.size()))p.history.select(entries[size_t(i)].file,true);};
  setupButton(previous);setupButton(next);setupButton(folder);previous.onClick=[this]{browse(-1);};next.onClick=[this]{browse(1);};folder.onClick=[this]{p.history.folder().startAsProcess();};
  addAndMakeVisible(midiBox);midiBox.addKeyListener(this);
  if(p.standalone()){midiBox.addItem("No MIDI device",1);midiBox.setSelectedId(1,juce::dontSendNotification);refreshMidi();midiBox.onChange=[this]{const int i=midiBox.getSelectedId()-2;p.selectMidi(i>=0&&i<devices.size()?devices[i].identifier:juce::String{});};if(!devices.isEmpty()&&p.selectedMidi().isEmpty())midiBox.setSelectedId(2,juce::sendNotificationSync);}
  else{midiBox.addItem("DAW MIDI: all notes",1);midiBox.setSelectedId(1,juce::dontSendNotification);midiBox.setEnabled(false);}
  piano.setName("Snap keyboard");piano.setAvailableRange(48,84);piano.setLowestVisibleKey(48);piano.setScrollButtonsVisible(false);piano.setWantsKeyboardFocus(false);piano.addKeyListener(this);piano.clearKeyMappings();piano.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,juce::Colour(0xffc6af7c));piano.setColour(juce::MidiKeyboardComponent::blackNoteColourId,juce::Colour(0xff17130e));piano.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,gold.withAlpha(.65f));addAndMakeVisible(piano);
  seenHits=p.hits.load();refreshHistory();timerCallback();startTimerHz(60);
 }
 ~SnapEditor()override{stopTimer();for(size_t i=0;i<held.size();++i)if(held[i])p.keyboard.noteOff(1,60+int(i),0);setLookAndFeel(nullptr);}
 bool keyPressed(const juce::KeyPress& k)override{return handleKey(k,this);}
 bool keyStateChanged(bool isDown)override{return keyStateChanged(isDown,this);}
 SnapWaveform& previewForTesting(){return wave;}
 void resized()override{
  const float scale=float(getWidth())/780;auto r=[&](int x,int y,int w,int h){return juce::Rectangle<int>(int(x*scale),int(y*scale),int(w*scale),int(h*scale));};
  const int xs[]={24,116,24,116,568,634,700,568,634,700},ys[]={94,94,188,188,94,94,94,188,188,188};
  for(size_t i=0;i<knobs.size();++i){const int w=i<4?78:62;labels[i].setBounds(r(xs[i]-2,ys[i]-18,w+4,18));knobs[i].setBounds(r(xs[i],ys[i],w,76));}
  for(int i=0;i<4;++i)bursts[size_t(i)].setBounds(r(568+(i%2)*98,295+(i/2)*31,94,27));
  for(int i=0;i<6;++i)styles[size_t(i)].setBounds(r(20+(i%3)*60,306+(i/3)*32,56,28));styles[6].setBounds(r(20,370,176,27));
  hand.setBounds(r(230,64,326,416));wave.setBounds(r(554,390,212,65));historyBox.setBounds(r(554,460,152,23));previous.setBounds(r(710,460,25,23));next.setBounds(r(739,460,27,23));
  midiBox.setBounds(r(14,427,190,25));folder.setBounds(r(149,458,55,23));piano.setBounds(r(14,490,752,24));piano.setKeyWidth(float(piano.getWidth())/22);
 }
 void paint(juce::Graphics& target)override{
  juce::Graphics::ScopedSaveState saved(target);target.addTransform(juce::AffineTransform::scale(float(getWidth())/780));auto& g=target;
  const double now=juce::Time::getMillisecondCounterHiRes(),t=now*.001;const float age=float((now-strike)*.001),pulse=age>=0&&age<.8f?std::exp(-age*8):0;
  g.fillAll(juce::Colour(0xff050403));g.drawImage(background,juce::Rectangle<float>(-3+float(std::sin(t*.18))*2,-3+float(std::cos(t*.14))*2,786,526));
  const juce::Point<float> centre{390,266};
  for(int ring=0;ring<4;++ring){juce::Path pth;const float radius=104+ring*17+float(std::sin(t*.8+ring))*3+pulse*9;const float a=float(t*(ring%2?.2:-.16)+ring*1.4);
   pth.addCentredArc(centre.x,centre.y,radius,radius*.91f,.12f,a,a+2.2f,true);g.setColour(gold.withAlpha(.16f+.09f*float(std::sin(t*.9+ring)*.5+.5)+pulse*.18f));g.strokePath(pth,juce::PathStrokeType(1+ring*.35f));}
  for(int i=0;i<42;++i){const double a=t*(.07+(i%5)*.011)+i*2.39996;const float radius=62+float(i%13)*8+float(std::sin(t*.6+i))*5;const float alpha=.12f+.26f*float(.5+.5*std::sin(t*1.2+i));g.setColour(gold.withAlpha(alpha));g.fillEllipse(centre.x+float(std::cos(a))*radius,centre.y+float(std::sin(a))*radius*.93f,1.2f+float(i%3)*.6f,1.2f+float(i%3)*.6f);}
  g.setColour(gold);g.setFont(juce::Font(juce::FontOptions(42,juce::Font::bold|juce::Font::italic)));g.drawText("SNAP",268,7,244,52,juce::Justification::centred);
  const auto handRect=juce::Rectangle<float>(230+float(std::sin(t*.8))*1.5f,64+float(std::sin(t*1.1))*2,326,416);
  float phase;
  if(scheduled>=0)phase=3.f*(1.f-std::clamp(float((scheduled-now)/65),0.f,1.f));
  else if(age>=0&&age<.3f)phase=4+3*std::clamp(age/.3f,0.f,1.f);
  else phase=.5f+.5f*float(std::sin(t*1.1));
  int f0=juce::jlimit(0,7,int(phase)),f1=std::min(7,f0+1);float u=phase-float(f0);
  if(scheduled<0&&age>=.3f&&age<.48f){f0=7;f1=0;u=(age-.3f)/.18f;}
  const float blend=u*u*(3-2*u);
  drawPose(g,f0,1-blend,handRect,t);drawPose(g,f1,blend,handRect,t);
  if(pulse>.005f){g.setColour(gold.withAlpha(pulse*.8f));for(int i=0;i<16;++i){const float a=float(i)*juce::MathConstants<float>::twoPi/16;const float radius=18+(1-pulse)*55;g.drawLine(397+std::cos(a)*radius,227+std::sin(a)*radius,397+std::cos(a)*(radius+12*pulse),227+std::sin(a)*(radius+12*pulse),1.5f);}}
  auto panel=[&](juce::Rectangle<float> b){g.setColour(juce::Colour(0xef090806));g.fillRoundedRectangle(b,10);g.setColour(juce::Colour(0xff6b5835));g.drawRoundedRectangle(b.reduced(.5f),10,.8f);};
  panel({14,73,190,202});panel({562,73,204,202});panel({14,285,190,120});panel({562,285,204,78});
  g.setColour(gold.withAlpha(.8f));g.setFont(10);g.drawText("VARIATION",23,286,100,17,juce::Justification::centredLeft);
  g.drawText("HISTORY  "+juce::String(int(entries.size())),555,369,140,18,juce::Justification::centredLeft);
  g.drawText(p.standalone()?"MIDI INPUT":"INSTRUMENT MIDI",15,408,180,17,juce::Justification::centredLeft);
  g.setFont(10);g.drawText("A–L: new   Space: replay",14,460,137,19,juce::Justification::centredLeft);
  const auto error=p.history.error();if(error.isNotEmpty()||p.history.overflow.load()){g.setColour(juce::Colours::orange);g.setFont(10);g.drawFittedText(error.isNotEmpty()?error:"History queue full — some hits could not be saved",210,461,335,23,juce::Justification::centred,2);}
  else if(p.history.pendingSaves.load()>0){g.setColour(gold.withAlpha(.65f));g.drawText("Saving snap…",306,461,150,20,juce::Justification::centred);}
 }
};
