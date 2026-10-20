# WhiteClock 工程指南

项目介绍、七套主题、操作表和容量汇总见[仓库首页](../README.md)。本目录保留芯片型号命名，EIDE 工作区为 `WhiteClock.code-workspace`。

## 当前版本

WhiteClock V1.0 已安装并完成 64 KiB 整片回读，设置和安全区域保留。用户确认显示正常、切换明显变快，BACK 按住即可返回。

| 项目 | 记录 |
|---|---|
| 产物目录 | `releases/whiteclock-v1-fast-20261020/` |
| BIN | 61,252 字节 |
| BIN SHA-256 | `4168abefc0c78b3616b113c409ae1da52b4ca56960b43bac1fb335b787d578a5` |
| 完整回读 SHA-256 | `2c71c13673301bdc95b55560055d5e0757a1d5aba0a17c9fd7c2b138ce08120d` |
| 静态 RAM / 栈顶余量 | 2,572 / 1,524 字节 |
| 验证时间 | 2026-10-20 17:02（Asia/Shanghai） |

完整回读哈希只代表当时整片快照；显示设置写入后会变化，**不能直接复制为下一次烧录的当前哈希**。

## 构建

安装 ARM GNU Toolchain、Make、原生 C 编译器和 Python 3.12 后，在本目录运行：

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
make -j4
python tools/verify_image.py
```

输出位于 `build/clock/`：`clock.bin`、`clock.hex`、`clock.elf`、`clock.map` 和 `manifest.json`。镜像检查会核对原始备份哈希、向量、应用边界、未解析符号及至少 1 KiB 栈空间。检查新构建不会将它标记为已经实机验收。

首次需要联网获取 GNU Unifont 16.0.04，下载内容由 SHA-256 校验。缓存位于 `build/font-cache/`；界面字形仅按整数像素生成。主题资源头文件已提交，普通编译不需要重画主题；需要重生成时运行：

```sh
make theme-assets
python -m unittest discover -s tests -p 'test_theme_assets.py'
```

资源生成依赖 Pillow / Matplotlib，版本固定在 `requirements-dev.txt`；生成的原图参考与元数据位于 `build/theme-assets/`。它们供独立逐像素测试使用，不作为设备运行时资源。

默认构建与实机确认配置一致：

```c
EPD_USE_FAST_LUT=1
EPD_USE_PARTIAL_WINDOW=0
EPD_FAST_BORDER_DATA=0x47
EPD_FULL_BORDER_DATA=0x47
```

不要重新启用裁剪窗口或换用相似面板的未知 LUT。当前固件保留原机 56 字节波形表、完整 OLD/NEW 双帧、黑白传输极性与周期全刷策略。

## 测试与预览

```sh
make -j4 test HOST_CC='cc -O2'
python -m unittest discover -s tests -p 'test_device.py'
make theme-assets
python -m unittest discover -s tests -p 'test_theme_assets.py'
python tools/preview_desktop.py --output-dir build/preview
```

测试包括 ASan / UBSan、日期与 RTC、设置掉电恢复、SPI 背压及超时、按键物理映射与长按边界、菜单文字和圆点、3528 组场景变化、141120 组主题布局、70 组单次 SET 跳页、番茄钟提醒以及 14 帧画面散列和解压工作量防退化。

预览来自与固件相同的 C 渲染器，`themes.png`、`theme-selection.png` 和 `menus.png` 为整数倍像素预览，**不是设备实拍**。源码快照与自动构建产物不包含字体下载缓存或本机临时文件。

## 安全烧录

只对 CW32L010F8U6 使用本工具。保持供电，确认 SWD 接线与电平正确。默认探针序列号对应开发设备，其他烧录器使用 `--probe` 指定。

`tools/device.py` 默认只读。针对新构建，先执行：

```sh
python tools/device.py --frequency 5000
```

检查生成的 `../evidence/device_*.json` 和 `before_*.bin`，确认实时整片哈希与预期固件一致，再执行：

```sh
python tools/device.py --flash --frequency 5000 \
  --expected-before-sha256 <本次读取的完整64KiB哈希>
```

只读检查、备份和写入使用默认 `build/clock/` 的镜像与清单。要烧录已验收的发布文件，必须同时指定匹配的目录，不能用旧 `build/` 冒充正式版本：

```sh
python -c 'from pathlib import Path; import sys; sys.path.insert(0,"tools"); import device; device.BUILD=Path("releases/whiteclock-v1-fast-20261020").resolve(); device.main()' --frequency 5000
```

确认新的整片读取结果后，使用同一命令追加 `--flash --expected-before-sha256 <实时哈希>`。`--restore` 仅用于明确需要恢复已校验原厂固件的情况，不属于正常升级流程。

工具禁止自动解锁、整片擦除，按 512 字节页写入并保留页尾和未写区域，最后核对整个 64 KiB；设置页位于 `0xFA00` / `0xFC00`，`0xFE00` 安全页不擦写。

如果在**写入开始后**连接断开，不要宣称原固件完好或反复复位运行：应按本次写入前备份和预期页面恢复校验，必要时补写，完整一致后才启动。V1.0 曾在 USB 中断后恢复整片校验，无需重写；记录见发布目录的 `NOTES.md`。

仅查看运行状态、不复位或擦写：

```sh
python tools/monitor_refresh.py --build releases/whiteclock-v1-fast-20261020 --seconds 60
```

监测工具先核对实机应用与本地镜像一致，再读取相应地址；不能用其他版本的调试地址推断当前固件状态。

## 实现与限制

- BACK PB2 / SET PB3。GPIO 双边沿中断唤醒采样，1 ms SysTick 对活动按键进行 20 ms 消抖；BACK 在按住满约 700 ms 时触发一次，不等松手。
- 主循环优先处理按键，5 ms 合并更新。NEW 帧发送前可替换待显示状态，开始发送后冻结快照，避免混帧。
- 屏幕 SPI 没有可用的 DMA 路径，使用有界短突发轮询；不调用 SDK 中误写模式位的 DMA 接口。
- 字体与背景压缩解码分别缓存，实际上传按列取位；不增加整屏 RAM 缓冲。
- 番茄钟到时发起五次完整波形。任意按键取消后续提醒，已开始的刷新正常结束；编辑设置不会丢失草稿。
- 内部温度不是校准室温，番茄钟不跨复位保存；物理刷新、长期残影、功耗和温度适应性仍受屏幕与硬件限制。

更多资料：[实现与验收](docs/DESKTOP-V4.md)、[屏幕驱动研究](docs/EPD-DRIVER-RESEARCH.md)、[原厂协议分析](../cw32l010_epd_analysis.md)、[字体许可](docs/FONT-LICENSE.md)、[审查记录](REVIEW.md)。
