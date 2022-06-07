// Copyright (c)2022 Fundament Software
// For conditions of distribution and use, see copyright notice in "Backend.h"

#ifndef GL__ERROR_H
#define GL__ERROR_H

#include "compiler.h"
#include "glad/glad.h"
#include <type_traits>
#include <utility>
#include <assert.h>
#include <coroutine>

#define GL_ERROR(name)  \
  if(GLError e{ name }) \
  {                     \
    return e;           \
  }

namespace GL {
  class Backend;

  // Wrapper around an openGL error code and source, with a debug checker that ensures the error was handled.
  class GLError
  {
  public:
    constexpr GLError() noexcept :
#ifdef _DEBUG
      _error(GL_NO_ERROR | UNCHECKED_FLAG),
#else
      _error(GL_NO_ERROR),
#endif
      _context(nullptr)
    {}
    constexpr GLError(GLError&& right) noexcept : _error(right._error), _context(right._context)
    {
      right._error   = GL_NO_ERROR;
      right._context = nullptr;
    }

    constexpr explicit GLError(const char* context) noexcept :
#ifdef _DEBUG
      _error(glGetError() | UNCHECKED_FLAG),
#else
      _error(glGetError()),
#endif
      _context(context)
    {}
    constexpr explicit GLError(GLenum err, const char* context) noexcept :
#ifdef _DEBUG
      _error(err | UNCHECKED_FLAG),
#else
      _error(err),
#endif
      _context(context)
    {}
    constexpr explicit GLError(std::in_place_t, GLenum err, const char* context) noexcept : _error(err), _context(context)
    {}
    constexpr ~GLError() { assert(!(_error & UNCHECKED_FLAG)); }

    constexpr void swap(GLError& right) noexcept
    {
      std::swap(_error, right._error);
      std::swap(_context, right._context);
    }

    // Returns true if there is an error and false if there isn't one (this is the opposite of GLExpected, necessary for
    // nice error handling)
    inline constexpr operator bool() noexcept { return has_error(); }
    inline constexpr bool has_error() noexcept
    {
      _checked();
      auto e = _clean();
      return e != INVALID_ERROR && e != GL_NO_ERROR;
    }
    inline constexpr bool peek() const noexcept { return _clean() == GL_NO_ERROR; }
    inline constexpr bool operator==(const GLError& e) noexcept { return e._error == _error; };
    inline constexpr bool operator!=(const GLError& e) noexcept { return e._error != _error; };
    // If there was an error, logs it to backend.
    GLenum log(Backend* backend);
    std::pair<GLenum, const char*> take() &&
    {
      // If this is an invalid error, we must leave it like that because _context could be invalid
      if(_error == INVALID_ERROR)
      {
        return { INVALID_ERROR, nullptr };
      }

      GLenum e = _error;
      _error   = GL_NO_ERROR;
      _context = nullptr;
      return { e, _context };
    }

    static const GLenum INVALID_ERROR = ~(1 << ((sizeof(GLenum) * 8) - 1));
#ifdef _DEBUG
    static const GLenum UNCHECKED_FLAG = ~INVALID_ERROR;
#endif

    inline constexpr GLError& operator=(GLError&& right) noexcept
    {
      this->~GLError();
      _error         = right._error;
      _context       = right._context;
      right._error   = GL_NO_ERROR;
      right._context = nullptr;
      return *this;
    }
    inline constexpr GLError& operator=(const GLError&) noexcept = delete;

  protected:
    inline constexpr GLenum _clean() const noexcept
    {
#ifdef _DEBUG
      return _error & (~UNCHECKED_FLAG);
#else
      return _error;
#endif
    }
    inline constexpr void _checked() noexcept
    {
#ifdef _DEBUG
      _error &= (~UNCHECKED_FLAG);
#endif
    }

    GLenum _error;
    const char* _context;
  };

  // This is a specialized version of what std::expected<T, GLError> would do, except it uses less space.
  template<class T> class GLExpected
  {
  public:
    using value_type = T;
    using error_type = GLError;

    template<class U> using rebind = GLExpected<U>;

    // constructors
    constexpr GLExpected() : _error(GLError::INVALID_ERROR) {}
    constexpr GLExpected(const GLExpected& right) = delete;
    constexpr explicit GLExpected(GLExpected&& right) noexcept : _error(right._error)
    {
      if(_error == GLError::INVALID_ERROR)
        return;

      if(peek())
      {
        new(&_result) T(std::move(right._result));
      }
      else
      {
        _context       = right._context;
        right._context = 0;
        right._error   = GLError::INVALID_ERROR;
      }
    }
    template<class U>
    constexpr explicit(!std::is_convertible_v<const U&, T>) GLExpected(GLExpected<U>&& right) noexcept :
      _error(right._error)
    {
      if(_error == GLError::INVALID_ERROR)
        return;

      if(peek())
      {
        new(&_result) T(std::move(right._result));
      }
      else
      {
        _context       = right._context;
        right._context = 0;
        right._error   = GLError::INVALID_ERROR;
      }
    }

    /* template<class U>
    using AllowDirectConversion = std::bool_constant<
      std::conjunction_v<std::negation<std::is_same<std::remove_cvref_t<U>, GLExpected>>,
                         std::negation<std::is_same<std::remove_cvref_t<U>, std::in_place_t>>, std::is_constructible<T,
    U>>>;*/

    template<class U = T>
    constexpr explicit(!std::is_convertible_v<U, T>) GLExpected(U&& right) :
      _error(GL_NO_ERROR | GLError::UNCHECKED_FLAG), _result(std::forward<U>(right))
    {}

    constexpr GLExpected(GLError&& right) noexcept
    {
      auto [err, ctx] = std::move(right).take();
      _error          = err;
      if(_error == GLError::INVALID_ERROR)
        return;

      _context       = ctx;
    }

    template<class... Args>
    constexpr explicit GLExpected(std::in_place_t, Args&&... args) :
      _error(GL_NO_ERROR | GLError::UNCHECKED_FLAG), _result(std::forward<Args>(args)...)
    {}
    template<class U, class... Args>
    constexpr explicit GLExpected(std::in_place_t, std::initializer_list<U> list, Args&&... args) :
      _error(GL_NO_ERROR | GLError::UNCHECKED_FLAG), _result(list, std::forward<Args>(args)...)
    {}

    // destructor
    constexpr ~GLExpected()
    {
      if(_error != GLError::INVALID_ERROR)
      {
        if(peek())
          _result.~T();
        assert(!(_error & GLError::UNCHECKED_FLAG));
      }
    }

    // assignment
    constexpr GLExpected& operator=(const GLExpected&) = delete;
    constexpr GLExpected& operator=(GLExpected&& right) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
      this->~GLExpected();

      _error = right._error;
      if(_error == GLError::INVALID_ERROR)
        return;

      if(peek())
      {
        new(&_result) T(std::move(right._result));
      }
      else
      {
        _context       = right._context;
        right._context = 0;
        right._error   = GLError::INVALID_ERROR;
      }
      return *this;
    }
    template<class U = T> constexpr GLExpected& operator=(U&& right)
    {
      this->~GLExpected();
      _error = GL_NO_ERROR | GLError::UNCHECKED_FLAG;
      new(&_result) T(std::forward<U>(right));
      return *this;
    }
    constexpr GLExpected& operator=(GLError&& e)
    {
      this->~GLExpected();
      auto [e, s] = std::move(e).take();
      _error      = e;
      _context    = s;
      return *this;
    }

    template<class... Args> constexpr T& emplace(Args&&... args) noexcept
    {
      this->~GLExpected();
      new(&_result) T(std::forward<Args>(args)...);
    }
    template<class U, class... Args> constexpr T& emplace(std::initializer_list<U> list, Args&&... args) noexcept
    {
      this->~GLExpected();
      new(&_result) T(list, std::forward<Args>(args)...);
    }

    // swap
    constexpr void swap(GLExpected& right) noexcept(std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_swappable_v<T>)
    {
      static_assert(std::is_move_constructible_v<T>, "GLExpected<T>::swap requires T to be move constructible.");
      static_assert(!std::is_move_constructible_v<T> || std::is_swappable_v<T>,
                    "GLExpected<T>::swap requires T to be swappable ");
      const bool valid = peek();
      if(valid == right.peek())
      {
        std::swap(this->_error, right._error);
        if(valid)
          std::swap(this->_result, right._result);
        else
          std::swap(this->_context, right._context);
      }
      else
      {
        GLExpected& source = valid ? *this : right;
        GLExpected& target = valid ? right : *this;

        auto err = target._error;
        auto ctx = target._context;
        target._Construct(std::move(*source));
        source._error = err;
        if(source._error != GLError::INVALID_ERROR)
        {
          source._context = ctx;
        }
      }
    }
    friend constexpr void swap(GLExpected& l, GLExpected& r) noexcept(
      std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_swappable_v<T>);

    // observers
    constexpr const T* operator->() const noexcept
    {
      assert(peek());
      return &_result;
    }

    constexpr T* operator->() noexcept
    {
      assert(peek());
      return &_result;
    }

    constexpr const T& operator*() const& noexcept { return value(); }
    constexpr T& operator*() & noexcept { return value(); }
    constexpr const T&& operator*() const&& noexcept { return value(); }
    constexpr T&& operator*() && noexcept { return value(); }
    inline constexpr explicit operator bool() const noexcept { return has_value(); }
    inline constexpr bool has_value() const noexcept
    {
      _checked();
      return peek();
    }
    inline constexpr bool has_error() const noexcept { return !has_value() && _error != GLError::INVALID_ERROR; }

    constexpr const T& value() const&
    {
      assert(peek());
      return _result;
    }

    constexpr T& value() &
    {
      assert(peek());
      return _result;
    }

    constexpr const T&& value() const&&
    {
      assert(peek());
      return _result;
    }

    constexpr T&& value() &&
    {
      assert(peek());
      return _result;
    }

    constexpr const GLError& error() const& { return _grab(); }
    constexpr GLError& error() & { return _grab(); }
    constexpr const GLError&& error() const&& { return _grab(); }
    constexpr GLError&& error() && { return _grab(); }

    template<class U> constexpr T value_or(U&& right) const&
    {
      static_assert(std::is_convertible_v<const T&, std::remove_cv_t<T>>,
                    "The const overload of GLExpected<T>::value_or requires const T& to be convertible to remove_cv_t<T> ");
      static_assert(std::is_convertible_v<U, T>, "GLExpected<T>::value_or(U) requires U to be convertible to T.");

      if(peek())
      {
        return _result;
      }

      return static_cast<std::remove_cv_t<T>>(std::forward<U>(right));
    }

    template<class U> constexpr T value_or(U&& right) &&
    {
      static_assert(std::is_convertible_v<T, std::remove_cv_t<T>>,
                    "The rvalue overload of GLExpected<T>::value_or requires T to be convertible to remove_cv_t<T> ");
      static_assert(std::is_convertible_v<U, T>, "GLExpected<T>::value_or(U) requires U to be convertible to T.");

      if(peek())
      {
        return std::move(_result);
      }

      return static_cast<std::remove_cv_t<T>>(std::forward<U>(right));
    }

    GLenum log(Backend* backend) noexcept
    {
      _checked();
      return _grab().log(backend);
    }
    constexpr GLError take() noexcept
    {
      assert(_error != GLError::INVALID_ERROR);
      auto e = _grab();
      if(!peek())
      {
        _error   = GLError::INVALID_ERROR;
        _context = nullptr;
      }

      return e;
    }
    inline constexpr bool peek() const noexcept
    {
#ifdef _DEBUG
      return (_error & (~GLError::UNCHECKED_FLAG)) == GL_NO_ERROR;
#else
      return _error == GL_NO_ERROR;
#endif
    }

    struct promise_type
    {
      GLExpected* expected;
      GLExpected get_return_object() { return { *this }; }
      void return_value(GLExpected result) { *expected = std::move(result); }

      std::suspend_never initial_suspend() const noexcept { return {}; }
      std::suspend_never final_suspend() const noexcept { return {}; }

      void unhandled_exception() noexcept {}
    };

    struct Awaiter
    {
      GLExpected expected;

      bool await_ready() { return expected.is_value(); }
      constexpr const T& await_resume() const& { return expected.value(); }
      constexpr T& await_resume() & { return expected.value(); }
      constexpr const T&& await_resume() const&& { return expected.value(); }
      constexpr T&& await_resume() const&& { return expected.value(); }
      void await_suspend(std::coroutine_handle<promise_type> handle)
      {
        *handle.promise().expected = expected;
        handle.destroy();
      }
    };

    Awaiter operator co_await() { return Awaiter{ *this }; }

  private:
    GLExpected(promise_type& promise) noexcept { promise.expected = this; }
    inline constexpr GLError _grab() noexcept { return GLError(std::in_place, _error, peek() ? nullptr : _context); }
    inline constexpr void _checked() noexcept
    {
#ifdef _DEBUG
      _error &= (~GLError::UNCHECKED_FLAG);
#endif
    }

    GLenum _error;
    union
    {
      const char* _context;
      T _result;
    };
  };

  // Void specialization
  template<class T>
  requires std::is_void_v<T>
  class GLExpected<T>
  {
  public:
    using value_type = T;
    using error_type = GLError;

    template<class U> using rebind = GLExpected<U>;

    // constructors
    constexpr GLExpected() noexcept {}
    constexpr explicit GLExpected(const GLExpected&) = delete;
    constexpr explicit GLExpected(GLExpected&& right) noexcept : GLError(std::move(right._error)) {}
    constexpr GLExpected(GLError&& right) noexcept : GLError(std::move(_error)) {}

    constexpr explicit GLExpected(std::in_place_t) noexcept {}

    // destructor
    constexpr ~GLExpected() {}

    // assignment
    constexpr GLExpected& operator=(GLExpected&& right) noexcept
    {
      _error = std::move(right._error);
      return *this;
    }
    constexpr GLExpected& operator=(GLError&& right) noexcept
    {
      _error           = std::move(right);
      return *this;
    }
    constexpr GLExpected& operator=(const GLExpected&) noexcept = delete;
    constexpr void emplace() noexcept { this->~GLExpected(); }

    // swap
    constexpr void swap(GLExpected& right) noexcept { _error.swap(right._error); }
    friend constexpr void swap(GLExpected&, GLExpected&) noexcept;

    // observers
    constexpr explicit operator bool() noexcept { return has_value(); }
    constexpr bool has_value() noexcept { return !has_error(); }
    constexpr void operator*() const noexcept {}
    constexpr void value() const& {}
    constexpr void value() && {}
    inline constexpr bool has_error() noexcept { return _error.has_error(); }
    constexpr const GLError& error() const& { return *this; }
    constexpr GLError& error() & { return *this; }
    constexpr const GLError&& error() const&& { return *this; }
    constexpr GLError&& error() && { return *this; }
    constexpr GLError take() noexcept { return std::move(*this); }
    bool log(Backend* backend) noexcept { return _error.log(backend); }
    inline constexpr bool peek() const noexcept { return _error.peek(); }

    // equality operators
    template<class T2>
    requires std::is_void_v<T2>
    friend constexpr bool operator==(const GLExpected& x, const GLExpected<T2>& y);

    GLError _error;
  };
}

#endif