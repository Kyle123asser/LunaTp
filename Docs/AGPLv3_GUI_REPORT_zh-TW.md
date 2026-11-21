# Luna Tp Alpha V0.2.1 About 版驗證

標題列的 About 開啟授權頁。Close 或 Esc 返回控制介面。聲明包含 Copyright © 2026 AAAA_Geen、AGPL-3.0-only、無擔保、依授權修改與再散布的權利、原始碼入口和完整授權連結。這些欄位對應 AGPLv3 §0 定義的介面聲明。[AGPLv3 全文](../LICENSE)

| 檢查 | 結果 |
| --- | --- |
| 元件與文字 | 測試主機核對 About、Close、Esc、Repository 與 License 連結，以及中英文控制頁和參數值 |
| Windows x64 | MSVC 19.51.36260、CMake、Ninja 與固定版 JUCE 9.0.5 建置成功 |
| 演奏測試 | CTest 2/2 通過，含 228 項檢查 |
| 音訊比對 | 44.1、48、96 kHz 共 15 組 MIDI、Mono／Poly、Glide 等情境與原版逐樣本相同 |
| VST3 | JUCE 測試主機載入成功，類別與參數 ID 相同 |

公開版採用已通過上列測試的 Windows VST3。比對確認合成器與係數和原版相同，插件原始碼變更限於 About 介面。VST3 DLL SHA-256

    5b557f2aa00f4392056fd738d55301b3f33b48885a294d54b96df2b7de756f7d

對應來源與公開下載安排見[發布說明](../SOURCE_DISTRIBUTION.md)。
