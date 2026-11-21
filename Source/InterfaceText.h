#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace LunaText {
inline constexpr const char* fontFamily="Microsoft JhengHei";
inline juce::String text(int language,const char* zh,const char* en){return juce::String::fromUTF8(language==0?zh:en);}
inline constexpr const char* labels[9][2]={
    {u8"發聲模式","Voice Mode"},{u8"複音單聲部音量","Poly Voice Gain"},
    {u8"力度音色影響","Velocity Timbre Amount"},{u8"啟用連音","Legato Enabled"},
    {u8"啟用滑音","Glide Enabled"},{u8"滑音時間","Glide Time"},
    {u8"滑音模式","Glide Mode"},{u8"彎音範圍","Pitch Bend Range"},{u8"諧波質感","Harmonic Texture"}};
inline constexpr const char* tips[9][2]={
    {u8"單音：最新按鍵優先。複音：最多八個聲部同時演奏。","Mono uses last-note priority. Poly plays up to eight voices."},
    {u8"只調整複音模式。100%與單音使用相同單聲部音量；12.5%為V0.2舊音量；0%靜音。多音疊加可能削波。","Poly only: 100% equals Mono voice gain; 12.5% matches V0.2; 0% mutes Poly. Multiple voices can overload."},
    {u8"額外的力度連動諧波變化。0%保留原本音色與力度映射。","Additional velocity-driven harmonic expression. 0% preserves the original timbre and velocity mapping."},
    {u8"單音模式下，重疊按鍵延續原包絡，不重新觸發吹奏起音。","In Mono, overlapping notes retain the envelope without retriggering the brass attack."},
    {u8"啟用單音模式的音高滑音。","Enable pitch glide in Mono mode."},
    {u8"滑音時間：0至500毫秒。0毫秒立即切換音高。","Glide duration: 0 to 500 ms. 0 ms switches pitch immediately."},
    {u8"連音：只有重疊按鍵滑音。始終滑音：分離音符也可滑音。","Legato glides overlapping notes only. Always also glides separated notes."},
    {u8"彎音輪最大範圍：正負2、12或24半音。","Pitch wheel range: plus/minus 2, 12 or 24 semitones."},
    {u8"47赫茲諧波調變。關閉或選擇8%、10%、12%、14%、16%；預設為中度12%。","47 Hz harmonic modulation: Off, 8%, 10%, 12%, 14%, or 16%. Default: Medium 12%."}};
inline constexpr const char* voice[2][2]={{u8"單音","Mono"},{u8"複音（最多8聲部）","Poly (8 voices)"}};
inline constexpr const char* glide[2][2]={{u8"連音時滑音","Legato"},{u8"始終滑音","Always"}};
inline constexpr const char* bend[3][2]={{u8"±2 半音","+/- 2 semitones"},{u8"±12 半音","+/- 12 semitones"},{u8"±24 半音","+/- 24 semitones"}};
inline constexpr const char* texture[6][2]={{u8"關閉","Off"},{u8"輕度 8%","Light 8%"},{u8"輕中度 10%","Light-Medium 10%"},{u8"中度 12%","Medium 12%"},{u8"輕重度 14%","Medium-Heavy 14%"},{u8"重度 16%","Heavy 16%"}};
inline constexpr const char* languageTip[2]={u8"立即切換介面顯示語言，不改變音訊或演奏設定。語言會隨工程保存。","Change display language immediately without changing sound or performance settings. Saved with the project."};
inline constexpr const char* risk[2]={u8"注意：複音音量由您控制，多聲部疊加可能削波。100%不會自動降低音量或限幅；請視需要降低「複音單聲部音量」。","Warning: multiple Poly voices can overload. At 100% no automatic attenuation or limiting is applied. Reduce Poly Voice Gain when needed."};
inline constexpr const char* overload[2]={u8"警告：偵測到輸出超過0 dBFS，後續輸出可能削波。請降低「複音單聲部音量」或預留混音餘裕。","Warning: output exceeded 0 dBFS and may clip downstream. Reduce Poly Voice Gain or provide mixer headroom."};
inline constexpr const char* mute[2]={u8"複音單聲部音量為0%，複音將靜音；單音音量不受影響。","Poly Voice Gain is 0%: Poly is muted. Mono gain is unaffected."};
inline constexpr const char* footer[2]={u8"連音與滑音只適用於單音模式。語言切換不改變音色。","Legato and Glide apply to Mono only. Language changes do not affect sound."};
inline juce::Font font(float size){return juce::Font(juce::FontOptions(size).withName(fontFamily));}
inline juce::String chineseCorpus(){
    juce::String result;
    for(auto& a:labels)result+=juce::String::fromUTF8(a[0]);for(auto& a:tips)result+=juce::String::fromUTF8(a[0]);
    for(auto& a:voice)result+=juce::String::fromUTF8(a[0]);for(auto& a:glide)result+=juce::String::fromUTF8(a[0]);
    for(auto& a:bend)result+=juce::String::fromUTF8(a[0]);for(auto& a:texture)result+=juce::String::fromUTF8(a[0]);
    for(auto* a:{languageTip[0],risk[0],overload[0],mute[0],footer[0],u8"繁體中文（台灣）介面語言就緒演奏中發聲數毫秒預設"})result+=juce::String::fromUTF8(a);
    return result;
}
}
