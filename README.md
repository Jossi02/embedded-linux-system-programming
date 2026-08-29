# Embedded Linux System Programming

ES-101 ARM 보드에서 GPIO interrupt, kernel timer, character device, PIR sensor를 실습한 2인 팀 Embedded System Term Project입니다.

![C](https://img.shields.io/badge/Language-C-blue?style=flat-square) ![Linux](https://img.shields.io/badge/Platform-Linux%20Kernel-black?style=flat-square&logo=linux) ![License](https://img.shields.io/badge/License-GPLv2-green?style=flat-square)

## Project Context

세 과제의 kernel module, device driver, userspace program 구현과 수업 당시 ES-101 보드 테스트를 두 팀원이 공동 수행했습니다.

수업 종료 후 팀원이 팀 결과물을 [`Hyxnmin/embedded-linux-system-programming`](https://github.com/Hyxnmin/embedded-linux-system-programming)에 최종 업로드했으며, 현재 저장소는 해당 팀 저장소의 fork입니다. 이후 저장소 소유자가 포트폴리오 공개를 위한 코드 감사, 문서 정리, 명백한 결함 수정을 수행했습니다. 개인별 세부 역할을 구분할 근거가 남아 있지 않으므로 Git commit author만으로 수업 당시 구현 기여를 판단할 수 없습니다.

## Project Overview

| Directory | Project | Key Concepts |
|:--- |:--- |:--- |
| `01_gpio_interrupt_timer` | GPIO Interrupt & Timer | GPIO Interrupt, Kernel Timer, Software Debouncing |
| `02_char_device_driver` | Character Device Driver | `file_operations`, User/Kernel Communication |
| `03_security_system_pir` | PIR Alarm | Sensor Interrupt, Timer-based Alarm Control |

## Key Implementation

### 01. GPIO Interrupt & Timer

- SW[0]: 전체 LED blink, SW[1]: LED shift, SW[2]: manual mode, SW[3]: reset
- GPIO interrupt, kernel timer, `jiffies` 기반 200 ms software debounce 사용
- manual mode에서는 SW[0]~SW[2]로 대응 LED를 toggle하고 SW[3]으로 reset

### 02. Character Device Driver

- character device와 `file_operations`의 `open`, `release`, `write` 구현
- `copy_from_user()`로 `/dev/assign2`의 1-byte command를 kernel buffer에 복사
- main mode는 `1`~`4`, manual mode는 `0`~`3` LED toggle 및 `4` reset
- 수업 환경의 fixed major 221 유지, `atomic_cmpxchg()`로 single-open 보장

### 03. PIR Alarm

- GPIO 7의 PIR interrupt로 alarm 시작, switch interrupt로 alarm 종료
- alarm 중 kernel timer로 전체 LED를 약 2초 간격으로 blink
- 평상시 LED OFF

## Technical Challenges

### Software Debouncing

기계식 switch의 chattering으로 발생하는 반복 interrupt를 `jiffies` 기준 200 ms 구간에서 무시하도록 구성했습니다.

```c
if (jiffies - last_irq_time < msecs_to_jiffies(200))
    return IRQ_HANDLED;
last_irq_time = jiffies;
```

### Asynchronous Timing with Kernel Timers

Interrupt context에서는 blocking/sleeping 방식이 부적절하므로 `struct timer_list`로 시간 기반 LED 동작을 비동기 처리했습니다. LED 동작 중에도 interrupt event를 처리할 수 있습니다.

### Safe User-Kernel Communication

Userspace pointer를 kernel에서 직접 역참조하지 않고 `copy_from_user()`로 kernel buffer에 안전하게 복사를 시도하며 실패 여부를 확인합니다. 이 함수가 command 자체의 semantic validation을 대신하지는 않습니다.

## Build & Usage

필요한 도구는 Linux kernel headers, GCC, Make입니다. 각 프로젝트 디렉터리에서 `make`를 실행하며, 기본 `KDIR`은 `/lib/modules/$(uname -r)/build`입니다.

| Directory | Build output |
|:--- |:--- |
| `01_gpio_interrupt_timer` | `assign1.ko` |
| `02_char_device_driver` | `assign2.ko`, `assign2-2` |
| `03_security_system_pir` | `assign3.ko` |

Character device 실행 예시는 다음과 같습니다.

```bash
cd 02_char_device_driver
make
sudo insmod assign2.ko
sudo mknod /dev/assign2 c 221 0
sudo ./assign2-2
```

`mknod`와 module load는 ES-101 수업 환경의 실행 절차입니다.

## Validation & Limitations

수업 당시 ES-101 Linux 환경을 기준으로 작성한 historical course project입니다. `gpio_request()`, `gpio_direction_*()`, `gpio_to_irq()` 등 legacy integer GPIO API를 유지하며, 최신 general-purpose Linux kernel이나 다른 board에서의 호환성을 보장하지 않습니다.

### Historical Validation

공동 보고서에는 수업 당시 ES-101 보드에서 세 과제를 build, module load, 실행한 결과가 기록되어 있습니다. 여기에는 #2 userspace/device 통신과 #3 PIR 감지 및 switch alarm 해제 동작이 포함됩니다.

### 2026 Portfolio Hardening Validation

Build/source 정합성, #2 command·write·open 계약, GPIO/IRQ/timer cleanup, provenance 문서를 점검하고 명백한 결함을 수정했습니다.

- userspace program warning-enabled build: **PASS**
- static code, resource cleanup flow 검토: **PASS**
- source/Makefile 정합성 확인: **PASS**
- `git diff --check`: **PASS**

### Not Retested

- kernel module actual Kbuild: **NOT RUN**
- 2026 hardening 이후 ES-101 hardware runtime retest: **NOT RUN**
- concurrent second-open runtime test: **NOT RUN**

## What This Project Demonstrates

- kernel module lifecycle과 kernel logging
- GPIO/IRQ resource allocation, partial-init cleanup, final teardown
- character device를 통한 user/kernel communication

## Contribution & Maintenance

- Original course implementation and board testing: 2인 팀 공동 수행
- Original team-result GitHub upload: 팀원 수행
- Current fork portfolio hardening: 저장소 소유자 수행
- Portfolio hardening 과정에서 생성형 AI coding tool을 활용했으며, 변경 범위와 결과를 검토한 뒤 반영

## License

기존 upstream의 [GPL-2.0 LICENSE](LICENSE)를 유지합니다.
