#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;
void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sig_handler(int sig) { last_signal = sig; }

void usage(char* prog_name){
    fprintf(stderr, "USAGE: %s -N", prog_name);
    exit(EXIT_FAILURE);
}
void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
void write_to_file(int count){
    char buff[69];
    snprintf(buff, sizeof(buff), "%d.txt", getpid());
    int fd = open(buff, O_CREAT|O_WRONLY, 0777 );
    snprintf(buff, sizeof(buff), "%d\n", count);
    write(fd, buff, sizeof(int));
    close(fd);
}
void do_work(int index) {
    sethandler(sig_handler, SIGUSR1);
    sethandler(sig_handler, SIGUSR2);
    sethandler(sig_handler, SIGINT);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    //sigaddset(&mask, SIGUSR2);

    // Block SIGUSR1 and SIGUSR2 in the process mask
     if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1)
           ERR("sigprocmask");

    srand(getpid());
    int count = 0;

    for (;;) {
        last_signal = 0;
        while (last_signal!=SIGUSR1 && last_signal!=SIGINT) {
            sigsuspend(&oldmask);
        }
        if(last_signal == SIGINT){
            break;
        }

        while (last_signal!=SIGUSR2 && last_signal!=SIGINT) {
            int delay = 100 + rand() % 101;  // 100–200 ms
            sleep_ms(delay);
            if(last_signal ==SIGINT){
                break;
            }
            ++count;
            printf("[%d] with index %d and counter %d\n",
                   getpid(), index, count);
            fflush(stdout);
        }
        if(last_signal ==SIGINT){
            break;
        }
    }
    write_to_file(count);
}
void create_processes(int n, pid_t*proc_list){
    pid_t pid;
    for(int i =0; i<n; i++){
        pid = fork();
        if(pid==-1){
            fprintf(stderr, "ERROR in fork");
            exit(EXIT_FAILURE);
        }
        if(pid==0){ // child 
            do_work(i);
            exit(EXIT_SUCCESS);
        }
        proc_list[i] = pid;
    }
    printf("parent[%d]\n", getpid());
    sethandler(sig_handler, SIGUSR1);
    sethandler(sig_handler, SIGUSR2);
    srand(getpid());
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    int child = 1;
    int active_child = 0;
    kill(proc_list[0], SIGUSR1);
    while (sigsuspend(&oldmask)){
            if(last_signal == SIGUSR1){
                kill(proc_list[child],SIGUSR1);
                kill(proc_list[active_child],SIGUSR2);
                active_child = child;
                child = (child+1)%n;
            }
        }

}
int main(int argc, char** argv){
    if(argc!=2){
        usage(argv[0]);
    }
    int workers_num = atoi(argv[1]);
    if(workers_num<=0)
        usage(argv[0]);
    pid_t processes[workers_num];
    create_processes(workers_num,processes );

}