# Qt 麒麟系统密码解锁示例

本项目用于验证 Qt 应用的启动解锁流程。应用启动时先锁定界面，用户输入当前麒麟本地账户的系统密码并通过验证后，才能进入主界面。密码由 Linux PAM 校验，应用无需创建或保存自己的密码。

**当前进度**　已实现 Linux 上的启动解锁窗口、当前用户 PAM 验证和重试间隔，并提供独立 PAM 服务配置。尚未在目标麒麟机器上实测。

## 构建与运行

需要 CMake 3.16 或以上版本、支持 C++17 的编译器，以及 Qt 5 或 Qt 6 的 Widgets 和 Concurrent 开发组件。Linux 构建还需要提供 `security/pam_appl.h` 和 `libpam` 的开发包。CMake 配置时会检查这些依赖。[Qt 5 与 Qt 6 的 CMake 用法](https://doc.qt.io/qt-6.5/cmake-qt5-and-qt6-compatibility.html)

在仓库根目录运行以下命令。

```sh
cmake -S . -B build
cmake --build build
```

如果 CMake 找不到 Qt，配置时传入 `-DCMAKE_PREFIX_PATH=<Qt 安装目录>`。部署下文的 PAM 服务后，在目标麒麟桌面上以普通用户运行 `./build/qt-kylin-pam-demo`。

## 启动与解锁

1. 由普通桌面用户启动应用。通过 `getuid()` 和 `getpwuid_r()` 获取启动进程的实际用户，解锁窗口只要求输入该用户的密码。
2. 启动时仅显示解锁窗口。在认证通过前，主窗口、菜单、快捷键和托盘等业务操作入口保持关闭。
3. 用户提交后，应用清空输入框并在后台线程调用 PAM。一次只处理一个认证请求，空输入直接拒绝。密码只用于本次认证，密码不写入日志、文件或命令行参数。
4. 密码和账户状态均通过检查后显示主界面。认证失败后等待 3 秒方可重试，应用始终保持锁定并显示相应提示。取消或关闭解锁窗口时放弃解锁，若认证仍在进行，程序等待认证结束后退出。

首版面向从银河麒麟桌面直接启动应用的本地普通用户。经 `sudo` 或系统服务启动时，进程用户可能与桌面登录用户不同。域账号和集中认证环境需要另行验证。

## PAM 配置

应用使用专用服务名 `qt-kylin-pam-demo`。在目标测试机上从仓库根目录运行以下命令部署样例。

```sh
sudo install -o root -g root -m 0644 pam.d/qt-kylin-pam-demo /etc/pam.d/qt-kylin-pam-demo
```

样例包含以下规则。

```pam
auth    required    pam_unix.so
account required    pam_unix.so
```

应用使用 `pam_start()` 指定服务和账户，通过 PAM 会话回调提交密码，依次执行 `pam_authenticate()` 和 `pam_acct_mgmt()`，最后调用 `pam_end()`。只有两项检查都成功才解锁。服务文件缺失或无法读取时，应用应直接报告配置错误并拒绝解锁，防止 PAM 走默认的 `other` 服务。[Linux-PAM 应用开发指南](https://github.com/linux-pam/linux-pam/blob/master/doc/adg/Linux-PAM_ADG.xml) · [PAM 服务配置语法](https://github.com/linux-pam/linux-pam/blob/master/doc/man/pam.conf-syntax.xml) · [`pam_unix` 文档](https://github.com/linux-pam/linux-pam/blob/master/modules/pam_unix/pam_unix.8.xml)

### 输错密码会锁定系统账户吗？

如果应用的 PAM 配置通过 `common-auth` 等系统配置引入 `pam_faillock`，并且与桌面登录共用失败记录目录，应用内的错误尝试可能累计到系统登录计数，达到阈值后影响系统登录。独立服务名本身无法隔离计数。[`faillock.conf` 文档](https://github.com/linux-pam/linux-pam/blob/master/modules/pam_faillock/faillock.conf.5.xml)

上述测试样例直接调用 `pam_unix`，应用在当前进程内限制解锁重试，重启应用会重新计时。目标是让应用内的输错只影响应用解锁，认证失败仍可能留下系统认证日志。实际行为以目标机加载的 PAM 配置和登录测试为准。

## 实施与验收

1. Qt Widgets 和 CMake 工程已建立。待在目标机确认 Qt 开发环境、C++ 编译器及 Linux-PAM 开发库。
2. 当前用户识别、PAM 调用、解锁窗口和重试限制已实现，仓库包含独立的 PAM 服务配置样例。
3. 在目标麒麟测试机上使用本地普通测试账户验证以下场景。

| 场景 | 预期结果 |
| --- | --- |
| 启动后未输入密码 | 仅显示解锁窗口，业务操作入口不可用。 |
| 正确的系统密码 | 认证通过并显示主界面。 |
| 错误密码、空输入 | 保持锁定，空输入不发起 PAM 请求。 |
| 取消或关闭解锁窗口 | 程序退出，主界面未显示。 |
| 输错后立即重试 | 应用等待 3 秒后重新开放提交。核对可用的系统失败计数，并确认桌面仍可用正确密码登录。 |
| 修改系统密码 | 重新打开应用后，旧密码验证失败，新密码可以解锁。 |
| PAM 服务文件缺失或不可读 | 提示配置错误，应用保持锁定。 |
