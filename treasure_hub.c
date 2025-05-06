/* ------------------------------------------------------------------
   treasure_hub  ‑ Phase‑2 interactive shell + monitor
   ---------------------------------------------------------------
   Requires treasure_manager built in the same directory.

   Build:
       gcc -Wall treasure_hub.c -o treasure_hub
       (make sure ./treasure_manager is also compiled)

   Session example:

       $ ./treasure_hub
       hub> start_monitor          # fork a monitor process
       Monitor pid 12345
       hub> list_hunts             # counts treasures per hunt
       game1 : 1 treasure(s)
       hub> list_treasures game1   # proxy to treasure_manager --list
       ----
       ID: t2
       ...
       hub> view_treasure game1 t2
       ID: t2
       User: bob
       ...
       hub> stop_monitor           # ask monitor to quit (½‑second delay)
       Waiting...
       Monitor: shutting down...
       hub> exit                   # safe now – monitor already gone
   ------------------------------------------------------------------ */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

#define CMD_FILE "monitor_cmd"
#define REC_SIZE (20+50+20+20+100+20)   /* bytes per treasure record */

static pid_t mon_pid = -1;
static volatile sig_atomic_t child_done = 0;

/* Parent: SIGCHLD handler */
static void chld(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) ;
    child_done = 1;
}

/* Monitor: SIGUSR1 handler flag */
static volatile sig_atomic_t got_sig = 0;
static void usr1(int sig) { (void)sig; got_sig = 1; }

/* Monitor helper – count treasures per hunt */
static void list_hunts(void)
{
    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[256] = {0};
        strcpy(p, e->d_name);
        strcat(p, "/treasures.dat");
        struct stat st;
        if (!stat(p, &st)) {
            long nr = st.st_size / REC_SIZE;
            printf("%s : %ld treasure(s)\n", e->d_name, nr);
        }
    }
    closedir(d);
}

/* Monitor loop – waits for requests, runs, repeats */
static void monitor_loop(void)
{
    struct sigaction sa = {0};
    sa.sa_handler = usr1;
    sigaction(SIGUSR1, &sa, NULL);

    for (;;) {
        pause();                        /* wait for SIGUSR1 */
        if (!got_sig) continue;
        got_sig = 0;

        char buf[256] = {0};
        int fd = open(CMD_FILE, O_RDONLY);
        if (fd < 0) continue;
        int n = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (n <= 0) continue;

        if (!strncmp(buf, "STOP", 4)) {
            printf("Monitor: shutting down...\n");
            usleep(500000);             /* let hub show busy state */
            _exit(0);
        } else if (!strncmp(buf, "LIST_HUNTS", 10)) {
            list_hunts();
        } else if (!strncmp(buf, "LIST_TREASURES", 14)) {
            char hunt[128] = {0};
            sscanf(buf + 15, "%127s", hunt);
            execl("./treasure_manager", "treasure_manager",
                  "--list", hunt, (char*)0);
            perror("execl");
        } else if (!strncmp(buf, "VIEW_TREASURE", 13)) {
            char hunt[128] = {0}, tid[128] = {0};
            sscanf(buf + 14, "%127s %127s", hunt, tid);
            execl("./treasure_manager", "treasure_manager",
                  "--view", hunt, tid, (char*)0);
            perror("execl");
        } else {
            printf("Monitor: bad request\n");
        }
    }
}

/* Parent helpers – write command file + poke child */
static int push_cmd(const char *txt)
{
    int fd = open(CMD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) { perror("open"); return -1; }
    write(fd, txt, strlen(txt));
    close(fd);
    kill(mon_pid, SIGUSR1);
    return 0;
}

/* Simple interactive REPL */
static void shell_loop(void)
{
    char line[256];

    while (1) {
        printf("hub> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\n")] = 0;

        if (!strcmp(line, "start_monitor")) {
            if (mon_pid != -1 && !child_done) {
                puts("Monitor already running");
                continue;
            }
            mon_pid = fork();
            if (mon_pid == 0) monitor_loop();
            printf("Monitor pid %d\n", mon_pid);
            child_done = 0;
        }
        else if (!strncmp(line, "list_hunts", 10)) {
            if (mon_pid == -1 || child_done) puts("No monitor");
            else push_cmd("LIST_HUNTS");
        }
        else if (!strncmp(line, "list_treasures", 14)) {
            char hunt[128];
            if (sscanf(line + 15, "%127s", hunt) != 1) {
                puts("Usage: list_treasures <hunt>");
            } else if (mon_pid == -1 || child_done) {
                puts("No monitor");
            } else {
                char tmp[256]; sprintf(tmp, "LIST_TREASURES %s", hunt);
                push_cmd(tmp);
            }
        }
        else if (!strncmp(line, "view_treasure", 13)) {
            char h[128], t[128];
            if (sscanf(line + 14, "%127s %127s", h, t) != 2) {
                puts("Usage: view_treasure <hunt> <id>");
            } else if (mon_pid == -1 || child_done) {
                puts("No monitor");
            } else {
                char tmp[256]; sprintf(tmp, "VIEW_TREASURE %s %s", h, t);
                push_cmd(tmp);
            }
        }
        else if (!strcmp(line, "stop_monitor")) {
            if (mon_pid == -1 || child_done) puts("No monitor");
            else {
                push_cmd("STOP");
                puts("Waiting...");
            }
        }
        else if (!strcmp(line, "exit")) {
            if (mon_pid != -1 && !child_done)
                puts("Monitor still running");
            else break;
        }
        else puts("Unknown command");
    }
}

int main(void)
{
    struct sigaction sa = {0};
    sa.sa_handler = chld;
    sigaction(SIGCHLD, &sa, NULL);

    shell_loop();
    return 0;
}
