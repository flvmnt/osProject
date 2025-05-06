/* ------------------------------------------------------------------
   treasure_manager  ‑ Phase‑1 utility
   ---------------------------------------------------------------
   Build:
       gcc -Wall treasure_manager.c -o treasure_manager

   Basic usage:
       # add two treasures to hunt “game1”
       ./treasure_manager --add game1 t1 alice 44.44 26.11 "under oak" 50
       ./treasure_manager --add game1 t2 bob   44.41 26.10 "by lake"   75

       # list everything in a hunt
       ./treasure_manager --list game1

       # show a single treasure
       ./treasure_manager --view game1 t1

       # remove a treasure and check result
       ./treasure_manager --remove_treasure game1 t1
       ./treasure_manager --view game1 t1      # prints “Not found”

       # delete the whole hunt (dir + symlink)
       ./treasure_manager --remove_hunt game1
   ------------------------------------------------------------------ */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

/* Minimal record with lat/lon stored as strings. */
typedef struct {
    char tid[20];
    char uname[50];
    char lat[20];
    char lon[20];
    char clue[100];
    char val[20];
} trec;

/* Write a C‑string to stdout using write() */
void wout(const char *s) { write(1, s, strlen(s)); }

/* Convert a long long to string (no printf) */
char *my_itoa(long long v, char *buf) {
    if (!v) { buf[0] = '0'; buf[1] = 0; return buf; }
    int neg = v < 0; if (neg) v = -v;
    char t[32]; int i = 0;
    while (v && i < 31) { t[i++] = '0' + (v % 10); v /= 10; }
    int p = 0; if (neg) buf[p++] = '-';
    while (i)  buf[p++] = t[--i];
    buf[p] = 0; return buf;
}

/* Log each operation to <hunt>/logged_hunt; ensure symlink */
void log_op(const char *h, const char *op, const char *data) {
    char lpath[256] = {0}, ln[256] = {0}, row[512] = {0};
    strcpy(lpath, h); strcat(lpath, "/logged_hunt");
    int fd = open(lpath, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0) {
        strcat(row, op); strcat(row, ": ");
        if (data) strcat(row, data);
        strcat(row, "\n");
        write(fd, row, strlen(row));
        close(fd);
    }
    strcpy(ln, "logged_hunt-"); strcat(ln, h);
    unlink(ln);                       /* refresh link */
    symlink(lpath, ln);
}

/* Ensure directory for hunt exists */
int mk_hunt(const char *h) {
    struct stat st;
    if (!stat(h, &st)) return (S_ISDIR(st.st_mode) ? 0 : -1);
    return mkdir(h, 0777);
}

/* Add new treasure */
int do_add(char **av, int ac) {
    if (ac < 9) return 1;
    if (mk_hunt(av[2])) return 1;

    trec r; memset(&r, 0, sizeof r);
    strncpy(r.tid,   av[3], sizeof r.tid  -1);
    strncpy(r.uname, av[4], sizeof r.uname-1);
    strncpy(r.lat,   av[5], sizeof r.lat  -1);
    strncpy(r.lon,   av[6], sizeof r.lon  -1);
    strncpy(r.clue,  av[7], sizeof r.clue -1);
    strncpy(r.val,   av[8], sizeof r.val  -1);

    char path[256] = {0};
    strcpy(path, av[2]); strcat(path, "/treasures.dat");
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return 1;
    write(fd, &r, sizeof r);
    close(fd);
    log_op(av[2], "ADD", r.tid);
    return 0;
}

/* List treasures */
int do_list(char **av, int ac) {
    if (ac < 3) return 1;
    char path[256] = {0};
    strcpy(path, av[2]); strcat(path, "/treasures.dat");
    struct stat st;
    if (stat(path, &st)) return 1;

    char sbuf[64];
    wout("Hunt: "); wout(av[2]); wout("\n");
    my_itoa(st.st_size, sbuf);  wout("File size: "); wout(sbuf); wout("\n");
    my_itoa(st.st_mtime, sbuf); wout("Last mtime (epoch): "); wout(sbuf); wout("\n");

    int fd = open(path, O_RDONLY); if (fd < 0) return 1;
    trec r;
    while (read(fd, &r, sizeof r) == sizeof r) {
        wout("----\n");
        wout("ID: ");   wout(r.tid);   wout("\n");
        wout("User: "); wout(r.uname); wout("\n");
        wout("Lat: ");  wout(r.lat);   wout("\n");
        wout("Lon: ");  wout(r.lon);   wout("\n");
        wout("Clue: "); wout(r.clue);  wout("\n");
        wout("Val: ");  wout(r.val);   wout("\n");
    }
    close(fd);
    log_op(av[2], "LIST", 0);
    return 0;
}

/* View treasure */
int do_view(char **av, int ac) {
    if (ac < 4) return 1;
    char path[256] = {0};
    strcpy(path, av[2]); strcat(path, "/treasures.dat");
    int fd = open(path, O_RDONLY); if (fd < 0) return 1;

    trec r; int ok = 0;
    while (read(fd, &r, sizeof r) == sizeof r) {
        if (!strcmp(r.tid, av[3])) {
            wout("ID: ");   wout(r.tid);   wout("\n");
            wout("User: "); wout(r.uname); wout("\n");
            wout("Lat: ");  wout(r.lat);   wout("\n");
            wout("Lon: ");  wout(r.lon);   wout("\n");
            wout("Clue: "); wout(r.clue);  wout("\n");
            wout("Val: ");  wout(r.val);   wout("\n");
            ok = 1; break;
        }
    }
    close(fd);
    if (!ok) { wout("Not found\n"); return 1; }
    log_op(av[2], "VIEW", av[3]);
    return 0;
}

/* Remove one treasure (rewrite file) */
int do_remove_treasure(char **av, int ac) {
    if (ac < 4) return 1;
    char datap[256] = {0}, tmp[256] = {0};
    strcpy(datap, av[2]); strcat(datap, "/treasures.dat");
    strcpy(tmp, av[2]);   strcat(tmp, "/treasures.tmp");

    int in = open(datap, O_RDONLY); if (in < 0) return 1;
    int out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) { close(in); return 1; }

    trec r; int gone = 0;
    while (read(in, &r, sizeof r) == sizeof r) {
        if (!strcmp(r.tid, av[3])) { gone = 1; continue; }
        write(out, &r, sizeof r);
    }
    close(in); close(out);
    if (!gone) { unlink(tmp); wout("ID not found\n"); return 1; }
    rename(tmp, datap);
    log_op(av[2], "REMOVE_TREASURE", av[3]);
    return 0;
}

/* recursive dir delete (very small) */
int rm_dir(const char *d) {
    DIR *dp = opendir(d); if (!dp) return -1;
    struct dirent *e; char p[512];
    while ((e = readdir(dp))) {
        if (!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
        strcpy(p, d); strcat(p, "/"); strcat(p, e->d_name);
        struct stat st; if (lstat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) rm_dir(p);
        else unlink(p);
    }
    closedir(dp);
    return rmdir(d);
}

/* Remove whole hunt */
int do_remove_hunt(char **av, int ac) {
    if (ac < 3) return 1;
    char ln[256] = {0};
    strcpy(ln, "logged_hunt-"); strcat(ln, av[2]);
    unlink(ln);                      /* drop symlink */
    if (rm_dir(av[2])) { wout("Cannot remove\n"); return 1; }
    /* no log, directory is gone */
    return 0;
}

int main(int ac, char **av) {
    if (ac < 2) {
        wout("Usage:\n");
        wout("  "); wout(av[0]); wout(" --add <hunt> <tid> <user> <lat> <lon> <clue> <val>\n");
        wout("  "); wout(av[0]); wout(" --list <hunt>\n");
        wout("  "); wout(av[0]); wout(" --view <hunt> <tid>\n");
        wout("  "); wout(av[0]); wout(" --remove_treasure <hunt> <tid>\n");
        wout("  "); wout(av[0]); wout(" --remove_hunt <hunt>\n");
        return 1;
    }
    if (!strcmp(av[1], "--add"))             return do_add(av, ac);
    if (!strcmp(av[1], "--list"))            return do_list(av, ac);
    if (!strcmp(av[1], "--view"))            return do_view(av, ac);
    if (!strcmp(av[1], "--remove_treasure")) return do_remove_treasure(av, ac);
    if (!strcmp(av[1], "--remove_hunt"))     return do_remove_hunt(av, ac);

    wout("Unknown command\n");
    return 1;
}
