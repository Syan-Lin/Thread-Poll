#include "thread_pool.h"
#include <iostream>

using namespace std;
using namespace thread_pool;

void count(atomic<int>& num){
    this_thread::sleep_for(chrono::milliseconds(10));
    num++;
}

int main(){
    atomic<int> num(0);
    ThreadPool tp(20);
    int times = 1000;
    vector<future<void>> futures;
    for(int i = 0; i < times; i++){
        futures.emplace_back(tp.submit(count, std::ref(num)));
    }
    // reduce worker thread
    tp.setSize(10);
    this_thread::sleep_for(chrono::milliseconds(500));
    // increase worker thread
    tp.setSize(20);
    // cancel tasks remained in queue
    // with this called, the result may less than 1000
    tp.cancel();

    for(auto& e : futures){
        e.wait();
    }
    // the result should be 1000 if cancel is not called
    cout << "result: " << num << endl;

    return 0;
}