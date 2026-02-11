#include <atomic>
#include <vector>
#include <optional>
#include <iostream>
#include <new>
#include <thread>


//compiler hints
#pragma once
#include <cstddef>
#include <cstdint>


template<typename T>

class SPSCQueue{
    public:
    explicit SPSCQueue(size_t capacity){
        buffer_.resize(capacity+1);
        //allocate extra slot to chk if full or empty
    }

    bool isEmpty() const {
        return read_idx_.load() == write_idx_.load();
        //determine if the queue is mepty
    }


    //producer enque logic
    //he producer puts data in and moves the write_idx_.
    bool enqueue(const T& item){
        //Load current indices
        //we use relaxed cuz we the only thread writing to write_idx_
        size_t current_write = write_idx_.load(std::memory_order_relaxed);
        //calc next positon
        size_t next_write = current_write +1;

        if(next_write==buffer_.size()){
            next_write=0;
        }

        //check if full 
        //we need acquire here to ensure we see the latest read_idx_ update from the consumer
        size_t current_read= read_idx_.load(std::memory_order_acquire);
        if(next_write==current_read){
            return false; //queue full
        }

        //write the data
        buffer_[current_write]=item;

        write_idx_.store(next_write, std::memory_order_relaxed);

        return true;
    }


    //dequeue logic/ consumer

    std::optional<T> try_dequeue(){
        //load current read index (relaxed: only if we modify it)
        size_t current_read=read_idx_.load(std::memory_order_relaxed);

        //check if empty
        // We need 'acquire' to ensure that if we see the write_idx has moved, 
        // we also see the data that the producer wrote.
        size_t current_write = write_idx_.load(std::memory_order_acquire);

        if(current_read==current_write){
            return std::nullopt; //queue is empty
        }

        //read data
        T value = buffer_[current_read];

        //calc next read pos
        size_t next_read=current_read+1;
        if(next_read==buffer_.size()){
            next_read=0;
        }

        //update the read idx
        read_idx_.store(next_read,std::memory_order_relaxed);

        return value;
    }

    //false sharing implemmentation for readerwriterqueue
    //false sharing fix or avoidance basically




    private:
    std::vector<T> buffer_;
    alignas(64)std::atomic<size_t> read_idx_{0};
    

    char padding_[64-sizeof(std::atomic<size_t>)];

    alignas(64) std::atomic<size_t> write_idx_{0};
};



