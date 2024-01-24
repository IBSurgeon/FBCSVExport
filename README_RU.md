# Пример многопоточного экспорта Firebird

Данная утилита создана как пример для статьи "Параллельное чтение данных в СУБД Firebird":

На русском языке:

* [Parallel_reading_in_Firebird_ru.pdf](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_ru.pdf)
* [Parallel_reading_in_Firebird_ru.html](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_ru-html.zip)

На английском языке:

* [Parallel_reading_in_Firebird_en.pdf](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_en.pdf)
* [Parallel_reading_in_Firebird_ru.html](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/Parallel_reading_in_Firebird_en-html.zip)

Утилита `FBCSVExport` является 100% бесплатной и с открытым исходным кодом, с лицензией [IDPL](https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/).

Бинарный файл для Windows можно скачать по ссылке [CSVExport.exe](https://github.com/IBSurgeon/FBCSVExport/releases/download/1.0/CSVExport.exe)

Для операционных систем Linux, вы можете собрать утилиту из исходных файлов:

```
git clone https://github.com/IBSurgeon/FBCSVExport.git
cd FBCSVExport
mkdir build; cd build
cmake ../projects/CSVExport
make
```

## Описание утилиты CSVExport

Утилита `CSVExport` предназначена для экспорта данных из таблиц БД Firebird в формат CSV.

Каждая таблица экспортируется в файл с именем `<tablename>.csv`. В обычном (однопоточном режиме)
данные из таблиц экспортируется последовательно в алфавитном порядке имени таблиц.

В параллельном режиме, таблицы экспортируются параллельно, каждая таблица в отдельном потоке. Если
таблица очень большая, то она разбивается на части, и каждая часть экспортируется в отдельном потоке.
Для каждой части большой таблицы создаётся отдельный файл с именем `<tablename>.csv.partN`, где N - номер части.
Когда все части большой таблицы экспортированы, файлы частей сливаются в общий файл с именем `<tablename>.csv`.

Для того, чтобы указать какие именно таблицы будут экспортированы используется регулярное выражение.
Возможен экспорт только обычных таблиц (системные таблицы, GTT, представления, внешние таблицы не поддерживаются).
Регулярные выражения должны быть в SQL синтаксисе, то есть такие, которые используются в предикате `SIMILAR TO`.

Далее описаны переключатели командной строки утилиты `CSVExport`.

Для получения справки по параметрам командной строки наберите:

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
* `-f` или `--table-filter` -- задаёт регулярное выражение, по которому выбираются таблицы для экспорта;
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

Для начала посмотрим на результаты сравнения многопоточного и однопоточного режима экспорта на моём домашнем не самом современном компьютере.

### Windows

* Операционная система: Windows 10 x64.
* Процессор: Intel Core i3 8100, 4 ядра, 4 потока.
* Память: 16 Гб
* Дисковая подсистема: NVME SSD (база данных), SATA SSD (папка для размещения CSV файлов).
* Firebird 4.0.4 x64

Результаты:

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

Из результата тестирования видно, что при использовании двух потоков, ускорении составило 1.8 раза, что является хорошим результатом.
Но параллельное выполнение экспорта в 4 потоках, тоже дало ускорение в 1.8 раза. Почему не в 3-4?
Дело в том, что сервер Firebird и утилита экспорта запущены на одном и том же компьютере, у которого всего 4 ядра.
Таким образом сам сервер Firebird, использует 4 потока для чтения таблицы и утилита `CSVExport`, тоже использует 4 потока.
Очевидно, что в таком случае довольно затруднительно получить ускорение более чем в 2 раза.
Поэтому попробуем на другом железе, где количество ядер существенно больше.

### Linux

* Операционная система: CentOS 8.
* Процессор: 2 процессора Intel Xeon E5-2603 v4, всего 12 ядер, 12 потоков.
* Память: 32 Гб
* Дисковая подсистема: SAS HDD (RAID 10)
* Firebird 4.0.4 x64

Результаты:

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

В данном случае оптимальном числом потоков для экспорта является 6 (6 потоков для Firebird и 6 потоков для утилиты `CSVExport`).
При этом удалось получить ускорение в 5 раз, что говорит о достаточно хорошей масштабируемости. Хотелось бы отметить, что для проверки
на Linux и Windows использовались идентичные базы данных почти одинакового размера. В одном потоке, на Windows экспорт прошёл почти в 2 раза
быстрее, из-за более быстрой дисковой подсистемы. Всё таки NVME диски намного быстрее SAS дисков объединённых в RAID.

