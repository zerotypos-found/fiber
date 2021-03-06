
//          Copyright Oliver Kowalke 2015.
//          Copyright Brandon Kohn 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

#include <boost/fiber/all.hpp>

#include "barrier.hpp"
#include "bind/bind_processor.hpp"

using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using time_point_type = clock_type::time_point;
using channel_type = boost::fibers::buffered_channel< std::uint64_t >;
using allocator_type = boost::fibers::fixedsize_stack;
using lock_type = std::unique_lock< std::mutex >;

static bool done = false;
static std::mutex mtx{};
static boost::fibers::condition_variable_any cnd{};

// microbenchmark
std::uint64_t skynet(allocator_type& salloc, std::uint64_t num, std::uint64_t size, std::uint64_t div)
{
    if ( size != 1){
        size /= div;

        std::vector<boost::fibers::future<std::uint64_t> > results;
        results.reserve( div);

        for ( std::uint64_t i = 0; i != div; ++i) {
            std::uint64_t sub_num = num + i * size;
            results.emplace_back(boost::fibers::async(
                  boost::fibers::launch::dispatch
                , std::allocator_arg, salloc
                , skynet
                , std::ref( salloc), sub_num, size, div));
        }

        std::uint64_t sum = 0;
        for ( auto& f : results)
            sum += f.get();
            
        return sum;
    }

    return num;
}

void thread( unsigned int max_idx, unsigned int idx, barrier * b) {
    bind_to_processor( idx);
    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >( max_idx, idx);
    b->wait();
    lock_type lk( mtx);
    cnd.wait(lk, [](){ return done; });
    BOOST_ASSERT( done);
}

int main() {
    try {
        unsigned int cpus = std::thread::hardware_concurrency();
        barrier b( cpus);
        unsigned int max_idx = cpus - 1;
        boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >( max_idx, max_idx);
        bind_to_processor( max_idx);
        std::size_t stack_size{ 4048 };
        std::size_t size{ 100000 };//! NOTE: This can handle the standard 1 million.
        std::size_t div{ 10 };
        std::vector< std::thread > threads;
        for ( unsigned int idx = 0; idx < max_idx; ++idx) {
            threads.push_back( std::thread( thread, max_idx, idx, & b) );
        };
        allocator_type salloc{ stack_size };
        std::uint64_t result{ 0 };
        duration_type duration{ duration_type::zero() };
        b.wait();
        time_point_type start{ clock_type::now() };
        result = skynet( salloc, 0, size, div);
        duration = clock_type::now() - start;
        std::cout << "Result: " << result << " in " << duration.count() / 1000000 << " ms" << std::endl;
        lock_type lk( mtx);
        done = true;
        lk.unlock();
        cnd.notify_all();
        for ( std::thread & t : threads) {
            t.join();
        }
        std::cout << "done." << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
	return EXIT_FAILURE;
}
