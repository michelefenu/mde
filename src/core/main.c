/* main — Entry point: locale init, editor setup, event loop. */
#include <stdio.h>
#include <locale.h>
#include "editor.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    Editor ed;
    editor_init(&ed);

    if (argc >= 2)
        editor_open(&ed, argv[1]);

    editor_run(&ed);
    editor_free(&ed);

    return 0;
}
