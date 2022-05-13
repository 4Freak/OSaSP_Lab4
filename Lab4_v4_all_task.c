
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>


#define AM_OF_PROCESSES 9
#define AM_OF_SIGNALS 101
#define SHARED_MEMORY_OBJECT_NAME "shmem_proc_tree" 
#define IS_CREATED_SEM_NAME "/is_created_semaphore"
#define ERROR_CHECK(value, error_value, err_msg, exit_code) {\
  if ((value) < (error_value)){\
    perror((err_msg));\
    exit((exit_code));\
  }\
}

typedef struct _TChild
{
  pid_t selfPid;
  pid_t parentPid;
  int childCount;
  struct sigaction act;
  pid_t selfPgid;
  int elInGroup;
}TChild;

#define SHARED_MEMORY_OBJECT_SIZE (AM_OF_PROCESSES * sizeof(TChild))


//Fillig shared memory with future tree structure
int FillSharedMemory(TChild* shmemAddr);

//Create process tree according tree structure in shared memory
int CreateTree(TChild* shmemAddr, int childNum);

TChild* shmemAddr;
int numOfSig = 0;

int main()
{
  int iResult;
  //Create shared memory
  int shmem = shm_open(SHARED_MEMORY_OBJECT_NAME, O_CREAT | O_RDWR, 0777);
  ERROR_CHECK(shmem, 0, "Cannot create shared memory", -1); //MB fix
  iResult = ftruncate(shmem, SHARED_MEMORY_OBJECT_SIZE+1);
  ERROR_CHECK(iResult, 0, "Cannot 'allocate' shared memory", -1);
  shmemAddr = mmap(0, SHARED_MEMORY_OBJECT_SIZE+1, PROT_WRITE|PROT_READ, MAP_SHARED, shmem, 0);
  ERROR_CHECK(shmemAddr, 0, "Cannot map shared memory", -1);

  //Create semaphore
  sem_t *semIsCreated = sem_open(IS_CREATED_SEM_NAME, O_CREAT, 0777, 0);
  if (semIsCreated == SEM_FAILED){
    perror("Could not create semaphore");
    exit(-1);
  } 

  //Fill table
  FillSharedMemory(shmemAddr);

  //Create tree
  CreateTree(shmemAddr, 0);

  //Debug output
  TChild treeStructure[AM_OF_PROCESSES];
  memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
  for (int i = 0; i < AM_OF_PROCESSES; i++){
    printf("Child %d, Pid: %04d, PPid: %04d, ChildCount: %d, Group: %d, GroupEl: %d\n", i, treeStructure[i].selfPid, treeStructure[i].parentPid, treeStructure[i].childCount, treeStructure[i].selfPgid, treeStructure[i].elInGroup); 
  }

  iResult = kill(treeStructure[1].selfPid, SIGUSR1);
  ERROR_CHECK(iResult, 0, "Can not send SIGUSR1", -1);

  iResult = waitpid(treeStructure[1].selfPid, NULL, 0);
  ERROR_CHECK(iResult, 0, "Can not wait pid", -1);

  //Free semaphore
  iResult = sem_close(semIsCreated);
  ERROR_CHECK(iResult, 0, "Cannot close semaphore", -1);

  //Free shared memory
  iResult = munmap(shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
  ERROR_CHECK(iResult, 0, "Cannot unmap shared memory", -1);
  iResult = close(shmem);
  ERROR_CHECK(iResult, 0, "Cannot 'close' shared memory", -1);
  iResult = shm_unlink(SHARED_MEMORY_OBJECT_NAME);
  ERROR_CHECK(iResult, 0, "Cannot unlink shared memory", -1);
  printf("No errors\n %d", getpid());
  return 0;
}

void ChildSIGUSR1Recive(int signal, siginfo_t* signalInfo, void* context);
void ChildSIGUSR1ReciveSend(int signal, siginfo_t* signalInfo, void* context);
void ChildSIGTERMRecive(int signal, siginfo_t* signalInfo, void* context);

int FillSharedMemory(TChild* shmemAddr)
{
  TChild treeStructure[AM_OF_PROCESSES];
  treeStructure[0].selfPid = getpid();
  int i = 0;
  int iResult;
  
  //Filling child count
  for (; i < AM_OF_PROCESSES; i++){
    treeStructure[i].childCount = 0;
  }
  treeStructure[0].childCount = 1;
  treeStructure[1].childCount = 4;
  treeStructure[5].childCount = 3;
  
  //Filling ket group element
  for (i = 2; i < 6; i++){
    treeStructure[i].elInGroup = 2;
  }
  treeStructure[1].elInGroup = -1;
  treeStructure[2].elInGroup = -1;
  for (i = 7; i < AM_OF_PROCESSES; i++){
    treeStructure[i].elInGroup = 6;
  }
  treeStructure[6].elInGroup = -1;

  //Filling sigaction structure
  sigset_t maskSIGUSR1;
  iResult = sigemptyset(&maskSIGUSR1);
  ERROR_CHECK(iResult, 0, "Can not empty mask", -1);
  iResult = sigaddset(&maskSIGUSR1, SIGUSR1);
  ERROR_CHECK(iResult, 0, "Cannot set mask", -1);
  for (i = 0; i < AM_OF_PROCESSES; i++){
    treeStructure[i].act.sa_sigaction = ChildSIGUSR1Recive;
    treeStructure[i].act.sa_mask = maskSIGUSR1;
    treeStructure[i].act.sa_flags = SA_SIGINFO;
  }
  treeStructure[1].act.sa_sigaction = ChildSIGUSR1ReciveSend;
  treeStructure[5].act.sa_sigaction = ChildSIGUSR1ReciveSend;
  treeStructure[8].act.sa_sigaction = ChildSIGUSR1ReciveSend;

  memcpy(shmemAddr, treeStructure, SHARED_MEMORY_OBJECT_SIZE); //May be 1 mistake here
  return 0;
}

void ChildSIGUSR1Recive(int signal, siginfo_t* signalInfo, void* context)
{
  struct timeval TV;
  if (gettimeofday(&TV, NULL) < 0)
		perror("Can not get current time\n");
	else {
		long long MKSec = TV.tv_sec * 1000000 + TV.tv_usec;

    TChild treeStructure[AM_OF_PROCESSES];
    memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
    int childNum = 0;
    while (treeStructure[childNum].selfPid != getpid()){
      childNum++;
    }
    printf("Num: %d Pid: %d PPid: %d Get  SIGUSR1 Time: %lld\n", childNum, getpid(), getppid(), MKSec);
  }
}

void ChildSIGUSR1ReciveSend(int signal, siginfo_t* signalInfo, void* context)
{
  struct timeval TV;
  numOfSig++;
  if (gettimeofday(&TV, NULL) < 0)
		perror("Can not get current time\n");
	else {
		long long MKSec = TV.tv_sec * 1000000 + TV.tv_usec;

    TChild treeStructure[AM_OF_PROCESSES];
    memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
    int childNum = 0;
    while (treeStructure[childNum].selfPid != getpid()){
      childNum++;
    }
    printf("Num: %d Pid: %d PPid: %d Get  SIGUSR1 Time: %lld\n", childNum, getpid(), getppid(), MKSec);
    
    int recvChildNum = childNum + 1;
    if (recvChildNum == AM_OF_PROCESSES){
      recvChildNum = 1;
    }
    if (gettimeofday(&TV, NULL) < 0)
		  perror("Can not get current time\n");
	  else {
      MKSec = TV.tv_sec * 1000000 + TV.tv_usec;
      int iResult;
      if (childNum == 1 && numOfSig >= AM_OF_SIGNALS + 1){
        printf("Send first sigterm"); //Debug
        iResult = kill(0, SIGTERM);
        ERROR_CHECK(iResult, 0, "Can not send SIGTERM", -1);
      }else{
        iResult = kill(-1 * treeStructure[recvChildNum].selfPid, SIGUSR1);
        ERROR_CHECK(iResult, 0, "Can not send SIGUSR1", -1);
        printf("Num: %d has number: %d", childNum, numOfSig);
        printf("Num: %d Pid: %d PPid: %d Send SIGUSR1 Time: %lld\n", childNum, getpid(), getppid(), MKSec);
      }
    }
  }
}

void ChildSIGTERMRecive(int signal, siginfo_t* signalInfo, void* context)
{
  TChild treeStructure[AM_OF_PROCESSES];
  memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
  int childNum = 0;
  int iResult;
  while (treeStructure[childNum].selfPid != getpid()){
    childNum++;
  }
  if (treeStructure[childNum].childCount != 0){
    iResult = kill(-1 * (treeStructure[childNum+1].selfPid), SIGTERM);
    ERROR_CHECK(iResult, 0, "Can not send SIGTERM", -1);
    int i = 0;
    for(; i < treeStructure[childNum].childCount; i++){
      if(waitpid(treeStructure[childNum + i + 1].selfPid, NULL, 0) < 0)
        perror("Cannot wait child");
    }
  }
  printf("Pid %d PPid: %d finish the work after %d SIGUR1\n", getpid(), getppid(), numOfSig);
  exit(0);
}

int FillChildElement(TChild* shmemAddr, int childNum);

int CreateTree(TChild* shmemAddr, int childNum)
{
  TChild treeStructure[AM_OF_PROCESSES];
  memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
  sem_t *semIsCreated = sem_open(IS_CREATED_SEM_NAME, 0);
  if (semIsCreated == SEM_FAILED){
    perror("Could not open semaphore");
    exit(-1);
  } 

  //Create childs in cycle
  int i = 0, iResult;
  int isParent = 1, isMyChild;
  int parNum = childNum;
  while (i < treeStructure[parNum].childCount && isParent){
    isParent = 0;
    childNum++;
    treeStructure[childNum].selfPid = fork();
    switch(treeStructure[childNum].selfPid){
      case -1:
        perror("Child process could not be created\n");
        exit(-1);
        break;
      case 0:
        FillChildElement(shmemAddr, childNum);

        //If process has childs then call function recursively
        if (treeStructure[childNum].childCount != 0){
          CreateTree(shmemAddr, childNum);
        } 

        //Set semaphore to parent process
        memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
        treeStructure[0].parentPid = getppid();
        memcpy(shmemAddr, treeStructure, SHARED_MEMORY_OBJECT_SIZE);
        iResult = sem_post(semIsCreated);
        ERROR_CHECK(iResult, 0, "Could not set semaphore", -1);        
        while(1); //MB should delete
        break;
      default:
        isParent = 1; 
        isMyChild = 0;
        while (!isMyChild){
          iResult = sem_wait(semIsCreated);
          ERROR_CHECK(iResult, 0, "Can not wait semaphore", -1);
          memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);

          //Check is correct process recive semaphore
          if (treeStructure[0].parentPid == getpid()){
            isMyChild = 1;
          }else{
            iResult = sem_post(semIsCreated);
            ERROR_CHECK(iResult, 0, "Could not set semaphore", -1);
            sleep(1);
          }
        }
        break;
    }
    i++;
  }
  return 0;
}

int FillChildElement(TChild* shmemAddr, int childNum)
{
  TChild treeStructure[AM_OF_PROCESSES];
  memcpy(treeStructure, shmemAddr, SHARED_MEMORY_OBJECT_SIZE);
  int iResult;

  treeStructure[childNum].selfPid = getpid();
  treeStructure[childNum].parentPid = getppid();

  //Set process group
  if (treeStructure[childNum].elInGroup == -1){
    iResult = setpgid(0, 0);
  }else{
    iResult = setpgid(0, treeStructure[treeStructure[childNum].elInGroup].selfPgid);
  }
  ERROR_CHECK(iResult, 0, "Can not create group", -1);
  treeStructure[childNum].selfPgid = getpgid(0);
  ERROR_CHECK(treeStructure[childNum].selfPgid, 0, "Can not get self group", -1);

  //Set signal handler
  iResult = sigaction(SIGUSR1, &treeStructure[childNum].act, NULL);
  ERROR_CHECK(iResult, 0, "Can not create SIGUSR1 handler", -1);
  sigset_t maskSIGTERM = treeStructure[childNum].act.sa_mask;
  iResult = sigaddset(&maskSIGTERM, SIGTERM);
  ERROR_CHECK(iResult, 0, "Can not set mask", -1);
  struct sigaction act;
  act.sa_sigaction = ChildSIGUSR1Recive;
  act.sa_mask = maskSIGTERM;
  act.sa_flags = SA_SIGINFO;
  iResult = sigaction(SIGTERM, &act, NULL);
  ERROR_CHECK(iResult, 0, "Can not set SIGTERM handler", -1);

  memcpy(shmemAddr, treeStructure, SHARED_MEMORY_OBJECT_SIZE);
  return 0;
}