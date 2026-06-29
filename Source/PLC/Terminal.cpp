// Terminal.cpp
#include "Terminal.h"
#include "WiringInteraction.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"

ATerminal::ATerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Hình hiển thị – mặc định trụ engine cho dễ thấy, đổi tùy ý trong editor.
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SceneRoot);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		MeshComp->SetStaticMesh(CylinderAsset.Object);
		MeshComp->SetWorldScale3D(FVector(0.1f, 0.1f, 0.1f));
	}

	// Vùng va chạm để trace dưới chuột trúng và làm đích snap.
	SnapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SnapSphere"));
	SnapSphere->SetupAttachment(SceneRoot);
	SnapSphere->InitSphereRadius(8.0f);
	SnapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SnapSphere->SetCollisionObjectType(ECC_WorldDynamic);
	SnapSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	SnapSphere->SetCollisionResponseToChannel(WIRING_TRACE_CHANNEL, ECR_Block);

	// Điểm gắn đầu dây.
	SnapPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SnapPoint"));
	SnapPoint->SetupAttachment(SnapSphere);
}

FVector ATerminal::GetSnapLocation() const
{
	return SnapPoint ? SnapPoint->GetComponentLocation() : GetActorLocation();
}
