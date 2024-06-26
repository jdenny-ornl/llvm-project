//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <memory>

// shared_ptr

// template<class D> shared_ptr(nullptr_t, D d);

#include <memory>
#include <cassert>
#include "test_macros.h"
#include "deleter_types.h"

#include "types.h"
struct A
{
    static int count;

    A() {++count;}
    A(const A&) {++count;}
    ~A() {--count;}
};

int A::count = 0;

// LWG 3233. Broken requirements for shared_ptr converting constructors
// https://cplusplus.github.io/LWG/issue3233
static_assert( std::is_constructible<std::shared_ptr<int>,  std::nullptr_t, test_deleter<int> >::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int>, std::nullptr_t, bad_deleter>::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int>,  std::nullptr_t, no_move_deleter>::value, "");

#if TEST_STD_VER >= 17
static_assert(std::is_constructible<std::shared_ptr<int[]>, std::nullptr_t, test_deleter<int[]> >::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int[]>,  std::nullptr_t, bad_deleter>::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int[]>,  std::nullptr_t, no_nullptr_deleter>::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int[]>,  std::nullptr_t, no_move_deleter>::value, "");

static_assert(std::is_constructible<std::shared_ptr<int[5]>, std::nullptr_t, test_deleter<int[5]> >::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int[5]>,  std::nullptr_t, bad_deleter>::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int[5]>,  std::nullptr_t, no_nullptr_deleter>::value, "");
static_assert(!std::is_constructible<std::shared_ptr<int[5]>,  std::nullptr_t, no_move_deleter>::value, "");
#endif

int main(int, char**)
{
    {
        std::shared_ptr<A> p(nullptr, test_deleter<A>(3));
        assert(A::count == 0);
        assert(p.use_count() == 1);
        assert(p.get() == 0);
        assert(test_deleter<A>::count == 1);
        assert(test_deleter<A>::dealloc_count == 0);
#ifndef TEST_HAS_NO_RTTI
        test_deleter<A>* d = std::get_deleter<test_deleter<A> >(p);
        assert(d);
        assert(d->state() == 3);
#endif
    }
    assert(A::count == 0);
    assert(test_deleter<A>::count == 0);
    assert(test_deleter<A>::dealloc_count == 1);

    {
        std::shared_ptr<A const> p(nullptr, test_deleter<A const>(3));
        assert(p.get() == nullptr);
    }

    return 0;
}
