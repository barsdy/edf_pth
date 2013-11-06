/*
 * =====================================================================================
 *
 *       Filename:  edf.c
 *
 *    Description:  a test for edf algorithm in pth enviroment
 *
 *        Version:  1.0
 *        Created:  2013年07月20日 00时31分48秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Murray Ma (test), 
 *        Company:  
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pth.h"

/* task thread's arg */
struct task_arg_st {
    int     task_no;
};
typedef struct task_arg_st task_arg_t;

/* lists of edf task */
struct edf_tasklist_st {
    pth_t tid;
    pth_attr_t attr;
    int latency;
    struct task_arg_st *arg;
    struct edf_tasklist_st *next;
};
typedef struct edf_tasklist_st edf_tasklist_t;


static void *thread_func(void *arg)
{
    task_arg_t *task_arg = (task_arg_t *)arg;

    printf("The task%d get into!\n", task_arg->task_no);
    while(1);
    printf("The task%d get out!\n", task_arg->task_no);
    return NULL;
}

static void input_thread_attr(pth_attr_t attr)
{
    char resp[2] = "";
    int  type = TSTASK;

    printf("Is this thread real-time?[y/n]\n");
    scanf("%s", resp);
    type = (resp[0]=='y') ? (type | RTTASK):(type | TSTASK);

    resp[0] = '\0';
    printf("Is this thread hard real-time?[y/n]\n");
    scanf("%s", resp);
    type = (resp[0]=='y') ? (type | HARDRT):(type | SOFTRT);

    resp[0] = '\0';
    printf("Is this thread a period task?[y/n]\n");
    scanf("%s", resp);
    type = (resp[0]=='y') ? (type | PERTASK):(type | APERTASK);

    pth_attr_set(attr, PTH_ATTR_TYPE, type);
}


static void input_thread_time(pth_attr_t attr)
{
    int        resp_execute;
    int        resp_period;
    pth_time_t tmp_time;

retry_input_time:
    printf("Input the main execute time and period(us)\n");
    printf("\tlike 100 300\n");
    scanf("%d %d", &resp_execute, &resp_period);
    if (resp_execute > resp_period) {
        printf("Execute time cannot lager than period!\n");
        goto retry_input_time;
    }
    tmp_time.tv_sec = (int)(resp_execute / 1000000);
    tmp_time.tv_usec = resp_execute % 1000000;
    pth_attr_set(attr, PTH_ATTR_EXETIME, &tmp_time);
    printf("the task execute time is %ds, %dus\n", (int)tmp_time.tv_sec, (int)tmp_time.tv_usec);

    tmp_time.tv_sec = (int)(resp_period / 1000000);
    tmp_time.tv_usec = resp_period % 1000000;
    pth_attr_set(attr, PTH_ATTR_PERIOD, &tmp_time);
    printf("the task period is %ds, %dus\n", (int)tmp_time.tv_sec, (int)tmp_time.tv_usec);
}

static void input_thread_arg(task_arg_t *arg, int task_no)
{
    arg->task_no = task_no;
}

static void add_to_end(edf_tasklist_t *head, edf_tasklist_t *new)
{
    static edf_tasklist_t *last;

    if (last == NULL) 
        head->next = new;
    else 
        last->next = new;

    last = new;
}


int
main( )
{
    char       resp_str[2];
    pth_attr_t main_attr;
    int        cnt;
    int        task_cnt;
    char       task_name[8];
    edf_tasklist_t *task_head = NULL;
    edf_tasklist_t *new_task = NULL, *task_walker = NULL;

    if (pth_init() == FALSE) {
        perror("pth_init fail!\n");
        return -1;
    }

    /* create main attr */
    if ((main_attr = pth_attr_new()) == NULL) {
        perror("pth_attr_new fail!\n");
        return -1;
    }

retry_main_attr:
    memset(resp_str, 0x00, sizeof(resp_str));
    printf("Input the main attr!\n");
    printf("Use the default attr(RTTASK|HARDRT|PERTASK)?[y/n]\n");
    scanf("%s", resp_str); /* ignore the input security */
    if (resp_str[0] == 'y') 
        pth_attr_set(main_attr, PTH_ATTR_TYPE, RTTASK|HARDRT|PERTASK);
    else if (resp_str[0] == 'n') 
        input_thread_attr(main_attr);
    else
        goto retry_main_attr;

    if (pth_mod_main_attr(main_attr)) {
        printf("modify main thread attr fail!\n");
        return -1;
    }

retry_task_cnt:
    printf("how many threads do you wish to spawn:\n");
    scanf("%d", &task_cnt);
    if (task_cnt < 0) 
        goto retry_task_cnt;

    for (cnt=0; cnt<task_cnt; cnt++) {
        if ((new_task = malloc(sizeof(struct edf_tasklist_st))) == NULL) {
            perror("malloc fail!\n");
            return -1;
        }
        memset(new_task, 0x00, sizeof(struct edf_tasklist_st));
        if ((new_task->attr = pth_attr_new()) == NULL) {
            perror("pth_attr_new fail!\n");
            return -1;
        }
        if ((new_task->arg = malloc(sizeof(struct task_arg_st))) == NULL) {
            perror("malloc fail!\n");
            return -1;
        }
        memset(new_task->arg, 0x00, sizeof(struct task_arg_st));

        memset(task_name, 0x00, sizeof(task_name));
        sprintf(task_name, "task%d", cnt);
        pth_attr_set(new_task->attr, PTH_ATTR_NAME, task_name);

        input_thread_attr(new_task->attr);
        input_thread_time(new_task->attr);
        input_thread_arg(new_task->arg, cnt);

        printf("Input the thread latency(relative to the previos)(us):\n");
        scanf("%d", &(new_task->latency));

        if (task_head == NULL)
            task_head = new_task;
        else
            add_to_end(task_head, new_task);
    }

    printf("input done. now spawn thread!\n");
    task_walker = task_head;
    while (task_walker->next != NULL) {
        pth_usleep(task_walker->latency);

        if ((task_walker->tid = pth_spawn(task_walker->attr, thread_func, (void *)task_walker->arg)) == NULL) {
            perror("pth_spawn fail!\n");
            return -1;
        }

        task_walker = task_walker->next;
    }
    printf("spawn done!\n");

    task_walker = task_head;
    while (task_walker->next != NULL) {
        if (pth_join(task_walker->tid, NULL) == FALSE) {
            perror("pth_join fail!\n");
            return -1;
        }

        task_walker = task_walker->next;
    }
    printf("join done!\n");

    /* ignore release */
    return 0;
}

