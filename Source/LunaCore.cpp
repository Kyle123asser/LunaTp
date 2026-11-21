#include "LunaCore.h"
#include "GeneratedData.h"

namespace Luna {
void Biquad::set(int type,double fs,double hz,double db,double q){
    double w=2*pi*hz/fs,c=std::cos(w),s=std::sin(w),A=std::pow(10.,db/40.),alpha=s/(2*q),a0=1;
    if(type==0){b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;}
    else if(type==1){b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;}
    else if(type==2){b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;}
    else {double root=2*std::sqrt(A)*alpha;b0=A*((A+1)-(A-1)*c+root);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-root);a0=(A+1)+(A-1)*c+root;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-root;}
    b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0;reset();
}
double Noise::uniform(){state^=state>>12;state^=state<<25;state^=state>>27;return ((state*2685821657736338717ULL)>>11)*0x1.0p-53;}
double Noise::normal(){if(spareReady){spareReady=false;return spare;}double r=std::sqrt(-2*std::log(std::max(uniform(),1e-15))),p=2*pi*uniform();spare=r*std::sin(p);spareReady=true;return r*std::cos(p);}

static double registerGain(double midi,int h,double hz,double fs){
    double key=smooth((midi-60.)/30.),band=smooth((h-5.)/3.)*(1-smooth((hz-12000.)/6000.));
    double db=12*.6*key*band;
    double low=1-smooth((midi-48.)/23.),high=smooth((midi-71.)/24.);
    double mid=h==1?0:std::exp(-.5*std::pow((h-5.)/2.,2));
    double upper=h==1?0:std::exp(-.5*std::pow((h-4.5)/1.6,2));
    double extra=6*(low*mid+high*upper)*(1-smooth((hz-.25*fs)/(.11*fs)))*std::max(0.,1-db/12);
    return std::pow(10.,(db+extra)/20.);
}
static double onsetGain(int h,double hz){
    double resonance=.65*std::exp(-.5*std::pow(std::log2(hz/2400.)/.30,2))+.55*std::exp(-.5*std::pow(std::log2(hz/3400.)/.28,2));
    double hi=smooth((h-2.)/4.)*smooth((hz-1200.)/1600.)*(1-smooth((hz-6000.)/2500.));
    double db=std::min(7.,4.5*resonance+2.5*hi);
    double extra=h==3?2.:h==5?3.5:h==7?2.5:0.;
    db=std::min(7.,db+extra*(1-smooth((hz-5500.)/3000.)));
    return std::pow(10.,db/20.);
}
void Voice::begin(int note,int vel,int ch,double sampleRate,uint64_t seed,bool enableVibrato,double timbre){
    *this=Voice{};fs=sampleRate;midi=note;channel=ch;velocity=vel/127.;f0=frequency(note);active=true;vibrato=enableVibrato;
    spectralMidi=rootPitch=pitchStart=pitchTarget=note;targetVelocity=velocity;
    velocityDecay=std::exp(-1/(fs*.005));timbreAmount=timbre;
    double maxF=f0*std::pow(2.,20./1200.);
    harmonics=std::clamp(int(.43*fs/maxF),0,H);
    int v0=0;while(v0<3&&vel>LunaData::velocities[v0+1])++v0;
    int v1=std::min(v0+1,4);double mix=std::clamp((vel-double(LunaData::velocities[v0]))/(LunaData::velocities[v1]-LunaData::velocities[v0]),0.,1.);
    for(int k=0;k<6;++k)calibration[k]=LunaData::calibration[note][v0][k]*(1-mix)+LunaData::calibration[note][v1][k]*mix;
    for(int h=1;h<=harmonics;++h){reg[h-1]=registerGain(note,h,f0*h,fs);attackGain[h-1]=onsetGain(h,f0*h);guard[h-1]=smooth((.43*fs-h*maxF)/(.07*fs));}
    rng.seed(seed);airRng.seed(seed+12012026);burstRng.seed(seed+1422026);
    nHP.set(0,fs,1100,0,.7071);nLP.set(1,fs,7800,0,.7071);
    aHP.set(0,fs,900,0,.7071);aLP.set(1,fs,4800,0,.7071);
    bHP.set(0,fs,400,0,.7071);bLP.set(1,fs,6500,0,.7071);
    coefficients(0,start,enhStart);coefficients(64/fs,next,enhNext);
}
void Voice::noteOff(){if(gate<0)gate=t;}
void Voice::release(){
    if(gate>=0)return;
    noteOff();
    double f=framePos/64.;
    for(int k=0;k<harmonics;++k){start[k]=start[k]*(1-f)+next[k]*f;enhStart[k]=enhStart[k]*(1-f)+enhNext[k]*f;}
    framePos=0;coefficients(t+64/fs,next,enhNext);
}
static double rateIntegral(double t);
static double initialRate(double note){return note<71?.35+2.65/(1+std::pow((71.-note)/24.,2)):3+4.5*smooth((note-71.)/24.);}
void Voice::activateDynamicFlutter(){
    if(flutterDynamic)return;
    double held=gate<0?t:std::min(t,gate),elapsed=rateIntegral(held);
    if(gate>=0)elapsed+=(t-gate)*(1+smooth(gate/1.1));
    flutterPhase1=2*pi*initialRate(midi)*elapsed;flutterPhase2=flutterPhase1*1.5;flutterDynamic=true;
}
void Voice::setTargetPitch(double note,double seconds){
    if(note==pitchTarget&&seconds>0&&glideDuration>0&&glidePosition<glideDuration)return;
    activateDynamicFlutter();movingSpectrum=true;
    pitchStart=rootPitch;pitchTarget=note;glidePosition=0;glideDuration=seconds;
    if(seconds<=0)rootPitch=note;
    // Extra partials start at zero and fade through the existing control frames.
    harmonics=H;
}
void Voice::setSpectrum(double note){
    spectralMidi=note;f0=440*std::pow(2.,(note-69)/12.);
    double maxF=f0*std::pow(2.,20./1200.);
    double n=std::clamp(note,0.,127.);int n0=int(n),n1=std::min(n0+1,127);double nm=n-n0;
    double vel=velocity*127.;int v0=0;while(v0<3&&vel>LunaData::velocities[v0+1])++v0;
    int v1=std::min(v0+1,4);double vm=std::clamp((vel-LunaData::velocities[v0])/(LunaData::velocities[v1]-double(LunaData::velocities[v0])),0.,1.);
    for(int k=0;k<6;++k){
        double a=LunaData::calibration[n0][v0][k]*(1-vm)+LunaData::calibration[n0][v1][k]*vm;
        double b=LunaData::calibration[n1][v0][k]*(1-vm)+LunaData::calibration[n1][v1][k]*vm;
        calibration[k]=a*(1-nm)+b*nm;
    }
    for(int h=1;h<=H;++h){reg[h-1]=registerGain(note,h,f0*h,fs);attackGain[h-1]=onsetGain(h,f0*h);guard[h-1]=smooth((.43*fs-h*maxF)/(.07*fs));}
}
void Voice::controls(double at,double& amp,double& pressure)const{
    double v=velocity,attack=.019*(1.22-.4*v),rise=smooth(at/attack);
    double rel=gate<0?1:smooth(1-(at-gate)/.145);
    pressure=(.19+.73*std::pow(v,.82))*(.66+.34*(1-std::exp(-at/.036)));
    pressure+=.10*std::exp(-std::pow((at-attack*1.4)/.055,2))*v+.005*std::sin(2*pi*3.7*at+.4);
    pressure=std::clamp(pressure,.02,1.15)*(.4+.6*rel*rel);
    amp=rise*(1+.13*v*std::exp(-at/.10))*(.2+.8*std::pow(v,1.35))*std::pow(rel,1.65);
    double held=gate<0?at:std::min(at,gate),ramp=smooth((held-.08)/.12),pt=std::max(held-.08,0.);
    double slow=std::sin(2*pi*1.35*pt),fine=std::sin(2*pi*3.15*pt);
    amp*=1+ramp*(.028*slow+.009*fine);
    pressure=std::clamp(pressure+.008*ramp*slow*(.4+.6*rel*rel),.02,1.15);
}
double Voice::shape(double hz,int h,double pressure,double at,bool bright)const{
    double key=std::pow(2.,(spectralMidi-69)/12.*.14);
    double cutoff=(bright?5200.:4550.)*(.24+.99*std::pow(pressure,1.45))*std::pow(2.,(spectralMidi-69)/12.*.27);
    double roll=1/std::sqrt(1+std::pow(hz/std::max(cutoff,100.),6));
    double centers[3]={1040,1820,3250},widths[3]={.42,.35,.48},gains[3]={1.25,.95,bright?.42:.35};
    double formant=.30;
    for(int k=0;k<3;++k){double center=centers[k]*key*(1+.08*(pressure-.6));formant+=gains[k]*std::exp(-.5*std::pow(std::log2(hz/center)/widths[k],2));}
    double partialAttack=1-std::exp(-at/((bright?.0018:.0026)*(1+.1*h)));
    double release=gate<0?0:std::max(at-gate,0.);
    return roll*formant*(1+.16*(h%2?1:-1))*(h==1?1.2:1.)*partialAttack*std::exp(-release*h/.68);
}
void Voice::coefficients(double at,std::array<std::complex<double>,H>& out,std::array<std::complex<double>,H>& enhanced)const{
    double amp,pressure;controls(at,amp,pressure);
    double q=std::clamp(pressure/1.2*32.,0.,32.);int i=std::min(31,int(q));double w=q-i;
    double release=gate<0?0:std::max(at-gate,0.),brightness=.15+.85*std::exp(-release/.025);
    for(int h=1;h<=harmonics;++h){
        std::complex<double> a(LunaData::bank[i][h-1][0],LunaData::bank[i][h-1][1]),b(LunaData::bank[i+1][h-1][0],LunaData::bank[i+1][h-1][1]);
        double s0=shape(f0*h,h,pressure,at,false),s1=shape(f0*h,h,pressure,at,true);
        auto c=(a*(1-w)+b*w)*(s0+brightness*(s1-s0))*guard[h-1]*reg[h-1];
        if(timbreAmount>0){
            // Additional velocity expression only; original amplitude mapping stays intact.
            double tilt=3.*timbreAmount*(2*velocity-1)*smooth((h-2.)/6.)*(.65+.35*std::exp(-at/.06));
            c*=std::pow(10.,tilt/20.);
        }
        out[h-1]=c*calibration[0];enhanced[h-1]=c*attackGain[h-1]*calibration[1];
    }
}
static double rateIntegral(double t){double T=1.1,u=std::clamp(t/T,0.,1.);return t+T*(u*u*u-.5*u*u*u*u)+std::max(t-T,0.);}
double Voice::sample(double textureDepth,double bend,double timbre){
    if(!active)return 0;
    if(gate>=0&&t>=gate+.145){active=false;return 0;}
    timbreAmount=timbre;
    if(velocity!=targetVelocity){velocity=targetVelocity+(velocity-targetVelocity)*velocityDecay;if(std::abs(velocity-targetVelocity)<1e-10)velocity=targetVelocity;}
    if(glideDuration>0){rootPitch=pitchStart+(pitchTarget-pitchStart)*smooth(glidePosition/glideDuration);glidePosition=std::min(glideDuration,glidePosition+1/fs);}
    if(bend!=0){activateDynamicFlutter();movingSpectrum=true;harmonics=H;}
    double effectivePitch=rootPitch+bend;
    if(framePos>=64){start=next;enhStart=enhNext;framePos=0;if(movingSpectrum)setSpectrum(effectivePitch);coefficients(t+64/fs,next,enhNext);}
    double amp,pressure;controls(t,amp,pressure);
    double vib=vibrato?18*smooth((t-.22)/.15)*std::sin(2*pi*5.6*t+.12):0;
    double drift=vibrato?1.1*(std::sin(2*pi*.73*t)+.4*std::sin(2*pi*1.31*t+.8)):0;
    double cents=vib+drift-14*std::exp(-t/.025);
    double baseFrequency=movingSpectrum?440*std::pow(2.,(effectivePitch-69)/12.):f0;
    double inst=baseFrequency*std::pow(2.,cents/1200.);
    phase+=2*pi*inst/fs;if(phase>2*pi){phase-=2*pi;if(phase>2*pi)phase=std::fmod(phase,2*pi);}
    std::complex<double> unit(std::cos(phase),std::sin(phase)),osc=unit;
    double w=smooth(t/.003)*std::exp(-std::max(t-.012,0.)/.024)*(1-smooth((t-.070)/.040));
    if(gate>=0)w*=1-smooth((t-gate)/.005);
    double fraction=framePos/64.,tone=0;
    double microRamp=smooth((t-.04)/.06),am=std::sin(2*pi*47*t+.18*std::sin(2*pi*13.7*t));
    double topFrequency=baseFrequency*std::pow(2.,20./1200.);
    for(int h=1;h<=harmonics;++h){
        auto normal=start[h-1]*(1-fraction)+next[h-1]*fraction;
        auto enhanced=enhStart[h-1]*(1-fraction)+enhNext[h-1]*fraction;
        auto c=normal*(1-w)+enhanced*(w*calibration[2]);
        double selection=0;
        if(h>=3&&h<=12&&f0*h<16000){double width=std::min(f0*.18,std::max(20.,f0*h*.025));selection=std::exp(-.5*std::pow((inst-f0)*h/width,2))*(1-smooth((f0*h-12000)/4000.));}
        double liveGuard=movingSpectrum?smooth((.43*fs-h*topFrequency)/(.07*fs)):1.;
        tone+=(c.real()*osc.real()-c.imag()*osc.imag())*(1+textureDepth*selection*microRamp*am)*liveGuard;
        osc*=unit;
    }
    double noise=nLP.process(nHP.process(rng.normal()))*2.0;
    double core=amp*(tone+noise*(.0011+.011*std::exp(-t/.023)));
    double hp=std::exp(-2*pi*10/fs);double dc=core-dcX+hp*dcY;dcX=core;dcY=dc;
    double air=aLP.process(aHP.process(airRng.normal()))*2.2;
    double onset=t<.028?std::pow(std::sin(pi*t/.028),2):0;
    double toneNote=movingSpectrum?effectivePitch:midi;
    double oct=(toneNote-72.)/12.;double airTrim=std::pow(10.,-(2*std::max(oct,0.)+std::max(-oct,0.))/20.);
    double layer=air*calibration[3]*airTrim*(onset*std::pow(10.,-44./20)+amp/std::max(calibration[4],1e-12)*.001);
    core=(dc+layer)*smooth(t/.002);
    double initial=initialRate(toneNote);
    double held=gate<0?t:std::min(t,gate),elapsed=rateIntegral(held);
    if(gate>=0)elapsed+=(t-gate)*(1+smooth(gate/1.1));
    double depth=1-.35*smooth((toneNote-48.)/36.),fade=smooth(t/.005);
    double p1=flutterDynamic?flutterPhase1:2*pi*initial*elapsed,p2=flutterDynamic?flutterPhase2:2*pi*initial*1.5*elapsed;
    double big=(1-depth*fade/3*(1-std::cos(p1))/2)*(1-depth*fade/6*(1-std::cos(p2))/2);
    if(flutterDynamic){double speed=initial*(1+smooth(held/1.1));flutterPhase1+=2*pi*speed/fs;flutterPhase2+=2*pi*speed*1.5/fs;}
    double burst=bLP.process(bHP.process(burstRng.normal()))*2.05;
    double duration=.036,env=smooth(t/.004)*std::exp(-std::max(t-.004,0.)/.008)*(1-smooth((t-.026)/.010));
    if(t>=duration)env=0;
    double bTrim=std::pow(10.,-2*std::max(0.,(toneNote-84.)/12.)/20.);
    double transient=burst*env*calibration[5]*std::pow(10.,-24./20)*.5*bTrim;
    double decay=held<=.036?1:.72+.28*std::pow(1-smooth((held-.036)/.144),1.5);
    double small=1-.12*(1-std::cos(2*pi*12*t))/2;
    double result=(core*big+transient)*decay*small;
    if(fadeLeft>=0){result*=smooth(fadeLeft/.005);fadeLeft-=1/fs;if(fadeLeft<=0)active=false;}
    ++framePos;t+=1/fs;return result;
}

void Engine::prepare(double sampleRate){
    fs=sampleRate;textureDecay=std::exp(-1/(fs*.005));bendDecay=std::exp(-1/(fs*.003));
    eq[0].set(3,fs,400,-2,.7071067811865476);eq[1].set(2,fs,1500,-1.8,1.1);eq[2].set(2,fs,3500,1.8,.9);reset();
}
void Engine::reset(){
    for(auto& v:voices)v.active=false;for(auto& v:retiring)v.active=false;
    for(auto& p:presses)p.held=false;for(auto& e:eq)e.reset();
    sequence=pressSequence=0;texture=targetTexture;timbre=targetTimbre;polyGain=targetPolyGain;wheel=8192;bend=targetBend=0;previousPitch=-1;
}
void Engine::setTextureLevel(int level){static constexpr double levels[6]={0,.08,.10,.12,.14,.16};targetTexture=levels[std::clamp(level,0,5)];}
bool Engine::sounding()const{for(const auto& v:voices)if(v.active)return true;for(const auto& v:retiring)if(v.active)return true;return false;}
int Engine::activeVoices()const{int n=0;for(const auto& v:voices)if(v.active)++n;return n;}
int Engine::heldNotes()const{int n=0;for(const auto& p:presses)if(p.held)++n;return n;}
Engine::Press* Engine::newestHeld(){Press* best=nullptr;for(auto& p:presses)if(p.held&&(!best||p.id>best->id))best=&p;return best;}
void Engine::retire(int slot){if(voices[slot].active){retiring[slot]=voices[slot];retiring[slot].fadeLeft=.005;voices[slot].active=false;}}
void Engine::setVoiceMode(int value){
    value=value==0?0:1;if(mode==value)return;
    for(int i=0;i<maxVoices;++i)retire(i);
    mode=value;previousPitch=-1;
    // Keep physical press identities across mode switches, including old
    // same-pitch Note Offs. Revoice held keys with normal attack + old 5ms fade.
    if(mode==0){if(auto* p=newestHeld()){startVoice(0,*p,-1,0);previousPitch=p->note;}}
    else{
        uint64_t limit=UINT64_MAX;
        for(int i=0;i<maxVoices;++i){Press* p=nullptr;
            for(auto& item:presses)if(item.held&&item.id<limit&&(!p||item.id>p->id))p=&item;
            if(!p)break;startVoice(i,*p,-1,0);limit=p->id;
        }
    }
}
void Engine::setGlideEnabled(bool value){
    if(glide&&!value&&mode==0&&voices[0].active&&voices[0].glideDuration>0)voices[0].setTargetPitch(voices[0].pitchTarget,0);
    glide=value;
}
void Engine::setPitchBendRange(int semitones){
    if(semitones!=2&&semitones!=12&&semitones!=24)semitones=2;
    bendRange=semitones;double x=wheel-8192;targetBend=(x<0?x/8192.:x/8191.)*bendRange;
}
void Engine::pitchWheel(int channel,int value){if(channel<1||channel>16)return;wheel=std::clamp(value,0,16383);setPitchBendRange(bendRange);}
void Engine::startVoice(int slot,const Press& press,double from,double glideTime){
    retire(slot);auto& v=voices[slot];
    v.begin(press.note,press.velocity,press.channel,fs,seed+sequence++*104729,!tuningTest,timbre);
    v.token=press.id;v.order=press.id;v.polyphonic=mode!=0;
    if(glideTime>0&&from>=0&&from!=press.note){v.rootPitch=v.pitchStart=from;v.setTargetPitch(press.note,glideTime);v.setSpectrum(from);v.coefficients(0,v.start,v.enhStart);v.coefficients(64/fs,v.next,v.enhNext);}
}
void Engine::monoTransition(const Press& press,bool overlapping){
    auto& v=voices[0];bool useGlide=glide&&glideSeconds>0&&(overlapping||glideAlways);
    double from=v.active?v.rootPitch:previousPitch;
    if(overlapping&&legato&&v.active&&v.gate<0){
        v.setTargetPitch(press.note,useGlide?glideSeconds:0);
        v.midi=press.note;v.channel=press.channel;v.token=press.id;v.targetVelocity=press.velocity/127.;
    }else startVoice(0,press,from,useGlide?glideSeconds:0);
    previousPitch=press.note;
}
void Engine::noteOn(int ch,int midi,int vel){
    if(vel==0){noteOff(ch,midi);return;}if(midi<0||midi>127||ch<1||ch>16)return;
    bool overlapping=newestHeld()!=nullptr;
    Press* p=nullptr;for(auto& item:presses)if(!item.held){p=&item;break;}
    if(!p){p=&presses[0];for(auto& item:presses)if(item.id<p->id)p=&item;}
    *p={++pressSequence,midi,std::clamp(vel,1,127),ch,true};
    if(mode==0){monoTransition(*p,overlapping);return;}
    int slot=-1;for(int i=0;i<maxVoices;++i)if(!voices[i].active){slot=i;break;}
    if(slot<0){
        // Release-stage voices first, then oldest held voice. Token tracking
        // retains the stolen press so its later Note Off cannot stop a new one.
        slot=0;for(int i=1;i<maxVoices;++i){bool a=voices[i].gate>=0,b=voices[slot].gate>=0;
            if((a&&!b)||(a==b&&voices[i].order<voices[slot].order))slot=i;}
    }
    startVoice(slot,*p,-1,0);
}
void Engine::noteOff(int ch,int midi){
    if(ch<1||ch>16||midi<0||midi>127)return;
    Press* p=nullptr;for(auto& item:presses)if(item.held&&item.channel==ch&&item.note==midi&&(!p||item.id<p->id))p=&item;
    if(!p)return;uint64_t id=p->id;p->held=false;
    if(mode!=0){for(auto& v:voices)if(v.active&&v.token==id)v.release();return;}
    auto& v=voices[0];if(!v.active||v.token!=id)return;
    if(auto* next=newestHeld())monoTransition(*next,true);else{previousPitch=v.rootPitch;v.release();}
}
void Engine::allNotesOff(bool immediate,int channel){
    for(auto& p:presses)if(channel==0||p.channel==channel)p.held=false;
    for(auto& v:voices)if(v.active&&(channel==0||v.channel==channel)){
        if(immediate)v.active=false;
        else if(mode==0){if(auto* next=newestHeld())monoTransition(*next,true);else v.release();}
        else v.release();
    }
    if(immediate){for(auto& v:retiring)if(channel==0||v.channel==channel)v.active=false;if(!sounding())for(auto& e:eq)e.reset();}
}
double Engine::sample(){
    texture=targetTexture+(texture-targetTexture)*textureDecay;if(std::abs(texture-targetTexture)<1e-7)texture=targetTexture;
    timbre=targetTimbre+(timbre-targetTimbre)*textureDecay;if(std::abs(timbre-targetTimbre)<1e-10)timbre=targetTimbre;
    bend=targetBend+(bend-targetBend)*bendDecay;if(std::abs(bend-targetBend)<1e-10)bend=targetBend;
    polyGain=targetPolyGain+(polyGain-targetPolyGain)*textureDecay;if(std::abs(polyGain-targetPolyGain)<1e-10)polyGain=targetPolyGain;
    double x=0;for(auto& v:voices)if(v.active)x+=v.sample(texture,bend,timbre)*(v.polyphonic?polyGain:1.);
    for(auto& v:retiring)if(v.active)x+=v.sample(texture,bend,timbre)*(v.polyphonic?polyGain:1.);
    for(auto& e:eq)x=e.process(x);return x*outputGain;
}
}
