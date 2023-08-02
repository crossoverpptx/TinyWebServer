# ifndef BLOCKQUEUE_H
# define BLOCKQUEUE_H

#include <deque> // 双端队列容器
#include <condition_variable> // 条件变量：C++11标准同步原语，它可以在同一时间阻塞一个线程或者多个线程，直到其他线程改变了共享变量（条件）并通知。
#include <mutex> // 在使用条件变量时，通常需要与互斥锁（mutex）结合使用。
#include <sys/time.h> // Linux系统的日期时间头文件

using namespace std;

template<typename T>
class BlockQueue { // 阻塞队列
public:
    explicit BlockQueue(size_t maxsize = 1000); // explicit修饰的构造函数必须被显式调用
    ~BlockQueue();
    void clear();
    void Close();
    bool empty();
    bool full();
    void push_back(const T& item);
    void push_front(const T& item); 
    bool pop(T& item);  // 弹出的任务放入item
    bool pop(T& item, int timeout);  // 等待时间
    T front();
    T back();
    size_t capacity();
    size_t size();
    void flush();

private:
    deque<T> deq_;                      // 底层数据结构
    mutex mtx_;                         // 锁：互斥锁
    bool isClose_;                      // 关闭标志
    size_t capacity_;                   // 容量
    condition_variable condConsumer_;   // 消费者条件变量
    condition_variable condProducer_;   // 生产者条件变量
};

// 构造函数：阻塞队列初始化
template<typename T>
BlockQueue<T>::BlockQueue(size_t maxsize) : capacity_(maxsize) {
    assert(maxsize > 0);
    isClose_ = false;
}

// 析构函数：阻塞队列关闭
template<typename T>
BlockQueue<T>::~BlockQueue() {
    Close();
}

// 清空队列
template<typename T>
void BlockQueue<T>::clear() {
    lock_guard<mutex> locker(mtx_);
    deq_.clear();
}

// 关闭队列
template<typename T>
void BlockQueue<T>::Close() {
    // lock_guard<mutex> locker(mtx_); // 操控队列之前，都需要上锁
    // deq_.clear();                   // 清空队列
    clear();
    isClose_ = true;
    condConsumer_.notify_all(); // notify_all :将所有的线程中的wait都被唤醒。
    condProducer_.notify_all(); // 虽然是将所有处于睡眠的wait()唤醒，但还是只能由一个线程拿到互斥锁，所以看起来的效果与notify_one相同，但可以用于不同的工作场景。
}

// 判断队列是否为空
template<typename T>
bool BlockQueue<T>::empty() {
    lock_guard<mutex> locker(mtx_);
    return deq_.empty();
}

// 判断队列是否已满
template<typename T>
bool BlockQueue<T>::full() {
    lock_guard<mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

// 向队列末尾添加一个元素
template<typename T>
void BlockQueue<T>::push_back(const T& item) {
    // 注意，条件变量需要搭配unique_lock。unique_lock比lock_guard灵活很多（多出来很多用法），效率差一点，内存占用多一点。
    unique_lock<mutex> locker(mtx_);    
    while(deq_.size() >= capacity_) {   // 队列满了，需要等待
        condProducer_.wait(locker);     // 暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_back(item);               // 队列有空，向末尾添加一个元素
    condConsumer_.notify_one();         // 唤醒消费者
}

// 向队列开头添加一个元素
template<typename T>
void BlockQueue<T>::push_front(const T& item) {
    unique_lock<mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {   // 队列满了，需要等待
        condProducer_.wait(locker);     // 暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_front(item);              // 队列有空，向开头添加一个元素
    condConsumer_.notify_one();         // 唤醒消费者
}

// 删除队首元素
template<typename T>
bool BlockQueue<T>::pop(T& item) {
    unique_lock<mutex> locker(mtx_);
    while(deq_.empty()) {
        condConsumer_.wait(locker);     // 队列空了，需要等待
    }
    item = deq_.front();                // 获取队首元素
    deq_.pop_front();                   // 删除队首元素
    condProducer_.notify_one();         // 唤醒生产者
    return true;
}

// 带计时器的pop函数
template<typename T>
bool BlockQueue<T>::pop(T &item, int timeout) {
    unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout))
                == std::cv_status::timeout){ // 阻塞时间超过指定的timeout，直接返回false
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

// 返回队首元素
template<typename T>
T BlockQueue<T>::front() {
    lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

// 返回队尾元素
template<typename T>
T BlockQueue<T>::back() {
    lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

// 返回队列容量
template<typename T>
size_t BlockQueue<T>::capacity() {
    lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

// 返回队列当前大小
template<typename T>
size_t BlockQueue<T>::size() {
    lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

// 唤醒消费者
template<typename T>
void BlockQueue<T>::flush() {
    condConsumer_.notify_one();
}

# endif
