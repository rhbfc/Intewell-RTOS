#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

char *mod2str (int mod);
char *num2str (unsigned int num);

#define P_a 0b1
#define P_l 0b10
#define P_R 0b100
#define P_t 0b1000
#define P_r 0b10000
#define P_i 0b100000
#define P_s 0b1000000

static unsigned param = 0;

static size_t listfile (struct stat *einfo, char *fname, char *buffer,
                        size_t maxLen)
{
#define OUTPRINT(...)                                                          \
    out_size += snprintf (&buffer[out_size], maxLen - out_size, __VA_ARGS__)
    size_t out_size = 0;
    if (param & P_i)
        OUTPRINT ("%8lu  ", einfo->st_ino);
    if (param & P_s)
        OUTPRINT ("%4ld  ", (einfo->st_size / 4096 * 4)
                                + (einfo->st_size % 4096 ? 4 : 0));
    if (param & P_l)
    {
        OUTPRINT ("%s  ", mod2str (einfo->st_mode));
        OUTPRINT ("%4d  ", (int)einfo->st_nlink);
        OUTPRINT ("%8ld  ", einfo->st_size);
        // OUTPRINT("%.12s  ", 4 + ctime((const time_t*)&einfo->st_mtim));
    }
    OUTPRINT (S_ISDIR (einfo->st_mode) ? "\033[1;34m%-10s\033[0m" : "%-10s",
              fname);
#undef OUTPRINT
    return out_size;
}

char *mod2str (int mod)
{
    static char str[] = "----------";
    str[0]            = S_ISDIR (mod) ? 'd' : S_ISLNK (mod) ? 'l' : '-';
    str[1]            = mod & S_IRUSR ? 'r' : '-';
    str[2]            = mod & S_IWUSR ? 'w' : '-';
    str[3]            = mod & S_IXUSR ? 'x' : '-';
    str[4]            = mod & S_IRGRP ? 'r' : '-';
    str[5]            = mod & S_IWGRP ? 'w' : '-';
    str[6]            = mod & S_IXGRP ? 'x' : '-';
    str[7]            = mod & S_IROTH ? 'r' : '-';
    str[8]            = mod & S_IWOTH ? 'w' : '-';
    str[9]            = mod & S_IXOTH ? 'x' : '-';
    return str;
}

char *num2str (unsigned int num)
{
    static char str[NAME_MAX];
    sprintf (str, "%d", num);
    return str;
}

size_t shelllistdir (char *dir, char *buffer, size_t maxLen)
{
#define OUTPRINT(...)                                                          \
    out_size += snprintf (&buffer[out_size], maxLen - out_size, __VA_ARGS__)
    size_t out_size = 0;
    DIR   *dirp     = opendir (dir);
    if (!dirp)
    {
        struct stat einfo;
        if (stat (dir, &einfo) == -1)
            OUTPRINT ("Cannot access '%s': No file or dirents\n", dir);
        else if (!(param & P_R))
        {
            out_size
                += listfile (&einfo, dir, &buffer[out_size], maxLen - out_size);
            OUTPRINT ("\n");
        }
        return out_size;
    }
    OUTPRINT ("%s:\n", dir);
    // 生成列表
    int entc, enti[PATH_MAX];
    struct entbrief {
        char   name[NAME_MAX];
        time_t mtime;
    };
    struct entbrief *entv = malloc (sizeof (struct entbrief[PATH_MAX]));
    struct dirent   *entp;

    for (entc = 0; (entp = readdir (dirp)) != NULL;)
    {
        if (!(param & P_a) && *entp->d_name == '.')
            continue;
        struct stat einfo;
        strcpy (entv[entc].name, entp->d_name);
        if (param & P_t)
        {
            char path[PATH_MAX];
            sprintf (path, "%s/%s", dir, entp->d_name);
            if (stat (path, &einfo) == -1)
                continue;
            entv[entc].mtime = einfo.st_mtim.tv_sec;
        }
        enti[entc] = entc;
        entc++;
    }
    // 排序
    for (int i = 1, j, temp; i < entc; i++)
    {
        temp = enti[i];
        for (j = i;
             // 排序的condition比较长，所以称之为“悲伤的猪大排”
             j > 0
             && !!(param & P_r)
                    ^ (param & P_t
                           ? entv[enti[j - 1]].mtime > entv[temp].mtime
                           : strcmp (entv[enti[j - 1]].name, entv[temp].name)
                                 > 0);
             j--)
            enti[j] = enti[j - 1];
        enti[j] = temp;
    }
    // 输出
    for (int i = 0; i < entc; i++)
    {
        struct stat einfo;
        char        path[PATH_MAX];
        sprintf (path, "%s/%s", dir, entv[enti[i]].name);
        if (stat (path, &einfo) == -1)
            continue;
        out_size += listfile (&einfo, entv[enti[i]].name, &buffer[out_size],
                              maxLen - out_size);
        OUTPRINT (param & P_l || i % 5 == 4 ? "\n" : "  \t");
    }
    OUTPRINT (param & P_l ? "\n" : "\n\n");
    closedir (dirp);
    // 递归
    for (int i = 0; i < entc && param & P_R; i++)
    {
        if (!strcmp (entv[i].name, ".") || !strcmp (entv[i].name, ".."))
            continue;
        char path[PATH_MAX];
        sprintf (path, "%s/%s", dir, entv[i].name);
        out_size += shelllistdir (path, &buffer[out_size], maxLen - out_size);
    }
    free (entv);
#undef OUTPRINT
    return out_size;
}
