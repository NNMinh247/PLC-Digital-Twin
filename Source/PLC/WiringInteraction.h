// WiringInteraction.h
// Hằng số dùng chung cho hệ thống đấu dây.
#pragma once

#include "CoreMinimal.h"

// Kênh trace tùy chỉnh "WiringInteract".
// Định nghĩa trong Config/DefaultEngine.ini:
//   +DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,...,Name="WiringInteract")
// Cọc (ATerminal) và đầu dây (AWire) Block kênh này để chuột trace trúng.
#define WIRING_TRACE_CHANNEL ECC_GameTraceChannel1
