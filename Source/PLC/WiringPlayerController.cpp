// WiringPlayerController.cpp
#include "WiringPlayerController.h"
#include "Wire.h"
#include "Terminal.h"
#include "WiringInteraction.h"
#include "HmiWidget.h"

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
#include "EngineUtils.h"           // TActorIterator (FindWireOnTerminal)
#include "InputCoreTypes.h"        // EKeys (Alt)

AWiringPlayerController::AWiringPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;

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
			// Click-click: chỉ cần sự kiện nhấn (Started) cho cả bắt đầu và hoàn tất dây.
			EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &AWiringPlayerController::OnClick);
		}
	}
}

void AWiringPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bPlacing && PendingWire)
	{
		FVector Point;
		ATerminal* Hover = nullptr;
		if (GetDragWorldPoint(Point, Hover))
		{
			PendingWire->SetFreeEndWorldLocation(EWireEnd::B, Point);
		}
	}
}

bool AWiringPlayerController::GetCursorRay(FVector& OutOrigin, FVector& OutDir) const
{
	// 1) Con trỏ trong ô board -> chiếu ngược qua camera capture (Cách A).
	if (Hmi && Hmi->GetBoardRayUnderCursor(OutOrigin, OutDir))
	{
		return true;
	}

	// 2) Fallback: deproject từ viewport chính (khi chưa có HUD, hoặc con trỏ ngoài ô board).
	FVector Origin, Dir;
	if (DeprojectMousePositionToWorld(Origin, Dir))
	{
		OutOrigin = Origin;
		OutDir = Dir;
		return true;
	}
	return false;
}

bool AWiringPlayerController::TraceRay(const FVector& Origin, const FVector& Dir, ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const
{
	const FVector End = Origin + Dir * TraceDistance;

	FCollisionQueryParams Params(FName(TEXT("WiringCursor")), bTraceComplex);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}
	return GetWorld()->LineTraceSingleByChannel(OutHit, Origin, End, Channel, Params);
}

bool AWiringPlayerController::TraceCursor(ECollisionChannel Channel, bool bTraceComplex, AActor* IgnoreActor, FHitResult& OutHit) const
{
	FVector Origin, Dir;
	if (!GetCursorRay(Origin, Dir))
	{
		return false;
	}
	return TraceRay(Origin, Dir, Channel, bTraceComplex, IgnoreActor, OutHit);
}

bool AWiringPlayerController::GetDragWorldPoint(FVector& OutPoint, ATerminal*& OutHoverTerminal) const
{
	OutHoverTerminal = nullptr;

	// 1) Hover trúng cọc -> dính vào điểm snap của cọc (preview).
	FHitResult Hit;
	if (TraceCursor(WIRING_TRACE_CHANNEL, false, PendingWire, Hit))
	{
		if (ATerminal* T = Cast<ATerminal>(Hit.GetActor()))
		{
			OutHoverTerminal = T;
			OutPoint = T->GetSnapLocation();
			return true;
		}
	}

	// 2) Trúng hình học thế giới -> bám mặt phẳng đó.
	if (TraceCursor(ECC_Visibility, true, PendingWire, Hit))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}

	// 3) Không trúng gì -> đặt cách nguồn một đoạn theo tia con trỏ.
	FVector Origin, Dir;
	if (GetCursorRay(Origin, Dir))
	{
		OutPoint = Origin + Dir * 200.0f;
		return true;
	}
	return false;
}

void AWiringPlayerController::OnClick()
{
	// Alt + click -> xoá dây (không tạo/nối dây).
	if (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))
	{
		DeleteWireUnderCursor();
		return;
	}

	FHitResult Hit;
	const bool bHit = TraceCursor(WIRING_TRACE_CHANNEL, false, PendingWire, Hit);
	ATerminal* HitTerm = bHit ? Cast<ATerminal>(Hit.GetActor()) : nullptr;

	if (!bPlacing)
	{
		// ===== Click 1: bắt đầu dây từ một cọc =====
		if (!HitTerm)
		{
			return; // click hụt -> không làm gì
		}

		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		UClass* ClassToSpawn = WireClass.Get() ? WireClass.Get() : AWire::StaticClass();
		AWire* NewWire = World->SpawnActor<AWire>(ClassToSpawn, HitTerm->GetSnapLocation(), FRotator::ZeroRotator);
		if (!NewWire)
		{
			return;
		}

		NewWire->AttachEndToTerminal(EWireEnd::A, HitTerm);       // ghim đầu A vào cọc vừa click
		NewWire->SetFreeEndWorldLocation(EWireEnd::B, HitTerm->GetSnapLocation());
		PendingWire = NewWire;
		bPlacing = true;
		return;
	}

	// ===== Click 2: hoàn tất hoặc huỷ dây đang đặt =====
	ATerminal* StartTerm = PendingWire ? PendingWire->GetTerminal(EWireEnd::A) : nullptr;

	if (PendingWire && HitTerm && HitTerm != StartTerm)
	{
		// Nối đầu B vào cọc khác -> hoàn tất. Cho phép nhiều dây chung 1 cọc (không chặn occupied).
		PendingWire->AttachEndToTerminal(EWireEnd::B, HitTerm); // gắn + CheckConnection bên trong
	}
	else
	{
		// Click vào chỗ trống hoặc chính cọc bắt đầu -> huỷ dây đang đặt.
		if (PendingWire)
		{
			PendingWire->Destroy();
		}
	}

	PendingWire = nullptr;
	bPlacing = false;
}

void AWiringPlayerController::DeleteWireUnderCursor()
{
	FVector Origin, Dir;
	if (!GetCursorRay(Origin, Dir))
	{
		return;
	}
	const FVector End = Origin + Dir * TraceDistance;

	// Multi-trace để bắt được đầu dây kể cả khi nằm sau cọc (đầu dây ghim tại cọc).
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(FName(TEXT("WiringDelete")), false);
	if (PendingWire)
	{
		Params.AddIgnoredActor(PendingWire); // đừng xoá dây đang đặt dở
	}
	GetWorld()->LineTraceMultiByChannel(Hits, Origin, End, WIRING_TRACE_CHANNEL, Params);

	// 1) Ưu tiên: trúng thẳng một dây (grab head).
	const ATerminal* HitTerm = nullptr;
	for (const FHitResult& H : Hits)
	{
		if (AWire* W = Cast<AWire>(H.GetActor()))
		{
			W->Destroy();
			return;
		}
		if (!HitTerm)
		{
			HitTerm = Cast<ATerminal>(H.GetActor());
		}
	}

	// 2) Không trúng đầu dây nhưng trúng cọc -> xoá 1 dây đang nối vào cọc đó.
	if (HitTerm)
	{
		if (AWire* W = FindWireOnTerminal(HitTerm))
		{
			W->Destroy();
		}
	}
}

AWire* AWiringPlayerController::FindWireOnTerminal(const ATerminal* T) const
{
	if (!T)
	{
		return nullptr;
	}
	for (TActorIterator<AWire> It(GetWorld()); It; ++It)
	{
		AWire* W = *It;
		if (W && W != PendingWire &&
			(W->GetTerminal(EWireEnd::A) == T || W->GetTerminal(EWireEnd::B) == T))
		{
			return W;
		}
	}
	return nullptr;
}

void AWiringPlayerController::CreateHud()
{
	if (Hmi || !HmiWidgetClass)
	{
		return;
	}
	UUserWidget* Widget = CreateWidget<UUserWidget>(this, HmiWidgetClass);
	Hmi = Cast<UHmiWidget>(Widget);   // cần cho click-chiếu-ngược trong ô board
	if (Widget)
	{
		Widget->AddToViewport();
	}
}
