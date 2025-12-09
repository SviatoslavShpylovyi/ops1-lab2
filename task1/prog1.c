#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

void sig_handler(int sig) {
    last_signal = sig;  
}

void sig2_handler(int sig) {
    printf("exit received, exiting child [%d]\n", getpid());
    exit(EXIT_SUCCESS);
}

void sethandler(void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sigchld_handler(int sig) {
    pid_t pid;
    while (1) {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            return;
        if (pid <= 0) {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}

void parent_work(sigset_t oldmask, pid_t* pids, int n) {
    int count = 0;
    while (1) {
        last_signal = 0;
        while (last_signal != SIGUSR1)  
            sigsuspend(&oldmask);
        printf("[(%d)] *\n", count);
        if (count >= 100) { 
            for (int i = 0; i < n; i++) {
                kill(pids[i], SIGUSR2); 
            }
            return;
        }
        count++;
    }
}
void child_work() {
    srand(time(NULL) * getpid());
    int s = rand() % (200 - 100 + 1) + 100;
    printf("[%d] rand num is [%d] ms\n", getpid(), s);


    sethandler(sig2_handler, SIGUSR2);

    struct timespec time = {0, s * 1000000};  
    for (int i = 1; i <= 30; i++) {
        nanosleep(&time, NULL);
       
        if (kill(getppid(), SIGUSR1)) 
            ERR("kill");
    }

    pause();
    printf("[%d] Exiting...\n", getpid());
    return;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: <NUMBER OF CHILDREN>: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "USAGE: <NUMBER OF CHILDREN>: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    
    sethandler(sigchld_handler, SIGCHLD);
    sethandler(sig_handler, SIGUSR1);
    sethandler(sig_handler, SIGUSR2);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    pid_t pid;
    pid_t pids[n];
    for (int i = 0; i < n; ++i) {
        if ((pid = fork()) < 0) {
            fprintf(stderr, "ERROR on children\n");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            child_work();
            exit(EXIT_SUCCESS);
        }
        pids[i] = pid;
    }

    parent_work(oldmask, pids, n);
    while (wait(NULL) > 0);

    printf("Parent exits\n");
    return EXIT_SUCCESS;
}
