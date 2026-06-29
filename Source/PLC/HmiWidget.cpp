// HmiWidget.cpp
#include "HmiWidget.h"
#include "HmiCaptureActor.h"
#include "PlcLinkActor.h"
#include "EngineUtils.h"

void UHmiWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1) Lấy 2 render target từ các AHmiCaptureActor trong level.
	FindCaptureTargets();
	OnRenderTargetsReady();

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

void UHmiWidget::FindCaptureTargets()
{
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AHmiCaptureActor> It(W); It; ++It)
		{
			if (It->GetView() == EHmiView::Board16x9)
			{
				BoardRT = It->GetRenderTarget();
			}
			else
			{
				LightsRT = It->GetRenderTarget();
			}
		}
	}
}

void UHmiWidget::HandleInputChanged(int32 Index, bool bOn)
{
	OnActionLog(FString::Printf(TEXT("%s X%d"), bOn ? TEXT("Bật") : TEXT("Tắt"), Index + 1));
	RefreshStatus();
}

void UHmiWidget::HandleLightChanged(int32 Index, bool bOn)
{
	OnResultLog(FString::Printf(TEXT("Đèn %d %s"), Index + 1, bOn ? TEXT("BẬT") : TEXT("TẮT")));
}

void UHmiWidget::HandleRegisterChanged(const FString& Name, int32 Value)
{
	OnResultLog(FString::Printf(TEXT("Gán %s = %d"), *Name, Value));
}

void UHmiWidget::RefreshStatus()
{
	if (Link)
	{
		OnStatusUpdated(Link->BuildInputStatusString());
	}
}
