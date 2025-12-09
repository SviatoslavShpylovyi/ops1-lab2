#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

sig_atomic_t solved = 0;
sig_atomic_t last_signal;
pid_t sender_pid;
int p;

void sethandler(void (*f)(int, siginfo_t*, void*), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO; // Use SA_SIGINFO to get additional signal info
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void usage(char* argv)
{
    fprintf(stderr, "Usage: %s p t probability1 [probability2 ...]\n", argv);
    fprintf(stderr, "p: number of task parts (1 <= p <= 10)\n");
    fprintf(stderr, "t: time multiplier (1 < t <= 10)\n");
    fprintf(stderr, "probability: probability of an issue for each student (0-100)\n");
    exit(EXIT_FAILURE);
}

void t_handler(int signal, siginfo_t* info, void* context) {
    last_signal = signal;
    solved++;
    sender_pid = info->si_pid; // Capture the PID of the signaling process
    printf("[TEACHER] Signal from PID %d\n", sender_pid);
}

void child_work(int percentage, int id, int t)
{
    srand(getpid());
    int issues = 0;

    for (int i = 0; i < p; i++) {
        printf("[STUDENT %d, PID %d] Starting part %d\n", id, getpid(), i + 1);

        for (int j = 0; j < t; j++) {
            usleep(100000);
            if (rand() % 100 < percentage) {
                usleep(50000);
                issues++;
                printf("[STUDENT %d, PID %d] Encountered an issue in part %d (total: %d issues)\n", id, getpid(), i + 1, issues);
            }
        }

        kill(getppid(), SIGUSR1);

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGUSR2);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        int sig;
        sigwait(&mask, &sig);
        printf("[STUDENT %d, PID %d] Received confirmation for part %d\n", id, getpid(), i + 1);
    }

    printf("[STUDENT %d, PID %d] Finished all parts with %d issues.\n", id, getpid(), issues);
    exit(issues);
}


    pid_t* create_children(int n, int probabilities[], int t)
{
    pid_t* child_pids = malloc(n * sizeof(pid_t));
    if (child_pids == NULL) {
        ERR("malloc");
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            ERR("fork");
        } else if (pid == 0) {
            child_work(probabilities[i], i + 1, t);
        } else {
            child_pids[i] = pid;
        }
    }
    return child_pids;
}
void teacher_work(pid_t* student_pids, int num_students)
{
    int student_issues[num_students];
    while (solved < num_students * p) {
        pause();
        if (last_signal == SIGUSR1) {
            printf("[TEACHER] Received SIGUSR1. Sending SIGUSR2 to PID %d.\n", sender_pid);
            kill(sender_pid, SIGUSR2);
            last_signal = 0;
        }
    }
    int total_issues = 0;
   for (int i = 0; i < num_students; i++) {
        int status;
        pid_t pid = waitpid(student_pids[i], &status, 0);
        if (WIFEXITED(status)) {
            int issues = WEXITSTATUS(status);
            student_issues[i] = issues;
            total_issues += issues;
        }
    }
    printf("\nNo. | Student ID | Issue count\n");
    printf("---------------------------------\n");
    for (int i = 0; i < num_students; i++) {
        printf("%3d | %10d | %10d\n", i + 1, student_pids[i], student_issues[i]);
    }
    printf("---------------------------------\n");
    printf("Total issues: %d\n", total_issues);
}


int main(int argc, char** argv)
{
     if (argc < 4) {
        usage(argv[0]);
    }

    p = atoi(argv[1]);
    int t = atoi(argv[2]);
    if (p < 1 || p > 10 || t <= 1 || t > 10) {
        usage(argv[0]);
    }

    int num_students = argc - 3;
    int probabilities[num_students];
    for (int i = 0; i < num_students; i++) {
        probabilities[i] = atoi(argv[i + 3]);
        if (probabilities[i] < 0 || probabilities[i] > 100) {
            usage(argv[0]);
        }
    }

    printf("[TEACHER] Starting the class with %d students.\n", num_students);
    sethandler(t_handler, SIGUSR1);

    pid_t* student_pids = create_children(num_students, probabilities, t);

    teacher_work(student_pids, num_students);

    free(student_pids);
    printf("[TEACHER] All students have completed their tasks. Exiting.\n");
    return EXIT_SUCCESS;
    }