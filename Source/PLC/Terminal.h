// Terminal.h
// Cọc đấu nối (C++ thuần).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Terminal.generated.h"

class UStaticMeshComponent;
class USphereComponent;

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
};
