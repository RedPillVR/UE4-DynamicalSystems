#include "NetAvatar.h"
#include "DynamicalSystemsPrivatePCH.h"

DEFINE_LOG_CATEGORY(NetAvatar);

UNetAvatar::UNetAvatar()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNetAvatar::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(NetAvatar, Warning, TEXT("NetAvatar[%s]: BeginPlay"), *GetOwner()->GetName());

	for (TActorIterator<ANetClient> ActorItr(GetWorld()); ActorItr; ++ActorItr) {
		NetClient = *ActorItr;
		break;
	}

	LastUpdateTime = UGameplayStatics::GetRealTimeSeconds(GetWorld());

	if (IsNetProxy) {
		NetClient->RegisterAvatar(this);
	}
	else if (IsValid(NetClient)) {
		NetClient->Avatar = this;
		NetID = NetClient->Uuid;
	}
}

void UNetAvatar::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	float CurrentTime = UGameplayStatics::GetRealTimeSeconds(GetWorld());

	if (!IsNetProxy && IsValid(NetClient)) {
		LastUpdateTime = CurrentTime;
	}
	//This, is very dangerous. It has the possibility to flip a user's VR camera.  
	if ((CurrentTime - LastUpdateTime) > 20 && IsValid(NetClient)) {
		AController* Controller = Cast<AController>(GetOwner());
		if (IsValid(Controller)) {
			APawn* Pawn = Controller->GetPawn();
			if (IsValid(Pawn)) {
				UE_LOG(NetAvatar, Warning, TEXT("NetAvatar[%s]: Destroy Pawn"), *GetOwner()->GetName());
				Pawn->Destroy();
			}
			UE_LOG(NetAvatar, Warning, TEXT("NetAvatar[]%s: Destroy Controller"), *GetOwner()->GetName());
			Controller->Destroy();
		}
	}
}

