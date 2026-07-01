# Bàn giao dự án — Tín hiệu đèn PLC (cập nhật 2026-07-01)

Tài liệu này dành cho **người tiếp nhận tiếp theo**. Đọc cùng với [`README.md`](../README.md)
(kiến trúc tổng thể) và [`HUONG_DAN_HMI.md`](./HUONG_DAN_HMI.md) (dựng HMI).

---

## 1. Trạng thái hiện tại

- **Nhánh:** `MVP`. **Code lần này đã commit** tại `b0db335 "update light index, PLC simulator"`.
- **Build:** Development Editor / Win64 **xác nhận sạch** (UHT `WarningsAsErrors` vẫn qua).
- **Chưa commit (asset, do chỉnh trong editor):**
  - Đổi thư mục HDRI: xoá `Content/DHRI/...` → thêm `Content/HDRI/...` (dùng cho SkyLight).
  - Vài file trong `Content/__ExternalActors__/Map/...` (chỉnh level: ánh sáng/đặt actor).
  - → Người nhận nên **commit các asset này** để đồng bộ level + HDRI, hoặc hỏi lại chủ dự án.

---

## 2. Đã làm lần này (trọng tâm: tín hiệu đèn)

### 2.1. Đèn — `APlcLight` ([PlcLight.h](../Source/PLC/PlcLight.h) / [.cpp](../Source/PLC/PlcLight.cpp))
- **Tắt = tàng hình** (`Mesh->SetVisibility(false)`), **bật = quả cầu sáng ĐỎ** + point light đỏ.
- **Không cần gán material thủ công:** constructor tự gán engine material
  `/Engine/EngineMaterials/EmissiveMeshMaterial` (unlit emissive, có Vector Param `Color`).
  `SetOn()` set cả `Color` lẫn `EmissiveColor` để chắc ăn nếu sau này đổi material.
- **Đánh dấu đèn:** thêm `TextRenderComponent` hiện nhãn **"H{LightIndex+1}"** (H1..H8) nổi trên
  mỗi đèn, **chỉ trong editor** (tự ẩn khi Play). Đổi `LightIndex` trong Details → nhãn cập nhật
  ngay (qua `OnConstruction`). Tắt nhãn bằng ô `bShowLabel`.
- **Props chỉnh trong Details:** `LightIndex` (0=H1..7=H8), `OnColor` (mặc định đỏ), `OnIntensity`
  (độ chói/bloom), `GlowIntensity` (độ hắt sáng point light), `bShowLabel`.
- Mesh đặt **NoCollision** (đèn chỉ để hiển thị, không cản trace nối dây).

### 2.2. Giả lập bật đèn — `APlcLinkActor` ([PlcLinkActor.h](../Source/PLC/PlcLinkActor.h) / [.cpp](../Source/PLC/PlcLinkActor.cpp))
- **Phím `S`:** bật/tắt chế độ giả lập (`bSimulationMode`). Có báo cố định trên màn hình khi bật.
- **Phím `1..8`:**
  - Khi **giả lập BẬT** → `SimToggleLight(i)`: đảo đèn H1..H8 **ngay trong Unreal** (không cần PLC),
    cập nhật `LastY`, gọi `ApplyLight`, broadcast `OnLightChanged`, in log màn hình.
  - Khi **giả lập TẮT** → giữ nguyên hành vi cũ: gửi `{"toggle":n}` xuống bridge (`ToggleLight`).
- **Log màn hình** dùng `GEngine->AddOnScreenDebugMessage` (độc lập với HMI — chạy được khi chưa có
  `WBP_HMI`). Chuỗi log **không dấu** theo đúng convention của codebase.
- Có ô `bSimulationMode` trong Details để bật sẵn giả lập từ đầu.

### 2.3. Camera cố định — `AFixedViewPawn` ([.h](../Source/PLC/FixedViewPawn.h) / [.cpp](../Source/PLC/FixedViewPawn.cpp))
- Kế thừa `ADefaultPawn`, đặt `bAddDefaultMovementBindings = false` → **bỏ toàn bộ di chuyển/xoay**
  WASD-chuột. Camera đứng yên tại PlayerStart (digital twin, tránh bấm nhầm).
- [`WiringGameMode.cpp`](../Source/PLC/WiringGameMode.cpp) đặt `DefaultPawnClass = AFixedViewPawn`.

---

## 3. Cách build & chạy nhanh

**Build (Git Bash / PowerShell):**
```
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" PLCEditor Win64 Development -Project="C:\PLC_UEProject\PLC-Digital-Twin\PLC.uproject" -WaitMutex
```

**Trong editor:**
1. Mỗi `APlcLight` → Details → đặt `LightIndex` (0..7 ⇔ H1..H8); nhãn vàng H# hiện để khỏi lẫn.
2. Đặt **1 `APlcLinkActor`** trong level (nếu chưa) — nó bắt phím S/1–8 và tự quét đèn theo `LightIndex`.
3. **Play → `S`** (bật giả lập) → **`1..8`** bật/tắt từng đèn. Đèn hiện đỏ, có log trên màn hình.

---

## 4. Việc còn mở / lưu ý cho người tiếp theo

- **Chưa test với PLC thật:** bridge cần gửi `{"x":[...],"y":[...],"d":{...}}` (spec ở `HUONG_DAN_HMI.md`).
  Giả lập chỉ thay tạm chiều nhận `y`. Chiều gửi `{"toggle":n}` vẫn giữ.
- **`WBP_HMI` chưa dựng:** log giả lập đang dùng on-screen debug, **không** phải ScrollBox của HMI.
  Khi dựng xong `WBP_HMI`, `OnLightChanged` sẽ tự vào log HMI (đã broadcast sẵn).
- **Phím `1..8` là TOGGLE** (nhấn lại để tắt). Nếu muốn "chỉ bật", sửa `SimToggleLight` trong
  `PlcLinkActor.cpp`.
- **Ánh sáng đều (đang chỉnh trong editor, không đụng code):** khoá phơi sáng bằng Post Process Volume
  (Infinite Extent + Manual Exposure) TRƯỚC, rồi SkyLight để `Movable` + Real Time Capture / Specified
  Cubemap (HDRI). Tắt Cast Shadows trên Directional Light nếu không muốn bóng đổ.
- **Số kênh:** mặc định 8. Đổi `NumChannels` (`APlcLinkActor`) + nhãn trong `BuildInputStatusString()`.

---

## 5. Cạm bẫy môi trường (giữ nguyên từ README)

- **.NET 8** bắt buộc cho UBT — `global.json` đã ghim (máy này có cả .NET 10). Thiếu → lỗi `exited with code 6`.
- **Engine:** UE **5.6** tại `C:\Program Files\Epic Games\UE_5.6`. MSVC 14.38 (14.5x chỉ warning).
- Thêm file C++ mới → build lại bằng Build.bat (UBT tự quét module, không cần sửa `PLC.Build.cs`).
