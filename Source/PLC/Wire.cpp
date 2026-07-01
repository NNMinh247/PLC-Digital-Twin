// Wire.cpp
#include "Wire.h"
#include "Terminal.h"
#include "WiringInteraction.h"
#include "CableComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AWire::AWire()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Cable: đầu A ghim ở gốc component, đầu B GHIM vào EndLocation (bAttachEnd=true).
	// Nếu để bAttachEnd=false thì đầu B là đầu rope tự do -> mô phỏng bay "lung tung".
	Cable = CreateDefaultSubobject<UCableComponent>(TEXT("Cable"));
	Cable->SetupAttachment(SceneRoot);
	Cable->bAttachStart = true;
	Cable->bAttachEnd = true;
	Cable->CableLength = CableLength;
	Cable->CableWidth = CableWidth;
	Cable->NumSegments = 8;
	Cable->NumSides = 6;
	Cable->EndLocation = FVector(1.0f, 0.0f, 0.0f);
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Dây căng thẳng, ổn định: bỏ trọng lực + solver cứng. 2 đầu đã ghim + CableLength = đúng
	// khoảng cách 2 đầu (đặt trong RefreshCable) -> cable là đoạn thẳng, không nhảy/không võng.
	Cable->CableGravityScale = 0.0f;
	Cable->SolverIterations = 16;

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

	// Va chạm THÂN dây: capsule dọc theo dây, Block WiringInteract để click trúng giữa dây.
	BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
	BodyCollision->SetupAttachment(SceneRoot);
	BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyCollision->SetCollisionObjectType(ECC_WorldDynamic);
	BodyCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyCollision->SetCollisionResponseToChannel(WIRING_TRACE_CHANNEL, ECR_Block);

	// GreenMaterial/RedMaterial để trống mặc định: màu do ResolveColorMaterial tạo runtime
	// (ưu tiên asset /Game/Data/M_Wire nếu có, không thì EmissiveMeshMaterial của engine).
	// Có thể gán tay material riêng cho từng instance trong editor nếu muốn.
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

void AWire::Destroyed()
{
	// Gỡ khỏi chồng của cả 2 cọc để không để lại con trỏ treo + cho dây còn lại tụt tầng.
	if (CurrentTerminal_A)
	{
		CurrentTerminal_A->DetachWire(this);
		CurrentTerminal_A = nullptr;
	}
	if (CurrentTerminal_B)
	{
		CurrentTerminal_B->DetachWire(this);
		CurrentTerminal_B = nullptr;
	}
	Super::Destroyed();
}

void AWire::RefreshGeometry()
{
	RefreshCable();
}

void AWire::RefreshCable()
{
	// Đầu gắn cọc -> lấy vị trí theo TẦNG của dây này trên cọc đó (nhiều dây cùng cọc thì xếp chồng).
	const FVector StartW = CurrentTerminal_A
		? CurrentTerminal_A->GetPlugLocation(CurrentTerminal_A->GetLayer(this))
		: FreeWorldA;
	const FVector EndW = CurrentTerminal_B
		? CurrentTerminal_B->GetPlugLocation(CurrentTerminal_B->GetLayer(this))
		: FreeWorldB;

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

	// Capsule va chạm bám theo THÂN dây (đoạn thẳng StartW->EndW): tâm ở giữa, trục Z dọc dây.
	if (BodyCollision)
	{
		const FVector Mid = (StartW + EndW) * 0.5f;
		const FVector Seg = EndW - StartW;
		const float Len = Seg.Size();
		const float Radius = FMath::Max(CableWidth, 3.0f);
		BodyCollision->SetWorldLocation(Mid);
		if (Len > KINDA_SMALL_NUMBER)
		{
			BodyCollision->SetWorldRotation(FRotationMatrix::MakeFromZ(Seg / Len).Rotator());
		}
		// HalfHeight tính cả 2 chỏm bán cầu -> phải >= Radius.
		BodyCollision->SetCapsuleSize(Radius, FMath::Max(Len * 0.5f, Radius));
	}
}

void AWire::ApplyColor(bool bConnected)
{
	if (!Cable)
	{
		return;
	}
	if (UMaterialInterface* M = ResolveColorMaterial(bConnected))
	{
		Cable->SetMaterial(0, M);
	}
}

UMaterialInterface* AWire::ResolveColorMaterial(bool bConnected)
{
	// 1) Ưu tiên material asset nếu người dùng gán tay cho instance.
	if (UMaterialInterface* Asset = bConnected ? GreenMaterial : RedMaterial)
	{
		return Asset;
	}

	// 2) Tạo dynamic material từ base có param "Color" + "Opacity".
	//    Ưu tiên /Game/Data/M_Wire (unlit, TRANSLUCENT alpha -> màu đặc, đục 80%).
	//    Nếu chưa có thì lùi về EmissiveMeshMaterial của engine (additive -> dễ mờ trên nền sáng).
	if (!BaseColorMaterial)
	{
		BaseColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Data/M_Wire.M_Wire"));
		if (!BaseColorMaterial)
		{
			BaseColorMaterial = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
		}
	}
	if (!BaseColorMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic*& MID = bConnected ? GreenMID : RedMID;
	if (!MID)
	{
		MID = UMaterialInstanceDynamic::Create(BaseColorMaterial, this);
		if (MID)
		{
			const FLinearColor C = bConnected
				? FLinearColor(0.05f, 1.0f, 0.12f)    // xanh: đã nối đủ 2 đầu
				: FLinearColor(1.0f, 0.05f, 0.05f);   // đỏ: chưa nối xong
			MID->SetVectorParameterValue(TEXT("Color"), C);
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.8f);
		}
	}
	return MID;
}

void AWire::SetTerminal(EWireEnd End, ATerminal* T)
{
	ATerminal*& Slot = (End == EWireEnd::A) ? CurrentTerminal_A : CurrentTerminal_B;
	if (Slot == T)
	{
		return;
	}

	// Gỡ khỏi chồng của cọc cũ (các dây còn lại tụt tầng + tự refresh).
	if (Slot)
	{
		Slot->DetachWire(this);
	}
	Slot = T;
	// Thêm vào chồng của cọc mới -> nhận tầng kế tiếp.
	if (T)
	{
		T->AttachWire(this);
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
	CheckConnection();   // chỉ còn 1 đầu -> chuyển về đỏ
}

void AWire::SetFreeEndWorldLocation(EWireEnd End, const FVector& WorldLocation)
{
	if (End == EWireEnd::A) { FreeWorldA = WorldLocation; }
	else { FreeWorldB = WorldLocation; }
	RefreshCable();
}

bool AWire::CheckConnection()
{
	// Tạm thời bỏ logic đúng/sai theo GameplayTag: chỉ cần đủ 2 đầu là "nối xong" -> xanh.
	// (Khi cần kiểm tra cắm đúng cặp thì so TerminalTag/TargetPartnerTag ở đây.)
	const bool bConnected = (CurrentTerminal_A != nullptr && CurrentTerminal_B != nullptr);
	ApplyColor(bConnected);
	return bConnected;
}

ATerminal* AWire::GetTerminal(EWireEnd End) const
{
	return (End == EWireEnd::A) ? CurrentTerminal_A : CurrentTerminal_B;
}

FVector AWire::GetEndWorldLocation(EWireEnd End) const
{
	if (End == EWireEnd::A)
	{
		return CurrentTerminal_A
			? CurrentTerminal_A->GetPlugLocation(CurrentTerminal_A->GetLayer(this))
			: FreeWorldA;
	}
	return CurrentTerminal_B
		? CurrentTerminal_B->GetPlugLocation(CurrentTerminal_B->GetLayer(this))
		: FreeWorldB;
}

bool AWire::TryGetEndFromComponent(const UPrimitiveComponent* Comp, EWireEnd& OutEnd) const
{
	if (Comp == GrabHeadA) { OutEnd = EWireEnd::A; return true; }
	if (Comp == GrabHeadB) { OutEnd = EWireEnd::B; return true; }
	return false;
}
