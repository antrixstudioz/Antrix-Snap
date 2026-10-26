#include "Editor.h"
#include <iostream>
#include <functional>
void require(bool condition,const char* message){if(!condition){std::cerr<<message<<std::endl;std::exit(1);}}
bool waitUntil(std::function<bool()> predicate){for(int i=0;i<1000;++i){if(predicate())return true;juce::Thread::sleep(10);}return false;}
void setValue(SnapProcessor& p,const char* id,float value){auto* parameter=p.state.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
std::vector<float> audio(SnapProcessor& p,int length,bool midiNote=false){
 std::vector<float> result;result.reserve(size_t(length)*2);juce::AudioBuffer<float> block(2,512);juce::MidiBuffer midi;
 for(int offset=0;offset<length;offset+=512){const int n=std::min(512,length-offset);block.setSize(2,n,false,false,true);midi.clear();if(offset==0&&midiNote){midi.addEvent(juce::MidiMessage::noteOn(1,60,.9f),31);midi.addEvent(juce::MidiMessage::noteOff(1,60),400);}p.processBlock(block,midi);
  for(int i=0;i<n;++i)for(int c=0;c<2;++c)result.push_back(block.getSample(c,i));}
 return result;
}
double energy(const std::vector<float>& values){double total=0;for(auto x:values)total+=x*x;return total;}
void writePng(const juce::Image& image,const char* name){auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);auto out=file.createOutputStream();require(bool(out),"Screenshot output failed");out->setPosition(0);out->truncate();juce::PNGImageFormat png;require(png.writeImageToStream(image,*out),"Screenshot encoding failed");}
int main(){
 juce::ScopedJuceInitialiser_GUI gui;
 {SnapEffects fx;snap::Settings s;s.space=0;s.delay=0;fx.prepare(48000);fx.configure(s);auto dry=fx.process({.2f,.2f});require(std::abs(dry[0]-.2f)<.00001f,"Dry effects path must have unity gain");
  s.delay=1;s.delayTime=.1f;fx.prepare(48000);fx.configure(s);fx.process({.4f,.4f});double early=0,echo=0;for(int i=1;i<5000;++i){auto y=fx.process({0,0});if(i<4700)early+=std::abs(y[0]);else echo+=std::abs(y[0]);}require(early<.00001&&echo>.1,"Delay must produce a delayed tap, not alter dry attack");
  s.delay=0;s.space=1;fx.prepare(48000);fx.configure(s);fx.process({.4f,.4f});double tail=0;for(int i=0;i<48000;++i){auto y=fx.process({0,0});tail+=y[0]*y[0]+y[1]*y[1];}require(tail>.0001,"Reverb must produce a diffuse tail");}
 const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("SnapWorkflowTest","",false);root.createDirectory();
 juce::MemoryBlock savedState;juce::File firstFile;
 {
  auto processor=std::make_unique<SnapProcessor>(root);auto& p=*processor;p.prepareToPlay(48000,512);setValue(p,"space",0);setValue(p,"delay",0);
  require(waitUntil([&]{return p.history.initialised.load();}),"History initialization timed out");
  auto generated=audio(p,144000,true);require(energy(generated)>1,"Host MIDI note must produce audible snap");for(int i=0;i<62;++i)require(generated[size_t(i)]==0,"MIDI timing must respect sample offset");
  require(p.midiHits.load()==1,"MIDI note must trigger once");require(waitUntil([&]{return p.history.entries().size()==1&&p.history.pendingSaves.load()==0;}),"MIDI snap was not persisted");
  firstFile=p.history.entries()[0].file;require(firstFile.existsAsFile(),"History WAV missing");
  p.requestReplay();require(waitUntil([&]{return p.history.previewReady();}),"Preview was not prepared");auto replayed=audio(p,144000);
  double error=0;for(size_t i=0;i+62<generated.size();++i)error=std::max(error,double(std::abs(replayed[i]-generated[i+62])));
  require(error<.0002,"Preview must replay the original audio without random regeneration");require(p.history.entries().size()==1,"Replaying must not generate another history entry");
  std::unique_ptr<juce::AudioProcessorEditor> base(p.createEditor());auto* editor=dynamic_cast<SnapEditor*>(base.get());require(editor!=nullptr,"Editor missing");
  require(editor->getWidth()==780&&editor->getHeight()==520,"Default editor must be compact");
  require(editor->keyPressed(juce::KeyPress('A',{},'a')),"Computer-keyboard A was not handled");auto keyed=audio(p,144000);require(energy(keyed)>1,"Computer keyboard must produce audio");
  require(waitUntil([&]{return p.history.entries().size()==2&&p.history.pendingSaves.load()==0;}),"Keyboard snap was not persisted");
  p.history.select(firstFile,false);require(waitUntil([&]{return p.history.display().file==firstFile;}),"Previous snap could not be selected");
  require(editor->keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)),"Space replay key not handled");require(waitUntil([&]{return p.history.previewReady();}),"Space did not prepare replay");require(energy(audio(p,144000))>1,"Space replay must produce audio");
  auto display=p.history.display();editor->previewForTesting().clip=display;require(editor->previewForTesting().exportFile()==firstFile,"Drag/export must reference selected saved WAV");
  juce::WavAudioFormat format;auto stream=firstFile.createInputStream();std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(),true));require(reader&&reader->sampleRate==48000&&reader->numChannels==2,"Export must be a valid 48k stereo WAV");
  juce::Component* hand=nullptr;for(int i=0;i<editor->getNumChildComponents();++i){auto* c=editor->getChildComponent(i);if(c->getName()=="Snap hand")hand=c;}require(hand!=nullptr,"Hand target missing");
  auto target=hand->createComponentSnapshot(hand->getLocalBounds());for(int y=0;y<target.getHeight();++y)for(int x=0;x<target.getWidth();++x)require(target.getPixelAt(x,y).getAlpha()==0,"Hand must not paint a border");
  auto before=editor->createComponentSnapshot(editor->getLocalBounds());juce::Thread::sleep(180);auto after=editor->createComponentSnapshot(editor->getLocalBounds());int differences=0;
  for(int y=80;y<400;y+=3)for(int x=250;x<540;x+=3)if(before.getPixelAt(x,y)!=after.getPixelAt(x,y))++differences;
  require(differences>100,"Idle hand and background must animate without clicks");writePng(after,"Snap-UI.png");writePng(before,"Snap-UI-Idle.png");
  p.getStateInformation(savedState);base.reset();p.releaseResources();
 }
 {
  auto reopened=std::make_unique<SnapProcessor>(root);reopened->setStateInformation(savedState.getData(),int(savedState.getSize()));
  require(waitUntil([&]{return reopened->history.initialised.load()&&reopened->history.entries().size()==2&&reopened->history.display().file==firstFile;}),"Saved history and selection must survive closing/reopening");
 }
 root.deleteRecursively();
 std::cout<<"MIDI timing, keyboard audio, exact preview replay, persistent history, WAV export, reverb/delay, compact UI and continuous idle animation passed"<<std::endl;
}
