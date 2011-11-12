#include <malloc.h>
#include <stdio.h>
#include <inttypes.h>
#include "coroutine.h"

struct coroutine
{
    int state;              // XXX uint32
    void *ret_addr;         // return address of coroutine_switch's caller
    void *stack;            // the coroutine's stack (i.e. used with ESP register)
    coroutine_t caller;     // the coroutine which resumed this one
    coroutine_func entry;
    void *orig_stack;
};

#define COROUTINE_STARTED       0x10000000
#define COROUTINE_FINISHED      0x80000000
#define COROUTINE_USER          0x0fffffff

#define COROUTINE_STACK_SIZE    4096

coroutine_t current = NULL;

extern void *coroutine_switch(coroutine_t co, void *arg);
void coroutine_terminated(coroutine_t co);

coroutine_t coroutine_current()
{
    return current;
}

int coroutine_alive(coroutine_t co)
{
    return co && ((co->state & COROUTINE_FINISHED) == 0);
}

int coroutine_get_user_state(coroutine_t co)
{
    return co ? co->state & COROUTINE_USER : 0;
}

void coroutine_set_user_state(coroutine_t co, int state)
{
    if(co)
        co->state |= (state & COROUTINE_USER);
}

void coroutine_main(coroutine_func f, void *arg)
{
    if(coroutine_current())
    {
        fprintf(stderr, "coroutine_main(): a coroutine is already executing.\n");
        return;
    }
    
    // create a coroutine for the 'main' function
    coroutine_t co = (coroutine_t)malloc(sizeof(struct coroutine));
    co->state = COROUTINE_STARTED;
    co->entry = f;
    co->ret_addr = NULL;
    co->caller = NULL;
    co->stack = NULL;
    co->orig_stack = NULL;
    current = co;
    
    // execute it
    co->entry(arg);
    coroutine_terminated(co);
    coroutine_free(co);
}

coroutine_t coroutine_create(coroutine_func f, size_t stacksize)
{
    // create a coroutine for the function
    coroutine_t co;
    void *stack;
    
    if(!coroutine_current())
    {
        fprintf(stderr, "coroutine_create(): no coroutine is executing.\n");
        return NULL;
    }
    
    co = (coroutine_t)malloc(sizeof(struct coroutine));
    if(!co)
        return NULL;
    if(stacksize < COROUTINE_STACK_SIZE)
        stacksize = COROUTINE_STACK_SIZE;
    stack = malloc(stacksize);
    if(!stack)
    {
        free(co);
        return NULL;
    }
    co->state = 0;
    co->entry = f;
    co->ret_addr = NULL;
    co->caller = NULL;
    co->stack = stack + stacksize; // stack grows downwards
    co->orig_stack = stack;
    return co;
}

void *coroutine_yield(void *arg)
{
    coroutine_t cur = coroutine_current();
    if(!cur)
    {
        fprintf(stderr, "coroutine_yield(): no coroutine is executing.\n");
        return NULL;
    }
    coroutine_t caller = cur->caller;
    if(!caller)
    {
        fprintf(stderr, "coroutine_yield(): current coroutine doesn't have a caller.\n");
        return NULL;
    }
    else if(!coroutine_alive(caller))
    {
        fprintf(stderr, "coroutine_yield(): caller must be still alive.\n");
        return NULL;
    }
    return coroutine_switch(caller, arg);
}

void *coroutine_resume(coroutine_t co, void *arg)
{
    if(!coroutine_current())
    {
        fprintf(stderr, "coroutine_resume(): no coroutine is executing.\n");
        return NULL;
    }
    else if(!co)
    {
        fprintf(stderr, "coroutine_resume(): coroutine 'co' must not be null.\n");
        return NULL;
    }
    else if(!coroutine_alive(co))
    {
        fprintf(stderr, "coroutine_resume(): coroutine 'co' must be still alive.\n");
        return NULL;
    }
    return coroutine_switch(co, arg);
}

void coroutine_terminated(coroutine_t co)
{
    co->state |= COROUTINE_FINISHED;
    current = co->caller;
    co->caller = NULL;
}

void coroutine_free(coroutine_t co)
{
    if(!co)
        return;
    if((co->state & COROUTINE_STARTED) && !(co->state & COROUTINE_FINISHED))
    {
        fprintf(stderr, "coroutine_free(): coroutine 'co' must be terminated or unstarted.\n");
        return;
    }

    if(co->orig_stack)
        free(co->orig_stack);
    free(co);
}
