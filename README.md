# 🐧 Embedded Linux System Programming

> ES-101 보드에서 GPIO interrupt, kernel timer, character device를 실습한 Embedded System 수업 Term Project입니다.

![C](https://img.shields.io/badge/Language-C-blue?style=flat-square) ![Linux](https://img.shields.io/badge/Platform-Linux%20Kernel-black?style=flat-square&logo=linux) ![License](https://img.shields.io/badge/License-GPLv2-green?style=flat-square)

## 프로젝트 배경 및 기여

이 저장소는 ES-101 ARM 기반 실습 장비를 사용한 **2인 팀 프로젝트** 결과물입니다. 세 과제의 커널 모듈, 디바이스 드라이버, 사용자 프로그램 구현과 ES-101 보드 테스트를 두 팀원이 공동 수행했습니다. 개인별 세부 역할을 나눌 근거는 남아 있지 않습니다.

수업 종료 후 팀원이 팀 결과물을 [`Hyxnmin/embedded-linux-system-programming`](https://github.com/Hyxnmin/embedded-linux-system-programming)에 최종 업로드했으며, 현재 저장소는 해당 팀 저장소의 fork입니다. 이후 저장소 소유자가 포트폴리오 공개를 위해 코드 감사, 문서 정리, 명백한 결함 수정을 수행했습니다. Git commit author만으로 수업 당시의 코드 기여를 판단할 수 없습니다.

## 📂 Project Overview

| Directory | Project | Key Concepts |
|:--- |:--- |:--- |
| `01_gpio_interrupt_timer` | **GPIO Control Module** | Interrupt Handling, Kernel Timer, Software Debouncing |
| `02_char_device_driver` | **Character Device Driver** | VFS(Virtual File System), System Call, User-Kernel Data Transfer |
| `03_security_system_pir` | **PIR Security System** | Event-Driven Architecture, Sensor Integration, State Machine |

### 01 GPIO Interrupt & Timer

- SW[0]: 전체 LED blink, SW[1]: LED shift, SW[2]: manual mode, SW[3]: reset
- GPIO switch interrupt, kernel timer, 200 ms software debounce 사용
- manual mode에서는 SW[0]~SW[2]로 대응 LED를 toggle하고 SW[3]으로 reset

### 02 Character Device Driver

- `file_operations`의 `open`, `release`, `write`와 `copy_from_user()` 사용
- userspace 프로그램이 `/dev/assign2`에 1-byte 명령을 전달
- main mode: `1`~`4`, manual mode: `0`~`3` LED toggle 및 `4` reset
- 수업 환경의 고정 major number 221 유지

### 03 PIR Alarm

- GPIO 7 PIR interrupt로 alarm 시작, switch interrupt로 alarm 종료
- alarm 중 kernel timer로 전체 LED를 약 2초 간격으로 blink
- 평상시 LED OFF

## Historical Environment and Scope

- 수업 당시 ES-101 및 Linux 4.19 계열 환경을 기준으로 작성한 historical course project입니다.
- `gpio_request()`, `gpio_direction_*()`, `gpio_to_irq()` 등 legacy integer GPIO API를 유지합니다.
- 최신 general-purpose Linux kernel이나 다른 board에서의 호환성을 보장하지 않습니다.
- 2026 portfolio hardening 이후 실제 ES-101 hardware에서는 재검증하지 않았습니다.

---

## 🚀 Key Technical Challenges & Solutions

프로젝트 진행 중 발생한 주요 이슈와 이를 해결하기 위해 적용한 엔지니어링 접근 방식입니다.

### 1. Software Debouncing (in `01_gpio_interrupt_timer`)
* **Issue:** 기계식 스위치 조작 시 물리적 진동(Chattering)으로 인해 한 번의 입력에 수십 번의 인터럽트가 발생하는 현상 확인.
* **Solution:** 리눅스 커널의 시간 단위인 `jiffies`를 활용하여 디바운싱 로직을 구현. 마지막 인터럽트 발생 시점과 현재 시점의 차이가 **200ms 미만일 경우 노이즈로 간주하고 무시**하여 입력 신뢰성을 확보했습니다.
    ```c
    // Code Snippet: Debouncing Logic
    if (jiffies - last_irq_time < msecs_to_jiffies(200)) {
        return IRQ_HANDLED; // Ignore noise
    }
    last_irq_time = jiffies;
    ```

### 2. Asynchronous Timing with Kernel Timers
* **Approach:** interrupt context에서는 blocking/sleeping 방식이 부적절합니다.
* **Implementation:** `struct timer_list`로 시간 기반 LED 동작을 비동기 처리하여 LED 동작 중에도 interrupt event에 대응하도록 구성했습니다.

### 3. Safe User-Kernel Communication (in `02_char_device_driver`)
* **Principle:** userspace pointer를 kernel에서 직접 역참조하지 않습니다.
* **Implementation:** `copy_from_user()`로 kernel buffer에 복사를 시도하고 복사 실패 여부를 확인합니다. 이 함수가 명령 내용 자체의 유효성을 대신 검증하는 것은 아닙니다.

---

## 🛠️ Build & Usage

각 디렉터리의 Makefile은 `$(MAKE) -C $(KDIR) M=$(PWD) modules` 방식으로 Kbuild를 호출합니다.

### Prerequisites
* Linux Kernel Headers (`sudo apt install linux-headers-$(uname -r)`)
* GCC Compiler, Make

### How to Build
각 프로젝트 폴더에서 `make`를 실행합니다. 기본 `KDIR`은 `/lib/modules/$(uname -r)/build`입니다.

```bash
# Example: Build GPIO Module
cd 01_gpio_interrupt_timer
make

# Load Module
sudo insmod assign1.ko

# Check Kernel Log
dmesg | tail
```

생성되는 커널 모듈은 각각 `assign1.ko`, `assign2.ko`, `assign3.ko`입니다. #2는 userspace 실행 파일 `assign2-2`도 함께 빌드합니다.

```bash
cd 02_char_device_driver
make
sudo insmod assign2.ko
sudo mknod /dev/assign2 c 221 0
sudo ./assign2-2
```

`mknod`와 module load는 ES-101 수업 환경의 실행 절차이며, CI나 일반 개발 PC에서 실행하도록 의도하지 않았습니다.

## Validation Status

- **Historical hardware validation:** 수업 당시 ES-101 보드에서 세 과제를 build, `insmod`, 실행하여 동작을 확인했습니다. #2의 `/dev/assign2` 생성과 userspace 통신, #3의 PIR 감지 및 switch alarm 해제 로그도 공동 보고서에 기록되었습니다.
- **2026 portfolio hardening:** userspace 프로그램은 일반 compiler warning을 켜고 재빌드했으며, kernel module은 사용 가능한 header 환경에서만 검증 결과를 기록합니다.
- **Hardware retest:** 2026 hardening 이후 ES-101에서는 재검증하지 않았습니다.

## 📝 Learning Outcomes

1.  **Kernel Mechanics:** 커널 모듈의 생명주기(`init`, `exit`)와 커널 심볼 테이블에 대한 이해.
2.  **Resource Management:** `request_irq`, `gpio_request` 등을 통한 하드웨어 리소스 할당 및 해제와 메모리 누수 방지.
3.  **Low-Level Debugging:** `dmesg`와 커널 로그(`printk`)를 활용한 트러블 슈팅 능력.

## 👥 Contributors and Maintenance

세 과제의 구현과 수업 당시 ES-101 보드 검증은 2인 팀이 공동 수행했습니다. 현재 fork의 포트폴리오 hardening과 문서 정리는 저장소 소유자가 수행했으며, 이 과정에는 생성형 AI 코딩 도구를 활용하고 변경 범위와 결과를 검토하여 반영했습니다.

기존 upstream의 GPL-2.0 `LICENSE`를 유지합니다.
