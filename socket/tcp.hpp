#pragma once

#include<socket.hpp>
#include<string_view>
#include<limits>
#include<utility>
#include<chrono>
#include<expected>
#include<nettool.hpp>
#include<variant>


namespace shape
{

namespace tcp
{
using exstring::Explicit_string;
class In_connecting;
class Socket_handle:public RAII_unique_fd
{
public:
    friend class Listener;
    friend class In_connecting;
    Socket_handle()noexcept:RAII_unique_fd{-1}{}
    Socket_handle(const Socket_handle&)=delete;
    Socket_handle& operator=(const Socket_handle&)=delete;
    Socket_handle(Socket_handle&&)noexcept=default;
    Socket_handle& operator=(Socket_handle&&)noexcept=default;

    std::expected<std::size_t,Socket_error> read(char* ,std::size_t,std::chrono::milliseconds,IO_strategy)noexcept;

    std::expected<std::size_t,Socket_error> write(const char*,std::size_t,std::chrono::milliseconds,IO_strategy)noexcept;

    //真正的close永远只会交给RAII_fd的析构，这里只shutdown
    void shut_read()noexcept;
    void shut_write()noexcept;

private:
    explicit Socket_handle(int fd)noexcept:RAII_unique_fd{fd}{}
};

class In_connecting:public RAII_unique_fd
{
    friend std::expected<In_connecting,Socket_error> start_connect(std::string_view ip,std::uint16_t)noexcept;
    friend std::expected<In_connecting,Socket_error> start_connect(const addrinfo*)noexcept;
    explicit In_connecting(int fd,bool is_ok)noexcept:RAII_unique_fd{fd},already{is_ok}{}
    bool already{false};
public:
    In_connecting()noexcept:RAII_unique_fd{-1}{}
    In_connecting(const In_connecting&)=delete;
    In_connecting& operator=(const In_connecting&)=delete;
    In_connecting(In_connecting&&)noexcept=default;
    In_connecting& operator=(In_connecting&&)noexcept=default;
    std::expected<Socket_handle,Socket_error> poll_wait(std::chrono::milliseconds)noexcept;
};
//本函数配合DNS_resolver,用于域名的连接
std::expected<In_connecting,Socket_error> start_connect(const addrinfo*)noexcept;
inline std::expected<In_connecting,Socket_error> start_connect(DNS_resolver::iterator it)noexcept{return start_connect(it.get_pointer());}
//本函数必须用 主机ip-服务端口作为参数，不提供DNS解析
std::expected<In_connecting,Socket_error> start_connect(std::string_view ip,std::uint16_t)noexcept;


class Listener:public RAII_unique_fd
{
    friend std::expected<Listener,Socket_error> listen(std::string_view,std::string_view,int)noexcept;
    friend std::expected<Listener,Socket_error> listen(std::string_view,int)noexcept;
public:
    Listener()noexcept:RAII_unique_fd{-1}{}
    Listener(const Listener&)=delete;
    Listener& operator=(const Listener&)=delete;
    Listener(Listener&&)noexcept=default;
    Listener& operator=(Listener&&)noexcept=default;

    Socket_error get_last_error()noexcept
    {
        auto result=self_errno;
        self_errno=Socket_error::NO_ERROR;
        return result;
    }
    std::pair<Socket_handle,Address_info> accept(std::chrono::milliseconds=std::numeric_limits<std::chrono::milliseconds>::max())noexcept;
private:
    Socket_error self_errno{Socket_error::NO_ERROR};
    explicit Listener(int fd)noexcept:RAII_unique_fd{fd}{}
};
std::expected<Listener,Socket_error> listen(std::string_view,int)noexcept;
std::expected<Listener,Socket_error> listen(std::string_view,std::string_view,int)noexcept;

}
}
//接下来准备一下必须修复的end_time上溢漏洞