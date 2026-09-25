# Third-party software

## DirectXTex

This repository contains DirectXTex headers in `include/DirectXTex*` and prebuilt
libraries in `lib/DirectXTex_*.lib`. DirectXTex is copyright Microsoft
Corporation and is distributed under the MIT License, as stated in the bundled
headers. The license text is retained at `licenses/DirectXTex-LICENSE.txt`.
When these binaries are upgraded, update headers and both Debug and Release
libraries together and record the upstream release or commit here.

The current bundled binary's exact upstream release was not recorded in the
original project history. Reproducible future upgrades should build or download
all three artifacts from one tagged DirectXTex release.

## Dear ImGui

`third_party/imgui/` contains the SP31 project's Dear ImGui v1.92.7 WIP copy,
including its DX11 and Win32 backends. ShaderGallery alone compiles these
sources. Its MIT license is retained at `third_party/imgui/LICENSE.txt`.
