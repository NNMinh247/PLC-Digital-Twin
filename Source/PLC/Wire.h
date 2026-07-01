// Wire.h
// Dây nối hai cọc (C++ thuần).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wire.generated.h"

class UCableComponent;
class USphereComponent;
class UCapsuleComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
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
	float CableWidth = 4.0f;   // dày gấp đôi (trước = 2.0)

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

	// Cập nhật màu theo trạng thái nối: đủ 2 đầu -> XANH, chưa đủ -> ĐỎ.
	// (Logic đúng/sai theo GameplayTag để lại làm sau — hiện chỉ cần nối được.)
	UFUNCTION(BlueprintCallable, Category = "Wiring")
	bool CheckConnection();

	// Cập nhật lại hình học cable + grab head (cọc gọi khi các dây trong chồng tụt tầng).
	void RefreshGeometry();

	UFUNCTION(BlueprintCallable, Category = "Wiring")
	ATerminal* GetTerminal(EWireEnd End) const;

	// Vị trí thế giới hiện tại của một đầu
	FVector GetEndWorldLocation(EWireEnd End) const;

	// Xác định đầu dây từ component bị chuột trace trúng (grab head)
	bool TryGetEndFromComponent(const UPrimitiveComponent* Comp, EWireEnd& OutEnd) const;

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;   // tự gỡ khỏi chồng của 2 cọc khi bị xoá

	// Cập nhật hình học cable + grab head theo trạng thái 2 đầu
	void RefreshCable();

	// Đổi material theo trạng thái nối (xanh = đã nối đủ 2 đầu, đỏ = chưa).
	void ApplyColor(bool bConnected);

	// Chọn material cho trạng thái: ưu tiên asset gán tay, không có thì tạo dynamic material từ engine.
	UMaterialInterface* ResolveColorMaterial(bool bConnected);

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

	// Va chạm dọc THÂN dây (capsule) để chuột trace trúng chính giữa dây -> Alt+click xoá cả dây.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* BodyCollision;

	// Vị trí thế giới của đầu tự do (dùng khi cọc tương ứng = null)
	FVector FreeWorldA = FVector::ZeroVector;
	FVector FreeWorldB = FVector::ZeroVector;

	// Material màu tạo lúc runtime khi không có asset M_Wire_Green/Red (nạp từ engine EmissiveMeshMaterial).
	UPROPERTY(Transient)
	UMaterialInterface* BaseColorMaterial = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* GreenMID = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* RedMID = nullptr;
};
