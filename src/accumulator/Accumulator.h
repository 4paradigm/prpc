#ifndef PARADIGM4_PICO_CORE_ACCUMULATOR_H
#define PARADIGM4_PICO_CORE_ACCUMULATOR_H

#include "Aggregator.h"
#include "AggregatorFactory.h"
#include "AccumulatorManager.h"
#include "AccumulatorClient.h"
#include "AccumulatorServer.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief Accumulator do something like parameter server, but save variables that
 *  most operation is add. in this case Accumulator is faster than parameter server.
 *  Accumulator usually save total instance num, process progress, etc.
 */
template<class AGG>
class Accumulator : public NoncopyableObject {
public:
    static_assert(std::is_base_of<Aggregator<typename AGG::value_type, AGG>, AGG>::value, 
            "not inherit from Aggregator");

    typedef AGG aggregator_type;

    Accumulator() = delete;
    
    Accumulator(const std::string& name) : Accumulator(name, 1024) {}

    Accumulator(const std::string& name, const size_t flush_freq, bool final_need_flush = true) :
            _name(name), _flush_freq(flush_freq), _final_need_flush(final_need_flush) {
        _agg.init();
        _cached_count = 0;
        AccumulatorManager::singleton().alloc_accumulator_id<AGG>(_name, &_id);
        AccumulatorClient::singleton().add_aggregator<AGG>(_name);
    }

    virtual ~Accumulator() {
        if(_final_need_flush) {
            flush();
        }
    }

    // TODO: remove
	bool read() {
        std::lock_guard<std::mutex> lock(_mutex);
        return read(_agg);
    }

    // TODO: remove
    bool try_to_string(std::string& ret_str) {
        return _agg.try_to_string(ret_str);
    }

    /*!
     * \brief read accumulator $_name from server and try to convert it to string
     * \param str converted string
     * \return whether success
     */
    bool try_read_to_string(std::string& str) {
        AGG agg;
        bool ret = read(agg);
        if (ret) {
            ret = agg.try_to_string(str);
        }
        return ret;
    }

    /*!
     * \brief read accumulator $_name from server and convert it to string
     * \return the converted string, abort the process while failure
     */
    std::string read_to_string() {
        AGG agg;
        std::string str;
        SCHECK(read(agg)) << "read accumulator failed, " << _name;
        SCHECK(agg.try_to_string(str)) << "accumulator try_to_string failed, " << _name;
        return str;
    }

    /*!
     * \brief read $_name from server into ret
     */
    bool read(AGG& ret) {
        return AccumulatorClient::singleton().read<AGG>(_name, ret);
    }

    /*!
     * \brief add to local, push to client when get _flush_freq
     */
    bool write(const typename AGG::value_type& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _agg.merge_value(value);
        ++_cached_count;
        if (_flush_freq > 0 && _cached_count >= _flush_freq) {
            return flush_nonlock();
        }

        return true;
    }

    bool write(typename AGG::value_type&& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _agg.merge_value(std::move(value));
        ++_cached_count;
        if (_flush_freq > 0 && _cached_count >= _flush_freq) {
            return flush_nonlock();
        }

        return true;
    }

    /*!
     * \brief reset an agg from server
     */
    bool reset() {
        return AccumulatorClient::singleton().reset<AGG>(_name);
    }

    // DO NOT use erase related functions unless you know what is it
    bool erase() {
        return AccumulatorClient::singleton().erase({_name});
    }

    /*!
     * \brief can not write when flush
     */
    bool flush() {
        std::lock_guard<std::mutex> lock(_mutex);
        return flush_nonlock();
    }

    const std::string& name() const {
        return _name;
    }

private:
    /*!
     * \brief flush local data to client
     */
    bool flush_nonlock() {
        if (_cached_count == 0) {
            return true;
        }
        bool ret = AccumulatorClient::singleton().write<AGG>(_name, std::move(_agg));
        _agg.init();
        _cached_count = 0;
        return ret;
    }

private:
    AGG _agg;
    size_t _id;
    std::mutex _mutex;
    std::string _name;
    size_t _cached_count;
    size_t _flush_freq;
    bool _final_need_flush;
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_ACCUMULATOR_H
