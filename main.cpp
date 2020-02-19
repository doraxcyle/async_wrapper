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

void func1(const int& a, int b, const std::function<void(int)>& f, float c) {
    std::async([&]() { f(a + b); });
}

void func2(int&& a, int b, std::function<void()> f) {
    std::async([&]() { f(); });
}

void func3(int a, int b, std::function<void(int, int)>&& f) {
    std::async([&]() { f(a + b, b); });
}

using func_type = void(int);
void func4(int a, int b, func_type f) {
    std::async([&]() { f(a + b); });
}

int main(int argc, char** argv) {
    auto r1 = async_wrapper(func1, 1, 2, placeholder::callback, 1.0);
    static_assert(std::is_same<decltype(r1), std::future<int>>::value);
    auto r1r = r1.get();
    std::cout << r1r << std::endl;

    auto r2 = async_wrapper(func2, 1, 2, placeholder::callback);
    static_assert(std::is_same<decltype(r2), std::future<void>>::value);
    r2.wait();

    auto r3 = async_wrapper(func3, 1, 2, placeholder::callback);
    static_assert(std::is_same<decltype(r3), std::future<std::tuple<int, int>>>::value);
    auto r3r = r3.get();
    std::cout << std::get<0>(r3r) << " " << std::get<1>(r3r) << std::endl;

    // auto r4 = async_wrapper(func4, 1, 2, placeholder::callback);
    // static_assert(std::is_same<decltype(r4), std::future<int>>::value);
    // auto r4r = r4.get();

    return 0;
}
