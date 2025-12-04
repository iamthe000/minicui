#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_LINE_LEN 256
#define MAX_VARS 26
#define MAX_STACK 100 
#define MAX_VAR_LEN 64 // 変数に格納できる文字列の最大長

// すべての変数は文字列として保存
char str_variables[MAX_VARS][MAX_VAR_LEN];

// コールスタック (関数呼び出し用)
long call_stack[MAX_STACK];
int stack_ptr = 0;

void clear_screen() { printf("\033[2J"); }
void set_cursor(int x, int y) { printf("\033[%d;%dH", y, x); }
void set_color(int color_code) { printf("\033[%dm", color_code); }
void reset_color() { printf("\033[0m"); }

void error(const char *msg) {
    fprintf(stderr, "\033[31mError: %s\033[0m\n", msg);
    exit(1);
}

int get_var_index(char *token) {
    if (!token || strlen(token) != 1 || !isupper(token[0])) {
        return -1;
    }
    return token[0] - 'A';
}

// 数値を取得 (CALC, IF用)
long get_num_value(char *token) {
    int idx = get_var_index(token);
    if (idx >= 0) {
        // 変数の内容を数値に変換
        return atol(str_variables[idx]);
    }
    // 数値リテラルとして変換
    return atol(token);
}

// 引用符を考慮した引数解析
char **parse_args(char *line, int *argc) {
    static char *argv_storage[30]; 
    char *p = line;
    *argc = 0;

    while (*p != '\0' && *argc < 30) {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;

        argv_storage[*argc] = p;

        if (*p == '"') {
            p++; 
            while (*p != '\0' && *p != '"') p++;
            if (*p == '"') {
                *p = '\0';
                p++;
            }
        } else {
            while (*p != '\0' && !isspace((unsigned char)*p)) p++;
        }
        
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
        (*argc)++;
    }
    return argv_storage;
}

// ジャンプ関数 (省略)
void jump_to_label(FILE *fp, const char *target_label) {
    rewind(fp);
    char line[MAX_LINE_LEN];
    char label_def[MAX_LINE_LEN];
    sprintf(label_def, "%s:", target_label);

    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, label_def) == 0) {
            return;
        }
    }
    error("Label not found.");
}


void run_script(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) error("Could not open file.");

    char line[MAX_LINE_LEN];
    
    // 初期化 (すべての変数)
    for (int i = 0; i < MAX_VARS; i++) {
        str_variables[i][0] = '\0';
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        line[strcspn(line, "\n")] = 0;
        
        char *line_content = line;
        while (isspace((unsigned char)*line_content)) line_content++;
        if (strlen(line_content) == 0) continue;

        int argc = 0;
        char **argv = parse_args(line_content, &argc);
        
        if (argc == 0) continue;
        char *cmd = argv[0];

        if (cmd[strlen(cmd)-1] == ':') continue;

        // --- コマンド処理 ---

        if (strcmp(cmd, "PRINT") == 0) {
            for (int i = 1; i < argc; i++) {
                char *arg = argv[i];
                int idx = get_var_index(arg);
                
                if (arg[0] == '"') {
                    // 引用符付き文字列リテラル
                    printf("%s", arg + 1); 
                } else if (idx >= 0) {
                    // 変数
                    printf("%s", str_variables[idx]);
                } else {
                    // 引用符なし、変数名でもないトークンはそのまま出力（0/1/-1表示を防ぐ）
                    printf("%s", arg);
                }
                printf(" "); 
            }
            printf("\n");

        } else if (strcmp(cmd, "SET") == 0) {
            if (argc < 3) error("SET: Not enough arguments");
            char *var_name = argv[1];
            char *val_str = argv[2];
            int idx = get_var_index(var_name);
            if (idx < 0) error("Invalid variable for SET");
            
            // すべて文字列として代入
            if (val_str[0] == '"') {
                 strncpy(str_variables[idx], val_str + 1, MAX_VAR_LEN - 1);
            } else {
                 strncpy(str_variables[idx], val_str, MAX_VAR_LEN - 1);
            }
            str_variables[idx][MAX_VAR_LEN - 1] = '\0';

        } else if (strcmp(cmd, "CALC") == 0) {
            if (argc < 4) error("CALC: Not enough arguments");
            char *var_name = argv[1];
            char *op = argv[2];
            char *val_str = argv[3];
            int idx = get_var_index(var_name);
            if (idx < 0) error("Invalid variable for CALC");
            
            // 文字列を数値に変換して計算
            long target_val = get_num_value(var_name);
            long val = get_num_value(val_str);
            
            if (strcmp(op, "+") == 0) target_val += val;
            else if (strcmp(op, "-") == 0) target_val -= val;
            else if (strcmp(op, "*") == 0) target_val *= val;
            else if (strcmp(op, "/") == 0) {
                if (val == 0) error("Division by zero");
                target_val /= val;
            }
            else if (strcmp(op, "%") == 0) target_val %= val;

            // 結果を再び文字列として変数に保存
            snprintf(str_variables[idx], MAX_VAR_LEN, "%ld", target_val);

        } else if (strcmp(cmd, "INPUT") == 0) {
            if (argc < 2) error("INPUT: Variable missing");
            char *var_name = argv[1];
            char *prompt = (argc >= 3 && argv[2][0] == '"') ? argv[2] : NULL;
            int idx = get_var_index(var_name);
            if (idx < 0) error("Invalid variable for INPUT");
            
            if (prompt) {
                 printf("%s", prompt + 1);
            } else {
                printf("? ");
            }

            // 文字列として入力を受け付ける (fgetsを使用)
            if (fgets(str_variables[idx], MAX_VAR_LEN, stdin) == NULL) {
                error("Input error");
            }
            str_variables[idx][strcspn(str_variables[idx], "\n")] = '\0';
            
        // IF文 (数値のみ対応)
        } else if (strcmp(cmd, "IF") == 0) {
            if (argc < 6) error("IF: Not enough arguments");
            // すべての引数を数値として評価
            long lhs = get_num_value(argv[1]);
            char *op = argv[2];
            long rhs = get_num_value(argv[3]);
            char *goto_kw = argv[4];
            char *label = argv[5];
            
            int condition = 0;
            if (strcmp(op, "==") == 0) condition = (lhs == rhs);
            else if (strcmp(op, "!=") == 0) condition = (lhs != rhs);
            else if (strcmp(op, ">") == 0) condition = (lhs > rhs);
            else if (strcmp(op, "<") == 0) condition = (lhs < rhs);
            if (condition) {
                if (label && strcmp(goto_kw, "GOTO") == 0) jump_to_label(fp, label);
                else if (label && strcmp(goto_kw, "CALL") == 0) {
                     if (stack_ptr >= MAX_STACK) error("Stack overflow");
                     call_stack[stack_ptr++] = ftell(fp);
                     jump_to_label(fp, label);
                }
            }

        } else if (strcmp(cmd, "GOTO") == 0) {
            if (argc < 2) error("GOTO: Label missing");
            jump_to_label(fp, argv[1]);

        } else if (strcmp(cmd, "CALL") == 0) {
            if (argc < 2) error("CALL: Label missing");
            char *label = argv[1];
            if (stack_ptr >= MAX_STACK) error("Stack overflow");
            
            call_stack[stack_ptr++] = ftell(fp);
            jump_to_label(fp, label);

        } else if (strcmp(cmd, "RET") == 0) {
            if (stack_ptr <= 0) error("Stack underflow (Return without Call)");
            
            long ret_pos = call_stack[--stack_ptr];
            fseek(fp, ret_pos, SEEK_SET);

        } else if (strcmp(cmd, "WAIT") == 0) {
            if (argc < 2) error("WAIT: Value missing");
            long sec = get_num_value(argv[1]);
            fflush(stdout);
            sleep(sec);

        } else if (strcmp(cmd, "COLOR") == 0) {
            if (argc < 2) error("COLOR: Value missing");
            set_color(get_num_value(argv[1]));
        } else if (strcmp(cmd, "CLEAR") == 0) {
            clear_screen();
        } else if (strcmp(cmd, "POS") == 0) {
            if (argc < 3) error("POS: Coordinates missing");
            long x = get_num_value(argv[1]);
            long y = get_num_value(argv[2]);
            set_cursor(x, y);
        } else if (strcmp(cmd, "EXIT") == 0) {
            break;
        }
    }

    fclose(fp);
    reset_color();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <script_file>\n", argv[0]);
        return 1;
    }
    run_script(argv[1]);
    return 0;
}