#include <juce_audio_processors/juce_audio_processors.h>
#include "LunaCore.h"
#include "InterfaceText.h"
#include <fstream>
#include <iostream>
#include <map>
#include <chrono>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void parameter(Luna::Engine& e,const juce::String& name,double value){
    if(name=="Harmonic Texture")e.setTextureLevel(int(std::round(value*5)));
    else if(name=="Voice Mode")e.setVoiceMode(value>=.5);
    else if(name=="Velocity Timbre Amount")e.setVelocityTimbre(value*100);
    else if(name=="Legato Enabled")e.setLegatoEnabled(value>=.5);
    else if(name=="Glide Enabled")e.setGlideEnabled(value>=.5);
    else if(name=="Glide Time")e.setGlideTime(value*500);
    else if(name=="Glide Mode")e.setGlideMode(value>=.5);
    else if(name=="Pitch Bend Range"){static constexpr int r[3]={2,12,24};e.setPitchBendRange(r[std::clamp(int(std::round(value*2)),0,2)]);}
    else if(name=="Poly Voice Gain")e.setPolyVoiceGain(value*100);
}
static juce::MidiMessage message(const juce::var& v){
    juce::String type=v["type"].toString();int ch=v.hasProperty("channel")?int(v["channel"]):1;
    int note=int(v["note"]),value=int(v["value"]);
    if(type=="on")return juce::MidiMessage::noteOn(ch,note,(juce::uint8)value);
    if(type=="off")return juce::MidiMessage::noteOff(ch,note);
    if(type=="bend")return juce::MidiMessage::pitchWheel(ch,value);
    if(type=="cc")return juce::MidiMessage::controllerEvent(ch,int(v["controller"]),value);
    throw std::runtime_error("Unknown MIDI event");
}
static void dispatch(Luna::Engine& e,const juce::MidiMessage& m){
    if(m.isNoteOn())e.noteOn(m.getChannel(),m.getNoteNumber(),m.getVelocity());
    else if(m.isNoteOff())e.noteOff(m.getChannel(),m.getNoteNumber());
    else if(m.isPitchWheel())e.pitchWheel(m.getChannel(),m.getPitchWheelValue());
    else if(m.isAllSoundOff())e.allNotesOff(true,m.getChannel());
    else if(m.isAllNotesOff())e.allNotesOff(false,m.getChannel());
}
int main(int argc,char** argv){try{
    check(argc==4,"plugin.vst3 scenario.json output.raw");
    juce::ScopedJuceInitialiser_GUI init;
    auto scene=juce::JSON::parse(juce::File(argv[2]));check(!scene.isVoid(),"Invalid JSON");
    double fs=double(scene["fs"]);int block=scene.hasProperty("block")?int(scene["block"]):128;
    int samples=int(std::round(double(scene["seconds"])*fs));bool reference=bool(scene["reference"]);
    juce::AudioPluginFormatManager manager;juce::addDefaultFormatsToManager(manager);
    juce::OwnedArray<juce::PluginDescription> types;
    for(int i=0;i<manager.getNumFormats();++i)manager.getFormat(i)->findAllTypesForFile(types,argv[1]);
    check(!types.isEmpty(),"No VST3 class");
    juce::String error;auto plugin=manager.createPluginInstance(*types[0],fs,block,error);check(plugin!=nullptr,error.toRawUTF8());
    check(plugin->acceptsMidi(),"MIDI input missing");
    std::map<juce::String,juce::AudioProcessorParameter*> params;
    for(auto* p:plugin->getParameters()){
        auto name=p->getName(128);
        // JUCE's VST3 wrapper also publishes MIDI-controller transport and bypass.
        if(name=="Bypass"||name.startsWith("MIDI CC "))continue;
        params[name]=p;
    }
    juce::MemoryBlock defaultState;plugin->getStateInformation(defaultState);
    if(!reference){
        std::map<juce::String,double> expected={{"Harmonic Texture",.6},{"Voice Mode",0},{"Velocity Timbre Amount",0},{"Legato Enabled",1},{"Glide Enabled",0},{"Glide Time",.16},{"Glide Mode",0},{"Pitch Bend Range",0},{"Poly Voice Gain",1},{"Interface Language",0}};
        check(params.size()==10,"Ten parameters required");
        for(auto& x:expected){auto it=params.find(x.first);check(it!=params.end(),"Missing parameter");check(it->second->isAutomatable(),"Not automatable");check(std::abs(it->second->getDefaultValue()-x.second)<2e-5,"Wrong default");it->second->setValueNotifyingHost(x.second==1?0.f:1.f);}
        juce::MemoryBlock edited;plugin->getStateInformation(edited);
        auto restored=manager.createPluginInstance(*types[0],fs,block,error);check(restored!=nullptr,"Reload instance failed");
        restored->setStateInformation(edited.getData(),int(edited.getSize()));
        for(auto* p:restored->getParameters())if(auto it=params.find(p->getName(128));it!=params.end())check(std::abs(p->getValue()-it->second->getValue())<2e-5,"State restore mismatch");
        plugin->setStateInformation(defaultState.getData(),int(defaultState.getSize()));
    }
    auto core=std::make_unique<Luna::Engine>();core->prepare(fs);
    if(scene.hasProperty("legacy_state")){
        // Loading an old state must use defaults for absent new parameters,
        // including when the instance was previously edited.
        params["Poly Voice Gain"]->setValueNotifyingHost(.25f);params["Interface Language"]->setValueNotifyingHost(1.f);
        juce::MemoryBlock legacy;check(juce::File(scene["legacy_state"].toString()).loadFileAsData(legacy),"Cannot read V0.1 state");
        plugin->setStateInformation(legacy.getData(),int(legacy.getSize()));
        if(auto* expected=scene["legacy_expect"].getDynamicObject())for(auto& item:expected->getProperties())check(std::abs(params[item.name.toString()]->getValue()-float(item.value))<2e-5,"Legacy parameter mismatch");
        check(params["Poly Voice Gain"]->getValue()==1.f&&params["Interface Language"]->getValue()==0.f,"Old state must use gain100 and Chinese defaults");
        for(auto& p:params)parameter(*core,p.first,p.second->getValue());
    }
    if(auto* settings=scene["parameters"].getDynamicObject())for(auto& p:settings->getProperties()){
        auto name=p.name.toString();check(params.count(name)!=0,"Unknown parameter");params[name]->setValueNotifyingHost(float(p.value));parameter(*core,name,double(p.value));
    }
    plugin->prepareToPlay(fs,block);core->reset();check(plugin->getLatencySamples()==0,"Unexpected latency");
    if(bool(scene["save_state"])){
        juce::MemoryBlock saved;plugin->getStateInformation(saved);
        check(juce::File(argv[3]).withFileExtension(".state").replaceWithData(saved.getData(),saved.getSize()),"Could not write state fixture");
    }
    if(bool(scene["capture_gui"])){
        auto* editor=plugin->createEditorIfNeeded();check(editor!=nullptr,"No editor");
        check(editor->getWidth()==760&&editor->getHeight()==650,"Wrong editor dimensions");
        delete editor;
        // A hosted editor embeds a native HWND and cannot be captured by JUCE's
        // component snapshot. Paint the identical editor source offscreen.
        std::unique_ptr<juce::AudioProcessor> local(createPluginFilter());
        std::unique_ptr<juce::AudioProcessorEditor> localEditor(local->createEditorIfNeeded());
        localEditor->setVisible(true);
        auto* selector=dynamic_cast<juce::ComboBox*>(localEditor->findChildWithID("ui_language"));check(selector!=nullptr,"Language selector missing");
        auto face=LunaText::font(16).getTypefacePtr();int glyphs=0;
        for(auto c:LunaText::chineseCorpus())if(c>32){auto glyph=face->getNominalGlyphForCodepoint(c);check(glyph&&*glyph!=0,"Missing Chinese glyph");++glyphs;}
        std::vector<float> before;for(auto* p:local->getParameters())before.push_back(p->getValue());
        auto capture=[&](const char* extension){auto image=localEditor->createComponentSnapshot(localEditor->getLocalBounds(),true,1.f,juce::SoftwareImageType());check(image.getPixelAt(1,1).getAlpha()!=0,"Empty GUI image");juce::FileOutputStream stream(juce::File(argv[3]).withFileExtension(extension));juce::PNGImageFormat format;check(format.writeImageToStream(image,stream),"GUI snapshot failed");};
        const char* ids[9]={"voice_mode","poly_voice_gain","velocity_timbre_amount","legato_enabled","glide_enabled","glide_time_ms","glide_mode","pitch_bend_range","harmonic_texture_level"};
        for(int lang=0;lang<2;++lang){selector->setSelectedId(lang+1,juce::sendNotificationSync);
            for(int i=0;i<9;++i){auto* label=dynamic_cast<juce::Label*>(localEditor->findChildWithID("label_"+juce::String(ids[i])));check(label&&label->getText()==juce::String::fromUTF8(LunaText::labels[i][lang]),"Immediate label translation failed");
                juce::GlyphArrangement layout;layout.addLineOfText(LunaText::font(16),label->getText(),0,0);check(layout.getBoundingBox(0,-1,true).getWidth()<label->getWidth()-8,"Translated label truncated");}
            const char* boxes[4]={"voice_mode","glide_mode","pitch_bend_range","harmonic_texture_level"};
            for(int k=0;k<4;++k){auto* box=dynamic_cast<juce::ComboBox*>(localEditor->findChildWithID(boxes[k]));check(box&&box->getSelectedId()>0,"Translated selection lost");
                for(int i=0;i<box->getNumItems();++i){const char* wanted=k==0?LunaText::voice[i][lang]:k==1?LunaText::glide[i][lang]:k==2?LunaText::bend[i][lang]:LunaText::texture[i][lang];check(box->getItemText(i)==juce::String::fromUTF8(wanted),"Menu item translation failed");}
                check(box->getText()==box->getItemText(box->getSelectedItemIndex()),"Selected text still has old language");}
            int i=0;for(auto* p:local->getParameters()){auto* identified=dynamic_cast<juce::AudioProcessorParameterWithID*>(p);if(!identified||identified->paramID!="ui_language")check(p->getValue()==before[size_t(i)],"Language changed sound parameter");++i;}
            capture(lang==0?".zh-TW.png":".en.png");
        }
        auto* about = dynamic_cast<juce::TextButton*>(localEditor->findChildWithID("about_button"));
        auto* panel = localEditor->findChildWithID("about_panel");
        check(about && about->isVisible() && about->getButtonText() == "About", "About button missing");
        check(!about->getBounds().intersects(selector->getBounds()), "About overlaps language controls");
        check(panel && !panel->isVisible(), "About should start closed");
        std::vector<float> beforeAbout;
        for (auto* p : local->getParameters())
            beforeAbout.push_back(p->getValue());
        about->onClick();
        check(panel->isVisible(), "About did not open");
        for (const auto* id : ids)
            check(!localEditor->findChildWithID(id)->isVisible(), "Controls should be hidden in About view");
        auto labelText=[&](const char* id){
            auto* label=dynamic_cast<juce::Label*>(panel->findChildWithID(id));
            check(label && label->isVisible(),"About label missing");
            return label->getText();
        };
        check(labelText("about_name")=="Luna Tp" && labelText("about_version")=="Alpha V0.2.1", "About identity incorrect");
        auto notice = labelText("about_notice");
        for (const auto* expected : {u8"Copyright © 2026 AAAA_Geen", "Licensed under GNU AGPL-3.0-only.",
             "This software comes without warranty.",
             "You may redistribute and modify this software under the terms of the GNU AGPLv3."})
            check(notice.contains(juce::String::fromUTF8(expected)), "Incomplete About notice");
        for (const auto& line : juce::StringArray::fromLines(notice)) {
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText(LunaText::font(16), line, 0, 0);
            check(glyphs.getBoundingBox(0, -1, true).getWidth() <
                panel->findChildWithID("about_notice")->getWidth() - 10, "About notice truncated");
        }
        auto linkURL=[&](const char* id) {
            auto* link=dynamic_cast<juce::HyperlinkButton*>(panel->findChildWithID(id));
            check(link && link->isVisible() && link->isEnabled(),"About link unavailable");
            return link->getURL().toString(true);
        };
        check(linkURL("about_repository")=="https://github.com/Kyle123asser/LunaTp", "Repository URL incorrect");
        check(linkURL("about_license")=="https://github.com/Kyle123asser/LunaTp/blob/main/LICENSE", "License URL incorrect");
        capture(".about.png");
        auto* close=dynamic_cast<juce::TextButton*>(panel->findChildWithID("about_close"));
        check(close && close->isVisible() && close->getButtonText() == "Close", "About Close missing");
        close->onClick();
        check(!panel->isVisible(), "Close did not dismiss About");
        for (const auto* id : ids)
            check(localEditor->findChildWithID(id)->isVisible(), "A control did not return");
        about->onClick();
        check(panel->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !panel->isVisible(),
              "Escape did not dismiss About");
        int aboutParam = 0;
        for (auto* p : local->getParameters())
            check(p->getValue()==beforeAbout[size_t(aboutParam++)], "About changed a parameter");
        // Exercise the warning at an actual >0dBFS signal; no limiter is inserted.
        for(auto* p:local->getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(p);id&&id->paramID=="voice_mode")p->setValueNotifyingHost(1);
        local->prepareToPlay(48000,128);
        for(int block=0;block<80;++block){juce::AudioBuffer<float> a(2,128);a.clear();juce::MidiBuffer m;if(block==0)for(int n=0;n<8;++n)m.addEvent(juce::MidiMessage::noteOn(1,71,(juce::uint8)127),0);local->processBlock(a,m);}
        selector->setSelectedId(1,juce::sendNotificationSync);
        auto* warning=dynamic_cast<juce::Label*>(localEditor->findChildWithID("gain_warning"));check(warning&&warning->getText().contains("0 dBFS"),"Overload warning missing");capture(".overload.png");
        auto gui=std::make_unique<juce::DynamicObject>();gui->setProperty("font",LunaText::fontFamily);gui->setProperty("checked_glyphs",glyphs);gui->setProperty("languages",2);gui->setProperty("immediate_translation",true);gui->setProperty("sound_parameters_unchanged",true);gui->setProperty("overload_warning",true);
        gui->setProperty("about_open_close_escape",true);
        gui->setProperty("about_legal_text_and_urls",true);
        gui->setProperty("about_parameters_unchanged",true);
        juce::File(argv[3]).withFileExtension(".gui.json").replaceWithText(juce::JSON::toString(juce::var(gui.release()),true));
    }
    auto* events=scene["events"].getArray();check(events!=nullptr,"No events array");
    std::ofstream out(argv[3],std::ios::binary);
    std::ofstream trace(std::string(argv[3])+".csv");trace<<"sample,voices,held,note,pitch,bend,age,phase,gate\n";
    double peak=0,delta=0,late=0,maxError=0,last=0,pluginTime=0,maxBlock=0;int over=0;auto start=std::chrono::steady_clock::now();
    std::vector<float> bufferCore(size_t(block),0.f);
    for(int pos=0;pos<samples;pos+=block){int n=std::min(block,samples-pos);juce::AudioBuffer<float> audio(2,n);audio.clear();juce::MidiBuffer midi;
        for(const auto& event:*events)if(event["type"].toString()=="parameter"&&int(event["sample"])==pos){
            auto name=event["name"].toString();check(params.count(name)!=0,"Unknown automation parameter");
            params[name]->setValueNotifyingHost(float(event["value"]));parameter(*core,name,double(event["value"]));
        }
        for(const auto& event:*events){int s=int(event["sample"]);if(s>=pos&&s<pos+n&&event["type"].toString()!="parameter")midi.addEvent(message(event),s-pos);}
        for(int j=0;j<n;++j){for(const auto& event:*events)if(int(event["sample"])==pos+j&&event["type"].toString()!="parameter")dispatch(*core,message(event));
            bufferCore[size_t(j)]=float(core->sample());if((pos+j)%64==0){const auto& v=core->voice(0);trace<<pos+j<<','<<core->activeVoices()<<','<<core->heldNotes()<<','<<core->currentNote()<<','<<v.rootPitch<<','<<core->bendSemitones()<<','<<v.t<<','<<v.phase<<','<<v.gate<<'\n';}}
        auto beginBlock=std::chrono::steady_clock::now();plugin->processBlock(audio,midi);
        double blockTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-beginBlock).count();pluginTime+=blockTime;maxBlock=std::max(maxBlock,blockTime);
        for(int j=0;j<n;++j){float x=audio.getSample(0,j);check(std::isfinite(x),"Non-finite audio");check(x==audio.getSample(1,j),"Stereo mismatch");
            peak=std::max(peak,std::abs(double(x)));delta=std::max(delta,std::abs(x-last));last=x;
            if(std::abs(x)>=1)++over;
            if(pos+j>samples-int(.05*fs))late=std::max(late,std::abs(double(x)));
            if(!reference)maxError=std::max(maxError,std::abs(double(x)-bufferCore[size_t(j)]));out.write((char*)&x,sizeof(x));}
    }
    if(!bool(scene["allow_overload"]))check(peak<1,"Clipping");if(!reference)check(maxError<2e-6,"Host/core event mismatch");
    if(bool(scene["expect_silence_end"]))check(late<1e-6,"Stuck note / release tail");
    auto result=std::make_unique<juce::DynamicObject>();
    result->setProperty("plugin",types[0]->name);result->setProperty("format",types[0]->pluginFormatName);result->setProperty("version",types[0]->version);
    result->setProperty("parameters",int(params.size()));result->setProperty("sample_rate",fs);result->setProperty("peak",peak);result->setProperty("max_sample_step",delta);result->setProperty("end_peak",late);result->setProperty("core_max_difference",maxError);
    result->setProperty("samples_above_unity",over);result->setProperty("overload_risk",peak>=1);
    auto ids=std::make_unique<juce::DynamicObject>();
    for(auto& item:params){auto* hosted=dynamic_cast<juce::HostedAudioProcessorParameter*>(item.second);check(hosted!=nullptr,"Hosted parameter ID unavailable");ids->setProperty(item.first,hosted->getParameterID());}
    result->setProperty("parameter_ids",juce::var(ids.release()));
    result->setProperty("render_seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());result->setProperty("state_restore_checked",!reference);
    result->setProperty("plugin_process_seconds",pluginTime);result->setProperty("plugin_realtime_fraction",pluginTime/(samples/fs));result->setProperty("max_block_seconds",maxBlock);
    std::cout<<juce::JSON::toString(juce::var(result.release()),true)<<'\n';plugin->releaseResources();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
