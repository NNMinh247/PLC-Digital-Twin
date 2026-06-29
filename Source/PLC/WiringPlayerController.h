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
class AWire;
class ATerminal;
enum class EWireEnd : uint8;

/**
 * Điều khiển thao tác đấu dây bằng chuột (Enhanced Input).
 *
 * Luồng:
 *  - IA_Grab Started   -> OnGrabStarted: trace dưới chuột (kênh WiringInteract).
 *        + Nếu trúng grab head của một AWire  -> nhấc đầu dây đó ra để kéo.
 *        + Nếu trúng một ATerminal trống      -> spawn dây mới, ghim đầu A vào cọc, kéo đầu B.
 *  - PlayerTick        -> đầu dây đang cầm bám theo con trỏ (ưu tiên dính vào cọc đang hover).
 *  - IA_Grab Completed -> OnGrabReleased: thả trúng cọc hợp lệ thì gắn và CheckConnection;
 *        thả vào chỗ trống mà dây chưa nối đầu nào thì hủy dây.
 *
 * Cần đối chiếu với hành vi mong muốn: cơ chế grab head hai đầu, spawn dây khi bấm cọc trống,
 * quy tắc hủy dây khi thả hụt. Điều chỉnh trong các hàm OnGrabStarted, OnGrabReleased, PlayerTick.
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

	void OnGrabStarted();
	void OnGrabReleased();

	// Trace 1 tia từ chuột theo kênh cho trước, có thể bỏ qua 1 actor.
	bool TraceCursor(ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const;

	// Điểm thế giới cho đầu dây đang kéo: ưu tiên dính cọc -> world geometry -> deproject.
	bool GetDragWorldPoint(FVector& OutPoint, ATerminal*& OutHoverTerminal) const;

	static EWireEnd OtherEnd(EWireEnd End);

	UPROPERTY()
	AWire* HeldWire = nullptr;

	// Đầu dây đang cầm. Khởi tạo trong constructor.
	EWireEnd HeldEnd;

	bool bIsHolding = false;

	// Tạo + add HUD vào viewport (gọi sau BeginPlay 1 nhịp để các capture actor kịp khởi tạo).
	void CreateHud();

	UPROPERTY()
	UUserWidget* HmiWidget = nullptr;
};
