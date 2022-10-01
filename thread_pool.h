#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <future>
#include <functional>

namespace thread_pool {

/**************************
* @author   Yuan.
* @date     2022/9/30
* @brief    线程安全的队列
***************************/
template <typename Func>
class TaskQueue {
public:
    TaskQueue() = default;
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue(TaskQueue&&) = delete;
    ~TaskQueue() = default;
    TaskQueue& operator=(const TaskQueue&) = delete;
    TaskQueue& operator=(TaskQueue&&) = delete;

    bool empty(){
        std::lock_guard<std::mutex> lock(queue_lock_);
        return queue_.empty();
    }
    int size(){
        std::lock_guard<std::mutex> lock(queue_lock_);
        return queue_.size();
    }
    void enqueue(Func& task){
        std::lock_guard<std::mutex> lock(queue_lock_);
        queue_.emplace(task);
    }
    bool dequeue(Func& func) {
        std::lock_guard<std::mutex> lock(queue_lock_);
        if(!queue_.empty()){
            func = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false;
    }

private:
    std::queue<Func> queue_;
    std::mutex queue_lock_;
};

/**************************
* @author   Yuan.
* @date     2022/9/30
* @brief    线程池实现
***************************/
class ThreadPool{
    friend class Thread;
private:
    /**************************
    * @author   Yuan.
    * @date     2022/9/30
    * @brief    类内对象，线程
    ***************************/
    class Thread{
        friend class ThreadPool;
    public:
        Thread(ThreadPool* pool) : thread_id_(count_++), thread_pool_(pool){
            thread_ = std::thread(std::ref(*this));
        }
        Thread(const Thread&) = delete;
        Thread(Thread&&) = delete;
        Thread& operator=(const Thread&) = delete;
        Thread& operator=(Thread&&) = delete;
        void shutdown(){ is_shutdown_ = true; }
        int getId() const { return thread_id_; }
        void operator()(){
            std::function<void()> func;
            bool is_get = false;
            while(!is_shutdown_){
                is_get = false;
                {
                    std::unique_lock<std::mutex> lock(thread_pool_->lock_);
                    thread_pool_->cv_.wait(lock, [this]{
                        if(this->is_shutdown_) return true;
                        return !this->thread_pool_->task_queue_.empty();
                    });
                    is_get = thread_pool_->task_queue_.dequeue(func);
                }
                if(is_get){
                    func();
                }
            }
        }
    public:
        int thread_id_;
        bool is_shutdown_ = false;
        ThreadPool* thread_pool_;
        std::thread thread_;
        static int count_;
    };

public:
    ThreadPool(size_t num = 6) : thread_size_(num){
        createThread(num);
    }
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;
    ~ThreadPool(){
        this->shutdown();
    }
    template<typename T>
    void test(T&& t);

    // 向线程池提交任务
    template<typename Func, typename... Args>
    auto submit(Func&& f, Args&&... args) -> std::future<decltype(f(args...))>{
        // 1. 包装一层 function，消去参数
        // <decltype(f(args...))()> 表示返回值为函数 f 的返回值的一个函数，但没有参数
        // 参数用 std::bind 将 args 绑定到 func 上，所以 func 里不需要指定参数
        // test<Args...>();
        std::function<decltype(f(args...))()> func = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);

        // 2. 用 packaged_task 包装函数，结果可以用 future 获得，使函数获得异步特性
        // packaged_task 不可复制，使用指针进行传递
        auto tast_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // 3. 再包装一层 function，使接口统一，可以放入队列
        std::function<void()> unified_func = [tast_ptr](){
            (*tast_ptr)();
        };

        task_queue_.enqueue(unified_func);
        cv_.notify_one();
        return tast_ptr->get_future();
    }
    // 查看线程数
    int size() const { return thread_size_; }
    // 真正运行的线程数
    int workingSize() const { return threads_.size(); }
    // 设置线程数
    void setSize(int size){
        if(size < thread_size_){
            shutdownThread(thread_size_ - size);
        }else{
            createThread(size - thread_size_);
        }
        thread_size_ = size;
    }
    // 关闭线程池
    void shutdown(){
        shutdownThread(threads_.size());
        int count = 0;
        cv_.notify_all();
        for(auto& ptr : threads_){
            if(ptr->thread_.joinable()){
                ptr->thread_.join();
                // std::cout << "thread " << ptr->thread_id_ << " destroyed" << std::endl;
                delete ptr;
            }
        }
    }
private:
    void shutdownThread(int num){
        for(int i = 0; i < num; i++){
            threads_[i]->shutdown();
        }
    }
    void createThread(int num){
        for(int i = 0; i < num; i++){
            threads_.push_back(new Thread(this));
        }
    }
    TaskQueue<std::function<void()>> task_queue_;
    std::vector<Thread*> threads_;
    std::condition_variable cv_;
    std::mutex lock_;
    int thread_size_;
};
int ThreadPool::Thread::count_ = 0;

}