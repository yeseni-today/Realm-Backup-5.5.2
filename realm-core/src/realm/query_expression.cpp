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

#include <realm/query_expression.hpp>
#include <realm/group.hpp>

namespace realm {

void LinkMap::set_base_table(ConstTableRef table)
{
    if (table == get_base_table())
        return;

    m_tables.clear();
    m_tables.push_back(table);
    m_link_types.clear();
    m_only_unary_links = true;

    for (size_t i = 0; i < m_link_column_keys.size(); i++) {
        ColKey link_column_key = m_link_column_keys[i];
        // Link column can be either LinkList or single Link
        ColumnType type = link_column_key.get_type();
        REALM_ASSERT(Table::is_link_type(type) || type == col_type_BackLink);
        if (type == col_type_LinkList || type == col_type_BackLink) {
            m_only_unary_links = false;
        }

        m_link_types.push_back(type);
        REALM_ASSERT(table->valid_column(link_column_key));
        table = table.unchecked_ptr()->get_opposite_table(link_column_key);
        m_tables.push_back(table);
    }
}

void LinkMap::collect_dependencies(std::vector<TableKey>& tables) const
{
    for (auto& t : m_tables) {
        TableKey k = t->get_key();
        if (find(tables.begin(), tables.end(), k) == tables.end()) {
            tables.push_back(k);
        }
    }
}

std::string LinkMap::description(util::serializer::SerialisationState& state) const
{
    std::string s;
    for (size_t i = 0; i < m_link_column_keys.size(); ++i) {
        if (i < m_tables.size() && m_tables[i]) {
            s += state.get_column_name(m_tables[i], m_link_column_keys[i]);
            if (i != m_link_column_keys.size() - 1) {
                s += util::serializer::value_separator;
            }
        }
    }
    return s;
}

void LinkMap::map_links(size_t column, ObjKey key, LinkMapFunction& lm) const
{
    bool last = (column + 1 == m_link_column_keys.size());
    ColumnType type = m_link_types[column];
    ConstObj obj = m_tables[column]->get_object(key);
    if (type == col_type_Link) {
        if (ObjKey k = obj.get<ObjKey>(m_link_column_keys[column])) {
            if (last)
                lm.consume(k);
            else
                map_links(column + 1, k, lm);
        }
    }
    else if (type == col_type_LinkList) {
        auto linklist = obj.get_list<ObjKey>(m_link_column_keys[column]);
        size_t sz = linklist.size();
        for (size_t t = 0; t < sz; t++) {
            ObjKey k = linklist.get(t);
            if (last) {
                bool continue2 = lm.consume(k);
                if (!continue2)
                    return;
            }
            else
                map_links(column + 1, k, lm);
        }
    }
    else if (type == col_type_BackLink) {
        auto backlink_column = m_link_column_keys[column];
        auto backlinks = obj.get_all_backlinks(backlink_column);
        for (auto k : backlinks) {
            if (last) {
                bool continue2 = lm.consume(k);
                if (!continue2)
                    return;
            }
            else
                map_links(column + 1, k, lm);
        }
    }
    else {
        REALM_ASSERT(false);
    }
}

void LinkMap::map_links(size_t column, size_t row, LinkMapFunction& lm) const
{
    REALM_ASSERT(m_leaf_ptr != nullptr);

    bool last = (column + 1 == m_link_column_keys.size());
    ColumnType type = m_link_types[column];
    if (type == col_type_Link) {
        if (ObjKey k = static_cast<const ArrayKey*>(m_leaf_ptr)->get(row)) {
            if (last)
                lm.consume(k);
            else
                map_links(column + 1, k, lm);
        }
    }
    else if (type == col_type_LinkList) {
        if (ref_type ref = static_cast<const ArrayList*>(m_leaf_ptr)->get(row)) {
            BPlusTree<ObjKey> links(get_base_table()->get_alloc());
            links.init_from_ref(ref);
            size_t sz = links.size();
            for (size_t t = 0; t < sz; t++) {
                ObjKey k = links.get(t);
                if (last) {
                    bool continue2 = lm.consume(k);
                    if (!continue2)
                        return;
                }
                else
                    map_links(column + 1, k, lm);
            }
        }
    }
    else if (type == col_type_BackLink) {
        auto back_links = static_cast<const ArrayBacklink*>(m_leaf_ptr);
        size_t sz = back_links->get_backlink_count(row);
        for (size_t t = 0; t < sz; t++) {
            ObjKey k = back_links->get_backlink(row, t);
            if (last) {
                bool continue2 = lm.consume(k);
                if (!continue2)
                    return;
            }
            else
                map_links(column + 1, k, lm);
        }
    }
    else {
        REALM_ASSERT(false);
    }
}

std::vector<ObjKey> LinkMap::get_origin_ndxs(ObjKey key, size_t column) const
{
    if (column == m_link_types.size()) {
        return {key};
    }
    std::vector<ObjKey> keys = get_origin_ndxs(key, column + 1);
    std::vector<ObjKey> ret;
    auto origin_col = m_link_column_keys[column];
    auto origin = m_tables[column];
    auto link_type = m_link_types[column];
    if (link_type == col_type_BackLink) {
        auto link_table = origin->get_opposite_table(origin_col);
        ColKey link_col_ndx = origin->get_opposite_column(origin_col);
        auto forward_type = link_table->get_column_type(link_col_ndx);

        for (auto k : keys) {
            ConstObj o = link_table.unchecked_ptr()->get_object(k);
            if (forward_type == type_Link) {
                ret.push_back(o.get<ObjKey>(link_col_ndx));
            }
            else {
                REALM_ASSERT(forward_type == type_LinkList);
                auto ll = o.get_linklist(link_col_ndx);
                auto sz = ll.size();
                for (size_t i = 0; i < sz; i++) {
                    ret.push_back(ll.get(i));
                }
            }
        }
    }
    else {
        auto target = m_tables[column + 1];
        for (auto k : keys) {
            ConstObj o = target->get_object(k);
            auto cnt = o.get_backlink_count(*origin, origin_col);
            for (size_t i = 0; i < cnt; i++) {
                ret.push_back(o.get_backlink(*origin, origin_col, i));
            }
        }
    }
    return ret;
}

void Columns<Link>::evaluate(size_t index, ValueBase& destination)
{
    // Destination must be of Key type. It only makes sense to
    // compare keys with keys
    REALM_ASSERT_DEBUG(dynamic_cast<Value<ObjKey>*>(&destination));
    auto d = static_cast<Value<ObjKey>*>(&destination);
    std::vector<ObjKey> links = m_link_map.get_links(index);

    if (m_link_map.only_unary_links()) {
        ObjKey key;
        if (!links.empty()) {
            key = links[0];
        }
        d->init(false, 1, key);
    }
    else {
        d->init(true, links);
    }
}

void ColumnListBase::set_cluster(const Cluster* cluster)
{
    m_leaf_ptr = nullptr;
    m_array_ptr = nullptr;
    if (m_link_map.has_links()) {
        m_link_map.set_cluster(cluster);
    }
    else {
        // Create new Leaf
        m_array_ptr = LeafPtr(new (&m_leaf_cache_storage) ArrayList(m_link_map.get_base_table()->get_alloc()));
        cluster->init_leaf(m_column_key, m_array_ptr.get());
        m_leaf_ptr = m_array_ptr.get();
    }
}

void ColumnListBase::get_lists(size_t index, Value<ref_type>& destination, size_t nb_elements)
{
    if (m_link_map.has_links()) {
        std::vector<ObjKey> links = m_link_map.get_links(index);
        auto sz = links.size();

        if (m_link_map.only_unary_links()) {
            ref_type val = 0;
            if (sz == 1) {
                ConstObj obj = m_link_map.get_target_table()->get_object(links[0]);
                val = to_ref(obj._get<int64_t>(m_column_key.get_index()));
            }
            destination.init(false, 1, val);
        }
        else {
            destination.init(true, sz);
            for (size_t t = 0; t < sz; t++) {
                ConstObj obj = m_link_map.get_target_table()->get_object(links[t]);
                ref_type val = to_ref(obj._get<int64_t>(m_column_key.get_index()));
                destination.m_storage.set(t, val);
            }
        }
    }
    else {
        size_t rows = std::min(m_leaf_ptr->size() - index, nb_elements);

        destination.init(false, rows);

        for (size_t t = 0; t < rows; t++) {
            destination.m_storage.set(t, to_ref(m_leaf_ptr->get(index + t)));
        }
    }
}

Query Subexpr2<StringData>::equal(StringData sd, bool case_sensitive)
{
    return string_compare<StringData, Equal, EqualIns>(*this, sd, case_sensitive);
}

Query Subexpr2<StringData>::equal(const Subexpr2<StringData>& col, bool case_sensitive)
{
    return string_compare<Equal, EqualIns>(*this, col, case_sensitive);
}

Query Subexpr2<StringData>::not_equal(StringData sd, bool case_sensitive)
{
    return string_compare<StringData, NotEqual, NotEqualIns>(*this, sd, case_sensitive);
}

Query Subexpr2<StringData>::not_equal(const Subexpr2<StringData>& col, bool case_sensitive)
{
    return string_compare<NotEqual, NotEqualIns>(*this, col, case_sensitive);
}

Query Subexpr2<StringData>::begins_with(StringData sd, bool case_sensitive)
{
    return string_compare<StringData, BeginsWith, BeginsWithIns>(*this, sd, case_sensitive);
}

Query Subexpr2<StringData>::begins_with(const Subexpr2<StringData>& col, bool case_sensitive)
{
    return string_compare<BeginsWith, BeginsWithIns>(*this, col, case_sensitive);
}

Query Subexpr2<StringData>::ends_with(StringData sd, bool case_sensitive)
{
    return string_compare<StringData, EndsWith, EndsWithIns>(*this, sd, case_sensitive);
}

Query Subexpr2<StringData>::ends_with(const Subexpr2<StringData>& col, bool case_sensitive)
{
    return string_compare<EndsWith, EndsWithIns>(*this, col, case_sensitive);
}

Query Subexpr2<StringData>::contains(StringData sd, bool case_sensitive)
{
    return string_compare<StringData, Contains, ContainsIns>(*this, sd, case_sensitive);
}

Query Subexpr2<StringData>::contains(const Subexpr2<StringData>& col, bool case_sensitive)
{
    return string_compare<Contains, ContainsIns>(*this, col, case_sensitive);
}

Query Subexpr2<StringData>::like(StringData sd, bool case_sensitive)
{
    return string_compare<StringData, Like, LikeIns>(*this, sd, case_sensitive);
}

Query Subexpr2<StringData>::like(const Subexpr2<StringData>& col, bool case_sensitive)
{
    return string_compare<Like, LikeIns>(*this, col, case_sensitive);
}


// BinaryData

Query Subexpr2<BinaryData>::equal(BinaryData sd, bool case_sensitive)
{
    return binary_compare<BinaryData, Equal, EqualIns>(*this, sd, case_sensitive);
}

Query Subexpr2<BinaryData>::equal(const Subexpr2<BinaryData>& col, bool case_sensitive)
{
    return binary_compare<Equal, EqualIns>(*this, col, case_sensitive);
}

Query Subexpr2<BinaryData>::not_equal(BinaryData sd, bool case_sensitive)
{
    return binary_compare<BinaryData, NotEqual, NotEqualIns>(*this, sd, case_sensitive);
}

Query Subexpr2<BinaryData>::not_equal(const Subexpr2<BinaryData>& col, bool case_sensitive)
{
    return binary_compare<NotEqual, NotEqualIns>(*this, col, case_sensitive);
}

Query Subexpr2<BinaryData>::begins_with(BinaryData sd, bool case_sensitive)
{
    return binary_compare<BinaryData, BeginsWith, BeginsWithIns>(*this, sd, case_sensitive);
}

Query Subexpr2<BinaryData>::begins_with(const Subexpr2<BinaryData>& col, bool case_sensitive)
{
    return binary_compare<BeginsWith, BeginsWithIns>(*this, col, case_sensitive);
}

Query Subexpr2<BinaryData>::ends_with(BinaryData sd, bool case_sensitive)
{
    return binary_compare<BinaryData, EndsWith, EndsWithIns>(*this, sd, case_sensitive);
}

Query Subexpr2<BinaryData>::ends_with(const Subexpr2<BinaryData>& col, bool case_sensitive)
{
    return binary_compare<EndsWith, EndsWithIns>(*this, col, case_sensitive);
}

Query Subexpr2<BinaryData>::contains(BinaryData sd, bool case_sensitive)
{
    return binary_compare<BinaryData, Contains, ContainsIns>(*this, sd, case_sensitive);
}

Query Subexpr2<BinaryData>::contains(const Subexpr2<BinaryData>& col, bool case_sensitive)
{
    return binary_compare<Contains, ContainsIns>(*this, col, case_sensitive);
}

Query Subexpr2<BinaryData>::like(BinaryData sd, bool case_sensitive)
{
    return binary_compare<BinaryData, Like, LikeIns>(*this, sd, case_sensitive);
}

Query Subexpr2<BinaryData>::like(const Subexpr2<BinaryData>& col, bool case_sensitive)
{
    return binary_compare<Like, LikeIns>(*this, col, case_sensitive);
}
}
