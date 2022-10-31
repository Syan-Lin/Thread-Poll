# Thread-Poll
![](https://img.shields.io/badge/c%2B%2B-11-blue) ![](https://img.shields.io/badge/license-mit-blue)

### 介绍
Thread-Poll 是一个用 C++11 实现的一个简易线程池，可动态增减当前运行线程。导入只需引入头文件，无第三方库依赖

### 示例

```cpp
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
```
