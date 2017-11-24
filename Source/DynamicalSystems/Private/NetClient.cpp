#include "NetClient.h"
#include "NetAvatar.h"
#include "NetVoice.h"
#include "NetRigidBody.h"
#include "RustyDynamics.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "DynamicalSystemsPrivatePCH.h"

DEFINE_LOG_CATEGORY(RustyNet);

ANetClient::ANetClient()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANetClient::RegisterRigidBody(UNetRigidBody* RigidBody)
{
	UE_LOG(RustyNet, Warning, TEXT("ANetClient::RegisterRigidBody %s"), *RigidBody->GetOwner()->GetName());
	NetRigidBodies.Add(RigidBody);
}

void ANetClient::RegisterAvatar(UNetAvatar* _Avatar)
{
    UE_LOG(RustyNet, Warning, TEXT("ANetClient::RegisterAvatar %i %s"), _Avatar->NetID, *_Avatar->GetOwner()->GetName());
    NetAvatars.Add(_Avatar);
}

void ANetClient::RegisterVoice(UNetVoice* Voice)
{
    UE_LOG(RustyNet, Warning, TEXT("ANetClient::RegisterVoice %s"), *Voice->GetOwner()->GetName());
    NetVoices.Add(Voice);
}

void ANetClient::Say(uint8* Bytes, uint32 Count)
{
}

void ANetClient::RebuildConsensus()
{
    int Count = NetClients.Num();
    FRandomStream Rnd(Count);
    
    MappedClients.Empty();
    NetClients.GetKeys(MappedClients);
    MappedClients.Sort();

	NetIndex = MappedClients.IndexOfByKey(Uuid);
    
    NetRigidBodies.Sort([](const UNetRigidBody& LHS, const UNetRigidBody& RHS) {
        return LHS.NetID > RHS.NetID; });
    for (auto It = NetRigidBodies.CreateConstIterator(); It; ++It) {
        (*It)->NetOwner = Rnd.RandRange(0, Count);
    }
}

void ANetClient::BeginPlay()
{
    Super::BeginPlay();
	Uuid = rb_uuid();
    bool bCanBindAll;
    TSharedPtr<class FInternetAddr> localIp = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
    Local = localIp->ToString(true);
    UE_LOG(RustyNet, Warning, TEXT("GetLocalHostAddr %s"), *Local);
    LastPingTime = UGameplayStatics::GetRealTimeSeconds(GetWorld());
    LastAvatarTime = LastPingTime;
	LastRigidbodyTime = LastPingTime;
    if (Client == NULL) {
        Client = rd_netclient_open(TCHAR_TO_ANSI(*Local), TCHAR_TO_ANSI(*Server), TCHAR_TO_ANSI(*MumbleServer));
        NetClients.Add(Uuid, -1);
        UE_LOG(RustyNet, Warning, TEXT("NetClient BeginPlay %i"), Uuid);
    }
}

void ANetClient::BeginDestroy()
{
    Super::BeginDestroy();
    if (Client != NULL) {
		UE_LOG(RustyNet, Warning, TEXT("NetClient BeginDestroy %i"), Uuid);
        rd_netclient_drop(Client);
        Client = NULL;
    }
}

void ANetClient::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
    float CurrentTime = UGameplayStatics::GetRealTimeSeconds(GetWorld());
    float CurrentAvatarTime = CurrentTime;
	float CurrentRigidbodyTime = CurrentTime;
    
	for (int Idx=0; Idx<NetRigidBodies.Num();) {
		if (!IsValid(NetRigidBodies[Idx])) {
			NetRigidBodies.RemoveAt(Idx);
		}
		else {
			++Idx;
		}
	}

    for (int Idx=0; Idx<NetAvatars.Num();) {
        if (!IsValid(NetAvatars[Idx])) {
            NetAvatars.RemoveAt(Idx);
        }
        else {
            ++Idx;
        }
    }
    
    for (int Idx=0; Idx<NetVoices.Num();) {
        if (!IsValid(NetVoices[Idx])) {
            NetVoices.RemoveAt(Idx);
        }
        else {
            ++Idx;
        }
    }
    
	{
        TArray<int32> DeleteList;
        for (auto& Elem : NetClients) {
            if (Elem.Value > 0 && (CurrentTime - Elem.Value) > 20) {
				UE_LOG(RustyNet, Warning, TEXT("NetClient DELETED: %i"), Elem.Key);
                DeleteList.Add(Elem.Key);
            }
        }
        for (auto& Key : DeleteList) {
            NetClients.Remove(Key);
        }
    }
    
    if (CurrentTime > LastPingTime + 1) {
		uint8 Msg[5];
        Msg[0] = 0; // Ping
		//TODO: byte order
		uint8* bytes = (uint8*)(&Uuid);
		Msg[1] = bytes[0];
		Msg[2] = bytes[1];
		Msg[3] = bytes[2];
		Msg[4] = bytes[3];

        rd_netclient_msg_push(Client, Msg, 5);
        LastPingTime = CurrentTime;
    }
    
    if (CurrentAvatarTime > LastAvatarTime + 0.1) {

		if (IsValid(Avatar) && !Avatar->IsNetProxy) {
			AvatarPack Pack;
			memset(&Pack, 0, sizeof(AvatarPack));
			FQuat Rotation;

			Pack.id = Avatar->NetID;

			Pack.root_px = Avatar->Location.X; Pack.root_py = Avatar->Location.Y; Pack.root_pz = Avatar->Location.Z;
			Rotation = Avatar->Rotation.Quaternion();
			Pack.root_rx = Rotation.X; Pack.root_ry = Rotation.Y; Pack.root_rz = Rotation.Z; Pack.root_rw = Rotation.W;

			Pack.head_px = Avatar->LocationHMD.X; Pack.head_py = Avatar->LocationHMD.Y; Pack.head_pz = Avatar->LocationHMD.Z;
			Rotation = Avatar->RotationHMD.Quaternion();
			Pack.head_rx = Rotation.X; Pack.head_ry = Rotation.Y; Pack.head_rz = Rotation.Z; Pack.head_rw = Rotation.W;

			Pack.handL_px = Avatar->LocationHandL.X; Pack.handL_py = Avatar->LocationHandL.Y; Pack.handL_pz = Avatar->LocationHandL.Z;
			Rotation = Avatar->RotationHandL.Quaternion();
			Pack.handL_rx = Rotation.X; Pack.handL_ry = Rotation.Y; Pack.handL_rz = Rotation.Z; Pack.handL_rw = Rotation.W;

			Pack.handR_px = Avatar->LocationHandR.X; Pack.handR_py = Avatar->LocationHandR.Y; Pack.handR_pz = Avatar->LocationHandR.Z;
			Rotation = Avatar->RotationHandR.Quaternion();
			Pack.handR_rx = Rotation.X; Pack.handR_ry = Rotation.Y; Pack.handR_rz = Rotation.Z; Pack.handR_rw = Rotation.W;

			Pack.height = Avatar->Height;
			Pack.floor = Avatar->Floor;

			rd_netclient_push_avatar(Client, &Pack);
		}

		// WorldPack WorldPack;
		// memset(&WorldPack, 0, sizeof(WorldPack));

        // TArray<RigidBodyPack> BodyPacks;
        // for (int Idx=0; Idx<NetRigidBodies.Num(); ++Idx) {
        //     UNetRigidBody* Body = NetRigidBodies[Idx];
        //     if (IsValid(Body) && Body->NetOwner == NetIndex) {
        //         AActor* Actor = Body->GetOwner();
        //         if (IsValid(Actor)) {
        //             FVector LinearVelocity;
        //             FVector Location = Actor->GetActorLocation();
        //             UStaticMeshComponent* StaticMesh = Actor->FindComponentByClass<UStaticMeshComponent>();
        //             if (StaticMesh) {
        //                 LinearVelocity = StaticMesh->GetBodyInstance()->GetUnrealWorldVelocity();
        //                 RigidBodyPack Pack = {Body->NetID,
        //                     Location.X, Location.Y, Location.Z, 1,
        //                     LinearVelocity.X, LinearVelocity.Y, LinearVelocity.Z, 0,
        //                 };
        //                 BodyPacks.Add(Pack);
        //             }
        //         }
        //     }
        // }

        LastAvatarTime = CurrentAvatarTime;
    }

	if (CurrentRigidbodyTime > LastRigidbodyTime + 0.1) {
		
	}
    
	int Loop = 0;
	for (; Loop < 1000; Loop += 1) {
		RustVec* RustMsg = rd_netclient_msg_pop(Client);
		uint8* Msg = (uint8*)RustMsg->vec_ptr;
		if (RustMsg->vec_len > 0) {

			if (Msg[0] == 0) { // Ping
				uint32 RemoteUuid = *((uint32*)(Msg + 1));
				// float* KeyValue = NetClients.Find(RemoteUuid);
				// if (KeyValue != NULL) {
				UE_LOG(RustyNet, Warning, TEXT("PING: %i"), RemoteUuid);
				// }
				NetClients.Add(RemoteUuid, CurrentTime);
				RebuildConsensus();
			}
			else if (Msg[0] == 1) { // World
				// WorldPack* WorldPack = rd_netclient_dec_world(&Msg[1], RustMsg->vec_len - 1);
				// uint64_t NumOfBodies = WorldPack->rigidbodies.vec_len;
				// RigidBodyPack* Bodies = (RigidBodyPack*)WorldPack->rigidbodies.vec_ptr;
				// for (auto Idx=0; Idx<NumOfBodies; ++Idx) {
				// 	FVector Location(Bodies[Idx].px, (MirrorSyncY ? -1 : 1) * Bodies[Idx].py, Bodies[Idx].pz);
				// 	FVector LinearVelocity(Bodies[Idx].lx, (MirrorSyncY ? -1 : 1) * Bodies[Idx].ly, Bodies[Idx].lz);
				// 	uint16 NetID = Bodies[Idx].id;
				// 	UNetRigidBody** NetRigidBody = NetRigidBodies.FindByPredicate([NetID](const UNetRigidBody* Item) {
				// 		return IsValid(Item) && Item->NetID == NetID;
				// 	});
				// 	if (NetRigidBody != NULL && *NetRigidBody != NULL) {
				// 		(*NetRigidBody)->SyncTarget = true;
				// 		(*NetRigidBody)->TargetLocation = Location;
				// 		(*NetRigidBody)->TargetLinearVelocity = LinearVelocity;
				// 	}
				// }
				// uint64_t NumOfParts = WorldPack->avatarparts.vec_len;
				// if (NumOfParts >= 4) {
				// 	AvatarPack* Parts = (AvatarPack*)WorldPack->avatarparts.vec_ptr;
				// 	uint16 NetID = Parts[0].id;
				// 	UNetAvatar** NetAvatar = NetAvatars.FindByPredicate([NetID](const UNetAvatar* Item) {
				// 		return IsValid(Item) && Item->NetID == NetID;
				// 	});
				// 	if (NetAvatar != NULL && *NetAvatar != NULL) {
				// 		(*NetAvatar)->LastUpdateTime = CurrentTime;
				// 		(*NetAvatar)->Location = FVector(Parts[0].px, Parts[0].py, Parts[0].pz);
				// 		(*NetAvatar)->Rotation = FRotator(FQuat(Parts[0].rx, Parts[0].ry, Parts[0].rz, Parts[0].rw));
				// 		(*NetAvatar)->LocationHMD = FVector(Parts[1].px, Parts[1].py, Parts[1].pz);
				// 		(*NetAvatar)->RotationHMD = FRotator(FQuat(Parts[1].rx, Parts[1].ry, Parts[1].rz, Parts[1].rw));
				// 		(*NetAvatar)->LocationHandL = FVector(Parts[2].px, Parts[2].py, Parts[2].pz);
				// 		(*NetAvatar)->RotationHandL = FRotator(FQuat(Parts[2].rx, Parts[2].ry, Parts[2].rz, Parts[2].rw));
				// 		(*NetAvatar)->LocationHandR = FVector(Parts[3].px, Parts[3].py, Parts[3].pz);
				// 		(*NetAvatar)->RotationHandR = FRotator(FQuat(Parts[3].rx, Parts[3].ry, Parts[3].rz, Parts[3].rw));
				// 	}
				// 	else {
				// 		MissingAvatar = (int)NetID;
				// 		UE_LOG(RustyNet, Warning, TEXT("NetClient MissingAvatar: %i"), NetID);
				// 	}
				// }
				// rd_netclient_drop_world(WorldPack);
			}
			else if (Msg[0] == 2) {
				AvatarPack* Pack = rd_netclient_dec_avatar(&Msg[1], RustMsg->vec_len - 1);
				uint32 NetID = Pack->id;
				UNetAvatar** NetAvatar = NetAvatars.FindByPredicate([NetID](const UNetAvatar* Item) {
					return IsValid(Item) && Item->NetID == NetID;
				});
				if (NetAvatar != NULL && *NetAvatar != NULL) {
					(*NetAvatar)->LastUpdateTime = CurrentTime;
					(*NetAvatar)->Location = FVector(Pack->root_px, Pack->root_py, Pack->root_pz);
					(*NetAvatar)->Rotation = FRotator(FQuat(Pack->root_rx, Pack->root_ry, Pack->root_rz, Pack->root_rw));
					(*NetAvatar)->LocationHMD = FVector(Pack->head_px, Pack->head_py, Pack->head_pz);
					(*NetAvatar)->RotationHMD = FRotator(FQuat(Pack->head_rx, Pack->head_ry, Pack->head_rz, Pack->head_rw));
					(*NetAvatar)->LocationHandL = FVector(Pack->handL_px, Pack->handL_py, Pack->handL_pz);
					(*NetAvatar)->RotationHandL = FRotator(FQuat(Pack->handL_rx, Pack->handL_ry, Pack->handL_rz, Pack->handL_rw));
					(*NetAvatar)->LocationHandR = FVector(Pack->handR_px, Pack->handR_py, Pack->handR_pz);
					(*NetAvatar)->RotationHandR = FRotator(FQuat(Pack->handR_rx, Pack->handR_ry, Pack->handR_rz, Pack->handR_rw));
					(*NetAvatar)->Height = Pack->height;
					(*NetAvatar)->Floor = Pack->floor;
				}
				else {
					MissingAvatar = (int)NetID;
					UE_LOG(RustyNet, Warning, TEXT("NetClient MissingAvatar: %i"), NetID);
				}
				rd_netclient_drop_avatar(Pack);
			}
			else if (Msg[0] == 10) { // System Float
				uint8 MsgSystem = Msg[1];
				uint8 MsgId = Msg[2];
				float* MsgValue = (float*)(Msg + 3);
				UE_LOG(RustyNet, Warning, TEXT("Msg IN MsgSystem: %u MsgId: %u MsgValue: %f"), Msg[1], Msg[2], *MsgValue);
				OnSystemFloatMsg.Broadcast(MsgSystem, MsgId, *MsgValue);
			}
			else if (Msg[0] == 11) { // System Int
				uint8 MsgSystem = Msg[1];
				uint8 MsgId = Msg[2];
				int32* MsgValue = (int32*)(Msg + 3);
				UE_LOG(RustyNet, Warning, TEXT("Msg IN MsgSystem: %u MsgId: %u MsgValue: %i"), Msg[1], Msg[2], *MsgValue);
				OnSystemIntMsg.Broadcast(MsgSystem, MsgId, *MsgValue);
			}
			else if (Msg[0] == 12) { // System String
				uint8 MsgSystem = Msg[1];
				uint8 MsgId = Msg[2];
				const char* MsgValuePtr = (const char*)(Msg + 3);
				FString MsgValue(MsgValuePtr);
				UE_LOG(RustyNet, Warning, TEXT("Msg IN MsgSystem: %u MsgId: %u MsgValue: %s"), Msg[1], Msg[2], *MsgValue);
				OnSystemStringMsg.Broadcast(MsgSystem, MsgId, *MsgValue);
			}
		}
		rd_netclient_msg_drop(RustMsg);
		if (RustMsg->vec_len == 0) {
			//UE_LOG(RustyNet, Warning, TEXT("NetClient Loop: %i"), Loop);
			break;
		}
	}
}

void ANetClient::SendSystemFloat(int32 System, int32 Id, float Value)
{
	uint8 Msg[7];
	Msg[0] = 10;
	Msg[1] = (uint8)System;
	Msg[2] = (uint8)Id;

	//TODO: byte order
	uint8* fbytes = (uint8*)(&Value);
	Msg[3] = fbytes[0];
	Msg[4] = fbytes[1];
	Msg[5] = fbytes[2];
	Msg[6] = fbytes[3];

	UE_LOG(RustyNet, Warning, TEXT("Msg OUT System Float: %u MsgId: %u MsgValue: %f"), Msg[1], Msg[2], Value);
	rd_netclient_msg_push(Client, Msg, 7);
}

void ANetClient::SendSystemInt(int32 System, int32 Id, int32 Value)
{
	uint8 Msg[7];
	Msg[0] = 11;
	Msg[1] = (uint8)System;
	Msg[2] = (uint8)Id;

	//TODO: byte order
	uint8* ibytes = (uint8*)(&Value);
	Msg[3] = ibytes[0];
	Msg[4] = ibytes[1];
	Msg[5] = ibytes[2];
	Msg[6] = ibytes[3];

	UE_LOG(RustyNet, Warning, TEXT("Msg OUT System Int: %u MsgId: %u MsgValue: %i"), Msg[1], Msg[2], Value);
	rd_netclient_msg_push(Client, Msg, 7);
}

void ANetClient::SendSystemString(int32 System, int32 Id, FString Value)
{
	uint8 Msg[2000];
	memset(Msg, 0, 2000);

	Msg[0] = 12;
	Msg[1] = (uint8)System;
	Msg[2] = (uint8)Id;

	const char* String = TCHAR_TO_ANSI(*Value);
	uint32 StringLen = (uint32)strnlen(String, 1000);
	strncpy((char*)(Msg + 3), String, StringLen);

	UE_LOG(RustyNet, Warning, TEXT("Msg OUT System Int: %u MsgId: %u MsgValue: %s"), Msg[1], Msg[2], *Value);
	rd_netclient_msg_push(Client, Msg, StringLen + 10);
}

