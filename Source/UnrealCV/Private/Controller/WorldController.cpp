// Weichao Qiu @ 2017
#include "WorldController.h"

#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Classes/Engine/GameViewportClient.h"

#include "Sensor/CameraSensor/PawnCamSensor.h"
#include "VisionBPLib.h"
#include "UnrealcvServer.h"
#include "PlayerViewMode.h"
#include "UnrealcvLog.h"

AUnrealcvWorldController::AUnrealcvWorldController(const FObjectInitializer& ObjectInitializer)
{
	Progress = 0;
	IsInCapure = false;
	CurCaptureIndex = 0;

	PlayerViewMode = CreateDefaultSubobject<UPlayerViewMode>(TEXT("PlayerViewMode"));
}

void AUnrealcvWorldController::AttachPawnSensor()
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	check(PlayerController);
	APawn* Pawn = PlayerController->GetPawn();
	FUnrealcvServer& Server = FUnrealcvServer::Get();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("No available pawn to mount a PawnSensor"));
		return;
	}

	UE_LOG(LogUnrealCV, Display, TEXT("Attach a UnrealcvSensor to the pawn"));
	// Make sure this is the first one.
	UPawnCamSensor* PawnCamSensor = NewObject<UPawnCamSensor>(Pawn, TEXT("PawnSensor")); // Make Pawn as the owner of the component
	// UFusionCamSensor* FusionCamSensor = ConstructObject<UFusionCamSensor>(UFusionCamSensor::StaticClass(), Pawn);

	UWorld* PawnWorld = Pawn->GetWorld(), * GameWorld = FUnrealcvServer::Get().GetWorld();
	// check(Pawn->GetWorld() == FUnrealcvServer::GetWorld());
	check(PawnWorld == GameWorld);
	PawnCamSensor->AttachToComponent(Pawn->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	// AActor* OwnerActor = FusionCamSensor->GetOwner();
	PawnCamSensor->RegisterComponent(); // Is this neccessary?
	UVisionBPLib::UpdateInput(Pawn, Server.Config.EnableInput);
}

void AUnrealcvWorldController::InitWorld()
{
	UE_LOG(LogUnrealCV, Display, TEXT("Overwrite the world setting with some UnrealCV extensions"));
	FUnrealcvServer& UnrealcvServer = FUnrealcvServer::Get();
	if (UnrealcvServer.TcpServer && !UnrealcvServer.TcpServer->IsListening())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("The tcp server is not running"));
	}

	ObjectAnnotator.AnnotateWorld(GetWorld());

	FEngineShowFlags ShowFlags = GetWorld()->GetGameViewport()->EngineShowFlags;
	this->PlayerViewMode->SaveGameDefault(ShowFlags);

	this->AttachPawnSensor();

	// TODO: remove legacy code
	// Update camera FOV

	//PlayerController->PlayerCameraManager->SetFOV(Server.Config.FOV);
	//FCaptureManager::Get().AttachGTCaptureComponentToCamera(Pawn);
}

void AUnrealcvWorldController::PostActorCreated()
{
	UE_LOG(LogTemp, Display, TEXT("AUnrealcvWorldController::PostActorCreated"));
	Super::PostActorCreated();
}


void AUnrealcvWorldController::BeginPlay()
{
	UE_LOG(LogTemp, Display, TEXT("AUnrealcvWorldController::BeginPlay"));
	Super::BeginPlay();
}

void AUnrealcvWorldController::OpenLevel(FName LevelName)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UGameplayStatics::OpenLevel(World, LevelName);
	UGameplayStatics::FlushLevelStreaming(World);
	UE_LOG(LogUnrealCV, Warning, TEXT("Level loaded"));
}


void AUnrealcvWorldController::Tick(float DeltaTime)
{

}

bool AUnrealcvWorldController::Capture(const FCaptureParamer& InCaptureParamter, const TArray<FVector>& InPoints)
{
	if (InPoints.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UnrealcvWorldController::Capture : Capture Point List is empty"));
		return false;
	}

	APawn* Pawn = FUnrealcvServer::Get().GetPawn();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UnrealcvWorldController::Capture : The Pawn of the scene is invalid."));
		return false;
	}

	if (IsInCapure)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UnrealcvWorldController::Capture : Other Capture Operation is running."));
		return false;
	}

	IsInCapure = true;

	Progress = 0.0f;
	CaptureParamter = InCaptureParamter;
	SetCaptureParamater(InCaptureParamter);

	GenerateOrientations(InCaptureParamter, InPoints, CaptureOrientations);

	StartCapture();

	return false;
}

void AUnrealcvWorldController::SetCaptureParamater(const FCaptureParamer& InCaptureParamter)
{
	// Create Directory 
	if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*InCaptureParamter.TargetDirectory))
	{
		FPlatformFileManager::Get().Get().GetPlatformFile().DeleteDirectoryRecursively(*InCaptureParamter.TargetDirectory);
	}
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*CaptureParamter.TargetDirectory);

	// Fov
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UnrealcvWorldController::SetCaptureParamater : The APlayerController of the Pawn is invalid."));
		return;
	}

	APlayerCameraManager* PlayerCamManager = PlayerController->PlayerCameraManager;
	if (PlayerCamManager)
	{
		PlayerCamManager->SetFOV(InCaptureParamter.Fov);
	}

	// ImageWidth
	// ImageHeight
}

void AUnrealcvWorldController::StartCapture()
{
	UE_LOG(LogUnrealCV, Log, TEXT("AUnrealcvWorldController::StartCapture"));
	APawn* Pawn = FUnrealcvServer::Get().GetPawn();
	OldCameraOrientation.Location = Pawn->GetActorLocation();
	OldCameraOrientation.Rotator = Pawn->GetActorRotation();

	Progress = 0.0f;
	CurCaptureIndex = 0;
	SetCameraOrientationDelay();
}

void AUnrealcvWorldController::SetCameraOrientationDelay()
{
	UE_LOG(LogUnrealCV, Log, TEXT("UnrealcvWorldController::SetCameraOrientationDelay, Index: %d"), CurCaptureIndex);

	if (CurCaptureIndex < CaptureOrientations.Num())
	{
		if (!SetCameraOrientation(CaptureOrientations[CurCaptureIndex]))
		{
			StopCapture();
		}
		GetWorldTimerManager().SetTimer(InHandle, this, &AUnrealcvWorldController::ScreenShotDelay, 0.05f, false);
	}
	else
	{
		StopCapture();
	}
}

void AUnrealcvWorldController::ScreenShotDelay()
{
	GetWorldTimerManager().ClearTimer(InHandle);
	//Continue Even if have wrong
	ScreenShot(CaptureOrientations[CurCaptureIndex]);
	CurCaptureIndex++;

	Progress = (float)CurCaptureIndex / (float)CaptureOrientations.Num() - 0.001f;
	Progress = FMath::Max(Progress, 0.0f);
	GetWorldTimerManager().SetTimerForNextTick(this, &AUnrealcvWorldController::SetCameraOrientationDelay);
}

void AUnrealcvWorldController::StopCapture()
{
	UE_LOG(LogUnrealCV, Log, TEXT("AUnrealcvWorldController::StopCapture"));

	//Restore Position
	SetCameraOrientation(OldCameraOrientation);
	Progress = 1.0f;
	IsInCapure = false;
}

void AUnrealcvWorldController::GenerateOrientations(const FCaptureParamer& InCaptureParamters, const TArray<FVector>& InPoints, TArray<FCaptureOrientation>& Orientations)
{
	Orientations.Empty();

	int32 YawNum = FMath::CeilToInt(360.0f / InCaptureParamters.YawAngleGap);  //[)
	int32 PitchNum = FMath::CeilToInt((InCaptureParamters.MaxPitchAngle - InCaptureParamters.MinPitchAngle) / InCaptureParamters.PitchGap) + 1;  // []

	for (int32 PointIndex = 0; PointIndex < InPoints.Num(); ++PointIndex)
	{
		for (int32 PitchIndex = 0; PitchIndex < PitchNum; PitchIndex++)
		{
			float CurPitctAngle = FMath::Min<float>(InCaptureParamters.MinPitchAngle + PitchIndex * InCaptureParamters.PitchGap, InCaptureParamters.MaxPitchAngle);
			for (int32 YawIndex = 0; YawIndex < YawNum; ++YawIndex)
			{
				float CurYawAngle = FMath::Min<float>(YawIndex * InCaptureParamters.YawAngleGap, 360.0f);
				UE_LOG(LogUnrealCV, Log, TEXT("UnrealcvWorldController::Capture : Location - %s , CurPitctAngle - %f , CurYawAngle - %f "), *InPoints[PointIndex].ToCompactString(), CurPitctAngle, CurYawAngle);

				//TODO Roll Angle = 0
				FRotator Rotator = FRotator(CurPitctAngle, CurYawAngle, 0);

				FCaptureOrientation Orientation;
				Orientation.Rotator = Rotator;
				Orientation.Location = InPoints[PointIndex];
				Orientation.PointId = PointIndex;
				Orientation.InterId = PitchIndex * YawNum + YawIndex;
				Orientations.Add(Orientation);
			}
		}
	}
}

bool  AUnrealcvWorldController::SetCameraOrientation(const FCaptureOrientation& Orientation)
{
	APawn* Pawn = FUnrealcvServer::Get().GetPawn();
	UE_LOG(LogUnrealCV, Log, TEXT("UnrealcvWorldController::SetCameraOrientation : Location - %s , Rotator - %s"), *Orientation.Location.ToCompactString(), *Orientation.Rotator.ToCompactString());

	bool IsSucess = Pawn->SetActorLocation(Orientation.Location, false, NULL, ETeleportType::TeleportPhysics);
	if (!IsSucess)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UnrealcvWorldController::SetCameraOrientation : SetActorLocation Method execute Error."));
		return false;
	}

	AController* Controller = Pawn->GetController();
	if (!IsValid(Controller))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UnrealcvWorldController::SetCameraOrientation : The Controller of the Pawn is invalid."));
		return false;
	}
	Controller->ClientSetRotation(Orientation.Rotator); // Teleport action

	return true;
}

bool  AUnrealcvWorldController::ScreenShot(const FCaptureOrientation& InOrientation)
{
	//Save Path
	FString SaveFilePath = CaptureParamter.TargetDirectory + "ScreenShot_" + FString::FromInt(InOrientation.PointId) + "_" + FString::FromInt(InOrientation.InterId) + "_" + FString::FromInt(InOrientation.Rotator.Pitch) + "_" + FString::FromInt(InOrientation.Rotator.Yaw)  + ".png";

	//-------------------------------------- ReadPixel  Development版本下失败
	//FString Cmd = "vget /screenshot " + SaveFilePath;
	//UE_LOG(LogUnrealCV, Warning, TEXT("AUnrealcvWorldController::ScreenShot:  ScreenShot Command is %s"), *Cmd);
	//FExecStatus ExecStatus1 = FUnrealcvServer::Get().CommandDispatcher->Exec(Cmd);
	//UE_LOG(LogUnrealCV, Warning, TEXT("%s"), *ExecStatus1.GetMessage());

	// ------------------------------------- HighResShot 指令 Development可用，Shipping下不可用
	FString Cmd = "vrun HighResShot " + FString::FromInt(CaptureParamter.ImageWidth) + "x" + FString::FromInt(CaptureParamter.ImageHeight)  + " filename=" + SaveFilePath;
	UE_LOG(LogUnrealCV, Log, TEXT("UnrealcvWorldController::HighResShot:  HighResShot Command is %s"), *Cmd);
	FExecStatus ExecStatus1 = FUnrealcvServer::Get().CommandDispatcher->Exec(Cmd);
	UE_LOG(LogUnrealCV, Log, TEXT("HighResShot Result : %s"), *ExecStatus1.GetMessage());

	return true;
}

