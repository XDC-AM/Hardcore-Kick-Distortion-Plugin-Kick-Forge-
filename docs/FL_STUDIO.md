# KickForge in FL Studio / 在 FL Studio 中使用 KickForge

## 1. Install / 安装

1. Copy `KickForge.dll` to your VST2 folder, for example:
2. 把 `KickForge.dll` 复制到 VST2 目录，例如：

```text
C:\Program Files\Common Files\VST2\KickForge.dll
```

3. Open **64-bit FL Studio**.
4. 打开 **64 位 FL Studio**。
5. Go to **Options → Manage plugins** and add the VST2 folder if it is not listed.
6. 进入 **Options → Manage plugins**，如果列表中没有该 VST2 目录就添加它。
7. Click **Find more plugins / Scan**.
8. 点击 **Find more plugins / Scan** 扫描。
9. Load **KickForge** from the plugin browser.
10. 从插件浏览器加载 **KickForge**。

Only the DLL is needed. No background image, theme file, or sample pack is required.

只需要 DLL，不需要背景图、主题文件或采样包。

---

## 2. First sound / 第一次出声

1. Load the **HARDSTYLE PUNCH** preset.
2. 载入 **HARDSTYLE PUNCH** 预设。
3. Confirm that **Mono** is ON and **Oversample** is 4x.
4. 确认 **Mono** 开启、**Oversample** 为 4x。
5. If the kick is already loud, set **Input** to about -6 dB.
6. 如果 kick 本身很响，把 **Input** 设到约 -6 dB。
7. Use **Output** and **Ceiling** to control the final peak.
8. 用 **Output** 与 **Ceiling** 控制最终峰值。

---

## 3. UI SETUP / 界面设置

Open **UI SETUP** in the preset panel to change:

在预设面板打开 **UI SETUP**，可以修改：

| English | 中文 |
|---|---|
| Interface scale: 100% / 125% / 150%. | 界面比例：100% / 125% / 150%。 |
| Finish: STYLE 01 Chrome Glass or STYLE 02 Brood Chrome. | 外观：STYLE 01 Chrome Glass 或 STYLE 02 Brood Chrome。 |
| Accent: ORIGINAL, MAGENTA, CRIMSON, or INK BLUE. | 强调色：ORIGINAL、MAGENTA、CRIMSON、INK BLUE。 |

Changes are applied immediately. Scale also resizes the host window.

修改会立即生效；比例变化同时调整宿主窗口大小。

---

## 4. Theme memory / 主题记忆

KickForge saves the current scale, finish, and accent color to:

KickForge 会把当前比例、外观和强调色保存到：

```text
%APPDATA%\KickForge\settings.ini
```

The next instance restores the same global look. A project chunk stores its own UI state and overrides the global value when that project is opened.

下一次打开插件会恢复同一套全局外观。工程 chunk 也保存自己的 UI 状态；打开该工程时会覆盖全局值。

If `%APPDATA%` is not writable, the plugin falls back to `KickForge.ini` in the DLL folder.

如果 `%APPDATA%` 不可写，会回退到 DLL 同目录的 `KickForge.ini`。

To reset the look, close FL Studio, delete the settings file, and reopen the plugin.

想恢复默认外观：关闭 FL Studio，删除该设置文件，再重新打开插件。

---

## 5. Loading old projects / 打开旧工程

- 2.1.2 reads v1, v2, and v3 chunks.
- 2.1.2 可读取 v1、v2、v3 chunk。
- If an old project has no UI state, the global settings file is used.
- 如果旧工程没有 UI 状态，则使用全局设置文件。
- If the sample rate changes, the DSP recalculates coefficients automatically.
- 采样率变化时，DSP 会自动重算系数。

---

## 6. Stability / 稳定性

2.1.1 and later fix the `KickForge.dll_unloaded` crash: the editor window procedure is detached and the window is destroyed before the DLL unloads.

2.1.1 及后续版本修复了 `KickForge.dll_unloaded` 闪退：DLL 卸载前会解除编辑器窗口过程并销毁窗口。

If a crash still occurs, check **Windows Event Viewer → Windows Logs → Application** and note:

如果仍然闪退，请查看 **Windows 事件查看器 → Windows 日志 → 应用程序**，记录：

- Faulting module name / 出错模块名称
- Exception code / 异常代码
- Fault offset / 错误偏移

Send those three values with the FL Studio version.

请把这三项和 FL Studio 版本一起反馈。

---

## 7. FAQ / 常见问题

| English | 中文 |
|---|---|
| KickForge does not appear: confirm the DLL is 64-bit and in a scanned VST2 folder. | 找不到 KickForge：确认 DLL 是 64 位，且位于已扫描的 VST2 目录。 |
| No sound: check Mix, Bypass, Output, Ceiling, and the channel routing. | 没有声音：检查 Mix、Bypass、Output、Ceiling 与通道路由。 |
| Too much CPU: use 2x or 1x oversampling and disable Auto Gain. | CPU 占用过高：使用 2x 或 1x 过采样，并关闭 Auto Gain。 |
| Too much sub: enable Mono, raise Low Cut, or reduce Body. | 低频过多：开启 Mono、提高 Low Cut，或降低 Body。 |
| Too sharp: lower High Drive or raise the High Split. | 高频太刺：降低 High Drive，或提高 High Split。 |
| UI too small: use UI SETUP and choose 125% or 150%. | 界面太小：打开 UI SETUP，选择 125% 或 150%。 |
| Theme not remembered: confirm the settings file is writable. | 主题没有记住：确认设置文件可写。 |
