# WhiteClock 工程指南

[项目首页](../README.md) · [烧录教程](docs/FLASHING.md) · [AI 烧录交接](docs/AI-FLASHING-HANDOFF.md)

## 工程配置

目标为 CW32L010F8U6，64 KiB Flash、4 KiB RAM；横向界面 296 × 152。工程目录保留芯片型号命名，EIDE 工作区为 `WhiteClock.code-workspace`。

| V1.0 发布镜像 | 配置 |
|---|---|
| 目录 | `releases/whiteclock-v1-fast-20261020/` |
| BIN 大小 | 61,252 字节 |
| BIN SHA-256 | `4168abefc0c78b3616b113c409ae1da52b4ca56960b43bac1fb335b787d578a5` |
| 应用上限 / 剩余 | 64,000 / 2,748 字节 |
| 静态 RAM / 栈顶余量 | 2,572 / 1,524 字节 |
| 设置页 / 安全页 | `0xFA00`、`0xFC00` / `0xFE00` |

以上为随版本提供的镜像数据。工具链变化可能改变重新编译的镜像大小和哈希；烧录时应使用目标产物自身的清单。

## 构建

需要 ARM GNU Toolchain、Make、原生 C 编译器和 Python 3.12。在本目录运行：

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
make -j4
python tools/verify_image.py
```

输出在 `build/clock/`：BIN、HEX、ELF、MAP 和 `manifest.json`。检查项包括原始参考备份哈希、向量、应用边界、未解析符号和至少 1 KiB 栈预算。编译通过不替代设备回读与显示检查。

首次构建下载固定 SHA-256 的 GNU Unifont 16.0.04，缓存于 `build/font-cache/`。界面使用原生点阵；主题位图已固化在 `user/inc/theme_assets.h`，普通构建不重新生成主题。

默认面板配置：

```c
EPD_USE_FAST_LUT=1
EPD_USE_PARTIAL_WINDOW=0
EPD_FAST_BORDER_DATA=0x47
EPD_FULL_BORDER_DATA=0x47
```

## 测试与预览

```sh
make -j4 test HOST_CC='cc -O2'
python -m unittest discover -s tests -p 'test_*.py'
python tools/preview_desktop.py --output-dir build/preview
```

测试覆盖 ASan / UBSan、RTC、掉电设置恢复、物理按键映射、SPI 和 EPD 错误路径、菜单、主题布局、番茄钟提醒和参考画面散列，详见[验证说明](REVIEW.md)。预览与设备共用 C 渲染器，不是设备实拍。

`tests/fixtures/theme-assets/` 保存独立像素参考和字形元数据。CI 对源码内固定主题资源运行解码逐像素检查和 14 帧固定散列检查，不以本机重新栅格化的结果替换参考。

## 修改主题资源

```sh
make theme-assets
make test HOST_CC='cc -O2'
```

此命令显式覆盖 `user/inc/theme_assets.h`，并在 `build/theme-assets/` 生成候选参考图。Pillow 的 FreeType 与文本布局组件会随平台不同，**仅固定 Python 包版本不保证字形逐像素一致**。资源变更需要检查候选图、与固定参考比较，并在明确接受设计变化后一起更新参考、散列和发布镜像；不要为消除测试失败直接覆盖预期值。

## 调试与烧录

完整流程见[烧录教程](docs/FLASHING.md)。`tools/device.py` 默认只读，正式写入必须使用本台设备即时读取的 64 KiB 哈希。禁止整片擦除、自动解除保护、跨设备恢复参考备份。

读取运行指标时指定匹配镜像目录：

```sh
python tools/monitor_refresh.py --build releases/whiteclock-v1-fast-20261020 --seconds 60
```

工具先核对设备应用与本地镜像，再读取调试地址。`epd_metrics` 包含准备、OLD、NEW、BUSY、收尾、总耗时及渲染工作量；启动计时、主机渲染耗时均不能当作按键到画面的响应延迟。

## 技术资料

- [架构与硬件](docs/DESKTOP-V4.md)
- [屏幕驱动分析](docs/EPD-DRIVER-RESEARCH.md)
- [原厂协议分析](../cw32l010_epd_analysis.md)
- [字体许可](docs/FONT-LICENSE.md)

芯片温度仅为近似值；番茄钟不跨复位保存。长期残影、低温表现、功耗与硬件栈水位没有完整量测数据。
