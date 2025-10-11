#include "pch.h"

#include "Picking.h"
#include "Actor.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "CameraActor.h"
#include "MeshLoader.h"
#include"Vector.h"
#include "SelectionManager.h"
#include <cmath>

#include "GizmoActor.h"
#include "GizmoScaleComponent.h"
#include "GizmoRotateComponent.h"
#include "GizmoArrowComponent.h"
#include "UI/GlobalConsole.h"
#include "ObjManager.h"
#include"stdio.h"
#include "AABoundingBoxComponent.h"
#include "PickingTimer.h"
#include "Octree.h"
#include "BVH.h"
#include "BoundingVolumeHierarchy.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Triangle.h"
#include "RenderingStats.h"
#include "BillboardComponent.h"

FRay MakeRayFromMouse(const FMatrix& InView,
                      const FMatrix& InProj)
{
    // 1) Mouse to NDC (DirectX viewport convention: origin top-left)
    //    Query current screen size from InputManager
    FVector2D screen = UInputManager::GetInstance().GetScreenSize();
    float viewportW = (screen.X > 1.0f) ? screen.X : 1.0f;
    float viewportH = (screen.Y > 1.0f) ? screen.Y : 1.0f;

    const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
    const float NdcX = (2.0f * MousePosition.X / viewportW) - 1.0f;
    const float NdcY = 1.0f - (2.0f * MousePosition.Y / viewportH);

    // 2) View-space direction using projection scalars (PerspectiveFovLH)
    // InProj.M[0][0] = XScale, InProj.M[1][1] = YScale
    const float XScale = InProj.M[0][0];
    const float YScale = InProj.M[1][1];
    const float ViewDirX = NdcX / (XScale == 0.0f ? 1.0f : XScale);
    const float ViewDirY = NdcY / (YScale == 0.0f ? 1.0f : YScale);
    const float ViewDirZ = 1.0f; // Forward in view space

    // 3) Extract camera basis/position from InView (row-vector convention: basis in rows)
    const FVector Right = FVector(InView.M[0][0], InView.M[0][1], InView.M[0][2]);
    const FVector Up = FVector(InView.M[1][0], InView.M[1][1], InView.M[1][2]);
    const FVector Forward = FVector(InView.M[2][0], InView.M[2][1], InView.M[2][2]);
    const FVector t = FVector(InView.M[3][0], InView.M[3][1], InView.M[3][2]);
    // = (-dot(Right,Eye), -dot(Up,Eye), -dot(Fwd,Eye))
    const FVector Eye = (Right * (-t.X)) + (Up * (-t.Y)) + (Forward * (-t.Z));

    // 4) To world space
    const FVector WorldDirection = (Right * ViewDirX + Up * ViewDirY + Forward * ViewDirZ).GetSafeNormal();

    FRay Ray;
    Ray.Origin = Eye;
    Ray.Direction = WorldDirection;
    return Ray;
}

FRay MakeRayFromMouseWithCamera(const FMatrix& InView,
                                const FMatrix& InProj,
                                const FVector& CameraWorldPos,
                                const FVector& CameraRight,
                                const FVector& CameraUp,
                                const FVector& CameraForward)
{
    // 1) Mouse to NDC (DirectX viewport convention: origin top-left)
    FVector2D screen = UInputManager::GetInstance().GetScreenSize();
    float viewportW = (screen.X > 1.0f) ? screen.X : 1.0f;
    float viewportH = (screen.Y > 1.0f) ? screen.Y : 1.0f;

    const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
    const float NdcX = (2.0f * MousePosition.X / viewportW) - 1.0f;
    const float NdcY = 1.0f - (2.0f * MousePosition.Y / viewportH);

    // 2) View-space direction using projection scalars
    const float XScale = InProj.M[0][0];
    const float YScale = InProj.M[1][1];
    const float ViewDirX = NdcX / (XScale == 0.0f ? 1.0f : XScale);
    const float ViewDirY = NdcY / (YScale == 0.0f ? 1.0f : YScale);
    const float ViewDirZ = 1.0f; // Forward in view space

    // 3) Use camera's actual world-space orientation vectors
    // Transform view direction to world space using camera's real orientation
    const FVector WorldDirection = (CameraRight * ViewDirX + CameraUp * ViewDirY + CameraForward * ViewDirZ).
        GetSafeNormal();

    FRay Ray;
    Ray.Origin = CameraWorldPos;
    Ray.Direction = WorldDirection;
    return Ray;
}

FRay MakeRayFromViewport(const FMatrix& InView,
                         const FMatrix& InProj,
                         const FVector& CameraWorldPos,
                         const FVector& CameraRight,
                         const FVector& CameraUp,
                         const FVector& CameraForward,
                         const FVector2D& ViewportMousePos,
                         const FVector2D& ViewportSize,
                         const FVector2D& ViewportOffset)
{
    // 1) Convert global mouse position to viewport-relative position
    float localMouseX = ViewportMousePos.X - ViewportOffset.X;
    float localMouseY = ViewportMousePos.Y - ViewportOffset.Y;

    // 2) Use viewport-specific size for NDC conversion
    float viewportW = (ViewportSize.X > 1.0f) ? ViewportSize.X : 1.0f;
    float viewportH = (ViewportSize.Y > 1.0f) ? ViewportSize.Y : 1.0f;

    const float NdcX = (2.0f * localMouseX / viewportW) - 1.0f;
    const float NdcY = 1.0f - (2.0f * localMouseY / viewportH);

    // Check if this is orthographic projection
    bool bIsOrthographic = std::fabs(InProj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;

    FRay Ray;

    if (bIsOrthographic)
    {
        // Orthographic projection
        // Get orthographic bounds from projection matrix
        float OrthoWidth = 2.0f / InProj.M[0][0];
        float OrthoHeight = 2.0f / InProj.M[1][1];

        // Calculate world space offset from camera center
        float WorldOffsetX = NdcX * OrthoWidth * 0.5f;
        float WorldOffsetY = NdcY * OrthoHeight * 0.5f;

        // Ray origin is offset from camera position on the viewing plane
        Ray.Origin = CameraWorldPos + (CameraRight * WorldOffsetX) + (CameraUp * WorldOffsetY);

        // Ray direction is always forward for orthographic
        Ray.Direction = CameraForward;
    }
    else
    {
        // Perspective projection (existing code)
        const float XScale = InProj.M[0][0];
        const float YScale = InProj.M[1][1];
        const float ViewDirX = NdcX / (XScale == 0.0f ? 1.0f : XScale);
        const float ViewDirY = NdcY / (YScale == 0.0f ? 1.0f : YScale);
        const float ViewDirZ = 1.0f;

        const FVector WorldDirection = (CameraRight * ViewDirX + CameraUp * ViewDirY + CameraForward * ViewDirZ).GetSafeNormal();

        Ray.Origin = CameraWorldPos;
        Ray.Direction = WorldDirection;
    }

    return Ray;
}

bool IntersectRaySphere(const FRay& InRay, const FVector& InCenter, float InRadius, float& OutT)
{
    // Solve ||(RayOrigin + T*RayDir) - Center||^2 = Radius^2
    const FVector OriginToCenter = InRay.Origin - InCenter;
    const float QuadraticA = FVector::Dot(InRay.Direction, InRay.Direction); // Typically 1 for normalized ray
    const float QuadraticB = 2.0f * FVector::Dot(OriginToCenter, InRay.Direction);
    const float QuadraticC = FVector::Dot(OriginToCenter, OriginToCenter) - InRadius * InRadius;

    const float Discriminant = QuadraticB * QuadraticB - 4.0f * QuadraticA * QuadraticC;
    if (Discriminant < 0.0f)
    {
        return false;
    }

    const float SqrtD = std::sqrt(Discriminant >= 0.0f ? Discriminant : 0.0f);
    const float Inv2A = 1.0f / (2.0f * QuadraticA);
    const float T0 = (-QuadraticB - SqrtD) * Inv2A;
    const float T1 = (-QuadraticB + SqrtD) * Inv2A;

    // Pick smallest positive T
    const float ClosestT = (T0 > 0.0f) ? T0 : T1;
    if (ClosestT <= 0.0f)
    {
        return false;
    }

    OutT = ClosestT;
    return true;
}

bool IntersectRayTriangleMT(const FRay& InRay,
                            const FVector& InA,
                            const FVector& InB,
                            const FVector& InC,
                            float& OutT)
{
    const float Epsilon = KINDA_SMALL_NUMBER;

    const FVector Edge1 = InB - InA;
    const FVector Edge2 = InC - InA;

    const FVector Perpendicular = FVector::Cross(InRay.Direction, Edge2);
    const float Determinant = FVector::Dot(Edge1, Perpendicular);

    if (Determinant > -Epsilon && Determinant < Epsilon)
        return false; // Parallel to triangle

    const float InvDeterminant = 1.0f / Determinant;
    const FVector OriginToA = InRay.Origin - InA;
    const float U = InvDeterminant * FVector::Dot(OriginToA, Perpendicular);
    if (U < -Epsilon || U > 1.0f + Epsilon)
        return false;

    const FVector CrossQ = FVector::Cross(OriginToA, Edge1);
    const float V = InvDeterminant * FVector::Dot(InRay.Direction, CrossQ);
    if (V < -Epsilon || (U + V) > 1.0f + Epsilon)
        return false;

    // Compute T to find out where the intersection point is on the ray
    const float Distance = InvDeterminant * FVector::Dot(Edge2, CrossQ);

    if (Distance > Epsilon) // ray intersection
    {
        OutT = Distance;
        return true;
    }
    return false;
}

// slab method - check intersect between Ray and AABB - 미완성
bool IntersectRayBound(const FRay& InRay, const FBound& InBound, float* OutT)
{
    float tmin = 0.0f;
    float tmax = FLT_MAX;

    // 브랜치리스 최적화된 3축 테스트
    const float* rayOrigin = &InRay.Origin.X;
    const float* rayDir = &InRay.Direction.X;
    const float* boxMin = &InBound.Min.X;
    const float* boxMax = &InBound.Max.X;

    for (int i = 0; i < 3; ++i)
    {
        // 레이 방향이 0에 가까운지 체크
        float absDir = std::abs(rayDir[i]);
        bool nearZero = absDir < KINDA_SMALL_NUMBER;

        if (nearZero)
        {
            // 평행한 경우: 레이가 슬랩 바깥에 있으면 교차 없음
            if (rayOrigin[i] < boxMin[i] || rayOrigin[i] > boxMax[i])
                return false;
            continue;
        }

        // 브랜치리스 min/max 계산
        float invDir = 1.0f / rayDir[i];
        float t0 = (boxMin[i] - rayOrigin[i]) * invDir;
        float t1 = (boxMax[i] - rayOrigin[i]) * invDir;

        // 브랜치리스 swap: fmin/fmax를 사용하여 분기 제거
        float tNear = std::fmin(t0, t1);
        float tFar = std::fmax(t0, t1);

        // 브랜치리스 update
        tmin = std::fmax(tmin, tNear);
        tmax = std::fmin(tmax, tFar);

        // 조기 종료 체크
        if (tmin > tmax) return false;
    }

    // 최종 유효성 검사
    if (tmax < 0.0f) return false;

    if (OutT)
    {
        *OutT = (tmin > 0.0f) ? tmin : tmax;
    }

    return true;
}

/**
  * @brief 메시의 BVH(Bounding Volume Hierarchy)를 재귀적으로 순회하며, 주어진 광선과 가장 가까운 삼각형의 교차점을 찾는 헬퍼 함수
   *        '분할 정복' 전략을 사용하여 불필요한 삼각형 검사를 건너뛰어 성능을 크게 향상시킨다.
   *
   * @param LocalRay          메시의 로컬 공간(모델 원점 기준)으로 변환된 광선. BVH는 로컬 공간 기준으로 만들어짐
   * @param Node              현재 탐색하고 있는 BVH 트리의 노드.
   * @param MeshAsset         삼각형의 원본 정점/인덱스 데이터를 담고 있는 FStaticMesh 객체. 리프 노드에서 실제 정점 위치를 참조할 때 필요
   * @param OutClosestHitDistance 현재까지 찾은 가장 가까운 충돌 거리를 저장하는 변수. 재귀 호출을 거치며 계속 갱신 가능
   * @return bool             노드 or 자식들 중에서 유효한 충돌 발생 시 true
*/
bool IntersectTriangleBVH(const FRay& LocalRay, FNarrowPhaseBVHNode* Node, const FStaticMesh* MeshAsset, float& OutClosestHitDistance)
{
    if (!Node) return false;
    
    // 광선이 현재 노드를 감싸는 거대한 Bounding box와 충돌하는지 확인
    // 광선이 box와 충돌하지 않거나, 충돌거리가 이미 찾은 가장 가까운 삼각형보다 멀리 있으면 
    // 이 노드 내부에 있는 삼각형들은 검사할 필요 없으므로 탐색 종료
    float nodeHitDist;
    if (!Node->Bounds.RayIntersects(LocalRay.Origin, LocalRay.Direction, nodeHitDist) 
        || nodeHitDist >= OutClosestHitDistance)
    {
        return false;
    }
    // 현재 노드 or 자식 노드에서 충돌이 발생했는지 추적위한 플래그
    bool bHit = false;
    
    // mesh BVH의 리프 노드에 도달했는지 확인
    if (Node->IsLeaf())
    {
        // 리프 노드에 포함된 소수의 삼각형들에 대해서만 충돌 검사
        for (const auto& Primitive : Node->Primitives)
        {
            // 리프 노드 삼각형의 Primitive 
            const uint32 i0 = MeshAsset->Indices[Primitive.TriangleIndex * 3 + 0];
            const uint32 i1 = MeshAsset->Indices[Primitive.TriangleIndex * 3 + 1];
            const uint32 i2 = MeshAsset->Indices[Primitive.TriangleIndex * 3 + 2];
    
            const FVector& v0 = MeshAsset->Vertices[i0].pos;
            const FVector& v1 = MeshAsset->Vertices[i1].pos;
            const FVector& v2 = MeshAsset->Vertices[i2].pos;
    
            
            // 가져온 정점으로 묄러트럼보어 실행 -> 실제 광선과 삼각형의 교차 검사
            float triangleHitDist;
            if (IntersectRayTriangleMT(LocalRay, v0, v1, v2, triangleHitDist))
            {
                if (triangleHitDist < OutClosestHitDistance)
                {
                    OutClosestHitDistance = triangleHitDist;
                    bHit = true;
                }
            }
        }
        return bHit;
    }
    
    // 리프노드가 아닌 경우 재귀
    if (IntersectTriangleBVH(LocalRay, Node->Left, MeshAsset, OutClosestHitDistance))
    {
        // 왼쪽 자식 노드에서 충돌 발생했다면 플래그 설정
        // 더 가까운 삼각형을 찾아 OutClosestHitDistance가 갱신되었다면, 오른쪽 노드 탐색은 더 엄격한 거리 조건으로 탐색하게됨 -> 효율 상승
        bHit = true;
    } 
    if (IntersectTriangleBVH(LocalRay, Node->Right, MeshAsset, OutClosestHitDistance))
    {
        bHit = true;
    }
    
    return bHit;
    
}

AActor* CPickingSystem::PerformViewportPicking(const TArray<AActor*>& Actors,
                                               ACameraActor* Camera,
                                               const FVector2D& ViewportMousePos,
                                               const FVector2D& ViewportSize,
                                               const FVector2D& ViewportOffset,
                                               float ViewportAspectRatio, FViewport*  Viewport)
{
    TStatId ViewportAspectPickingStatId;
    FScopeCycleCounter ViewportAspectPickingTimer(ViewportAspectPickingStatId);


    if (!Camera) return nullptr;

    // 1. 월드 공간 Ray 생성
    const FMatrix View = Camera->GetViewMatrix();
    const FMatrix Proj = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
    FRay WorldRay = MakeRayFromViewport(View, Proj, Camera->GetActorLocation(), Camera->GetRight(),
        Camera->GetUp(), Camera->GetForward(),
        ViewportMousePos, ViewportSize, ViewportOffset);

    // 2. 최종적으로 선택될 액터와 그 거리
    AActor* finalHitActor = nullptr;
    float finalClosestHitDistance = FLT_MAX;

    FBVH* BVH = GetEngine()->GetWorld()->GetBVH();
    if (BVH)
    {
        // 3. BVH로 Ray와 가장 가까운 Actor 반환
        float hitDistance;
        // Ray와 충돌하는 가장 가까운 액터를 정밀 검사까지 마쳐서 찾아줌.
        AActor* HitActor = BVH->Intersect(WorldRay.Origin, WorldRay.Direction, hitDistance);

        //if (CheckActorPicking(HitActor, WorldRay, hitDistance))
        if(HitActor)
        {
            // 충돌했고, 기존에 찾은 것보다 더 가깝다면 최종 후보를 교체
            if (hitDistance < finalClosestHitDistance)
            {
                finalClosestHitDistance = hitDistance;
                finalHitActor = HitActor;
               
            }
        }
    }

    uint64_t ViewportAspectCycleDiff = ViewportAspectPickingTimer.Finish();
    double ViewportAspectPickingTimeMs = FPlatformTime::ToMilliseconds(ViewportAspectCycleDiff);
    URenderingStatsCollector::GetInstance().UpdatePickingStats(ViewportAspectPickingTimeMs);
    // 4. 모든 후보 검사가 끝난 후, 최종 결과를 반환
    if (finalHitActor)
    {
        char buf[256];
      /*  sprintf_s(buf, "[Precision Pick] Hit actor '%s' at distance %.3f\n",
            finalHitActor->GetName().ToString(), finalClosestHitDistance);*/
        sprintf_s(buf, "[Viewport Pick] Hit %s at t=%.3f (Time: %.3fms)\n",
            finalHitActor->GetName().ToString().c_str(), finalClosestHitDistance, ViewportAspectPickingTimeMs);
        UE_LOG(buf);
    }
    else
    {
        UE_LOG("[Precision Pick] No hit found\n");
    }

    return finalHitActor;
}

uint32 CPickingSystem::IsHoveringGizmo(AGizmoActor* GizmoTransActor, const ACameraActor* Camera)
{
    if (!GizmoTransActor || !Camera)
        return 0;

    // 현재 활성 뷰포트 정보 가져오기 (UI 시스템에서)
    FVector2D ViewportMousePos = UInputManager::GetInstance().GetMousePosition();
    FVector2D ViewportSize = UInputManager::GetInstance().GetScreenSize();
    FVector2D ViewportOffset = FVector2D(0, 0);

    // 멀티 뷰포트인 경우 현재 뷰포트의 정보를 사용
    // 4분할 화면 등에서는 각 뷰포트의 offset과 size를 올바르게 계산해야 함

    // 뷰포트별 레이 생성
    const FMatrix View = Camera->GetViewMatrix();
    const FMatrix Proj = Camera->GetProjectionMatrix();
    const FVector CameraWorldPos = Camera->GetActorLocation();
    const FVector CameraRight = Camera->GetRight();
    const FVector CameraUp = Camera->GetUp();
    const FVector CameraForward = Camera->GetForward();
    FRay Ray = MakeRayFromViewport(View, Proj, CameraWorldPos, CameraRight, CameraUp, CameraForward,
                                   ViewportMousePos, ViewportSize, ViewportOffset);

    uint32 ClosestAxis = 0;
    float ClosestDistance = 1e9f;
    float HitDistance;

    // X축 화살표 검사
    //Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowX());

    switch (GizmoTransActor->GetMode())
    {
        case EGizmoMode::Translate:
            if (UStaticMeshComponent* ArrowX = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowX()))
            {
                if (CheckGizmoComponentPicking(ArrowX, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 1;
                    }
                }
            }

            // Y축 화살표 검사
            if (UStaticMeshComponent* ArrowY = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowY()))
            {
                if (CheckGizmoComponentPicking(ArrowY, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 2;
                    }
                }
            }

            // Z축 화살표 검사
            if (UStaticMeshComponent* ArrowZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowZ()))
            {
                if (CheckGizmoComponentPicking(ArrowZ, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 3;
                    }
                }
            }
            break;
        case EGizmoMode::Scale:
            if (UStaticMeshComponent* ScaleX = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleX()))
            {
                if (CheckGizmoComponentPicking(ScaleX, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 1;
                    }
                }
            }

            // Y축 화살표 검사
            if (UStaticMeshComponent* ScaleY = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleY()))
            {
                if (CheckGizmoComponentPicking(ScaleY, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 2;
                    }
                }
            }

            // Z축 화살표 검사
            if (UStaticMeshComponent* ScaleZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleZ()))
            {
                if (CheckGizmoComponentPicking(ScaleZ, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 3;
                    }
                }
            }
            break;
        case EGizmoMode::Rotate:
            if (UStaticMeshComponent* RotateX = Cast<UStaticMeshComponent>(GizmoTransActor->GetRotateX()))
            {
                if (CheckGizmoComponentPicking(RotateX, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 1;
                    }
                }
            }

            // Y축 화살표 검사
            if (UStaticMeshComponent* RotateY = Cast<UStaticMeshComponent>(GizmoTransActor->GetRotateY()))
            {
                if (CheckGizmoComponentPicking(RotateY, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 2;
                    }
                }
            }

            // Z축 화살표 검사
            if (UStaticMeshComponent* RotateZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetRotateZ()))
            {
                if (CheckGizmoComponentPicking(RotateZ, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 3;
                    }
                }
            }
            break;
    default:
        break;
    }



    return ClosestAxis;
}

uint32 CPickingSystem::IsHoveringGizmoForViewport(AGizmoActor* GizmoTransActor, const ACameraActor* Camera,
                                                  const FVector2D& ViewportMousePos,
                                                  const FVector2D& ViewportSize,
                                                  const FVector2D& ViewportOffset,FViewport* Viewport)
{
    if (!GizmoTransActor || !Camera)
        return 0;
    float ViewportAspectRatio = ViewportSize.X / ViewportSize.Y;
    if (ViewportSize.Y == 0) ViewportAspectRatio = 1.0f; // 0으로 나누기 방지
    // 뷰포트별 레이 생성 - 전달받은 뷰포트 정보 사용
    const FMatrix View = Camera->GetViewMatrix();
    const FMatrix Proj = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
    const FVector CameraWorldPos = Camera->GetActorLocation();
    const FVector CameraRight = Camera->GetRight();
    const FVector CameraUp = Camera->GetUp();
    const FVector CameraForward = Camera->GetForward();
    FRay Ray = MakeRayFromViewport(View, Proj, CameraWorldPos, CameraRight, CameraUp, CameraForward,
                                   ViewportMousePos, ViewportSize, ViewportOffset);
    // char debugBuf[512];
    //sprintf_s(
    //    debugBuf,
    //    "Mouse Local: (%.1f, %.1f) | Global: (%.1f, %.1f)\n"
    //    "Viewport Size: (%.1f, %.1f) | Offset: (%.1f, %.1f)\n"
    //    "Ray Origin: (%.2f, %.2f, %.2f) | Ray Dir: (%.2f, %.2f, %.2f)\n",
    //    static_cast<float>(ViewportMousePos.X), static_cast<float>(ViewportMousePos.Y),        // 로컬 마우스 좌표
    //    ViewportMousePos.X, ViewportMousePos.Y,              // 글로벌 마우스 좌표
    //    ViewportSize.X, ViewportSize.Y,                      // 뷰포트 크기
    //    ViewportOffset.X, ViewportOffset.Y,                  // 뷰포트 오프셋
    //    Ray.Origin.X, Ray.Origin.Y, Ray.Origin.Z,            // 레이 시작점
    //    Ray.Direction.X, Ray.Direction.Y, Ray.Direction.Z    // 레이 방향
    //);
    //UE_LOG(debugBuf);
    uint32 ClosestAxis = 0;
    float ClosestDistance = 1e9f;
    float HitDistance;

    // X축 화살표 검사
    //Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowX());

    switch (GizmoTransActor->GetMode())
    {
        case EGizmoMode::Translate:
            if (UStaticMeshComponent* ArrowX = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowX()))
            {
                if (CheckGizmoComponentPicking(ArrowX, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 1;
                    }
                }
            }

            // Y축 화살표 검사
            if (UStaticMeshComponent* ArrowY = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowY()))
            {
                if (CheckGizmoComponentPicking(ArrowY, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 2;
                    }
                }
            }

            // Z축 화살표 검사
            if (UStaticMeshComponent* ArrowZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowZ()))
            {
                if (CheckGizmoComponentPicking(ArrowZ, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 3;
                    }
                }
            }
            break;
        case EGizmoMode::Scale:
            if (UStaticMeshComponent* ScaleX = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleX()))
            {
                if (CheckGizmoComponentPicking(ScaleX, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 1;
                    }
                }
            }

            // Y축 화살표 검사
            if (UStaticMeshComponent* ScaleY = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleY()))
            {
                if (CheckGizmoComponentPicking(ScaleY, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 2;
                    }
                }
            }

            // Z축 화살표 검사
            if (UStaticMeshComponent* ScaleZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleZ()))
            {
                if (CheckGizmoComponentPicking(ScaleZ, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 3;
                    }
                }
            }
            break;
        case EGizmoMode::Rotate:
            if (UStaticMeshComponent* RotateX = Cast<UStaticMeshComponent>(GizmoTransActor->GetRotateX()))
            {
                if (CheckGizmoComponentPicking(RotateX, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 1;
                    }
                }
            }

            // Y축 화살표 검사
            if (UStaticMeshComponent* RotateY = Cast<UStaticMeshComponent>(GizmoTransActor->GetRotateY()))
            {
                if (CheckGizmoComponentPicking(RotateY, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 2;
                    }
                }
            }

            // Z축 화살표 검사
            if (UStaticMeshComponent* RotateZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetRotateZ()))
            {
                if (CheckGizmoComponentPicking(RotateZ, Ray, HitDistance))
                {
                    if (HitDistance < ClosestDistance)
                    {
                        ClosestDistance = HitDistance;
                        ClosestAxis = 3;
                    }
                }
            }
            break;
    default:
        break;
    }

    return ClosestAxis;
}



void CPickingSystem::DragActorWithGizmo(AActor* Actor, AGizmoActor*  GizmoActor,uint32 GizmoAxis, const FVector2D& MouseDelta, const ACameraActor* Camera, EGizmoMode InGizmoMode)
{
    
    if (!Actor || !Camera || GizmoAxis == 0) 
        return;
    if (GizmoActor) {
        GizmoActor->OnDrag(Actor, GizmoAxis, MouseDelta.X, MouseDelta.Y, Camera, nullptr);
    }
}


bool CPickingSystem::CheckGizmoComponentPicking(const UStaticMeshComponent* Component, const FRay& Ray, float& OutDistance)
{
    if (!Component) return false;

    // Gizmo 메시는 FStaticMesh(쿠킹된 데이터)를 사용
    FStaticMesh* StaticMesh = FObjManager::LoadObjStaticMeshAsset(
        Component->GetStaticMesh()->GetFilePath()
    );
    if (!StaticMesh) return false;

    // 피킹 계산에는 컴포넌트의 월드 변환 행렬 사용
    FMatrix WorldMatrix = Component->GetWorldMatrix();

    auto TransformPoint = [&](float X, float Y, float Z) -> FVector
        {
            // row-vector (v^T) * M 방식으로 월드 변환 (translation 반영)
            FVector4 V4(X, Y, Z, 1.0f);
            FVector4 OutV4;
            OutV4.X = V4.X * WorldMatrix.M[0][0] + V4.Y * WorldMatrix.M[1][0] + V4.Z * WorldMatrix.M[2][0] + V4.W * WorldMatrix.M[3][0];
            OutV4.Y = V4.X * WorldMatrix.M[0][1] + V4.Y * WorldMatrix.M[1][1] + V4.Z * WorldMatrix.M[2][1] + V4.W * WorldMatrix.M[3][1];
            OutV4.Z = V4.X * WorldMatrix.M[0][2] + V4.Y * WorldMatrix.M[1][2] + V4.Z * WorldMatrix.M[2][2] + V4.W * WorldMatrix.M[3][2];
            OutV4.W = V4.X * WorldMatrix.M[0][3] + V4.Y * WorldMatrix.M[1][3] + V4.Z * WorldMatrix.M[2][3] + V4.W * WorldMatrix.M[3][3];
            return FVector(OutV4.X, OutV4.Y, OutV4.Z);
        };

    float ClosestT = 1e9f;
    bool bHasHit = false;

    // 인덱스가 있는 경우: 인덱스 삼각형 집합 검사
    if (StaticMesh->Indices.Num() >= 3)
    {
        uint32 IndexNum = StaticMesh->Indices.Num();
        for (uint32 Idx = 0; Idx + 2 < IndexNum; Idx += 3)
        {
            const FNormalVertex& V0N = StaticMesh->Vertices[StaticMesh->Indices[Idx + 0]];
            const FNormalVertex& V1N = StaticMesh->Vertices[StaticMesh->Indices[Idx + 1]];
            const FNormalVertex& V2N = StaticMesh->Vertices[StaticMesh->Indices[Idx + 2]];

            FVector A = TransformPoint(V0N.pos.X, V0N.pos.Y, V0N.pos.Z);
            FVector B = TransformPoint(V1N.pos.X, V1N.pos.Y, V1N.pos.Z);
            FVector C = TransformPoint(V2N.pos.X, V2N.pos.Y, V2N.pos.Z);

            float THit;
            if (IntersectRayTriangleMT(Ray, A, B, C, THit))
            {
                if (THit < ClosestT)
                {
                    ClosestT = THit;
                    bHasHit = true;
                }
            }
        }
    }
    // 인덱스가 없는 경우: 정점 배열을 순차적 삼각형으로 간주
    else if (StaticMesh->Vertices.Num() >= 3)
    {
        uint32 VertexNum = StaticMesh->Vertices.Num();
        for (uint32 Idx = 0; Idx + 2 < VertexNum; Idx += 3)
        {
            const FNormalVertex& V0N = StaticMesh->Vertices[Idx + 0];
            const FNormalVertex& V1N = StaticMesh->Vertices[Idx + 1];
            const FNormalVertex& V2N = StaticMesh->Vertices[Idx + 2];

            FVector A = TransformPoint(V0N.pos.X, V0N.pos.Y, V0N.pos.Z);
            FVector B = TransformPoint(V1N.pos.X, V1N.pos.Y, V1N.pos.Z);
            FVector C = TransformPoint(V2N.pos.X, V2N.pos.Y, V2N.pos.Z);

            float THit;
            if (IntersectRayTriangleMT(Ray, A, B, C, THit))
            {
                if (THit < ClosestT)
                {
                    ClosestT = THit;
                    bHasHit = true;
                }
            }
        }
    }

    // 가장 가까운 교차가 있으면 거리 반환
    if (bHasHit)
    {
        OutDistance = ClosestT;
        return true;
    }

    return false;
}

bool CPickingSystem::CheckActorPicking(AActor* Actor, USceneComponent*& OutComponent, const FRay& Ray, float& OutDistance)
{
    if (!Actor) return false;

    float ClosestDistance = FLT_MAX;
    USceneComponent* ClosestComponent = nullptr;
    bool bHit = false;

    // 1. StaticMeshComponent 검사
    const TSet<UStaticMeshComponent*> StaticMeshComponents = Actor->GetComponents<UStaticMeshComponent>();
    for (UStaticMeshComponent* MeshComponent : StaticMeshComponents)
    {
        UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
        if (StaticMesh == nullptr)
        {
            continue;
        }

        FNarrowPhaseBVHNode* MeshBVH = StaticMesh->GetMeshBVH();
        if (MeshBVH)
        {
            // 월드 → 로컬 변환
            FMatrix InvWorldMatrix = MeshComponent->GetWorldMatrix().InverseAffine();

            FRay LocalRay;
            {
                // Origin (위치, w=1)
                FVector4 Origin4(Ray.Origin.X, Ray.Origin.Y, Ray.Origin.Z, 1.0f);
                FVector4 TransformedOrigin4 = Origin4 * InvWorldMatrix;
                LocalRay.Origin = FVector(TransformedOrigin4.X, TransformedOrigin4.Y, TransformedOrigin4.Z);

                // Direction (벡터, w=0)
                FVector4 Direction4(Ray.Direction.X, Ray.Direction.Y, Ray.Direction.Z, 0.0f);
                FVector4 TransformedDirection4 = Direction4 * InvWorldMatrix;
                LocalRay.Direction = FVector(TransformedDirection4.X, TransformedDirection4.Y, TransformedDirection4.Z);
                LocalRay.Direction.Normalize();
            }

            float ClosestLocalHitDist = FLT_MAX;
            if (IntersectTriangleBVH(LocalRay, MeshBVH, StaticMesh->GetStaticMeshAsset(), ClosestLocalHitDist))
            {
                // 로컬 히트 포인트
                FVector LocalHitPoint = LocalRay.Origin + LocalRay.Direction * ClosestLocalHitDist;

                // 로컬 → 월드 변환 (row-vector 규약: v * WorldMatrix)
                FMatrix WorldMatrix = Actor->GetWorldMatrix();
                FVector4 LocalHit4(LocalHitPoint.X, LocalHitPoint.Y, LocalHitPoint.Z, 1.0f);
                FVector4 WorldHit4 = LocalHit4 * WorldMatrix;
                FVector WorldHitPoint(WorldHit4.X, WorldHit4.Y, WorldHit4.Z);

                // 월드 거리 계산
                float Distance = (WorldHitPoint - Ray.Origin).Size();
                if (Distance < ClosestDistance)
                {
                    ClosestDistance = Distance;
                    ClosestComponent = MeshComponent;
                    bHit = true;
                }
            }
        }
    }

    // 2. BillboardComponent 검사
    const TSet<UBillboardComponent*> BillboardComponents = Actor->GetComponents<UBillboardComponent>();
    for (UBillboardComponent* BillboardComponent : BillboardComponents)
    {
        float HitDistance;
        if (CheckBillboardComponentPicking(BillboardComponent, Ray, HitDistance))
        {
            if (HitDistance < ClosestDistance)
            {
                ClosestDistance = HitDistance;
                ClosestComponent = BillboardComponent;
                bHit = true;
            }
        }
    }

    // 가장 가까운 컴포넌트 반환
    if (bHit)
    {
        OutDistance = ClosestDistance;
        OutComponent = ClosestComponent;
        return true;
    }

    OutDistance = -1;
    return false;
}


float CPickingSystem::GetAdaptiveThreshold(float cameraDistance)
{
    // 거리 기반 적응형 임계값 - 가까운 거리일수록 정밀하게
    if (cameraDistance < 1.0f)   return 0.001f;  // 1mm (매우 가까운 UI/도구)
    if (cameraDistance < 10.0f)  return 0.01f;   // 1cm (가까운 객체)
    if (cameraDistance < 100.0f) return 0.1f;    // 10cm (중간 거리)
    if (cameraDistance < 1000.0f) return 1.0f;   // 1m (먼 거리)
    return 10.0f;  // 10m (매우 먼 거리)
}

USceneComponent* CPickingSystem::PerformGlobalBVHPicking(const TArray<AActor*>& Actors,
    ACameraActor* Camera,
    const FVector2D& ViewportMousePos,
    const FVector2D& ViewportSize,
    const FVector2D& ViewportOffset,
    float ViewportAspectRatio, FViewport* Viewport)
{
    TStatId GlobalBVHPickingStatId;
    FScopeCycleCounter GlobalBVHPickingTimer(GlobalBVHPickingStatId);

    if (!Camera) return nullptr;

    const FMatrix View = Camera->GetViewMatrix();
    const FMatrix Proj = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
    const FVector CameraWorldPos = Camera->GetActorLocation();
    const FVector CameraRight = Camera->GetRight();
    const FVector CameraUp = Camera->GetUp();
    const FVector CameraForward = Camera->GetForward();

    FRay ray = MakeRayFromViewport(View, Proj, CameraWorldPos, CameraRight, CameraUp, CameraForward,
        ViewportMousePos, ViewportSize, ViewportOffset);

    float closestDistance = FLT_MAX;

    // 적응형 임계값 계산
    float cameraDistanceEstimate = (CameraWorldPos - FVector(0, 0, 0)).Size();
    float adaptiveThreshold = GetAdaptiveThreshold(cameraDistanceEstimate);

    // Global BVH 우선 사용
    FBVH* BVH = GWorld ? GWorld->GetBVH() : nullptr;
    if (BVH)
    {
        //float hitDistance;
        //AActor* HitActor = BVH->Intersect(ray.Origin, ray.Direction, hitDistance);

        //if (HitActor && !HitActor->GetActorHiddenInGame())
        //{
        //    uint64_t GlobalBVHCycleDiff = GlobalBVHPickingTimer.Finish();
        //    double GlobalBVHPickingTimeMs = FPlatformTime::ToMilliseconds(GlobalBVHCycleDiff);
        //    URenderingStatsCollector::GetInstance().UpdatePickingStats(GlobalBVHPickingTimeMs);
        //    char buf[256];
        //    sprintf_s(buf, "[Global BVH Pick] Hit actor at distance %.3f (Time: %.3fms)\n", hitDistance, GlobalBVHPickingTimeMs);
        //    UE_LOG(buf);
        //    return HitActor;
        //}
    }

    USceneComponent* SelectedComponent = nullptr;
    // BVH 실패 시 브루트 포스 백업
    for (int i = 0; i < Actors.Num(); ++i)
    {
        AActor* Actor = Actors[i];
        if (!Actor || Actor->GetActorHiddenInGame()) continue;

        float hitDistance;
        USceneComponent* OutHitComponent = nullptr;
        if (CheckActorPicking(Actor, OutHitComponent, ray, hitDistance) && hitDistance < closestDistance)
        {
            closestDistance = hitDistance;
            SelectedComponent = OutHitComponent;
            if (closestDistance < adaptiveThreshold)
            {
                break;
            }
        }
    }


    uint64_t GlobalBVHCycleDiff = GlobalBVHPickingTimer.Finish();
    double GlobalBVHPickingTimeMs = FPlatformTime::ToMilliseconds(GlobalBVHCycleDiff);

    if (SelectedComponent)
    {
        char buf[256];
        sprintf_s(buf, "[Global BVH Pick Fallback] Hit actor at distance %.3f (Time: %.3fms)\n", closestDistance, GlobalBVHPickingTimeMs);
        UE_LOG(buf);
        return SelectedComponent;
    }
    else
    {
        char buf[256];
        sprintf_s(buf, "[Global BVH Pick] No hit (Time: %.3fms)\n", GlobalBVHPickingTimeMs);
        UE_LOG(buf);
        return nullptr;
    }
}

bool CPickingSystem::CheckBillboardComponentPicking(const UBillboardComponent* Component, const FRay& Ray, float& OutDistance)
{
    if (!Component) return false;

    // 빌보드의 월드 위치 (쿼드의 중심점)
    FVector BillboardWorldPos = Component->GetWorldLocation();

    // 빌보드 크기
    float HalfWidth = Component->GetBillboardWidth() * 0.5f;
    float HalfHeight = Component->GetBillboardHeight() * 0.5f;

    // 카메라 정보 가져오기 (빌보드는 항상 카메라를 향함)
    UWorld* World = Component->GetOwner()->GetWorld();
    if (!World) return false;

    ACameraActor* CameraActor = World->GetCameraActor();
    if (!CameraActor) return false;

    FVector CamRight = CameraActor->GetActorRight();
    FVector CamUp = CameraActor->GetActorUp();
    FVector CamForward = CameraActor->GetActorForward();

    // 빌보드 평면의 법선 벡터 (카메라를 향하므로 -CamForward)
    FVector PlaneNormal = -CamForward;
    PlaneNormal.Normalize();

    // Ray와 평면의 교차 검사
    // 평면 방정식: dot(PlaneNormal, P - BillboardWorldPos) = 0
    // Ray 방정식: P = Ray.Origin + t * Ray.Direction
    // 교차점: dot(PlaneNormal, Ray.Origin + t * Ray.Direction - BillboardWorldPos) = 0
    // t = dot(PlaneNormal, BillboardWorldPos - Ray.Origin) / dot(PlaneNormal, Ray.Direction)

    float Denominator = FVector::Dot(PlaneNormal, Ray.Direction);

    // Ray가 평면과 평행하거나 뒤를 향하면 교차하지 않음
    if (std::abs(Denominator) < KINDA_SMALL_NUMBER)
        return false;

    float t = FVector::Dot(PlaneNormal, BillboardWorldPos - Ray.Origin) / Denominator;

    // Ray가 음의 방향이면 교차하지 않음
    if (t < 0.0f)
        return false;

    // 교차점 계산
    FVector HitPoint = Ray.Origin + Ray.Direction * t;

    // 교차점이 빌보드 쿼드 내부에 있는지 확인
    // 빌보드 로컬 좌표계로 변환하여 검사
    FVector LocalHitPoint = HitPoint - BillboardWorldPos;

    // 카메라 오른쪽/위쪽 벡터로 투영
    float LocalX = FVector::Dot(LocalHitPoint, CamRight);
    float LocalY = FVector::Dot(LocalHitPoint, CamUp);

    // 쿼드 경계 내부에 있는지 확인
    if (LocalX >= -HalfWidth && LocalX <= HalfWidth &&
        LocalY >= -HalfHeight && LocalY <= HalfHeight)
    {
        OutDistance = t;
        return true;
    }

    return false;
}

