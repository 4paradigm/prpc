#ifndef PARADIGM4_PICO_CORE_COMMON_H
#define PARADIGM4_PICO_CORE_COMMON_H

#include <string>
#include <mutex>
#include <atomic>
#include <vector>
#include <utility>
#include <thread>
#include <ctime>
#include <cxxabi.h>
#include <sstream>
#include <iomanip>
#include <random>

#include <boost/format.hpp>
#include <glog/logging.h>
#include "pico_log.h"
#include "pico_memory.h"

#include "VirtualObject.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#include "MurmurHash3.h"
#pragma GCC diagnostic pop

namespace paradigm4 {
namespace pico {
namespace core {

typedef int16_t comm_rank_t;

#ifndef PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT
#define PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT(name, op) \
template<class T, class = decltype(std::declval<const T&>() op std::declval<const T&>())> \
std::true_type pico_inner_ ## name ## _test(const T&); \
std::false_type pico_inner_ ## name ## _test(...); \
template<class T> using name = \
    decltype(pico_inner_ ## name ## _test(std::declval<T>()))
#endif // PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT

PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT(IsLessEqualComparable, <=);
PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT(IsLessComparable, <);
PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT(IsGreaterEqualComparable, >=);
PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT(IsGreaterComparable, >);
PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT(IsEqualComparable, ==);

#undef PICO_GENERATE_COMPARISON_OPERATOR_CONCEPT

/*！
 * \brief convert a certain time to a given format
 * \param tmb std::tm* time
 * \param fmt char* format
 * \return string of formated time
 */
inline std::string pico_format_time(const std::tm* tmb, const char* fmt) {
    std::stringstream ss;
    ss << std::put_time(tmb, fmt);
    return ss.str();
}

/*!
 * \brief convert a certain time point to a given format in local time zone
 * \param time_point std::chrono::system_clock::time_point&, time to be convert
 * \param fmt char*, format
 * \return string of formated time in local time zone.
 */
inline std::string pico_format_time_point_local(const std::chrono::system_clock::time_point& time_point,
        const char* fmt) {
    std::time_t t = std::chrono::system_clock::to_time_t(time_point);
    return pico_format_time(std::localtime(&t), fmt);
}

/*!
 * \brief convert a certain time point to a fiven format in GMT
 * \param time_point std::chrono::system_clock::time_point&, time to be convert
 * \param fmt, char*, format
 * \return string of formated time in GMT
 */
inline std::string pico_format_time_point_gm(const std::chrono::system_clock::time_point& time_point,
        const char* fmt) {
    std::time_t t = std::chrono::system_clock::to_time_t(time_point);
    return pico_format_time(std::gmtime(&t), fmt);
}

/*!
 * \brief get time point of now
 * \return std::chrono::system_clock::time_point, time point of now 
 */
inline std::chrono::system_clock::time_point pico_current_time() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    return now;
}

/*!
 * \brief get time point of pico initailize time.
 *  a static variable get current time only once when pico initailize
 * \return std::chrono::system_clock::time_point, time point of pico initailize.
 */
inline std::chrono::system_clock::time_point pico_initialize_time() {
    static auto now = pico_current_time();
    return now;
}

template<typename T> struct IsVector : public std::false_type {};

template<typename T, typename A>
struct IsVector<std::vector<T, A>> : public std::true_type {};

/*!
 * \brief generate global mutex.
 *  this function return a static mutex variable, so each process only have
 *  one global mutex
 * \return reference of the global mutex.
 */
inline std::mutex& global_fork_mutex() {
    static std::mutex fork_mutex;
    return fork_mutex;
}

/*!
 * \brief exec popen with lock, unlock after popen.
 *  the lock is global_fork_mutex, so only one thread in process can execution
 *  popen at same time
 * \return popen result
 */
inline FILE* guarded_popen(const char* cmd, const char* type) {
    std::lock_guard<std::mutex> mlock(global_fork_mutex());
    return popen(cmd, type);
}

/*!
 * \brief exec pclose with lock
 *  the lock is global_fork_mutex, so only one thread in process can execution
 *  pclose at same time
 * \return pclose result
 */
inline int guarded_pclose(FILE* file) {
    std::lock_guard<std::mutex> mlock(global_fork_mutex());
    return pclose(file);
}

/*!
 * \brief StringFormatter struct used to store string template and render it with func format
 */
struct StringFormatter {
    explicit StringFormatter(boost::format& formatter) : formatter(formatter) {}
    void format() {
    }

    template <class T>
    void format(const T& val) {
        formatter % val;
    }

    template <class T, class... ARGS>
    void format(const T& val, ARGS&&... args) {
        formatter % val;
        format(std::forward<ARGS>(args)...);
    }
    boost::format& formatter;
};

/*!
 * \brief return an empty string when format string get empty argument
 * \return ""
 */
inline std::string format_string() {
    return "";
}

/*!
 * \brief check a typename content can be LOG or not
 */
template<class T, class = decltype(LOG(INFO) << std::declval<const T&>())>
std::true_type is_gloggable(const T&);
std::false_type is_gloggable(...);
template<class T> using IsGloggable = decltype(is_gloggable(std::declval<const T&>()));

/*!
 * \brief append a vector to another vector
 * \param vect  input vector, put vect after result, vect must be MoveAssignable
 *  and MoveConstructible
 * \param result  output vector, put vect after result
 */
template<class T>
void vector_move_append(std::vector<T>& result, std::vector<T>& vect) {
    static_assert(std::is_move_assignable<T>::value, "must be MoveAssignable");
    static_assert(std::is_move_constructible<T>::value, "must be MoveConstructible");
    result.reserve(result.size() + vect.size());
    typedef typename std::vector<T>::iterator iter_t;
    result.insert(result.end(), std::move_iterator<iter_t>(vect.begin()),
            std::move_iterator<iter_t>(vect.end()));
}

/*!
 * \brief operator < for pair, compare first element only
 */
struct IsPairFirstLessThan {
    template<class T, class U>
    bool operator()(const std::pair<T, U>& a, const std::pair<T, U>& b) {
        return a.first < b.first;
    }
};

/*!
 * \brief operator > for pair, compare first element only
 */
struct IsPairFirstLargerThan {
    template<class T, class U>
    bool operator()(const std::pair<T, U>& a, const std::pair<T, U>& b) {
        return a.first > b.first;
    }
};

/*!
 * \brief operator < for pair, compare second element only
 */
struct IsPairSecondLessThan {
    template<class T, class U>
    bool operator()(const std::pair<T, U>& a, const std::pair<T, U>& b) {
        return a.second < b.second;
    }
};

/*!
 * \brief operator > for pair, compare second element only
 */
struct IsPairSecondLargerThan {
    template<class T, class U>
    bool operator()(const std::pair<T, U>& a, const std::pair<T, U>& b) {
        return a.second > b.second;
    }
};

/*!
 * \brief sign of val
 * \return 1 when val positive, -1 when val negtive, 0 when val equals 0
 */
template<typename T>
int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

/*!
 * \brief render template string fmt_str with args
 * \param fmt_str format in string form
 * \param args... variables to put into fmt_str, like printf
 * \return string after format
 */
template<class ...ARGS>
std::string format_string(const std::string& fmt_str, ARGS&&... args) {
    boost::format boost_fmt(fmt_str);
    StringFormatter str_fmt(boost_fmt);
    str_fmt.format(args...);
    return boost_fmt.str();
}

/*!
 * \brief try to run func(args) until it exec correctly
 * \param func  function to run, func must has return code to indicate the function
 *  finish correctly or not
 * \param args...  arguments of func
 * \return return code of func(args)
 */
template<class FUNC, class... ARGS>
static auto retry_eintr_call(FUNC func, ARGS&&... args) 
            -> typename std::result_of<FUNC(ARGS...)>::type {
    for (;;) {
        auto err = func(args...);
        if (err < 0 && errno == EINTR) {
            SLOG(WARNING) << "Signal is caught. Ignored.";
            continue;        
        }        
        return err;
    }
}


template<typename A, typename FUNC> 
std::pair<A, FUNC>& pico_var_arg_call(std::pair<A, FUNC>& p) {
    return p;
}

template<typename A, typename FUNC, typename T>
std::pair<A, FUNC>& pico_var_arg_call(std::pair<A, FUNC>& p, T val) {
    (std::bind(std::forward<FUNC>(p.second), std::forward<A>(p.first), std::forward<T>(val)))();
    return p;
}

template<typename A, typename FUNC, typename B, typename... T>
std::pair<A, FUNC>& pico_var_arg_call(std::pair<A, FUNC>& p, B val, T... vals) {
    (std::bind(std::forward<FUNC>(p.second), std::forward<A>(p.first), std::forward<B>(val)))();
    return pico_var_arg_call<A, FUNC, T...>(p, vals...);
}

/*!
 * \brief run FUNC with first const arg A, and second arg val foreach in vals
 *  ie, pico_var_arg_call(pair<a, func>, b, c, d ...)
 *  run  func(a, b)
 *  then func(a, c)
 *  then func(a, d) ...
 * \param p  a value function pair
 * \param vals...  arguments for func
 * \return same value function pair with param
 */
template<typename A, typename FUNC, typename... T>
std::pair<A, FUNC>& pico_var_arg_call(std::pair<A, FUNC>& p, T... vals) {
    return pico_var_arg_call<A, FUNC, T...>(p, vals...);
}

/*!
 * \brief get a random uniform real number between [0, 1).
 * \return a random real number
 */
template<class T = double>
T pico_real_random() {
    static thread_local std::random_device rd; 
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_real_distribution<T> dis;

    return dis(gen);
}

/*!
 * \brief get a random real number to normal distribution.
 *  the default normal distribution has mean value of 0.0, variance of 1.0
 * \return a normal real number
 */
template<class T = double>
T pico_normal_random() {
    static thread_local std::random_device rd; 
    static thread_local std::mt19937 gen(rd());
    static thread_local std::normal_distribution<T> dis(0.0, 1.0);

    return dis(gen);
}

/*!
 * \brief change a typename into readable form
 * \param mangled a typename in const char*
 * \return a readable form in string
 */
inline std::string demangle(const char* mangled) {
    int status = 0;
    std::unique_ptr<char[], void (*)(void*)> result(
            abi::__cxa_demangle(mangled, 0, 0, &status), std::free);
    return result.get() ? std::string(result.get()) : "error occurred";
}

/*!
 * \brief get typename of T in pretty format
 * \return typename of T in readable form
 */
template<class T>
std::string readable_typename() { 
    return demangle(typeid(T).name());
}

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

/*!
 * \brief an atomic id, ++ after each gen.
 *  can be used to discretization
 * \return an auto-increment id
 */
template<class TYPE, class ID_TYPE>
struct AtomicID {
    static ID_TYPE gen() {
        static std::atomic<ID_TYPE> s_id;
        return s_id++;
    }
};

/*!
 * \brief get default number of concurrency.
 *  default to half of the number of threads supported
 * \return default concurrency number
 */
inline unsigned pico_process_default_concurrency() {
    static unsigned concurrency = (std::thread::hardware_concurrency() + 1) / 2;
    return concurrency;
}

const uint32_t MURMURHASH_SEED = 142857;
/*!
 * \brief hash string into int32
 * \param key  string to be hash
 * \return int32_t hash code of key
 */
inline int32_t pico_murmurhash3_32(const std::string& key) {
    int32_t hash_code = 0;
    murmur_hash3_x86_32(key.c_str(), key.size(), MURMURHASH_SEED, &hash_code);
    return hash_code;
}

/*!
 * \brief hash string key into int64
 * \param key  string to be hash
 * \return int64_t hash code of key
 */
inline int64_t pico_murmurhash3_64(const std::string& key) {
    std::pair<int64_t, int64_t> hash_code;
#if __x86_64__
    murmur_hash3_x64_128(key.c_str(), key.size(), MURMURHASH_SEED, &hash_code);
#else
    murmur_hash3_x86_128(key.c_str(), key.size(), MURMURHASH_SEED, &hash_code);
#endif
    return hash_code.first;
}

/**
 * Convert an unique_ptr<FROM> to an unique_ptr<TO>
 * @tparam TO usually the Derived class
 * @tparam FROM usually the Base class
 * @param old unique_ptr<FROM>
 * @return unique_ptr<TO>
 */
template<typename TO, typename FROM>
inline std::unique_ptr<TO> static_unique_pointer_cast (std::unique_ptr<FROM>&& old){
    return std::unique_ptr<TO>{static_cast<TO*>(old.release())};
}

struct EnumClassHash {
    template<typename T>
    std::size_t operator()(T t) const {
        return static_cast<std::size_t>(t);
    }
};

class PicoArgs {
public:
    PicoArgs() {}

    PicoArgs(const std::string& name) {
        _vec.push_back(name);
    }

    void add(const std::string& arg) {
        _vec.push_back(arg);
    }

    int argc() {
        return _vec.size();
    }

    char** argv() {
        _args.clear();
        for (size_t i = 0; i < _vec.size(); ++i) {
            _args.push_back((char*)_vec[i].data());
        }
        _args.push_back(nullptr);
        return &_args[0];
    }
private:
    std::vector<std::string> _vec;
    std::vector<char*> _args;
};

int gettid();
int getpid();
void set_thread_name(const std::string& name);

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_COMMON_H
