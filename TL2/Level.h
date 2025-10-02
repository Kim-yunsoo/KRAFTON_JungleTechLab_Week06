﻿#pragma once
#include "pch.h"
#include "Object.h"
#include "Actor.h"

class ULevel : public UObject
{
public:
	DECLARE_CLASS(ULevel, UObject)
	ULevel();
	~ULevel() override;

	void AddActor(AActor* InActor);
	void RemoveActor(AActor* InActor);
	const TArray<AActor*>& GetActors() const;
	TArray<AActor*>& GetActors();
private:
	TArray<AActor*> Actors;
};

