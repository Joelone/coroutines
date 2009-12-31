#include <malloc.h>
#include <stdio.h>
#include "coroutine.h"

struct coroutine_ref
{
    coroutine_t co;
    coroutine_ref_t next;
};

struct coroutine
{
    int state;              // unstarted, running, paused, finished
    void *pc;               // the coroutine's program counter
    void *stack;            // the coroutine's stack (i.e. used with ESP register)
    coroutine_t caller;     // the coroutine which resumed this one
    coroutine_func entry;
    void *orig_stack;
    struct coroutine_ref refs; // list of coroutines linked to this one
};

#define STATE_UNSTARTED     0
#define STATE_STARTED       1
#define STATE_FINISHED      2

#define COROUTINE_STACK_SIZE    4096

coroutine_t current = NULL;

extern void *coroutine_switch(coroutine_t co, void *arg);
void coroutine_add_ref(coroutine_t co, coroutine_t ref, coroutine_ref_t tail);
void coroutine_remove_ref(coroutine_t co, coroutine_t ref);
int coroutine_find_ref(coroutine_t co, coroutine_t ref, coroutine_ref_t *tail);
void coroutine_set_caller(coroutine_t co, coroutine_t caller);
void coroutine_terminated(coroutine_t co);

coroutine_t coroutine_current()
{
    return current;
}

int coroutine_alive(coroutine_t co)
{
    return co && (co->state < STATE_FINISHED);
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
    co->state = STATE_STARTED;
    co->entry = f;
    co->pc = NULL;
    co->caller = NULL;
    co->stack = NULL;
    co->orig_stack = NULL;
    co->refs.co = NULL;
    co->refs.next = NULL;
    current = co;
    
    // execute it
    co->entry(arg);
    
    // the coroutine finished
    co->state = STATE_FINISHED;
    co->caller = NULL;
    coroutine_free(co);
    current = NULL;
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
    co->state = STATE_UNSTARTED;
    co->entry = f;
    co->pc = NULL;
    co->caller = NULL;
    co->stack = stack + stacksize; // stack grows downwards
    co->orig_stack = stack;
    co->refs.co = NULL;
    co->refs.next = NULL;
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

void coroutine_add_ref(coroutine_t co, coroutine_t ref, coroutine_ref_t tail)
{
    // is the reference list empty?
    if(co->refs.co == NULL)
    {
        co->refs.co = co;
        return;
    }
    
    // find the tail of the reference list
    if(!tail)
    {
        tail = &co->refs;
        while(tail->next)
            tail = tail->next;
    }
    
    // add the coroutine at the end of the list
    tail->next = (coroutine_ref_t)malloc(sizeof(struct coroutine_ref));
    tail->next->co = co;
    tail->next->next = NULL;
}

void coroutine_remove_ref(coroutine_t co, coroutine_t ref)
{
    coroutine_ref_t entry = NULL, next = NULL;
    if(coroutine_find_ref(co, ref, &entry))
    {
        next = entry->next;
        if(next)
        {
            entry->co = next->co;
            entry->next = next->next;
            free(next);
        }
        else
        {
            entry->co = NULL;
        }
    }
}

// find a reference in a coroutine's ref list
int coroutine_find_ref(coroutine_t co, coroutine_t ref, coroutine_ref_t *tail)
{
    coroutine_ref_t last = &co->refs;
    if(last->co == NULL)
    {
        if(tail)
            *tail = NULL;
        return 0;
    }
    else if(last->co = ref)
    {
        if(tail)
            *tail = last;
        return 1;
    }
    else
    {
        while(last->next)
        {
            if(last->co == ref)
            {
                if(tail)
                    *tail = last;
                return 1;
            }
            last = last->next;
        }
        if(tail)
            *tail = last;
        return 0;
    }
}

void coroutine_set_caller(coroutine_t co, coroutine_t caller)
{
    coroutine_ref_t tail = NULL;
    
    //printf(" %p <=> %p\n", co, caller);
    
    co->caller = caller;
    if(!coroutine_find_ref(co, caller, &tail))
        coroutine_add_ref(co, caller, tail);
    if(!coroutine_find_ref(caller, co, &tail))
        coroutine_add_ref(caller, co, tail);
}

void coroutine_terminated(coroutine_t co)
{
    co->state = STATE_FINISHED;
    current = co->caller;
    co->caller = NULL;
    
    // find all coroutines which have a reference to this coroutine
}

void coroutine_free(coroutine_t co)
{
    if(!co)
    {
        fprintf(stderr, "coroutine_free(): coroutine 'co' must not be null.\n");
        return;
    }
    else if(co->state == STATE_STARTED)
    {
        fprintf(stderr, "coroutine_free(): coroutine 'co' must be terminated or unstarted.\n");
        return;
    }
    printf("freeing coroutine %p\n", co);
    
    coroutine_ref_t tail = &co->refs;
    
    /*
    // we need to remove references to this coroutine for every callee
    if(tail->callee != NULL)
    {
        while(tail)
        {
            printf("coroutine %p held a reference to coroutine %p\n", tail->callee, co);
            if(tail->callee->caller == co)
                tail->callee->caller = NULL;
            tail = tail->next;
        }
    }
    */
    
    // free the caller list
    coroutine_ref_t current = co->refs.next;
    while(current)
    {
        tail = current->next;
        free(current);
        current = tail;
    }
    
    /*
    // maybe this coroutine has a reference to a callee, too
    if(co->caller)
    {
        printf("Removing coroutine %p from coroutine %p's list \n", co, co->caller);
        coroutine_remove_callee(co->caller, co);
        co->caller = NULL;
    }
    */

    if(co->orig_stack)
        free(co->orig_stack);
    free(co);
}
