// WiringPlayerController.h
// Điều khiển thao tác đấu dây bằng chuột (C++ thuần).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"
#include "WiringPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UHmiWidget;
class UTexture2D;
class AWire;
class ATerminal;
class SWidget;
enum class EWireEnd : uint8;
struct FHitResult;
struct FSlateBrush;

/**
 * Điều khiển thao tác đấu dây bằng chuột (Enhanced Input).
 *
 * Cơ chế nối dây kiểu "click-click" (không kéo-thả):
 *  - Click 1 (IA_Grab) trúng một cọc  -> spawn dây mới, ghim đầu A vào cọc, đầu B bám con trỏ.
 *  - PlayerTick                        -> đầu B bám theo con trỏ (ưu tiên dính vào cọc đang hover).
 *  - Click 2:
 *        + trúng một cọc KHÁC cọc bắt đầu -> nối đầu B vào đó + CheckConnection (dây giữ nguyên).
 *        + trúng chỗ trống / chính cọc bắt đầu -> huỷ dây đang đặt.
 *
 * Một cọc có thể cắm NHIỀU dây (vd A->B và A->C dùng chung điểm A) — không giới hạn occupied.
 * Click được chiếu ngược qua camera ô board (Cách A) nếu con trỏ nằm trong ô HMI; nếu không thì
 * dùng deproject từ viewport chính (chạy được cả khi chưa có HUD, vd test trong editor).
 *
 * Alt + click: xoá dây dưới con trỏ (trúng đầu dây, hoặc trúng cọc thì xoá 1 dây nối vào cọc đó).
 */
UCLASS()
class PLC_API AWiringPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWiringPlayerController();

	// Mapping context + action (mặc định nạp /Game/Inputs/IMC_Wiring và /Game/Inputs/IA_Grab)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring|Input")
	UInputMappingContext* WiringMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring|Input")
	UInputAction* GrabAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring|Input")
	int32 MappingPriority = 0;

	// Khoảng cách trace tối đa từ camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring")
	float TraceDistance = 100000.0f;

	// Lớp dây spawn khi bắt đầu kéo từ một cọc trống
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring")
	TSubclassOf<AWire> WireClass;

	// Widget giao diện 4 ô (WBP_HMI). Mặc định nạp /Game/UI/WBP_HMI nếu có.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HMI")
	TSubclassOf<UUserWidget> HmiWidgetClass;

	// Icon con trỏ mềm. Mặc định nạp /Game/UI/T_Cursor (bake sao cho đầu nhọn ở TÂM ảnh).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	UTexture2D* CursorTexture = nullptr;

	// Chiều cao hiển thị (px) của cả khung con trỏ; bề rộng suy ra theo tỉ lệ texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	float SoftwareCursorHeight = 88.0f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// Một cú click chuột (IA_Grab Started). Xử lý cả bắt đầu lẫn hoàn tất dây (và Alt = xoá).
	void OnClick();

	// Alt + click: trúng THÂN dây -> xoá cả dây; trúng NODE -> tháo đầu dây trên cùng ở node đó
	// rồi kéo đầu vừa tháo theo chuột để nối chỗ khác.
	void HandleAltClick();

	// Multi-trace theo tia con trỏ (near->far). Trả false nếu không có tia.
	bool MultiTraceCursor(TArray<FHitResult>& OutHits, AActor* IgnoreActor) const;

	// Cọc gần nhất dưới con trỏ (bỏ qua thân dây). Dùng cho click bắt đầu/hoàn tất + preview.
	bool FindTerminalUnderCursor(ATerminal*& OutTerm, AActor* IgnoreActor) const;

	// Suy ra cọc từ 1 hit: trúng cọc -> chính nó; trúng đầu dây đã cắm -> cọc mà đầu dây đó đang cắm.
	// Nhờ vậy click lên một tầng dây trên cọc = click chính cọc đó (để cắm thêm tầng).
	ATerminal* ResolveTerminalFromHit(const FHitResult& Hit) const;

	// Tia dưới con trỏ: ưu tiên ô board (chiếu ngược qua camera) -> deproject viewport chính.
	bool GetCursorRay(FVector& OutOrigin, FVector& OutDir) const;

	// Trace theo 1 tia cho trước.
	bool TraceRay(const FVector& Origin, const FVector& Dir, ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const;

	// Trace 1 tia từ con trỏ theo kênh cho trước, có thể bỏ qua 1 actor.
	bool TraceCursor(ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const;

	// Điểm thế giới cho đầu dây đang đặt: ưu tiên dính cọc -> world geometry -> theo tia.
	bool GetDragWorldPoint(FVector& OutPoint, ATerminal*& OutHoverTerminal) const;

	// Dây đang đặt (một đầu đã ghim vào cọc, đầu kia đang bám chuột chờ click nối). Null nếu không đặt.
	UPROPERTY()
	AWire* PendingWire = nullptr;

	bool bPlacing = false;

	// Đầu ĐANG KÉO của dây đang đặt (đầu tự do bám chuột). Mặc định B khi bắt đầu dây mới;
	// khi Alt tháo đầu ở node thì có thể là A hoặc B.
	EWireEnd PlacingEnd;

	// Cọc mà đầu đang kéo vừa được tháo ra (chỉ khi Alt-tháo dây có sẵn). Click huỷ -> gắn lại về đây.
	// Null nghĩa là dây vừa spawn mới -> huỷ thì xoá hẳn.
	UPROPERTY()
	ATerminal* PlacingOriginTerminal = nullptr;

	// Dò cạnh nhấn chuột trái trong PlayerTick (fallback khi chưa có IA_Grab — BindKey không
	// kích hoạt ổn trong input mode GameAndUI). Chỉ dùng khi GrabAction == null.
	bool bLmbWasDown = false;

	// Tạo + add HUD vào viewport (gọi sau BeginPlay 1 nhịp để các capture actor kịp khởi tạo).
	void CreateHud();

	UPROPERTY()
	UHmiWidget* Hmi = nullptr;

	// Đăng ký con trỏ mềm (widget do app tự vẽ) lên game viewport. An toàn khi chưa có texture.
	void SetupSoftwareCursor();

	// Giữ brush + widget con trỏ sống suốt vòng đời controller (SImage giữ con trỏ tới brush).
	TSharedPtr<FSlateBrush> CursorBrush;
	TSharedPtr<SWidget> CursorWidget;
};
