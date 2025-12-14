#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <termios.h>
#include <sys/stat.h>
#include <time.h>

typedef struct { char items[100][256]; int is_dir[100]; int count; int cursor; } CUI_List;
CUI_List LIST_L1 = {0}, LIST_L2 = {0};

// --- MiniCUI Global Variables ---
long VAR_A = 0; long VAR_B = 0; long VAR_C = 0; long VAR_D = 0; long VAR_E = 0; long VAR_F = 0; long VAR_G = 0; long VAR_H = 0; long VAR_I = 0; long VAR_J = 0; long VAR_K = 0; long VAR_L = 0; long VAR_M = 0; long VAR_N = 0; long VAR_O = 0; long VAR_P = 0; long VAR_Q = 0; long VAR_R = 0; long VAR_T = 0; long VAR_U = 0; long VAR_V = 0; long VAR_W = 0; long VAR_X = 0; long VAR_Y = 0; long VAR_Z = 0; char VAR_S[256] = {0}; char VAR_S2[256] = {0}; char VAR_S3[256] = {0};

// --- MiniCUI Runtime Function Prototypes ---
void mc_cls();
void mc_pos(int x, int y);
void mc_color(int c);
void mc_reset();
void mc_sleep(int ms);
void mc_box(int x, int y, int w, int h);
void mc_center(int y, char* str);
int mc_get_key();
void mc_load_dir(CUI_List *list);
void mc_render_list(CUI_List *list, int x, int y, int h);
void mc_input(int x, int y, int max_len, char* dest);
void mc_exit_prog();

// --- User Function Prototypes ---

int main() {
    srand(time(NULL));
    mc_cls();
    VAR_C = 5;
    mc_pos(1, 1);
    printf("%s\n", "Starting WHILE test...");
WHILE_START_1:
    if (!(VAR_C > 0)) goto WHILE_END_1;
    mc_pos(1, 2);
    printf("%s\n", "Countdown: ");
    mc_pos(12, 2);
    printf("%ld\n", VAR_C);
    mc_sleep(500);
    VAR_C -= 1;
    goto WHILE_START_1;
WHILE_END_1:
    mc_pos(1, 3);
    printf("%s\n", "WHILE test finished.");
    mc_sleep(1000);
    mc_exit_prog();
MC_END_LABEL:
    mc_exit_prog();
    return 0;
}

void mc_cls() { printf("\033[2J"); }
void mc_pos(int x, int y) { printf("\033[%d;%dH", y, x); }
void mc_color(int c) { printf("\033[%dm", c); }
void mc_reset() { printf("\033[0m"); }
void mc_exit_prog() { mc_pos(1, 25); mc_reset(); exit(0); }
void mc_sleep(int ms) { usleep(ms * 1000); }
void mc_box(int x, int y, int w, int h) {
    mc_pos(x, y); printf("+"); for(int i=0;i<w-2;i++) printf("-"); printf("+");
    for(int i=1; i<h-1; i++) { mc_pos(x, y+i); printf("|"); mc_pos(x+w-1, y+i); printf("|"); }
    mc_pos(x, y+h-1); printf("+"); for(int i=0;i<w-2;i++) printf("-"); printf("+");
}
void mc_center(int y, char* str) {
    int len = strlen(str);
    int x = (80 - len) / 2;
    if(x<1) x=1;
    mc_pos(x, y); printf("%s", str);
}
int mc_get_key() {
    struct termios orig_termios, raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios; raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    int c = getchar();
    if (c == 27) {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) == 1) {
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) == 1) {
                    switch (seq[1]) {
                        case 'A': c = 1000; break; /* UP */
                        case 'B': c = 1001; break; /* DOWN */
                        case 'D': c = 1002; break; /* LEFT */
                        case 'C': c = 1003; break; /* RIGHT */
                        default: c = 27; break; /* Unknown sequence, treat as ESC */
                    }
                }
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    return c;
}
void mc_load_dir(CUI_List *list) {
    DIR *d = opendir(".");
    struct dirent *dir;
    struct stat st;
    list->count = 0; list->cursor = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (list->count >= 100) break;
            strcpy(list->items[list->count], dir->d_name);
            stat(dir->d_name, &st);
            list->is_dir[list->count] = S_ISDIR(st.st_mode);
            list->count++;
        }
        closedir(d);
    }
}
void mc_render_list(CUI_List *list, int x, int y, int h) {
    mc_reset();
    int start_index = list->cursor - h / 2;
    if (start_index < 0) start_index = 0;
    if (start_index > list->count - h) start_index = list->count - h;
    if (start_index < 0) start_index = 0; 
    for(int i = 0; i < h; i++) {
        int list_index = start_index + i;
        mc_pos(x, y + i);
        if (list_index >= 0 && list_index < list->count) {
            if (list_index == list->cursor) { mc_color(47); mc_color(30); printf(">"); } else { mc_color(40); printf(" "); }
            if (list->is_dir[list_index]) mc_color(34); else mc_color(37);
            char temp_item[40];
            strncpy(temp_item, list->items[list_index], 39);
            temp_item[39] = '\0';
            printf("%s", temp_item);
            if (list->is_dir[list_index]) printf("/");
            for(int j=0; j<40-(int)strlen(temp_item)-(list->is_dir[list_index]?1:0); j++) printf(" ");
        } else {
            printf("                                             ");
        }
        mc_reset();
    }
}
void mc_input(int x, int y, int max_len, char* dest) {
    mc_pos(x, y);
    mc_color(47); mc_color(30);
    char buffer[256] = {0};
    for(int i=0; i<max_len; i++) printf(" ");
    mc_pos(x, y);
    fflush(stdout);
    struct termios orig_termios, raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios; raw.c_lflag |= (ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    fgets(buffer, max_len, stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    strcpy(dest, buffer);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    mc_reset();
}
