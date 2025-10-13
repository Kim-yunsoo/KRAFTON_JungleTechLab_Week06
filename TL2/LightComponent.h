#pragma once
#include "SceneComponent.h"

class ULightComponent : public USceneComponent
{
public:
    DECLARE_CLASS(ULightComponent, USceneComponent)
    ULightComponent();

    UObject* Duplication(FObjectDuplicationParameters Parameters);

protected:
    ~ULightComponent();

private:
    /** 빛의 색을 정하는 인자*/
    FLinearColor LightColor;

    /** 감쇠되는 Radius */
    float AttenuationRadius;

    /** 빛의 세기, 강도 */
    float Intensity;
     
};

