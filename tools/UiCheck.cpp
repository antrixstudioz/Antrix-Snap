#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int main(){
 juce::ScopedJuceInitialiser_GUI gui;
 std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
 processor->prepareToPlay(48000,256);
 std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
 juce::Component* hand=nullptr;
 for(int i=0;i<editor->getNumChildComponents();++i){auto* c=editor->getChildComponent(i);if(c->getName()=="Snap hand")hand=c;}
 if(!hand){std::cerr<<"Hand target missing\n";return 1;}
 auto target=hand->createComponentSnapshot(hand->getLocalBounds());
 for(int y=0;y<target.getHeight();++y)for(int x=0;x<target.getWidth();++x)
  if(target.getPixelAt(x,y).getAlpha()!=0){std::cerr<<"Hand target paints a visible box\n";return 2;}
 editor->setSize(900,600);
 auto screenshot=editor->createComponentSnapshot(editor->getLocalBounds());
 auto file=juce::File::getCurrentWorkingDirectory().getChildFile("Snap-UI.png");
 auto output=file.createOutputStream();
 juce::PNGImageFormat png;
 if(!output||!png.writeImageToStream(screenshot,*output))return 3;
 std::cout<<"Hand target is fully transparent; minimum-size editor rendered to Snap-UI.png\n";
 editor.reset();processor->releaseResources();
}
