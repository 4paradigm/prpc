#ifndef PARADIGM4_PICO_CORE_FILE_LINE_READER_H
#define PARADIGM4_PICO_CORE_FILE_LINE_READER_H

#include <cstdio>
#include "pico_log.h"
#include "VirtualObject.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief class FileLineReader used to read line from file.
 */
class FileLineReader : public NoncopyableObject {
public:
    ~FileLineReader() {
        free(_buffer);
    }

    FileLineReader() {
    }

    /*!
     * \brief a safe getline from file
     * \param stream  file to readline from
     * \param drop_delim  whether drop '\n' or not
     * return char*, private member variable _buffer, where put the getline result
     */
    char* getline(FILE* stream, bool drop_delim = true) {
        return getdelim(stream, '\n', drop_delim);
    }

    char* getdelim(FILE* stream, char delim, bool drop_delim = true) {
        if (stream == nullptr) {
            return nullptr;
        }
        ssize_t ret = ::getdelim(&_buffer, &_buffer_size, delim, stream);
        _size = _buffer_size;
        if (ret == -1) {
            SCHECK(feof(stream));
            _size = 0;
            return nullptr;
        } else if (ret >= 1) {
            if (_buffer[ret - 1] == delim && drop_delim) {
                _buffer[ret - 1] = '\0';
                _size = ret - 1;
            } else {
                _size = ret;
            }
        }
        return _buffer;
    }

    char* buffer() {
        return _buffer;
    }

    size_t size() {
        return _size;
    }

private:
    char* _buffer = nullptr;
    size_t _buffer_size = 0;
    size_t _size;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif
