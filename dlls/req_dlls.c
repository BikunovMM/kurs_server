/*
*   PROGRAMM COPY ALL REQUIRED DLLS FROM TXT TO THE DESTINATION PATH. 
*       GET ALL REQUIRED DLLS BY "ldd EXECUTABLE.exe > TEXT_FILE.txt" in ucrt64.exe
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif


#define BUFF_LEN 256


void set_console_color (int fg, int bg);
void reset_console_color ();


int main (int argc, char *argv[])
{
    char buffer[BUFF_LEN];
    const char *req_dlls_txt_path = "req_dlls.txt";
    const char *destination_dlls_dir = ".";
    char path[BUFF_LEN];
    char cmd[BUFF_LEN];
    char *f_slash;
    char *f_ext;
    int len;
    int cmd_len;
    int res = 1;
    int all_rows_len = 0;
    int dll_rows_len = 0;
    FILE *file;
    FILE *cmd_file;

    file = fopen (req_dlls_txt_path, "r");
    if (!file) 
        return -1;

    while (fgets (buffer, BUFF_LEN, file)) {
        all_rows_len += 1;
        if (strstr (buffer, "Windows") || strstr (buffer, ".a") || !strstr (buffer, ".dll") || strlen (buffer) < 3) {
            //set_console_color (4, 0);
            //printf ("*%s^\n", buffer);
            //reset_console_color ();
            continue;
        }
        f_slash = strchr (buffer, '/');
        f_ext = strrchr (buffer, '.');

        len = f_ext - f_slash + 4;
        strncpy (path, f_slash, len);
        path[len] = '\0';

        for (int i = 0; i < len; ++i) {
            if (path[i] == '/')
                path[i] = '\\';
        }

        snprintf (cmd, BUFF_LEN, "copy \"C:\\msys64%s\" %s", path, destination_dlls_dir);

        cmd_file = popen (cmd, "w");
        pclose (cmd_file);

        printf ("*%s^\n", cmd);
        dll_rows_len += 1;
    }
    set_console_color (2, 0);
    printf ("\nall_rows: %d, dll_rows: %d\n", all_rows_len, dll_rows_len);
    reset_console_color ();

    fclose (file);

    return 0;
}

void set_console_color (int fg, int bg) 
{
    #ifdef _WIN32
        HANDLE console = GetStdHandle (STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute (console, bg<<4|fg);
    #else
        printf ("\033[%d;%dm", foreground, background);
    #endif
}

void reset_console_color ()
{
    #ifdef _WIN32
        HANDLE console = GetStdHandle (STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute (console, 7);
    #else
        printf ("\033[0m");
    #endif
}