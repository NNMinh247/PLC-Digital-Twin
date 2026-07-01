// HmiWidget.cpp
#include "HmiWidget.h"
#include "HmiCaptureActor.h"
#include "PlcLinkActor.h"
#include "EngineUtils.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"

void UHmiWidget::NativeConstruct()
{
	Super::NativeConstruct();

	WarnMissingWidgets();

	// 1) Lấy 2 render target từ các AHmiCaptureActor trong level rồi gán vào 2 Image bên trái.
	//    Nếu chưa sẵn (capture actor chưa BeginPlay) -> NativeTick sẽ thử lại.
	FindCaptureTargets();
	ApplyRenderTargets();
	bRenderTargetsApplied = (BoardRT != nullptr && LightsRT != nullptr);

	// 2) Tìm PlcLinkActor và lắng nghe sự kiện.
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<APlcLinkActor> It(W); It; ++It)
		{
			Link = *It;
			break;
		}
	}

	if (Link)
	{
		Link->OnInputChanged.AddDynamic(this, &UHmiWidget::HandleInputChanged);
		Link->OnLightChanged.AddDynamic(this, &UHmiWidget::HandleLightChanged);
		Link->OnRegisterChanged.AddDynamic(this, &UHmiWidget::HandleRegisterChanged);
		RefreshStatus();
	}
}

void UHmiWidget::NativeDestruct()
{
	if (Link)
	{
		Link->OnInputChanged.RemoveDynamic(this, &UHmiWidget::HandleInputChanged);
		Link->OnLightChanged.RemoveDynamic(this, &UHmiWidget::HandleLightChanged);
		Link->OnRegisterChanged.RemoveDynamic(this, &UHmiWidget::HandleRegisterChanged);
	}
	Super::NativeDestruct();
}

void UHmiWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Thử lại gán render target cho tới khi cả 2 sẵn sàng (hoặc hết thời gian).
	if (!bRenderTargetsApplied)
	{
		RenderTargetRetry -= InDeltaTime;
		FindCaptureTargets();
		ApplyRenderTargets();
		if ((BoardRT && LightsRT) || RenderTargetRetry <= 0.f)
		{
			bRenderTargetsApplied = true;
		}
	}
}

void UHmiWidget::FindCaptureTargets()
{
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AHmiCaptureActor> It(W); It; ++It)
		{
			if (It->GetView() == EHmiView::Board16x9)
			{
				BoardRT = It->GetRenderTarget();
				BoardCapture = *It;   // giữ lại để chiếu ngược click trong ô board
			}
			else
			{
				LightsRT = It->GetRenderTarget();
			}
		}
	}
}

void UHmiWidget::ApplyRenderTargets()
{
	// Gán render target làm brush cho Image (không cần tạo material UI).
	auto SetImage = [](UImage* Img, UTextureRenderTarget2D* RT)
	{
		if (!Img || !RT)
		{
			return;
		}
		FSlateBrush Brush;
		Brush.SetResourceObject(RT);
		Brush.ImageSize = FVector2D((float)RT->SizeX, (float)RT->SizeY);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Img->SetBrush(Brush);
	};

	SetImage(BoardView, BoardRT);
	SetImage(LightsView, LightsRT);
}

void UHmiWidget::HandleInputChanged(int32 Index, bool bOn)
{
	AppendLog(ActionLog, FString::Printf(TEXT("%s X%d"), bOn ? TEXT("Bật") : TEXT("Tắt"), Index + 1));
	RefreshStatus();
}

void UHmiWidget::HandleLightChanged(int32 Index, bool bOn)
{
	AppendLog(ResultLog, FString::Printf(TEXT("Đèn %d %s"), Index + 1, bOn ? TEXT("BẬT") : TEXT("TẮT")));
}

void UHmiWidget::HandleRegisterChanged(const FString& Name, int32 Value)
{
	AppendLog(ResultLog, FString::Printf(TEXT("Gán %s = %d"), *Name, Value));
}

void UHmiWidget::RefreshStatus()
{
	if (Link && StatusText)
	{
		StatusText->SetText(FText::FromString(Link->BuildInputStatusString()));
	}
}

void UHmiWidget::AppendLog(UScrollBox* Box, const FString& Line)
{
	if (!Box || !WidgetTree)
	{
		return;
	}

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!Text)
	{
		return;
	}

	Text->SetText(FText::FromString(Line));
	Text->SetColorAndOpacity(LogTextColor);

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = LogFontSize;
	Text->SetFont(Font);

	Box->AddChild(Text);

	// Cắt bớt dòng cũ nếu vượt giới hạn.
	if (MaxLogLines > 0)
	{
		while (Box->GetChildrenCount() > MaxLogLines)
		{
			Box->RemoveChildAt(0);
		}
	}

	Box->ScrollToEnd();
}

void UHmiWidget::WarnMissingWidgets() const
{
	// Cảnh báo nếu WBP thiếu widget đúng tên (giúp người dựng UMG biết cần thêm gì).
	if (!LightsView) { UE_LOG(LogTemp, Warning, TEXT("[HMI] Thiếu Image 'LightsView' trong WBP_HMI")); }
	if (!BoardView)  { UE_LOG(LogTemp, Warning, TEXT("[HMI] Thiếu Image 'BoardView' trong WBP_HMI (ô board sẽ không tương tác được)")); }
	if (!StatusText) { UE_LOG(LogTemp, Warning, TEXT("[HMI] Thiếu TextBlock 'StatusText' trong WBP_HMI")); }
	if (!ActionLog)  { UE_LOG(LogTemp, Warning, TEXT("[HMI] Thiếu ScrollBox 'ActionLog' trong WBP_HMI")); }
	if (!ResultLog)  { UE_LOG(LogTemp, Warning, TEXT("[HMI] Thiếu ScrollBox 'ResultLog' trong WBP_HMI")); }
}

bool UHmiWidget::GetBoardCursorUV(FVector2D& OutUV) const
{
	if (!BoardView || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	// Hình học ô board lúc runtime (tự cập nhật dù bạn kéo/resize ô trong UMG).
	const FGeometry& Geo = BoardView->GetCachedGeometry();
	const FVector2D Abs = FSlateApplication::Get().GetCursorPos();
	if (!Geo.IsUnderLocation(Abs))
	{
		return false; // con trỏ nằm ngoài ô board
	}

	const FVector2D Size = Geo.GetLocalSize();
	if (Size.X <= 0.f || Size.Y <= 0.f)
	{
		return false;
	}

	const FVector2D Local = Geo.AbsoluteToLocal(Abs);
	OutUV = FVector2D(Local.X / Size.X, Local.Y / Size.Y);
	return true;
}

bool UHmiWidget::IsCursorOverBoard() const
{
	FVector2D UV;
	return GetBoardCursorUV(UV);
}

bool UHmiWidget::GetBoardRayUnderCursor(FVector& OutOrigin, FVector& OutDir) const
{
	FVector2D UV;
	if (!BoardCapture || !GetBoardCursorUV(UV))
	{
		return false;
	}
	return BoardCapture->DeprojectUVToWorldRay(UV, OutOrigin, OutDir);
}
