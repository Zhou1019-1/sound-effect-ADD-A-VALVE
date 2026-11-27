# ADD A VALVE

<p align="center">
  <img src="icon/章鱼哥胆机icon.png" alt="ADD A VALVE 图标" width="180">
</p>

一款面向 Windows 的单轨音频染色与轻量编辑工具。通过谐波染色、动态响应、4 倍过采样和干湿混合，为音频增加可调的电子管风格色彩。

> “胆味”是主观音色描述。本项目采用算法化谐波与动态处理，并不宣称精确复刻某款真实电子管电路。

## 下载

前往 [Releases](https://github.com/Zhou1019-1/sound-effect-ADD-A-VALVE/releases/latest) 下载 `ADD-A-VALVE-0.1.0-Windows-x64.zip`。

1. 完整解压 ZIP。
2. 运行 `ADD A VALVE.exe`。
3. 保持 `lame.exe` 与主程序在同一目录，否则无法导出 MP3。

当前版本为 **v0.1.0**，支持 Windows 10/11 x64，无需安装。

## 功能

- WAV、FLAC、MP3 单声道/立体声导入。
- 波形缩放、选区、播放定位和实时参数试听。
- 驱动、特性、温暖、混合、输出五项音色控制。
- 处理/旁通 A/B 对比与内置预设。
- 裁剪、删除选区、淡入、淡出、增益、撤销和重做。
- 动态 VU、L/R 峰值表与削波锁存。
- 后台导出、取消、进度显示、临时文件和导出回读校验。
- 不会覆盖导入的原始音频文件。

## 音频格式

| 格式 | 导入 | 导出 |
| --- | --- | --- |
| WAV | 支持 | 24-bit PCM、32-bit float |
| FLAC | 支持 | 24-bit 无损 |
| MP3 | 支持 | 128 / 192 / 256 / 320 kbps CBR |

MP3 是有损格式。MP3 导出保持原采样率，并支持标准 MP3 采样率；不支持的采样率会明确提示，可改用 WAV 或 FLAC。MP3 编码器延迟及帧尾填充属于格式特性，导出回读会检查采样率、声道和编码帧长度。

## 使用流程

1. 点击“导入音频”，选择 WAV、FLAC 或 MP3。
2. 拖动波形创建选区，滚轮缩放，双击全选。
3. 调整音色参数，使用“试听：处理/旁通”对比。
4. 按需进行裁剪、淡入淡出或增益编辑。
5. 选择导出格式，点击“导出”。

## 从源码构建

### 环境

- Windows 10/11 x64
- CMake 3.22 或更新版本
- Visual Studio 2019/2022，安装“使用 C++ 的桌面开发”工作负载
- Git

首次配置会从 GitHub 获取固定版本的 JUCE 8.0.8；Windows 构建会编译随包发布的 LAME 3.100.1。无需预先安装 JUCE、LAME 或 FFmpeg。

建议把源码放在纯 ASCII 路径中。JUCE 8.0.8 的 `juceaide` 在部分中文路径下无法正常工作；如必须保留中文路径，可先使用 `subst` 映射一个 ASCII 盘符，再从映射路径配置和构建。

### 命令

```powershell
git clone https://github.com/Zhou1019-1/sound-effect-ADD-A-VALVE.git
cd sound-effect-ADD-A-VALVE
cmake -S . -B build -A x64
cmake --build build --config Release
```

主程序输出到 `build/AddAValve_artefacts/Release/ADD A VALVE.exe`，同目录会包含 MP3 导出所需的 `lame.exe`。

### 测试与打包

```powershell
ctest --test-dir build -C Release --output-on-failure
cpack --config build/CPackConfig.cmake -C Release
```

测试覆盖 DSP 谐波、混叠与延迟，音频编辑，预设，仪表，以及 WAV/FLAC/MP3 导出回读。MP3 测试覆盖全部四种码率，并在补偿编码器延迟后检查解码误差。

## 项目结构

```text
src/dsp/     电子管风格 DSP 与参数
src/audio/   播放、仪表和多格式导出
src/model/   音频文档、编辑历史与预设
src/ui/      波形、控制面板和动态仪表
src/app/     应用入口与主界面
tests/       JUCE UnitTest 自动化测试
cmake/       固定版本的第三方依赖配置
icon/        PNG 原图与多尺寸 Windows ICO
```

## 第三方组件与许可

- **JUCE 8.0.8**：JUCE 8 End User Licence Agreement 或 GNU AGPLv3。
- **LAME 3.100.1**：GNU LGPL v2 或更新版本，作为独立的 `lame.exe` 进程用于 MP3 编码。

本项目以 **GNU Affero General Public License v3.0** 发布，详见 [LICENSE](LICENSE)。固定依赖版本与第三方许可见 [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)，Windows 发布包同时附带 LAME 许可文件。
