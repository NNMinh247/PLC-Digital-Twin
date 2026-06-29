// WiringGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WiringGameMode.generated.h"

/**
 * GameMode đặt PlayerController = AWiringPlayerController.
 * DefaultEngine.ini đã trỏ GlobalDefaultGameMode = /Script/PLC.WiringGameMode.
 */
UCLASS()
class PLC_API AWiringGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWiringGameMode();
};
