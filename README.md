# shape net(SocketHandleAndPollEr)

[![License](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)](https://en.cppreference.com/w/cpp/23)
[![linux](https://img.shields.io/badge/Platform-Linux-lightgrey)]()

这是一个针对linux平台的套接字API的薄封装，旨在简化使用socket时要手写的各种胶水

## 为什么用shape net
- 如名字所言——这个库只包含RAII的套接字句柄与轮询器，而不提供任何调度模型，因而它有着很强的可塑性。如果你对并发模型和发送格式有强烈的定制需求，又不想仍受裸socket的复杂性，可以尝试shape net
- 库作者强烈建议将shape net与[Asynckernel](https://github.com/zhaorker/AsyncKernel)库一起使用。因为shape net设计的初衷就是为AsyncKernel提供一个同样自由的网络io库，让你可以完全掌控网络io和并发模型

## 功能简介
> 原计划提供以下接口，但是因为作者高三，不得不为学业冲刺了，这个库目前只完成了tcp的部分，直到28年高考结束前我都不会再维护这个项目
- 对应用层友好的套接字错误码映射
- 易用的DNS解析工具——DNS_resolver。
- 将sockaddr与`string ip,uint16_t port`互相转化的工具
- RAII的tcp套接字`shape::tcp::Socket_handle`类，含有带超时和读写策略的io接口
- 用于非阻塞connect的`start_connect`函数与`In_connecting`类，可以询问In_connecting类获得连接好的Socket_handle
- RAII的tcp监听套接字`shape::tcp::Listener`类，由`shape::tcp::listen`函数创建

- RAII的udp套接字`shape::udp::Socket_handle`，可选支持广播，它们都用Socket_handle表示，由内部数据策略驱动
- RAII的udp bind套接字`shape::udp::Binded_socket`，可选支持广播，由`shape::udp::bind`函数创建

- 应用层零拷贝视图型缓冲区Buffer，考虑到io收发缓冲区本身的用途，Buffer是SCSP的
- 用epoll封装成的Epoller,以及用io_uring封装成的uring_Poller，区别就是前者只提供就绪通知，后者在读完成时会返回一段视图，这意味着内部会为每个套接字绑定一个Buffer,而使用Epoller则需要应用层自己使用Buffer

## 环境要求
- **linux平台上编译**
  - 因为错误码映射和系统api行为依赖都是按linux的标准来写的
- **C++23 兼容编译器**：
  - GCC 11+（推荐 13+）
  - Clang 14+（推荐 17+）
  - MSVC 2022 17.5+
- **CMake 3.20+**（推荐 3.25+）

## 许可证
本项目采用 GNU General Public License v3.0 授权。详见 LICENSE 文件。

## 将来计划
1. 目前tcp的接口里还存在std::chrono::time_point上溢的逻辑漏洞，虽然比较难触发，但迟早要修
1. udp套接字类的补全
1. Buffer类实现
1. 另设转为uringAPI的应用层错误码映射
1. Poller类的实现
1. 与AsyncKernel集成成一个相对开箱即用的网络框架

## 快速开始
可以直接见项目根目录下的[echoserver](server.cpp)和[echoclient](client.cpp)，服务端并没有做并发设计
关于DNS解析的接口用法见[dns_test](test.cpp)