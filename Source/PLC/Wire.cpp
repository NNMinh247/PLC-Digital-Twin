// Wire.cpp
#include "Wire.h"
#include "Terminal.h"
#include "WiringInteraction.h"
#include "CableComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AWire::AWire()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Cable: đầu A = gốc component, đầu B = EndLocation (ta tự điều khiển).
	Cable = CreateDefaultSubobject<UCableComponent>(TEXT("Cable"));
	Cable->SetupAttachment(SceneRoot);
	Cable->bAttachStart = true;
	Cable->bAttachEnd = false;
	Cable->CableLength = CableLength;
	Cable->CableWidth = CableWidth;
	Cable->NumSegments = 10;
	Cable->NumSides = 4;
	Cable->EndLocation = FVector(1.0f, 0.0f, 0.0f);
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Ổn định preview: bỏ trọng lực (không rơi/đung đưa như dây thừng) + solver cứng hơn.
	// -> khi rê chuột, dây bám thẳng theo 2 đầu, không "nhảy". Muốn dây võng thì tăng CableGravityScale.
	Cable->CableGravityScale = 0.0f;
	Cable->SolverIterations = 8;

	// Grab head 2 đầu: Block kênh WiringInteract để chuột trace trúng.
	auto MakeHead = [this](const TCHAR* Name) -> USphereComponent*
	{
		USphereComponent* S = CreateDefaultSubobject<USphereComponent>(Name);
		S->SetupAttachment(SceneRoot);
		S->InitSphereRadius(5.0f);
		S->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		S->SetCollisionObjectType(ECC_WorldDynamic);
		S->SetCollisionResponseToAllChannels(ECR_Ignore);
		S->SetCollisionResponseToChannel(WIRING_TRACE_CHANNEL, ECR_Block);
		return S;
	};
	GrabHeadA = MakeHead(TEXT("GrabHeadA"));
	GrabHeadB = MakeHead(TEXT("GrabHeadB"));

	// Material xanh/đỏ mặc định (đổi được trong editor cho từng instance).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreenAsset(
		TEXT("/Game/Data/M_Wire_Green.M_Wire_Green"));
	if (GreenAsset.Succeeded()) { GreenMaterial = GreenAsset.Object; }

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedAsset(
		TEXT("/Game/Data/M_Wire_Red.M_Wire_Red"));
	if (RedAsset.Succeeded()) { RedMaterial = RedAsset.Object; }
}

void AWire::BeginPlay()
{
	Super::BeginPlay();

	// Khởi tạo vị trí đầu tự do = vị trí actor để cable không bị "dồn" về gốc thế giới.
	FreeWorldA = GetActorLocation();
	FreeWorldB = GetActorLocation();

	if (Cable)
	{
		Cable->CableWidth = CableWidth;
	}
	RefreshCable();
}

void AWire::RefreshCable()
{
	const FVector StartW = CurrentTerminal_A ? CurrentTerminal_A->GetSnapLocation() : FreeWorldA;
	const FVector EndW = CurrentTerminal_B ? CurrentTerminal_B->GetSnapLocation() : FreeWorldB;

	// Đầu A = gốc actor.
	SetActorLocation(StartW);

	// Grab head A ở gốc (đã attach root), đặt lại world cho chắc.
	if (GrabHeadA) { GrabHeadA->SetWorldLocation(StartW); }

	// Đầu B = EndLocation tương đối so với cable.
	if (Cable)
	{
		const FVector EndRel = Cable->GetComponentTransform().InverseTransformPosition(EndW);
		Cable->EndLocation = EndRel;
		Cable->CableLength = FMath::Max(1.0f, FVector::Dist(StartW, EndW));
	}
	if (GrabHeadB) { GrabHeadB->SetWorldLocation(EndW); }
}

void AWire::ApplyColor(bool bValid)
{
	UMaterialInterface* M = bValid ? GreenMaterial : RedMaterial;
	if (M && Cable)
	{
		Cable->SetMaterial(0, M);
	}
}

void AWire::SetTerminal(EWireEnd End, ATerminal* T)
{
	ATerminal*& Slot = (End == EWireEnd::A) ? CurrentTerminal_A : CurrentTerminal_B;

	// Giải phóng cọc cũ nếu khác
	if (Slot && Slot != T)
	{
		Slot->bIsOccupied = false;
	}
	Slot = T;
	if (T)
	{
		T->bIsOccupied = true;
	}
}

void AWire::AttachEndToTerminal(EWireEnd End, ATerminal* Terminal)
{
	if (!Terminal)
	{
		return;
	}
	SetTerminal(End, Terminal);
	RefreshCable();
	CheckConnection();
}

void AWire::DetachEnd(EWireEnd End)
{
	// Lưu vị trí hiện tại làm điểm tự do để cable không nhảy.
	const FVector Current = GetEndWorldLocation(End);
	SetTerminal(End, nullptr);
	if (End == EWireEnd::A) { FreeWorldA = Current; }
	else { FreeWorldB = Current; }
	RefreshCable();
}

void AWire::SetFreeEndWorldLocation(EWireEnd End, const FVector& WorldLocation)
{
	if (End == EWireEnd::A) { FreeWorldA = WorldLocation; }
	else { FreeWorldB = WorldLocation; }
	RefreshCable();
}

bool AWire::CheckConnection()
{
	if (!CurrentTerminal_A || !CurrentTerminal_B)
	{
		// Chưa đủ 2 đầu -> coi như chưa hợp lệ, để màu đỏ.
		ApplyColor(false);
		return false;
	}

	const FGameplayTag ATag = CurrentTerminal_A->TerminalTag;
	const FGameplayTag BTag = CurrentTerminal_B->TerminalTag;
	const FGameplayTag AWant = CurrentTerminal_A->TargetPartnerTag;
	const FGameplayTag BWant = CurrentTerminal_B->TargetPartnerTag;

	bool bValid = false;
	if (AWant.IsValid() && BWant.IsValid())
	{
		bValid = AWant.MatchesTagExact(BTag) && BWant.MatchesTagExact(ATag);
	}
	else if (AWant.IsValid())
	{
		bValid = AWant.MatchesTagExact(BTag);
	}
	else if (BWant.IsValid())
	{
		bValid = BWant.MatchesTagExact(ATag);
	}

	ApplyColor(bValid);
	return bValid;
}

ATerminal* AWire::GetTerminal(EWireEnd End) const
{
	return (End == EWireEnd::A) ? CurrentTerminal_A : CurrentTerminal_B;
}

FVector AWire::GetEndWorldLocation(EWireEnd End) const
{
	if (End == EWireEnd::A)
	{
		return CurrentTerminal_A ? CurrentTerminal_A->GetSnapLocation() : FreeWorldA;
	}
	return CurrentTerminal_B ? CurrentTerminal_B->GetSnapLocation() : FreeWorldB;
}

bool AWire::TryGetEndFromComponent(const UPrimitiveComponent* Comp, EWireEnd& OutEnd) const
{
	if (Comp == GrabHeadA) { OutEnd = EWireEnd::A; return true; }
	if (Comp == GrabHeadB) { OutEnd = EWireEnd::B; return true; }
	return false;
}
