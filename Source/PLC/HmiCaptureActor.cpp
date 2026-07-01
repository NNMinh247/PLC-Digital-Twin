// HmiCaptureActor.cpp
#include "HmiCaptureActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

AHmiCaptureActor::AHmiCaptureActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	RootComponent = Capture;
	Capture->bCaptureEveryFrame = true;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
}

void AHmiCaptureActor::BeginPlay()
{
	Super::BeginPlay();

	int32 W = RTWidth;
	int32 H = RTHeight;
	if (W <= 0 || H <= 0)
	{
		if (View == EHmiView::Board16x9) { W = 1280; H = 720; }
		else { W = 1280; H = 240; } // ~16:3 (dải đèn dài, dẹt)
	}

	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("HmiRenderTarget"));
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(W, H);
	RenderTarget->UpdateResourceImmediate(true);

	if (Capture)
	{
		Capture->TextureTarget = RenderTarget;
	}
}

bool AHmiCaptureActor::DeprojectUVToWorldRay(const FVector2D& UV, FVector& OutOrigin, FVector& OutDir) const
{
	if (!Capture || !RenderTarget)
	{
		return false;
	}

	const float W = (float)RenderTarget->SizeX;
	const float H = (float)RenderTarget->SizeY;
	if (W <= 0.f || H <= 0.f)
	{
		return false;
	}
	const float Aspect = W / H;

	// FOVAngle của SceneCapture là FOV NGANG (độ).
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle * 0.5f));

	// UV: (0,0) góc trên-trái, (1,1) góc dưới-phải.
	// NDC: x sang phải +, y hướng lên +.
	const float NdcX = UV.X * 2.f - 1.f;
	const float NdcY = 1.f - UV.Y * 2.f;

	// Không gian camera của Unreal: X tiến, Y phải, Z lên.
	// (Nếu ảnh bị lật trái/phải -> đổi dấu NdcX; lật trên/dưới -> đổi dấu NdcY.)
	FVector DirCam;
	DirCam.X = 1.f;
	DirCam.Y = NdcX * HalfTan;
	DirCam.Z = NdcY * HalfTan / Aspect;

	const FTransform T = Capture->GetComponentTransform();
	OutOrigin = T.GetLocation();
	OutDir = T.TransformVectorNoScale(DirCam).GetSafeNormal();
	return true;
}
