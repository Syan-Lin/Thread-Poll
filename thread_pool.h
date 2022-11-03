#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <future>
#include <functional>

namespace thread_pool {

template <typename Func>
class TaskQueue {
public:
    TaskQueue() = default;
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue(TaskQueue&&) = delete;
    ~TaskQueue() = default;
    TaskQueue& operator=(const TaskQueue&) = delete;
    TaskQueue& operator=(TaskQueue&&) = delete;

    bool empty() {
        std::lock_guard<std::mutex> lock(queue_lock_);
        return queue_.empty();
    }
    int size() {
        std::lock_guard<std::mutex> lock(queue_lock_);
        return queue_.size();
    }
    void clear() {
        std::lock_guard<std::mutex> lock(queue_lock_);
        while(!queue_.empty()){
            queue_.pop();
        }
    }
    void enqueue(Func& task) {
        std::lock_guard<std::mutex> lock(queue_lock_);
        queue_.emplace(task);
    }
    bool dequeue(Func& func) {
        std::lock_guard<std::mutex> lock(queue_lock_);
        if(!queue_.empty()) {
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

class ThreadPool {
    friend class Thread;

    class Thread {
        friend class ThreadPool;
    public:
        Thread(ThreadPool* pool) : thread_pool_(pool) {
            static int count = 0;
            thread_id_ = count++;
            thread_ = std::thread(std::ref(*this));
        }
        Thread(const Thread&) = delete;
        Thread(Thread&&) = delete;
        Thread& operator=(const Thread&) = delete;
        Thread& operator=(Thread&&) = delete;
        void shutdown() { is_shutdown_ = true; }
        int getId() const { return thread_id_; }
        void operator()() {
            std::function<void()> func;
            bool is_get = false;
            while(!is_shutdown_) {
                is_get = false;
                {
                    std::unique_lock<std::mutex> lock(thread_pool_->task_lock_);
                    thread_pool_->cv_.wait(lock, [this] {
                        if(this->is_shutdown_) return true;
                        return !this->thread_pool_->task_queue_.empty();
                    });
                    is_get = thread_pool_->task_queue_.dequeue(func);
                }
                if(is_get) {
                    func();
                }
            }
            is_over_ = true;
        }
    public:
        int thread_id_;
        bool is_shutdown_ = false;
        bool is_over_ = false;
        ThreadPool* thread_pool_;
        std::thread thread_;
    };

public:
    ThreadPool(size_t num = 6) {
        createThread(num);
    }
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;
    ~ThreadPool() { shutdown(); }

    template<typename Func, typename... Args>
    auto submit(Func&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        // 1. 包装一层 function，消去参数
        // <decltype(f(args...))()> 表示返回值为函数 f 的返回值的一个函数，但没有参数
        // 参数用 std::bind 将 args 绑定到 func 上，所以 func 里不需要指定参数
        std::function<decltype(f(args...))()> func = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);

        // 2. 用 packaged_task 包装函数，结果可以用 future 获得，使函数获得异步特性
        // packaged_task 不可复制，使用指针进行传递
        auto tast_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // 3. 再包装一层 function，使接口统一，可以放入队列
        std::function<void()> unified_func = [tast_ptr]() {
            (*tast_ptr)();
        };

        task_queue_.enqueue(unified_func);
        cv_.notify_one();
        return tast_ptr->get_future();
    }
    size_t size() const { return threads_.size(); }
    void setSize(size_t target_size) {
        if(target_size == 0 || target_size == threads_.size()) return;
        size_t cur_size = size();
        if(target_size < cur_size) {
            shutdownThread(cur_size - target_size);
        } else {
            createThread(target_size - cur_size);
        }
    }
    void shutdown() { shutdownThread(size()); }
    void cancel() { task_queue_.clear(); }
private:
    void shutdownThread(size_t num) {
        for(size_t i = 0; i < num; ++i) {
            {
                std::unique_lock<std::mutex> lock(task_lock_);
                threads_[i]->shutdown();
            }
            cv_.notify_all();
            if(threads_[i]->thread_.joinable()) {
                threads_[i]->thread_.join();
                delete threads_[i];
            }
        }
        threads_.erase(threads_.begin(), threads_.begin() + num);
    }
    void createThread(size_t num) {
        for(size_t i = 0; i < num; i++) {
            threads_.push_back(new Thread(this));
        }
    }
    TaskQueue<std::function<void()>> task_queue_;
    std::vector<Thread*> threads_;
    std::condition_variable cv_;
    std::mutex task_lock_;
};

} // namespace thread_pool