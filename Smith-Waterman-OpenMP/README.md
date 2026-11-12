# Smith-Waterman OpenMP — Tuần 1–2

Đề tài 6, môn **Tính toán hiệu năng cao**: khớp mẫu chuỗi gene sinh học quy mô lớn.
Tuần 1 xây dựng nền tảng C++17: Smith-Waterman tuần tự, đọc FASTA, sinh DNA,
CLI và kiểm tra runtime OpenMP. Tuần 2 bổ sung **Parallel Batch Smith-Waterman**:
một query được so sánh với nhiều reference độc lập bằng OpenMP, hỗ trợ serial,
parallel, static/dynamic scheduling và cấu hình số luồng. Chương trình tìm **căn chỉnh cục bộ** tốt nhất,
không phải căn chỉnh toàn bộ hai chuỗi. Điểm mặc định: match **+2**, mismatch **−1**,
mỗi ký tự gap **−2** (linear gap, không dùng affine gap).

## Công nghệ và môi trường đã xác minh

- Windows, PowerShell trong Terminal của VS Code.
- GCC/G++ 16.2.0 của MSYS2 UCRT64, C++17; CMake 4.4.5, Ninja 1.13.2.
- OpenMP được CMake nhận diện là 5.2, cờ `-fopenmp`, macro `_OPENMP=202111`.
- Git 2.50.1.windows.1; không dùng thư viện ngoài, không cần Google Test.

Các phiên bản trên là kết quả kiểm tra máy ngày **12/11/2026**; không phải yêu cầu
phiên bản tối thiểu. Project yêu cầu compiler hỗ trợ C++17, CMake ≥ 3.16 và OpenMP.

## Cấu trúc và danh sách file bàn giao

```text
Smith-Waterman-OpenMP/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── .vscode/
│   ├── c_cpp_properties.json
│   └── settings.json
├── include/
│   ├── smith_waterman.h
│   ├── smith_waterman_parallel.h
│   ├── fasta_reader.h
│   └── dna_generator.h
├── src/
│   ├── main.cpp
│   ├── smith_waterman_serial.cpp
│   ├── smith_waterman_parallel.cpp
│   ├── fasta_reader.cpp
│   └── dna_generator.cpp
├── tests/
│   ├── test_support.h
│   ├── test_smith_waterman.cpp
│   ├── test_fasta_reader.cpp
│   ├── test_dna_generator.cpp
│   ├── test_cli.cmake
│   ├── test_parallel.cpp
│   ├── test_parallel_cli.cmake
│   └── test_openmp.cpp
├── data/
│   ├── samples/
│   │   ├── queries.fasta
│   │   └── references.fasta
│   └── generated/
│       ├── small_seed42.fasta
│       └── medium_seed2026.fasta
├── docs/
│   ├── week1_report.md
│   ├── week1_fix_report.md
│   └── week2_report.md
└── results/
    ├── verification.md
    ├── ctest_debug.txt
    ├── ctest_release.txt
    ├── medium_search.txt
    ├── openmp.txt
    ├── week1_acceptance_20261112.md
    ├── week1_fix_20261112/     # Log trước/sau khắc phục và xác minh
    ├── week1_audit_20261112/   # Bằng chứng nghiệm thu đã tách khỏi build cũ
    └── week2/                 # Baseline, build/test và demo tuần 2
```

File bổ sung so với đề bài: `test_dna_generator.cpp` kiểm tra sinh/xuất dữ liệu;
`test_cli.cmake` kiểm tra executable thực tế và mã thoát; `test_openmp.cpp` tách
kiểm tra OpenMP; `test_support.h` chia sẻ bộ kiểm tra và file tạm, hoạt động cả
Release (không phụ thuộc `assert`). Các file FASTA là dữ liệu demo;
`results/*` giữ bằng chứng chạy thực tế. `.vscode/` cấu hình IntelliSense và
CMake Tools dùng đúng GCC UCRT64; thư mục cha có cấu hình tương ứng cho trường hợp
mở toàn bộ workspace `BTL HPC`. `build-debug/` và `build-release/` là sản phẩm
biên dịch cục bộ đang sử dụng, được Git bỏ qua. Các thư mục build kiểm tra cũ
đã được dọn; log nghiệm thu được giữ trong `results/week1_audit_20261112/`.
README và lịch sử Git có sẵn tại thư mục cha được giữ nguyên.

Tuần 2 thêm `smith_waterman_parallel.h/.cpp` để tách batch execution khỏi kernel
serial, `test_parallel.cpp` kiểm tra API và `test_parallel_cli.cmake` kiểm tra
cú pháp/chế độ CLI. Target `sw_parallel` liên kết `sw_core` và OpenMP; `sw_core`
vẫn không phụ thuộc OpenMP. Không có thư viện bên ngoài hoặc hệ thống benchmark mới.

## Chuẩn bị môi trường

Máy hiện tại đã có đủ công cụ, không cần cài thêm. Nếu chuẩn bị máy khác,
cài VS Code và MSYS2 theo quy trình của máy đó. Trong **MSYS2 UCRT64**, có thể
cài bộ công cụ bằng lệnh sau (người dùng tự thực hiện khi cần):

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja git
```

Trong **PowerShell**, bổ sung PATH cho phiên Terminal hiện tại nếu cần; lệnh này
không thay đổi cấu hình hệ thống:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
g++ --version
cmake --version
ninja --version
git --version
```

## Build và kiểm thử trong VS Code Windows

Mở thư mục project bằng VS Code, mở Terminal → PowerShell. Từ workspace hiện tại:

```powershell
Set-Location 'D:\BTL HPC\Smith-Waterman-OpenMP'
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Debug thuận tiện để gỡ lỗi; Release bật tối ưu của CMake cho GCC. Mỗi cấu hình
có thư mục riêng. Nếu dùng `MinGW Makefiles`, thay `-G Ninja` bằng
`-G "MinGW Makefiles"` trong một thư mục build mới. Dừng và xử lý lỗi nếu một
lệnh build trả mã khác 0, không chạy executable cũ để nghiệm thu.

### Nếu không có CMake

Vẫn build trực tiếp bằng G++ từ thư mục project, không cần cài thêm thư viện:

```powershell
New-Item -ItemType Directory -Force build-manual | Out-Null
$sources = @('src/smith_waterman_serial.cpp', 'src/fasta_reader.cpp', 'src/dna_generator.cpp')
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fopenmp -Iinclude @sources src/smith_waterman_parallel.cpp src/main.cpp -o build-manual/sw_cli.exe
foreach ($module in 'smith_waterman','fasta_reader','dna_generator') {
    g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude @sources "tests/test_$module.cpp" -o "build-manual/test_$module.exe"
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $module" }
    & ".\build-manual\test_$module.exe"
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $module" }
}
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fopenmp tests/test_openmp.cpp -o build-manual/test_openmp.exe
.\build-manual\test_openmp.exe
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fopenmp -Iinclude @sources src/smith_waterman_parallel.cpp tests/test_parallel.cpp -o build-manual/test_parallel.exe
.\build-manual\test_parallel.exe
```

Các lệnh dự phòng này tạo bốn bộ unit test và kiểm tra OpenMP; hai bộ tích hợp
CLI `.cmake` cần CMake. CLI có thể được demo thủ công bằng các lệnh bên dưới.

## Demo hai chuỗi và truy vết

```powershell
.\build-release\sw_cli.exe --help
.\build-release\sw_cli.exe align --seq1 ACGT --seq2 ACGT
.\build-release\sw_cli.exe align --seq1 AAAACCCC --seq2 AAAAGCCCC --alignment
.\build-release\sw_cli.exe align --seq1 AAAA --seq2 TTTT --alignment
```

Kết quả lần lượt có điểm **8**, **14**, **0**. `--alignment` in một đường truy vết:

```text
Score: 14
A [0, 8)
B [0, 9)
AAAA-CCCC
|||| ||||
AAAAGCCCC
```

`|` là match, `.` là mismatch, khoảng trắng là gap. Tọa độ bắt đầu từ 0 và
loại trừ đầu cuối: `[start, end)`, tính trên chuỗi gốc không có dấu `-`.
Không có match dương: điểm 0, căn chỉnh rỗng, các tọa độ đều 0.
API C++ chấp nhận cả chuỗi rỗng; ví dụ `smith_waterman_score("", "ACGT") == 0`.
PowerShell cũ có thể không chuyển đối số rỗng `""` cho executable native;
trường hợp rỗng được xác minh trực tiếp bằng unit test C++.

## Đọc FASTA và chọn query

```powershell
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_1 --references data/samples/references.fasta
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --alignment-id ref_gap
```

`--query-id` là ID ngay sau `>`, trước khoảng trắng đầu tiên; phần còn lại là
mô tả. Theo mặc định query được so sánh **tuần tự** với mọi reference theo thứ tự trong file.
`--alignment-id` chỉ truy vết reference được chọn; còn lại dùng score-only.
Đường dẫn chứa khoảng trắng phải đặt trong dấu nháy.

Quy tắc FASTA: nhiều bản ghi, nhiều dòng/sequence, chữ thường được chuyển hoa,
hỗ trợ LF/CRLF và dòng trống (kể cả dòng chỉ có whitespace). Các dòng sequence
có nội dung chỉ được chứa A/C/G/T; khoảng trắng trong/đầu/cuối dòng không được
tự xóa. N, U, các mã IUPAC khác, dấu gap, dòng comment và BOM không được hỗ trợ.
Header phải bắt đầu bằng `>` ở cột đầu tiên, ID không rỗng và không trùng trong
cùng file, mỗi bản ghi phải có sequence không rỗng. ID phân biệt hoa/thường.
Những quy tắc bổ sung về ID và sequence giúp chọn query/reference không mơ hồ.
Lỗi được báo kèm tên nguồn, số dòng và vị trí ký tự sai khi thích hợp.

## Search serial và OpenMP parallel — Tuần 2

```powershell
# Cú pháp tuần 1 giữ nguyên; mặc định vẫn serial và không in metadata.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta

# Serial rõ ràng, ghi metadata vào stderr.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --mode serial --threads 1 --stats

# Parallel 4 luồng; thử thêm --schedule static hoặc --threads 1/2/8 khi khả dụng.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --mode parallel --threads 4 --schedule dynamic --stats

# Batch score song song, traceback của đúng một cặp vẫn dùng API serial.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --mode parallel --threads 4 --alignment-id ref_gap
```

Serial và parallel đều cho các điểm **4, 4, 0, 14** theo thứ tự `ref_exact`,
`ref_partial`, `ref_none`, `ref_gap`. Stdout giữ cùng định dạng và thứ tự FASTA;
các luồng không in trực tiếp. `--stats` ghi vào stderr, ví dụ:

```text
Mode: parallel
Schedule: dynamic
Requested threads: 4
Actual threads: 4
Compute seconds: ...
```

| Option | Mặc định / quy tắc |
|---|---|
| `--mode serial\|parallel` | `serial` để giữ tương thích tuần 1 |
| `--threads N` | Serial chỉ nhận 1; parallel mặc định `min(4, max_batch_threads())` |
| `--schedule static\|dynamic` | `dynamic`; chỉ phân chia công việc khi chạy parallel |
| `--stats` | Không bật mặc định; báo mode, schedule cấu hình, team yêu cầu/thực tế, thời gian compute |

N phải dương và không vượt `min(omp_get_num_procs(), omp_get_thread_limit())`.
Đây là chính sách giới hạn request của mỗi batch theo môi trường; không tự yêu
cầu team vượt giới hạn đó. `--help` in giới hạn hiện tại. Runtime có thể cấp ít
luồng hơn vì dynamic teams hoặc nested parallelism, nên luôn ghi nhận **actual**.
Actual là kích thước team được tạo, kể cả thành viên không nhận việc nếu reference
ít. API trả actual 0 và thời gian 0 cho batch rỗng; FASTA CLI vẫn từ chối file rỗng.

Static chia công việc theo chỉ số thành các phần cố định, ít chi phí điều phối;
hữu ích khi độ dài reference tương tự. Dynamic cấp từng reference khi worker
rảnh (`chunk=1`), thường giúp cân bằng việc dài/ngắn nhưng có thêm chi phí runtime.
Không kết luận lịch nào nhanh hơn nếu chưa đo trên dataset thực tế.

Từng cặp query/reference gọi lại kernel serial và sở hữu hai hàng DP riêng.
Mảng score/error cấp phát trước; iteration i chỉ ghi phần tử i. Query/reference
và scoring chỉ đọc, không push_back, không đọc file hoặc ghi terminal trong
worker. Dữ liệu/config được kiểm tra trước vùng OpenMP; ngoại lệ worker được
bắt tại chỗ và ném lại trên luồng gọi sau khi join.

API có thể dùng lại để đo tuần 3 mà không tính đọc FASTA vào compute:

```cpp
sw::BatchOptions options{sw::ExecutionMode::Parallel, 4, sw::Schedule::Dynamic};
auto batch = sw::smith_waterman_batch(query.sequence, references,
                                    sw::ScoringConfig{2, -1, -2}, options);
// batch.scores[i] ứng với references[i]; metadata có actual_threads/compute_seconds.
```

API scoring yêu cầu match > 0, mismatch <= 0, gap < 0, có kiểm tra giới hạn điểm
kiểu int. Overload score-only mới nhận scoring; API hai đối số, build matrix và
alignment tuần 1 vẫn giữ điểm mặc định +2/−1/−2. CLI chưa có cờ chỉnh scoring.
Không thay đổi/mutate input từ thread khác trong thời gian lời gọi đang chạy.

`compute_seconds` dùng `steady_clock`, bắt đầu sau kiểm tra batch/cấp phát mảng
đầu ra và kết thúc sau join. Bao gồm kernel (kiểm tra nội bộ, cấp phát hàng DP),
fork/join và scheduling; không gồm file I/O, định dạng output, traceback hoặc
vòng duyệt lỗi sau join. Đây là chuẩn bị đo compute, chưa phải benchmark tuần 3.

## Sinh dữ liệu DNA có thể tái lập

```powershell
# Dùng tên file mới: chương trình từ chối ghi đè file có sẵn.
.\build-release\sw_cli.exe generate --count 3 --length 32 --seed 42 --output data/generated/my_small.fasta
.\build-release\sw_cli.exe generate --count 4 --length 512 --seed 2026 --output data/generated/my_medium.fasta
.\build-release\sw_cli.exe search --query data/generated/medium_seed2026.fasta --query-id seq_1 --references data/generated/medium_seed2026.fasta
```

`count`, `length` phải dương; seed nằm trong `[0, 4294967295]`. Bộ sinh dùng
`std::mt19937(seed)` và ánh xạ `engine() % 4` sang A/C/G/T; không dùng ánh xạ
phụ thuộc implementation của `uniform_int_distribution`. Cùng tham số cho cùng
record và byte FASTA (xuống dòng LF, tối đa 80 base/dòng). Thư mục cha phải tồn tại.
Bộ sinh giữ toàn bộ record trong RAM; chọn dữ liệu vừa phải khi demo.
Hai file nhỏ/vừa đã bàn giao có tổng 2.144 base, không phải dữ liệu quy mô lớn.

## Test và OpenMP

```powershell
ctest --test-dir build-release --output-on-failure
# Xem chi tiết một bộ test:
.\build-release\test_smith_waterman.exe
.\build-release\test_fasta_reader.exe
.\build-release\test_dna_generator.exe
.\build-release\test_parallel.exe
.\build-release\test_openmp.exe
```

Kết quả tuần 2 đã chạy: **7/7 nhóm CTest đạt ở cả Debug và Release**; 17 nhóm
Smith-Waterman, 12 FASTA, 10 generator, 23 batch parallel, 36 tình huống CLI cũ,
35 tình huống CLI mới và 1 OpenMP runtime — **134 PASS / 0 FAIL** mỗi cấu hình
trên máy này. Số ca CLI mới tùy số mức luồng khả dụng. Bộ Smith-Waterman gồm
7 ca bắt buộc, gap hai chiều, mismatch, tie, ma trận mẫu, kiểm tra lỗi, chữ thường,
7.225 cặp được đối chiếu oracle độc lập, 250 cặp sinh theo seed và ca đồng nhất
1.200 base (điểm 2.400). Mỗi đường truy vết được tính lại điểm và đối chiếu
substring/tọa độ gốc. Kiểm tra không bị vô hiệu hóa trong Release.

OpenMP thực tế: máy báo 12 processor, yêu cầu 4 luồng, nhận đủ **4 luồng**,
ID **0, 1, 2, 3**. Thứ tự in ID không cố định. Test dùng `#pragma omp parallel`,
tắt dynamic teams và yêu cầu `min(4, omp_get_num_procs())`; nếu runtime cấp ít
hơn yêu cầu (ví dụ `OMP_THREAD_LIMIT`), test trả lỗi thay vì báo đạt.
Batch được kiểm tra với **1, 2, 4, 8, 12** luồng, cả static/dynamic, lặp 20 lần
mỗi lịch, ít/nhiều reference hơn team, dữ liệu nhỏ/vừa và độ dài không đều,
scoring tùy chỉnh, input lỗi và hai caller độc lập đồng thời. CLI mới đối chiếu
stdout với serial, kiểm tra actual threads, traceback, dữ liệu vừa và thread limit.
Xem [báo cáo tuần 2](docs/week2_report.md) và [log Debug](results/week2/ctest_debug.txt)/
[Release](results/week2/ctest_release.txt). Tài liệu lịch sử tuần 1:
[bằng chứng kiểm tra](results/verification.md), [báo cáo tuần 1](docs/week1_report.md).

Sau nghiệm thu đã sửa lỗi in dở `Score: ` khi DNA sai. Mỗi tình huống CLI kiểm
tra riêng stdout, stderr và exit code; lỗi đầu vào phải có stdout rỗng, stderr
báo lỗi và exit code 1. Có regression cho DNA hợp lệ, N/ký tự đặc biệt, chuỗi
rỗng, thiếu tham số và FASTA không hợp lệ. Xem [báo cáo khắc phục tuần 1](docs/week1_fix_report.md)
để đọc kết quả mới và hướng dẫn checkpoint Git. Các biên bản nghiệm thu cũ giữ
nguyên kết quả trước sửa.

## Thuật toán, kiểm tra tay và độ phức tạp

Với `a` dài m và `b` dài n, hàng/cột đầu là 0:

```text
H(i,j) = max(0,
             H(i-1,j-1) + (a[i-1] == b[j-1] ? 2 : -1),
             H(i-1,j) - 2,
             H(i,j-1) - 2)
```

Giá trị lớn nhất trong ma trận là điểm tối ưu cục bộ. Traceback bắt đầu tại ô đó,
đi ngược theo recurrence và dừng ở ô 0. Hòa điểm: chọn ô cực đại đầu tiên khi
quét theo hàng; trong traceback ưu tiên **chéo → trên → trái**.

Ví dụ tính tay `AC` và `AG`:

| H | ∅ | A | G |
|---|---:|---:|---:|
| ∅ | 0 | 0 | 0 |
| A | 0 | 2 | 0 |
| C | 0 | 0 | 1 |

`H(1,1)=2`; `H(2,2)=max(0,2−1,0−2,0−2)=1`. Cực đại là 2, truy vết
cho `A/A`. Với `ACGT/ACGT`, đường chéo là 2,4,6,8: bốn match × 2 = **8**.
Với ví dụ gap phía trên: tám match × 2 − một gap × 2 = **14**.

| Chế độ | Thời gian DP | Bộ nhớ phụ | Kết quả |
|---|---|---|---|
| Score-only | O(mn) | O(min(m,n)) | Điểm tối ưu |
| Alignment | O(mn) | O((m+1)(n+1)) | Điểm, tọa độ và hai chuỗi căn chỉnh |

Kiểm tra DNA tốn thêm O(m+n); traceback tốn O(m+n) trong trường hợp xấu nhất.
Với đầu vào không rỗng, tổng thời gian vẫn O(mn). Khi có chuỗi rỗng, thời gian
kiểm tra đầu vào là O(m+n). Score-only trả về sớm sau khi kiểm tra cả hai chuỗi;
alignment vẫn giữ hàng/cột biên. Thư viện không giữ trạng thái toàn cục có thể thay
đổi, chuẩn bị cho việc gọi độc lập ở nhiều luồng trong tuần 2.

## Lỗi thường gặp và giới hạn

- File C++ bị gạch đỏ nhưng build đạt: kiểm tra cấu hình IntelliSense **MSYS2
  UCRT64**. CMake xuất `build-debug/compile_commands.json` để editor nhận đúng
  thư mục `include`, C++17 và `-fopenmp` của từng file. Cấu hình `.vscode/` hỗ trợ
  cả khi mở thư mục cha lẫn mở riêng project. Nếu còn chẩn đoán cũ, mở Command
  Palette (`Ctrl+Shift+P`), chạy `C/C++: Reset IntelliSense Database`, rồi
  `Developer: Reload Window`. Đọc nội dung cụ thể ở Problems (`Ctrl+Shift+M`)
  nếu dấu đỏ vẫn còn; dấu đỏ trong editor và kết quả chạy test là hai nguồn khác nhau.
- Không tìm thấy `g++`, CMake, Ninja hoặc `libgomp-1.dll`: kiểm tra PATH UCRT64
  trong phiên Terminal; mở Terminal mới nếu vừa cấu hình môi trường. Không trộn
  compiler UCRT64 với DLL/toolchain MINGW64 khác.
- CMake thiếu OpenMP: kiểm tra `g++ -fopenmp`, đúng UCRT64 và `libgomp-1.dll`.
  CMake yêu cầu OpenMP để không bỏ qua hạng mục kiểm tra; vẫn có thể build
  phần serial bằng các lệnh G++ dự phòng.
- CMake báo generator/compiler khác cache: dùng một thư mục build mới.
- Sai đường dẫn FASTA: kiểm tra thư mục làm việc và dùng dấu nháy khi có khoảng trắng.
- DNA chứa `N`/ký tự lạ hoặc FASTA rỗng/trùng ID: sửa đầu vào theo thông báo,
  chương trình không tự bỏ qua dữ liệu lỗi.
- `Refusing to overwrite`: chọn tên đầu ra mới; thư mục cha phải tồn tại.
- Thiếu RAM khi truy vết: bỏ `--alignment`, bỏ `--alignment-id`, hoặc giảm độ dài.
  Kiểm tra tràn kích thước/điểm và ngoại lệ cấp phát đã có; chưa có giới hạn RAM cấu hình.
- Mã thoát CLI: `0` thành công, `1` lỗi tham số, dữ liệu, I/O hoặc tài nguyên.

Tuần 2 đã phân phối các **cặp query/reference độc lập** cho các luồng. Mỗi ma trận
vẫn tuần tự vì phụ thuộc ô chéo/trên/trái; không dùng `omp for` trực tiếp trên
hai vòng lặp ô DP. Chưa có wavefront, BLAST, CUDA, MPI, GUI hoặc benchmark toàn
diện. Unit test đạt và phân tích code hỗ trợ tính đúng đắn, nhưng chưa chạy race
detector. Sẵn sàng chuẩn bị benchmark tuần 3; chưa có kết luận speedup/efficiency.
Repository chưa được commit tự động; bản trước tuần 2 được lưu trong
`.week1-backups/` tại thư mục cha. Người dùng nên duyệt và lưu checkpoint Git.
