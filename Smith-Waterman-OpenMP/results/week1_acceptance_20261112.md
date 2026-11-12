# Biên bản nghiệm thu tuần 1 — 12/11/2026

**Kết luận:** Đạt các hạng mục chức năng bắt buộc tuần 1 trên Windows/GCC MSYS2
UCRT64 hiện tại; đủ điều kiện kỹ thuật bắt đầu tuần 2. Không phát hiện lỗi thuật
toán hoặc lỗi build chặn tiến độ. Có một lỗi hiển thị CLI mức nhẹ và một vấn đề
giao diện IntelliSense chưa xác nhận trực tiếp, được ghi rõ bên dưới.

## Phạm vi và nguyên tắc kiểm tra

Rà soát toàn bộ mã nguồn, header, kiểm thử, CMake, cấu hình VS Code, dữ liệu mẫu,
dữ liệu sinh, README và báo cáo. Chụp SHA-256 của 31 file có sẵn ngoài `.git/`
và thư mục build để đối chiếu sau nghiệm thu. Không sửa mã nguồn, cấu hình,
unit test, tài liệu cũ hoặc dữ liệu bàn giao; không triển khai chức năng tuần 2,
không commit/push hoặc cài đặt phần mềm. Chỉ tạo bản build mới, đầu ra kiểm tra
trong các thư mục build đó và biên bản này.

## Môi trường và build

| Hạng mục | Kết quả |
|---|---|
| Hệ điều hành / shell | Windows / PowerShell |
| Compiler | `C:/msys64/ucrt64/bin/g++.exe`, GCC 16.2.0 |
| C++ | C++17, không dùng GNU extensions |
| CMake / Ninja | 4.4.5 / 1.13.2 |
| Git | 2.50.1.windows.1 |
| OpenMP | CMake nhận 5.2, `-fopenmp`, macro 202111 |
| Build Debug | Thành công, 0 cảnh báo và 0 lỗi compiler |
| Build Release | Thành công, 0 cảnh báo và 0 lỗi compiler |

Hai thư mục build được tạo mới, không dùng executable/cache cũ. Lệnh từ thư mục
`D:\BTL HPC\Smith-Waterman-OpenMP`:

```powershell
cmake -S . -B build-audit-20261112-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-audit-20261112-debug
ctest --test-dir build-audit-20261112-debug --verbose --output-on-failure

cmake -S . -B build-audit-20261112-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-audit-20261112-release
ctest --test-dir build-audit-20261112-release --verbose --output-on-failure
```

Log của lần nghiệm thu đã được chuyển sang `results/` trước khi dọn hai thư mục
build audit cũ. Các lệnh phía trên được giữ để có thể tái tạo bản build khi cần.

- Debug: [configure](week1_audit_20261112/debug/configure.txt),
  [build](week1_audit_20261112/debug/build.txt),
  [CTest chi tiết](week1_audit_20261112/debug/ctest_detail.txt).
- Release: [configure](week1_audit_20261112/release/configure.txt),
  [build](week1_audit_20261112/release/build.txt),
  [CTest chi tiết](week1_audit_20261112/release/ctest_detail.txt).

## Module đã hoàn thành

| Module | Đánh giá nghiệm thu |
|---|---|
| Smith-Waterman serial | Đúng recurrence local `max(0, diagonal, up, left)`, match +2, mismatch −1, gap −2; hàng/cột biên 0; score-only, xây ma trận, tìm cực đại, traceback, tọa độ và hiển thị |
| Kiểm tra DNA | Chấp nhận A/C/G/T và chữ thường; từ chối ký tự lạ; chấp nhận chuỗi rỗng ở API căn chỉnh |
| FASTA Reader | Đọc file/stream nhiều record và nhiều dòng; ID, description, sequence; LF/CRLF; bỏ dòng trống hợp lệ; kiểm tra nucleotide, record rỗng/trùng ID, file không tồn tại, lỗi định dạng/I/O |
| DNA Generator | Count/length/seed, `mt19937`, ánh xạ A/C/G/T tái lập; FASTA LF gói dòng 80 base; kiểm tra tham số và từ chối ghi đè file có sẵn |
| CLI | `align`, `search`, `generate`, `--help`; chọn query/reference theo ID; score-only mặc định, traceback theo yêu cầu; xử lý mã thoát và lỗi tham số |
| OpenMP test | Chương trình riêng dùng `#pragma omp parallel`; yêu cầu và nhận đủ 4 luồng, in đúng tập ID 0–3 |
| CMake / CTest | C++17, thư viện serial dùng lại được, executable, 5 nhóm CTest, cấu hình Debug/Release và cờ OpenMP tách riêng |
| Dữ liệu / tài liệu | Hai file FASTA mẫu, hai bộ DNA sinh nhỏ/vừa, README tiếng Việt và báo cáo tuần 1 có phân công/kế hoạch |
| VS Code | Cấu hình cho cả thư mục cha và project; compiler/include/compile database tồn tại, file OpenMP có `-fopenmp`; trạng thái Problems thực tế chưa quan sát được |

## Số kiểm tra PASS / FAIL

| Bộ kiểm tra | Debug PASS | Debug FAIL | Release PASS | Release FAIL |
|---|---:|---:|---:|---:|
| Smith-Waterman | 17 | 0 | 17 | 0 |
| FASTA Reader | 12 | 0 | 12 | 0 |
| DNA Generator | 10 | 0 | 10 | 0 |
| CLI integration | 22 | 0 | 22 | 0 |
| OpenMP runtime | 1 | 0 | 1 | 0 |
| **Tổng nhóm/tình huống nội bộ** | **62** | **0** | **62** | **0** |

Theo cách đếm của CTest: **5 PASS / 0 FAIL mỗi cấu hình**, tổng 10 lượt nhóm CTest
đạt ở hai cấu hình. Ba executable unit test thuần có tổng **39 nhóm PASS / 0 FAIL**
mỗi cấu hình. Tổng 62 bao gồm thêm CLI và OpenMP, không phải 62 mục CTest riêng.
Các vòng lặp 7.225 cặp và 250 cặp nằm trong các nhóm Smith-Waterman, không được
cộng lại thành số mục CTest.

### Smith-Waterman

Đã chạy lại 7 ca bắt buộc, xác nhận cùng điểm ở score-only và alignment:

| Chuỗi A | Chuỗi B | Kỳ vọng | Thực tế |
|---|---|---:|---:|
| ACGT | ACGT | 8 | 8 |
| ACGT | ACG | 6 | 6 |
| AAAA | TTTT | 0 | 0 |
| A | A | 2 | 2 |
| A | G | 0 | 0 |
| Rỗng | Rỗng | 0 | 0 |
| Rỗng | ACGT | 0 | 0 |

- Mọi cặp từ 85 chuỗi DNA dài 0–3: **7.225 cặp** khớp oracle độc lập liệt kê
  substring và căn chỉnh toàn cục với biên âm.
- **250 cặp** sinh theo seed: điểm hai chế độ khớp, đối xứng; traceback chấm lại
  đúng điểm, chuỗi bỏ gap khớp substring và tọa độ gốc.
- Chuỗi giống nhau dài **1.200 base**: điểm **2.400**.
- Có ca gap hai chiều (14 điểm), mismatch (13 điểm), không có điểm dương,
  ma trận mẫu, chữ thường, đầu vào sai và hòa điểm.
- Quy tắc hòa điểm trong mã nhất quán: cực đại đầu tiên khi quét theo hàng;
  traceback ưu tiên chéo, trên, trái, dừng tại ô 0.
- Score-only giữ hai hàng dài `min(m,n)+1`, không sao chép cả input:
  bộ nhớ phụ O(min(m,n)); alignment giữ ma trận O((m+1)(n+1)). Thời gian DP
  O(mn), thêm O(m+n) kiểm tra đầu vào và tối đa O(m+n) traceback.

Serial không có trạng thái toàn cục thay đổi được và không phụ thuộc OpenMP;
phù hợp giữ làm chuẩn đối chiếu và gọi độc lập trong thiết kế tuần 2.

### FASTA Reader và DNA Generator

Các unit test đã chạy lại bao gồm multi-record/multiline, LF/CRLF, lowercase,
dòng trống, file thật, file thiếu, header sai, record rỗng/trùng ID, N/U/gap/
whitespace không hợp lệ và lỗi stream. Không phát hiện bỏ qua nucleotide sai.
Generator được kiểm tra miền seed, tham số sai, overflow kích thước, round trip,
gói dòng, chuẩn hóa, lỗi đường dẫn và bảo toàn byte của file đã tồn tại.

Kiểm tra bổ sung trên **chính dữ liệu bàn giao**:

- `small_seed42.fasta`: 3 chuỗi × 32 base, 144 byte. Sinh lại cùng tham số vào
  thư mục audit cho file giống từng byte; SHA-256 khớp dữ liệu gốc.
- `medium_seed2026.fasta`: 4 chuỗi × 512 base, 2.144 byte. Sinh lại cùng seed
  2026 cho file giống từng byte; SHA-256 khớp dữ liệu gốc.
- Query `query_1` trên references mẫu: điểm **8, 6, 2, 3** theo thứ tự record.
- Query `query_2`: điểm **4, 4, 0, 14**.
- Query `seq_1` trên dữ liệu vừa: **1024, 227, 221, 223**.

Kết quả bổ sung cuối: **7 PASS / 0 FAIL**. Bằng chứng:
[audit_integration_verified.json](week1_audit_20261112/release/audit_integration_verified.json).

Ghi chú kiểm toán: lần kiểm tra bổ sung đầu có 6 PASS / 1 FAIL do người kiểm tra
đặt sai kỳ vọng thủ công `ACGT/AAAAGCCCC = 4`. Oracle độc lập cho điểm đúng **3**,
khớp chương trình: `ACG/AAG`, `2−1+2=3`. Đã sửa kỳ vọng của phép kiểm tra tạm
và xác minh lại mục này; không sửa project hoặc test có sẵn. Đầu ra ban đầu
được giữ trong [audit_integration.json](week1_audit_20261112/release/audit_integration.json).
Không có lỗi thuật toán được xác nhận từ lần lệch kỳ vọng này.

### OpenMP

Cả hai cấu hình đều in đủ ID **0, 1, 2, 3**, máy báo 12 processor, yêu cầu 4 và
nhận 4 luồng. Có tắt dynamic teams và kiểm tra kích thước team; thứ tự ID khác
nhau là bình thường. Mã serial/CLI không chứa parallel loop; OpenMP chỉ nằm
trong chương trình kiểm tra riêng. Chưa có kết quả speedup hoặc benchmark.

## Lỗi, điểm chưa xác minh và giới hạn còn tồn tại

1. **Lỗi hiển thị CLI mức nhẹ — đã tái hiện.**
   Tại `src/main.cpp:92`, nhãn `Score: ` được in trước khi hàm tính điểm kiểm tra
   DNA. Lệnh `sw_cli align --seq1 ACNT --seq2 ACGT` trả mã 1 đúng và thông báo
   lỗi đúng trên stderr, nhưng stdout còn `Score: ` dở dang. Không ảnh hưởng điểm
   của input hợp lệ hoặc thuật toán; không chặn bắt đầu tuần 2. Chưa sửa theo
   yêu cầu chỉ nghiệm thu.
2. **Dấu đỏ VS Code — chưa xác minh trực tiếp giao diện Problems.**
   Cấu hình hiện tại hợp lệ về JSON, compiler/include/database đều tồn tại và
   database có cờ OpenMP cho file tương ứng. Tuy nhiên không có nội dung Problems
   hoặc ảnh màn hình để kết luận dấu đỏ đã hết hay nguyên nhân cụ thể. Đây là
   mục còn mở về môi trường editor, không phải một test thất bại đã tái hiện.
3. **Git chưa có checkpoint mã tuần 1.**
   Các file project và cấu hình mới vẫn là untracked; commit gần nhất là
   `b70c148 first commit`. Nên lưu mốc tuần 1 trước khi sửa sang tuần 2 để có bản
   serial đối chiếu. Lần nghiệm thu không stage/commit/push.
4. **Giới hạn đã công bố, không phải thiếu chức năng tuần 1:** FASTA reader và
   generator giữ dataset trong RAM; alignment dùng ma trận bậc hai; chưa có
   streaming/hạn mức RAM cấu hình. Chưa xác minh compiler/máy khác. Chỉ hỗ trợ
   DNA A/C/G/T, linear gap, một traceback tối ưu. Không có BLAST/CUDA/MPI/GUI
   hoặc benchmark toàn diện trong phạm vi này.

## Đánh giá sẵn sàng tuần 2

**Đủ điều kiện kỹ thuật bắt đầu tuần 2**: có baseline serial đúng theo các kiểm
tra đã chạy, API không giữ trạng thái chung thay đổi được, FASTA/dataset tái lập,
CMake/CTest hoạt động cả Debug/Release và OpenMP đã chạy nhiều luồng thực tế.
Một lỗi hiển thị nhẹ không làm sai kết quả; phần giao diện editor cần theo dõi
nếu người dùng còn thấy chẩn đoán đỏ. Nên lưu mốc Git trước giai đoạn tiếp theo.

Không coi kết quả runtime OpenMP là xác nhận Smith-Waterman song song. Lần kiểm
tra này không tạo hoặc chỉnh sửa chức năng tuần 2; chỉ đánh giá nền tảng hiện tại.
