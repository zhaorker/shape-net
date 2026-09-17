#pragma once

#include<errno.h>
#include<netdb.h>

namespace shape
{
// class Errorno
// {
//     thread_local static int err_no;
// public:
//     int get_last_error()const noexcept{return err_no;}
//     void set_errno(int no)const noexcept{err_no=no;}
// }thread_local constexpr inline last_error;
/**
 *@brief封装socket专用errno，以及部分库内部需要的错误码
 *此外这个枚举将很多触发条件相近和处理方法相近的errno值合并到同一枚举，旨在尽量减少库本身的错误码总量
 *目前的错误码涵盖connect\accept\io场景下可能遇到的异常，而暂时不涉及io_uring，文件io的错误码，那些不属于Socket_error
 *@par 原本计划将要涵盖56个errno值，但有大量errno可以由库的封装来避免触发，因而实际这个枚举表示的errno只有15个。
 *@par 这些错误码不追求面面俱到，但可以保证用户在拿到Socket_error后知道要做什么
 *@note 请仔细阅读各个函数的文档，他们将明确自己返回那些错误码及确切含义
*/
enum class Socket_error:unsigned int
{
    NO_ERROR,
//泛用错误码
    PERMISSION_DENIED,//EACCES和EPERM映射为此错误码
    INVALID_SOCKET,//默认构造的Socket_handle值为-1,通常在调用系统接口前会自检是否-1。但如果上下文get_fd并在其他地方单独使用该套接字导致触发close，库会将ENOTSOCK和EBADF映射为该错误码
    PEER_CLOSED,//正常对端关闭，EPIPE也会映射为该错误码，因为EPIPE通常由关后写导致，本质原因是对端关闭。
    PEER_RESET,//由ETIMEDOUT/ECONNRESET/ECONNREFUS映射而来E表示发了reset,是对端崩溃，另外connect失败也可能返回PEER_RESET表示对端拒绝连接
    UNREACHABLE,//EHOSTUNREACH,ENETUNREACH,EHOSTDOWN,ENETDOWN还包含一个极少见的ENONET
    INVALID_PARAM,//一般表示向方法传入了无效参数，或者系统调用认为某些参数overflow、值不存在也会返回该错误
    RESOURCE_EXHAUSTED,//由EMFILE,ENFILE映射而来，listen和accept和connect创建新套接字的操作都可能返回该错误
    NEED_RECONNECT,//由ENETRESET映射而来，在各种函数里都可能返回
    ALLOC_FAIL,//内存不足，可能是内核引发，或库层捕获bad_alloc
    
//专用错误码
    ADDRESS_INVALID,//该错误由EADDRNOTAVAIL映射而来,bind指定的不正确的hostname或者connect分配ip失败会返回该错误
    SEND_AFTER_SHUT,//特指tcp套接字在shutdown后的write,如果是close的话往往改为触发INVALID_SOCKET
    MESSAGE_TOOBIG,//udp::send_package专属，由EMSGSIZE映射而来
    TIME_OUT,//这不由任何错误码映射而来，ETIMEDOUT和ECONNREFUSE全都映射到PEER_RESET了(因为在库内部认为connect返回ETIMEDOUT意味着拒绝连接;io全部是通过poll之后进行非阻塞io,如果poll真的返回ETIMEDOUT会视为对端静默崩溃而返回PEER_RESET)，返回TIME_OUT的含义是io函数poll超时仍然没有数据，但连接依然有效且完好；

//DNS特有
    DNS_RESOLVE_AGAIN,//解析暂时失败，可能由于网络波动或系统信号
    DNS_RESOLVE_FAIL,//解析彻底失败
    DNS_NOT_FOUND,//解析成功但不存在的域名

    SYS_ERROR,//EBUSY,EIO,EPROTO,EBADMSG
    UNKNOWN
};
inline Socket_error error_map(int err_no)noexcept
{
    // last_error.set_errno(err_no);
    switch(err_no){
    case EACCES:
    case EPERM:
        return Socket_error::PERMISSION_DENIED;
    case EADDRNOTAVAIL:
        return Socket_error::ADDRESS_INVALID;
    case EBADF:
    case EBADFD:
    case ENOTCONN://其实不太可能触发
    case ENOTSOCK:
        return Socket_error::INVALID_SOCKET;
    case EPIPE:
        return Socket_error::PEER_CLOSED;
    case ETIMEDOUT:
    case ECONNREFUSED:
    case ECONNRESET:
        return Socket_error::PEER_RESET;
    case EHOSTUNREACH:
    case EHOSTDOWN:
    case ENETUNREACH:
    case ENETDOWN:
    case ENONET:
        return Socket_error::UNREACHABLE;
    case EDESTADDRREQ:
    case EFAULT:
    case EINVAL:
        return Socket_error::INVALID_PARAM;
    case EMFILE:
    case ENFILE:
        return Socket_error::RESOURCE_EXHAUSTED;
    case ENETRESET:
        return Socket_error::NEED_RECONNECT;
    case ENOMEM:
    case ENOBUFS:
        return Socket_error::ALLOC_FAIL;
    case ESHUTDOWN:
        return Socket_error::SEND_AFTER_SHUT;
    case EMSGSIZE:
        return Socket_error::MESSAGE_TOOBIG;
    case EBUSY:
    case EIO:
    case EPROTO:
    case EBADMSG:
        return Socket_error::SYS_ERROR;
    // case EAI_AGAIN:
    //     return Socket_error::DNS_RESOLVE_AGAIN;
    // case EAI_FAIL:
    //     return Socket_error::DNS_RESOLVE_FAIL;
    // case EAI_NODATA:
    // case EAI_NONAME:
    //     return Socket_error::DNS_NOT_FOUND;
    // case EAI_SYSTEM:
    //     return error_map(errno);
    default:
        return Socket_error::UNKNOWN;
    }
}
inline Socket_error eai_error_map(int eai)noexcept
{
    switch(eai){
    case EAI_ADDRFAMILY:
    case EAI_BADFLAGS:
    case EAI_FAMILY:
    case EAI_SERVICE:
    case EAI_SOCKTYPE:
        return Socket_error::INVALID_PARAM;
    case EAI_AGAIN:
        return Socket_error::DNS_RESOLVE_AGAIN;
    case EAI_FAIL:
        return Socket_error::DNS_RESOLVE_FAIL;
    case EAI_MEMORY:
        return Socket_error::ALLOC_FAIL;
    case EAI_NODATA:
    case EAI_NONAME:
        return Socket_error::DNS_NOT_FOUND;
    case EAI_SYSTEM:
        return error_map(errno);
    default:
        return Socket_error::UNKNOWN;
    }
}
}