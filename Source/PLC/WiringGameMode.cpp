// WiringGameMode.cpp
#include "WiringGameMode.h"
#include "WiringPlayerController.h"
#include "FixedViewPawn.h"

AWiringGameMode::AWiringGameMode()
{
	PlayerControllerClass = AWiringPlayerController::StaticClass();
	// Digital twin: camera cố định, không di chuyển (bỏ binding WASD/chuột của DefaultPawn).
	DefaultPawnClass = AFixedViewPawn::StaticClass();
}
