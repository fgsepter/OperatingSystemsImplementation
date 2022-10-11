#include "thread.h"
#include "ucontext.h"
#include "muteximpl.h"
#include "raiiinterrupt.h"
#include <queue>
#include <map>

extern std::queue<int> ready_queue; // queue of threads that are ready
extern int current_index; // 
extern std::map<int, ucontext_t*> index2context; // map of indices to ucontext pointers

// MODIFIES: status, holder
// EFFECTS: mutex impl_ptr constructor: initializes mutex impl_ptr
mutex::impl::impl(){
    this->status = true;
    this->holder = -1;
}

// EFFECTS: mutex impl_ptr destructor
mutex::impl::~impl(){
    ;
}

// MODIFIES: waiting_queue, ready_queue, status, holder, current_index
// EFFECTS: Locks the thread.
void mutex::impl::inner_lock() {
    if (this->status) {
        this->status = false;
        this->holder = current_index;
    } 
    else {
        if (!ready_queue.empty()){
            assert_interrupts_disabled();
            //if the ready queue is not empty, we run the first thread in ready queue and put current thread to mutex waiting queue
            int temp = current_index;
            waiting_queue.push(temp);
            current_index = ready_queue.front();
            ready_queue.pop();
            swapcontext(index2context[temp], index2context[current_index]);
        }
        else{
            //if the ready queue is empty, no runnable thread exists, the project should terminate (test code is incorrect)
            cpu::interrupt_enable_suspend();
            return;
        }
    }
}

// REQUIRES:  Thread holds the mutex.
// MODIFIES:  waiting_queue, ready_queue, status, holder
// EFFECTS: Unlocks the thread.
void mutex::impl::inner_unlock() {
    if (current_index != this->holder || this->holder == -1) {
        throw std::runtime_error("You don't own this mutex");
    }
    this->holder = -1;
    this->status = true;
    if (!waiting_queue.empty()) {
        ready_queue.push(waiting_queue.front()); //move waiting thread to ready queue
        this->holder = waiting_queue.front();
        this->status = false;
        waiting_queue.pop();
    }
}

// EFFECTS: mutex constructor: Initializes mutex by creating a new impl_ptr
mutex::mutex() {
    raii_interrupt interrupt;
    try {
        this->impl_ptr = new impl;
    } catch (std::bad_alloc& ba) {
        throw(ba);
    }
}

// EFFECTS: mutex destructor: Deletes impl_ptr and sets impl_ptr to null
mutex::~mutex() {
    raii_interrupt interrupt;
    delete this->impl_ptr;
    this->impl_ptr = nullptr;
}

// EFFECTS: locks the thread
void mutex::lock() {
    raii_interrupt interrupt;
    this->impl_ptr->inner_lock();
}

// EFFECTS: unlocks the thread
void mutex::unlock() {
    raii_interrupt interrupt;
    this->impl_ptr->inner_unlock();
}