// PlcLight.cpp
#include "PlcLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APlcLight::APlcLight()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;

    // Gán sẵn hình cầu của engine cho dễ nhìn (đổi mesh khác trong editor nếu muốn)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereAsset.Succeeded())
    {
        Mesh->SetStaticMesh(SphereAsset.Object);
        Mesh->SetWorldScale3D(FVector(0.3f));
    }

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Mesh);
    Glow->SetIntensity(0.0f);
    Glow->SetAttenuationRadius(150.0f);
}

void APlcLight::BeginPlay()
{
    Super::BeginPlay();

    // Tạo material động để đổi emissive lúc runtime.
    // YÊU CẦU: vật liệu gán ở slot 0 phải có Vector Parameter tên "EmissiveColor".
    if (Mesh && Mesh->GetMaterial(0))
    {
        DynMat = Mesh->CreateDynamicMaterialInstance(0);
    }

    SetOn(false); // mặc định tắt
}

void APlcLight::SetOn(bool bOn)
{
    // 1) Đổi emissive trên material
    if (DynMat)
    {
        const FLinearColor Emissive = bOn ? (OnColor * OnIntensity) : FLinearColor::Black;
        DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
    }

    // 2) Bật/tắt point light để có ánh sáng hắt ra môi trường
    if (Glow)
    {
        Glow->SetLightColor(OnColor);
        Glow->SetIntensity(bOn ? 5000.0f : 0.0f);
    }
}
