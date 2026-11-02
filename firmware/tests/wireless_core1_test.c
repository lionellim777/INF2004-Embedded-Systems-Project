/* Exercise production transport ownership while its radio blocks for 1.1s.
 * Host threads stand in for the two processor cores; no RF is simulated. */
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#define main unused_transport_tests_main
#include "wireless_calibration_transport_test.c"
#undef main
#include "../app/robot_firmware/motor_guard.h"
static _Thread_local unsigned lock_depth;
static pthread_t worker;
static void (*worker_entry)(void);
static atomic_bool radio_blocked,shutdown_worker,radio_finished;
static void delay_ms(unsigned ms){struct timespec t={ms/1000,(long)(ms%1000)*1000000};nanosleep(&t,NULL);}
void mutex_init(mutex_t*p){assert(!pthread_mutex_init(p,NULL));}
void mutex_enter_blocking(mutex_t*p){assert(!lock_depth);assert(!pthread_mutex_lock(p));lock_depth++;}
void mutex_exit(mutex_t*p){assert(lock_depth==1);lock_depth--;assert(!pthread_mutex_unlock(p));}
void sem_init(semaphore_t*p,int value,int maximum){assert(maximum==1);assert(!pthread_mutex_init(&p->mutex,NULL));p->count=(unsigned)value;}
void sem_release(semaphore_t*p){pthread_mutex_lock(&p->mutex);p->count++;pthread_mutex_unlock(&p->mutex);}
bool sem_acquire_timeout_ms(semaphore_t*p,uint32_t ms){
    uint64_t until=time_us_64()+ms*1000;
    while(time_us_64()<until){pthread_mutex_lock(&p->mutex);bool got=p->count!=0;if(got)p->count--;pthread_mutex_unlock(&p->mutex);if(got)return true;delay_ms(1);}return false;
}
static void *start_worker(void*arg){(void)arg;worker_entry();return NULL;}
void multicore_launch_core1_with_stack(void(*entry)(void),uint32_t*stack,size_t bytes){assert(stack&&bytes>=8192);worker_entry=entry;assert(!pthread_create(&worker,NULL,start_worker,NULL));}
void sleep_ms(uint32_t ms){assert(!lock_depth);if(atomic_load(&shutdown_worker))pthread_exit(NULL);delay_ms(ms);}
static void radio_poll_hook(void){
    assert(pthread_equal(pthread_self(),worker)&&!lock_depth);
    if(!atomic_exchange(&radio_blocked,true)){
        delay_ms(1100); /* Reproduce the recorded CYW43 service delay. */
        client_t*c=open_client();const char*req="GET /logs?since=0 HTTP/1.1\r\n\r\n";
        feed(c,req,strlen(req));drain(c);assert(strstr(wire,"motor loop alive"));
        c=open_client();req="POST /command/x HTTP/1.1\r\nX-Calibrate: 1\r\n\r\n";
        feed(c,req,strlen(req));drain(c);assert(strstr(wire,"202 Accepted"));
        atomic_store(&radio_finished,true);
    }
}
int main(void){
    calibration_wifi_capture_init();assert(calibration_wifi_init());
    uint64_t begin=time_us_64();unsigned updates=0;bool stop_received=false;
    motor_guard_t guard={0};motor_guard_refresh(&guard,(uint32_t)begin,true);
    while(time_us_64()-begin<1500000){
        uint64_t before=time_us_64();assert(!motor_guard_tick(&guard,(uint32_t)before));
        calibration_wifi_poll(false);capture("motor loop alive\n",17);
        if(calibration_wifi_getchar()=='x')stop_received=true;
        assert(time_us_64()-before<100000); /* no inherited 1.1s radio stall */
        assert(motor_guard_refresh(&guard,(uint32_t)time_us_64(),true));updates++;delay_ms(5);
    }
    assert(atomic_load(&radio_finished)&&updates>100&&!guard.tripped&&stop_received);
    assert(calibration_wifi_max_poll_us()>=1100000);
    atomic_store(&shutdown_worker,true);assert(!pthread_join(worker,NULL));
    /* Once a mission is consumed, a later request cannot change its config. */
    shared_enter();core.selection_allowed=true;assert(calibration_course_command(&core,"/mission/l/r/120"));shared_exit();
    assert(calibration_wifi_getchar()=='q');
    shared_enter();assert(calibration_course_command(&core,"/mission/r/l/150"));shared_exit();
    course_config_t config=calibration_wifi_course_config();assert(config.y_action=='l'&&config.advance_mm==120);
    assert(calibration_wifi_getchar()=='q');config=calibration_wifi_course_config();assert(config.y_action=='r'&&config.advance_mm==150);
    /* A genuine missed control deadline still removes permission to drive. */
    assert(motor_guard_tick(&guard,guard.refreshed_us+150000)&&guard.tripped);
    printf("PASS core1: 1.1s radio stall, %u live control updates, logs, queued STOP, coherent config, unchanged motor guard\n",updates);
    return 0;
}
