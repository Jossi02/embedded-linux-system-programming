#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#define HIGH 1
#define LOW 0

#define STOP       0
#define BLINK_ALL  1
#define SHIFT      2
#define MANUAL     3

// 핀 설정
int led[4] = { 23, 24, 25, 1 };
int sw[4] = { 4, 17, 27, 22 };
int sw_irq[4];
static int manual_state[4] = { 0, 0, 0, 0 };

// 디바운싱 변수
static unsigned long last_irq_time = 0;

// 타이머 및 상태 전역 변수
static struct timer_list timer;
static int mode = STOP;
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

// 스위치 인터럽트 핸들러
irqreturn_t irq_handler(int irq, void* dev_id) {
    // 디바운싱 조건문
    if (jiffies - last_irq_time < msecs_to_jiffies(200)) {
        return IRQ_HANDLED;
    }
    last_irq_time = jiffies;
    
  	// 수동모드일 때
    if (mode == MANUAL) {
        int target = -1;
        if (irq == sw_irq[0]) target = 0;
        else if (irq == sw_irq[1]) target = 1;
        else if (irq == sw_irq[2]) target = 2;
        else if (irq == sw_irq[3]) {
            update_mode(STOP);
            return IRQ_HANDLED;
        }

        if (target != -1) {
            manual_state[target] = !manual_state[target];
            gpio_direction_output(led[target], manual_state[target]);
        }
    }
    // 수동모드 아닐때
    else {
        if (irq == sw_irq[0]) {
            update_mode(BLINK_ALL);
        }
        else if (irq == sw_irq[1]) {
            update_mode(SHIFT);
        }
        else if (irq == sw_irq[2]) {
            update_mode(MANUAL);
        }
        else if (irq == sw_irq[3]) {
            update_mode(STOP);
        }
    }
    return IRQ_HANDLED;
}

static int assign1_init(void) {
    int res, i;
    int led_count = 0;
    int sw_count = 0;
    int irq_count = 0;

    printk(KERN_INFO "Assign1 Init!\n");
    
    // 타이머 초기화
    timer_setup(&timer, timer_cb, 0);

    // LED GPIO 요청 및 초기화
    for (i = 0; i < 4; i++) {
        res = gpio_request(led[i], "LED");
        if (res < 0) {
            printk(KERN_ERR "assign1: LED gpio_request failed: %d\n", res);
            goto err_resources;
        }
        led_count++;

        res = gpio_direction_output(led[i], LOW); // 초기 상태 OFF
        if (res < 0) {
            printk(KERN_ERR "assign1: LED direction failed: %d\n", res);
            goto err_resources;
        }
    }

    // Switch GPIO 요청 및 인터럽트 등록
    for(i = 0; i < 4; i++) {
        res = gpio_request(sw[i], "SW");
        if (res < 0) {
            printk(KERN_ERR "assign1: switch gpio_request failed: %d\n", res);
            goto err_resources;
        }
        sw_count++;

        res = gpio_direction_input(sw[i]);
        if (res < 0) {
            printk(KERN_ERR "assign1: switch direction failed: %d\n", res);
            goto err_resources;
        }
        sw_irq[i] = gpio_to_irq(sw[i]);
        if (sw_irq[i] < 0) {
            res = sw_irq[i];
            printk(KERN_ERR "assign1: gpio_to_irq failed: %d\n", res);
            goto err_resources;
        }

        res = request_irq(sw_irq[i], (irq_handler_t)irq_handler,
            IRQF_TRIGGER_RISING, "SW_IRQ", &sw[i]);

        if (res < 0) {
            printk(KERN_ERR "assign1: IRQ request failed: %d\n", res);
            goto err_resources;
        }
        irq_count++;
    }

    return 0;

err_resources:
    while (irq_count > 0) {
        irq_count--;
        free_irq(sw_irq[irq_count], &sw[irq_count]);
    }
    mode = STOP;
    del_timer_sync(&timer);
    while (sw_count > 0)
        gpio_free(sw[--sw_count]);
    while (led_count > 0)
        gpio_free(led[--led_count]);
    return res;
}

static void assign1_exit(void) {
    int i;
    printk(KERN_INFO "Assign1 Exit!\n");
    // 새 타이머 동작이 시작되지 않도록 IRQ를 먼저 해제
    for (i = 0; i < 4; i++)
        free_irq(sw_irq[i], &sw[i]);

    mode = STOP;
    del_timer_sync(&timer);

	// LED 끄기 및 GPIO 해제
    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
        gpio_free(led[i]);
    }
    // Switch GPIO 해제
    for (i = 0; i < 4; i++)
        gpio_free(sw[i]);
}

module_init(assign1_init);
module_exit(assign1_exit);
MODULE_LICENSE("GPL");
