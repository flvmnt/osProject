#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
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

/* Write a C-string to stdout using write() */
void wout(const char *s) {
    write(1, s, strlen(s));
}

/* Convert a long long to string (no printf). */
char *my_itoa(long long v, char *buf) {
    if (v == 0) {
        buf[0] = '0'; 
        buf[1] = 0; 
        return buf;
    }
    int neg = (v < 0);
    if (neg) v = -v;
    char tmp[32];
    int i = 0;
    while (v > 0 && i < 31) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    int p = 0;
    if (neg) buf[p++] = '-';
    while (i > 0) {
        buf[p++] = tmp[--i];
    }
    buf[p] = 0;
    return buf;
}

/* Log each operation to <hunt>/logged_hunt; ensure symlink. */
void log_op(const char *h, const char *op, const char *data) {
    char lpath[256]; 
    char ln[256]; 
    char t[512];
    int fd;

    /* Directory log file */
    memset(lpath, 0, 256);
    strcpy(lpath, h);
    strcat(lpath, "/logged_hunt");
    fd = open(lpath, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0) {
        memset(t, 0, 512);
        strcat(t, op);
        strcat(t, ": ");
        if (data) strcat(t, data);
        strcat(t, "\n");
        write(fd, t, strlen(t));
        close(fd);
    }
    /* Symlink in root */
    memset(ln, 0, 256);
    strcpy(ln, "logged_hunt-");
    strcat(ln, h);
    unlink(ln);
    symlink(lpath, ln);
}

/* Ensure directory for hunt exists */
int mk_hunt(const char *h) {
    struct stat st;
    if (stat(h, &st) == 0) {
        if ((st.st_mode & S_IFMT) == S_IFDIR) return 0;
        return -1;
    }
    if (mkdir(h, 0777) < 0) return -1;
    return 0;
}

/* Add new treasure (record) into <hunt>/treasures.dat */
int do_add(char **av, int ac) {
    if (ac < 9) return 1; 
    if (mk_hunt(av[2]) < 0) return 1;
    trec r;
    memset(&r, 0, sizeof(r));
    /* argv:
       0: ./treasure_manager
       1: --add
       2: hunt
       3: tid
       4: uname
       5: lat
       6: lon
       7: clue
       8: val
    */
    strncpy(r.tid,   av[3], sizeof(r.tid)-1);
    strncpy(r.uname, av[4], sizeof(r.uname)-1);
    strncpy(r.lat,   av[5], sizeof(r.lat)-1);
    strncpy(r.lon,   av[6], sizeof(r.lon)-1);
    strncpy(r.clue,  av[7], sizeof(r.clue)-1);
    strncpy(r.val,   av[8], sizeof(r.val)-1);

    char path[256];
    memset(path, 0, 256);
    strcpy(path, av[2]);
    strcat(path, "/treasures.dat");
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return 1;
    write(fd, &r, sizeof(r));
    close(fd);
    log_op(av[2], "ADD", r.tid);
    return 0;
}

/* List treasures in <hunt>/treasures.dat, plus file size and mtime */
int do_list(char **av, int ac) {
    if (ac < 3) return 1;
    char path[256];
    memset(path, 0, 256);
    strcpy(path, av[2]);
    strcat(path, "/treasures.dat");
    struct stat st;
    if (stat(path, &st) < 0) return 1;
    char sbuf[64];
    my_itoa((long long)st.st_size, sbuf);
    wout("Hunt: "); wout(av[2]); wout("\n");
    wout("File size: "); wout(sbuf); wout("\n");
    my_itoa((long long)st.st_mtime, sbuf);
    wout("Last mtime (epoch): "); wout(sbuf); wout("\n");

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 1;
    trec r;
    while (1) {
        int rr = read(fd, &r, sizeof(r));
        if (rr == 0) break;
        if (rr < 0) { close(fd); return 1; }
        if (rr != sizeof(r)) { close(fd); return 1; }
        wout("----\n");
        wout("ID: "); wout(r.tid); wout("\n");
        wout("User: "); wout(r.uname); wout("\n");
        wout("Lat: "); wout(r.lat); wout("\n");
        wout("Lon: "); wout(r.lon); wout("\n");
        wout("Clue: "); wout(r.clue); wout("\n");
        wout("Val: "); wout(r.val); wout("\n");
    }
    close(fd);
    log_op(av[2], "LIST", 0);
    return 0;
}

/* View details of a specific treasure by ID */
int do_view(char **av, int ac) {
    if (ac < 4) return 1;
    char path[256];
    memset(path, 0, 256);
    strcpy(path, av[2]);
    strcat(path, "/treasures.dat");
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 1;
    trec r;
    int found = 0;
    while (1) {
        int rr = read(fd, &r, sizeof(r));
        if (rr == 0) break;
        if (rr < 0) { close(fd); return 1; }
        if (rr != sizeof(r)) { close(fd); return 1; }
        if (strcmp(r.tid, av[3]) == 0) {
            wout("ID: "); wout(r.tid); wout("\n");
            wout("User: "); wout(r.uname); wout("\n");
            wout("Lat: "); wout(r.lat); wout("\n");
            wout("Lon: "); wout(r.lon); wout("\n");
            wout("Clue: "); wout(r.clue); wout("\n");
            wout("Val: "); wout(r.val); wout("\n");
            found = 1;
            break;
        }
    }
    close(fd);
    if (!found) {
        wout("Not found\n");
        return 1;
    }
    log_op(av[2], "VIEW", av[3]);
    return 0;
}

int main(int ac, char **av) {
    if (ac < 2) {
        wout("Usage:\n");
        wout("  "); wout(av[0]); wout(" --add <hunt> <tid> <user> <lat> <lon> <clue> <val>\n");
        wout("  "); wout(av[0]); wout(" --list <hunt>\n");
        wout("  "); wout(av[0]); wout(" --view <hunt> <tid>\n");
        return 1;
    }
    if (!strcmp(av[1], "--add")) return do_add(av, ac);
    if (!strcmp(av[1], "--list")) return do_list(av, ac);
    if (!strcmp(av[1], "--view")) return do_view(av, ac);
    wout("Unknown command\n");
    return 1;
}
