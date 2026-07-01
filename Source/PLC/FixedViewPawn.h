// FixedViewPawn.h
// Pawn "xem cố định" cho digital twin: KHÔNG di chuyển, KHÔNG xoay bằng phím/chuột.
// Kế thừa DefaultPawn để giữ nguyên khung hình camera, chỉ bỏ các binding di chuyển/nhìn
// mặc định -> đứng yên một chỗ, tránh bấm nhầm (đây là digital twin, không phải game).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "FixedViewPawn.generated.h"

UCLASS()
class PLC_API AFixedViewPawn : public ADefaultPawn
{
    GENERATED_BODY()

public:
    AFixedViewPawn();
};
