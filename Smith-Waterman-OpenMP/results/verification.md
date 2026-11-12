# Biên bản xác minh tuần 1

Ngày chạy: **12/11/2026**, Windows, múi giờ Asia/Bangkok. Thư mục project:
`D:\BTL HPC\Smith-Waterman-OpenMP`. Các kết quả bên dưới được lấy từ chạy thực tế,
không phải số đo benchmark.

## Công cụ

```text
g++ (Rev6, Built by MSYS2 project) 16.2.0
cmake 4.4.5
ninja 1.13.2
git 2.50.1.windows.1
CMake OpenMP_CXX: -fopenmp (version 5.2)
```

## Lệnh đã chạy

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -fopenmp tests/test_openmp.cpp -o build-probe/test_openmp.exe
.\build-probe\test_openmp.exe

cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Cả Debug và Release: **100% tests passed, 0 tests failed out of 5**.
Log chi tiết nguyên bản từ `Testing/Temporary/LastTest.log`:
[ctest_debug.txt](ctest_debug.txt), [ctest_release.txt](ctest_release.txt).

| Nhóm | Kết quả nội bộ |
|---|---|
| Smith-Waterman | Passed: 17, Failed: 0; gồm 7.225 cặp oracle độc lập, 250 cặp seed, ca 1.200 base |
| FASTA reader | Passed: 12, Failed: 0 |
| DNA generator | Passed: 10, Failed: 0 |
| CLI | Passed: 22 CLI cases |
| OpenMP | Requested threads: 4; Actual threads: 4; ID 0–3; Processors: 12; macro 202111 |

Log OpenMP chạy trực tiếp sau build Release: [openmp.txt](openmp.txt).
Thứ tự ID thay đổi được; tiêu chí là đủ đúng bốn ID và kích thước team.

## Demo căn chỉnh

```powershell
.\build-debug\sw_cli.exe align --seq1 ACGT --seq2 ACGT --alignment
.\build-debug\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --alignment-id ref_gap
```

Lệnh đầu: `Score: 8`, đoạn A và B đều `[0,4)`, căn chỉnh `ACGT/ACGT`.
Lệnh sau: `ref_exact=4`, `ref_partial=4`, `ref_none=0`, `ref_gap=14`;
traceback A `[0,8)`, B `[0,9)`:

```text
AAAA-CCCC
|||| ||||
AAAAGCCCC
```

## Dữ liệu sinh thực tế

```powershell
.\build-debug\sw_cli.exe generate --count 3 --length 32 --seed 42 --output data/generated/small_seed42.fasta
.\build-debug\sw_cli.exe generate --count 4 --length 512 --seed 2026 --output data/generated/medium_seed2026.fasta
.\build-release\sw_cli.exe search --query data/generated/medium_seed2026.fasta --query-id seq_1 --references data/generated/medium_seed2026.fasta
```

Hai lệnh sinh phía trên đã chạy và tạo file bàn giao; chạy lại phải chọn đường
dẫn mới vì CLI từ chối ghi đè. Kích thước tương ứng 144 byte và 2.144 byte.
Log tìm kiếm dữ liệu vừa: [medium_search.txt](medium_search.txt), điểm 1024/227/221/223.

SHA-256 từ `Get-FileHash -Algorithm SHA256`:

```text
small_seed42.fasta
1CE0A2E943898F3FF4DC6AAB3A495EB3C65CF8B27BDC689DF3564126BBDC0599

medium_seed2026.fasta
E9ADD3FA5A69278326A81EE6C6DF104E7F43DB1E43DF0A0808AB32408369BD1B
```

Không có số liệu speedup/efficiency trong tuần 1. Các lần chạy trên chỉ xác minh
chức năng serial, khả năng biên dịch và hoạt động runtime OpenMP.
