// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "DynamicalSystemsSettings.generated.h"

USTRUCT(BlueprintType)
struct FNetClientSettings {
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString Server;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString MumbleServer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString AudioDevice;
};
/**
 * 
 */
UCLASS(config = "DynamicalSystems", defaultconfig)
class DYNAMICALSYSTEMS_API UDynamicalSystemsSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UDynamicalSystemsSettings() {
		this->CurrentSettings = this->GetEnvSetting();
		this->_AvatarTable = nullptr;
	}
	~UDynamicalSystemsSettings() {}
private:
	static UDynamicalSystemsSettings* Get() {
		return GetMutableDefault<UDynamicalSystemsSettings>();
	}
public:
	UFUNCTION(BlueprintCallable,BlueprintPure)
	static UDynamicalSystemsSettings* GetDynamicalSettings() {
		return Get();
	}
public:

	UPROPERTY(config, EditAnywhere, Category = Dynamical)
		FName CurrentEnvironment;

	UPROPERTY(config, EditAnywhere, Category = Dynamical)
		TMap<FName, FNetClientSettings> Environments;

	UFUNCTION(BlueprintGetter)
		FNetClientSettings GetEnvSetting() {

		if (!Environments.Contains(CurrentEnvironment)) {
			return FNetClientSettings();
		}
		return  *Environments.Find(CurrentEnvironment);
	}
	UPROPERTY(config,VisibleAnywhere, Category = Dynamical, BlueprintGetter = GetEnvSetting)
		FNetClientSettings CurrentSettings;

	UPROPERTY(config, EditAnywhere, Category = Dynamical)
		float PingTimeout = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "Dynamical|Avatar")
	FSoftObjectPath DefaultAvatarTable;
	UPROPERTY(config, EditAnywhere, Category = "Dynamical|Avatar")
	FSoftObjectPath NewIKAvatarTable;
protected:
	UDataTable* _AvatarTable;
public:
	UFUNCTION(BlueprintCallable, Category = "Dynamical|Avatar")
		static UDataTable* GetAvatarTable() {
		if (Get()->_AvatarTable) return Get()->_AvatarTable;
		FName Path;
		if (Get()->UseNewIK) {
			Path = FName(*Get()->NewIKAvatarTable.ToString());
		}
		else {
			Path = FName(*Get()->DefaultAvatarTable.ToString());
		}
		Get()->_AvatarTable = LoadObjFromPath<UDataTable>(Path);
		return Get()->_AvatarTable;
	}
	UPROPERTY(config, EditAnywhere, Category = "Dynamical|Avatar")
	bool UseNewIK=false;
#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& e)
	{
		FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicalSystemsSettings, CurrentEnvironment))
		{
			CurrentSettings = GetEnvSetting();
		}
		Super::PostEditChangeProperty(e);
	}
#endif
	template <typename ObjClass>
	static FORCEINLINE ObjClass* LoadObjFromPath(const FName& Path)
	{
		if (Path == NAME_None) return NULL;
		//~

		return Cast<ObjClass>(StaticLoadObject(ObjClass::StaticClass(), NULL, *Path.ToString()));
	}
};
	