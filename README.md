[![Build Status](https://travis-ci.com/negativ/skvdb.svg?branch=master)](https://travis-ci.com/negativ/skvdb)
[![Build status](https://ci.appveyor.com/api/projects/status/github/negativ/skvdb?branch=master&svg=true)](https://ci.appveyor.com/project/negativ/skvdb)
[![License](https://img.shields.io/github/license/negativ/skvdb?style=round-square)](https://github.com/negativ/skvdb/blob/master/LICENSE)

# SKVDB
Embedded simple key-value database library written in C++17.

# How to build

On Ubuntu-like distro install C++17-compatible compiler (like G++ or Clang), CMake, Google Test and Boost (filesystem, system) libraries with command like:

```bash
$ sudo apt install g++ cmake cmake-data cmake-extras libgtest-dev googletest libboost1.65-dev
```
Note: check Boost library version in repository of your distro. Boost library version should be >= 1.60

Now you should be able to build SKVDB:

```bash
$ cd /path/to/skvdb/sources
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make && make test
```

The simplest way to build library on Windows is to install Microsoft Visual Studio 2019 with C++ support, open SKVDB project with "Open folder" feature and update CMakeSettings.json file with actual values of BOOST_INCLUDEDIR and BOOST_LIBRARYDIR variables. If you dont have boost library installed on your system, you can download installer with pre-built library from [here](https://sourceforge.net/projects/boost/files/boost-binaries/1.71.0/).

# Usage example

```C++
#include <chrono>
#include <iostream>

#include <ondisk/Volume.hpp>
#include <vfs/Storage.hpp>

int main() {
    using namespace std::literals;
    
    Status initStatus;
    vfs::Storage storage{initStatus};
    auto vol1 = std::make_shared<ondisk::Volume>(initStatus);
    auto vol2 = std::make_shared<ondisk::Volume>(initStatus);

    if (!(vol1->initialize("/tmp", "volume1").isOk() &&
          vol2->initialize("/tmp", "volume2").isOk())) {
        std::cerr << "Unable to initialize ondisk volumes!" << std::endl;
    }

    storage.mount(std::static_pointer_cast<vfs::Ivolume>(vol1),     "/",    "/");                                      // volume #1 become root item for VFS
    storage.mount(std::static_pointer_cast<vfs::Ivolume>(vol1),     "/",    "/combined",    Storage::DefaultPriority); // volume #1 & #2 would be accessible via /combined path
    storage.mount(std::static_pointer_cast<vfs::Ivolume>(vol2),     "/",    "/combined",    Storage::MinPriority);     // volume 2 has minimal priority

    {   // You can work directly with volume #1
        auto handle = vol1->entry("/");

        entry->setProperty("shared_property", vfs::Property{std::vector<char>(1024, 'A')});
        entry->setProperty("int_property", vfs::Property{123});
        entry->setProperty("dbl_property", vfs::Property{873.0});

        vol1->link(*handle, "volume1child"); // creating new child with name volume1child
    }

    {   // You can work directly with volume #2
        auto handle = vol2->open("/");

        handle->setProperty("shared_property", vfs::Property{std::vector<char>(1024, 'B')}); // this property would be shadowed in VFS by volume #1
                                                                                                   // as volume 2 has minimal priority
        handle->setProperty("int_property", vfs::Property{123});
        handle->setProperty("flt_property", vfs::Property{512.0F});

        vol2->link(*handle, "volume2child"); // creating new child with name volume2child

        vol2->close(handle);
    }

    auto handle = storage.entry("/combined");

    if (handle) {
        handle->setProperty("some_text_prop", vfs::Property{"Some text here"});
        handle->setProperty("int_property", vfs::Property{1000});
        handle->setProperty("another_text_property", vfs::Property{"text from VFS"});

        auto [status, properties] = storage.properties(handle);

        if (status.isOk() && !properties.empty()) {
            std::cout << "/combined has properties: " << std::endl;

            for (const auto& [name, value] : properties) {
                std::cout << "\t> " << name << std::endl;
                // Do something with each property/value
            }
        }

        if (auto [s, v] = handle->hasProperty("not_existing"); s.isOk() && v) {
            std::cout << "Property \"not_existing\" doesn\'t exists" << std::endl;
        }

        if (auto [status, value] = handle->property("shared_property"); status.isOk()) {
            if (value == vfs::Property{std::vector<char>(1024, 'A')})
                std::cout << "Property \"shared_property\" full of 'A's" << std::endl;
            else if (value == vfs::Property{std::vector<char>(1024, 'B')})
                std::cout << "Property \"shared_property\" full of 'B's" << std::endl;
            else
                std::cout << "Hmmmm.... Impossible" << std::endl;
        }
        else {
            std::cout << "Property \"shared_property\" doesn\'t exists" << std::endl;
        }

        if (auto [status, links] = storage.links(handle); status.isOk() && !links.empty()) {
            std::cout << "/combined has links: " << std::endl;

            for (const auto& l : links) {
                std::cout << "\t> " << l << std::endl;
                // Do something with each property/value
            }
        }

        handle->expireProperty("some_text_prop", 100ms);

        std::this_thread::sleep_for(200ms);

        handle->removeProperty("int_property");
    }

    {
        auto handle = vol1->open("/");

        if (auto [status, properties] = vol1->properties(handle); status.isOk() && !properties.empty()) {
            std::cout << "volume's #1 '/' has properties: " << std::endl;

            for (const auto& [name, value] : properties) {
                std::cout << "\t> " << name << std::endl;
                // Do something with each property/value
            }
        }
    }

    {
        auto handle = vol2->open("/");

        handle->removeProperty("shared_property");

        if (auto [status, properties] = vol2->properties(handle); status.isOk() && !properties.empty()) {
            std::cout << "volume's #2 '/' has properties: " << std::endl;

            for (const auto& [name, value] : properties) {
                std::cout << "\t> " << name << std::endl;
                // Do something with each property/value
            }
        }
    }

    storage.unmount(std::static_pointer_cast<vfs::Ivolume>(vol2),   "/",    "/combined");
    storage.unmount(std::static_pointer_cast<vfs::Ivolume>(vol1),   "/",    "/combined");
    storage.unmount(std::static_pointer_cast<vfs::Ivolume>(vol1),   "/",    "/");

    vol2->deinitialize();
    vol1->deinitialize();
    
    return 0;
}
```

# Storage format

Each volume consists of two files: Index Table (.index) and Log Device (.logd). 
Log Device format is very simple: just an array of blocks with fixed size (1024/2048/4096/etc., 2048 bytes by default).
Index Table file contains count of records (64-bit LE integer) and array of Index Records (1 root index record with id #1 at least).

Index Record format is following:

Field       | Type               | Description
------------|--------------------|------------
Key         | 64-bit integer, LE | Id of index record
Block index | 32-bit integer, LE | Index of record on Log Device
Bytes count | 32-bit integer, LE | Length of record in bytes

So every record in DB is described by Index Record. Each time you updating some record, it content would be appended to the end of Log Device, there is no way to overwrite old blocks - Log Device works only for read and append. When opening ondisk volume you can override some conditions when Log Device compaction can be started (compaction ratio & min. size of Log Device file to start compaction, for more details see ondisk/Volume.hpp file header). For now SKVDB support only offline compaction of Log Device file.


Each record stored in Log Device in following form:

Field                       | Type               | Description
----------------------------|--------------------|------------
Key                         | 64-bit integer, LE | Id of record (eq. to id of corresponding index record)
Parent                      | 64-bit integer, LE | Index of record on Log Device
Name length                 | 64-bit integer, LE | 
Name                        | String             | Name of record
Properties count            | 64-bit integer, LE | 
Property #1                 | Property           | 
...........                 | Property           | 
Property #N                 | Property           |
Children count              | 64-bit integer, LE | 
Child #1                    | Child              | 
...........                 | Child              | 
Child #N                    | Child              |
Expiring properties count   | 64-bit integer, LE | 
Exp.property #1             | Exp.property       | 
...........                 | Exp.property       | 
Exp.property #N             | Exp.property       |



String format is following:

Field                       | Type               | Description
----------------------------|--------------------|------------
Length                      | 64-bit integer     |
Data                        | Byte array         |


Property format is following:

Field                       | Type               | Description
----------------------------|--------------------|------------
Type tag                    | 16-bit, LE         | 0 - 11 (for now)
Type data                   | Byte array         |


Type tag points to the one of the following types: 
* uint8_t (tag #0)
* int8_t
* uint16_t
* int16_t
* uint32_t
* int32_t
* uint64_t
* int64_t
* float
* double
* string
* vector<char> (BLOB) (tag #11)
  
String and BLOB stored the same way (see String format). Other types stored as LE version of native value.

Child format is following:

Field                       | Type               | Description
----------------------------|--------------------|------------
Name                        | String             | Child name
Key                         | 64-bit integer, LE | Child key


Expiring property format:

Field                       | Type               | Description
----------------------------|--------------------|------------
Name                        | String             | Property name
Timestamp                   | 64-bit integer, LE | UNIX-timestamp, ms

  
# Benchmark

SKVDB was benchmarked on Ubuntu 19.04/18.04 with next CPUs: AMD FX(tm)-8150 @ 3.6GHz @ 8 cores, Intel i7-4820K @ 3.7 GHz @ 8 cores (hyper-threading ON), ARM ThunderX CVM @ 2.0GHz @ 32 cores (aarch64). Each machine has 16 - 32 GB of DDR3 RAM.

All entry property names was up to 12 bytes length long, property types: uint8_t, uint32_t, uint64_t, float, double, string (256 bytes), BLOB (1024 bytes) in equal proportions.

Each benchmark was run on one mount point of virtual storage with 3 disk volume entries mounted to it.

To start benchmark on your own machine run *skv-perfomance-test* binary in terminal window.


**Table 1**. One record, multiple threads writing properties to it. Properties/sec. per-thread

No. threads | FX-8150 @ 3.6GHz | i7-4820K @ 3.7 GHz | ARM ThunderX CVM @ 2.0GHz |
------------|------------------|--------------------|----------------------------
2           | 90000	           | 300000	            | 62000
3	        | 63000	           | 253000	            | 60000
4	        | 66000	           | 233000	            | 53000
5	        | 51000	           | 160000	            | 47000
6	        | 36000	           | 116000	            | 40000
7	        | 36000	           | 123000	            | 42000
8	        | 32000	           | 77000	            | 33000
9	        | 29000	           | 76000	            | 29000
10	        | 22000	           | 62000	            | 29000
11	        | 20000	           | 56000	            | 26000
12	        | 17000	           | 46000	            | 27000
13	        | 20000	           | 48000	            | 26000
14	        | 13000	           | 30000	            | 27000
15	        | 14000	           | 48000	            | 25000
16	        | 15000	           | 24000	            | 23000


![One Record Multiple Threads Writing](/tests/images/one_record_multiple_threads_set_prop.png)


**Table 2**. One record, multiple threads reading properties from it. Properties/sec. per-thread

No. threads | FX-8150 @ 3.6GHz | i7-4820K @ 3.7 GHz | ARM ThunderX CVM @ 2.0GHz |
------------|------------------|--------------------|----------------------------
2	        | 85000	           | 220000	            | 62000
3	        | 73000	           | 270000	            | 62000
4	        | 56000	           | 192000	            | 53000
5	        | 50000	           | 163000	            | 43000
6	        | 39000	           | 147000	            | 62000
7	        | 42000	           | 123000	            | 62000
8	        | 39000	           | 114000	            | 62000
9	        | 34000	           | 102000	            | 40000
10	        | 35000	           | 91000	            | 33000
11	        | 31000	           | 87000	            | 38000
12	        | 30000	           | 79000	            | 33000
13	        | 28000	           | 72000	            | 30000
14	        | 26000	           | 65000	            | 30000
15	        | 23000	           | 64000	            | 33000
16	        | 23000	           | 59000	            | 30000


![One Record Multiple Threads Reading](/tests/images/one_record_multiple_threads_get_prop.png)


**Table 3**. Mutiple records, multiple threads writing properties to them (each thread to owned record). Properties/sec. per-thread

No. threads | FX-8150 @ 3.6GHz | i7-4820K @ 3.7 GHz | ARM ThunderX CVM @ 2.0GHz |
------------|------------------|--------------------|----------------------------
2	        | 77000	           | 244000             | 64000
3	        | 77000	           | 225000	            | 63000
4	        | 55000	           | 215000	            | 63000
5	        | 56000	           | 154000	            | 63000
6	        | 50000	           | 135000	            | 43000
7	        | 39000	           | 123000	            | 38000
8	        | 37000	           | 117000	            | 36000
9	        | 36000	           | 114000	            | 36000
10	        | 32000	           | 103000	            | 34000
11	        | 33000	           | 91000	            | 31000
12	        | 30000	           | 83000	            | 33000
13	        | 27000	           | 72000	            | 34000
14	        | 25000	           | 66000	            | 30000
15	        | 26000	           | 64000	            | 31000
16	        | 25000	           | 60000	            | 31000


![Multiple Records Multiple Threads Writing](/tests/images/multiple_records_multiple_threads_set_prop.png)


**Table 4**. Mutiple records, multiple threads reading properties from them (each thread from owned record). Properties/sec. per-thread

No. threads | FX-8150 @ 3.6GHz | i7-4820K @ 3.7 GHz | ARM ThunderX CVM @ 2.0GHz |
------------|------------------|--------------------|----------------------------
2	        | 77000            | 282000	            | 62000
3	        | 69000	           | 222000	            | 62000
4	        | 54000	           | 204000	            | 62000
5	        | 55000	           | 156000	            | 43000
6	        | 46000	           | 136000	            | 42000
7	        | 40000	           | 119000	            | 35000
8	        | 40000	           | 111000	            | 32000
9	        | 28000	           | 102000	            | 39000
10	        | 32000	           | 93000	            | 39000
11	        | 30000	           | 84000	            | 36000
12	        | 29000	           | 73000	            | 30000
13	        | 29000	           | 74000	            | 29000
14	        | 28000	           | 70000	            | 31000
15	        | 26000	           | 62000	            | 28000
16	        | 25000	           | 56000	            | 29000


![Multiple Records Multiple Threads Reading](/tests/images/multiple_records_multiple_threads_get_prop.png)


**Table 5**. Mutiple records, multiple threads removing properties from them (each thread from owned record). Properties/sec. per-thread

No. threads | FX-8150 @ 3.6GHz | i7-4820K @ 3.7 GHz | ARM ThunderX CVM @ 2.0GHz |
------------|------------------|--------------------|----------------------------
2	        | 57000            | 160000	            | 36000
3	        | 54000	           | 127000	            | 32000
4	        | 44000	           | 99000	            | 30000
5	        | 37000	           | 80000	            | 27000
6	        | 29000	           | 70000	            | 25000
7	        | 30000	           | 60000	            | 24000
8	        | 29000	           | 53000	            | 23000
9	        | 25000	           | 51000	            | 21000
10	        | 24000	           | 46000	            | 21000
11	        | 21000	           | 42000	            | 18000
12	        | 19000	           | 38000	            | 21000
13	        | 18000	           | 35000	            | 19000
14	        | 17000	           | 33000	            | 19000
15	        | 16000	           | 30000	            | 17000
16	        | 14000	           | 28000	            | 18000


![Multiple Records Multiple Threads Removing](/tests/images/rm_property.png)


**Table 6**. Create record

Count to create | Speed, records/s |
------|--------|
1000  |	3000
2000  |	1250
3000  |	910
4000  |	670
5000  |	510
6000  |	435
7000  |	365
8000  |	315
9000  |	270
10000 |	250

![Creating records](/tests/images/link_perf.png)


**Table 7**. Remove record

Count to remove | Speed, records/s |
------|--------|
1000  |	3750
2000  |	1700
3000  |	975
4000  |	710
5000  |	550
6000  |	435
7000  |	360
8000  |	315
9000  |	272
10000 |	240

![Removing records](/tests/images/unlink_perf.png)


Both creating and removing are closely related to disk I/O (creating of record allocates block on disk and updates index table, removing of record perform loading from disk to check that it doesn't contains any child item (yeah, i would fix it in near future) and it safe to delete it). Table 6 & 7 show results from machine with AMD FX-8150 CPU and  Intel® SSD 520 Series.
