/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#include "testsettings.hpp"
#ifdef TEST_BASIC_UTILS

#include <realm/alloc_slab.hpp>
#include <realm/util/file.hpp>
#include <realm/util/inspect.hpp>
#include <realm/util/shared_ptr.hpp>

#include "test.hpp"

using namespace realm;
using namespace realm::util;

namespace {
struct Foo {
    void func()
    {
    }
    void modify()
    {
        c = 123;
    }
    char c;
};
}

TEST(Utils_SharedPtr)
{
    const SharedPtr<Foo> foo1 = new Foo();
    Foo* foo2 = foo1.get();
    static_cast<void>(foo2);

    const SharedPtr<Foo> foo3 = new Foo();
    foo3->modify();

    SharedPtr<Foo> foo4 = new Foo();
    foo4->modify();

    SharedPtr<const int> a = new int(1);
    const int* b = a.get();
    CHECK_EQUAL(1, *b);

    const SharedPtr<const int> c = new int(2);
    const int* const d = c.get();
    CHECK_EQUAL(2, *d);

    const SharedPtr<int> e = new int(3);
    const int* f = e.get();
    static_cast<void>(f);
    CHECK_EQUAL(3, *e);
    *e = 123;

    SharedPtr<int> g = new int(4);
    int* h = g.get();
    static_cast<void>(h);
    CHECK_EQUAL(4, *g);
    *g = 123;
}

#endif
