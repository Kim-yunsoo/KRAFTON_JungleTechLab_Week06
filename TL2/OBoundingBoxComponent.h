﻿#pragma once
#include "ShapeComponent.h"

struct FBox
{
    FVector Min;
    FVector Max;

    FBox() : Min(FVector()), Max(FVector()) {}
    FBox(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}
};

class UOBoundingBoxComponent :
    public UShapeComponent
{
    DECLARE_CLASS(UOBoundingBoxComponent,UShapeComponent)
public:
    UOBoundingBoxComponent();

    UOBoundingBoxComponent(const FMatrix& World) : LineColor(1, 0, 0, 1)
    {
        UpdateFromWorld(World); 
    }

    // Update OBB 
    void UpdateFromWorld(const FMatrix& World);

    bool IntersectWithAABB(FBound AABB);

    // 주어진 로컬 버텍스들로부터 Min/Max 계산
    void SetFromVertices(const std::vector<FVector>& Verts);

    // 월드 좌표계에서의 AABB 반환
    FBox GetWorldBox() const;

    FVector GetCenter() const { return Center; }

    // 로컬 공간에서의 Extent (절반 크기)
    //FVector GetExtent() const;
    
    //월드 공간에서의 Extent
    FVector GetExtent() const;

    // 로컬 기준 8개 꼭짓점 반환
    std::vector<FVector> GetLocalCorners() const;

    FBox GetWorldOBBFromAttachParent() const;

	void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport = nullptr) override;
    // Debug 렌더링용
    // void DrawDebug(ID3D11DeviceContext* DC);


    void CreateLineData( 
        OUT TArray<FVector>& Start,
        OUT TArray<FVector>& End, 
        OUT TArray<FVector4>& Color);
private:
    FVector Axis[3];
    FVector Extent; // World 기준 Extent

    FVector Center;

    FVector LocalMin;
    FVector LocalMax;
    
    FVector4 LineColor;

};

