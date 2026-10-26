#include "Engine.h"
#include <fstream>
#include <iterator>
#include <cstdint>
#include <memory>
void u16(std::ofstream& o,uint16_t n){o.put(char(n&255));o.put(char(n>>8));}
void u32(std::ofstream& o,uint32_t n){u16(o,uint16_t(n));u16(o,uint16_t(n>>16));}
int main(int argc,char** argv){
 if(argc!=3)return 1;
 std::ifstream in(argv[1],std::ios::binary);std::vector<char> bank((std::istreambuf_iterator<char>(in)),{});
 auto engine=std::make_unique<snap::Engine>();if(!engine->loadBank(bank.data(),bank.size()))return 2;
 const int rate=48000,count=rate*8;std::ofstream o(argv[2],std::ios::binary);if(!o)return 3;
 o.write("RIFF",4);u32(o,36+count*4);o.write("WAVEfmt ",8);u32(o,16);u16(o,1);u16(o,2);u32(o,rate);u32(o,rate*4);u16(o,4);u16(o,16);o.write("data",4);u32(o,count*4);
 engine->prepare(rate);engine->setSeed(2026);snap::Settings s;s.space=0;
 for(int i=0;i<count;++i){if(i%rate==0)engine->trigger(s);auto y=engine->sample(s);for(auto v:y)u16(o,uint16_t(int16_t(v*32767)));}
}
