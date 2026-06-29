// Wire.h
// Dây nối hai cọc (C++ thuần).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wire.generated.h"

class UCableComponent;
class USphereComponent;
class UMaterialInterface;
class ATerminal;

/** Đầu của dây. */
UENUM(BlueprintType)
enum class EWireEnd : uint8
{
	A,
	B
};

/**
 * Dây nối hai cọc (ATerminal).
 *
 * - Đầu A là gốc cable (vị trí actor), đầu B là điểm cuối cable.
 * - Mỗi đầu có một "grab head" (SphereComponent) Block kênh WiringInteract để
 *   AWiringPlayerController có thể trace dưới chuột và cầm đúng đầu dây.
 * - CheckConnection() so khớp GameplayTag của 2 cọc, tô material xanh (đúng) / đỏ (sai).
 *
 * GỢI Ý ASSET (thuần C++): GreenMaterial/RedMaterial mặc định nạp từ
 *   /Game/Data/M_Wire_Green và /Game/Data/M_Wire_Red qua ConstructorHelpers.
 */
UCLASS()
class PLC_API AWire : public AActor
{
	GENERATED_BODY()

public:
	AWire();

	// Cọc đang gắn ở mỗi đầu (null nếu đầu đó đang tự do/đang kéo)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wiring")
	ATerminal* CurrentTerminal_A = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wiring")
	ATerminal* CurrentTerminal_B = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring")
	float CableWidth = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring")
	float CableLength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring")
	UMaterialInterface* GreenMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wiring")
	UMaterialInterface* RedMaterial = nullptr;

	// Gắn một đầu vào cọc (đánh dấu cọc đã occupied + CheckConnection)
	UFUNCTION(BlueprintCallable, Category = "Wiring")
	void AttachEndToTerminal(EWireEnd End, ATerminal* Terminal);

	// Tháo một đầu khỏi cọc (đầu đó thành tự do)
	UFUNCTION(BlueprintCallable, Category = "Wiring")
	void DetachEnd(EWireEnd End);

	// Khi đang kéo: đặt vị trí thế giới cho đầu tự do
	UFUNCTION(BlueprintCallable, Category = "Wiring")
	void SetFreeEndWorldLocation(EWireEnd End, const FVector& WorldLocation);

	// So khớp tag 2 cọc, tô xanh/đỏ. Trả về true nếu đúng cặp.
	UFUNCTION(BlueprintCallable, Category = "Wiring")
	bool CheckConnection();

	UFUNCTION(BlueprintCallable, Category = "Wiring")
	ATerminal* GetTerminal(EWireEnd End) const;

	// Vị trí thế giới hiện tại của một đầu
	FVector GetEndWorldLocation(EWireEnd End) const;

	// Xác định đầu dây từ component bị chuột trace trúng (grab head)
	bool TryGetEndFromComponent(const UPrimitiveComponent* Comp, EWireEnd& OutEnd) const;

protected:
	virtual void BeginPlay() override;

	// Cập nhật hình học cable + grab head theo trạng thái 2 đầu
	void RefreshCable();

	// Đổi material theo đúng/sai
	void ApplyColor(bool bValid);

	// Gán cọc cho một đầu, tự xử lý cờ occupied của cọc cũ
	void SetTerminal(EWireEnd End, ATerminal* T);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCableComponent* Cable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* GrabHeadA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* GrabHeadB;

	// Vị trí thế giới của đầu tự do (dùng khi cọc tương ứng = null)
	FVector FreeWorldA = FVector::ZeroVector;
	FVector FreeWorldB = FVector::ZeroVector;
};
