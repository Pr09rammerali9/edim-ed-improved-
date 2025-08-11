#include <ncurses.h>                              #include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define CTRL(c) ((c) & 0x1f)
#define MAX_LINE_LEN 256
#define TAB_STOP 4

typedef struct Line {
    char *text;
    int len;
    struct Line *next;
    struct Line *prev;
} Line;

Line *head = NULL;
Line *tail = NULL;
Line *current_line = NULL;
int cursor_x = 0;
int cursor_y = 0;
int offset_y = 0;
char *filename = NULL;
int status_msg_timer = 0;
char status_msg[MAX_LINE_LEN] = "";

void free_buffer() {
    Line *line = head;
    while (line != NULL) {
        Line *temp = line;
        line = line->next;
        free(temp->text);
        free(temp);
    }
    head = tail = current_line = NULL;
}

Line* create_line(const char *content) {
    Line *new_line = (Line*)malloc(sizeof(Line));
    if (content) {
        new_line->len = strlen(content);
        new_line->text = strdup(content);
    } else {
        new_line->len = 0;
        new_line->text = (char*)calloc(1, sizeof(char));
    }
    new_line->next = NULL;
    new_line->prev = NULL;
    return new_line;
}

void append_line(const char *content) {
    Line *new_line = create_line(content);
    if (head == NULL) {
        head = tail = current_line = new_line;
    } else {
        tail->next = new_line;
        new_line->prev = tail;
        tail = new_line;
    }
}

void insert_char(int ch) {
    if (!current_line) {
        append_line(NULL);
        current_line = head;
    }

    current_line->text = (char*)realloc(current_line->text, current_line->len + 2);
    memmove(&current_line->text[cursor_x + 1], &current_line->text[cursor_x], current_line->len - cursor_x + 1);
    current_line->text[cursor_x] = ch;
    current_line->len++;
    cursor_x++;
}

void delete_char() {
    if (!current_line || cursor_x >= current_line->len) return;

    memmove(&current_line->text[cursor_x], &current_line->text[cursor_x + 1], current_line->len - cursor_x);
    current_line->len--;
    current_line->text = (char*)realloc(current_line->text, current_line->len + 1);
}

void insert_newline() {
    if (!current_line) {
        append_line(NULL);
        current_line = head;
        return;
    }

    char *remaining_text = strdup(&current_line->text[cursor_x]);
    current_line->text[cursor_x] = '\0';
    current_line->len = cursor_x;

    Line *new_line = create_line(remaining_text);
    free(remaining_text);

    new_line->next = current_line->next;
    new_line->prev = current_line;
    if (current_line->next) {
        current_line->next->prev = new_line;
    } else {
        tail = new_line;
    }
    current_line->next = new_line;

    current_line = new_line;
    cursor_y++;
    cursor_x = 0;
}

void insert_tab() {
    int tab_spaces = TAB_STOP - (cursor_x % TAB_STOP);
    for (int i = 0; i < tab_spaces; i++) {
        insert_char(' ');
    }
}

void open_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        snprintf(status_msg, MAX_LINE_LEN, "New file: %s", path);
        status_msg_timer = 50;
        append_line(NULL);
        current_line = head;
        filename = strdup(path);
        return;
    }

    free_buffer();
    char *line_buf = NULL;
    size_t line_len = 0;
    ssize_t read;

    while ((read = getline(&line_buf, &line_len, fp)) != -1) {
        if (read > 0 && line_buf[read - 1] == '\n') {
            line_buf[read - 1] = '\0';
        }
        append_line(line_buf);
    }
    if (head == NULL) {
        append_line(NULL);
    }
    current_line = head;

    fclose(fp);
    free(line_buf);
    filename = strdup(path);
    snprintf(status_msg, MAX_LINE_LEN, "Opened file: %s", filename);
    status_msg_timer = 50;
}

void save_file() {
    if (!filename) {
        snprintf(status_msg, MAX_LINE_LEN, "No filename specified.");
        status_msg_timer = 50;
        return;
    }

    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        snprintf(status_msg, MAX_LINE_LEN, "Error: Could not save file!");
        status_msg_timer = 50;
        return;
    }

    Line *line = head;
    while (line != NULL) {
        fprintf(fp, "%s\n", line->text);
        line = line->next;
    }

    fclose(fp);
    snprintf(status_msg, MAX_LINE_LEN, "File saved successfully!");
    status_msg_timer = 50;
}

void draw_statusbar(int max_y, int max_x) {
    attron(A_REVERSE);
    mvprintw(max_y - 1, 0, "%.*s", max_x, status_msg);
    clrtoeol();

    char file_info[MAX_LINE_LEN];
    snprintf(file_info, MAX_LINE_LEN, "[ %s ] L: %d, C: %d",
             filename ? filename : "newfile", cursor_y + 1, cursor_x + 1);
    mvprintw(max_y - 1, max_x - strlen(file_info), "%s", file_info);

    attroff(A_REVERSE);
}

void draw_screen() {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int y = 0;
    Line *line = head;

    for (int i = 0; i < offset_y && line != NULL; i++) {
        line = line->next;
    }

    for (; line != NULL && y < max_y - 1; y++, line = line->next) {
        mvprintw(y, 0, "%.*s", max_x, line->text);
    }

    draw_statusbar(max_y, max_x);

    int current_y_on_screen = cursor_y - offset_y;
    move(current_y_on_screen, cursor_x);
    refresh();
}

void handle_cursor_movement(int ch) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    switch (ch) {
        case KEY_UP:
            if (current_line && current_line->prev) {
                current_line = current_line->prev;
                cursor_y--;
            }
            break;
        case KEY_DOWN:
            if (current_line && current_line->next) {
                current_line = current_line->next;
                cursor_y++;
            }
            break;
        case KEY_LEFT:
            if (cursor_x > 0) {
                cursor_x--;
            }
            break;
        case KEY_RIGHT:
            if (current_line && cursor_x < current_line->len) {
                cursor_x++;
            }
            break;
    }

    if (cursor_y < offset_y) offset_y = cursor_y;
    if (cursor_y >= offset_y + max_y - 1) offset_y = cursor_y - max_y + 2;

    if (current_line && cursor_x > current_line->len) {
        cursor_x = current_line->len;
    }
}

void init_editor() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
    }
}

void editor_loop() {
    int ch;
    while (1) {
        draw_screen();
        if (status_msg_timer > 0) status_msg_timer--;

        ch = getch();
        if (ch == CTRL('q')) {
            break;
        } else if (ch == CTRL('s')) {
            save_file();
        } else if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT) {
            handle_cursor_movement(ch);
        } else if (ch == '\n') {
            insert_newline();
        } else if (ch == '\t') {
            insert_tab();
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (cursor_x > 0) {
                cursor_x--;
                delete_char();
            } else if (current_line && current_line->prev) {
                Line *prev_line = current_line->prev;
                int old_len = prev_line->len;

                prev_line->text = (char*)realloc(prev_line->text, old_len + current_line->len + 1);
                memcpy(&prev_line->text[old_len], current_line->text, current_line->len + 1);
                prev_line->len += current_line->len;

                prev_line->next = current_line->next;
                if (current_line->next) {
                    current_line->next->prev = prev_line;
                } else {
                    tail = prev_line;
                }

                free(current_line->text);
                free(current_line);

                current_line = prev_line;
                cursor_y--;
                cursor_x = old_len;
            }
        } else if (isprint(ch)) {
            insert_char(ch);
        }
    }
}

void cleanup() {
    free_buffer();
    if (filename) free(filename);
    endwin();
}

int main(int argc, char *argv[]) {
    init_editor();

    if (argc > 1) {
        open_file(argv[1]);
    } else {
        append_line(NULL);
        current_line = head;
        snprintf(status_msg, MAX_LINE_LEN, "Type Ctrl+S to save, Ctrl+Q to quit.");
    }

    editor_loop();
    cleanup();
    return 0;
}