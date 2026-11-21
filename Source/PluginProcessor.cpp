#include <juce_audio_processors/juce_audio_processors.h>
#include "LunaCore.h"
#include "InterfaceText.h"
#include <atomic>
#include <functional>
#include <utility>

class LunaProcessor final : public juce::AudioProcessor {
public:
    LunaProcessor() : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
        state(*this,nullptr,"LunaTpState",layout()) {
        const char* ids[10]={"harmonic_texture_level","voice_mode","velocity_timbre_amount","legato_enabled","glide_enabled","glide_time_ms","glide_mode","pitch_bend_range","poly_voice_gain","ui_language"};
        for(int i=0;i<10;++i)values[i]=state.getRawParameterValue(ids[i]);
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"harmonic_texture_level",1},"Harmonic Texture",
            juce::StringArray{"Off","Light 8%","Light-Medium 10%","Medium 12%","Medium-Heavy 14%","Heavy 16%"},3));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"voice_mode",1},"Voice Mode",juce::StringArray{"Mono","Poly (8 voices)"},0));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"velocity_timbre_amount",1},"Velocity Timbre Amount",juce::NormalisableRange<float>(0,100,.1f),0.f,"%"));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"legato_enabled",1},"Legato Enabled",true));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"glide_enabled",1},"Glide Enabled",false));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"glide_time_ms",1},"Glide Time",juce::NormalisableRange<float>(0,500,.1f),80.f,"ms"));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"glide_mode",1},"Glide Mode",juce::StringArray{"Legato","Always"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"pitch_bend_range",1},"Pitch Bend Range",juce::StringArray{"+/- 2 semitones","+/- 12 semitones","+/- 24 semitones"},0));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"poly_voice_gain",1},"Poly Voice Gain",juce::NormalisableRange<float>(0,100,.1f),100.f,"%"));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"ui_language",1},"Interface Language",juce::StringArray{juce::String::fromUTF8(u8"繁體中文（台灣）"),"English"},0));
        return p;
    }
    const juce::String getName() const override { return "Luna Tp"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return .2; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return values[9]&&parameter(9)>=.5f?"Default":juce::String::fromUTF8(u8"預設"); }
    void changeProgramName(int,const juce::String&) override {}
    float parameter(int index) const { return values[index]->load(std::memory_order_relaxed); }
    void syncParameters() {
        engine.setVoiceMode(int(parameter(1)));
        engine.setTextureLevel(int(parameter(0)));
        engine.setVelocityTimbre(parameter(2));
        engine.setLegatoEnabled(parameter(3)>=.5f);
        engine.setGlideTime(parameter(5));
        engine.setGlideMode(int(parameter(6)));
        engine.setGlideEnabled(parameter(4)>=.5f);
        static constexpr int ranges[3]={2,12,24};
        engine.setPitchBendRange(ranges[std::clamp(int(parameter(7)),0,2)]);
        engine.setPolyVoiceGain(parameter(8));
    }
    void prepareToPlay(double fs,int) override { engine.prepare(fs);syncParameters();engine.reset(); }
    void releaseResources() override { engine.reset(); }
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainInputChannelSet().isDisabled() && (l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()||l.getMainOutputChannelSet()==juce::AudioChannelSet::mono());
    }
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) override {
        juce::ScopedNoDenormals noDenormals;audio.clear();syncParameters();
        auto event=midi.begin();auto end=midi.end();
        float blockPeak=0;
        for(int i=0;i<audio.getNumSamples();++i) {
            while(event!=end&&(*event).samplePosition<=i) {
                auto m=(*event).getMessage();int ch=m.getChannel();
                if(m.isNoteOn())engine.noteOn(ch,m.getNoteNumber(),m.getVelocity());
                else if(m.isNoteOff())engine.noteOff(ch,m.getNoteNumber());
                else if(m.isPitchWheel())engine.pitchWheel(ch,m.getPitchWheelValue());
                else if(m.isAllSoundOff())engine.allNotesOff(true,ch);
                else if(m.isAllNotesOff())engine.allNotesOff(false,ch);
                ++event;
            }
            float x=float(engine.sample());blockPeak=std::max(blockPeak,std::abs(x));for(int c=0;c<audio.getNumChannels();++c)audio.setSample(c,i,x);
        }
        count.store(engine.activeVoices(),std::memory_order_relaxed);midi.clear();
        float previous=peak.load(std::memory_order_relaxed);
        while(previous<blockPeak&&!peak.compare_exchange_weak(previous,blockPeak,std::memory_order_relaxed)){}
    }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock& out) override { auto xml=state.copyState().createXml();copyXmlToBinary(*xml,out); }
    void setStateInformation(const void* data,int size) override {
        auto xml=getXmlFromBinary(data,size);
        if(!xml||!xml->hasTagName(state.state.getType()))return;
        // Missing parameters use defined defaults even when loading an old state
        // into an instance whose new controls were previously edited.
        auto incoming=juce::ValueTree::fromXml(*xml),merged=state.copyState();
        for(auto target:merged){auto id=target.getProperty("id").toString();if(auto* p=state.getParameter(id))target.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);}
        for(auto child:incoming) {
            auto id=child.getProperty("id");
            for(auto target:merged)if(target.getProperty("id")==id)target.setProperty("value",child.getProperty("value"),nullptr);
        }
        state.replaceState(merged);
    }
    Luna::Engine engine;
    juce::AudioProcessorValueTreeState state;
    std::array<std::atomic<float>*,10> values{};
    std::atomic<int> count{0};
    std::atomic<float> peak{0};
};

class LunaLook final : public juce::LookAndFeel_V4 {
public:
    juce::Font getLabelFont(juce::Label& label) override {
        if (label.getComponentID() == "about_name")
            return LunaText::font(23).boldened();
        if (label.getComponentID() == "gain_warning")
            return LunaText::font(16).boldened();
        return LunaText::font(16);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override{return LunaText::font(16);}
    juce::Font getPopupMenuFont() override{return LunaText::font(16);}
    juce::Rectangle<int> getTooltipBounds(const juce::String&,juce::Point<int> position,juce::Rectangle<int> parent) override {
        return juce::Rectangle<int>(position.x+12,position.y+18,500,86).constrainedWithin(parent);
    }
    void drawTooltip(juce::Graphics& g,const juce::String& s,int w,int h) override {
        g.fillAll(juce::Colour(0xff343b49));g.setColour(juce::Colours::white);g.setFont(LunaText::font(14));
        g.drawFittedText(s,8,6,w-16,h-12,juce::Justification::centredLeft,4,1.f);
    }
};
class LunaAboutPanel final : public juce::Component {
public:
    explicit LunaAboutPanel(std::function<void()> onClose) {
        setComponentID("about_panel");
        setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
        setTitle("Luna Tp license information");

        heading.setText("Luna Tp", juce::dontSendNotification);
        heading.setComponentID("about_name");
        version.setText("Alpha V0.2.1", juce::dontSendNotification);
        version.setComponentID("about_version");
        notice.setText(juce::String::fromUTF8(
            u8"Copyright © 2026 AAAA_Geen\n"
            "Licensed under GNU AGPL-3.0-only.\n"
            "This software comes without warranty.\n"
            "You may redistribute and modify this software under the terms of the GNU AGPLv3."
        ), juce::dontSendNotification);
        notice.setComponentID("about_notice");
        for (auto* label : {&heading, &version, &notice}) {
            label->setColour(juce::Label::textColourId, juce::Colours::white);
            label->setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(label);
        }
        notice.setJustificationType(juce::Justification::topLeft);

        repository.setComponentID("about_repository");
        repository.setTooltip("Open the Luna Tp source repository on GitHub");
        license.setComponentID("about_license");
        license.setTooltip("Read the complete GNU AGPLv3 license on GitHub");
        for (auto* link : {&repository, &license}) {
            link->setColour(juce::HyperlinkButton::textColourId, juce::Colour(0xffa8d0ff));
            link->setFont(LunaText::font(16), false, juce::Justification::centredLeft);
            addAndMakeVisible(link);
        }
        close.setComponentID("about_close");
        close.onClick = std::move(onClose);
        addAndMakeVisible(close);
    }

    void focusCloseButton() {
        if (isShowing())
            close.grabKeyboardFocus();
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key.getKeyCode() != juce::KeyPress::escapeKey)
            return false;
        close.onClick();
        return true;
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff292e39));
        g.fillRoundedRectangle(bounds, 6.f);
        g.setColour(juce::Colour(0xff565f70));
        g.drawRoundedRectangle(bounds, 6.f, 1.f);
    }

    void resized() override {
        auto body = getLocalBounds().reduced(24);
        heading.setBounds(body.removeFromTop(32));
        version.setBounds(body.removeFromTop(26));
        body.removeFromTop(16);
        notice.setBounds(body.removeFromTop(104));
        body.removeFromTop(12);
        auto links = body.removeFromTop(32);
        repository.setBounds(links.removeFromLeft(142).reduced(4, 0));
        license.setBounds(links.removeFromLeft(120).reduced(4, 0));
        close.setBounds(body.removeFromBottom(32).removeFromRight(108));
    }

private:
    juce::Label heading, version, notice;
    juce::HyperlinkButton repository{"Repository", juce::URL("https://github.com/Kyle123asser/LunaTp")};
    juce::HyperlinkButton license{"License", juce::URL("https://github.com/Kyle123asser/LunaTp/blob/main/LICENSE")};
    juce::TextButton close{"Close"};
};

class LunaEditor final : public juce::AudioProcessorEditor,private juce::Timer {
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
public:
    explicit LunaEditor(LunaProcessor& p):AudioProcessorEditor(p),processor(p),tooltip(this,650) {
        setLookAndFeel(&look);
        for(auto& label:labels)addAndMakeVisible(label);
        for(auto* label:{&languageLabel,&status,&warning,&hint})addAndMakeVisible(label);
        mode.addItemList({"Mono","Poly (8 voices)"},1);glideMode.addItemList({"Legato","Always"},1);
        bendRange.addItemList({"+/- 2 semitones","+/- 12 semitones","+/- 24 semitones"},1);
        texture.addItemList({"Off","Light 8%","Light-Medium 10%","Medium 12%","Medium-Heavy 14%","Heavy 16%"},1);
        language.addItem(juce::String::fromUTF8(u8"繁體中文（台灣）"),1);language.addItem("English",2);
        for(auto* c:{&mode,&glideMode,&bendRange,&texture,&language})addAndMakeVisible(c);
        for(auto* slider:{&velocity,&time,&polyGain}){
            slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,110,26);addAndMakeVisible(slider);
        }
        velocity.setTextValueSuffix(" %");polyGain.setTextValueSuffix(" %");
        addAndMakeVisible(legato);addAndMakeVisible(glide);
        aboutButton.setComponentID("about_button");
        aboutButton.setTooltip("View copyright, license and source information");
        aboutButton.onClick = [this] { showAbout(!aboutPanel.isVisible()); };
        addAndMakeVisible(aboutButton);
        addChildComponent(aboutPanel);
        const char* controlIDs[9]={"voice_mode","poly_voice_gain","velocity_timbre_amount","legato_enabled","glide_enabled","glide_time_ms","glide_mode","pitch_bend_range","harmonic_texture_level"};
        juce::Component* namedControls[9]={&mode,&polyGain,&velocity,&legato,&glide,&time,&glideMode,&bendRange,&texture};
        for(int i=0;i<9;++i){namedControls[i]->setComponentID(controlIDs[i]);labels[i].setComponentID("label_"+juce::String(controlIDs[i]));}
        language.setComponentID("ui_language");warning.setComponentID("gain_warning");status.setComponentID("playing_status");
        languageLabel.setJustificationType(juce::Justification::centredRight);
        warning.setJustificationType(juce::Justification::centredLeft);
        warning.setMinimumHorizontalScale(1.f);
        auto& s=p.state;
        modeA=std::make_unique<ComboAttachment>(s,"voice_mode",mode);
        velocityA=std::make_unique<SliderAttachment>(s,"velocity_timbre_amount",velocity);
        legatoA=std::make_unique<ButtonAttachment>(s,"legato_enabled",legato);
        glideA=std::make_unique<ButtonAttachment>(s,"glide_enabled",glide);
        timeA=std::make_unique<SliderAttachment>(s,"glide_time_ms",time);
        glideModeA=std::make_unique<ComboAttachment>(s,"glide_mode",glideMode);
        bendA=std::make_unique<ComboAttachment>(s,"pitch_bend_range",bendRange);
        textureA=std::make_unique<ComboAttachment>(s,"harmonic_texture_level",texture);
        gainA=std::make_unique<SliderAttachment>(s,"poly_voice_gain",polyGain);
        languageA=std::make_unique<ComboAttachment>(s,"ui_language",language);
        language.onChange=[this]{translate(language.getSelectedItemIndex());};
        setSize(760,650);translate(int(p.parameter(9)));refreshStatus();startTimerHz(30);
    }
    ~LunaEditor() override{setLookAndFeel(nullptr);}
    void translate(int selected){
        lang=selected==1?1:0;
        juce::ComboBox* translatedBoxes[4]={&mode,&glideMode,&bendRange,&texture};
        int selectedIDs[4];for(int i=0;i<4;++i)selectedIDs[i]=translatedBoxes[i]->getSelectedId();
        for(int i=0;i<9;++i){labels[i].setText(juce::String::fromUTF8(LunaText::labels[i][lang]),juce::dontSendNotification);labels[i].setTooltip(juce::String::fromUTF8(LunaText::tips[i][lang]));}
        juce::Component* controls[9]={&mode,&polyGain,&velocity,&legato,&glide,&time,&glideMode,&bendRange,&texture};
        for(int i=0;i<9;++i)if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(controls[i]))tip->setTooltip(juce::String::fromUTF8(LunaText::tips[i][lang]));
        // Item IDs and selections are preserved; translating never sends a DSP value.
        for(int i=0;i<2;++i){mode.changeItemText(i+1,juce::String::fromUTF8(LunaText::voice[i][lang]));glideMode.changeItemText(i+1,juce::String::fromUTF8(LunaText::glide[i][lang]));}
        for(int i=0;i<3;++i)bendRange.changeItemText(i+1,juce::String::fromUTF8(LunaText::bend[i][lang]));
        for(int i=0;i<6;++i)texture.changeItemText(i+1,juce::String::fromUTF8(LunaText::texture[i][lang]));
        // JUCE caches the displayed label separately from the menu item text.
        for(int i=0;i<4;++i)translatedBoxes[i]->setSelectedId(selectedIDs[i],juce::dontSendNotification);
        languageLabel.setText(LunaText::text(lang,u8"介面語言","Language"),juce::dontSendNotification);
        language.setTooltip(juce::String::fromUTF8(LunaText::languageTip[lang]));
        time.setTextValueSuffix(LunaText::text(lang,u8" 毫秒"," ms"));
        hint.setText(juce::String::fromUTF8(LunaText::footer[lang]),juce::dontSendNotification);
        refreshStatus();repaint();
    }
    void refreshStatus(){
        int n=processor.count.load(std::memory_order_relaxed);
        auto ready=LunaText::text(lang,n==0?u8"就緒":u8"演奏中",n==0?"Ready":"Playing");
        status.setText(ready+LunaText::text(lang,u8"  |  發聲數：","  |  Active voices: ")+juce::String(n)+"  |  A4 = 440 Hz",juce::dontSendNotification);
        if(processor.peak.exchange(0.f,std::memory_order_relaxed)>=1.f)overloadTicks=90;
        const char* msg=overloadTicks>0?LunaText::overload[lang]:processor.parameter(8)==0?LunaText::mute[lang]:LunaText::risk[lang];
        warning.setText(juce::String::fromUTF8(msg),juce::dontSendNotification);
        warning.setColour(juce::Label::textColourId,overloadTicks>0?juce::Colour(0xffff8c85):juce::Colour(0xffffd18a));
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff20232b));g.setColour(juce::Colours::white);g.setFont(LunaText::font(23));
        g.drawText("Luna Tp | Alpha V0.2.1",18,12,300,32,juce::Justification::centredLeft);
        if (!aboutPanel.isVisible()) {
            g.setColour(juce::Colour(0xff8a6132));
            g.drawRoundedRectangle(18.f,518.f,722.f,90.f,5.f,1.f);
        }
    }
    void resized() override {
        aboutButton.setBounds(326, 14, 96, 30);
        aboutPanel.setBounds(18, 62, 724, 378);
        languageLabel.setBounds(430,14,110,30);language.setBounds(550,14,190,30);status.setBounds(18,56,722,28);
        juce::Component* controls[9]={&mode,&polyGain,&velocity,&legato,&glide,&time,&glideMode,&bendRange,&texture};
        for(int i=0;i<9;++i){int y=104+i*44;labels[i].setBounds(18,y,250,30);controls[i]->setBounds(280,y,460,30);}
        warning.setBounds(28,526,702,74);hint.setBounds(18,614,722,26);
    }
private:
    void showAbout(bool open) {
        for (auto& label : labels)
            label.setVisible(!open);
        juce::Component* controls[] = {&mode, &polyGain, &velocity, &legato, &glide, &time,
                                      &glideMode, &bendRange, &texture, &status, &warning, &hint};
        for (auto* component : controls)
            component->setVisible(!open);
        aboutPanel.setVisible(open);
        if (open)
            aboutPanel.focusCloseButton();
        else if (isShowing())
            aboutButton.grabKeyboardFocus();
        repaint();
    }

    void timerCallback() override {if(int(processor.parameter(9))!=lang)translate(int(processor.parameter(9)));if(overloadTicks>0)--overloadTicks;refreshStatus();}
    LunaProcessor& processor;
    LunaLook look;
    juce::TooltipWindow tooltip;
    int lang=-1,overloadTicks=0;
    std::array<juce::Label,9> labels;
    juce::Label languageLabel,status,warning,hint;
    juce::ComboBox mode,glideMode,bendRange,texture,language;
    juce::Slider velocity,time,polyGain;
    juce::ToggleButton legato,glide;
    juce::TextButton aboutButton{"About"};
    LunaAboutPanel aboutPanel{[this] { showAbout(false); }};
    std::unique_ptr<SliderAttachment> velocityA,timeA,gainA;
    std::unique_ptr<ButtonAttachment> legatoA,glideA;
    std::unique_ptr<ComboAttachment> modeA,glideModeA,bendA,textureA,languageA;
};
juce::AudioProcessorEditor* LunaProcessor::createEditor(){return new LunaEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new LunaProcessor;}

