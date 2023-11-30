# Пример многопоточного экспорта Firebird

Компиляция на Linux

```
git clone https://github.com/IBSurgeon/FBCSVExport.git
cd FBCSVExport
mkdir build; cd build
cmake ../projects/CSVExport
make
```

## Описание утилиты CSVExport

Для получения справки по параметрам командной строки наберите

```
CSVExport -h
```

В результате будет выведена следующая справка:

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

Описание параметров:

* `-h` или `--help` -- вывод справки;
* `-o` или `--output-dit` -- задаёт директорию в которую будут помещены CSV файлы с данными экспортированных таблиц;
* `-H` или `--print-header`. Если указан данный переключатель, то первой строкой в CSV файлах будут имена полей экспортированной таблицы;
* `-f` или `--table-filter` -- задаёт регулярное выражение, по которому выбираются таблицы для экспорта. Возможен экспорт только обычных таблиц 
  (системные таблицы, GTT, представления, внешние таблицы не поддерживаются). Регулярные выражения должны быть в SQL синтаксисе, то есть такие,
  которые используются в предикате `SIMILAR TO`;
* `-S` или `--column-separator` -- разделитель значений столбцов в CSV. По умолчанию запятая ",". 
  Поддерживаются следующие разделители: запятая ",", точка с запятой ";" или буква "t".
  Здесь буква t кодирует табуляцию, то есть символ `\t`;
* `-P` или `--parallel` -- задаёт количество потоков, которое будет использовано при экспорте;
* `-d` или `--database` -- строка соединения с базой данных;
* `-u` или `--username` -- имя пользователя для соединения с базой данных;
* `-p` или `--password` -- пароль для соединения с базой данных;
* `-c` или `--charset` -- набор символов соединения с базой данных. По умолчанию UTF-8; 
* `-s` или `--sql-dialect` -- SQL диалект. По умолчанию 3. Допустимые значения 1 и 3.

## Результаты тестирования

Результаты на Windows 10 x64, NVME SSD, 4 ядра.

```
CSVExport.exe -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=1 -d inet://localhost:3054/horses -u SYSDBA -p masterkey --charset=WIN1251 -o ./single
Elapsed time in milliseconds parallel_part: 35790 ms
Elapsed time in milliseconds: 36066 ms

CSVExport.exe -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=4 -d inet://localhost:3054/horses -u SYSDBA -p masterkey --charset=WIN1251 -o ./multi
Elapsed time in milliseconds parallel_part: 19457 ms
Elapsed time in milliseconds: 20927 ms
```

Результаты на Linux (CentOS 8), SAS HDD (RAID 10), 12 ядер.

```
[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=1 -d i
net://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./single
Elapsed time in milliseconds parallel_part: 57547 ms
Elapsed time in milliseconds: 57595 ms

[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=4 -d i
net://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./multi
Elapsed time in milliseconds parallel_part: 17755 ms
Elapsed time in milliseconds: 18148 ms

[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=6 -d i
net://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./multi
Elapsed time in milliseconds parallel_part: 13243 ms
Elapsed time in milliseconds: 13624 ms

[denis@copyserver build]$ ./CSVExport -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=12 -d
inet://localhost/horses -u SYSDBA -p masterkey --charset=UTF8 -o ./multi
Elapsed time in milliseconds parallel_part: 12712 ms
Elapsed time in milliseconds: 13140 ms
```
