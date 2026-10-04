# CW32L010 桌面墨水屏时钟 · v4 候选版

当前开发分支新增 **v4.1 抖动翻页主题**：双翻页卡片、颗粒灰阶、几何像素数字和精确夜间反色，操作与实机验收说明见 [FLIP-CLOCK.md](docs/FLIP-CLOCK.md)。

开发分支 `codex/desktop-clock-v4`，基于 v3 提交 `091ce0f`。**尚未烧录或实机验收**。原始恢复镜像、主分支和历史烧录记录保持不变。

## 本版内容

三套纯黑白像素主题、中文日期和星期、一天进度、12/24 小时制、定时反色、芯温与专注倒计时。
屏幕/SPI/ADC 和运行时 RTC 保存采用分步状态机，按键独立采样；在新帧发送前合并最新操作。
屏幕黑白极性在 SPI 数据边界统一翻转，旧、新两帧一致；不改原机波形表。

操作、实现细节、温度限制与实机验收清单见 [v4 文档](docs/DESKTOP-V4.md)。
点阵来源和许可见 [字体说明](docs/FONT-LICENSE.md)。

## 硬件与保护

CW32L010F8U6，64 KiB Flash、4 KiB RAM；逻辑画布 296×152。
BUSY=PA1（低忙）、RST=PA2、CS=PA3、DC=PA4、SPI=PA5/PA6；K1=PB2、K2=PB3；SWD=PA7/PA8。
RTC 保留 v3 的外部 32768 Hz LSE。设置仍用 0xFA00/0xFC00 双页和 CRC；0xFE00 安全页不擦写。
芯温不是校准室温；专注计时无声音提醒，不跨重启保存。

## 构建、下载、烧录

```sh
make clean
make -j2
make test
python3 tools/verify_image.py
python3 -m unittest discover -s tests -p 'test_device.py'
python3 tools/preview_desktop.py
python3 tools/preview_flip.py
```

需要 ARM GCC、Python、Pillow 和 pyocd；首次构建需要网络获取固定版本点阵依赖，随后可缓存。
**下载与刷写只使用本分支最新成功的 Actions 产物和其中的 manifest.json，不使用仓库内历史 build/ 或 releases/ 文件。**
预览为固件同一份 C 渲染器生成，不是设备实拍。自动测试不能代替按键手感、屏幕残影、ADC 温度、功耗和烧录回读验收。

烧录沿用 `tools/device.py` 的芯片/镜像校验、实时备份、预期整片哈希检查、局部擦写和完整回读。必须先取得当前设备的真实备份哈希；不可照抄旧版本哈希、自动解锁或整片擦除。
历史 v3 的操作与恢复记录请查看主分支对应提交，不把历史安装状态当作本版已烧录。
