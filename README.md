# M5C3SE_MQTT 开发纪录

本项目是基于 **M5Stack CoreS3** 开发的 MQTT 监控与 OTA 更新系统，支持双网切换、实时诊断可视化、数值物理量监控及离线历史回顾。

---

## 开发规范 (Development Guidelines)

### 1. 版本号管理 (Versioning)
- **文件位置**：`version.h` (`#define FIRMWARE_VERSION`)
- **变更原则**：每次代码修改或 Bug 修复后，必须将 **Minor Version 第三位** 递增（例如 `1.1.16` -> `1.1.17`）。
- **同步要求**：需同步更新 `version.txt` 以确保 OTA 系统能识别最新版本。

### 2. 开发环境与工具链
- **编译器**：使用 `arduino-cli` 进行本地编译。
  - 核心 FQBN: `esp32:esp32:m5stack_cores3`
- **版本控制**：使用 `git` 进行管理，代码及编译后的 `.bin` 文件需同步推送至 GitHub 仓库。
- **本地烧录**：通过 USB 使用 `arduino-cli upload` 进行快速测试。
- **远程更新**：通过 GitHub HTTPS 实现无线 OTA 更新。

---

## DIAG 診斷圖示排列架構 (4x3 Grid Layout)

目前 DIAG 模式下的 4x3 圖示矩陣排列順序如下：

| 位置 | 欄 1 (Col 1) | 欄 2 (Col 2) | 欄 3 (Col 3) | 欄 4 (Col 4) |
| :--- | :--- | :--- | :--- | :--- |
| **列 1 (Row 1)** | 6. Bearing housing bolts looseness | 10. Bearing looseness | 7. Bearing housing looseness | 9. Bearing sleeve |
| **列 2 (Row 2)** | 8. Bearing damage | 5. Structural looseness | 2. Misalignment | 1. Unbalance |
| **列 3 (Row 3)** | 4. Oil Whirl | 11. Gearbox damage | 3. Vortex problem | 12. rotor eccentricity |

---

## 版本演進 (Development Log)

### v1.1.33 (Latest)
- **RSSI Topic 文字動態加大**：在 MQTT 訊息顯示模式中（含即時訊息與瀏覽歷史舊訊息），當 Topic 包含 `RSSI`（不限大小寫）時，內文顯示字體加大一號由 `Size 2` 自動升為 `Size 3`。

### v1.1.32
- **MQTT Server 自動設定為 Gateway IP**：在無 SD 卡且連線成功後，自動將 MQTT Broker 位址 (`mqttServer`) 設定為經由 DHCP 取得的網關位址 (`gatewayIP`)，PORT 保持為 `1883`。

### v1.1.31
- **無 SD 卡模式優化與 Topic 字體動態加大**：
  - **無 SD 自動切換與 MQTT 設定**：若開機檢測不到 SD 卡，自動切換開機與運行網路模式為 `LAN (DHCP)`，並於取得 IP 後將取得之本機 IP 設定為 `MQTT Broker IP`，連接埠固定為 `1883`。
  - **無 SD 卡訊息儲存過濾**：當 SD 卡不存在時，接收 MQTT 訊息自動跳過檔案儲存 (`/RECEIVR.TXT`)。
  - **Topic 選單預設值更新**：預設 Topic 列表更新為 `Prowave/#`、`PW/#`、`Advantech/#`，預設訂閱為 `Prowave/#`。
  - **動態字體加大**：在 MQTT 訊息顯示模式中，若 Topic 符合 `PW/#` 或 `Advantech/#`（含前綴），內文顯示字體加大一號由 `Size 2` 自動升為 `Size 3`。

### v1.1.30
- **DIAG 圖示排列重構與邏輯修正**：
  - **螢幕顯示位置重構**：更新 DIAG 模式下的 4x3 螢幕圖示位置對映（按指定 Display Position 排列：Row1 = 6, 10, 7, 9；Row2 = 8, 5, 2, 1；Row3 = 4, 11, 3, 12）。
  - **Address 7 解析修正**：修正 `Address7 == 1` 時僅單獨觸發水漩的問題，改為同時觸發 **3. Vortex problem** 與 **4. Oil Whirl**。

### v1.1.29
- **MQTT 訂閱 Topic 選單化與 SD 擴充**：
  - **Topic 選擇選單**：在 `SET MQTT` 的 Step 3 訂閱 Topic 設定中，改為圖形化選擇清單 (List UI)，取代原本的手動鍵盤輸入。
  - **預設選項與動態載入**：清單預設包含 `PROWAVE/#`、`ADVANTECH/#`、`PW/#`，並自動由 SD 卡 `/MQTTList.txt` 載入其他客製 Topic 選項。
  - **線上燒錄與驗證**：完成 `v1.1.29` 固件編譯並經由 USB (`COM5`) 成功燒錄至 M5Stack CoreS3 設備。

### v1.1.28
- **Speed 顯示切換與持久化**：
  - **切換功能**：在 VALUES 數值顯示模式下，點擊 Speed 顯示行可切換是否顯示（隱藏時顯示 `[Hidden]` 灰字）。
  - **設定保存**：設定狀態會寫入 SD 卡的 `/gui_pref.txt` 中，開機時自動載入。

### v1.1.27
- **架構重構與模組化**：
  - **模組拆分**：將單一龐大的 `M5C3SE_MQTT.ino` 拆分為 `config_mgr`、`network_mgr`、`mqtt_mgr`、`diag_mgr`、`gui` 與 `ota_mgr` 等 5 大模組。
  - **狀態解耦**：引進 `state.h` 集中管理狀態機與切換邏輯，降低模組間的耦合度。
  - **架構設計圖**：新增 `architecture.mmd` 設計檔，提供系統依賴架構圖以利未來擴充維護。

### v1.1.26
- **比例與警報優化**：
  - **轉速比例修正**：將 Address 4 (Speed) 比例修正為 **6.0 RPM**，單位同步更新。
  - **警報顏色連動**：在數值模式下，解析 JSON 中的 `AlarmX/Y/Z`。若警報觸發（值為 1），對應的 X/Y/Z 速度數值將以 **紅色** 顯示。

### v1.1.25
- **數值模式優化**：精簡數值模式 (VALUE Mode) 下的故障顯示。移除 "FAULT DETECTED" 標題，直接以 **紅字 Size 2** 顯示故障原因，視覺感官與上方數據一致。

### v1.1.24
- **語系回退**：應需求將診斷文字切換回 **全英文顯示**。
- **故障列表捲動**：實作數值模式下的多行故障捲動功能。當故障超過兩行時，右側會出現 **藍色箭頭 (^/v)**，支援觸控捲動查看完整列表。

### v1.1.23
- **中文字型支持 (實驗性)**：導入 `efont` 字型庫，試圖解決中文字元顯示為空白的問題。

### v1.1.22
- **新增「數值顯示」模式**：
  - **上半部**：即時顯示 Address 0-4 之物理量（X/Y/Z 速度、溫度、轉速），並自動進行單位換算（0.01/0.001/0.1 比例）。
  - **下半部**：以文字形式顯示診斷結果，無異常時顯示「Status Normal」。
- **模式選擇選單**：點擊螢幕任意處不再直接切換，而是彈出 **橘色模式選擇選單**（Monitor / Diagnostic / Values），並支援 5 秒無操作自動返回。

### v1.1.21
- **診斷狀態視覺化**：在 DIAG 模式下，若所有監測項目皆正常，螢幕中央會顯示大型綠色 **"ALL PASS"** 字樣，方便快速判斷。

### v1.1.20
- **圖標能見度提升**：將未觸發狀態下的圖標底色由黑色更改為 **深藍色 (Navy)**，確保在未發生故障時仍能清晰看見圖標內容。

### v1.1.19
- **強健性 JSON 解析**：
  - **結構相容**：支援標準 JSON 結構及嵌套於 `MODBUS` 物件下的格式。
  - **大小寫相容**：同時支援 `ADDRESS5` 與 `Address5` 等鍵名。
- **主題過濾**：明確過濾 `Prowave/IVM` 以外的主題，並加入 `try-catch` 風格的解析錯誤保護邏輯，防止異常 Payload 導致當機。

### v1.1.18 
- **MQTT JSON 解析**：導入 `ArduinoJson` 函式庫，支援解析 `Prowave/IVM` 主題之 JSON 資料（Address 5-9）。
- **DIAG 畫面強化**：實作圖示狀態顯示。觸發故障時顯示紅框並維持正常亮度；未觸發時則以變暗效果顯示。
- **Address 映射實作**：根據 `condition.txt` 完整實作 27 種 Address 5 組合邏輯及 Address 6-9 單一映射。

### v1.1.17
- **新增 DIAG 模式**：引入圖形化診斷界面（4x3 圖示矩陣），參考自 `M5_PDM_PLOT` 專案。
- **UI 切換邏輯**：在 `RUNNING` 模式點擊頂部標題區域即可切換至 `DIAG` 模式，點擊診斷畫面任意處可返回監控界面。

### v1.1.16 - v1.1.10 (核心摘要)
- **歷史紀錄與捲動**：實作長訊息垂直捲動與歷史紀錄回溯。
- **雙網與自動運行**：支援 WiFi/LAN 雙網記憶，並實作 30 秒無操作自動連線運行。
- **穩定性提升**：擴大 MQTT Buffer 至 2048 bytes，修復連線診斷 UI 與狀態鎖定邏輯。

---
*Developed by Gemini CLI Helper*
