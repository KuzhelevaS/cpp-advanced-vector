#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <iostream>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
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
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept {
        Swap(other);
    }
    
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector temp (rhs);
                Swap(temp);
            } else {
                std::copy_n(
                    rhs.data_.GetAddress(),
                    std::min(rhs.size_, size_),
                    data_.GetAddress());
                std::uninitialized_copy_n(
                    rhs.data_.GetAddress() + std::min(rhs.size_, size_), 
                    rhs.size_ - std::min(rhs.size_, size_), 
                    data_.GetAddress() + std::min(rhs.size_, size_));
                if (rhs.size_<size_) {
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        MoveItemsInNewMemory(data_.GetAddress(), new_data.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        if (new_size > size_) {
            if (new_size > Capacity()) {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }
    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }
    
    void PopBack() noexcept {
        --size_;
        std::destroy_at(data_.GetAddress() + size_);
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        Emplace(begin() + size_, std::forward<Args>(args)...);
        return data_[size_ - 1];
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
        return data_.GetAddress()[index];
    }

    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return cbegin();
    }
    const_iterator end() const noexcept {
        return cend();
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t index = pos - begin();
        if (size_ == Capacity() || index == Capacity()) {
            InsertWithReallocate(index, std::forward<Args>(args)...);
        } else {
            InsertInPlace(index, std::forward<Args>(args)...);
        }
        ++size_;
        return begin() + index;
    }
    
    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        size_t index = pos - begin();
        std::move(begin() + index + 1, end(), begin() + index);
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        --size_;
        return begin() + index;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
private:
    RawMemory<T> data_;
    size_t size_ = 0;
    
    template <typename... Args>
    void InsertWithReallocate(size_t index, Args&&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);
        MoveItemsInNewMemory(data_.GetAddress(), new_data.GetAddress(), index);
        MoveItemsInNewMemory(data_.GetAddress() + index, new_data.GetAddress() + index + 1, size_ - index);
        data_.Swap(new_data);
    }
    
    template <typename... Args>
    void InsertInPlace(size_t index, Args&&... args) {
        if (index == size_) {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        } else {
            T temp (std::forward<Args>(args)...);
            new (data_.GetAddress() + size_) T(std::move(*(begin() + size_ -  1)));
            std::move_backward(begin() + index, end() - 1, begin() + size_);
            *(begin() + index) = std::move(temp);
        }
    }

    template <typename Pointer>
    void MoveItemsInNewMemory(Pointer from, Pointer to, size_t count) {
        // Конструируем элементы в new_data, копируя/перемещая их из data_
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, count, to);
        } else {
            std::uninitialized_copy_n(from, count, to);
        }
        // Разрушаем элементы в data_
        std::destroy_n(from, count);
    }
};
