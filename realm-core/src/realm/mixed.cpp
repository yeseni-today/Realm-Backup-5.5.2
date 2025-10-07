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

#include <realm/mixed.hpp>
#include <realm/unicode.hpp>

namespace realm {

namespace _impl {
inline int compare_string(StringData a, StringData b)
{
    if (a == b)
        return 0;
    return utf8_compare(a, b) ? -1 : 1;
}

inline int compare_binary(BinaryData a, BinaryData b)
{
    size_t asz = a.size();
    size_t bsz = b.size();
    size_t min_sz = std::min(asz, bsz);
    int ret = memcmp(a.data(), b.data(), min_sz);
    if (ret == 0) {
        if (asz > bsz)
            ret = 1;
        else if (asz < bsz)
            ret = -1;
    }
    return ret;
}

template <int>
struct IntTypeForSize;
template <>
struct IntTypeForSize<1> {
    using type = uint8_t;
};
template <>
struct IntTypeForSize<2> {
    using type = uint16_t;
};
template <>
struct IntTypeForSize<4> {
    using type = uint32_t;
};
template <>
struct IntTypeForSize<8> {
    using type = uint64_t;
};

template <typename Float>
inline int compare_float(Float a_raw, Float b_raw)
{
    bool a_nan = std::isnan(a_raw);
    bool b_nan = std::isnan(b_raw);
    if (!a_nan && !b_nan) {
        // Just compare as IEEE floats
        return a_raw == b_raw ? 0 : a_raw < b_raw ? -1 : 1;
    }
    if (a_nan && b_nan) {
        // Compare the nan values as unsigned
        using IntType = typename _impl::IntTypeForSize<sizeof(Float)>::type;
        IntType a = 0, b = 0;
        memcpy(&a, &a_raw, sizeof(Float));
        memcpy(&b, &b_raw, sizeof(Float));
        return a == b ? 0 : a < b ? -1 : 1;
    }
    // One is nan, the other is not
    // nans are treated as being less than all non-nan values
    return a_nan ? -1 : 1;
}
} // namespace _impl

int Mixed::compare(const Mixed& b) const
{
    // nulls are treated as being less than all other values
    if (is_null()) {
        return b.is_null() ? 0 : -1;
    }
    if (b.is_null())
        return 1;

    switch (get_type()) {
        case type_Int:
            if (get<int64_t>() > b.get<int64_t>())
                return 1;
            else if (get<int64_t>() < b.get<int64_t>())
                return -1;
            break;
        case type_String:
            return _impl::compare_string(get<StringData>(), b.get<StringData>());
            break;
        case type_Binary:
            return _impl::compare_binary(get<BinaryData>(), b.get<BinaryData>());
            break;
        case type_Float:
            return _impl::compare_float(get<float>(), b.get<float>());
        case type_Double:
            return _impl::compare_float(get<double>(), b.get<double>());
        case type_Bool:
            if (get<bool>() > b.get<bool>())
                return 1;
            else if (get<bool>() < b.get<bool>())
                return -1;
            break;
        case type_Timestamp:
            if (get<Timestamp>() > b.get<Timestamp>())
                return 1;
            else if (get<Timestamp>() < b.get<Timestamp>())
                return -1;
            break;
        case type_Link:
            if (get<ObjKey>() > b.get<ObjKey>())
                return 1;
            else if (get<ObjKey>() < b.get<ObjKey>())
                return -1;
            break;
        case type_OldTable:
        case type_OldDateTime:
        case type_OldMixed:
        case type_LinkList:
            REALM_ASSERT_RELEASE(false && "Compare not supported for this column type");
            break;
    }

    return 0;
}

// LCOV_EXCL_START
std::ostream& operator<<(std::ostream& out, const Mixed& m)
{
    out << "Mixed(";
    if (m.is_null()) {
        out << "null";
    }
    else {
        switch (m.get_type()) {
            case type_Int:
                out << m.int_val;
                break;
            case type_Bool:
                out << (m.bool_val ? "true" : "false");
                break;
            case type_Float:
                out << m.float_val;
                break;
            case type_Double:
                out << m.double_val;
                break;
            case type_String:
                out << StringData(m.str_val, m.short_val);
                break;
            case type_Binary:
                out << BinaryData(m.str_val, m.short_val);
                break;
            case type_Timestamp:
                out << Timestamp(m.int_val, m.short_val);
                break;
            case type_Link:
                out << ObjKey(m.int_val);
                break;
            case type_OldDateTime:
            case type_OldTable:
            case type_OldMixed:
            case type_LinkList:
                REALM_ASSERT(false);
        }
    }
    out << ")";
    return out;
}
// LCOV_EXCL_STOP


} // namespace realm
