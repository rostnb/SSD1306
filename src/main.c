#include "../inc/ssd1306.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define MAX_STR_LEN 128

volatile sig_atomic_t shutting_down = 0;

void signal_handler(int sig) {
    shutting_down = 1;
}

char* get_command_output(const char* cmd) {
    char* buffer = malloc(MAX_STR_LEN);
    if (!buffer) return strdup("N/A");
    FILE* fp = popen(cmd, "r");
    if (!fp) {
        free(buffer);
        return strdup("N/A");
    }
    if (fgets(buffer, MAX_STR_LEN, fp)) {
        buffer[strcspn(buffer, "\n")] = 0;
    } else {
        strcpy(buffer, "N/A");
    }
    pclose(fp);
    return buffer;
}

float get_free_space(const char* path) {
    char cmd[MAX_STR_LEN];
    snprintf(cmd, MAX_STR_LEN, "df -BG %s | tail -1 | awk '{print $4}' | sed 's/G//'", path);
    char* result = get_command_output(cmd);
    float free_space = atof(result);
    free(result);
    return free_space > 0 ? free_space : 0;
}

float get_total_space(const char* path) {
    char cmd[MAX_STR_LEN];
    snprintf(cmd, MAX_STR_LEN, "df -BG %s | tail -1 | awk '{print $2}' | sed 's/G//'", path);
    char* result = get_command_output(cmd);
    float total_space = atof(result);
    free(result);
    return total_space > 0 ? total_space : 1;
}

int main() {
    bus = i2c_init((char*)"/dev/i2c-0", 0x3c);
    if (bus < 0) {
        return 1;
    }
    ssd1306Init(SSD1306_SWITCHCAPVCC);

    // Очистка экрана сразу после инициализации
    ssd1306ClearScreen(LAYER0 | LAYER1);

    // Экран загрузки
    _font = (FONT_INFO*)&ubuntuMono_16ptFontInfo;
    ssd1306DrawString(10, 20, (int8_t*)"Booting...", 1, WHITE, LAYER0);
    ssd1306Refresh();
    sleep(5);

    // Перехват SIGTERM для shutdown
    signal(SIGTERM, signal_handler);

    const char* disk1_path = "/srv/dev-disk-by-uuid-bb4df8fd-9c8d-4379-b989-8b9386243872"; // /dev/sda1, 741G
    const char* disk2_path = "/srv/dev-disk-by-uuid-206df386-9033-4460-aaa3-ba0e96949fe9"; // /dev/sdb1, 749G

    sleep(10); // Ждём монтирования дисков

    while (!shutting_down) {
        ssd1306ClearScreen(LAYER0 | LAYER1);

        // 1. Температура (16pt, верх, без "C")
        _font = (FONT_INFO*)&ubuntuMono_16ptFontInfo;
        char temp_str[MAX_STR_LEN];
        FILE* temp_fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        if (temp_fp) {
            int temp;
            fscanf(temp_fp, "%d", &temp);
            fclose(temp_fp);
            float temp_c = temp / 1000.0;
            snprintf(temp_str, MAX_STR_LEN, "%.1f\xb0", temp_c);
        } else {
            strcpy(temp_str, "N/A");
        }
        ssd1306DrawString(0, 0, (int8_t*)temp_str, 1, WHITE, LAYER0);

        // 2. IP-адрес (8pt, справа, без "IP:")
        _font = (FONT_INFO*)&ubuntuMono_8ptFontInfo;
        char ip_cmd[] = "ip -4 addr show eth0 | grep inet | awk '{print $2}' | cut -d'/' -f1";
        char* ip_addr = get_command_output(ip_cmd);
        int ip_width = strlen(ip_addr) * 6;
        ssd1306DrawString(128 - ip_width, 0, (int8_t*)ip_addr, 1, WHITE, LAYER0);
        free(ip_addr);

        // Линия
        ssd1306DrawLine(0, 18, 127, 18, WHITE, LAYER0);

        // 3. Диски (8pt, шаг 10 пикселей, график сдвинут и растянут)
        float d1_free = get_free_space(disk1_path);
        float d1_total = get_total_space(disk1_path);
        float d2_free = get_free_space(disk2_path);
        float d2_total = get_total_space(disk2_path);

        char disk1_str[MAX_STR_LEN], disk2_str[MAX_STR_LEN];
        snprintf(disk1_str, MAX_STR_LEN, "D1:%.0f", d1_free);
        snprintf(disk2_str, MAX_STR_LEN, "D2:%.0f", d2_free);

        ssd1306DrawString(0, 20, (int8_t*)disk1_str, 1, WHITE, LAYER0);
        float d1_used = d1_free > 0 ? 1.0 - d1_free / d1_total : 0;
        ssd1306FillRect(40, 20, (int)(88 * d1_used), 8, WHITE, LAYER0);
        ssd1306DrawRect(40, 20, 88, 8, WHITE, LAYER0);

        ssd1306DrawString(0, 30, (int8_t*)disk2_str, 1, WHITE, LAYER0);
        float d2_used = d2_free > 0 ? 1.0 - d2_free / d2_total : 0;
        ssd1306FillRect(40, 30, (int)(88 * d2_used), 8, WHITE, LAYER0);
        ssd1306DrawRect(40, 30, 88, 8, WHITE, LAYER0);

        // Линия
        ssd1306DrawLine(0, 40, 127, 40, WHITE, LAYER0);

        // 4. Память (8pt, без шкалы, с "RAM")
        char mem_cmd[] = "free -h | grep Mem | awk '{print $3\"/\"$2}'";
        char* mem_info = get_command_output(mem_cmd);
        char mem_str[MAX_STR_LEN];
        snprintf(mem_str, MAX_STR_LEN, "RAM: %s", mem_info); // Изменено с "R" на "RAM"
        ssd1306DrawString(0, 42, (int8_t*)mem_str, 1, WHITE, LAYER0);
        free(mem_info);

        // 5. Время (8pt, внизу, с "TIME")
        time_t rawtime;
        struct tm* timeinfo;
        char time_str[MAX_STR_LEN];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(time_str, MAX_STR_LEN, "TIME: %H:%M:%S", timeinfo); // Добавлено "TIME"
        ssd1306DrawString(0, 52, (int8_t*)time_str, 1, WHITE, LAYER0);

        ssd1306Refresh();
        SSD1306MSDELAY(1000);
    }

    // Экран выключения
    ssd1306ClearScreen(LAYER0 | LAYER1);
    _font = (FONT_INFO*)&ubuntuMono_16ptFontInfo;
    ssd1306DrawString(10, 20, (int8_t*)"Shutdown...", 1, WHITE, LAYER0);
    ssd1306Refresh();
    sleep(2);
    ssd1306ClearScreen(LAYER0 | LAYER1);
    ssd1306Refresh();
    close(bus);
    return 0;
}
