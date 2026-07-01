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
class AWire;
class ATerminal;
enum class EWireEnd : uint8;

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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// Một cú click chuột (IA_Grab Started). Xử lý cả bắt đầu lẫn hoàn tất dây (và Alt = xoá).
	void OnClick();

	// Alt + click: xoá dây dưới con trỏ.
	void DeleteWireUnderCursor();

	// Tìm 1 dây đang nối vào cọc T (dùng khi Alt+click trúng cọc thay vì trúng đầu dây). Null nếu không có.
	AWire* FindWireOnTerminal(const ATerminal* T) const;

	// Tia dưới con trỏ: ưu tiên ô board (chiếu ngược qua camera) -> deproject viewport chính.
	bool GetCursorRay(FVector& OutOrigin, FVector& OutDir) const;

	// Trace theo 1 tia cho trước.
	bool TraceRay(const FVector& Origin, const FVector& Dir, ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const;

	// Trace 1 tia từ con trỏ theo kênh cho trước, có thể bỏ qua 1 actor.
	bool TraceCursor(ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const;

	// Điểm thế giới cho đầu dây đang đặt: ưu tiên dính cọc -> world geometry -> theo tia.
	bool GetDragWorldPoint(FVector& OutPoint, ATerminal*& OutHoverTerminal) const;

	// Dây đang đặt (đã ghim đầu A, đang chờ click 2 để nối đầu B). Null nếu không đặt.
	UPROPERTY()
	AWire* PendingWire = nullptr;

	bool bPlacing = false;

	// Tạo + add HUD vào viewport (gọi sau BeginPlay 1 nhịp để các capture actor kịp khởi tạo).
	void CreateHud();

	UPROPERTY()
	UHmiWidget* Hmi = nullptr;
};
