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

#include <realm.hpp>
#include <gperftools/profiler.h>
#include <unistd.h>
#include <wait.h>

// benchmark - requires linux and links with google-pprof

using namespace realm;

int main(int argc, char* argv[])
{
    int spawns = 0;
    if (argc != 4) {
        printf("Arguments: profile_name num_processes reads_per_write\n");
        return 0;
    }
    spawns = atoi(argv[2]);
    const int reads_per_write = atoi(argv[3]);
    int pid = 0;
    for (int i = 0; i < spawns - 1; i++) {
        pid = fork();
        if (pid == 0) {
            spawns = 0;
            break;
        }
    }
    srand(time(NULL));
    if (spawns) {
        char name[100];
        sprintf(name, "%s_%s_%s.prof", argv[1], argv[2], argv[3]);
        if (strcmp("0", argv[1]))
            ProfilerStart(name);
    }
    {
        DB db("parallel_benchmark.realm", true, DBOptions(DBOptions::Durability::Async));

        for (size_t round = 0; round < 20; ++round) {

            for (size_t i = 0; i < 1000000; ++i) {
                if (reads_per_write != 0 && (i % reads_per_write) == 0) {
                    WriteTransaction trx(db);

                    TableRef t = trx.get_table("test");

                    size_t ndx = rand() % 1000000;

                    int v = t->get_int(0, ndx);
                    t->set_int(0, ndx, v + 1);
                    trx.commit();
                }
                else {
                    ReadTransaction trx(db);

                    ConstTableRef t = trx.get_table("test");

                    size_t ndx = rand() % 1000000;
                    volatile int v;
                    v = t->get_int(0, ndx);
                    (void)v;
                }
            }
        }
    }
    if (spawns)
        if (strcmp("0", argv[1]))
            ProfilerStop();
    while (spawns > 1) {
        int status = 0;
        wait(&status); // wait for child to complete
        spawns--;
    }

    return 0;
}
