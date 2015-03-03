#include "application.h"

int SendMessage(QUEUE *queue, MSG *msg)
{
    if(NULL == queue)
    {
        return -1;
    }
    
    mico_rtos_push_to_queue(queue,msg,MICO_WAIT_FOREVER);
}

int PostMessage(QUEUE *queue, MSG *msg)
{
    if(NULL == queue)
    {
        return -1;
    }
    
    mico_rtos_push_to_queue(queue,msg,MICO_WAIT_FOREVER);
}

