// HmiWidget.h
// Lớp nền C++ cho widget giao diện 4 ô. WBP_HMI kế thừa class này.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HmiWidget.generated.h"

class UTextureRenderTarget2D;
class APlcLinkActor;

/**
 * Lớp nền cho HUD thực hành PLC (4 ô).
 *
 * C++ lo: tìm render target 2 camera, lắng nghe sự kiện PLC, định dạng chuỗi log/status.
 * WBP lo: bố cục 4 ô + cài 4 BlueprintImplementableEvent dưới đây (append ScrollBox / set Text /
 * set brush Image từ BoardRT & LightsRT).
 */
UCLASS()
class PLC_API UHmiWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 2 ô trái: WBP bind brush Image vào 2 render target này (trong OnRenderTargetsReady).
	UPROPERTY(BlueprintReadOnly, Category = "HMI")
	UTextureRenderTarget2D* BoardRT = nullptr;   // ô trái-trên (16:9)

	UPROPERTY(BlueprintReadOnly, Category = "HMI")
	UTextureRenderTarget2D* LightsRT = nullptr;  // ô trái-dưới (16:3)

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ===== WBP cài đặt các event này =====

	// Render target đã sẵn sàng -> WBP set brush cho 2 Image bên trái.
	UFUNCTION(BlueprintImplementableEvent, Category = "HMI")
	void OnRenderTargetsReady();

	// Ô phải-trên: cập nhật dòng trạng thái công tắc "X1: ON  X2: OFF ...".
	UFUNCTION(BlueprintImplementableEvent, Category = "HMI")
	void OnStatusUpdated(const FString& StatusText);

	// Ô phải-trên: thêm 1 dòng vào scrollbox hành động ("Bật X1", "Tắt X2"...).
	UFUNCTION(BlueprintImplementableEvent, Category = "HMI")
	void OnActionLog(const FString& Line);

	// Ô phải-dưới: thêm 1 dòng vào scrollbox kết quả ("Đèn 1 BẬT", "Gán D0 = 100"...).
	UFUNCTION(BlueprintImplementableEvent, Category = "HMI")
	void OnResultLog(const FString& Line);

	// ===== Xử lý sự kiện từ PLC =====
	UFUNCTION()
	void HandleInputChanged(int32 Index, bool bOn);             // X -> action log + status

	UFUNCTION()
	void HandleLightChanged(int32 Index, bool bOn);             // Y -> result log

	UFUNCTION()
	void HandleRegisterChanged(const FString& Name, int32 Value); // D -> result log

	UPROPERTY()
	APlcLinkActor* Link = nullptr;

private:
	void FindCaptureTargets();
	void RefreshStatus();
};
