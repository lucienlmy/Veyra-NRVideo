# Veyra

<p align="center"><img src="assets/veyra-logo.png" alt="Veyra" width="200"></p>

English | [简体中文](README.md)

<p align="center">
  <a href="https://github.com/Likely7/Veyra-NRVideo/blob/main/REAMDE%20MP4.mp4">
    <img src="assets/readme-demo.gif" alt="Veyra demo video" width="960">
  </a>
</p>

A Windows video player and capture-card enhancement tool. Play videos, view images, preview capture
devices and stream from a PS5 on your LAN, with optional super resolution, NR enhancement and frame generation.

[Download 1.4.4 portable](https://github.com/Likely7/Veyra-NRVideo/releases/tag/v1.4.4) · [Full changes and known issues](https://github.com/Likely7/Veyra-NRVideo/releases/tag/v1.4.4) · [Report an issue](https://github.com/Likely7/Veyra-NRVideo/issues)

## 1.4.4 update

- **Frame-generation flicker**: all three independent causes are fixed. File-playback frame skips, repeated presents and capture drops no longer wipe the NR / frame-generation temporal history (which forced a provider warm-up every time).
- **Frame pacing redesigned**: the three mode selectors that measured identically are gone, replaced by three independent controls (low-latency queue, display sync, output cap). The **output cap now decides how many frames are actually generated** instead of dropping finished ones (native DLSS 6X: generated frames −71%, power 221→174 W; the true-upscale tier only drops about 4 W).
- **New strict FG-cadence switch** (default off = the 1.4.0 admission rule), together with a fix for the row that used to overlap its neighbours.
- **Capture**: fixed the format mix-up behind "selected 1080p YUY2 but opened 4K RGB24"; added input color-space / range selection and an Elgato 4K60 Pro MK.2 HDR metadata path.
- **UI**: fixed white ghosting and duplicated rows when scrolling the professional panel quickly; re-laid out the enhancement and frame-generation pages; subtitles no longer shrink just because they wrap to two lines; fullscreen can be locked.
- The complete fix list, measured data and **known limits** are in the [1.4.4 release notes](https://github.com/Likely7/Veyra-NRVideo/releases/tag/v1.4.4).

## Download and run

1. Download **Veyra-1.4.4-win64-portable.zip** from [Releases](https://github.com/Likely7/Veyra-NRVideo/releases) (do not download "Source code").
2. Extract the whole archive to a writable folder and run **Veyra.exe** (Windows 11 x64 / DirectX 12). No SDK, Python or developer tools required.
3. NR, super resolution and internal frame generation all start disabled; enable them after checking the base picture. NR / DLSS / RTX Video SR / NVENC need a compatible NVIDIA RTX GPU; this build was mainly validated on an RTX 5070.

Exit the old version and extract to a new folder when upgrading. To keep your settings, copy only `runtime_local/*.v1` and `veyra.ini` from the old folder; never overwrite the new runtime components with the old ones.

## Support and feedback

If this project helps you, you can buy the author a coffee (WeChat QR code). Join the group for bug reports and beta builds.

<p align="center">
  <img src="docs/images/1.4.0/donate-wechat.jpg" alt="WeChat donation" width="220">
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/images/1.4.4/community-group.png" alt="Veyra community group / bug reports / beta builds" width="220">
</p>
<p align="center"><small>Left: WeChat donation (optional)　Right: Veyra community group (bug reports and beta builds)</small></p>

## License and credits

Veyra's own source is [GPLv3](LICENSE); the combined build that includes remote play also carries the
[AGPLv3 with the upstream OpenSSL exception](licenses/remoteplay/CHIAKI_AGPL3_OPENSSL.txt). Build
instructions are in [docs/BUILD.md](docs/BUILD.md), the runtime component list in
[docs/RUNTIME_COMPONENTS_1.4.4.md](docs/RUNTIME_COMPONENTS_1.4.4.md), and third-party sources and
licenses in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Thanks to [Magpie Experimental](https://github.com/SAOG0721/Magpie/tree/experimental),
[chiaki-ng](https://github.com/streetpea/chiaki-ng) and
[XeSS-GPU-Motion](https://github.com/gggz114514-oss/XeSS-GPU-Motion).

NR and DLSS frame generation are **community experimental integrations**. They are not NVIDIA
certified, officially supported, or equivalent to native in-game integration.
