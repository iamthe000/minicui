#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE_LEN 1024
#define MAX_LIST_ITEMS 100
#define MAX_PATH_LEN 256
#define MAX_ARGS 20 

// --- MiniCUI Runtime Structures ---
typedef struct {
    char items[MAX_LIST_ITEMS][MAX_PATH_LEN];
    int is_dir[MAX_LIST_ITEMS];
    int count;
    int cursor;
} CUI_List;

// グローバルなリストと変数
CUI_List LIST_L1 = {0}, LIST_L2 = {0};

FILE *out;

// ===================================================
// ランタイム機能
// ===================================================


// ===================================================
// コンパイラのコード生成部分
// ===================================================

void write_runtime_functions() {
    fprintf(out, "%s",
        "void mc_cls() { printf(\"\\033[2J\"); }\n"
        "void mc_pos(int x, int y) { printf(\"\\033[%d;%dH\", y, x); }\n"
        "void mc_color(int c) { printf(\"\\033[%dm\", c); }\n"
        "void mc_reset() { printf(\"\\033[0m\"); }\n"
        "void mc_sleep(int ms) { usleep(ms * 1000); }\n"
        "void mc_box(int x, int y, int w, int h) {\n"
        "    mc_pos(x, y); printf(\"+\"); for(int i=0;i<w-2;i++) printf(\"-\"); printf(\"+\");\n"
        "    for(int i=1; i<h-1; i++) { mc_pos(x, y+i); printf(\"|\"); mc_pos(x+w-1, y+i); printf(\"|\"); }\n"
        "    mc_pos(x, y+h-1); printf(\"+\"); for(int i=0;i<w-2;i++) printf(\"-\"); printf(\"+\");\n"
        "}\n"
        "void mc_center(int y, char* str) {\n"
        "    int len = strlen(str);\n"
        "    int x = (80 - len) / 2;\n"
        "    if(x<1) x=1;\n"
        "    mc_pos(x, y); printf(\"%s\", str);\n"
        "}\n"
        "int mc_get_key() {\n"
        "    struct termios orig_termios, raw;\n"
        "    tcgetattr(STDIN_FILENO, &orig_termios);\n"
        "    raw = orig_termios; raw.c_lflag &= ~(ICANON | ECHO);\n"
        "    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);\n"
        "    int c = getchar();\n"
        "    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);\n"
        "    return c;\n"
        "}\n"
        "void mc_load_dir(CUI_List *list) {\n"
        "    DIR *d = opendir(\".\");\n"
        "    struct dirent *dir;\n"
        "    struct stat st;\n"
        "    list->count = 0; list->cursor = 0;\n"
        "    if (d) {\n"
        "        while ((dir = readdir(d)) != NULL) {\n"
        "            if (list->count >= 100) break;\n"
        "            strcpy(list->items[list->count], dir->d_name);\n"
        "            stat(dir->d_name, &st);\n"
        "            list->is_dir[list->count] = S_ISDIR(st.st_mode);\n"
        "            list->count++;\n"
        "        }\n"
        "        closedir(d);\n"
        "    }\n"
        "}\n"
        "void mc_render_list(CUI_List *list, int x, int y, int h) {\n"
        "    mc_reset();\n"
        "    int start_index = list->cursor - h / 2;\n"
        "    if (start_index < 0) start_index = 0;\n"
        "    if (start_index > list->count - h) start_index = list->count - h;\n"
        "    if (start_index < 0) start_index = 0; \n"
        "    for(int i = 0; i < h; i++) {\n"
        "        int list_index = start_index + i;\n"
        "        mc_pos(x, y + i);\n"
        "        if (list_index >= 0 && list_index < list->count) {\n"
        "            if (list_index == list->cursor) { mc_color(47); mc_color(30); printf(\">\"); } else { mc_color(40); printf(\" \"); }\n"
        "            if (list->is_dir[list_index]) mc_color(34); else mc_color(37);\n"
        "            char temp_item[40];\n"
        "            strncpy(temp_item, list->items[list_index], 39);\n"
        "            temp_item[39] = '\\0';\n"
        "            printf(\"%s\", temp_item);\n"
        "            if (list->is_dir[list_index]) printf(\"/\");\n"
        "            for(int j=0; j<40-(int)strlen(temp_item)-(list->is_dir[list_index]?1:0); j++) printf(\" \");\n"
        "        } else {\n"
        "            printf(\"                                             \");\n"
        "        }\n"
        "        mc_reset();\n"
        "    }\n"
        "}\n"
    );
}

void write_header() {
    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n#include <unistd.h>\n#include <string.h>\n#include <ctype.h>\n#include <dirent.h>\n#include <termios.h>\n#include <sys/stat.h>\n\n");
    fprintf(out, "typedef struct { char items[%d][%d]; int is_dir[%d]; int count; int cursor; } CUI_List;\n", MAX_LIST_ITEMS, MAX_PATH_LEN, MAX_LIST_ITEMS);
    
    fprintf(out, "CUI_List LIST_L1 = {0}, LIST_L2 = {0};\n");
    fprintf(out, "\n// --- MiniCUI Variables ---\n");
    for (char c = 'A'; c <= 'Z'; c++) fprintf(out, "long VAR_%c = 0; ", c);
    fprintf(out, "\n\n");

    fprintf(out, "void mc_cls();\nvoid mc_pos(int x, int y);\nvoid mc_color(int c);\nvoid mc_reset();\nvoid mc_sleep(int ms);\nvoid mc_box(int x, int y, int w, int h);\nvoid mc_center(int y, char* str);\nint mc_get_key();\nvoid mc_load_dir(CUI_List *list);\nvoid mc_render_list(CUI_List *list, int x, int y, int h);\n");

    fprintf(out, "int main() {\n");
    fprintf(out, "    mc_cls();\n");
}

void write_footer() {
    fprintf(out, "    mc_pos(1, 25); mc_reset();\n");
    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
}

// -------------------------------------------------------------------
// パース処理
// -------------------------------------------------------------------
void parse_line(char *line) {
    char clean_line[MAX_LINE_LEN] = {0};
    strcpy(clean_line, line);
    clean_line[strcspn(clean_line, "\n")] = 0;
    
    char *comment = strchr(clean_line, '#');
    if (comment) *comment = '\0';
    if (strlen(clean_line) == 0) return;

    char line_copy[MAX_LINE_LEN];
    strcpy(line_copy, clean_line);
    
    char *p = line_copy;
    while(isspace(*p)) p++;
    if (*p == '\0') return;

    char *cmd = p;
    while(*p && !isspace(*p)) p++;
    if (*p) { *p = '\0'; p++; }

    if (cmd[strlen(cmd)-1] == ':') {
        cmd[strlen(cmd)-1] = '\0';
        fprintf(out, "%s:\n", cmd);
        return;
    }

    char *args[MAX_ARGS] = { NULL };
    int arg_count = 0;

    while(*p && arg_count < MAX_ARGS) {
        while(isspace(*p)) p++;
        if (*p == '\0') break;

        if (*p == '"') {
            args[arg_count++] = p;
            p++;
            while(*p && *p != '"') p++;
            if (*p == '"') p++;
            if (*p) { *p = '\0'; p++; }
        } else {
            args[arg_count++] = p;
            while(*p && !isspace(*p)) p++;
            if (*p) { *p = '\0'; p++; }
        }
    }

    // --- コマンド解析 ---
    
    if (strcmp(cmd, "PRINT") == 0) {
        if (arg_count >= 1) {
            if (args[0][0] == '"') {
                fprintf(out, "    printf(\"%%s\\n\", %s);\n", args[0]);
            } else if (isupper(args[0][0])) {
                fprintf(out, "    printf(\"%%ld\\n\", VAR_%s);\n", args[0]);
            } else {
                 fprintf(out, "    printf(\"%%s\\n\", \"%s\");\n", args[0]);
            }
        }
    }
    else if (strcmp(cmd, "SLEEP") == 0) {
        if (arg_count == 1) fprintf(out, "    mc_sleep(%s);\n", args[0]);
    }
    else if (strcmp(cmd, "CENTER") == 0) {
        if (arg_count == 2) {
            fprintf(out, "    mc_center(%s, %s);\n", args[0], args[1]);
        }
        else if (arg_count == 3) {
            fprintf(out, "    mc_color(%s); mc_center(%s, %s);\n", args[1], args[0], args[2]);
        }
    }
    else if (strcmp(cmd, "SPLASH") == 0) {
        if (arg_count == 4) {
            fprintf(out, "    mc_cls();\n");
            fprintf(out, "    mc_color(%s);\n", args[2]);
            fprintf(out, "    mc_box(20, 8, 42, 8);\n");
            fprintf(out, "    mc_center(11, %s);\n", args[0]);
            fprintf(out, "    mc_color(37);\n");
            fprintf(out, "    mc_center(13, %s);\n", args[1]);
            fprintf(out, "    fflush(stdout);\n");
            fprintf(out, "    mc_sleep(%s);\n", args[3]);
            fprintf(out, "    mc_cls();\n");
        }
    }
    else if (strcmp(cmd, "SET") == 0) {
        if (arg_count == 2) fprintf(out, "    VAR_%s = %s;\n", args[0], args[1]);
    } 

    else if (strcmp(cmd, "IF") == 0) {
        if (arg_count < 3) return;

        char *rhs = args[2];
        char temp_rhs[MAX_PATH_LEN] = {0};
        
        if (rhs && isalpha(rhs[0]) && isupper(rhs[0])) {
            snprintf(temp_rhs, sizeof(temp_rhs), "VAR_%s", args[2]);
            rhs = temp_rhs;
        }

        if (arg_count == 5 && strcmp(args[3], "GOTO") == 0) {
            fprintf(out, "    if (VAR_%s %s %s) goto %s;\n", args[0], args[1], rhs, args[4]);
        }
        else if (arg_count == 7 && strcmp(args[3], "CURSOR") == 0 && strcmp(args[4], "ADJ") == 0) {
            fprintf(out, "    if (VAR_%s %s %s) LIST_%s.cursor += %s;\n", 
                args[0], args[1], rhs, args[5], args[6]);
        }
    }

    else if (strcmp(cmd, "GOTO") == 0) { 
        if (arg_count == 1) fprintf(out, "    goto %s;\n", args[0]);
    } else if (strcmp(cmd, "POS") == 0) { 
        if (arg_count == 2) fprintf(out, "    mc_pos(%s, %s);\n", args[0], args[1]);
    } else if (strcmp(cmd, "COLOR") == 0) { 
        if (arg_count == 1) fprintf(out, "    mc_color(%s);\n", args[0]);
    } else if (strcmp(cmd, "BOX") == 0) { 
        if (arg_count == 4) fprintf(out, "    mc_box(%s, %s, %s, %s);\n", args[0], args[1], args[2], args[3]);
    } else if (strcmp(cmd, "CLEAR") == 0) { fprintf(out, "    mc_cls();\n");
    } else if (strcmp(cmd, "EXIT") == 0) { fprintf(out, "    return 0;\n");
    } 
    else if (strcmp(cmd, "LIST") == 0) {
        if (arg_count == 2 && strcmp(args[0], "LOAD") == 0) {
            fprintf(out, "    mc_load_dir(&LIST_%s);\n", args[1]);
        } 
        else if (arg_count == 5 && strcmp(args[0], "RENDER") == 0) { 
            fprintf(out, "    mc_render_list(&LIST_%s, %s, %s, %s);\n", args[1], args[2], args[3], args[4]);
        }
    } 
    else if (strcmp(cmd, "CURSOR") == 0) {
        if (arg_count == 3 && strcmp(args[0], "ADJ") == 0) {
            fprintf(out, "    LIST_%s.cursor += %s;\n", args[1], args[2]);
        } else if (arg_count == 2 && strcmp(args[0], "LIMIT") == 0) { 
            fprintf(out, "    if (LIST_%s.cursor < 0) LIST_%s.cursor = 0;\n", args[1], args[1]); 
            fprintf(out, "    if (LIST_%s.cursor >= LIST_%s.count) LIST_%s.cursor = LIST_%s.count - 1;\n", args[1], args[1], args[1], args[1]); 
        }
    } else if (strcmp(cmd, "KEYWAIT") == 0) { 
        if (arg_count == 1) fprintf(out, "    VAR_%s = mc_get_key();\n", args[0]);
    } else if (strcmp(cmd, "GET") == 0) {
        if (arg_count == 3 && strcmp(args[0], "ITEM_ISDIR") == 0) {
            fprintf(out, "    VAR_%s = LIST_%s.is_dir[LIST_%s.cursor];\n", args[2], args[1], args[1]);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: %s <script.mc>\n", argv[0]); return 1; }
    
    FILE *in = fopen(argv[1], "r");
    if (!in) { perror("File open error"); return 1; }
    out = fopen("out.c", "w");
    if (!out) { perror("Output error"); return 1; }

    write_header(); 
    char line[MAX_LINE_LEN];
    int in_c_block = 0;
    
    while (fgets(line, sizeof(line), in)) {
        if (strstr(line, "{{")) { in_c_block = 1; continue; }
        if (strstr(line, "}}")) { in_c_block = 0; continue; }
        if (in_c_block) fprintf(out, "%s", line);
        else parse_line(line);
    }
    
    write_footer();
    write_runtime_functions();
    fclose(in); fclose(out);
    
    printf("Compiling to binary...\n");
    int ret = system("gcc out.c -o app");
    if (ret == 0) {
        printf("Done! Run ./app\n");
    } else {
        printf("Compilation failed. (gcc returned %d). The generated code is in out.c\n", ret);
    }
    return 0;
}
