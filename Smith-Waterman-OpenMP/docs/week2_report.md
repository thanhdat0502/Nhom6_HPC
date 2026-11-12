# Báo cáo tuần 2 — Parallel Batch Smith-Waterman với OpenMP

**Ngày xác minh:** 12/11/2026, Asia/Bangkok.  
**Project:** `D:\BTL HPC\Smith-Waterman-OpenMP`.  
**Kết luận:** đã triển khai và chạy thực tế serial/parallel batch, static/dynamic,
cấu hình số luồng, metadata compute và kiểm thử tự động. Debug/Release đều build
thành công, **7/7 nhóm CTest PASS**, **134 kiểm tra nội bộ PASS / 0 FAIL** mỗi cấu
hình trên máy hiện tại. Chưa kết luận hiệu năng hoặc speedup.

## 1. Khảo sát và bảo toàn baseline tuần 1

Đã đọc CMake, header, source, các test hiện có và cách CLI đọc FASTA/search.
API tuần 1 có `smith_waterman_score(a,b)`, ma trận, cực đại và alignment/traceback.
Kernel score-only giữ hai hàng O(min(m,n)); search cũ lặp tuần tự qua reference.

Trạng thái thực tế khác một phần mô tả đầu vào: lỗi in dở `Score: ` đã được sửa
trong lượt khắc phục trước và có 36 ca CLI. Trước thay đổi tuần 2, đã chạy lại
configure/build/CTest Debug và Release: **5/5 PASS**, 76 kiểm tra nội bộ PASS mỗi
cấu hình. Không đưa lại lỗi CLI cũ; toàn bộ test tuần 1 được giữ nguyên.

Git vẫn chỉ có commit `b70c148 first commit`, project mới còn untracked. Đã báo
rõ điều này và không stage/commit/push. Trước sửa đã tạo bản sao **56 file**:

```text
D:\BTL HPC\.week1-backups\before-week2-20261112-145331\week1-final-before-week2.zip
D:\BTL HPC\.week1-backups\before-week2-20261112-145331\manifest.json
```

Đã đọc lại mọi entry và xác nhận SHA-256 khớp file gốc. SHA-256 ZIP:

```text
59e186e281ebcb28a1a2518fa1db7bc28d62ab807962b19ac87115499ab08244
```

Đây là bản sao cục bộ, không thay thế checkpoint Git. Dữ liệu FASTA, module
FASTA/generator, các test tuần 1 và cấu hình VS Code được giữ nguyên.
Baseline log: [Debug](../results/week2/baseline_debug.txt),
[Release](../results/week2/baseline_release.txt).

## 2. Mục tiêu và phân công nhóm

| Thành viên | Phạm vi công việc tuần 2 theo vai trò của nhóm |
|---|---|
| Nguyễn Lê Thành Đạt | Giữ kernel serial làm chuẩn, kiểm tra recurrence/điểm và đối chiếu API cũ |
| Anh Trần Hải Đăng | Thiết kế batch OpenMP, số luồng, scheduling và phân tích shared/private/exception |
| Mai Văn Thành Đạt | Dataset theo seed/FASTA, test đối chiếu, lưu kết quả và tài liệu |

Bảng này là ánh xạ phân công theo thông tin nhóm, không xác nhận lịch sử đóng
góp cá nhân. Mã và log báo cáo được tạo/chạy trực tiếp trong phiên triển khai này.

## 3. Phương pháp song song hóa và phụ thuộc DP

Mỗi ô Smith-Waterman phụ thuộc ba ô đã tính:

```text
H(i,j) = max(0,
             H(i-1,j-1) + substitution,
             H(i-1,j) + gap,
             H(i,j-1) + gap)
```

Vì vậy các ô cùng hàng/cột không độc lập. Gắn `parallel for` trực tiếp vào hai
vòng lặp DP có thể đọc ô chưa được tính, sai thứ tự và gây race trên ma trận.
Tuần 2 không triển khai wavefront hoặc đồng bộ theo đường chéo.

Phương án đã dùng: một query Q và N reference R[i], phân phối **N cặp độc lập**
`SW(Q,R[i])` cho các worker. Mỗi cặp gọi lại score-only serial; hai hàng DP của
phép gọi đó được cấp phát riêng. Công việc giữa các cặp không có phụ thuộc.

| Scheduling | Thực hiện | Ưu điểm và giới hạn |
|---|---|---|
| Static | `omp for schedule(static)` | Chia chỉ số thành các phần cố định, ít điều phối; reference dài/ngắn phân bố lệch có thể làm một worker kết thúc muộn |
| Dynamic | `omp for schedule(dynamic,1)` | Worker rảnh nhận thêm một reference; phù hợp thử với độ dài không đều, nhưng có thêm chi phí cấp việc |

Không suy luận lịch nào nhanh hơn từ unit test. Kết quả vẫn nằm ở chỉ số gốc,
không phụ thuộc worker nào nhận reference hoặc thứ tự hoàn tất.

## 4. Kiến trúc và API

```text
FASTA Reader (luồng gọi)
    -> chọn query / tập reference
    -> smith_waterman_batch(query, references, scoring, options)
       -> kiểm tra mode, threads, schedule, DNA, scoring và giới hạn int
       -> cấp phát scores[N], errors[N]
       -> serial loop HOẶC OpenMP parallel + for static/dynamic
          -> smith_waterman_score(query, reference[i], scoring)
          -> chỉ ghi scores[i] hoặc errors[i]
       -> join; rethrow lỗi đầu tiên theo chỉ số trên luồng gọi
    -> traceback một cặp nếu yêu cầu (serial, ngoài timer batch)
    -> in toàn bộ bảng theo thứ tự FASTA; metadata tùy chọn qua stderr
```

Header mới `smith_waterman_parallel.h` cung cấp:

- `ExecutionMode::{Serial,Parallel}` và `Schedule::{Static,Dynamic}`.
- `BatchOptions`: mode, threads và schedule; mặc định serial/1/dynamic.
- `BatchResult`: vector điểm, mode/schedule, số luồng yêu cầu/thực tế và compute seconds.
- `max_batch_threads()`: giới hạn request theo số processor khả dụng và OpenMP thread limit.
- `smith_waterman_batch(...)`: nhận query, vector `FastaRecord` chỉ đọc, scoring và options.

API chấp nhận DNA rỗng, vector reference rỗng; batch rỗng trả actual 0, time 0.
FASTA CLI tiếp tục từ chối record/file rỗng theo chính sách tuần 1.
Nếu reference ít hơn team, actual vẫn là kích thước team, không phải số worker
được giao ít nhất một reference.

Để nhận scoring config thật sự, thêm `ScoringConfig` và overload
`smith_waterman_score(a,b,scoring)`. Hàm hai đối số vẫn tồn tại và dùng +2/−1/−2.
Chỉ tổng quát hóa cùng recurrence hai hàng, không thêm thuật toán DP khác.
Matrix/alignment tuần 1 tiếp tục dùng scoring mặc định; CLI chưa thêm cờ scoring.
Scoring nhận match > 0, mismatch <= 0, gap < 0; kiểm tra `min(m,n)*match` trong
miền int bằng phép chia để tránh overflow ngay trong bước kiểm tra.

`sw_core` giữ độc lập với OpenMP. Thư viện mới `sw_parallel` liên kết công khai
`sw_core` và `OpenMP::OpenMP_CXX`; CLI và test parallel dùng thư viện mới.
CMake giữ các target tuần 1, thêm `test_parallel` và nhóm CTest `parallel_cli`.
Không hardcode đường dẫn compiler trong `CMakeLists.txt`; đường dẫn UCRT64 ở
lệnh build/cấu hình editor chỉ phục vụ môi trường hiện tại.

## 5. CLI và khả năng tương thích

```powershell
# Serial: cú pháp cũ vẫn chạy, không có metadata nếu không yêu cầu.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta

# Serial có metadata để chuẩn bị đối chiếu compute.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --mode serial --threads 1 --stats

# Parallel 4 luồng, thay dynamic bằng static để chọn lịch khác.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --mode parallel --threads 4 --schedule dynamic --stats

# Điểm batch song song và traceback một cặp bằng serial.
.\build-release\sw_cli.exe search --query data/samples/queries.fasta --query-id query_2 --references data/samples/references.fasta --mode parallel --threads 4 --alignment-id ref_gap
```

Stdout theo thứ tự reference: **4, 4, 0, 14** ở cả hai chế độ.
`--stats` ghi `Mode`, `Schedule`, `Requested threads`, `Actual threads`,
`Compute seconds` qua stderr; không thay đổi bảng stdout. Schedule là cấu hình
được chọn; không thực hiện phân phối OpenMP trong chế độ serial.

Serial chỉ chấp nhận threads 1. Parallel mặc định `min(4, max_batch_threads())`,
nhận các số dương đến giới hạn hiện tại (máy này 12). Runtime có thể giảm team
do dynamic/nesting; module ghi nhận actual, không tự thay ICV toàn cục.
Thread count 0/âm/quá miền/quá giới hạn, mode/schedule lạ, thiếu/trùng tham số
đều trả lỗi rõ ràng, exit 1, stdout rỗng. Cờ mode/threads chỉ mở rộng `search`,
không biến `align` một cặp thành thuật toán parallel.

## 6. Kết quả build và test thực tế

GCC 16.2.0 MSYS2 UCRT64, C++17, CMake 4.4.5, Ninja 1.13.2; CMake tìm thấy
OpenMP 5.2 (`-fopenmp`), runtime macro 202111, 12 processor.
Đã configure, **rebuild toàn bộ** bằng `--clean-first` và chạy CTest verbose.
Cả Debug/Release: **0 lỗi compiler, 0 cảnh báo compiler**, 7/7 nhóm CTest đạt.

| Nhóm | Debug PASS / FAIL | Release PASS / FAIL |
|---|---:|---:|
| Smith-Waterman tuần 1 | 17 / 0 | 17 / 0 |
| FASTA Reader tuần 1 | 12 / 0 | 12 / 0 |
| DNA Generator tuần 1 | 10 / 0 | 10 / 0 |
| Parallel batch API mới | 23 / 0 | 23 / 0 |
| CLI tuần 1 giữ nguyên | 36 / 0 | 36 / 0 |
| Parallel CLI mới | 35 / 0 | 35 / 0 |
| OpenMP runtime tuần 1 | 1 / 0 | 1 / 0 |
| **Tổng kiểm tra nội bộ** | **134 / 0** | **134 / 0** |

134 là số nhóm/tình huống nội bộ trên máy này, không phải 134 mục CTest riêng.
Các vòng lặp nhiều cặp/nhiều luồng nằm bên trong nhóm. Số ca parallel CLI tùy
các mức thread khả dụng; ở đây đã kiểm tra **1, 2, 4, 8, 12**.

23 nhóm API mới kiểm tra đầy đủ: một/nhiều reference, đồng nhất, không có điểm
dương, độ dài khác nhau/gap/mismatch, ties/trùng sequence, DNA/batch rỗng,
lowercase, dữ liệu nhỏ theo seed, 17 reference × 512 base, 73 reference có độ
dài không đều, nhiều/ít reference hơn thread, parallel một worker, 20 lần lặp
mỗi lịch, scoring tùy chỉnh, biên int/overflow, DNA/config/thread/enum sai,
không đổi ICV runtime và hai caller độc lập chạy đồng thời.

35 ca CLI mới kiểm tra output tương thích tuần 1, serial rõ ràng, stats, các
mức thread với hai lịch, traceback, FASTA vừa, input/option sai và thread limit.
Có kiểm tra process con với `OMP_THREAD_LIMIT=1`: request 2 bị từ chối, default
parallel nhận actual 1. Chỉ thay môi trường test con, không đổi cấu hình hệ thống.

Toàn bộ test cũ không bị sửa: vẫn đối chiếu oracle độc lập 7.225 cặp, 250 cặp
theo seed, traceback/tọa độ, sequence 1.200 base điểm 2.400, lỗi FASTA,
tái lập generator và regression stdout `Score:`.

### Demo thực tế và đối chiếu

Với file `medium_seed2026.fasta` và query `seq_1`:

| Reference | Serial | Static 4 | Dynamic 4 |
|---|---:|---:|---:|
| seq_1 | 1024 | 1024 | 1024 |
| seq_2 | 227 | 227 | 227 |
| seq_3 | 221 | 221 | 221 |
| seq_4 | 223 | 223 | 223 |

Ngoài CTest đã chạy 10 tổ hợp (5 mức thread × 2 lịch): stdout đều giống serial
từng byte, actual bằng request trong môi trường demo đã tắt dynamic teams ở
process con. Không lấy các lần demo này để kết luận tăng tốc.

Log: [build Debug](../results/week2/build_debug.txt),
[Release](../results/week2/build_release.txt),
[CTest Debug](../results/week2/ctest_debug.txt),
[CTest Release](../results/week2/ctest_release.txt),
[demo_runs.json](../results/week2/demo_runs.json),
[stdout serial](../results/week2/search_serial.txt),
[stdout parallel 4](../results/week2/search_parallel_4.txt),
[metadata parallel 4](../results/week2/search_parallel_4_stats.txt).

## 7. Phân tích an toàn đa luồng

Race condition xuất hiện khi kết quả phụ thuộc thứ tự thực thi không được kiểm
soát; data race C++ xảy ra khi các thread truy cập cùng vị trí nhớ, có ít nhất
một write, mà không có đồng bộ phù hợp. Unit test PASS không đủ để khẳng định
mọi trường hợp đều không có race; cần xét quyền sở hữu/truy cập trong mã.

| Dữ liệu | Shared/private và cách sử dụng |
|---|---|
| Query, references, scoring | Shared chỉ đọc; người gọi phải giữ ổn định suốt lời gọi |
| Hai hàng DP | Local trong mỗi lần gọi score kernel, độc lập giữa các worker |
| Index i, biến tạm của kernel/lambda | Private theo iteration/lần gọi |
| `scores` | Shared vector<int> đã resize N; iteration i chỉ ghi scores[i], không thay cấu trúc vector |
| `errors` | Shared vector<exception_ptr> đã cấp N; iteration i chỉ ghi errors[i] |
| Lambda `run_one` | Shared functor không có state mutable; biến cục bộ của từng invocation riêng |
| Actual team size | Chỉ một thread ghi trong `omp single`; caller đọc sau join |
| Result metadata và timer | Caller ghi trước/sau vùng parallel, không đồng thời với worker |
| File/terminal | Chỉ caller đọc FASTA trước vùng parallel và in sau khi hoàn tất |

OpenMP for bảo đảm mỗi chỉ số được phân phối một lần. Hai vector không resize,
không push_back, không dùng vector<bool>; các phần tử khác nhau là các đối tượng
riêng. Không có hai worker ghi cùng score/error. Caller không duyệt error hoặc
in result khi worker còn chạy. Barrier kết thúc parallel bảo đảm các write đã
hoàn tất trước khi đọc kết quả/exception.

`run_one` là noexcept với try/catch(...); exception của score kernel (bao gồm
lỗi cấp phát C++) được lưu vào slot riêng. Sau join, caller chọn lỗi đầu tiên
theo thứ tự reference và rethrow. Không có exception C++ đi thẳng ra qua biên
OpenMP. Kiểm tra input/scoring/overflow trước region tránh bắt đầu batch sai.

Module không gọi `omp_set_num_threads`, `omp_set_schedule` hoặc `omp_set_dynamic`;
chỉ dùng `num_threads` và hai worksharing schedule tường minh. Không có global
mutable state của project; kiểm tra ICV không đổi và concurrent callers đã đạt.
Không cần critical/atomic để ghi score; đồng bộ dùng cho actual và join.

**Giới hạn xác minh:** chưa chạy ThreadSanitizer/race detector, chưa fault-inject
hết các lỗi tài nguyên. Phân tích trên áp dụng khi caller không mutate input
đồng thời. Không gọi unit test PASS là chứng minh tuyệt đối cho mọi môi trường.

## 8. Khó khăn, xử lý và các lỗi đã khắc phục

- Prompt mô tả trạng thái cũ: đã xác minh trực tiếp lỗi CLI được sửa từ trước,
  không thay lại cú pháp hay làm mất regression đang có.
- Kernel cũ chỉ có scoring cố định: bổ sung overload cấu hình và validator,
  giữ nguyên hàm hai đối số, recurrence và các API matrix/traceback cũ.
- Exception không thể để thoát worker: dùng mảng exception_ptr theo index rồi
  rethrow sau join; không dùng shared push_back hoặc critical cho mỗi kết quả.
- Không đánh đồng request với actual: lấy team size ngay trong region và trả
  metadata; CLI/CTest cũng kiểm tra thread limit và trường hợp 1 worker.
- Giữ tương thích output: metadata đi stderr khi bật `--stats`; stdout được
  so sánh từng byte với serial, không có output từ các thread.
- Chỉ ghi bảng sau khi batch và traceback được yêu cầu đã thành công, tránh
  in dở nhãn kết quả khi bước tính toán phía sau ném lỗi.

Không có lỗi build/test còn bỏ qua. Cấu hình compile database vẫn được CMake
xuất cho editor, bao gồm module/test mới và cờ OpenMP. Không trực tiếp quan sát
Problems trong VS Code nên không tuyên bố mọi dấu đỏ đã hết.

## 9. File đã tạo và sửa

| Loại | File |
|---|---|
| Tạo | `include/smith_waterman_parallel.h`, `src/smith_waterman_parallel.cpp` |
| Tạo | `tests/test_parallel.cpp`, `tests/test_parallel_cli.cmake` |
| Tạo | `docs/week2_report.md`, `results/week2/*` |
| Sửa | `include/smith_waterman.h`, `src/smith_waterman_serial.cpp` để thêm scoring overload/validation dùng lại |
| Sửa | `src/main.cpp` để tích hợp batch và options/help/stats |
| Sửa | `CMakeLists.txt`, `README.md` |
| Giữ nguyên | FASTA/generator, dữ liệu bàn giao, toàn bộ test tuần 1, cấu hình VS Code/Git ignore và tài liệu tuần 1 |

Không có commit/push tự động hoặc phần mềm hệ thống được cài/thay đổi.

Đối chiếu SHA-256 với snapshot trước tuần 2: chỉ 5 file cũ thay đổi đúng danh
sách trên; 51 file cũ còn lại giữ nguyên từng byte. Các file tạo thêm được ghi
riêng trong Git status. Xem [patch so với tuần 1](../results/week2/changes.patch)
và [biên bản xác minh](../results/week2/verification.json). Index Git vẫn trống,
HEAD không đổi; người dùng tự duyệt và tạo checkpoint khi sẵn sàng.

## 10. Hạn chế và chuẩn bị benchmark tuần 3

- Một phép căn chỉnh đơn lẻ vẫn tuần tự; tăng thread không làm một ma trận
  riêng lẻ chạy nhanh hơn. Batch nhỏ có thể không đủ việc và có chi phí fork/join.
- Chỉ score batch chạy parallel; traceback một cặp được chọn vẫn serial và dùng
  scoring mặc định. Chưa thêm CLI scoring tùy chỉnh, affine gap hoặc wavefront.
- Reader/generator vẫn giữ dataset trong RAM. Mỗi worker cần hai hàng DP riêng;
  tổng bộ nhớ phụ ngoài dataset là O(N + tổng min(m,n_i) của các job đang chạy).
- Chưa có kiểm chứng trên compiler/máy khác, race detector hoặc đo hiệu năng
  toàn diện. Dấu đỏ editor và checkpoint Git vẫn cần người dùng xác nhận.

Đã sẵn sàng cho việc đo tuần 3: cùng một API và dataset cho serial/parallel,
thay đổi threads/schedule, ghi request/actual và compute seconds. Timer dùng
steady_clock, bắt đầu sau batch validation/cấp phát mảng kết quả và kết thúc
sau join; gồm kernel/DP allocation, kiểm tra nội bộ kernel và OpenMP overhead,
không gồm đọc FASTA, in output, traceback và duyệt error sau join.

Tuần 3 đề xuất duyệt/lưu checkpoint Git, chọn dataset đủ nhiều reference và các
phân bố độ dài, chạy lặp/warm-up, thống nhất thống kê và môi trường đo rồi mới
tính speedup/efficiency. Những phần đó chưa được triển khai trong tuần 2.
Không tạo biểu đồ hoặc số liệu hiệu năng giả, không có BLAST/CUDA/MPI/GUI.
