// HmiCaptureActor.h
// Actor chụp 1 góc nhìn scene -> RenderTarget để hiển thị trong HUD (UMG).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HmiCaptureActor.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** Hai ô camera bên trái của giao diện. */
UENUM(BlueprintType)
enum class EHmiView : uint8
{
	Board16x9,   // camera nhìn xuống bàn điều khiển (ô trái-trên)
	Lights16x3   // dải đèn H1..H8, cắt trên/dưới (ô trái-dưới)
};

/**
 * Đặt 2 actor này trong level:
 *  - 1 cái View=Board16x9: đặt ở vị trí camera nhìn xuống bàn.
 *  - 1 cái View=Lights16x3: ngắm vào hàng đèn, chỉnh FOV cho khít dải đèn.
 * RenderTarget tự tạo lúc BeginPlay; UHmiWidget sẽ tự tìm theo View.
 */
UCLASS()
class PLC_API AHmiCaptureActor : public AActor
{
	GENERATED_BODY()

public:
	AHmiCaptureActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HMI")
	EHmiView View = EHmiView::Board16x9;

	// Để 0 = tự chọn theo View (16:9 = 1280x720, 16:3 = 1280x240).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HMI")
	int32 RTWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HMI")
	int32 RTHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HMI")
	UTextureRenderTarget2D* RenderTarget = nullptr;

	FORCEINLINE EHmiView GetView() const { return View; }
	FORCEINLINE UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HMI")
	USceneCaptureComponent2D* Capture;
};
