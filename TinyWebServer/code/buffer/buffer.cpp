#include "buffer.h"

// vector<char>初始化，读写下标初始化
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {} 

// 可写的数量：buffer大小 - 写下标
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 可读的数量：写下标 - 读下标
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

// 可预留空间：已经读过的就没用了，等于读下标
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

// 读指针的位置
const char* Buffer::Peek() const {
    return &buffer_[readPos_];
}

// 确保可写的长度
void Buffer::EnsureWriteable(size_t len) {
    if(len > WritableBytes()) {
        MakeSpace_(len);
    }
    assert(len <= WritableBytes()); // assert是一个宏定义，如果 `len <= WritableBytes()` 为假，
                                    // 则代码会终止运行，并且会把源文件，错误的代码，以及行号，都输出来。
}

// 移动写下标，在Append中使用
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
}

// 读取len长度，移动读下标
void Buffer::Retrieve(size_t len) {
    readPos_ += len;
}

// 读取到end位置
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek()); // 长度：end指针 - 读指针
}

// 取出所有数据，buffer归零，读写下标归零,在别的函数中会用到
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size()); // 覆盖原本数据
    readPos_ = writePos_ = 0;
}

// 取出剩余可读的str
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 写指针的位置（安全）
const char* Buffer::BeginWriteConst() const {
    return &buffer_[writePos_];
}

// 写指针的位置
char* Buffer::BeginWrite() {
    return &buffer_[writePos_];
}

// 添加str到缓冲区
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);   // 确保可写的长度
    std::copy(str, str + len, BeginWrite());    // 将str放到写下标开始的地方
    HasWritten(len);    // 移动写下标
}

// 添加str到缓冲区：string类型
void Buffer::Append(const std::string& str) {
    Append(str.c_str(), str.size());
}

// 添加str到缓冲区：静态转换
void Append(const void* data, size_t len) {
    Append(static_cast<const char*>(data), len);
}

// 将buffer中的读下标的地方放到该buffer中的写下标位置 <=> 将buffer中的可读内容放到该buffer中的写下标位置
void Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

// 将fd的内容读到缓冲区，即writable的位置
ssize_t Buffer::ReadFd(int fd, int* Errno) {
    char buff[65535];   // 栈区：缓冲区
    struct iovec iov[2]; // iovec是一个结构体，用于描述一个数据缓冲区，通常与readv和writev系统调用一起使用，用于在一次系统调用中读取或写入多个缓冲区
    size_t writeable = WritableBytes(); // 先记录能写多少：可写的数量
    // 分散读， 保证数据全部读完
    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    ssize_t len = readv(fd, iov, 2); // readv和writev函数：允许单个系统调用读入或写出多个缓冲区
    if(len < 0) { // 读数据错误
        *Errno = errno;
    } else if(static_cast<size_t>(len) <= writeable) {   // 若len小于writable，说明写区可以容纳len
        writePos_ += len;   // 直接移动写下标
    } else {    
        writePos_ = buffer_.size(); // 写区写满了,下标移到最后
        Append(buff, static_cast<size_t>(len - writeable)); // 剩余的长度放入缓冲区
    }
    return len; // 返回：读数据的长度
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::WriteFd(int fd, int* Errno) {
    ssize_t len = write(fd, Peek(), ReadableBytes()); // fd，可读的位置，可读的数量
    if(len < 0) {
        *Errno = errno;
        return len;
    } 
    Retrieve(len); // 移动读下标
    return len;
}

// buffer开头
char* Buffer::BeginPtr_() {
    return &buffer_[0];
}

// buffer开头（安全）
const char* Buffer::BeginPtr_() const{
    return &buffer_[0];
}

// 扩展空间
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) { // 当 `可写的空间 + 预留的空间`的长度 < len，扩展现有buffer
        buffer_.resize(writePos_ + len + 1); // 扩展buffer空间：可写的位置 + buffer长度 + 1（字符串结束符\0）
    } else { // 当 `可写的空间 + 预留的空间`的长度 >= len，不扩展现有buffer，直接将可读位置放到起始位置
        size_t readable = ReadableBytes(); // 可读的数量
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_()); // 将可读位置放到起始位置
        readPos_ = 0;   // 更新可读位置
        writePos_ = readable;   // 更新可写位置
        assert(readable == ReadableBytes());
    }
}
