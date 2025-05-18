/* ------------------------------------------------------------------
   score_calc – calculates user scores in a hunt
   ---------------------------------------------------------------
   Usage:
       ./score_calc <hunt_path>/treasures.dat

   Output:
       alice : 150
       bob   : 90
   ------------------------------------------------------------------ */
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    char tid[20];
    char uname[50];
    char lat[20];
    char lon[20];
    char clue[100];
    char val[20];
} trec;

typedef struct user_score {
    char uname[50];
    int score;
    struct user_score *next;
} user_score;

user_score *add_score(user_score *head, const char *name, int val) {
    user_score *curr = head;
    while (curr) {
        if (!strcmp(curr->uname, name)) {
            curr->score += val;
            return head;
        }
        curr = curr->next;
    }
    user_score *new_node = malloc(sizeof(user_score));
    strcpy(new_node->uname, name);
    new_node->score = val;
    new_node->next = head;
    return new_node;
}

int main(int ac, char **av) {
    if (ac < 2) {
        write(2, "Usage: score_calc <file>\n", 26);
        return 1;
    }

    int fd = open(av[1], O_RDONLY);
    if (fd < 0) return 1;

    trec r;
    user_score *head = NULL;

    while (read(fd, &r, sizeof r) == sizeof r) {
        int val = atoi(r.val);
        head = add_score(head, r.uname, val);
    }
    close(fd);

    user_score *curr = head;
    while (curr) {
        printf("%s : %d\n", curr->uname, curr->score);
        user_score *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    return 0;
}
