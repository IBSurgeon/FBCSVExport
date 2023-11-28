# Пример многопоточного экспорта Firebird

```
git clone https://github.com/sim1984/FBCSVExport.git
cd FBCSVExport
mkdir build; cd build
cmake ../projects/CSVExport
make
```

```
CSVExport.exe -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=1 -d inet://localhost:3054/horses -u SYSDBA -p masterkey --charset=WIN1251 -o ./single

CSVExport.exe -H --table-filter="COLOR|BREED|HORSE|COVER|MEASURE|LAB_LINE|SEX" --parallel=4 -d inet://localhost:3054/horses -u SYSDBA -p masterkey --charset=WIN1251 -o ./multi
```

