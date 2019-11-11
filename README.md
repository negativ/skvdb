[![Build Status](https://travis-ci.com/negativ/skvdb.svg?branch=master)](https://travis-ci.com/negativ/skvdb)
[![Build status](https://ci.appveyor.com/api/projects/status/github/negativ/skvdb?branch=master&svg=true)](https://ci.appveyor.com/project/negativ/skvdb)
[![License](https://img.shields.io/github/license/negativ/skvdb?style=round-square)](https://github.com/negativ/skvdb/blob/master/LICENSE)

# SKVDB
Embedded simple key-value database library written in C++17.

# How to build

On Ubuntu-like distro install C++17-compatible compiler (like G++ or Clang), CMake, Google Test and Boost libraries with command like:

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

The simplest way to build library on Windows is to install Microsoft Visual Studio 2019 with C++ support, open SKVDB project with "Open folder" feature and update CMakeSettings.json file with actual values of BOOST_INCLUDEDIR and BOOST_LIBRARYDIR variables. If you dont have boost library installed on your system, you can download installer with pre-built library from [here](https://sourceforge.net/projects/boost/files/boost-binaries/1.71.0/) .

# Storage format

Each volume consists of two files: Index Table (.index) and Log Device (.logd). 
Log Device format is very simple: just an array of blocks with fixed size (1024/2048/4096/etc., 2048 bytes by default).
Index Table file contains array of Index Records (1 root index record with id #1 at least).

Index Record format is following:

Field       | Type               | Description
------------|--------------------|------------
Key         | 64-bit integer, LE | Id of index record
Block index | 32-bit integer, LE | Index of record on Log Device
Bytes count | 32-bit integer, LE | Length of record in bytes

So every record in DB is described by Index Record. Each time you updating some record, it content would be written to the end of Log Device, there is no way to overwrite old blocks - Log Device working only for reading and appending of new data.


Each record stored in Log Device in following form:

Field                       | Type               | Description
----------------------------|--------------------|------------
Key                         | 64-bit integer, LE | Id of record (eq. to id of corresponding index record)
Parent                      | 64-bit integer, LE | Index of record on Log Device
Name length                 | 64-bit integer, LE | 
Name                        | String             | 
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

SKVDB was benchmarked on Ubuntu 19.04 (CPU: AMD FX(tm)-8150 Eight-Core Processor, RAM: 16 GB DDR3, Intel® SSD 520 Series). 

All entry property names was up to 12 bytes length long, property types: uint8_t, uint32_t, uint64_t, float, double, string (256 bytes), BLOB (1024 bytes) in equal proportions.

Each benchmark was run on one mount point of virtual storage with 1, 2 and 3 disk volume entries mounted to it.


**Table 1**. One record, multiple threads writing properties to it. Properties/sec. per-thread

No. threads | 3 entries | 2 entries| 1 entry|
---|--------|-------|-------
2  |	71000 |	88000 |	385000
3  |	58000 |	76000 |	268000
4  |	45000 |	62000 |	155000
5  |	40000 |	50000 |	130000
6  |	37000 |	42000 |	110000
7  |	33000 |	38000 |	91000
8  |	32000 |	40000 |	80000
9  |	32000 |	37000 |	70000
10 |	26000 |	30000 |	62000
11 |	24000 |	32000 |	55000
12 |	24000 |	30000 |	52000
13 |	20000 |	26000 |	47000
14 |	18000 |	20000 |	42000
15 |	15000 |	23000 |	38000
16 |	14000 |	20000 |	32000

![One Record Multiple Threads Writing](/tests/images/one_record_multiple_threads_set_prop.png)


**Table 2**. One record, multiple threads reading properties from it. Properties/sec. per-thread

No. threads | 3 entries | 2 entries| 1 entry|
---|--------|-------|-------
2  |	71000	| 86000	| 430000
3  |	57000	| 78000	| 225000
4	 |  45000	| 64000	| 144000
5	 |  41000	| 55000	| 125000
6	 |  37000	| 49000	| 101000
7	 |  35000	| 42000	| 90000
8	 |  34000	| 41000	| 80000
9	 |  31000	| 39000	| 71000
10 |	28000	| 37000	| 63000
11 |	28000	| 35000	| 58000
12 |	26000	| 33000	| 53000
13 |	24000	| 31000	| 49000
14 |	23000	| 30000	| 45000
15 |	22000	| 28000	| 42000
16 |	20000	| 26000	| 40000

![One Record Multiple Threads Reading](/tests/images/one_record_multiple_threads_get_prop.png)


**Table 3**. Mutiple records, multiple threads writing properties to them (each thread to owned record). Properties/sec. per-thread

No. threads | 3 entries | 2 entries| 1 entry|
---|--------|-------|-------
2  |	67000	| 87000	| 526000
3  |	56000	| 77000	| 213000
4  |	50000	| 60000	| 136000
5  |	40000	| 52000	| 123000
6  |	42000	| 54000	| 102000
7  |	36000	| 43000	| 91000
8  |	32000	| 44000	| 83000
9  |	32000	| 39000	| 73000
10 |	29000	| 39000	| 68000
11 |	28000	| 35000	| 55000
12 |	27000	| 34000	| 56000
13 |	25000	| 32000	| 51000
14 |	23000	| 31000	| 48000
15 |	22000	| 28000	| 45000
16 |	20000	| 27000	| 42000

![Multiple Records Multiple Threads Writing](/tests/images/multiple_records_multiple_threads_set_prop.png)


**Table 4**. Mutiple records, multiple threads reading properties from them (each thread from owned record). Properties/sec. per-thread

No. threads | 3 entries | 2 entries| 1 entry|
---|--------|-------|-------
2  |	73000	|	83000	|	500000
3  |	56000	|	75000	|	213000
4  |	46000	|	60000	|	138000
5  |	38000	|	51000	|	123000
6  |	39000	|	49000	|	106000
7  |	34000	|	43000	|	94000
8  |	33000	|	39000	|	85000
9  |	31000	|	40000	|	75000
10 |	30000	|	36000	|	69000
11 |	27000	|	34000	|	62000
12 |	25000	|	33000	|	57000
13 |	24000	|	31000	|	52000
14 |	23000	|	30000	|	49000
15 |	21000	|	28000	|	45000
16 |	20000	|	26000	|	43000

![Multiple Records Multiple Threads Reading](/tests/images/multiple_records_multiple_threads_get_prop.png)


**Table 5**. Mutiple records, multiple threads removing properties from them (each thread from owned record). Properties/sec. per-thread

No. threads | 3 entries | 2 entries| 1 entry|
---|--------|-------|-------
2  |	52000 |	61000 |	193000
3  |	40000 |	53000 |	122000
4  |	33000 |	44000 |	93000
5  |	31000 |	39000 |	75000
6  |	28000 |	35000 |	74000
7  |	25000 |	32000 |	60000
8  |	22000 |	29000 |	53000
9  |	20000 |	26000 |	44000
10 |	18000 |	24000 |	41000
11 |	17000 |	23000 |	37000
12 |	15000 |	22000 |	33000
13 |	14000 |	19000 |	31000
14 |	13000 |	17000 |	30000
15 |	12000 |	17000 |	26000
16 |	11000 |	15000 |	25000

![Multiple Records Multiple Threads Removing](/tests/images/rm_property.png)


**Table 6**. Create record

Count to create | Speed, records/s |
------|--------|
1000  |	3000
2000  |	1250
3000  |	910
4000  |	670
5000  |	510
6000 ||	435
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
6000 ||	435
7000  |	360
8000  |	315
9000  |	272
10000 |	240

![Removing records](/tests/images/unlink_perf.png)


Both creating and removing are closely related to disk I/O (creating of record allocates block on disk and updates index table, removing of record perform loading from disk to check that it doesn't contains any child item (yeah, i would fix it in near future) and it safe to delete it). 
