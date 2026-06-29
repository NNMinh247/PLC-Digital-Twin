// WiringGameMode.cpp
#include "WiringGameMode.h"
#include "WiringPlayerController.h"

AWiringGameMode::AWiringGameMode()
{
	PlayerControllerClass = AWiringPlayerController::StaticClass();
	// Giữ DefaultPawnClass mặc định (DefaultPawn bay) — đủ để nhận input.
}
