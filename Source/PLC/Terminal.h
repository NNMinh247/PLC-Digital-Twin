// Terminal.h
// Cọc đấu nối (C++ thuần).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Terminal.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AWire;

/**
 * Cọc đấu nối trên mô hình PLC.
 *
 * - TerminalTag: định danh của cọc (vd: Terminal.PLC.Y0, Terminal.Motor.A_Plus).
 * - TargetPartnerTag: tag của cọc ĐÚNG mà cọc này cần được nối tới. AWire::CheckConnection()
 *   dùng cặp tag này để tô dây xanh (đúng) hoặc đỏ (sai). Để trống nếu không cần kiểm tra.
 *
 * GỢI Ý GÁN ASSET (thuần C++): mesh mặc định là hình trụ của engine để nhìn thấy ngay,
 * có thể đổi mesh cho từng cọc trong Details panel của instance đặt trong level.
 */
UCLASS()
class PLC_API ATerminal : public AActor
{
	GENERATED_BODY()

public:
	ATerminal();

	// Định danh cọc
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wiring")
	FGameplayTag TerminalTag;

	// Tag của cọc đối tác đúng (để CheckConnection). Để trống = không kiểm tra phía cọc này.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wiring")
	FGameplayTag TargetPartnerTag;

	// Chỉ số sắp xếp tùy chọn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring")
	int32 ElementIndex = 0;

	// Đang có dây cắm vào hay chưa
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wiring")
	bool bIsOccupied = false;

	// Vị trí (thế giới) để gắn đầu dây vào
	UFUNCTION(BlueprintCallable, Category = "Wiring")
	FVector GetSnapLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Wiring")
	USceneComponent* GetSnapPoint() const { return SnapPoint; }

	FORCEINLINE USphereComponent* GetSnapSphere() const { return SnapSphere; }
	FORCEINLINE UStaticMeshComponent* GetMeshComp() const { return MeshComp; }

	// ===== Chồng nhiều dây trên cùng 1 cọc (xếp theo tầng) =====
	// Ý tưởng: mỗi đầu dây là một "cọc con"; cắm dây thứ nhất = tầng 0, cắm thêm = tầng 1, 2...
	// Đầu dây xếp dọc theo trục cọc (StackAxisLocal) để không đè khít lên nhau.

	// Hướng "cắm ra" của cọc trong KHÔNG GIAN CỤC BỘ (mặc định +Z = trục của trụ mesh).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring|Stack")
	FVector StackAxisLocal = FVector(0.f, 0.f, 1.f);

	// Khoảng cách giữa 2 tầng liền nhau (đơn vị world). Chỉnh trong editor cho hợp scale bàn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring|Stack")
	float LayerStep = 4.f;

	// Độ nâng của tầng thấp nhất so với điểm snap (để dây nằm phía TRÊN cọc).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring|Stack")
	float BaseLift = 2.f;

	// Đăng ký một dây vào chồng của cọc; trả về chỉ số tầng (0 = thấp nhất).
	int32 AttachWire(AWire* Wire);

	// Gỡ một dây khỏi chồng; các dây còn lại tụt tầng và tự cập nhật hình học.
	void DetachWire(AWire* Wire);

	// Chỉ số tầng của một dây trong chồng (0 nếu không có).
	int32 GetLayer(const AWire* Wire) const;

	// Số dây đang cắm vào cọc.
	FORCEINLINE int32 GetStackCount() const { return WireStack.Num(); }

	// Dây ở tầng trên cùng (mới cắm nhất) — dùng khi Alt+click vào node để tháo đầu dây đó.
	FORCEINLINE AWire* GetTopWire() const { return WireStack.Num() > 0 ? WireStack.Last() : nullptr; }

	// Vị trí thế giới để gắn đầu dây ở tầng cho trước.
	UFUNCTION(BlueprintCallable, Category = "Wiring")
	FVector GetPlugLocation(int32 Layer) const;

protected:
	// Gốc
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	// Hình hiển thị của cọc (chỉ để nhìn, không va chạm)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	// Vùng va chạm: chuột trace trúng + làm đích snap (Block kênh WiringInteract)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SnapSphere;

	// Điểm gắn đầu dây (có thể lệch so với tâm cọc)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SnapPoint;

	// Các dây đang cắm vào cọc này, theo thứ tự tầng (index 0 = tầng thấp nhất). Chỉ dùng lúc runtime.
	UPROPERTY(Transient)
	TArray<AWire*> WireStack;
};
