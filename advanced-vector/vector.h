#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <new>
#include <memory>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other) 
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                for (size_t i = 0; i != std::min(size_, rhs.size_); ++i) {
                    data_[i] = rhs[i];
                }
                if (size_ > rhs.size_) {
                    std::destroy_n(&data_[rhs.size_], size_ - rhs.size_);
                } else if (size_ < rhs.size_) {
                    std::uninitialized_copy_n(&rhs.data_[size_], rhs.size_ - size_, &data_[size_]);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(&data_[size_], new_size - size_);
        } else if (new_size < size_) {
            std::destroy_n(&data_[new_size], size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_memory(size_ == 0 ? 1 : size_ * 2);
            new (&new_memory[size_]) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_memory.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_memory.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_memory);
        } else {
            new (&data_[size_]) T(value);
        }
        ++size_;
    }

    void PushBack(T&& value) {
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_memory(size_ == 0 ? 1 : size_ * 2);
            new (&new_memory[size_]) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_memory.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_memory.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_memory);
        } else {
            new (&data_[size_]) T(std::move(value));
        }
        ++size_;
    }

    void PopBack() {
        if (size_ == 0) {
            throw std::logic_error("Trying to pop element in zero-size Vector.");
        }
        std::destroy_at(&data_[size_ - 1]);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_memory(size_ == 0 ? 1 : size_ * 2);
            new (&new_memory[size_]) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_memory.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_memory.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_memory);
        } else {
            new (&data_[size_]) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return reinterpret_cast<iterator>(data_.GetAddress());
    }

    iterator end() noexcept {
        return reinterpret_cast<iterator>(data_.GetAddress() + size_);
    }
    const_iterator begin() const noexcept {
        return reinterpret_cast<const_iterator>(data_.GetAddress());
    }

    const_iterator end() const noexcept {
        return reinterpret_cast<const_iterator>(data_.GetAddress() + size_);
    }

    const_iterator cbegin() const noexcept {
        return reinterpret_cast<const_iterator>(data_.GetAddress());
    }

    const_iterator cend() const noexcept {
        return reinterpret_cast<const_iterator>(data_.GetAddress() + size_);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t pos_id = pos - cbegin();
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_memory(size_ == 0 ? 1 : size_ * 2);
            new (new_memory + pos_id) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move_n(data_.GetAddress(), pos_id, new_memory.GetAddress());
                } catch (...) {
                    std::destroy(data_ + pos_id, data_ + pos_id + 1);
                    throw;
                }
                try {
                    std::uninitialized_move_n(data_ + pos_id, size_ - pos_id, new_memory + pos_id + 1);
                } catch (...) {
                    std::destroy(begin(), data_ + pos_id + 1);
                    throw;
                }
            } else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), pos_id, new_memory.GetAddress());
                } catch (...) {
                    std::destroy(data_ + pos_id, data_ + pos_id + 1);
                    throw;
                }
                try {
                    std::uninitialized_copy_n(data_ + pos_id, size_ - pos_id, new_memory + pos_id + 1);
                } catch (...) {
                    std::destroy(begin(), data_ + pos_id + 1);
                    throw;
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_memory);
        } else {
            if (pos == end()) {
                new (end()) T(std::forward<Args>(args)...);
            } else {
                T new_obj(std::forward<Args>(args)...);
                new (end()) T(std::forward<T>(data_[size_ - 1]));
                std::move_backward(begin() + pos_id, end() - 1, end());
                data_[pos_id] = std::forward<T>(new_obj);
            }
        }
        ++size_;
        return &data_[pos_id];
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        size_t pos_id = pos - cbegin();
        for (size_t i = size_ - pos_id - 1, cur_pos = pos_id; i != 0; --i, ++cur_pos) {
            data_[cur_pos] = std::forward<T>(data_[cur_pos + 1]);
        }
        std::destroy_at(end() - 1);
        --size_;
        return &data_[pos_id];
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::forward<T>(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
