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

#include <deque>
#include <string>
#include <sstream>
#include <iostream>

#include <realm.hpp>

using namespace realm;


namespace {

REALM_TABLE_2(MyTable, number, Int, text, String)

} // anonymous namespace


int main(int argc, const char* const argv[])
{
    string database_file = "/tmp/push_data.realm";

    deque<string> positional_args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.size() < 2 || arg.substr(0, 2) != "--") {
            positional_args.push_back(arg);
            continue;
        }

        if (arg == "--database-file") {
            if (i + 1 < argc) {
                database_file = argv[++i];
                continue;
            }
        }
        goto bad_command_line;
    }

    if (positional_args.size() < 2) {
    bad_command_line:
        std::cerr << "ERROR: Bad command line.\n\n"
                     "Synopsis: "
                  << argv[0] << "  NUM-REPS  TEXT...\n\n"
                                "Options:\n"
                                "  --database-file STRING   (default: \""
                  << database_file << "\")\n";
        return 1;
    }

    int num_reps;
    {
        istringstream in(positional_args[0]);
        in >> noskipws >> num_reps;
        if (!in || !in.eof())
            goto bad_command_line;
        positional_args.pop_front();
    }

    DB db(database_file.c_str());
    if (!db.is_valid())
        throw runtime_error("Failed to open database");

    {
        Group& group = db.begin_write();
        if (group.has_table("my_table") && !group.has_table<MyTable>("my_table"))
            throw runtime_error("Table type mismatch");
        MyTable::Ref table = group.get_table<MyTable>("my_table");

        int counter = 0;
        for (int i = 0; i < num_reps; ++i) {
            for (size_t j = 0; j < positional_args.size(); ++j) {
                table->add(++counter, positional_args[j].c_str());
            }
        }
    }

    db.commit(); // FIXME: Must roll back on error

    return 0;
}
