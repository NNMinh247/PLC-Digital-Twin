# PLC Wiring & HMI — Digital Twin (Unreal Engine 5.6)

Mô phỏng/đồng bộ bàn thực hành PLC Mitsubishi **FX3U** trong Unreal Engine: hiển thị trạng thái
thiết bị real-time, mô phỏng cắm dây, và một giao diện HMI 4 ô để giám sát + ghi log.

> Tài liệu này dành cho người **nhận bàn giao dự án**. Việc cần làm tiếp để chạy được: xem
> [`VIEC_CAN_LAM_TIEP.md`](./VIEC_CAN_LAM_TIEP.md). Chi tiết dựng giao diện HMI: xem
> [`HUONG_DAN_HMI.md`](./HUONG_DAN_HMI.md).

---

## 1. Kiến trúc tổng thể

```
 PLC FX3U          PlcBridge (C#, .NET)            Unreal Engine 5.6 (project "PLC")
 X0..X7  ─ serial ─►  đọc X/Y/D từ PLC  ─ WebSocket {x,y,d} ─►  APlcLinkActor (client)
 Y0..Y7  ◄────────►  HslCommunication        ◄── {"toggle":n} ──   │   ├─► APlcLight (đèn)
 D...                + Fleck WebSocket                              │   └─► sự kiện (X/Y/D)
                                                                    ▼
                                                  UHmiWidget  ◄── render target ── AHmiCaptureActor x2
                                                  (WBP_HMI = layout 4 ô)
```

- **PLC ↔ Bridge:** serial (COM, 9600/7/Even/1). Bridge đọc thiết bị thật của PLC.
- **Bridge ↔ Unreal:** WebSocket `ws://127.0.0.1:8080`. Bridge gửi trạng thái; Unreal gửi lệnh toggle.
- **Unreal:** `APlcLinkActor` là đầu mối dữ liệu; phát sự kiện cho đèn (`APlcLight`) và HMI (`UHmiWidget`).

> ⚠️ Project **PlcBridge (C#)** KHÔNG nằm trong repo Unreal này. Đây là 2 thành phần tách rời chạy chung.

---

## 2. Yêu cầu môi trường

| Thành phần | Yêu cầu |
|---|---|
| Unreal Engine | **5.6** (`EngineAssociation` trong `.uproject`) |
| IDE / compiler | Visual Studio + MSVC (UE 5.6 ưa **MSVC 14.38**; bản mới hơn vẫn build nhưng có cảnh báo) |
| .NET SDK | **.NET 8** cho UnrealBuildTool. Máy có .NET 9/10 dễ gây lỗi NuGet → xem mục Cạm bẫy |
| PLC | Mitsubishi FX3U + cáp serial; nạp ladder bằng GX Works2 |
| Bridge | PlcBridge (C#) — project riêng |

---

## 3. Cấu trúc code C++ (module `PLC`, macro `PLC_API`)

`Source/PLC/`

| File | Class | Vai trò |
|---|---|---|
| `WiringInteraction.h` | *(macro)* | `WIRING_TRACE_CHANNEL = ECC_GameTraceChannel1` (kênh trace "WiringInteract") |
| `Terminal.h/.cpp` | `ATerminal` | Cọc đấu: SphereComponent (Block kênh WiringInteract) + SnapPoint; `TerminalTag`, `TargetPartnerTag`, `bIsOccupied` |
| `Wire.h/.cpp` | `AWire` | Dây: `CableComponent` + 2 grab head; `CheckConnection()` so GameplayTag → tô xanh/đỏ; **spawn lúc runtime** |
| `WiringPlayerController.h/.cpp` | `AWiringPlayerController` | Enhanced Input (IMC_Wiring/IA_Grab); trace chuột để grab/kéo/thả dây; **tạo HUD `WBP_HMI` khi Play** |
| `WiringGameMode.h/.cpp` | `AWiringGameMode` | Đặt `PlayerControllerClass = AWiringPlayerController` |
| `PlcLight.h/.cpp` | `APlcLight` | Đèn Y: `SetOn()` đổi emissive + point light; `LightIndex` (0..7) |
| `PlcLinkActor.h/.cpp` | `APlcLinkActor` | **WebSocket client**: parse `{x,y,d}`; events `OnInputChanged`/`OnLightChanged`/`OnRegisterChanged`; `ToggleLight()` gửi `{toggle:n}`; bind phím 1–8; map index→`APlcLight` |
| `HmiCaptureActor.h/.cpp` | `AHmiCaptureActor` | `SceneCaptureComponent2D` → `RenderTarget`; `EHmiView` = Board16x9 / Lights16x3 |
| `HmiWidget.h/.cpp` | `UHmiWidget` | Lớp nền cho `WBP_HMI`: **logic UI 100% C++** (bind widget theo tên `LightsView/BoardView/StatusText/ActionLog/ResultLog`), tự gán ảnh 2 camera, cập nhật status/log, và chiếu ngược click trong ô `BoardView` để nối dây. Không cần Graph |

---

## 4. Cấu hình quan trọng

- **`Source/PLC/PLC.Build.cs`** — deps: `Core, CoreUObject, Engine, InputCore, EnhancedInput, GameplayTags, WebSockets, Json, CableComponent, UMG, Slate, SlateCore`.
- **`PLC.uproject`** — plugin bật: `ModelingToolsEditorMode`, `CableComponent`, `PixelStreaming`.
- **`Config/DefaultEngine.ini`**
  - `GlobalDefaultGameMode = /Script/PLC.WiringGameMode`
  - Kênh va chạm tùy chỉnh: `ECC_GameTraceChannel1` = **"WiringInteract"** (bắt buộc cho trace cắm dây).
- **`Config/DefaultGameplayTags.ini`** — 17 tag `Terminal.*` (PLC.Y0/Y1/COM, Driver.*, Motor.*, Power.0V/24V).
- **`Config/DefaultInput.ini`** — Enhanced Input đặt mặc định (`DefaultPlayerInputClass`, `DefaultInputComponentClass`).

---

## 5. Giao thức WebSocket (JSON)

**Bridge → Unreal** (mỗi ~100 ms), object gồm các trường tùy chọn:

```json
{ "x": [0,1,0,0,0,0,0,0], "y": [1,0,0,0,0,0,0,0], "d": { "D0": 100 } }
```
- `x` = công tắc X0..X7, `y` = đèn/output Y0..Y7 (0/1). `d` = thanh ghi (tùy chọn).
- Tương thích cũ: `{"lights":[...]}` được hiểu là `y`.

**Unreal → Bridge:** `{"toggle": n}` (đảo M[n] qua ladder `LD M[n] / OUT Y[n]`).

> Lưu ý địa chỉ X/Y của Mitsubishi là **bát phân** (X0–X7 rồi X10–X17). 8 kênh đầu không vướng.

---

## 6. Trạng thái hiện tại

**✅ Đã xong (trong repo này):**
- Toàn bộ 9 class C++ + `WiringInteraction.h`.
- `Build.cs`, `.uproject` (plugin), `DefaultEngine.ini` (channel + GameMode), `DefaultGameplayTags.ini`.
- **Build C++ xác nhận sạch** (Development Editor / Win64) + `global.json` ghim .NET 8.
- **HMI logic 100% C++** (bind widget theo tên, không Graph) + **nối dây tương tác trong ô `BoardView`** (click‑click, nhiều dây/cọc, Alt+click xoá).

**⏳ Chưa làm (cần người tiếp tục — xem `VIEC_CAN_LAM_TIEP.md`):**
- **Migrate asset** (model PLC, map, `IA_Grab`, `IMC_Wiring`, `M_Wire_Green/Red`) — giữ đúng đường dẫn ở mục Cạm bẫy.
- Tạo **`WBP_HMI`** (parent `UHmiWidget`) ở `/Game/UI` + dựng layout 4 ô (chỉ vẽ + đặt đúng tên widget, không Graph).
- Đặt **2 `AHmiCaptureActor`** trong level (`View` = Board16x9 ngắm bàn dây, Lights16x3 ngắm đèn).
- Đặt actor gameplay: `ATerminal` (gán tag), `APlcLight` (LightIndex 0–7), 1 `APlcLinkActor`.
- **Mở rộng PlcBridge (C#)** gửi thêm `x` và `d`.
- (Plan) **Công tắc ảo bấm được trong Unreal** — chưa triển khai.

---

## 7. Build & chạy (tóm tắt)

1. Chuột phải `PLC.uproject` → *Generate Visual Studio project files*.
2. Mở `.sln`, build **Development Editor / Win64** (hoặc double-click `.uproject` → Yes để rebuild).
3. Chạy PlcBridge (đã gửi `x/y/d`).
4. Bấm **Play** trong editor.

---

## 8. Cạm bẫy đã biết (đọc trước khi sửa)

- **Thuần C++ cho actor:** không còn Blueprint logic cho Terminal/Wire/… Nếu mang BP cũ sang phải **reparent** về class C++ tương ứng.
- **Dây spawn lúc runtime:** `AWire` không đặt sẵn trong level — chỉ xuất hiện khi Play và bấm vào cọc. Editor không thấy dây là bình thường.
- **HMI 4 ô, logic 100% C++:** ô trên‑trái (`LightsView`) là camera đèn thuần hiển thị; ô dưới‑trái (`BoardView`) là bàn PLC **tương tác được** (click chiếu ngược qua camera). `BoardView` phải để **Hit Test Invisible** để click xuyên xuống game. Nối dây: click‑click, một cọc nhiều dây, **Alt+click = xoá dây**.
- **.NET:** UE 5.6 cần .NET 8. Máy có .NET 9/10 → lỗi `Microsoft.Build/Microsoft.IO.Redist restored using .NETFramework…`, `exited with code 6`. Khắc phục: cài .NET 8 SDK; nếu vẫn lấy nhầm thì thêm `global.json` (`{"sdk":{"version":"8.0.0","rollForward":"latestMinor"}}`) ở gốc project. **(Project PLC ĐÃ có `global.json` ghim .NET 8 — máy này có cả .NET 10 nên nếu thiếu file này, build qua VS/`.uproject` sẽ lỗi `exited with code 6`.)**
- **MSVC:** UE 5.6 ưa MSVC 14.38; bản 14.5x mới hơn chỉ là cảnh báo, không chặn.
- **Đường dẫn asset cho `ConstructorHelpers` (phải khớp khi migrate):**
  - `/Game/Inputs/IMC_Wiring`, `/Game/Inputs/IA_Grab`
  - `/Game/Data/M_Wire_Green`, `/Game/Data/M_Wire_Red`
  - HUD: `/Game/UI/WBP_HMI`
  - Nếu đặt khác đường dẫn → chỉ là warning lúc khởi động, gán lại bằng tay trong editor được.
- **Bẫy UHT:** không để chuỗi `*/` lọt vào giữa comment `/** ... */` (đóng comment sớm → UHT báo "GENERATED_BODY in a block being skipped"). Đã rà sạch.

---

## 9. Tài liệu liên quan

- [`VIEC_CAN_LAM_TIEP.md`](./VIEC_CAN_LAM_TIEP.md) — checklist các bước còn lại (theo thứ tự).
- [`HUONG_DAN_HMI.md`](./HUONG_DAN_HMI.md) — spec JSON cho bridge + cách dựng `WBP_HMI` và 2 capture actor.
- *(Tham khảo lịch sử ở project cũ `PLC_WiringSim`: `HUONG_DAN_PLC_UNREAL.md`, `CHUYEN_DOI_BLUEPRINT_SANG_CPP.md`.)*
