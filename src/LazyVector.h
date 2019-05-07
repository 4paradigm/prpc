//
// Created by Huang Geyang on 2018/8/16.
//

#ifndef PARADIGM4_PICO_COMMON_LAZYVECTOR_H
#define PARADIGM4_PICO_COMMON_LAZYVECTOR_H

#include "common/include/pico_log.h"

namespace paradigm4 {
namespace pico {

const size_t EXTRA_SPACE_RESERVED = 64;

/**
 * 在遍历中如果进行push，那指针将会失效
 * @tparam T
 */
template<typename T>
class LazyVector {

    // Modify option: can use T* begin
    struct lazy_view_t {
        // the beginning index of Item in lazy_data
        size_t begin;
        // the ending index of Item in lazy_data
        size_t end;
        // is item pushable, meaning is the view at the end of lazy_data
        bool pushable;
        // merge from std::vector<std::shared_ptr<LazyData<T>>> _lazy_datas;
        std::shared_ptr <std::vector<T>> lazy_data;
    };

    struct iterator {
        LazyVector<T> *_lazy_vector_ptr;
        lazy_view_t *_lazy_view_ptr;
        T *_cursor = nullptr;
        size_t _idx = 0;
        size_t _global_idx = 0;

        iterator() = default;


        iterator(LazyVector<T> *lazy_vector,
                 lazy_view_t *lazy_view_ptr,
                 T *start,
                 size_t idx,
                 size_t global_idx) :
                _lazy_vector_ptr(lazy_vector),
                _lazy_view_ptr(lazy_view_ptr),
                _cursor(start),
                _idx(idx),
                _global_idx(global_idx) {};

        T &operator*() {
            return *_cursor;
        }

        T *operator->() {
            return _cursor;
        }

        iterator &operator++() {
            // CAUTION: extemely likely to cause bugs
            ++_idx;
            ++_cursor;
            ++_global_idx;

            // The increment is over datas, but remember don't go outside the datas
            while (_lazy_view_ptr->end <= (_lazy_view_ptr->begin + _idx) &&
                   _lazy_view_ptr != &(_lazy_vector_ptr->_lazy_views.back())) {
                ++_lazy_view_ptr;
                _idx = 0;
                _cursor = (_lazy_view_ptr->lazy_data->data());
            }

            return *this;
        }

        iterator operator++(int) {
            iterator it(*this);
            ++(*this);
            return it;
        }

        bool operator==(const iterator &it) const {
            return (*_lazy_view_ptr).lazy_data == (*it._lazy_view_ptr).lazy_data && _cursor == it._cursor;
        }

        bool operator!=(const iterator &it) const {
            return (*_lazy_view_ptr).lazy_data != (*it._lazy_view_ptr).lazy_data || _cursor != it._cursor;
        }


    };

public:
    LazyVector() {
        _lazy_views.clear();
        _total_item_num = 0;
        append_a_new_lazy_data();
    }

    // for test convenience
    explicit LazyVector(size_t) {
        LazyVector();
    }

    void clear() {
        _lazy_views.clear();
        _total_item_num = 0;
        append_a_new_lazy_data();
    }

    /**
     * Push back an item in the rear
     * @param item
     */
    void push_back(T &&item) {

        // See if empty datas
        if (_lazy_views.empty()) {
            append_a_new_lazy_data();
        }

        // This line must be separated from above.
        if (_lazy_views.back().pushable == false) {
            append_a_new_lazy_data();
        }

        // Now, non-empty datas, and pushable at rear, but safety check
        if (_lazy_views.back().end != _lazy_views.back().lazy_data->size()) {
            _lazy_views.back().pushable = false;
            append_a_new_lazy_data();
        }

        // Now push
        _lazy_views.back().lazy_data->push_back(item);
        _lazy_views.back().end = _lazy_views.back().lazy_data->size();
        _total_item_num += 1;
    }

    void push_back(const T& item) {
        // See if empty datas
        if (_lazy_views.empty()) {
            append_a_new_lazy_data();
        }

        // This line must be separated from above.
        if (_lazy_views.back().pushable == false) {
            append_a_new_lazy_data();
        }

        // Now, non-empty datas, and pushable at rear, but safety check
        if (_lazy_views.back().end != _lazy_views.back().lazy_data->size()) {
            _lazy_views.back().pushable = false;
            append_a_new_lazy_data();
        }

        // Now push
        _lazy_views.back().lazy_data->push_back(item);
        _lazy_views.back().end = _lazy_views.back().lazy_data->size();
        _total_item_num += 1;
    }

    void push_back(std::vector<T> &&lazy_data) {
        if (lazy_data.size() > 0) {
            _total_item_num += lazy_data.size();
            lazy_view_t lazy_view = {0, lazy_data.size(), true, std::make_shared<std::vector<T>>(std::move(lazy_data))};
            _lazy_views.push_back(std::move(lazy_view));
        }
    }

    void push_back(const lazy_view_t& lazy_view) {
        if (lazy_view.end - lazy_view.begin > 0) {
            _total_item_num += lazy_view.end - lazy_view.begin;
            _lazy_views.push_back(lazy_view);
        }
    }

    iterator begin() {
        if (_total_item_num == 0) {
            return end();
        }
        return {this, _lazy_views.data(), _lazy_views[0].lazy_data->data() + _lazy_views[0].begin, 0, 0};
    }

    iterator end() {
        return {this, &_lazy_views.back(), _lazy_views.back().lazy_data->data() + _lazy_views.back().end,
                _lazy_views.back().end, _total_item_num};
    }

    T &operator[](size_t i) {
        SCHECK(i < _total_item_num) << "index out of bound";
        size_t idx = i;
        if (idx < _total_item_num) {
            lazy_view_t *lazy_view_ptr = _lazy_views.data();
            // CAUTION: size_t cannot be negative
            while (idx + lazy_view_ptr->begin >= lazy_view_ptr->end) {
                idx -= lazy_view_ptr->end - lazy_view_ptr->begin;
                ++lazy_view_ptr;
            }
            return *(lazy_view_ptr->lazy_data->data() + idx + lazy_view_ptr->begin);
        } else {
            return *begin();
        }
    };

    size_t size() {
        return _total_item_num;
    }

    /**
     * cut at the idx th item
     * move the tail part to tail
     * @param idx
     * @param tail
     */
    void cut(size_t idx, LazyVector<T> &tail) {
        if (idx < _total_item_num) {
            size_t data_id = 0;
            size_t data_num = _lazy_views.size();
            size_t new_total_item_num = idx;

            // Step1, identify cut which data
            lazy_view_t *lazy_view_ptr = _lazy_views.data();
            // CAUTION: size_t cannot be negative
            while (idx + lazy_view_ptr->begin >= lazy_view_ptr->end) {
                idx -= lazy_view_ptr->end - lazy_view_ptr->begin;
                ++lazy_view_ptr;
                ++data_id;
            }

            // Step2, move the rear part to tail
            tail.clear_without_append();
            tail.push_back({lazy_view_ptr->begin + idx, lazy_view_ptr->end, true, lazy_view_ptr->lazy_data});

            for (size_t i = 1; i < data_num - data_id; ++i) {
                tail.push_back(std::move(*(lazy_view_ptr + i)));
            }

            // Step3, truncate this vector
            _lazy_views.resize(data_id + 1);
            _lazy_views[data_id].end = idx + _lazy_views[data_id].begin;
            _lazy_views[data_id].pushable = false;
            _total_item_num = new_total_item_num;

        }
    }

    void merge(LazyVector<T> &&another_lazy_vector) {
        // 可以用 std::make_move_iterator 优化
        _lazy_views.insert(_lazy_views.end(), another_lazy_vector._lazy_views.begin(),
                           another_lazy_vector._lazy_views.end());
        _total_item_num += another_lazy_vector._total_item_num;
    }

    LazyVector<T> sub(size_t begin_idx, size_t end_idx) {
        LazyVector<T> sub_vector;
        sub_vector.clear();
        if (begin_idx < end_idx && end_idx <= _total_item_num) {
            sub_vector.clear_without_append();
            size_t idx = begin_idx;
            size_t begin_data_id = 0;

            // Step1, identify begin in which data
            lazy_view_t *begin_meta_ptr = _lazy_views.data();
            while (idx + begin_meta_ptr->begin >= begin_meta_ptr->end) {
                idx -= begin_meta_ptr->end - begin_meta_ptr->begin;
                ++begin_meta_ptr;
                ++begin_data_id;
            }

            // Step2, identify end in which data
            lazy_view_t *end_meta_ptr = begin_meta_ptr;
            size_t end_data_id = begin_data_id;
            size_t local_end_idx = idx + end_idx - begin_idx;

            while (local_end_idx + end_meta_ptr->begin >= end_meta_ptr->end) {
                local_end_idx -= end_meta_ptr->end - end_meta_ptr->begin;
                ++end_meta_ptr;
                ++end_data_id;
            }

            // Step3 Push
            if (begin_data_id == end_data_id) {
                sub_vector.push_back({begin_meta_ptr->begin + idx, begin_meta_ptr->begin + local_end_idx, false,
                                      begin_meta_ptr->lazy_data});
            } else {
                sub_vector.push_back({begin_meta_ptr->begin + idx, begin_meta_ptr->end, begin_meta_ptr->pushable,
                                      begin_meta_ptr->lazy_data});
                for (size_t i = 1; i < end_data_id - begin_data_id; ++i) {
                    sub_vector.push_back(*(begin_meta_ptr + i));
                }
                sub_vector.push_back(
                        {end_meta_ptr->begin, end_meta_ptr->begin + local_end_idx, false, end_meta_ptr->lazy_data});
            }

        }

        return std::move(sub_vector);
    }

    LazyVector<T> sub(iterator begin, iterator end) {
        return std::move(sub(begin._global_idx, end._global_idx));
    }

    /**
     * Shrink all lazy_views into single lazy_view
     */
    void shrink() {
        lazy_view_t new_lazy_view = {0, 0, true, std::make_shared<std::vector<T>>()};
        for (lazy_view_t &lazy_view: _lazy_views) {
            if (lazy_view.end - lazy_view.begin > 0) {
                new_lazy_view.lazy_data->insert(new_lazy_view.lazy_data->end(),
                                                lazy_view.lazy_data->data() + lazy_view.begin, lazy_view.lazy_data->data() + lazy_view.end);
                new_lazy_view.end += lazy_view.end - lazy_view.begin;
            }
        }
        _lazy_views.clear();
        _lazy_views.push_back(std::move(new_lazy_view));
    }


private:

    std::vector <lazy_view_t> _lazy_views;
    size_t _total_item_num = 0;

    void append_a_new_lazy_data() {
        lazy_view_t lazy_view = {0, 0, true, std::make_shared<std::vector<T>>()};
        _lazy_views.push_back(std::move(lazy_view));
    }

    // using for test
    T &get(size_t i, size_t j) {
        if (i < _lazy_views.size() && j < _lazy_views[i].lazy_data->size()) {
            return _lazy_views[i].lazy_data->get(j);
        }
        // return a random value
        return *_lazy_views[0].lazy_data->data();
    }

    void clear_without_append() {
        _lazy_views.clear();
        _total_item_num = 0;
    }

};


} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_LAZYVECTOR_H
