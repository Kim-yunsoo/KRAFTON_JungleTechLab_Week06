#pragma once
#include "LightComponent.h"

class UPointLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UPointLightComponent, ULightComponent);
	UPointLightComponent();
	
	UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
	
	float GetFalloff() { return LightFalloffExponent; } 
	void SetFalloff(float InFalloff) { LightFalloffExponent = InFalloff; }
protected:
	~UPointLightComponent();


protected:
	/** 반경 끝으로 갈 수록 얼마나 부드럽게 사라질지 정하는 인자, 8 전후로 설정*/
	float LightFalloffExponent;

};

