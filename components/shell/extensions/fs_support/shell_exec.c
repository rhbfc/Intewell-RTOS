#include "shell.h"
#include "shell_fs.h"
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <ttos.h>
#include <ttosProcess.h>
#include <unistd.h>

int   exec_command (int argc, const char *argv[])
{
    pid_t    pid;
    int      ret        = 0;
    char    *__envp[]   = { "HOME=/", NULL, NULL };
    Shell   *shell      = shellGetCurrent ();
    ShellFs *shellFs    = shellCompanionGet (shell, SHELL_COMPANION_ID_FS);

    if(access("/bin/sh", X_OK) != 0)
    {
        printf("/bin/sh: %s", strerror(errno));
        return -ENOEXEC;
    }

    char *sh_argv[4] = {"/bin/sh", "-c", NULL, NULL};

    SHELL_ASSERT (shellFs, return -1);

    if (strcmp (argv[0], "exec") == 0)
    {
        if (argc < 2)
        {
            printf ("%s\n", strerror (EINVAL));
            return -EINVAL;
        }
        argv = &argv[1];
        argc--;
    }

    __envp[1] = malloc (strlen ("PWD=") + strlen (shellFs->info.path) + 1);
    if (__envp[1] == NULL)
    {
        printf ("%s\n", strerror (ENOMEM));
        return -ENOMEM;
    }
    strcpy (__envp[1], "PWD=");
    strcat (__envp[1], shellFs->info.path);
    
    asprintf(&sh_argv[2], "%s", argv[0]);
    for(int i = 1; i < argc; i++)
    {
        char * tmparg;
        tmparg = sh_argv[2];
        asprintf(&sh_argv[2], "%s %s", tmparg, argv[i]);
        free(tmparg);
    }

    pid = kernel_execve ("/bin/sh", (const char *const *)sh_argv,
                         (const char *const *)__envp);
                         
    free(sh_argv[2]);
    free (__envp[1]);

    return ret;
}
SHELL_EXPORT_CMD (SHELL_CMD_PERMISSION (0)
                      | SHELL_CMD_TYPE (SHELL_TYPE_CMD_MAIN)
                      | SHELL_CMD_DISABLE_RETURN,
                  exec, exec_command, run program);
