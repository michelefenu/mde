/* main — Entry point: locale init, editor setup, event loop. */
#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include "editor.h"
#include "buffer.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    Editor ed;
    editor_init(&ed);

    if (argc >= 2) {
        editor_open(&ed, argv[1]);
    } else if (!isatty(STDIN_FILENO)) {
        /* Piped input: load content then redirect STDIN_FILENO to the tty */
        buffer_load_fp(&ed.buf, stdin);
        ed.dirty = 0;

        int tty_fd = open("/dev/tty", O_RDONLY);
        if (tty_fd < 0) {
            perror("mde: cannot open /dev/tty");
            editor_free(&ed);
            return 1;
        }
        dup2(tty_fd, STDIN_FILENO);
        close(tty_fd);
    }

    editor_run(&ed);
    editor_free(&ed);

    return 0;
}
