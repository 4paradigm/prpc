#ifndef PARADIGM4_PICO_CORE_ARCHIVE_H
#define PARADIGM4_PICO_CORE_ARCHIVE_H

#include <string>
#include <cstring>
#include <vector>
#include <valarray>
#include <map>
#include <set>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <tuple>
#include <queue>
#include <type_traits>

#include <sparsehash/dense_hash_map>
#include "HashTable.h"
#include "PicoJsonNode.h"
#include "StringUtility.h"
#include "macro.h"
#include "ArenaAllocator.h"
#include "pico_log.h"
#include "pico_lexical_cast.h"
#include "throwable_shared_ptr.h"

namespace paradigm4 {
namespace pico {
namespace core {

class ArchiveBase {
public:
    PICO_DEPRECATED("")
    void read_raw(void*, size_t) {
        SLOG(FATAL) << "not implemented";
    }

    bool inner_read(void*, size_t) {
        SLOG(FATAL) << "not implemented";
        return false;
    }

    PICO_DEPRECATED("")
    void write_raw(const void*, size_t) {
        SLOG(FATAL) << "not implemented";
    }

    bool inner_write(void*, size_t) {
        SLOG(FATAL) << "not implemneted";
        return false;
    }

    void read_back(void*, size_t) {
        SLOG(FATAL) << "not implemented";
    }
};

/*!
 * \brief file operating class
 */
class FileArchive : public ArchiveBase {
public:
    FileArchive() = default;

    explicit FileArchive(FILE* file) {
        SCHECK(file);
        _file.reset(file, [](FILE*){});
    }

    explicit FileArchive(const shared_ptr<FILE>& file) {
        _file = file;
    }

    explicit FileArchive(shared_ptr<FILE>&& file) {
        _file = std::move(file);
    }

    explicit FileArchive(const std::string& file_name, const std::string& mode) {
        FILE* file = fopen(file_name.c_str(), mode.c_str());
        SCHECK(file) << "open " << file_name << " failed.";
        _file.reset(file, [](FILE* file) {fclose(file);});
    }

    FileArchive(const FileArchive& other) {
        _file = other._file;
    }

    FileArchive(FileArchive&& other) {
        _file = std::move(other._file);
    }

    ~FileArchive() {
        free(_buffer);
    }

    FileArchive& operator=(const FileArchive& other) {
        _file = other._file;
        return *this;
    }

    FileArchive& operator=(FileArchive&& other) {
        _file = std::move(other._file);
        return *this;
    }

    FILE* file() {
        return _file.get();
    }

    void reset() {
        _file.reset();
    }

    void reset(const shared_ptr<FILE>& file) {
        _file = file;
    }

    void reset(shared_ptr<FILE>&& file) {
        _file = std::move(file);
    }

    void reset(FILE* file) {
        _file.reset(file, [](FILE*) {});
    }

    void reset(const std::string& file_name, const std::string& mode) {
        FILE* file = fopen(file_name.c_str(), mode.c_str());
        SCHECK(file) << "open " << file_name << " failed.";
        _file.reset(file, [](FILE* file) {fclose(file);});
    }

    shared_ptr<FILE> release() {
        return shared_ptr<FILE>(std::move(_file));
    }

    PICO_DEPRECATED("")
    std::pair<char*, size_t> getdelim(char delim) {
        free(_buffer);
        size_t size = 0;
        ssize_t ret = ::getdelim(&_buffer, &size, delim, &*_file);
        if (ret == -1) {
            SCHECK(feof(&*_file));
            return {nullptr, 0};
        } else if (ret >= 1) {
            if (_buffer[ret - 1] == delim) {
                _buffer[ret - 1] = '\0';
                size = ret - 1;
            }
        }
        return {_buffer, size};
    }
    
    std::pair<char*, size_t> getdelim_uncheck(char delim) {
        free(_buffer);
        size_t size = 0;
        ssize_t ret = ::getdelim(&_buffer, &size, delim, &*_file);
        if (ret == -1) {
            return {nullptr, 0};
        } else if (ret >= 1) {
            if (_buffer[ret - 1] == delim) {
                _buffer[ret - 1] = '\0';
                size = ret - 1;
            }
        }
        return {_buffer, size};
    }

    PICO_DEPRECATED("")
    void read_raw(void* p, size_t len) {
        if (likely(len > 0)) {
            SCHECK(fread(p, sizeof(char), len, &*_file) == len) << "read failed";
        }
    }

    bool read_raw_uncheck(void* p, size_t len) {
        if (likely(len > 0)) {
            return fread(p, sizeof(char), len, &*_file) == len;
        }
        return true;
    }

    PICO_DEPRECATED("")
    void write_raw(const void* p, size_t len) {
        if (likely(len > 0)) {
            SCHECK(fwrite(p, sizeof(char), len, &*_file) == len) << "write failed";
        }
    }

    bool write_raw_uncheck(const void* p, size_t len) {
        if (likely(len > 0)) {
            return fwrite(p, sizeof(char), len, &*_file) == len;
        }
        return true;
    }

    void read_back(void* p, size_t len) {
        if (likely(len > 0)) {
            std::fpos_t pos;
            PSCHECK(fgetpos(&*_file, &pos) == 0);
            SCHECK(fseek(&*_file, -len, SEEK_END) == 0) << "seek failed";
            SCHECK(fread(p, sizeof(char), len, &*_file) == len) << "read failed";
            PSCHECK(fsetpos(&*_file, &pos) == 0);
        }
    }

private:
    shared_ptr<FILE> _file = nullptr;
    char* _buffer = nullptr;
};

class MemoryArchive : public ArchiveBase {
public:
    typedef PicoAllocator<char> allocator_type;
    typedef RpcAllocator<char> rpc_allocator_type;

    typedef std::function<void(void*)> deleter_type;

    deleter_type _deleter = [](void* ptr) {
        if (ptr) {
            SLOG(WARNING) << "never set deleter. address at" << ptr
                          << " may be leaked.";
        }
    };

    static rpc_allocator_type rpc_alloc() {
        static rpc_allocator_type ins;
        return ins;
    }

    static char* buf_malloc(size_t size, bool is_msg = false) {
        char* ret = nullptr;
        if (is_msg) {
            ret = rpc_alloc().allocate(size);
        } else {
            ret = allocator_type().allocate(size);
        }
        return ret;
    }

    static char* buf_realloc(char* buffer,
          size_t cur_size,
          size_t new_size,
          bool is_msg = false) {
        char* ret = nullptr;
        if (is_msg) {
            ret = rpc_alloc().allocate(new_size);
            std::memcpy(ret, buffer, std::min(cur_size, new_size));
            rpc_alloc().deallocate(buffer, 1);
        } else {
            ret = allocator_type().reallocate(buffer, new_size);
        }
        return ret;
    }

    static void buf_free(char* p, bool is_msg = false) {
        if (!p)
            return;
        if (is_msg) {
            rpc_alloc().deallocate(p, 1);
        } else {
            allocator_type().deallocate(p);
        }
    }

    MemoryArchive(bool is_msg = false) : _is_msg(is_msg) {
        set_default_deleter();
    }

    MemoryArchive(const MemoryArchive& other) {
        *this = other;
    }

    MemoryArchive(MemoryArchive&& other) {
        *this = std::move(other);
    }

    ~MemoryArchive() {
        reset();
    }

    // 返回一个Archive，没有内存的所有权
    MemoryArchive view() {
        MemoryArchive ret;
        ret._buffer = _buffer;
        ret._cursor = _cursor;
        ret._end = _end;
        ret._border = _border;
        ret._is_msg = _is_msg;
        ret._is_default_malloc = false;
        ret._deleter = [](void*) {};
        return ret;
    }

    void set_default_deleter() {
        _is_default_malloc = true;
        bool is_msg = _is_msg;
        _deleter = [is_msg](void* ptr) {
            buf_free((char*)ptr, is_msg);
        };
    }

    /*!
     * \brief assign other to self, reset other when it is not const
     */
    MemoryArchive& operator=(const MemoryArchive& other)  {
        if (&other == this) {
            return *this;
        }
        reset();
        size_t capacity = other.capacity();
        _is_msg = other._is_msg;
        if (capacity == 0) {
            return *this;
        }
        _buffer = buf_malloc(capacity, _is_msg);
        set_default_deleter();
        _cursor = _buffer + (other._cursor - other._buffer);
        _end = _buffer + (other._end - other._buffer);
        memcpy(_buffer, other._buffer, size_t(_end - _buffer));
        _border = _buffer + capacity;
        return *this;
    }

    MemoryArchive& operator=(MemoryArchive&& other) {
        if (&other == this) {
            return *this;
        }
        reset();
        _buffer = other._buffer;
        _cursor = other._cursor;
        _end = other._end;
        _border = other._border;
        _is_msg = other._is_msg;
        _is_default_malloc = other._is_default_malloc;
        _deleter = other._deleter;
        other.release();
        return *this;
    }

    char* buffer() {
        return _buffer;
    }

    const char* buffer() const {
        return _buffer;
    }

    /*!
     * \brief like queue is_empty
     */
    bool is_exhausted() const {
        return _end == _cursor;
    }

    /*!
     * \brief use [buffer, buffer + length) for this archive,
     * then buffer's owner is archive, so DO NOT delete[] buffer!
     */
    void set_read_buffer(char* buffer, size_t length, deleter_type deleter = [](void*) {}) {
        set_buffer(buffer, length, length, deleter);
    }

    /*!
     * \brief use [buffer, buffer + capacity) for this archive,
     * then buffer's owner is archive, so DO NOT delete[] buffer!
     */
    void set_write_buffer(char* buffer, size_t capacity, deleter_type deleter = [](void*) {}) {
        set_buffer(buffer, 0, capacity, deleter);
    }

    /*!
     * \brief use [buffer, buffer + capacity) for this archive,
     * [buffer, buffer + length) is read avaliable 
     * use the deleter to own the buffer, defualt is not own
     */
    void set_buffer(char* buffer, size_t length, size_t capacity, deleter_type deleter = [](void*) {}) {
        SCHECK(buffer != nullptr || capacity == 0) << "buffer == nullptr && capacity is " << capacity;
        SCHECK(length <= capacity) << "length larger than capacity";
        reset();
        _deleter = deleter;
        _is_default_malloc = false;
        _buffer = buffer;
        _cursor = _buffer;
        _end = _buffer + length;
        _border = _buffer + capacity;
    }

    /*!
     * \brief assign _buffer with given [buffer, buffer+length)
     * is O(length)
     */
    void assign(const char* buffer, size_t length) {
        SCHECK(buffer != nullptr);
        if (_buffer == nullptr || length > capacity()) {
            reset();
            _buffer = buf_malloc(length, _is_msg);
            set_default_deleter();
            _border = _buffer + length;
        }
        memcpy(_buffer, buffer, length);
        _cursor = _buffer;
        _end = _buffer + length;
    }

    /*!
     * \brief move _cursor to begin of _buffer
     */
    void rewind() {
        _cursor = _buffer;
    }

    char* cursor() {
        return _cursor;
    }

    void set_cursor(char* cursor) {
        SCHECK(cursor >= _buffer && cursor <= _end);
        _cursor = cursor;
    }

    void advance_cursor(size_t offset) {
        SCHECK(offset <= size_t(_end - _cursor));
        _cursor += offset;
    }

    char* end() {
        return _end;
    }

    bool empty() const {
        return _cursor == _end;
    }

    void set_end(char* end) {
        SCHECK(end >= _cursor && end <= _border);
        _end = end;
    }

    void advance_end(size_t offset) {
        SCHECK(offset <= size_t(_border - _end));
        _end += offset;
    }

    char* border() {
        return _border;
    }

    /*!
     * \breif return label of current cursor
     */
    size_t position() const {
        return _cursor - _buffer;
    }

    /*!
     * \brief _end - _buffer, NOT _end - _cursor
     */
    size_t length() const {
        return _end - _buffer;
    }

    size_t readable_length() const {
        return _end - _cursor;
    }

    size_t capacity() const {
        return _border - _buffer;
    }

    /*!
     * \brief move _cursor and _end to begin of _buffer
     */
    void clear() {
        _cursor = _buffer;
        _end = _buffer;
    }

    /*!
     * \brief
     */
    void reset() {
        _deleter(_buffer);
        _buffer = nullptr;
        _cursor = nullptr;
        _end = nullptr;
        _border = nullptr;
    }

    /*!
     * \brief release buffer ownership.
     */
    char* release() {
        char* buf = _buffer;
        _deleter = [](void*) {};
        _buffer = nullptr;
        _cursor = nullptr;
        _end = nullptr;
        _border = nullptr;
        return buf;
    }

    /*!
     * \brief release buffer ownership.
     * \return the buffer, PLEASE delete[] buffer outside.
     */
    std::shared_ptr<char> release_shared() {
        auto ret = std::shared_ptr<char>(_buffer, _deleter);
        release();
        return ret;
    }

    /*!
     * \brief release buffer ownership.
     * \return the buffer, PLEASE delete[] buffer outside.
     */
    auto release_unique() {
        auto ret = std::unique_ptr<char, decltype(_deleter)>(_buffer, _deleter);
        release();
        return ret;
    }

    /*!
     * \brief resize _buffer when origin size smaller than newcap
     */
    void reserve(size_t newcap) {
        if (newcap > capacity()) {
            char* buffer = nullptr;
            newcap = (newcap + 64 - 1) / 64 * 64;
            if (_is_default_malloc) {
                buffer = buf_realloc(_buffer, length(), newcap, _is_msg);
            } else {
                buffer = buf_malloc(newcap, _is_msg);
                if (length() > 0) {
                    memcpy(buffer, _buffer, length());
                }
                reset();
                set_default_deleter();
            }
            _cursor = buffer + (_cursor - _buffer);
            _end = buffer + (_end - _buffer);
            _border = buffer + newcap;
            _buffer = buffer;
        }
    }

    /*!
     * \brief move _end to _buffer+size
     */
    void resize(size_t size) {
        if (unlikely(size > capacity())) {
            reserve(std::max(capacity() * 2, size));
        }

        _end = _buffer + size;
        _cursor = std::min(_cursor, _end);
    }

    /*!
     * \brief check if size to read is larger than _end-_buffer
     */
    void prepare_read(size_t size) {
        if (unlikely(size > size_t(_end - _cursor))) {
            SCHECK(size <= size_t(_end - _cursor)) << "prepared size is more than its data size";
        }
    }

    /*!
     * \brief check if size to write is larger than _end-_buffer
     */
    void prepare_write(size_t size) {
        if (unlikely(size > size_t(_border - _end))) {
            reserve(std::max(capacity() * 2, length() + size));
        }
    }

    /*!
     * \brief read one raw of length len from _buffer to p,
     *  move _cursor len ahead
     */
    void read_raw(void* p, size_t len) {
        if (len > 0) {
            prepare_read(len);
            memcpy(p, cursor(), len);
            advance_cursor(len);
        }
    }

    bool read_raw_uncheck(void* p, size_t len) {
        if (likely(len > 0)) {
            if (unlikely(is_exhausted()))
                return false;
            read_raw(p, len);
        }
        return true;
    }


    /*!
     * \brief write one raw of length len from p to _buffer, 
     *  move _end len ahead
     */
    void write_raw(const void* p, size_t len) {
        if (len > 0) {
            prepare_write(len);
            memcpy(end(), p, len);
            advance_end(len);
        }
    }

    bool write_raw_uncheck(const void* p, size_t len) {
        write_raw(p, len);
        return true;
    }


    /*!
     * \brief read length len from _buffer back,
     *  move _end len back
     */
    void read_back(void* p, size_t len) {
        if (len > 0) {
            SCHECK(len <= size_t(_end - _cursor));
            memcpy(p, _end - len, len);
            _end -= len;
        }
    }

    bool _is_msg = false;
private:
    char* _buffer = nullptr;
    char* _cursor = nullptr;
    char* _end = nullptr;
    char* _border = nullptr;
    bool _is_default_malloc = false;
};

class BinaryArchiveType {
};

class TextArchiveType {
};

class BinaryFileArchiveType {
};

class TextFileArchiveType {
};

template<class Type>
class Archive : public ArchiveBase {
};

/*!
 * \brief add read_pod, write_pod, to deal with val in binary form
 */
template<>
class Archive<BinaryArchiveType> : public MemoryArchive {
public:

    Archive<BinaryArchiveType>(bool is_msg = false) : MemoryArchive(is_msg) {}
    Archive<BinaryArchiveType>(MemoryArchive&& o) : MemoryArchive(std::move(o)) {}
    Archive<BinaryArchiveType>(const MemoryArchive& o) : MemoryArchive(o) {}

    template<class T>
    void write_pod(const T& val) {
        write_raw(&val, sizeof(T));
    }

    template<class T>
    bool write_pod_uncheck(const T& val) {
        write_raw(&val, sizeof(T));
        return true;
    }

    template<class T>
    void read_pod(T& val) {
        read_raw(&val, sizeof(T));
    }

    template<class T>
    bool read_pod_uncheck(T& val) {
        if (unlikely(is_exhausted())) {
            return false;
        }
        read_pod(val);
        return true;
    }

    template<class T>
    T get() {
        T val;
        *this >> val;
        return val;
    }
};

/*!
 * \brief add read_arithmetic, write_arithmetic, to deal with val in text form
 */
template<>
class Archive<TextArchiveType> : public MemoryArchive {
public:

    Archive<TextArchiveType>(bool is_msg = false) : MemoryArchive(is_msg) {}
    Archive<TextArchiveType>(MemoryArchive&& o) : MemoryArchive(std::move(o)) {}
    Archive<TextArchiveType>(const MemoryArchive& o) : MemoryArchive(o) {}

    template<class T>
    void write_arithmetic(const T& val) {
        //typedef std::array<char, 50> buf_t;
        typedef std::string buf_t;
        //buf_t buffer = boost::lexical_cast<buf_t>(val);
        buf_t buffer = StringUtility::to_string(val);
        prepare_write(buffer.size() + 1);
        char* p = &buffer[0];
        while (*p != '\0') {
            *end() = *p;
            ++p;
            advance_end(1);
        }
        *end() = ' ';
        advance_end(1);
    }

    template<class T>
    bool write_arithmetic_uncheck(const T& val) {
        write_arithmetic(val);
        return true;
    }

    void write_arithmetic(const char& c) {
        prepare_write(1);
        *end() = c;
        advance_end(1);
    }

    void write_arithmetic(const int8_t& val) {
        write_arithmetic(static_cast<int32_t>(val));
    }

    void write_arithmetic(const uint8_t& val) {
        write_arithmetic(static_cast<uint32_t>(val));
    }

    template<class T>
    void read_arithmetic(T& val) {
        char* start = cursor();
        while (start < end() && *start == ' ') {
            ++start;
            advance_cursor(1);
        }

        char* finish = cursor();
        while (finish < end() && *finish != ' ') {
            ++finish;
            advance_cursor(1);
        }
        SCHECK(finish > start);
        SCHECK(finish != end());
        advance_cursor(1);
        val = pico_lexical_cast<T>(start, finish - start);
    }

    template<class T>
    bool read_arithmetic_uncheck(T& val) {
        if (unlikely(is_exhausted())) {
            return false;
        }
        read_arithmetic(val);
        return true;
    }

    void read_arithmetic(int8_t& val) {
        int32_t t;
        read_arithmetic(t);
        val = t;
    }

    void read_arithmetic(uint8_t& val) {
        uint32_t t;
        read_arithmetic(t);
        val = t;
    }

    void read_arithmetic(char& c) {
        c = *cursor();
        advance_cursor(1);
    }

    template<class T>
    T get() {
        T val;
        *this >> val;
        return val;
    }
};

/*!
 * \brief class to deal with binary file form, but read and write func
 *  just put val in _buffer
 */
template<>
class Archive<BinaryFileArchiveType> : public FileArchive {
public:
    using FileArchive::FileArchive;

    template<class T>
    PICO_DEPRECATED("")
    void write_pod(const T& val) {
        write_raw(&val, sizeof(T));
    }

    template<class T>
    bool write_pod_uncheck(const T& val) {
        return write_raw_uncheck(&val, sizeof(T));
    }

    template<class T>
    PICO_DEPRECATED("")
    void read_pod(T& val) {
        read_raw(&val, sizeof(T));
    }

    template<class T>
    bool read_pod_uncheck(T& val) {
        return read_raw_uncheck(&val, sizeof(T));
    }

    template<class T>
    T get() {
        T val;
        *this >> val;
        return val;
    };
};

/*!
 * \brief class to deal with binary file form, but read and write func
 *  just put val in _buffer, add special case char to speed up
 */
template<>
class Archive<TextFileArchiveType> : public FileArchive {
public:
    using FileArchive::FileArchive;

    template<class T>
    PICO_DEPRECATED("")
    void write_arithmetic(const T& val) {
        //typedef std::array<char, 50> buf_t;
        typedef std::string buf_t;
        //buf_t buffer = boost::lexical_cast<buf_t>(val);
        buf_t buffer = StringUtility::to_string(val);
        write_raw(&buffer[0], std::strlen(&buffer[0]));
        write_raw(" ", sizeof(char));
    }

    template<class T>
    bool write_arithmetic_uncheck(const T& val) {
        //typedef std::array<char, 50> buf_t;
        typedef std::string buf_t;
        //buf_t buffer = boost::lexical_cast<buf_t>(val);
        buf_t buffer = StringUtility::to_string(val);
        if (!write_raw_uncheck(&buffer[0], std::strlen(&buffer[0])))
            return false;
        if (!write_raw_uncheck(" ", sizeof(char)))
            return false;
        return true;
    }

    PICO_DEPRECATED("")
    void write_arithmetic(const char& c) {
        write_raw(&c, sizeof(char));
    }

    bool write_arithmetic_uncheck(const char& c) {
        return write_raw_uncheck(&c, sizeof(char));
    }

    PICO_DEPRECATED("")
    void write_arithmetic(const int8_t& val) {
        write_arithmetic(static_cast<int32_t>(val));
    }

    bool write_arithmetic_uncheck(const int8_t& val) {
        return write_arithmetic_uncheck(static_cast<int32_t>(val));
    }

    PICO_DEPRECATED("")
    void write_arithmetic(const uint8_t& val) {
        write_arithmetic(static_cast<int32_t>(val));
    }

    bool write_arithmetic_uncheck(const uint8_t& val) {
        return write_arithmetic_uncheck(static_cast<int32_t>(val));
    }

    template<class T>
    PICO_DEPRECATED("")
    void read_arithmetic(T& val) {
        std::pair<char*, size_t> p = getdelim(' ');
        val = pico_lexical_cast<T>(p.first, p.second);
    }

    void read_arithmetic(int8_t& val) {
        int32_t t;
        read_arithmetic(t);
        val = t;
    }

    void read_arithmetic(uint8_t& val) {
        int32_t t;
        read_arithmetic(t);
        val = t;
    }

    template<class T>
    bool read_arithmetic_uncheck(T& val) {
        std::pair<char*, size_t> p = getdelim_uncheck(' ');
        if (unlikely(p.first == nullptr)) {
            return false;
        }
        val = pico_lexical_cast<T>(p.first, p.second);
        return true;
    }

    bool read_arithmetic_uncheck(int8_t& val) {
        int32_t t;
        if (!read_arithmetic_uncheck(t))
            return false;
        val = t;
        return true;
    }

    bool read_arithmetic_uncheck(uint8_t& val) {
        int32_t t;
        if (!read_arithmetic_uncheck(t))
            return false;
        val = t;
        return true;
    }

    PICO_DEPRECATED("")
    void read_arithmetic(char& c) {
        read_raw(&c, sizeof(char));
    }

    bool read_arithmetic_uncheck(char& c) {
        return read_raw_uncheck(&c, sizeof(char));
    }

    template<class T>
    T get() {
        T val;
        *this >> val;
        return val;
    }
};

typedef Archive<BinaryArchiveType> BinaryArchive;
typedef Archive<TextArchiveType> TextArchive;
typedef Archive<BinaryFileArchiveType> BinaryFileArchive;
typedef Archive<TextFileArchiveType> TextFileArchive;

/*!
 * \brief define pico_serialize and pico_deserialize, used to serialize different type of value
 */
template<class AR>
struct ArchiveSerializer {
    explicit ArchiveSerializer(Archive<AR>& ar) : ar(ar) {}

    bool serizlize() {
        return true;
    }

    template<class T>
    bool serizlize(const T& x) {
        return pico_serialize(ar, x);
    }

    template<class T, class... Types>
    bool serizlize(const T& x, const Types&... types) {
        if (!serizlize(x))
            return false;
        return serizlize(types...);
    }

    template<class T>
    ArchiveSerializer& operator,(const T& x) {
        ar << x;
        return *this;
    }

    Archive<AR>& ar;
};

template<class AR>
struct ArchiveDeserializer {
    explicit ArchiveDeserializer(Archive<AR>& ar) : ar(ar) {}

    bool deserialize() {
        return true;
    }

    template<class T>
    bool deserialize(T& x) {
        return pico_deserialize(ar, x);
    }

    template<class T, class... Types>
    bool deserialize(T& x, Types&... types) {
        if (!deserialize(x))
            return false;
        return deserialize(types...);
    }

    template<class T>
    ArchiveDeserializer& operator,(T& x) {
        ar >> x;
        return *this;
    }

    Archive<AR>& ar;
};

template<class AR, class T,
    class = decltype(pico_serialize(std::declval<Archive<AR>&>(), std::declval<const T&>()))>
Archive<AR>& operator<<(Archive<AR>& ar, const T& x) {
    SCHECK(pico_serialize(ar, x));
    return ar;
}

template<class AR, class T,
    class = decltype(pico_deserialize(std::declval<Archive<AR>&>(), std::declval<T&>()))>
Archive<AR>& operator>>(Archive<AR>& ar, T& x) {
    SCHECK(pico_deserialize(ar, x));
    return ar;
}

/*!
 * \brief enable class to use archive serializer
 */
#define PICO_SERIALIZATION(FIELDS...) \
    template<class AR>\
    bool _archive_serialize_internal_(paradigm4::pico::core::Archive<AR>& _ar_) const { \
        paradigm4::pico::core::ArchiveSerializer<AR> _serializer_(_ar_); \
        return _serializer_.serizlize(FIELDS); \
    } \
    template<class AR>\
    bool _archive_deserialize_internal_(paradigm4::pico::core::Archive<AR>& _ar_) { \
        paradigm4::pico::core::ArchiveDeserializer<AR> _deserializer_(_ar_); \
        return _deserializer_.deserialize(FIELDS); \
    } \
    friend struct ::paradigm4::pico::core::serialize_helper;

#ifndef PICO_SERIALIZED_SIZE
#define PICO_SERIALIZED_SIZE(FIELDS...) \
    size_t _serialized_size_internal_()const {\
        paradigm4::pico::core::SerializedSize _serialized_size_; \
        return _serialized_size_.serialized_size(FIELDS);\
    }
#endif

#ifndef PICO_ENUM_SERIALIZATION
#define PICO_ENUM_SERIALIZATION(TYPE, INTTYPE) \
    template<class AR> \
    bool pico_deserialize(AR& ar, TYPE& val) { \
        INTTYPE t; \
        if (!pico_deserialize(ar, t)) \
            return false; \
        val = static_cast<TYPE>(t); \
        return true; \
    } \
    template<class AR> \
    bool pico_serialize(AR& ar, const TYPE& val) { \
        if (!pico_serialize(ar, static_cast<INTTYPE>(val))) \
            return false; \
        return true; \
    }
#endif

template<class AR, typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename = void>
bool pico_deserialize(AR& ar, T& val) {
    int t;
    if (!pico_deserialize(ar, t))
        return false;
    val = static_cast<T>(t);
    return true;
}

template<class AR, typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename = void>
bool pico_serialize(AR& ar, const T& val) {
    if (!pico_serialize(ar, static_cast<int>(val)))
        return false;
    return true;
}


struct SerializableObject : public Object {
    virtual bool _binary_archive_serialize_internal_(BinaryArchive&) const {
        return true;
    }
    virtual bool _binary_archive_deserialize_internal_(BinaryArchive&) {
        return true;
    }
};

#ifndef PICO_PS_SERIALIZATION
#define PICO_PS_SERIALIZATION(TYPE, FIELDS...)                                                \
    PICO_SERIALIZATION(FIELDS)                                                          \
    virtual bool _binary_archive_serialize_internal_(paradigm4::pico::core::BinaryArchive& ar)      \
          const override {                                                                    \
        return _archive_serialize_internal_(ar);                                              \
    }                                                                                         \
    virtual bool _binary_archive_deserialize_internal_(paradigm4::pico::core::BinaryArchive& ar)    \
          override {                                                                          \
        return _archive_deserialize_internal_(ar);                                            \
    }
#endif

struct serialize_helper {
public:
    template <class AR, class T,
        class = decltype(std::declval<T>()
            .template _archive_serialize_internal_(std::declval<AR&>()))>
    static std::true_type pico_has_member_func__archive_serialize_internal__test(AR&, const T&);
    static std::false_type pico_has_member_func__archive_serialize_internal__test(...);
    template <class AR, class T>
    using can_serial = decltype(pico_has_member_func__archive_serialize_internal__test(std::declval<AR&>(), std::declval<T>()));

    template <class AR, class T,
        class = decltype(std::declval<T>()
            .template _archive_deserialize_internal_(std::declval<AR&>()))>
    static std::true_type pico_has_member_func__archive_deserialize_internal__test(AR&, T&);
    static std::false_type pico_has_member_func__archive_deserialize_internal__test(...);
    template <class AR, class T>
    using can_deserial = decltype(pico_has_member_func__archive_deserialize_internal__test(std::declval<AR&>(), std::declval<T&>()));

    template <class T, class = decltype(std::declval<T>()._serialized_size_internal_())>
    static std::true_type pico_has_member_func__serialized_size_internal__test(T&);
    static std::false_type pico_has_member_func__serialized_size_internal__test(...);
    template <class T>
    using can_serialized_size = decltype(pico_has_member_func__serialized_size_internal__test(std::declval<T&>()));

    template<typename AR, typename T>
    static inline bool serialize(AR& ar, const T& x) {
        return x._archive_serialize_internal_(ar);
    }

    template<typename AR, typename T>
    static inline bool deserialize(AR& ar, T& x) {
        return x._archive_deserialize_internal_(ar);
    }

    template<class T>
    static inline size_t serialized_size(T& x) {
        return x._serialized_size_internal_();
    }
};

template<typename AR, typename T, typename=std::enable_if_t<serialize_helper::can_serial<AR, T>::value>>
inline bool pico_serialize(AR& ar, const T& x) {
    return serialize_helper::serialize(ar,x);
}

template<typename AR, typename T, typename=std::enable_if_t<serialize_helper::can_deserial<AR, T>::value>>
inline bool pico_deserialize(AR& ar, T& x) {
    return serialize_helper::deserialize(ar,x);
}


//大约估计，可以不准确，控制发送消息大小用。
template <class T>
std::enable_if_t<serialize_helper::can_serialized_size<T>::value, size_t>
pico_serialized_size(const T& value) {
    return serialize_helper::serialized_size(value);
}

template <class T>
std::enable_if_t<!serialize_helper::can_serialized_size<T>::value &&
std::is_trivially_copyable<T>::value, size_t>
pico_serialized_size(const T&) {
    return sizeof(T);
}

inline size_t pico_serialized_size(const std::string& value) {
    return sizeof(size_t) + value.size() * sizeof(char) + 1;
}

template <class T>
std::enable_if_t<std::is_trivially_copyable<T>::value, size_t>
pico_serialized_size(const std::vector<T>& value) {
    return sizeof(size_t) + value.size() * sizeof(T);
}

template <class T>
std::enable_if_t<std::is_trivially_copyable<T>::value, size_t>
pico_serialized_size(const std::valarray<T>& value) {
    return sizeof(size_t) + value.size() * sizeof(T);
}

template<class T1, class T2>
auto pico_serialized_size(const std::pair<T1, T2>& value) {
    return pico_serialized_size(value.first) + pico_serialized_size(value.second);
}

struct SerializedSize {
    size_t serialized_size() {
        return 0;
    }

    template<class T>
    size_t serialized_size(const T& x) {
        return pico_serialized_size(x);
    }

    template<class T, class... Types>
    size_t serialized_size(T& x, Types&... types) {
        return pico_serialized_size(x) + serialized_size(types...);
    }
};

/*!
 * \brief define serializer of base type
 */
#define PICO_DEF(T) \
    inline bool pico_serialize(BinaryArchive& ar, const T& x) { \
        return ar.write_pod_uncheck(x); \
    } \
    inline bool pico_deserialize(BinaryArchive& ar, T& x) { \
        return ar.read_pod_uncheck(x); \
    } \
    inline bool pico_serialize(TextArchive& ar, const T& x) { \
        return ar.write_arithmetic_uncheck(x); \
    } \
    inline bool pico_deserialize(TextArchive& ar, T& x) { \
        return ar.read_arithmetic_uncheck(x); \
    } \
    inline bool pico_serialize(BinaryFileArchive& ar, const T& x) { \
        return ar.write_pod_uncheck(x); \
    } \
    inline bool pico_deserialize(BinaryFileArchive& ar, T& x) { \
        return ar.read_pod_uncheck(x); \
    } \
    inline bool pico_serialize(TextFileArchive& ar, const T& x) { \
        return ar.write_arithmetic_uncheck(x); \
    } \
    inline bool pico_deserialize(TextFileArchive& ar, T& x) { \
        return ar.read_arithmetic_uncheck(x); \
    }
//    PICO_DEF(int8_t) PICO_DEF(uint8_t) PICO_DEF(int16_t) PICO_DEF(uint16_t)
//    PICO_DEF(int32_t) PICO_DEF(uint32_t) PICO_DEF(int64_t) PICO_DEF(uint64_t)
//    PICO_DEF(float) PICO_DEF(double) PICO_DEF(long double) PICO_DEF(bool) PICO_DEF(char)
#undef PICO_DEF

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_serialize(BinaryArchive& ar, const T& x) {
    return ar.write_pod_uncheck(x);
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_deserialize(BinaryArchive& ar, T& x) {
    return ar.read_pod_uncheck(x);
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_serialize(TextArchive& ar, const T& x) {
    return ar.write_arithmetic_uncheck(x);
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_deserialize(TextArchive& ar, T& x) {
    return ar.read_arithmetic_uncheck(x);
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_serialize(BinaryFileArchive& ar, const T& x) {
    return ar.write_pod_uncheck(x);
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_deserialize(BinaryFileArchive& ar, T& x) {
    return ar.read_pod_uncheck(x);
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_serialize(TextFileArchive& ar, const T& x) {
    return ar.write_arithmetic_uncheck(x);
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
inline bool pico_deserialize(TextFileArchive& ar, T& x) {
    return ar.read_arithmetic_uncheck(x);
}

template<class AR>
inline bool pico_serialize(Archive<AR>& ar, const Archive<AR>& in) {
    if (!pico_serialize(ar, (size_t)in.length()))
        return false;
    return ar.write_raw_uncheck(in.buffer(), in.length());
}

template<class AR>
inline bool pico_deserialize(Archive<AR>& ar, Archive<AR>& out) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    out.resize(size);
    out.set_cursor(out.buffer());
    return ar.read_raw_uncheck(out.buffer(), out.length());
}

template<class AR, class T>
std::enable_if_t<(std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value)
        && std::is_trivially_copyable<T>::value, 
bool> pico_serialize(Archive<AR>& ar, const std::vector<T>& vect) {
    if (!pico_serialize(ar, vect.size()))
        return false;
    return ar.write_raw_uncheck(vect.data(), sizeof(T) * vect.size());
}

template<class AR, class T>
std::enable_if_t<(std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value)
        || !std::is_trivially_copyable<T>::value, 
bool> pico_serialize(Archive<AR>& ar, const std::vector<T>& vect) {
    if (!pico_serialize(ar, vect.size()))
        return false;
    for (const auto& val : vect) {
        if (!pico_serialize(ar, val))
            return false;
    }
    return true;
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value)
        && std::is_trivially_copyable<T>::value, 
bool> pico_serialize(Archive<AR>& ar, const std::array<T, N>& vect) {
    return ar.write_raw_uncheck(vect.data(), sizeof(T) * vect.size());
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value)
        || !std::is_trivially_copyable<T>::value, 
bool> pico_serialize(Archive<AR>& ar, const std::array<T, N>& vect) {
    for (const auto& val : vect) {
        if (!pico_serialize(ar, val))
            return false;
    }
    return true;
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value)
        && std::is_trivially_copyable<T>::value, 
bool> pico_serialize(Archive<AR>& ar, const T (&arr)[N]) {
    if (!pico_serialize(ar, N))
        return false;
    return ar.write_raw_uncheck(arr, sizeof(T) * N);
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value)
        || !std::is_trivially_copyable<T>::value, 
bool> pico_serialize(Archive<AR>& ar, const T (&arr)[N]) {
    if (!pico_serialize(ar, N))
        return false;
    for (size_t i = 0; i < N; ++i) {
        if (!pico_serialize(ar, arr[i]))
            return false;
    }
    return true;
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value) 
        && std::is_trivially_copyable<T>::value, 
bool> pico_deserialize(Archive<AR>& ar, T (&arr)[N]) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    if (size != N)
        return false;
    return ar.read_raw_uncheck(arr, sizeof(T) * N);
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value) 
    || !std::is_trivially_copyable<T>::value, 
bool> pico_deserialize(Archive<AR>& ar, T (&arr)[N]) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    if (size != N)
        return false;
    for (size_t i = 0; i < N; ++i) {
        if (!pico_deserialize(ar, arr[i]))
            return false;
    }
    return true;
}

template<class AR, class T>
bool pico_serialize(Archive<AR>& ar, const std::deque<T>& vect) {
    if (!pico_serialize(ar, vect.size()))
        return false;
    for (const auto& val : vect) {
        if (!pico_serialize(ar, val))
            return false;
    }
    return true;
}

template<class AR, class T>
bool pico_deserialize(Archive<AR>& ar, std::deque<T>& vect) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    vect.resize(size);
    for (auto& val : vect) {
        if (!pico_deserialize(ar, val))
            return false;
    }
    return true;
}

template<class AR, class T>
std::enable_if_t<(std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value) 
    && std::is_trivially_copyable<T>::value, 
bool> pico_deserialize(Archive<AR>& ar, std::vector<T>& vect) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    vect.resize(size);
    return ar.read_raw_uncheck(vect.data(), sizeof(T) * vect.size());
}

template<class AR, class T>
std::enable_if_t<(std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value) 
    || !std::is_trivially_copyable<T>::value, 
bool> pico_deserialize(Archive<AR>& ar, std::vector<T>& vect) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    vect.resize(size);
    for (auto& val : vect) {
        if (!pico_deserialize(ar, val))
            return false;
    }
    return true;
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value) 
    && std::is_trivially_copyable<T>::value, 
bool> pico_deserialize(Archive<AR>& ar, std::array<T, N>& vect) {
    return ar.read_raw_uncheck(vect.data(), sizeof(T) * vect.size());
}

template<class AR, class T, size_t N>
std::enable_if_t<(std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value) 
    || !std::is_trivially_copyable<T>::value, 
bool> pico_deserialize(Archive<AR>& ar, std::array<T, N>& vect) {
    for (auto& val : vect) {
        if (!pico_deserialize(ar, val))
            return false;
    }
    return true;
}

template<class AR>
std::enable_if_t<std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value, 
bool> pico_deserialize(Archive<AR>& ar, std::vector<bool>& vect) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    vect.resize(size);
    for (size_t i = 0; i < vect.size(); ++i) {
        bool v;
        if (!pico_deserialize(ar, v))
            return false;
        vect[i] = v;
    }
    return true;
}

template<class AR>
std::enable_if_t<std::is_same<AR, TextArchiveType>::value || std::is_same<AR, TextFileArchiveType>::value, 
bool> pico_serialize(Archive<AR>& ar, const std::vector<bool>& vect) {
    if (!pico_serialize(ar, vect.size()))
        return false;
    for (const auto& val : vect) {
        if (!pico_serialize(ar, val))
            return false;
    }
    return true;
}

template<class AR> 
std::enable_if_t<std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value, 
bool> pico_deserialize(Archive<AR>& ar, std::vector<bool>& vect) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    vect.resize(size);
    uint64_t bits = 0;
    int offset = 64;
    for (size_t i = 0; i < vect.size(); ++i) {
        if (offset == 64) {
            if (!pico_deserialize(ar, bits))
                return false;
            offset = 0;
        }
        vect[i] = bits & (1llu << offset);
        ++offset;
    }
    return true; 
}

template<class AR>
std::enable_if_t<(std::is_same<AR, BinaryArchiveType>::value || std::is_same<AR, BinaryFileArchiveType>::value), 
bool> pico_serialize(Archive<AR>& ar, const std::vector<bool>& vect) {
    if (!pico_serialize(ar, vect.size()))
        return false;
    uint64_t bits = 0;
    int offset = 0;
    for (const bool& val : vect) {
        if (offset == 64) {
            if (!pico_serialize(ar, bits))
                return false;
            bits = 0;
            offset = 0;
        }
        if (val) {
            bits |= 1llu << offset;
        }
        ++offset;
    }
    if (offset > 0) {
        ar << bits;
    }
    return true;
}

template<class AR>
inline bool pico_serialize(Archive<AR>& ar, const std::string& str) {
    if (!pico_serialize(ar, str.size()))
        return false;
    return ar.write_raw_uncheck(&str[0], str.size());
}

template<class AR>
inline bool pico_deserialize(Archive<AR>& ar, std::string& str) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    str.resize(size);
    return ar.read_raw_uncheck(&str[0], str.size());
}

template<>
inline bool pico_serialize(Archive<TextArchiveType>& ar, const std::string& str) {
    if (!pico_serialize(ar, str.size()))
        return false;
    if (!ar.write_raw_uncheck(&str[0], str.size()))
        return false;
    if (!pico_serialize(ar, ' '))
        return false;
    return true;
}

template<>
inline bool pico_deserialize(Archive<TextArchiveType>& ar, std::string& str) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    str.resize(size);
    if (!ar.read_raw_uncheck(&str[0], str.size()))
        return false;
    char c;
    return pico_deserialize(ar, c);
}

template<>
inline bool pico_serialize(Archive<TextFileArchiveType>& ar,
        const std::string& str) {
    if (!pico_serialize(ar, str.size()))
        return false;
    if (!ar.write_raw_uncheck(&str[0], str.size()))
        return false;
    if (!pico_serialize(ar, ' '))
        return false;
    return true;
}

template<>
inline bool pico_deserialize(Archive<TextFileArchiveType>& ar,
        std::string& str) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    str.resize(size);
    if (!ar.read_raw_uncheck(&str[0], str.size()))
        return false;
    char c;
    return pico_deserialize(ar, c);
}

template<class AR, class T>
inline bool pico_serialize(Archive<AR>& ar, const std::valarray<T>& valarr) {
    if (!pico_serialize(ar, valarr.size()))
        return false;
    for (size_t i = 0; i < valarr.size(); ++i) {
        if (!pico_serialize(ar, valarr[i]))
            return false;
    }
    return true;
}

template<class AR, class T>
inline bool pico_deserialize(Archive<AR>& ar, std::valarray<T>& valarr) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    valarr.resize(size);
    for (size_t i = 0; i < valarr.size(); ++i) {
        if (!pico_deserialize(ar, valarr[i]))
            return false;
    }
    return true;
}

template<class AR>
inline bool pico_serialize(Archive<AR>& ar, const PicoJsonNode& json) {
    std::string str;
    if (!json.save(str))
        return false;
    return pico_serialize(ar, str);
}

template<class AR>
inline bool pico_deserialize(Archive<AR>& ar, PicoJsonNode& json) {
    std::string str;
    if (!pico_deserialize(ar, str))
        return false;
    return json.load(str);
}

template<class AR, class KEY, class VALUE>
bool pico_serialize(Archive<AR>& ar, const std::map<KEY, VALUE>& m) {
    if (!pico_serialize(ar, m.size()))
        return false;
    for (const auto& item : m) {
        if (!pico_serialize(ar, item.first))
            return false;
        if (!pico_serialize(ar, item.second))
            return false;
    }
    return true;
}

template<class AR, class KEY, class VALUE>
bool pico_deserialize(Archive<AR>& ar, std::map<KEY, VALUE>& m) {
    m.clear();
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    for (size_t i = 0; i < size; i++) {
        KEY k;
        if (!pico_deserialize(ar, k))
            return false;
        VALUE v;
        if (!pico_deserialize(ar, v))
            return false;
        if (!m.insert({std::move(k), std::move(v)}).second)
            return false;
    }
    return true;
}

template<class AR, 
    class KEY,
    class VALUE,
    class HASH=std::hash<KEY>,
    class EQUAL=std::equal_to<KEY>,
    class ALLOCATOR=std::allocator< std::pair<const KEY, VALUE> >
> bool pico_serialize(Archive<AR>& ar, 
        const std::unordered_map<KEY, VALUE, HASH, EQUAL, ALLOCATOR>& m) {
    if (!pico_serialize(ar, m.size()))
        return false;
    for (const auto& item : m) {
        if (!pico_serialize(ar, item.first))
            return false;
        if (!pico_serialize(ar, item.second))
            return false;
    }
    return true;
}

template<class AR, 
    class KEY,
    class VALUE,
    class HASH=std::hash<KEY>,
    class EQUAL=std::equal_to<KEY>,
    class ALLOCATOR=std::allocator< std::pair<const KEY, VALUE> >
> bool pico_deserialize(Archive<AR>& ar, 
        std::unordered_map<KEY, VALUE, HASH, EQUAL, ALLOCATOR>& m) {
    size_t size;
    if (!pico_deserialize(ar, size))
       return false; 
    m.clear();
    for (size_t i = 0; i < size; i++) {
        KEY k;
        if (!pico_deserialize(ar, k))
            return false;
        VALUE v;
        if (!pico_deserialize(ar, v))
            return false;
        if (!m.insert({std::move(k), std::move(v)}).second)
            return false;
    }
    return true;
}

template<class AR, class KEY>
bool pico_serialize(Archive<AR>& ar, const std::unordered_set<KEY>& s) {
    if (!pico_serialize(ar, s.size()))
        return false;
    for (const auto& item : s) {
        if (!pico_serialize(ar, item))
            return false;
    }
    return true;
}

template<class AR, class KEY>
bool pico_deserialize(Archive<AR>& ar, std::unordered_set<KEY>& s) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    s.clear();
    for (size_t i = 0; i < size; i++) {
        KEY k;
        if (!pico_deserialize(ar, k))
            return false;
        if (!s.insert(std::move(k)).second)
            return false;
    }
    return true;
}

template<class AR, class KEY, class HASHER>
bool pico_serialize(Archive<AR>& ar, const std::unordered_set<KEY, HASHER>& s) {
    if (!pico_serialize(ar, s.size()))
        return false;
    for (const auto& item : s) {
        if (!pico_serialize(ar, item))
            return false;
    }
    return true;
}

template<class AR, class KEY, class HASHER>
bool pico_deserialize(Archive<AR>& ar, std::unordered_set<KEY, HASHER>& s) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    s.clear();
    for (size_t i = 0; i < size; i++) {
        KEY k;
        if (!pico_deserialize(ar, k))
            return false;
        if (!s.insert(std::move(k)).second)
            return false;
    }
    return true;
}

template<class AR, class KEY>
bool pico_serialize(Archive<AR>& ar, const std::set<KEY>& s) {
    if (!pico_serialize(ar, s.size()))
        return false;
    for (const auto& item : s) {
        if (!pico_serialize(ar, item))
            return false;
    }
    return true;
}

template<class AR, class KEY>
bool pico_deserialize(Archive<AR>& ar, std::set<KEY>& s) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    s.clear();
    for (size_t i = 0; i < size; i++) {
        KEY k;
        if (!pico_deserialize(ar, k))
            return false;
        if (!s.insert(std::move(k)).second)
            return false;
    }
    return true;
}

template<class AR, class KEY, class VALUE, class HASH>
bool pico_serialize(Archive<AR>& ar, const HashTable<KEY, VALUE, HASH>& m) {
    if (!pico_serialize(ar, m.size()))
        return false;
    for (auto iter = m.begin(); iter != m.end(); iter++) {
        if (!pico_serialize(ar, iter->first))
            return false;
        if (!pico_serialize(ar, iter->second))
            return false;
    }
    return true;
}

template<class AR, class KEY, class VALUE, class HASH>
bool pico_deserialize(Archive<AR>& ar, HashTable<KEY, VALUE, HASH>& m) {
    size_t size;
    if (!pico_deserialize(ar, size))
        return false;
    m.clear();
    for (size_t i = 0; i < size; i++) {
        KEY k;
        if (!pico_deserialize(ar, k))
            return false;
        VALUE v;
        if (!pico_deserialize(ar, v))
            return false;
        if (!m.insert({std::move(k), std::move(v)}).second)
            return false;
    }
    return true;
}

template<class AR>
struct GoogleSparseHashIO {
    GoogleSparseHashIO(Archive<AR>& ar) : _ar(&ar) {}
    size_t Write(const void* data, size_t len) {
        _ar->write_raw(data, len);
        return len;
    }
    size_t Read(void* data, size_t len) {
        _ar->read_raw(data, len);
        return len;
    }
    Archive<AR>* _ar;
};

template<class AR>
class GoogleSparseHashSerializer {
public:
    template <class T, class U>
    bool operator()(GoogleSparseHashIO<AR>* ar, const std::pair<const T, U>& value) const {
        SCHECK(ar != nullptr);
        *(ar->_ar) << value.first << value.second;
        return true;
    }

    template <class T, class U>
    bool operator()(GoogleSparseHashIO<AR>* ar, std::pair<const T, U>* value) const {
        SCHECK(ar != nullptr);
        *(ar->_ar) >> *(const_cast<T*>(&value->first)) >> value->second; 
        return true;
    }
};

template<class AR, class KEY, class VALUE, class HASH>
Archive<AR>& operator<<(Archive<AR>& ar, google::dense_hash_map<KEY, VALUE, HASH>& m) {
    GoogleSparseHashIO<AR> ar_(ar);
    m.serialize(GoogleSparseHashSerializer<AR>(), &ar_);
    return ar;
}

template<class AR, class KEY, class VALUE, class HASH>
Archive<AR>& operator>>(Archive<AR>& ar, google::dense_hash_map<KEY, VALUE, HASH>& m) {
    GoogleSparseHashIO<AR> ar_(ar);
    m.unserialize(GoogleSparseHashSerializer<AR>(), &ar_);
    return ar;
}

template<class AR, class T1, class T2>
bool pico_serialize(Archive<AR>& ar, const std::pair<T1, T2>& p) {
    if (!pico_serialize(ar, p.first))
        return false;
    return pico_serialize(ar, p.second);
}

template<class AR, class T1, class T2>
bool pico_deserialize(Archive<AR>& ar, std::pair<T1, T2>& p) {
    if (!pico_deserialize(ar, p.first))
        return false;
    return pico_deserialize(ar, p.second);
}

template<class AR, class... TYPES, size_t... IS>
bool inner_tuple_serializer(Archive<AR>& ar, const std::tuple<TYPES...>& t,
        std::index_sequence<IS...>) {
    auto res = {true, pico_serialize(ar, std::get<IS>(t))...};
    for (auto& r : res) {
        if (!r)
            return false;
    }
    return true;
}

template<class AR, class... TYPES>
bool pico_serialize(Archive<AR>& ar, const std::tuple<TYPES...>& t) {
    return inner_tuple_serializer(ar, t, std::index_sequence_for<TYPES...>{});
}

template<class AR, class... TYPES, size_t... IS>
bool inner_tuple_deserializer(Archive<AR>& ar, std::tuple<TYPES...>& t,
        std::index_sequence<IS...>) {
    auto res = {true, pico_deserialize(ar, std::get<IS>(t))...};
    for (auto& r : res) {
        if (!r)
            return false;
    }
    return true;
}

template<class AR, class... TYPES>
bool pico_deserialize(Archive<AR>& ar, std::tuple<TYPES...>& t) {
    return inner_tuple_deserializer(ar, t, std::index_sequence_for<TYPES...>{});
}

// static checker for serializable / deserializable / archivable
template<class AR, class T,
    class = decltype(pico_serialize(std::declval<Archive<AR>&>(), std::declval<const T&>()))>
std::true_type is_serializable_test(const Archive<AR>&, const T&);
std::false_type is_serializable_test(...);
template<class AR, class T> using IsSerializable =
    decltype(is_serializable_test(std::declval<Archive<AR>>(), std::declval<T>()));

template<class AR, class T,
    class = decltype(pico_deserialize(std::declval<Archive<AR>&>(), std::declval<T&>()))>
std::true_type is_deserializable_test(const Archive<AR>&, const T&);
std::false_type is_deserializable_test(...);
template<class AR, class T> using IsDeserializable = 
    decltype(is_deserializable_test(std::declval<Archive<AR>>(), std::declval<T>()));

template<class AR, class T> using IsArchivable =
    std::integral_constant<bool, 
        IsSerializable<AR, T>::value && IsDeserializable<AR, T>::value>;

template<class T> using IsBinarySerializable = IsSerializable<BinaryArchiveType, T>;
template<class T> using IsBinaryDeserializable = IsDeserializable<BinaryArchiveType, T>;
template<class T> using IsBinaryArchivable = IsArchivable<BinaryArchiveType, T>;

template<class T> using IsTextSerializable = IsSerializable<TextArchiveType, T>;
template<class T> using IsTextDeserializable = IsDeserializable<TextArchiveType, T>;
template<class T> using IsTextArchivable = IsArchivable<TextArchiveType, T>;

template<class T> using IsBinaryFileSerializable = IsSerializable<BinaryFileArchiveType, T>;
template<class T> using IsBinaryFileDeserializable = IsDeserializable<BinaryFileArchiveType, T>;
template<class T> using IsBinaryFileArchivable = IsArchivable<BinaryFileArchiveType, T>;

template<class T> using IsTextFileSerializable = IsSerializable<TextFileArchiveType, T>;
template<class T> using IsTextFileDeserializable = IsDeserializable<TextFileArchiveType, T>;
template<class T> using IsTextFileArchivable = IsArchivable<TextFileArchiveType, T>;

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_ARCHIVE_H
