#include "thread.h"
#include "ucontext.h"
#include "muteximpl.h"
#include "raiiinterrupt.h"
#include <queue>
#include <map>

extern std::queue<int> ready_queue;
extern int current_index;
extern std::map<int, ucontext_t*> index2context;

class cv::impl
{
public:
    impl();
    ~impl();
    std::queue<int> waiting_queue;
};

// implement impl class here
cv::impl::impl(){;}

cv::impl::~impl(){;}

cv::cv(){
    raii_interrupt interrupt;
    try {
        this->impl_ptr = new cv::impl();
    }
    catch(std::bad_alloc& ba){
        throw(ba);
    }
};

cv::~cv(){
    raii_interrupt interrupt;
    delete this->impl_ptr;
    this->impl_ptr = nullptr;
};

void cv::signal(){
/*
    Effect: In this function, we check whether the waiting queue of this condition variable is empty or not.
    If the waiting queue is empty, we do nothing. If the waiting queue is not empty, we pop one element from
    the waiting queue and push it into the ready queue.
    Input:None
    Output:None
*/
    raii_interrupt interrupt;
    if(!this->impl_ptr->waiting_queue.empty()){
        ready_queue.push(this->impl_ptr->waiting_queue.front());
        this->impl_ptr->waiting_queue.pop();
    }
}

void cv::broadcast(){
/*
    Effect: In this function, we check whether the waiting queue of this condition variable is empty or not.
    If the waiting queue is empty, we do nothing. If the waiting queue is not empty, we pop all the element from
    the waiting queue and push them into the ready queue.
    Input:None
    Output:None
*/
    raii_interrupt interrupt;
    while(!this->impl_ptr->waiting_queue.empty()){
        ready_queue.push(this->impl_ptr->waiting_queue.front());
        this->impl_ptr->waiting_queue.pop();
    }
}

void cv::wait(mutex& mutex){
/*
    Effect: In this function, we first relase the lock. Then we add the current thread to waiting list and
    switch to the next ready thread in the ready queue. If the ready queue is empty, the project should be
    terminated. When this thread is waken up again, it request to acquire the lock.s 
    Input:None
    Output:None
*/
    raii_interrupt interrupt;
    mutex.impl_ptr->inner_unlock();
    int temp = current_index;
    if (!ready_queue.empty()){
        //if the ready queue is not empty, we run the first thread in ready queue and put current thread to cv waiting queue
        this->impl_ptr->waiting_queue.push(temp);
        current_index = ready_queue.front();
        ready_queue.pop();
    }
    else{
        //if the ready queue is empty, no runnable thread exists, the project should terminate
        assert_interrupts_disabled();
        cpu::interrupt_enable_suspend();
    }
    
    swapcontext(index2context[temp], index2context[current_index]);
    mutex.impl_ptr->inner_lock();
}
