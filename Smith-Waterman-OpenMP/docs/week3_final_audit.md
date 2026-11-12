# Nghiệm thu cuối tuần 3

Ngày kiểm tra: **12/11/2026**, múi giờ Asia/Bangkok. Phạm vi: hoàn thiện bằng
chứng nghiệm thu của project C++17/OpenMP hiện tại trước tuần 4.

**Kết luận: đủ điều kiện bắt đầu tuần 4 trên nền tảng hiện tại.** Clean build
Debug/Release thành công; mỗi cấu hình đạt **9/9 nhóm CTest, 158 PASS / 0 FAIL**.
Phiên benchmark độc lập xác minh đủ 594 mẫu; thử nghiệm bổ sung trên FASTA thật
khớp điểm serial/static/dynamic. Không phát hiện lỗi chức năng cần sửa trong lõi.
Kết quả hiệu năng có biến động giữa các phiên: **0/6 cấu hình thắng phiên gốc
giữ vị trí nhanh nhất ở phiên mới**. Chỉ trình bày speedup như số đo trên máy,
dataset và phiên cụ thể. Chưa có bằng chứng từ race detector.

Toàn bộ dữ liệu mới ở [results/week3_validation](../results/week3_validation/).
Không commit/push, không triển khai CUDA, MPI, BLAST hoặc chức năng tuần 4.

## 1. Trạng thái ban đầu và bảo toàn project

HEAD là `4069ccb3b3c0a3b16967dd4aa84bcbc6798fd865` (checkpoint tuần 1–2).
Các nguồn benchmark, script, tài liệu và kết quả tuần 3 đang nằm trong working
tree chưa commit. Không có checkpoint Git tuần 3 riêng. Trước thay đổi quan
trọng đã hướng dẫn người dùng tự lưu checkpoint bằng:

```powershell
git add -- Smith-Waterman-OpenMP
git diff --cached --stat
git diff --cached
git commit -m "Checkpoint week 3 before final audit"
```

Đây là hướng dẫn chạy từ thư mục repo cha; các lệnh trên **không được tự chạy**.
Bản sao trước kiểm tra được lưu tại:

```text
D:\BTL HPC\.week1-backups\before-week3-final-audit-20261112-163444\
  project-before-final-audit.zip
  manifest.json
```

ZIP chứa 163 file được Git theo dõi hoặc chưa bị ignore, đã đối chiếu SHA-256
từng entry. Bản manifest được lưu kèm tại
[pre_audit_sha256.json](../results/week3_validation/checks/pre_audit_sha256.json).
Kiểm tra cuối so với snapshot xác nhận **64 file trong `results/week3/` và
15 file trong `results/week3_recheck/` giữ nguyên từng byte**, kể cả pilot,
CSV, FASTA, PNG, log và báo cáo kết quả gốc. README là file có sẵn duy nhất được
sửa trong lần nghiệm thu này để thêm liên kết; toàn bộ nguồn C++, header, test,
CMake, script vẽ và báo cáo tuần 3 trước đó giữ nguyên.
Xem [preservation.json](../results/week3_validation/checks/preservation.json).

`.gitignore` đã loại build, executable/object/library, log tạm, Python cache;
ignore cấp repo loại `.week1-backups/`. Không cần sửa quy tắc. Đã kiểm tra 9
đường dẫn: build/cache/fixture lỗi/backup bị ignore; CSV nghiệm thu và FASTA thật,
script kiểm tra vẫn có thể đưa vào Git. Các bộ DNA nhỏ có seed và dữ liệu thực
nghiệm là bằng chứng cần giữ, không phải file rác. Xem
[gitignore_verification.json](../results/week3_validation/checks/gitignore_verification.json).

## 2. Những hạn chế trước kiểm tra và cách xử lý

| Hạng mục ban đầu | Xử lý trong lần nghiệm thu |
|---|---|
| Chưa có đối chiếu phiên độc lập trong báo cáo nghiệm thu cuối | Chạy mới toàn bộ 6 dataset, 11 cấu hình, giữ cả kết quả bất lợi |
| Kết quả nổi bật gần 5× mới là quan sát của một phiên | So sánh đầy đủ 66 cấu hình, công bố thay đổi thứ hạng và mức dao động |
| Dữ liệu chính là DNA giả lập | Tải hai FASTA công khai từ NCBI, lưu nguyên bản, accession, URL, SHA-256 và kiểm tra điểm |
| Chưa có bằng chứng detector cho an toàn luồng | Rà soát code và chạy test concurrency; thử biên dịch TSan tối thiểu, ghi rõ không có runtime |
| Cần xác minh đồ thị thực sự sinh từ CSV | Tái tính toàn bộ công thức; vẽ lại hình gốc vào thư mục mới và so khớp SHA-256 PNG |
| Cần bằng chứng build/test cập nhật | Clean build cả hai cấu hình, CTest verbose, lưu log và đếm lại từng module |
| Chưa có báo cáo nghiệm thu cuối tuần 3 | Tạo tài liệu này và bổ sung liên kết trong README |

Không sửa lõi chỉ để tạo ra một thay đổi tối ưu. Các giới hạn thực nghiệm chưa
thể loại bỏ bằng việc đổi tham số được trình bày ở mục 12.

## 3. Phiên benchmark độc lập

Phiên chính chạy từ **09:39:42.654 đến 09:40:13.046 UTC** (16:39 giờ địa phương).
Nguồn đo là Release, GCC 16.2.0 UCRT64, C++17, `-O3 -DNDEBUG -fopenmp`.
CPU i5-13420H, 8 nhân vật lý/12 luồng logic, RAM 16.890.519.552 byte; Windows 11
Pro build 26200. CMake 4.4.5, Ninja 1.13.2, OpenMP macro 202111.
Đọc lại WMI thành công sau phiên đo; Balanced, sau đo máy báo cắm điện/pin 100%.
Clock WMI 2100 MHz không phải phép đo xung duy trì. Không có telemetry nhiệt độ,
tần số hoặc affinity. Xem [environment.json](../results/week3_validation/environment.json).

Lệnh thực tế, chạy trong thư mục project:

```powershell
$env:OMP_DYNAMIC='FALSE'
$env:OMP_WAIT_POLICY='PASSIVE'
.\build-release\sw_benchmark.exe --output results/week3_validation --suite all --threads 1,2,4,8,12 --schedules static,dynamic --warmup 2 --runs 7
```

Thư mục đầu ra chưa tồn tại trước lệnh; runner từ chối ghi đè. Không chạy
build/test/vẽ đồ thị đồng thời với phiên chính. Ứng dụng nền của người dùng
vẫn hoạt động; không thay đổi power plan hoặc cài phần mềm.

| Dataset | Seed | Query | Số reference | Min..max reference | DP cells/batch |
|---|---:|---:|---:|---:|---:|
| small_uniform | 3101 | 192 | 8 | 192..192 | 294912 |
| small_uneven | 3101 | 192 | 8 | 128..256 | 294912 |
| medium_uniform | 3102 | 768 | 64 | 768..768 | 37748736 |
| medium_uneven | 3102 | 768 | 64 | 512..1024 | 37748736 |
| large_uniform | 3103 | 1536 | 96 | 1536..1536 | 226492416 |
| large_uneven | 3103 | 1536 | 96 | 1024..2048 | 226492416 |

12 FASTA đầu vào của hai phiên khớp SHA-256; scoring vẫn **+2/−1/−2**, không đổi
kernel hay công thức. Mỗi dataset có serial 1 và parallel static/dynamic với
1/2/4/8/12 luồng: tổng 11 cấu hình. Giữ cách xáo trộn block có seed theo dataset
ID, 2 warm-up ngay trước 7 lần đo mỗi block. Không trộn pilot vào kết quả mới.

Runner lấy vector điểm serial làm oracle ngoài vùng đo, sau **mọi** lần chạy so
từng phần tử, không chỉ checksum. Kết quả: **594/594 mẫu verified**, gồm 132
warm-up và 462 lần đo, **66 summary groups**, 0 nhóm thiếu mẫu, 0 lệch luồng;
actual=requested ở mọi mẫu. Metadata trạng thái `complete`.

Timer là `BatchResult.compute_seconds`, thống nhất cả hai chế độ. Timer gồm
kernel, validation bên trong từng kernel, cấp phát/giải phóng hai hàng DP,
chi phí runtime/fork-join/lập lịch. Nạp/sinh dữ liệu, validation cấp batch,
cấp phát vector kết quả/error, kiểm tra điểm, I/O CSV và traceback nằm ngoài
timer. Đây là thời gian vùng compute, không phải toàn bộ thời gian tìm FASTA.

## 4. Đối chiếu hai phiên và độ ổn định

Nguồn: [comparison.csv](../results/week3_validation/comparison/comparison.csv)
và [comparison.json](../results/week3_validation/comparison/comparison.json).
Script đối chiếu xác minh cùng dữ liệu từng byte, cấu hình, protocol, compiler,
OpenMP/environment; checksum điểm xuyên phiên cũng trùng nhau.

`S4` = static, actual 4; `D8` = dynamic, actual 8. Thời gian là median **ms**.
“Tốt nhất” chỉ có nghĩa nhỏ nhất trong 10 cấu hình parallel của **chính phiên đó**.

| Dataset | Serial gốc → mới | Tốt nhất gốc: cấu hình / ms / S | Chạy lại đúng cấu hình thắng cũ: ms / S | Tốt nhất phiên mới: cấu hình / ms / S / E% |
|---|---:|---|---|---|
| small_uniform | 0.3041 → 0.3428 | S4 / 0.1423 / 2.1370 | 0.2224 / 1.5414 | S2 / 0.1937 / 1.7697 / 88.49 |
| small_uneven | 0.3131 → 0.2958 | S4 / 0.1594 / 1.9642 | 0.2114 / 1.3992 | D4 / 0.1797 / 1.6461 / 41.15 |
| medium_uniform | 41.8688 → 39.9965 | S8 / 9.7067 / 4.3134 | 10.4317 / 3.8341 | D8 / 8.9863 / 4.4508 / 55.64 |
| medium_uneven | 38.8206 → 39.7658 | D12 / 7.7498 / 5.0092 | 9.1218 / 4.3594 | D8 / 8.9454 / 4.4454 / 55.57 |
| large_uniform | 244.6499 → 237.8019 | S12 / 61.5066 / 3.9776 | 67.4349 / 3.5264 | D12 / 62.6644 / 3.7948 / 31.62 |
| large_uneven | 239.2112 → 240.2042 | S12 / 59.3873 / 4.0280 | 60.5074 / 3.9698 | D12 / 56.1599 / 4.2771 / 35.64 |

Trên cả 66 nhóm, thay đổi median `(T_mới/T_gốc−1)×100` nằm trong **−73.77% đến
+56.29%**, trung vị mô tả là **−2.64%**. Không lấy con số này làm mức cải thiện
toàn chương trình: các workload và thời gian khác nhau. Khoảng min..max quan sát
giao nhau ở 51/66 nhóm; min/max không phải khoảng tin cậy hay kiểm định ý nghĩa.
Không loại outlier, không chỉnh CSV, không chọn lại một phiên thuận lợi hơn.

Điểm số và xu hướng coarse-grained parallel trên medium/large tái lập được.
Thứ hạng cấu hình và một số phép đo nhỏ **không ổn định**. Cụ thể mức 5.0092×
của medium_uneven D12 giảm còn 4.3594× khi chạy lại đúng D12; D8 đạt 4.4454×
trong phiên mới. Không được quảng bá “ổn định 5×” hoặc “static/dynamic luôn thắng”.

Các khả năng kỹ thuật gồm overhead trên batch rất ngắn, trạng thái worker pool,
lập lịch Windows, vị trí luồng và biến động xung/tải nền. Đây là **giả thuyết**;
không có trace hoặc telemetry để quy nguyên nhân cho từng chênh lệch. 7 lần đo
liên tiếp trong block cũng không phải 7 phiên độc lập. Không suy luận cache miss,
bandwidth saturation hoặc throttling từ thời gian đơn thuần.

Thư mục `week3_recheck/` có sẵn cũng được xác minh riêng: cùng 594 mẫu/66 nhóm,
dữ liệu/protocol/điểm khớp; chỉ 1/6 winner cũ giữ thứ hạng. Dao động median so
gốc −72.71%..+276.19% (trung vị +2.70%). Không dùng phiên này thay phiên nghiệm
thu mới; giữ nguyên và lưu đối chiếu tại
[recheck_comparison](../results/week3_validation/recheck_comparison/).

## 5. Phân tích hiệu năng và quyết định tối ưu

Không thay mã kernel, API hoặc cấu trúc OpenMP trong lần này. Bằng chứng hiện
tại chưa đủ để chứng minh một sửa đổi mới tốt hơn nhiễu giữa các phiên. Vì không
có implementation mới, **không có phép so sánh trước/sau tối ưu kernel**; bảng
ở mục 4 là chạy lại cùng implementation. Không gọi sự khác biệt đó là tối ưu.

| Thành phần | Nhận xét từ code/số đo | Quyết định |
|---|---|---|
| Tạo/kích hoạt team, barrier, join | Mỗi batch vào một region; runtime có thể tái dùng worker, không mặc định tạo OS thread mới mỗi lần. Chi phí này có thể chiếm tỷ trọng lớn ở batch nhỏ | Giữ một region/batch; không thêm persistent team khi chưa profiling |
| Cân bằng tải | Static chia đoạn chỉ số; uneven được sắp độ dài tăng dần nên đoạn cuối nhiều DP hơn. Dynamic chunk=1 có cơ sở giảm lệch tải | Giữ cả hai schedule, để số liệu quyết định theo workload |
| Chi phí lập lịch | Dynamic lấy từng reference, có overhead ngay cả với độ dài đồng đều | Không ép dynamic là mặc định tối ưu cho mọi dữ liệu |
| Cấp phát | Score-only có hai vector int cục bộ/cặp; vector score/error cấp một lần trước timer | Workspace riêng mỗi worker là hướng khảo sát, chưa triển khai vì thiếu bằng chứng lợi ích |
| Kiểm tra đầu vào | Batch đã kiểm tra, kernel vẫn tự kiểm tra DNA/scoring cho mỗi cặp; query bị duyệt lại N lần | Có công việc lặp O(m+n), nhưng DP O(mn) chiếm ưu thế ở chuỗi dài; chưa bỏ validation/API an toàn |
| Tái sử dụng/cache | Hai hàng DP O(min(m,n)), truy cập liên tiếp, dùng lại hàng qua swap; query/reference chỉ đọc | Giữ thiết kế này; chưa có hardware counters để kết luận bottleneck cache |
| Ghi kết quả | Chỉ một score/error slot mỗi cặp, không có append/critical trong vòng lặp | Có thể có false sharing ở phần tử gần nhau, không phải data race; chưa thêm padding khi chưa đo |

Ví dụ overhead thực tế: small_uniform phiên mới serial 0.3428 ms, static1
0.4134 ms, dynamic12 **0.3741 ms**, speedup **0.9163×**, tức chậm hơn serial.
Batch chỉ có 8 reference, team 12 có worker không có cặp để xử lý. Phiên gốc
static12 là 1.1569 ms so với serial 0.3041 ms, cũng phải giữ trên đồ thị.

Ví dụ không tăng tốc đơn điệu: medium_uniform dynamic8 8.9863 ms nhưng dynamic12
9.2375 ms; static8 10.4317 ms nhưng static12 11.4852 ms. Large_uniform dynamic8
63.2361 ms và dynamic12 62.6644 ms khá gần. Tăng số luồng không bảo đảm nhanh hơn;
efficiency dùng số luồng actual, không dùng số nhân vật lý.

Uneven medium, D4 11.2070 ms so S4 14.2092 ms; uneven large, D8 58.4682 ms so
S8 75.1607 ms. Kết quả phù hợp động cơ cân bằng tải, chưa cô lập riêng chi phí
lập lịch khỏi tác động runtime/hệ điều hành.

Tuning đã có từ tuần 3: khảo sát số luồng/schedule/workload, Release, warm-up,
median, kiểm tra actual và score; sửa protocol từ pilot quay vòng sang block
xáo trộn có warm-up tức thời. Pilot và source hỗ trợ cũ vẫn được giữ. Đây là
cải thiện phương pháp đo và tuning tham số, không phải đổi thuật toán.
`tuning_comparison.csv` cũ chọn cấu hình tốt nhất trong cùng phiên, mang tính
khảo sát và có thiên lệch lựa chọn; lần nghiệm thu này không xác nhận một cấu
hình tối ưu cố định cho ứng dụng.

## 6. Thử nghiệm bổ sung với FASTA sinh học thật

Tải qua **NCBI E-utilities EFetch**, không qua trang kiểm tra trình duyệt.
Kết nối trong sandbox ban đầu bị WinError 10013; truy cập API công khai bằng
công cụ được cấp quyền sau đó thành công. Không cài phần mềm hoặc dùng nguồn
không rõ xuất xứ.

| Accession.version | Nguồn chính thức | Độ dài | SHA-256 file gốc |
|---|---|---:|---|
| NC_001422.1 | [NCBI: Escherichia phage phiX174](https://www.ncbi.nlm.nih.gov/nuccore/NC_001422.1) | 5386 bp | `caa155c83967cc080f4f188f14f1f28c18d77448e87c3d00d3579ed348f4de2a` |
| NC_001330.1 | [NCBI: Enterobacteria phage alpha3](https://www.ncbi.nlm.nih.gov/nuccore/NC_001330.1) | 6087 bp | `a5660acbb30d5de50d79029f4511590c5dade0a00468f50766424cccaf3dde1a` |

Thời điểm tải UTC 09:42:19 và 09:42:20 ngày 12/11/2026. URL EFetch cụ thể,
header, kích thước file, alphabet và hash nằm trong
[provenance.json](../results/week3_validation/real_fasta/provenance.json).
Hai genome công khai chỉ chứa A/C/G/T, không dùng dữ liệu cá nhân. Lưu nguyên
byte FASTA nguồn. Query là phiX174 đầy đủ; references gồm phiX174 (self-match
control) và alpha3 đầy đủ. Không tự sinh base, không bỏ `N`, không cắt hoặc đổi
base; file references chỉ nối hai record và chuẩn hóa dấu phân cách cuối record.

Thực chạy CLI serial1 và static/dynamic với actual 1/2/4: **7 cấu hình thành công**,
stdout trùng nhau từng byte, thứ tự reference giữ nguyên:

| Reference | Serial | Static 1/2/4 | Dynamic 1/2/4 |
|---|---:|---:|---:|
| NC_001422.1 | 10772 | 10772 | 10772 |
| NC_001330.1 | 3541 | 3541 | 3541 |

Self-match đúng `2×5386=10772`. Đây là phép kiểm tra tính nhất quán phần mềm;
không suy ra độ chính xác sinh học, quan hệ tiến hóa hoặc tương đương BLAST.

Ba fixture lỗi riêng biệt (`N`, thiếu header, record rỗng), chạy qua serial,
parallel static4 và dynamic4: **9/9 bị từ chối đúng**, exit 1, stdout rỗng,
stderr có thông báo lỗi. Các fixture này được đặt tên rõ là đầu vào lỗi nhân
tạo; không ghi chúng là genome thật. Tổng CLI bổ sung: **16 PASS / 0 FAIL**.
Xem [verification.json](../results/week3_validation/real_fasta/verification.json).

Runner còn đo bổ sung chính bộ FASTA thật: 61.793.578 DP cells/batch, serial1,
static/dynamic2/4, 2 warm-up + 7 lần đo: **45/45 mẫu verified**, 5 summary, 0
thiếu/lệch actual. Serial median 129.5078 ms; static2 67.7338 ms; static4
67.7051 ms; dynamic2 69.6895 ms; dynamic4 68.2933 ms. Chỉ hai reference nên
tối đa hai cặp hoạt động đồng thời dù actual team 4. Truy vấn WMI chỉ đọc đã
trùng thời gian với phần bổ sung này; **không dùng các số đó để xếp hạng hiệu
năng hoặc thay thế benchmark chính**. Dữ liệu nằm trong
[real_benchmark](../results/week3_validation/real_benchmark/).

## 7. Rà soát an toàn đa luồng

Region duy nhất phân phối các cặp độc lập, không parallel hóa các ô DP phụ thuộc.

| Biến/trạng thái | Phạm vi và đồng bộ |
|---|---|
| Query, vector references, scoring | Đọc qua closure dùng chung; không sửa trong region. Caller phải giữ đầu vào còn sống và không sửa đồng thời |
| `run_one` | Closure const chia sẻ, không thay đổi closure khi gọi |
| Chỉ số `i` vòng `omp for` | Private theo quy tắc OpenMP, mỗi iteration chỉ được một worker xử lý |
| Hai hàng DP và `best` | Biến cục bộ của mỗi lần gọi score kernel, không static/global |
| `result.scores[i]`, `errors[i]` | Hai vector đã resize trước region, ghi vào các phần tử khác nhau, không `push_back`/resize trong worker, không phải vector<bool> |
| `actual` | Một worker ghi trong `omp single`; có barrier mặc định, caller đọc sau join |
| `scheduling`, `reference_count` | Shared chỉ đọc; các worker đi cùng một nhánh worksharing |
| Exception | Worker bắt mọi exception vào đúng slot, closure noexcept; sau join caller rethrow lỗi đầu tiên theo thứ tự input |

`default(none)` giới hạn biến shared trực tiếp; vùng song song kết thúc trước
đọc scores/errors. Không có reduction/append/critical vào DP; không chỉnh
global OpenMP ICV bằng `omp_set_num_threads` hoặc `omp_set_schedule`. Validate
options, DNA, range điểm và số reference trước dispatch. Lỗi cấp phát output
xảy ra trước region; lỗi cấp phát DP được bắt trong worker. Thứ tự trả kết
quả luôn theo FASTA.

23 test parallel có lặp 20 lần/schedule, heterogeneous lengths, ít reference
hơn worker, custom scoring, input/overflow lỗi, kiểm tra runtime settings và
các caller độc lập chạy đồng thời. Cả Debug/Release thực chạy đều PASS.

Đã thử `g++ -std=c++17 -fsanitize=thread` trên chương trình `int main(){return 0;}`:
linker báo **cannot find -ltsan**, exit 1. Không tìm thấy clang++, Valgrind hoặc
Dr. Memory trên PATH. **Không chạy được race detector** và không cài thêm.
Đây là probe thất bại do thiếu công cụ, không tính là một Unit Test FAIL và
không được gọi là “TSan PASS”. Xem
[race_detector_probe.txt](../results/week3_validation/checks/race_detector_probe.txt).
Phân tích code và test là bằng chứng hỗ trợ, không chứng minh không có race
ở mọi execution hoặc mọi runtime.

## 8. CSV, công thức và biểu đồ

Chạy validator Python độc lập với phép tính thống kê C++ trên cả `week3`,
`week3_recheck`, `week3_validation` và phần FASTA thật. Kiểm tra FASTA/metadata,
DP cells, Release/verified, run indices, số lần lặp, grouping theo actual,
checksum, dữ liệu hữu hạn/dương, và tính lại toàn bộ summary.

```text
T = median(7 execution_seconds của phase=measure)
S = median(serial cùng dataset) / T
E = S / actual_threads; E% = 100 × E
GCUPS = DP_cells / (T × 10^9)
population_stddev = sqrt(sum((Ti - mean)^2) / n)
```

Warm-up không vào summary. Không làm tròn CSV trước tính; lưu 17 chữ số.
Không chặn efficiency ở 100%: ví dụ small_uniform dynamic1 mới có S=1.0855,
E=108.55%; đó là độ nhạy giữa các block đo, không phải bằng chứng tăng tốc
siêu tuyến tính. Script hiện tại xác minh mọi nhóm đầy đủ, không ghép team
actual khác nhau; các đồ thị chỉ lấy nhóm đủ mẫu và actual=requested.

Ba thử nghiệm làm hỏng **bản sao trong build bị ignore**: đổi speedup, bỏ một
measurement, đặt verified=0. Validator từ chối cả ba: **3 PASS / 0 FAIL** về
khả năng phát hiện sai dữ liệu. Không đụng CSV gốc. Xem
[negative_csv_checks.json](../results/week3_validation/checks/negative_csv_checks.json).

| Biểu đồ | Dữ liệu/đơn vị và kết quả rà soát |
|---|---|
| execution_time.png | Median ms (seconds ×1000), bars=min/max, baseline serial của cùng dataset |
| speedup.png | Không đơn vị, Tserial/Tparallel, đường tham chiếu 1 |
| efficiency.png | Phần trăm, S/actual ×100, giữ nguyên cả giá trị >100 khi xuất hiện |
| scheduling_comparison.png | Tstatic/Tdynamic ở cùng dataset/actual; >1 có lợi cho dynamic |
| size_scaling.png | Log-log DP cells/batch và median seconds, so cùng mức luồng/schedule qua các cỡ |

Đã mở và xem cả 5 hình gốc và 5 hình phiên mới: tiêu đề, nhãn trục, legend
đọc được, không bị cắt; thể hiện cả trường hợp chậm hơn serial. Tái tạo 5 hình
gốc vào `checks/reproduced_original_figures/`: **5/5 PNG khớp SHA-256 từng byte**,
xác nhận hình lưu sẵn khớp CSV và script hiện tại. Không cần sửa trình bày.
Hình 200 dpi: ba hình 2600×2039 px, hai hình 2600×960 px, có thể chèn Word/slide;
khi trình chiếu nên dành đủ diện tích cho panel, tránh ép 6 panel vào vùng nhỏ.
Size-scaling chỉ có 3 quy mô, không dùng đường nối làm bằng chứng fit mô hình.

[figure_verification.json](../results/week3_validation/checks/figure_verification.json)
lưu kích thước/dpi/hash đối chiếu. Hình mới ở
[figures/](../results/week3_validation/figures/); các JSON validator ở `checks/`.

## 9. Build và toàn bộ kiểm thử

Đã chạy clean build, không chỉ dựa vào executable cũ:

```powershell
cmake --build build-debug --clean-first --parallel 4
ctest --test-dir build-debug --verbose
cmake --build build-release --clean-first --parallel 4
ctest --test-dir build-release --verbose
```

Cả hai build biên dịch/link đủ 24 bước sau clean, exit 0; log không có compiler
warning. CTest exit 0. Bảng dưới được đếm lại từ stdout verbose thực tế:

| Module/nhóm | Debug PASS/FAIL | Release PASS/FAIL |
|---|---:|---:|
| Smith-Waterman tuần tự | 17/0 | 17/0 |
| FASTA Reader | 12/0 | 12/0 |
| DNA Generator | 10/0 | 10/0 |
| Parallel API | 23/0 | 23/0 |
| Benchmark Math | 12/0 | 12/0 |
| Benchmark CLI | 12/0 | 12/0 |
| CLI tuần 1 | 36/0 | 36/0 |
| Parallel CLI tuần 2 | 35/0 | 35/0 |
| OpenMP runtime | 1/0 | 1/0 |
| **Tổng kiểm tra nội bộ** | **158/0** | **158/0** |
| **Nhóm CTest** | **9/0** | **9/0** |

17 test serial bao gồm 7225 cặp DNA ngắn so oracle độc lập, 250 cặp seeded dài
hơn, traceback/tọa độ/gap/tie và rolling rows. FASTA kiểm tra multiline/LF/CRLF,
duplicate ID, ký tự lỗi, I/O; generator kiểm tra seed, mapping và tái lập byte.
Benchmark Math kiểm tra median/mean/pstdev/formula, grouping actual và dataset
tái lập; các fixture thống kê nhân tạo chỉ dùng unit test, không đưa vào CSV
thực nghiệm. Test không đòi mọi cấu hình phải speedup >1.

Các kiểm tra bổ sung 16 CLI FASTA thật và 3 CSV âm tính ở trên được tính riêng,
không cộng vào 158 hoặc tự nhận là nhóm CTest mới. 594 + 45 mẫu benchmark verified
là số lần đo, không phải số Unit Test. Probe TSan thiếu thư viện được ghi là
giới hạn công cụ, không giấu dưới số test PASS.

Log lưu trực tiếp để sử dụng báo cáo:

- [Build Debug](../results/week3_validation/checks/build_debug.txt), [CTest Debug](../results/week3_validation/checks/ctest_debug.txt).
- [Build Release](../results/week3_validation/checks/build_release.txt), [CTest Release](../results/week3_validation/checks/ctest_release.txt).
- [Đếm test theo module](../results/week3_validation/checks/test_counts.json), [Console benchmark chính](../results/week3_validation/checks/benchmark_console.txt).

Lõi tuần 1–2 không đổi và các test cũ đều thực chạy: không thấy hồi quy chức năng.

## 10. Tái lập kiểm tra

Chạy từ thư mục project; chọn **tên output mới** thay cho các thư mục đã có.
Không chạy benchmark đồng thời với build, test hoặc vẽ hình.

```powershell
$env:OMP_DYNAMIC='FALSE'
$env:OMP_WAIT_POLICY='PASSIVE'
.\build-release\sw_benchmark.exe --output results/week3_next_validation --suite all --threads 1,2,4,8,12 --schedules static,dynamic --warmup 2 --runs 7
python scripts/plot_benchmark.py results/week3_next_validation
python scripts/compare_benchmark_runs.py results/week3 results/week3_next_validation --output results/week3_next_comparison
```

Kiểm tra các file đã lưu mà không ghi lại:

```powershell
python scripts/plot_benchmark.py results/week3 --validate-only
python scripts/plot_benchmark.py results/week3_recheck --validate-only
python scripts/plot_benchmark.py results/week3_validation --validate-only
python scripts/plot_benchmark.py results/week3_validation/real_benchmark --validate-only
```

Tái tải và kiểm tra FASTA thật, vào thư mục mới, cần mạng chỉ ở bước download:

```powershell
python scripts/check_real_fasta.py download results/week3_next_real
python scripts/check_real_fasta.py verify results/week3_next_real
.\build-release\sw_benchmark.exe --output results/week3_next_real_benchmark --query results/week3_next_real/query.fasta --query-id NC_001422.1 --references results/week3_next_real/references.fasta --dataset-id ncbi_phages --threads 2,4 --schedules static,dynamic --warmup 2 --runs 7
```

`verify` dùng lại FASTA/provenance đã lưu, không tải lại. Không ghi đè
verification có sẵn. Hash byte của FASTA NCBI có thể thay do header/wrapping
ở lần tải tương lai; kiểm tra accession.version, sequence và provenance thay
vì sửa hash cũ để che khác biệt. Không thay raw của phiên cũ.

## 11. File sửa/thêm trong lần nghiệm thu này

| Đường dẫn | Vai trò |
|---|---|
| `README.md` | Thêm liên kết nghiệm thu và ghi chú về độ ổn định hiệu năng |
| `docs/week3_final_audit.md` | Báo cáo mới này |
| `scripts/compare_benchmark_runs.py` | Validate hai phiên rồi so đủ cấu hình/dữ liệu/protocol; không ghi input, output phải mới |
| `scripts/check_real_fasta.py` | Tải hai accession từ NCBI, lưu provenance; kiểm tra CLI/score/FASTA lỗi, không đổi module C++ |
| `results/week3_validation/raw_benchmark.csv`, `summary_benchmark.csv`, `run_metadata.json` | Dữ liệu thực nghiệm độc lập mới |
| `results/week3_validation/environment.json`, `comparison/`, `recheck_comparison/` | Môi trường và đối chiếu đầy đủ |
| `results/week3_validation/figures/` | Năm PNG của phiên mới và JSON kiểm tra |
| `results/week3_validation/real_fasta/`, `real_benchmark/` | FASTA NCBI gốc, inputs dẫn xuất, provenance, stdout/stderr, fixture lỗi và số đo bổ sung |
| `results/week3_validation/checks/` | Log build/test, hash bảo toàn/source, CSV checks, detector probe, hình gốc tái tạo và manifest bàn giao |

File C++/header/CMake/test/plot script có sẵn không sửa trong audit; các thay đổi
tuần 3 đã tồn tại trong Git status trước khi bắt đầu vẫn được giữ. Các file tạm
probe/fixture hỏng nằm dưới build bị ignore. Không thêm dependency C++ hay cài
thư viện hệ thống.

## 12. Giới hạn còn lại

1. Không có race detector chạy được với toolchain hiện tại; chưa xác minh runtime
   khác hoặc mọi interleaving. Không chứng nhận “race-free tuyệt đối”.
2. Chưa kiểm soát affinity, nhiệt độ, xung và tải nền; biến động/thứ hạng giữa
   các phiên còn rõ, đặc biệt thời gian dưới 1 ms. Chưa có CI/statistical test
   hoặc profiling counters. Không khẳng định nguyên nhân vi kiến trúc.
3. Benchmark chính vẫn là bộ DNA giả lập có seed, chỉ ba quy mô/phân bố đã mô tả.
   Hai genome phage chỉ là supplement về tính nhất quán, có một self-control,
   không đại diện database gene lớn hay xác nhận độ chính xác sinh học.
4. Linear gap, chỉ A/C/G/T; parser cố ý từ chối `N`/IUPAC khác. Không có BLAST,
   wavefront, affine gap, CUDA, MPI hay tìm kiếm database theo nghĩa công cụ sinh học.
5. Chưa tối ưu workspace/validation lặp, chưa đo end-to-end hoặc peak RAM.
   Traceback đầy đủ vẫn O(mn) bộ nhớ; score-only O(min(m,n)).
6. Chưa có checkpoint Git tuần 3. ZIP là bản bảo toàn cục bộ, không thay thế
   lịch sử Git hay bản sao trên remote. Không tự commit hoặc push.

## 13. Quyết định nghiệm thu

**Đạt nghiệm thu nền tảng tuần 3, có thể bắt đầu tuần 4.** Các module tuần 1–2,
benchmark runner/math/CSV/đồ thị đều hoạt động qua kiểm thử thực tế. Dữ liệu,
protocol, công thức và điểm có thể tái lập; kết quả gốc bảo toàn, có thực nghiệm
độc lập và supplement NCBI có nguồn kiểm chứng.

Khi sử dụng số liệu cho Word/slide tuần 4 phải nêu đúng dataset, phiên đo,
actual threads và giới hạn. Giữ cả kết quả chậm, kết quả không ổn định và
thiếu race detector trong phần thảo luận. Điều kiện này là cách diễn giải
đúng bằng chứng hiện có, không phải cam kết speedup cố định. Không có hạng mục
chức năng tuần 4 nào được triển khai trong lần nghiệm thu này.
