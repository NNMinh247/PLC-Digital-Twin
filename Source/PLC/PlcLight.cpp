// PlcLight.cpp
#include "PlcLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
        Mesh->SetRelativeScale3D(FVector(0.3f));
    }

    // Gán material phát sáng có sẵn của engine (unlit emissive, có Vector Param "Color").
    // Nhờ vậy đèn tự sáng ĐỎ mà KHÔNG cần gán material thủ công cho model.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> EmissiveMat(
        TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    if (EmissiveMat.Succeeded())
    {
        Mesh->SetMaterial(0, EmissiveMat.Object);
    }

    // Đèn chỉ để hiển thị -> không va chạm (khỏi cản trace/click nối dây).
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Mesh);
    Glow->SetIntensity(0.0f);
    Glow->SetAttenuationRadius(150.0f);
    Glow->SetCastShadows(false);

    // Nhãn đánh dấu số đèn (H1..H8) — chỉ để nhìn trong editor, tự ẩn khi Play.
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Mesh);
    Label->SetAbsolute(false, false, true);      // bỏ ảnh hưởng scale 0.3 của Mesh (chữ không bị co)
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(18.0f);
    Label->SetTextRenderColor(FColor::Yellow);
    Label->SetHiddenInGame(true);
    Label->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f)); // *0.3 = ~27 uu trên tâm cầu
    Label->SetText(FText::FromString(TEXT("H1")));
}

void APlcLight::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateLabel(); // đổi LightIndex trong Details -> nhãn cập nhật ngay trong editor
}

void APlcLight::UpdateLabel()
{
    if (Label)
    {
        Label->SetVisibility(bShowLabel);
        Label->SetText(FText::FromString(FString::Printf(TEXT("H%d"), LightIndex + 1)));
    }
}

void APlcLight::BeginPlay()
{
    Super::BeginPlay();

    // Material động để đổi màu emissive lúc runtime (parent = EmissiveMeshMaterial gán ở constructor).
    if (Mesh && Mesh->GetMaterial(0))
    {
        DynMat = Mesh->CreateDynamicMaterialInstance(0);
    }

    SetOn(false); // mặc định tắt -> tàng hình
}

void APlcLight::SetOn(bool bOn)
{
    // 1) Tắt = tàng hình; bật = hiện quả cầu sáng đỏ
    if (Mesh)
    {
        Mesh->SetVisibility(bOn);
        if (bOn && DynMat)
        {
            const FLinearColor Emissive = OnColor * OnIntensity;
            DynMat->SetVectorParameterValue(TEXT("Color"), Emissive);         // param của EmissiveMeshMaterial
            DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive); // fallback nếu dùng material khác
        }
    }

    // 2) Point light hắt sáng đỏ ra môi trường
    if (Glow)
    {
        Glow->SetLightColor(OnColor);
        Glow->SetIntensity(bOn ? GlowIntensity : 0.0f);
    }
}
