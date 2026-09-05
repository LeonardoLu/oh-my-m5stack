# 工作任务

根据任务内容，指派 teammates 执行，验收 teammates 结果。

## 任务内容

### esp32 bot ux

实现 grok bot 的 bot 可控动画组件。要求设计合理、功能充沛、性能优秀、个性化，可以被如下设备集成使用。

### m5stack stopwatch

- 集成 bot ux
- 根据设备特点增强 bot ux 的互动性
- 具备一定的手表功能，可以展示时间、电量
- 设置页

### m5stack core2

- 集成 bot ux
- 根据设备特点增强 bot ux 的互动性
- 模拟 codex 定制键盘
- 根据设备特点设计模拟的 codex 定制键盘的功能的交互
- 设置页

### 技术&架构

- 目录设计：
  - stopwatch
    - bot-ux-watch
  - core2
    - bot-ux-codex-core2
  - bot-ux
- 多级目录，添加 AGENTS.md
- tmp：临时目录

## teammates 列表

> 根据如下要求和定义创建 teammates

- researcher：检索信息，汇总资料。
- ux professor：擅长用户交互、体验设计。
- esp32 professor：esp32 开发专家，深度了解 esp32、m5stack 技术栈，负责技术方案和代码的 review 、工程架构设计。
- m5stack stopwatch developer：m5stack stopwatch 的开发者。
- m5stack core2 developer：m5stack core2 的开发者。
- m5stack stopwatch QA：m5stack stopwatch 的测试人员。
- m5stack core2 QA：m5stack core2 的测试人员。

## 执行建议

- 添加 .gitignore ，将临时文件放入 tmp 内，其他临时目录、产物目录、依赖目录记录到 .gitignore 中。
- 使用 subagent 和 teammates 并行执行。
- 不过度设计边界场景。
- 优先执行开发和 code review，对于需要人工参与和测试环境的验收先跳过。
- 可自行安装依赖和必要的工具。
- 允许自主决策。
- 按需合理 commit 和切出 worktree 。
  