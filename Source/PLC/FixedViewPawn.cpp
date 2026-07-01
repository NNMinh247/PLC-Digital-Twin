// FixedViewPawn.cpp
#include "FixedViewPawn.h"

AFixedViewPawn::AFixedViewPawn()
{
    // Tắt binding di chuyển/nhìn mặc định của DefaultPawn (WASD, chuột xoay...).
    // -> Camera đứng yên tại vị trí spawn; phím không làm pawn nhúc nhích.
    bAddDefaultMovementBindings = false;
}
