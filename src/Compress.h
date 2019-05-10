#ifndef PARADIGM4_PICO_COMMON_COMPRESS_H
#define PARADIGM4_PICO_COMMON_COMPRESS_H

#include <type_traits>

#include "common.h"
#include "pico_lexical_cast.h"
#include "Archive.h"
#include "pico_log.h"

#include "snappy.h"
namespace zlib {
#include "zlib.h"
}
namespace LZ4 {
#include "lz4.h"
#include "lz4hc.h"
}

namespace paradigm4 {
namespace pico {
namespace core {

// NOT thread safe !!!!!!
class CompressEntity : public VirtualObject {
public:
    static inline char* buf_realloc(void* ptr, size_t size) {
        char* ret = (char*) pico_realloc(ptr, size);
        return ret;
    }

    template<class T>
    void compress(const T& in, BinaryArchive& out) {
        _in.clear();
        _in << in;
        raw_compress(_in, out);
    }

    template<class T>
    void uncompress(BinaryArchive& in,  T& out) {
        raw_uncompress(in, _out);
        _out >> out;
        SCHECK(_out.is_exhausted());
    }

    template<class T>
    void compress(const T& in, char** out, size_t* size) {
        _in.clear();
        _in << in;
        raw_compress((char*) _in.cursor(), _in.readable_length(), out, size);
    }

    template<class T>
    void uncompress(const char* in, size_t in_size, T& out) {
        BinaryArchive ar;
        ar.set_read_buffer((char*)in, in_size);
        raw_uncompress(ar, _out);
        _out >> out;
        SCHECK(_out.is_exhausted());
    }

    template<class T>
    void compress(const T& in, std::string& out) {
        _in.clear();
        _in << in;
        raw_compress(_in, out);
    }

    template<class T>
    void uncompress(const std::string& in, T& out) {
        raw_uncompress(in, _out);
        _out >> out;
        SCHECK(_out.is_exhausted());
    }

    // subclass may override this method to avoid memcpy
    virtual void raw_compress(BinaryArchive& in, std::string& out) {
        raw_compress(in, _out);
        out.resize(_out.readable_length());
        memcpy((char*) out.data(), _out.cursor(), _out.readable_length());
    }

    virtual void raw_uncompress(const std::string& in, BinaryArchive& out) {
        BinaryArchive ar;
        ar.set_read_buffer((char*)in.data(), in.length());
        raw_uncompress(ar, out);
    }

    virtual void raw_compress(BinaryArchive& in, BinaryArchive& out) {
        SCHECK(&in != &out);
        out.reset();
        size_t out_size = 0;
        char* out_ptr = nullptr;
        raw_compress((char*) in.cursor(), in.readable_length(), &out_ptr, &out_size);
        out.set_read_buffer(out_ptr, out_size, [](void* p) { pico_free(p); });
    }

    virtual void raw_uncompress(BinaryArchive& in, BinaryArchive& out) {
        SCHECK(&in != &out);
        out.reset();
        size_t out_size = 0;
        char* out_ptr = nullptr;
        raw_uncompress((char*) in.cursor(), in.readable_length(), &out_ptr, &out_size);
        out.set_read_buffer(out_ptr, out_size, [](void* p) { pico_free(p); });
    }

    virtual void raw_compress(const char* in, size_t in_size, char** out, size_t* out_size) = 0;

    virtual void raw_uncompress(const char* in, size_t in_size, char** out, size_t* out_size) = 0;

    virtual void set_property(const std::string& key, const std::string&) {
        SLOG(FATAL) << "useless property : " << key;
    }

protected:
    BinaryArchive _in;
    BinaryArchive _out;
};

class ZlibCompressEntity : public CompressEntity {
public:
    void raw_compress(BinaryArchive& in, std::string& out) {
        size_t len = zlib::compressBound(in.readable_length()) + sizeof(in.readable_length());
        out.resize(len);
        char* out_ptr = (char*)out.data();
        size_t out_size = out.size();
        raw_compress(in.cursor(), in.readable_length(), &out_ptr, &out_size);
        out.resize(out_size);
    }

    void raw_uncompress(const char* in, size_t in_size, char** out, size_t* out_size) {
        size_t len;
        BinaryArchive ar;
        ar.set_read_buffer((char*)in, in_size);
        ar.read_back(&len, sizeof(len));
        ar.release();
        if (*out_size < len) {
            *out = buf_realloc(*out, len);
        }
        *out_size = len;
        zlib::Bytef* dst = (zlib::Bytef*) *out;
        zlib::uLong dst_len = *out_size;
        SCHECK(Z_OK == zlib::uncompress(dst, &dst_len, 
                    (zlib::Bytef*)in, (zlib::uLong)in_size));
    }

    void raw_compress(const char* in, size_t in_size, char** out, size_t* out_size) {
        size_t max_buffer_size = zlib::compressBound(in_size) + sizeof(in_size);
        if (*out_size < max_buffer_size) {
            *out = buf_realloc(*out, max_buffer_size);
        }
        *out_size = max_buffer_size;
        BinaryArchive ar;
        ar.set_read_buffer(*out, *out_size);
        zlib::Bytef* dst = (zlib::Bytef*) *out;
        zlib::uLong dst_len = *out_size;
        SCHECK(Z_OK == zlib::compress2(dst, &dst_len, 
                    (zlib::Bytef*)in, (zlib::uLong)in_size, _compress_level));
        ar.set_end((char*) (dst + dst_len));
        ar << in_size;
        *out_size = ar.length();
        *out = ar.release();
    }

    void set_property(const std::string& key, const std::string& value) {
        if (key == "compress_level") {
            pico_lexical_cast(value, _compress_level);
            SCHECK(_compress_level >= 0 && _compress_level <= 9) << "compress level must be 0~9";
        } else {
            SLOG(FATAL) << "useless property : " << key;
        }
    }

private:
    int _compress_level = 1;
};

class SnappyCompressEntity : public CompressEntity {
public:
    void raw_compress(BinaryArchive& in, std::string& out) {
        snappy::Compress(in.cursor(), in.readable_length(), &out);
    }

    void raw_compress(const char* in, size_t in_size, char** out, size_t* out_size) {
        size_t len = snappy::MaxCompressedLength(in_size);
        if (*out_size < len) {
            *out = buf_realloc(*out, len);
        }
        *out_size = len;
        snappy::RawCompress(in, in_size, *out, out_size);
    }

    void raw_uncompress(const char* in, size_t in_size, char** out, size_t* out_size) {
        size_t len = 0;
        SCHECK(snappy::GetUncompressedLength(in, in_size, &len)) 
            << "snappy uncompress failed";
        if (*out_size < len) {
            *out = buf_realloc(*out, len);
        }
        *out_size = len;
        SCHECK(snappy::RawUncompress(in, in_size, *out)) 
            << "snappy uncompress failed"; 
    }
};

class LZ4CompressEntity : public CompressEntity {
public:
    void raw_compress(BinaryArchive& in, std::string& out) {
        size_t max_buffer_size = LZ4::LZ4_compressBound(in.readable_length()) + sizeof(in.readable_length());
        out.resize(max_buffer_size);
        char* out_ptr = (char*)out.data();
        size_t out_size = out.size();
        raw_compress(in.cursor(), in.readable_length(), &out_ptr, &out_size);
        out.resize(out_size);
    }

    void raw_uncompress(const char* in, size_t in_size, char** out, size_t* out_size) {
        size_t len;
        BinaryArchive ar;
        ar.set_read_buffer((char*)in, in_size);
        ar.read_back(&len, sizeof(len));
        size_t compressed_size = in_size - sizeof(len);
        ar.release();
        if (*out_size < len) {
            *out = buf_realloc(*out, len);
        }
        *out_size = len;
        auto ret = LZ4::LZ4_decompress_fast(in, *out, *out_size);
        SCHECK(ret == static_cast<int>(compressed_size)) << "lz4 uncompress failed";
    }

    void raw_compress(const char* in, size_t in_size, char** out, size_t* out_size) {
        size_t max_buffer_size = LZ4::LZ4_compressBound(in_size) + sizeof(in_size);
        if (*out_size < max_buffer_size) {
            *out = buf_realloc(*out, max_buffer_size);
        }
        *out_size = max_buffer_size;
        BinaryArchive ar;
        ar.set_read_buffer(*out, *out_size);
        *out_size = LZ4::LZ4_compress_fast(in, *out, in_size, *out_size, _acceleration);
        ar.set_end((char*) (*out + *out_size));
        ar << in_size;
        *out_size = ar.length();
        *out = ar.release();
    }

    void set_property(const std::string& key, const std::string& value) {
        if (key == "acceleration") {
            pico_lexical_cast(value, _acceleration);
            SCHECK(_acceleration > 0) << "compress level must be larger than 0";
        } else {
            SLOG(FATAL) << "useless property : " << key;
        }
    }

private:
    int _acceleration = 1;
};

class Compress {
public:
    Compress() {
    }

    Compress(const std::string& method) {
        if (method == "zlib") {
            _c.reset(new ZlibCompressEntity());
        } else if (method == "snappy") {
            _c.reset(new SnappyCompressEntity());
        } else if (method == "lz4") {
            _c.reset(new LZ4CompressEntity());
        } else {
            SLOG(FATAL) << "unknown compress method";
        }
    }

    template<class T> 
    void compress(const T& in, char** out, size_t* out_size) {
        static_assert(IsBinaryArchivable<T>::value, "compress element is not Binary Archivable.");
        _c->compress(in, out, out_size);
    }

    template<class T> 
    void uncompress(const char* in, size_t in_size, T& out) {
        static_assert(IsBinaryArchivable<T>::value, "compress element is not Binary Archivable.");
        _c->uncompress(in, in_size, out);
    }

    template<class T> 
    void compress(const T& in, BinaryArchive& out) {
        static_assert(IsBinaryArchivable<T>::value, "compress element is not Binary Archivable.");
        _c->compress(in, out);
    }

    template<class T> 
    void uncompress(BinaryArchive& in, T& out) {
        static_assert(IsBinaryArchivable<T>::value, "compress element is not Binary Archivable.");
        _c->uncompress(in, out);
    }

    template<class T> 
    void compress(const T& in, std::string& out) {
        static_assert(IsBinaryArchivable<T>::value, "compress element is not Binary Archivable.");
        _c->compress(in, out);
    }

    template<class T> 
    void uncompress(const std::string& in, T& out) {
        static_assert(IsBinaryArchivable<T>::value, "compress element is not Binary Archivable.");
        _c->uncompress(in, out);
    }

    void raw_compress(char* in, size_t in_size, char** out, size_t* out_size) {
        _c->raw_compress(in, in_size, out, out_size);
    }

    void raw_uncompress(char* in, size_t in_size, char** out, size_t* out_size) {
        _c->raw_uncompress(in, in_size, out, out_size);
    }

    void raw_compress(BinaryArchive& in, BinaryArchive& out) {
        _c->raw_compress(in, out);
    }

    void raw_uncompress(BinaryArchive& in, BinaryArchive& out) {
        _c->raw_uncompress(in, out);
    }

    void set_property(const std::string& key, const std::string& value) {
        _c->set_property(key, value);
    }

private:
    std::shared_ptr<CompressEntity> _c;
};

inline Compress pico_compress(const std::string& method) {
    return Compress(method);
}

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_PICO_CAST_H
