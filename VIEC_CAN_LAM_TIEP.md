# Việc cần làm tiếp (PLC project)

Phần C++ + cấu hình đã xong. Đây là các bước còn lại để chạy được, **theo đúng thứ tự**.
Chi tiết HMI xem [`HUONG_DAN_HMI.md`](./HUONG_DAN_HMI.md); tổng quan dự án xem [`README.md`](./README.md).

---

## Bước 0 — Môi trường build (.NET)

- [ ] Kiểm tra: `dotnet --list-sdks` phải có **8.0.x**.
- [ ] Nếu build báo lỗi NuGet `Microsoft.Build / Microsoft.IO.Redist … .NETFramework … exited with code 6`:
  tạo file **`global.json`** ở thư mục gốc `PLC` với nội dung:
  ```json
  { "sdk": { "version": "8.0.0", "rollForward": "latestMinor" } }
  ```
  *(Hiện chưa có file này.)*

## Bước 1 — Build C++

- [ ] Chuột phải `PLC.uproject` → **Generate Visual Studio project files**.
- [ ] Build **Development Editor / Win64** trong Visual Studio (hoặc double-click `.uproject` → **Yes** để rebuild).
- [ ] Nếu lỗi: copy **~30 dòng đầu** của log compiler (phần UHT/compile, không phải IntelliSense) để nhờ sửa.

> Phải build xong thì editor mới có các class `HmiWidget`, `HmiCaptureActor`, `ATerminal`… cho các bước sau.

## Bước 2 — Migrate asset (giữ đúng đường dẫn)

- [ ] Mang sang project này: model bàn PLC, map, và các asset sau **đúng đường dẫn** để code C++ tự tìm thấy:
  - [ ] `/Game/Inputs/IMC_Wiring`, `/Game/Inputs/IA_Grab`
  - [ ] `/Game/Data/M_Wire_Green`, `/Game/Data/M_Wire_Red`
- [ ] Nếu để khác đường dẫn: lúc Play chỉ là warning "Failed to find object" — gán lại bằng tay trong Details/WBP được.

## Bước 3 — Dựng giao diện HMI (`WBP_HMI`)

- [ ] Tạo folder **`/Game/UI`**.
- [ ] Tạo **Widget Blueprint**, parent class = **`HmiWidget`**, đặt tên đúng **`WBP_HMI`** (controller tự nạp theo path `/Game/UI/WBP_HMI`).
- [ ] Layout 4 ô (xem chi tiết trong `HUONG_DAN_HMI.md` mục 3):
  - [ ] Trái‑trên: `Image` (16:9) — bind brush từ `BoardRT`.
  - [ ] Trái‑dưới: `Image` (16:3) — bind brush từ `LightsRT` *(crop xem mục bên dưới)*.
  - [ ] Phải‑trên: `TextBlock` (status) + `ScrollBox` (action log).
  - [ ] Phải‑dưới: `ScrollBox` (result log).
- [ ] Cài 4 event (Override): `OnRenderTargetsReady`, `OnStatusUpdated`, `OnActionLog`, `OnResultLog`.

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

## Quyết định còn để mở (cần chốt khi làm tới)

- **HMI giám sát vs cắm dây:** HUD phủ full màn sẽ che thao tác kéo dây bằng chuột. Nếu muốn cả hai trên cùng màn hình → cần thêm nút bật/tắt HUD hoặc nhúng 1 ô tương tác. (Chưa làm.)
- **Câu chữ log kết quả:** hiện báo theo thiết bị đổi ("Đèn 1 BẬT", "Gán D0 = 100"). Muốn diễn giải kiểu "X1 → bật Đèn 1" thì cần khai báo bảng quy tắc ánh xạ. (Chưa làm.)
- **Số kênh / nhãn:** mặc định 8 (X1–X8, Đèn 1–8). Đổi ở `NumChannels` (`APlcLinkActor`) và hàm format trong `HmiWidget.cpp` / `BuildInputStatusString()`.
