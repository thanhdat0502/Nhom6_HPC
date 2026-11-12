# Báo cáo khắc phục sau nghiệm thu tuần 1

**Ngày:** 12/11/2026, Asia/Bangkok.  
**Phạm vi:** chỉ sửa lỗi và kiểm tra nền tảng tuần 1 trong project hiện tại.  
**Kết luận:** lỗi stdout CLI đã sửa và có regression xác nhận; build/test Debug
và Release đều đạt. Không có lỗi chức năng đã tái hiện chặn bắt đầu tuần 2.
Chưa xác nhận trực tiếp bảng Problems hoặc dấu đỏ trên giao diện VS Code.

## 1. Vấn đề ban đầu và bảo toàn trạng thái

| Vấn đề | Trạng thái trước sửa |
|---|---|
| CLI in dở nhãn kết quả | `align --seq1 ACNT --seq2 ACGT` có stdout `Score: `, stderr báo DNA sai, exit 1 |
| Regression chưa kiểm tra riêng hai stream | Test cũ chỉ ghép stdout/stderr để tìm thông báo, nên không bắt được nhãn thừa |
| Dấu đỏ VS Code | Có cấu hình IntelliSense từ lượt trước nhưng chưa xác nhận Problems trực tiếp |
| Chưa có checkpoint tuần 1 | Git chỉ theo dõi README ban đầu; các file project mới còn untracked |
| Cần rà soát file đưa vào Git | Build đã được bỏ qua, cần loại thêm bản sao lưu và thư mục tạm của unit test |

Đã đọc cấu trúc, Git status, `.gitignore`, CMake, `main.cpp`, README và các test
hiện tại, rồi build/chạy CTest trước sửa ở cả Debug và Release.

Trước khi sửa mã đã thông báo chưa có checkpoint, đưa lệnh để người dùng tự
kiểm tra và commit, đồng thời tạo bản sao lưu **32 file** ngoài `.git` và build:

```text
D:\BTL HPC\.week1-backups\20261112-142919\week1-before-fixes.zip
D:\BTL HPC\.week1-backups\20261112-142919\manifest.json
```

Mỗi file trong ZIP được đọc lại và đối chiếu SHA-256 với manifest. SHA-256 ZIP:

```text
5e0d125b08d6cc6f8df9a1c55cea497ea8950f69861b087964ca8a6e12771a72
```

Bản ZIP là bản sao lưu cục bộ trước sửa, **không phải Git checkpoint**. Thư mục
này được Git bỏ qua và không nằm trong build để tránh bị dọn cùng sản phẩm build.
Không tự stage, commit, push, reset hoặc thay đổi lịch sử Git.

## 2. Nguyên nhân và sửa lỗi CLI

Mã cũ:

```cpp
std::cout << "Score: " << sw::smith_waterman_score(a, b) << '\n';
```

Phép chèn chuỗi `Score: ` vào stdout xảy ra trước lời gọi hàm tính điểm. Nếu hàm
phát hiện nucleotide sai và ném ngoại lệ, nhãn đã được ghi ra; khối catch vẫn
báo lỗi đúng nhưng không thể thu hồi stdout đã phát.

Mã mới tại `src/main.cpp`, nhánh `align` score-only:

```cpp
const int score = sw::smith_waterman_score(a, b);
std::cout << "Score: " << score << '\n';
```

Chỉ in sau khi hàm kiểm tra DNA và tính điểm thành công. Không thay recurrence,
điểm match/mismatch/gap, traceback, cú pháp CLI, khối catch hay exit code.

Kết quả thực tế ở cả Debug và Release:

| Lệnh | Exit | Stdout | Stderr |
|---|---:|---|---|
| `align --seq1 ACNT --seq2 ACGT` | 1 | Rỗng | `Error: Invalid DNA; only A, C, G, T are supported.` |
| `align --seq1 ACGT --seq2 ACGT` | 0 | `Score: 8` và newline | Rỗng |

## 3. File đã thay đổi

| File | Thay đổi |
|---|---|
| `src/main.cpp` | Tính điểm vào biến cục bộ trước khi ghi stdout |
| `tests/test_cli.cmake` | Kiểm tra riêng exit/stdout/stderr; tăng 22 thành 36 ca; đếm ca tự động |
| `Smith-Waterman-OpenMP/.gitignore` | Bỏ qua thư mục tạm `/test-temp-*/` khi chạy unit test trực tiếp |
| `.gitignore` tại repository root | File mới, bỏ qua `/.week1-backups/` |
| `README.md` | Cập nhật số ca CLI, liên kết báo cáo khắc phục và cây thư mục |
| `docs/week1_fix_report.md` | Báo cáo này |
| `results/week1_fix_20261112/` | Bằng chứng trước/sau sửa, build log và xác minh |

Không sửa `smith_waterman_serial.cpp`, FASTA Reader, DNA Generator, header,
CMake, unit test C++ hoặc cấu hình `.vscode` hiện tại. Các file dữ liệu bàn giao
giữ nguyên SHA-256. Không tạo project mới hoặc chức năng tuần 2.

## 4. Regression test

`tests/test_cli.cmake` kiểm tra độc lập cả ba kết quả của mỗi lần chạy:

- Exit code bằng đúng giá trị mong đợi.
- Stdout có kết quả hợp lệ; các trường hợp lỗi phải khớp `^$`.
- Stderr rỗng khi thành công; trường hợp lỗi phải bắt đầu bằng thông báo có ý nghĩa.

Các ca mới bổ sung so với 22 ca cũ:

1. DNA hợp lệ có mismatch: điểm 13.
2. Hai chuỗi không có điểm dương: điểm 0.
3. DNA chữ thường: điểm 8.
4. N ở chuỗi thứ hai bị từ chối, không in nhãn thành công.
5. Ký tự đặc biệt `!` ở score-only bị từ chối.
6. DNA chứa N trong chế độ alignment bị từ chối.
7. Ký tự gap `-` trong DNA đầu vào alignment bị từ chối.
8. Chuỗi A rỗng, B không rỗng: điểm 0.
9. Chuỗi B rỗng, A không rỗng: điểm 0.
10. Cả hai chuỗi rỗng ở score-only: điểm 0.
11. Cả hai chuỗi rỗng ở alignment: điểm 0, thông báo không có căn chỉnh dương.
12. FASTA chứa N không được hỗ trợ bị từ chối.
13. FASTA thiếu header bị từ chối.
14. FASTA có record rỗng bị từ chối.

Ca DNA chứa N ở chuỗi thứ nhất và các ca thiếu tham số đã có, nay được tăng
độ chặt bằng kiểm tra hai stream riêng. Các ca thành công cũ vẫn kiểm tra điểm,
traceback, search/generate/help; các ca lỗi cũ vẫn kiểm tra tham số, ID, I/O và
từ chối ghi đè.

Đối số rỗng được truyền trực tiếp bằng `execute_process(... --seq1 "" ...)`,
không đi qua mở rộng list `ARGN` vốn có thể bỏ phần tử rỗng. Vì vậy các ca rỗng
kiểm tra đúng chuỗi DNA rỗng, không vô tình biến thành ca thiếu tham số.
Fixture FASTA lỗi được tạo tại đường dẫn ngẫu nhiên mới trong build, kiểm tra
tồn tại trước khi ghi và dọn đúng các file đã tạo, không xóa đệ quy.

**Xác nhận regression:** sau khi cập nhật test nhưng trước khi sửa `main.cpp`,
đã chạy test CLI trên executable cũ. Test thất bại đúng tại:

```text
invalid_dna: unexpected stdout [Score: ]; expected pattern [^$]
```

Sau sửa/rebuild, ca đó và toàn bộ 36 ca CLI đều PASS ở cả hai cấu hình.
Đây là lỗi đã tái hiện rồi khắc phục, không chỉ là một test mới luôn đạt.

## 5. Build và CTest trước/sau sửa

Môi trường xác minh: GCC 16.2.0, GDB 18.1, CMake 4.4.5, Ninja 1.13.2,
MSYS2 UCRT64. Sau sửa đã configure lại và rebuild toàn bộ bằng `--clean-first`.

| Thời điểm / cấu hình | Build | Cảnh báo compiler | CTest PASS / FAIL |
|---|---|---:|---:|
| Trước sửa — Debug | Đạt | Không phát sinh bước compile mới | 5 / 0 |
| Trước sửa — Release | Đạt | Không phát sinh bước compile mới | 5 / 0 |
| Test regression mới / executable cũ | Không đổi executable | Không áp dụng | CLI: 0 / 1, thất bại đúng lỗi stdout |
| Sau sửa — Debug, rebuild đầy đủ | Đạt | 0 | 5 / 0 |
| Sau sửa — Release, rebuild đầy đủ | Đạt | 0 | 5 / 0 |

| Nhóm kiểm tra nội bộ sau sửa | Debug PASS / FAIL | Release PASS / FAIL |
|---|---:|---:|
| Smith-Waterman | 17 / 0 | 17 / 0 |
| FASTA Reader | 12 / 0 | 12 / 0 |
| DNA Generator | 10 / 0 | 10 / 0 |
| CLI integration | 36 / 0 | 36 / 0 |
| OpenMP runtime | 1 / 0 | 1 / 0 |
| **Tổng** | **76 / 0** | **76 / 0** |

Mỗi cấu hình có 5 nhóm CTest, không phải 76 mục CTest riêng.
Trong 17 nhóm Smith-Waterman có 7.225 cặp oracle độc lập, 250 cặp theo seed,
ca 1.200 base đạt 2.400 điểm và các kiểm tra điểm/substring/tọa độ traceback.
Hai chế độ score-only/alignment vẫn khớp điểm. FASTA vẫn đọc đúng và phát hiện
lỗi; generator vẫn tái lập byte và bảo toàn file cũ. OpenMP ở cả hai cấu hình
yêu cầu 4, nhận đủ 4 luồng, in tập ID 0–3; máy báo 12 processor.

Lệnh tái hiện từ thư mục project:

```powershell
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-debug
ctest --test-dir build-debug --verbose --output-on-failure

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-release
ctest --test-dir build-release --verbose --output-on-failure
```

Log mới, giữ riêng các log lịch sử:

- [CTest trước sửa Debug](../results/week1_fix_20261112/ctest_before_debug.txt),
  [Release](../results/week1_fix_20261112/ctest_before_release.txt).
- [Regression bắt lỗi cũ](../results/week1_fix_20261112/regression_before_fix.txt).
- [Build Debug](../results/week1_fix_20261112/build_debug.txt),
  [Release](../results/week1_fix_20261112/build_release.txt).
- [CTest sau sửa Debug](../results/week1_fix_20261112/ctest_after_debug.txt),
  [Release](../results/week1_fix_20261112/ctest_after_release.txt).
- [Xác minh CLI và editor](../results/week1_fix_20261112/verification.json).

## 6. IntelliSense và CMake Tools

Đã kiểm tra `.vscode/c_cpp_properties.json` và `settings.json` ở cả repository
root lẫn thư mục project, cùng phần cấu hình liên quan trong user settings.

| Nội dung | Kết quả |
|---|---|
| Compiler | Đúng `C:/msys64/ucrt64/bin/g++.exe`, tồn tại, GCC 16.2.0 |
| Chuẩn và mode | `cppStandard=c++17`, `intelliSenseMode=windows-gcc-x64` |
| Include project | `include/`, `tests/` tồn tại và trỏ đúng từ từng workspace |
| Header chuẩn C++ | Probe dùng string/vector/filesystem/random/iostream biên dịch syntax-only thành công |
| `omp.h` và macro | Probe tìm thấy `omp.h`, `_OPENMP` được định nghĩa khi có `-fopenmp` |
| Compiler args dự phòng | Có `-fopenmp` trong cấu hình IntelliSense |
| Compile database | Cả hai cấu hình trỏ đúng `build-debug/compile_commands.json`; có đủ 4 file test C++ và cờ OpenMP đúng cho file tương ứng |
| CMake Tools | Source/build directory đúng; compiler khớp C/C++ Extension; generator Ninja |
| Provider | Không thấy cấu hình provider chồng chéo trong các file đã đọc; dùng compile database trực tiếp |
| Schema | Cấu hình C/C++ được kiểm tra với schema của extension đã cài |

Không phát hiện cấu hình sai cần sửa thêm nên giữ nguyên `.vscode` và CMake,
không thay đổi cấu hình hệ thống Windows. Header probe kiểm tra GCC theo cấu
hình editor, không phải bằng chứng rằng engine IntelliSense đang không báo lỗi.

**Chưa trực tiếp đọc được bảng Problems trong VS Code. Không khẳng định đã hết
mọi dấu đỏ.** Người dùng xác minh thủ công:

1. Mở repository root hoặc riêng `Smith-Waterman-OpenMP`.
2. Nhấn `Ctrl+Shift+P`, chạy `C/C++: Select a Configuration`, chọn `MSYS2 UCRT64`.
3. Chạy `C/C++: Reset IntelliSense Database`, rồi `Developer: Reload Window`.
4. Mở `Ctrl+Shift+M` để xem Problems. Nếu còn lỗi, lấy nguyên nội dung, mã lỗi,
   nguồn chẩn đoán và file/dòng tương ứng.
5. Có thể chạy `C/C++: Log Diagnostics` để đối chiếu compiler và compile database.
   Không tắt error squiggles để che lỗi.

## 7. Git và checkpoint

Commit gần nhất trước sửa là `b70c148 first commit`. Các file project mới vẫn
chưa được Git theo dõi; chưa có checkpoint Git tuần 1 được tạo tự động.

Đã kiểm tra ignore rules bằng Git: build Debug/Release và compile database,
thư mục `test-temp-*`, ZIP/manifest sao lưu đều bị bỏ qua. Mã nguồn/header,
test, `.vscode` cần bàn giao, README/docs, FASTA mẫu/dữ liệu nhỏ và các log bằng
chứng `.txt` vẫn có thể đưa vào Git. Không bỏ qua toàn bộ `data/` hoặc cấu hình.

Quét các file dự kiến đưa vào Git theo mẫu khóa/token/private key và tên file
nhạy cảm; không phát hiện credential trong phạm vi quét. Log có đường dẫn
workspace cục bộ nên người dùng vẫn cần đọc danh sách/nội dung trước khi commit.
Đây không phải cam kết phát hiện mọi dạng bí mật.

Danh sách file và kết quả quét: [git_review.txt](../results/week1_fix_20261112/git_review.txt).
Để đối chiếu các file có sẵn đã sửa với bản sao lưu trước sửa, xem
[changes.patch](../results/week1_fix_20261112/changes.patch). Đối chiếu manifest
xác nhận chỉ 4 file cũ thay đổi như bảng trên; 28 file cũ còn lại giữ nguyên byte.

Lệnh an toàn để người dùng tự kiểm tra rồi lưu phiên bản sau khắc phục:

```powershell
Set-Location 'D:\BTL HPC'
git status --short --untracked-files=all
git add --dry-run -- .gitignore .vscode Smith-Waterman-OpenMP

# Chỉ chạy sau khi kiểm tra danh sách dry-run:
git add -- .gitignore .vscode Smith-Waterman-OpenMP
git diff --cached --stat
git diff --cached --name-only
git diff --cached

# Người dùng tự chạy sau khi đồng ý nội dung staged:
git commit -m "Complete week 1 and fix CLI output regression"
```

Không có lệnh push/force/reset/xóa lịch sử. Bản trước sửa được bảo toàn trong ZIP
nếu người dùng cần đối chiếu; checkpoint Git sau sửa chỉ được tạo khi người dùng
tự xem và thực hiện commit. Không tự khôi phục ZIP đè lên workspace đang có.

## 8. Các vấn đề còn mở và khả năng bắt đầu tuần 2

- Chưa quan sát trực tiếp Problems nên trạng thái dấu đỏ còn chờ người dùng xác minh.
- Chưa có checkpoint Git do người dùng chưa commit; đã có bản sao lưu kiểm chứng.
- Các giới hạn tuần 1 giữ nguyên: FASTA/generator giữ dataset trong RAM,
  alignment dùng ma trận bậc hai; chỉ DNA A/C/G/T và linear gap; chưa đo benchmark.

Lỗi stdout đã xử lý và có regression bảo vệ. Không có lỗi build/unit test/runtime
đã tái hiện còn chặn triển khai tuần 2 trên máy này. Nền tảng serial, FASTA,
generator và OpenMP vẫn đúng theo các kiểm tra đã chạy. Việc tiếp theo về quản
lý phiên bản là người dùng duyệt danh sách file và lưu checkpoint Git.
Không triển khai Smith-Waterman Parallel hoặc bất kỳ chức năng tuần 2 nào.
