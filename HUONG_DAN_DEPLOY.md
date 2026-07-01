# Hướng dẫn bàn giao & chạy bản package (PLC Digital Twin)

> Dành cho **người nhận**: chạy mô phỏng PLC trên máy của bạn. **Không cần cài Unreal Engine, Visual Studio hay .NET SDK.**
> Máy bạn chỉ cần: bản package, PlcBridge, và một bàn PLC FX3U cắm cáp serial.

---

## A. Gói bàn giao gồm những gì (người gửi chuẩn bị)

Nén thành 1 file `.zip` cho người nhận, gồm **3 phần**:

1. **Bản package Unreal** — nguyên thư mục sau khi Package (Windows), bên trong có:
   - `PLC.exe`
   - thư mục `Engine/`, `PLC/` (chứa `Content/Paks/*.pak`)
   - thư mục prereqs (`.../Engine/Extras/Redist/...`) — installer Visual C++ Redistributable.
2. **PlcBridge (C#)** — ⚠️ **KHÔNG nằm trong project Unreal**, phải gửi riêng.
   - Nên `dotnet publish` dạng **self-contained** (kèm .NET runtime) để máy nhận khỏi cài .NET.
   - Kèm file cấu hình để chỉnh **cổng COM**.
3. **File này** (`HUONG_DAN_DEPLOY.md`).

> Ladder (`LD M[n] / OUT Y[n]`…) do người nhận tự nạp bằng GX Works2 trên PLC của họ — không gửi qua đây.

### Checklist trước khi gửi
- [ ] Bản package chạy thử **trên máy khác không có Unreal** (để chắc chắn không thiếu DLL).
- [ ] PlcBridge publish self-contained, chạy được độc lập.
- [ ] Ghi rõ **số kênh mặc định = 8** (X1–X8, Đèn 1–8) nếu bàn của họ khác.

---

## B. Yêu cầu máy người nhận

| Thành phần | Bắt buộc | Ghi chú |
|---|---|---|
| Windows 64-bit | ✅ | |
| Visual C++ Redistributable x64 | ✅ | Chạy installer trong thư mục prereqs nếu `PLC.exe` báo thiếu DLL |
| PlcBridge (đã kèm) | ✅ | Self-contained thì không cần cài .NET |
| PLC Mitsubishi **FX3U** + cáp serial | ✅ | Đã nạp ladder |
| GX Works2 | ⚠️ chỉ để nạp ladder | **Phải đóng lại** trước khi chạy (nếu không sẽ giữ cổng COM) |
| Unreal Engine / Visual Studio / .NET SDK | ❌ KHÔNG cần | Chỉ cần khi *build lại*, không cần để *chạy* |

---

## C. Các bước chạy (người nhận làm theo thứ tự)

### 1. Nạp ladder & giải phóng cổng COM
- Mở **GX Works2**, nạp chương trình ladder xuống PLC FX3U.
- **Đóng GX Works2** (rất quan trọng — nó chiếm cổng COM, bridge sẽ không mở được).

### 2. Xác định cổng COM
- Cắm cáp serial PLC vào máy.
- Mở **Device Manager → Ports (COM & LPT)** → ghi lại số cổng, ví dụ `COM3`.
- Thông số serial của FX3U: **9600 baud, 7 data bits, Even parity, 1 stop bit**.

### 3. Chỉnh cấu hình & chạy PlcBridge
- Mở file cấu hình của bridge, sửa **cổng COM** cho khớp bước 2 (mỗi máy một số COM khác nhau).
- Chạy bridge → phải thấy log kiểu **"listening ws://127.0.0.1:8080"** và đọc được PLC.

### 4. Chạy ứng dụng
- Chạy **`PLC.exe`**.
- App tự kết nối tới bridge ở `ws://127.0.0.1:8080` (cùng máy nên **không cần đổi IP, không cần mở firewall**).

### 5. Nghiệm thu
- Màn HMI 4 ô hiện lên; 2 ô trái là camera mô phỏng bàn/đèn.
- **Lật công tắc thật trên PLC** → ô phải-trên đổi trạng thái + thêm dòng log "Bật X1".
- Ladder cho ra đèn/giá trị → đèn trong 3D sáng + ô phải-dưới thêm "Đèn 1 BẬT", "Gán D0 = …".

---

## D. Thao tác trong app

| Thao tác | Kết quả |
|---|---|
| Phím **1–8** | Gửi lệnh `{"toggle":n}` xuống PLC (đảo Đèn 1–8 qua ladder `LD M[n] / OUT Y[n]`) |
| Công tắc vật lý trên PLC | Cập nhật trạng thái X1–X8 lên HMI |

> Chiều dữ liệu: PLC → PlcBridge (serial) → Unreal (WebSocket, gửi `{x,y,d}` mỗi ~100ms).
> Chiều lệnh: Unreal → PlcBridge (`{toggle:n}`) → PLC.

---

## E. Xử lý sự cố thường gặp

| Hiện tượng | Nguyên nhân & cách xử lý |
|---|---|
| `PLC.exe` không mở / thiếu `.dll` | Cài **VC++ Redistributable x64** (installer trong thư mục prereqs của bản package) |
| Bridge không mở được cổng COM | **GX Works2 chưa đóng**, hoặc sai số COM, hoặc cáp chưa cắm |
| App mở nhưng HMI không đổi khi lật công tắc | Bridge chưa chạy / chưa kết nối PLC. Kiểm tra log bridge thấy "listening ws://…:8080" và đọc được X/Y |
| 2 ô camera hiện nhưng ô phải trống | Bình thường khi chưa có bridge — chỉ 2 ô trái không cần dữ liệu PLC |
| Bàn PLC khác 8 kênh | Số kênh chốt lúc build (`NumChannels` mặc định 8). Đổi số kênh phải **build lại** project, không sửa được ở bản package |
| Bridge chạy trên **máy khác** trong LAN | Phải sửa `ServerUrl` (mặc định `ws://127.0.0.1:8080`) sang IP máy bridge — nhưng cái này chốt lúc build, bản package không sửa được. Khuyến nghị chạy bridge **cùng máy** |

---

## F. Lưu ý quan trọng

- **Địa chỉ X/Y của Mitsubishi là BÁT PHÂN** (X0–X7 rồi X10–X17). 8 kênh đầu (X0–X7) không vướng; nếu mở rộng >8 kênh mới phải để ý nhảy X10.
- **HMI là màn hình GIÁM SÁT**: 2 ô trái là ảnh SceneCapture (không tương tác). Khi HUD phủ full màn, thao tác kéo dây bằng chuột sẽ bị che — đây là 2 chế độ khác nhau.
- Muốn đổi **IP bridge, số kênh, hay nhãn log** → phải **build lại** trên máy có Unreal 5.6, rồi package lại. Bản package không chỉnh được các thứ này.

---

## G. Tóm tắt 1 dòng cho người nhận

> Nạp ladder → **đóng GX Works2** → chỉnh COM & chạy **PlcBridge** → chạy **`PLC.exe`** → lật công tắc PLC để kiểm tra.
