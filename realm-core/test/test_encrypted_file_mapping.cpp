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

// FIXME: Unit tests use POSIX not compatible with Visual Studio and 
// should be updated.
#if defined(TEST_ENCRYPTED_FILE_MAPPING) && !defined(_WIN32)

#include <realm/util/aes_cryptor.hpp>
#include <realm/util/encrypted_file_mapping.hpp>

#include "test.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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

#if REALM_ENABLE_ENCRYPTION

using namespace realm::util;

namespace {
const uint8_t test_key[] = "1234567890123456789012345678901123456789012345678901234567890123";
}

TEST(EncryptedFile_CryptorBasic)
{
    TEST_PATH(path);

    AESCryptor cryptor(test_key);
    cryptor.set_file_size(16);
    const char data[4096] = "test data";
    char buffer[4096];

    int fd = open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    cryptor.write(fd, 0, data, sizeof(data));
    cryptor.read(fd, 0, buffer, sizeof(buffer));
    CHECK(memcmp(buffer, data, strlen(data)) == 0);
    close(fd);
}

TEST(EncryptedFile_CryptorRepeatedWrites)
{
    TEST_PATH(path);
    AESCryptor cryptor(test_key);
    cryptor.set_file_size(16);

    const char data[4096] = "test data";
    char raw_buffer_1[8192] = {0}, raw_buffer_2[8192] = {0};
    int fd = open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    cryptor.write(fd, 0, data, sizeof(data));
    lseek(fd, 0, SEEK_SET);
    ssize_t actual_read_1 = read(fd, raw_buffer_1, sizeof(raw_buffer_1));
    CHECK_EQUAL(actual_read_1, sizeof(raw_buffer_1));

    cryptor.write(fd, 0, data, sizeof(data));
    lseek(fd, 0, SEEK_SET);
    ssize_t actual_read_2 = read(fd, raw_buffer_2, sizeof(raw_buffer_2));
    CHECK_EQUAL(actual_read_2, sizeof(raw_buffer_2));

    CHECK(memcmp(raw_buffer_1, raw_buffer_2, sizeof(raw_buffer_1)) != 0);

    close(fd);
}

TEST(EncryptedFile_SeparateCryptors)
{
    TEST_PATH(path);

    const char data[4096] = "test data";
    char buffer[4096];

    int fd = open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    {
        AESCryptor cryptor(test_key);
        cryptor.set_file_size(16);
        cryptor.write(fd, 0, data, sizeof(data));
    }
    {
        AESCryptor cryptor(test_key);
        cryptor.set_file_size(16);
        cryptor.read(fd, 0, buffer, sizeof(buffer));
    }

    CHECK(memcmp(buffer, data, strlen(data)) == 0);
    close(fd);
}

TEST(EncryptedFile_InterruptedWrite)
{
    TEST_PATH(path);

    const char data[4096] = "test data";

    int fd = open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    {
        AESCryptor cryptor(test_key);
        cryptor.set_file_size(16);
        cryptor.write(fd, 0, data, sizeof(data));
    }

    // Fake an interrupted write which updates the IV table but not the data
    char buffer[4096];
    ssize_t actual_pread = pread(fd, buffer, 64, 0);
    CHECK_EQUAL(actual_pread, 64);

    memcpy(buffer + 32, buffer, 32);
    buffer[5]++; // first byte of "hmac1" field in iv table
    ssize_t actual_pwrite = pwrite(fd, buffer, 64, 0);
    CHECK_EQUAL(actual_pwrite, 64);

    {
        AESCryptor cryptor(test_key);
        cryptor.set_file_size(16);
        cryptor.read(fd, 0, buffer, sizeof(buffer));
        CHECK(memcmp(buffer, data, strlen(data)) == 0);
    }

    close(fd);
}

TEST(EncryptedFile_LargePages)
{
    TEST_PATH(path);

    char data[4096 * 4];
    for (size_t i = 0; i < sizeof(data); ++i)
        data[i] = static_cast<char>(i);

    AESCryptor cryptor(test_key);
    cryptor.set_file_size(sizeof(data));
    char buffer[sizeof(data)];

    int fd = open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    cryptor.write(fd, 0, data, sizeof(data));
    cryptor.read(fd, 0, buffer, sizeof(buffer));
    CHECK(memcmp(buffer, data, sizeof(data)) == 0);
    close(fd);
}

#endif // REALM_ENABLE_ENCRYPTION
#endif // TEST_ENCRYPTED_FILE_MAPPING
