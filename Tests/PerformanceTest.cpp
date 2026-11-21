#include "LunaCore.h"
#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>

static std::vector<std::string> passed;
static double maximum=0;
static void require(bool ok,const std::string& name){if(!ok)throw std::runtime_error(name);passed.push_back(name);}
static void tick(Luna::Engine& e,int samples){for(int i=0;i<samples;++i){double x=e.sample();if(!std::isfinite(x)||std::abs(x)>=1)throw std::runtime_error("Invalid/clipped DSP sample");maximum=std::max(maximum,std::abs(x));}}
int main(){try{
    auto storage=std::make_unique<Luna::Engine>();auto& e=*storage;e.setPolyVoiceGain(12.5);
    for(double fs:{44100.,48000.,96000.}){
        auto duration=[&](double s){tick(e,int(std::round(s*fs)));};
        auto suffix=" at "+std::to_string(int(fs));
        e.prepare(fs);e.setVoiceMode(0);e.setLegatoEnabled(true);e.setGlideEnabled(false);e.setVelocityTimbre(0);e.reset();
        e.noteOn(1,60,100);duration(.2);double age=e.voice(0).t,phase=e.voice(0).phase;
        e.noteOn(1,64,100);require(e.currentNote()==64&&e.voice(0).t==age&&e.voice(0).phase==phase,"Legato retains oscillator/envelope"+suffix);
        e.noteOff(1,60);require(e.currentNote()==64&&e.voice(0).gate<0,"Old off ignores new note"+suffix);
        e.noteOff(1,64);duration(.2);require(!e.sounding(),"Note Off release completes"+suffix);

        e.reset();e.noteOn(1,60,100);duration(.05);e.noteOn(1,64,100);duration(.05);e.noteOn(1,67,100);
        e.noteOff(1,67);require(e.currentNote()==64,"Mono newest-held fallback 1"+suffix);
        e.noteOff(1,64);require(e.currentNote()==60,"Mono newest-held fallback 2"+suffix);
        e.noteOff(1,60);duration(.2);require(!e.sounding()&&e.heldNotes()==0,"Mono stack empty"+suffix);

        e.reset();e.noteOn(1,71,100);duration(.04);e.noteOn(1,71,100);auto token=e.voice(0).token;
        e.noteOff(1,71);require(e.voice(0).token==token&&e.voice(0).gate<0,"Repeated pitch FIFO"+suffix);
        e.noteOn(1,71,0);require(e.voice(0).gate>=0&&e.heldNotes()==0,"Velocity zero is off"+suffix);duration(.2);
        e.noteOn(1,69,100);duration(.1);e.allNotesOff(true,1);require(!e.sounding()&&e.sample()==0,"CC120 immediate including filter state"+suffix);
        e.noteOn(1,69,100);duration(.1);e.allNotesOff(false,1);require(e.sounding(),"CC123 uses release"+suffix);duration(.2);require(!e.sounding(),"CC123 finishes"+suffix);

        e.setLegatoEnabled(false);e.reset();e.noteOn(1,60,100);duration(.2);e.noteOn(1,64,100);require(e.voice(0).t==0,"Legato Off retriggers"+suffix);
        e.setLegatoEnabled(true);e.reset();e.noteOn(1,60,100);duration(.1);e.noteOff(1,60);duration(.01);e.noteOn(1,64,100);require(e.voice(0).t==0,"Non-overlap retriggers"+suffix);

        e.setGlideEnabled(true);e.setGlideMode(0);
        for(double ms:{0.,80.,250.,500.}){
            e.setGlideTime(ms);e.reset();e.noteOn(1,60,100);duration(.4);e.noteOn(1,72,100);
            if(ms==0){require(e.voice(0).rootPitch==72,"Zero glide switches immediately"+suffix);continue;}
            require(e.voice(0).rootPitch==60,"Glide starts at current pitch"+suffix);
            duration(ms/2000.);tick(e,1);require(std::abs(e.voice(0).rootPitch-66)<.003,"Glide smoothstep midpoint "+std::to_string(ms)+suffix);
            duration(ms/2000.);tick(e,2);require(e.voice(0).rootPitch==72,"Glide reaches target on time "+std::to_string(ms)+suffix);
            require(e.voice(0).t>.4&&e.voice(0).gate<0,"Glide keeps note age"+suffix);
        }
        e.setGlideTime(80);e.reset();e.noteOn(1,60,100);duration(.2);e.noteOff(1,60);duration(.2);e.noteOn(1,72,100);require(e.voice(0).rootPitch==72,"Legato glide skips separated notes"+suffix);
        e.setGlideMode(1);e.reset();e.noteOn(1,60,100);require(e.voice(0).rootPitch==60,"Always first note has no stale pitch"+suffix);duration(.2);e.noteOff(1,60);duration(.2);e.noteOn(1,72,100);require(e.voice(0).rootPitch==60,"Always glide includes separated notes"+suffix);duration(.1);require(e.voice(0).rootPitch==72,"Always reaches target"+suffix);

        e.setGlideEnabled(false);e.setTuningTest(true);
        for(int range:{2,12,24})for(int wheel:{0,8192,16383}){
            e.reset();e.setPitchBendRange(range);e.noteOn(1,69,100);duration(.6);e.pitchWheel(1,wheel);duration(.08);
            double expected=wheel==0?-range:wheel==8192?0:range;
            require(std::abs(e.bendSemitones()-expected)<1e-7,"Bend endpoint "+std::to_string(range)+"/"+std::to_string(wheel)+suffix);
            phase=e.voice(0).phase;e.sample();double step=e.voice(0).phase-phase;if(step<0)step+=2*Luna::pi;
            double measured=step*fs/(2*Luna::pi),target=440*std::pow(2.,expected/12.);
            require(std::abs(measured/target-1)<1e-8,"Actual oscillator bend frequency"+suffix);
        }
        e.reset();e.setTuningTest(false);e.setGlideEnabled(true);e.setGlideMode(0);e.setGlideTime(500);e.setPitchBendRange(12);
        e.noteOn(1,60,100);duration(.8);e.noteOn(1,72,100);e.pitchWheel(1,16383);duration(.21);
        double at=e.voice(0).t;phase=e.voice(0).phase;e.sample();double step=e.voice(0).phase-phase;if(step<0)step+=2*Luna::pi;
        double cents=18*Luna::smooth((at-.22)/.15)*std::sin(2*Luna::pi*5.6*at+.12)+1.1*(std::sin(2*Luna::pi*.73*at)+.4*std::sin(2*Luna::pi*1.31*at+.8))-14*std::exp(-at/.025);
        double target=440*std::pow(2.,(e.voice(0).rootPitch+e.bendSemitones()-69)/12.)*std::pow(2.,cents/1200.);
        require(std::abs(step*fs/(2*Luna::pi)/target-1)<1e-9,"Glide + Bend + Vibrato exactly once"+suffix);

        e.setVoiceMode(1);e.reset();e.noteOn(1,60,100);e.noteOn(1,64,100);duration(.2);
        require(e.activeVoices()==2&&e.voice(0).phase!=e.voice(1).phase,"Poly C4+E4 independent oscillators"+suffix);
        e.noteOff(1,60);require(e.voice(0).gate>=0&&e.voice(1).gate<0,"Poly off only matching note"+suffix);duration(.2);require(e.activeVoices()==1,"Other poly voice survives"+suffix);
        e.noteOn(1,67,100);duration(.02);require(e.activeVoices()==2,"Poly independent note ages"+suffix);
        e.allNotesOff(false);duration(.2);require(!e.sounding(),"Poly CC123"+suffix);
        e.reset();for(int n:{60,64,67})e.noteOn(1,n,100);duration(.1);require(e.activeVoices()==3,"Poly triad"+suffix);
        e.reset();for(int i=0;i<8;++i)e.noteOn(1,69,100);duration(.1);e.noteOn(1,69,100);
        require(e.activeVoices()==8&&e.voice(0).token==9,"Eight voices oldest stealing"+suffix);
        e.noteOff(1,69);require(e.voice(0).gate<0&&e.activeVoices()==8,"Stolen old same-pitch off ignored"+suffix);
        for(int i=0;i<8;++i)e.noteOff(1,69);duration(.2);require(!e.sounding()&&e.heldNotes()==0,"Repeated poly notes finish"+suffix);
        e.reset();for(int i=0;i<8;++i)e.noteOn(1,60+i,100);duration(.1);e.noteOff(1,67);e.noteOn(1,84,100);
        require(e.voice(7).midi==84&&e.voice(0).midi==60,"Steal release-stage voice first"+suffix);
        e.reset();e.noteOn(1,60,100);e.noteOn(2,64,100);duration(.1);e.allNotesOff(true,1);require(e.activeVoices()==1&&e.voice(1).active,"Channel-scoped CC120"+suffix);e.allNotesOff(true,2);require(!e.sounding()&&e.sample()==0,"All channels silent"+suffix);

        e.setVelocityTimbre(100);e.setTextureLevel(5);e.reset();e.setPitchBendRange(24);
        for(int n:{48,55,60,64,67,71,76,84})e.noteOn(1,n,127);
        duration(.2);e.pitchWheel(2,16383);duration(.3);
        for(int i=0;i<8;++i)require(std::abs(e.voice(i).spectralMidi-e.voice(i).midi-24)<1e-5,"Poly bend reaches every voice"+suffix);
        e.pitchWheel(1,0);duration(.3);e.allNotesOff(false);duration(.2);
        require(!e.sounding(),"Eight-voice maximum-expression audio finite/unclipped"+suffix);
        e.setVelocityTimbre(0);e.setTextureLevel(3);e.reset();
        for(int i=0;i<50;++i){e.noteOn(1,60+i%12,100);tick(e,17);e.noteOff(1,60+i%12);tick(e,31);}duration(.2);
        require(!e.sounding(),"Rapid repeated poly audio finite/unclipped"+suffix);
        e.setVoiceMode(0);e.reset();e.noteOn(1,71,100);duration(.1);e.setVoiceMode(1);e.noteOn(1,71,100);
        e.noteOff(1,71);require(e.voice(1).active&&e.voice(1).gate<0,"Mode switch preserves duplicate press identities"+suffix);
        e.noteOff(1,71);duration(.2);require(!e.sounding(),"Mode switch duplicate notes finish"+suffix);
        e.setVoiceMode(1);e.setPolyVoiceGain(100);e.reset();e.noteOn(1,69,100);duration(.2);e.setPolyVoiceGain(0);
        tick(e,1);require(std::abs(e.polyVoiceGain()-std::exp(-1/(fs*.005)))<1e-12,"Poly gain first sample is smooth"+suffix);
        int count=int(std::round(fs*.005));tick(e,count-1);
        require(std::abs(e.polyVoiceGain()-std::exp(-count/(fs*.005)))<1e-11,"Poly gain 5ms exponential"+suffix);
        duration(.2);require(e.polyVoiceGain()==0,"Poly gain settles to exact mute"+suffix);
        e.setPolyVoiceGain(12.5);e.reset();
    }
    std::cout<<"{\"passed_checks\":"<<passed.size()<<",\"max_peak\":"<<maximum<<",\"checks\":[";
    for(size_t i=0;i<passed.size();++i)std::cout<<(i?",":"")<<'"'<<passed[i]<<'"';std::cout<<"]}\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
