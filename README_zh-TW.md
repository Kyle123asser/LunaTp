# Luna Tp

[English](README.md)

Luna Tp 是 Windows x64 的原創合成銅管 VST3。這是個人興趣開發、AI 輔助程式製作的音源，靈感來自東方 Project 玩家所稱的 ZUNpet 音色。Luna Tp 為非官方作品，與東方 Project 或 Roland 無隸屬關係。音色使用解析諧波與合成噪聲產生。本次檢查的 VST3 原始碼與束件未包含商業音源採樣。

**Alpha V0.2.1 · Experimental / As-is**  
專案不保證持續更新、維護或提供技術支援。

## 安裝與演奏

1. 依下文自行建置，或在正式發布後從專案的 Release 頁面取得 Windows ZIP。
2. 關閉 DAW，將完整 **Luna Tp.vst3** 資料夾複製到 **C:\Program Files\Common Files\VST3**。Windows 可能要求管理員權限。
3. 在 FL Studio 開啟 **Options → Manage plugins**，勾選 **Verify plugins** 後掃描。更新舊版時可勾選 **Rescan previously verified plugins**。將 Luna Tp 加入為樂器，從 Piano Roll 或 MIDI 控制器送入音符。[FL Studio 安裝手冊](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm)

建置目標為 Windows x64。Alpha V0.2 曾在 FL Studio 實機使用。本次重編的 V0.2.1 已在 JUCE VST3 測試主機載入與驗證。這個 V0.2.1 束件尚待 FL Studio 與其他 DAW 實測。若 DAW 回報缺少 MSVC 執行階段，可使用 Microsoft 提供的現行 x64 Visual C++ v14 Redistributable。[Microsoft 執行階段說明](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)

| 控制項 | 行為 |
| --- | --- |
| 發聲模式 | 預設 Mono 單音。Poly 同時支援至多八聲部，滿額時會替換聲部。 |
| 連音 | Mono 下重疊音符可延續原有起音與包絡。預設開啟。 |
| 滑音 | Mono 的音高滑動。預設關閉，時間 80 ms。Legato 模式於音符重疊時滑音，Always 也處理分離音符。 |
| 彎音範圍 | MIDI 音高輪可選 ±2、±12、±24 半音，預設 ±2。Poly 下作用於所有發聲聲部。 |
| 力度音色影響 | 調整擊鍵力度對音色的影響，預設 0%。 |
| 諧波質感 | 六段，由關閉至 16%，預設 12%。 |
| 複音單聲部音量 | 預設 100%。12.5% 對應 V0.2 的複音增益。只作用於 Poly。 |
| 介面語言 | 繁體中文及英文，預設繁中。 |

放開按鍵或送入 Note Off 可釋放聲部，也支援 MIDI All Notes Off 和 All Sound Off。參數由 DAW 保存。讀取 V0.2 工程時，新複音音量欄位採 100%，播放前請留意音量。高音量疊加可能超過 0 dBFS，介面會提示過載，輸出未經限制器處理。

## 編譯與測試

需要現有 Windows x64 MSVC 開發環境與 Windows SDK、CMake 3.25 以上、Ninja 和 PowerShell。可使用 x64 Developer PowerShell，腳本也能透過 Visual Studio Installer 尋找 MSVC。CMake 固定使用 JUCE 9.0.5，下載時核對 SHA-256。測試需 Python 3.10 以上及標準函式庫。

~~~powershell
.\build.ps1
.\test.ps1
~~~

成品位於 **Build/LunaTp_artefacts/Release/VST3/Luna Tp.vst3**。**-SkipTests** 只建置插件。兩個腳本都接受 **-BuildDirectory**。已取得的官方 9.0.5 ZIP 可透過 **-JuceArchive** 離線設定。自訂安裝位置可用 **-VcVarsPath** 指定 MSVC 的 x64 環境批次檔。[JUCE 9.0.5 官方版本](https://github.com/juce-framework/JUCE/releases/tag/9.0.5)

建置環境、音色比對與選用的係數重生工具，見[建置驗證紀錄](Docs/BUILD_VERIFICATION.md)。

## 授權與對應原始碼

Luna Tp 自有程式、資料與文件採 **GNU Affero General Public License 第 3 版限定（AGPL-3.0-only）**。Copyright © 2026 AAAA_Geen。完整條款見 [LICENSE](LICENSE)，授權範圍見 [COPYRIGHT.md](COPYRIGHT.md)。JUCE、VST 3 SDK 及其元件保留各自的[版權與授權聲明](THIRD_PARTY_LICENSES.md)。

取得 Windows 二進位檔時，可在同一 GitHub Release 頁面下載對應原始碼 ZIP。相同的 Git 標籤包含 Luna Tp 原始碼、生成係數、生成工具與建置腳本。JUCE 9.0.5 的固定來源、SHA-256 及離線建置方式見[對應原始碼說明](SOURCE_DISTRIBUTION.md)。

在適用法律允許範圍內，本專案依**現況（As-is）**提供，沒有保證。詳細條款見 AGPLv3 第 15 至 17 節。專案不保證持續更新、維護或提供技術支援。
