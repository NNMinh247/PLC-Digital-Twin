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
	PlacingEnd = EWireEnd::B;

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
	// Không có IA_Grab -> KHÔNG dùng BindKey (không kích hoạt ổn trong GameAndUI).
	// Thay vào đó PlayerTick sẽ dò cạnh nhấn chuột trái (xem PlayerTick).

	// DEBUG: xác nhận đã bind theo đường nào (xem trong Output Log lúc Play).
	UE_LOG(LogTemp, Warning, TEXT("[WIRING] SetupInputComponent: GrabAction=%d InputComponent=%d -> input path '%s'"),
		GrabAction != nullptr, InputComponent != nullptr,
		GrabAction ? TEXT("EnhancedInput(IA_Grab)") : TEXT("PlayerTick poll LMB"));
}

void AWiringPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Fallback input: khi chưa có IA_Grab, dò cạnh nhấn chuột trái tại đây.
	if (!GrabAction)
	{
		const bool bDown = IsInputKeyDown(EKeys::LeftMouseButton);
		if (bDown && !bLmbWasDown)
		{
			OnClick();
		}
		bLmbWasDown = bDown;
	}

	if (bPlacing && PendingWire)
	{
		FVector Point;
		ATerminal* Hover = nullptr;
		if (GetDragWorldPoint(Point, Hover))
		{
			PendingWire->SetFreeEndWorldLocation(PlacingEnd, Point);
		}
	}
}

// Đầu còn lại của một đầu dây.
static EWireEnd OtherEnd(EWireEnd End)
{
	return End == EWireEnd::A ? EWireEnd::B : EWireEnd::A;
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

ATerminal* AWiringPlayerController::ResolveTerminalFromHit(const FHitResult& Hit) const
{
	if (ATerminal* T = Cast<ATerminal>(Hit.GetActor()))
	{
		return T;
	}
	// Trúng ĐẦU dây (grab head) đã cắm -> trả về cọc mà đầu đó đang cắm (để cắm chồng thêm tầng).
	// Lưu ý: trúng THÂN dây (capsule) thì TryGetEndFromComponent trả false -> bỏ qua (không phải cọc).
	if (AWire* W = Cast<AWire>(Hit.GetActor()))
	{
		EWireEnd End;
		if (W->TryGetEndFromComponent(Hit.GetComponent(), End))
		{
			return W->GetTerminal(End);
		}
	}
	return nullptr;
}

bool AWiringPlayerController::MultiTraceCursor(TArray<FHitResult>& OutHits, AActor* IgnoreActor) const
{
	OutHits.Reset();
	FVector Origin, Dir;
	if (!GetCursorRay(Origin, Dir))
	{
		return false;
	}
	const FVector End = Origin + Dir * TraceDistance;
	FCollisionQueryParams Params(FName(TEXT("WiringMulti")), false);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}
	GetWorld()->LineTraceMultiByChannel(OutHits, Origin, End, WIRING_TRACE_CHANNEL, Params);
	return OutHits.Num() > 0;
}

bool AWiringPlayerController::FindTerminalUnderCursor(ATerminal*& OutTerm, AActor* IgnoreActor) const
{
	OutTerm = nullptr;
	// Multi-trace + lấy cọc GẦN NHẤT, bỏ qua thân dây (capsule không map ra cọc) -> thân dây
	// không che việc chọn cọc ngay cả khi dây nằm trước cọc.
	TArray<FHitResult> Hits;
	MultiTraceCursor(Hits, IgnoreActor);
	for (const FHitResult& H : Hits)
	{
		if (ATerminal* T = ResolveTerminalFromHit(H))
		{
			OutTerm = T;
			return true;
		}
	}
	return false;
}

bool AWiringPlayerController::GetDragWorldPoint(FVector& OutPoint, ATerminal*& OutHoverTerminal) const
{
	OutHoverTerminal = nullptr;

	// 1) Hover trúng cọc (hoặc tầng dây trên cọc) -> dính vào tầng KẾ TIẾP mà dây sẽ nhận.
	ATerminal* T = nullptr;
	if (FindTerminalUnderCursor(T, PendingWire) && T)
	{
		OutHoverTerminal = T;
		OutPoint = T->GetPlugLocation(T->GetStackCount());
		return true;
	}

	// 2) Không hover cọc -> chiếu con trỏ lên MẶT PHẲNG đi qua đầu CỐ ĐỊNH, đối diện camera.
	//    Giữ đầu tự do trượt đúng độ sâu của bàn -> preview mượt, không giật.
	FVector Origin, Dir;
	if (GetCursorRay(Origin, Dir))
	{
		const ATerminal* FixedT = PendingWire ? PendingWire->GetTerminal(OtherEnd(PlacingEnd)) : nullptr;
		const FVector PlanePoint = FixedT ? FixedT->GetSnapLocation() : (Origin + Dir * 200.0f);
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
	// Alt + click: xoá dây (trúng thân dây) hoặc tháo đầu ở node. Chỉ khi CHƯA đang đặt dây.
	if (!bPlacing && (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt)))
	{
		HandleAltClick();
		return;
	}

	// Cọc gần nhất dưới con trỏ (bỏ qua thân dây; bỏ qua dây đang đặt).
	ATerminal* HitTerm = nullptr;
	FindTerminalUnderCursor(HitTerm, PendingWire);

	UE_LOG(LogTemp, Log, TEXT("[WIRING] OnClick Placing=%d term=%s"), bPlacing, *GetNameSafe(HitTerm));

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
		PlacingEnd = EWireEnd::B;          // đầu B bám chuột
		PlacingOriginTerminal = nullptr;   // dây mới -> huỷ thì xoá hẳn
		bPlacing = true;
		return;
	}

	// ===== Click 2: hoàn tất hoặc huỷ dây đang đặt =====
	// Đầu cố định là đầu còn lại của đầu đang kéo.
	ATerminal* FixedTerm = PendingWire ? PendingWire->GetTerminal(OtherEnd(PlacingEnd)) : nullptr;

	if (PendingWire && HitTerm && HitTerm != FixedTerm)
	{
		// Nối đầu đang kéo vào cọc khác -> hoàn tất (một cọc cắm được nhiều dây).
		PendingWire->AttachEndToTerminal(PlacingEnd, HitTerm); // gắn + CheckConnection (xanh)
	}
	else if (PendingWire)
	{
		// Click chỗ trống / trúng chính đầu cố định -> huỷ.
		if (PlacingOriginTerminal)
		{
			// Dây có sẵn vừa tháo ra -> gắn lại về cọc gốc (khôi phục, không mất dây).
			PendingWire->AttachEndToTerminal(PlacingEnd, PlacingOriginTerminal);
		}
		else
		{
			// Dây vừa spawn -> xoá hẳn.
			PendingWire->Destroy();
		}
	}

	PendingWire = nullptr;
	bPlacing = false;
	PlacingOriginTerminal = nullptr;
	PlacingEnd = EWireEnd::B;
}

void AWiringPlayerController::HandleAltClick()
{
	TArray<FHitResult> Hits;
	if (!MultiTraceCursor(Hits, nullptr))
	{
		return;
	}

	// Gom: có trúng NODE (cọc) không, và có trúng DÂY (thân/đầu) không.
	ATerminal* HitNode = nullptr;
	AWire* HitWire = nullptr;
	for (const FHitResult& H : Hits)
	{
		if (!HitNode) { HitNode = Cast<ATerminal>(H.GetActor()); }
		if (!HitWire) { HitWire = Cast<AWire>(H.GetActor()); }
	}

	UE_LOG(LogTemp, Log, TEXT("[WIRING] AltClick node=%s wire=%s"), *GetNameSafe(HitNode), *GetNameSafe(HitWire));

	// Ưu tiên NODE: click vào node -> THÁO đầu dây trên cùng ở node đó rồi kéo đi nối chỗ khác.
	if (HitNode)
	{
		AWire* W = HitNode->GetTopWire();
		if (!W)
		{
			return; // node chưa có dây -> không có gì để tháo
		}
		const EWireEnd E = (W->GetTerminal(EWireEnd::A) == HitNode) ? EWireEnd::A : EWireEnd::B;
		W->DetachEnd(E);                    // tháo đầu E (đầu kia vẫn giữ), dây thành đỏ
		PendingWire = W;
		PlacingEnd = E;                     // kéo đầu vừa tháo theo chuột
		PlacingOriginTerminal = HitNode;    // huỷ -> gắn lại về đây
		bPlacing = true;
		return;
	}

	// Không trúng node mà trúng THÂN/ĐẦU dây -> xoá cả dây.
	if (HitWire)
	{
		HitWire->Destroy();
	}
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
