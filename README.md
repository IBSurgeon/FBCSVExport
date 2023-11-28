# Пример многопоточного экспорта Firebird

Компиляция на Linux

```
git clone https://github.com/IBSurgeon/FBCSVExport.git
cd FBCSVExport
mkdir build; cd build
cmake ../projects/CSVExport
make
```

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
