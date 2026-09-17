/*
 *event_handle是个虚基类，所有塞进Reactor的handle都要继承自这个类，
 *handle要提供统一的call_back,get_handle等接口以便于reactor调用
*/
#pragma once

#include<unistd.h>
#include<memory>
#include<exstring.hpp>
#include"err_map.hpp"

namespace shape
{
//可以承载文件描述符，socket和signalfd

enum class IO_strategy:int
{
    DEFAULT,
    WAIT_ALL,//一直等到要求字节数全部集齐。反之（即默认情况）有多少读多少,但不会读个0
};

//就一个八字节的结构体还要用unique_ptr保证安全实在是太多余了，还不如直接保存结构体
//RAII_unique_fd要遵守一个契约：如果不为-1,这个套接字必须是本端有效的
class RAII_unique_fd
{
    int _fd;
protected:
    void set_fd(int fd)noexcept
    {
        _fd=fd;
    }
public:
    RAII_unique_fd(int fd)noexcept:_fd{fd}{}

    RAII_unique_fd& operator=(const RAII_unique_fd&)=delete;
    RAII_unique_fd(const RAII_unique_fd&)=delete;
    RAII_unique_fd& operator=(RAII_unique_fd &&other)noexcept
    {
        if(&other == this)
            return *this;
        if(_fd!=-1)
            ::close(_fd);
        _fd=other._fd;
        other._fd=-1;
        return *this;
    }
    RAII_unique_fd(RAII_unique_fd&& other)noexcept:_fd{other._fd}
    {
        other._fd=-1;
    }
    int get_fd()const noexcept{return _fd;}
    void close()noexcept
    {
        if(_fd!=-1)
        {
            ::close(_fd);
            _fd=-1;
        }
    }
    bool valid()const noexcept
    {
        return _fd!=-1;
    }
    ~RAII_unique_fd()
    {
        if(_fd!=-1)
            ::close (_fd);
    }
};

}
