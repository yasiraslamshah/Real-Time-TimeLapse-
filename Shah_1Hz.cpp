/*Yasir Aslam Shah
Code Reffered from ProfesSor Sam Siewart
RTES 5623
Timelapsing at 1hZ and 10Hz
Summer2018*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv/highgui.h>
#include <sstream>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>
#include <iostream>
#include <syslog.h>
#include <sys/time.h>
#include <fstream>
#include <errno.h>

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (3)
#define TRUE (1)
#define FALSE (0)

#define HRES 640
#define VRES 480
using namespace cv;
using namespace std;
int abortTest=FALSE;
int abortS1=FALSE, abortS2=FALSE;
int frames=0, savedframe=0;
sem_t semS1, semS2;

struct timeval start_time_val;
struct timeval start_time_val1;
struct timespec timestamp;
double difference;
void *sequencer(void *threadp);
void *service1(void *threadp);
void *service2(void *threadp);

CvCapture * capture;
IplImage* frame;

//threads
pthread_t sequencer_thread;
pthread_t service1_thread;
pthread_t service2_thread;
/*Thread Attributes*/
pthread_attr_t sequencer_attr;
pthread_attr_t service1_attr;
pthread_attr_t service2_attr;
pthread_attr_t main_thread_attr;
//Thread param
struct sched_param sequencer_param;
struct sched_param service1_param;
struct sched_param service2_param;
struct sched_param main_param;
char jitter[]="hi.csv";
pthread_mutex_t yasir;
typedef struct
{
    int threadIdx;
    unsigned long long sequencePeriods;
} threadParams_t;

Mat a;
void *Sequencer(void *threadp);
void *Service_1(void *threadp);
void *Service_2(void *threadp);
char fileppm[100];
char filejpg[100];

int main ()
{
    struct timeval current_time_val;

capture = cvCreateCameraCapture(0);                                //START CAPTURING FRAMES FROM CAMERA
cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH,640);                   //CAPTURED FRAME PROPERTY WIDTH-RESOLUTION
cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_HEIGHT,480);                  //CAPTURED FRAME PROPERTY HEIGHT-RESOLUTION

cpu_set_t threadcpu;
int i,rc = -1,scope;
threadParams_t param0, param1, param2;

int rt_max_prio, rt_min_prio,ddd;
ddd=10;
pid_t mainpid;
cpu_set_t allcpuset;

//printf("Starting Sequencer Demo\n");
gettimeofday(&start_time_val, (struct timezone *)0);
gettimeofday(&current_time_val, (struct timezone *)0);
syslog(LOG_CRIT, "syslog int Sequencer @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec),     (int)current_time_val.tv_usec/USEC_PER_MSEC);

CPU_ZERO(&allcpuset);

for(i=0; i < NUM_CPU_CORES; i++)
CPU_SET(i, &allcpuset);
printf("Using CPUS=%d from total available.\n", CPU_COUNT(&allcpuset));

// initialize the sequencer semaphores
if (sem_init (&semS1, 0, 0)) { printf ("Failed to initialize S1 semaphore\n"); exit (-1); }
if (sem_init (&semS2, 0, 0)) { printf ("Failed to initialize S2 semaphore\n"); exit (-1); }


mainpid=getpid();

rt_max_prio = sched_get_priority_max(SCHED_FIFO);
rt_min_prio = sched_get_priority_min(SCHED_FIFO);
printf("MAX PRIORITY =%d\n", rt_max_prio);
printf("MIN PRIORITY =%d\n", rt_min_prio);

rc=sched_getparam(mainpid, &main_param);
main_param.sched_priority=rt_max_prio;
rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
if(rc < 0) perror("main_param");
//  print_scheduler();
pthread_attr_getscope(&main_thread_attr, &scope);

if(scope == PTHREAD_SCOPE_SYSTEM)
printf("PTHREAD SCOPE SYSTEM\n");
 else if (scope == PTHREAD_SCOPE_PROCESS)
 printf("PTHREAD SCOPE PROCESS\n");
 else
 printf("PTHREAD SCOPE UNKNOWN\n");

//INITAILIZES SCHEDULING ATTRIBUTES
pthread_attr_init(&sequencer_attr);
pthread_attr_init(&service1_attr);
pthread_attr_init(&service2_attr);

//ASSIGNING SCHEDULING POLICY OF SERVICE THREAD
	pthread_attr_setinheritsched(&sequencer_attr, PTHREAD_EXPLICIT_SCHED);	// Specifies that the scheduling policy and associated attributes are to be set to the corresponding values from this attribute object.
	pthread_attr_setschedpolicy(&sequencer_attr, SCHED_FIFO);                    	//SCHEDULING POLICY SET TO FIFO
	/*rc=pthread_attr_setaffinity_np(&sequencer_attr, sizeof(cpu_set_t), &cpuset);
	if(rc<0){perror("pthread_setaffinity_np");}*/
	sequencer_param.sched_priority = rt_max_prio;   //PTHREAD CAPTURE_IMAGE ENJOYS HIGHEST PRIORITY
	pthread_attr_setschedparam(&sequencer_attr, &sequencer_param);
	param0.threadIdx=0;


//ASSIGNING SCHEDULING POLICY OF SEQUENCER THREAD
	pthread_attr_setinheritsched(&service1_attr, PTHREAD_EXPLICIT_SCHED); // Specifies that the scheduling policy and associated attributes are to be set to the corresponding values from this attribute object.
	pthread_attr_setschedpolicy(&service1_attr, SCHED_FIFO);
	/*rc=pthread_attr_setaffinity_np(&service1_attr, sizeof(cpu_set_t), &cpuset);
	if(rc<0){perror("pthread_setaffinity_np");}*/
	service1_param.sched_priority = rt_max_prio - 1;          //PTHREAD CAPTURE_IMAGE ENJOYS MIDDLE PRIORITY
	pthread_attr_setschedparam(&service1_attr, &service1_param);
	param1.threadIdx=1;

//ASSIGNING SCHEDULING POLICY OF SERVICE THREAD
	pthread_attr_setinheritsched(&service2_attr, PTHREAD_EXPLICIT_SCHED); // Specifies that the scheduling policy and associated attributes are to be set to the corresponding values fromthis attribute object.
	pthread_attr_setschedpolicy(&service2_attr, SCHED_FIFO);
	/*rc=pthread_attr_setaffinity_np(&service2_attr, sizeof(cpu_set_t), &cpuset);
	if(rc<0){perror("pthread_setaffinity_np");}*/
	service2_param.sched_priority = rt_max_prio - 2;          //PTHREAD CAPTURE_IMAGE ENJOYS LOWEST PRIORITY
	pthread_attr_setschedparam(&service2_attr, &service2_param);
	param2.threadIdx=2;

	CPU_ZERO(&threadcpu);
	CPU_SET(3, &threadcpu);
	printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

//CREATES THREADS

	rc = pthread_create(&service1_thread , &service1_attr, service1, (void *)&param1) ;
	if(rc < 0)              perror("pthread_create for service 1");
	else                    printf("pthread_create successful for service 1\n");

	rc = pthread_create(&service2_thread , &service2_attr, service2, (void *)&param2) ;
	if(rc < 0)  perror("pthread_create for service 2");
	else                    printf("pthread_create successful for service 2\n");

	printf("Start sequencer\n");
	param0.sequencePeriods=60000;
	rc = pthread_create(&sequencer_thread , &sequencer_attr, sequencer , (void *)&param0);
	if(rc < 0)  perror("pthread_create for sequencer service 0");
	else        printf("pthread_create successful for sequeencer service 0\n");

	pthread_join(sequencer_thread,NULL);    		   //THREAD 1 JOIN
        pthread_join(service1_thread,NULL);                  //THREAD 2 JOIN
        pthread_join(service2_thread,NULL);                  //THREAD 3 JOIN
printf("\nTEST COMPLETE\n");
}


void *sequencer(void *threadp)
{
         int seqCnt = 0,rc;
    uint32_t sec = 0;
    uint32_t nsec = 0;
static struct timespec delay, remaining_delay;


        sec =0 ;
	nsec=999999999L;	//for 1Hz
//	nsec=99999999L;	//for 10Hz

      	 while(seqCnt < 60000)
    	{

        	sem_post(&semS1);
		pthread_mutex_lock(&yasir);
	delay.tv_sec = sec;
	delay.tv_nsec = nsec;  // 1 second in nano
do
        {
        nanosleep(&delay, &remaining_delay);
        delay.tv_sec = remaining_delay.tv_sec;
        delay.tv_nsec = remaining_delay.tv_nsec;
        }
    while ((remaining_delay.tv_sec > 0) || (remaining_delay.tv_nsec > 0));

        	pthread_mutex_unlock(&yasir);
  seqCnt++; 
 }
    sem_post(&semS2); 
}






void *service1(void *threadp)
{
	char a1[35];
char b1[35];
//CvFont font;
char buffer[50];
CvFont font;
	struct timeval current_time_val,previous_time_val;
gettimeofday(&previous_time_val,(struct timezone *)0);

        struct timeval new_time_val;
	unsigned long long S1Cnt=0;
        threadParams_t *threadParams = (threadParams_t *)threadp;
	difference=0.9;
current_time_val.tv_sec=0;
previous_time_val.tv_sec=0;
////	while(!abortS1){
while(frames<6000){

 		sem_wait(&semS1);
//	pthread_mutex_lock(&yasir);
        frame=cvQueryFrame(capture);
        clock_gettime(CLOCK_REALTIME,&timestamp);
        Mat mat_frame(cvarrToMat(frame));     //FUNCTION CONVERTS CVARR TO MAT

       if(!frame) break;
         S1Cnt++;
	gettimeofday(&current_time_val, (struct timezone *)0);
	syslog(LOG_CRIT, "frames %d [SEC:mSEC %d:%d]\n",frames, (int)(current_time_val.tv_sec-previous_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
	///frames, (int)(current_time_val.tv_sec-previous_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
	printf("frames %d     sec=%d,msec=%d\n", frames,(int)(current_time_val.tv_sec-previous_time_val.tv_sec),(int)current_time_val.tv_usec/USEC_PER_MSEC);

frames++;
sem_post(&semS2);


}
pthread_exit((void *)0);
}




void *service2(void *threadp){
char b1[50];
    struct timeval current_time_val;
    double current_time;
    threadParams_t *threadParams = (threadParams_t *)threadp;
    unsigned long long S2Cnt=0;

while(!abortS2)
    {
        sem_wait(&semS2);

	sprintf(fileppm,"img/image%d.ppm",frames);
	sprintf(filejpg,"img/image%d.jpg",frames);
	       Mat mat_frame(cvarrToMat(frame));     //FUNCTION CONVERTS CVARR TO MAT
      CvFont font;
sprintf(b1,"  Time:%d",timestamp);

      cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX,0.5,0.5);
//        cvPutText(frame,b1,cvPoint(30,30),&font,cvScalar(200,200,250));

	imwrite(fileppm,mat_frame);
	a = imread(fileppm);

	imwrite(filejpg,a);
    }

sem_post(&semS1);
   	pthread_exit((void *)0);

}




