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
typedef task_arg_st task_arg_t;

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

    printf("The task%d get into!\n", task_arg->cnt);
    while(1);
    printf("The task%d get out!\n", task_arg->cnt);
    return NULL;
}

static void input_thread_attr(pth_attr_t attr)
{
    printf("Do the main thread is real-time?[y/n]\n");
    scanf("%s", resp_str);
    main_attr->flag = resp_str[0] = 'y'?(main_attr->flag | RTTASK):(main_attr->flag | TSTASK);

}


static void input_thread_time(put_attr_t attr)
{

}

static void input_thread_arg(task_arg_t *arg)
{

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
    int        resp_execute;
    int        resp_period;
    pth_attr_t main_attr;
    void       *ret = NULL;
    pth_time_t tmp_time;
    int        cnt;
    int        task_cnt;
    edf_tasklist_t *task_head = NULL, *task_walker = NULL;

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
    if (resp_str[0] == 'y') 
        main_attr->flag |= (RTTASK | HARDRT | PERTASK);
    else if (resp_str[0] == 'n') 
        input_thread_attr(main_attr);
    else
        goto retry_main_attr;

retry_main_time:
    resp_str[0] = '\0';
    printf("Input the main execute time and period(us)\n");
    printf("\tlike 100 300\n");
    scanf("%d %d", &resp_execute, &resp_period);
    if (resp_execute > resp_period) {
        printf("Execute time cannot lager than period!\n");
        goto retry_main_time;
    }
    tmp_time.tv_sec = (int)(resp_execute / 1000000);
    tmp_time.tv_usec = resp_execute % 1000000;
    pth_attr_set(main_attr, PTH_ATTR_EXETIME, &tmp_time);
    printf("main execute time is %ds, %dus\n", tmp_time.tv_sec, tmp_time.tv_usec);

    tmp_time.tv_sec = (int)(resp_period / 1000000);
    tmp_time.tv_usec = resp_period % 1000000;
    pth_attr_set(main_attr, PTH_ATTR_PERIOD, &tmp_time);
    printf("main period is %ds, %dus\n", tmp_time.tv_sec, tmp_time.tv_usec);

    if (pth_mod_main_attr(attr)) {
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
        if ((new_task->task_arg = malloc(sizeof(struct task_arg_st))) == NULL) {
            perror("malloc fail!\n");
            return -1;
        }
        memset(new_task->task_arg, 0x00, sizeof(struct task_arg_st));

        input_thread_attr(new_task->attr);
        input_thread_time(new_task->attr);
        input_thread_arg(new_task->task_arg);

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

        if ((task_walker->tid = pth_spawn(task_walker->attr, thread_func, (void *)task_walker->task_arg)) == NULL) {
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

