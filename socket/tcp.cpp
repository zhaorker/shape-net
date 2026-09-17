#include"tcp.hpp"
#include<cstdlib>
#include<netdb.h>
#include<poll.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<cerrno>
#include<cstring>

namespace shape
{
namespace tcp
{
using namespace std::literals;


/**class Socket_handle **/
void Socket_handle::shut_read()noexcept
{
    if(valid())
        ::shutdown(get_fd(),SHUT_RD);
}
void Socket_handle::shut_write()noexcept
{
    if(valid())
        ::shutdown(get_fd(),SHUT_WR);
}

std::expected<std::size_t,Socket_error> Socket_handle::read(char* buffer,std::size_t max_len,
    std::chrono::milliseconds timeout,IO_strategy strategy)noexcept
{
    if(get_fd()==-1)
        return std::unexpected(Socket_error::INVALID_SOCKET);
    if(!buffer || max_len<=0)
        return std::unexpected(Socket_error::INVALID_PARAM);

    auto end_time=std::chrono::steady_clock::now()+timeout;
    std::size_t read_count{};

    while(true)
    {
        auto poll_result=poll_chrono(get_fd(),POLLIN,timeout);

        if(!poll_result)
            return read_count ? std::expected<std::size_t,Socket_error>(read_count) : std::unexpected(poll_result.error());
        if(!poll_result.value())
            return read_count ? std::expected<std::size_t,Socket_error>(read_count) : std::unexpected(Socket_error::TIME_OUT);
        int rcv_ret=::recv(get_fd(),buffer,max_len,MSG_NOSIGNAL);
        while(rcv_ret==-1 && errno==EINTR)
            rcv_ret=::recv(get_fd(),buffer,max_len,MSG_NOSIGNAL);
        if(rcv_ret==-1)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)
            {
                timeout=std::chrono::duration_cast<std::chrono::milliseconds>(end_time-std::chrono::steady_clock::now());
                continue;
            }
            return std::unexpected(error_map(errno));
        }
        if(rcv_ret==0)
        {
            if(read_count)
                return read_count;//先返回读到的数据，下次再触发关闭流程
            return std::unexpected(Socket_error::PEER_CLOSED);
        }

        if(strategy!=IO_strategy::WAIT_ALL)
            return rcv_ret;
        read_count+=rcv_ret;
        max_len-=rcv_ret;
        buffer+=rcv_ret;

        if(max_len<=0)
            return read_count;

        timeout=std::chrono::duration_cast<std::chrono::milliseconds>(end_time-std::chrono::steady_clock::now());
    }
}
std::expected<std::size_t,Socket_error> Socket_handle::write(const char*buffer,std::size_t max_len,
    std::chrono::milliseconds timeout,IO_strategy strategy)noexcept
{
    if(get_fd()==-1)
        return std::unexpected(Socket_error::INVALID_SOCKET);
    if(!buffer || max_len<=0)
        return std::unexpected(Socket_error::INVALID_PARAM);

    auto end_time=std::chrono::steady_clock::now()+timeout;
    std::size_t write_count{};

    while(true)
    {
        auto poll_result=poll_chrono(get_fd(),POLLOUT,timeout);

        if(!poll_result)
            return write_count ? std::expected<std::size_t,Socket_error>(write_count) : std::unexpected(poll_result.error());
        if(!poll_result.value())
            return write_count ? std::expected<std::size_t,Socket_error>(write_count) : std::unexpected(Socket_error::TIME_OUT);
        int send_ret=::send(get_fd(),buffer,max_len,MSG_NOSIGNAL);
        while(send_ret==-1 && errno==EINTR)
            send_ret=::send(get_fd(),buffer,max_len,MSG_NOSIGNAL);
        if(send_ret==-1)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)
            {
                timeout=std::chrono::duration_cast<std::chrono::milliseconds>(end_time-std::chrono::steady_clock::now());
                continue;
            }
            return std::unexpected(error_map(errno));
        }
        if(send_ret==0)
        {
            timeout=std::chrono::duration_cast<std::chrono::milliseconds>(end_time-std::chrono::steady_clock::now());
            continue;
        }

        if(strategy!=IO_strategy::WAIT_ALL)
            return send_ret;
        write_count+=send_ret;
        max_len-=send_ret;
        buffer+=send_ret;

        if(max_len<=0)
            return write_count;

        timeout=std::chrono::duration_cast<std::chrono::milliseconds>(end_time-std::chrono::steady_clock::now());
    }
}
//class Socket_handle



/**class In_connecting**/
std::expected<In_connecting,Socket_error> 
    start_connect(const addrinfo*address)noexcept
{
    int _fd=::socket(address->ai_family,address->ai_socktype,address->ai_protocol);
    if(_fd<0)
        return std::unexpected(error_map(errno));
    if(!set_nonblock(_fd))
    {
        ::close(_fd);
        return std::unexpected(error_map(errno));
    }
    int ret=::connect(_fd,address->ai_addr,address->ai_addrlen);
    if(ret==0)
        return In_connecting{_fd,true};
    else if(errno == EINPROGRESS)
        return In_connecting{_fd,false};
    int error=errno;
    ::close(_fd);
    return std::unexpected(error_map(error));
}
std::expected<In_connecting,Socket_error> 
    start_connect(std::string_view ip,std::uint16_t port)noexcept
{
    auto [success,address]=inaddress(ip,port);
    if(!success)
        return std::unexpected(Socket_error::INVALID_PARAM);
    int _fd=::socket(address.ss_family,SOCK_STREAM,0);
    if(_fd<0)
        return std::unexpected(error_map(errno));
    if(!set_nonblock(_fd))
    {
        ::close(_fd);
        return std::unexpected(error_map(errno));
    }
    int ret=::connect(_fd,reinterpret_cast<sockaddr*>(&address),sizeof(address));
    if(ret==0)
        return In_connecting{_fd,true};
    else if(errno == EINPROGRESS)
        return In_connecting{_fd,false};
    int error=errno;
    ::close(_fd);
    return std::unexpected(error_map(error));
}
std::expected<Socket_handle,Socket_error> In_connecting::poll_wait(std::chrono::milliseconds timeout)noexcept
{
    if(get_fd()==-1)
        return std::unexpected(Socket_error::INVALID_SOCKET);
    if(already)
    {
        int fd=get_fd();
        set_fd(-1);
        return Socket_handle{fd};
    }
    auto poll_result=poll_chrono(get_fd(),POLLOUT,timeout);
    if(!poll_result)
        return std::unexpected(poll_result.error());
    if(!poll_result.value())
        return std::unexpected(Socket_error::TIME_OUT);
    int error{};
    socklen_t len=sizeof(error);
    if(::getsockopt(get_fd(),SOL_SOCKET,SO_ERROR,&error,&len)<0)
        return std::unexpected(error_map(errno));
    if(error==0)
    {
        int fd=get_fd();
        set_fd(-1);
        return Socket_handle{fd};
    }
    return std::unexpected(error_map(error));
}
//class In_connecting



/**class Listener**/
std::expected<Listener,Socket_error> listen(std::string_view port,int queue_len)noexcept
{
    int last_error=0;
    int _fd=-1;
    auto resolve_result=DNS_resolver::resolve(port,DNS_resolver::TCP_SELF);
    if(!resolve_result)
        return std::unexpected(resolve_result.error());
    for(addrinfo & address : *resolve_result)
    {
        _fd=::socket(address.ai_family,address.ai_socktype,address.ai_protocol);
        if(_fd==-1)
        {
            last_error=errno;
            continue;
        }
        int param=1;
        ::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &param, sizeof(param));
        ::setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &param, sizeof(param));
        if(address.ai_family==AF_INET6)
        {
            param=0;
            ::setsockopt(_fd, IPPROTO_IPV6, IPV6_V6ONLY, &param, sizeof(param));
        }
        if(::bind(_fd,address.ai_addr,address.ai_addrlen)==0)
        {
            if(::listen(_fd,queue_len)==0)
                break;
        }
        last_error=errno;
        ::close(_fd);
        _fd=-1;
    }
    if(_fd==-1)
        return std::unexpected(error_map(last_error));
    return Listener{_fd};
}
std::expected<Listener,Socket_error> listen(std::string_view host,std::string_view port,int queue_len)noexcept
{
    int last_error;
    int _fd=-1;
    auto resolve_result=DNS_resolver::resolve(host,port,DNS_resolver::TCP_SELF);
    if(!resolve_result)
        return std::unexpected(resolve_result.error());
    for(addrinfo & address : *resolve_result)
    {
        _fd=::socket(address.ai_family,address.ai_socktype,address.ai_protocol);
        if(_fd==-1)
        {
            last_error=errno;
            continue;
        }
        int param=1;
        ::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &param, sizeof(param));
        ::setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &param, sizeof(param));
        if(address.ai_family==AF_INET6)
        {
            param=0;
            ::setsockopt(_fd, IPPROTO_IPV6, IPV6_V6ONLY, &param, sizeof(param));
        }
        if(::bind(_fd,address.ai_addr,address.ai_addrlen)==0)
        {
            if(::listen(_fd,queue_len)==0)
                break;
        }
        last_error=errno;
        ::close(_fd);
        _fd=-1;
    }
    if(_fd==-1)
        return std::unexpected(error_map(last_error));
    return Listener{_fd};
}
std::pair<Socket_handle,Address_info> Listener::accept(std::chrono::milliseconds timeout)noexcept
{
    auto end_time=std::chrono::steady_clock::now()+timeout;
    //那就是说有连接可以accept了，接下来不论干什么都不会阻塞
    sockaddr_storage peer_address{};
    socklen_t addrlen=sizeof(sockaddr_storage);
    while(true)
    {
        auto poll_result=poll_chrono(get_fd(),POLLIN,timeout);
        if(!poll_result || (poll_result && !poll_result.value()))//超时或致命错误
        {
            if(!poll_result)
                self_errno=poll_result.error();
            // else 即普通的超时，不要设置TIME_OUT,把上轮连接失败的原因留给用户
            return {};
        }
        int ret_fd=::accept(get_fd(),reinterpret_cast<sockaddr*>(&peer_address),&addrlen);
        while (ret_fd==-1 && errno==EINTR)
        {
            addrlen = sizeof(sockaddr_storage);
            ret_fd=::accept(get_fd(),reinterpret_cast<sockaddr*>(&peer_address),&addrlen);
        }
        if(ret_fd==-1)
        {
            if(errno == EINTR || errno == ECONNABORTED) {
                timeout = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - std::chrono::steady_clock::now());
                if(timeout < std::chrono::milliseconds::zero()) { self_errno = Socket_error::TIME_OUT; return {}; }
                continue;
            }
            self_errno = error_map(errno);
            return {};
        }
        if(!set_nonblock(ret_fd))
        {
            self_errno=error_map(errno);
            ::close(ret_fd);
            timeout=std::chrono::duration_cast<std::chrono::milliseconds>(end_time-std::chrono::steady_clock::now());
            continue;
        }

        int ret;
        int so_error{};
        socklen_t len=sizeof(so_error);
        if ((ret=::getsockopt(ret_fd, SOL_SOCKET, SO_ERROR, &so_error, &len)) == 0 && so_error==0)
        {
            Socket_handle handle{ret_fd};
            try {
                Address_info info = deaddress(reinterpret_cast<sockaddr*>(&peer_address));
                return std::pair<Socket_handle, Address_info>{std::move(handle), std::move(info)};
            } catch (...) {
                self_errno = Socket_error::ALLOC_FAIL;
                return std::pair<Socket_handle, Address_info>{std::move(handle), Address_info{}};
            }
        }
        if(ret==0)
            self_errno=error_map(so_error);
        else
            self_errno=error_map(errno);
        ::close(ret_fd);
        timeout=std::chrono::duration_cast<std::chrono::milliseconds>(end_time-std::chrono::steady_clock::now());
    }
}
//class Listener
}
}