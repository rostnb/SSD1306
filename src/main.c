#include "../inc/ssd1306.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_STR_LEN 64

char* get_command_output(const char* cmd) {
    static char buffer[MAX_STR_LEN];
    FILE* fp = popen(cmd, "r");
    if (fp && fgets(buffer, MAX_STR_LEN, fp)) {
        buffer[strcspn(buffer, "\n")] = 0;
        pclose(fp);
        return buffer;
    }
    pclose(fp);
    return "N/A";
}

int main() {
    bus = i2c_init((char*)"/dev/i2c-0", 0x3c);
    ssd1306Init(SSD1306_SWITCHCAPVCC);

    _font = (FONT_INFO*)&ubuntuMono_8ptFontInfo;

    while (1) {
        ssd1306ClearScreen(LAYER0 | LAYER1);
        int y = 0;

        time_t rawtime;
        struct tm* timeinfo;
        char time_str[MAX_STR_LEN];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(time_str, MAX_STR_LEN, "%H:%M:%S", timeinfo);
        ssd1306DrawString(0, y, (int8_t*)time_str, 1, WHITE, LAYER0);
        y += 8;

        char ip_cmd[] = "ip -4 addr show eth0 | grep inet | awk '{print $2}' | cut -d'/' -f1";
        char* ip_addr = get_command_output(ip_cmd);
        char ip_str[MAX_STR_LEN];
        snprintf(ip_str, MAX_STR_LEN, "IP:%s", ip_addr);
        ssd1306DrawString(0, y, (int8_t*)ip_str, 1, WHITE, LAYER0);
        y += 8;

        char disk1_cmd[] = "df -h /srv/dev-disk-by-uuid-bb4df8fd-9c8d-4379-b989-8b9386243872 | tail -1 | awk '{print $2\"/\"$4}'";
        char disk2_cmd[] = "df -h /srv/dev-disk-by-uuid-206df386-9033-4460-aaa3-ba0e96949fe9 | tail -1 | awk '{print $2\"/\"$4}'";
        char* disk1_info = get_command_output(disk1_cmd);
        char* disk2_info = get_command_output(disk2_cmd);
        char disk1_str[MAX_STR_LEN], disk2_str[MAX_STR_LEN];
        snprintf(disk1_str, MAX_STR_LEN, "D1:%s", disk1_info);
        snprintf(disk2_str, MAX_STR_LEN, "D2:%s", disk2_info);
        ssd1306DrawString(0, y, (int8_t*)disk1_str, 1, WHITE, LAYER0);
        y += 8;
        ssd1306DrawString(0, y, (int8_t*)disk2_str, 1, WHITE, LAYER0);
        y += 8;

        char temp_str[MAX_STR_LEN];
        FILE* temp_fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        if (temp_fp) {
            int temp;
            fscanf(temp_fp, "%d", &temp);
            fclose(temp_fp);
            float temp_c = temp / 1000.0;
            snprintf(temp_str, MAX_STR_LEN, "T:%.1fC", temp_c);
        } else {
            strcpy(temp_str, "T:N/A");
        }
        ssd1306DrawString(0, y, (int8_t*)temp_str, 1, WHITE, LAYER0);
        y += 8;

        char mem_cmd[] = "free -h | grep Mem | awk '{print $3\"/\"$2}'";
        char* mem_info = get_command_output(mem_cmd);
        char mem_str[MAX_STR_LEN];
        snprintf(mem_str, MAX_STR_LEN, "R:%s", mem_info);
        ssd1306DrawString(0, y, (int8_t*)mem_str, 1, WHITE, LAYER0);

        ssd1306Refresh();
        SSD1306MSDELAY(2000);
    }

    close(bus);
    return 0;
}

