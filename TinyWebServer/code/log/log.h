#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>             // C++11标准库中的多线程库
#include <sys/time.h>
#include <cstring>
#include <stdarg.h>           // 可变参数头文件,允许函数里面定义非固定长度的可变参数：va_start、va_end
#include <cassert>
#include <sys/stat.h>         // unix/linux系统定义文件状态所在的伪标准头文件：mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    // 初始化日志实例（日志保存路径、日志文件后缀、阻塞队列最大容量）
    void init(int level, 
              const char* path = "./log", 
              const char* suffix =".log",
              int maxQueueCapacity = 1024);

    static Log* Instance();         // 单例模式：懒汉模式，这种方式不需要加锁和解锁操作
    static void FlushLogThread();   // 异步写日志公有方法，调用私有方法AsyncWrite
    
    void write(int level, const char *format, ...);  // 将输出内容按照标准格式整理
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_(); // 异步写日志方法

private:
    static const int LOG_PATH_LEN = 256;    // 日志文件最长文件名：文件夹
    static const int LOG_NAME_LEN = 256;    // 日志最长名字：文件
    static const int MAX_LINES = 50000;     // 日志文件内的最长日志条数

    const char* path_;          // 路径名
    const char* suffix_;        // 后缀名

    int MAX_LINES_;             // 最大日志行数

    int lineCount_;             // 日志行数记录
    int toDay_;                 // 按当天日期区分文件

    bool isOpen_;               
 
    Buffer buff_;       // 输出的内容，缓冲区
    int level_;         // 日志等级
    bool isAsync_;      // 是否开启异步日志

    FILE* fp_;                                          // 打开log的文件指针
    std::unique_ptr<BlockQueue<std::string>> deque_;    // 阻塞队列：指针
    std::unique_ptr<std::thread> writeThread_;          // 写线程的指针
    std::mutex mtx_;                                    // 同步日志必需的互斥量
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\    
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

// 四个宏定义，主要用于不同类型的日志输出，也是外部使用日志的接口。
// ...：表示可变参数
// __VA_ARGS__：就是将...的值复制到这里
// 前面加上 ## 的作用是：当可变参数的个数为0时，这里的 ##可以把把前面多余的","去掉，否则会编译出错。
// do {} while(0); 结构：辅助定义复杂的宏，避免引用的时候出错
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);    
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
