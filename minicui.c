#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_LINE_LEN 1024
#define MAX_LIST_ITEMS 100
#define MAX_PATH_LEN 256
#define MAX_ARGS 20
#define MAX_SCRIPT_LINES 2048
#define MAX_FUNCTIONS 50
#define MAX_WHILE_NESTING_DEPTH 50

// --- MiniCUI Data Structures ---
typedef struct {
    char items[MAX_LIST_ITEMS][MAX_PATH_LEN];
    int is_dir[MAX_LIST_ITEMS];
    int count;
    int cursor;
} CUI_List;

typedef struct {
    char name[100];
    int start_line;
    int end_line;
} McFunction;

// --- Global Variables ---
FILE *out;
char* script_lines[MAX_SCRIPT_LINES];
int line_count = 0;
McFunction functions[MAX_FUNCTIONS];
int function_count = 0;
int while_label_stack[MAX_WHILE_NESTING_DEPTH];
int while_stack_ptr = 0;
int while_label_counter = 0;

// ===================================================
// Code Generation - Runtime
// ===================================================

void write_runtime_functions() {
    fprintf(out, "%s",
        "void mc_cls() { printf(\"\\033[2J\"); }\n"
        "void mc_pos(int x, int y) { printf(\"\\033[%d;%dH\", y, x); }\n"
        "void mc_color(int c) { printf(\"\\033[%dm\", c); }\n"
        "void mc_reset() { printf(\"\\033[0m\"); }\n"
        "void mc_exit_prog() { mc_pos(1, 25); mc_reset(); exit(0); }\n"
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
        "    if (c == 27) {\n"
        "        char seq[2];\n"
        "        if (read(STDIN_FILENO, &seq[0], 1) == 1) {\n"
        "            if (seq[0] == '[') {\n"
        "                if (read(STDIN_FILENO, &seq[1], 1) == 1) {\n"
        "                    switch (seq[1]) {\n"
        "                        case 'A': c = 1000; break; /* UP */\n"
        "                        case 'B': c = 1001; break; /* DOWN */\n"
        "                        case 'D': c = 1002; break; /* LEFT */\n"
        "                        case 'C': c = 1003; break; /* RIGHT */\n"
        "                        default: c = 27; break; /* Unknown sequence, treat as ESC */\n"
        "                    }\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "    }\n"
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
        "void mc_input(int x, int y, int max_len, char* dest) {\n"
        "    mc_pos(x, y);\n"
        "    mc_color(47); mc_color(30);\n"
        "    char buffer[256] = {0};\n"
        "    for(int i=0; i<max_len; i++) printf(\" \");\n"
        "    mc_pos(x, y);\n"
        "    fflush(stdout);\n"
        "    struct termios orig_termios, raw;\n"
        "    tcgetattr(STDIN_FILENO, &orig_termios);\n"
        "    raw = orig_termios; raw.c_lflag |= (ECHO | ICANON);\n"
        "    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);\n"
        "    fgets(buffer, max_len, stdin);\n"
        "    buffer[strcspn(buffer, \"\\n\")] = 0;\n"
        "    strcpy(dest, buffer);\n"
        "    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);\n"
        "    mc_reset();\n"
        "}\n"
    );
}

// ===================================================
// Code Generation - Boilerplate
// ===================================================

void write_header() {
    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n#include <unistd.h>\n#include <string.h>\n#include <ctype.h>\n#include <dirent.h>\n#include <termios.h>\n#include <sys/stat.h>\n#include <time.h>\n\n");
    fprintf(out, "typedef struct { char items[%d][%d]; int is_dir[%d]; int count; int cursor; } CUI_List;\n", MAX_LIST_ITEMS, MAX_PATH_LEN, MAX_LIST_ITEMS);
    
    fprintf(out, "CUI_List LIST_L1 = {0}, LIST_L2 = {0};\n");
    fprintf(out, "\n// --- MiniCUI Global Variables ---\n");
    for (char c = 'A'; c <= 'R'; c++) fprintf(out, "long VAR_%c = 0; ", c);
    for (char c = 'T'; c <= 'Z'; c++) fprintf(out, "long VAR_%c = 0; ", c);
    fprintf(out, "char VAR_S[256] = {0}; char VAR_S2[256] = {0}; char VAR_S3[256] = {0};\n\n");

    fprintf(out, "// --- MiniCUI Runtime Function Prototypes ---\n");
    fprintf(out, "void mc_cls();\nvoid mc_pos(int x, int y);\nvoid mc_color(int c);\nvoid mc_reset();\nvoid mc_sleep(int ms);\nvoid mc_box(int x, int y, int w, int h);\nvoid mc_center(int y, char* str);\nint mc_get_key();\nvoid mc_load_dir(CUI_List *list);\nvoid mc_render_list(CUI_List *list, int x, int y, int h);\nvoid mc_input(int x, int y, int max_len, char* dest);\nvoid mc_exit_prog();\n\n");
}

void write_function_prototypes() {
    fprintf(out, "// --- User Function Prototypes ---\n");
    for (int i = 0; i < function_count; i++) {
        fprintf(out, "void func_%s();\n", functions[i].name);
    }
    fprintf(out, "\n");
}

void write_main_entry() {
    fprintf(out, "int main() {\n");
    fprintf(out, "    srand(time(NULL));\n");
    fprintf(out, "    mc_cls();\n");
}

void write_main_exit() {
    fprintf(out, "MC_END_LABEL:\n");
    fprintf(out, "    mc_exit_prog();\n");
    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n\n");
}

// ===================================================
// Parser
// ===================================================

int is_numeric(const char *s) {
    if (s == NULL || *s == '\0') return 0;
    // 負の数に対応
    if (*s == '-') s++;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

const char* get_c_var_name(const char* arg) {
    if (!arg) return "0";
    static char buffer[MAX_PATH_LEN];
    
    // 文字列リテラルはそのまま返す
    if (arg[0] == '"') return arg;
    
    // 変数名 (VAR_A, VAR_Sなど) の処理
    if (isalpha(arg[0]) && isupper(arg[0])) {
        if (strlen(arg) == 1 && arg[0] != 'S') { 
            snprintf(buffer, sizeof(buffer), "VAR_%s", arg);
            return buffer;
        } else if (arg[0] == 'S' && (strlen(arg) == 1 || (strlen(arg) > 1 && isdigit(arg[1]) && strlen(arg) <= 2))) { 
            snprintf(buffer, sizeof(buffer), "VAR_%s", arg);
            return buffer;
        }
    }
    
    // 数値リテラルはそのまま返す
    if (is_numeric(arg)) return arg;
    
    // それ以外の引数（未定義、ラベルなど）は一旦文字列リテラルとして扱う
    snprintf(buffer, sizeof(buffer), "\"%s\"", arg);
    return buffer; 
}

void parse_line(char *line) {
    char clean_line[MAX_LINE_LEN] = {0};
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';
    strncpy(clean_line, line, MAX_LINE_LEN -1);
    clean_line[strcspn(clean_line, "\n")] = 0;
    
    char *p = clean_line;
    while(isspace(*p)) p++;
    if (*p == '\0') return;

    char *cmd = p;
    while(*p && !isspace(*p)) p++;
    if (*p) { *p = '\0'; p++; }

    // ラベル処理
    if (cmd[strlen(cmd)-1] == ':') {
        cmd[strlen(cmd)-1] = '\0';
        fprintf(out, "%s:\n", cmd);
        return;
    }

    char *args[MAX_ARGS] = { NULL };
    int arg_count = 0;
    // 引数解析
    while(*p && arg_count < MAX_ARGS) {
        while(isspace(*p)) p++;
        if (*p == '\0') break;
        // 文字列リテラル (") の処理
        if (*p == '"') {
            args[arg_count++] = p;
            p++;
            while(*p && *p != '"') p++;
            if (*p == '"') p++;
            char* end_quote = p;
            while(*end_quote && !isspace(*end_quote)) end_quote++;
            if (*end_quote) { *end_quote = '\0'; p = end_quote + 1; } else { p = end_quote; }
        } 
        // 1文字リテラル (') の処理
        else if (*p == '\'') { 
            args[arg_count++] = p;
            p++;
            if(*p && *p != '\'') p++;
            if (*p == '\'') p++;
            char* end_quote = p;
            while(*end_quote && !isspace(*end_quote)) end_quote++;
            if (*end_quote) { *end_quote = '\0'; p = end_quote + 1; } else { p = end_quote; }
        } 
        // 通常引数 (変数、数値) の処理
        else {
            args[arg_count++] = p;
            while(*p && !isspace(*p)) p++;
            if (*p) { *p = '\0'; p++; }
        }
    }

    // --- コマンド処理 ---
    
    // PRINT
    if (strcmp(cmd, "PRINT") == 0 && arg_count >= 1) {
        const char* var = get_c_var_name(args[0]);
        if (strstr(var, "VAR_S") == var) fprintf(out, "    printf(\"%%s\\n\", %s);\n", var);
        else if (strstr(var, "VAR_") == var) fprintf(out, "    printf(\"%%ld\\n\", %s);\n", var);
        else fprintf(out, "    printf(\"%%s\\n\", %s);\n", var);
    }
    // CALL
    else if (strcmp(cmd, "CALL") == 0 && arg_count == 1) { fprintf(out, "    func_%s();\n", args[0]); }
    // SLEEP
    else if (strcmp(cmd, "SLEEP") == 0 && arg_count == 1) { fprintf(out, "    mc_sleep(%s);\n", get_c_var_name(args[0])); }
    // CENTER
    else if (strcmp(cmd, "CENTER") == 0 && arg_count >= 2) {
        const char* y = get_c_var_name(args[0]);
        const char* text = get_c_var_name(args[1]);
        if (strstr(text, "VAR_") == text && strstr(text, "VAR_S") != text) {
            fprintf(out, "    { char temp[32]; sprintf(temp, \"%%ld\", %s); mc_center(%s, temp); }\n", text, y);
        } else {
            fprintf(out, "    mc_center(%s, %s);\n", y, text);
        }
    }
    // SET (重要修正ポイント)
    else if (strcmp(cmd, "SET") == 0 && arg_count == 2) {
        const char* var_l = get_c_var_name(args[0]);
        const char* val_r = get_c_var_name(args[1]);
        
        // 文字列変数への代入はstrcpyを使用
        if (strstr(var_l, "VAR_S") == var_l) {
            fprintf(out, "    strcpy(%s, %s);\n", var_l, val_r); 
        } 
        // 数値変数への代入は直接代入を使用
        else {
            fprintf(out, "    %s = %s;\n", var_l, val_r); 
        }
    }
    // STRCAT
    else if (strcmp(cmd, "STRCAT") == 0 && arg_count == 2) {
        const char* dest_var_cname = get_c_var_name(args[0]);
        const char* src_cname = get_c_var_name(args[1]);
        
        if (strstr(dest_var_cname, "VAR_S") != dest_var_cname) {
             fprintf(out, "    // STRCAT ERROR: Destination must be S, S2, or S3.\n");
             return;
        }
        if (strstr(src_cname, "VAR_") == src_cname && strstr(src_cname, "VAR_S") != src_cname) {
            fprintf(out, "    { char temp[32]; sprintf(temp, \"%%ld\", %s); strcat(%s, temp); }\n", src_cname, dest_var_cname);
        } else {
            fprintf(out, "    strcat(%s, %s);\n", dest_var_cname, src_cname);
        }
    }
    // ADD/SUB/MUL/DIV/RAND
    else if (strcmp(cmd, "ADD") == 0 && arg_count == 2) { fprintf(out, "    %s += %s;\n", get_c_var_name(args[0]), get_c_var_name(args[1])); }
    else if (strcmp(cmd, "SUB") == 0 && arg_count == 2) { fprintf(out, "    %s -= %s;\n", get_c_var_name(args[0]), get_c_var_name(args[1])); }
    else if (strcmp(cmd, "MUL") == 0 && arg_count == 2) { fprintf(out, "    %s *= %s;\n", get_c_var_name(args[0]), get_c_var_name(args[1])); }
    else if (strcmp(cmd, "DIV") == 0 && arg_count == 2) { fprintf(out, "    if(%s != 0) %s /= %s;\n", get_c_var_name(args[1]), get_c_var_name(args[0]), get_c_var_name(args[1])); }
    else if (strcmp(cmd, "RAND") == 0 && arg_count == 2) { fprintf(out, "    %s = rand() %% %s;\n", get_c_var_name(args[0]), get_c_var_name(args[1])); }

    // INPUT
    else if (strcmp(cmd, "INPUT") == 0 && arg_count == 3) { fprintf(out, "    mc_input(%s, %s, %s, VAR_S);\n", get_c_var_name(args[0]), get_c_var_name(args[1]), get_c_var_name(args[2])); }
    
    // WHILE / ENDWHILE
    else if (strcmp(cmd, "WHILE") == 0 && arg_count == 3) {
        int label_id = ++while_label_counter;
        while_label_stack[while_stack_ptr++] = label_id;
        
        fprintf(out, "WHILE_START_%d:\n", label_id);
        const char* var = get_c_var_name(args[0]);
        const char* op = args[1];
        const char* val = get_c_var_name(args[2]);
        const char* c_op = (strcmp(op, "=") == 0) ? "==" : op;
        
        if (strstr(var, "VAR_S") == var) {
            fprintf(out, "    if (!(strcmp(%s, %s) %s 0)) goto WHILE_END_%d;\n", var, val, c_op, label_id);
        } else {
            fprintf(out, "    if (!(%s %s %s)) goto WHILE_END_%d;\n", var, c_op, val, label_id);
        }
    }
    else if (strcmp(cmd, "ENDWHILE") == 0) {
        if (while_stack_ptr > 0) {
            int label_id = while_label_stack[--while_stack_ptr];
            fprintf(out, "    goto WHILE_START_%d;\n", label_id);
            fprintf(out, "WHILE_END_%d:\n", label_id);
        }
    }
    // IF GOTO
    else if (strcmp(cmd, "IF") == 0 && arg_count == 5 && strcmp(args[3], "GOTO") == 0) {
        const char* var = get_c_var_name(args[0]);
        const char* op = args[1];
        const char* val_arg = args[2];
        const char* c_op = (strcmp(op, "=") == 0) ? "==" : op; 

        char temp_char_literal[5];
        const char* val_c_code;
        
        if (val_arg[0] == '\'' && val_arg[strlen(val_arg)-1] == '\'' && strlen(val_arg) == 3) {
            snprintf(temp_char_literal, sizeof(temp_char_literal), "'%c'", val_arg[1]);
            val_c_code = temp_char_literal;
        } else {
            val_c_code = get_c_var_name(val_arg);
        }
        
        if (strstr(var, "VAR_S") == var) {
            fprintf(out, "    if (strcmp(%s, %s) %s 0) goto %s;\n", var, val_c_code, c_op, args[4]);
        } else {
            fprintf(out, "    if (%s %s %s) goto %s;\n", var, c_op, val_c_code, args[4]);
        }
    }
    // EXIT
    else if (strcmp(cmd, "EXIT") == 0) { fprintf(out, "    mc_exit_prog();\n"); } 
    
    // GOTO
    else if (strcmp(cmd, "GOTO") == 0 && arg_count == 1) { fprintf(out, "    goto %s;\n", args[0]); }
    // POS/COLOR/BOX/CLEAR
    else if (strcmp(cmd, "POS") == 0 && arg_count == 2) { fprintf(out, "    mc_pos(%s, %s);\n", get_c_var_name(args[0]), get_c_var_name(args[1])); }
    else if (strcmp(cmd, "COLOR") == 0 && arg_count == 1) { fprintf(out, "    mc_color(%s);\n", get_c_var_name(args[0])); }
    else if (strcmp(cmd, "BOX") == 0 && arg_count == 4) { fprintf(out, "    mc_box(%s, %s, %s, %s);\n", get_c_var_name(args[0]), get_c_var_name(args[1]), get_c_var_name(args[2]), get_c_var_name(args[3])); }
    else if (strcmp(cmd, "CLEAR") == 0) { fprintf(out, "    mc_cls();\n"); }
    // LIST/CURSOR/GET
    else if (strcmp(cmd, "LIST") == 0 && arg_count == 2 && strcmp(args[0], "LOAD") == 0) { fprintf(out, "    mc_load_dir(&LIST_%s);\n", args[1]); }
    else if (strcmp(cmd, "LIST") == 0 && arg_count == 5 && strcmp(args[0], "RENDER") == 0) { fprintf(out, "    mc_render_list(&LIST_%s, %s, %s, %s);\n", args[1], get_c_var_name(args[2]), get_c_var_name(args[3]), get_c_var_name(args[4])); }
    else if (strcmp(cmd, "CURSOR") == 0 && arg_count == 3 && strcmp(args[0], "ADJ") == 0) { fprintf(out, "    LIST_%s.cursor += %s;\n", args[1], get_c_var_name(args[2])); }
    else if (strcmp(cmd, "CURSOR") == 0 && arg_count == 2 && strcmp(args[0], "LIMIT") == 0) {
        fprintf(out, "    if (LIST_%s.cursor < 0) LIST_%s.cursor = 0;\n", args[1], args[1]);
        fprintf(out, "    if (LIST_%s.cursor >= LIST_%s.count) LIST_%s.cursor = LIST_%s.count - 1;\n", args[1], args[1], args[1], args[1]);
    }
    else if (strcmp(cmd, "KEYWAIT") == 0 && arg_count == 1) { fprintf(out, "    VAR_%s = mc_get_key();\n", args[0]); }
    else if (strcmp(cmd, "GET") == 0 && arg_count == 3 && strcmp(args[0], "ITEM_ISDIR") == 0) { fprintf(out, "    VAR_%s = LIST_%s.is_dir[LIST_%s.cursor];\n", args[2], args[1], args[1]); }
    else if (strcmp(cmd, "GET") == 0 && arg_count == 3 && strcmp(args[0], "ITEM_NAME") == 0) { fprintf(out, "    strcpy(VAR_%s, LIST_%s.items[LIST_%s.cursor]);\n", args[2], args[1], args[1]); }
    // CD/SYSTEM
    else if (strcmp(cmd, "CD") == 0 && arg_count == 1) { fprintf(out, "    chdir(%s);\n", get_c_var_name(args[0])); }
    else if (strcmp(cmd, "SYSTEM") == 0 && arg_count > 0) { fprintf(out, "    mc_pos(1, 25); system(%s);\n", get_c_var_name(args[0])); }
}

// ===================================================
// Compiler Main Logic
// ===================================================

void discover_functions() {
    int in_func = 0;
    for (int i = 0; i < line_count; i++) {
        char clean_line[MAX_LINE_LEN];
        strncpy(clean_line, script_lines[i], MAX_LINE_LEN - 1);
        clean_line[strcspn(clean_line, "\n")] = 0;
        char *p = clean_line;
        while(isspace(*p)) p++;
        
        char *cmd = p;
        while(*p && !isspace(*p)) p++;
        if(*p) {*p = '\0'; p++;}

        if (strcmp(cmd, "FUNC") == 0) {
            if (function_count < MAX_FUNCTIONS) {
                while(isspace(*p)) p++;
                char* func_name = p;
                while(*p && !isspace(*p)) p++;
                *p = '\0';
                
                strcpy(functions[function_count].name, func_name);
                functions[function_count].start_line = i;
                in_func = 1;
            }
        } else if (strcmp(cmd, "ENDFUNC") == 0) {
            if (in_func) {
                functions[function_count].end_line = i;
                function_count++;
                in_func = 0;
            }
        }
    }
}

int is_in_any_function(int line_num) {
    for (int i = 0; i < function_count; i++) {
        if (line_num >= functions[i].start_line && line_num <= functions[i].end_line) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: %s <script.mc>\n", argv[0]); return 1; }
    
    FILE *in = fopen(argv[1], "r");
    if (!in) { perror("File open error"); return 1; }
    
    // Read all lines into memory
    char buffer[MAX_LINE_LEN];
    while (fgets(buffer, sizeof(buffer), in) && line_count < MAX_SCRIPT_LINES) {
        script_lines[line_count++] = strdup(buffer);
    }
    fclose(in);

    out = fopen("out.c", "w");
    if (!out) { perror("Output error"); return 1; }

    // --- Compilation Passes ---
    discover_functions();

    write_header();
    write_function_prototypes();
    
    // 1. Write main function body
    write_main_entry();
    while_stack_ptr = 0;
    for (int i = 0; i < line_count; i++) {
        if (!is_in_any_function(i)) {
            parse_line(script_lines[i]);
        }
    }
    if (while_stack_ptr > 0) {
        printf("Compiler Error: Unclosed WHILE statement in main code block.\n");
        fclose(out);
        return 1;
    }
    write_main_exit();

    // 2. Write user function bodies
    for (int i = 0; i < function_count; i++) {
        fprintf(out, "void func_%s() {\n", functions[i].name);
        while_stack_ptr = 0;
        for (int j = functions[i].start_line + 1; j < functions[i].end_line; j++) {
            parse_line(script_lines[j]);
        }
        if (while_stack_ptr > 0) {
            printf("Compiler Error: Unclosed WHILE statement in function '%s'.\n", functions[i].name);
            fclose(out);
            return 1;
        }
        fprintf(out, "}\n\n");
    }

    // 3. Write runtime C functions
    write_runtime_functions();
    
    fclose(out);
    
    // Free memory
    for (int i = 0; i < line_count; i++) {
        free(script_lines[i]);
    }
    
    printf("Compiling to binary...\n");
    int ret = system("gcc out.c -o app");
    if (ret == 0) {
        printf("Done! Run ./app\n");
    } else {
        printf("Compilation failed. (gcc returned %d). The generated code is in out.c\n", ret);
    }
    return 0;
}
