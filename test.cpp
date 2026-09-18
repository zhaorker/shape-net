#include"include/nettool.hpp"
#include<iostream>
int main()
{
    auto result=shape::DNS_resolver::resolve("::1","8080",shape::DNS_resolver::TCP_SELF);
    if(!result)
        return 0;
    for(auto it=result->begin();it!=result->end();++it)
    {
        auto [ip ,port]=shape::deaddress(it.get_address());
        std::cout<<ip<<' '<<port<<'\n';
    }
}