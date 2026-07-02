// PlcLinkActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IWebSocket.h"
#include "Engine/TimerHandle.h"
#include "PlcLinkActor.generated.h"

class FJsonObject;

// Y (đèn / output) đổi trạng thái
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLightChanged, int32, Index, bool, bIsOn);
// X (công tắc / input) đổi trạng thái
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInputChanged, int32, Index, bool, bIsOn);
// D (thanh ghi) đổi giá trị
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRegisterChanged, const FString&, Name, int32, Value);

UCLASS()
class PLC_API APlcLinkActor : public AActor
{
    GENERATED_BODY()

public:
    APlcLinkActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // Gửi lệnh đảo trạng thái đèn xuống PLC: gửi JSON {"toggle":n}
    UFUNCTION(BlueprintCallable, Category = "PLC")
    void ToggleLight(int32 Index);

    // Số kênh X/Y theo dõi (mặc định 8 -> X0..X7, Y0..Y7)
    UPROPERTY(EditAnywhere, Category = "PLC")
    int32 NumChannels = 8;

    // Địa chỉ Bridge. Dùng IP máy chạy Bridge nếu khác máy.
    // Có thể override lúc chạy bằng dòng lệnh:
    //   -PlcBridgeUrl=ws://host:port   (ghi đè toàn bộ)
    //   -PlcBridgePort=9090            (chỉ đổi port, giữ 127.0.0.1)
    UPROPERTY(EditAnywhere, Category = "PLC")
    FString ServerUrl = TEXT("ws://127.0.0.1:8080");

    // Chế độ giả lập: bật bằng phím S. Khi bật, phím 1..8 tự bật/tắt đèn ngay trong Unreal
    // (không gửi xuống PLC) để test khi chưa cắm PLC. Có thể đặt sẵn = true trong Details.
    UPROPERTY(EditAnywhere, Category = "PLC|Simulation")
    bool bSimulationMode = false;

    // ===== Sự kiện cho UI / Blueprint =====
    UPROPERTY(BlueprintAssignable, Category = "PLC")
    FOnLightChanged OnLightChanged;       // Y / đèn

    UPROPERTY(BlueprintAssignable, Category = "PLC")
    FOnInputChanged OnInputChanged;       // X / công tắc

    UPROPERTY(BlueprintAssignable, Category = "PLC")
    FOnRegisterChanged OnRegisterChanged; // D / thanh ghi

    // ===== Truy vấn trạng thái hiện tại =====
    UFUNCTION(BlueprintCallable, Category = "PLC")
    bool GetInputState(int32 Index) const;        // X

    UFUNCTION(BlueprintCallable, Category = "PLC")
    bool GetOutputState(int32 Index) const;        // Y

    UFUNCTION(BlueprintCallable, Category = "PLC")
    int32 GetRegister(const FString& Name) const;  // D

    // Tạo chuỗi trạng thái công tắc: "X1: ON   X2: OFF   ..."
    UFUNCTION(BlueprintCallable, Category = "PLC")
    FString BuildInputStatusString() const;

private:
    void HandleMessage(const FString& Message);
    void ApplyLight(int32 Index, bool bOn);
    void BindToggleKeys();

    // Tạo socket + gắn handler + Connect(). Gọi lại được nhiều lần (dùng cho reconnect).
    void ConnectToBridge();
    // Hẹn giờ thử kết nối lại (chống đặt trùng nhiều timer).
    void ScheduleReconnect();

    // Phím 1..8: giả lập -> đảo đèn tại chỗ; thường -> gửi toggle xuống PLC.
    void HandleNumberKey(int32 Index);
    // Phím S: bật/tắt chế độ giả lập (kèm log trên màn hình).
    void ToggleSimulation();
    // Đảo trạng thái đèn ngay trong Unreal (dùng cho giả lập).
    void SimToggleLight(int32 Index);
    // Ghi 1 dòng log lên màn hình (AddOnScreenDebugMessage).
    void ShowScreenLog(const FString& Text, const FColor& Color, int32 Key, float Duration = 3.0f);

    // Root component (để actor đầy đủ + đặt được trong level)
    UPROPERTY(VisibleAnywhere, Category = "PLC")
    class USceneComponent* SceneRoot;

    TSharedPtr<IWebSocket> WebSocket;

    // Reconnect — tất cả là member THƯỜNG (không UPROPERTY) để KHÔNG đổi schema
    // serialize của actor (nếu để UPROPERTY sẽ lệch với bản cook cũ -> crash lúc load).
    float ReconnectIntervalSec = 3.0f;   // giây
    FTimerHandle ReconnectTimer;
    bool bShuttingDown = false;   // true khi EndPlay -> không thử nối lại nữa

    // Trạng thái gần nhất (để chỉ bắn event khi đổi)
    TArray<bool> LastX;            // công tắc
    TArray<bool> LastY;            // đèn / output
    TMap<FString, int32> LastD;    // thanh ghi

    // Bảng tra: index 0..N-1 -> con trỏ tới actor đèn trong level
    UPROPERTY()
    TMap<int32, class APlcLight*> LightMap;
};
