# Báo cáo tuần 1 — Đề tài 6

**Môn:** Tính toán hiệu năng cao.  
**Đề tài:** Khớp mẫu chuỗi gene sinh học quy mô lớn, hướng Smith-Waterman/OpenMP.  
**Ngày xác minh:** 12/11/2026 (Asia/Bangkok).  
**Phạm vi:** nền tảng tuần tự C++17 và kiểm tra runtime OpenMP trên Windows.

## 1. Mục tiêu

Xây dựng project CMake có thể biên dịch bằng GCC MSYS2 UCRT64; hiện thực
Smith-Waterman tuần tự đúng điểm và traceback, đọc FASTA nhiều bản ghi,
sinh DNA theo seed, CLI dễ demo, kiểm thử tự động và tài liệu sử dụng.
Kiểm tra nhiều luồng OpenMP riêng, chưa song song hóa thuật toán căn chỉnh.

## 2. Phân công theo đề bài

| Thành viên | Phân công dự kiến |
|---|---|
| Nguyễn Lê Thành Đạt | Nhóm trưởng dự kiến, nghiên cứu và lập trình Smith-Waterman tuần tự |
| Anh Trần Hải Đăng | Nghiên cứu OpenMP, chuẩn bị kiến trúc xử lý song song |
| Mai Văn Thành Đạt | Đọc FASTA, tạo DNA, kiểm thử và quản lý kết quả |

Bảng này ghi lại phân công được cung cấp, không phải xác nhận đóng góp cá nhân
hay lịch sử thực hiện của từng thành viên. Mã và kết quả bên dưới được tạo,
biên dịch và chạy trong workspace ở phiên triển khai này.

## 3. Khảo sát workspace và công cụ

Ban đầu workspace `D:\BTL HPC` chỉ có `.git/` và `README.md` chứa `# Nhom6_HPC`.
Không tìm thấy `AGENTS.md`. Project được tạo trong thư mục con
`Smith-Waterman-OpenMP`; giữ nguyên dữ liệu, README và lịch sử Git ban đầu.
Không cài phần mềm, thay đổi cấu hình hệ thống, commit hoặc push lên GitHub.

| Công cụ | Kết quả thực tế |
|---|---|
| G++ | `C:\msys64\ucrt64\bin\g++.exe`, GCC 16.2.0 (Rev6, MSYS2) |
| CMake | 4.4.5, dùng generator Ninja |
| Ninja | 1.13.2 |
| Git | 2.50.1.windows.1 |
| OpenMP | CMake nhận 5.2, `-fopenmp`, macro 202111; có `libgomp-1.dll` |
| Processor theo OpenMP | 12 |

Tất cả công cụ cần thiết đã có. Đã kiểm tra OpenMP trực tiếp bằng G++ trước
khi tích hợp CMake. Không cần cơ chế thay thế CMake trên máy này; README vẫn
cung cấp lệnh G++ trực tiếp để sử dụng khi thiếu CMake.

## 4. Công việc hoàn thành và module

| Module/file | Nội dung đã triển khai |
|---|---|
| `include/smith_waterman.h`, `src/smith_waterman_serial.cpp` | Kiểm tra/chuẩn hóa DNA, ma trận DP, tìm ô cực đại, score-only hai hàng, traceback và in căn chỉnh |
| `include/fasta_reader.h`, `src/fasta_reader.cpp` | Đọc đường dẫn/stream FASTA, ID/mô tả/sequence, nhiều bản ghi và nhiều dòng, LF/CRLF, phát hiện lỗi |
| `include/dna_generator.h`, `src/dna_generator.cpp` | Sinh DNA theo count/length/seed, xuất FASTA LF, gói dòng, từ chối ghi đè |
| `src/main.cpp` | CLI `align`, `search`, `generate`, `--help`, kiểm tra tham số và mã thoát |
| `tests/test_smith_waterman.cpp` | Ca điểm bắt buộc, gap/mismatch/tie, oracle độc lập, kiểm tra tọa độ/điểm traceback, dữ liệu dài hơn |
| `tests/test_fasta_reader.cpp` | Kiểm tra FASTA đúng/sai, dòng LF/CRLF, dữ liệu file thật, lỗi I/O |
| `tests/test_dna_generator.cpp` | Tính tái lập, ánh xạ seed đã biết, round trip, byte đầu ra và bảo toàn file cũ |
| `tests/test_support.h` | Bộ kiểm tra nhỏ không phụ thuộc thư viện, file tạm độc lập, không dùng `assert` |
| `tests/test_cli.cmake` | Chạy executable thực tế với 22 tình huống thành công/thất bại |
| `tests/test_openmp.cpp` | Vùng parallel, ID luồng, xác minh đủ số luồng yêu cầu |
| `CMakeLists.txt` | C++17, thư viện serial dùng lại được, executable, CTest, OpenMP tách riêng |
| `data/`, `README.md`, `results/` | Dữ liệu nhỏ/vừa, hướng dẫn build/demo và bằng chứng chạy thực tế |

Danh sách đầy đủ các file nằm trong cây thư mục ở [README](../README.md).
Các file bổ sung phục vụ kiểm thử độc lập từng module, tích hợp CLI và lưu kết quả;
không mở rộng phạm vi sang tuần 2.

## 5. Hoạt động của Smith-Waterman

Với hai chuỗi a, b, tạo ma trận H có `(m+1) × (n+1)` phần tử, khởi tạo 0.
Mỗi ô phụ thuộc ô chéo trên trái, ô phía trên và ô bên trái:

```text
H(i,j) = max(0,
             H(i-1,j-1) + (a[i-1] == b[j-1] ? 2 : -1),
             H(i-1,j) - 2,
             H(i,j-1) - 2)
```

Thành phần 0 cho phép bắt đầu lại căn chỉnh tại bất kỳ vị trí nào.
Điểm tối ưu là cực đại của toàn ma trận, không nhất thiết là ô cuối.
Traceback từ cực đại, chọn bước giải thích được điểm hiện tại và dừng tại 0.
Đường truy vết cho một căn chỉnh cục bộ tối ưu, không liệt kê mọi phương án.

Quy tắc nhất quán: ô cực đại đầu tiên theo thứ tự hàng; traceback hòa điểm ưu tiên
chéo, rồi trên, rồi trái. Không có điểm dương: kết quả rỗng với điểm 0 và tọa độ 0.
API chấp nhận chuỗi DNA rỗng; bản ghi FASTA rỗng được báo lỗi theo chính sách file.
Chữ thường được chuẩn hóa; các ký tự ngoài A/C/G/T bị từ chối.

**Kiểm tra tay:** `ACGT/ACGT` có các ô chéo chính 2,4,6,8. Truy vết đi chéo bốn
bước về ô 0 nên điểm 8. Với `AC/AG`, ma trận:

| H | ∅ | A | G |
|---|---:|---:|---:|
| ∅ | 0 | 0 | 0 |
| A | 0 | 2 | 0 |
| C | 0 | 0 | 1 |

Cực đại 2 cho `A/A`; ô cuối chỉ có 1. Ví dụ có gap:
`AAAA-CCCC/AAAAGCCCC` có 8 match và 1 gap, điểm `8×2−2=14`.

**Chi phí:** với m,n > 0, cả hai chế độ tốn O(mn) thời gian.
Score-only giữ hai hàng có `min(m,n)+1` phần tử và không sao chép toàn bộ input,
bộ nhớ phụ O(min(m,n)); alignment giữ ma trận O((m+1)(n+1)) và chuỗi traceback.
Chuẩn hóa/kiểm tra đầu vào O(m+n), traceback O(m+n). Khi có chuỗi rỗng, việc
kiểm tra đầu vào vẫn tốn O(m+n). Có kiểm tra tràn kích thước ma trận và giới hạn
điểm kiểu `int`; CLI bắt lỗi cấp phát bộ nhớ.

Các hàm serial dùng bộ nhớ cục bộ, không có biến toàn cục thay đổi được và không
phụ thuộc runtime OpenMP. Đó là điểm chuẩn bị để gọi lại theo từng cặp độc lập ở tuần 2.

## 6. Kết quả kiểm thử thực tế

Đã cấu hình, biên dịch và chạy CTest bằng GCC cho cả **Debug** và **Release**.
Build cuối cùng không có cảnh báo với `-Wall -Wextra -Wpedantic`.

| Nhóm CTest | Nội dung | Debug | Release |
|---|---|---|---|
| `smith_waterman` | 17 nhóm kiểm tra, gồm các vòng kiểm tra nhiều cặp | Đạt | Đạt |
| `fasta_reader` | 12 nhóm kiểm tra | Đạt | Đạt |
| `dna_generator` | 10 nhóm kiểm tra | Đạt | Đạt |
| `cli` | 22 tình huống chạy executable | Đạt | Đạt |
| `openmp` | Đủ 4 luồng, ID 0–3 | Đạt | Đạt |

**Tổng: 5/5 nhóm CTest đạt cho từng cấu hình; 0 nhóm thất bại.**
Đây là số nhóm, không cộng dồn các cặp trong vòng lặp thành số test CTest.

| Chuỗi 1 | Chuỗi 2 | Điểm kỳ vọng | Điểm cả hai chế độ |
|---|---|---:|---:|
| ACGT | ACGT | 8 | 8 |
| ACGT | ACG | 6 | 6 |
| AAAA | TTTT | 0 | 0 |
| A | A | 2 | 2 |
| A | G | 0 | 0 |
| Rỗng | Rỗng | 0 | 0 |
| Rỗng | ACGT | 0 | 0 |

Các xác minh bổ sung:

- **7.225 cặp**: mọi cặp từ 85 chuỗi A/C/G/T độ dài 0–3. Oracle độc lập liệt kê
  cặp substring và căn chỉnh **toàn cục** từng cặp với biên âm, không dùng recurrence
  local của mã cần kiểm tra. Cả score-only và alignment khớp oracle.
- **250 cặp** từ bộ sinh seed 12345, độ dài tối đa 96: hai chế độ khớp điểm, đối xứng
  khi đổi thứ tự chuỗi; xác nhận chuỗi bỏ gap đúng substring và tọa độ gốc.
- Đồng nhất **1.200 base**, seed 42: điểm **2.400** ở cả hai chế độ.
- Traceback được chấm lại bằng `+2/−1/−2`; gap được kiểm tra ở cả hai phía.
- FASTA lỗi không bị bỏ qua, có kiểm tra file không tồn tại, record rỗng/trùng ID,
  nội dung trước header, ký tự lạ, whitespace trong sequence và lỗi stream.
- Sinh dữ liệu cùng tham số cho cùng byte FASTA; seed 42, 8 base cho `GTAGGTAA`.
  Xuất dữ liệu không ghi đè file cũ; đầu vào sai bị từ chối trước khi tạo file.
- CLI kiểm tra tham số thiếu/trùng/lạ, số sai hoặc vượt miền, ID không có, file
  không có, sinh dữ liệu rồi tìm lại query chính nó và bảo toàn đầu ra đã tồn tại.

Thời gian CTest ghi trong log chỉ là thời gian chạy kiểm tra ở phiên này,
không dùng làm benchmark hay suy luận speedup.

### Kiểm tra OpenMP

Đã biên dịch trực tiếp `tests/test_openmp.cpp` bằng `g++ -std=c++17 -fopenmp`,
sau đó build bằng CMake và chạy ở cả Debug/Release. Runtime báo 12 processor;
chương trình yêu cầu 4 luồng, nhận đúng 4, in đủ ID 0,1,2,3. Thứ tự in thay đổi
giữa các lần chạy do lập lịch luồng. Không có OpenMP trong các vòng lặp DP.

### Dữ liệu và demo

| File sinh thực tế | Số chuỗi | Độ dài | Seed | Kích thước file |
|---|---:|---:|---:|---:|
| `small_seed42.fasta` | 3 | 32 | 42 | 144 byte |
| `medium_seed2026.fasta` | 4 | 512 | 2026 | 2.144 byte |

Chạy query `seq_1` trên file vừa, điểm theo reference: `seq_1=1024`, `seq_2=227`,
`seq_3=221`, `seq_4=223`. Bộ mẫu `query_2/ref_gap` cho traceback điểm 14.

Log: [Debug](../results/ctest_debug.txt), [Release](../results/ctest_release.txt),
[OpenMP](../results/openmp.txt), [demo dữ liệu vừa](../results/medium_search.txt).
[Biên bản xác minh](../results/verification.md) ghi lệnh và checksum dữ liệu.

## 7. Khó khăn và cách xử lý

- PowerShell đọc tệp yêu cầu bằng encoding mặc định làm sai dấu tiếng Việt;
  đã đọc lại bằng UTF-8, tài liệu được ghi UTF-8.
- GCC 16 đưa cảnh báo tham chiếu có thể dangling ở hàm trả option trong map do
  tham số key tạo temporary `std::string`. Đổi key thành `const char*` để biểu diễn
  đúng thời gian sống và loại bỏ cảnh báo; reference trả về vẫn trỏ vào map sống
  trong cùng scope. Build Debug và Release cuối không còn cảnh báo này.
- Để đảm bảo bộ nhớ score-only O(min(m,n)), không tạo bản sao chuẩn hóa của cả
  hai input; kiểm tra trước và chuyển hoa từng ký tự trong phép so sánh.
- Căn chỉnh lớn có ma trận bậc hai; giữ score-only làm mặc định cho CLI và chỉ
  truy vết một reference được chỉ định. Không tạo dữ liệu khổng lồ trong tuần 1.
- Tái lập byte giữa thư viện chuẩn được xử lý bằng ánh xạ trực tiếp đầu ra
  `mt19937`, không phụ thuộc cách triển khai distribution.

## 8. Việc chưa hoàn thành và giới hạn

Không có hạng mục bắt buộc tuần 1 còn bị chặn trong môi trường hiện tại.
Phạm vi đã xác minh là Windows/GCC UCRT64 trên máy này; chưa xác minh máy khác
hoặc compiler khác. Bộ kiểm tra không phải chứng minh cho mọi kích thước đầu vào.

Các phần sau **chưa triển khai**, đúng phạm vi tuần 1: Smith-Waterman song song,
đánh giá speedup/efficiency, benchmark toàn diện, dữ liệu quy mô lớn, affine gap,
DNA mơ hồ, BLAST, CUDA, MPI, GUI/web. Generator và FASTA reader giữ toàn bộ dataset
trong RAM; chưa có streaming dữ liệu lớn. Chưa cấu hình hạn mức RAM tùy chỉnh
cho alignment. Không suy diễn kết quả hiệu năng từ chương trình kiểm tra luồng.

## 9. Kế hoạch đề xuất cho tuần 2

1. Anh Trần Hải Đăng thiết kế phân phối các cặp query/reference độc lập bằng
   OpenMP; giữ đúng phụ thuộc dữ liệu bên trong mỗi ma trận.
2. Nguyễn Lê Thành Đạt giữ serial làm oracle, đối chiếu điểm và traceback giữa
   serial với phương án đa luồng; thống nhất cách lưu kết quả theo thứ tự reference.
3. Mai Văn Thành Đạt chuẩn bị dataset theo seed và nhiều độ dài, quản lý metadata,
   bổ sung test cho kết quả tìm kiếm nhiều reference.
4. Sau khi đúng kết quả mới đo thời gian với 1/2/4/... luồng, thống nhất phương
   pháp chạy lặp và kiểm soát môi trường. Ghi nhận chi phí bộ nhớ mỗi luồng.

Các mục này là kế hoạch, chưa được thực hiện trong mã tuần 1.
