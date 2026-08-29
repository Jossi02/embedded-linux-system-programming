#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static int send_command(int dev, char command) {
    ssize_t written = write(dev, &command, 1);

    if (written < 0) {
        perror("write");
        return -1;
    }
    if (written != 1) {
        fprintf(stderr, "short write: %zd bytes\n", written);
        return -1;
    }
    return 0;
}

int main(void) {
    int dev;
    int status = EXIT_SUCCESS;
    char r1, r2;

    // 디바이스 드라이버 파일 열기
    dev = open("/dev/assign2", O_RDWR);
    if (dev < 0) {
        perror("open /dev/assign2");
        return EXIT_FAILURE;
    }

    // 메뉴 출력
    printf("Mode 1 : 1\nMode 2 : 2\nMode 3 : 3\nMode 4 : 4\n");

    // 무한 루프를 돌며 사용자 입력 대기
    while (1) {
        printf("Type a Mode: ");
        fflush(stdout);
        if (scanf(" %c", &r1) != 1)
            break;
        if (r1 < '1' || r1 > '4') {
            fprintf(stderr, "Mode must be 1-4.\n");
            continue;
        }

        // 입력받은 모드 값을 드라이버에 전달
        if (send_command(dev, r1) < 0) {
            status = EXIT_FAILURE;
            break;
        }

        // 수동 모드일 경우
        if (r1 == '3') {
            while (1) {
                printf("LED to enable: ");
                fflush(stdout);
                if (scanf(" %c", &r2) != 1)
                    goto out;
                if (r2 < '0' || r2 > '4') {
                    fprintf(stderr, "LED must be 0-4.\n");
                    continue;
                }
                // LED 제어 명령 전달
                if (send_command(dev, r2) < 0) {
                    status = EXIT_FAILURE;
                    goto out;
                }
                if (r2 == '4') break;
            }
        }
        sleep(1);
    }
out:
    if (close(dev) < 0) {
        perror("close");
        status = EXIT_FAILURE;
    }

    return status;
}
