#include "pch.h"
#include "PrimitiveComponent.h"

void UPrimitiveComponent::SetMaterial(const FString& FilePath, EVertexLayoutType layoutType)
{
   
    Material = UResourceManager::GetInstance().GetOrCreateMaterial(FilePath,layoutType);
         
    
}