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
	float	 FovX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	float	 FovY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	int32   YawNum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	int32   PitchNum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	int32   ImageWidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	int32   ImageHeight;

	//TODO 目前按绝对路径方式使用, 如 'H:/ScreenShotResult/' ， 每次执行会删除之前的数据，重新创建目录， 
    //建议使用相对工程的路径(参考FPaths类), 保证linux下的兼容
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureParamers")
	FString  TargetDirectory;

	FCaptureParamer()
		: FovX(60.0f)
		, FovY(60.0f)
		, PitchNum(5)
		, YawNum(12)
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

	void SetCaptureParamater(const FCaptureParamer& InCaptureParamter);
	void GenerateOrientations(const FCaptureParamer& InCaptureParamters, const TArray<FVector>& InPoints , TArray<FCaptureOrientation>& Orientations);
	bool SetCameraOrientation(const FCaptureOrientation& InOrientation);

	bool ScreenShot(const FCaptureOrientation& InOrientation);
};
