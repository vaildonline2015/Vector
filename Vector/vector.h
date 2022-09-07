#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
	}

	RawMemory(const RawMemory&) = delete;
	RawMemory& operator = (const RawMemory&) = delete;

	RawMemory(RawMemory&& other) noexcept :
		buffer_(std::exchange(other.buffer_, nullptr)),
		capacity_(std::exchange(other.capacity_, 0))
	{
	}

	RawMemory& operator = (RawMemory&& other) noexcept {
		Deallocate(buffer_);
		buffer_ = std::exchange(other.buffer_, nullptr);
		capacity_ = std::exchange(other.capacity_, 0);
		return *this;
	}

	~RawMemory() {
		Deallocate(buffer_);
	}

	T* operator+(size_t offset) noexcept {
		assert(offset <= capacity_);
		return buffer_ + offset;
	}

	const T* operator+(size_t offset) const noexcept {
		return const_cast<RawMemory&>(*this) + offset;
	}

	T& operator[](size_t index) noexcept {
		assert(index < capacity_);
		return buffer_[index];
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<RawMemory&>(*this)[index];
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

	static T* Allocate(size_t n) {
		return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
	}

	static void Deallocate(T* buf) noexcept {
		operator delete(buf);
	}

	T* buffer_ = nullptr;
	size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

	using iterator = T*;
	using const_iterator = const T*;

	iterator begin() noexcept {
		return data_ + 0;
	}
	iterator end() noexcept {
		return data_ + size_;
	}
	const_iterator begin() const noexcept {
		return data_ + 0;
	}
	const_iterator end() const noexcept {
		return data_ + size_;
	}
	const_iterator cbegin() const noexcept {
		return data_ + 0;
	}
	const_iterator cend() const noexcept {
		return data_ + size_;
	}

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
		std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
	}

	Vector& operator = (const Vector& other) {

		if (this != &other) {

			if (other.size_ > Capacity()) {
				Vector other_copy(other);
				Swap(other_copy);
			}
			else {
				size_t min_size = std::min(size_, other.size_);

				for (size_t i = 0; i < min_size; ++i) {
					data_[i] = other.data_[i];
				}
				if (size_ >= other.size_) {
					std::destroy_n(data_ + other.size_, size_ - other.size_);
				}
				else {
					std::uninitialized_copy_n(other.data_ + size_, other.size_ - size_, data_ + size_);
				}
				size_ = other.size_;
			}
		}
		return *this;
	}

	Vector(Vector&& other) noexcept : 
		data_(std::move(other.data_)),
		size_(std::exchange(other.size_, 0)) {
	}

	Vector& operator = (Vector&& other) noexcept {

		if (this != &other) {
			Swap(other);
			//	Vector move_other(std::move(other));
			//	Swap(move_other);
		}
		return *this;
	}

	~Vector() {
		std::destroy_n(data_.GetAddress(), size_);
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

	void Reserve(size_t new_capacity) {

		if (new_capacity <= data_.Capacity()) {
			return;
		}
		RawMemory<T> new_data(new_capacity);
		MakeNewData(new_data);
	}

	void Resize(size_t new_size) {
		if (new_size < size_) {
			std::destroy_n(data_ + new_size, size_ - new_size);
		}
		else if (new_size > size_) {
			Reserve(new_size);
			std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
		}
		size_ = new_size;
	}

	void PushBack(const T& value) {

		if (size_ != data_.Capacity()) {
			new (data_ + size_) T(value);
		}
		else {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data + size_) T(value);
			MakeNewDataSafe(new_data, new_data + size_);
		}
		++size_;
	}

	void PushBack(T&& value) {
		if (size_ != data_.Capacity()) {
			new (data_ + size_) T(std::move(value));
		}
		else {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data + size_) T(std::move(value));
			MakeNewDataSafe(new_data, new_data + size_);
		}
		++size_;
	}

	template <typename... Types>
	T& EmplaceBack(Types&&... args) {
		if (size_ != data_.Capacity()) {
			new (data_ + size_) T(std::forward<decltype(args)>(args)...);
		}
		else {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data + size_) T(std::forward<decltype(args)>(args)...);
			MakeNewDataSafe(new_data, new_data + size_);
		}
		return data_[size_++];
	}

	template <typename... Args>
	iterator Emplace(const_iterator cpos, Args&&... args) {

		assert(cpos >= cbegin() && cpos <= cend());

		if (size_ != data_.Capacity()) {

			if (size_ == 0) {
				new (data_.GetAddress()) T(std::forward<decltype(args)>(args)...);
				++size_;
				return data_.GetAddress();
			}
			else {

				T emplacing_element(std::forward<decltype(args)>(args)...);

				new (end()) T(std::move(data_[size_ - 1]));

				iterator pos = begin() + (cpos - cbegin());

				try {
					std::move_backward(pos, end() - 1, end());
				}
				catch (...) {
					std::destroy_at(end());
					throw;
				}
				*pos = std::move(emplacing_element);	
				++size_;
				return pos;
			}
		}
		else {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

			iterator pos = new_data + (cpos - cbegin());

			new (pos) T(std::forward<decltype(args)>(args)...);
			
			try {
				UninitializedCopyOrMoveN(data_.GetAddress(), cpos - cbegin(), new_data.GetAddress());
			}
			catch (...) {
				std::destroy_at(pos);
				throw;
			}
			try {
				iterator old_pos = data_ + (cpos - cbegin());
				UninitializedCopyOrMoveN(old_pos, cend() - cpos, pos + 1);
			}
			catch (...) {
				std::destroy_n(new_data.GetAddress(), cpos - cbegin() + 1);
				throw;
			}

			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);

			++size_;
			return pos;
		}
	}

	iterator Insert(const_iterator pos, const T& value) {
		return Emplace(pos, value);
	}

	iterator Insert(const_iterator pos, T&& value) {
		return Emplace(pos, std::move(value));
	}

	void PopBack() {

		if (size_) {
			std::destroy_at(data_ + (--size_));
		}
	}

	iterator Erase(const_iterator cpos) {

		assert(cpos >= cbegin() && cpos < cend());

		iterator pos = begin() + (cpos - cbegin());
		std::move(pos + 1, end(), pos);
		std::destroy_at(end() - 1);
		--size_;
		return pos;
	}


	void Swap(Vector& other) noexcept {
		data_.Swap(other.data_);
		std::swap(size_, other.size_);
	}

private:

	void UninitializedCopyOrMoveN(T* from, size_t N, T* to) {

		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(from, N, to);
		}
		else {
			std::uninitialized_copy_n(from, N, to);
		}
	}

	void MakeNewData(RawMemory<T>& new_data) {

		UninitializedCopyOrMoveN(data_.GetAddress(), size_, new_data.GetAddress());
		
		std::destroy_n(data_.GetAddress(), size_);
		data_.Swap(new_data);
	}

	void MakeNewDataSafe(RawMemory<T>& new_data, T* possible_leak_pos) {

		try {
			MakeNewData(new_data);
		}
		catch (...) {
			std::destroy_at(possible_leak_pos);
			throw;
		}
	}

	RawMemory<T> data_;
	size_t size_ = 0;
};