# Giao diện thực hành PLC — 4 ô (HMI)

> Phần C++ đã xong. Còn lại: (1) mở rộng **PlcBridge (C#)** gửi thêm dữ liệu theo spec JSON bên dưới,
> (2) dựng **WBP_HMI** (layout) trong editor, (3) đặt **2 AHmiCaptureActor** trong level.

## 1. Kiến trúc

```
PLC FX3U ──serial──► PlcBridge (C#) ──WebSocket {x,y,d}──► APlcLinkActor (C++)
                                                              │  bắn event
                                                              ▼
                                                          UHmiWidget  ◄── 2 render target (AHmiCaptureActor)
                                                          (WBP_HMI = layout 4 ô)
```

- **C++ lo:** đọc JSON, giữ trạng thái X/Y/D, bắn event, format chuỗi log/status, tạo render target.
- **WBP lo:** bố cục 4 ô + hiển thị (Image, Text, ScrollBox).

## 2. Spec JSON — bridge phải gửi (mỗi ~100 ms)

Một object, có thể gồm các trường (gửi trường nào cũng được, thiếu thì bỏ qua):

```json
{
  "x": [0,1,0,0,0,0,0,0],          // 8 công tắc X0..X7 (0/1)
  "y": [1,0,0,0,0,0,0,0],          // 8 đèn/output Y0..Y7 (0/1)
  "d": { "D0": 100, "D2": 5 }       // tùy chọn: thanh ghi cần theo dõi
}
```

- **Tương thích ngược:** vẫn nhận `{"lights":[...]}` (coi như `y`).
- **Chiều ngược lại (UE → bridge)** giữ nguyên: `{"toggle": n}`.
- Chỉ cần gửi khi có thay đổi cũng được (C++ tự lọc trùng).

### Bridge C# cần đọc gì từ PLC (HslCommunication)
- `X0..X7` (công tắc) → mảng `x`. **Lưu ý: địa chỉ X/Y của Mitsubishi là BÁT PHÂN** (X0–X7 rồi X10–X17). 8 kênh đầu là X0..X7 nên không vướng, nhưng nếu mở rộng >8 thì nhớ nhảy X10.
- `Y0..Y7` (đèn) → mảng `y`.
- `D0, D2, …` (thanh ghi) → object `d` (đọc `ReadInt16`). Chỉ đọc những D bạn quan tâm.

> "Đồng bộ với GX Works" = đọc trạng thái thiết bị thật trên PLC mà ladder (nạp bằng GX Works) đang chạy.
> Ta không đọc trực tiếp phần mềm GX Works.

## 3. Dựng WBP_HMI (layout 4 ô)

1. Content Browser → tạo folder **`/Game/UI`**.
2. Chuột phải → User Interface → **Widget Blueprint** → khi hỏi parent class chọn **HmiWidget** (class C++) → đặt tên đúng **`WBP_HMI`** (controller tự nạp theo path này).
3. Bố cục (gợi ý dùng Horizontal Box phủ full màn, chia 2 cột 50/50):
   - **Cột trái** (Vertical Box):
     - `Image` **BoardView** (trên, tỷ lệ 16:9).
     - `Image` **LightsView** (dưới, tỷ lệ 16:3).
   - **Cột phải** (Vertical Box):
     - **Ô phải-trên:** `TextBlock` **StatusText** + `ScrollBox` **ActionLog**.
     - **Ô phải-dưới:** `ScrollBox` **ResultLog**.
4. Mở **Graph**, cài 4 event (Override → các event do C++ expose):
   - **OnRenderTargetsReady:** với mỗi Image, gọi *Set Brush from Texture* (hoặc tạo brush) dùng `BoardRT` cho BoardView và `LightsRT` cho LightsView (2 biến này C++ đã gán sẵn).
   - **OnStatusUpdated(StatusText):** `StatusText.SetText`(tham số).
   - **OnActionLog(Line):** tạo 1 `TextBlock` = Line → `ActionLog.AddChild` (cuộn xuống nếu muốn).
   - **OnResultLog(Line):** tương tự, AddChild vào `ResultLog`.

> Nếu Image không nhận trực tiếp Render Target làm brush, tạo 1 vật liệu UI mẫu (Material Domain = User Interface) có tham số Texture, rồi set render target vào tham số đó.

## 4. Đặt 2 AHmiCaptureActor trong level

- Kéo **AHmiCaptureActor** vào level, đặt **View = Board16x9**, đặt vị trí/ô góc **nhìn xuống bàn** (giống CameraActor cũ).
- Kéo cái thứ 2, **View = Lights16x3**, ngắm vào **hàng đèn H1–H8**; chỉnh *Field of View* và vị trí của component **Capture** cho khít dải đèn (render target đã là 16:3 nên phần trên/dưới tự bị cắt).
- Không cần gán render target thủ công — actor tự tạo lúc Play, widget tự tìm theo `View`.

## 5. Build & chạy

1. Regenerate VS project files (vì thêm file C++ mới) → build (đã thêm module `UMG`).
2. GameMode đã là `AWiringGameMode` → controller `AWiringPlayerController` tự tạo `WBP_HMI` khi Play (sau ~0.2s).
3. Chạy bridge (đã gửi `x/y/d`) → bấm Play. Lật công tắc thật trên PLC → ô phải-trên đổi trạng thái + thêm dòng "Bật X1"; ladder chạy ra đèn/giá trị → ô phải-dưới thêm "Đèn 1 BẬT", "Gán D0 = …".

## 6. Lưu ý quan trọng

- **HMI này là màn hình GIÁM SÁT** (2 ô trái là ảnh từ SceneCapture, không phải viewport tương tác). Khi HUD phủ full màn, thao tác **kéo dây bằng chuột** (tính năng AWire) sẽ bị che. Đây là 2 chế độ khác nhau — nếu cần vừa giám sát vừa cắm dây, mình sẽ làm thêm nút bật/tắt HUD hoặc nhúng 1 ô tương tác sau.
- Nhãn mặc định: công tắc `X1..X8` (từ x[0..7]), đèn `Đèn 1..8` (từ y[0..7]). Muốn đổi nhãn/đổi số kênh: sửa `NumChannels` trên `APlcLinkActor` và hàm format trong `HmiWidget.cpp` / `BuildInputStatusString`.
- Log kết quả hiện báo theo thiết bị đổi (đèn Y, thanh ghi D). Nếu muốn câu chữ "thông minh" hơn (vd "X1 → bật Đèn 1") thì cần khai báo bảng quy tắc — nói mình nếu cần.
