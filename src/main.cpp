// Copyright 2019 municHMotorsport e.V. <info@munichmotorsport.de>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>
#include <thread>
#include <random>
#include <vector>
#include <queue>
#include <algorithm>

#include "../library/autogen-SRL-macros.h"

/// What kind of work do we want to simulate
enum WORK : uint8_t {
    SMALL = 0,
    HEAVY = 1 
};

/// helper to use auto instead of the type
std::chrono::high_resolution_clock::time_point get_current_time()
{
    return std::chrono::high_resolution_clock::now();
}



/// simple producer class which run() function has to be started in a thread
class Producer 
{
// constructors
public:
    Producer(uint32_t payload_count
           , uint32_t producer_frequency_ms
           , uint32_t heavy_work_every_n_ticks)
    : _payload_count(payload_count)
    , _producer_frequency_ms(producer_frequency_ms)
    , _heavy_work_every_n_ticks(heavy_work_every_n_ticks)
    , _current_payload_count(0)
    {}

    Producer(Producer & other) = delete;
    Producer(Producer && other) = delete;


// methods
public:

    /// run this function as a thread
    void run()
    {
        while(!done())
        {   
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(_producer_frequency_ms));
            if ((_current_payload_count % _heavy_work_every_n_ticks) == 0)
            {
                _buffered_payload.push(WORK::HEAVY);
                DEBUG_MSG_SRL("Producer: Creating HEAVY workload, got " << _buffered_payload.size() << " left in queue\n")
            } else 
            {
                _buffered_payload.push(WORK::SMALL);
                DEBUG_MSG_SRL("Producer: Creating SMALL workload, got " << _buffered_payload.size() << " left in queue\n")
            }
            _current_payload_count += 1;
    //        DEBUG_MSG_SRL("Producer: Current counter: " << _current_payload_count << "\n");
        }
    }

    /// can only be accessed if `has_payload` returns true (not more than one consumer - otherwise you get race conditions)
    enum WORK get_payload()
    {
        const enum WORK work = _buffered_payload.front();
        _buffered_payload.pop();
        return work;
    }
   
    /// if the buffer is empty we have to wait until something gets produced
    bool has_payload()
    { 
    //    DEBUG_MSG_SRL("Producer: bufferd_payload.size(): " << _buffered_payload.size() << "\n");
        return !_buffered_payload.empty();
    }
    
    /// if we don't have any data left, stop producing
    bool done(){ return _payload_count == _current_payload_count; }

// const member
private:
    /// how many payloads are to be created
    const uint32_t _payload_count;
    /// how fast should payloads be produced
    const uint32_t _producer_frequency_ms;

// non-const member
private:
    // saved work to be accessed by `get_payload` should maybe hold shared pointer 
    std::queue<enum WORK> _buffered_payload;
    // how often do we want to create `enum WORK::HEAVY` 
    uint32_t _heavy_work_every_n_ticks;
    /// current counter
    uint32_t _current_payload_count;
};

// simple time agnostic consumer, it simply takes time to process work
class Consumer
{
// constructors
public:
    Consumer(uint32_t small_work_time_ms
           , uint32_t heavy_work_time_ms
           , uint32_t target_frequency_hz)
    : _small_work_time_ms(small_work_time_ms)
    , _heavy_work_time_ms(heavy_work_time_ms)
    , _target_frequency_ms(1000.0 / static_cast<double>(target_frequency_hz))
    , _lag(0.0) { }

/// methods 
public:
    
    void do_work(const enum WORK & work)
    {
        auto start = get_current_time();
        switch (work) {
            case SMALL:
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(_small_work_time_ms));
                DEBUG_MSG_SRL("Consumer - working on a SMALL payload\n");
                break;
            case HEAVY:
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(_heavy_work_time_ms));
                DEBUG_MSG_SRL("Consumer - working on a HEAVY payload\n");
                break;
            default:
                DEBUG_MSG_SRL("Consumer ERROR: enum WORK case not implemented yet\n"); 
                break;
        }
        auto end = get_current_time();
        const std::chrono::duration<double, std::milli> work_time = end - start;
        // how much faster were we than we should've been?
        const double sleep_time_ms =  _target_frequency_ms -  work_time.count();
        // apply the accumulted lag, if there was any (it's negative)
        const double gained_time = sleep_time_ms + _lag;
        // if we catched up to the lag, sleep
        if (gained_time > 0)
        {
            _lag = 0;
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(gained_time));
        } else // otherwise set the new lag
        {
            _lag = gained_time;
        }
    }

/// const member
private:
    const uint32_t _small_work_time_ms;
    const uint32_t _heavy_work_time_ms;
    const double   _target_frequency_ms;

// member
private:
    double _lag;
};

int main(void)
{
    // producer_frequency_ms has to be FASTER than the target_frequency_hz, otherwise the
    // resulting measurement won't work

    // create producer
    uint32_t producer_count = 100;
    uint32_t producer_frequency_ms    = 10;  // 100Hz
    uint32_t heavy_work_every_n_ticks = 5;   // 20Hz
    Producer producer(producer_count, producer_frequency_ms, heavy_work_every_n_ticks);
    
    // create consumer
    uint32_t small_work_time_ms = 2;    // 500Hz throughput
    uint32_t heavy_work_time_ms = 20;   // 50Hz throughput
    uint32_t target_frequency_hz = 100; // should be called every 20ms
    uint32_t consumer_finished_payloads = 0; // finished payloads
    Consumer consumer(small_work_time_ms, heavy_work_time_ms, target_frequency_hz);

    // run producer
    std::thread producer_thread([&](){ producer.run(); });

    while(!producer.done())
    {
        if(producer.has_payload())
        {
            // track time between work steps
            const auto start = get_current_time();
            // do main work which may trigger UDP send requests or whatever (should be a shared_ptr)
            enum WORK work = producer.get_payload();
            consumer.do_work(work);
            // stop timetracking 
            const auto end = get_current_time();
            const std::chrono::duration<double, std::milli> work_time = end - start;
            const double frequency = 1000.0 / static_cast<double>(work_time.count());
            consumer_finished_payloads += 1;
            //std::cout << "Step [" << consumer_finished_payloads << "/" << producer_count << "]: " << frequency << "Hz\n";
            //std::cout << frequency << ", ";
        }
    }

    producer_thread.join();

    return 0;
}
