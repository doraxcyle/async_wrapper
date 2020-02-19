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

namespace cue {

namespace detail {

struct placeholder_t final {};

template <typename T>
using is_callback_placeholder = std::is_same<std::decay_t<T>, placeholder_t>;

} // namespace detail

namespace placeholder {

constexpr detail::placeholder_t callback{};

} // namespace placeholder

namespace detail {

template <typename T, typename Tuple>
struct type_index;

template <typename T, typename... Args>
struct type_index<T, std::tuple<T, Args...>> : public std::integral_constant<std::size_t, 0> {};

template <typename T, typename U, typename... Args>
struct type_index<T, std::tuple<U, Args...>>
    : public std::integral_constant<std::size_t, 1 + type_index<T, std::tuple<Args...>>{}> {};

template <typename Tuple>
using callback_index = type_index<placeholder_t, Tuple>;

template <typename T>
struct function_args;

template <typename R, typename... Args>
struct function_args<R(Args...)> {
    using type = R(Args...);
    using function_type = std::function<type>;
    using return_type = R;
    static constexpr std::size_t arity{sizeof...(Args)};

    using args_tuple = std::tuple<std::decay_t<Args>...>;

    template <std::size_t N>
    struct arg {
        using type = std::decay_t<typename std::tuple_element<N, std::tuple<Args...>>::type>;
    };

    template <std::size_t N>
    using arg_t = typename arg<N>::type;
};

template <typename R, typename Arg>
struct function_args<R(Arg)> {
    using type = R(Arg);
    using function_type = std::function<type>;
    using return_type = R;
    static constexpr std::size_t arity{1};

    using args_tuple = std::decay_t<Arg>;

    template <std::size_t N>
    struct arg {
        using type = std::decay_t<Arg>;
    };

    template <std::size_t N>
    using arg_t = typename arg<N>::type;
};

template <typename R>
struct function_args<R()> {
    using type = R();
    using function_type = std::function<type>;
    using return_type = R;
    static constexpr std::size_t arity{0};

    using args_tuple = void;

    template <std::size_t N>
    struct arg {
        using type = void;
    };

    template <std::size_t N>
    using arg_t = typename arg<N>::type;
};

template <typename R, typename... Args>
struct function_args<R (*)(Args...)> : function_args<R(Args...)> {};

template <typename R, typename... Args>
struct function_args<std::function<R(Args...)>> : function_args<R(Args...)> {};

template <typename R, typename T, typename... Args>
struct function_args<R (T::*)(Args...)> : function_args<R(Args...)> {};

template <typename R, typename T, typename... Args>
struct function_args<R (T::*)(Args...) const> : function_args<R(Args...)> {};

template <typename T>
struct function_args : function_args<decltype(&T::operator())> {};

template <typename Func, std::size_t... Indexes>
constexpr auto index_apply_impl(Func&& func, std::index_sequence<Indexes...>) {
    return func(std::integral_constant<std::size_t, Indexes>{}...);
}

template <std::size_t Index, typename Func>
constexpr auto index_apply(Func&& func) {
    return index_apply_impl(std::forward<Func>(func), std::make_index_sequence<Index>{});
}

template <std::size_t Index, typename Tuple>
constexpr auto take_front(Tuple&& tp) {
    return index_apply<Index>(
        [&](auto... Indexes) { return std::make_tuple(std::get<Indexes>(std::forward<Tuple>(tp))...); });
}

template <typename Tuple, typename Func>
constexpr auto apply(Tuple&& tp, Func&& func) {
    return index_apply<std::tuple_size<Tuple>{}>(
        [&](auto... Indexes) { return func(std::get<Indexes>(std::forward<Tuple>(tp))...); });
}

template <typename Tuple>
constexpr auto reverse(Tuple tp) {
    return index_apply<std::tuple_size<Tuple>{}>(
        [&](auto... Indexes) { return std::make_tuple(std::get<std::tuple_size<Tuple>{} - 1 - Indexes>(tp)...); });
}

template <typename Tuple, std::size_t Index, typename T,
          typename Indexes = std::make_index_sequence<std::tuple_size<Tuple>{}>>
struct replace_type;

template <typename... Args, std::size_t Index, typename T, std::size_t... Indexes>
struct replace_type<std::tuple<Args...>, Index, T, std::index_sequence<Indexes...>> {
    using type = std::tuple<std::conditional_t<Indexes == Index, T, Args>...>;
};

template <typename T, typename... Args, std::size_t Index = callback_index<std::tuple<std::decay_t<Args>...>>{}>
constexpr auto replace_arg(T&& t, Args&&... args) {
    auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
    return std::tuple_cat(take_front<Index>(args_tuple), std::make_tuple(std::forward<T>(t)),
                          reverse(take_front<sizeof...(Args) - Index - 1>(reverse(args_tuple))));
}

template <typename Promise>
void apply_callback(Promise promise) {
    promise->set_value();
}

template <typename Promise, typename Arg>
void apply_callback(Promise promise, Arg&& arg) {
    promise->set_value(std::forward<Arg>(arg));
}

template <typename Promise, typename... Args>
void apply_callback(Promise promise, Args&&... args) {
    promise->set_value(std::make_tuple(std::forward<Args>(args)...));
}

template <typename Callback, typename Promise = std::promise<typename function_args<Callback>::args_tuple>>
class callback_wrapper final : public std::enable_shared_from_this<callback_wrapper<Callback, Promise>> {
public:
    callback_wrapper() noexcept : promise_{std::make_shared<Promise>()} {
    }

    auto get_future() {
        return promise_->get_future();
    }

    auto callback() {
        typename function_args<Callback>::function_type callback{
            [this, self = this->shared_from_this()](auto&&... args) {
                apply_callback(promise_, std::forward<decltype(args)>(args)...);
            }};
        return callback;
    }

private:
    std::shared_ptr<Promise> promise_;
};

} // namespace detail

template <typename Func, typename... Args,
          std::size_t Index = detail::callback_index<std::tuple<std::decay_t<Args>...>>{}>
decltype(auto) async_wrapper(Func&& func, Args&&... args) {
    using callback_t = typename detail::function_args<std::decay_t<Func>>::template arg_t<Index>;
    auto wrapper = std::make_shared<detail::callback_wrapper<callback_t>>();
    detail::apply(detail::replace_arg(wrapper->callback(), std::forward<Args>(args)...), std::forward<Func>(func));
    return wrapper->get_future();
}

} // namespace cue

#endif // ASYNC_WRAPPER_HPP_
