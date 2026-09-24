#include "WavOutput.h"
#include <stdexcept>
static void u16(FILE*f,unsigned x){fputc(x&255,f);fputc((x>>8)&255,f);}
static void u32(FILE*f,unsigned x){u16(f,x);u16(f,x>>16);}
void WavOutput::open(const std::string &path){file=fopen(path.c_str(),"wb");if(!file)throw std::runtime_error("cannot write WAV");for(unsigned i=0;i<44;++i)fputc(0,file);}
void WavOutput::sample(int16_t value){if(count>=0x7fffffd0)throw std::runtime_error("WAV length limit");u16(file,uint16_t(value));++count;}
void WavOutput::close(){if(!file)return;bool bad=ferror(file)||fseek(file,0,SEEK_SET);fwrite("RIFF",1,4,file);u32(file,36+2*count);fwrite("WAVEfmt ",1,8,file);u32(file,16);u16(file,1);u16(file,1);u32(file,44100);u32(file,88200);u16(file,2);u16(file,16);fwrite("data",1,4,file);u32(file,2*count);bad|=ferror(file);bad|=fclose(file)!=0;file=nullptr;if(bad)throw std::runtime_error("WAV write failed");}
WavOutput::~WavOutput(){if(file)fclose(file);}
