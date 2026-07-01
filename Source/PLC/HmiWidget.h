// HmiWidget.h
// Lớp nền C++ cho widget giao diện 4 ô. WBP_HMI kế thừa class này.
// TOÀN BỘ LOGIC Ở C++: trong UMG chỉ cần vẽ layout + đặt ĐÚNG TÊN các widget dưới đây
// (không cần kéo node nào trong Graph).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateColor.h"
#include "HmiWidget.generated.h"

class UTextureRenderTarget2D;
class APlcLinkActor;
class AHmiCaptureActor;
class UImage;
class UTextBlock;
class UScrollBox;

/**
 * HUD thực hành PLC (4 ô) — logic viết hoàn toàn bằng C++.
 *
 * WBP_HMI chỉ cần chứa các widget SAU, đặt tên CHÍNH XÁC (C++ tự tìm qua BindWidget):
 *   - Image      "LightsView"  : ô trên-trái  (camera đèn, thuần hiển thị)
 *   - Image      "BoardView"   : ô dưới-trái  (bàn PLC tương tác) — Visibility = Hit Test Invisible
 *   - TextBlock  "StatusText"  : ô trên-phải  (dòng trạng thái X)
 *   - ScrollBox  "ActionLog"   : ô trên-phải  (log hành động)
 *   - ScrollBox  "ResultLog"   : ô dưới-phải  (log kết quả)
 *
 * Dùng BindWidgetOptional: thiếu widget nào thì widget đó bị bỏ qua (kèm cảnh báo log),
 * WBP vẫn compile được — tiện dựng dần trong UMG.
 */
UCLASS()
class PLC_API UHmiWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ===== Chỉnh nhanh trong WBP > Class Defaults (Details), không cần sửa code =====

	// Màu chữ cho các dòng log tạo lúc runtime.
	UPROPERTY(EditAnywhere, Category = "HMI|Log")
	FSlateColor LogTextColor = FSlateColor(FLinearColor::White);

	// Cỡ chữ log.
	UPROPERTY(EditAnywhere, Category = "HMI|Log")
	float LogFontSize = 18.f;

	// Giới hạn số dòng log (0 = không giới hạn). Vượt thì xoá dòng cũ nhất.
	UPROPERTY(EditAnywhere, Category = "HMI|Log")
	int32 MaxLogLines = 200;

	// ===== Ô board tương tác (click-to-wire trong ô HMI) =====

	// Con trỏ có đang nằm trong ô board không.
	bool IsCursorOverBoard() const;

	// Tia thế giới đi qua con trỏ NẾU con trỏ đang nằm trong ô board. Trả false nếu không.
	// AWiringPlayerController dùng hàm này để trace cọc/dây khi click trong ô board.
	bool GetBoardRayUnderCursor(FVector& OutOrigin, FVector& OutDir) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== Widget do UMG cung cấp (bind theo tên) =====

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* LightsView = nullptr;   // ô trên-trái: camera đèn

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* BoardView = nullptr;    // ô dưới-trái: bàn PLC tương tác (Hit Test Invisible)

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText = nullptr; // ô trên-phải: dòng trạng thái

	UPROPERTY(meta = (BindWidgetOptional))
	UScrollBox* ActionLog = nullptr;  // ô trên-phải: log hành động

	UPROPERTY(meta = (BindWidgetOptional))
	UScrollBox* ResultLog = nullptr;  // ô dưới-phải: log kết quả

	// ===== Xử lý sự kiện từ PLC (bind vào delegate của APlcLinkActor) =====
	UFUNCTION()
	void HandleInputChanged(int32 Index, bool bOn);             // X -> action log + status

	UFUNCTION()
	void HandleLightChanged(int32 Index, bool bOn);             // Y -> result log

	UFUNCTION()
	void HandleRegisterChanged(const FString& Name, int32 Value); // D -> result log

	UPROPERTY()
	APlcLinkActor* Link = nullptr;

	// 2 render target lấy từ AHmiCaptureActor (C++ tự gán vào brush của 2 Image bên trái).
	UPROPERTY()
	UTextureRenderTarget2D* BoardRT = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* LightsRT = nullptr;

	// Capture actor đang chụp bàn PLC (View = Board16x9). Dùng để chiếu ngược click.
	UPROPERTY()
	AHmiCaptureActor* BoardCapture = nullptr;

private:
	// Đã gán xong render target chưa (để dừng thử lại trong NativeTick).
	bool bRenderTargetsApplied = false;
	// Thời gian (giây) còn thử lại gán render target khi mới Play.
	float RenderTargetRetry = 3.f;

	void FindCaptureTargets();
	void ApplyRenderTargets();               // gán 2 render target vào brush 2 Image
	void RefreshStatus();
	void AppendLog(UScrollBox* Box, const FString& Line);
	void WarnMissingWidgets() const;

	// Toạ độ UV (0..1) của con trỏ trong ô board. Trả false nếu con trỏ ngoài ô.
	bool GetBoardCursorUV(FVector2D& OutUV) const;
};
