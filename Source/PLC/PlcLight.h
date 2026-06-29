// PlcLight.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlcLight.generated.h"

UCLASS()
class PLC_API APlcLight : public AActor
{
    GENERATED_BODY()

public:
    APlcLight();

    // Số thứ tự đèn 0..7 (ứng với Y0..Y7). ĐẶT GIÁ TRỊ NÀY CHO TỪNG ĐÈN trong level.
    UPROPERTY(EditAnywhere, Category = "PLC")
    int32 LightIndex = 0;

    // Màu khi sáng
    UPROPERTY(EditAnywhere, Category = "PLC")
    FLinearColor OnColor = FLinearColor(0.05f, 1.0f, 0.1f);

    // Độ mạnh emissive khi bật
    UPROPERTY(EditAnywhere, Category = "PLC")
    float OnIntensity = 30.0f;

    // Bật/tắt đèn (đổi emissive material + point light)
    UFUNCTION(BlueprintCallable, Category = "PLC")
    void SetOn(bool bOn);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "PLC")
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, Category = "PLC")
    class UPointLightComponent* Glow;

    UPROPERTY()
    UMaterialInstanceDynamic* DynMat;
};
