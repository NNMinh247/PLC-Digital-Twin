// PlcLight.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlcLight.generated.h"

class UTextRenderComponent;

UCLASS()
class PLC_API APlcLight : public AActor
{
    GENERATED_BODY()

public:
    APlcLight();

    // Số thứ tự đèn 0..7 (ứng với Y0..Y7 / nhãn H1..H8). ĐẶT GIÁ TRỊ NÀY CHO TỪNG ĐÈN trong level.
    // Nhãn trong editor sẽ hiện "H{LightIndex+1}" để đánh dấu, tránh xếp lẫn.
    UPROPERTY(EditAnywhere, Category = "PLC")
    int32 LightIndex = 0;

    // Màu khi sáng (mặc định ĐỎ)
    UPROPERTY(EditAnywhere, Category = "PLC")
    FLinearColor OnColor = FLinearColor(1.0f, 0.0f, 0.0f);

    // Độ mạnh emissive khi bật (nhân với OnColor để lên bloom)
    UPROPERTY(EditAnywhere, Category = "PLC")
    float OnIntensity = 30.0f;

    // Cường độ point light hắt ra môi trường khi bật
    UPROPERTY(EditAnywhere, Category = "PLC")
    float GlowIntensity = 5000.0f;

    // Hiện nhãn "H1..H8" trong editor để đánh dấu đèn (luôn tự ẩn khi Play)
    UPROPERTY(EditAnywhere, Category = "PLC")
    bool bShowLabel = true;

    // Bật/tắt đèn: tắt = tàng hình; bật = quả cầu sáng đỏ + point light
    UFUNCTION(BlueprintCallable, Category = "PLC")
    void SetOn(bool bOn);

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    // Cập nhật chữ nhãn theo LightIndex (chạy trong editor khi đổi Details)
    void UpdateLabel();

    UPROPERTY(VisibleAnywhere, Category = "PLC")
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, Category = "PLC")
    class UPointLightComponent* Glow;

    // Nhãn đánh dấu số đèn (H1..H8) — chỉ hiện trong editor
    UPROPERTY(VisibleAnywhere, Category = "PLC")
    UTextRenderComponent* Label;

    UPROPERTY()
    UMaterialInstanceDynamic* DynMat;
};
