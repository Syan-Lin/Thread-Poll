#include "thread_pool.h"
#include <iostream>
#include <functional>

using namespace std;
using namespace thread_pool;

void test(atomic<int>& temp){
    this_thread::sleep_for(std::chrono::milliseconds(1));
    temp++;
}

int main(){
    ThreadPool tp(20);
    atomic<int> temp(0);
    int times = 10;
    chrono::system_clock::time_point time_point_now = chrono::system_clock::now();
    for(int i = 0; i < times; i++){
        // this_thread::sleep_for(chrono::milliseconds(5));
        tp.submit(test, std::ref(temp));
    }
    while(temp < times){
    }
    cout << "done" << endl;
    chrono::system_clock::time_point time_point_after = chrono::system_clock::now();
    chrono::system_clock::duration duration = time_point_after - time_point_now;
    time_t abc = chrono::duration_cast<chrono::milliseconds>(duration).count();
    cout << "time: " << abc << "ms" << endl;
    return 0;
}
