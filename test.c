#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>


typedef struct Pcb{
	int _pid;
	int _remaining_time_quantum;
	int _remaining_io_time;
}Pcb;


typedef struct ProcQ{
	struct ProcQ*	_next;
	struct Pcb*	_pcb;
}ProcQ;

typedef struct msgNode{
	long		_msgType;
	int		_pid;
	int		_ioTime;
}msgNode;



int global_tick;

void pAlarmHandler(int signo);

int main(void){
	
	int pid;

	msgNode msg;

	struct sigaction sa;
	struct itimerval timer;

	//init
	global_tick = 0;

	int i;
	for(i=0;i<10;i++){
		Pcb* tmp;
		pid = fork();
		if(pid<0){
			//error code
			printf("Fork Error\n");
			return 0;
		}
		else if(pid == 0){
			//child code
			child_action();
			return 0;
		}
		else{
			printf("child process is created : pcb[%d]: %d\n",i,pid);
			tmp = (Pcb*) malloc(sizeof(Pcb));
			memset(tmp,0,sizeof(Pcb));
			tmp->_pid = pid;
			tmp->_remaining_time_quantum = 5;
			AddNode(&runq, tmp);
			pcb[i] = tmp;
		} 
	}
	 sa.sa_handler = pAlarmHandler;
	 sigemptyset(&sa.sa_mask);
	 sigaction(SIGALRM, &sa, 0); 
	
	
	//timer
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 100000;

	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 100000;
	setitimer(ITIMER_REAL, &timer, NULL);
	
	//MsgQ
	key_t msgqid;

	if((msgqid = msgget((key_t)QUEUE_KEY, IPC_CREAT|0644)) == -1){
		printf("msgget err \n");
		exit(0);
	}
	while(1){
		if(msgrcv(msgqid, &msg, sizeof(msg), 4, 0) == -1)
			continue;
		else{
			//????
			//printf("msg receive \n");

			printf("msg_pid : %d \n",msg._pid);
			if(runq !=NULL)
				printf("curr runq pid : %d\n",runq->_pcb->_pid);
		
			//if(msg._pid != runq->_pcb->_pid){
			//	printf("message ignore\n");
			//}
	
			//if(msg._pid == runq->_pcb->_pid){
			//	printf("runq to waitq \n");
			//	AddNode(&waitq, tmp->_pcb);
			//	free(tmp);
			//}

	
		}
	}
}

void pAlarmHandler(int signo){
	printf("-----------------------------------------------\n");
	global_tick++;
	printf("global_tick : %d\n",global_tick);

	///이후에????????


}
