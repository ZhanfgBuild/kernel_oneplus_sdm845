# OnePlus 6 (enchilada) 4500mAh 电池：第二阶段 / Dash 大电流兼容性

状态：**只读诊断阶段。禁止作为已验证大电流刷机补丁发布。**

## 已确认的基线

- 第一阶段代码：PR #27，`feature/op6-4500mah-battery`，源码提交 `4025907070ac9702659094420a97d35b683ea734`。
- 本分支已将 FG Gen3 标称设计容量设为 4500mAh，同时保留原电池 profile 与充电硬件限制，外部 BQ 电量计 FCC 不会因为这一设置自动变成 4500mAh。
- 原机 Dash 原厂规格 **5V / 4A（20W 适配器侧输入）**；这不是电池两端的充电电流规格，不能把 `battery/current_now` 与 `usb/current_max` 直接比较。
- 既有只读探针检测到 `dashd`、`/dev/dash`、`oneplus_fastchg` 和 MCU 固件初始化路径，但这**不能证明快充握手已经持续成功，也不能证明更换电芯能承受原厂 Dash 电流**。
- 原内核 PR 的 Standard 与 PowerSave 包曾成功构建，仅 Build Verified，尚未获得本阶段真机充电/温控验证。
- 另一路正在日用的 Android 17 / BakaSU Linux 4.19.325 内核（`Zhanfg/android_kernel_oneplus_sdm845`）**不能直接视为与旧 ABK 构建包可互刷或 ABI 一致**。两条开发线必须单独移植审查。

## 第二阶段最小准入

1. **无需常驻服务的取样**：接入已验证兼容的原厂或正规 Dash 5V/4A 电源及专用线，记录 USB type、online、`current_now`、`current_max`、`input_current_limit`、电池 `current_now`、`temp`、`voltage_now` 和 SOC。保留原始 uA/uV/0.1°C，不直接推断实际 4A。
2. **确认真实电量计型号**：现有驱动有 BQ27411 与 BQ27541 两类寄存器布局。必须由内核设备识别日志或经验证的硬件探测确定；未确认前**严禁写 TI DataFlash**。
3. **确认替换电池硬件规格**：制造商电芯/保护板允许的充电电流、截止电压和热参数，以及替换电池 NTC、保护板、线束与触点适配。标称 4500mAh 不是连续高电流能力证明。
4. **对比场景**：原厂/兼容 Dash、普通 5V 适配器；屏幕亮/灭，SOC 20–40%、50–70%、80–95% 对比；记录室温、电芯温度、充电速度、电流跌落、充电故障与断续充。异常发热、鼓包、异味或连接器过热即终止测试。
5. **回滚**：内核镜像 / boot.img 及原电池容量设定可回退；决不修改 thermal、JEITA、OVP、OCP、battery health 上报值来绕过硬件保护。

## 保守的实现策略

- **P0（当前）**：只读采样 + 日志比对；不写 sysfs/proc，不运行 daemon，不改 charging vote，不加电压 / 不提高限流上限。
- **P1（经实测后）**：仅针对证据证明过度保守且仍在供应商允许范围内的场景，设计受控且可撤销的 charging vote 调整；默认禁用，断线/温升/握手失败必须退回原生策略；每次改动独立内核 PR 与真机测试。
- **P2（单独项目）**：确认 BQ 芯片型别和匹配的 DataFlash 布局后，再讨论可回退的 4500mAh design-capacity 及学习周期标定；这不属于 Dash 提速手段。

辅助诊断工具：`OP6_Battery_Dash_Audit_v2.sh`（单独发行的只读独立脚本 / ReSukiSU 模块）；诊断工具无法取代上述硬件验证。

**验收门槛**：必须先通过工具链/构建验证，再在与当前用户 Android17 ROM 精确匹配的内核上完成启动、休眠、USB、热管理、SOC、Dash 和电芯温升的连续实测。未达到这一级别不能标注 Stable。
