#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>
#include <string.h>
#include <board.h>
#include <finsh.h>
#include <drv_lcd.h>

/* 第一组滑台和推手定义 */
#define PWM_DEV_NAME_1        "pwm2"
#define PWM_DEV_CHANNEL_1     4
#define DIR_PIN_1             GET_PIN(A, 0)
#define PUSH_IN1_PIN_1        GET_PIN(A, 1)
#define PUSH_IN2_PIN_1        GET_PIN(A, 2)

/* 第二组滑台和推手定义 */
#define PWM_DEV_NAME_2        "pwm2"
#define PWM_DEV_CHANNEL_2     3
#define DIR_PIN_2             GET_PIN(A, 5)
#define PUSH_IN3_PIN_2        GET_PIN(A, 7)
#define PUSH_IN4_PIN_2        GET_PIN(A, 6)

/* 按键引脚定义 */
#define KEY_UP_PIN    GET_PIN(C, 5)
#define KEY_DOWN_PIN  GET_PIN(C, 1)
#define KEY_SELECT_PIN GET_PIN(C, 3)  // 新增选择按键

/* 颜色宏定义 */
#ifndef WHITE
#define WHITE 0xFFFF
#endif
#ifndef BLACK
#define BLACK 0x0000
#endif
#ifndef RED
#define RED 0xF800
#endif
#ifndef GREEN
#define GREEN 0x07E0
#endif

/* 文本行结构体 */
typedef struct {
    const char *text;
    int y;
    int device_group;  // 1:第一组, 2:第二组
    const char *command;  // 对应执行的命令
} TextLine;

/* 商品信息与控制命令映射 */
static TextLine text_lines[] = {
    {"Snickers",          50, 1, "EXHIBIT_1"},
    {"Fresh:3.50  ",      70, 1, "OUT1_1"},
    {"Short:2.00  ",      90, 1, "OUT2_1"},
    {"Halls Candies",     120, 2, "EXHIBIT_2"},
    {"Fresh:4.00  ",      140, 2, "OUT1_2"},
    {"Short:2.50  ",      160, 2, "OUT2_2"},
};

#define LINE_COUNT (sizeof(text_lines) / sizeof(text_lines[0]))
static int cursor_idx = 0;  // 当前选中行索引
static rt_bool_t operation_in_progress = RT_FALSE;  // 操作进行中标志

/* 第一组相关全局变量 */
static struct rt_device_pwm *pwm_dev_1 = RT_NULL;
static rt_thread_t slide_thread_1 = RT_NULL;
static rt_thread_t push_thread_1 = RT_NULL;
static rt_bool_t push_completed_1 = RT_FALSE;

/* 第二组相关全局变量 */
static struct rt_device_pwm *pwm_dev_2 = RT_NULL;
static rt_thread_t slide_thread_2 = RT_NULL;
static rt_thread_t push_thread_2 = RT_NULL;
static rt_bool_t push_completed_2 = RT_FALSE;

/* 第一组推手控制线程 */
static void push_control_thread_1(void *parameter)
{
    rt_pin_mode(PUSH_IN1_PIN_1, PIN_MODE_OUTPUT);
    rt_pin_mode(PUSH_IN2_PIN_1, PIN_MODE_OUTPUT);

    rt_uint8_t direction = 0;
    rt_uint8_t move_count = 0;

    while (1)
    {
        if (direction == 0)
        {
            rt_pin_write(PUSH_IN1_PIN_1, PIN_HIGH);
            rt_pin_write(PUSH_IN2_PIN_1, PIN_LOW);
            rt_kprintf("Pusher 1 moving forward\n");
        }
        else
        {
            rt_pin_write(PUSH_IN1_PIN_1, PIN_LOW);
            rt_pin_write(PUSH_IN2_PIN_1, PIN_HIGH);
            rt_kprintf("Pusher 1 moving backward\n");
            move_count++;
        }

        direction = !direction;
        rt_thread_mdelay(5500);

        if (move_count >= 1)
        {
            rt_kprintf("Pusher 1 completed one round trip\n");
            push_completed_1 = RT_TRUE;

            rt_thread_t self = rt_thread_self();
            self->stat = RT_THREAD_CLOSE;
            operation_in_progress = RT_FALSE;
            return;
        }
    }
}

/* 第一组滑台控制线程 */
static void slide_control_thread_1(void *parameter)
{
    const char *cmd = (const char *)parameter;
    rt_uint32_t period, pulse;
    rt_uint8_t dir = 1;
    rt_int32_t delay_ms = 0;

    if (pwm_dev_1 == RT_NULL)
    {
        pwm_dev_1 = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME_1);
        if (pwm_dev_1 == RT_NULL)
        {
            rt_kprintf("PWM device %s not found!\n", PWM_DEV_NAME_1);
            operation_in_progress = RT_FALSE;
            return;
        }
    }

    rt_pin_mode(DIR_PIN_1, PIN_MODE_OUTPUT);

    if (!strcmp(cmd, "ENDPOINT_1"))
    {
        rt_kprintf("Command: ENDPOINT_1\n");
        dir = 0;
        delay_ms = 60000;
    }
    else if (!strcmp(cmd, "EXHIBIT_1"))
    {
        rt_kprintf("Command: EXHIBIT_1\n");
        dir = 1;
        delay_ms = 4360;
    }
    else if (!strcmp(cmd, "OUT1_1"))
    {
        rt_kprintf("Command: OUT1_1\n");
        dir = 1;
        delay_ms = 34020;
    }
    else if (!strcmp(cmd, "TRANSPORT_1"))
    {
        rt_kprintf("Command: TRANSPORT_1\n");
        dir = 1;
        delay_ms = 6000;
    }
    else if (!strcmp(cmd, "OUT2_1"))
    {
        rt_kprintf("Command: OUT2_1\n");
        dir = 1;
        delay_ms = 17590;
    }
    else if (!strcmp(cmd, "EXIT_1"))
    {
        rt_kprintf("Command: EXIT_1\n");
        rt_pwm_disable(pwm_dev_1, PWM_DEV_CHANNEL_1);
        rt_kprintf("Slide 1 stopped\n");
        operation_in_progress = RT_FALSE;
        return;
    }
    else
    {
        rt_kprintf("Unknown command for Slide 1: %s\n", cmd);
        operation_in_progress = RT_FALSE;
        return;
    }

    rt_pin_write(DIR_PIN_1, dir);

    rt_uint32_t rpm = 200;
    period = 60 * 1000000000ULL / (rpm * 400);
    pulse = period / 2;
    rt_pwm_set(pwm_dev_1, PWM_DEV_CHANNEL_1, period, pulse);
    rt_pwm_enable(pwm_dev_1, PWM_DEV_CHANNEL_1);

    rt_thread_mdelay(delay_ms);

    rt_pwm_disable(pwm_dev_1, PWM_DEV_CHANNEL_1);
    rt_kprintf("Slide 1 movement complete, ready for next command\n");

    if (slide_thread_1)
    {
        slide_thread_1 = RT_NULL;
    }

    rt_kprintf("\nmsh > ");
    operation_in_progress = RT_FALSE;
}

/* 第一组推手控制命令 */
static int push_sample_1(int argc, char *argv[])
{
    if (push_thread_1 && (push_thread_1->stat != RT_THREAD_CLOSE) || operation_in_progress)
    {
        rt_kprintf("Pusher 1 operation in progress\n");
        return RT_ERROR;
    }

    push_completed_1 = RT_FALSE;
    operation_in_progress = RT_TRUE;
    push_thread_1 = rt_thread_create("push_ctrl_1",
                                     push_control_thread_1,
                                     RT_NULL,
                                     1024,
                                     20,
                                     10);
    if (push_thread_1 != RT_NULL)
    {
        rt_thread_startup(push_thread_1);
        return RT_EOK;
    }
    else
    {
        rt_kprintf("Failed to start Pusher 1 thread.\n");
        operation_in_progress = RT_FALSE;
        return RT_ERROR;
    }
}

/* 第一组统一的滑台控制命令实现 */
static int slide_command_1(const char *cmd)
{
    if (slide_thread_1 && (slide_thread_1->stat != RT_THREAD_CLOSE) || operation_in_progress)
    {
        rt_kprintf("Previous Slide 1 operation still running.\n");
        return RT_ERROR;
    }

    operation_in_progress = RT_TRUE;
    slide_thread_1 = rt_thread_create("slide_ctrl_1",
                                      slide_control_thread_1,
                                      (void *)cmd,
                                      1024,
                                      20,
                                      10);
    if (slide_thread_1 != RT_NULL)
    {
        rt_thread_startup(slide_thread_1);
        return RT_EOK;
    }
    else
    {
        rt_kprintf("Failed to start Slide 1 thread.\n");
        operation_in_progress = RT_FALSE;
        return RT_ERROR;
    }
}

/* 第二组推手控制线程 */
static void push_control_thread_2(void *parameter)
{
    rt_pin_mode(PUSH_IN3_PIN_2, PIN_MODE_OUTPUT);
    rt_pin_mode(PUSH_IN4_PIN_2, PIN_MODE_OUTPUT);

    rt_uint8_t direction = 0;
    rt_uint8_t move_count = 0;

    while (1)
    {
        if (direction == 0)
        {
            rt_pin_write(PUSH_IN3_PIN_2, PIN_HIGH);
            rt_pin_write(PUSH_IN4_PIN_2, PIN_LOW);
            rt_kprintf("Pusher 2 moving forward\n");
        }
        else
        {
            rt_pin_write(PUSH_IN3_PIN_2, PIN_LOW);
            rt_pin_write(PUSH_IN4_PIN_2, PIN_HIGH);
            rt_kprintf("Pusher 2 moving backward\n");
            move_count++;
        }

        direction = !direction;
        rt_thread_mdelay(5500);

        if (move_count >= 1)
        {
            rt_kprintf("Pusher 2 completed one round trip\n");
            push_completed_2 = RT_TRUE;

            rt_thread_t self = rt_thread_self();
            self->stat = RT_THREAD_CLOSE;
            operation_in_progress = RT_FALSE;
            return;
        }
    }
}

/* 第二组滑台控制线程 */
static void slide_control_thread_2(void *parameter)
{
    const char *cmd = (const char *)parameter;
    rt_uint32_t period, pulse;
    rt_uint8_t dir = 1;
    rt_int32_t delay_ms = 0;

    if (pwm_dev_2 == RT_NULL)
    {
        pwm_dev_2 = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME_2);
        if (pwm_dev_2 == RT_NULL)
        {
            rt_kprintf("PWM device %s not found!\n", PWM_DEV_NAME_2);
            operation_in_progress = RT_FALSE;
            return;
        }
    }

    rt_pin_mode(DIR_PIN_2, PIN_MODE_OUTPUT);

    if (!strcmp(cmd, "ENDPOINT_2"))
    {
        rt_kprintf("Command: ENDPOINT_2\n");
        dir = 0;
        delay_ms = 60000;
    }
    else if (!strcmp(cmd, "EXHIBIT_2"))
    {
        rt_kprintf("Command: EXHIBIT_2\n");
        dir = 1;
        delay_ms = 4360;
    }
    else if (!strcmp(cmd, "OUT1_2"))
    {
        rt_kprintf("Command: OUT1_2\n");
        dir = 1;
        delay_ms = 28920;
    }
    else if (!strcmp(cmd, "TRANSPORT_2"))
    {
        rt_kprintf("Command: TRANSPORT_2\n");
        dir = 1;
        delay_ms = 6890;
    }
    else if (!strcmp(cmd, "OUT2_2"))
    {
        rt_kprintf("Command: OUT2_2\n");
        dir = 1;
        delay_ms = 17590;
    }
    else if (!strcmp(cmd, "EXIT_2"))
    {
        rt_kprintf("Command: EXIT_2\n");
        rt_pwm_disable(pwm_dev_2, PWM_DEV_CHANNEL_2);
        rt_kprintf("Slide 2 stopped\n");
        operation_in_progress = RT_FALSE;
        return;
    }
    else
    {
        rt_kprintf("Unknown command for Slide 2: %s\n", cmd);
        operation_in_progress = RT_FALSE;
        return;
    }

    rt_pin_write(DIR_PIN_2, dir);

    rt_uint32_t rpm = 200;
    period = 60 * 1000000000ULL / (rpm * 400);
    pulse = period / 2;
    rt_pwm_set(pwm_dev_2, PWM_DEV_CHANNEL_2, period, pulse);
    rt_pwm_enable(pwm_dev_2, PWM_DEV_CHANNEL_2);

    rt_thread_mdelay(delay_ms);

    rt_pwm_disable(pwm_dev_2, PWM_DEV_CHANNEL_2);
    rt_kprintf("Slide 2 movement complete, ready for next command\n");

    if (slide_thread_2)
    {
        slide_thread_2 = RT_NULL;
    }

    rt_kprintf("\nmsh > ");
    operation_in_progress = RT_FALSE;
}

/* 第二组推手控制命令 */
static int push_sample_2(int argc, char *argv[])
{
    if (push_thread_2 && (push_thread_2->stat != RT_THREAD_CLOSE) || operation_in_progress)
    {
        rt_kprintf("Pusher 2 operation in progress\n");
        return RT_ERROR;
    }

    push_completed_2 = RT_FALSE;
    operation_in_progress = RT_TRUE;
    push_thread_2 = rt_thread_create("push_ctrl_2",
                                     push_control_thread_2,
                                     RT_NULL,
                                     1024,
                                     20,
                                     10);
    if (push_thread_2 != RT_NULL)
    {
        rt_thread_startup(push_thread_2);
        return RT_EOK;
    }
    else
    {
        rt_kprintf("Failed to start Pusher 2 thread.\n");
        operation_in_progress = RT_FALSE;
        return RT_ERROR;
    }
}

/* 第二组统一的滑台控制命令实现 */
static int slide_command_2(const char *cmd)
{
    if (slide_thread_2 && (slide_thread_2->stat != RT_THREAD_CLOSE) || operation_in_progress)
    {
        rt_kprintf("Previous Slide 2 operation still running.\n");
        return RT_ERROR;
    }

    operation_in_progress = RT_TRUE;
    slide_thread_2 = rt_thread_create("slide_ctrl_2",
                                      slide_control_thread_2,
                                      (void *)cmd,
                                      1024,
                                      20,
                                      10);
    if (slide_thread_2 != RT_NULL)
    {
        rt_thread_startup(slide_thread_2);
        return RT_EOK;
    }
    else
    {
        rt_kprintf("Failed to start Slide 2 thread.\n");
        operation_in_progress = RT_FALSE;
        return RT_ERROR;
    }
}

/* 绘制单行文本 */
void draw_line(int idx, rt_bool_t selected) {
    int y = text_lines[idx].y;

    /* 清除该行区域 */
    lcd_set_color(BLACK, BLACK);
    for (int i = 0; i < 16; i++) {
        lcd_draw_line(10, y + i, 230, y + i);
    }

    /* 绘制光标框 */
    if (selected) {
        lcd_set_color(RED, BLACK);
        lcd_draw_line(10, y - 2, 230, y - 2);
        lcd_draw_line(10, y + 18, 230, y + 18);
        lcd_draw_line(10, y - 2, 10, y + 18);
        lcd_draw_line(230, y - 2, 230, y + 18);
    }

    /* 绘制文本内容 */
    lcd_set_color(selected ? RED : WHITE, BLACK);
    lcd_show_string(10, y, 16, text_lines[idx].text);
}

/* 绘制全部行内容 */
void draw_all_lines(void) {
    for (int i = 0; i < LINE_COUNT; i++) {
        draw_line(i, i == cursor_idx);
    }
}

/* 显示操作提示 */
void show_operation_status(const char *status) {
    lcd_set_color(WHITE, BLACK);
    lcd_show_string(10, 260, 16, "                "); // 先清除原有内容
    lcd_show_string(10, 260, 16, status);
}

/* 初始化显示界面 */
void init_display(void) {
    lcd_clear(BLACK);
    lcd_set_color(WHITE, BLACK);
    lcd_show_string(10, 10, 24, "Vending Machine");
    lcd_draw_line(0, 40, 240, 40);
    draw_all_lines();
    lcd_show_string(10, 260, 16, "Use Up/Down to select, Select to buy");
}

/* 按键检测函数 */
int get_key_value(void) {
    if (rt_pin_read(KEY_UP_PIN) == PIN_LOW) {
        rt_thread_mdelay(20);
        if (rt_pin_read(KEY_UP_PIN) == PIN_LOW) return 1; // 上键
    }
    if (rt_pin_read(KEY_DOWN_PIN) == PIN_LOW) {
        rt_thread_mdelay(20);
        if (rt_pin_read(KEY_DOWN_PIN) == PIN_LOW) return 2; // 下键
    }
    if (rt_pin_read(KEY_SELECT_PIN) == PIN_LOW) {
        rt_thread_mdelay(20);
        if (rt_pin_read(KEY_SELECT_PIN) == PIN_LOW) return 3; // 选择键
    }
    return 0;
}

/* 执行选择的商品对应的命令 */
void execute_selected_command(void) {
    const char *cmd = text_lines[cursor_idx].command;
    int group = text_lines[cursor_idx].device_group;
    
    show_operation_status("Processing...      ");
    rt_kprintf("Executing command: %s for group %d\n", cmd, group);
    
    if (group == 1) {
        if (strstr(cmd, "EXHIBIT")) {
            slide_command_1(cmd);
        } else if (strstr(cmd, "OUT")) {
            slide_command_1(cmd);
            rt_thread_mdelay(2000); // 等待滑台到位
            push_sample_1(0, NULL);
        }
    } else if (group == 2) {
        if (strstr(cmd, "EXHIBIT")) {
            slide_command_2(cmd);
        } else if (strstr(cmd, "OUT")) {
            slide_command_2(cmd);
            rt_thread_mdelay(2000); // 等待滑台到位
            push_sample_2(0, NULL);
        }
    }
    
    show_operation_status("Operation complete ");
    rt_thread_mdelay(2000); // 显示操作完成信息
}

/* 第一组命令封装 */
static int ENDPOINT_1(int argc, char *argv[]) { return slide_command_1("ENDPOINT_1"); }
static int EXHIBIT_1(int argc, char *argv[]) { return slide_command_1("EXHIBIT_1"); }
static int OUT1_1(int argc, char *argv[]) { return slide_command_1("OUT1_1"); }
static int TRANSPORT_1(int argc, char *argv[]) { return slide_command_1("TRANSPORT_1"); }
static int OUT2_1(int argc, char *argv[]) { return slide_command_1("OUT2_1"); }
static int EXIT_1(int argc, char *argv[]) { return slide_command_1("EXIT_1"); }
static int PUSH_CTRL_1(int argc, char *argv[]) { return push_sample_1(argc, argv); }

/* 第二组命令封装 */
static int ENDPOINT_2(int argc, char *argv[]) { return slide_command_2("ENDPOINT_2"); }
static int EXHIBIT_2(int argc, char *argv[]) { return slide_command_2("EXHIBIT_2"); }
static int OUT1_2(int argc, char *argv[]) { return slide_command_2("OUT1_2"); }
static int TRANSPORT_2(int argc, char *argv[]) { return slide_command_2("TRANSPORT_2"); }
static int OUT2_2(int argc, char *argv[]) { return slide_command_2("OUT2_2"); }
static int EXIT_2(int argc, char *argv[]) { return slide_command_2("EXIT_2"); }
static int PUSH_CTRL_2(int argc, char *argv[]) { return push_sample_2(argc, argv); }

/* 注册MSH命令 */
MSH_CMD_EXPORT(ENDPOINT_1, move Slide 1 to endpoint position);
MSH_CMD_EXPORT(EXHIBIT_1, move Slide 1 to exhibit position);
MSH_CMD_EXPORT(OUT1_1, move Slide 1 to output 1 position);
MSH_CMD_EXPORT(TRANSPORT_1, move Slide 1 to transport position);
MSH_CMD_EXPORT(OUT2_1, move Slide 1 to output 2 position);
MSH_CMD_EXPORT(EXIT_1, stop Slide 1 movement);
MSH_CMD_EXPORT(PUSH_CTRL_1, start Pusher 1 for one round trip);

MSH_CMD_EXPORT(ENDPOINT_2, move Slide 2 to endpoint position);
MSH_CMD_EXPORT(EXHIBIT_2, move Slide 2 to exhibit position);
MSH_CMD_EXPORT(OUT1_2, move Slide 2 to output 1 position);
MSH_CMD_EXPORT(TRANSPORT_2, move Slide 2 to transport position);
MSH_CMD_EXPORT(OUT2_2, move Slide 2 to output 2 position);
MSH_CMD_EXPORT(EXIT_2, stop Slide 2 movement);
MSH_CMD_EXPORT(PUSH_CTRL_2, start Pusher 2 for one round trip);

/* 主函数 */
int main(void) {
    rt_device_t lcd_dev;
    
    /* 初始化LCD设备 */
    lcd_dev = rt_device_find("lcd");
    if (lcd_dev) {
        rt_device_init(lcd_dev);
        lcd_clear(BLACK);
    } else {
        rt_kprintf("LCD device not found!\n");
    }
    
    /* 初始化按键输入 */
    rt_pin_mode(KEY_UP_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(KEY_DOWN_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(KEY_SELECT_PIN, PIN_MODE_INPUT_PULLUP);
    
    /* 初始化PWM设备（提前查找，避免运行时查找失败） */
    pwm_dev_1 = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME_1);
    pwm_dev_2 = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME_2);
    
    if (pwm_dev_1 == RT_NULL) {
        rt_kprintf("PWM device %s not found!\n", PWM_DEV_NAME_1);
    }
    if (pwm_dev_2 == RT_NULL) {
        rt_kprintf("PWM device %s not found!\n", PWM_DEV_NAME_2);
    }
    
    /* 初始化显示界面 */
    init_display();
    
    /* 主循环 */
    while (1) {
        int key = get_key_value();
        
        if (key == 1 && cursor_idx > 0) {
            draw_line(cursor_idx, RT_FALSE);
            cursor_idx--;
            draw_line(cursor_idx, RT_TRUE);
        } else if (key == 2 && cursor_idx < LINE_COUNT - 1) {
            draw_line(cursor_idx, RT_FALSE);
            cursor_idx++;
            draw_line(cursor_idx, RT_TRUE);
        } else if (key == 3 && !operation_in_progress) {
            execute_selected_command();
            init_display(); // 操作完成后刷新显示
        }
        
        rt_thread_mdelay(100);
    }

    return 0;
}