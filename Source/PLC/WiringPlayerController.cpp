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
#include "Engine/Engine.h"          // GEngine->AddOnScreenDebugMessage (debug)
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

	// Tạo HUD NGAY để 4 ô HMI phủ kín màn hình từ frame đầu (không thấy viewport thô).
	// Render target có thể chưa sẵn ở thời điểm này -> UHmiWidget tự thử lại trong NativeTick.
	CreateHud();
}

void AWiringPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Ưu tiên Enhanced Input nếu đã có asset IA_Grab (/Game/Inputs/IA_Grab).
	if (GrabAction)
	{
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		{
			// Click-click: chỉ cần sự kiện nhấn (Started) cho cả bắt đầu và hoàn tất dây.
			EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &AWiringPlayerController::OnClick);
		}
	}
	else if (InputComponent)
	{
		// Fallback thuần C++: chưa migrate IA_Grab/IMC_Wiring -> bind thẳng chuột trái.
		// Nhờ vậy nối dây chạy được ngay cả khi thiếu asset Enhanced Input.
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AWiringPlayerController::OnClick);
	}

	// DEBUG: xác nhận đã bind theo đường nào (xem trong Output Log lúc Play).
	UE_LOG(LogTemp, Warning, TEXT("[WIRING] SetupInputComponent: GrabAction=%d InputComponent=%d -> bind '%s'"),
		GrabAction != nullptr, InputComponent != nullptr,
		GrabAction ? TEXT("EnhancedInput(IA_Grab)") : TEXT("Fallback LMB"));
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

	// 2) Không hover cọc -> chiếu con trỏ lên MẶT PHẲNG bàn (đi qua đầu A, đối diện camera).
	//    Giữ đầu tự do trượt đúng độ sâu của bàn -> preview mượt, không giật theo hình học lồi lõm.
	FVector Origin, Dir;
	if (GetCursorRay(Origin, Dir))
	{
		const ATerminal* StartT = PendingWire ? PendingWire->GetTerminal(EWireEnd::A) : nullptr;
		const FVector PlanePoint = StartT ? StartT->GetSnapLocation() : (Origin + Dir * 200.0f);
		const FVector PlaneNormal = (Origin - PlanePoint).GetSafeNormal(); // mặt phẳng hướng về camera
		if (!PlaneNormal.IsNearlyZero())
		{
			OutPoint = FMath::RayPlaneIntersection(Origin, Dir, FPlane(PlanePoint, PlaneNormal));
		}
		else
		{
			OutPoint = Origin + Dir * 200.0f;
		}
		return true;
	}
	return false;
}

void AWiringPlayerController::OnClick()
{
	// ===== DEBUG 1: click có TỚI controller không + con trỏ có trong ô board không =====
	{
		const bool bOverBoard = Hmi ? Hmi->IsCursorOverBoard() : false;
		UE_LOG(LogTemp, Warning, TEXT("[WIRING] OnClick fired. Hmi=%d OverBoard=%d bPlacing=%d"),
			Hmi != nullptr, bOverBoard, bPlacing);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
				FString::Printf(TEXT("[WIRING] Click! Hmi=%d OverBoard=%d Placing=%d"),
					Hmi != nullptr, bOverBoard, bPlacing));
		}
	}

	// Alt + click -> xoá dây (không tạo/nối dây).
	if (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))
	{
		DeleteWireUnderCursor();
		return;
	}

	FHitResult Hit;
	const bool bHit = TraceCursor(WIRING_TRACE_CHANNEL, false, PendingWire, Hit);
	ATerminal* HitTerm = bHit ? Cast<ATerminal>(Hit.GetActor()) : nullptr;

	// ===== DEBUG 2: tia trace có TRÚNG cọc không =====
	UE_LOG(LogTemp, Warning, TEXT("[WIRING]   trace bHit=%d actor='%s' isTerminal=%d"),
		bHit, *GetNameSafe(Hit.GetActor()), HitTerm != nullptr);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, HitTerm ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("[WIRING] trace=%s"), bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("MISS")));
	}

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
