#ifndef SHELL_H
#define SHELL_H
#include <kernel/filesystem/Ext2.h>

void initShell();
void shell();
static void command_eval(char *cmd, char *args);

#endif
