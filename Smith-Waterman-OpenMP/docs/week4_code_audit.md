# Nhật ký nghiệm thu mã nguồn tuần 4

Ngày: 12/11/2026. Chỉ thực hiện code, build, demo, đóng gói và kiểm thử.

## Checkpoint và phạm vi thay đổi

- Trước sửa: `main`, HEAD `2275c454e0dbdcae3a4743c3d52ce239aa080e4b`, đồng bộ
  `origin/main`; working tree sạch. Remote: `https://github.com/thanhdat0502/Nhom6_HPC.git`.
- Chạy lại baseline: Debug/Release đều **9/9 nhóm CTest**, 158 PASS / 0 FAIL.
- Đọc lại source/header, recurrence, rolling rows, traceback, FASTA, generator,
  CLI, OpenMP và benchmark. Không tìm thấy lỗi cần thay kernel/điểm/API/định dạng
  đầu ra cũ. CLI vẫn chỉ có scoring mặc định; API score/batch vẫn hỗ trợ custom scoring.
- Không sửa các file C++ trong `src/`, header trong `include/`, công thức benchmark
  hoặc script vẽ. Không đo lại benchmark lớn, không thay dữ liệu tuần 1–3.
- Không commit/push, không cài phần mềm; không thêm CUDA/MPI/BLAST/wavefront/affine gap.

## Lỗi thực tế đã sửa

CMake gọi Git khi giao source không kèm `.git`: Git có trên PATH nhưng lệnh thất
bại, làm revision rỗng và dirty=false, đồng thời in `fatal: not a git repository`.
Đã tạo regression test và chạy trước sửa: **FAIL đúng tại metadata**. Sau sửa,
chỉ dùng kết quả Git khi lệnh thành công; trường hợp không có repo dùng revision
`unknown`, dirty=true (không xác nhận sạch), không in fatal từ metadata tùy chọn.

Đây là sửa thông tin build/provenance, không ảnh hưởng compute kernel hoặc
cách đo/thống kê. Regression `cmake_metadata` chạy bằng source copy, chặn Git
leo lên repo cha bằng `GIT_CEILING_DIRECTORIES`; không chỉ giả định source ZIP
đã tách khỏi Git.

## Demo và đóng gói

- `scripts/demo_week4.ps1`: chạy score 8, traceback score 14, FASTA mẫu, serial,
  parallel default/static/dynamic; so toàn bộ stdout để xác minh điểm và thứ tự.
  Chọn tối đa 4 luồng theo giới hạn CLI, in actual; đã thử giới hạn 1 luồng.
  Thiếu exe/FASTA trả exit 1, không báo thành công giả; thiếu exe có lệnh build.
- `scripts/native_process.ps1`: gọi native process với CRT argument quoting,
  nhận stdout/stderr riêng, đọc đồng thời và có timeout. Tránh PowerShell 5.1
  hiểu `--stats` stderr hợp lệ thành lỗi; đã thử argv rỗng, quote/backslash và
  đường dẫn có khoảng trắng.
- `scripts/package_release.ps1`: chỉ nhận single-config Release; đọc compiler
  từ CMake cache (kể cả cache type UNINITIALIZED), dùng objdump cùng prefix để
  duyệt imports đệ quy. Không tải/cài toolchain, không lấy DLL tùy tiện từ PATH.
- Runtime thực tế: `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libgomp-1.dll`,
  `libwinpthread-1.dll`. Windows/API-set/UCRT do OS cung cấp. Gói kèm notices
  GCC Runtime Library Exception/GPL, winpthreads và MinGW-w64 từ toolchain.
- Gói có hai exe, DLL cần thiết, FASTA mẫu, demo, source C++/CMake/tests/scripts,
  hướng dẫn `README_RUN.md`, dependency graph và SHA-256 `package_manifest.json`.
  Không kèm `.git`, build cache, compiler hay dữ liệu benchmark lịch sử.
- Destination/ZIP phải mới; không ghi đè/xóa bản cũ. Lỗi giữ thư mục mới chưa
  hoàn tất để chẩn đoán. `/dist/` được thêm vào `.gitignore`.
- Packager thực chạy align/search/demo và benchmark help bằng PATH chỉ có Windows.
  CTest Release giải nén ZIP ra thư mục tạm ngoài repo, chạy demo và benchmark
  smoke, đối chiếu inventory hash. **Đã thực chạy thành công**, không chỉ kiểm
  tra file ZIP tồn tại.

Lệnh sử dụng và yêu cầu môi trường nằm trong [README](../README.md#chạy-demo-và-đóng-gói--tuần-4).
Gói bàn giao: `dist/Smith-Waterman-OpenMP-Week4-Release-20261112.zip`.
Thông tin kiểm tra gói cuối: [package_verification.json](../results/week4_code/package_verification.json).

## Kiểm thử cuối

Clean build Debug và Release: mỗi build 26 bước biên dịch/link, exit 0,
**không có compiler warning**; GCC 16.2.0 UCRT64, C++17/OpenMP. Release dùng
`-O3 -DNDEBUG`, không thay flags có thể giảm độ chính xác.

| Hạng mục | Debug | Release |
|---|---:|---:|
| Nhóm CTest trước tuần 4 | 9/9 | 9/9 |
| Nhóm CTest sau tuần 4 | **12/12** | **12/12** |
| Kiểm tra nội bộ cũ | 158 PASS / 0 FAIL | 158 PASS / 0 FAIL |
| Allocation failure trong batch | 3 PASS / 0 FAIL | 3 PASS / 0 FAIL |
| Metadata source không có Git | 1 PASS / 0 FAIL | 1 PASS / 0 FAIL |
| Demo/đóng gói | 8 PASS / 0 FAIL | 15 PASS / 0 FAIL |
| **Tổng kiểm tra nội bộ** | **170 PASS / 0 FAIL** | **177 PASS / 0 FAIL** |

Ba nhóm mới:

1. `parallel_exceptions`: fault injection vào cấp phát hàng DP trong executable
   test riêng. Serial/static/dynamic phải hoàn tất các iteration, join, rethrow
   lỗi đầu tiên theo input về caller, rồi chạy lại thành công sau khi hết lỗi.
   Không thêm fault hook hoặc mutable global state vào thư viện production.
2. `cmake_metadata`: regression cho source handoff không có repository.
3. `delivery_scripts`: argv, demo từ CWD khác, thiếu exe/FASTA, giới hạn luồng,
   thiếu cache/Debug; Release thêm tạo ZIP, không ghi đè folder/ZIP, DLL manifest,
   giải nén chạy PATH tối giản, benchmark smoke, hash và loại cache khỏi source.

Release có thêm 7 ca kiểm tra gói vì Debug bị từ chối đóng gói theo thiết kế.
Windows PowerShell integration chỉ đăng ký trên Windows có PowerShell; test
metadata dùng Ninja. Số 12 nhóm là của môi trường Windows/Ninja hiện tại.

Các kiểm tra tuần 1–3 vẫn thực chạy đầy đủ: chuỗi rỗng/invalid, FASTA lỗi,
heterogeneous references, mức luồng 1/2/4/8/12, actual/reduced team, lặp schedule,
caller đồng thời, overflow, seed reproducibility, CLI stdout/stderr/exit,
benchmark math/CSV/no-overwrite. Không thêm assertion bắt buộc speedup >1.

Bản **source đã lấy từ ZIP ngoài repo** cũng configure, build Release và chạy
**12/12 CTest thành công**, kể cả chu trình đóng gói. Không dùng source tại chỗ
để thay cho phép kiểm tra source handoff này.

Log thực tế tại [results/week4_code](../results/week4_code/):

- `baseline_ctest_debug.txt`, `baseline_ctest_release.txt`.
- `build_debug.txt`, `ctest_debug.txt`, `build_release.txt`, `ctest_release.txt`.
- `source_configure.txt`, `source_build.txt`, `source_ctest.txt`, `test_counts.json`.
- `original_results_sha256.json`, `benchmark_preservation.json`, `packaged_smoke_csv_validation.json`.

## Bảo toàn và giới hạn

- Đã lưu SHA-256 trước sửa cho **223 file kết quả cũ**; kiểm tra lại cuối phiên
  phải khớp toàn bộ. Chi tiết ở `benchmark_preservation.json`; chỉ thêm thư mục
  log tuần 4 mới, không ghi lại CSV/PNG/FASTA cũ.
- Các phép đo mới chỉ là smoke rất nhỏ để kiểm tra runner, không dùng làm kết
  luận hiệu năng hay thay các kết quả tuần 3. Chi phí DLL/khả năng chạy được trên
  máy khác không được suy ra từ speedup.
- Rà soát OpenMP: input chỉ đọc, loop index/DP rows riêng, output/error slot
  riêng và đã cấp trước region; single ghi actual, caller đọc sau join;
  exception không thoát qua region. Test lỗi cấp phát bổ sung bằng chứng cho
  nhánh catch/rethrow; không thay thế race detector. **Chưa có xác minh bằng
  race detector**, không tuyên bố an toàn luồng tuyệt đối.
- Bản ZIP nhắm Windows x86-64/UCRT tương thích, đã thử trên chính máy build với
  PATH tối giản và thư mục tách biệt; **chưa thử máy Windows thứ hai**. Không
  cam kết chạy trên mọi hệ điều hành/CPU. Muốn build source vẫn cần toolchain.
- Những giới hạn thuật toán giữ nguyên: DNA A/C/G/T, linear gap, traceback
  O(mn) bộ nhớ, score-only O(min(m,n)); hiệu năng còn phụ thuộc dataset/runtime.

File sửa: `CMakeLists.txt`, `.gitignore`, `README.md`. File mới: ba script trong
`scripts/`, ba test trong `tests/`, nhật ký này và log `results/week4_code/`.
Không tạo tài liệu Word/slide/video/nội dung học thuật. Không phát hiện lỗi
nghiêm trọng còn mở trong phạm vi kiểm tra; code tuần 4 sẵn sàng demo/bàn giao.
