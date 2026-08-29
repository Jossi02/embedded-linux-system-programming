#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define HIGH 1
#define LOW 0

// 주번호 221
#define DEV_MAJOR_NUMBER 221
#define DEV_NAME "assign2"

#define STOP       0
#define BLINK_ALL  1
#define SHIFT      2
#define MANUAL     3

// 핀 설정
int led[4] = { 23, 24, 25, 1 };

// 타이머 및 상태 전역 변수
static struct timer_list timer;
static int mode = STOP;
static int manual_state[4] = { 0, 0, 0, 0 };

static int flag = 0;
static int idx = 0;

// 모드 변경 로직
static void update_mode(int new_mode) {
    int i;
    mode = new_mode;
    del_timer(&timer);
	// STOP, MANUAL이면 끄기
    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
        if (mode == STOP || mode == MANUAL) {
          manual_state[i] = 0;
        }
    }

    flag = 0;
    idx = 0;
	// BLINK ALL, SHIFT면 켜기
    if (mode == BLINK_ALL || mode == SHIFT) {
        mod_timer(&timer, jiffies + HZ * 2);
    }
}

// 타이머 콜백
static void timer_cb(struct timer_list* timer) {
    int i;
	// 전체모드
    if (mode == BLINK_ALL) {
        flag = !flag;
        for (i = 0; i < 4; i++) {
        	gpio_direction_output(led[i], flag);
        }
    }
    // 개별모드
    else if (mode == SHIFT) {
        for (i = 0; i < 4; i++) {
          	gpio_direction_output(led[i], LOW);
        }
        gpio_direction_output(led[idx], HIGH);

        idx++;
        if (idx >= 4) idx = 0;
    }

    if (mode == BLINK_ALL || mode == SHIFT) {
        mod_timer(timer, jiffies + HZ * 2);
    }
}

// 저수준 파일 입출력 대응 함수
static ssize_t assign2_write(struct file* file, const char __user* buf, size_t length, loff_t* ofs) {
    char kbuf;
    int target = -1;

    if (length == 0)
        return 0;
    if (copy_from_user(&kbuf, buf, 1))
        return -EFAULT;

    printk(KERN_INFO "Driver received: %c, Mode: %d\n", kbuf, mode);
	// 수동모드일 때
    if (mode == MANUAL) {
        if (kbuf >= '0' && kbuf <= '3')
            target = kbuf - '0';
        else if (kbuf == '4') {
            update_mode(STOP);
            return 1;
        }

        if (target != -1) {
            manual_state[target] = !manual_state[target];
            gpio_direction_output(led[target], manual_state[target]);
        }
    }
    // 수동모드 아닐때
    else {
        if (kbuf == '1') {
            update_mode(BLINK_ALL);
        }
        else if (kbuf == '2') {
            update_mode(SHIFT);
        }
        else if (kbuf == '3') {
            update_mode(MANUAL);
        }
        else if (kbuf == '4') {
            update_mode(STOP);
        }
    }
    return 1;
}
// GPIO 초기화
static int assign2_open(struct inode* inode, struct file* file) {
    int ret, i;
    printk(KERN_INFO "assign2_driver_open!\n");

    timer_setup(&timer, timer_cb, 0);
    mode = STOP;
    flag = 0;
    idx = 0;

    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0)
			goto err_gpio;
        ret = gpio_direction_output(led[i], LOW);
        if (ret < 0) {
            gpio_free(led[i]);
            goto err_gpio;
        }
        manual_state[i] = 0;
    }

    return 0;

err_gpio:
    while (--i >= 0)
        gpio_free(led[i]);
    return ret;
}
// GPIO 반환
static int assign2_release(struct inode* inode, struct file* file) {
    int i;
    mode = STOP;
    del_timer_sync(&timer);

    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
        gpio_free(led[i]);
    }

    return 0;
}
// 파일 오퍼레이션 구조체
static struct file_operations assign2_fops = {
    .owner = THIS_MODULE,
    .open = assign2_open,
    .release = assign2_release,
    .write = assign2_write,
};

static int assign2_init(void) {
	int ret;

	printk(KERN_INFO "Assign2 Init!\n");
	// 문자 디바이스 등록
    ret = register_chrdev(DEV_MAJOR_NUMBER, DEV_NAME, &assign2_fops);
    if (ret < 0) {
        printk(KERN_ERR "assign2: register_chrdev failed: %d\n", ret);
        return ret;
    }

    return 0;
}

static void assign2_exit(void) {
  	printk(KERN_INFO "Assign2 Exit!\n");
    // 문자 디바이스 해제
    unregister_chrdev(DEV_MAJOR_NUMBER, DEV_NAME);
}

module_init(assign2_init);
module_exit(assign2_exit);
MODULE_LICENSE("GPL");
