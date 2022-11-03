#include "thread_pool.h"
#include <iostream>

using namespace thread_pool;

void count(std::atomic<int>& num){
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    num++;
}

int main(){
    std::atomic<int> num(0);
    ThreadPool tp;
    int times = 1000;
    std::vector<std::future<void>> futures;
    for(int i = 0; i < times; i++){
        futures.push_back(tp.submit(::count, std::ref(num)));
    }
    // set the quantity of worker threads
    tp.setSize(10);

    // cancel all tasks remained in task queue
    // with this called, the result may less than 1000
    tp.cancel();

    for(auto& e : futures){
        e.wait();
    }

    // the result should be 1000 if cancel is not called
    std::cout << "result: " << num << std::endl;

    return 0;
}