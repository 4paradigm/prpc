#ifndef PARADIGM4_PICO_CORE_CHANNEL_H
#define PARADIGM4_PICO_CORE_CHANNEL_H

#include <memory>

#include "ChannelEntity.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief include a ChannelEntity pointer, pack ChannelEntity function
 *  NOTE: the default setting of Channel is unbounded,
 *        if you want a bounded Channel, you must manually set the capacity of the Channel.
 */
template<class T>
class Channel {
public:
    Channel() {
        _channel.reset(new ChannelEntity<T>);
    }

    explicit Channel(size_t capacity) {
        _channel.reset(new ChannelEntity<T>);
        _channel->set_capacity_unlocked(capacity);
    }

    virtual ~Channel() = default;

    void set_capacity(size_t capacity) {
        _channel->set_capacity(capacity);
    }

    size_t capacity() {
        return _channel->capacity();
    }

    size_t capacity_unlocked() {
        return _channel->capacity_unlocked();
    }

    size_t size() {
        return _channel->size();
    }

    size_t size_unlocked() {
        return _channel->size_unlocked();
    }

    bool closed() {
        return _channel->closed();
    }

    void shared_open() {
        _channel->shared_open();
    }

    void open() {
        _channel->open();
    }

    void close() {
        _channel->close();
    }

    bool empty() {
        return _channel->empty();
    }

    bool empty_unlocked() {
        return _channel->empty_unlocked();
    }

    bool full() {
        return _channel->full();
    }

    bool full_unlocked() {
        return _channel->full_unlocked();
    }

    bool read(T& val) {
        return _channel->read(val);
    }

    bool try_read(T& val) {
        return _channel->try_read(val);
    }

    size_t read(T* vals, size_t n) { // read n elements, unless the channel is closed
        return _channel->read(vals, n);
    }

    bool write(const T& val) {
        return _channel->write(val);
    }

    bool write(T&& val) {
        return _channel->write(std::move(val));
    }

    bool try_write(const T& val) {
        return _channel->try_write(val);
    }

    bool try_write(T&& val) {
        return _channel->try_write(std::move(val));
    }

    size_t write(const T* vals, size_t n) { // write n elements, unless channel is closed
        return _channel->write(vals, n);
    }

    size_t write_move(T* vals, size_t n) { // write on move
        return _channel->write_move(vals, n);
    }

    //    bool write_front(T&& val) {
    //        return _channel->write_front(std::move(val));
    //    }

    bool operator==(const Channel& other) {
        return _channel == other._channel;
    }

    bool operator!=(const Channel& other) {
        return _channel != other._channel;
    }

    typename ChannelEntity<T>::buffer_type& data() {
        return _channel->data();
    }

private:
    std::shared_ptr<ChannelEntity<T>> _channel;
};

template<>
class Channel<void> {
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_CHANNEL_H
