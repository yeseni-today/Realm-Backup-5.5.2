/*************************************************************************
 *
 * Copyright 2020 Realm Inc.
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

#ifndef REALM_TEST_TYPES_HELPER_HPP
#define REALM_TEST_TYPES_HELPER_HPP

#include <realm.hpp>
#include <realm/column_type_traits.hpp>

namespace realm {
namespace test_util {

struct TestValueGenerator {
    template <typename T>
    inline T convert_for_test(int64_t v)
    {
        return static_cast<T>(v);
    }

    template <typename T>
    std::vector<T> values_from_int(const std::vector<int64_t>& values)
    {
        std::vector<T> ret;
        for (size_t i = 0; i < values.size(); ++i) {
            ret.push_back(convert_for_test<typename util::RemoveOptional<T>::type>(values[i]));
        }
        return ret;
    }

private:
    std::vector<util::StringBuffer> m_buffer_space;
};


template <>
inline bool TestValueGenerator::convert_for_test<bool>(int64_t v)
{
    return v % 2 == 0;
}

template <>
inline Timestamp TestValueGenerator::convert_for_test<Timestamp>(int64_t v)
{
    return Timestamp{v, 0};
}

template <>
inline StringData TestValueGenerator::convert_for_test<StringData>(int64_t t)
{
    std::string str = util::format("string %1", t);
    util::StringBuffer b;
    b.append(str);
    m_buffer_space.emplace_back(std::move(b));
    return StringData(m_buffer_space[m_buffer_space.size() - 1].data(), str.size());
}

template <>
inline BinaryData TestValueGenerator::convert_for_test<BinaryData>(int64_t t)
{
    std::string str = util::format("string %1", t);
    util::StringBuffer b;
    b.append(str);
    m_buffer_space.emplace_back(std::move(b));
    return BinaryData(m_buffer_space[m_buffer_space.size() - 1].data(), str.size());
}

enum class ColumnState { Normal = 0, Nullable = 1, Indexed = 2, NullableIndexed = 3 };

template <ColumnState s>
constexpr static bool col_state_is_nullable = (s == ColumnState::Nullable || s == ColumnState::NullableIndexed);

template <ColumnState s>
constexpr static bool col_state_is_indexed = (s == ColumnState::Indexed || s == ColumnState::NullableIndexed);

template <typename T, ColumnState state = ColumnState::Normal, typename Enable = void>
struct Prop {
    static constexpr bool is_nullable = col_state_is_nullable<state>;
    static constexpr bool is_indexed = col_state_is_indexed<state>;
    static constexpr DataType data_type = ColumnTypeTraits<T>::id;
    using type = T;
    using underlying_type = type;
    static type default_value()
    {
        return ColumnTypeTraits<type>::cluster_leaf_type::default_value(is_nullable);
    }
    static underlying_type default_non_nullable_value()
    {
        return ColumnTypeTraits<underlying_type>::cluster_leaf_type::default_value(false);
    }
};

template <typename T, ColumnState state>
struct Prop<T, state,
            std::enable_if_t<col_state_is_nullable<state> && !ObjectTypeTraits<T>::self_contained_null, void>> {
    static constexpr bool is_nullable = col_state_is_nullable<state>;
    static constexpr bool is_indexed = col_state_is_indexed<state>;
    static constexpr DataType data_type = ColumnTypeTraits<T>::id;
    using type = typename util::Optional<T>;
    using underlying_type = T;
    static type default_value()
    {
        if (realm::is_any<type, util::Optional<float>, util::Optional<double>>::value) {
            return type(); // optional float/double would return NaN and for consistency we want to operate on null
        }
        return ColumnTypeTraits<type>::cluster_leaf_type::default_value(is_nullable);
    }
    static underlying_type default_non_nullable_value()
    {
        return ColumnTypeTraits<underlying_type>::cluster_leaf_type::default_value(false);
    }
};

template <typename T>
using Nullable = Prop<T, ColumnState::Nullable>;
template <typename T>
using Indexed = Prop<T, ColumnState::Indexed>;
template <typename T>
using NullableIndexed = Prop<T, ColumnState::NullableIndexed>;

struct less {
    template <typename T>
    auto operator()(T&& a, T&& b) const noexcept
    {
        return Mixed(a).compare(Mixed(b)) < 0;
    }
};
struct greater {
    template <typename T>
    auto operator()(T&& a, T&& b) const noexcept
    {
        return Mixed(a).compare(Mixed(b)) > 0;
    }
};

} // namespace test_util
} // namespace realm

#endif // REALM_TEST_TYPES_HELPER
