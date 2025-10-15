#pragma once
#include "SceneComponent.h"

class ULightComponent : public USceneComponent
{
public:
    DECLARE_CLASS(ULightComponent, USceneComponent)
    ULightComponent();
     
    // Proper duplicate override to carry light-specific properties
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;

    //FLinearColor GetLightColor() { return LightColor; }
    FVector4 GetLightColor() { return LightColor; }
    float GetAttenuationRadius() { return AttenuationRadius; }
    float GetIntensity() { return Intensity; }

    //void SetLightColor(FLinearColor Color) { LightColor = Color; }
    void SetLightColor(FVector4 Color) { LightColor = Color; }
    void SetAttenuationRadius(float Att) { AttenuationRadius = Att; }
    void SetIntensity(float InIntensity) {Intensity = InIntensity; }

protected:
    ~ULightComponent();

private:
    /** 빛의 색을 정하는 인자*/
    //FLinearColor LightColor;
    FVector4 LightColor;

    /** 감쇠되는 Radius */
    float AttenuationRadius;

    /** 빛의 세기, 강도 */
    float Intensity;
     
};

