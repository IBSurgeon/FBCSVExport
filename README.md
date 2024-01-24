# Firebird multi-threaded export example

This utility was created as an example for the article "Parallel reading of data in the Firebird DBMS".

In English:

* [Parallel_reading_in_Firebird_en.pdf](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_en.pdf)
* [Parallel_reading_in_Firebird_ru.html](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_en-html.zip)

In Russian:

* [Parallel_reading_in_Firebird_ru.pdf](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_ru.pdf)
* [Parallel_reading_in_Firebird_ru.html](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_ru-html.zip)

The `FBCSVExport` utility is 100% free and open source, licensed under [IDPL](https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/).

The binary file for Windows can be downloaded from the link [CSVExport.exe](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/CSVExport.exe)

For Linux operating systems, you can build the utility from the source files:

```
git clone https://github.com/IBSurgeon/FBCSVExport.git
cd FBCSVExport
mkdir build; cd build
cmake ../projects/CSVExport
make
```

## Description of the CSVExport utility

The `CSVExport` utility is designed to export data from Firebird database tables to CSV format.

Each table is exported to a file named `<tablename>.csv`. In normal (single-threaded) mode, 
data from tables is exported sequentially in alphabetical order of table names.

In parallel mode, tables are exported in parallel, each table in a separate thread. 
If the table is very large, then it is split into parts, and each part is exported in a separate stream. 
For each part of a large table, a separate file is created with the name `<tablename>.csv.partN`, 
where N is the part number. When all parts of a large table are exported, the part files are merged 
into a common file called `<tablename>.csv`.

A regular expression is used to specify which tables will be exported. Only regular tables can be 
exported (system tables, GTT, views, external tables are not supported). Regular expressions must be in SQL syntax, 
that is, those that are used in the `SIMILAR TO` predicate.

The following describes the command line switches of the `CSVExport` utility.

For help with command line options, type:

```
CSVExport -h
```

As a result, the following help will be displayed:

```
Usage CSVExport [out_dir] <options>
General options:
    -h [ --help ]                        Show help
    -o [ --output-dir ] path             Output directory
    -H [ --print-header ]                Print CSV header, default false
    -f [ --table-filter ]                Table filter
    -S [ --column-separator ]            Column separator, default ",". Supported: ",", ";" and "t".
                                         Where "t" is '\t'.
    -P [ --parallel ]                    Parallel threads, default 1

Database options:
    -d [ --database ] connection_string  Database connection string
    -u [ --username ] user               User name
    -p [ --password ] password           Password
    -c [ --charset ] charset             Character set, default UTF8
    -s [ --sql-dialect ] dialect         SQL dialect, default 3
```

Description of parameters:

* `-h` or `--help` -- help output;
* `-o` or `--output-dit` -- specifies the directory in which CSV files with data from exported tables will be placed;
* `-H` or `--print-header`. If this switch is specified, then the first line in the CSV files will be the names of the fields of the exported table;
* `-f` or `--table-filter` -- specifies a regular expression used to select tables for export;
* `-S` or `--column-separator` -- column value separator in CSV. The default is comma ",".
  The following delimiters are supported: comma ",", semicolon ";" or the letter "t".
  Here the letter "t" encodes a tab, that is, the `\t` character;
* `-P` or `--parallel` -- sets the number of threads that will be used during export;
* `-d` or `--database` -- database connection string;
* `-u` or `--username` -- username for connecting to the database;
* `-p` or `--password` -- password for connecting to the database;
* `-c` or `--charset` -- database connection character set. Default is UTF-8;
* `-s` or `--sql-dialect` -- SQL dialect. Default is 3. Valid values are 1 and 3.

## Performance Test Results

First, let's look at the results of comparing multi-threaded and single-threaded export modes on my home computer, which is not the most modern.

### Windows

* Operating system: Windows 10 x64.
* CPU: Intel Core i3 8100, 4 cores, 4 threads.
* RAM: 16 Гб
* Disk subsystem: NVME SSD (database), SATA SSD (directory for placing CSV files).
* Firebird 4.0.4 x64

Results:

```
CSVExport.exe -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=1 \
  -d inet://localhost:3054/horses -u SYSDBA -p masterkey --charset=WIN1251 -o ./

Elapsed time in milliseconds parallel_part: 35894 ms
Elapsed time in milliseconds: 36317 ms

CSVExport.exe -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=4 \
  -d inet://localhost:3054/horses -u SYSDBA -p masterkey --charset=WIN1251 -o ./multi

Elapsed time in milliseconds parallel_part: 19259 ms
Elapsed time in milliseconds: 20760 ms

CSVExport.exe -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=4 \
  -d inet://localhost:3054/horses -u SYSDBA -p masterkey --charset=WIN1251 -o ./multi

Elapsed time in milliseconds parallel_part: 19600 ms
Elapsed time in milliseconds: 21137 ms
```

From the testing result it is clear that when using two threads, the acceleration was 1.8 times, which is a good result. 
But parallel execution of export in 4 threads also gave a 1.8 times speedup. Why not at 3-4? 
The fact is that the Firebird server and the export utility are running on the same computer, which has only 4 cores. 
Thus, the Firebird server itself uses 4 threads to read the table, and the `CSVExport` utility also uses 4 threads. 
Obviously, in this case it is quite difficult to achieve an acceleration of more than 2 times. Therefore, we will 
try on another hardware, where the number of cores is significantly larger.

### Linux

* Operating system: CentOS 8.
* CPU: 2 CPU Intel Xeon E5-2603 v4, total 12 cores, 12 threads.
* RAM: 32 Гб
* Disk subsystem: SAS HDD (RAID 10)
* Firebird 4.0.4 x64

Results:

```
[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=1 \
  -d inet://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./single

Elapsed time in milliseconds parallel_part: 57547 ms
Elapsed time in milliseconds: 57595 ms

[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=4 \
  -d inet://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./multi

Elapsed time in milliseconds parallel_part: 17755 ms
Elapsed time in milliseconds: 18148 ms

[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=6 \
  -d inet://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./multi

Elapsed time in milliseconds parallel_part: 13243 ms
Elapsed time in milliseconds: 13624 ms

[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=12 \
  -d inet://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./multi

Elapsed time in milliseconds parallel_part: 12712 ms
Elapsed time in milliseconds: 13140 ms
```

In this case, the optimal number of threads for export is 6 (6 threads for Firebird and 6 threads for the `CSVExport` utility). 
At the same time, we managed to achieve a 5-fold acceleration, which indicates fairly good scalability.
I would like to note that identical databases of almost the same size were used for testing on Linux and Windows. In one stream, on Windows, 
the export was almost 2 times faster, due to the faster disk subsystem. Still, NVME drives are much faster than SAS drives combined in RAID.

