/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef ASYNC_WRAPPER_HPP_
#define ASYNC_WRAPPER_HPP_

#include <functional>
#include <type_traits>
#include <future>
#include <tuple>
#if defined(ENABLE_CO_AWAIT)
#include <experimental/coroutine>
#endif // defined(ENABLE_CO_AWAIT)

namespace cue {

namespace detail {

struct placeholder_std_future_t final {};

struct placeholder_awaitable_t final {};

template <typename _Ty>
using is_std_future_placeholder = std::is_same<std::decay_t<_Ty>, placeholder_std_future_t>;

template <typename _Ty>
using is_awaitable_placeholder = std::is_same<std::decay_t<_Ty>, placeholder_awaitable_t>;

#if defined(ENABLE_CO_AWAIT)
template <typename _Ty>
class awaitable_promise;
#endif // defined(ENABLE_CO_AWAIT)

} // namespace detail

#if defined(ENABLE_CO_AWAIT)
template <typename _Ty>
class awaitable_future {
public:
    using promise_type = detail::awaitable_promise<_Ty>;

    awaitable_future() noexcept = default;
    awaitable_future(const awaitable_future&) = delete;
    awaitable_future& operator=(const awaitable_future&) = delete;

    awaitable_future(awaitable_future&& rhs) noexcept {
        if (std::addressof(rhs) != this) {
            promise_ = rhs.promise_;
            rhs.promise_ = nullptr;
        }
    }

    awaitable_future& operator=(awaitable_future&& rhs) noexcept {
        if (std::addressof(rhs) != this) {
            promise_ = rhs.promise_;
            rhs.promise_ = nullptr;
        }
        return *this;
    }

    // for coroutine
    bool await_ready() const noexcept {
        return promise_->ready();
    }

    // for coroutine
    void await_suspend(std::experimental::coroutine_handle<> handle) noexcept {
        promise_->suspend(std::move(handle));
    }

    // for coroutine
    _Ty await_resume() noexcept {
        return promise_->get();
    }

private:
    template <typename>
    friend class detail::awaitable_promise;

    explicit awaitable_future(detail::awaitable_promise<_Ty>* promise) noexcept : promise_{promise} {
    }

    detail::awaitable_promise<_Ty>* promise_{nullptr};
};

template <typename _Ty>
using awaitable_t = awaitable_future<_Ty>;

using awaitable = awaitable_t<void>;
#endif // defined(ENABLE_CO_AWAIT)

namespace placeholder {

constexpr detail::placeholder_std_future_t std_future{};
constexpr detail::placeholder_awaitable_t awaitable{};

} // namespace placeholder

namespace detail {

template <typename _Ty, typename _Tuple>
struct tuple_has_type;

template <typename _Ty>
struct tuple_has_type<_Ty, std::tuple<>> : std::false_type {};

template <typename _Ty, typename _Head, typename... _Args>
struct tuple_has_type<_Ty, std::tuple<_Head, _Args...>> : tuple_has_type<_Ty, std::tuple<_Args...>> {};

template <typename _Ty, typename... _Args>
struct tuple_has_type<_Ty, std::tuple<_Ty, _Args...>> : std::true_type {};

template <typename _Tuple>
using has_std_future = tuple_has_type<placeholder_std_future_t, _Tuple>;

template <typename _Tuple>
using has_awaitable = tuple_has_type<placeholder_awaitable_t, _Tuple>;

template <typename _Ty, typename _Tuple>
struct type_index;

template <typename _Ty, typename... _Args>
struct type_index<_Ty, std::tuple<_Ty, _Args...>> : public std::integral_constant<std::size_t, 0> {};

template <typename _Ty, typename _Head, typename... _Args>
struct type_index<_Ty, std::tuple<_Head, _Args...>>
    : public std::integral_constant<std::size_t, 1 + type_index<_Ty, std::tuple<_Args...>>{}> {};

template <typename _Tuple>
using std_future_index = type_index<placeholder_std_future_t, _Tuple>;

template <typename _Tuple>
using awaitable_index = type_index<placeholder_awaitable_t, _Tuple>;

template <typename _Ty>
struct function_args;

template <typename _Ret, typename... _Args>
struct function_args<_Ret(_Args...)> {
    using type = _Ret(_Args...);
    using function_type = std::function<type>;
    using return_type = _Ret;
    static constexpr std::size_t arity{sizeof...(_Args)};

    using args_tuple = std::tuple<std::decay_t<_Args>...>;

    template <std::size_t _Index>
    struct arg {
        using type = std::decay_t<typename std::tuple_element<_Index, std::tuple<_Args...>>::type>;
    };

    template <std::size_t _Index>
    using arg_t = typename arg<_Index>::type;
};

template <typename _Ret, typename _Arg>
struct function_args<_Ret(_Arg)> {
    using type = _Ret(_Arg);
    using function_type = std::function<type>;
    using return_type = _Ret;
    static constexpr std::size_t arity{1};

    using args_tuple = std::decay_t<_Arg>;

    template <std::size_t _Index>
    struct arg {
        using type = std::decay_t<_Arg>;
    };

    template <std::size_t _Index>
    using arg_t = typename arg<_Index>::type;
};

template <typename _Ret>
struct function_args<_Ret()> {
    using type = _Ret();
    using function_type = std::function<type>;
    using return_type = _Ret;
    static constexpr std::size_t arity{0};

    using args_tuple = void;

    template <std::size_t _Index>
    struct arg {
        using type = void;
    };

    template <std::size_t _Index>
    using arg_t = typename arg<_Index>::type;
};

template <typename _Ret, typename... _Args>
struct function_args<_Ret (*)(_Args...)> : function_args<_Ret(_Args...)> {};

template <typename _Ret, typename... _Args>
struct function_args<std::function<_Ret(_Args...)>> : function_args<_Ret(_Args...)> {};

template <typename _Ret, typename _Ty, typename... _Args>
struct function_args<_Ret (_Ty::*)(_Args...)> : function_args<_Ret(_Args...)> {};

template <typename _Ret, typename _Ty, typename... _Args>
struct function_args<_Ret (_Ty::*)(_Args...) const> : function_args<_Ret(_Args...)> {};

template <typename _Ty>
struct function_args : function_args<decltype(&_Ty::operator())> {};

#if defined(ENABLE_CO_AWAIT)
class awaitable_promise_base {
public:
    awaitable_promise_base() noexcept : state_{std::make_unique<state>()} {
    }

    awaitable_promise_base(const awaitable_promise_base&) = delete;
    awaitable_promise_base& operator=(const awaitable_promise_base&) = delete;

    awaitable_promise_base(awaitable_promise_base&& rhs) noexcept {
        assign_rv(std::move(rhs));
    }

    awaitable_promise_base& operator=(awaitable_promise_base&& rhs) noexcept {
        assign_rv(std::move(rhs));
        return *this;
    }

    void set_exception(std::exception_ptr exception) noexcept {
        std::call_once(state_->flag(), [this, exception = std::move(exception)]() {
            exception_ = std::move(exception);
            state_->ready(true);
            handle_.resume();
        });
    }

    bool ready() const noexcept {
        return state_->ready();
    }

    void suspend(std::experimental::coroutine_handle<> handle) noexcept {
        handle_ = std::move(handle);
    }

    // for coroutine
    auto initial_suspend() noexcept {
        return std::experimental::suspend_never{};
    }

    // for coroutine
    auto final_suspend() noexcept {
        return std::experimental::suspend_always{};
    }

    // for coroutine
    void unhandled_exception() {
        set_exception(std::current_exception());
    }

    void rethrow_exception() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    void swap(awaitable_promise_base& other) noexcept {
        if (this != std::addressof(other)) {
            std::swap(handle_, other.handle_);
            std::swap(exception_, other.exception_);
            std::swap(state_, other.state_);
        }
    }

protected:
    struct state final {
        bool ready() const noexcept {
            return ready_;
        }

        void ready(bool ready) noexcept {
            ready_ = ready;
        }

        std::once_flag& flag() noexcept {
            return flag_;
        }

    private:
        std::once_flag flag_;
        std::atomic_bool ready_{false};
    };

    std::experimental::coroutine_handle<> handle_;
    std::exception_ptr exception_{nullptr};
    std::unique_ptr<state> state_{nullptr};

private:
    void assign_rv(awaitable_promise_base&& other) noexcept {
        if (this != std::addressof(other)) {
            handle_ = nullptr;
            exception_ = nullptr;
            state_.reset();
            swap(other);
        }
    }
};

template <typename _Ty>
class awaitable_promise final : public awaitable_promise_base {
public:
    using promise_type = awaitable_promise<_Ty>;
    using future_type = awaitable_future<_Ty>;

    awaitable_promise() noexcept = default;
    awaitable_promise(const awaitable_promise&) = delete;
    awaitable_promise& operator=(const awaitable_promise&) = delete;
    awaitable_promise(awaitable_promise&&) = default;
    awaitable_promise& operator=(awaitable_promise&&) = default;

    future_type get_future() {
        return future_type{this};
    }

    // for coroutine
    future_type get_return_object() noexcept {
        return future_type{this};
    };

    // for coroutine
    template <typename _Value>
    void return_value(_Value&& value) {
        set_value(std::forward<_Value>(value));
    }

    template <typename _Value>
    void set_value(_Value&& value) {
        std::call_once(state_->flag(), [this, value = std::forward<_Value>(value)]() {
            result_ = std::move(value);
            state_->ready(true);
            handle_.resume();
        });
    }

    _Ty get() {
        if (!state_->ready()) {
            exception_ = std::make_exception_ptr(std::runtime_error{"no value"});
        }
        rethrow_exception();
        return std::move(result_);
    }

private:
    _Ty result_;
};

template <>
class awaitable_promise<void> final : public awaitable_promise_base {
public:
    using promise_type = awaitable_promise<void>;
    using future_type = awaitable_future<void>;

    awaitable_promise() noexcept = default;
    awaitable_promise(const awaitable_promise&) = delete;
    awaitable_promise& operator=(const awaitable_promise&) = delete;
    awaitable_promise(awaitable_promise&&) = default;
    awaitable_promise& operator=(awaitable_promise&&) = default;

    future_type get_future() {
        return future_type{this};
    }

    // for coroutine
    future_type get_return_object() noexcept {
        return future_type{this};
    };

    // for coroutine
    void return_void() {
    }

    void set_value() {
        std::call_once(state_->flag(), [this]() {
            state_->ready(true);
            handle_.resume();
        });
    }

    void get() {
        if (!state_->ready()) {
            exception_ = std::make_exception_ptr(std::runtime_error{"no value"});
        }
        rethrow_exception();
    }
};
#endif // defined(ENABLE_CO_AWAIT)

template <typename _Func, std::size_t... _Indexes>
constexpr auto index_apply_impl(_Func&& func, std::index_sequence<_Indexes...>) {
    return func(std::integral_constant<std::size_t, _Indexes>{}...);
}

template <std::size_t _Index, typename _Func>
constexpr auto index_apply(_Func&& func) {
    return index_apply_impl(std::forward<_Func>(func), std::make_index_sequence<_Index>{});
}

template <std::size_t _Index, typename _Tuple>
constexpr auto take_front(_Tuple&& tp) {
    return index_apply<_Index>(
        [&](auto... _Indexes) { return std::make_tuple(std::get<_Indexes>(std::forward<_Tuple>(tp))...); });
}

template <typename _Tuple, typename _Func>
constexpr auto apply(_Tuple&& tp, _Func&& func) {
    return index_apply<std::tuple_size<_Tuple>{}>(
        [&](auto... _Indexes) { return func(std::get<_Indexes>(std::forward<_Tuple>(tp))...); });
}

template <typename _Tuple>
constexpr auto reverse(const _Tuple& tp) {
    return index_apply<std::tuple_size<_Tuple>{}>(
        [&](auto... _Indexes) { return std::make_tuple(std::get<std::tuple_size<_Tuple>{} - 1 - _Indexes>(tp)...); });
}

template <typename _Tuple, std::size_t _Index, typename _Ty,
          typename _Indexes = std::make_index_sequence<std::tuple_size<_Tuple>{}>>
struct replace_type_by_index;

template <typename... _Args, std::size_t _Index, typename _Ty, std::size_t... _Indexes>
struct replace_type_by_index<std::tuple<_Args...>, _Index, _Ty, std::index_sequence<_Indexes...>> {
    using type = std::tuple<std::conditional_t<_Indexes == _Index, _Ty, _Args>...>;
};

template <std::size_t _Index, typename _Tuple, typename _Ty>
constexpr auto replace_tuple_arg_by_index(const _Tuple& tp, _Ty&& t) {
    return std::tuple_cat(take_front<_Index>(tp), std::make_tuple(std::forward<_Ty>(t)),
                          reverse(take_front<std::tuple_size<_Tuple>{} - _Index - 1>(reverse(tp))));
}

template <std::size_t _Index, typename _Ty, typename... _Args>
constexpr auto replace_arg_by_index(_Ty&& t, _Args&&... args) {
    auto args_tuple = std::make_tuple(std::forward<_Args>(args)...);
    return replace_tuple_arg_by_index<_Index>(args_tuple, std::forward<_Ty>(t));
}

template <typename _Ty, typename _Func>
constexpr auto replace_arg_impl(_Ty&& t, _Func&& f, std::true_type) {
    return std::forward<_Func>(f);
}

template <typename _Ty, typename _Func>
constexpr auto replace_arg_impl(_Ty&& t, _Func&& f, std::false_type) {
    return std::forward<_Ty>(t);
}

template <bool _True, typename _Ty, typename _Func>
constexpr auto replace_arg(_Ty&& t, _Func&& f) {
    return replace_arg_impl(std::forward<_Ty>(t), std::forward<_Func>(f), std::integral_constant<bool, _True>{});
}

template <typename _Ty, typename _Func>
constexpr auto replace_std_future(_Ty&& t, _Func&& f) {
    return replace_arg<is_std_future_placeholder<_Ty>{}>(std::forward<_Ty>(t), std::forward<_Func>(f));
}

template <typename _Ty, typename _Func>
constexpr auto replace_awaitable(_Ty&& t, _Func&& f) {
    return replace_arg<is_awaitable_placeholder<_Ty>{}>(std::forward<_Ty>(t), std::forward<_Func>(f));
}

template <typename _Promise>
void apply_callback(_Promise promise) {
    promise->set_value();
}

template <typename _Promise, typename _Arg>
void apply_callback(_Promise promise, _Arg&& arg) {
    promise->set_value(std::forward<_Arg>(arg));
}

template <typename _Promise, typename... _Args>
void apply_callback(_Promise promise, _Args&&... args) {
    promise->set_value(std::make_tuple(std::forward<_Args>(args)...));
}

template <typename _Callback, template <typename> class _Promise>
class callback_wrapper final : public std::enable_shared_from_this<callback_wrapper<_Callback, _Promise>> {
public:
    using promise_type = _Promise<typename function_args<_Callback>::args_tuple>;

    callback_wrapper() noexcept : promise_{std::make_shared<promise_type>()} {
    }

    auto get_future() {
        return promise_->get_future();
    }

    auto callback() {
        typename function_args<_Callback>::function_type callback{
            [this, self = this->shared_from_this()](auto&&... args) {
                apply_callback(promise_, std::forward<decltype(args)>(args)...);
            }};
        return callback;
    }

private:
    std::shared_ptr<promise_type> promise_;
};

template <typename _Func, typename... _Args>
auto async_wrapper_impl(std::true_type, _Func&& func, _Args&&... args) {
    constexpr std::size_t index = std_future_index<std::tuple<std::decay_t<_Args>...>>{};
    using callback_t = typename function_args<std::decay_t<_Func>>::template arg_t<index>;
    auto wrapper = std::make_shared<callback_wrapper<callback_t, std::promise>>();
    // 1.
    // detail::apply(replace_arg_by_index<index>(wrapper->callback(), std::forward<_Args>(args)...),
    //               std::forward<_Func>(func));

    // 2.
    // auto args_tupe = std::make_tuple(std::forward<_Args>(args)...);
    // auto tp = replace_tuple_arg_by_index<index>(args_tupe, wrapper->callback());
    // detail::apply(std::move(tp), std::forward<_Func>(func));

    // 3.
    detail::apply(std::make_tuple(replace_std_future(std::forward<_Args>(args), wrapper->callback())...),
                  std::forward<_Func>(func));
    return wrapper->get_future();
}

#if defined(ENABLE_CO_AWAIT)
template <typename _Func, typename... _Args>
auto async_wrapper_impl(std::false_type, _Func&& func, _Args&&... args) {
    constexpr std::size_t index = awaitable_index<std::tuple<std::decay_t<_Args>...>>{};
    using callback_t = typename function_args<std::decay_t<_Func>>::template arg_t<index>;
    auto wrapper = std::make_shared<callback_wrapper<callback_t, awaitable_promise>>();
    detail::apply(std::make_tuple(replace_awaitable(std::forward<_Args>(args), wrapper->callback())...),
                  std::forward<_Func>(func));
    return wrapper->get_future();
}
#endif // defined(ENABLE_CO_AWAIT)

} // namespace detail

template <typename _Func, typename... _Args, typename _Tuple = std::tuple<std::decay_t<_Args>...>>
auto async_wrapper(_Func&& func, _Args&&... args) {
    return detail::async_wrapper_impl(detail::has_std_future<_Tuple>{}, std::forward<_Func>(func),
                                      std::forward<_Args>(args)...);
}

} // namespace cue

#endif // ASYNC_WRAPPER_HPP_
