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

#include <realm/util/to_string.hpp>
#include <realm/util/assert.hpp>

#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>

namespace {
std::locale locale_classic = std::locale::classic();
}

namespace realm {
namespace util {

void Printable::print(std::ostream& out, bool quote) const
{
    switch (m_type) {
        case Printable::Type::Bool:
            out << (m_uint ? "true" : "false");
            break;
        case Printable::Type::Uint:
            out << m_uint;
            break;
        case Printable::Type::Int:
            out << m_int;
            break;
        case Printable::Type::String:
            if (quote) {
#if __cplusplus >= 201402L
                out << std::quoted(m_string);
#else
                out << '"' << m_string << '"';
#endif
            }
            else
                out << m_string;
            break;
    }
}

void Printable::print_all(std::ostream& out, const std::initializer_list<Printable>& values, bool quote)
{
    if (values.size() == 0)
        return;

    bool is_first = true;
    out << " [";
    for (auto&& value : values) {
        if (!is_first)
            out << ", ";
        is_first = false;
        value.print(out, quote);
    }
    out << "]";
}

std::string Printable::str() const
{
    std::ostringstream ss;
    ss.imbue(locale_classic);
    ss.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    print(ss, true);
    return ss.str();
}

std::string format(const char* fmt, std::initializer_list<Printable> values)
{
    std::stringstream ss;
    while (*fmt) {
        auto next = strchr(fmt, '%');

        // emit the rest of the format string if there are no more percents
        if (!next) {
            ss << fmt;
            break;
        }

        // emit everything up to the next percent
        ss.write(fmt, next - fmt);
        ++next;
        REALM_ASSERT(*next);

        // %% produces a single escaped %
        if (*next == '%') {
            ss << '%';
            fmt = next + 1;
            continue;
        }
        REALM_ASSERT(isdigit(*next));

        // The const_cast is safe because stroul does not actually modify
        // the pointed-to string, but it lacks a const overload
        auto index = strtoul(next, const_cast<char**>(&fmt), 10) - 1;
        REALM_ASSERT(index < values.size());
        (values.begin() + index)->print(ss, false);
    }
    return ss.str();
}


} // namespace util
} // namespace realm
