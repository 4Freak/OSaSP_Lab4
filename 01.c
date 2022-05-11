#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>

#define AM_OF_PIDS 2
#define IS_CREATED_SEM_NAME "/is_created_semaphore"

//Procedure to run it to child nodes
int ChildProcMain(int childInd);

//SIGUSR2 paraent handler
void ParentSIGUSR2(int signal, siginfo_t* signalInfo, void* context);

int main()
{
  //Semaphore to chack is child processes created
  sem_t *semIsCreated = sem_open(IS_CREATED_SEM_NAME, O_CREAT, 0777, 0);
  if (semIsCreated == SEM_FAILED){
    perror("Could not create semaphore");
    return -1;
  } 

  //Array of child processes
  pid_t childPids[AM_OF_PIDS];
  int i = 0;
  int childCount = 0, isParent = 1;

  //While less then amount of child processes and it's Parent process do create new child process
  while (i < AM_OF_PIDS && isParent){
    isParent = 0;
    childPids[i] = fork();
    switch(childPids[i]){
      case -1: 
        perror("Child process could not be created\n");
        return -2;
        break;
      case 0:
        ChildProcMain(i);
        break;
      default:
        isParent = 1;
        childCount++;
        if (sem_wait(semIsCreated) < 0){
          perror("Do not wait semaphore");
          return -3;
        }
        break;
    }
    i++;
  }

  //Delete semaphore
  if (sem_close(semIsCreated) < 0){
    perror("Cannot close semaphore");
    return -4;
  }

  //Ingnore SIGUSER1 signals
  sigset_t mask; 
  if (sigemptyset(&mask) < 0){
    perror("Cannot empty mask");
    return -5;
  }
  if (sigaddset(&mask, SIGUSR1)){
    perror("Cannot set mask");
    return -6;
  }
  if (sigdelset(&mask ,SIGINT)){
    perror("Cannot set mask");
    return -6;
  }
  if (sigprocmask(SIG_BLOCK, &mask, 0) < 0){
    perror("Cannot ingnore SIGUSR1 in parent");
    return -7;
  }

  //Set mask to sigaction
  if (sigdelset(&mask, SIGUSR1)){
    perror("Cannot delete element in mask");
    return -8;
  }
  if (sigaddset(&mask, SIGUSR2)){
    perror("Cannnot set mask");
    return -6; 
  }
  struct sigaction act;
  act.sa_sigaction = ParentSIGUSR2;
  act.sa_mask = mask;
  act.sa_flags = SA_SIGINFO;
  if (sigaction(SIGUSR2, &act, NULL)){
    perror("Cannot create SIGUSER2 handler in parent");
    return -9;
  }

  //Start sending signals
  struct timeval TV;
  if (gettimeofday(&TV, NULL) < 0)
    perror("Can not get current time\n");
  else {
	  int MSec = TV.tv_usec / 1000;
    if (kill(0, SIGUSR1)){
      perror("Cannot send SIGUSER1 to childs");
      return -10;
    } 
    printf("Parrent Pid: %d                  Time: %03d send SIGUSER1 to childs\n", getpid(), MSec); 
  }
  while(1);

  return 0;
};  

void ParentSIGUSR2(int signal, siginfo_t* signalInfo, void* context)
{
	struct timeval TV;
	
	if (gettimeofday(&TV, NULL) < 0)
		perror("Can not get current time\n");
	else {
		int MSec = TV.tv_usec / 1000;
	
		printf("Parrent Pid: %d                  Time: %03d Get  SIGUSR2 from %d\n", getpid(), MSec, signalInfo->si_pid);	

    if (gettimeofday(&TV, NULL) < 0)
      perror("Can not get current time\n");
    else {
		  int MSec = TV.tv_usec / 1000;
      if (kill(0, SIGUSR1)){
        perror("Cannot send SIGUSER1 to childs");
      } 
      printf("Parrent Pid: %d                  Time: %03d Send SIGUSER1 to childs\n", getpid(), MSec); 
    }
  }
}

//SIGUSRT1 signal handler for child 
void ChildUSR1(int signal, siginfo_t* signalInfo, void* context);

int ChildProcMain(int childInd)
{
  //Get semaphore
  sem_t *semIsCreated = sem_open(IS_CREATED_SEM_NAME, 0);
  if (semIsCreated == SEM_FAILED){
    perror("Could not open semaphore");
    return -1;
  } 

  //Set mask to SIGNUSR1
  sigset_t mask; 
  if (sigemptyset(&mask) < 0){
    perror("Cannot empty mask");
    return -5;
  }
  if (sigaddset(&mask, SIGINT) < 0){
    perror("Cannot set mask");
    return -6;
  }
  if (sigaddset(&mask, SIGUSR1)){
    perror("Cannot set mask");
    return -6;
  }

  struct sigaction act;
  act.sa_sigaction = ChildUSR1;
  act.sa_mask = mask;
  act.sa_flags = SA_SIGINFO;
  if (sigaction(SIGUSR1, &act, NULL)){
    perror("Cannot create SIGUSER1 handler in child");
    return -9;
  }

  //Block SIGUSR2
  if (sigemptyset(&mask) < 0){
    perror("Cannot empty mask");
    return -5;
  }
  if (sigaddset(&mask, SIGUSR2) < 0){
    perror("Cannot set mask");
    return -6;
  }
  if (sigprocmask(SIG_BLOCK, &mask, 0) < 0){
    perror("Cannot ingnore SIGUSR2 in child");
  }  

  //Set semaphore to 'Child ready'
  if (sem_post(semIsCreated) < 0){
    perror("Could not set semaphore");
    return -1;
  }

  while(1);
  return 0;
}

void ChildUSR1(int signal, siginfo_t* signalInfo, void* context)
{
	struct timeval TV;
	
	if (gettimeofday(&TV, NULL) == -1)
		perror("Can not get current time\n");
	else {
		int MSec = TV.tv_usec / 1000;
	
		printf("Pid:         %d Parent pid: %d Time: %03d Get  SIGUSR1\n", getpid(), getppid(), MSec);	

    if (gettimeofday(&TV, NULL) < 0)
      perror("Can not get current time\n");
    else {
		  int MSec = TV.tv_usec / 1000;
      if (kill(getppid(), SIGUSR2)){
        perror("Cannot send SIGUSER2 to parent");
      } 
      printf("Pid:         %d Parent pid: %d Time: %03d Send SIGNUSR2\n", getpid(), getppid(), MSec);
    }    
  }
}
