#pragma once
#include "Effects.h"
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>

template<class T,size_t Capacity> class SnapQueue {
 std::array<T,Capacity> data{};std::atomic<size_t> head{0},tail{0};
public:
 bool push(const T& v){const auto h=head.load(std::memory_order_relaxed),next=(h+1)%Capacity;if(next==tail.load(std::memory_order_acquire))return false;data[h]=v;head.store(next,std::memory_order_release);return true;}
 bool pop(T& v){const auto t=tail.load(std::memory_order_relaxed);if(t==head.load(std::memory_order_acquire))return false;v=data[t];tail.store((t+1)%Capacity,std::memory_order_release);return true;}
 bool empty()const{return tail.load(std::memory_order_acquire)==head.load(std::memory_order_acquire);}
 bool hasSpace()const{return (head.load(std::memory_order_relaxed)+1)%Capacity!=tail.load(std::memory_order_acquire);}
};

class SnapHistory final:private juce::Thread {
public:
 struct Entry {juce::File file;juce::String label;};
 struct Display {juce::File file;juce::String label;std::array<float,160> peaks{};double seconds=0;};
 struct Preview {juce::AudioBuffer<float> audio;double sampleRate=48000;};
 std::atomic<unsigned> revision{0},pendingSaves{0};std::atomic<bool> followLatest{true},overflow{false},initialised{false};
 static juce::File defaultFolder(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Antrix/Snap/History");}
 explicit SnapHistory(const void* data,size_t size,juce::File folder=defaultFolder()):juce::Thread("Snap audio history"),root(std::move(folder)){
  renderer.loadBank(data,size);startThread();
 }
 ~SnapHistory()override{signalThreadShouldExit();notify();waitForThreadToExit(-1);delete ready.exchange(nullptr);Preview* p=nullptr;while(retired.pop(p))delete p;}
 // Audio producer: bounded, lock-free; WAV work happens on this thread only.
 bool record(const snap::Recipe& r){pendingSaves.fetch_add(1);if(events.push(r))return true;pendingSaves.fetch_sub(1);overflow.store(true);return false;}
 bool canTakePreview()const{return retired.hasSpace();}
 Preview* takePreview(){return ready.exchange(nullptr,std::memory_order_acq_rel);}
 bool previewReady()const{return ready.load(std::memory_order_acquire)!=nullptr;}
 void retire(Preview* p){if(p){const bool ok=retired.push(p);jassert(ok);juce::ignoreUnused(ok);}}
 juce::File folder()const{return root;}
 std::vector<Entry> entries()const{std::lock_guard<std::mutex> lock(mutex);return index;}
 Display display()const{std::lock_guard<std::mutex> lock(mutex);return current;}
 juce::String error()const{std::lock_guard<std::mutex> lock(mutex);return lastError;}
 void select(const juce::File& file,bool play){
  if(!file.isAChildOf(root)||file.getFileExtension()!=".wav")return;
  followLatest.store(false);std::lock_guard<std::mutex> lock(mutex);requested=file;requestPlay=play;requestPending=true;notify();
 }
 void replay(){auto d=display();if(d.file.existsAsFile())select(d.file,true);}
 void restoreSelection(const juce::String& name){if(name.startsWith("Snap_")&&name.endsWith(".wav")&&!name.containsAnyOf("/\\"))select(root.getChildFile(name),false);}
private:
 juce::File root;juce::String session=juce::Uuid().toString().substring(0,8);uint64_t serial=0;
 snap::Engine renderer;SnapEffects fx;
 SnapQueue<snap::Recipe,4096> events;SnapQueue<Preview*,512> retired;
 std::atomic<Preview*> ready{nullptr};
 mutable std::mutex mutex;std::vector<Entry> index;Display current;juce::String lastError;juce::File requested;bool requestPlay=false,requestPending=false;
 static juce::String labelFor(const juce::File& file){
  const auto parts=juce::StringArray::fromTokens(file.getFileNameWithoutExtension(),"_","");
  if(parts.size()>=5)return juce::Time(parts[1].getLargeIntValue()).formatted("%d %b %H:%M:%S")+"  "+parts.getLast()+"  #"+parts[3];
  return file.getFileNameWithoutExtension();
 }
 void setError(const juce::String& text){std::lock_guard<std::mutex> lock(mutex);lastError=text;revision.fetch_add(1);}
 std::unique_ptr<Preview> read(const juce::File& file){
  auto stream=file.createInputStream();if(!stream)return {};
  juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(),true));
  if(!reader||reader->lengthInSamples<=0||reader->lengthInSamples>48000*60)return {};
  auto p=std::make_unique<Preview>();p->sampleRate=reader->sampleRate;p->audio.setSize(2,int(reader->lengthInSamples));
  if(!reader->read(&p->audio,0,p->audio.getNumSamples(),0,true,true))return {};return p;
 }
 void show(const Entry& entry,const Preview& p){
  Display d;d.file=entry.file;d.label=entry.label;d.seconds=p.audio.getNumSamples()/p.sampleRate;
  for(size_t i=0;i<d.peaks.size();++i){const int a=int(i*size_t(p.audio.getNumSamples())/d.peaks.size()),b=int((i+1)*size_t(p.audio.getNumSamples())/d.peaks.size());
   for(int c=0;c<2;++c)for(int j=a;j<b;++j)d.peaks[i]=std::max(d.peaks[i],std::abs(p.audio.getSample(c,j)));}
  {std::lock_guard<std::mutex> lock(mutex);current=std::move(d);}revision.fetch_add(1);
 }
 std::unique_ptr<Preview> render(const snap::Recipe& r){
  auto p=std::make_unique<Preview>();p->audio.setSize(2,48000*8);renderer.prepare(48000);renderer.playRecipe(r);fx.prepare(48000);fx.configure(r.settings);
  int quiet=0,used=0;
  for(;used<p->audio.getNumSamples();++used){const auto y=fx.process(renderer.sample(r.settings));p->audio.setSample(0,used,y[0]);p->audio.setSample(1,used,y[1]);
   quiet=(!renderer.isActive()&&std::max(std::abs(y[0]),std::abs(y[1]))<.000015f)?quiet+1:0;
   // Keep enough silence to include the next delay tap, not just the dry transient.
   const int gap=int(48000*std::max(.3f,r.settings.delay>0?r.settings.delayTime+.08f:.3f));
   if(quiet>gap){++used;break;}
  }
  p->audio.setSize(2,used,true,false,true);return p;
 }
 void save(const snap::Recipe& r){
  static const char* names[]={"Natural","Soft","Sharp","Deep","Bright","Airy"};
  auto p=render(r);const auto stamp=juce::Time::currentTimeMillis();
  const auto name="Snap_"+juce::String(stamp)+"_"+session+"_"+juce::String(++serial)+"_"+names[std::clamp(r.effectiveStyle,0,5)];
  const auto file=root.getChildFile(name+".wav"),temp=root.getChildFile(name+".part");
  bool wrote=false;{
   auto stream=temp.createOutputStream();if(stream){juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000,2,24,{},0));
    if(writer){stream.release();wrote=writer->writeFromAudioSampleBuffer(p->audio,0,p->audio.getNumSamples());}
   }
  }
  if(!wrote||!temp.moveFileTo(file)){setError("Could not save snap history. Check disk space and folder access.");pendingSaves.fetch_sub(1);return;}
  const auto label=juce::Time(stamp).formatted("%d %b %H:%M:%S")+"  "+names[std::clamp(r.effectiveStyle,0,5)]+"  #"+juce::String(serial);
  Entry e{file,label};{std::lock_guard<std::mutex> lock(mutex);index.push_back(e);}revision.fetch_add(1);
  if(followLatest.load())show(e,*p);pendingSaves.fetch_sub(1);
 }
 void run()override{
  if(root.createDirectory().failed())setError("History folder could not be created.");
  auto files=root.findChildFiles(juce::File::findFiles,false,"Snap_*.wav");std::sort(files.begin(),files.end(),[](const juce::File& a,const juce::File& b){return a.getFileName()<b.getFileName();});
  {std::lock_guard<std::mutex> lock(mutex);for(const auto& f:files)index.push_back({f,labelFor(f)});}
  if(!files.isEmpty()){auto p=read(files.getLast());if(p)show({files.getLast(),labelFor(files.getLast())},*p);}
  initialised.store(true);revision.fetch_add(1);
  while(!threadShouldExit()||!events.empty()){
   Preview* old=nullptr;while(retired.pop(old))delete old;
   juce::File selection;bool play=false,haveRequest=false;{
    std::lock_guard<std::mutex> lock(mutex);if(requestPending){selection=requested;play=requestPlay;haveRequest=true;requestPending=false;}}
   if(haveRequest){auto p=read(selection);if(p){show({selection,labelFor(selection)},*p);if(play)delete ready.exchange(p.release(),std::memory_order_acq_rel);}else setError("This history WAV could not be opened.");}
   snap::Recipe recipe;if(events.pop(recipe)){save(recipe);continue;}
   wait(4);
  }
 }
};
