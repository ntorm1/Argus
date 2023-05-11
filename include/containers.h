#ifndef ARGUS_CONTAINERS_H
#define ARGUS_CONTAINERS_H

#include <queue>

#include "pybind11/numpy.h"
#include "utils_array.h"

namespace py = pybind11;

namespace Argus{

template <typename T>
struct ArrayWindow
{
    /// pointer to the start the array
    T* start_ptr;

    /// pointer to the end of the array
    T* end_ptr;

    /// stride of the array 
    size_t stride;

    /// number of elements in the array window 
    size_t length;

    /// latest value to be popped off of  the end
    T popped_element;

    ArrayWindow() = default;
    ArrayWindow(T* start_ptr, size_t stride, size_t length)
    {
        this->start_ptr = start_ptr;
        this->end_ptr = start_ptr + (stride * length);
        this->stride = stride;
        this->length = length;
    };

    /**
     * @brief step the sliding window forward one position
     * 
     */
    inline void step()
    {   
        // store the last element to be popped off
        this->popped_element = *this->start_ptr;

        // step the pointer forward
        this->start_ptr++;
        this->end_ptr++;
    }

    /**
     * @brief get the underlying window as a numpy array to return to python wrapper
     * 
     * @return py::array_t<T> numpy array of strided values
     */
    py::array_t<T> get_window()
    {
        return py::array( 
        py::buffer_info
            (
                this->start_ptr,                    /* Pointer to buffer */
                sizeof(T),                          /* Size of one scalar */
                py::format_descriptor<T>::format(), /* Python struct-style format descriptor */
                1,                                  /* Number of dimensions */
                { length },                         /* Buffer dimensions */
                { sizeof(T) * this->stride}
            )
        );
    }

    struct iterator {
        T* ptr;
        size_t stride;

        // Constructor
        iterator(T* p) : ptr(p) {}

        // Dereference operator
        T& operator*() const {
            return *ptr;
        }

        // Member access operator
        T* operator->() const {
            return ptr;
        }

        // Increment operator (prefix)
        // Increment operator (prefix)
        iterator& operator++() {
            ptr += stride;
            return *this;
        }

        // Increment operator (postfix)
        iterator operator++(int) {
            iterator temp = *this;
            ptr += stride;
            return temp;
        }

        // Equality operator
        bool operator==(const iterator& other) const {
            return ptr == other.ptr;
        }

        // Inequality operator
        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }
    };

    // Begin iterator
    iterator begin() const {
        return iterator(this->start_ptr, this->stride);
    }

    // End iterator
    iterator end() const {
        return iterator(this->end_ptr,this->stride);
    }
};

template <typename T>
class FixedDeque {
public:
    explicit FixedDeque(size_t max_size)
        : max_size_(max_size) {
    }

    void push_back(const T& value) {
        if (data_.size() == max_size_) {
            data_.pop_front();
        }
        data_.push_back(value);
    }

    void clear()
    {
        this->data_.clear();
    }

    size_t size() const {
        return data_.size();
    }

    const T& operator[](size_t index) const {
        return data_[index];
    }

    T& operator[](size_t index) {
        return data_[index];
    }

private:
    std::deque<T> data_;
    size_t max_size_;
};

}
#endif 