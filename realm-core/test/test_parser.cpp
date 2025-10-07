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

#if defined(TEST_PARSER)

#include "test.hpp"


// Test independence and thread-safety
// -----------------------------------
//
// All tests must be thread safe and independent of each other. This
// is required because it allows for both shuffling of the execution
// order and for parallelized testing.
//
// In particular, avoid using std::rand() since it is not guaranteed
// to be thread safe. Instead use the API offered in
// `test/util/random.hpp`.
//
// All files created in tests must use the TEST_PATH macro (or one of
// its friends) to obtain a suitable file system path. See
// `test/util/test_path.hpp`.
//
//
// Debugging and the ONLY() macro
// ------------------------------
//
// A simple way of disabling all tests except one called `Foo`, is to
// replace TEST(Foo) with ONLY(Foo) and then recompile and rerun the
// test suite. Note that you can also use filtering by setting the
// environment varible `UNITTEST_FILTER`. See `README.md` for more on
// this.
//
// Another way to debug a particular test, is to copy that test into
// `experiments/testcase.cpp` and then run `sh build.sh
// check-testcase` (or one of its friends) from the command line.

#include <realm.hpp>
#include <realm/history.hpp>
#include <realm/parser/parser.hpp>
#include <realm/parser/query_builder.hpp>
#include <realm/query_expression.hpp>
#include <realm/replication.hpp>
#include <realm/util/any.hpp>
#include <realm/util/encrypted_file_mapping.hpp>
#include <realm/util/to_string.hpp>
#include "test_table_helper.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <set>

using namespace realm;
using namespace realm::metrics;
using namespace realm::test_util;
using namespace realm::util;

// clang-format off
static std::vector<std::string> valid_queries = {
    // true/false predicates
    "truepredicate",
    "falsepredicate",
    " TRUEPREDICATE ",
    " FALSEPREDICATE ",
    "truepredicates = falsepredicates", // keypaths

    // characters/strings
    "\"\" = ''",
    "'azAZ09/ :()[]{}<>,.^@-+=*&~`' = '\\\" \\' \\\\ \\/ \\b \\f \\n \\r \\t \\0'",
    "\"azAZ09/\" = \"\\\" \\' \\\\ \\/ \\b \\f \\n \\r \\t \\0\"",
    "'\\uffFf' = '\\u0020'",
    "'\\u01111' = 'asdf\\u0111asdf'",

    // expressions (numbers, bools, keypaths, arguments)
    "-1 = 12",
    "0 = 001",
    "0x0 = -0X398235fcAb",
    "10. = -.034",
    "10.0 = 5.034",
    "true = false",
    "truelove = false",
    "true = falsey",
    "nullified = null",
    "nullified = nil",
    "_ = a",
    "_a = _.aZ",
    "a09._br.z = __-__.Z-9",
    "$0 = $19",
    "$0=$0",
    // properties can contain '$'
    "a$a = a",
    "$-1 = $0",
    "$a = $0",
    "$ = $",

    // operators
    "0=0",
    "0 = 0",
    "0 =[c] 0",
    "0!=0",
    "0 != 0",
    "0 !=[c] 0",
    "0!=[c]0",
    "0 <> 0",
    "0<>0",
    "0 <>[c] 0",
    "0<>[c]0",
    "0==0",
    "0 == 0",
    "0==[c]0",
    "0 == [c] 0",
    "0>0",
    "0 > 0",
    "0>=0",
    "0 >= 0",
    "0 => 0",
    "0=>0",
    "0<0",
    "0 < 0",
    "0<=0",
    "0 <= 0",
    "0 =< 0",
    "0<=0",
    "0 contains 0",
    "a CONTAINS[c] b",
    "a contains [c] b",
    "'a'CONTAINS[c]b",
    "0 BeGiNsWiTh 0",
    "0 ENDSWITH 0",
    "contains contains 'contains'",
    "beginswith beginswith 'beginswith'",
    "endswith endswith 'endswith'",
    "NOT NOT != 'NOT'",
    "AND == 'AND' AND OR == 'OR'",
    // FIXME - bug
    // "truepredicate == 'falsepredicate' && truepredicate",

    // atoms/groups
    "(0=0)",
    "( 0=0 )",
    "((0=0))",
    "!0=0",
    "! 0=0",
    "!(0=0)",
    "! (0=0)",
    "NOT0=0",    // keypath NOT0
    "NOT0.a=0",  // keypath NOT0
    "NOT0a.b=0", // keypath NOT0a
    "not-1=1",
    "not 0=0",
    "NOT(0=0)",
    "not (0=0)",
    "NOT (!0=0)",

    // compound
    "a==a && a==a",
    "a==a || a==a",
    "a==a&&a==a||a=a",
    "a==a and a==a",
    "a==a OR a==a",
    "and=='AND'&&'or'=='||'",
    "and == or && ORE > GRAND",
    "a=1AND NOTb=2",

    // sort/distinct
    "a=b SORT(p ASCENDING)",
    "a=b SORT(p asc)",
    "a=b SORT(p Descending)",
    "a=b sort (p.q desc)",
    "a=b distinct(p)",
    "a=b DISTINCT(P)",
    "a=b DISTINCT(p)",
    "a == b sort(a ASC, b DESC)",
    "a == b sort(a ASC, b DESC) sort(c ASC)",
    "a=b DISTINCT(p) DISTINCT(q)",
    "a=b DISTINCT(p, q, r) DISTINCT(q)",
    "a == b sort(a ASC, b DESC) DISTINCT(p)",
    "a == b sort(a ASC, b DESC) DISTINCT(p) sort(c ASC, d DESC) DISTINCT(q.r)",
    "a == b and c==d sort(a ASC, b DESC) DISTINCT(p) sort(c ASC, d DESC) DISTINCT(q.r)",
    "a == b  sort(     a   ASC  ,  b DESC) and c==d   DISTINCT(   p )  sort(   c   ASC  ,  d   DESC  )  DISTINCT(   "
    "q.r ,   p)   ",

    // limit
    "a=b LIMIT(1)",
    "a=b LIMIT ( 1 )",
    "a=b LIMIT( 1234567890 )",
    "a=b LIMIT(1) && c=d",
    "a=b && c=d || e=f LIMIT(1)",
    "a=b LIMIT(1) SORT(a ASC) DISTINCT(b)",
    "a=b SORT(a ASC) LIMIT(1) DISTINCT(b)",
    "a=b SORT(a ASC) DISTINCT(b) LIMIT(1)",
    "a=b LIMIT(2) LIMIT(1)",
    "a=b LIMIT(5) && c=d LIMIT(2)",
    "a=b LIMIT(5) SORT(age ASC) DISTINCT(name) LIMIT(2)",

    // include
    "a=b INCLUDE(c)",
    "a=b include(c,d)",
    "a=b INCLUDE(c.d)",
    "a=b INCLUDE(c.d.e, f.g, h)",
    "a=b INCLUDE ( c )",
    "a=b INCLUDE(d, e, f    , g )",
    "a=b INCLUDE(c) && d=f",
    "a=b INCLUDE(c) INCLUDE(d)",
    "a=b && c=d || e=f INCLUDE(g)",
    "a=b LIMIT(5) SORT(age ASC) DISTINCT(name) INCLUDE(links1, links2)",
    "a=b INCLUDE(links1, links2) LIMIT(5) SORT(age ASC) DISTINCT(name)",

    // subquery expression
    "SUBQUERY(items, $x, $x.name == 'Tom').@size > 0",
    "SUBQUERY(items, $x, $x.name == 'Tom').@count > 0",
    "SUBQUERY(items, $x, $x.allergens.@min.population_affected < 0.10).@count > 0",
    "SUBQUERY(items, $x, $x.name == 'Tom').@count == SUBQUERY(items, $x, $x.price < 10).@count",

    // backlinks
    "p.@links.class.prop.@count > 2",
    "p.@links.class.prop.@sum.prop2 > 2",
};

static std::vector<std::string> invalid_queries = {
    "predicate",
    "'\\a' = ''", // invalid escape

    // invalid unicode
    "'\\u0' = ''",

    // invalid strings
    "\"' = ''",
    "\" = ''",
    "' = ''",

    // expressions
    "03a = 1",
    "1..0 = 1",
    "1.0. = 1",
    "1-0 = 1",
    "0x = 1",
    "- = a",
    "a..b = a",
    "{} = $0",

    // operators
    "0===>0",
    "0 contains1",
    "a contains_something",
    "endswith 0",

    // atoms/groups
    "0=0)",
    "(0=0",
    "(0=0))",
    "! =0",
    "NOTNOT(0=0)",
    "not.a=0",
    "(!!0=0)",
    "0=0 !",

    // compound
    "a==a & a==a",
    "a==a | a==a",
    "a==a &| a==a",
    "a==a && OR a==a",
    "a==aORa==a",
    "a==a ORa==a",
    "a==a AND==a",
    "a==a ANDa==a",
    "a=1ANDNOT b=2",

    "truepredicate &&",
    "truepredicate & truepredicate",

    // sort/distinct
    "SORT(p ASCENDING)",                      // no query conditions
    "a=b SORT(p)",                            // no asc/desc
    "a=b SORT(0 Descending)",                 // bad keypath
    "a=b sort()",                             // missing condition
    "a=b sort",                               // no target property
    "distinct(p)",                            // no query condition
    "a=b DISTINCT()",                         // no target property
    "a=b Distinct",                           // no target property
    "sort(a ASC b, DESC) a == b",             // before query condition
    "sort(a ASC b, DESC) a == b sort(c ASC)", // before query condition
    "a=bDISTINCT(p)",                         // bad spacing
    "a=b sort p.q desc",                      // no braces
    "a=b sort(p.qDESC)",                      // bad spacing
    "a=b DISTINCT p",                         // no braces
    "a=b SORT(p ASC",                         // bad braces
    "a=b DISTINCT(p",                         // no braces
    "a=b sort(p.q DESC a ASC)",               // missing comma
    "a=b DISTINCT(p q)",                      // missing comma

    // limit
    "LIMIT(1)",          // no query conditions
    "a=b LIMIT",         // no params
    "a=b LIMIT()",       // no params
    "a=b LIMIT(2",       // missing end paren
    "a=b LIMIT2)",       // missing open paren
    "a=b LIMIT(-1)",     // negative limit
    "a=b LIMIT(2.7)",    // input must be an integer
    "a=b LIMIT(0xFFEE)", // input must be an integer
    "a=b LIMIT(word)",   // non numeric limit
    "a=b LIMIT(11asdf)", // non numeric limit
    "a=b LIMIT(1, 1)",   // only accept one input

    // include
    "INCLUDE(a)",         // no query conditions
    "a=b INCLUDE",        // no parameters
    "a=b INCLUDE()",      // empty params
    "a=b INCLUDE(a",      // missing end paren
    "a=b INCLUDEb)",      // missing open paren
    "a=b INCLUDE(1)",     // numeric input
    "a=b INCLUDE(a,)",    // missing param
    "a=b INCLUDE(,a)",    // missing param
    "a=b INCLUDE(a.)",    // incomplete keypath
    "a=b INCLUDE(a b)",   // missing comma
    "a=b INCLUDE(a < b)", // parameters should not be a predicate

    // subquery
    "SUBQUERY(items, $x, $x.name == 'Tom') > 0",        // missing .@count
    "SUBQUERY(items, $x, $x.name == 'Tom').@min > 0",   // @min not yet supported
    "SUBQUERY(items, $x, $x.name == 'Tom').@max > 0",   // @max not yet supported
    "SUBQUERY(items, $x, $x.name == 'Tom').@sum > 0",   // @sum not yet supported
    "SUBQUERY(items, $x, $x.name == 'Tom').@avg > 0",   // @avg not yet supported
    "SUBQUERY(items, var, var.name == 'Tom').@avg > 0", // variable must start with '$'
    "SUBQUERY(, $x, $x.name == 'Tom').@avg > 0",        // a target keypath is required
    "SUBQUERY(items, , name == 'Tom').@avg > 0",        // a variable name is required
    "SUBQUERY(items, $x, ).@avg > 0",                   // the subquery is required

    // no @ allowed in keypaths except for keyword '@links'
    "@prop > 2",
    "@backlinks.@count > 2",
    "prop@links > 2",
};
// clang-format on

TEST(Parser_valid_queries) {
    for (auto& query : valid_queries) {
        //std::cout << "query: " << query << std::endl;
        realm::parser::parse(query);
    }
}

TEST(Parser_invalid_queries) {
    for (auto& query : invalid_queries) {
        //std::cout << "query: " << query << std::endl;
        CHECK_THROW_ANY(realm::parser::parse(query));
    }
}

TEST(Parser_grammar_analysis)
{
    CHECK(realm::parser::analyze_grammar() == 0);
}

Query verify_query(test_util::unit_test::TestContext& test_context, TableRef t, std::string query_string, size_t num_results) {
    Query q = t->where();
    realm::query_builder::NoArguments args;

    parser::ParserResult res = realm::parser::parse(query_string);
    realm::query_builder::apply_predicate(q, res.predicate, args);

    CHECK_EQUAL(q.count(), num_results);
    std::string description = q.get_description();
    //std::cerr << "original: " << query_string << "\tdescribed: " << description << "\n";
    Query q2 = t->where();

    parser::ParserResult res2 = realm::parser::parse(description);
    realm::query_builder::apply_predicate(q2, res2.predicate, args);
    CHECK_EQUAL(q2.count(), num_results);
    return q2;
}


TEST(Parser_empty_input)
{
    Group g;
    std::string table_name = "table";
    TableRef t = g.add_table(table_name);
    t->add_column(type_Int, "int_col");
    std::vector<ObjKey> keys;
    t->create_objects(5, keys);

    // an empty query string is an invalid predicate
    CHECK_THROW_ANY(verify_query(test_context, t, "", 5));

    Query q = t->where(); // empty query
    std::string empty_description = q.get_description();
    CHECK(!empty_description.empty());
    CHECK_EQUAL(0, empty_description.compare("TRUEPREDICATE"));
    realm::parser::Predicate p = realm::parser::parse(empty_description).predicate;
    query_builder::NoArguments args;
    realm::query_builder::apply_predicate(q, p, args);
    CHECK_EQUAL(q.count(), 5);

    verify_query(test_context, t, "TRUEPREDICATE", 5);
    verify_query(test_context, t, "!TRUEPREDICATE", 0);

    verify_query(test_context, t, "FALSEPREDICATE", 0);
    verify_query(test_context, t, "!FALSEPREDICATE", 5);
}


TEST(Parser_ConstrainedQuery)
{
    Group g;
    std::string table_name = "table";
    TableRef t = g.add_table(table_name);
    auto int_col = t->add_column(type_Int, "age");
    auto list_col = t->add_column_link(type_LinkList, "self_list", *t);

    Obj obj0 = t->create_object();
    Obj obj1 = t->create_object();

    obj1.set(int_col, 1);

    auto list_0 = obj0.get_linklist(list_col);
    list_0.add(obj0.get_key());
    list_0.add(obj1.get_key());

    TableView tv = obj0.get_backlink_view(t, list_col);
    Query q(t, &tv);
    CHECK_EQUAL(q.count(), 1);
    q.and_query(t->column<Int>(int_col) <= 0);
    CHECK_EQUAL(q.count(), 1);
    CHECK_THROW(q.get_description(), SerialisationError);

    Query q2(t, list_0);
    CHECK_EQUAL(q2.count(), 2);
    q2.and_query(t->column<Int>(int_col) <= 0);
    CHECK_EQUAL(q2.count(), 1);
    CHECK_THROW(q2.get_description(), SerialisationError);
}

TEST(Parser_basic_serialisation)
{
    Group g;
    std::string table_name = "person";
    TableRef t = g.add_table(table_name);
    auto int_col_key = t->add_column(type_Int, "age");
    t->add_column(type_String, "name");
    t->add_column(type_Double, "fees", true);
    t->add_column(type_Bool, "licensed", true);
    auto link_col = t->add_column_link(type_Link, "buddy", *t);
    auto time_col = t->add_column(type_Timestamp, "time", true);
    t->add_search_index(int_col_key);
    std::vector<std::string> names = {"Billy", "Bob", "Joe", "Jane", "Joel"};
    std::vector<double> fees = { 2.0, 2.23, 2.22, 2.25, 3.73 };
    std::vector<ObjKey> keys;

    t->create_objects(5, keys);
    for (size_t i = 0; i < t->size(); ++i) {
        t->get_object(keys[i]).set_all(int(i), StringData(names[i]), fees[i], (i % 2 == 0));
    }
    t->get_object(keys[0]).set(time_col, Timestamp(realm::null()));
    t->get_object(keys[1]).set(time_col, Timestamp(1512130073, 0));   // 2017/12/02 @ 12:47am (UTC)
    t->get_object(keys[2]).set(time_col, Timestamp(1512130073, 505)); // with nanoseconds
    t->get_object(keys[3]).set(time_col, Timestamp(1, 2));
    t->get_object(keys[4]).set(time_col, Timestamp(0, 0));
    t->get_object(keys[0]).set(link_col, keys[1]);

    Query q = t->where();

    verify_query(test_context, t, "time == NULL", 1);
    verify_query(test_context, t, "time == NIL", 1);
    verify_query(test_context, t, "time != NULL", 4);
    verify_query(test_context, t, "time > T0:0", 3);
    verify_query(test_context, t, "time == T1:2", 1);
    verify_query(test_context, t, "time > 2017-12-1@12:07:53", 1);
    verify_query(test_context, t, "time == 2017-12-01@12:07:53:505", 1);
    verify_query(test_context, t, "buddy == NULL", 4);
    verify_query(test_context, t, "buddy == nil", 4);
    verify_query(test_context, t, "buddy != NULL", 1);
    verify_query(test_context, t, "buddy <> NULL", 1);
    verify_query(test_context, t, "buddy.name == NULL", 4); // matches null links
    verify_query(test_context, t, "buddy.age == NULL", 4);
    verify_query(test_context, t, "age > 2", 2);
    verify_query(test_context, t, "!(age >= 2)", 2);
    verify_query(test_context, t, "!(age => 2)", 2);
    verify_query(test_context, t, "3 <= age", 2);
    verify_query(test_context, t, "3 =< age", 2);
    verify_query(test_context, t, "age > 2 and age < 4", 1);
    verify_query(test_context, t, "age = 1 || age == 3", 2);
    verify_query(test_context, t, "fees = 1.2 || fees = 2.23", 1);
    verify_query(test_context, t, "fees = 2 || fees = 3", 1);
    verify_query(test_context, t, "fees = 2 || fees = 3 || fees = 4", 1);
    verify_query(test_context, t, "fees = 0 || fees = 1", 0);

    verify_query(test_context, t, "fees != 2.22 && fees > 2.2", 3);
    verify_query(test_context, t, "(age > 1 || fees >= 2.25) && age == 4", 1);
    verify_query(test_context, t, "licensed == true", 3);
    verify_query(test_context, t, "licensed == false", 2);
    verify_query(test_context, t, "licensed = true || licensed = true", 3);
    verify_query(test_context, t, "licensed = 1 || licensed = 0", 5);
    verify_query(test_context, t, "licensed = true || licensed = false", 5);
    verify_query(test_context, t, "licensed == true || licensed == false", 5);
    verify_query(test_context, t, "licensed == true || buddy.licensed == true", 3);
    verify_query(test_context, t, "buddy.licensed == true", 0);
    verify_query(test_context, t, "buddy.licensed == false", 1);
    verify_query(test_context, t, "licensed == false || buddy.licensed == false", 3);
    verify_query(test_context, t, "licensed == true or licensed = true || licensed = TRUE", 3);
    verify_query(test_context, t, "name = \"Joe\"", 1);
    verify_query(test_context, t, "buddy.age > 0", 1);
    verify_query(test_context, t, "name BEGINSWITH \"J\"", 3);
    verify_query(test_context, t, "name ENDSWITH \"E\"", 0);
    verify_query(test_context, t, "name ENDSWITH[c] \"E\"", 2);
    verify_query(test_context, t, "name CONTAINS \"OE\"", 0);
    verify_query(test_context, t, "name CONTAINS[c] \"OE\"", 2);
    verify_query(test_context, t, "name LIKE \"b*\"", 0);
    verify_query(test_context, t, "name LIKE[c] \"b*\"", 2);
    verify_query(test_context, t, "TRUEPREDICATE", 5);
    verify_query(test_context, t, "FALSEPREDICATE", 0);
    verify_query(test_context, t, "age > 2 and TRUEPREDICATE", 2);
    verify_query(test_context, t, "age > 2 && FALSEPREDICATE", 0);
    verify_query(test_context, t, "age > 2 or TRUEPREDICATE", 5);
    verify_query(test_context, t, "age > 2 || FALSEPREDICATE", 2);
    verify_query(test_context, t, "age > 2 AND !FALSEPREDICATE", 2);
    verify_query(test_context, t, "age > 2 AND !TRUEPREDICATE", 0);

    CHECK_THROW_ANY(verify_query(test_context, t, "buddy.age > $0", 0)); // no external parameters provided

    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "missing_property > 2", 0), message);
    CHECK(message.find(table_name) != std::string::npos); // no prefix modification for names without "class_"
    CHECK(message.find("missing_property") != std::string::npos);
}


TEST(Parser_LinksToSameTable)
{
    Group g;
    TableRef t = g.add_table("class_Person");
    ColKey age_col = t->add_column(type_Int, "age");
    ColKey name_col = t->add_column(type_String, "name");
    ColKey link_col = t->add_column_link(type_Link, "buddy", *t);
    std::vector<std::string> names = {"Billy", "Bob", "Joe", "Jane", "Joel"};
    std::vector<ObjKey> people_keys;
    t->create_objects(names.size(), people_keys);
    for (size_t i = 0; i < t->size(); ++i) {
        Obj obj = t->get_object(people_keys[i]);
        obj.set(age_col, int64_t(i));
        obj.set(name_col, StringData(names[i]));
        obj.set(link_col, people_keys[(i + 1) % t->size()]);
    }
    t->get_object(people_keys[4]).set_null(link_col);

    verify_query(test_context, t, "age > 0", 4);
    verify_query(test_context, t, "buddy.age > 0", 4);
    verify_query(test_context, t, "buddy.buddy.age > 0", 3);
    verify_query(test_context, t, "buddy.buddy.buddy.age > 0", 2);
    verify_query(test_context, t, "buddy.buddy.buddy.buddy.age > 0", 1);
    verify_query(test_context, t, "buddy.buddy.buddy.buddy.buddy.age > 0", 0);

    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "buddy.buddy.missing_property > 2", 0), message);
    CHECK(message.find("Person") != std::string::npos); // without the "class_" prefix
    CHECK(message.find("missing_property") != std::string::npos);
}

TEST(Parser_LinksToDifferentTable)
{
    Group g;

    TableRef discounts = g.add_table("class_Discounts");
    ColKey discount_off_col = discounts->add_column(type_Double, "reduced_by");
    ColKey discount_active_col = discounts->add_column(type_Bool, "active");

    using discount_t = std::pair<double, bool>;
    std::vector<discount_t> discount_info = {{3.0, false}, {2.5, true}, {0.50, true}, {1.50, true}};
    std::vector<ObjKey> discount_keys;
    discounts->create_objects(discount_info.size(), discount_keys);
    for (size_t i = 0; i < discount_keys.size(); ++i) {
        Obj obj = discounts->get_object(discount_keys[i]);
        obj.set(discount_off_col, discount_info[i].first);
        obj.set(discount_active_col, discount_info[i].second);
    }

    TableRef items = g.add_table("class_Items");
    ColKey item_name_col = items->add_column(type_String, "name");
    ColKey item_price_col = items->add_column(type_Double, "price");
    ColKey item_discount_col = items->add_column_link(type_Link, "discount", *discounts);
    using item_t = std::pair<std::string, double>;
    std::vector<item_t> item_info = {{"milk", 5.5}, {"oranges", 4.0}, {"pizza", 9.5}, {"cereal", 6.5}};
    std::vector<ObjKey> item_keys;
    items->create_objects(item_info.size(), item_keys);
    for (size_t i = 0; i < item_keys.size(); ++i) {
        Obj obj = items->get_object(item_keys[i]);
        obj.set(item_name_col, StringData(item_info[i].first));
        obj.set(item_price_col, item_info[i].second);
    }
    items->get_object(item_keys[0]).set(item_discount_col, discount_keys[2]); // milk -0.50
    items->get_object(item_keys[2]).set(item_discount_col, discount_keys[1]); // pizza -2.5
    items->get_object(item_keys[3]).set(item_discount_col, discount_keys[0]); // cereal -3.0 inactive

    TableRef t = g.add_table("class_Person");
    ColKey id_col = t->add_column(type_Int, "customer_id");
    ColKey items_col = t->add_column_link(type_LinkList, "items", *items);

    Obj person0 = t->create_object();
    Obj person1 = t->create_object();
    Obj person2 = t->create_object();
    person0.set(id_col, int64_t(0));
    person1.set(id_col, int64_t(1));
    person2.set(id_col, int64_t(2));

    LnkLst list_0 = person0.get_linklist(items_col);
    list_0.add(item_keys[0]);
    list_0.add(item_keys[1]);
    list_0.add(item_keys[2]);
    list_0.add(item_keys[3]);

    LnkLst list_1 = person1.get_linklist(items_col);
    for (size_t i = 0; i < 10; ++i) {
        list_1.add(item_keys[0]);
    }

    LnkLst list_2 = person2.get_linklist(items_col);
    list_2.add(item_keys[2]);
    list_2.add(item_keys[2]);
    list_2.add(item_keys[3]);

    verify_query(test_context, t, "items.@count > 2", 3); // how many people bought more than two items?
    verify_query(test_context, t, "items.price > 3.0", 3); // how many people buy items over $3.0?
    verify_query(test_context, t, "items.name ==[c] 'milk'", 2); // how many people buy milk?
    verify_query(test_context, t, "items.discount.active == true", 3); // how many people bought items with an active sale?
    verify_query(test_context, t, "items.discount.reduced_by > 2.0", 2); // how many people bought an item marked down by more than $2.0?
    verify_query(test_context, t, "items.@sum.price > 50", 1); // how many people would spend more than $50 without sales applied?
    verify_query(test_context, t, "items.@avg.price > 7", 1); // how manay people like to buy items more expensive on average than $7?

    std::string message;
    // missing property
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "missing_property > 2", 0), message);
    CHECK(message.find("Person") != std::string::npos); // without the "class_" prefix
    CHECK(message.find("missing_property") != std::string::npos);
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "items.absent_property > 2", 0), message);
    CHECK(message.find("Items") != std::string::npos); // without the "class_" prefix
    CHECK(message.find("absent_property") != std::string::npos);
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "items.discount.nonexistent_property > 2", 0), message);
    CHECK(message.find("Discounts") != std::string::npos); // without the "class_" prefix
    CHECK(message.find("nonexistent_property") != std::string::npos);
    // property is not a link
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "customer_id.property > 2", 0), message);
    CHECK(message.find("Person") != std::string::npos); // without the "class_" prefix
    CHECK(message.find("customer_id") != std::string::npos); // is not a link
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "items.price.property > 2", 0), message);
    CHECK(message.find("Items") != std::string::npos); // without the "class_" prefix
    CHECK(message.find("price") != std::string::npos); // is not a link
    // Null cannot be compared to lists
    CHECK_THROW_ANY(verify_query(test_context, t, "items == NULL", 0));
    CHECK_THROW_ANY(verify_query(test_context, t, "items != NULL", 0));
}


TEST(Parser_StringOperations)
{
    Group g;
    TableRef t = g.add_table("person");
    ColKey name_col = t->add_column(type_String, "name", true);
    ColKey link_col = t->add_column_link(type_Link, "father", *t);
    std::vector<std::string> names = {"Billy", "Bob", "Joe", "Jake", "Joel"};
    std::vector<ObjKey> people_keys;
    t->create_objects(names.size(), people_keys);
    for (size_t i = 0; i < t->size(); ++i) {
        Obj obj = t->get_object(people_keys[i]);
        obj.set(name_col, StringData(names[i]));
        obj.set(link_col, people_keys[(i + 1) % people_keys.size()]);
    }
    t->create_object(); // null
    t->get_object(people_keys[4]).set_null(link_col);

    verify_query(test_context, t, "name == 'Bob'", 1);
    verify_query(test_context, t, "father.name == 'Bob'", 1);
    verify_query(test_context, t, "name ==[c] 'Bob'", 1);
    verify_query(test_context, t, "father.name ==[c] 'Bob'", 1);

    verify_query(test_context, t, "name != 'Bob'", 5);
    verify_query(test_context, t, "father.name != 'Bob'", 5);
    verify_query(test_context, t, "name !=[c] 'bOB'", 5);
    verify_query(test_context, t, "father.name !=[c] 'bOB'", 5);

    verify_query(test_context, t, "name contains \"oe\"", 2);
    verify_query(test_context, t, "father.name contains \"oe\"", 2);
    verify_query(test_context, t, "name contains[c] \"OE\"", 2);
    verify_query(test_context, t, "father.name contains[c] \"OE\"", 2);

    verify_query(test_context, t, "name beginswith \"J\"", 3);
    verify_query(test_context, t, "father.name beginswith \"J\"", 3);
    verify_query(test_context, t, "name beginswith[c] \"j\"", 3);
    verify_query(test_context, t, "father.name beginswith[c] \"j\"", 3);

    verify_query(test_context, t, "name endswith \"e\"", 2);
    verify_query(test_context, t, "father.name endswith \"e\"", 2);
    verify_query(test_context, t, "name endswith[c] \"E\"", 2);
    verify_query(test_context, t, "father.name endswith[c] \"E\"", 2);

    verify_query(test_context, t, "name like \"?o?\"", 2);
    verify_query(test_context, t, "father.name like \"?o?\"", 2);
    verify_query(test_context, t, "name like[c] \"?O?\"", 2);
    verify_query(test_context, t, "father.name like[c] \"?O?\"", 2);

    verify_query(test_context, t, "name == NULL", 1);
    verify_query(test_context, t, "name == nil", 1);
    verify_query(test_context, t, "NULL == name", 1);
    verify_query(test_context, t, "name != NULL", 5);
    verify_query(test_context, t, "NULL != name", 5);
    verify_query(test_context, t, "name ==[c] NULL", 1);
    verify_query(test_context, t, "NULL ==[c] name", 1);
    verify_query(test_context, t, "name !=[c] NULL", 5);
    verify_query(test_context, t, "NULL !=[c] name", 5);

    // for strings 'NULL' is also a synonym for the null string
    verify_query(test_context, t, "name CONTAINS NULL", 6);
    verify_query(test_context, t, "name CONTAINS[c] NULL", 6);
    verify_query(test_context, t, "name BEGINSWITH NULL", 6);
    verify_query(test_context, t, "name BEGINSWITH[c] NULL", 6);
    verify_query(test_context, t, "name ENDSWITH NULL", 6);
    verify_query(test_context, t, "name ENDSWITH[c] NULL", 6);
    verify_query(test_context, t, "name LIKE NULL", 1);
    verify_query(test_context, t, "name LIKE[c] NULL", 1);

    // string operators are not commutative
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL CONTAINS name", 6));
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL CONTAINS[c] name", 6));
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL BEGINSWITH name", 6));
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL BEGINSWITH[c] name", 6));
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL ENDSWITH name", 6));
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL ENDSWITH[c] name", 6));
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL LIKE name", 1));
    CHECK_THROW_ANY(verify_query(test_context, t, "NULL LIKE[c] name", 1));
}


TEST(Parser_Timestamps)
{
    Group g;
    TableRef t = g.add_table("person");
    ColKey birthday_col = t->add_column(type_Timestamp, "birthday");          // disallow null
    ColKey internal_col = t->add_column(type_Timestamp, "T399", true);        // allow null
    ColKey readable_col = t->add_column(type_Timestamp, "T2017-12-04", true); // allow null
    ColKey link_col = t->add_column_link(type_Link, "linked", *t);
    std::vector<ObjKey> keys;
    t->create_objects(5, keys);

    t->get_object(keys[0]).set(birthday_col, Timestamp(-1, -1)); // before epoch by 1 second and one nanosecond
    t->get_object(keys[1]).set(birthday_col, Timestamp(0, -1));  // before epoch by one nanosecond

    t->get_object(keys[0]).set(internal_col, Timestamp(realm::null()));
    t->get_object(keys[1]).set(internal_col, Timestamp(1512130073, 0));   // 2017/12/02 @ 12:47am (UTC)
    t->get_object(keys[2]).set(internal_col, Timestamp(1512130073, 505)); // with nanoseconds
    t->get_object(keys[3]).set(internal_col, Timestamp(1, 2));
    t->get_object(keys[4]).set(internal_col, Timestamp(0, 0));

    t->get_object(keys[0]).set(readable_col, Timestamp(1512130073, 0));
    t->get_object(keys[1]).set(readable_col, Timestamp(1512130073, 505));

    t->get_object(keys[0]).set(link_col, keys[1]);
    t->get_object(keys[2]).set(link_col, keys[0]);

    Query q = t->where();
    auto verify_with_format = [&](const char* separator) {
        verify_query(test_context, t, "T399 == NULL", 1);
        verify_query(test_context, t, "T399 != NULL", 4);
        verify_query(test_context, t, "linked.T399 == NULL", 4); // null links count as a match for null here
        verify_query(test_context, t, "linked != NULL && linked.T399 == NULL", 1);
        verify_query(test_context, t, "linked.T399 != NULL", 1);
        verify_query(test_context, t, "linked != NULL && linked.T399 != NULL", 1);
        verify_query(test_context, t, "T399 == T399:0", 0);
        verify_query(test_context, t, "linked.T399 == T399:0", 0);
        verify_query(test_context, t, std::string("T399 == 2017-12-04") + separator + "0:0:0", 0);

        verify_query(test_context, t, "T2017-12-04 == NULL", 3);
        verify_query(test_context, t, "T2017-12-04 != NULL", 2);
        verify_query(test_context, t, "T2017-12-04 != NIL", 2);
        verify_query(test_context, t, "linked.T2017-12-04 == NULL", 3); // null links count as a match for null here
        verify_query(test_context, t, "linked != NULL && linked.T2017-12-04 == NULL", 0);
        verify_query(test_context, t, "linked.T2017-12-04 != NULL", 2);
        verify_query(test_context, t, "linked != NULL && linked.T2017-12-04 != NULL", 2);
        verify_query(test_context, t, "T2017-12-04 == T399:0", 0);
        verify_query(test_context, t, "linked.T2017-12-04 == T399:0", 0);
        verify_query(test_context, t, "T2017-12-04 == 2017-12-04@0:0:0", 0);

        verify_query(test_context, t, "birthday == NULL", 0);
        verify_query(test_context, t, "birthday == NIL", 0);
        verify_query(test_context, t, "birthday != NULL", 5);
        verify_query(test_context, t, "birthday != NIL", 5);
        verify_query(test_context, t, "birthday == T0:0", 3);
        verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0:0:0", 3); // epoch is default non-null Timestamp

#ifndef _WIN32 // windows native functions do not support pre epoch conversions, other platforms stop at ~1901
        verify_query(test_context, t, std::string("birthday == 1969-12-31") + separator + "23:59:59:1", 1); // just before epoch
        verify_query(test_context, t, std::string("birthday > 1905-12-31") + separator + "23:59:59", 5);
        verify_query(test_context, t, std::string("birthday > 1905-12-31") + separator + "23:59:59:2020", 5);
#endif

        // two column timestamps
        verify_query(test_context, t, "birthday == T399", 1); // a null entry matches

        // dates pre 1900 are not supported by functions like timegm
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday > 1800-12-31") + separator + "23:59:59", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday > 1800-12-31") + separator + "23:59:59:2020", 4));

        // negative nanoseconds are not allowed
        CHECK_THROW_ANY(verify_query(test_context, t, "birthday == T-1:1", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, "birthday == T1:-1", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0:1:-1", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1969-12-31") + separator + "23:59:59:-1", 1));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0:0:-1", 1));

        // Invalid predicate
        CHECK_THROW_ANY(verify_query(test_context, t, "birthday == T1:", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, "birthday == T:1", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, "birthday == 1970-1-1", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator, 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0:", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0:0:", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0:0:0:", 0));
        CHECK_THROW_ANY(verify_query(test_context, t, std::string("birthday == 1970-1-1") + separator + "0:0:0:0:0", 0));
    };

    // both versions are allowed
    verify_with_format("@");
    verify_with_format("T");

    // using both separators at the same time is an error
    CHECK_THROW_ANY(verify_query(test_context, t, "birthday == 1970-1-1T@0:0:0:0", 3));
    CHECK_THROW_ANY(verify_query(test_context, t, "birthday == 1970-1-1@T0:0:0:0", 3));
    // omitting the separator is an error
    CHECK_THROW_ANY(verify_query(test_context, t, "birthday == 1970-1-10:0:0:0:0", 0));
}


TEST(Parser_NullableBinaries)
{
    Group g;
    TableRef items = g.add_table("item");
    TableRef people = g.add_table("person");
    ColKey binary_col = items->add_column(type_Binary, "data");
    ColKey nullable_binary_col = items->add_column(type_Binary, "nullable_data", true);
    std::vector<ObjKey> item_keys;
    items->create_objects(5, item_keys);
    BinaryData bd0("knife", 5);
    items->get_object(item_keys[0]).set(binary_col, bd0);
    items->get_object(item_keys[0]).set(nullable_binary_col, bd0);
    BinaryData bd1("plate", 5);
    items->get_object(item_keys[1]).set(binary_col, bd1);
    items->get_object(item_keys[1]).set(nullable_binary_col, bd1);
    BinaryData bd2("fork", 4);
    items->get_object(item_keys[2]).set(binary_col, bd2);
    items->get_object(item_keys[2]).set(nullable_binary_col, bd2);

    ColKey fav_item_col = people->add_column_link(type_Link, "fav_item", *items);
    std::vector<ObjKey> people_keys;
    people->create_objects(5, people_keys);
    for (size_t i = 0; i < people_keys.size(); ++i) {
        people->get_object(people_keys[i]).set(fav_item_col, item_keys[i]);
    }

    // direct checks
    verify_query(test_context, items, "data == NULL", 0);
    verify_query(test_context, items, "data != NULL", 5);
    verify_query(test_context, items, "nullable_data == NULL", 2);
    verify_query(test_context, items, "nullable_data != NULL", 3);
    verify_query(test_context, items, "data == NIL", 0);
    verify_query(test_context, items, "data != NIL", 5);
    verify_query(test_context, items, "nullable_data == NIL", 2);
    verify_query(test_context, items, "nullable_data != NIL", 3);

    verify_query(test_context, items, "nullable_data CONTAINS 'f'", 2);
    verify_query(test_context, items, "nullable_data BEGINSWITH 'f'", 1);
    verify_query(test_context, items, "nullable_data ENDSWITH 'e'", 2);
    verify_query(test_context, items, "nullable_data LIKE 'f*'", 1);
    verify_query(test_context, items, "nullable_data CONTAINS[c] 'F'", 2);
    verify_query(test_context, items, "nullable_data BEGINSWITH[c] 'F'", 1);
    verify_query(test_context, items, "nullable_data ENDSWITH[c] 'E'", 2);
    verify_query(test_context, items, "nullable_data LIKE[c] 'F*'", 1);

    verify_query(test_context, items, "nullable_data CONTAINS NULL", 5);
    verify_query(test_context, items, "nullable_data BEGINSWITH NULL", 5);
    verify_query(test_context, items, "nullable_data ENDSWITH NULL", 5);
    verify_query(test_context, items, "nullable_data LIKE NULL", 2);
    verify_query(test_context, items, "nullable_data CONTAINS[c] NULL", 3);
    verify_query(test_context, items, "nullable_data BEGINSWITH[c] NULL", 5);
    verify_query(test_context, items, "nullable_data ENDSWITH[c] NULL", 5);
    verify_query(test_context, items, "nullable_data LIKE[c] NULL", 2);

    // these operators are not commutative
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL CONTAINS nullable_data", 5));
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL BEGINSWITH nullable_data", 5));
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL ENDSWITH nullable_data", 5));
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL LIKE nullable_data", 2));
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL CONTAINS[c] nullable_data", 5));
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL BEGINSWITH[c] nullable_data", 5));
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL ENDSWITH[c] nullable_data", 5));
    CHECK_THROW_ANY(verify_query(test_context, items, "NULL LIKE[c] nullable_data", 2));

    // check across links
    verify_query(test_context, people, "fav_item.data == NULL", 0);
    verify_query(test_context, people, "fav_item.data != NULL", 5);
    verify_query(test_context, people, "fav_item.nullable_data == NULL", 2);
    verify_query(test_context, people, "fav_item.nullable_data != NULL", 3);
    verify_query(test_context, people, "NULL == fav_item.data", 0);

    verify_query(test_context, people, "fav_item.data ==[c] NULL", 0);
    verify_query(test_context, people, "fav_item.data !=[c] NULL", 5);
    verify_query(test_context, people, "fav_item.nullable_data ==[c] NULL", 2);
    verify_query(test_context, people, "fav_item.nullable_data !=[c] NULL", 3);
    verify_query(test_context, people, "NULL ==[c] fav_item.data", 0);

    verify_query(test_context, people, "fav_item.data CONTAINS 'f'", 2);
    verify_query(test_context, people, "fav_item.data BEGINSWITH 'f'", 1);
    verify_query(test_context, people, "fav_item.data ENDSWITH 'e'", 2);
    verify_query(test_context, people, "fav_item.data LIKE 'f*'", 1);
    verify_query(test_context, people, "fav_item.data CONTAINS[c] 'F'", 2);
    verify_query(test_context, people, "fav_item.data BEGINSWITH[c] 'F'", 1);
    verify_query(test_context, people, "fav_item.data ENDSWITH[c] 'E'", 2);
    verify_query(test_context, people, "fav_item.data LIKE[c] 'F*'", 1);

    // two column
    verify_query(test_context, people, "fav_item.data == fav_item.nullable_data", 3);
    verify_query(test_context, people, "fav_item.data == fav_item.data", 5);
    verify_query(test_context, people, "fav_item.nullable_data == fav_item.nullable_data", 5);

    verify_query(test_context, items,
                 "data contains NULL && data contains 'fo' && !(data contains 'asdfasdfasdf') && data contains 'rk'",
                 1);
}


TEST(Parser_OverColumnIndexChanges)
{
    Group g;
    TableRef table = g.add_table("table");
    ColKey first_col = table->add_column(type_Int, "to_remove");
    ColKey int_col = table->add_column(type_Int, "ints");
    ColKey double_col = table->add_column(type_Double, "doubles");
    ColKey string_col = table->add_column(type_String, "strings");
    std::vector<ObjKey> keys;
    table->create_objects(3, keys);
    for (size_t i = 0; i < keys.size(); ++i) {
        Obj obj = table->get_object(keys[i]);
        obj.set(int_col, int64_t(i));
        obj.set(double_col, double(i));
        std::string str(i, 'a');
        obj.set(string_col, StringData(str));
    }

    std::string ints_before = verify_query(test_context, table, "ints >= 1", 2).get_description();
    std::string doubles_before = verify_query(test_context, table, "doubles >= 1", 2).get_description();
    std::string strings_before = verify_query(test_context, table, "strings.@count >= 1", 2).get_description();

    table->remove_column(first_col);

    std::string ints_after = verify_query(test_context, table, "ints >= 1", 2).get_description();
    std::string doubles_after = verify_query(test_context, table, "doubles >= 1", 2).get_description();
    std::string strings_after = verify_query(test_context, table, "strings.@count >= 1", 2).get_description();

    CHECK_EQUAL(ints_before, ints_after);
    CHECK_EQUAL(doubles_before, doubles_after);
    CHECK_EQUAL(strings_before, strings_after);
}


TEST(Parser_TwoColumnExpressionBasics)
{
    Group g;
    TableRef table = g.add_table("table");
    ColKey int_col = table->add_column(type_Int, "ints", true);
    ColKey double_col = table->add_column(type_Double, "doubles");
    ColKey string_col = table->add_column(type_String, "strings");
    ColKey link_col = table->add_column_link(type_Link, "link", *table);
    std::vector<ObjKey> keys;
    table->create_objects(3, keys);
    for (size_t i = 0; i < keys.size(); ++i) {
        Obj obj = table->get_object(keys[i]);
        obj.set(int_col, int64_t(i));
        obj.set(double_col, double(i));
        std::string str(i, 'a');
        obj.set(string_col, StringData(str));
    }
    table->get_object(keys[1]).set(link_col, keys[0]);

    Query q = table->where().and_query(table->column<Int>(int_col) == table->column<String>(string_col).size());
    CHECK_EQUAL(q.count(), 3);
    std::string desc = q.get_description();

    verify_query(test_context, table, "ints == 0", 1);
    verify_query(test_context, table, "ints == ints", 3);
    verify_query(test_context, table, "ints == strings.@count", 3);
    verify_query(test_context, table, "strings.@count == ints", 3);
    verify_query(test_context, table, "ints == NULL", 0);
    verify_query(test_context, table, "doubles == doubles", 3);
    verify_query(test_context, table, "strings == strings", 3);
    verify_query(test_context, table, "ints == link.@count", 2); // row 0 has 0 links, row 1 has 1 link

    // type mismatch
    CHECK_THROW_ANY(verify_query(test_context, table, "doubles == ints", 0));
    CHECK_THROW_ANY(verify_query(test_context, table, "doubles == strings", 0));
    CHECK_THROW_ANY(verify_query(test_context, table, "ints == doubles", 0));
    CHECK_THROW_ANY(verify_query(test_context, table, "strings == doubles", 0));

}


TEST(Parser_TwoColumnAggregates)
{
    Group g;

    TableRef discounts = g.add_table("class_Discounts");
    ColKey discount_name_col = discounts->add_column(type_String, "promotion", true);
    ColKey discount_off_col = discounts->add_column(type_Double, "reduced_by");
    ColKey discount_active_col = discounts->add_column(type_Bool, "active");

    using discount_t = std::pair<double, bool>;
    std::vector<discount_t> discount_info = {{3.0, false}, {2.5, true}, {0.50, true}, {1.50, true}};
    std::vector<ObjKey> discount_keys;
    discounts->create_objects(discount_info.size(), discount_keys);
    for (size_t i = 0; i < discount_keys.size(); ++i) {
        Obj obj = discounts->get_object(discount_keys[i]);
        obj.set(discount_off_col, discount_info[i].first);
        obj.set(discount_active_col, discount_info[i].second);
    }
    discounts->get_object(discount_keys[0]).set(discount_name_col, StringData("back to school"));
    discounts->get_object(discount_keys[1]).set(discount_name_col, StringData("pizza lunch special"));
    discounts->get_object(discount_keys[2]).set(discount_name_col, StringData("manager's special"));

    TableRef items = g.add_table("class_Items");
    ColKey item_name_col = items->add_column(type_String, "name");
    ColKey item_price_col = items->add_column(type_Double, "price");
    ColKey item_discount_col = items->add_column_link(type_Link, "discount", *discounts);
    using item_t = std::pair<std::string, double>;
    std::vector<item_t> item_info = {{"milk", 5.5}, {"oranges", 4.0}, {"pizza", 9.5}, {"cereal", 6.5}};
    std::vector<ObjKey> item_keys;
    items->create_objects(item_info.size(), item_keys);
    for (size_t i = 0; i < item_keys.size(); ++i) {
        Obj obj = items->get_object(item_keys[i]);
        obj.set(item_name_col, StringData(item_info[i].first));
        obj.set(item_price_col, item_info[i].second);
    }
    items->get_object(item_keys[0]).set(item_discount_col, discount_keys[2]); // milk -0.50
    items->get_object(item_keys[2]).set(item_discount_col, discount_keys[1]); // pizza -2.5
    items->get_object(item_keys[3]).set(item_discount_col, discount_keys[0]); // cereal -3.0 inactive

    TableRef t = g.add_table("class_Person");
    ColKey id_col = t->add_column(type_Int, "customer_id");
    ColKey account_col = t->add_column(type_Double, "account_balance");
    ColKey items_col = t->add_column_link(type_LinkList, "items", *items);

    Obj person0 = t->create_object();
    Obj person1 = t->create_object();
    Obj person2 = t->create_object();

    person0.set(id_col, int64_t(0));
    person0.set(account_col, double(10.0));
    person1.set(id_col, int64_t(1));
    person1.set(account_col, double(20.0));
    person2.set(id_col, int64_t(2));
    person2.set(account_col, double(30.0));

    LnkLst list_0 = person0.get_linklist(items_col);
    list_0.add(item_keys[0]);
    list_0.add(item_keys[1]);
    list_0.add(item_keys[2]);
    list_0.add(item_keys[3]);

    LnkLst list_1 = person1.get_linklist(items_col);
    for (size_t i = 0; i < 10; ++i) {
        list_1.add(item_keys[0]);
    }

    LnkLst list_2 = person2.get_linklist(items_col);
    list_2.add(item_keys[2]);
    list_2.add(item_keys[2]);
    list_2.add(item_keys[3]);

    // int vs linklist count/size
    verify_query(test_context, t, "customer_id < items.@count", 3);
    verify_query(test_context, t, "customer_id < items.@size", 3);

    // double vs linklist count/size
    verify_query(test_context, t, "items.@min.price > items.@count", 1);
    verify_query(test_context, t, "items.@min.price > items.@size", 1);

    // double vs string/binary count/size is not supported due to a core implementation limitation
    CHECK_THROW_ANY(verify_query(test_context, items, "name.@count > price", 3));
    CHECK_THROW_ANY(verify_query(test_context, items, "price < name.@size", 3));

    // double vs double
    verify_query(test_context, t, "items.@sum.price > account_balance", 2);
    verify_query(test_context, t, "items.@min.price > account_balance", 0);
    verify_query(test_context, t, "items.@max.price > account_balance", 0);
    verify_query(test_context, t, "items.@avg.price > account_balance", 0);

    // cannot aggregate string
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@min.name > account_balance", 0));
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@max.name > account_balance", 0));
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@sum.name > account_balance", 0));
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@avg.name > account_balance", 0));
    // cannot aggregate link
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@min.discount > account_balance", 0));
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@max.discount > account_balance", 0));
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@sum.discount > account_balance", 0));
    CHECK_THROW_ANY(verify_query(test_context, t, "items.@avg.discount > account_balance", 0));

    verify_query(test_context, t, "items.@count < account_balance", 3); // linklist count vs double
    verify_query(test_context, t, "items.@count > 3", 2);   // linklist count vs literal int
    // linklist count vs literal double, integer promotion done here so this is true!
    verify_query(test_context, t, "items.@count == 3.1", 1);

    // two string counts is allowed (int comparison)
    verify_query(test_context, items, "discount.promotion.@count > name.@count", 3);
    // link count vs string count (int comparison)
    verify_query(test_context, items, "discount.@count < name.@count", 4);

    // string operators
    verify_query(test_context, items, "discount.promotion == name", 0);
    verify_query(test_context, items, "discount.promotion != name", 4);
    verify_query(test_context, items, "discount.promotion CONTAINS name", 1);
    verify_query(test_context, items, "discount.promotion BEGINSWITH name", 1);
    verify_query(test_context, items, "discount.promotion ENDSWITH name", 0);
    verify_query(test_context, items, "discount.promotion LIKE name", 0);
    verify_query(test_context, items, "discount.promotion ==[c] name", 0);
    verify_query(test_context, items, "discount.promotion !=[c] name", 4);
    verify_query(test_context, items, "discount.promotion CONTAINS[c] name", 1);
    verify_query(test_context, items, "discount.promotion BEGINSWITH[c] name", 1);
    verify_query(test_context, items, "discount.promotion ENDSWITH[c] name", 0);
    verify_query(test_context, items, "discount.promotion LIKE[c] name", 0);
}

void verify_query_sub(test_util::unit_test::TestContext& test_context, TableRef t, std::string query_string, const util::Any* arg_list, size_t num_args, size_t num_results) {

    query_builder::AnyContext ctx;
    std::string empty_string;
    realm::query_builder::ArgumentConverter<util::Any, query_builder::AnyContext> args(ctx, arg_list, num_args);

    Query q = t->where();

    realm::parser::Predicate p = realm::parser::parse(query_string).predicate;
    realm::query_builder::apply_predicate(q, p, args);

    CHECK_EQUAL(q.count(), num_results);
    std::string description = q.get_description();
    //std::cerr << "original: " << query_string << "\tdescribed: " << description << "\n";
    Query q2 = t->where();

    realm::parser::Predicate p2 = realm::parser::parse(description).predicate;
    realm::query_builder::apply_predicate(q2, p2, args);

    CHECK_EQUAL(q2.count(), num_results);
}

TEST(Parser_substitution)
{
    Group g;
    TableRef t = g.add_table("person");
    ColKey int_col = t->add_column(type_Int, "age");
    ColKey str_col = t->add_column(type_String, "name");
    ColKey double_col = t->add_column(type_Double, "fees");
    ColKey bool_col = t->add_column(type_Bool, "paid", true);
    ColKey time_col = t->add_column(type_Timestamp, "time", true);
    ColKey binary_col = t->add_column(type_Binary, "binary", true);
    ColKey float_col = t->add_column(type_Float, "floats", true);
    ColKey nullable_double_col = t->add_column(type_Float, "nuldouble", true);
    ColKey link_col = t->add_column_link(type_Link, "links", *t);
    ColKey list_col = t->add_column_link(type_LinkList, "list", *t);
    std::vector<std::string> names = {"Billy", "Bob", "Joe", "Jane", "Joel"};
    std::vector<double> fees = { 2.0, 2.23, 2.22, 2.25, 3.73 };
    std::vector<ObjKey> obj_keys;
    t->create_objects(names.size(), obj_keys);

    for (size_t i = 0; i < obj_keys.size(); ++i) {
        Obj obj = t->get_object(obj_keys[i]);
        obj.set(int_col, int64_t(i));
        obj.set(str_col, StringData(names[i]));
        obj.set(double_col, fees[i]);
    }
    t->get_object(obj_keys[0]).set(bool_col, true);
    t->get_object(obj_keys[1]).set(bool_col, false);
    t->get_object(obj_keys[1])
        .set(time_col, Timestamp(1512130073, 505)); // 2017/12/02 @ 12:47am (UTC) + 505 nanoseconds
    BinaryData bd0("oe");
    BinaryData bd1("eo");
    t->get_object(obj_keys[0]).set(binary_col, bd0);
    t->get_object(obj_keys[1]).set(binary_col, bd1);
    t->get_object(obj_keys[0]).set(float_col, 2.33f);
    t->get_object(obj_keys[1]).set(float_col, 2.22f);
    t->get_object(obj_keys[0]).set(nullable_double_col, 2.33f);
    t->get_object(obj_keys[1]).set(nullable_double_col, 2.22f);
    t->get_object(obj_keys[0]).set(link_col, obj_keys[1]);
    t->get_object(obj_keys[1]).set(link_col, obj_keys[0]);
    LnkLst list_0 = t->get_object(obj_keys[0]).get_linklist(list_col);
    list_0.add(obj_keys[0]);
    list_0.add(obj_keys[1]);
    list_0.add(obj_keys[2]);
    LnkLst list_1 = t->get_object(obj_keys[1]).get_linklist(list_col);
    list_1.add(obj_keys[0]);

    util::Any args[] = {Int(2), Double(2.22), String("oe"), realm::null{}, Bool(true), Timestamp(1512130073, 505),
                        bd0,    Float(2.33),  Int(1),       Int(3),        Int(4),     Bool(false)};
    size_t num_args = 12;
    verify_query_sub(test_context, t, "age > $0", args, num_args, 2);
    verify_query_sub(test_context, t, "age > $0 || fees == $1", args, num_args, 3);
    verify_query_sub(test_context, t, "name CONTAINS[c] $2", args, num_args, 2);
    verify_query_sub(test_context, t, "paid == $3", args, num_args, 3);
    verify_query_sub(test_context, t, "paid != $3", args, num_args, 2);
    verify_query_sub(test_context, t, "paid == $4", args, num_args, 1);
    verify_query_sub(test_context, t, "paid != $4", args, num_args, 4);
    verify_query_sub(test_context, t, "paid = $11", args, num_args, 1);
    verify_query_sub(test_context, t, "time == $5", args, num_args, 1);
    verify_query_sub(test_context, t, "time == $3", args, num_args, 4);
    verify_query_sub(test_context, t, "binary == $6", args, num_args, 1);
    verify_query_sub(test_context, t, "binary == $3", args, num_args, 3);
    verify_query_sub(test_context, t, "floats == $7", args, num_args, 1);
    verify_query_sub(test_context, t, "floats == $3", args, num_args, 3);
    verify_query_sub(test_context, t, "nuldouble == $7", args, num_args, 1);
    verify_query_sub(test_context, t, "nuldouble == $3", args, num_args, 3);
    verify_query_sub(test_context, t, "links == $3", args, num_args, 3);

    // substitutions through collection aggregates is a different code path
    verify_query_sub(test_context, t, "list.@min.age < $0", args, num_args, 2);
    verify_query_sub(test_context, t, "list.@max.age >= $0", args, num_args, 1);
    verify_query_sub(test_context, t, "list.@sum.age >= $0", args, num_args, 1);
    verify_query_sub(test_context, t, "list.@avg.age < $0", args, num_args, 2);
    verify_query_sub(test_context, t, "list.@count > $0", args, num_args, 1);
    verify_query_sub(test_context, t, "list.@size > $0", args, num_args, 1);
    verify_query_sub(test_context, t, "name.@count > $0", args, num_args, 5);
    verify_query_sub(test_context, t, "name.@size > $0", args, num_args, 5);
    verify_query_sub(test_context, t, "binary.@count > $0", args, num_args, 2);
    verify_query_sub(test_context, t, "binary.@size > $0", args, num_args, 2);

    // reusing properties, mixing order
    verify_query_sub(test_context, t, "(age > $0 || fees == $1) && age == $0", args, num_args, 1);

    // negative index
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $-1", args, num_args, 0));
    // missing index
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $", args, num_args, 0));
    // non-numerical index
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $age", args, num_args, 0));
    // leading zero index
    verify_query_sub(test_context, t, "name CONTAINS[c] $002", args, num_args, 2);
    // double digit index
    verify_query_sub(test_context, t, "age == $10", args, num_args, 1);

    std::string message;
    // referencing a parameter outside of the list size throws
    CHECK_THROW_ANY_GET_MESSAGE(verify_query_sub(test_context, t, "age > $0", args, /*num_args*/ 0, 0), message);
    CHECK_EQUAL(message, "Request for argument at index 0 but no arguments are provided");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query_sub(test_context, t, "age > $1", args, /*num_args*/ 1, 0), message);
    CHECK_EQUAL(message, "Request for argument at index 1 but only 1 argument is provided");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query_sub(test_context, t, "age > $2", args, /*num_args*/ 2, 0), message);
    CHECK_EQUAL(message, "Request for argument at index 2 but only 2 arguments are provided");

    // invalid types
    // int
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $1", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $2", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $3", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $4", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $5", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $6", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "age > $7", args, num_args, 0));
    // double
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "fees > $0", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "fees > $2", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "fees > $3", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "fees > $4", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "fees > $5", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "fees > $6", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "fees > $7", args, num_args, 0));
    // float
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "floats > $0", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "floats > $1", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "floats > $2", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "floats > $3", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "floats > $4", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "floats > $5", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "floats > $6", args, num_args, 0));
    // string
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "name == $0", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "name == $1", args, num_args, 0));
                    verify_query_sub(test_context, t, "name == $3", args, num_args, 0);
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "name == $4", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "name == $5", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "name == $6", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "name == $7", args, num_args, 0));
    // bool
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "paid == $0", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "paid == $1", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "paid == $2", args, num_args, 0));
                    verify_query_sub(test_context, t, "paid == $3", args, num_args, 3);
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "paid == $5", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "paid == $6", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "paid == $7", args, num_args, 0));
    // timestamp
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "time == $0", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "time == $1", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "time == $2", args, num_args, 0));
                    verify_query_sub(test_context, t, "time == $3", args, num_args, 4);
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "time == $4", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "time == $6", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "time == $7", args, num_args, 0));
    // binary
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "binary == $0", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "binary == $1", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "binary == $2", args, num_args, 0));
                    verify_query_sub(test_context, t, "binary == $3", args, num_args, 3);
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "binary == $4", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "binary == $5", args, num_args, 0));
    CHECK_THROW_ANY(verify_query_sub(test_context, t, "binary == $7", args, num_args, 0));
}

TEST(Parser_string_binary_encoding)
{
    Group g;
    TableRef t = g.add_table("person");
    ColKey str_col = t->add_column(type_String, "string_col", true);
    ColKey bin_col = t->add_column(type_Binary, "binary_col", true);

    std::vector<std::string> test_strings = {
        // Credit of the following list to https://github.com/minimaxir/big-list-of-naughty-strings (MIT)
        "undefined",
        "undef",
        "null",
        "NULL",
        "(null)",
        "nil",
        "NIL",
        "true",
        "false",
        "True",
        "False",
        "TRUE",
        "FALSE",
        "None",
        "hasOwnProperty",
        "\\\\",
        "1.00",
        "$1.00",
        "1/2",
        "1E2",
        "1E02",
        "1E+02",
        "-1",
        "-1.00",
        "-$1.00",
        "-1/2",
        "-1E2",
        "-1E02",
        "-1E+02",
        "1/0",
        "0/0",
        "-2147483648/-1",
        "-9223372036854775808/-1",
        "-0",
        "-0.0",
        "+0",
        "+0.0",
        "0.00",
        "0..0",
        "0.0.0",
        "0,00",
        "0,,0",
        "0,0,0",
        "0.0/0",
        "1.0/0.0",
        "0.0/0.0",
        "1,0/0,0",
        "0,0/0,0",
        "--1",
        "-.",
        "-,",
        "999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999",
        "NaN",
        "Infinity",
        "-Infinity",
        "INF",
        "1#INF",
        "-1#IND",
        "1#QNAN",
        "1#SNAN",
        "1#IND",
        "0x0",
        "0xffffffff",
        "0xffffffffffffffff",
        "0xabad1dea",
        "123456789012345678901234567890123456789",
        "1,000.00",
        "1 000.00",
        "1'000.00",
        "1,000,000.00",
        "1 000 000.00",
        "1'000'000.00",
        "1.000,00",
        "1 000,00",
        "1'000,00",
        "1.000.000,00",
        "1 000 000,00",
        "1'000'000,00",
        "01000",
        "08",
        "09",
        "2.2250738585072011e-308",
        ",./;'[]\\-=",
        "<>?:\"{}|_+",
        "!@#$%^&*()`~",
        "''",
        "\"\"",
        "'\"'",
        "\"''''\"'\"",
        "\"'\"'\"''''\"",
        "<foo val=“bar” />",
        "<foo val=`bar' />"
    };

    t->create_object(); // nulls
    // add a single char of each value
    for (size_t i = 0; i < 256; ++i) {
        char c = static_cast<char>(i);
        test_strings.push_back(std::string(1, c));
    }
    // a single string of 100 nulls
    test_strings.push_back(std::string(100, '\0'));

    for (const std::string& buff : test_strings) {
        StringData sd(buff);
        BinaryData bd(buff);
        Obj obj = t->create_object();
        obj.set(str_col, sd);
        obj.set(bin_col, bd);
    }

    struct TestValues {
        TestValues(size_t processed, bool replace)
            : num_processed(processed)
            , should_be_replaced(replace)
        {
        }
        TestValues()
        {
        }
        size_t num_processed = 0;
        bool should_be_replaced = false;
    };

    std::unordered_map<unsigned char, TestValues> expected_replacements;
    expected_replacements['\x0'] = TestValues{0, true}; // non printable characters require replacement
    expected_replacements['\x7f'] = TestValues{0, true};
    expected_replacements['\x80'] = TestValues{0, true};
    expected_replacements['\xad'] = TestValues{0, true};
    expected_replacements['\xff'] = TestValues{0, true};
    expected_replacements['A'] = TestValues{0, false}; // ascii characters can be represented in plain text
    expected_replacements['z'] = TestValues{0, false};
    expected_replacements['0'] = TestValues{0, false};
    expected_replacements['9'] = TestValues{0, false};
    expected_replacements['"'] = TestValues{0, true}; // quotes must be replaced as b64
    expected_replacements['\''] = TestValues{0, true};
    static const std::string base64_prefix = "B64\"";
    static const std::string base64_suffix = "==\"";

    for (const std::string& buff : test_strings) {
        size_t num_results = 1;
        Query qstr = t->where().equal(str_col, StringData(buff), true);
        Query qbin = t->where().equal(bin_col, BinaryData(buff));
        CHECK_EQUAL(qstr.count(), num_results);
        CHECK_EQUAL(qbin.count(), num_results);
        std::string string_description = qstr.get_description();
        std::string binary_description = qbin.get_description();

        if (buff.size() == 1) {
            auto it = expected_replacements.find(buff[0]);
            if (it != expected_replacements.end()) {
                ++it->second.num_processed;


                // std::cout << "string: '" << it->first << "' described: " << string_description << std::endl;
                if (!it->second.should_be_replaced) {
                    bool validate = string_description.find(base64_prefix) == std::string::npos &&
                                    string_description.find(base64_suffix) == std::string::npos &&
                                    binary_description.find(base64_prefix) == std::string::npos &&
                                    binary_description.find(base64_suffix) == std::string::npos &&
                                    string_description.find(it->first) != std::string::npos &&
                                    binary_description.find(it->first) != std::string::npos;
                    CHECK(validate);
                    if (!validate) {
                        std::stringstream ss;
                        ss << "string should not be replaced: '" << it->first
                           << "' described: " << string_description;
                        CHECK_EQUAL(ss.str(), "");
                    }
                }
                else {
                    size_t str_b64_pre_pos = string_description.find(base64_prefix);
                    size_t str_b64_suf_pos = string_description.find(base64_suffix);
                    size_t bin_b64_pre_pos = binary_description.find(base64_prefix);
                    size_t bin_b64_suf_pos = binary_description.find(base64_suffix);

                    bool validate = str_b64_pre_pos != std::string::npos && str_b64_suf_pos != std::string::npos &&
                                    bin_b64_pre_pos != std::string::npos && bin_b64_suf_pos != std::string::npos;
                    CHECK(validate);

                    size_t contents_str = string_description.find(it->first, str_b64_pre_pos + base64_prefix.size());
                    size_t contents_bin = binary_description.find(it->first, bin_b64_pre_pos + base64_prefix.size());

                    bool validate_contents = contents_str > str_b64_suf_pos && contents_bin > bin_b64_suf_pos;
                    CHECK(validate_contents);
                    if (!validate || !validate_contents) {
                        std::stringstream ss;
                        ss << "string should be replaced: '" << it->first << "' described: " << string_description;
                        CHECK_EQUAL(ss.str(), "");
                    }
                }
            }
        }

        //std::cerr << "original: " << buff << "\tdescribed: " << string_description << "\n";

        query_builder::NoArguments args;
        Query qstr2 = t->where();
        realm::parser::Predicate pstr2 = realm::parser::parse(string_description).predicate;
        realm::query_builder::apply_predicate(qstr2, pstr2, args);
        CHECK_EQUAL(qstr2.count(), num_results);

        Query qbin2 = t->where();
        realm::parser::Predicate pbin2 = realm::parser::parse(binary_description).predicate;
        realm::query_builder::apply_predicate(qbin2, pbin2, args);
        CHECK_EQUAL(qbin2.count(), num_results);
    }

    for (auto it = expected_replacements.begin(); it != expected_replacements.end(); ++it) {
        bool processed = it->second.num_processed == 1;
        CHECK(processed);
        if (!processed) { // the check is expected to fail, but will print which character is failing
            CHECK_EQUAL(it->first, it->second.num_processed);
        }
    }
}


TEST(Parser_collection_aggregates)
{
    Group g;
    TableRef people = g.add_table("class_Person");
    TableRef courses = g.add_table("class_Course");
    auto title_col = courses->add_column(type_String, "title");
    auto credits_col = courses->add_column(type_Double, "credits");
    auto hours_col = courses->add_column(type_Int, "hours_required");
    auto fail_col = courses->add_column(type_Float, "failure_percentage");
    auto int_col = people->add_column(type_Int, "age");
    auto str_col = people->add_column(type_String, "name");
    auto courses_col = people->add_column_link(type_LinkList, "courses_taken", *courses);
    auto binary_col = people->add_column(type_Binary, "hash");
    using info_t = std::pair<std::string, int64_t>;
    std::vector<info_t> person_info
        = {{"Billy", 18}, {"Bob", 17}, {"Joe", 19}, {"Jane", 20}, {"Joel", 18}};
    size_t j = 0;
    for (info_t i : person_info) {
        Obj obj = people->create_object();
        obj.set(str_col, StringData(i.first));
        obj.set(int_col, i.second);
        std::string hash(j++, 'a'); // a repeated j times
        BinaryData payload(hash);
        obj.set(binary_col, payload);
    }
    using cinfo = std::tuple<std::string, double, int64_t, float>;
    std::vector<cinfo> course_info
            = { cinfo{"Math", 5.0, 42, 0.36f}, cinfo{"Comp Sci", 4.5, 45, 0.25f}, cinfo{"Chemistry", 4.0, 41, 0.40f},
            cinfo{"English", 3.5, 40, 0.07f}, cinfo{"Physics", 4.5, 42, 0.42f} };
    std::vector<ObjKey> course_keys;
    for (cinfo course : course_info) {
        Obj obj = courses->create_object();
        course_keys.push_back(obj.get_key());
        obj.set(title_col, StringData(std::get<0>(course)));
        obj.set(credits_col, std::get<1>(course));
        obj.set(hours_col, std::get<2>(course));
        obj.set(fail_col, std::get<3>(course));
    }
    auto it = people->begin();
    LnkLstPtr billy_courses = it->get_linklist_ptr(courses_col);
    billy_courses->add(course_keys[0]);
    billy_courses->add(course_keys[1]);
    billy_courses->add(course_keys[4]);
    ++it;
    LnkLstPtr bob_courses = it->get_linklist_ptr(courses_col);
    bob_courses->add(course_keys[0]);
    bob_courses->add(course_keys[1]);
    bob_courses->add(course_keys[1]);
    ++it;
    LnkLstPtr joe_courses = it->get_linklist_ptr(courses_col);
    joe_courses->add(course_keys[3]);
    ++it;
    LnkLstPtr jane_courses = it->get_linklist_ptr(courses_col);
    jane_courses->add(course_keys[2]);
    jane_courses->add(course_keys[4]);

    Query q = people->where();

    // int
    verify_query(test_context, people, "courses_taken.@min.hours_required <= 41", 2);
    verify_query(test_context, people, "courses_taken.@max.hours_required >= 45", 2);
    verify_query(test_context, people, "courses_taken.@sum.hours_required <= 100", 3);
    verify_query(test_context, people, "courses_taken.@avg.hours_required > 41", 3);

    // double
    verify_query(test_context, people, "courses_taken.@min.credits == 4.5", 2);
    verify_query(test_context, people, "courses_taken.@max.credits == 5.0", 2);
    verify_query(test_context, people, "courses_taken.@sum.credits > 8.6", 2);
    verify_query(test_context, people, "courses_taken.@avg.credits > 4.0", 3);

    // float
    verify_query(test_context, people, "courses_taken.@min.failure_percentage < 0.10", 1);
    verify_query(test_context, people, "courses_taken.@max.failure_percentage > 0.40", 2);
    verify_query(test_context, people, "courses_taken.@sum.failure_percentage > 0.5", 3);
    verify_query(test_context, people, "courses_taken.@avg.failure_percentage > 0.40", 1);

    // count and size are interchangeable but only operate on certain types
    // count of lists
    verify_query(test_context, people, "courses_taken.@count > 2", 2);
    verify_query(test_context, people, "courses_taken.@size > 2", 2);
    verify_query(test_context, people, "courses_taken.@count == 0", 1);
    verify_query(test_context, people, "courses_taken.@size == 0", 1);

    // size of strings
    verify_query(test_context, people, "name.@count == 0", 0);
    verify_query(test_context, people, "name.@size == 0", 0);
    verify_query(test_context, people, "name.@count > 3", 3);
    verify_query(test_context, people, "name.@size > 3", 3);

    // size of binary data
    verify_query(test_context, people, "hash.@count == 0", 1);
    verify_query(test_context, people, "hash.@size == 0", 1);
    verify_query(test_context, people, "hash.@count > 2", 2);
    verify_query(test_context, people, "hash.@size > 2", 2);

    std::string message;

    // string
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@min.title <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@max.title <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@sum.title <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@avg.title <= 41", 2));

    // min, max, sum, avg require a target property on the linked table
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@min <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@max <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@sum <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "courses_taken.@avg <= 41", 2));

    // aggregate operations on a non-linklist column must throw
    CHECK_THROW_ANY(verify_query(test_context, people, "name.@min.hours_required <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "name.@max.hours_required <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "name.@sum.hours_required <= 41", 2));
    CHECK_THROW_ANY(verify_query(test_context, people, "name.@avg.hours_required <= 41", 2));
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, people, "name.@min.hours_required <= 41", 2), message);
    CHECK(message.find("list") != std::string::npos);
    CHECK(message.find("name") != std::string::npos);

    // size and count do not allow paths on the destination object
    CHECK_THROW_ANY(verify_query(test_context, people, "name.@count.hours_required <= 2", 0));
    CHECK_THROW_ANY(verify_query(test_context, people, "name.@size.hours_required <= 2", 0));

    // size is only allowed on certain types
    CHECK_THROW_ANY(verify_query(test_context, people, "age.@size <= 2", 0));
    CHECK_THROW_ANY(verify_query(test_context, courses, "credits.@size == 2", 0));
    CHECK_THROW_ANY(verify_query(test_context, courses, "failure_percentage.@size <= 2", 0));
}

TEST(Parser_NegativeAgg)
{
    Group g;

    TableRef items = g.add_table("class_Items");
    ColKey item_name_col = items->add_column(type_String, "name");
    ColKey item_price_col = items->add_column(type_Double, "price");
    ColKey item_price_float_col = items->add_column(type_Float, "price_float");
    using item_t = std::pair<std::string, double>;
    std::vector<item_t> item_info = {{"milk", -5.5}, {"oranges", -4.0}, {"pizza", -9.5}, {"cereal", -6.5}};
    std::vector<ObjKey> item_keys;
    items->create_objects(item_info.size(), item_keys);
    for (size_t i = 0; i < item_keys.size(); ++i) {
        Obj obj = items->get_object(item_keys[i]);
        obj.set(item_name_col, StringData(item_info[i].first));
        obj.set(item_price_col, item_info[i].second);
        obj.set(item_price_float_col, float(item_info[i].second));
    }

    TableRef t = g.add_table("class_Person");
    ColKey id_col = t->add_column(type_Int, "customer_id");
    ColKey account_col = t->add_column(type_Double, "account_balance");
    ColKey items_col = t->add_column_link(type_LinkList, "items", *items);
    ColKey account_float_col = t->add_column(type_Float, "account_balance_float");

    Obj person0 = t->create_object();
    Obj person1 = t->create_object();
    Obj person2 = t->create_object();

    person0.set(id_col, int64_t(0));
    person0.set(account_col, double(10.0));
    person0.set(account_float_col, float(10.0));
    person1.set(id_col, int64_t(1));
    person1.set(account_col, double(20.0));
    person1.set(account_float_col, float(20.0));
    person2.set(id_col, int64_t(2));
    person2.set(account_col, double(30.0));
    person2.set(account_float_col, float(30.0));

    LnkLst list_0 = person0.get_linklist(items_col);
    list_0.add(item_keys[0]);
    list_0.add(item_keys[1]);
    list_0.add(item_keys[2]);
    list_0.add(item_keys[3]);

    LnkLst list_1 = person1.get_linklist(items_col);
    for (size_t i = 0; i < 10; ++i) {
        list_1.add(item_keys[0]);
    }

    LnkLst list_2 = person2.get_linklist(items_col);
    list_2.add(item_keys[2]);
    list_2.add(item_keys[2]);
    list_2.add(item_keys[3]);

    verify_query(test_context, t, "items.@min.price == -9.5", 2);   // person0, person2
    verify_query(test_context, t, "items.@max.price == -4.0", 1);   // person0
    verify_query(test_context, t, "items.@sum.price == -25.5", 2);  // person0, person2
    verify_query(test_context, t, "items.@avg.price == -6.375", 1); // person0

    verify_query(test_context, t, "items.@min.price_float == -9.5", 2);   // person0, person2
    verify_query(test_context, t, "items.@max.price_float == -4.0", 1);   // person0
    verify_query(test_context, t, "items.@sum.price_float == -25.5", 2);  // person0, person2
    verify_query(test_context, t, "items.@avg.price_float == -6.375", 1); // person0
}

TEST(Parser_SortAndDistinctSerialisation)
{
    Group g;
    TableRef people = g.add_table("person");
    TableRef accounts = g.add_table("account");

    ColKey name_col = people->add_column(type_String, "name");
    ColKey age_col = people->add_column(type_Int, "age");
    ColKey account_col = people->add_column_link(type_Link, "account", *accounts);

    ColKey balance_col = accounts->add_column(type_Double, "balance");
    ColKey transaction_col = accounts->add_column(type_Int, "num_transactions");

    Obj account0 = accounts->create_object();
    account0.set(balance_col, 50.55);
    account0.set(transaction_col, 2);
    Obj account1 = accounts->create_object();
    account1.set(balance_col, 175.23);
    account1.set(transaction_col, 73);
    Obj account2 = accounts->create_object();
    account2.set(balance_col, 98.92);
    account2.set(transaction_col, 17);

    Obj person0 = people->create_object();
    person0.set(name_col, StringData("Adam"));
    person0.set(age_col, 28);
    Obj person1 = people->create_object();
    person1.set(name_col, StringData("Frank"));
    person1.set(age_col, 30);
    Obj person2 = people->create_object();
    person2.set(name_col, StringData("Ben"));
    person2.set(age_col, 18);

    // person:                      | account:
    // name     age     account     | balance       num_transactions
    // Adam     28      0 ->        | 50.55         2
    // Frank    30      1 ->        | 175.23        73
    // Ben      18      2 ->        | 98.92         17

    // sort serialisation
    TableView tv = people->where().find_all();
    tv.sort(name_col, false);
    tv.sort(age_col, true);
    tv.sort(SortDescriptor({{account_col, balance_col}, {account_col, transaction_col}}, {true, false}));
    std::string description = tv.get_descriptor_ordering_description();
    CHECK(description.find("SORT(account.balance ASC, account.num_transactions DESC, age ASC, name DESC)") != std::string::npos);

    // distinct serialisation
    tv = people->where().find_all();
    tv.distinct(name_col);
    tv.distinct(age_col);
    tv.distinct(DistinctDescriptor({{account_col, balance_col}, {account_col, transaction_col}}));
    description = tv.get_descriptor_ordering_description();
    CHECK(description.find("DISTINCT(name) DISTINCT(age) DISTINCT(account.balance, account.num_transactions)") != std::string::npos);

    // combined sort and distinct serialisation
    tv = people->where().find_all();
    tv.distinct(DistinctDescriptor({{name_col}, {age_col}}));
    tv.sort(SortDescriptor({{account_col, balance_col}, {account_col, transaction_col}}, {true, false}));
    description = tv.get_descriptor_ordering_description();
    CHECK(description.find("DISTINCT(name, age)") != std::string::npos);
    CHECK(description.find("SORT(account.balance ASC, account.num_transactions DESC)") != std::string::npos);
}

TableView get_sorted_view(TableRef t, std::string query_string)
{
    Query q = t->where();
    query_builder::NoArguments args;

    parser::ParserResult result = realm::parser::parse(query_string);
    realm::query_builder::apply_predicate(q, result.predicate, args);
    DescriptorOrdering ordering;
    realm::query_builder::apply_ordering(ordering, t, result.ordering);
    std::string query_description = q.get_description();
    std::string ordering_description = ordering.get_description(t);
    std::string combined = query_description + " " + ordering_description;

    //std::cerr << "original: " << query_string << "\tdescribed: " << combined << "\n";
    Query q2 = t->where();

    parser::ParserResult result2 = realm::parser::parse(combined);
    realm::query_builder::apply_predicate(q2, result2.predicate, args);
    DescriptorOrdering ordering2;
    realm::query_builder::apply_ordering(ordering2, t, result2.ordering);

    TableView tv = q2.find_all();
    tv.apply_descriptor_ordering(ordering2);
    return tv;
}

TEST(Parser_SortAndDistinct)
{
    Group g;
    TableRef people = g.add_table("person");
    TableRef accounts = g.add_table("account");

    ColKey name_col = people->add_column(type_String, "name");
    ColKey age_col = people->add_column(type_Int, "age");
    ColKey account_col = people->add_column_link(type_Link, "account", *accounts);

    ColKey balance_col = accounts->add_column(type_Double, "balance");
    ColKey transaction_col = accounts->add_column(type_Int, "num_transactions");

    Obj account0 = accounts->create_object();
    account0.set(balance_col, 50.55);
    account0.set(transaction_col, 2);
    Obj account1 = accounts->create_object();
    account1.set(balance_col, 50.55);
    account1.set(transaction_col, 73);
    Obj account2 = accounts->create_object();
    account2.set(balance_col, 98.92);
    account2.set(transaction_col, 17);

    Obj p1 = people->create_object();
    p1.set(name_col, StringData("Adam"));
    p1.set(age_col, 28);
    p1.set(account_col, account0.get_key());
    Obj p2 = people->create_object();
    p2.set(name_col, StringData("Frank"));
    p2.set(age_col, 30);
    p2.set(account_col, account1.get_key());
    Obj p3 = people->create_object();
    p3.set(name_col, StringData("Ben"));
    p3.set(age_col, 28);
    p3.set(account_col, account2.get_key());

    // person:                      | account:
    // name     age     account     | balance       num_transactions
    // Adam     28      0 ->        | 50.55         2
    // Frank    30      1 ->        | 50.55         73
    // Ben      28      2 ->        | 98.92         17

    // sort serialisation
    TableView tv = get_sorted_view(people, "age > 0 SORT(age ASC)");
    for (size_t row_ndx = 1; row_ndx < tv.size(); ++row_ndx) {
        CHECK(tv.get(row_ndx - 1).get<Int>(age_col) <= tv.get(row_ndx).get<Int>(age_col));
    }

    tv = get_sorted_view(people, "age > 0 SORT(age DESC)");
    for (size_t row_ndx = 1; row_ndx < tv.size(); ++row_ndx) {
        CHECK(tv.get(row_ndx - 1).get<Int>(age_col) >= tv.get(row_ndx).get<Int>(age_col));
    }

    tv = get_sorted_view(people, "age > 0 SORT(age ASC, name DESC)");
    CHECK_EQUAL(tv.size(), 3);
    CHECK_EQUAL(tv.get(0).get<String>(name_col), "Ben");
    CHECK_EQUAL(tv.get(1).get<String>(name_col), "Adam");
    CHECK_EQUAL(tv.get(2).get<String>(name_col), "Frank");

    tv = get_sorted_view(people, "TRUEPREDICATE SORT(account.balance ascending)");
    for (size_t row_ndx = 1; row_ndx < tv.size(); ++row_ndx) {
        ObjKey link_ndx1 = tv.get(row_ndx - 1).get<ObjKey>(account_col);
        ObjKey link_ndx2 = tv.get(row_ndx).get<ObjKey>(account_col);
        CHECK(accounts->get_object(link_ndx1).get<double>(balance_col) <=
              accounts->get_object(link_ndx2).get<double>(balance_col));
    }

    tv = get_sorted_view(people, "TRUEPREDICATE SORT(account.balance descending)");
    for (size_t row_ndx = 1; row_ndx < tv.size(); ++row_ndx) {
        ObjKey link_ndx1 = tv.get(row_ndx - 1).get<ObjKey>(account_col);
        ObjKey link_ndx2 = tv.get(row_ndx).get<ObjKey>(account_col);
        CHECK(accounts->get_object(link_ndx1).get<double>(balance_col) >=
              accounts->get_object(link_ndx2).get<double>(balance_col));
    }

    tv = get_sorted_view(people, "TRUEPREDICATE DISTINCT(age)");
    CHECK_EQUAL(tv.size(), 2);
    for (size_t row_ndx = 1; row_ndx < tv.size(); ++row_ndx) {
        CHECK(tv.get(row_ndx - 1).get<Int>(age_col) != tv.get(row_ndx).get<Int>(age_col));
    }

    tv = get_sorted_view(people, "TRUEPREDICATE DISTINCT(age, account.balance)");
    CHECK_EQUAL(tv.size(), 3);
    CHECK_EQUAL(tv.get(0).get<String>(name_col), "Adam");
    CHECK_EQUAL(tv.get(1).get<String>(name_col), "Frank");
    CHECK_EQUAL(tv.get(2).get<String>(name_col), "Ben");

    tv = get_sorted_view(people, "TRUEPREDICATE DISTINCT(age) DISTINCT(account.balance)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get(0).get<String>(name_col), "Adam");

    tv = get_sorted_view(people, "TRUEPREDICATE SORT(age ASC) DISTINCT(age)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get(0).get<Int>(age_col), 28);
    CHECK_EQUAL(tv.get(1).get<Int>(age_col), 30);

    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name DESC) DISTINCT(age) SORT(name ASC) DISTINCT(name)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get(0).get<String>(name_col), "Ben");
    CHECK_EQUAL(tv.get(1).get<String>(name_col), "Frank");

    tv = get_sorted_view(people, "account.num_transactions > 10 SORT(name ASC)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get(0).get<String>(name_col), "Ben");
    CHECK_EQUAL(tv.get(1).get<String>(name_col), "Frank");

    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(get_sorted_view(people, "TRUEPREDICATE DISTINCT(balance)"), message);
    CHECK_EQUAL(message, "No property 'balance' found on object type 'person' specified in 'distinct' clause");

    CHECK_THROW_ANY_GET_MESSAGE(get_sorted_view(people, "TRUEPREDICATE sort(account.name ASC)"), message);
    CHECK_EQUAL(message, "No property 'name' found on object type 'account' specified in 'sort' clause");
}

TEST(Parser_Limit)
{
    SHARED_GROUP_TEST_PATH(path);
    std::unique_ptr<Replication> hist(make_in_realm_history(path));
    auto sg = DB::create(*hist, DBOptions(crypt_key()));

    auto wt = sg->start_write();
    TableRef people = wt->add_table("person");

    auto name_col = people->add_column(type_String, "name");
    people->add_column(type_Int, "age");

    people->create_object().set_all("Adam", 28);
    people->create_object().set_all("Frank", 30);
    people->create_object().set_all("Ben", 28);

    // solely limit
    TableView tv = get_sorted_view(people, "TRUEPREDICATE LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 3);
    tv = get_sorted_view(people, "TRUEPREDICATE LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 2);
    tv = get_sorted_view(people, "TRUEPREDICATE LIMIT(2)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    tv = get_sorted_view(people, "TRUEPREDICATE LIMIT(3)");
    CHECK_EQUAL(tv.size(), 3);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "TRUEPREDICATE LIMIT(4)");
    CHECK_EQUAL(tv.size(), 3);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);

    // sort + limit
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 3);
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 2);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) LIMIT(2)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    CHECK_EQUAL(tv[1].get<String>(name_col), "Ben");
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) LIMIT(3)");
    CHECK_EQUAL(tv.size(), 3);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    CHECK_EQUAL(tv[1].get<String>(name_col), "Ben");
    CHECK_EQUAL(tv[2].get<String>(name_col), "Frank");
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) LIMIT(4)");
    CHECK_EQUAL(tv.size(), 3);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);

    // sort + distinct + limit
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) DISTINCT(age) LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 2);
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) DISTINCT(age) LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) DISTINCT(age) LIMIT(2)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    CHECK_EQUAL(tv[1].get<String>(name_col), "Frank");
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) DISTINCT(age) LIMIT(3)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    CHECK_EQUAL(tv[1].get<String>(name_col), "Frank");
    tv = get_sorted_view(people, "TRUEPREDICATE SORT(name ASC) DISTINCT(age) LIMIT(4)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);

    // query + limit
    tv = get_sorted_view(people, "age < 30 SORT(name ASC) DISTINCT(age) LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    tv = get_sorted_view(people, "age < 30 SORT(name ASC) DISTINCT(age) LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    tv = get_sorted_view(people, "age < 30 SORT(name ASC) DISTINCT(age) LIMIT(2)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    tv = get_sorted_view(people, "age < 30 SORT(name ASC) DISTINCT(age) LIMIT(3)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    tv = get_sorted_view(people, "age < 30 SORT(name ASC) DISTINCT(age) LIMIT(4)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);

    // compound query + limit
    tv = get_sorted_view(people, "age < 30 && name == 'Adam' LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    tv = get_sorted_view(people, "age < 30 && name == 'Adam' LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");

    // limit multiple times, order matters
    tv = get_sorted_view(people, "TRUEPREDICATE LIMIT(2) LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 2);
    tv = get_sorted_view(people, "TRUEPREDICATE LIMIT(3) LIMIT(2) LIMIT(1) LIMIT(10)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 2);
    tv = get_sorted_view(people, "age > 0 SORT(name ASC) LIMIT(2)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    CHECK_EQUAL(tv[1].get<String>(name_col), "Ben");
    tv = get_sorted_view(people, "age > 0 LIMIT(2) SORT(name ASC)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Adam");
    CHECK_EQUAL(tv[1].get<String>(name_col), "Frank");
    tv = get_sorted_view(people, "age > 0 SORT(name ASC) LIMIT(2) DISTINCT(age)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1); // the other result is excluded by distinct not limit
    tv = get_sorted_view(people, "age > 0 SORT(name DESC) LIMIT(2) SORT(age ASC) LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 2);
    CHECK_EQUAL(tv[0].get<String>(name_col), "Ben");

    // size_unlimited() checks
    tv = get_sorted_view(people, "age == 30");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 30 LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    tv = get_sorted_view(people, "age == 1000");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 1000 LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 1000 SORT(name ASC)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 1000 SORT(name ASC) LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 28 SORT(name ASC)");
    CHECK_EQUAL(tv.size(), 2);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 28 SORT(name ASC) LIMIT(1)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    tv = get_sorted_view(people, "age == 28 DISTINCT(age)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 28 DISTINCT(age) LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    tv = get_sorted_view(people, "age == 28 SORT(name ASC) DISTINCT(age)");
    CHECK_EQUAL(tv.size(), 1);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "age == 28 SORT(name ASC) DISTINCT(age) LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 1);
    tv = get_sorted_view(people, "FALSEPREDICATE");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "FALSEPREDICATE LIMIT(0)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);
    tv = get_sorted_view(people, "FALSEPREDICATE LIMIT(1)");
    CHECK_EQUAL(tv.size(), 0);
    CHECK_EQUAL(tv.get_num_results_excluded_by_limit(), 0);

    // errors
    CHECK_THROW_ANY(get_sorted_view(people, "TRUEPREDICATE LIMIT(-1)"));    // only accepting positive integers
    CHECK_THROW_ANY(get_sorted_view(people, "TRUEPREDICATE LIMIT(age)"));   // only accepting positive integers
    CHECK_THROW_ANY(get_sorted_view(people, "TRUEPREDICATE LIMIT('age')")); // only accepting positive integers

    wt->commit();

    // handover
    auto reader = sg->start_read();
    ConstTableRef peopleRead = reader->get_table("person");

    TableView items = peopleRead->where().find_all();
    CHECK_EQUAL(items.size(), 3);
    realm::DescriptorOrdering desc;
    CHECK(!desc.will_apply_limit());
    desc.append_limit(1);
    CHECK(desc.will_apply_limit());
    items.apply_descriptor_ordering(desc);
    CHECK_EQUAL(items.size(), 1);

    auto tr = reader->duplicate();
    auto tv2 = tr->import_copy_of(items, PayloadPolicy::Copy);
    CHECK(tv2->is_attached());
    CHECK(tv2->is_in_sync());
    CHECK_EQUAL(tv2->size(), 1);
}


TEST(Parser_IncludeDescriptor)
{
    Group g;
    TableRef people = g.add_table("person");
    TableRef accounts = g.add_table("account");

    auto name_col = people->add_column(type_String, "name");
    auto age_col = people->add_column(type_Int, "age");
    auto account_col = people->add_column_link(type_Link, "account", *accounts);
    static_cast<void>(age_col);     // silence warning
    static_cast<void>(account_col); // do

    auto balance_col = accounts->add_column(type_Double, "balance");
    static_cast<void>(balance_col);
    auto transaction_col = accounts->add_column(type_Int, "num_transactions");

    auto a0 = accounts->create_object().set_all(50.55, 2);
    auto a1 = accounts->create_object().set_all(50.55, 73);
    auto a2 = accounts->create_object().set_all(98.92, 17);

    auto p0 = people->create_object().set_all("Adam", 28, a0.get_key());
    auto p1 = people->create_object().set_all("Frank", 30, a1.get_key());
    auto p2 = people->create_object().set_all("Ben", 28, a2.get_key());

    // person:                      | account:
    // name     age     account     | balance       num_transactions
    // Adam     28      0 ->        | 50.55         2
    // Frank    30      1 ->        | 50.55         73
    // Ben      28      2 ->        | 98.92         17

    // include serialisation
    TableView tv = get_sorted_view(accounts, "balance > 0 SORT(num_transactions ASC) INCLUDE(@links.person.account)");
    IncludeDescriptor includes = tv.get_include_descriptors();
    CHECK(includes.is_valid());
    CHECK_EQUAL(tv.size(), 3);

    CHECK_EQUAL(tv.get(0).get<Int>(transaction_col), 2);
    CHECK_EQUAL(tv.get(1).get<Int>(transaction_col), 17);
    CHECK_EQUAL(tv.get(2).get<Int>(transaction_col), 73);

    std::vector<std::string> expected_include_names;
    auto reporter = [&](const Table* table, const std::unordered_set<ObjKey>& objs) {
        CHECK(table == people.unchecked_ptr());
        CHECK_EQUAL(expected_include_names.size(), objs.size());
        for (auto obj : objs) {
            std::string obj_value = table->get_object(obj).get<String>(name_col);
            CHECK(std::find(expected_include_names.begin(), expected_include_names.end(), obj_value) !=
                  expected_include_names.end());
        }
    };

    expected_include_names = {"Adam"};
    includes.report_included_backlinks(accounts, tv.get_key(0), reporter);
    expected_include_names = {"Ben"};
    includes.report_included_backlinks(accounts, tv.get_key(1), reporter);
    expected_include_names = {"Frank"};
    includes.report_included_backlinks(accounts, tv.get_key(2), reporter);

    // Error checking
    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(get_sorted_view(people, "age > 0 INCLUDE(account)"), message);
    CHECK_EQUAL(message,
                "Invalid INCLUDE path at [0, 0]: the last part of an included path must be a backlink column.");
    CHECK_THROW_ANY_GET_MESSAGE(get_sorted_view(people, "age > 0 INCLUDE(age)"), message);
    CHECK_EQUAL(message, "Property 'age' is not a link in object of type 'person' in 'INCLUDE' clause");
    CHECK_THROW_ANY_GET_MESSAGE(get_sorted_view(accounts, "balance > 0 INCLUDE(bad_property_name)"), message);
    CHECK_EQUAL(message, "No property 'bad_property_name' on object of type 'account'");
    CHECK_THROW_ANY_GET_MESSAGE(get_sorted_view(people, "age > 0 INCLUDE(@links.person.account)"), message);
    CHECK_EQUAL(message, "No property 'account' found in type 'person' which links to type 'person'");
}


TEST(Parser_IncludeDescriptorMultiple)
{
    Group g;
    TableRef people = g.add_table("person");
    TableRef accounts = g.add_table("account");
    TableRef banks = g.add_table("bank");
    TableRef languages = g.add_table("language");

    auto name_col = people->add_column(type_String, "name");
    auto account_col = people->add_column_link(type_Link, "account", *accounts);

    auto balance_col = accounts->add_column(type_Double, "balance");
    auto transaction_col = accounts->add_column(type_Int, "num_transactions");
    auto account_bank_col = accounts->add_column_link(type_Link, "bank", *banks);

    auto bank_name_col = banks->add_column(type_String, "name");

    auto language_name_col = languages->add_column(type_String, "name");
    auto language_available_col = languages->add_column_link(type_LinkList, "available_from", *banks);

    auto b0 = banks->create_object().set_all("Danske Bank");
    auto b1 = banks->create_object().set_all("RBC");

    auto l0 = languages->create_object().set_all("English");
    auto l1 = languages->create_object().set_all("Danish");
    auto l2 = languages->create_object().set_all("French");
    auto ll0 = l0.get_linklist(language_available_col);
    ll0.add(b0.get_key());
    ll0.add(b1.get_key());
    auto ll1 = l1.get_linklist(language_available_col);
    ll1.add(b0.get_key());
    auto ll2 = l2.get_linklist(language_available_col);
    ll2.add(b1.get_key());

    auto a0 = accounts->create_object().set_all(50.55, 2, b1.get_key());
    auto a1 = accounts->create_object().set_all(50.55, 73, b0.get_key());
    auto a2 = accounts->create_object().set_all(98.92, 17, b1.get_key());

    people->create_object().set_all("Adam", a0.get_key());
    people->create_object().set_all("Frank", a1.get_key());
    people->create_object().set_all("Ben", a2.get_key());

    static_cast<void>(account_col);
    static_cast<void>(balance_col);
    static_cast<void>(account_bank_col);
    static_cast<void>(bank_name_col);
    // person:             | account:                              | bank:        | languages:
    // name    account     | balance       num_transactions  bank  | name         | name      available_from
    // Adam    0 ->        | 50.55         2                 1     | Danske Bank  | English    {0, 1}
    // Frank   1 ->        | 50.55         73                0     | RBC          | Danish     {0}
    // Ben     2 ->        | 98.92         17                1     |              | French     {1}

    // include serialisation
    TableView tv = get_sorted_view(
        accounts,
        "balance > 0 SORT(num_transactions ASC) INCLUDE(@links.person.account, bank.@links.language.available_from)");
    CHECK_EQUAL(tv.size(), 3);

    CHECK_EQUAL(tv.get(0).get<Int>(transaction_col), 2);
    CHECK_EQUAL(tv.get(1).get<Int>(transaction_col), 17);
    CHECK_EQUAL(tv.get(2).get<Int>(transaction_col), 73);

    std::vector<std::string> expected_people_names;
    std::vector<std::string> expected_language_names;
    auto reporter = [&](const Table* table, const std::unordered_set<ObjKey>& rows) {
        CHECK(table == people.unchecked_ptr() || table == languages.unchecked_ptr());
        if (table == people.unchecked_ptr()) {
            CHECK_EQUAL(expected_people_names.size(), rows.size());
            for (auto row : rows) {
                std::string row_value = table->get_object(row).get<StringData>(name_col);
                CHECK(std::find(expected_people_names.begin(), expected_people_names.end(), row_value) !=
                      expected_people_names.end());
            }
        }
        else if (table == languages.unchecked_ptr()) {
            CHECK_EQUAL(expected_language_names.size(), rows.size());
            if (expected_language_names.size() != rows.size()) {
                throw std::runtime_error("blob");
            }
            for (auto row : rows) {
                std::string row_value = table->get_object(row).get<StringData>(language_name_col);
                CHECK(std::find(expected_language_names.begin(), expected_language_names.end(), row_value) !=
                      expected_language_names.end());
            }
        }
    };

    {
        IncludeDescriptor includes = tv.get_include_descriptors();
        CHECK(includes.is_valid());
        expected_people_names = {"Adam"};
        expected_language_names = {"English", "French"};
        includes.report_included_backlinks(accounts, tv.get_key(0), reporter);
        expected_people_names = {"Ben"};
        expected_language_names = {"English", "French"};
        includes.report_included_backlinks(accounts, tv.get_key(1), reporter);
        expected_people_names = {"Frank"};
        expected_language_names = {"Danish", "English"};
        includes.report_included_backlinks(accounts, tv.get_key(2), reporter);
    }
    {
        parser::KeyPathMapping mapping;
        // disable parsing backlink queries, INCLUDE still allows backlinks
        mapping.set_allow_backlinks(false);
        // include supports backlink mappings
        mapping.add_mapping(accounts, "account_owner", "@links.person.account");
        mapping.add_mapping(banks, "service_languages", "@links.language.available_from");
        query_builder::NoArguments args;

        Query query = accounts->where();
        realm::parser::ParserResult result = realm::parser::parse(
            "balance > 0 SORT(num_transactions ASC) INCLUDE(account_owner, bank.service_languages)");
        realm::query_builder::apply_predicate(query, result.predicate, args, mapping);
        DescriptorOrdering ordering;
        realm::query_builder::apply_ordering(ordering, accounts, result.ordering, args, mapping);
        CHECK_EQUAL(query.count(), 3);
        IncludeDescriptor includes = ordering.compile_included_backlinks();
        CHECK(includes.is_valid());

        expected_people_names = {"Adam"};
        expected_language_names = {"English", "French"};
        includes.report_included_backlinks(accounts, tv.get_key(0), reporter);
        expected_people_names = {"Ben"};
        expected_language_names = {"English", "French"};
        includes.report_included_backlinks(accounts, tv.get_key(1), reporter);
        expected_people_names = {"Frank"};
        expected_language_names = {"Danish", "English"};
        includes.report_included_backlinks(accounts, tv.get_key(2), reporter);
    }
    {
        parser::KeyPathMapping mapping;
        mapping.set_allow_backlinks(false);
        mapping.add_mapping(banks, "service_languages", "@links.language.available_from");
        query_builder::NoArguments args;

        Query query = accounts->where();
        realm::parser::ParserResult result = realm::parser::parse("balance > 0 SORT(num_transactions ASC)");
        realm::query_builder::apply_predicate(query, result.predicate, args, mapping);
        DescriptorOrdering ordering;
        realm::query_builder::apply_ordering(ordering, accounts, result.ordering, args, mapping);
        CHECK_EQUAL(ordering.compile_included_backlinks().is_valid(), false); // nothing included yet

        DescriptorOrdering parsed_ordering_1;
        realm::parser::DescriptorOrderingState include_1 = realm::parser::parse_include_path("@links.person.account");
        realm::query_builder::apply_ordering(parsed_ordering_1, accounts, include_1, args, mapping);
        CHECK(parsed_ordering_1.compile_included_backlinks().is_valid());
        DescriptorOrdering parsed_ordering_2;
        realm::parser::DescriptorOrderingState include_2 =
            realm::parser::parse_include_path("bank.service_languages");
        realm::query_builder::apply_ordering(parsed_ordering_2, accounts, include_2, args, mapping);
        CHECK(parsed_ordering_2.compile_included_backlinks().is_valid());

        ordering.append_include(parsed_ordering_1.compile_included_backlinks());
        CHECK(ordering.compile_included_backlinks().is_valid());
        ordering.append_include(parsed_ordering_2.compile_included_backlinks());
        CHECK(ordering.compile_included_backlinks().is_valid());

        CHECK_EQUAL(query.count(), 3);
        IncludeDescriptor includes = ordering.compile_included_backlinks();
        CHECK(includes.is_valid());

        expected_people_names = {"Adam"};
        expected_language_names = {"English", "French"};
        includes.report_included_backlinks(accounts, tv.get_key(0), reporter);
        expected_people_names = {"Ben"};
        expected_language_names = {"English", "French"};
        includes.report_included_backlinks(accounts, tv.get_key(1), reporter);
        expected_people_names = {"Frank"};
        expected_language_names = {"Danish", "English"};
        includes.report_included_backlinks(accounts, tv.get_key(2), reporter);

        std::string message;
        CHECK_THROW_ANY_GET_MESSAGE(realm::parser::parse_include_path(""), message);
        CHECK(message.find("Invalid syntax encountered while parsing key path for 'INCLUDE'.") != std::string::npos);
        CHECK_THROW_ANY_GET_MESSAGE(realm::parser::parse_include_path("2"), message);
        CHECK(message.find("Invalid syntax encountered while parsing key path for 'INCLUDE'.") != std::string::npos);
        CHECK_THROW_ANY_GET_MESSAGE(realm::parser::parse_include_path("something with spaces"), message);
        CHECK(message.find("Invalid syntax encountered while parsing key path for 'INCLUDE'.") != std::string::npos);
    }
}


TEST(Parser_IncludeDescriptorDeepLinks)
{
    Group g;
    TableRef people = g.add_table("person");

    auto name_col = people->add_column(type_String, "name");
    auto link_col = people->add_column_link(type_Link, "father", *people);

    auto bones = people->create_object().set(name_col, "Bones");
    auto john = people->create_object().set(name_col, "John").set(link_col, bones.get_key());
    auto mark = people->create_object().set(name_col, "Mark").set(link_col, john.get_key());
    auto jonathan = people->create_object().set(name_col, "Jonathan").set(link_col, mark.get_key());
    auto eli = people->create_object().set(name_col, "Eli").set(link_col, jonathan.get_key());

    // include serialisation
    TableView tv = get_sorted_view(people, "name contains[c] 'bone' SORT(name DESC) INCLUDE(@links.person.father.@links.person.father.@links.person.father.@links.person.father)");
    IncludeDescriptor includes = tv.get_include_descriptors();
    CHECK(includes.is_valid());
    CHECK_EQUAL(tv.size(), 1);

    CHECK_EQUAL(tv.get_object(0).get<String>(name_col), "Bones");

    size_t cur_ndx_to_check = 0;
    std::vector<std::string> expected_include_names;
    auto reporter = [&](const Table* table, const std::unordered_set<ObjKey>& rows) {
        CHECK(table == people.unchecked_ptr());
        CHECK_EQUAL(1, rows.size());
        std::string row_value = table->get_object(*rows.begin()).get<String>(name_col);
        CHECK_EQUAL(row_value, expected_include_names[cur_ndx_to_check++]);
    };

    expected_include_names = {"John", "Mark", "Jonathan", "Eli"};
    includes.report_included_backlinks(people, tv.get_key(0), reporter);
}


TEST(Parser_Backlinks)
{
    Group g;

    TableRef items = g.add_table("class_Items");
    ColKey item_name_col = items->add_column(type_String, "name");
    ColKey item_price_col = items->add_column(type_Double, "price");
    using item_t = std::pair<std::string, double>;
    std::vector<item_t> item_info = {
        {"milk", 5.5}, {"oranges", 4.0}, {"pizza", 9.5}, {"cereal", 6.5}, {"bread", 3.5}};
    std::vector<ObjKey> item_keys;
    items->create_objects(item_info.size(), item_keys);
    for (size_t i = 0; i < item_info.size(); ++i) {
        Obj row_obj = items->get_object(item_keys[i]);
        item_t cur_item = item_info[i];
        row_obj.set(item_name_col, StringData(cur_item.first));
        row_obj.set(item_price_col, cur_item.second);
    }

    TableRef t = g.add_table("class_Person");
    ColKey id_col = t->add_column(type_Int, "customer_id");
    ColKey name_col = t->add_column(type_String, "name");
    ColKey account_col = t->add_column(type_Double, "account_balance");
    ColKey items_col = t->add_column_link(type_LinkList, "items", *items);
    ColKey fav_col = t->add_column_link(type_Link, "fav_item", *items);
    std::vector<ObjKey> people_keys;
    t->create_objects(3, people_keys);
    for (size_t i = 0; i < people_keys.size(); ++i) {
        Obj obj = t->get_object(people_keys[i]);
        obj.set(id_col, int64_t(i));
        obj.set(account_col, double((i + 1) * 10.0));
        obj.set(fav_col, obj.get_key());
        if (i == 0) {
            obj.set(name_col, StringData("Adam"));
            LnkLst list_0 = obj.get_linklist(items_col);
            list_0.add(item_keys[0]);
            list_0.add(item_keys[1]);
            list_0.add(item_keys[2]);
            list_0.add(item_keys[3]);
        }
        else if (i == 1) {
            obj.set(name_col, StringData("James"));
            LnkLst list_1 = obj.get_linklist(items_col);
            for (size_t j = 0; j < 10; ++j) {
                list_1.add(item_keys[0]);
            }
        }
        else if (i == 2) {
            obj.set(name_col, StringData("John"));
            LnkLst list_2 = obj.get_linklist(items_col);
            list_2.add(item_keys[2]);
            list_2.add(item_keys[2]);
            list_2.add(item_keys[3]);
        }
    }

    Query q = items->backlink(*t, fav_col).column<Double>(account_col) > 20;
    CHECK_EQUAL(q.count(), 1);
    std::string desc = q.get_description();
    CHECK(desc.find("@links.class_Person.fav_item.account_balance") != std::string::npos);

    q = items->backlink(*t, items_col).column<Double>(account_col) > 20;
    CHECK_EQUAL(q.count(), 2);
    desc = q.get_description();
    CHECK(desc.find("@links.class_Person.items.account_balance") != std::string::npos);

    // favourite items bought by people who have > 20 in their account
    verify_query(test_context, items, "@links.class_Person.fav_item.account_balance > 20", 1); // backlinks via link
    // items bought by people who have > 20 in their account
    verify_query(test_context, items, "@links.class_Person.items.account_balance > 20", 2); // backlinks via list
    // items bought by people who have 'J' as the first letter of their name
    verify_query(test_context, items, "@links.class_Person.items.name LIKE[c] 'j*'", 3);
    verify_query(test_context, items, "@links.class_Person.items.name BEGINSWITH 'J'", 3);

    // items purchased more than twice
    verify_query(test_context, items, "@links.class_Person.items.@count > 2", 2);
    verify_query(test_context, items, "@LINKS.class_Person.items.@size > 2", 2);
    // items bought by people with only $10 in their account
    verify_query(test_context, items, "@links.class_Person.items.@min.account_balance <= 10", 4);
    // items bought by people with more than $10 in their account
    verify_query(test_context, items, "@links.class_Person.items.@max.account_balance > 10", 3);
    // items bought where the sum of the account balance of purchasers is more than $20
    verify_query(test_context, items, "@links.class_Person.items.@sum.account_balance > 20", 3);
    verify_query(test_context, items, "@links.class_Person.items.@avg.account_balance > 20", 1);

    // subquery over backlinks
    verify_query(test_context, items, "SUBQUERY(@links.class_Person.items, $x, $x.account_balance >= 20).@count > 2",
                 1);

    // backlinks over link
    // people having a favourite item which is also the favourite item of another person
    verify_query(test_context, t, "fav_item.@links.class_Person.fav_item.@count > 1", 0);
    // people having a favourite item which is purchased more than once (by anyone)
    verify_query(test_context, t, "fav_item.@links.class_Person.items.@count > 1 ", 2);

    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, items, "@links.class_Person.items == NULL", 1), message);
    CHECK_EQUAL(message, "Comparing a list property to 'null' is not supported");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, items, "@links.class_Person.fav_item == NULL", 1),
                                message);
    CHECK_EQUAL(message, "Comparing a list property to 'null' is not supported");
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.attr. > 0", 1));
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, items, "@links.class_Factory.items > 0", 1), message);
    CHECK_EQUAL(message, "No property 'items' found in type 'Factory' which links to type 'Items'");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, items, "@links.class_Person.artifacts > 0", 1), message);
    CHECK_EQUAL(message, "No property 'artifacts' found in type 'Person' which links to type 'Items'");

    // check that arbitrary aliasing for named backlinks works
    parser::KeyPathMapping mapping;
    mapping.add_mapping(items, "purchasers", "@links.class_Person.items");
    mapping.add_mapping(t, "money", "account_balance");
    query_builder::NoArguments args;

    q = items->where();
    realm::parser::Predicate p = realm::parser::parse("purchasers.@count > 2").predicate;
    realm::query_builder::apply_predicate(q, p, args, mapping);
    CHECK_EQUAL(q.count(), 2);

    q = items->where();
    p = realm::parser::parse("purchasers.@max.money >= 20").predicate;
    realm::query_builder::apply_predicate(q, p, args, mapping);
    CHECK_EQUAL(q.count(), 3);

    // disable parsing backlink queries
    mapping.set_allow_backlinks(false);
    q = items->where();
    p = realm::parser::parse("purchasers.@max.money >= 20").predicate;
    CHECK_THROW_ANY_GET_MESSAGE(realm::query_builder::apply_predicate(q, p, args, mapping), message);
    CHECK_EQUAL(message, "Querying over backlinks is disabled but backlinks were found in the inverse relationship "
                         "of property 'items' on type 'Person'");

    // check that arbitrary aliasing for named backlinks works with a arbitrary prefix
    parser::KeyPathMapping mapping_with_prefix;
    mapping_with_prefix.set_backlink_class_prefix("class_");
    mapping_with_prefix.add_mapping(items, "purchasers", "@links.Person.items");
    mapping_with_prefix.add_mapping(t, "money", "account_balance");

    q = items->where();
    p = realm::parser::parse("purchasers.@count > 2").predicate;
    realm::query_builder::apply_predicate(q, p, args, mapping_with_prefix);
    CHECK_EQUAL(q.count(), 2);

    q = items->where();
    p = realm::parser::parse("purchasers.@max.money >= 20").predicate;
    realm::query_builder::apply_predicate(q, p, args, mapping_with_prefix);
    CHECK_EQUAL(q.count(), 3);
}


TEST(Parser_BacklinkCount)
{
    Group g;

    TableRef items = g.add_table("class_Items");
    items->add_column(type_Int, "item_id");
    auto item_link_col = items->add_column_link(type_Link, "self", *items);
    items->add_column(type_Double, "double_col");

    std::vector<int64_t> item_ids{5, 2, 12, 14, 20};
    ObjKeyVector item_keys(item_ids);
    for (size_t i = 0; i < item_keys.size(); ++i) {
        items->create_object(item_keys[i]).set_all(item_ids[i], item_keys[i], double(i) + 0.5);
    }
    items->get_object(item_keys[4]).set(item_link_col, null_key); // last item will have a total of 0 backlinks

    TableRef t = g.add_table("class_Person");
    auto id_col = t->add_column(type_Int, "customer_id");
    auto items_col = t->add_column_link(type_LinkList, "items", *items);
    auto fav_col = t->add_column_link(type_Link, "fav_item", *items);
    auto float_col = t->add_column(type_Float, "float_col");

    for (int i = 0; i < 3; ++i) {
        Obj obj = t->create_object();
        obj.set(id_col, i);
        obj.set(fav_col, item_keys[2 - i]);
        obj.set(float_col, float(i) + 0.5f);
    }

    auto it = t->begin();
    auto list_0 = it->get_linklist(items_col);
    list_0.add(item_keys[0]);
    list_0.add(item_keys[1]);
    list_0.add(item_keys[2]);

    ++it;
    auto list_1 = it->get_linklist(items_col);
    for (size_t i = 0; i < 10; ++i) {
        list_1.add(item_keys[0]);
    }

    ++it;
    auto list_2 = it->get_linklist(items_col);
    list_2.add(item_keys[2]);
    list_2.add(item_keys[2]);

    verify_query(test_context, items, "@links.@count == 0", 1);
    verify_query(test_context, items, "@links.@count == 0 && item_id == 20", 1);
    verify_query(test_context, items, "@links.@count == 1", 1);
    verify_query(test_context, items, "@links.@count == 1 && item_id == 14", 1);
    verify_query(test_context, items, "@links.@count == 5", 1);
    verify_query(test_context, items, "@links.@count == 5 && item_id == 12", 1);
    verify_query(test_context, items, "@links.@count == 3", 1);
    verify_query(test_context, items, "@links.@count == 3 && item_id == 2", 1);
    verify_query(test_context, items, "@links.@count == 13", 1);
    verify_query(test_context, items, "@links.@count == 13 && item_id == 5", 1);

    // @size is still a synonym to @count
    verify_query(test_context, items, "@links.@size == 0", 1);
    verify_query(test_context, items, "@links.@size == 0 && item_id == 20", 1);

    // backlink count through forward links
    verify_query(test_context, t, "fav_item.@links.@count == 5 && fav_item.item_id == 12", 1);
    verify_query(test_context, t, "fav_item.@links.@count == 3 && fav_item.item_id == 2", 1);
    verify_query(test_context, t, "fav_item.@links.@count == 13 && fav_item.item_id == 5", 1);

    // backlink count through lists; the semantics are to sum the backlinks for each connected row
    verify_query(test_context, t, "items.@links.@count == 21 && customer_id == 0", 1);  // 13 + 3 + 5
    verify_query(test_context, t, "items.@links.@count == 130 && customer_id == 1", 1); // 13 * 10
    verify_query(test_context, t, "items.@links.@count == 10 && customer_id == 2", 1);  // 5 + 5

    // backlink count through backlinks first
    verify_query(test_context, items, "@links.class_Items.self.@links.@count == 1 && item_id == 14", 1);
    verify_query(test_context, items, "@links.class_Person.items.@links.@count == 0", 5);

    // backlink count through backlinks and forward links
    verify_query(test_context, items, "@links.class_Person.fav_item.items.@links.@count == 130 && item_id == 2", 1);
    verify_query(test_context, items, "@links.class_Person.fav_item.fav_item.@links.@count == 3 && item_id == 2", 1);

    // backlink count compared to int
    verify_query(test_context, items, "@links.@count == 0", 1);
    verify_query(test_context, items, "@links.@count >= item_id", 2); // 2 items have an id less than
                                                                      // their backlink count
    verify_query(test_context, items, "@links.@count >= @links.class_Person.fav_item.customer_id", 3);

    // backlink count compared to double
    verify_query(test_context, items, "@links.@count == 0.0", 1);
    verify_query(test_context, items, "@links.@count >= double_col", 3);

    // backlink count compared to float
    verify_query(test_context, items, "@links.@count >= @links.class_Person.fav_item.float_col", 3);

    // backlink count compared to link count
    verify_query(test_context, items, "@links.@count >= self.@count", 5);
    verify_query(test_context, t, "items.@count >= fav_item.@links.@count", 1); // second object

    // all backlinks count compared to single column backlink count
    // this is essentially checking if a single column contains all backlinks of a object
    verify_query(test_context, items, "@links.@count == @links.class_Person.fav_item.@count", 1); // item 5 (0 links)
    verify_query(test_context, items, "@links.@count == @links.class_Person.items.@count", 1);    // item 5 (0 links)
    verify_query(test_context, items, "@links.@count == @links.class_Items.self.@count", 2); // items 4,5 (1,0 links)

    std::string message;
    // backlink count requires comparison to a numeric type
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, items, "@links.@count == 'string'", -1), message);
    CHECK_EQUAL(message, "Cannot convert string 'string'");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, items, "@links.@count == 2018-04-09@14:21:0", -1),
                                message);
    CHECK_EQUAL(message, "Attempting to compare a numeric property to a non-numeric value");

    // no suffix after @links.@count is allowed
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@count.item_id == 0", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@count.@avg.items_id == 0", -1));

    // other aggregate operators are not supported
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@avg == 1", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@sum == 1", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@min == 1", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@max == 1", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@avg.item_id == 1", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@sum.item_id == 1", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@min.item_id == 1", -1));
    CHECK_THROW_ANY(verify_query(test_context, items, "@links.@max.item_id == 1", -1));
}


TEST(Parser_SubqueryVariableNames)
{
    Group g;
    util::serializer::SerialisationState test_state;

    TableRef test_table = g.add_table("test");

    CHECK_EQUAL(test_state.get_variable_name(test_table), "$x");

    for (char c = 'a'; c <= 'z'; ++c) {
        std::string col_name = std::string("$") + c;
        test_table->add_column(type_Int, col_name);
    }
    test_state.subquery_prefix_list.push_back("$xx");
    test_state.subquery_prefix_list.push_back("$xy");
    test_state.subquery_prefix_list.push_back("$xz");
    test_state.subquery_prefix_list.push_back("$xa");

    std::string unique_variable = test_state.get_variable_name(test_table);

    CHECK_EQUAL(unique_variable, "$xb");
}


TEST(Parser_Subquery)
{
    Group g;

    TableRef discounts = g.add_table("class_Discounts");
    ColKey discount_name_col = discounts->add_column(type_String, "promotion", true);
    ColKey discount_off_col = discounts->add_column(type_Double, "reduced_by");
    ColKey discount_active_col = discounts->add_column(type_Bool, "active");

    using discount_t = std::pair<double, bool>;
    std::vector<discount_t> discount_info = {{3.0, false}, {2.5, true}, {0.50, true}, {1.50, true}};
    std::vector<ObjKey> discount_keys;
    discounts->create_objects(discount_info.size(), discount_keys);
    for (size_t i = 0; i < discount_keys.size(); ++i) {
        Obj obj = discounts->get_object(discount_keys[i]);
        obj.set(discount_off_col, discount_info[i].first);
        obj.set(discount_active_col, discount_info[i].second);
        if (i == 0) {
            obj.set(discount_name_col, StringData("back to school"));
        }
        else if (i == 1) {
            obj.set(discount_name_col, StringData("pizza lunch special"));
        }
        else if (i == 2) {
            obj.set(discount_name_col, StringData("manager's special"));
        }
    }

    TableRef ingredients = g.add_table("class_Allergens");
    ColKey ingredient_name_col = ingredients->add_column(type_String, "name");
    ColKey population_col = ingredients->add_column(type_Double, "population_affected");
    std::vector<std::pair<std::string, double>> ingredients_list = {
        {"dairy", 0.75}, {"nuts", 0.01}, {"wheat", 0.01}, {"soy", 0.005}};
    std::vector<ObjKey> ingredients_keys;
    ingredients->create_objects(ingredients_list.size(), ingredients_keys);
    for (size_t i = 0; i < ingredients_list.size(); ++i) {
        Obj obj = ingredients->get_object(ingredients_keys[i]);
        obj.set(ingredient_name_col, StringData(ingredients_list[i].first));
        obj.set(population_col, ingredients_list[i].second);
    }

    TableRef items = g.add_table("class_Items");
    ColKey item_name_col = items->add_column(type_String, "name");
    ColKey item_price_col = items->add_column(type_Double, "price");
    ColKey item_discount_col = items->add_column_link(type_Link, "discount", *discounts);
    ColKey item_contains_col = items->add_column_link(type_LinkList, "allergens", *ingredients);
    using item_t = std::pair<std::string, double>;
    std::vector<item_t> item_info = {{"milk", 5.5}, {"oranges", 4.0}, {"pizza", 9.5}, {"cereal", 6.5}};
    std::vector<ObjKey> item_keys;
    items->create_objects(item_info.size(), item_keys);
    for (size_t i = 0; i < item_info.size(); ++i) {
        Obj obj = items->get_object(item_keys[i]);
        obj.set(item_name_col, StringData(item_info[i].first));
        obj.set(item_price_col, item_info[i].second);
        if (i == 0) {
            obj.set(item_discount_col, discount_keys[2]); // milk -0.50
            LnkLst milk_contains = obj.get_linklist(item_contains_col);
            milk_contains.add(ingredients_keys[0]);
        }
        else if (i == 2) {
            obj.set(item_discount_col, discount_keys[1]); // pizza -2.5
            LnkLst pizza_contains = obj.get_linklist(item_contains_col);
            pizza_contains.add(ingredients_keys[0]);
            pizza_contains.add(ingredients_keys[2]);
            pizza_contains.add(ingredients_keys[3]);
        }
        else if (i == 3) {
            obj.set(item_discount_col, discount_keys[0]); // cereal -3.0 inactive
            LnkLst cereal_contains = obj.get_linklist(item_contains_col);
            cereal_contains.add(ingredients_keys[0]);
            cereal_contains.add(ingredients_keys[1]);
            cereal_contains.add(ingredients_keys[2]);
        }
    }

    TableRef t = g.add_table("class_Person");
    ColKey id_col = t->add_column(type_Int, "customer_id");
    ColKey account_col = t->add_column(type_Double, "account_balance");
    ColKey items_col = t->add_column_link(type_LinkList, "items", *items);
    ColKey fav_col = t->add_column_link(type_Link, "fav_item", *items);
    std::vector<ObjKey> people_keys;
    t->create_objects(3, people_keys);
    for (size_t i = 0; i < t->size(); ++i) {
        Obj obj = t->get_object(people_keys[i]);
        obj.set(id_col, int64_t(i));
        obj.set(account_col, double((i + 1) * 10.0));
        obj.set(fav_col, item_keys[i]);
        LnkLst list = obj.get_linklist(items_col);
        if (i == 0) {
            list.add(item_keys[0]);
            list.add(item_keys[1]);
            list.add(item_keys[2]);
            list.add(item_keys[3]);
        }
        else if (i == 1) {
            for (size_t j = 0; j < 10; ++j) {
                list.add(item_keys[0]);
            }
        }
        else if (i == 2) {
            list.add(item_keys[2]);
            list.add(item_keys[2]);
            list.add(item_keys[3]);
        }
    }


    Query sub = items->column<String>(item_name_col).contains("a") && items->column<Double>(item_price_col) > 5.0 &&
                items->link(item_discount_col).column<Double>(discount_off_col) > 0.5 &&
                items->column<Link>(item_contains_col).count() > 1;
    Query q = t->column<Link>(items_col, sub).count() > 1;

    std::string subquery_description = q.get_description();
    CHECK(subquery_description.find("SUBQUERY(items, $x,") != std::string::npos);
    CHECK(subquery_description.find(" $x.name ") != std::string::npos);
    CHECK(subquery_description.find(" $x.price ") != std::string::npos);
    CHECK(subquery_description.find(" $x.discount.reduced_by ") != std::string::npos);
    CHECK(subquery_description.find(" $x.allergens.@count") != std::string::npos);
    TableView tv = q.find_all();
    CHECK_EQUAL(tv.size(), 2);

    // not variations inside/outside subquery, no variable substitution
    verify_query(test_context, t, "SUBQUERY(items, $x, TRUEPREDICATE).@count > 0", 3);
    verify_query(test_context, t, "!SUBQUERY(items, $x, TRUEPREDICATE).@count > 0", 0);
    verify_query(test_context, t, "SUBQUERY(items, $x, !TRUEPREDICATE).@count > 0", 0);
    verify_query(test_context, t, "SUBQUERY(items, $x, FALSEPREDICATE).@count == 0", 3);
    verify_query(test_context, t, "!SUBQUERY(items, $x, FALSEPREDICATE).@count == 0", 0);
    verify_query(test_context, t, "SUBQUERY(items, $x, !FALSEPREDICATE).@count == 0", 0);

    // simple variable substitution
    verify_query(test_context, t, "SUBQUERY(items, $x, 5.5 == $x.price ).@count > 0", 2);
    // string constraint subquery
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.name CONTAINS[c] 'MILK').@count >= 1", 2);
    // compound subquery &&
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.name CONTAINS[c] 'MILK' && $x.price == 5.5).@count >= 1",
                 2);
    // compound subquery ||
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.name CONTAINS[c] 'MILK' || $x.price >= 5.5).@count >= 1",
                 3);
    // variable name change
    verify_query(test_context, t,
                 "SUBQUERY(items, $anyNAME_-0123456789, 5.5 == $anyNAME_-0123456789.price ).@count > 0", 2);
    // variable names cannot contain '.'
    CHECK_THROW_ANY(verify_query(test_context, t, "SUBQUERY(items, $x.y, 5.5 == $x.y.price ).@count > 0", 2));
    // variable name must begin with '$'
    CHECK_THROW_ANY(verify_query(test_context, t, "SUBQUERY(items, x, 5.5 == x.y.price ).@count > 0", 2));
    // subquery with string size
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.name.@size == 4).@count > 0", 2);
    // subquery with list count
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.allergens.@count > 1).@count > 0", 2);
    // subquery with list aggregate operation
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.allergens.@min.population_affected < 0.10).@count > 0", 2);
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.allergens.@max.population_affected > 0.50).@count > 0", 3);
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.allergens.@sum.population_affected > 0.75).@count > 0", 2);
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.allergens.@avg.population_affected > 0.50).@count > 0", 2);
    // two column subquery
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.discount.promotion CONTAINS[c] $x.name).@count > 0", 2);
    // subquery count (int) vs double
    verify_query(test_context, t,
                 "SUBQUERY(items, $x, $x.discount.promotion CONTAINS[c] $x.name).@count < account_balance", 3);
    // subquery over link
    verify_query(test_context, t, "SUBQUERY(fav_item.allergens, $x, $x.name CONTAINS[c] 'dairy').@count > 0", 2);
    // nested subquery
    verify_query(test_context, t, "SUBQUERY(items, $x, SUBQUERY($x.allergens, $allergy, $allergy.name CONTAINS[c] "
                                  "'dairy').@count > 0).@count > 0",
                 3);
    // nested subquery operating on the same table with same variable is not allowed
    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "SUBQUERY(items, $x, "
                                                              "SUBQUERY($x.discount.@links.class_Items.discount, $x, "
                                                              "$x.price > 5).@count > 0).@count > 0",
                                             2),
                                message);
    CHECK_EQUAL(message, "Unable to create a subquery expression with variable '$x' since an identical variable "
                         "already exists in this context");

    // target property must be a list
    CHECK_THROW_ANY_GET_MESSAGE(
        verify_query(test_context, t, "SUBQUERY(account_balance, $x, TRUEPREDICATE).@count > 0", 3), message);
    CHECK_EQUAL(message, "A subquery must operate on a list property, but 'account_balance' is type 'Double'");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "SUBQUERY(fav_item, $x, TRUEPREDICATE).@count > 0", 3),
                                message);
    CHECK_EQUAL(message, "A subquery must operate on a list property, but 'fav_item' is type 'Link'");
}


TEST(Parser_AggregateShortcuts)
{
    Group g;

    TableRef allergens = g.add_table("class_Allergens");
    ColKey ingredient_name_col = allergens->add_column(type_String, "name");
    ColKey population_col = allergens->add_column(type_Double, "population_affected");
    std::vector<std::pair<std::string, double>> allergens_list = {
        {"dairy", 0.75}, {"nuts", 0.01}, {"wheat", 0.01}, {"soy", 0.005}};
    std::vector<ObjKey> allergens_keys;
    allergens->create_objects(allergens_list.size(), allergens_keys);
    for (size_t i = 0; i < allergens_list.size(); ++i) {
        Obj obj = allergens->get_object(allergens_keys[i]);
        obj.set(ingredient_name_col, StringData(allergens_list[i].first));
        obj.set(population_col, allergens_list[i].second);
    }

    TableRef items = g.add_table("class_Items");
    ColKey item_name_col = items->add_column(type_String, "name");
    ColKey item_price_col = items->add_column(type_Double, "price");
    ColKey item_contains_col = items->add_column_link(type_LinkList, "allergens", *allergens);
    using item_t = std::pair<std::string, double>;
    std::vector<item_t> item_info = {{"milk", 5.5}, {"oranges", 4.0}, {"pizza", 9.5}, {"cereal", 6.5}};
    std::vector<ObjKey> items_keys;
    items->create_objects(item_info.size(), items_keys);
    for (size_t i = 0; i < item_info.size(); ++i) {
        Obj obj = items->get_object(items_keys[i]);
        obj.set(item_name_col, StringData(item_info[i].first));
        obj.set(item_price_col, item_info[i].second);
        if (i == 0) {
            LnkLst milk_contains = obj.get_linklist(item_contains_col);
            milk_contains.add(allergens_keys[0]);
        }
        else if (i == 2) {
            LnkLst pizza_contains = obj.get_linklist(item_contains_col);
            pizza_contains.add(allergens_keys[0]);
            pizza_contains.add(allergens_keys[2]);
            pizza_contains.add(allergens_keys[3]);
        }
        else if (i == 3) {
            LnkLst cereal_contains = obj.get_linklist(item_contains_col);
            cereal_contains.add(allergens_keys[0]);
            cereal_contains.add(allergens_keys[1]);
            cereal_contains.add(allergens_keys[2]);
        }
    }

    TableRef t = g.add_table("class_Person");
    ColKey id_col = t->add_column(type_Int, "customer_id");
    ColKey account_col = t->add_column(type_Double, "account_balance");
    ColKey items_col = t->add_column_link(type_LinkList, "items", *items);
    ColKey fav_col = t->add_column_link(type_Link, "fav_item", *items);
    std::vector<ObjKey> people_keys;
    t->create_objects(3, people_keys);
    for (size_t i = 0; i < people_keys.size(); ++i) {
        Obj obj = t->get_object(people_keys[i]);
        obj.set(id_col, int64_t(i));
        obj.set(account_col, double((i + 1) * 10.0));
        obj.set(fav_col, items_keys[i]);
        LnkLst list = obj.get_linklist(items_col);
        if (i == 0) {
            list.add(items_keys[0]);
            list.add(items_keys[1]);
            list.add(items_keys[2]);
            list.add(items_keys[3]);
        }
        else if (i == 1) {
            for (size_t j = 0; j < 10; ++j) {
                list.add(items_keys[0]);
            }
        }
        else if (i == 2) {
            list.add(items_keys[2]);
            list.add(items_keys[2]);
            list.add(items_keys[3]);
        }
    }

    // any is implied over list properties
    verify_query(test_context, t, "items.price == 5.5", 2);

    // check basic equality
    verify_query(test_context, t, "ANY items.price == 5.5", 2);  // 0, 1
    verify_query(test_context, t, "SOME items.price == 5.5", 2); // 0, 1
    verify_query(test_context, t, "ALL items.price == 5.5", 1);  // 1
    verify_query(test_context, t, "NONE items.price == 5.5", 1); // 2

    // and
    verify_query(test_context, t, "customer_id > 0 and ANY items.price == 5.5", 1);
    verify_query(test_context, t, "customer_id > 0 and SOME items.price == 5.5", 1);
    verify_query(test_context, t, "customer_id > 0 and ALL items.price == 5.5", 1);
    verify_query(test_context, t, "customer_id > 0 and NONE items.price == 5.5", 1);
    // or
    verify_query(test_context, t, "customer_id > 1 or ANY items.price == 5.5", 3);
    verify_query(test_context, t, "customer_id > 1 or SOME items.price == 5.5", 3);
    verify_query(test_context, t, "customer_id > 1 or ALL items.price == 5.5", 2);
    verify_query(test_context, t, "customer_id > 1 or NONE items.price == 5.5", 1);
    // not
    verify_query(test_context, t, "!(ANY items.price == 5.5)", 1);
    verify_query(test_context, t, "!(SOME items.price == 5.5)", 1);
    verify_query(test_context, t, "!(ALL items.price == 5.5)", 2);
    verify_query(test_context, t, "!(NONE items.price == 5.5)", 2);

    // inside subquery people with any items containing WHEAT
    verify_query(test_context, t, "SUBQUERY(items, $x, $x.allergens.name CONTAINS[c] 'WHEAT').@count > 0", 2);
    verify_query(test_context, t, "SUBQUERY(items, $x, ANY $x.allergens.name CONTAINS[c] 'WHEAT').@count > 0", 2);
    verify_query(test_context, t, "SUBQUERY(items, $x, SOME $x.allergens.name CONTAINS[c] 'WHEAT').@count > 0", 2);
    verify_query(test_context, t, "SUBQUERY(items, $x, ALL $x.allergens.name CONTAINS[c] 'WHEAT').@count > 0", 1);
    verify_query(test_context, t, "SUBQUERY(items, $x, NONE $x.allergens.name CONTAINS[c] 'WHEAT').@count > 0", 2);

    // backlinks
    verify_query(test_context, items, "ANY @links.class_Person.items.account_balance > 15", 3);
    verify_query(test_context, items, "SOME @links.class_Person.items.account_balance > 15", 3);
    verify_query(test_context, items, "ALL @links.class_Person.items.account_balance > 15", 0);
    verify_query(test_context, items, "NONE @links.class_Person.items.account_balance > 15", 1);

    // links in prefix
    verify_query(test_context, t, "ANY fav_item.allergens.name CONTAINS 'dairy'", 2);
    verify_query(test_context, t, "SOME fav_item.allergens.name CONTAINS 'dairy'", 2);
    verify_query(test_context, t, "ALL fav_item.allergens.name CONTAINS 'dairy'", 2);
    verify_query(test_context, t, "NONE fav_item.allergens.name CONTAINS 'dairy'", 1);

    // links in suffix
    verify_query(test_context, items, "ANY @links.class_Person.items.fav_item.name CONTAINS 'milk'", 4);
    verify_query(test_context, items, "SOME @links.class_Person.items.fav_item.name CONTAINS 'milk'", 4);
    verify_query(test_context, items, "ALL @links.class_Person.items.fav_item.name CONTAINS 'milk'", 1);
    verify_query(test_context, items, "NONE @links.class_Person.items.fav_item.name CONTAINS 'milk'", 0);

    // compare with property
    verify_query(test_context, t, "ANY items.name == fav_item.name", 2);
    verify_query(test_context, t, "SOME items.name == fav_item.name", 2);
    verify_query(test_context, t, "ANY items.price == items.@max.price", 3);
    verify_query(test_context, t, "SOME items.price == items.@max.price", 3);
    verify_query(test_context, t, "ANY items.price == items.@min.price", 3);
    verify_query(test_context, t, "SOME items.price == items.@min.price", 3);
    verify_query(test_context, t, "ANY items.price > items.@avg.price", 2);
    verify_query(test_context, t, "SOME items.price > items.@avg.price", 2);

    // ALL/NONE do not support testing against other columns currently because of how they are implemented in a
    // subquery
    // The restriction is because subqueries must operate on properties on the target table and cannot reference
    // properties in the parent scope. This restriction may be lifted if we actually implement ALL/NONE in core.
    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "ALL items.name == fav_item.name", 1), message);
    CHECK_EQUAL(message, "The comparison in an 'ALL' clause must be between a keypath and a value");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "NONE items.name == fav_item.name", 1), message);
    CHECK_EQUAL(message, "The comparison in an 'NONE' clause must be between a keypath and a value");

    // no list in path should throw
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "ANY fav_item.name == 'milk'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'ANY' or 'SOME' must contain a list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "SOME fav_item.name == 'milk'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'ANY' or 'SOME' must contain a list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "ALL fav_item.name == 'milk'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'ALL' must contain a list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "NONE fav_item.name == 'milk'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'NONE' must contain a list");

    // multiple lists in path should throw
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "ANY items.allergens.name == 'dairy'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'ANY' or 'SOME' must contain only one list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "SOME items.allergens.name == 'dairy'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'ANY' or 'SOME' must contain only one list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "ALL items.allergens.name == 'dairy'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'ALL' must contain only one list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "NONE items.allergens.name == 'dairy'", 1), message);
    CHECK_EQUAL(message, "The keypath following 'NONE' must contain only one list");

    // the expression following ANY/SOME/ALL/NONE must be a keypath list
    // currently this is restricted by the parser syntax so it is a predicate error
    CHECK_THROW_ANY(verify_query(test_context, t, "ANY 'milk' == fav_item.name", 1));
    CHECK_THROW_ANY(verify_query(test_context, t, "SOME 'milk' == fav_item.name", 1));
    CHECK_THROW_ANY(verify_query(test_context, t, "ALL 'milk' == fav_item.name", 1));
    CHECK_THROW_ANY(verify_query(test_context, t, "NONE 'milk' == fav_item.name", 1));
}


TEST(Parser_OperatorIN)
{
    Group g;

    TableRef allergens = g.add_table("class_Allergens");
    ColKey ingredient_name_col = allergens->add_column(type_String, "name");
    ColKey population_col = allergens->add_column(type_Double, "population_affected");
    std::vector<std::pair<std::string, double>> allergens_list = {
        {"dairy", 0.75}, {"nuts", 0.01}, {"wheat", 0.01}, {"soy", 0.005}};
    std::vector<ObjKey> allergens_keys;
    allergens->create_objects(allergens_list.size(), allergens_keys);
    for (size_t i = 0; i < allergens_list.size(); ++i) {
        Obj obj = allergens->get_object(allergens_keys[i]);
        obj.set(ingredient_name_col, StringData(allergens_list[i].first));
        obj.set(population_col, allergens_list[i].second);
    }

    TableRef items = g.add_table("class_Items");
    ColKey item_name_col = items->add_column(type_String, "name");
    ColKey item_price_col = items->add_column(type_Double, "price");
    ColKey item_contains_col = items->add_column_link(type_LinkList, "allergens", *allergens);
    using item_t = std::pair<std::string, double>;
    std::vector<item_t> item_info = {{"milk", 5.5}, {"oranges", 4.0}, {"pizza", 9.5}, {"cereal", 6.5}};
    std::vector<ObjKey> items_keys;
    items->create_objects(item_info.size(), items_keys);
    for (size_t i = 0; i < item_info.size(); ++i) {
        Obj obj = items->get_object(items_keys[i]);
        obj.set(item_name_col, StringData(item_info[i].first));
        obj.set(item_price_col, item_info[i].second);
        if (i == 0) {
            LnkLst milk_contains = obj.get_linklist(item_contains_col);
            milk_contains.add(allergens_keys[0]);
        }
        else if (i == 2) {
            LnkLst pizza_contains = obj.get_linklist(item_contains_col);
            pizza_contains.add(allergens_keys[0]);
            pizza_contains.add(allergens_keys[2]);
            pizza_contains.add(allergens_keys[3]);
        }
        else if (i == 3) {
            LnkLst cereal_contains = obj.get_linklist(item_contains_col);
            cereal_contains.add(allergens_keys[0]);
            cereal_contains.add(allergens_keys[1]);
            cereal_contains.add(allergens_keys[2]);
        }
    }

    TableRef t = g.add_table("class_Person");
    ColKey id_col = t->add_column(type_Int, "customer_id");
    ColKey account_col = t->add_column(type_Double, "account_balance");
    ColKey items_col = t->add_column_link(type_LinkList, "items", *items);
    ColKey fav_col = t->add_column_link(type_Link, "fav_item", *items);
    std::vector<ObjKey> people_keys;
    t->create_objects(3, people_keys);
    for (size_t i = 0; i < people_keys.size(); ++i) {
        Obj obj = t->get_object(people_keys[i]);
        obj.set(id_col, int64_t(i));
        obj.set(account_col, double((i + 1) * 10.0));
        obj.set(fav_col, items_keys[i]);
        LnkLst list = obj.get_linklist(items_col);
        if (i == 0) {
            list.add(items_keys[0]);
            list.add(items_keys[1]);
            list.add(items_keys[2]);
            list.add(items_keys[3]);
        }
        else if (i == 1) {
            for (size_t j = 0; j < 10; ++j) {
                list.add(items_keys[0]);
            }
        }
        else if (i == 2) {
            list.add(items_keys[2]);
            list.add(items_keys[2]);
            list.add(items_keys[3]);
        }
    }

    verify_query(test_context, t, "5.5 IN items.price", 2);
    verify_query(test_context, t, "!(5.5 IN items.price)", 1);              // group not
    verify_query(test_context, t, "'milk' IN items.name", 2);               // string compare
    verify_query(test_context, t, "'MiLk' IN[c] items.name", 2);            // string compare with insensitivity
    verify_query(test_context, t, "NULL IN items.price", 0);                // null
    verify_query(test_context, t, "'dairy' IN fav_item.allergens.name", 2); // through link prefix
    verify_query(test_context, items, "20 IN @links.class_Person.items.account_balance", 1); // backlinks
    verify_query(test_context, t, "fav_item.price IN items.price", 2); // single property in list

    // aggregate modifiers must operate on a list
    CHECK_THROW_ANY(verify_query(test_context, t, "ANY 5.5 IN items.price", 2));
    CHECK_THROW_ANY(verify_query(test_context, t, "SOME 5.5 IN items.price", 2));
    CHECK_THROW_ANY(verify_query(test_context, t, "ALL 5.5 IN items.price", 1));
    CHECK_THROW_ANY(verify_query(test_context, t, "NONE 5.5 IN items.price", 1));

    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "items.price IN 5.5", 1), message);
    CHECK_EQUAL(message, "The expression following 'IN' must be a keypath to a list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "5.5 in fav_item.price", 1), message);
    CHECK_EQUAL(message, "The keypath following 'IN' must contain a list");
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "'dairy' in items.allergens.name", 1), message);
    CHECK_EQUAL(message, "The keypath following 'IN' must contain only one list");
    // list property vs list property is not supported by core yet
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, t, "items.price IN items.price", 0), message);
    CHECK_EQUAL(
        message,
        "The keypath preceeding 'IN' must not contain a list, list vs list comparisons are not currently supported");
}


// we won't support full object comparisons until we have stable keys in core, but as an exception
// we allow comparison with null objects because we can serialise that and bindings use it to check agains nulls.
TEST(Parser_Object)
{
    Group g;
    TableRef table = g.add_table("table");
    auto int_col = table->add_column(type_Int, "ints", true);
    auto link_col = table->add_column_link(type_Link, "link", *table);
    for (size_t i = 0; i < 3; ++i) {
        table->create_object().set<int64_t>(int_col, i);
    }
    table->get_object(1).set(link_col, table->begin()->get_key());
    TableView tv = table->where().find_all();

    verify_query(test_context, table, "link == NULL", 2); // vanilla base check
    // FIXME: verify_query(test_context, table, "link == O0", 2);

    Query q0 = table->where().and_query(table->column<Link>(link_col) == tv.get(0));
    std::string description = q0.get_description(); // shouldn't throw
    CHECK(description.find("O0") != std::string::npos);

    Query q1 = table->column<Link>(link_col) == realm::null();
    description = q1.get_description(); // shouldn't throw
    CHECK(description.find("NULL") != std::string::npos);
    CHECK_EQUAL(q1.count(), 2);

    CHECK_THROW_ANY(verify_query(test_context, table, "link == link", 3));
}

TEST(Parser_Between)
{
    Group g;
    TableRef table = g.add_table("table");
    auto int_col_key = table->add_column(type_Int, "age", true);
    auto between_col_key = table->add_column(type_Int, "between", true);
    for (int i = 0; i < 3; ++i) {
        table->create_object().set(int_col_key, i + 24).set(between_col_key, i);
    }

    // normal querying on a property named "between" is allowed.
    verify_query(test_context, table, "between == 0", 1);
    verify_query(test_context, table, "between > 0", 2);
    verify_query(test_context, table, "between <= 3", 3);

    // operator between is not supported yet, but we at least use a friendly error message.
    std::string message;
    CHECK_THROW_ANY_GET_MESSAGE(verify_query(test_context, table, "age between {20, 25}", 1), message);
    CHECK(message.find("Invalid Predicate. The 'between' operator is not supported yet, please rewrite the expression using '>' and '<'.") != std::string::npos);
}

TEST(Parser_ChainedStringEqualQueries)
{
    Group g;
    TableRef table = g.add_table("table");
    ColKey a_col_ndx = table->add_column(type_String, "a", false);
    ColKey b_col_ndx = table->add_column(type_String, "b", true);
    ColKey c_col_ndx = table->add_column(type_String, "c", false);
    ColKey d_col_ndx = table->add_column(type_String, "d", true);

    table->add_search_index(c_col_ndx);
    table->add_search_index(d_col_ndx);

    std::vector<std::string> populated_data;
    std::stringstream ss;
    for (size_t i = 0; i < 100; ++i) {
        ss.str({});
        ss << i;
        std::string sd(ss.str());
        populated_data.push_back(sd);
        table->create_object().set(a_col_ndx, sd).set(b_col_ndx, sd).set(c_col_ndx, sd).set(d_col_ndx, sd);
    }
    table->create_object(); // one null/empty string

    verify_query(test_context, table, "a == '0' or a == '1' or a == '2'", 3);
    verify_query(test_context, table, "a == '0' or b == '2' or a == '3' or b == '4'", 4);
    verify_query(test_context, table,
                 "(a == '0' or b == '2' or a == '3' or b == '4') and (c == '0' or d == '2' or c == '3' or d == '4')",
                 4);
    verify_query(test_context, table, "a == '' or a == null", 1);
    verify_query(test_context, table, "b == '' or b == null", 1);
    verify_query(test_context, table, "c == '' or c == null", 1);
    verify_query(test_context, table, "d == '' or d == null", 1);
    verify_query(
        test_context, table,
        "(a == null or a == '') and (b == null or b == '') and (c == null or c == '') and (d == null or d == '')", 1);

    Random rd;
    rd.shuffle(populated_data.begin(), populated_data.end());
    std::string query;
    bool first = true;
    char column_to_query = 0;
    for (auto s : populated_data) {
        std::string column_name(1, 'a' + column_to_query);
        query += (first ? "" : " or ") + column_name + " == '" + s + "'";
        first = false;
        column_to_query = (column_to_query + 1) % 4;
    }
    verify_query(test_context, table, query, populated_data.size());
}

TEST(Parser_ChainedIntEqualQueries)
{
    Group g;
    TableRef table = g.add_table("table");
    auto a_col_key = table->add_column(type_Int, "a", false);
    auto b_col_key = table->add_column(type_Int, "b", true);
    auto c_col_key = table->add_column(type_Int, "c", false);
    auto d_col_key = table->add_column(type_Int, "d", true);

    table->add_search_index(c_col_key);
    table->add_search_index(d_col_key);

    std::vector<ObjKey> keys;
    table->create_objects(100, keys);
    std::vector<int64_t> populated_data;
    for (auto o = table->begin(); o != table->end(); ++o) {
        auto payload = o->get_key().value;
        populated_data.push_back(payload);
        o->set(a_col_key, payload);
        o->set(b_col_key, payload);
        o->set(c_col_key, payload);
        o->set(d_col_key, payload);
    }
    auto default_obj = table->create_object(); // one null/default 0 object

    verify_query(test_context, table, "a == 0 or a == 1 or a == 2", 4);
    verify_query(test_context, table, "a == 1 or b == 2 or a == 3 or b == 4", 4);
    verify_query(test_context, table,
                 "(a == 0 or b == 2 or a == 3 or b == 4) and (c == 0 or d == 2 or c == 3 or d == 4)", 5);
    verify_query(test_context, table, "a == 0 or a == null", 2);
    verify_query(test_context, table, "b == 0 or b == null", 2);
    verify_query(test_context, table, "c == 0 or c == null", 2);
    verify_query(test_context, table, "d == 0 or d == null", 2);
    verify_query(
        test_context, table,
        "(a == null or a == 0) and (b == null or b == 0) and (c == null or c == 0) and (d == null or d == 0)", 2);

    Random rd;
    rd.shuffle(populated_data.begin(), populated_data.end());
    std::string query;
    bool first = true;
    char column_to_query = 0;
    for (auto s : populated_data) {
        std::string column_name(1, 'a' + column_to_query);
        std::stringstream ss;
        ss << s;
        query += (first ? "" : " or ") + column_name + " == '" + ss.str() + "'";
        first = false;
        column_to_query = (column_to_query + 1) % 4;
    }
    default_obj.remove();
    verify_query(test_context, table, query, populated_data.size());
}

TEST(Parser_TimestampNullable)
{
    Group g;
    TableRef table = g.add_table("table");
    ColKey a_col = table->add_column(type_Timestamp, "a", false);
    ColKey b_col = table->add_column(type_Timestamp, "b", false);
    table->create_object().set(a_col, Timestamp(7, 0)).set(b_col, Timestamp(17, 0));
    table->create_object().set(a_col, Timestamp(7, 0)).set(b_col, Timestamp(17, 0));

    Query q = table->where()
      .equal(b_col, Timestamp(200, 0))
      .group()
      .equal(a_col, Timestamp(100, 0))
      .Or()
      .equal(a_col, Timestamp(realm::null()))
      .end_group();
    std::string description = q.get_description();
    CHECK(description.find("NULL") != std::string::npos);
    CHECK_EQUAL(description, "b == T200:0 and (a == T100:0 or a == NULL)");
}


#endif // TEST_PARSER
