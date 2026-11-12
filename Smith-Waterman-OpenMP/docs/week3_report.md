# Báo cáo tuần 3 — Thực nghiệm, speedup và efficiency

**Ngày:** 12/11/2026, Asia/Bangkok.  
**Phạm vi:** benchmark Parallel Batch Smith-Waterman trong project hiện có.  
**Kết quả nghiệm thu:** benchmark Release chạy đủ 6 dataset, 11 cấu hình/dataset,
2 warm-up và 7 lần đo/cấu hình. Có 594 mẫu, 66 dòng tổng hợp, 5 biểu đồ PNG;
mọi vector điểm đều khớp serial. Debug/Release đều đạt **9/9 nhóm CTest**, tương
ứng **158 PASS / 0 FAIL** nội bộ mỗi cấu hình. Không thay kernel hoặc công thức DP.

## 1. Mục tiêu và baseline được bảo toàn

Tuần 3 xây dựng quy trình thực nghiệm có thể tái lập: so sánh serial/parallel,
các mức luồng, static/dynamic và phân bố độ dài; xuất raw/summary CSV, kiểm tra
công thức độc lập và tạo hình dùng cho Chương 3 của báo cáo cuối kỳ.

Trước sửa đã xác nhận checkpoint **4069ccb — Complete weeks 1-2 with OpenMP batch
Smith-Waterman**, working tree sạch. Đã chạy lại build và 7/7 nhóm CTest ở cả
Debug/Release. Không cần yêu cầu tạo lại checkpoint. Không commit/push tuần 3.
API và mã serial, parallel, FASTA, generator, CLI cũ và các test tuần 1–2 giữ nguyên.

`BatchResult.compute_seconds` hiện bắt đầu sau kiểm tra batch/cấp phát mảng đầu
ra và kết thúc sau join. Đây là phạm vi được dùng trực tiếp ở cả hai chế độ,
không tự thay bằng wall-clock của toàn bộ lệnh CLI.

## 2. Môi trường thực nghiệm đã xác minh

| Thông số | Giá trị đo/đọc thực tế |
|---|---|
| CPU | 13th Gen Intel Core i5-13420H |
| Nhân vật lý / luồng logic | 8 / 12, qua Win32_Processor |
| RAM hệ thống báo cáo | 16.890.519.552 byte, khoảng 15,73 GiB |
| Windows | Windows 11 Pro, 10.0.26200, build 26200 |
| Compiler | GCC/G++ 16.2.0, MSYS2 UCRT64 |
| CMake / Ninja | 4.4.5 / 1.13.2 |
| Build dùng kết luận hiệu năng | Release, `-O3 -DNDEBUG`, C++17 |
| OpenMP | `-fopenmp`, CMake nhận 5.2, macro 202111 |
| Các mức luồng | 1, 2, 4, 8, 12; actual=requested ở mọi mẫu chính thức |
| Chính sách runtime | `OMP_DYNAMIC=FALSE`, `OMP_WAIT_POLICY=PASSIVE` |
| Nguồn điện / power plan | Cắm nguồn, pin báo 100%, Balanced |
| Điểm Smith-Waterman | Match +2, mismatch −1, linear gap −2 |

[environment.json](../results/week3/environment.json) lưu thông số. WMI ban đầu
bị chặn quyền; sau đó đã đọc thành công bằng truy vấn chỉ đọc được cấp quyền.
Clock WMI báo 2100 MHz không được coi là xung duy trì hoặc turbo trong lúc đo.
Không thay đổi power plan/Windows settings và không suy đoán topology P/E core.

Không chạy build/test/vẽ biểu đồ cùng lúc với benchmark chính thức. Không tắt
ứng dụng nền của người dùng; không bảo đảm máy hoàn toàn rỗi. Không pin affinity,
không khóa tần số/đo nhiệt độ. Đây là benchmark trên laptop đang sử dụng, không
phải môi trường hiệu năng chuyên dụng đã cách ly.

## 3. Thiết kế bộ dữ liệu

| Dataset | Seed | Query length | References | Min/max reference | Số ô DP/batch |
|---|---:|---:|---:|---:|---:|
| small_uniform | 3101 | 192 | 8 | 192..192 | 294912 |
| small_uneven | 3101 | 192 | 8 | 128..256 | 294912 |
| medium_uniform | 3102 | 768 | 64 | 768..768 | 37748736 |
| medium_uneven | 3102 | 768 | 64 | 512..1024 | 37748736 |
| large_uniform | 3103 | 1536 | 96 | 1536..1536 | 226492416 |
| large_uneven | 3103 | 1536 | 96 | 1024..2048 | 226492416 |

Các chuỗi giả lập được tạo bằng DNA Generator đã kiểm thử, không dùng để kết luận
độ chính xác sinh học. Cách tái tạo từng nhóm quy mô:

1. Gọi `generate_dna(N+1, 4*L/3, seed)` với L/N/seed ở bảng.
2. Query là prefix L base của chuỗi nguồn đầu tiên.
3. Reference i lấy từ chuỗi nguồn i+1. Uniform lấy L base.
4. Uneven có N chẵn: ghép cặp độ dài `L-delta`, `L+delta`, delta giảm đều từ L/3
   về 0 theo nửa đầu vector; sắp độ dài tăng theo chỉ số reference.

Cả hai phân bố dùng cùng query và cùng tổng độ dài reference, nên có cùng số
ô DP; không đánh đồng thời gian ít việc hơn với lập lịch tốt hơn. Cách xếp tăng
có chủ đích tạo chênh tải cho các đoạn chỉ số liên tiếp của static. Đây là một
phân bố nghiên cứu, không đại diện mọi tập gene thực tế.

Tất cả FASTA dùng để đo được lưu tại [data](../results/week3/data/). Metadata lưu
từng độ dài, query ID, seed, số reference và DP cells. Unit test sinh lại cả sáu
dataset cùng tham số, so từng sequence. SHA-256 file thực nghiệm nằm trong
[validation.json](../results/week3/figures/validation.json). FASTA pilot và chính
thức đã được đối chiếu giống từng byte. `.gitattributes` giữ LF cho FASTA/CSV khi
checkout trên Windows để tránh thay đổi checksum chỉ vì newline.

## 4. Runner và phương pháp đo chính thức

`sw_benchmark` là executable riêng, tái sử dụng `smith_waterman_batch()`.
`sw_benchmark_support` chứa dataset, thống kê, cấu hình đo và CSV writer; không
thay đổi thư viện `sw_core`/`sw_parallel`.

Quy trình cho từng dataset:

1. Nạp/sinh dữ liệu một lần trước vùng đo, không đọc lại FASTA trong các vòng lặp.
2. Chạy một batch serial tham chiếu ngoài phép đo để có toàn bộ vector điểm chuẩn.
3. Tạo 11 cấu hình: serial; parallel 1/2/4/8/12, mỗi mức có static và dynamic.
4. Xáo trộn thứ tự các **block cấu hình** có seed từ dataset ID. Công thức seed
   uint32: bắt đầu 7301, mỗi byte ID cập nhật `seed=seed*33+byte`; dùng mt19937/
   std::shuffle. Thứ tự thực tế được lưu bằng `order_index` trong raw CSV.
5. Trong mỗi block, chạy ngay 2 warm-up rồi 7 lần đo liên tiếp cùng cấu hình.
6. Sau từng lời gọi, so **tất cả điểm** với vector chuẩn ngoài timer. Sai bất kỳ
   điểm nào sẽ báo lỗi, không chấp nhận mẫu; không chỉ so checksum.
7. Lưu cả warm-up và measure; summary chỉ dùng measure. Sau khi mọi dataset
   hoàn tất mới đánh dấu metadata `status=complete`.

Nạp file, sinh DNA, xáo trộn, so vector, checksum, ghi CSV/terminal và vẽ hình
không nằm trong compute time. Timer của batch gồm kernel (kể cả validate nội bộ
của kernel và cấp phát/giải phóng hai hàng DP), fork/join, scheduling và chi phí
runtime trong vùng đó. Batch validation và mảng score/error được cấp trước timer.
Chỉ đo compute, không tuyên bố đây là thời gian end-to-end của tìm kiếm FASTA.

Runner từ chối thư mục đầu ra có sẵn. Nếu lỗi giữa chừng, thư mục còn lại không
có trạng thái complete; script không dùng nó để vẽ số liệu chính thức. Có hỗ trợ
FASTA ngoài và seed không biết để trống, không tự gán seed giả.
Debug chỉ được phép với `--allow-debug` cho smoke; script hiệu năng từ chối CSV Debug.

## 5. Pilot và thay đổi protocol có lưu bằng chứng

Lần chạy đầu dùng thứ tự xoay vòng cấu hình. Khi rà soát mã/thời gian, phát hiện
dynamic cùng p thường đi ngay sau static cùng p, có thể hưởng lợi từ worker
team đã có đúng kích thước. Chỉ xoay điểm bắt đầu vẫn giữ phần lớn quan hệ kề
này, chưa đủ loại thiên lệch. Đây là nguy cơ từ phương pháp đo, không phải lỗi DP.

Đã lưu nguyên lần đo đầu, metadata, CSV, biểu đồ và source support trước sửa tại
[pilot_rotation](../results/week3/pilot_rotation/). Sau đó đổi sang block xáo trộn,
warm-up ngay trước mỗi cấu hình, build/test lại và chạy lại cùng dataset thành
kết quả chính thức ở thư mục `week3/`. Không trộn mẫu của hai protocol, không
chọn số pilot đẹp hơn để thay số chính thức.

Thay đổi này cải thiện phương pháp kiểm soát so sánh, không phải tối ưu kernel.
Chênh lệch pilot/chính thức không được dùng làm chứng cứ tăng tốc thuật toán:
thứ tự đo và trạng thái máy thay đổi nên không đủ tách nguyên nhân.

## 6. Công thức thống kê và các chỉ số HPC

```text
T_serial       = median(7 thời gian serial của cùng dataset)
T_parallel(p)  = median(7 thời gian parallel của cùng cấu hình, cùng actual p)
Speedup S(p)   = T_serial / T_parallel(p)
Efficiency E   = S(p) / p
Efficiency (%) = E * 100
DP cells       = query_length * sum(reference_lengths)
GCUPS          = DP cells / (T_parallel(p) * 10^9)
```

DP cells chỉ đếm các ô bên trong recurrence, không đếm hàng/cột biên. GCUPS là
thông lượng ô DP, không phải số nucleotide sinh học đã được xác minh.
CSV còn lưu mean/min/max và độ lệch chuẩn **population** (chia N); warm-up bị
loại khỏi mọi thống kê summary. Số thực xuất 17 chữ số, dấu thập phân `.`.
Các bảng trong báo cáo làm tròn để đọc, không dùng số làm tròn để tính chỉ số.

Nhóm summary gồm dataset, mode, schedule, requested và actual threads. Nếu
actual giảm hoặc thay đổi giữa các lần đo thì tách nhóm, tính E theo actual,
ghi sample_count/complete_samples/thread_mismatch; không gộp hoặc chia E cho
request. Biểu đồ chỉ vẽ nhóm đủ mẫu và actual=requested. Phiên này không có
nhóm lệch thread hoặc thiếu mẫu.

Script Python tính lại độc lập median/mean/min/max/stddev, S/E/GCUPS từ raw,
đối chiếu mọi dòng summary, số lần đo, index và metadata. Kết quả: **594 raw,
462 measure, 66 summary hợp lệ**. Thử cố ý sửa speedup trong một bản sao dưới
build: validator từ chối; CSV chính thức không bị sửa.

## 7. Bảng thời gian và kết quả nổi bật

Đây là cấu hình parallel có median thấp nhất **trong các cấu hình đã đo của
phiên này**, không phải lựa chọn tối ưu đã xác nhận trên mọi phiên/máy:

| Dataset | Serial (ms) | Cấu hình quan sát tốt nhất | Parallel (ms) | Speedup | Efficiency (%) |
|---|---:|---|---:|---:|---:|
| small_uniform | 0.3041 | static 4 | 0.1423 | 2.137 | 53.43 |
| small_uneven | 0.3131 | static 4 | 0.1594 | 1.964 | 49.11 |
| medium_uniform | 41.8688 | static 8 | 9.7067 | 4.313 | 53.92 |
| medium_uneven | 38.8206 | dynamic 12 | 7.7498 | 5.009 | 41.74 |
| large_uniform | 244.6499 | static 12 | 61.5066 | 3.978 | 33.15 |
| large_uneven | 239.2112 | static 12 | 59.3873 | 4.028 | 33.57 |

Giá trị nổi bật: medium_uneven dynamic 12 đạt khoảng **5,009×**, E **41,74%**.
Large uniform/uneven nhanh nhất quan sát được ở static 12 khoảng **3,978×** và
**4,028×**. Việc thêm luồng không mang lại tăng tốc tỷ lệ thuận.

## 8. Phân tích speedup và efficiency

- Medium_uniform: static 8 đạt 9,7067 ms, S=4,313; static 12 lại 11,4933 ms,
  chậm hơn 8. Dynamic 12 đạt 9,9719 ms nhưng vẫn không hơn static 8 ở phiên này.
- Large_uniform: dynamic 8 là 64,7576 ms, dynamic 12 là 64,1834 ms: lợi ích rất
  nhỏ trong khi E giảm từ khoảng 47,22% xuống 31,76%.
- Với large_uniform static, E giảm từ khoảng 94,79% ở p=2 xuống 33,15% ở p=12.
  p là số thread OpenMP thực tế; 12 luồng logic không được coi là 12 nhân vật lý.
- Dữ liệu nhỏ: nhiều cấu hình chậm hơn serial. Ví dụ small_uniform static 12
  là 1,1569 ms so với serial 0,3041 ms, S=0,263. Chỉ 8 reference nên 12 thành
  viên team còn có thể không nhận việc; chi phí runtime/OS đáng kể ở quy mô này.
- Một số E p=1 hơi vượt 100% (ví dụ large_uniform dynamic p=1 khoảng 101,79%).
  Không cắt số hoặc coi là bằng chứng siêu tuyến tính; xung/cache/lập lịch và
  biến động giữa block có thể gây khác biệt nhỏ ở baseline. Cần phép đo kiểm soát
  hơn để quy nguyên nhân, không chỉ dựa vào tỉ số median.

Biểu đồ thời gian có thanh min/max để thấy dao động. Ví dụ serial small_uniform
có range khoảng 0,3001–0,7840 ms, medium_uniform 39,8483–59,6622 ms. Median 7
mẫu không loại hết nhiễu hoặc thiên lệch; không có khoảng tin cậy từ nhiều phiên.

## 9. So sánh static/dynamic và phân bố độ dài

| Dataset | Static p=4 (ms) | Dynamic p=4 (ms) | Static p=12 (ms) | Dynamic p=12 (ms) |
|---|---:|---:|---:|---:|
| small_uniform | 0.1423 | 0.4394 | 1.1569 | 0.9206 |
| small_uneven | 0.1594 | 0.4082 | 0.2632 | 0.2669 |
| medium_uniform | 12.2775 | 13.6928 | 11.4933 | 9.9719 |
| medium_uneven | 13.2799 | 12.3135 | 9.3899 | 7.7498 |
| large_uniform | 75.4066 | 87.7942 | 61.5066 | 64.1834 |
| large_uneven | 89.5262 | 75.7958 | 59.3873 | 59.9003 |

Với medium_uneven, dynamic nhanh hơn static tại p=2/4/8/12 trong phiên này, phù
hợp với khả năng cân bằng các reference dài/ngắn được xếp thành cụm chỉ số.
Large_uneven cũng nghiêng về dynamic ở p=2/4/8, nhưng p=12 static 59,3873 ms và
dynamic 59,9003 ms gần nhau; không kết luận một lịch luôn thắng.

Uniform cũng không có lịch thắng tuyệt đối. Static giảm chi phí cấp việc nhưng
runtime/OS/phân bố công việc trên các thread có thể ảnh hưởng. Đặc biệt kết quả
small dao động, không dùng vài phần mười ms để khẳng định quy luật phần cứng.

## 10. Nút thắt: điều đã quan sát và điều chưa chứng minh

| Yếu tố | Nhận xét có cơ sở / giới hạn |
|---|---|
| Overhead OpenMP và quá ít reference | Small có nhiều trường hợp S<1; p=12>N=8. Phù hợp giả thuyết overhead/worker wake-up, chưa profile tách riêng OS và runtime |
| Lệch tải | Uneven cố ý xếp tăng độ dài, cùng tổng ô với uniform; dynamic có lợi ở nhiều mức p. Không đồng nhất mọi chênh lệch với load imbalance |
| Quá nhiều luồng | Một số đường cong chậm lại hoặc gần bão hòa từ 8 lên 12; hiệu quả tính theo team giảm |
| Cấp phát hai hàng DP | Có trong timer nhưng chưa đo riêng thời gian allocator, nên chưa đủ cơ sở sửa kernel hoặc tái sử dụng buffer |
| Kiểm tra DNA lặp | Có trong kernel để giữ API an toàn; chưa profile phần chi phí, không bỏ validation chỉ để nâng số speedup |
| Băng thông/cache/SMT/xung | Có thể góp phần, nhưng không có hardware counter hoặc phép đo xung/nhiệt, không kết luận là nút thắt đã chứng minh |

## 11. Tối ưu đã thực hiện và đối chiếu trước/sau

Không sửa công thức, scoring, traceback, cấu trúc hai hàng hay kernel tuần 2.
Số liệu chưa tách được chi phí allocation/validation/bandwidth để biện minh cho
tối ưu code. Phần đã thực hiện là cải thiện protocol và **tuning lựa chọn tham số**
dựa trên cùng dataset: so cấu hình parallel mặc định trên máy này (dynamic 4)
với cấu hình có median nhỏ nhất quan sát được.

| Dataset | Dynamic 4 trước tuning (ms) | Lựa chọn sau tuning | Median (ms) | Tỉ số thời gian trước/sau |
|---|---:|---|---:|---:|
| small_uniform | 0.4394 | static 4 | 0.1423 | 3.088 |
| small_uneven | 0.4082 | static 4 | 0.1594 | 2.561 |
| medium_uniform | 13.6928 | static 8 | 9.7067 | 1.411 |
| medium_uneven | 12.3135 | dynamic 12 | 7.7498 | 1.589 |
| large_uniform | 87.7942 | static 12 | 61.5066 | 1.427 |
| large_uneven | 75.7958 | static 12 | 59.3873 | 1.276 |

Các dòng được suy ra từ cùng summary, không phải các lần chạy mới hoặc số giả.
[tuning_comparison.csv](../results/week3/tuning_comparison.csv) giữ số đầy đủ.
Đây là lựa chọn thăm dò trên chính tập mẫu đã đo, có selection bias; cần phiên
độc lập trước khi chốt cấu hình mặc định. Không đổi mặc định CLI hoặc tuyên bố
kernel được tăng tốc 3× chỉ vì chọn thread/schedule khác. Không có bảng tăng tốc
trước/sau sửa kernel vì không áp dụng sửa kernel; pilot cũng không được dùng
làm thay thế cho bảng đó.

## 12. CSV, metadata và biểu đồ

- [raw_benchmark.csv](../results/week3/raw_benchmark.csv): cả warm-up/measure,
  dataset/seed/kích thước/DP work, mode/schedule, request/actual, run/order index,
  thời gian, checksum và cờ verified.
- [summary_benchmark.csv](../results/week3/summary_benchmark.csv): 66 nhóm, số mẫu,
  cờ thiếu/lệch team, median/mean/min/max/stddev, S/E/%/GCUPS.
- [run_metadata.json](../results/week3/run_metadata.json): trạng thái hoàn tất,
  compiler/build type/revision gốc, dirty flag, env OpenMP, cấu hình, recipe thứ
  tự chạy, từng độ dài và đường dẫn FASTA.
- [environment.json](../results/week3/environment.json): phần cứng/hệ điều hành/power.
- [validation.json](../results/week3/figures/validation.json): công thức, số mẫu và SHA-256.
- [verification.json](../results/week3/verification.json): tổng hợp nghiệm thu, trạng thái
  source/Git, test và fingerprint của producer/CSV.

Năm PNG 200 dpi, chỉ vẽ số đo chính thức. Captions và đơn vị:

1. [Execution time](../results/week3/figures/execution_time.png): median compute ms,
   thanh min/max 7 lần đo; đường ngang serial, x là actual thread.
2. [Speedup](../results/week3/figures/speedup.png): Tserial/Tparallel theo actual p.
3. [Efficiency](../results/week3/figures/efficiency.png): S/p ×100; đường 100% là mốc.
4. [Scheduling comparison](../results/week3/figures/scheduling_comparison.png):
   Tstatic/Tdynamic; >1 nghiêng về dynamic, <1 nghiêng về static.
5. [Size scaling](../results/week3/figures/size_scaling.png): time giây theo DP cells,
   hai trục log, tách uniform/uneven.

Các đường nối chỉ hỗ trợ đọc các điểm đã đo, không phải dự báo tại mọi số luồng.
Đã kiểm tra hình để bảo đảm có tiêu đề, nhãn trục, đơn vị và legend rõ ràng.
Không cần Python cho lõi C++; Matplotlib đã có sẵn, không cài phần mềm mới.

## 13. Build, chạy lại và chính sách không ghi đè

```powershell
Set-Location 'D:\BTL HPC\Smith-Waterman-OpenMP'
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure

$env:OMP_DYNAMIC = 'FALSE'
$env:OMP_WAIT_POLICY = 'PASSIVE'
# Lần bàn giao đã dùng --output results/week3. Chạy lại phải dùng tên MỚI:
.\build-release\sw_benchmark.exe --suite all --threads 1,2,4,8,12 --schedules static,dynamic --warmup 2 --runs 7 --output results/week3_recheck
python scripts/plot_benchmark.py results/week3_recheck
python scripts/plot_benchmark.py results/week3 --validate-only
```

Các biến môi trường chỉ đổi trong phiên Terminal. Runner không thay thiết lập
OpenMP toàn cục bằng omp_set_* và không đổi cấu hình Windows. Có thể chọn một
suite nhỏ hơn hoặc nhập FASTA thật qua `--query`, `--query-id`, `--references`,
`--dataset-id`. Giá trị thread không hỗ trợ bị từ chối trước khi đo.

Script từ chối figures có sẵn, trừ khi chỉ định `--replace-figures` để chủ đích
render lại PNG/validation. Nó không sửa raw/summary CSV. Các kết quả cũ, bao gồm
pilot, đều được giữ; không chạy benchmark mới đè lên một thư mục cũ.

## 14. Kết quả kiểm thử sau triển khai

| Nhóm | Debug PASS / FAIL | Release PASS / FAIL |
|---|---:|---:|
| Smith-Waterman serial | 17 / 0 | 17 / 0 |
| FASTA Reader | 12 / 0 | 12 / 0 |
| DNA Generator | 10 / 0 | 10 / 0 |
| Parallel batch API | 23 / 0 | 23 / 0 |
| CLI tuần 1 | 36 / 0 | 36 / 0 |
| Parallel CLI | 35 / 0 | 35 / 0 |
| OpenMP runtime | 1 / 0 | 1 / 0 |
| Benchmark math/dataset | 12 / 0 | 12 / 0 |
| Benchmark CLI/smoke | 12 / 0 | 12 / 0 |
| **Tổng kiểm tra nội bộ** | **158 / 0** | **158 / 0** |

CTest đếm 9 nhóm, không phải 158 mục riêng. Các vòng lặp oracle và nhiều mức
luồng nằm trong nhóm. Không có assert bắt speedup >1. Các số timing tổng hợp
được đưa thủ công vào test chỉ là fixture số học, không được xuất thành số liệu
thực nghiệm trong báo cáo.

Test mới bao gồm median chẵn/lẻ, mean/min/max/population stddev, thời gian sai,
S/E/GCUPS và loại warm-up, nhóm actual riêng, đếm DP cells, replay sáu dataset,
FASTA roundtrip, cấu hình trùng/sai, smoke đo thật, block không bị xen ngang,
warm-up ngay trước measurement, CSV escape/precision/cờ thiếu mẫu. CLI kiểm tra
smoke, row count, complete metadata, không ghi đè, tham số sai, FASTA ngoài,
Debug guard/Release policy. Validator Python còn bắt lỗi công thức cố ý trong
bản sao dưới build, không thay đổi số liệu thật.

Đã rebuild đầy đủ Debug/Release, sau sửa protocol chạy lại toàn bộ CTest; sau
bổ sung kiểm tra block cũng chạy lại toàn bộ. Không có cảnh báo/lỗi compiler.
Log cuối: [CTest Debug](../results/week3/checks/ctest_debug.txt),
[Release](../results/week3/checks/ctest_release.txt),
[build Debug đầy đủ](../results/week3/checks/build_debug.txt),
[Release đầy đủ](../results/week3/checks/build_release.txt),
[xác minh âm CSV](../results/week3/checks/negative_csv_validation.json).

## 15. File tạo/sửa và hạn chế còn lại

| Loại | File/phạm vi |
|---|---|
| Tạo | `include/benchmark.h`, `src/benchmark_support.cpp`, `src/benchmark.cpp` |
| Tạo | `tests/test_benchmark.cpp`, `tests/test_benchmark_cli.cmake` |
| Tạo | `scripts/plot_benchmark.py`, `docs/week3_report.md`, `.gitattributes` |
| Tạo | `results/week3/` chứa CSV, FASTA, metadata, PNG, log và pilot |
| Sửa | CMakeLists thêm library/runner/tests và build metadata; README; .gitignore bỏ Python cache |
| Giữ nguyên | Mã và API Smith-Waterman tuần 1–2, FASTA/generator, CLI cũ, test cũ và dữ liệu đã bàn giao |

Hạn chế: một máy, một phiên đo chính thức, 7 mẫu mỗi cấu hình; không pin/khóa
xung/nhiệt độ hoặc chặn mọi tải nền. Có biến động và giá trị p=1 hơi vượt 100%
efficiency, cần thêm phiên kiểm chứng để kết luận nhỏ. Dữ liệu giả lập A/C/G/T
không đại diện đầy đủ dữ liệu sinh học thật. Không chạy profiler/race detector
hoặc thống kê đa phiên; không có bằng chứng đủ để quy mọi chênh lệch cho một
thành phần phần cứng. Lịch sử pilot giữ riêng, không chọn lọc bỏ lần chậm trong
raw chính thức. Chưa trực tiếp xác minh Problems của VS Code trong phiên này.

## 16. Kết luận và công việc tuần 4

Đủ điều kiện nghiệm thu chức năng tuần 3: runner chạy thật, sáu dataset đúng
tham số, serial/parallel khớp điểm, nhiều mức thread/hai lịch, công thức được
kiểm tra độc lập, có CSV/PNG, build/test đạt và tài liệu đủ dùng cho Chương 3.

Sẵn sàng tuần 4 để biên tập báo cáo/thuyết trình, chọn hình/bảng thích hợp,
thảo luận nhiễu và giới hạn, bổ sung lần đo độc lập hoặc FASTA thực tế nếu nhóm
cần kết luận mạnh hơn. Nên duyệt và lưu checkpoint Git tuần 3 trước giai đoạn
tiếp theo. Không tự commit/push, cài phần mềm, đổi Windows settings hoặc triển
khai CUDA/MPI/BLAST/wavefront/GUI và các phần việc tuần 4 trong lượt này.

## Phụ lục: toàn bộ thời gian, speedup, efficiency và GCUPS

Các bảng dưới được sinh từ summary chính thức, không lấy từ pilot.

### small_uniform

| Mode | Schedule | Actual p | Median (ms) | Speedup | Efficiency (%) | GCUPS |
|---|---|---:|---:|---:|---:|---:|
| serial | none | 1 | 0.3041 | 1.000 | 100.00 | 0.970 |
| parallel | static | 1 | 1.1816 | 0.257 | 25.74 | 0.250 |
| parallel | static | 2 | 0.5282 | 0.576 | 28.79 | 0.558 |
| parallel | static | 4 | 0.1423 | 2.137 | 53.43 | 2.072 |
| parallel | static | 8 | 0.4752 | 0.640 | 8.00 | 0.621 |
| parallel | static | 12 | 1.1569 | 0.263 | 2.19 | 0.255 |
| parallel | dynamic | 1 | 0.6636 | 0.458 | 45.83 | 0.444 |
| parallel | dynamic | 2 | 0.3559 | 0.854 | 42.72 | 0.829 |
| parallel | dynamic | 4 | 0.4394 | 0.692 | 17.30 | 0.671 |
| parallel | dynamic | 8 | 0.2729 | 1.114 | 13.93 | 1.081 |
| parallel | dynamic | 12 | 0.9206 | 0.330 | 2.75 | 0.320 |

### small_uneven

| Mode | Schedule | Actual p | Median (ms) | Speedup | Efficiency (%) | GCUPS |
|---|---|---:|---:|---:|---:|---:|
| serial | none | 1 | 0.3131 | 1.000 | 100.00 | 0.942 |
| parallel | static | 1 | 0.3089 | 1.014 | 101.36 | 0.955 |
| parallel | static | 2 | 0.2965 | 1.056 | 52.80 | 0.995 |
| parallel | static | 4 | 0.1594 | 1.964 | 49.11 | 1.850 |
| parallel | static | 8 | 0.2300 | 1.361 | 17.02 | 1.282 |
| parallel | static | 12 | 0.2632 | 1.190 | 9.91 | 1.120 |
| parallel | dynamic | 1 | 0.3132 | 1.000 | 99.97 | 0.942 |
| parallel | dynamic | 2 | 0.2510 | 1.247 | 62.37 | 1.175 |
| parallel | dynamic | 4 | 0.4082 | 0.767 | 19.18 | 0.722 |
| parallel | dynamic | 8 | 0.3017 | 1.038 | 12.97 | 0.978 |
| parallel | dynamic | 12 | 0.2669 | 1.173 | 9.78 | 1.105 |

### medium_uniform

| Mode | Schedule | Actual p | Median (ms) | Speedup | Efficiency (%) | GCUPS |
|---|---|---:|---:|---:|---:|---:|
| serial | none | 1 | 41.8688 | 1.000 | 100.00 | 0.902 |
| parallel | static | 1 | 41.9848 | 0.997 | 99.72 | 0.899 |
| parallel | static | 2 | 21.0026 | 1.994 | 99.68 | 1.797 |
| parallel | static | 4 | 12.2775 | 3.410 | 85.26 | 3.075 |
| parallel | static | 8 | 9.7067 | 4.313 | 53.92 | 3.889 |
| parallel | static | 12 | 11.4933 | 3.643 | 30.36 | 3.284 |
| parallel | dynamic | 1 | 43.1225 | 0.971 | 97.09 | 0.875 |
| parallel | dynamic | 2 | 21.0287 | 1.991 | 99.55 | 1.795 |
| parallel | dynamic | 4 | 13.6928 | 3.058 | 76.44 | 2.757 |
| parallel | dynamic | 8 | 11.2436 | 3.724 | 46.55 | 3.357 |
| parallel | dynamic | 12 | 9.9719 | 4.199 | 34.99 | 3.786 |

### medium_uneven

| Mode | Schedule | Actual p | Median (ms) | Speedup | Efficiency (%) | GCUPS |
|---|---|---:|---:|---:|---:|---:|
| serial | none | 1 | 38.8206 | 1.000 | 100.00 | 0.972 |
| parallel | static | 1 | 39.8908 | 0.973 | 97.32 | 0.946 |
| parallel | static | 2 | 22.7934 | 1.703 | 85.16 | 1.656 |
| parallel | static | 4 | 13.2799 | 2.923 | 73.08 | 2.843 |
| parallel | static | 8 | 12.0124 | 3.232 | 40.40 | 3.142 |
| parallel | static | 12 | 9.3899 | 4.134 | 34.45 | 4.020 |
| parallel | dynamic | 1 | 39.3668 | 0.986 | 98.61 | 0.959 |
| parallel | dynamic | 2 | 19.8623 | 1.954 | 97.72 | 1.901 |
| parallel | dynamic | 4 | 12.3135 | 3.153 | 78.82 | 3.066 |
| parallel | dynamic | 8 | 10.0439 | 3.865 | 48.31 | 3.758 |
| parallel | dynamic | 12 | 7.7498 | 5.009 | 41.74 | 4.871 |

### large_uniform

| Mode | Schedule | Actual p | Median (ms) | Speedup | Efficiency (%) | GCUPS |
|---|---|---:|---:|---:|---:|---:|
| serial | none | 1 | 244.6499 | 1.000 | 100.00 | 0.926 |
| parallel | static | 1 | 241.0477 | 1.015 | 101.49 | 0.940 |
| parallel | static | 2 | 129.0486 | 1.896 | 94.79 | 1.755 |
| parallel | static | 4 | 75.4066 | 3.244 | 81.11 | 3.004 |
| parallel | static | 8 | 70.8695 | 3.452 | 43.15 | 3.196 |
| parallel | static | 12 | 61.5066 | 3.978 | 33.15 | 3.682 |
| parallel | dynamic | 1 | 240.3424 | 1.018 | 101.79 | 0.942 |
| parallel | dynamic | 2 | 129.2301 | 1.893 | 94.66 | 1.753 |
| parallel | dynamic | 4 | 87.7942 | 2.787 | 69.67 | 2.580 |
| parallel | dynamic | 8 | 64.7576 | 3.778 | 47.22 | 3.498 |
| parallel | dynamic | 12 | 64.1834 | 3.812 | 31.76 | 3.529 |

### large_uneven

| Mode | Schedule | Actual p | Median (ms) | Speedup | Efficiency (%) | GCUPS |
|---|---|---:|---:|---:|---:|---:|
| serial | none | 1 | 239.2112 | 1.000 | 100.00 | 0.947 |
| parallel | static | 1 | 236.2141 | 1.013 | 101.27 | 0.959 |
| parallel | static | 2 | 141.3515 | 1.692 | 84.62 | 1.602 |
| parallel | static | 4 | 89.5262 | 2.672 | 66.80 | 2.530 |
| parallel | static | 8 | 69.0750 | 3.463 | 43.29 | 3.279 |
| parallel | static | 12 | 59.3873 | 4.028 | 33.57 | 3.814 |
| parallel | dynamic | 1 | 239.1984 | 1.000 | 100.01 | 0.947 |
| parallel | dynamic | 2 | 130.8568 | 1.828 | 91.40 | 1.731 |
| parallel | dynamic | 4 | 75.7958 | 3.156 | 78.90 | 2.988 |
| parallel | dynamic | 8 | 60.9822 | 3.923 | 49.03 | 3.714 |
| parallel | dynamic | 12 | 59.9003 | 3.993 | 33.28 | 3.781 |
