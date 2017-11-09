/*
io_time : 3
cpu_time : 4
time quantum : 2
*/

#include "procqADT.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>

#define QUEUE_KEY 3215

typedef struct msgNode{
	long msgType;
	int pid;
	int io_time;
}msgNode;

struct Pcb* pcbs[10];
struct Pcb* present;
struct msgNode msg;

struct Procq* runq;
struct Procq* waitq;

int global_tick = 0;
int cpu_time[10];
int remain_cpu_time;
int remain_io_time;

void PrintQueue(Procq *q);
void pAlarmHandler(int signo);
void cAlarmHandler(int signo);
void updateWaitq();
void child_action(int cpu_time);
void io_action();
void searchShort();
Pcb* scheduler();
key_t msgpid;

int main(){
	int pid, i;
	struct sigaction old_sa, new_sa;
	struct itimerval new_timer, old_timer;
	Pcb* next = NULL;
	
	runq = (Procq*)malloc(sizeof(Procq));
	waitq = (Procq*)malloc(sizeof(Procq));
	runq = createProcq();
	waitq = createProcq();
		
	srand((int)time(NULL));
	for(i = 0; i<10; i++){
//		srand(time(NULL));
		cpu_time[i] = (rand() % 10) + 1;
		printf("create cpu_time : %d\n",cpu_time[i]);	
	}

	for(i = 0; i<10; i++){
		pid = fork();
		if(pid < 0){
			printf("fork error\n");
			exit(0);
		}
		else if(pid == 0){
			//child
			child_action(cpu_time[i]);
			exit(0);
		}
		else{
			//parent
			pcbs[i] = (Pcb*)malloc(sizeof(Pcb));
			memset(pcbs[i],0,sizeof(Pcb));
			pcbs[i]->pid = pid;
			pcbs[i]->remain_cpu_time = cpu_time[i];
//			pcbs[i]->remain_time_quantum = 2;
			AddProcq(runq, pcbs[i]);
		}
	}
	
	//searchShort();

	memset(&new_sa,0,sizeof(sigaction));
	new_sa.sa_handler = &pAlarmHandler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	//timer
	new_timer.it_value.tv_sec = 0;
	new_timer.it_value.tv_usec = 100000;
	new_timer.it_interval.tv_sec = 0;
	new_timer.it_interval.tv_usec = 100000;
	//답답해서 잠시 바꿈
	//new_timer.it_value.tv_sec = 1;
	//new_timer.it_value.tv_usec = 0;
	//new_timer.it_interval.tv_sec = 1;
	//new_timer.it_interval.tv_usec = 0;
	
	setitimer(ITIMER_REAL, &new_timer, &old_timer);

//	key_t msgpid;
	if((msgpid = msgget((key_t)QUEUE_KEY, IPC_CREAT|0644)) == -1){
		printf("msgget error \n");
		exit(0);
	}
	printf("msg (key : %d, id : %d) created\n",QUEUE_KEY, msgpid);

	while(1){
		if(msgpid > 0){
			if((msgrcv(msgpid, &msg, (sizeof(msg) - sizeof(long)), 0, 0)) > 0){
				for(i = 0; i<10; i++){
					if(pcbs[i]->pid == msg.pid){
						pcbs[i]->remain_io_time = msg.io_time;
//						pcbs[i]->remain_time_quantum = 2;
						RemoveProcq(runq, pcbs[i]);
						AddProcq(waitq, pcbs[i]);
						printf("global_tick (%d) proc(%d) sleep (%d) ticks\n", global_tick, pcbs[i]->pid, pcbs[i]->remain_io_time);
					}
				}
				next = scheduler();
				if(next != NULL){
					present = next;
					printf("global_tick(%d) schedule proc(%d)\n",global_tick, present->pid);
				}
				
			}
		}
	}
	exit(0);
}

void pAlarmHandler(int signo){
	Pcb* next = NULL;
	global_tick++;
	//숫자 확인좀....
	if(global_tick >= 60){
		for(int i = 0; i<10; i++){
			printf("parent killed child)(%d)\n",pcbs[i]->pid);
			kill(pcbs[i]->pid, SIGKILL);		
		}
		kill(getpid(), SIGKILL);
		exit(0);
	}

	//waitq update
	if(waitq->count != 0)
		updateWaitq();
	if(present == NULL){
		present = scheduler();
		printf("global_tick(%d) schedule proc(%d)\n",global_tick, present->pid);
	}
//	else{
//		present->remain_time_quantum--;
//		if(present->remain_time_quantum == 0){
//			RemoveProcq(runq, present);
//			AddProcq(runq, present);
//			if((next = scheduler()) != NULL){
//				present = next;
//				printf("global_tick(%d) schedule proc(%d)\n",global_tick, present->pid);
//			}
//		}	
//	}
	printf("runq : ");
	PrintQueue(runq);
	printf("waitq : ");
	PrintQueue(waitq);

	kill(present->pid, SIGALRM);
}

void cAlarmHandler(int signo){
	printf("proc(%d) remain_cpu_time : %d\n",getpid(), remain_cpu_time);
	int origin;
	if(remain_cpu_time > origin)
		origin = remain_cpu_time;
	remain_cpu_time--;
	if(remain_cpu_time == 0){
//		printf("start io_action\n");
		io_action();
		remain_cpu_time = origin;
		origin = 0;	
	}
	return;
}

void updateWaitq(){
	ProcqNode* cur = NULL, *next = NULL;
	Pcb* tmpPcb = (Pcb*)malloc(sizeof(Pcb));
	tmpPcb = NULL;
	if(waitq->count != 0){
		for(cur = waitq->head; cur!=NULL; cur=next){
			next = cur->next;
			tmpPcb = cur->pcb;
			cur->pcb->remain_io_time--;
			if(cur->pcb->remain_io_time == 0){
//				printf("end proc (%d)\n", cur->pcb->pid);
				RemoveProcq(waitq, tmpPcb);
				AddProcq(runq,tmpPcb);
			}		
		}
	}
}

void child_action(int cpu_time){
	struct sigaction old_sa, new_sa;
//	printf("into child_Action\n");
	
	remain_cpu_time = cpu_time;
//	remain_io_time = 3;

	memset(&new_sa,0,sizeof(struct sigaction));
	new_sa.sa_handler = &cAlarmHandler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	msgpid = (key_t)malloc(sizeof(key_t));
	msgpid = -1;

	while(1){
	}
}

void io_action(){
	int mspid, ret;
	printf("child (%d) send msg\n",getpid());
	if((mspid = msgget((key_t)QUEUE_KEY, IPC_CREAT|0644)) == -1){
		printf("msgget error \n");
		exit(0);
	}
	memset(&msg, 0, sizeof(msg));
	msg.pid = getpid();
	msg.io_time = 2;
	msg.msgType = 1;

	if((msgsnd(mspid, &msg, (sizeof(msg) - sizeof(long)), IPC_NOWAIT)) == -1){
		printf("msgsnd error \n");
		exit(0);
	}
}


Pcb* scheduler(){
	//roundrobin
	if(runq->count != 0)
		return runq->head->pcb;
	else
		return NULL;
}

void PrintQueue(Procq* q){
	ProcqNode* tmp = (ProcqNode*)malloc(sizeof(ProcqNode));
 	tmp = q->head;
	if(tmp == NULL){
		printf("-\n");
		return;
	}
	do{
		printf("%d  ",tmp->pcb->pid);
		tmp = tmp->next;
	}while(tmp != NULL);
	printf("\n");
}


