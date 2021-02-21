// Weichao Qiu @ 2017
#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"

#include "ObjectAnnotator.h"
#include "PlayerViewMode.h"
#include "WorldController.generated.h"


USTRUCT(BlueprintType)
struct FCaptureParamer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	float	 Fov;

	// ˮƽ����ɼ����, Ĭ��0-360
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	float   YawAngleGap;

	// ǰ�������ҡ�ڽǶ�
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	float     MaxPitchAngle;

	// ǰ������Сҡ�ڽǶ�
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	float     MinPitchAngle;

	// ǰ����ɼ��Ƕȼ��
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	float   PitchGap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	int32   ImageWidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	int32   ImageHeight;

	//TODO Ŀǰ������·����ʽʹ��, �� 'H:/ScreenShotResult/' �� ÿ��ִ�л�ɾ��֮ǰ�����ݣ����´���Ŀ¼�� 
    //����ʹ����Թ��̵�·��(�ο�FPaths��), ��֤linux�µļ���
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	FString  TargetDirectory;

	FCaptureParamer()
		: Fov(90.0f)
		, PitchGap(12.0f)
		, MaxPitchAngle(36)
		, MinPitchAngle(-36)
		, YawAngleGap(60.0f)
		, ImageWidth(1920)
		, ImageHeight(1080)
		, TargetDirectory("/Output")
	{

	}
};

UCLASS()
class AUnrealcvWorldController : public AActor
{
	GENERATED_BODY()

public:
	FObjectAnnotator ObjectAnnotator;

	UPROPERTY()
		UPlayerViewMode* PlayerViewMode;

	AUnrealcvWorldController(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	virtual void PostActorCreated() override;

	/** Open new level */
	void OpenLevel(FName LevelName);

	void InitWorld();

	void AttachPawnSensor();

	void Tick(float DeltaTime);

	//----------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "UnrealcvWorldController")
	bool Capture(const FCaptureParamer& InCaptureParamter, const TArray<FVector>& InPoints);

private:

	struct FCaptureOrientation
	{
		FVector  Location;
		FRotator Rotator;
		int32    PointId;
		int32    InterId;
	};

	FCaptureParamer CaptureParamter;
	FString TargetDirectory;

	FCaptureOrientation  OldCameraOrientation;

	TArray<FCaptureOrientation> CaptureOrientations;
	int32 CurCaptureIndex; 

	void StartCapture();
	void ScreenShotDelay();
	void SetCameraOrientationDelay();
	void StopCapture();

	FTimerHandle InHandle;

	void SetCaptureParamater(const FCaptureParamer& InCaptureParamter);
	void GenerateOrientations(const FCaptureParamer& InCaptureParamters, const TArray<FVector>& InPoints , TArray<FCaptureOrientation>& Orientations);
	bool SetCameraOrientation(const FCaptureOrientation& InOrientation);

	bool ScreenShot(const FCaptureOrientation& InOrientation);
};
