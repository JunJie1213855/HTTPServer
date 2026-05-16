#ifndef HTTPSERVER_CORE_IOBUFFER_H_
#define HTTPSERVER_CORE_IOBUFFER_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace http::core
{

// Replacement for muduo::net::Buffer. Layout matches muduo's: a prepend
// region followed by the readable region followed by writable space, all
// inside a single contiguous std::vector<char>. The HTTP parser walks
// peek()..peek()+readableBytes() and consumes via retrieve/retrieveUntil.
class IOBuffer
{
public:
    static constexpr size_t kCheapPrepend = 8;
    static constexpr size_t kInitialSize = 1024;

    explicit IOBuffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    const char* peek() const { return begin() + readerIndex_; }
    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    void hasWritten(size_t len)
    {
        assert(len <= writableBytes());
        writerIndex_ += len;
    }

    void unwrite(size_t len)
    {
        assert(len <= readableBytes());
        writerIndex_ -= len;
    }

    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveUntil(const char* end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(static_cast<size_t>(end - peek()));
    }

    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAsString(size_t len)
    {
        assert(len <= readableBytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::memcpy(beginWrite(), data, len);
        hasWritten(len);
    }

    void append(const char* str) { append(str, std::strlen(str)); }
    void append(const std::string& s) { append(s.data(), s.size()); }
    void append(std::string_view sv) { append(sv.data(), sv.size()); }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    // Linear search for CRLF in the readable region. Returns nullptr if not
    // found. Matches muduo::Buffer::findCRLF().
    const char* findCRLF() const
    {
        static constexpr char kCRLF[] = "\r\n";
        const char* found = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return found == beginWrite() ? nullptr : found;
    }

    const char* findCRLF(const char* start) const
    {
        assert(peek() <= start);
        assert(start <= beginWrite());
        static constexpr char kCRLF[] = "\r\n";
        const char* found = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
        return found == beginWrite() ? nullptr : found;
    }

private:
    char* begin() { return buffer_.data(); }
    const char* begin() const { return buffer_.data(); }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            const size_t readable = readableBytes();
            std::memmove(begin() + kCheapPrepend, begin() + readerIndex_, readable);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

} // namespace http::core

#endif
