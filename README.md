# Veyra

<p align="center"><img src="assets/veyra-logo.png" alt="Veyra" width="200"></p>

[English](README_EN.md) | 简体中文

<p align="center">
  <a href="https://github.com/Likely7/Veyra-NRVideo/blob/main/REAMDE%20MP4.mp4">
    <img src="assets/readme-demo.gif" alt="Veyra 演示视频" width="960">
  </a>
</p>

Windows 视频播放器与采集卡增强工具。支持视频、图片、采集卡实时预览和 PS5 局域网串流，可组合使用超分辨率、NR 画面增强与补帧。

[下载 1.4.4 免安装版](https://github.com/Likely7/Veyra-NRVideo/releases/tag/v1.4.4) · [完整更新与已知问题](https://github.com/Likely7/Veyra-NRVideo/releases/tag/v1.4.4) · [反馈问题](https://github.com/Likely7/Veyra-NRVideo/issues)

## 1.4.4 更新

- **补帧闪烁**：三个独立成因都已修掉——文件播放跳帧、重复呈现同一帧、采集丢帧，都不再清空 NR/补帧时域历史（此前每次都要重新预热）。
- **帧同步重做**：删掉实测无差异的三个模式选择器，改为低延迟队列／显示同步／输出上限三项独立控件；**输出上限现在决定补帧实际生成多少**，不再是算完再丢（原生 DLSS 6X 实测生成帧 −71%、功耗 221→174 W；真超分档只降约 4 W）。
- **新增“严格补帧节奏”开关**（默认关闭＝1.4.0 的宽松规则），并修掉它与相邻控件重叠的排版问题。
- **采集卡**：修复“选 1080p YUY2 却打开 4K RGB24”的格式串档；新增输入色彩空间/范围选择与 Elgato 4K60 Pro MK.2 专用 HDR 元数据入口。
- **界面**：修复专业模式快速滚动的白影/重叠；增强页与补帧页重排；字幕不再因双行自动缩小；全屏可锁定。
- 完整修复清单、实测数据和**已知边界**见 [1.4.4 更新报告](https://github.com/Likely7/Veyra-NRVideo/releases/tag/v1.4.4)。

## 下载与运行

1. 在 [Releases](https://github.com/Likely7/Veyra-NRVideo/releases) 下载 **Veyra-1.4.4-win64-portable.zip**，不要下载 Source code。
2. 完整解压到一个可写文件夹，双击 **Veyra.exe**（Windows 11 x64 / DirectX 12）。无需安装 SDK、Python 或开发工具。
3. 首次启动 NR、超分和内部补帧均关闭，确认基础画面后按需开启。使用 NR / DLSS / RTX Video SR / NVENC 需要兼容的 NVIDIA RTX 显卡；本版主要在 RTX 5070 上验证。

升级请先退出旧版并解压到新目录。需要保留设置时，只复制旧目录下的 `runtime_local/*.v1` 与 `veyra.ini`；不要用旧目录整体覆盖新版运行组件。

## 支持与反馈

如果这个项目帮到了你，可以请作者喝杯咖啡（微信扫码）；有问题或想第一时间拿到 beta 版，欢迎进群反馈。

<p align="center">
  <img src="docs/images/1.4.0/donate-wechat.jpg" alt="微信赞助" width="220">
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/images/1.4.4/community-group.png" alt="Veyra 交流群 / bug 反馈 / beta 测试" width="220">
</p>
<p align="center"><small>左：微信赞助（自愿，不影响任何功能的可用性）　右：Veyra 交流群——bug 反馈与 beta 版本发布</small></p>

## 许可与来源

Veyra 源码采用 [GPLv3](LICENSE)；含串流的组合程序同时适用 [AGPLv3 与上游 OpenSSL 例外](licenses/remoteplay/CHIAKI_AGPL3_OPENSSL.txt)。构建说明见 [docs/BUILD.md](docs/BUILD.md)，运行组件清单见 [docs/RUNTIME_COMPONENTS_1.4.4.md](docs/RUNTIME_COMPONENTS_1.4.4.md)，第三方来源与许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

感谢 [Magpie Experimental](https://github.com/SAOG0721/Magpie/tree/experimental)、[chiaki-ng](https://github.com/streetpea/chiaki-ng) 与 [XeSS-GPU-Motion](https://github.com/gggz114514-oss/XeSS-GPU-Motion)。

NR 与 DLSS 帧生成属于 **community experimental / 社区实验集成**，不是 NVIDIA 官方认证、官方合作或完整游戏原生集成。
