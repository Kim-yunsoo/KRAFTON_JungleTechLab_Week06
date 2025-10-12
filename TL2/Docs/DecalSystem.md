# 투영 데칼(Projection Decal) 구현 보고서

## 개요
- 목적: 씬의 메시 표면에 데칼 텍스처를 원근/직교 프로젝션으로 투영하는 기능 구현
- 핵심: X축을 전방축(Forward)으로 사용하는 프로젝터 공간을 정의하고, 전용 VS/PS와 전용 렌더 패스를 통해 데칼을 안정적으로 합성
- 적용 범위: 에디터/PIE 모두, ShowFlag와 Stats 연동

## 설계 요점
- Projection 방향은 로컬 +X(Forward) 기준
  - 프로젝터 원점은 볼륨 중심-깊이 위치에 맞추기 위해 +X 반대 방향으로 `0.5 * Scale.X`만큼 이동해 Near와 Far를 대칭 정렬
  - Far 평면의 크기를 OBB의 Y/Z 스케일과 일치시켜 시각적 어색함 제거
- 전용 셰이더(HLSL) 추가/사용
  - 파일: `TL2/ProjectionDecal.hlsl`
  - VS: 메시 정점을 월드 공간으로 변환하여 PS에 전달
  - PS: 월드 좌표를 데칼 View/Proj로 변환해 NDC에서 UV 생성, 프러스텀 밖 픽셀 `discard` 처리, 알파로 합성
- 전용 렌더 패스
  - `UDecalComponent::RenderDecalProjection`에서 파이프라인 상태(블렌딩, 깊이), 상수버퍼 바인딩, 대상 메시 루프 등 일괄 처리
- UDecalComponent / ADecalActor
  - 컴포넌트가 투영 행렬/셰이더/대상 메시 수집을 책임지고, 액터는 컴포넌트를 루트로 보유
- Show Flag, Stats
  - 뷰포트 ShowFlag로 데칼 토글, 프레임 단위 Stats 수집/평균화

## 구성 요소

### 1) UDecalComponent
- 파일: `TL2/DecalComponent.h/.cpp`
- 역할
  - 투영용 View/Projection 행렬 생성(직교/원근)
  - 데칼 대상 메시 수집: AABB 선별 → OBB(SAT) 정밀 교차검사
  - 데칼 렌더 패스 구동 및 머티리얼/텍스처 바인딩
  - 페이드 애니메이션(FadeIn → Delay → FadeOut)과 알파 업데이트
- 주요 구현 포인트
  - Projection 전방축: 로컬 +X
  - Near/Far 정렬: `Translation += forward * (-0.5 * Scale.X)`
  - Perp FOV 산출: Far 평면 높이 = `Scale.Z`가 되도록 `Fov = 2*atan(Scale.Z/(2*Far))`
  - Far 평면 폭/비: Aspect = `Scale.Y / Scale.Z`
  - ColorBuffer의 `a`에 `CurrentAlpha` 반영하여 셰이더에서 페이드
  - 애니 초기값 최소 수정: 생성자 `CurrentAlpha = 0.0f`로 시작하도록 변경해 자연스러운 Fade-in

### 2) ADecalActor
- 파일: `TL2/DecalActor.h/.cpp`
- 역할: 데칼 전용 액터. 루트에 `UDecalComponent`를 소유하고 데칼 관련 속성/수명 관리

## 셰이더(Decal 전용 VS/PS)
- 파일: `TL2/ProjectionDecal.hlsl`
- 상수버퍼
  - `ModelBuffer`: 대상 메시 월드 행렬
  - `ViewProjBuffer`: 카메라 View/Projection
  - `DecalBuffer`: 데칼 View/Projection(전용)
  - `ColorBuffer`: `LerpColor.a`로 페이드 알파 전달
- VS
  - 입력 정점을 월드 좌표로 변환, 표준 카메라 클립 위치 산출, PS에 월드 좌표 전달
- PS
  - `worldPos → DecalView → DecalProj`로 투영
  - `abs(ndc.xy) > 1` 또는 `ndc.z` 범위 밖은 `discard`
  - `uv = ndc.xy*0.5+0.5`로 데칼 텍스처 샘플, `color *= LerpColor`로 페이드 반영

## 데칼 렌더 패스
- 진입점: `UDecalComponent::RenderDecalProjection(URenderer*, View, Proj)`
  - 파이프라인
    - 셰이더 로드/바인딩: `ProjectionDecal.hlsl`
    - 블렌딩 on, 깊이 읽기 전용(LessEqualReadOnly)
  - 행렬 구성
    - 직교: OrthoLH(폭 = `Scale.Y`, 높이 = `Scale.Z`, Near = `-0.5*Scale.X`, Far = `+0.5*Scale.X`)
    - 원근: 로컬 +X 반대 방향으로 `0.5*Scale.X` 이동, Far=Scale.X, FOV/Aspect는 OBB와 일치
  - 메시 선택
    - `FindAffectedMeshes`: BVH로 Broad Phase, AABB 교차 후 OBB(SAT)로 정밀 교차
  - 드로우
    - 대상 메시 월드 행렬/데칼 View/Proj/페이드 알파 상수버퍼 업데이트 후 인덱스 드로우

## Show Flag(데칼 토글)
- 플래그 정의: `EEngineShowFlags::SF_Decals` (`TL2/Enums.h`)
- UI 토글: `TL2/SViewportWindow.cpp` 내 디버그 체크박스
- 렌더 경로 게이팅: `TL2/World.cpp`에서 `SF_Primitives && SF_Decals && SF_StaticMeshes` 조건 만족 시 데칼 패스 포함

## Stats(데칼 렌더링 통계)
- 수집/평균 구조: `TL2/RenderingStats.cpp/.h`
- 항목 예시
  - `TotalDecalCount`, `ActiveDecalCount`, `AffectedMeshesCount`, `DecalDrawCalls`, `DecalPassTimeMs`, 상태 변경 수 등
- 업데이트 지점: `UDecalComponent::RenderDecalProjection` 시작 시 활성/영향 수 누적

## 에디터/UX 보조
- OBB 시각화: 데칼 선택 시 OBB 라인 렌더 (`RenderOBB`)
- Billboard 표시: 에디터 상에서 데칼 아이콘(빌보드) 렌더
- 텍스처 선택: `.dds/.png/.jpg` 지원
- Detail 패널에서 페이드 파라미터/정렬/텍스처 조정 가능

## 사용 가이드
1) 액터 배치
   - `ADecalActor` 스폰 → `UDecalComponent`의 위치/회전/스케일로 볼륨 설정
2) 텍스처 지정
   - Detail 패널 또는 코드에서 `SetDecalTexture(path)`
3) 투영 모드
   - 기본: 직교(bIsOrthoMatrix=true). 필요 시 원근으로 전환하고 X축 전방 기준으로 세팅
4) 페이드
   - `StartFade()` 호출로 FadeIn→Delay→FadeOut 진행
   - `FadeInDuration`, `FadeStartDelay`, `FadeDuration`, `MaxAlpha` 조절
5) 표시 토글
   - 뷰포트 ShowFlag "Decals" 체크로 표시/숨김

## 성능/안정성 메모
- Broad Phase에 BVH 사용으로 후보 메시 최소화, SAT로 정확도 확보
- 프러스텀 클립 기반 discard로 픽셀 셰이딩 절감
- 알파 블렌드/깊이 읽기 전용 상태로 GBuffer 충돌 최소화(현 파이프라인 기준)

## 변경 요약(핵심 반영)
- 투영 기준 축 및 기원 정렬: X축 Forward, `-0.5*Scale.X` 오프셋
- Far 평면 크기 = OBB 스케일(Y/Z) 일치
- 전용 Projection 셰이더 사용(파일: `ProjectionDecal.hlsl`)
- 렌더 패스: `UDecalComponent::RenderDecalProjection`
- 컴포넌트/액터: `UDecalComponent`, `ADecalActor`
- Show Flag: `SF_Decals` 토글 및 World 경로 연동
- Stats: 프레임별 수집/평균화

## 향후 과제(옵션)
- 프로젝터 뎁스 텍스처로 자기 차폐 지원(뒤로 비침 방지)
- 경계/입사각 페이드 마스크 추가로 가장자리 자연스러움 향상
- RNM 기반 노말 결합 및 ORM 채널 기반 머티리얼 데칼 확장

