#include"socket/tcp.hpp"
#include<cstdio>
#include<cstring>
using namespace std::chrono_literals;
int main()
{
    auto so=shape::tcp::start_connect("127.0.0.1",8000);
    if(!so)
        return 0;
    auto socket=so->poll_wait(1min).value();//如果连接尚未完成这里就直接抛异常

    char buffer[1024];
    scanf("%1023s",buffer);
    auto result=socket.write(buffer,strlen(buffer),1min,shape::IO_strategy::WAIT_ALL);
    if(!result)
        return 0;
    char receive_buff[1024]{};
    socket.read(receive_buff,1023,1min,shape::IO_strategy::DEFAULT);
    printf("%s\n",receive_buff);
}