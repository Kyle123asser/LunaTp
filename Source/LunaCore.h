#pragma once
#include <array>
#include <complex>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Luna {
constexpr double pi=3.14159265358979323846;
inline double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3.-2.*x);}
inline double frequency(int midi){return 440.*std::pow(2.,(midi-69)/12.);}
struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void reset(){z1=z2=0;}
    double process(double x){double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
    void set(int type,double fs,double hz,double db,double q);
};
struct Noise {
    uint64_t state=1;bool spareReady=false;double spare=0;
    void seed(uint64_t s){state=s+0x9e3779b97f4a7c15ULL;spareReady=false;}
    double uniform();double normal();
};
struct Voice {
    static constexpr int H=96;
    double fs=48000,t=0,phase=0,gate=-1,velocity=0,f0=440,fadeLeft=-1;
    int midi=-1,channel=1,harmonics=0,framePos=0;
    bool active=false,vibrato=true;
    std::array<double,6> calibration{};
    std::array<double,H> reg{},attackGain{},guard{};
    std::array<std::complex<double>,H> start{},next{},enhStart{},enhNext{};
    Noise rng,airRng,burstRng;
    Biquad nHP,nLP,aHP,aLP,bHP,bLP;
    double dcX=0,dcY=0;
    // V0.2 performance state. The unchanged static path remains exact at defaults.
    double spectralMidi=69,rootPitch=69,pitchStart=69,pitchTarget=69;
    double glidePosition=0,glideDuration=0,targetVelocity=0,velocityDecay=0;
    double timbreAmount=0,flutterPhase1=0,flutterPhase2=0;
    bool movingSpectrum=false,flutterDynamic=false,polyphonic=false;
    uint64_t token=0,order=0;
    void begin(int note,int vel,int ch,double sampleRate,uint64_t seed,bool enableVibrato,double timbre=0);
    void noteOff();
    void release();
    void setTargetPitch(double note,double seconds);
    void activateDynamicFlutter();
    void setSpectrum(double note);
    void controls(double at,double& amp,double& pressure)const;
    double shape(double hz,int h,double pressure,double at,bool bright)const;
    void coefficients(double at,std::array<std::complex<double>,H>& out,std::array<std::complex<double>,H>& enhanced)const;
    double sample(double textureDepth,double bend=0,double timbre=0);
};
class Engine {
public:
    static constexpr int maxVoices=8,maxHeld=256;
    static constexpr double outputGain=0.12452297462740328;
    void prepare(double sampleRate);
    void reset();
    void noteOn(int channel,int midi,int velocity);
    void noteOff(int channel,int midi);
    void allNotesOff(bool immediate=false,int channel=0);
    double sample();
    void setTextureLevel(int level);
    void setVoiceMode(int value);
    void setPolyVoiceGain(double percent){targetPolyGain=std::clamp(percent/100.,0.,1.);}
    double polyVoiceGain()const{return polyGain;}
    void setVelocityTimbre(double percent){targetTimbre=std::clamp(percent/100.,0.,1.);}
    void setLegatoEnabled(bool value){legato=value;}
    void setGlideEnabled(bool value);
    void setGlideTime(double ms){glideSeconds=std::clamp(ms,0.,500.)/1000.;}
    void setGlideMode(int value){glideAlways=value!=0;}
    void setPitchBendRange(int semitones);
    void pitchWheel(int channel,int value);
    void setTuningTest(bool value){tuningTest=value;}
    int currentNote()const{return voices[0].active?voices[0].midi:-1;}
    bool sounding()const;
    int activeVoices()const;
    int heldNotes()const;
    double bendSemitones()const{return bend;}
    const Voice& voice(int index)const{return voices[std::clamp(index,0,maxVoices-1)];}
    uint64_t seed=20261115;
private:
    struct Press {uint64_t id=0;int note=0,velocity=0,channel=1;bool held=false;};
    std::array<Voice,maxVoices> voices,retiring;
    std::array<Press,maxHeld> presses{};
    std::array<Biquad,3> eq;
    double fs=48000,texture=.12,targetTexture=.12,textureDecay=0;
    double timbre=0,targetTimbre=0,bend=0,targetBend=0,bendDecay=0;
    double polyGain=1,targetPolyGain=1;
    double glideSeconds=.08,previousPitch=-1;
    int mode=0,bendRange=2,wheel=8192;
    bool tuningTest=false,legato=true,glide=false,glideAlways=false;
    uint64_t sequence=0,pressSequence=0;
    Press* newestHeld();
    void retire(int slot);
    void startVoice(int slot,const Press& press,double from,double glideTime);
    void monoTransition(const Press& press,bool overlapping);
};
}
