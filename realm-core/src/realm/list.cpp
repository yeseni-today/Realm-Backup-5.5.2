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


#include "realm/list.hpp"
#include "realm/cluster_tree.hpp"
#include "realm/array_basic.hpp"
#include "realm/array_integer.hpp"
#include "realm/array_bool.hpp"
#include "realm/array_string.hpp"
#include "realm/array_binary.hpp"
#include "realm/array_timestamp.hpp"
#include "realm/column_type_traits.hpp"
#include "realm/table.hpp"
#include "realm/table_view.hpp"
#include "realm/group.hpp"
#include "realm/replication.hpp"

using namespace realm;

ConstLstBasePtr ConstObj::get_listbase_ptr(ColKey col_key) const
{
    auto attr = get_table()->get_column_attr(col_key);
    REALM_ASSERT(attr.test(col_attr_List));
    bool nullable = attr.test(col_attr_Nullable);

    switch (get_table()->get_column_type(col_key)) {
        case type_Int: {
            if (nullable)
                return std::make_unique<ConstLst<util::Optional<Int>>>(*this, col_key);
            else
                return std::make_unique<ConstLst<Int>>(*this, col_key);
        }
        case type_Bool: {
            if (nullable)
                return std::make_unique<ConstLst<util::Optional<Bool>>>(*this, col_key);
            else
                return std::make_unique<ConstLst<Bool>>(*this, col_key);
        }
        case type_Float: {
            if (nullable)
                return std::make_unique<ConstLst<util::Optional<Float>>>(*this, col_key);
            else
                return std::make_unique<ConstLst<Float>>(*this, col_key);
        }
        case type_Double: {
            if (nullable)
                return std::make_unique<ConstLst<util::Optional<Double>>>(*this, col_key);
            else
                return std::make_unique<ConstLst<Double>>(*this, col_key);
        }
        case type_String: {
            return std::make_unique<ConstLst<String>>(*this, col_key);
        }
        case type_Binary: {
            return std::make_unique<ConstLst<Binary>>(*this, col_key);
        }
        case type_Timestamp: {
            return std::make_unique<ConstLst<Timestamp>>(*this, col_key);
        }
        case type_LinkList: {
            const ConstLstBase* clb = get_linklist_ptr(col_key).release();
            return ConstLstBasePtr(const_cast<ConstLstBase*>(clb));
        }
        case type_Link:
        case type_OldDateTime:
        case type_OldTable:
        case type_OldMixed:
            REALM_ASSERT(false);
            break;
    }
    return {};
}

LstBasePtr Obj::get_listbase_ptr(ColKey col_key) const
{
    auto attr = get_table()->get_column_attr(col_key);
    REALM_ASSERT(attr.test(col_attr_List));
    bool nullable = attr.test(col_attr_Nullable);

    switch (get_table()->get_column_type(col_key)) {
        case type_Int: {
            if (nullable)
                return std::make_unique<Lst<util::Optional<Int>>>(*this, col_key);
            else
                return std::make_unique<Lst<Int>>(*this, col_key);
        }
        case type_Bool: {
            if (nullable)
                return std::make_unique<Lst<util::Optional<Bool>>>(*this, col_key);
            else
                return std::make_unique<Lst<Bool>>(*this, col_key);
        }
        case type_Float: {
            if (nullable)
                return std::make_unique<Lst<util::Optional<Float>>>(*this, col_key);
            else
                return std::make_unique<Lst<Float>>(*this, col_key);
        }
        case type_Double: {
            if (nullable)
                return std::make_unique<Lst<util::Optional<Double>>>(*this, col_key);
            else
                return std::make_unique<Lst<Double>>(*this, col_key);
        }
        case type_String: {
            return std::make_unique<Lst<String>>(*this, col_key);
        }
        case type_Binary: {
            return std::make_unique<Lst<Binary>>(*this, col_key);
        }
        case type_Timestamp: {
            return std::make_unique<Lst<Timestamp>>(*this, col_key);
        }
        case type_LinkList:
            return get_linklist_ptr(col_key);
        case type_Link:
        case type_OldDateTime:
        case type_OldTable:
        case type_OldMixed:
            REALM_ASSERT(false);
            break;
    }
    return {};
}

/********************************* LstBase **********************************/

ConstLstBase::ConstLstBase(ColKey col_key, const ConstObj* obj)
    : m_const_obj(obj)
    , m_col_key(col_key)
{
    if (!col_key.get_attrs().test(col_attr_List)) {
        throw LogicError(LogicError::list_type_mismatch);
    }
}

template <class T>
ConstLst<T>::ConstLst(const ConstObj& obj, ColKey col_key)
    : ConstLstBase(col_key, &m_obj)
    , ConstLstIf<T>(obj.get_alloc())
    , m_obj(obj)
{
    this->m_nullable = obj.get_table()->is_nullable(col_key);
    this->init_from_parent();
}

ConstLstBase::~ConstLstBase()
{
}

ref_type ConstLstBase::get_child_ref(size_t) const noexcept
{
    try {
        return to_ref(m_const_obj->_get<int64_t>(m_col_key.get_index()));
    }
    catch (const InvalidKey&) {
        return ref_type(0);
    }
}

std::pair<ref_type, size_t> ConstLstBase::get_to_dot_parent(size_t) const
{
    // TODO
    return {};
}

void ConstLstBase::erase_repl(Replication* repl, size_t ndx) const
{
    repl->list_erase(*this, ndx);
}

void ConstLstBase::move_repl(Replication* repl, size_t from, size_t to) const
{
    repl->list_move(*this, from, to);
}

void ConstLstBase::swap_repl(Replication* repl, size_t ndx1, size_t ndx2) const
{
    repl->list_swap(*this, ndx1, ndx2);
}

void ConstLstBase::clear_repl(Replication* repl) const
{
    repl->list_clear(*this);
}

template <class T>
Lst<T>::Lst(const Obj& obj, ColKey col_key)
    : ConstLstBase(col_key, &m_obj)
    , ConstLstIf<T>(obj.get_alloc())
    , m_obj(obj)
{
    if (m_obj) {
        this->m_nullable = col_key.get_attrs().test(col_attr_Nullable);
        ConstLstIf<T>::init_from_parent();
    }
}

/****************************** Lst aggregates *******************************/

// This will be defined when using C++17
template <typename... Ts>
struct make_void {
    typedef void type;
};
template <typename... Ts>
using void_t = typename make_void<Ts...>::type;


template <class T, class = void>
struct MinHelper {
    template <class U>
    static Mixed eval(U&, size_t*)
    {
        return Mixed{};
    }
};

template <class T>
struct MinHelper<T, void_t<typename ColumnTypeTraits<T>::minmax_type>> {
    template <class U>
    static Mixed eval(U& tree, size_t* return_ndx)
    {
        return Mixed(bptree_minimum<T>(tree, return_ndx));
    }
};

template <class T>
Mixed ConstLstIf<T>::min(size_t* return_ndx) const
{
    return MinHelper<T>::eval(*m_tree, return_ndx);
}

template <class T, class Enable = void>
struct MaxHelper {
    template <class U>
    static Mixed eval(U&, size_t*)
    {
        return Mixed{};
    }
};

template <class T>
struct MaxHelper<T, void_t<typename ColumnTypeTraits<T>::minmax_type>> {
    template <class U>
    static Mixed eval(U& tree, size_t* return_ndx)
    {
        return Mixed(bptree_maximum<T>(tree, return_ndx));
    }
};

template <class T>
Mixed ConstLstIf<T>::max(size_t* return_ndx) const
{
    return MaxHelper<T>::eval(*m_tree, return_ndx);
}

template <class T, class Enable = void>
class SumHelper {
public:
    template <class U>
    static Mixed eval(U&, size_t* return_cnt)
    {
        if (return_cnt)
            *return_cnt = 0;
        return Mixed{};
    }
};

template <class T>
class SumHelper<T, void_t<typename ColumnTypeTraits<T>::sum_type>> {
public:
    template <class U>
    static Mixed eval(U& tree, size_t* return_cnt)
    {
        return Mixed(bptree_sum<T>(tree, return_cnt));
    }
};

template <class T>
Mixed ConstLstIf<T>::sum(size_t* return_cnt) const
{
    return SumHelper<T>::eval(*m_tree, return_cnt);
}

template <class T, class = void>
struct AverageHelper {
    template <class U>
    static Mixed eval(U&, size_t* return_cnt)
    {
        if (return_cnt)
            *return_cnt = 0;
        return Mixed{};
    }
};

template <class T>
struct AverageHelper<T, void_t<typename ColumnTypeTraits<T>::sum_type>> {
    template <class U>
    static Mixed eval(U& tree, size_t* return_cnt)
    {
        return Mixed(bptree_average<T>(tree, return_cnt));
    }
};

template <class T>
Mixed ConstLstIf<T>::avg(size_t* return_cnt) const
{
    return AverageHelper<T>::eval(*m_tree, return_cnt);
}

template <class T>
void ConstLstIf<T>::sort(std::vector<size_t>& indices, bool ascending) const
{
    auto sz = size();
    auto sz2 = indices.size();

    indices.reserve(sz);
    if (sz < sz2) {
        // If list size has decreased, we have to start all over
        indices.clear();
        sz2 = 0;
    }
    for (size_t i = sz2; i < sz; i++) {
        // If list size has increased, just add the missing indices
        indices.push_back(i);
    }
    auto b = indices.begin();
    auto e = indices.end();
    if (ascending) {
        std::sort(b, e, [this](size_t i1, size_t i2) { return m_tree->get(i1) < m_tree->get(i2); });
    }
    else {
        std::sort(b, e, [this](size_t i1, size_t i2) { return m_tree->get(i1) > m_tree->get(i2); });
    }
}

template <class T>
void ConstLstIf<T>::distinct(std::vector<size_t>& indices, util::Optional<bool> sort_order) const
{
    indices.clear();
    sort(indices, sort_order ? *sort_order : true);
    auto duplicates = std::unique(indices.begin(), indices.end(),
                                  [this](size_t i1, size_t i2) { return m_tree->get(i1) == m_tree->get(i2); });
    // Erase the duplicates
    indices.erase(duplicates, indices.end());

    if (!sort_order) {
        // Restore original order
        std::sort(indices.begin(), indices.end(), std::less<size_t>());
    }
}


ConstObj ConstLnkLst::get_object(size_t link_ndx) const
{
    return m_const_obj->get_target_table(m_col_key)->get_object(ConstLstIf<ObjKey>::get(link_ndx));
}

/********************************* Lst<Key> *********************************/

template <>
void Lst<ObjKey>::do_set(size_t ndx, ObjKey target_key)
{
    CascadeState state;
    ObjKey old_key = get(ndx);
    bool recurse = m_obj.replace_backlink(m_col_key, old_key, target_key, state);

    m_tree->set(ndx, target_key);

    if (recurse) {
        auto table = m_obj.get_table();
        _impl::TableFriend::remove_recursive(*table, state); // Throws
    }
}

template <>
void Lst<ObjKey>::do_insert(size_t ndx, ObjKey target_key)
{
    m_obj.set_backlink(m_col_key, target_key);
    m_tree->insert(ndx, target_key);
}

template <>
void Lst<ObjKey>::do_remove(size_t ndx)
{
    CascadeState state;
    ObjKey old_key = get(ndx);
    bool recurse = m_obj.remove_backlink(m_col_key, old_key, state);

    m_tree->erase(ndx);

    if (recurse) {
        auto table = m_obj.get_table();
        _impl::TableFriend::remove_recursive(*table, state); // Throws
    }
}

template <>
void Lst<ObjKey>::clear()
{
    update_if_needed();
    if (size() > 0) {
        auto origin_table = m_obj.get_table();

        if (Replication* repl = m_const_obj->get_replication())
            repl->list_clear(*this); // Throws

        if (!origin_table->get_column_attr(m_col_key).test(col_attr_StrongLinks)) {
            size_t ndx = size();
            while (ndx--) {
                do_set(ndx, null_key);
                m_tree->erase(ndx);
                ConstLstBase::adj_remove(ndx);
            }
            //m_obj.bump_both_versions();
            m_obj.bump_content_version();
            return;
        }

        TableRef target_table = m_obj.get_target_table(m_col_key);
        TableKey target_table_key = target_table->get_key();
        ColKey backlink_col = origin_table->get_opposite_column(m_col_key);

        CascadeState state;

        typedef _impl::TableFriend tf;
        size_t num_links = size();
        for (size_t ndx = 0; ndx < num_links; ++ndx) {
            ObjKey target_key = m_tree->get(ndx);
            Obj target_obj = target_table->get_object(target_key);
            target_obj.remove_one_backlink(backlink_col, m_obj.get_key()); // Throws
            size_t num_remaining = target_obj.get_backlink_count(*origin_table, m_col_key);
            if (num_remaining == 0) {
                state.m_to_be_deleted.emplace_back(target_table_key, target_key);
            }
        }

        m_tree->clear();
        m_obj.bump_both_versions();

        tf::remove_recursive(*origin_table, state); // Throws
    }
}

#ifdef _WIN32
namespace realm {
// Explicit instantiation required on some windows builds
template void Lst<ObjKey>::do_insert(size_t ndx, ObjKey target_key);
template void Lst<ObjKey>::do_set(size_t ndx, ObjKey target_key);
template void Lst<ObjKey>::do_remove(size_t ndx);
template void Lst<ObjKey>::clear();
}
#endif

Obj LnkLst::get_object(size_t ndx)
{
    ObjKey k = get(ndx);
    return get_target_table()->get_object(k);
}

TableView LnkLst::get_sorted_view(SortDescriptor order) const
{
    TableView tv(get_target_table(), clone());
    tv.do_sync();
    tv.sort(std::move(order));
    return tv;
}

TableView LnkLst::get_sorted_view(ColKey column_key, bool ascending) const
{
    TableView v = get_sorted_view(SortDescriptor({{column_key}}, {ascending}));
    return v;
}

void LnkLst::remove_target_row(size_t link_ndx)
{
    // Deleting the object will automatically remove all links
    // to it. So we do not have to manually remove the deleted link
    ObjKey k = get(link_ndx);
    get_target_table()->remove_object(k);
}

void LnkLst::remove_all_target_rows()
{
    if (is_attached()) {
        _impl::TableFriend::batch_erase_rows(*get_target_table(), *this->m_tree);
    }
}

LnkLst::LnkLst(const Obj& owner, ColKey col_key)
    : ConstLstBase(col_key, &m_obj)
    , Lst<ObjKey>(owner, col_key)
    , ObjList(this->m_tree.get(), m_obj.get_target_table(m_col_key))
{
}

void LnkLst::get_dependencies(TableVersions& versions) const
{
    if (is_attached()) {
        auto table = get_table();
        versions.emplace_back(table->get_key(), table->get_content_version());
    }
}

void LnkLst::sync_if_needed() const
{
    if (this->is_attached()) {
        const_cast<LnkLst*>(this)->update_if_needed();
    }
}

namespace realm {
/***************************** Lst<T>::set_repl *****************************/
template <>
void Lst<Int>::set_repl(Replication* repl, size_t ndx, int64_t value)
{
    repl->list_set_int(*this, ndx, value);
}

template <>
void Lst<util::Optional<Int>>::set_repl(Replication* repl, size_t ndx, util::Optional<Int> value)
{
    if (value) {
        repl->list_set_int(*this, ndx, *value);
    }
    else {
        repl->list_set_null(*this, ndx);
    }
}

template <>
void Lst<Bool>::set_repl(Replication* repl, size_t ndx, bool value)
{
    repl->list_set_bool(*this, ndx, value);
}

template <>
void Lst<util::Optional<bool>>::set_repl(Replication* repl, size_t ndx, util::Optional<bool> value)
{
    if (value) {
        repl->list_set_bool(*this, ndx, *value);
    }
    else {
        repl->list_set_null(*this, ndx);
    }
}

template <>
void Lst<Float>::set_repl(Replication* repl, size_t ndx, float value)
{
    repl->list_set_float(*this, ndx, value);
}

template <>
void Lst<util::Optional<Float>>::set_repl(Replication* repl, size_t ndx, util::Optional<Float> value)
{
    if (value) {
        repl->list_set_float(*this, ndx, *value);
    }
    else {
        repl->list_set_null(*this, ndx);
    }
}

template <>
void Lst<Double>::set_repl(Replication* repl, size_t ndx, double value)
{
    repl->list_set_double(*this, ndx, value);
}

template <>
void Lst<util::Optional<Double>>::set_repl(Replication* repl, size_t ndx, util::Optional<Double> value)
{
    if (value) {
        repl->list_set_double(*this, ndx, *value);
    }
    else {
        repl->list_set_null(*this, ndx);
    }
}

template <>
void Lst<String>::set_repl(Replication* repl, size_t ndx, StringData value)
{
    if (value.is_null()) {
        repl->list_set_null(*this, ndx);
    }
    else {
        repl->list_set_string(*this, ndx, value);
    }
}

template <>
void Lst<Binary>::set_repl(Replication* repl, size_t ndx, BinaryData value)
{
    if (value.is_null()) {
        repl->list_set_null(*this, ndx);
    }
    else {
        repl->list_set_binary(*this, ndx, value);
    }
}

template <>
void Lst<Timestamp>::set_repl(Replication* repl, size_t ndx, Timestamp value)
{
    if (value.is_null()) {
        repl->list_set_null(*this, ndx);
    }
    else {
        repl->list_set_timestamp(*this, ndx, value);
    }
}

template <>
void Lst<ObjKey>::set_repl(Replication* repl, size_t ndx, ObjKey key)
{
    if (key) {
        repl->list_set_link(*this, ndx, key);
    }
    else {
        repl->list_set_null(*this, ndx);
    }
}

/*************************** Lst<T>::insert_repl ****************************/
template <>
void Lst<Int>::insert_repl(Replication* repl, size_t ndx, int64_t value)
{
    repl->list_insert_int(*this, ndx, value);
}

template <>
void Lst<util::Optional<Int>>::insert_repl(Replication* repl, size_t ndx, util::Optional<Int> value)
{
    if (value) {
        repl->list_insert_int(*this, ndx, *value);
    }
    else {
        repl->list_insert_null(*this, ndx);
    }
}

template <>
void Lst<Bool>::insert_repl(Replication* repl, size_t ndx, bool value)
{
    repl->list_insert_bool(*this, ndx, value);
}

template <>
void Lst<util::Optional<bool>>::insert_repl(Replication* repl, size_t ndx, util::Optional<bool> value)
{
    if (value) {
        repl->list_insert_bool(*this, ndx, *value);
    }
    else {
        repl->list_insert_null(*this, ndx);
    }
}

template <>
void Lst<Float>::insert_repl(Replication* repl, size_t ndx, float value)
{
    repl->list_insert_float(*this, ndx, value);
}

template <>
void Lst<util::Optional<Float>>::insert_repl(Replication* repl, size_t ndx, util::Optional<Float> value)
{
    if (value) {
        repl->list_insert_float(*this, ndx, *value);
    }
    else {
        repl->list_insert_null(*this, ndx);
    }
}

template <>
void Lst<Double>::insert_repl(Replication* repl, size_t ndx, double value)
{
    repl->list_insert_double(*this, ndx, value);
}

template <>
void Lst<util::Optional<Double>>::insert_repl(Replication* repl, size_t ndx, util::Optional<Double> value)
{
    if (value) {
        repl->list_insert_double(*this, ndx, *value);
    }
    else {
        repl->list_insert_null(*this, ndx);
    }
}

template <>
void Lst<String>::insert_repl(Replication* repl, size_t ndx, StringData value)
{
    if (value.is_null()) {
        repl->list_insert_null(*this, ndx);
    }
    else {
        repl->list_insert_string(*this, ndx, value);
    }
}

template <>
void Lst<Binary>::insert_repl(Replication* repl, size_t ndx, BinaryData value)
{
    if (value.is_null()) {
        repl->list_insert_null(*this, ndx);
    }
    else {
        repl->list_insert_binary(*this, ndx, value);
    }
}

template <>
void Lst<Timestamp>::insert_repl(Replication* repl, size_t ndx, Timestamp value)
{
    if (value.is_null()) {
        repl->list_insert_null(*this, ndx);
    }
    else {
        repl->list_insert_timestamp(*this, ndx, value);
    }
}

template <>
void Lst<ObjKey>::insert_repl(Replication* repl, size_t ndx, ObjKey key)
{
    if (key) {
        repl->list_insert_link(*this, ndx, key);
    }
    else {
        repl->list_insert_null(*this, ndx);
    }
}

/************************* template instantiations ***************************/

template class ConstLstIf<int64_t>;
template class ConstLstIf<util::Optional<Int>>;
template class ConstLstIf<bool>;
template class ConstLstIf<util::Optional<bool>>;
template class ConstLstIf<float>;
template class ConstLstIf<util::Optional<float>>;
template class ConstLstIf<double>;
template class ConstLstIf<util::Optional<double>>;
template class ConstLstIf<StringData>;
template class ConstLstIf<BinaryData>;
template class ConstLstIf<Timestamp>;
template class ConstLstIf<ObjKey>;

template class ConstLst<int64_t>;
template class ConstLst<util::Optional<Int>>;
template class ConstLst<bool>;
template class ConstLst<util::Optional<bool>>;
template class ConstLst<float>;
template class ConstLst<util::Optional<float>>;
template class ConstLst<double>;
template class ConstLst<util::Optional<double>>;
template class ConstLst<StringData>;
template class ConstLst<BinaryData>;
template class ConstLst<Timestamp>;
template class ConstLst<ObjKey>;

template class Lst<int64_t>;
template class Lst<util::Optional<Int>>;
template class Lst<bool>;
template class Lst<util::Optional<bool>>;
template class Lst<float>;
template class Lst<util::Optional<float>>;
template class Lst<double>;
template class Lst<util::Optional<double>>;
template class Lst<StringData>;
template class Lst<BinaryData>;
template class Lst<Timestamp>;
template class Lst<ObjKey>;

} // namespace realm
