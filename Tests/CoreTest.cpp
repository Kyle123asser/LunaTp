#include "LunaCore.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
int main(int argc,char** argv){
    if(argc<6){std::cerr<<"output midi gate velocity sampleRate [tuning] [textureLevel]\n";return 2;}
    int midi=std::stoi(argv[2]),vel=std::stoi(argv[4]);double gate=std::stod(argv[3]),fs=std::stod(argv[5]);
    Luna::Engine engine;engine.prepare(fs);if(argc>6)engine.setTuningTest(std::stoi(argv[6])!=0);if(argc>7)engine.setTextureLevel(std::stoi(argv[7]));engine.reset();
    engine.noteOn(1,midi,vel);
    std::ofstream f(argv[1],std::ios::binary);int off=int(std::round(gate*fs)),count=int(std::round((gate+.3)*fs));double peak=0;
    auto start=std::chrono::steady_clock::now();
    for(int i=0;i<count;++i){if(i==off)engine.noteOff(1,midi);float x=float(engine.sample());if(!std::isfinite(x))return 3;peak=std::max(peak,std::abs(double(x)));f.write((char*)&x,sizeof(x));}
    std::cout<<"peak="<<peak<<" secondsCPU="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" finalNote="<<engine.currentNote()<<"\n";
    // Stale Note Off, velocity zero, repeated pitch FIFO, and panic behavior.
    engine.reset();engine.noteOn(1,60,100);for(int i=0;i<100;++i)engine.sample();engine.noteOn(1,69,100);engine.noteOff(1,60);if(engine.currentNote()!=69)return 4;
    engine.noteOn(1,69,100);engine.noteOff(1,69);if(engine.currentNote()!=69)return 5;engine.noteOn(1,69,0);
    for(int i=0;i<int(.3*fs);++i)engine.sample();if(engine.sounding())return 6;
    engine.noteOn(1,60,100);engine.allNotesOff(true);for(int i=0;i<int(.01*fs);++i)engine.sample();if(engine.sounding())return 7;
    return 0;
}
