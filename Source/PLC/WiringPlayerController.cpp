// WiringPlayerController.cpp
#include "WiringPlayerController.h"
#include "Wire.h"
#include "Terminal.h"
#include "WiringInteraction.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

AWiringPlayerController::AWiringPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;

	HeldEnd = EWireEnd::B;
	WireClass = AWire::StaticClass();

	// Nạp sẵn input asset theo đường dẫn (thuần C++, không cần Blueprint).
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC(
		TEXT("/Game/Inputs/IMC_Wiring.IMC_Wiring"));
	if (IMC.Succeeded()) { WiringMappingContext = IMC.Object; }

	static ConstructorHelpers::FObjectFinder<UInputAction> IA(
		TEXT("/Game/Inputs/IA_Grab.IA_Grab"));
	if (IA.Succeeded()) { GrabAction = IA.Object; }

	// HUD: nạp WBP_HMI nếu đã tạo (đặt ở /Game/UI/WBP_HMI). Chưa có thì bỏ qua, không lỗi.
	static ConstructorHelpers::FClassFinder<UUserWidget> HmiBP(TEXT("/Game/UI/WBP_HMI"));
	if (HmiBP.Succeeded()) { HmiWidgetClass = HmiBP.Class; }
}

void AWiringPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsys =
			LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (WiringMappingContext)
			{
				Subsys->AddMappingContext(WiringMappingContext, MappingPriority);
			}
		}
	}

	// Cho phép vừa thấy con trỏ vừa nhận input game.
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	// Tạo HUD sau 1 nhịp ngắn để AHmiCaptureActor kịp tạo render target.
	FTimerHandle HudTimer;
	GetWorldTimerManager().SetTimer(HudTimer, this, &AWiringPlayerController::CreateHud, 0.2f, false);
}

void AWiringPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (GrabAction)
		{
			EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &AWiringPlayerController::OnGrabStarted);
			EIC->BindAction(GrabAction, ETriggerEvent::Completed, this, &AWiringPlayerController::OnGrabReleased);
		}
	}
}

void AWiringPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bIsHolding && HeldWire)
	{
		FVector Point;
		ATerminal* Hover = nullptr;
		if (GetDragWorldPoint(Point, Hover))
		{
			HeldWire->SetFreeEndWorldLocation(HeldEnd, Point);
		}
	}
}

EWireEnd AWiringPlayerController::OtherEnd(EWireEnd End)
{
	return (End == EWireEnd::A) ? EWireEnd::B : EWireEnd::A;
}

bool AWiringPlayerController::TraceCursor(ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const
{
	FVector Origin, Dir;
	if (!DeprojectMousePositionToWorld(Origin, Dir))
	{
		return false;
	}
	const FVector End = Origin + Dir * TraceDistance;

	FCollisionQueryParams Params(FName(TEXT("WiringCursor")), bTraceComplex);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}
	return GetWorld()->LineTraceSingleByChannel(OutHit, Origin, End, Channel, Params);
}

bool AWiringPlayerController::GetDragWorldPoint(FVector& OutPoint, ATerminal*& OutHoverTerminal) const
{
	OutHoverTerminal = nullptr;

	// 1) Hover trúng cọc -> dính vào điểm snap của cọc (preview).
	FHitResult Hit;
	if (TraceCursor(WIRING_TRACE_CHANNEL, false, HeldWire, Hit))
	{
		if (ATerminal* T = Cast<ATerminal>(Hit.GetActor()))
		{
			OutHoverTerminal = T;
			OutPoint = T->GetSnapLocation();
			return true;
		}
	}

	// 2) Trúng hình học thế giới -> bám mặt phẳng đó.
	if (TraceCursor(ECC_Visibility, true, HeldWire, Hit))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}

	// 3) Không trúng gì -> đặt cách camera một đoạn theo tia chuột.
	FVector Origin, Dir;
	if (DeprojectMousePositionToWorld(Origin, Dir))
	{
		OutPoint = Origin + Dir * 200.0f;
		return true;
	}
	return false;
}

void AWiringPlayerController::OnGrabStarted()
{
	FHitResult Hit;
	if (!TraceCursor(WIRING_TRACE_CHANNEL, false, nullptr, Hit))
	{
		return;
	}

	// a) Trúng grab head của một dây -> nhấc đầu đó ra để kéo.
	if (AWire* W = Cast<AWire>(Hit.GetActor()))
	{
		EWireEnd End;
		if (W->TryGetEndFromComponent(Hit.GetComponent(), End))
		{
			HeldWire = W;
			HeldEnd = End;
			bIsHolding = true;
			W->DetachEnd(End); // tách khỏi cọc (nếu đang cắm) để kéo tự do
			return;
		}
	}

	// b) Trúng cọc trống -> spawn dây mới, ghim đầu A vào cọc, kéo đầu B.
	if (ATerminal* T = Cast<ATerminal>(Hit.GetActor()))
	{
		if (T->bIsOccupied)
		{
			return;
		}

		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		UClass* ClassToSpawn = WireClass.Get() ? WireClass.Get() : AWire::StaticClass();
		AWire* NewWire = World->SpawnActor<AWire>(ClassToSpawn, T->GetSnapLocation(), FRotator::ZeroRotator);
		if (NewWire)
		{
			NewWire->AttachEndToTerminal(EWireEnd::A, T);
			NewWire->SetFreeEndWorldLocation(EWireEnd::B, T->GetSnapLocation());
			HeldWire = NewWire;
			HeldEnd = EWireEnd::B;
			bIsHolding = true;
		}
	}
}

void AWiringPlayerController::OnGrabReleased()
{
	if (!bIsHolding || !HeldWire)
	{
		bIsHolding = false;
		HeldWire = nullptr;
		return;
	}

	bool bSnapped = false;

	FHitResult Hit;
	if (TraceCursor(WIRING_TRACE_CHANNEL, false, HeldWire, Hit))
	{
		if (ATerminal* T = Cast<ATerminal>(Hit.GetActor()))
		{
			ATerminal* Other = HeldWire->GetTerminal(OtherEnd(HeldEnd));
			if (!T->bIsOccupied && T != Other)
			{
				HeldWire->AttachEndToTerminal(HeldEnd, T); // gắn + CheckConnection bên trong
				bSnapped = true;
			}
		}
	}

	if (!bSnapped)
	{
		// Thả hụt: nếu đầu kia cũng chưa nối -> bỏ dây; nếu đã nối -> để lủng lẳng.
		ATerminal* Other = HeldWire->GetTerminal(OtherEnd(HeldEnd));
		if (!Other)
		{
			HeldWire->Destroy();
		}
	}

	bIsHolding = false;
	HeldWire = nullptr;
}

void AWiringPlayerController::CreateHud()
{
	if (HmiWidget || !HmiWidgetClass)
	{
		return;
	}
	HmiWidget = CreateWidget<UUserWidget>(this, HmiWidgetClass);
	if (HmiWidget)
	{
		HmiWidget->AddToViewport();
	}
}
