# socket-shell-server

## 项目简介
socket-shell-server 是一个基于 UNIX 域套接字的远程 shell 命令执行服务. 它允许客户端通过本地 socket 发送 shell 命令, 由服务端接收并执行命令后返回执行结果. 项目主要使用 C 语言实现, 包含简单的 Shell 脚本用于编译. 

## 功能说明
- 在本地创建一个 UNIX 域套接字（默认路径 `/tmp/command_socket`, 可通过环境变量 `TMPDIR` 定制）. 
- 支持多客户端并发连接, 每个连接由子进程处理, 互不干扰. 
- 客户端可通过 socket 发送 shell 命令, 服务端会执行并返回状态. 
- 支持信号处理与子进程回收, 保证服务稳定运行. 
- 命令执行采用 `system()`, 可执行任意 shell 命令. 

## 依赖环境
- POSIX 兼容的 UNIX 或类 UNIX 系统
- C 语言编译器（gcc/clang）
- Shell 环境（bash/sh）

## 安装与使用方法

### 编译
确保环境已安装 C 编译器 (如 GCC 或 Clang, 默认跟随环境变量 `CC` 或使用 `cc`), 可直接运行 `build.sh` 脚本自动完成编译和 strip 操作:

```sh
sh build.sh
```
编译后会生成 `sss` 可执行文件. 

### 运行服务端
直接运行编译好的程序:

```sh
./sss
```
启动后会监听 UNIX 域 socket, 等待客户端连接. 

### 客户端连接与命令执行
你可以使用 `nc`（netcat）工具进行连接和测试:

```sh
echo "date" | nc -U /tmp/command_socket
```
或多行命令:

```text
$ nc -U /tmp/command_socket
ls -l /
whoami
^D
```

### 示例
服务端启动后, 客户端发送命令 `"date"`, 服务端执行并输出:
```sh
echo "date" | nc -U /tmp/command_socket
```
输出类似:
```text
Executing command: date
Tue Jul 29 11:21:35 CST 2025
Command exited with status: 0
```

## 注意事项
- 服务端会 fork 子进程处理每个连接, 资源消耗随连接数增加. 
- 任何收到的命令都会作为 shell 执行, 请确保 socket 权限受控, 避免安全风险. 
- 关闭服务时建议使用 SIGINT/SIGTERM 等信号, 以便自动清理 socket 文件. 

## 贡献指南
欢迎提出 Issue 或 PR 修复 bug、补充文档或添加新功能. 请确保所有提交遵循 GPL v3 许可协议. 代码请保持简洁易读, 建议多用注释说明关键逻辑. 

## 文件结构

- `sss.c`: 服务端主程序, 实现 socket 通信与命令执行逻辑. 
- `build.sh`: 编译脚本, 自动生成可执行文件并 strip. 
- `.gitignore`: 忽略编译生成的 `sss` 文件. 
- `LICENSE`: GNU GPL v3 许可证文本. 

## 许可证信息
本项目采用 [GNU GPL v3](LICENSE) 开源协议. 你可以自由使用、修改和分发本项目, 但须保证所有修改与分发也遵循相同协议. 
