#include"socket/tcp.hpp"
#include<chrono>
#include<cstdio>

int main()
{
    using namespace std::chrono_literals;
    auto listener=shape::tcp::listen("8000",100).value();
    for(;;)
    {
        auto [client,address]=listener.accept(5s);
        printf("accept peer:%s %uh\n",address.address.data(),address.port);
        if(!client.valid())
            continue;
        char receive_buff[1024]{};
        auto result=client.read(receive_buff,1023,1min,shape::IO_strategy::DEFAULT);
        if(result)
            printf("receive message:%s\n",receive_buff);
        else
            continue;
        result=client.write(receive_buff,*result,1min,shape::IO_strategy::WAIT_ALL);
    }
}