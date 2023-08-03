#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h> // 信号量（semaphore）是一种常被用于多线程或多进程场景中的同步机制，用于保证多线程或多进程对共享数据的读/写操作是顺序的
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();

    void Init(const char* host, uint16_t port,
              const char* user, const char* pwd, 
              const char* dbName, int connSize);

    MYSQL *GetConn();
    void FreeConn(MYSQL * conn);
    void ClosePool();
    int GetFreeConnCount();

private:
    SqlConnPool() = default;
    ~SqlConnPool() { ClosePool(); }

    int MAX_CONN_;

    std::queue<MYSQL *> connQue_;
    std::mutex mtx_;
    sem_t semId_;
};

/* 资源在对象构造初始化 资源在对象析构时释放 */
class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { 
            connpool_->FreeConn(sql_); 
        }
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif // SQLCONNPOOL_H
