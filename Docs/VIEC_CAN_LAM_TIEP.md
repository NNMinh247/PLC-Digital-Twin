# Việc cần làm tiếp (PLC project)

Phần C++ + cấu hình đã xong. Đây là các bước còn lại để chạy được, **theo đúng thứ tự**.
Chi tiết HMI xem [`HUONG_DAN_HMI.md`](./HUONG_DAN_HMI.md); tổng quan dự án xem [`README.md`](./README.md).

---

## Bước 0 — Môi trường build (.NET) ✅ đã xử lý

- [x] `global.json` (ghim .NET 8) **đã có sẵn** ở gốc project → tránh lỗi NuGet `... exited with code 6`
  khi máy có .NET 9/10. Nội dung: `{ "sdk": { "version": "8.0.0", "rollForward": "latestMinor" } }`.
- [ ] Máy đích chỉ cần có **.NET 8 SDK** (`dotnet --list-sdks` có `8.0.x`). Engine UE 5.6 cũng kèm .NET 8 bundled.

## Bước 1 — Build C++ ✅ đã xác nhận sạch

- [x] Build **Development Editor / Win64** đã chạy sạch (exit 0) — có sẵn `Binaries/Win64/UnrealEditor-PLC.dll`.
- [ ] Nếu build lại trên máy khác: chuột phải `PLC.uproject` → **Generate Visual Studio project files** → build,
  hoặc double-click `.uproject` → **Yes**. Lỗi thì copy **~30 dòng đầu** log compiler (UHT/compile) để nhờ sửa.

> Cảnh báo "MSVC 14.5x is not a preferred version (14.38)" chỉ là warning, không chặn build.

## Bước 2 — Migrate asset (giữ đúng đường dẫn)

- [ ] Mang sang project này: model bàn PLC, map, và các asset sau **đúng đường dẫn** để code C++ tự tìm thấy:
  - [ ] `/Game/Inputs/IMC_Wiring`, `/Game/Inputs/IA_Grab`
  - [ ] `/Game/Data/M_Wire_Green`, `/Game/Data/M_Wire_Red`
- [ ] Nếu để khác đường dẫn: lúc Play chỉ là warning "Failed to find object" — gán lại bằng tay trong Details/WBP được.

## Bước 3 — Dựng giao diện HMI (`WBP_HMI`) — CHỈ layout, KHÔNG Graph

> Logic viết 100% C++. Trong UMG chỉ vẽ layout + đặt **đúng tên** widget; **không cần kéo node Graph nào**.
> Chi tiết đầy đủ: `HUONG_DAN_HMI.md` mục 3.

- [ ] Tạo folder **`/Game/UI`**.
- [ ] Tạo **Widget Blueprint**, parent = **`HmiWidget`**, tên đúng **`WBP_HMI`** (controller tự nạp theo path).
- [ ] Layout 4 ô, **cột trái rộng hơn cột phải** (Horizontal Box, Fill 2.0 : 1.0). Đặt widget đúng TÊN + LOẠI:
  - [ ] Trên‑trái: `Image` **`LightsView`** — Visibility = Hit Test Invisible.
  - [ ] Dưới‑trái: `Image` **`BoardView`** — **Hit Test Invisible** (ô bàn PLC tương tác).
  - [ ] Trên‑phải: `TextBlock` **`StatusText`** + `ScrollBox` **`ActionLog`**.
  - [ ] Dưới‑phải: `ScrollBox` **`ResultLog`**.
- [ ] KHÔNG cần vào Graph. (Sai tên widget → khi Play có cảnh báo `[HMI] Thiếu ...` trong Output Log.)
- [ ] (Tuỳ chọn) Chỉnh màu/cỡ chữ log ở **WBP > Class Defaults**: `Log Text Color`, `Log Font Size`, `Max Log Lines`.

## Bước 4 — Đặt 2 `AHmiCaptureActor` trong level

- [ ] 1 cái **View = Board16x9**, đặt nhìn xuống bàn điều khiển.
- [ ] 1 cái **View = Lights16x3**, ngắm vào dải đèn H1–H8.
- [ ] **Cách crop ô 16:3 cho vừa ý** (không cần đụng camera): trên capture đèn đặt `RTWidth=1280, RTHeight=720` (render đủ 16:9), rồi trong `WBP_HMI` cho ô trái‑dưới **Clipping = Clip to Bounds** và phóng/kéo `Image` để chỉ chừa dải đèn.

## Bước 5 — Đặt actor gameplay trong level

- [ ] **`APlcLinkActor`** — đặt **1 cái** (đầu mối WebSocket; nếu cần đổi IP sửa `ServerUrl`).
- [ ] **`ATerminal`** — các cọc; gán `TerminalTag` + `TargetPartnerTag` (tag đã khai trong `DefaultGameplayTags.ini`). *Nếu mang BP_Terminal cũ sang thì reparent về `Terminal`.*
- [ ] **`APlcLight`** — các đèn; đặt `LightIndex` = 0..7; material slot 0 phải có Vector Parameter tên **`EmissiveColor`**.
- [ ] World Settings: GameMode để trống (đã có `GlobalDefaultGameMode` trong config) hoặc đặt = `AWiringGameMode`.

## Bước 6 — Mở rộng PlcBridge (C#)

- [ ] Sửa bridge gửi JSON `{"x":[...], "y":[...], "d":{...}}` (spec đầy đủ trong `HUONG_DAN_HMI.md` mục 2).
  - [ ] Đọc `X0..X7` (công tắc) → `x`.
  - [ ] Đọc `Y0..Y7` (đèn) → `y`.
  - [ ] Đọc các thanh ghi `D` cần theo dõi → `d`.
- [ ] Giữ chiều nhận `{"toggle":n}` như cũ.

## Bước 7 — Chạy & kiểm thử

- [ ] Nạp ladder bằng GX Works2 (`LD M[n] / OUT Y[n]`…), **đóng GX Works2**.
- [ ] Chạy PlcBridge → thấy log lắng nghe `ws://...:8080`.
- [ ] **Play** trong editor.
  - [ ] 2 ô trái hiện camera (không cần bridge cho bước này).
  - [ ] Lật công tắc thật → ô phải‑trên đổi trạng thái + thêm dòng "Bật X1".
  - [ ] Ladder ra đèn/giá trị → ô phải‑dưới thêm "Đèn 1 BẬT", "Gán D0 = …".

---

## Đã đổi: HMI tương tác (Cách A) — cơ chế nối dây click‑click

- **Bố cục 4 ô** (ô trái rộng hơn ô phải): trên‑trái = camera đèn (thuần hiển thị); **dưới‑trái = bàn PLC tương tác được** (render target `BoardRT` + chiếu ngược click); phải‑trên = log trạng thái; phải‑dưới = log kết quả.
- **Nối dây kiểu click‑click** (thay cơ chế kéo‑thả cũ): click cọc A → tạo dây (ghim đầu A), đầu B bám con trỏ → click cọc B khác → nối xong. Click chỗ trống/đúng cọc A → huỷ. **Một cọc cắm được nhiều dây** (A→B và A→C). Không kéo dây từ ngoài vào.
- **Alt + click = xoá dây**: giữ Alt rồi click vào đầu dây (hoặc vào cọc có dây) để xoá dây đó. Xử lý trong `AWiringPlayerController::DeleteWireUnderCursor` (multi‑trace, ưu tiên trúng dây; trượt thì xoá 1 dây nối vào cọc trúng).
- **Tương tác trong ô board (Cách A):** `AHmiCaptureActor::DeprojectUVToWorldRay` + `UHmiWidget::GetBoardRayUnderCursor` chiếu điểm click trong ô `BoardView` thành tia thế giới; `AWiringPlayerController` trace theo tia đó. Yêu cầu WBP có Image tên **`BoardView`** đặt **Visibility = Hit Test Invisible** (để click xuyên xuống game).

## Kế hoạch (chưa triển khai)

- **Công tắc ảo bấm được trong Unreal:** thêm actor công tắc (click được) trong ô board → gọi `APlcLinkActor::ToggleLight(n)` (tái dùng cơ chế `{"toggle":n}` có sẵn). Hiện tại vẫn bật/tắt bằng **phím số 1–8**. (Chưa làm — làm sau khi nối dây ổn.)

## Quyết định còn để mở (cần chốt khi làm tới)

- **Câu chữ log kết quả:** hiện báo theo thiết bị đổi ("Đèn 1 BẬT", "Gán D0 = 100"). Muốn diễn giải kiểu "X1 → bật Đèn 1" thì cần khai báo bảng quy tắc ánh xạ. (Chưa làm.)
- **Câu chữ log kết quả:** hiện báo theo thiết bị đổi ("Đèn 1 BẬT", "Gán D0 = 100"). Muốn diễn giải kiểu "X1 → bật Đèn 1" thì cần khai báo bảng quy tắc ánh xạ. (Chưa làm.)
- **Số kênh / nhãn:** mặc định 8 (X1–X8, Đèn 1–8). Đổi ở `NumChannels` (`APlcLinkActor`) và hàm format trong `HmiWidget.cpp` / `BuildInputStatusString()`.
