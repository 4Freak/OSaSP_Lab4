
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

int FillSharedMemory(TChild* shmemAddr);
int CreateTree(TChild* shmemAddr, int childNum);

int main()
{
  int iResult;
  //Create shared memory
  int shmem = shm_open(SHARED_MEMORY_OBJECT_NAME, O_CREAT | O_RDWR, 0777);
  ERROR_CHECK(shmem, 0, "Cannot create shared memory", -1); //MB fix
  iResult = ftruncate(shmem, SHARED_MEMORY_OBJECT_SIZE+1);
  ERROR_CHECK(iResult, 0, "Cannot 'allocate' shared memory", -1);
  TChild* shmemAddr = mmap(0, SHARED_MEMORY_OBJECT_SIZE+1, PROT_WRITE|PROT_READ, MAP_SHARED, shmem, 0);
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

//Fillig shared memory with future tree structure
int FillSharedMemory(TChild* shmemAddr)
{
  TChild treeStructure[AM_OF_PROCESSES];
  treeStructure[0].selfPid = getpid();
  int i = 0;
  for (; i < AM_OF_PROCESSES; i++){
    treeStructure[i].childCount = 0;
  }
  treeStructure[0].childCount = 1;
  treeStructure[1].childCount = 4;
  treeStructure[5].childCount = 3;
  for (i = 2; i < 6; i++){
    treeStructure[i].elInGroup = 2;
  }
  treeStructure[1].elInGroup = -1;
  treeStructure[2].elInGroup = -1;
  for (i = 7; i < AM_OF_PROCESSES; i++){
    treeStructure[i].elInGroup = 6;
  }
  treeStructure[6].elInGroup = -1;

  //treeStructure[0].act = ...
  memcpy(shmemAddr, treeStructure, SHARED_MEMORY_OBJECT_SIZE); //May be 1 mistake here
  return 0;
}

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
        memcpy(shmemAddr, treeStructure, SHARED_MEMORY_OBJECT_SIZE);

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