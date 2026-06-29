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

    // 3) Kết nối WebSocket tới Bridge C#
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    WebSocket = FWebSocketsModule::Get().CreateWebSocket(ServerUrl);

    WebSocket->OnConnected().AddLambda([this]()
        {
            UE_LOG(LogTemp, Warning, TEXT("[PLC] Da ket noi toi bridge: %s"), *ServerUrl);
        });

    WebSocket->OnConnectionError().AddLambda([](const FString& Error)
        {
            UE_LOG(LogTemp, Error, TEXT("[PLC] Loi ket noi: %s"), *Error);
        });

    WebSocket->OnClosed().AddLambda([](int32 Code, const FString& Reason, bool bWasClean)
        {
            UE_LOG(LogTemp, Warning, TEXT("[PLC] Dong ket noi (%d): %s"), Code, *Reason);
        });

    WebSocket->OnMessage().AddLambda([this](const FString& Message)
        {
            HandleMessage(Message);
        });

    WebSocket->Connect();
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
        Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, i]() { ToggleLight(i); });
        InputComponent->KeyBindings.Add(Binding);
    }
}

void APlcLinkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        WebSocket->Close();
    }
    Super::EndPlay(EndPlayReason);
}
