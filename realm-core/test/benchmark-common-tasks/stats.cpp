#include <cstdlib>
#include <stdio.h>
#include <string>
#include <iostream>

#include "compatibility.hpp"

using namespace compatibility;
using namespace realm;

#ifndef REALM_CLUSTER_IF
using ColKey = size_t;
#endif

namespace function {

const static std::string binary = "store_binary";
const static std::string transaction = "make_transactions";

} // end namespace function

void print_useage(std::string program_name) {
    std::cout << "This program performs different functions to profile\n"
        << "a realm file based on the specified parameters.\n"
        << function::binary << " will create a Realm file with the\n"
        << "specified name with a binary blob of the specified size.\n"
        << function::binary << " takes 2 arguments:\n"
        << "\t-output filename\n"
        << "\t-binary blob size\n"
        << "If a file with the same name exists, it will be overwritten.\n"
        << "For example: \n"
        << program_name << " " << function::binary << " simple_realm500.realm 500\n"
        << function::transaction << " will create a Realm file with the\n"
        << "specified name containing the specified number of rows of integers\n"
        << "which have each been set in the specified number of transactions\n"
        << function::transaction << " takes 3 arguments:\n"
        << "\t-output filename\n"
        << "\t-number of transactions\n"
        << "\t-number of rows\n"
        << "If a file with the same name exists, it will be overwritten.\n"
        << "For example: \n"
        << program_name << " " << function::transaction << " trans_10_50.realm 10 50\n";
}

void delete_file_if_exists(std::string file_name)
{
    remove(file_name.c_str());
}

void create_realm_with_data(std::string file_name, size_t data_size)
{
    delete_file_if_exists(file_name);
    DBRef sg = create_new_shared_group(file_name, RealmDurability::Full, nullptr); // DB::create(file_name);
    WrtTrans tr(sg); // TransactionRef tr = sg->start_write();
    TableRef table = tr.add_table("t0");
    auto c0 = table->add_column(type_Binary, "bin_col_0");
    std::string blob(data_size, 'a');
    BinaryData binary(blob.data(), data_size);
#ifdef REALM_CLUSTER_IF
    table->create_object().set(c0, binary);
#else
    table->add_empty_row(1);
    table->set_binary(c0, 0, binary); // copies data into realm
#endif
    tr.commit();
    sg->close();
}

void create_realm_with_transactions(std::string file_name,
                                    size_t num_transactions,
                                    size_t num_rows)
{
    delete_file_if_exists(file_name);
    DBRef sg = create_new_shared_group(file_name, RealmDurability::Full, nullptr);
    const std::string table_name = "table";
    ColKey int_col;
    {
        WrtTrans tr(sg);
        TableRef table = tr.add_table(table_name);
        int_col = table->add_column(type_Int, "int_col_0");
#ifdef REALM_CLUSTER_IF
        std::vector<ObjKey> keys;
        table->create_objects(num_rows, keys);
#else
        table->add_empty_row(num_rows);
#endif
        tr.commit();
    }
    for (size_t i = 0; i < num_transactions; ++i) {
        WrtTrans tr(sg);
        TableRef table = tr.get_table(table_name);
#ifdef REALM_CLUSTER_IF
        size_t row = 0;
        for (auto obj : *table) {
            obj.set<int64_t>(int_col, (i * num_rows) + row);
            row++;
        }
#else
        for (size_t row = 0; row < num_rows; ++row) {
            table->set_int(int_col, row, (i * num_rows) + row);
        }
#endif
        tr.commit();
    }
}

int main(int argc, const char* argv[])
{
    std::string program_name = argv[0];
    if (argc > 1) {
        std::string command = argv[1];
        if (command == function::binary) {
            if (argc == 4) {
                std::string file_name = argv[2];
                long int data_size = atol(argv[3]);
                if (data_size < 0) {
                    std::cout << "Error: Data size cannot be negative.\n";
                    print_useage(program_name);
                    return 1;
                }
                create_realm_with_data(file_name, static_cast<size_t>(data_size));
            }
            else {
                std::cout << "Error: command " << command << " takes exactly 3 arguments.\n";
                print_useage(program_name);
                return 1;
            }
        }
        else if (command == function::transaction) {
            if (argc == 5) {
                std::string file_name = argv[2];
                long int num_transactions = atol(argv[3]);
                long int num_rows = atol(argv[4]);
                if (num_transactions < 0) {
                    std::cout << "Error: number of transactions cannot be negative.\n";
                    print_useage(program_name);
                    return 1;
                }
                if (num_rows < 0) {
                    std::cout << "Error: number of rows cannot be negative.\n";
                    print_useage(program_name);
                    return 1;
                }
                create_realm_with_transactions(file_name,
                                               static_cast<size_t>(num_transactions),
                                               static_cast<size_t>(num_rows));
            }
            else {
                std::cout << "Error: command " << command << " takes exactely 4 arguments.\n";
                print_useage(program_name);
                return 1;
            }
        }
        else {
            std::cout << "Unrecognised command \"" << command << "\"\n";
            print_useage(program_name);
        }
    } else {
        print_useage(program_name);
    }
    return 0;
}
