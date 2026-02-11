//driver code
#include<code.h>
#include <atomic>
#include <vector>
#include <optional>
#include <iostream>
#include <new>
#include <thread>

int main(){
    SPSCQueue<int> queue(100);

    //prod thread
    std::thread producer([&](){
        for(int i=0; i<1000; ++i){
            while (!queue.enqueue(i)){
                //busy wait or yield if full
                std::this_thread::yield();
            }
    }});

    //consumer thread
     std::thread consumer([&](){
        int count =0;
        
        while (count<1000){
            auto val= queue.try_dequeue();
            if(val){
                count++;
            }else{
                std::this_thread::yield();
            }
                
    }});

    producer.join();
    consumer.join();

    std::cout <<"Done processinf 1000 items." << std::endl;
    return 0;
    
}