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


//float	 FovX;
//float	 FovY;
//uint32   PitchNum;
//uint32   YawNum;
//uint32   ImageWidth;
//uint32   ImageHeight;
//FString  FolderPath;


bool AUnrealcvWorldController::Capture(const FCaptureParamer& InCaptureParamter, const TArray<FVector>& InPoints)
{
	UE_LOG(LogUnrealCV, Error, TEXT("AUnrealcvWorldController::Capture Start"));

	if (InPoints.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AUnrealcvWorldController::Capture : Capture Point List is empty"));
		return false;
	}

	APawn* Pawn = FUnrealcvServer::Get().GetPawn();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AUnrealcvWorldController::Capture : The Pawn of the scene is invalid."));
		return false;
	}

	CaptureParamter = InCaptureParamter;

	// Set Parameters(Use FCaptureParamer)
	// Fov
	//ImageWidth
	//ImageHeight
	//OuptputPath
	//

	GenerateOrientations(InCaptureParamter, InPoints, CaptureOrientations);

	OldCameraOrientation.Location = Pawn->GetActorLocation();
	OldCameraOrientation.Rotator = Pawn->GetActorRotation();

	CurCaptureIndex = 0;
	SetTransfromDelay();

	UE_LOG(LogUnrealCV, Error, TEXT("AUnrealcvWorldController::Capture End"));

	return false;
}

void AUnrealcvWorldController::SetTransfromDelay()
{
	UE_LOG(LogUnrealCV, Error, TEXT("AUnrealcvWorldController::SetTransfromDelay, Index: %d"), CurCaptureIndex);

	if (CurCaptureIndex < CaptureOrientations.Num())
	{
		SetCameraOrientation(CaptureOrientations[CurCaptureIndex]);
		GetWorldTimerManager().SetTimerForNextTick(this, &AUnrealcvWorldController::ScreenShotDelay);
	}
	else
	{
		SetCameraOrientation(OldCameraOrientation);

		UE_LOG(LogUnrealCV, Error, TEXT("AUnrealcvWorldController::SetTransfromDelay End"));
	}
}

void AUnrealcvWorldController::ScreenShotDelay()
{
	FString FilePath = CaptureParamter.TargetDirectory + FString::FromInt(CaptureOrientations[CurCaptureIndex].PointId) + "-" + FString::FromInt(CaptureOrientations[CurCaptureIndex].InterId) + ".png";
	ScreenShot(FilePath);
	CurCaptureIndex++;
	GetWorldTimerManager().SetTimerForNextTick(this, &AUnrealcvWorldController::SetTransfromDelay);
}

void AUnrealcvWorldController::GenerateOrientations(const FCaptureParamer& InCaptureParamters, const TArray<FVector>& InPoints, TArray<FCaptureOrientation>& Orientations)
{
	Orientations.Empty();

	float YawAngleGap = 360.0f / (float)InCaptureParamters.YawNum;
	float PitchAngleGap = 360.0f / (float)InCaptureParamters.PitchNum;

	for (int32 PointIndex = 0; PointIndex < InPoints.Num(); ++PointIndex)
	{
		for (int32 PitchIndex = 0; PitchIndex < InCaptureParamters.PitchNum; ++PitchIndex)
		{
			float CurPitctAngle = PitchIndex * PitchAngleGap;

			for (int32 YawIndex = 0; YawIndex < InCaptureParamters.YawNum; ++YawIndex)
			{
				float CurYawAngle = YawIndex * YawAngleGap;

				//TODO Roll Angle = 0
				FRotator Rotator = FRotator(CurPitctAngle, CurYawAngle, 0);

				FCaptureOrientation Orientation;
				Orientation.Rotator = Rotator;
				Orientation.Location = InPoints[PointIndex];
				Orientation.PointId = PointIndex;
				Orientation.InterId = PitchIndex * InCaptureParamters.YawNum + YawIndex;

				UE_LOG(LogUnrealCV, Warning, TEXT("AUnrealcvWorldController::Capture : Location - %s , CurPitctAngle - %f , CurYawAngle - %f "), *InPoints[PointIndex].ToCompactString(), CurPitctAngle, CurYawAngle);

				Orientations.Add(Orientation);
			}
		}
	}
}

bool  AUnrealcvWorldController::SetCameraOrientation(const FCaptureOrientation& Orientation)
{
	// Set Transform
	APawn* Pawn = FUnrealcvServer::Get().GetPawn();
	if (!IsValid(Pawn))
	{
		return false;
	}

	UE_LOG(LogUnrealCV, Warning, TEXT("AUnrealcvWorldController::CameraShot : Location - %s , Rotator - %s"), *Orientation.Location.ToCompactString(), *Orientation.Rotator.ToCompactString());
	Pawn->SetActorLocation(Orientation.Location, false, NULL, ETeleportType::TeleportPhysics);
	Pawn->SetActorRotation(Orientation.Rotator, ETeleportType::TeleportPhysics);

	return true;
}

//TODO How to use HighResShot
void AUnrealcvWorldController::ScreenShot(const FString& SaveFilePath)
{
	FString Cmd = "vget /screenshot " + SaveFilePath;
	UE_LOG(LogUnrealCV, Warning, TEXT("AUnrealcvWorldController::ScreenShot:  ScreenShot Command is %s"), *Cmd);
	FExecStatus ExecStatus1 = FUnrealcvServer::Get().CommandDispatcher->Exec(Cmd);
	UE_LOG(LogUnrealCV, Warning, TEXT("%s"), *ExecStatus1.GetMessage());
}

