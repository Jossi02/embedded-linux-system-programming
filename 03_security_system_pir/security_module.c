#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#define HIGH 1
#define LOW 0
#define GPIO 7  

// 핀 설정
int led[4] = { 23, 24, 25, 1 };
int sw[4] = { 4, 17, 27, 22 };
static int pir_irq;
static int sw_irq[4];

// 타이머 및 상태 전역 변수 
static struct timer_list timer;
static int alarm_status = LOW; // 알람 활성화 상태
static int led_status = LOW;   // LED 점멸 상태

// 타이머 콜백 2초 간격으로 호출
static void timer_cb(struct timer_list *timer) {
    int i;
    // 알람이 활성화된 경우에만 동작
    if (alarm_status == HIGH) {
        led_status = !led_status;

        for(i = 0; i < 4; i++) {
            gpio_direction_output(led[i], led_status);
        }

        mod_timer(timer, jiffies + HZ * 2);
    }
}

// PIR 인터럽트 핸들러: 물체 감지 시 동작
irqreturn_t pir_irq_handler(int irq, void *dev_id) {
    int i;
    // 알람이 꺼져있을 때만 동작 시작
    if (alarm_status == LOW) {
        printk(KERN_INFO "PIR Detected! Alarm Start.\n");
        alarm_status = HIGH;
        led_status = HIGH;
        
        // 즉시 모든 LED 켜기 
        for(i = 0; i < 4; i++) {
            gpio_direction_output(led[i], led_status);
        }
        
        mod_timer(&timer, jiffies + HZ * 2);
    }
    return IRQ_HANDLED;
}

// 스위치 인터럽트 핸들러: 스위치 누르면 동작 
irqreturn_t sw_irq_handler(int irq, void *dev_id) {
    int i;
    // 알람이 켜져있을 때만 동작 (알람 종료)
    if (alarm_status == HIGH) {
        printk(KERN_INFO "Switch Pressed! Alarm Stop.\n");
        alarm_status = LOW;
        
        del_timer(&timer);

        // 모든 LED 끄기 
        for(i = 0; i < 4; i++) {
            gpio_direction_output(led[i], LOW);
        }
    }
    return IRQ_HANDLED;
}

static int assign3_init(void) {
    int res, i;
    int led_count = 0;
    int sw_count = 0;
    int sw_irq_count = 0;
    int pir_gpio_requested = 0;
    int pir_irq_requested = 0;

    printk(KERN_INFO "Assign3 Init!\n");

    alarm_status = LOW;
    led_status = LOW;
    timer_setup(&timer, timer_cb, 0);

    // LED GPIO 요청 및 초기화
    for(i = 0; i < 4; i++) {
        res = gpio_request(led[i], "LED");
        if (res < 0) {
            printk(KERN_ERR "assign3: LED gpio_request failed: %d\n", res);
            goto err_resources;
        }
        led_count++;

        res = gpio_direction_output(led[i], LOW); // 초기 상태 OFF
        if (res < 0) {
            printk(KERN_ERR "assign3: LED direction failed: %d\n", res);
            goto err_resources;
        }
    }

    // IRQ 등록 전에 모든 입력 GPIO를 준비
    res = gpio_request(GPIO, "PIR");
    if (res < 0) {
        printk(KERN_ERR "assign3: PIR gpio_request failed: %d\n", res);
        goto err_resources;
    }
    pir_gpio_requested = 1;

    res = gpio_direction_input(GPIO);
    if (res < 0) {
        printk(KERN_ERR "assign3: PIR direction failed: %d\n", res);
        goto err_resources;
    }
    pir_irq = gpio_to_irq(GPIO);
    if (pir_irq < 0) {
        res = pir_irq;
        printk(KERN_ERR "assign3: PIR gpio_to_irq failed: %d\n", res);
        goto err_resources;
    }

    // Switch GPIO 요청
    for(i = 0; i < 4; i++) {
        res = gpio_request(sw[i], "SW");
        if (res < 0) {
            printk(KERN_ERR "assign3: switch gpio_request failed: %d\n", res);
            goto err_resources;
        }
        sw_count++;

        res = gpio_direction_input(sw[i]);
        if (res < 0) {
            printk(KERN_ERR "assign3: switch direction failed: %d\n", res);
            goto err_resources;
        }
        sw_irq[i] = gpio_to_irq(sw[i]);
        if (sw_irq[i] < 0) {
            res = sw_irq[i];
            printk(KERN_ERR "assign3: switch gpio_to_irq failed: %d\n", res);
            goto err_resources;
        }
    }

    // GPIO와 timer가 준비된 뒤 IRQ 등록
    res = request_irq(pir_irq, (irq_handler_t)pir_irq_handler,
        IRQF_TRIGGER_FALLING, "PIR_IRQ", &pir_irq);
    if (res < 0) {
        printk(KERN_ERR "assign3: PIR IRQ request failed: %d\n", res);
        goto err_resources;
    }
    pir_irq_requested = 1;

    for (i = 0; i < 4; i++) {
        res = request_irq(sw_irq[i], (irq_handler_t)sw_irq_handler,
            IRQF_TRIGGER_RISING, "SW_IRQ", &sw[i]);
        if (res < 0) {
            printk(KERN_ERR "assign3: switch IRQ request failed: %d\n", res);
            goto err_resources;
        }
        sw_irq_count++;
    }

    return 0;

err_resources:
    while (sw_irq_count > 0) {
        sw_irq_count--;
        free_irq(sw_irq[sw_irq_count], &sw[sw_irq_count]);
    }
    if (pir_irq_requested)
        free_irq(pir_irq, &pir_irq);
    alarm_status = LOW;
    del_timer_sync(&timer);
    while (sw_count > 0)
        gpio_free(sw[--sw_count]);
    if (pir_gpio_requested)
        gpio_free(GPIO);
    while (led_count > 0)
        gpio_free(led[--led_count]);
    return res;
}

static void assign3_exit(void) {
    int i;
    printk(KERN_INFO "Assign3 Exit!\n");
    // IRQ를 먼저 해제해 새 타이머 동작을 막은 뒤 callback 종료를 기다림
    for(i = 0; i < 4; i++)
        free_irq(sw_irq[i], &sw[i]);
    free_irq(pir_irq, &pir_irq);
    alarm_status = LOW;
    del_timer_sync(&timer);

    // LED 끄기 및 GPIO 해제 
    for(i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
        gpio_free(led[i]);
    }

    // PIR GPIO 해제
    gpio_free(GPIO);

    // Switch GPIO 해제
    for(i = 0; i < 4; i++)
        gpio_free(sw[i]);
}

module_init(assign3_init);
module_exit(assign3_exit);
MODULE_LICENSE("GPL");
