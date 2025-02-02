/* Copyright 2017 https://github.com/mandreyel
 * Copyright 2023 Tinson Lai
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __MIO_MMAP_HEADER__
#define __MIO_MMAP_HEADER__

#include <cstdint>

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// #if !defined(_MSC_VER) || defined(_MSVC_TRADITIONAL) && !_MSVC_TRADITIONAL
// #define MIO_STD_PP
// #endif

#if __cplusplus >= 201402L
#  define __MIO_DEPRECATED [[deprecated]]
#  define __MIO_DEPRECATED_REASON(R) [[deprecated(R)]]
#elif defined(_MSC_VER)
#  define __MIO_DEPRECATED __declspec(deprecated)
#  define __MIO_DEPRECATED_REASON(R) __declspec(deprecated(R))
#elif defined(__GNUC__) || defined(__clang__)
#  define __MIO_DEPRECATED __attribute__((deprecated))
#  define __MIO_DEPRECATED_REASON(R) __attribute__((deprecated(R)))
#else
#  define __MIO_DEPRECATED
#  define __MIO_DEPRECATED_REASON(R)
#endif

#if __cplusplus >= 201703L
#  define __MIO_NODISCARD [[nodiscard]]
// _Check_return_ raises compiler error which is not identical to the standard attribute
// #elif defined(_MSC_VER)
// #  define __MIO_NODISCARD _Check_return_
#elif defined(__GNUC__) || defined(__clang__)
#  define __MIO_NODISCARD __attribute__((warn_unused_result))
#else
#  define __MIO_NODISCARD
#endif

#if __cplusplus >= 201703L
#  include <filesystem>
#  define MIO_FILESYSTEM_SUPPORT
#  include <string_view>
#  define MIO_STRING_VIEW_SUPPORT
#endif

#if __cplusplus >= 202002L
// #ifdef MIO_STD_PP
// #  define MIO_STD_PP_V
// #endif
#  ifdef __cpp_concepts
#    define MIO_CONCEPTS_SUPPORT
#  endif
// #  ifdef __cpp_lib_concepts
// #    include <concepts>
// #    define MIO_CONCEPTS_LIB_SUPPORT
// #  endif
#  ifdef __cpp_lib_ranges
#    include <ranges>
#    define MIO_RANGES_SUPPORT
#  endif
#endif

#ifdef MIO_CONCEPTS_SUPPORT
#  define __MIO_FUNCTION_TEMPLATE(R) template<typename F, typename... Args> requires std::is_invocable_r_v<R, F, Args...>
#elif __cplusplus >= 201703L
#  define __MIO_FUNCTION_TEMPLATE(R) template<typename F, typename... Args, typename = std::enable_if_t<std::is_invocable_r_v<R, F, Args...>>>
#else
#  define __MIO_FUNCTION_TEMPLATE(R) template<typename F, typename... Args>
#endif

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#    define __MIO_DEFINE_WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifdef __MIO_DEFINE_WIN32_LEAN_AND_MEAN
#    undef WIN32_LEAN_AND_MEAN
#    undef __MIO_DEFINE_WIN32_LEAN_AND_MEAN
#  endif
#  define __MIO_INVALID_HANDLE_VALUE INVALID_HANDLE_VALUE
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  define INVALID_HANDLE_VALUE -1
#  define __MIO_INVALID_HANDLE_VALUE -1
#endif

namespace mio
{

namespace experimental
{

enum class access_mode
{
	READ,
	WRITE
};

namespace detail
{

inline static void _last_error(std::error_code& ec) noexcept
{
#ifdef _WIN32
	ec.assign(GetLastError(), std::system_category());
#else
	ec.assign(errno, std::system_category());
#endif
}

struct handle_wrapper final
{
#ifdef _WIN32
	using underlying_type = HANDLE;
#else
	using underlying_type = int;
#endif
private:
	underlying_type _handle;
public:
	handle_wrapper() noexcept : _handle(__MIO_INVALID_HANDLE_VALUE) {}

	__MIO_FUNCTION_TEMPLATE(underlying_type)
	handle_wrapper(F&& f, Args&&... args) : _handle(f(std::forward<Args>(args)...))
	{
		if (_handle == __MIO_INVALID_HANDLE_VALUE)
		{
			std::error_code ec;
			_last_error(ec);
			throw std::system_error(ec);
		}
	}

	handle_wrapper(const handle_wrapper&) = delete;

	handle_wrapper(handle_wrapper&& o) noexcept : _handle(o._handle)
	{
		o._handle = __MIO_INVALID_HANDLE_VALUE;
	}

	handle_wrapper& operator=(const handle_wrapper&) = delete;

	handle_wrapper& operator=(handle_wrapper&& o)
	{
		close();
		std::swap(_handle, o._handle);
		return *this;
	}

	~handle_wrapper() noexcept
	{
		close();
	}

	inline void close()
	{
		std::error_code ec;
		close(ec);
		if (ec)
			throw std::system_error(ec);
	}

	void close(std::error_code& ec) noexcept
	{
		if (_handle != __MIO_INVALID_HANDLE_VALUE)
		{
#ifdef _WIN32
			if (!::CloseHandle(_handle))
#else
			if (::close(_handle))
#endif
				_last_error(ec);
			_handle = __MIO_INVALID_HANDLE_VALUE;
		}
	}

	__MIO_FUNCTION_TEMPLATE(underlying_type)
	inline void emplace(F&& f, Args&&... args) &
	{
		std::error_code ec;
		emplace(ec, std::forward<F>(f), std::forward<Args>(args)...);
		if (ec)
			throw std::system_error(ec);
	}

	__MIO_FUNCTION_TEMPLATE(underlying_type)
	void emplace(std::error_code& ec, F&& f, Args&... args) & noexcept
	{
		close(ec);
		if (ec)
			return;
		_handle = f(std::forward<Args>(args)...);
		if (_handle == __MIO_INVALID_HANDLE_VALUE)
			_last_error(ec);
	}

	__MIO_NODISCARD
	underlying_type raw() &
	{
		return _handle;
	}

	__MIO_NODISCARD
	bool valid() const noexcept
	{
		return _handle != __MIO_INVALID_HANDLE_VALUE;
	}
};

template<access_mode>
struct access_mode_trait;

template<>
struct access_mode_trait<access_mode::READ> final
{
#ifdef _WIN32
	static constexpr auto open_flags = GENERIC_READ;
	static constexpr auto access_flags = OPEN_EXISTING;
	static constexpr auto page_flags = PAGE_READONLY;
	static constexpr auto mmap_flags = FILE_MAP_READ;
#else
	static constexpr auto open_flags = O_RDONLY;
	static constexpr auto access_flags = PROT_READ;
	static constexpr auto mmap_flags = MAP_SHARED;
#endif
};

template<>
struct access_mode_trait<access_mode::WRITE> final
{
#ifdef _WIN32
	static constexpr auto open_flags = GENERIC_READ | GENERIC_WRITE;
	static constexpr auto access_flags = OPEN_ALWAYS;
	static constexpr auto page_flags = PAGE_READWRITE;
	static constexpr auto mmap_flags = FILE_MAP_READ | FILE_MAP_WRITE;
#else
	static constexpr auto open_flags = O_CREAT | O_RDWR;
	static constexpr auto access_flags = PROT_READ | PROT_WRITE;
	static constexpr auto mmap_flags = MAP_SHARED;
#endif
};

} // namespace mio::experimental::detail

#ifdef _WIN32
#  define __MIO_CREATE_FILE_PARAMETER(S) ::CreateFile##S, path.c_str(), _trait::open_flags, FILE_SHARE_READ | FILE_SHARE_WRITE, _trait::access_flags, FILE_ATTRIBUTE_NORMAL, nullptr
#else
#  define __MIO_CREATE_FILE_PARAMETER() ::open, path.c_str(), _trait::open_flags
#endif

template<access_mode M>
class file_mmap final
{
	using _trait = detail::access_mode_trait<M>;

	__MIO_NODISCARD
	inline static size_t _align_offset(size_t offset) noexcept
	{
#ifdef _WIN32
		::SYSTEM_INFO info;
		::GetSystemInfo(&info);
		size_t page_size = info.dwAllocationGranularity;
#else
		size_t page_size = ::sysconf(_SC_PAGE_SIZE);
#endif
		return offset / page_size * page_size;
	}

	detail::handle_wrapper _handle;
#ifdef _WIN32
	detail::handle_wrapper _mmap_handle;
#endif
	size_t _size, _offset;
	void *_ptr;

	inline void _map()
	{
		std::error_code ec;
		_map(ec);
		if (ec)
			throw std::system_error(ec);
		if (!_ptr)
			throw std::length_error("cannot map empty file");
	}

	inline void _map(std::error_code& ec) noexcept
	{
		if (!_size)
		{
			_size = file_size(ec);
			if (ec || !_size)
				return;
		}
		size_t page_offset;
		if (_offset)
		{
			page_offset = _align_offset(_offset);
			_offset -= page_offset;
		}
		else
			page_offset = 0;
#ifdef _WIN32
		size_t total_size = _offset + _size;
		_mmap_handle.emplace(
			ec,
			::CreateFileMappingW,
			_handle.raw(),
			_trait::page_flags,
			total_size >> 32,
			total_size & 0xffffffff,
			nullptr
		);
		if (ec)
			return;
		_ptr = ::MapViewOfFile(
			_mmap_handle.raw(),
			_trait::mmap_flags,
			page_offset >> 32,
			page_offset & 0xffffffff,
			total_size
		);
		if (!_ptr)
		{
			detail::_last_error(ec);
			std::error_code hec;
			_mmap_handle.close(hec);
			if (hec)
				ec = std::make_error_code(std::errc::state_not_recoverable);
		}
#else
		_ptr = ::mmap(
			nullptr,
			_offset + _size,
			_trait::access_flags,
			_trait::mmap_flags,
			_handle.raw(),
			page_offset
		);
		if (_ptr == MAP_FAILED)
			detail::_last_error(ec);
#endif
	}

#ifdef _WIN32
	void _open(std::error_code& ec, const std::wstring& path) noexcept
	{
		_handle.emplace(ec, __MIO_CREATE_FILE_PARAMETER(W));
	}

	__MIO_DEPRECATED_REASON("ANSI version of Windows API is considered deprecated")
#endif
	void _open(std::error_code& ec, const std::string& path) noexcept
	{
#ifdef _WIN32
		_handle.emplace(ec, __MIO_CREATE_FILE_PARAMETER(A));
#else
		_handle.emplace(ec, __MIO_CREATE_FILE_PARAMETER());
#endif
	}
public:
	file_mmap() noexcept :
		_handle(),
#ifdef _WIN32
		_mmap_handle(),
#endif
		_size(0),
		_offset(0),
		_ptr(nullptr)
	{}

#ifdef _WIN32
	file_mmap(const std::wstring& path, size_t size = 0, size_t offset = 0) :
		_handle(__MIO_CREATE_FILE_PARAMETER(W)),
		_mmap_handle(),
		_size(size),
		_offset(offset),
		_ptr(nullptr)
	{
		_map();
	}

	__MIO_DEPRECATED_REASON("ANSI version of Windows API is considered deprecated")
#endif
	file_mmap(const std::string& path, size_t size = 0, size_t offset = 0) :
#ifdef _WIN32
		_handle(__MIO_CREATE_FILE_PARAMETER(A)),
		_mmap_handle(),
#else
		_handle(__MIO_CREATE_FILE_PARAMETER()),
#endif
		_size(size),
		_offset(offset),
		_ptr(nullptr)
	{
		_map();
	}

#ifdef MIO_FILESYSTEM_SUPPORT
	inline file_mmap(const std::filesystem::path& path, size_t size = 0, size_t offset = 0) :
		file_mmap(path.native(), size, offset)
	{}
#endif

	__MIO_NODISCARD
	inline size_t file_size()
	{
		std::error_code ec;
		size_t size = file_size(ec);
		if (ec)
			throw std::system_error(ec);
		return size;
	}

	__MIO_NODISCARD
	size_t file_size(std::error_code& ec) noexcept
	{
#if _WIN32
		DWORD high, low = ::GetFileSize(_handle.unsafe(), &high);
		if (low == INVALID_FILE_SIZE)
		{
			auto e = GetLastError();
			if (e != ERROR_SUCCESS)
				ec.assign(e, std::system_category());
		}
		return (high << 32) | low;
#else
		struct stat s;
		if (::fstat(_handle.raw(), &s))
			ec.assign(errno, std::system_category());
		return s.st_size;
#endif
	}

	void map(size_t size = 0, size_t offset = 0)
	{
		_size = size;
		_offset = offset;
		_map();
	}

	void map(std::error_code& ec, size_t size = 0, size_t offset = 0) noexcept
	{
		_size = size;
		_offset = offset;
		_map(ec);
	}

	__MIO_NODISCARD
	bool mapped() const noexcept
	{
		return _ptr;
	}

	template<typename P>
	void open(std::error_code& ec, P&& path)
	{
		_open(ec, std::forward<P>(path));
	}

#ifdef MIO_FILESYSTEM_SUPPORT
	void open(std::error_code& ec, const std::filesystem::path& path) noexcept
	{
		_open(ec, path.native());
	}
#endif

	__MIO_NODISCARD
	bool opened() const noexcept
	{
		return _handle.valid();
	}

	size_t resize(size_t size)
	{
#ifdef _WIN32
		LONG high = size >> 32, low = ::SetFilePointer(_handle.unsafe(), size & 0xffffffff, &high, FILE_BEGIN);
		if (low == INVALID_SET_FILE_POINTER)
		{
			auto ec = GetLastError();
			if (ec != ERROR_SUCCESS)
				throw std::system_error(ec, std::system_category());
		}
		if (!::SetEndOfFile(_handle.unsafe()))
			throw std::system_error(GetLastError(), std::system_category());
		return (high << 32) | low;
#else
		if (::ftruncate(_handle.raw(), size))
			throw std::system_error(errno, std::system_category());
		return size;
#endif
	}
};

#undef __MIO_CREATE_FILE_PARAMETER

} // namespace mio::experimental

/**
 * This is used by `basic_mmap` to determine whether to create a read-only or
 * a read-write memory mapping.
 */
enum class access_mode
{
	read,
	write
};

// This value may be provided as the `length` parameter to the constructor or
// `map`, in which case a memory mapping of the entire file is created.
enum { map_entire_file = 0 };

#ifdef _WIN32
using file_handle_type = HANDLE;
#else
using file_handle_type = int;
#endif

template<access_mode AccessMode, typename ByteT>
struct basic_mmap
{
	using value_type = ByteT;
	using size_type = size_t;
	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = value_type*;
	using const_pointer = const value_type*;
	using difference_type = std::ptrdiff_t;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using iterator_category = std::random_access_iterator_tag;
	using handle_type = file_handle_type;

	static_assert(sizeof(ByteT) == sizeof(char), "ByteT must be the same size as char.");

private:
	// Points to the first requested byte, and not to the actual start of the mapping.
	pointer data_ = nullptr;

	// Length--in bytes--requested by user (which may not be the length of the
	// full mapping) and the length of the full mapping.
	size_type length_ = 0;
	size_type mapped_length_ = 0;

	// Letting user map a file using both an existing file handle and a path
	// introcudes some complexity (see `is_handle_internal_`).
	// On POSIX, we only need a file handle to create a mapping, while on
	// Windows systems the file handle is necessary to retrieve a file mapping
	// handle, but any subsequent operations on the mapped region must be done
	// through the latter.
	handle_type file_handle_ = INVALID_HANDLE_VALUE;
#ifdef _WIN32
	handle_type file_mapping_handle_ = INVALID_HANDLE_VALUE;
#endif

	// Letting user map a file using both an existing file handle and a path
	// introcudes some complexity in that we must not close the file handle if
	// user provided it, but we must close it if we obtained it using the
	// provided path. For this reason, this flag is used to determine when to
	// close `file_handle_`.
	bool is_handle_internal_;

public:
	/**
	 * The default constructed mmap object is in a non-mapped state, that is,
	 * any operation that attempts to access nonexistent underlying data will
	 * result in undefined behaviour/segmentation faults.
	 */
	basic_mmap() = default;

#ifdef __cpp_exceptions
	/**
	 * The same as invoking the `map` function, except any error that may occur
	 * while establishing the mapping is wrapped in a `std::system_error` and is
	 * thrown.
	 */
	template<typename String>
	basic_mmap(const String& path, const size_type offset = 0, const size_type length = map_entire_file)
	{
		std::error_code error;
		map(path, offset, length, error);
		if(error) { throw std::system_error(error); }
	}

	/**
	 * The same as invoking the `map` function, except any error that may occur
	 * while establishing the mapping is wrapped in a `std::system_error` and is
	 * thrown.
	 */
	basic_mmap(const handle_type handle, const size_type offset = 0, const size_type length = map_entire_file)
	{
		std::error_code error;
		map(handle, offset, length, error);
		if(error) { throw std::system_error(error); }
	}
#endif // __cpp_exceptions

	/**
	 * `basic_mmap` has single-ownership semantics, so transferring ownership
	 * may only be accomplished by moving the object.
	 */
	basic_mmap(const basic_mmap&) = delete;
	basic_mmap(basic_mmap&&);
	basic_mmap& operator=(const basic_mmap&) = delete;
	basic_mmap& operator=(basic_mmap&&);

	/**
	 * If this is a read-write mapping, the destructor invokes sync. Regardless
	 * of the access mode, unmap is invoked as a final step.
	 */
	~basic_mmap();

	/**
	 * On UNIX systems 'file_handle' and 'mapping_handle' are the same. On Windows,
	 * however, a mapped region of a file gets its own handle, which is returned by
	 * 'mapping_handle'.
	 */
	handle_type file_handle() const noexcept { return file_handle_; }
	handle_type mapping_handle() const noexcept;

	/** Returns whether a valid memory mapping has been created. */
	bool is_open() const noexcept { return file_handle_ != INVALID_HANDLE_VALUE; }

	/**
	 * Returns true if no mapping was established, that is, conceptually the
	 * same as though the length that was mapped was 0. This function is
	 * provided so that this class has Container semantics.
	 */
	bool empty() const noexcept { return length() == 0; }

	/** Returns true if a mapping was established. */
	bool is_mapped() const noexcept;

	/**
	 * `size` and `length` both return the logical length, i.e. the number of bytes
	 * user requested to be mapped, while `mapped_length` returns the actual number of
	 * bytes that were mapped which is a multiple of the underlying operating system's
	 * page allocation granularity.
	 */
	size_type size() const noexcept { return length(); }
	size_type length() const noexcept { return length_; }
	size_type mapped_length() const noexcept { return mapped_length_; }

	/** Returns the offset relative to the start of the mapping. */
	size_type mapping_offset() const noexcept
	{
		return mapped_length_ - length_;
	}

	/**
	 * Returns a pointer to the first requested byte, or `nullptr` if no memory mapping
	 * exists.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> pointer data() noexcept { return data_; }
	const_pointer data() const noexcept { return data_; }

	/**
	 * Returns an iterator to the first requested byte, if a valid memory mapping
	 * exists, otherwise this function call is undefined behaviour.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> iterator begin() noexcept { return data(); }
	const_iterator begin() const noexcept { return data(); }
	const_iterator cbegin() const noexcept { return data(); }

	/**
	 * Returns an iterator one past the last requested byte, if a valid memory mapping
	 * exists, otherwise this function call is undefined behaviour.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> iterator end() noexcept { return data() + length(); }
	const_iterator end() const noexcept { return data() + length(); }
	const_iterator cend() const noexcept { return data() + length(); }

	/**
	 * Returns a reverse iterator to the last memory mapped byte, if a valid
	 * memory mapping exists, otherwise this function call is undefined
	 * behaviour.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	const_reverse_iterator rbegin() const noexcept
	{ return const_reverse_iterator(end()); }
	const_reverse_iterator crbegin() const noexcept
	{ return const_reverse_iterator(end()); }

	/**
	 * Returns a reverse iterator past the first mapped byte, if a valid memory
	 * mapping exists, otherwise this function call is undefined behaviour.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
	const_reverse_iterator rend() const noexcept
	{ return const_reverse_iterator(begin()); }
	const_reverse_iterator crend() const noexcept
	{ return const_reverse_iterator(begin()); }

	/**
	 * Returns a reference to the `i`th byte from the first requested byte (as returned
	 * by `data`). If this is invoked when no valid memory mapping has been created
	 * prior to this call, undefined behaviour ensues.
	 */
	reference operator[](const size_type i) noexcept { return data_[i]; }
	const_reference operator[](const size_type i) const noexcept { return data_[i]; }

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
	 * reason is reported via `error` and the object remains in a state as if this
	 * function hadn't been called.
	 *
	 * `path`, which must be a path to an existing file, is used to retrieve a file
	 * handle (which is closed when the object destructs or `unmap` is called), which is
	 * then used to memory map the requested region. Upon failure, `error` is set to
	 * indicate the reason and the object remains in an unmapped state.
	 *
	 * `offset` is the number of bytes, relative to the start of the file, where the
	 * mapping should begin. When specifying it, there is no need to worry about
	 * providing a value that is aligned with the operating system's page allocation
	 * granularity. This is adjusted by the implementation such that the first requested
	 * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
	 * `offset` from the start of the file.
	 *
	 * `length` is the number of bytes to map. It may be `map_entire_file`, in which
	 * case a mapping of the entire file is created.
	 */
	template<typename String>
	void map(const String& path, const size_type offset,
			const size_type length, std::error_code& error);

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
	 * reason is reported via `error` and the object remains in a state as if this
	 * function hadn't been called.
	 *
	 * `path`, which must be a path to an existing file, is used to retrieve a file
	 * handle (which is closed when the object destructs or `unmap` is called), which is
	 * then used to memory map the requested region. Upon failure, `error` is set to
	 * indicate the reason and the object remains in an unmapped state.
	 *
	 * The entire file is mapped.
	 */
	template<typename String>
	void map(const String& path, std::error_code& error)
	{
		map(path, 0, map_entire_file, error);
	}

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is
	 * unsuccesful, the reason is reported via `error` and the object remains in
	 * a state as if this function hadn't been called.
	 *
	 * `handle`, which must be a valid file handle, which is used to memory map the
	 * requested region. Upon failure, `error` is set to indicate the reason and the
	 * object remains in an unmapped state.
	 *
	 * `offset` is the number of bytes, relative to the start of the file, where the
	 * mapping should begin. When specifying it, there is no need to worry about
	 * providing a value that is aligned with the operating system's page allocation
	 * granularity. This is adjusted by the implementation such that the first requested
	 * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
	 * `offset` from the start of the file.
	 *
	 * `length` is the number of bytes to map. It may be `map_entire_file`, in which
	 * case a mapping of the entire file is created.
	 */
	void map(const handle_type handle, const size_type offset,
			const size_type length, std::error_code& error);

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is
	 * unsuccesful, the reason is reported via `error` and the object remains in
	 * a state as if this function hadn't been called.
	 *
	 * `handle`, which must be a valid file handle, which is used to memory map the
	 * requested region. Upon failure, `error` is set to indicate the reason and the
	 * object remains in an unmapped state.
	 *
	 * The entire file is mapped.
	 */
	void map(const handle_type handle, std::error_code& error)
	{
		map(handle, 0, map_entire_file, error);
	}

	/**
	 * If a valid memory mapping has been created prior to this call, this call
	 * instructs the kernel to unmap the memory region and disassociate this object
	 * from the file.
	 *
	 * The file handle associated with the file that is mapped is only closed if the
	 * mapping was created using a file path. If, on the other hand, an existing
	 * file handle was used to create the mapping, the file handle is not closed.
	 */
	void unmap();

	void swap(basic_mmap& other);

	/** Flushes the memory mapped page to disk. Errors are reported via `error`. */
	template<access_mode A = AccessMode>
	typename std::enable_if<A == access_mode::write, void>::type
	sync(std::error_code& error);

	/**
	 * All operators compare the address of the first byte and size of the two mapped
	 * regions.
	 */

private:
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> pointer get_mapping_start() noexcept
	{
		return !data() ? nullptr : data() - mapping_offset();
	}

	const_pointer get_mapping_start() const noexcept
	{
		return !data() ? nullptr : data() - mapping_offset();
	}

	/**
	 * The destructor syncs changes to disk if `AccessMode` is `write`, but not
	 * if it's `read`, but since the destructor cannot be templated, we need to
	 * do SFINAE in a dedicated function, where one syncs and the other is a noop.
	 */
	template<access_mode A = AccessMode>
	typename std::enable_if<A == access_mode::write, void>::type
	conditional_sync();
	template<access_mode A = AccessMode>
	typename std::enable_if<A == access_mode::read, void>::type conditional_sync();
};

template<access_mode AccessMode, typename ByteT>
bool operator==(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator!=(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator<(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator<=(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator>(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator>=(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b);

/**
 * This is the basis for all read-only mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using basic_mmap_source = basic_mmap<access_mode::read, ByteT>;

/**
 * This is the basis for all read-write mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using basic_mmap_sink = basic_mmap<access_mode::write, ByteT>;

/**
 * These aliases cover the most common use cases, both representing a raw byte stream
 * (either with a char or an unsigned char/uint8_t).
 */
using mmap_source = basic_mmap_source<char>;
using ummap_source = basic_mmap_source<unsigned char>;

using mmap_sink = basic_mmap_sink<char>;
using ummap_sink = basic_mmap_sink<unsigned char>;

/**
 * Convenience factory method that constructs a mapping for any `basic_mmap` or
 * `basic_mmap` type.
 */
template<
	typename MMap,
	typename MappingToken
> MMap make_mmap(const MappingToken& token,
		int64_t offset, int64_t length, std::error_code& error)
{
	MMap mmap;
	mmap.map(token, offset, length, error);
	return mmap;
}

/**
 * Convenience factory method.
 *
 * MappingToken may be a String (`std::string`, `std::string_view`, `const char*`,
 * `std::filesystem::path`, `std::vector<char>`, or similar), or a
 * `mmap_source::handle_type`.
 */
template<typename MappingToken>
mmap_source make_mmap_source(const MappingToken& token, mmap_source::size_type offset,
		mmap_source::size_type length, std::error_code& error)
{
	return make_mmap<mmap_source>(token, offset, length, error);
}

template<typename MappingToken>
mmap_source make_mmap_source(const MappingToken& token, std::error_code& error)
{
	return make_mmap_source(token, 0, map_entire_file, error);
}

/**
 * Convenience factory method.
 *
 * MappingToken may be a String (`std::string`, `std::string_view`, `const char*`,
 * `std::filesystem::path`, `std::vector<char>`, or similar), or a
 * `mmap_sink::handle_type`.
 */
template<typename MappingToken>
mmap_sink make_mmap_sink(const MappingToken& token, mmap_sink::size_type offset,
		mmap_sink::size_type length, std::error_code& error)
{
	return make_mmap<mmap_sink>(token, offset, length, error);
}

template<typename MappingToken>
mmap_sink make_mmap_sink(const MappingToken& token, std::error_code& error)
{
	return make_mmap_sink(token, 0, map_entire_file, error);
}

/**
 * Exposes (nearly) the same interface as `basic_mmap`, but endowes it with
 * `std::shared_ptr` semantics.
 *
 * This is not the default behaviour of `basic_mmap` to avoid allocating on the heap if
 * shared semantics are not required.
 */
template<
	access_mode AccessMode,
	typename ByteT
> class basic_shared_mmap
{
	using impl_type = basic_mmap<AccessMode, ByteT>;
	std::shared_ptr<impl_type> pimpl_;

public:
	using value_type = typename impl_type::value_type;
	using size_type = typename impl_type::size_type;
	using reference = typename impl_type::reference;
	using const_reference = typename impl_type::const_reference;
	using pointer = typename impl_type::pointer;
	using const_pointer = typename impl_type::const_pointer;
	using difference_type = typename impl_type::difference_type;
	using iterator = typename impl_type::iterator;
	using const_iterator = typename impl_type::const_iterator;
	using reverse_iterator = typename impl_type::reverse_iterator;
	using const_reverse_iterator = typename impl_type::const_reverse_iterator;
	using iterator_category = typename impl_type::iterator_category;
	using handle_type = typename impl_type::handle_type;
	using mmap_type = impl_type;

	basic_shared_mmap() = default;
	basic_shared_mmap(const basic_shared_mmap&) = default;
	basic_shared_mmap& operator=(const basic_shared_mmap&) = default;
	basic_shared_mmap(basic_shared_mmap&&) = default;
	basic_shared_mmap& operator=(basic_shared_mmap&&) = default;

	/** Takes ownership of an existing mmap object. */
	basic_shared_mmap(mmap_type&& mmap)
		: pimpl_(std::make_shared<mmap_type>(std::move(mmap)))
	{}

	/** Takes ownership of an existing mmap object. */
	basic_shared_mmap& operator=(mmap_type&& mmap)
	{
		pimpl_ = std::make_shared<mmap_type>(std::move(mmap));
		return *this;
	}

	/** Initializes this object with an already established shared mmap. */
	basic_shared_mmap(std::shared_ptr<mmap_type> mmap) : pimpl_(std::move(mmap)) {}

	/** Initializes this object with an already established shared mmap. */
	basic_shared_mmap& operator=(std::shared_ptr<mmap_type> mmap)
	{
		pimpl_ = std::move(mmap);
		return *this;
	}

#ifdef __cpp_exceptions
	/**
	 * The same as invoking the `map` function, except any error that may occur
	 * while establishing the mapping is wrapped in a `std::system_error` and is
	 * thrown.
	 */
	template<typename String>
	basic_shared_mmap(const String& path, const size_type offset = 0, const size_type length = map_entire_file)
	{
		std::error_code error;
		map(path, offset, length, error);
		if (error) { throw std::system_error(error); }
	}

	/**
	 * The same as invoking the `map` function, except any error that may occur
	 * while establishing the mapping is wrapped in a `std::system_error` and is
	 * thrown.
	 */
	basic_shared_mmap(const handle_type handle, const size_type offset = 0, const size_type length = map_entire_file)
	{
		std::error_code error;
		map(handle, offset, length, error);
		if (error) { throw std::system_error(error); }
	}
#endif // __cpp_exceptions

	/**
	 * If this is a read-write mapping and the last reference to the mapping,
	 * the destructor invokes sync. Regardless of the access mode, unmap is
	 * invoked as a final step.
	 */
	~basic_shared_mmap() = default;

	/** Returns the underlying `std::shared_ptr` instance that holds the mmap. */
	std::shared_ptr<mmap_type> get_shared_ptr() { return pimpl_; }

	/**
	 * On UNIX systems 'file_handle' and 'mapping_handle' are the same. On Windows,
	 * however, a mapped region of a file gets its own handle, which is returned by
	 * 'mapping_handle'.
	 */
	handle_type file_handle() const noexcept
	{
		return pimpl_ ? pimpl_->file_handle() : INVALID_HANDLE_VALUE;
	}

	handle_type mapping_handle() const noexcept
	{
		return pimpl_ ? pimpl_->mapping_handle() : INVALID_HANDLE_VALUE;
	}

	/** Returns whether a valid memory mapping has been created. */
	bool is_open() const noexcept { return pimpl_ && pimpl_->is_open(); }

	/**
	 * Returns true if no mapping was established, that is, conceptually the
	 * same as though the length that was mapped was 0. This function is
	 * provided so that this class has Container semantics.
	 */
	bool empty() const noexcept { return !pimpl_ || pimpl_->empty(); }

	/**
	 * `size` and `length` both return the logical length, i.e. the number of bytes
	 * user requested to be mapped, while `mapped_length` returns the actual number of
	 * bytes that were mapped which is a multiple of the underlying operating system's
	 * page allocation granularity.
	 */
	size_type size() const noexcept { return pimpl_ ? pimpl_->length() : 0; }
	size_type length() const noexcept { return pimpl_ ? pimpl_->length() : 0; }
	size_type mapped_length() const noexcept
	{
		return pimpl_ ? pimpl_->mapped_length() : 0;
	}

	/**
	 * Returns a pointer to the first requested byte, or `nullptr` if no memory mapping
	 * exists.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> pointer data() noexcept { return pimpl_->data(); }
	const_pointer data() const noexcept { return pimpl_ ? pimpl_->data() : nullptr; }

	/**
	 * Returns an iterator to the first requested byte, if a valid memory mapping
	 * exists, otherwise this function call is undefined behaviour.
	 */
	iterator begin() noexcept { return pimpl_->begin(); }
	const_iterator begin() const noexcept { return pimpl_->begin(); }
	const_iterator cbegin() const noexcept { return pimpl_->cbegin(); }

	/**
	 * Returns an iterator one past the last requested byte, if a valid memory mapping
	 * exists, otherwise this function call is undefined behaviour.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> iterator end() noexcept { return pimpl_->end(); }
	const_iterator end() const noexcept { return pimpl_->end(); }
	const_iterator cend() const noexcept { return pimpl_->cend(); }

	/**
	 * Returns a reverse iterator to the last memory mapped byte, if a valid
	 * memory mapping exists, otherwise this function call is undefined
	 * behaviour.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> reverse_iterator rbegin() noexcept { return pimpl_->rbegin(); }
	const_reverse_iterator rbegin() const noexcept { return pimpl_->rbegin(); }
	const_reverse_iterator crbegin() const noexcept { return pimpl_->crbegin(); }

	/**
	 * Returns a reverse iterator past the first mapped byte, if a valid memory
	 * mapping exists, otherwise this function call is undefined behaviour.
	 */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> reverse_iterator rend() noexcept { return pimpl_->rend(); }
	const_reverse_iterator rend() const noexcept { return pimpl_->rend(); }
	const_reverse_iterator crend() const noexcept { return pimpl_->crend(); }

	/**
	 * Returns a reference to the `i`th byte from the first requested byte (as returned
	 * by `data`). If this is invoked when no valid memory mapping has been created
	 * prior to this call, undefined behaviour ensues.
	 */
	reference operator[](const size_type i) noexcept { return (*pimpl_)[i]; }
	const_reference operator[](const size_type i) const noexcept { return (*pimpl_)[i]; }

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
	 * reason is reported via `error` and the object remains in a state as if this
	 * function hadn't been called.
	 *
	 * `path`, which must be a path to an existing file, is used to retrieve a file
	 * handle (which is closed when the object destructs or `unmap` is called), which is
	 * then used to memory map the requested region. Upon failure, `error` is set to
	 * indicate the reason and the object remains in an unmapped state.
	 *
	 * `offset` is the number of bytes, relative to the start of the file, where the
	 * mapping should begin. When specifying it, there is no need to worry about
	 * providing a value that is aligned with the operating system's page allocation
	 * granularity. This is adjusted by the implementation such that the first requested
	 * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
	 * `offset` from the start of the file.
	 *
	 * `length` is the number of bytes to map. It may be `map_entire_file`, in which
	 * case a mapping of the entire file is created.
	 */
	template<typename String>
	void map(const String& path, const size_type offset,
		const size_type length, std::error_code& error)
	{
		map_impl(path, offset, length, error);
	}

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
	 * reason is reported via `error` and the object remains in a state as if this
	 * function hadn't been called.
	 *
	 * `path`, which must be a path to an existing file, is used to retrieve a file
	 * handle (which is closed when the object destructs or `unmap` is called), which is
	 * then used to memory map the requested region. Upon failure, `error` is set to
	 * indicate the reason and the object remains in an unmapped state.
	 *
	 * The entire file is mapped.
	 */
	template<typename String>
	void map(const String& path, std::error_code& error)
	{
		map_impl(path, 0, map_entire_file, error);
	}

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
	 * reason is reported via `error` and the object remains in a state as if this
	 * function hadn't been called.
	 *
	 * `handle`, which must be a valid file handle, which is used to memory map the
	 * requested region. Upon failure, `error` is set to indicate the reason and the
	 * object remains in an unmapped state.
	 *
	 * `offset` is the number of bytes, relative to the start of the file, where the
	 * mapping should begin. When specifying it, there is no need to worry about
	 * providing a value that is aligned with the operating system's page allocation
	 * granularity. This is adjusted by the implementation such that the first requested
	 * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
	 * `offset` from the start of the file.
	 *
	 * `length` is the number of bytes to map. It may be `map_entire_file`, in which
	 * case a mapping of the entire file is created.
	 */
	void map(const handle_type handle, const size_type offset,
		const size_type length, std::error_code& error)
	{
		map_impl(handle, offset, length, error);
	}

	/**
	 * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
	 * reason is reported via `error` and the object remains in a state as if this
	 * function hadn't been called.
	 *
	 * `handle`, which must be a valid file handle, which is used to memory map the
	 * requested region. Upon failure, `error` is set to indicate the reason and the
	 * object remains in an unmapped state.
	 *
	 * The entire file is mapped.
	 */
	void map(const handle_type handle, std::error_code& error)
	{
		map_impl(handle, 0, map_entire_file, error);
	}

	/**
	 * If a valid memory mapping has been created prior to this call, this call
	 * instructs the kernel to unmap the memory region and disassociate this object
	 * from the file.
	 *
	 * The file handle associated with the file that is mapped is only closed if the
	 * mapping was created using a file path. If, on the other hand, an existing
	 * file handle was used to create the mapping, the file handle is not closed.
	 */
	void unmap() { if (pimpl_) pimpl_->unmap(); }

	void swap(basic_shared_mmap& other) { pimpl_.swap(other.pimpl_); }

	/** Flushes the memory mapped page to disk. Errors are reported via `error`. */
	template<
		access_mode A = AccessMode,
		typename = typename std::enable_if<A == access_mode::write>::type
	> void sync(std::error_code& error) { if (pimpl_) pimpl_->sync(error); }

	/** All operators compare the underlying `basic_mmap`'s addresses. */

	friend bool operator==(const basic_shared_mmap& a, const basic_shared_mmap& b)
	{
		return a.pimpl_ == b.pimpl_;
	}

	friend bool operator!=(const basic_shared_mmap& a, const basic_shared_mmap& b)
	{
		return !(a == b);
	}

	friend bool operator<(const basic_shared_mmap& a, const basic_shared_mmap& b)
	{
		return a.pimpl_ < b.pimpl_;
	}

	friend bool operator<=(const basic_shared_mmap& a, const basic_shared_mmap& b)
	{
		return a.pimpl_ <= b.pimpl_;
	}

	friend bool operator>(const basic_shared_mmap& a, const basic_shared_mmap& b)
	{
		return a.pimpl_ > b.pimpl_;
	}

	friend bool operator>=(const basic_shared_mmap& a, const basic_shared_mmap& b)
	{
		return a.pimpl_ >= b.pimpl_;
	}

private:
	template<typename MappingToken>
	void map_impl(const MappingToken& token, const size_type offset,
		const size_type length, std::error_code& error)
	{
		if (!pimpl_)
		{
			mmap_type mmap = make_mmap<mmap_type>(token, offset, length, error);
			if (error) { return; }
			pimpl_ = std::make_shared<mmap_type>(std::move(mmap));
		}
		else
		{
			pimpl_->map(token, offset, length, error);
		}
	}
};

/**
 * This is the basis for all read-only mmap objects and should be preferred over
 * directly using basic_shared_mmap.
 */
template<typename ByteT>
using basic_shared_mmap_source = basic_shared_mmap<access_mode::read, ByteT>;

/**
 * This is the basis for all read-write mmap objects and should be preferred over
 * directly using basic_shared_mmap.
 */
template<typename ByteT>
using basic_shared_mmap_sink = basic_shared_mmap<access_mode::write, ByteT>;

/**
 * These aliases cover the most common use cases, both representing a raw byte stream
 * (either with a char or an unsigned char/uint8_t).
 */
using shared_mmap_source = basic_shared_mmap_source<char>;
using shared_ummap_source = basic_shared_mmap_source<unsigned char>;

using shared_mmap_sink = basic_shared_mmap_sink<char>;
using shared_ummap_sink = basic_shared_mmap_sink<unsigned char>;

/**
 * Determines the operating system's page allocation granularity.
 *
 * On the first call to this function, it invokes the operating system specific syscall
 * to determine the page size, caches the value, and returns it. Any subsequent call to
 * this function serves the cached value, so no further syscalls are made.
 */
inline size_t page_size()
{
	static const size_t page_size = []
	{
#ifdef _WIN32
		SYSTEM_INFO SystemInfo;
		GetSystemInfo(&SystemInfo);
		return SystemInfo.dwAllocationGranularity;
#else
		return sysconf(_SC_PAGE_SIZE);
#endif
	}();
	return page_size;
}

/**
 * Alligns `offset` to the operating's system page size such that it subtracts the
 * difference until the nearest page boundary before `offset`, or does nothing if
 * `offset` is already page aligned.
 */
inline size_t make_offset_page_aligned(size_t offset) noexcept
{
	const size_t page_size_ = page_size();
	// Use integer division to round down to the nearest page alignment.
	return offset / page_size_ * page_size_;
}

namespace detail
{

template<typename T>
inline const T *c_str(const std::basic_string<T>& s) noexcept
{
	return s.c_str();
}

template<typename T>
inline bool empty(const std::basic_string<T>& s) noexcept
{
	return s.empty();
}

#ifdef MIO_FILESYSTEM_SUPPORT

inline auto c_str(const std::filesystem::path& s) noexcept
{
	return s.c_str();
}

inline auto empty(const std::filesystem::path& s) noexcept
{
	return s.empty();
}

#endif

#define __MIO_DETAIL_EMPTY_EXPAND(T) \
inline bool empty(const T *s) noexcept \
{ \
	return s == nullptr || *s == 0; \
}

#define __MIO_DETAIL_C_STR_EXPAND(T) \
inline const T *c_str(const T *s) noexcept \
{ \
	return s; \
}

__MIO_DETAIL_EMPTY_EXPAND(char)

__MIO_DETAIL_C_STR_EXPAND(char)

#ifdef _WIN32

__MIO_DETAIL_EMPTY_EXPAND(wchar_t)

__MIO_DETAIL_C_STR_EXPAND(wchar_t)

namespace win
{

/** Returns the 4 upper bytes of an 8-byte integer. */
inline DWORD int64_high(int64_t n) noexcept
{
	return n >> 32;
}

/** Returns the 4 lower bytes of an 8-byte integer. */
inline DWORD int64_low(int64_t n) noexcept
{
	return n & 0xffffffff;
}

#define __MIO_DETAIL_WIN32_CREATE_FILE(F, P, M) \
F( \
	P, \
	M == access_mode::read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, \
	FILE_SHARE_READ | FILE_SHARE_WRITE, \
	0, \
	OPEN_EXISTING, \
	FILE_ATTRIBUTE_NORMAL, \
	0 \
);

__MIO_DEPRECATED
__MIO_NODISCARD
inline file_handle_type open_file_helper(LPCSTR path, const access_mode mode)
{
	return __MIO_DETAIL_WIN32_CREATE_FILE(::CreateFileA, path, mode);
}

__MIO_DEPRECATED
__MIO_NODISCARD
inline file_handle_type open_file_helper(LPCWSTR path, const access_mode mode)
{
	return __MIO_DETAIL_WIN32_CREATE_FILE(::CreateFileW, path, mode);
}

__MIO_DEPRECATED
__MIO_NODISCARD
inline file_handle_type open_file_helper(const std::string& path, const access_mode mode)
{
	return __MIO_DETAIL_WIN32_CREATE_FILE(::CreateFileA, path.c_str(), mode);
}

__MIO_NODISCARD
inline file_handle_type open_file_helper(const std::wstring& path, const access_mode mode)
{
	return __MIO_DETAIL_WIN32_CREATE_FILE(::CreateFileW, path.c_str(), mode);
}

#undef __MIO_DETAIL_WIN32_CREATE_FILE

#ifdef MIO_FILESYSTEM_SUPPORT

__MIO_NODISCARD
inline file_handle_type open_file_helper(const std::filesystem::path& path, const access_mode mode)
{
	return open_file_helper(path.wstring(), mode);
}

#endif

#ifdef MIO_RANGES_SUPPORT

__MIO_NODISCARD
inline file_handle_type open_file_helper(const std::ranges::contiguous_range auto& path, const access_mode mode)
{
	return open_file_helper(
		std::basic_string<std::ranges::range_value_t<path>> { std::ranges::data(path), std::ranges::size(path) },
		mode
	);
}

#else

template<typename T, size_t N>
__MIO_NODISCARD
inline file_handle_type open_file_helper(const T (&path)[N], const access_mode mode)
{
	return open_file_helper(std::basic_string<T> { path, N }, mode);
}

template<typename T, size_t N>
__MIO_NODISCARD
inline file_handle_type open_file_helper(const std::array<T, N>& path, const access_mode mode)
{
	return open_file_helper(std::basic_string<T> { path.data(), N }, mode);
}

template<typename T>
__MIO_NODISCARD
inline file_handle_type open_file_helper(const std::vector<T>& path, const access_mode mode)
{
	return open_file_helper(std::basic_string<T> { path.data(), path.size() }, mode);
}

#endif

#ifdef MIO_STRING_VIEW_SUPPORT

template<typename T>
__MIO_NODISCARD
inline file_handle_type open_file_helper(const std::basic_string_view<T> path, const access_mode mode)
{
	return open_file_helper(std::basic_string<T>(path), mode);
}

#endif

} // win

#endif // _WIN32

/**
 * Returns the last platform specific system error (errno on POSIX and
 * GetLastError on Win) as a `std::error_code`.
 */
inline std::error_code last_error() noexcept
{
	std::error_code error;
#ifdef _WIN32
	error.assign(GetLastError(), std::system_category());
#else
	error.assign(errno, std::system_category());
#endif
	return error;
}

template<typename String>
file_handle_type open_file(const String& path, const access_mode mode,
		std::error_code& error)
{
	error.clear();
	if(detail::empty(path))
	{
		error = std::make_error_code(std::errc::invalid_argument);
		return INVALID_HANDLE_VALUE;
	}
#ifdef _WIN32
	const auto handle = win::open_file_helper(path, mode);
#else // POSIX
	const auto handle = ::open(c_str(path),
			mode == access_mode::read ? O_RDONLY : O_RDWR);
#endif
	if(handle == INVALID_HANDLE_VALUE)
	{
		error = detail::last_error();
	}
	return handle;
}

inline size_t query_file_size(file_handle_type handle, std::error_code& error)
{
	error.clear();
#ifdef _WIN32
	LARGE_INTEGER file_size;
	if(::GetFileSizeEx(handle, &file_size) == 0)
	{
		error = detail::last_error();
		return 0;
	}
	return static_cast<int64_t>(file_size.QuadPart);
#else // POSIX
	struct stat sbuf;
	if(::fstat(handle, &sbuf) == -1)
	{
		error = detail::last_error();
		return 0;
	}
	return sbuf.st_size;
#endif
}

struct mmap_context
{
	char* data;
	int64_t length;
	int64_t mapped_length;
#ifdef _WIN32
	file_handle_type file_mapping_handle;
#endif
};

inline mmap_context memory_map(const file_handle_type file_handle, const int64_t offset,
	const int64_t length, const access_mode mode, std::error_code& error)
{
	const int64_t aligned_offset = make_offset_page_aligned(offset);
	const int64_t length_to_map = offset - aligned_offset + length;
#ifdef _WIN32
	const int64_t max_file_size = offset + length;
	const auto file_mapping_handle = ::CreateFileMapping(
			file_handle,
			0,
			mode == access_mode::read ? PAGE_READONLY : PAGE_READWRITE,
			win::int64_high(max_file_size),
			win::int64_low(max_file_size),
			0);
	if(file_mapping_handle == INVALID_HANDLE_VALUE)
	{
		error = detail::last_error();
		return {};
	}
	char* mapping_start = static_cast<char*>(::MapViewOfFile(
			file_mapping_handle,
			mode == access_mode::read ? FILE_MAP_READ : FILE_MAP_WRITE,
			win::int64_high(aligned_offset),
			win::int64_low(aligned_offset),
			length_to_map));
	if(mapping_start == nullptr)
	{
		// Close file handle if mapping it failed.
		::CloseHandle(file_mapping_handle);
		error = detail::last_error();
		return {};
	}
#else // POSIX
	char* mapping_start = static_cast<char*>(::mmap(
			0, // Don't give hint as to where to map.
			length_to_map,
			mode == access_mode::read ? PROT_READ : PROT_WRITE,
			MAP_SHARED,
			file_handle,
			aligned_offset));
	if(mapping_start == MAP_FAILED)
	{
		error = detail::last_error();
		return {};
	}
#endif
	mmap_context ctx;
	ctx.data = mapping_start + offset - aligned_offset;
	ctx.length = length;
	ctx.mapped_length = length_to_map;
#ifdef _WIN32
	ctx.file_mapping_handle = file_mapping_handle;
#endif
	return ctx;
}

} // namespace detail

// -- basic_mmap --

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>::~basic_mmap()
{
	conditional_sync();
	unmap();
}

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>::basic_mmap(basic_mmap&& other)
	: data_(std::move(other.data_))
	, length_(std::move(other.length_))
	, mapped_length_(std::move(other.mapped_length_))
	, file_handle_(std::move(other.file_handle_))
#ifdef _WIN32
	, file_mapping_handle_(std::move(other.file_mapping_handle_))
#endif
	, is_handle_internal_(std::move(other.is_handle_internal_))
{
	other.data_ = nullptr;
	other.length_ = other.mapped_length_ = 0;
	other.file_handle_ = INVALID_HANDLE_VALUE;
#ifdef _WIN32
	other.file_mapping_handle_ = INVALID_HANDLE_VALUE;
#endif
}

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>&
basic_mmap<AccessMode, ByteT>::operator=(basic_mmap&& other)
{
	if(this != &other)
	{
		// First the existing mapping needs to be removed.
		unmap();
		data_ = std::move(other.data_);
		length_ = std::move(other.length_);
		mapped_length_ = std::move(other.mapped_length_);
		file_handle_ = std::move(other.file_handle_);
#ifdef _WIN32
		file_mapping_handle_ = std::move(other.file_mapping_handle_);
#endif
		is_handle_internal_ = std::move(other.is_handle_internal_);

		// The moved from basic_mmap's fields need to be reset, because
		// otherwise other's destructor will unmap the same mapping that was
		// just moved into this.
		other.data_ = nullptr;
		other.length_ = other.mapped_length_ = 0;
		other.file_handle_ = INVALID_HANDLE_VALUE;
#ifdef _WIN32
		other.file_mapping_handle_ = INVALID_HANDLE_VALUE;
#endif
		other.is_handle_internal_ = false;
	}
	return *this;
}

template<access_mode AccessMode, typename ByteT>
typename basic_mmap<AccessMode, ByteT>::handle_type
basic_mmap<AccessMode, ByteT>::mapping_handle() const noexcept
{
#ifdef _WIN32
	return file_mapping_handle_;
#else
	return file_handle_;
#endif
}

template<access_mode AccessMode, typename ByteT>
template<typename String>
void basic_mmap<AccessMode, ByteT>::map(const String& path, const size_type offset,
		const size_type length, std::error_code& error)
{
	error.clear();
	if(detail::empty(path))
	{
		error = std::make_error_code(std::errc::invalid_argument);
		return;
	}
	const auto handle = detail::open_file(path, AccessMode, error);
	if(error)
	{
		return;
	}

	map(handle, offset, length, error);
	// This MUST be after the call to map, as that sets this to true.
	if(!error)
	{
		is_handle_internal_ = true;
	}
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::map(const handle_type handle,
		const size_type offset, const size_type length, std::error_code& error)
{
	error.clear();
	if(handle == INVALID_HANDLE_VALUE)
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return;
	}

	const auto file_size = detail::query_file_size(handle, error);
	if(error)
	{
		return;
	}

	if(offset + length > file_size)
	{
		error = std::make_error_code(std::errc::invalid_argument);
		return;
	}

	const auto ctx = detail::memory_map(handle, offset,
			length == map_entire_file ? (file_size - offset) : length,
			AccessMode, error);
	if(!error)
	{
		// We must unmap the previous mapping that may have existed prior to this call.
		// Note that this must only be invoked after a new mapping has been created in
		// order to provide the strong guarantee that, should the new mapping fail, the
		// `map` function leaves this instance in a state as though the function had
		// never been invoked.
		unmap();
		file_handle_ = handle;
		is_handle_internal_ = false;
		data_ = reinterpret_cast<pointer>(ctx.data);
		length_ = ctx.length;
		mapped_length_ = ctx.mapped_length;
#ifdef _WIN32
		file_mapping_handle_ = ctx.file_mapping_handle;
#endif
	}
}

template<access_mode AccessMode, typename ByteT>
template<access_mode A>
typename std::enable_if<A == access_mode::write, void>::type
basic_mmap<AccessMode, ByteT>::sync(std::error_code& error)
{
	error.clear();
	if(!is_open())
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return;
	}

	if(data())
	{
#ifdef _WIN32
		if(::FlushViewOfFile(get_mapping_start(), mapped_length_) == 0
		   || ::FlushFileBuffers(file_handle_) == 0)
#else // POSIX
		if(::msync(get_mapping_start(), mapped_length_, MS_SYNC) != 0)
#endif
		{
			error = detail::last_error();
			return;
		}
	}
#ifdef _WIN32
	if(::FlushFileBuffers(file_handle_) == 0)
	{
		error = detail::last_error();
	}
#endif
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::unmap()
{
	if(!is_open()) { return; }
	// TODO do we care about errors here?
#ifdef _WIN32
	if(is_mapped())
	{
		::UnmapViewOfFile(get_mapping_start());
		::CloseHandle(file_mapping_handle_);
	}
#else // POSIX
	if(data_) { ::munmap(const_cast<pointer>(get_mapping_start()), mapped_length_); }
#endif

	// If `file_handle_` was obtained by our opening it (when map is called with
	// a path, rather than an existing file handle), we need to close it,
	// otherwise it must not be closed as it may still be used outside this
	// instance.
	if(is_handle_internal_)
	{
#ifdef _WIN32
		::CloseHandle(file_handle_);
#else // POSIX
		::close(file_handle_);
#endif
	}

	// Reset fields to their default values.
	data_ = nullptr;
	length_ = mapped_length_ = 0;
	file_handle_ = INVALID_HANDLE_VALUE;
#ifdef _WIN32
	file_mapping_handle_ = INVALID_HANDLE_VALUE;
#endif
}

template<access_mode AccessMode, typename ByteT>
bool basic_mmap<AccessMode, ByteT>::is_mapped() const noexcept
{
#ifdef _WIN32
	return file_mapping_handle_ != INVALID_HANDLE_VALUE;
#else // POSIX
	return is_open();
#endif
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::swap(basic_mmap& other)
{
	if(this != &other)
	{
		using std::swap;
		swap(data_, other.data_);
		swap(file_handle_, other.file_handle_);
#ifdef _WIN32
		swap(file_mapping_handle_, other.file_mapping_handle_);
#endif
		swap(length_, other.length_);
		swap(mapped_length_, other.mapped_length_);
		swap(is_handle_internal_, other.is_handle_internal_);
	}
}

template<access_mode AccessMode, typename ByteT>
template<access_mode A>
typename std::enable_if<A == access_mode::write, void>::type
basic_mmap<AccessMode, ByteT>::conditional_sync()
{
	// This is invoked from the destructor, so not much we can do about
	// failures here.
	std::error_code ec;
	sync(ec);
}

template<access_mode AccessMode, typename ByteT>
template<access_mode A>
typename std::enable_if<A == access_mode::read, void>::type
basic_mmap<AccessMode, ByteT>::conditional_sync()
{
	// noop
}

template<access_mode AccessMode, typename ByteT>
bool operator==(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b)
{
	return a.data() == b.data()
		&& a.size() == b.size();
}

template<access_mode AccessMode, typename ByteT>
bool operator!=(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b)
{
	return !(a == b);
}

template<access_mode AccessMode, typename ByteT>
bool operator<(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b)
{
	if(a.data() == b.data()) { return a.size() < b.size(); }
	return a.data() < b.data();
}

template<access_mode AccessMode, typename ByteT>
bool operator<=(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b)
{
	return !(a > b);
}

template<access_mode AccessMode, typename ByteT>
bool operator>(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b)
{
	if(a.data() == b.data()) { return a.size() > b.size(); }
	return a.data() > b.data();
}

template<access_mode AccessMode, typename ByteT>
bool operator>=(const basic_mmap<AccessMode, ByteT>& a,
		const basic_mmap<AccessMode, ByteT>& b)
{
	return !(a < b);
}

} // namespace mio

#undef __MIO_RETURN_TYPE_HINT
#undef __MIO_DEPRECATED
#undef __MIO_DEPRECATED_REASON
#undef __MIO_NODISCARD
#undef __MIO_FUNCTION_TEMPLATE
#undef __MIO_INVALID_HANDLE_VALUE

#endif // __MIO_MMAP_HEADER__
