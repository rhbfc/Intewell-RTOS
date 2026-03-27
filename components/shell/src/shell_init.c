#include <errno.h>
#include <fcntl.h>
#include <fs/kpoll.h>
#include <limits.h>
#include <page.h>
#include <shell.h>
#include <shell_fs.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <ttos.h>
#include <ttosMM.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <unistd.h>
#include <version.h>

#undef KLOG_TAG
#define KLOG_TAG "shell"
#include <klog.h>

#if SHELL_USING_LOCK == 1
#define SHELL_COMPANION_ID_MUTEX ('L' | ('O' << 8) | ('C' << 16) | ('K' << 24))
#endif

static Shell g_uart_shell;
ShellFs g_uart_shellFs;
char shellPathBuffer[PATH_MAX] = "/";

/**
 * @brief shell读取数据函数原型
 *
 * @param data shell读取的字符
 * @param len 请求读取的字符数量
 *
 * @return unsigned short 实际读取到的字符数量
 */
static short ttos_shellRead(char *data, unsigned short len)
{
    int readlen = 0;
    int ret;

    struct kpollfd fds;
    fds.pollfd.fd = STDIN_FILENO;
    fds.pollfd.events = POLLIN;
    ret = kpoll(&fds, 1, TTOS_WAIT_FOREVER);
    if (ret == 1)
    {
        readlen = read(STDIN_FILENO, data, len);
    }

    return readlen;
}

/**
 * @brief shell写数据函数原型
 *
 * @param data 需写的字符数据
 * @param len 需要写入的字符数
 *
 * @return unsigned short 实际写入的字符数量
 */
static short ttos_shellWrite(char *data, unsigned short len)
{
    len = write(STDOUT_FILENO, data, len);
    return len;
}

uint64_t ttos_shellTickGet(void)
{
    T_UDWORD ticks;
    TTOS_GetSystemTicks(&ticks);
    return ticks;
}

#if SHELL_USING_LOCK == 1
/**
 * @brief shell上锁
 *
 * @param struct shell_def shell对象
 *
 * @return 0
 */
static int ttos_shellLock(struct shell_def *shell)
{
    MUTEX_ID mutex = shellCompanionGet(shell, SHELL_COMPANION_ID_MUTEX);
    return (int)TTOS_ObtainMutex(mutex, TTOS_MUTEX_WAIT_FOREVER);
}

#include <sys/stat.h>

static char __pwd[256] = "/";
char *path_join(const char *path, const char *join);
static char *__getcwd(char *buf, size_t size)
{
    if (buf != NULL)
    {
        strncpy(buf, __pwd, size);
        return buf;
    }

    return __pwd;
}

static int __chdir_r(const char *filename)
{
    int ret;
    struct stat einfo;
    ret = stat(filename, &einfo);
    if (ret < 0)
    {
        return ret;
    }
    if (!S_ISDIR(einfo.st_mode))
    {
        return -ENOTDIR;
    }
    strncpy(__pwd, filename, sizeof(__pwd));
    return 0;
}

static int __chdir(const char *filename)
{
    int ret;

    if (filename == NULL)
    {
        errno = ENOENT;
        return -ENOENT;
    }

    if (filename[0] != '/')
    {
        char *new_path;
        new_path = path_join(__pwd, filename);
        ret = __chdir_r(new_path);
        free(new_path);
    }
    else
    {
        ret = __chdir_r(filename);
    }
    if (ret < 0)
    {
        errno = -ret;
    }
    return ret;
}

/**
 * @brief shell解锁
 *
 * @param struct shell_def shell对象
 *
 * @return 0
 */
static int ttos_shellUnLock(struct shell_def *shell)
{
    MUTEX_ID mutex = shellCompanionGet(shell, SHELL_COMPANION_ID_MUTEX);
    return (int)TTOS_ReleaseMutex(mutex);
}
#endif
extern size_t shelllistdir(char *dir, char *buffer, size_t maxLen);

const char *logo_net =
    "\n"
    "       ____      __                    ____   ____ __________ _____\n"
    "      /  _____  / /____ _      _____  / / /  / __ /_  __/ __ / ___/\n"
    "      / // __ \\\\/ __/ _ | | /| / / _ \\\\/ / /  / /_/ // / / / / \\\\__ \\\\ "
    "\n"
    "    _/ // / / / /_/  __| |/ |/ /  __/ / /  / _, _// / / /_/ ___/ / \n"
    "   /___/_/ /_/\\\\__/\\\\___/|__/|__/\\\\___/_/_/  /_/ |_|/_/  \\\\____/____/  "
    "\n"
    "\n"
    "\n\n"
    "Build:       " __DATE__ " " __TIME__ "\r\n"
    "Version:     " INTEWELL_VERSION_STRING "\r\n"
    "Git:         " GIT_INFO "\r\n"
    "Copyright:   (C) 2022 Intewell Inc. All Rights Reserved.\r\n host: \\n \r\n tty:  \\l \r\n";

const char *logo = "\n"
                   "       ____      __                    ____   ____ __________ _____\n"
                   "      /  _____  / /____ _      _____  / / /  / __ /_  __/ __ / ___/\n"
                   "      / // __ \\\\/ __/ _ | | /| / / _ \\\\/ / /  / /_/ // / / / / \\\\__ \\\\ "
                   "\n"
                   "    _/ // / / / /_/  __| |/ |/ /  __/ / /  / _, _// / / /_/ ___/ / \n"
                   "   /___/_/ /_/\\\\__/\\\\___/|__/|__/\\\\___/_/_/  /_/ |_|/_/  \\\\____/____/  "
                   "\n"
                   "\n"
                   "\n\n"
                   "Build:       " __DATE__ " " __TIME__ "\r\n"
                   "Version:     " INTEWELL_VERSION_STRING "\r\n"
                   "Git:         " GIT_INFO "\r\n"
                   "Copyright:   (C) 2022 Intewell Inc. All Rights Reserved.\r\n";

const char *logo_motd = "\n"
                        "       ____      __                    ____   ____ __________ _____\n"
                        "      /  _____  / /____ _      _____  / / /  / __ /_  __/ __ / ___/\n"
                        "      / // __ \\/ __/ _ | | /| / / _ \\/ / /  / /_/ // / / / / \\__ \\ "
                        "\n"
                        "    _/ // / / / /_/  __| |/ |/ /  __/ / /  / _, _// / / /_/ ___/ / \n"
                        "   /___/_/ /_/\\__/\\___/|__/|__/\\___/_/_/  /_/ |_|/_/  \\____/____/  "
                        "\n"
                        "\n"
                        "\n\n"
                        "Build:       " __DATE__ " " __TIME__ "\r\n"
                        "Version:     " INTEWELL_VERSION_STRING "\r\n"
                        "Git:         " GIT_INFO "\r\n"
                        "Copyright:   (C) 2022 Intewell Inc. All Rights Reserved.\r\n";

int ttosShellInit(void)
{
    int fd;
    int ret = -1;

    fd = vfs_open("/dev/console", O_RDWR);

    if (fd < 0)
    {
        fd = vfs_open("/dev/ttyS0", O_RDWR);
        symlink("/dev/ttyS0", "/dev/console");
    }

    if (fd >= 0)
    {
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2)
        {
            vfs_close(fd);
        }

        symlink("/proc/self/fd", "/dev/fd");
        symlink("/proc/self/fd/0", "/dev/stdin");
        symlink("/proc/self/fd/1", "/dev/stdout");
        symlink("/proc/self/fd/2", "/dev/stderr");
    }

    {
        int issue_fd = vfs_open("/etc/issue.net", O_RDWR | O_CREAT | O_TRUNC);
        if (issue_fd)
        {
            write(issue_fd, logo_net, strlen(logo_net));
            vfs_close(issue_fd);
        }
    }

    {
        int issue_fd = vfs_open("/etc/issue", O_RDWR | O_CREAT | O_TRUNC);
        if (issue_fd)
        {
            write(issue_fd, logo, strlen(logo));
            vfs_close(issue_fd);
        }
    }

    {
        int issue_fd = vfs_open("/etc/motd", O_RDWR | O_CREAT | O_TRUNC);
        if (issue_fd)
        {
            write(issue_fd, logo_motd, strlen(logo_motd));
            vfs_close(issue_fd);
        }
    }

    // char *argv[] = {"/sbin/init", "3", NULL};
    char *argv[] = {CONFIG_INIT_PROCESS_NAME, NULL, NULL};
    char *__envp[] = {"CONSOLE=/dev/console", "TERM=vt102", NULL};
    ret = kernel_execve(CONFIG_INIT_PROCESS_NAME, (const char *const *)argv,
                        (const char *const *)__envp);

    if (ret < 0)
        g_uart_shell.read = ttos_shellRead;
    else
        shell_ksh_init(&g_uart_shell);

    TASK_ID tid;
    struct termios termiosp;

    g_uart_shell.write = ttos_shellWrite;
#if SHELL_USING_LOCK == 1
    MUTEX_ID mutex;
    g_uart_shell.lock = ttos_shellLock;
    g_uart_shell.unlock = ttos_shellUnLock;
    ret = TTOS_CreateMutex(1, 25, &mutex);
    if (ret != TTOS_OK)
    {
        KLOG_E("Mutex Create error %d", ret);
    }
    shellCompanionAdd(&g_uart_shell, SHELL_COMPANION_ID_MUTEX, (void *)mutex);
#endif

    ioctl(STDIN_FILENO, TCGETS, &termiosp);
    termiosp.c_lflag &= ~ECHO;
    ioctl(STDIN_FILENO, TCSETS, &termiosp);

    g_uart_shellFs.getcwd = __getcwd;
    g_uart_shellFs.chdir = __chdir;
    g_uart_shellFs.listdir = shelllistdir;
    shellFsInit(&g_uart_shellFs, shellPathBuffer, PATH_MAX);

    shellSetPath(&g_uart_shell, shellPathBuffer);
    shellCompanionAdd(&g_uart_shell, SHELL_COMPANION_ID_FS, &g_uart_shellFs);

    shellInit(&g_uart_shell, (char *)page_address(pages_alloc(0, ZONE_NORMAL)), PAGE_SIZE);

    ret = TTOS_CreateTaskEx((T_UBYTE *)"Kernel shell", 225, TRUE, TRUE, shellTask, &g_uart_shell,
                            81920, &tid);

    if (ret != 0)
    {
        KLOG_E("%s", strerror(errno));
        return ret;
    }

    return 0;
}
INIT_EXPORT_SERVE_FS(ttosShellInit, "init shell");
