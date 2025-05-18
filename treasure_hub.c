/* ------------------------------------------------------------------
   treasure_hub – Phase‑3 interactive shell + monitor with pipes
   ---------------------------------------------------------------
   Build:
       gcc -Wall treasure_hub.c -o treasure_hub
       (ensure ./treasure_manager and score_calc are also built)

   Usage:
       $ ./treasure_hub
       hub> start_monitor
       hub> list_hunts
       hub> list_treasures game1
       hub> view_treasure game1 t1
       hub> calculate_score
       hub> stop_monitor
       hub> exit
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
#include <stdlib.h>

#define CMD_FILE "monitor_cmd"
#define PIPE_READ  0
#define PIPE_WRITE 1
#define REC_SIZE (20+50+20+20+100+20)

static pid_t mon_pid = -1;
static volatile sig_atomic_t child_done = 0;
static int pipefd[2];

static void chld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    child_done = 1;
}

static volatile sig_atomic_t got_sig = 0;
static void usr1(int sig) { (void)sig; got_sig = 1; }

static void list_hunts(void) {
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
            dprintf(pipefd[PIPE_WRITE], "%s : %ld treasure(s)\n", e->d_name, nr);
        }
    }
    closedir(d);
}

static void monitor_loop(void) {
    struct sigaction sa = {0};
    sa.sa_handler = usr1;
    sigaction(SIGUSR1, &sa, NULL);

    close(pipefd[PIPE_READ]);

    for (;;) {
        pause();
        if (!got_sig) continue;
        got_sig = 0;

        char buf[256] = {0};
        int fd = open(CMD_FILE, O_RDONLY);
        if (fd < 0) continue;
        int n = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (n <= 0) continue;

        if (!strncmp(buf, "STOP", 4)) {
            dprintf(pipefd[PIPE_WRITE], "Monitor: shutting down...\n");
            usleep(500000);
            _exit(0);
        } else if (!strncmp(buf, "LIST_HUNTS", 10)) {
            list_hunts();
        } else if (!strncmp(buf, "LIST_TREASURES", 14)) {
            char hunt[128] = {0};
            sscanf(buf + 15, "%127s", hunt);
            int fd2[2]; pipe(fd2);
            if (fork() == 0) {
                dup2(fd2[PIPE_WRITE], STDOUT_FILENO);
                close(fd2[PIPE_READ]);
                execl("./treasure_manager", "treasure_manager", "--list", hunt, NULL);
                _exit(1);
            }
            close(fd2[PIPE_WRITE]);
            char c;
            while (read(fd2[PIPE_READ], &c, 1) == 1)
                write(pipefd[PIPE_WRITE], &c, 1);
            close(fd2[PIPE_READ]);
            wait(NULL);
        } else if (!strncmp(buf, "VIEW_TREASURE", 13)) {
            char hunt[128] = {0}, tid[128] = {0};
            sscanf(buf + 14, "%127s %127s", hunt, tid);
            int fd2[2]; pipe(fd2);
            if (fork() == 0) {
                dup2(fd2[PIPE_WRITE], STDOUT_FILENO);
                close(fd2[PIPE_READ]);
                execl("./treasure_manager", "treasure_manager", "--view", hunt, tid, NULL);
                _exit(1);
            }
            close(fd2[PIPE_WRITE]);
            char c;
            while (read(fd2[PIPE_READ], &c, 1) == 1)
                write(pipefd[PIPE_WRITE], &c, 1);
            close(fd2[PIPE_READ]);
            wait(NULL);
        } else {
            dprintf(pipefd[PIPE_WRITE], "Monitor: bad request\n");
        }
    }
}

static int push_cmd(const char *txt) {
    int fd = open(CMD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return -1;
    write(fd, txt, strlen(txt));
    close(fd);
    kill(mon_pid, SIGUSR1);
    return 0;
}

static void calculate_score(void) {
    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char file[256];
        snprintf(file, sizeof file, "%s/treasures.dat", e->d_name);
        struct stat st;
        if (!stat(file, &st)) {
            int fd[2]; pipe(fd);
            if (fork() == 0) {
                dup2(fd[PIPE_WRITE], STDOUT_FILENO);
                close(fd[PIPE_READ]);
                execl("./score_calc", "score_calc", file, NULL);
                _exit(1);
            }
            close(fd[PIPE_WRITE]);
            char c;
            while (read(fd[PIPE_READ], &c, 1) == 1)
                write(1, &c, 1);
            close(fd[PIPE_READ]);
            wait(NULL);
        }
    }
    closedir(d);
}

static void shell_loop(void) {
    char line[256];
    while (1) {
        printf("hub> "); fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\n")] = 0;

        if (!strcmp(line, "start_monitor")) {
            if (mon_pid != -1 && !child_done) {
                puts("Monitor already running"); continue;
            }
            pipe(pipefd);
            mon_pid = fork();
            if (mon_pid == 0) monitor_loop();
            close(pipefd[PIPE_WRITE]);
            printf("Monitor pid %d\n", mon_pid);
            child_done = 0;
        } else if (!strcmp(line, "list_hunts") ||
                   !strncmp(line, "list_treasures", 14) ||
                   !strncmp(line, "view_treasure", 13)) {
            if (mon_pid == -1 || child_done) puts("No monitor");
            else {
                push_cmd(line[5] == '_' ?
                         (strncmp(line, "list_hunts", 10) == 0 ? "LIST_HUNTS" :
                         strncmp(line, "list_treasures", 14) == 0 ?
                         (sprintf(line, "LIST_TREASURES %s", line + 15), line) :
                         (sprintf(line, "VIEW_TREASURE %s", line + 14), line)) :
                         "");
                char c;
                while (read(pipefd[PIPE_READ], &c, 1) == 1)
                    write(1, &c, 1);
            }
        } else if (!strcmp(line, "calculate_score")) {
            calculate_score();
        } else if (!strcmp(line, "stop_monitor")) {
            if (mon_pid == -1 || child_done) puts("No monitor");
            else { push_cmd("STOP"); puts("Waiting..."); }
        } else if (!strcmp(line, "exit")) {
            if (mon_pid != -1 && !child_done)
                puts("Monitor still running");
            else break;
        } else puts("Unknown command");
    }
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = chld;
    sigaction(SIGCHLD, &sa, NULL);
    shell_loop();
    return 0;
}
