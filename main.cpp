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

#include <iostream>
#include <thread>

#include <async_wrapper.hpp>

using namespace cue;

// 为了显示支持多种参数类型
// 并不推荐使用const int&/int&&/const std::function<void(int)>&/std::function<void(int, int)>&&
// 这些引用类型
void func1(const int& a, int b, const std::function<void(int)>& f, float c) {
    std::thread{[=]() {
        std::this_thread::sleep_for(std::chrono::seconds{1});
        f(a + b);
    }}.detach();
}

void func2(int&& a, int b, std::function<void()> f) {
    std::thread{[=]() {
        std::this_thread::sleep_for(std::chrono::seconds{1});
        f();
    }}.detach();
}

void func3(int a, int b, std::function<void(int, int)>&& f) {
    std::thread{[=]() {
        std::this_thread::sleep_for(std::chrono::seconds{1});
        f(a + b, b);
    }}.detach();
}

int main(int argc, char** argv) {
    auto r1 = async_wrapper(func1, 1, 2, placeholder::std_future, 1.0);
    static_assert(std::is_same<decltype(r1), std::future<int>>{}, "");
    auto r1r = r1.get();
    std::cout << r1r << std::endl;

    auto r2 = async_wrapper(func2, 1, 2, placeholder::std_future);
    static_assert(std::is_same<decltype(r2), std::future<void>>{}, "");
    r2.wait();

    auto r3 = async_wrapper(func3, 1, 2, placeholder::std_future);
    static_assert(std::is_same<decltype(r3), std::future<std::tuple<int, int>>>{}, "");
    auto r3r = r3.get();
    std::cout << std::get<0>(r3r) << " " << std::get<1>(r3r) << std::endl;

#if defined(ENABLE_CO_AWAIT)
    []() -> awaitable {
        auto r4 = co_await async_wrapper(func1, 1, 2, placeholder::awaitable, 1.0);
        static_assert(std::is_same<decltype(r4), int>{}, "");
        std::cout << r4 << std::endl;
    }();

    []() -> awaitable { co_await async_wrapper(func2, 1, 2, placeholder::awaitable); }();

    []() -> awaitable {
        auto r6 = co_await async_wrapper(func3, 1, 2, placeholder::awaitable);
        static_assert(std::is_same<decltype(r6), std::tuple<int, int>>{}, "");
        std::cout << std::get<0>(r6) << " " << std::get<1>(r6) << std::endl;
    }();
#endif // defined(ENABLE_CO_AWAIT)

    getchar();

    return 0;
}
