# Giao diện thực hành PLC — 4 ô (HMI)

> **Logic HMI viết 100% bằng C++.** Trong UMG bạn **chỉ vẽ layout + đặt đúng TÊN widget** — KHÔNG cần
> kéo node nào trong Graph. C++ tự: gán ảnh 2 camera, cập nhật trạng thái, ghi log, và xử lý click nối dây.

## 1. Kiến trúc

```
PLC FX3U ──serial──► PlcBridge (C#) ──WebSocket {x,y,d}──► APlcLinkActor (C++)
                                                              │  bắn event
                                                              ▼
                                                          UHmiWidget (C++)  ◄── 2 render target (AHmiCaptureActor)
                                                          (WBP_HMI = layout 4 ô, không Graph)
```

- **C++ (`UHmiWidget`) lo:** tìm 2 render target và gán vào 2 Image; nghe event PLC; cập nhật StatusText;
  append dòng vào 2 ScrollBox log; chiếu ngược click trong ô board để nối dây.
- **WBP lo:** bố cục 4 ô + đặt đúng tên widget (bảng ở mục 3).

## 2. Spec JSON — bridge phải gửi (mỗi ~100 ms)

```json
{
  "x": [0,1,0,0,0,0,0,0],          // 8 công tắc X0..X7 (0/1)
  "y": [1,0,0,0,0,0,0,0],          // 8 đèn/output Y0..Y7 (0/1)
  "d": { "D0": 100, "D2": 5 }       // tùy chọn: thanh ghi cần theo dõi
}
```

- **Tương thích ngược:** vẫn nhận `{"lights":[...]}` (coi như `y`).
- **Chiều ngược lại (UE → bridge):** `{"toggle": n}` (phím 1–8 đảo đèn).
- Gửi trường nào cũng được; C++ tự lọc trùng.

### Bridge C# đọc gì từ PLC (HslCommunication)
- `X0..X7` → mảng `x`. **Địa chỉ X/Y của Mitsubishi là BÁT PHÂN** (X0–X7 rồi X10–X17). 8 kênh đầu không vướng.
- `Y0..Y7` → mảng `y`.
- `D0, D2, …` → object `d` (`ReadInt16`). Chỉ đọc D bạn quan tâm.

## 3. Dựng WBP_HMI — CHỈ layout, KHÔNG Graph

1. Content Browser → tạo folder **`/Game/UI`**.
2. Chuột phải → User Interface → **Widget Blueprint** → parent class = **HmiWidget** → đặt tên đúng **`WBP_HMI`**
   (controller tự nạp theo path `/Game/UI/WBP_HMI`).
3. Dựng cây widget (ô trái rộng hơn ô phải):

```
[Horizontal Box]  (Visibility = Self Hit Test Invisible)
 ├─ [Vertical Box]  Slot: Size = Fill, Fill = 2.0   ← CỘT TRÁI (rộng)
 │    ├─ Image      "LightsView"   (trên)  — Visibility = Hit Test Invisible
 │    └─ Image      "BoardView"    (dưới)  — Visibility = Hit Test Invisible   ★ ô tương tác
 └─ [Vertical Box]  Slot: Size = Fill, Fill = 1.0   ← CỘT PHẢI (hẹp)
      ├─ [Vertical Box]  (ô phải-trên)
      │    ├─ TextBlock  "StatusText"
      │    └─ ScrollBox  "ActionLog"     — để mặc định (Visible) cho cuộn được
      └─ ScrollBox      "ResultLog"      (ô phải-dưới) — Visible
```

### ⚠️ Bảng widget BẮT BUỘC (đặt đúng TÊN + đúng LOẠI — C++ tự bind theo tên)

| Tên widget  | Loại       | Ô          | Vai trò                         | Visibility |
|-------------|------------|------------|---------------------------------|------------|
| `LightsView`| Image      | trên-trái  | camera đèn (hiển thị)           | Hit Test Invisible |
| `BoardView` | Image      | dưới-trái  | bàn PLC **tương tác**           | **Hit Test Invisible** ★ |
| `StatusText`| TextBlock  | trên-phải  | dòng trạng thái X               | (tuỳ) |
| `ActionLog` | ScrollBox  | trên-phải  | log hành động                   | Visible |
| `ResultLog` | ScrollBox  | dưới-phải  | log kết quả                     | Visible |

- **Sai tên → widget đó bị bỏ qua** (khi Play sẽ có cảnh báo trong Output Log: `[HMI] Thiếu ...`).
- **`BoardView` phải là `Hit Test Invisible`** để cú click **xuyên** widget xuống game (nếu để `Visible`, widget nuốt click → KHÔNG nối dây được).
- **KHÔNG cần vào Graph.** C++ tự làm hết. Bạn có thể để Graph trống.

### Chỉnh nhanh (không sửa code) — WBP > Class Defaults (Details)
- `Log Text Color`, `Log Font Size`: màu/cỡ chữ các dòng log tạo lúc runtime.
- `Max Log Lines`: giới hạn số dòng log (0 = vô hạn).

> Tỷ lệ trái/phải: chỉnh 2 số **Fill** (đang `2.0 : 1.0` = trái gấp đôi phải). Kéo trực quan hoặc sửa số tuỳ ý.
> Ảnh camera méo do khác tỷ lệ khung? Để `BoardView` ~16:9 cho đẹp — **độ chính xác click vẫn đúng** dù ảnh bị giãn.

## 4. Đặt 2 AHmiCaptureActor trong level (phân biệt theo `View`)

Code nhận diện camera **theo property `View`**, không theo tên actor:

| `View`        | Ngắm vào        | Hiển thị ở ô | Ghi chú |
|---------------|-----------------|--------------|---------|
| **Board16x9** | bàn dây (cọc)   | `BoardView`  | ô TƯƠNG TÁC — camera này dùng để chiếu ngược click |
| **Lights16x3**| dải đèn H1–H8   | `LightsView` | thuần hiển thị |

- Render target tự tạo lúc Play (`RTWidth/RTHeight` = 0 → tự chọn 1280×720 cho Board, 1280×240 cho Lights).
  Có thể đặt tay `RTWidth/RTHeight` nếu muốn khung khác.

## 5. Cơ chế tương tác trong ô board

- **Nối dây (click-click):** click cọc A → dây hiện, đầu B bám con trỏ → click cọc B khác → nối xong
  (xanh nếu đúng cặp tag, đỏ nếu sai). Click chỗ trống / đúng cọc A → huỷ.
- **Một cọc cắm nhiều dây:** A→B và A→C dùng chung điểm A đều được.
- **Alt + click = xoá dây:** giữ Alt rồi click vào đầu dây (hoặc vào cọc có dây). Muốn xoá đúng 1 dây
  trong nhiều dây chung cọc → Alt+click vào **đầu riêng** của dây đó.
- **Phím 1–8:** đảo đèn (gửi `{"toggle":n}` xuống PLC) — độc lập với chuột.

> Kỹ thuật: `AHmiCaptureActor::DeprojectUVToWorldRay` + `UHmiWidget::GetBoardRayUnderCursor` chiếu điểm click
> trong `BoardView` thành tia thế giới; `AWiringPlayerController` trace theo tia đó. Ngoài ô board, click dùng
> deproject viewport chính (nên nối dây vẫn chạy cả khi CHƯA có WBP_HMI — tiện test trong editor).

### Nếu click bị lệch (lần đầu dễ gặp, chỉ sửa 1 dòng)
Cơ chế chiếu ngược phụ thuộc hướng camera, có thể bị lật gương:
- Click trái mà dây bắt cọc **phải** (lật ngang) → đổi dấu `NdcX` trong `HmiCaptureActor.cpp::DeprojectUVToWorldRay`.
- Click trên mà bắt cọc **dưới** (lật dọc) → đổi dấu `NdcY`.
- Lệch đều một khoảng → camera đang để Orthographic; đổi về Perspective (hoặc báo dev xử riêng).

## 6. Build & chạy

1. Regenerate VS project files nếu thêm file C++ mới → build **Development Editor / Win64** (module đã có `UMG`).
2. GameMode đã là `AWiringGameMode` → `AWiringPlayerController` tự tạo `WBP_HMI` khi Play (sau ~0.2 s).
3. Chạy bridge (đã gửi `x/y/d`) → bấm Play.
   - 2 ô trái hiện camera (không cần bridge).
   - Lật công tắc thật → ô phải-trên đổi trạng thái + thêm dòng "Bật X1".
   - Ladder ra đèn/giá trị → ô phải-dưới thêm "Đèn 1 BẬT", "Gán D0 = …".
   - Nối/tháo dây ngay trong ô dưới-trái bằng chuột.

## 7. Lưu ý

- **Nhãn mặc định:** công tắc `X1..X8`, đèn `Đèn 1..8`. Đổi số kênh: `NumChannels` trên `APlcLinkActor`
  + hàm `BuildInputStatusString()` / format trong `HmiWidget.cpp`.
- **Log kết quả** hiện báo theo thiết bị đổi. Muốn câu chữ "X1 → bật Đèn 1" cần bảng quy tắc ánh xạ (chưa làm).
- **Công tắc ảo bấm được trong Unreal:** chưa làm — xem plan trong `VIEC_CAN_LAM_TIEP.md`.
