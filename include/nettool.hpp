#pragma once
//nettool.hpp
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<sys/socket.h>
#include<poll.h>
#include<cctype>
#include"err_map.hpp"
#include"exstring.hpp"
#include<expected>
#include<memory>
#include<cstring>
#include<chrono>
#include<fcntl.h>
#include<utility>
#include<variant>
#include<cassert>

namespace shape
{
// 快速设置非阻塞套接字
//调用契约：必须是非-1的描述符,而无效描述符导致的错误通过系统api和errno反映
inline bool set_nonblock(int fd)noexcept
{
    assert(fd!=-1);
    int _fd_flag=::fcntl(fd,F_GETFL,0);
    if(_fd_flag==-1)
        return false;
    if(::fcntl(fd,F_SETFL,_fd_flag|O_NONBLOCK)==-1)
        return false;
    return true;
}



//用于向字符串头写入/读取包长度信息的函数
using Netlong=decltype(ntohl(1));
using Netshort=decltype(htons(1));

inline Netlong read_head_flag(
    const char* pos
)noexcept
{
    assert(pos!=nullptr);
    Netlong buff=0;
    char * ptr=reinterpret_cast<char*>(&buff);
    for(int count = 0;count<sizeof(Netlong);++count)
        *(ptr++)=*(pos++);
    return ::ntohl(buff);
}
inline void write_head_flag(
    char *pos,
    Netlong value
)noexcept
{
    assert(pos!=nullptr);
    Netlong buff=::htonl(value);
    const char *ptr=reinterpret_cast<char*>(&buff);
    for(int count=0;count<sizeof(Netlong);++count)
        *(pos++)=*(ptr++);
}



/*用于简化单套接字poll的函数*/
[[deprecated("请使用时间更准，类型更安全的poll_chrono")]]
inline std::expected<bool,Socket_error> poll(int fd,short io/*直接传POLLIN/OUT*/,int timeout)noexcept
{
    if(fd==-1)
        return std::unexpected(Socket_error::INVALID_SOCKET);
    pollfd input{.fd=fd,.events=io};
    auto poll_time=std::chrono::steady_clock::now();
    while(true)
    {
        int ret=::poll(&input,1,timeout);
        if(ret<0)
        {
            if(errno==EINTR){
                if(timeout==-1)
                    continue;
                timeout-=static_cast<int>( std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()-poll_time
                ).count());
                if(timeout<=0)
                    return false;
                poll_time=std::chrono::steady_clock::now();
                continue;
            }
            return std::unexpected(error_map(errno));
        }else if(ret==0)
            return false;//timeout
        if(input.revents & POLLNVAL) return std::unexpected(Socket_error::INVALID_SOCKET);
        // if(input.revents & POLLERR) return std::unexpected(Socket_error::PEER_RESET);
        if(input.revents & POLLERR) {
            int soerr = 0;
            socklen_t len = sizeof(soerr);
            if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) == 0 && soerr != 0)
                return std::unexpected(error_map(soerr));
            return std::unexpected(Socket_error::UNKNOWN);
        }
        if(input.revents & io) return true;
        //先考虑读数据，再考虑发生连接断开
        if(input.revents & POLLHUP) return std::unexpected(Socket_error::PEER_CLOSED);

        return std::unexpected(Socket_error::UNKNOWN);
    }
}
/**
 *@brief 单套接字轮训函数
 *@param int套接字，short要等待的操作，直接使用系统的POLLIN,POLLOUT宏，std::chrono::milliseconds等待的超时，若传numeric_limits<milliseconds>::max则表示无限超时
 *@return expected<bool,Socket_error>若已可读/可写返回true,若超时返回false,套接字异常返回Socket_error。
 *@note poll返回的错误码不会是TIME_OUT,如果超时返回false。再次强调TIME_OUT是上层封装的io函数和accept返回的
 *@note 函数契约:
            fd必须非-1,但可以使用无效描述符或非套接字，系统api会检测到
            timeout可以小于0,但会直接返回false
            timeout取milliseconds的最大值代表无限阻塞
 */
inline std::expected<bool,Socket_error> poll_chrono(int fd,short io/*直接传POLLIN/OUT*/,std::chrono::milliseconds timeout)noexcept
{
    using namespace std::chrono;
    assert(fd!=-1);
    if(timeout<milliseconds::zero())
        return false;

    pollfd input{.fd=fd,.events=io};
    auto deadline=steady_clock::now()+timeout;
    //值得注意的是除非使用代表无穷超时的milliseconds::max(),deadline几乎没有上溢的可能性
    //而timeout_ms被设为-1后，便不会再触碰deadline变量，因而真的触发的deadline上溢引发的严重后果，那大概是千年虫又来了
    int timeout_ms;
    if(timeout == std::numeric_limits<milliseconds>::max())
        timeout_ms=-1;
    else if(timeout <= milliseconds(std::numeric_limits<int>::max()))
        timeout_ms=static_cast<int>(timeout.count());
    else
        timeout_ms=std::numeric_limits<int>::max();
    while(true)
    {
        int ret=::poll(&input,1,timeout_ms);
        if(ret < 0)
        {
            if(errno==EINTR)
            {
                if(timeout_ms==-1)
                    continue;
                timeout=duration_cast<milliseconds>(deadline-steady_clock::now());
                if(timeout < milliseconds::zero())
                    return false;
                if(timeout <= milliseconds(std::numeric_limits<int>::max()))
                    timeout_ms=static_cast<int>(timeout.count());
                else
                    timeout_ms=std::numeric_limits<int>::max();
                continue;
            }
            return std::unexpected(error_map(errno));
        }else if(ret==0)
            // return false;
            //一定要考虑是int::max的超时，此时还没有真正超时,timeout_ms==-1是不可能的
        {
            //非max的真超时
            if(timeout_ms < std::numeric_limits<int>::max())
                return false;

            timeout=duration_cast<milliseconds>(deadline-steady_clock::now());
            if(timeout < milliseconds::zero())
                return false;
            if(timeout <= milliseconds(std::numeric_limits<int>::max()))
                timeout_ms=static_cast<int>(timeout.count());
            else
                timeout_ms=std::numeric_limits<int>::max();
            continue;
        }
        if(input.revents & POLLNVAL) return std::unexpected(Socket_error::INVALID_SOCKET);
        if(input.revents & POLLERR) {
            int soerr = 0;
            socklen_t len = sizeof(soerr);
            if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) == 0 && soerr != 0)
                return std::unexpected(error_map(soerr));
            return std::unexpected(Socket_error::UNKNOWN);
        }
        if(input.revents & io) return true;
        if(input.revents & POLLHUP) return std::unexpected(Socket_error::PEER_CLOSED);

        return std::unexpected(Socket_error::UNKNOWN);
    }
}



//给用户代码看的对端ip-port结构体，以及解析sockaddr的函数
struct Address_info
{
    exstring::Explicit_string address;
    std::uint16_t port;
};
inline Address_info deaddress(const sockaddr* peer_address)
{
    char ip[INET6_ADDRSTRLEN]{};
    uint16_t port{0};
    if(peer_address->sa_family==AF_INET)
    {
        const sockaddr_in *addr4=reinterpret_cast<const sockaddr_in*>(peer_address);
        ::inet_ntop(AF_INET,&(addr4->sin_addr),ip,sizeof(ip));
        port=::ntohs(addr4->sin_port);
    }else if(peer_address->sa_family==AF_INET6)
    {
        const sockaddr_in6 *addr6=reinterpret_cast<const sockaddr_in6*>(peer_address);
        ::inet_ntop(AF_INET6,&(addr6->sin6_addr),ip,sizeof(ip));
        port=::ntohs(addr6->sin6_port);
    }
    return {exstring::Explicit_string{ip},port};//meybe throw
}
inline std::pair<bool,sockaddr_storage> inaddress(std::string_view ip,std::uint16_t port)noexcept
{
    std::pair<bool,sockaddr_storage> result{false,{}};
    if(ip.size() >= INET6_ADDRSTRLEN)
        return result;
    char ip_str[INET6_ADDRSTRLEN]{};
    std::memcpy(ip_str,ip.data(),ip.size());
    {
        result.second.ss_family=AF_INET;
        sockaddr_in *ipv4=reinterpret_cast<sockaddr_in*>(&(result.second));
        if(::inet_pton(AF_INET,ip_str,&(ipv4->sin_addr))==1)
        {
            ipv4->sin_port=::htons(port);
            result.first=true;
            return result;
        }
    }
    {
        result.second.ss_family=AF_INET6;
        sockaddr_in6 *ipv6=reinterpret_cast<sockaddr_in6*>(&(result.second));
        if(::inet_pton(AF_INET6,ip_str,&(ipv6->sin6_addr))==1)
        {
            ipv6->sin6_port=::htons(port);
            result.first=true;
            return result;
        }
    }
    return result;
}




//用户态使用的域名解析类，是类似链表的，符合范围for的鸭子类接口规范，只能用工厂函数resolve创建
class DNS_resolver
{
    addrinfo *_addr_list{nullptr};
    DNS_resolver(addrinfo* list)noexcept:_addr_list{list}{}
public:
    /*
    2bit
         本地    对端
    tcp   00      01
    udp   10      11
    */
    enum Resolve_strategy{TCP_SELF=0,TCP_PEER=1,UDP_SELF=2,UDP_PEER=3};
    DNS_resolver(const DNS_resolver&)=delete;
    DNS_resolver(DNS_resolver&&other)noexcept:_addr_list{other._addr_list} {other._addr_list=nullptr;}
    DNS_resolver& operator=(const DNS_resolver&)=delete;
    DNS_resolver& operator=(DNS_resolver&&other)noexcept
    {
        if(&other==this)
            return *this;
        if(_addr_list)
            ::freeaddrinfo(_addr_list);
        _addr_list=other._addr_list;
        other._addr_list=nullptr;
    }
    static std::expected<DNS_resolver,Socket_error> resolve(
        std::string_view host,
        std::string_view port,
        Resolve_strategy resolve_strategy
    )noexcept
    {
        if(port.empty())
            return std::unexpected(Socket_error::INVALID_PARAM);
        std::unique_ptr<char[]> _host,_port{new(std::nothrow)char[port.size()+1]{}};
        if(!_port)
            return std::unexpected(Socket_error::ALLOC_FAIL);
        std::memcpy(_port.get(),port.data(),port.size());
        addrinfo hint{
            .ai_family=AF_UNSPEC
        },*result{nullptr};

        if(resolve_strategy & 2)
            hint.ai_socktype=SOCK_DGRAM;
        else
            hint.ai_socktype=SOCK_STREAM;
        
        if(resolve_strategy & 1)
        {
            // hint.ai_flags=...;peer取默认flags
            if(host.empty())
                return std::unexpected(Socket_error::INVALID_PARAM);
            _host.reset(new(std::nothrow)char[host.size()+1]);
            if(!_host)
                return std::unexpected(Socket_error::ALLOC_FAIL);
            std::memcpy(_host.get(),host.data(),host.size());
        }else
        {
            hint.ai_flags=AI_PASSIVE;
            if(!host.empty())
            {
                _host.reset(new(std::nothrow)char[host.size()+1]);
                if(!_host)
                    return std::unexpected(Socket_error::ALLOC_FAIL);
                std::memcpy(_host.get(),host.data(),host.size());
            }
        }
        int ret=0;
        if((ret=::getaddrinfo(_host.get(),_port.get(),&hint,&result))!=0)
            return std::unexpected(eai_error_map(ret));
        return DNS_resolver{result};
    }
    static std::expected<DNS_resolver,Socket_error> resolve(
        std::string_view port,
        Resolve_strategy resolve_strategy
    )noexcept
    {
        if(port.empty())
            return std::unexpected(Socket_error::INVALID_PARAM);
        
        std::unique_ptr<char[]> _port{new(std::nothrow)char[port.size()+1]{}};
        if(!_port)
            return std::unexpected(Socket_error::ALLOC_FAIL);
        std::memcpy(_port.get(),port.data(),port.size());
        
        addrinfo hint{
            .ai_flags=AI_PASSIVE,
            .ai_family=AF_UNSPEC
        },*result{nullptr};

        if(resolve_strategy & 1)
            return std::unexpected(Socket_error::INVALID_PARAM);

        if(resolve_strategy & 2)
            hint.ai_socktype=SOCK_DGRAM;
        else
            hint.ai_socktype=SOCK_STREAM;
        
        int ret=0;
        if((ret=::getaddrinfo(nullptr,_port.get(),&hint,&result))!=0)
            return std::unexpected(eai_error_map(ret));
        return DNS_resolver(result);
    }
    ~DNS_resolver()
    {
        if(_addr_list)
            ::freeaddrinfo(_addr_list);
    }
    class iterator
    {
        mutable addrinfo *pointer;
        friend class DNS_resolver;
        iterator(addrinfo* ptr)noexcept:pointer{ptr}{}
    public:
        iterator(const iterator&)noexcept=default;
        iterator(iterator&&other)noexcept:pointer{other.pointer}{other.pointer=nullptr;}
        iterator& operator=(const iterator&)noexcept=default;
        iterator& operator=(iterator&&other)noexcept
        {
            addrinfo *cur=other.pointer;
            other.pointer=nullptr;
            pointer=cur;
            return *this;
        }

        iterator& operator++()noexcept
        {
            if(pointer)
                pointer=pointer->ai_next;
            return *this;
        }
        iterator operator++(int)noexcept
        {
            iterator result=*this;
            ++*this;
            return result;
        }
        bool operator==(const iterator&other)const noexcept{return pointer==other.pointer;}
        bool operator!=(const iterator&other)const noexcept{return pointer!=other.pointer;}
        addrinfo& operator*()noexcept{return *pointer;}
        const addrinfo& operator*()const noexcept{return *pointer;}
        addrinfo* operator->()noexcept{return pointer;}
        const addrinfo* operator->()const noexcept{return pointer;}
        const addrinfo* get_pointer()const noexcept{return pointer;}
        const sockaddr* get_address()const noexcept{return pointer->ai_addr;}
    };
    iterator begin()const noexcept{return _addr_list;}
    iterator end()const noexcept{return nullptr;}
};
}