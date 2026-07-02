// PlcLinkActor.cpp
#include "PlcLinkActor.h"
#include "PlcLight.h"
#include "WebSocketsModule.h"
#include "EngineUtils.h"                       // TActorIterator
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"                     // GEngine->AddOnScreenDebugMessage
#include "Misc/CommandLine.h"                  // FCommandLine::Get
#include "Misc/Parse.h"                        // FParse::Value
#include "TimerManager.h"                      // FTimerManager (reconnect)

APlcLinkActor::APlcLinkActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void APlcLinkActor::BeginPlay()
{
    Super::BeginPlay();

    NumChannels = FMath::Max(1, NumChannels);
    LastX.Init(false, NumChannels);
    LastY.Init(false, NumChannels);

    // 1) Quét tất cả đèn APlcLight trong level, lập bảng index -> đèn
    LightMap.Empty();
    for (TActorIterator<APlcLight> It(GetWorld()); It; ++It)
    {
        if (APlcLight* L = *It)
        {
            LightMap.Add(L->LightIndex, L);
        }
    }

    // 2) Bind phím số 1..8 để toggle đèn 0..7
    BindToggleKeys();

    // 3) Xác định địa chỉ Bridge (cho phép override qua dòng lệnh).
    //      -PlcBridgeUrl=ws://host:port   (ghi đè toàn bộ)
    //      -PlcBridgePort=9090            (chỉ đổi port, giữ 127.0.0.1)
    {
        FString UrlOverride;
        int32   PortOverride = 0;
        if (FParse::Value(FCommandLine::Get(), TEXT("PlcBridgeUrl="), UrlOverride) && !UrlOverride.IsEmpty())
        {
            ServerUrl = UrlOverride;
        }
        else if (FParse::Value(FCommandLine::Get(), TEXT("PlcBridgePort="), PortOverride) && PortOverride > 0)
        {
            ServerUrl = FString::Printf(TEXT("ws://127.0.0.1:%d"), PortOverride);
        }
        UE_LOG(LogTemp, Warning, TEXT("[PLC] Bridge URL = %s"), *ServerUrl);
    }

    // 4) Kết nối WebSocket tới Bridge C# (tự nối lại nếu rớt).
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    bShuttingDown = false;
    ConnectToBridge();
}

void APlcLinkActor::ConnectToBridge()
{
    if (bShuttingDown)
    {
        return;
    }
    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        return; // đã nối rồi, khỏi tạo lại
    }

    WebSocket = FWebSocketsModule::Get().CreateWebSocket(ServerUrl);

    WebSocket->OnConnected().AddLambda([this]()
        {
            UE_LOG(LogTemp, Warning, TEXT("[PLC] Da ket noi toi bridge: %s"), *ServerUrl);
        });

    WebSocket->OnConnectionError().AddLambda([this](const FString& Error)
        {
            UE_LOG(LogTemp, Error, TEXT("[PLC] Loi ket noi: %s  -> thu lai sau %.0fs"), *Error, ReconnectIntervalSec);
            ScheduleReconnect();
        });

    WebSocket->OnClosed().AddLambda([this](int32 Code, const FString& Reason, bool bWasClean)
        {
            UE_LOG(LogTemp, Warning, TEXT("[PLC] Dong ket noi (%d): %s  -> thu lai sau %.0fs"), Code, *Reason, ReconnectIntervalSec);
            ScheduleReconnect();
        });

    WebSocket->OnMessage().AddLambda([this](const FString& Message)
        {
            HandleMessage(Message);
        });

    WebSocket->Connect();
}

void APlcLinkActor::ScheduleReconnect()
{
    if (bShuttingDown)
    {
        return;
    }
    UWorld* W = GetWorld();
    if (!W)
    {
        return;
    }
    // Đã có timer đang chờ -> không đặt thêm (tránh nối lại dồn dập).
    if (W->GetTimerManager().IsTimerActive(ReconnectTimer))
    {
        return;
    }
    W->GetTimerManager().SetTimer(
        ReconnectTimer,
        FTimerDelegate::CreateUObject(this, &APlcLinkActor::ConnectToBridge),
        FMath::Max(0.5f, ReconnectIntervalSec),
        false);
}

// Bridge gửi (mỗi ~100ms), 1 trong các trường:
//   {"x":[0,1,...], "y":[0,0,1,...], "d":{"D0":100,...}}
//   (tương thích cũ: {"lights":[...]} = y)
void APlcLinkActor::HandleMessage(const FString& Message)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return; // không phải JSON hợp lệ -> bỏ qua
    }

    // ---- Y / đèn (chấp nhận "y" hoặc "lights") ----
    const TArray<TSharedPtr<FJsonValue>>* YArr = nullptr;
    if (Root->TryGetArrayField(TEXT("y"), YArr) || Root->TryGetArrayField(TEXT("lights"), YArr))
    {
        const int32 Count = FMath::Min(YArr->Num(), LastY.Num());
        for (int32 i = 0; i < Count; ++i)
        {
            const bool bOn = ((*YArr)[i]->AsNumber() != 0.0);
            if (bOn != LastY[i])
            {
                LastY[i] = bOn;
                ApplyLight(i, bOn);
                OnLightChanged.Broadcast(i, bOn);
            }
        }
    }

    // ---- X / công tắc ----
    const TArray<TSharedPtr<FJsonValue>>* XArr = nullptr;
    if (Root->TryGetArrayField(TEXT("x"), XArr))
    {
        const int32 Count = FMath::Min(XArr->Num(), LastX.Num());
        for (int32 i = 0; i < Count; ++i)
        {
            const bool bOn = ((*XArr)[i]->AsNumber() != 0.0);
            if (bOn != LastX[i])
            {
                LastX[i] = bOn;
                OnInputChanged.Broadcast(i, bOn);
            }
        }
    }

    // ---- D / thanh ghi (object {"D0":100,...}) ----
    const TSharedPtr<FJsonObject>* DObj = nullptr;
    if (Root->TryGetObjectField(TEXT("d"), DObj) && DObj && DObj->IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*DObj)->Values)
        {
            const int32 Val = Pair.Value.IsValid() ? (int32)Pair.Value->AsNumber() : 0;
            const int32* Prev = LastD.Find(Pair.Key);
            if (!Prev || *Prev != Val)
            {
                LastD.Add(Pair.Key, Val);
                OnRegisterChanged.Broadcast(Pair.Key, Val);
            }
        }
    }
}

void APlcLinkActor::ApplyLight(int32 Index, bool bOn)
{
    if (APlcLight** Found = LightMap.Find(Index))
    {
        (*Found)->SetOn(bOn);
    }
}

void APlcLinkActor::ToggleLight(int32 Index)
{
    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        const FString Msg = FString::Printf(TEXT("{\"toggle\":%d}"), Index);
        WebSocket->Send(Msg);
        UE_LOG(LogTemp, Log, TEXT("[PLC] Gui toggle %d"), Index);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[PLC] Chua ket noi, khong gui duoc toggle %d"), Index);
    }
}

bool APlcLinkActor::GetInputState(int32 Index) const
{
    return LastX.IsValidIndex(Index) && LastX[Index];
}

bool APlcLinkActor::GetOutputState(int32 Index) const
{
    return LastY.IsValidIndex(Index) && LastY[Index];
}

int32 APlcLinkActor::GetRegister(const FString& Name) const
{
    const int32* P = LastD.Find(Name);
    return P ? *P : 0;
}

FString APlcLinkActor::BuildInputStatusString() const
{
    FString S;
    for (int32 i = 0; i < LastX.Num(); ++i)
    {
        S += FString::Printf(TEXT("X%d: %s    "), i + 1, LastX[i] ? TEXT("ON") : TEXT("OFF"));
    }
    return S;
}

void APlcLinkActor::BindToggleKeys()
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        EnableInput(PC); // tạo + đăng ký InputComponent cho actor này
    }
    if (!InputComponent)
    {
        return;
    }

    static const FKey Keys[8] = {
        EKeys::One, EKeys::Two,  EKeys::Three, EKeys::Four,
        EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight
    };

    for (int32 i = 0; i < 8; ++i)
    {
        FInputKeyBinding Binding(FInputChord(Keys[i], false, false, false, false), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, i]() { HandleNumberKey(i); });
        InputComponent->KeyBindings.Add(Binding);
    }

    // Phím S: bật/tắt chế độ giả lập.
    {
        FInputKeyBinding SimBind(FInputChord(EKeys::S, false, false, false, false), IE_Pressed);
        SimBind.KeyDelegate.GetDelegateForManualSet().BindLambda([this]() { ToggleSimulation(); });
        InputComponent->KeyBindings.Add(SimBind);
    }
}

// Phím 1..8: khi giả lập -> đảo đèn tại chỗ; khi thường -> gửi toggle xuống PLC như cũ.
void APlcLinkActor::HandleNumberKey(int32 Index)
{
    if (bSimulationMode)
    {
        SimToggleLight(Index);
    }
    else
    {
        ToggleLight(Index);
    }
}

// Bật/tắt chế độ giả lập + báo trên màn hình.
void APlcLinkActor::ToggleSimulation()
{
    bSimulationMode = !bSimulationMode;
    UE_LOG(LogTemp, Warning, TEXT("[PLC] Che do gia lap: %s"), bSimulationMode ? TEXT("BAT") : TEXT("TAT"));

    if (bSimulationMode)
    {
        // Chỉ báo cố định trên màn hình trong suốt thời gian giả lập.
        ShowScreenLog(TEXT("[GIA LAP] DANG BAT  -  phim 1..8: bat/tat den   |   S: tat gia lap"),
                      FColor::Green, 100, 1.0e9f);
    }
    else
    {
        if (GEngine)
        {
            GEngine->RemoveOnScreenDebugMessage(100);
        }
        ShowScreenLog(TEXT("[GIA LAP] DA TAT"), FColor::Yellow, 100, 3.0f);
    }
}

// Đảo trạng thái đèn ngay trong Unreal (không gửi xuống PLC).
void APlcLinkActor::SimToggleLight(int32 Index)
{
    if (!LastY.IsValidIndex(Index))
    {
        return;
    }

    const bool bOn = !LastY[Index];
    LastY[Index] = bOn;
    ApplyLight(Index, bOn);              // đổi hình đèn trong level
    OnLightChanged.Broadcast(Index, bOn); // để HMI/log nghe được (nếu có)

    ShowScreenLog(
        FString::Printf(TEXT("[GIA LAP] Den %d (H%d): %s"), Index + 1, Index + 1, bOn ? TEXT("BAT") : TEXT("TAT")),
        bOn ? FColor::Red : FColor::Silver, 200 + Index, 3.0f);
}

void APlcLinkActor::ShowScreenLog(const FString& Text, const FColor& Color, int32 Key, float Duration)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(Key, Duration, Color, Text);
    }
    UE_LOG(LogTemp, Log, TEXT("%s"), *Text);
}

void APlcLinkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bShuttingDown = true; // chặn mọi lần thử nối lại
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(ReconnectTimer);
    }
    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        WebSocket->Close();
    }
    Super::EndPlay(EndPlayReason);
}
