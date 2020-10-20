#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t g_mutex;
pthread_cond_t g_cond /*=PTHREAD_MUTEX_INITIALIZER*/; //��������,���ú���г�ʼ��


void threadFun1(void)

{
	int i;

	pthread_mutex_lock(&g_mutex); //1

	pthread_cond_wait(&g_cond,&g_mutex); //��g_cond���ź�,������

	for( i = 0;i < 2; i++ ){
	printf("thread threadFun1.\n");

	sleep(1);

	}

	pthread_cond_signal(&g_cond);

	pthread_mutex_unlock(&g_mutex);

}

int main(void)
{
	pthread_t id1; //�̵߳ı�ʶ��

	pthread_t id2;

	pthread_cond_init(&g_cond,NULL); //Ҳ���Գ��������ʼ��

	pthread_mutex_init(&g_mutex,NULL); //���������ʼ��

	int i,ret;

	ret = pthread_create(&id1,NULL,(void *)threadFun1, NULL);

	if ( ret!=0 ) { //��Ϊ0˵���̴߳���ʧ��

		printf ("Create pthread1 error!\n");

		exit (1);

	}

	sleep(5); //�ȴ����߳��ȿ�ʼ

	pthread_mutex_lock(&g_mutex); //2

	pthread_cond_signal(&g_cond); //������ʼ�ź�,ע������Ҫ�ȵ����߳̽���ȴ�״̬�ڷ��ź�,������Ч

	pthread_mutex_unlock(&g_mutex);

	pthread_join(id1,NULL);

	pthread_cond_destroy(&g_cond); //�ͷ�

	pthread_mutex_destroy(&g_mutex); //�ͷ�

	return 0;

}
