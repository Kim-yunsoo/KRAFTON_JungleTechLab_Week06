# Projection Decal System – Implementation Summary

본 문서는 본 프로젝트에 구현된 “투영 데칼(Projection Decal)” 기능의 핵심 구성과 사용법을 정리합니다. 아래 항목 중심으로 기술했습니다.

- Projection 방향은 X 축 Forward Vector로 한다
- Decal 전용 Pixel Shader와 Vertex Shader를 추가 구현한다
- Decal을 렌더링하는 Pass를 추가하고 구현한다
- UDecalComponent를 구현한다
- DecalComponent를 맴버 변수로 갖는 ADecalActor를 구현한다
- Decal Show Flag를 구현한다
- Decal Stat을 구현한다

## Projection 방향 (X-axis Forward)
- 프로젝터의 전방축을 로컬 +X로 사용합니다.
- 원근 투영 시 데칼 볼륨의 중심-깊이에서 프러스텀이 시작되도록, 프로젝터 원점을 로컬 +X의 반대 방향으로 `0.5 * Scale.X` 만큼 이동시킵니다.
- Far 평면의 가로/세로 크기는 OBB 스케일(Y/Z)과 일치시켜 시각적 왜곡을 방지합니다.

참고 코드
- `TL2/DecalComponent.cpp` 내 `RenderDecalProjection()` (View/Proj 구성)

## Decal 전용 VS/PS
- 전용 셰이더 파일: `TL2/ProjectionDecal.hlsl`
- Vertex Shader
  - 메시 정점을 월드 공간으로 변환하여 PS에 전달합니다.
  - 표준 카메라용 SV_Position도 산출합니다.
- Pixel Shader
  - 입력된 월드 좌표를 데칼 View/Projection으로 투영해 NDC를 얻습니다.
  - NDC에서 프러스텀 바깥 픽셀은 discard 합니다.
  - `uv = ndc.xy * 0.5 + 0.5`로 데칼 텍스처를 샘플링하고, ColorBuffer의 알파로 페이드 가중을 곱합니다.

참고 코드
- `TL2/ProjectionDecal.hlsl`

## Decal 렌더 패스
- 진입점: `UDecalComponent::RenderDecalProjection(URenderer*, View, Proj)`
- 처리 순서
  - 파이프라인 상태 설정: 블렌드 on, 깊이 ReadOnly
  - 데칼용 뷰/프로젝션 행렬 및 페이드 알파 상수 버퍼 업데이트
  - 데칼 영향 대상 메시들에 대해 인덱스 드로우 수행
- 행렬 구성
  - 직교: 폭=`Scale.Y`, 높이=`Scale.Z`, Near=`-0.5*Scale.X`, Far=`+0.5*Scale.X`
  - 원근: 로컬 +X 반대방향으로 `0.5*Scale.X` 이동, Far=`Scale.X`, Aspect=`Scale.Y/Scale.Z`, FOV는 Far 평면 높이가 `Scale.Z`가 되도록 계산

참고 코드
- `TL2/DecalComponent.cpp` – `RenderDecalProjection()`

## UDecalComponent
- 파일: `TL2/DecalComponent.h/.cpp`
- 역할
  - 투영용 View/Projection 행렬 생성(직교/원근)
  - 데칼 대상 메시 수집: BVH Broad Phase → AABB 교차 → OBB(SAT) 정밀 검사
  - 페이드 애니메이션(FadeIn → Delay → FadeOut)과 알파 업데이트
  - 전용 셰이더 바인딩 및 렌더 패스 실행
- 기본 페이드 동작
  - `StartFade()` 호출 시 `CurrentAlpha=0`에서 Fade-in이 시작되도록 초기값을 0으로 설정

핵심 메서드
- `FindAffectedMeshes(UWorld*)`
- `RenderDecalProjection(URenderer*, View, Proj)`
- `StartFade()`, `DecalAnimTick()`, `ActivateFadeEffect()`

## ADecalActor (데칼 전용 액터)
- 파일: `TL2/DecalActor.h/.cpp`
- `UDecalComponent`를 멤버(루트)로 보유하는 전용 액터입니다.
- 씬 배치/복제/삭제 라이프사이클을 액터 인터페이스로 제공합니다.

## Decal Show Flag
- 플래그: `EEngineShowFlags::SF_Decals`
- 토글 UI: `TL2/SViewportWindow.cpp` (ImGui 체크박스)
- 렌더 경로: `TL2/World.cpp`에서 `SF_Primitives && SF_Decals && SF_StaticMeshes` 조건일 때 데칼 패스 포함

## Decal Stats
- 수집/평균: `TL2/RenderingStats.cpp/.h`
- 예시 항목: `TotalDecalCount`, `ActiveDecalCount`, `AffectedMeshesCount`, `DecalDrawCalls`, `DecalPassTimeMs`, 상태 변경 수 등
- 업데이트 지점: `UDecalComponent::RenderDecalProjection()`에서 활성 데칼/영향 메시 누적

## 사용 가이드
1) 액터 배치
   - `ADecalActor`를 스폰하고 `UDecalComponent`의 위치/회전/스케일로 볼륨 정의
2) 텍스처 지정
   - `UDecalComponent::SetDecalTexture(path)` (dds/png/jpg 지원)
3) 투영 모드
   - `bIsOrthoMatrix`로 직교/원근 전환, 전방축은 X 고정
4) 페이드
   - `StartFade()` 호출로 Fade-in→Delay→Fade-out 자동 진행
   - `FadeIn/Delay/Duration/MaxAlpha` 조절로 타이밍/강도 제어
5) 표시 토글
   - 뷰포트 ShowFlag “Decals”로 온/오프

## 참고 파일 경로
- 컴포넌트/액터: `TL2/DecalComponent.*`, `TL2/DecalActor.*`
- 셰이더: `TL2/ProjectionDecal.hlsl`
- 플래그/UI/월드 경로: `TL2/Enums.h`, `TL2/SViewportWindow.cpp`, `TL2/World.cpp`
- 통계: `TL2/RenderingStats.*`

