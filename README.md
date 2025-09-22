# On Device

## 단어 정리

### GGUF

GGUF(Georgi Gerganov Unified Format)는 딥러닝 모델을 효율적으로 저장하고 배포하기 위한 새로운 파일 형식.

장점.

1. 뛰어난 호환성, 특정 프레임워크나 라이브러리에 종속되지 않음.
2. 효율적인 저장과 로딩, 모델 가중치를 효과적으로 저장하고 로딩 가능. -> 모델 크기를 줄이고 로딩 속도 향상.
3. 다양항 플랫폼 가능, CPU, GPU, TPU 등 다양한 환경에서 모델 실행 가능.
4. 오픈소스 프로젝트, 현재도 활발히 진행 중.

그 중 내가 사용하려는 이유는 llama.cpp(https://github.com/ggml-org/llama.cpp)이 읽는 표준 포맷.

안드로이드(핸드폰)에서 llama.cpp를 사용할 수 있기 때문에 GGUF로 변환 후 사용 예정.

### 양자화

양자화(Quantization)는 모델 크기를 줄이고 속도를 향상시키기 위해 가중치를 저정밀도로 변환하는 과정. <br>

이를 통해 메모리 사용량을 절감하고, 저사양 환경에서도 모델을 실행 가능. 단, 정밀도가 낮아질수록 품질 손실이 발생 가능성 존재.<br>

1. 개인적으로는 약간 손실 압축 방식 같은 느낌.

Q4_K_M / Q5_K_M 이런 것들은 양자화 단계 구분.

- Q#: “가중치를 #비트로 양자화”. 숫자가 작을수록 작고 빠르지만 손실 증가.
- K: llama.cpp의 K-quant(그룹/블록 단위 스케일) 방식. 블록마다 스케일·민값을 따로 둬서, 같은 비트수라도 품질을 낭비 없이 끌어올리는 계열.
- S/M/L: K-quant 내부 블록/스케일링 구성 차이. 보통 M이 사이즈와 품질 사이의 밸런스가 좋아 실사용 기본값으로 많이 추천됨. Q5_K_M은 Q4_K_M보다 약간 큰 대신 품질이 더 안전한 선택.(https://jobilman.tistory.com/entry/quantization-guide-q5k-q5ks-q5km-differences)

### KV-cache 8-bit

추론 중 토큰마다 저장하는 Key/Value 텐서(KV)를 FP16 → 8-bit(혹은 4-bit)로 낮춰 메모리 사용을 크게 줄이는 기술.

메모리 절감에 있어서 큰 효과를 볼 수 있음. if. ~36KB/토큰(FP16)에 적용한다면 ~18 KB/토큰정도로 줄일 수 있음. 긴 텍스트에 대해 굉장히 효과적임.(https://huggingface.co/blog/kv-cache-quantization?utm_source=chatgpt.com)


## 일단 해보기
### 안드로이드 폰에 모델 올려보기.
안드로이드 스튜디오와 핸드폰 연결.
https://developer.samsung.com/android-usb-driver

드라이버 설치 및 핸드폰 설정 개발자 모드 활성화. → usb 디버깅 허용.
모델 로드까지 정상적으로 작동.

#### 문제 발생: 모델이 커서 그런지 생성에서 굉장히 오래 걸림. 9시 26분 34초 시작 → 9사 36분 53초 생성

#### 해결 방안:
  1. assets 폴더에 새로운 모델을 넣고, MainActivity.kt의 modelFileNameInAssets 변수 수정.

```kotlin
// In MainActivity.kt
        private val modelFileNameInAssets = "새로운_모델_파일이름.gguf"
```

  2. 생성 토큰 수 조절. max_new_tokens 값 줄여서 테스트.

```kotlin
// In native-lib.cpp, inside generateTextJNI
        const int max_new_tokens = 5; // 아주 작게 줄여서 첫 토큰 생성 여부 확인
```
