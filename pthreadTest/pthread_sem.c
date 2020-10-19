#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
 
#define MAXSIZE 10
 
int stack[MAXSIZE];
 
int size =0;
sem_t sem;
 
void privide_data(void)
{
  int i;
  for(i =0;i<MAXSIZE;++i)
  {
    stack[i] = i;
    sem_post(&sem);	//sem_post本身不会进行任务调度
	//printf("%d%d%d%d\r\n",i,i,i,i);
	usleep(1);
  }
}
 
void handle_data(void)
{
  int i;
  while((i = size ++) <MAXSIZE)
  {
    sem_wait(&sem);		//等privider释放一个信号量，才会打印下面的信息
    printf("cross : %d X %d = %d \n",stack[i],stack[i],stack[i] * stack[i]);
  }
}
 
int main()
{
  pthread_t privider,handler;
  sem_init(&sem,0,0);
  pthread_create(&privider,NULL,(void *)&privide_data,NULL);
  pthread_create(&handler,NULL,(void *)&handle_data,NULL);
  pthread_join(privider,NULL);
  pthread_join(handler,NULL);
  sem_destroy(&sem);
 
  return 0;
}

