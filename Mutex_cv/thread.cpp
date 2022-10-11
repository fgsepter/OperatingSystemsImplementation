#include "thread.h"
#include "ucontext.h"
#include "raiiinterrupt.h"
#include <queue>
#include <map>


std::queue<int> ready_queue;               // ready_queue contains index of threads that are ready to run

std::map<int, std::queue<int>> join_queue; // join_queue is a map from thread index to a queue of thread index. 
                                           // each queue contains the threads that calls join of the certain thread. 

std::map<int, bool> pos_map;               // pos_map is a map from thread index to a bool variable.
                                           // false if the context is not finished, true is the context finishes running.

std::map<int, char*> stack_map;            // stack_map is a map from thread index to a stack pointer.

std::map<int, ucontext_t*> index2context;  // index2context is a map that map from thread index to ucontext_t. 
                                           // we can use this map to track the thread context using the thread index. 

int ptr_to_delete = -1;                    // ptr_to_delete track the index of the previous thread that finishes running
                                           // we delete structures and pointers accociated with the thread when the next thread finishes running

int current_index = -1;                    // current_index track the index of the current running thread.

int thread_index_generator = 0;            // thread_index_generator generates unique index for each new generated thread. 
                                           // it increments by 1 each time a new thread is generated.

bool first_thread = true;                  // first thread track whether we have generated the first thread. 

class thread::impl{
public:
    impl(int);
    ~impl();
    int thread_index;
private:
};

thread::impl::impl(int index){
    this->thread_index = index;
}

thread::impl::~impl(){;}

// Implement code here
void Thread_running(thread_startfunc_t func, void* para){
    // REQUIRES: thread_startfunc_t func, void* para
    // MODIFIES: ready_queue(), join_queue, index2context, stack_map, pos_map, ptr_to_delete
    // EFFECTS:  This Thread_running function aims for running an input thread_startfunc_t func on the operating systems.
    //           After the input func finishes running, the corresponing ucontext_t, stack pointer, and structures are deleted, 
    //           ucontext_t waiting in the join_queue are pushed into the ready_queue, and current context is changed to the 
    //           front of the ready_queue.
    //           When all the thread finishes running, i.e. the ready queue is empty, the program terminate

    assert_interrupts_disabled();
    cpu::interrupt_enable();
    func(para);
    raii_interrupt interrupt;
    
    pos_map[current_index] = true; //The context finishes running
    while (!join_queue[current_index].empty()){
        // push all ucontext_t waiting in the join_queue into the ready_queue
        ready_queue.push(join_queue[current_index].front());
        join_queue[current_index].pop();
    }
    if (ptr_to_delete != -1){
        delete [] stack_map[ptr_to_delete];                     // delete stack
        delete index2context[ptr_to_delete];                    // delete finished context here
        join_queue.erase(join_queue.find(ptr_to_delete));       // erase space for join_queue
        index2context.erase(index2context.find(ptr_to_delete)); // erase space for index2context
        stack_map.erase(stack_map.find(ptr_to_delete));         // erase space for stack_map
    }
	
    ptr_to_delete = current_index;                              // set next pointer to delete to the current thread

    if (ready_queue.empty()){
        assert_interrupts_disabled();
        cpu::interrupt_enable_suspend();
    }
    else {
        current_index = ready_queue.front();
        ready_queue.pop();
        setcontext(index2context[current_index]);
    }
}

thread::thread(thread_startfunc_t func, void* para){
    // REQUIRES: thread_startfunc_t func, void* para
    // MODIFIES: ready_queue, pos_map, index2pointer, thread_index_generator
    // EFFECTS:  This is a thread constructor, we create a new ucontext_t and push it into the ready_queue
    //           thread_index_generator is used to create a unique index for each thread so that they can be easily tracked. 
    if (!first_thread){
        cpu::interrupt_disable();
    }
    try{
        this->impl_ptr = new thread::impl(thread_index_generator);
        ucontext_t *upr = new ucontext_t;
        char *stack = new char [STACK_SIZE];
        stack_map[thread_index_generator] = stack;
        upr->uc_stack.ss_sp = stack;
        upr->uc_stack.ss_size = STACK_SIZE;
        upr->uc_stack.ss_flags = 0;
        upr->uc_link = nullptr;
        makecontext(upr, (void (*)()) Thread_running, 2, func, para);
        ready_queue.push(thread_index_generator);
        pos_map[thread_index_generator] = false; 
        index2context[thread_index_generator] = upr;
        thread_index_generator++;
    }
    catch (std::bad_alloc& ba) {
        cpu::interrupt_enable();
        throw(ba);
    }
    if (first_thread){
        first_thread = false;
    }
    else{
        cpu::interrupt_enable();
    }
        
}

thread::~thread(){
    // MODIFIES: this->impl_ptr
    // EFFECTS:  This is a thread destructor. We delete impl_ptr and free the space.
    raii_interrupt interrupt;
    delete this->impl_ptr;
    impl_ptr = nullptr;
}

void thread::yield(){
    // REQUIRES: interrupt is enabled
    // MODIFIES: ready_queue, current_index
    // EFFECTS:  We stop running the current thread and swap to the first thread in the ready queue. 
    //           We swap the current running context to the front of the ready queue, 
    //           and push the current thread to the back of the running queue

    raii_interrupt interrupt;
    if (!ready_queue.empty()){
        //if the ready queue is not empty, we run the first thread in ready queue and put current thread to ready queue
        int temp = current_index;
        ready_queue.push(temp);
        current_index = ready_queue.front();
        ready_queue.pop();
        swapcontext(index2context[temp], index2context[current_index]);
    }
    // if the ready queue is empty, we keep running current thread
}




void thread::join(){
    // REQUIRES: interrupt is enabled
    // MODIFIES: ready_queue, join_queue, current_index
    // EFFECTS:  We stop running the current thread and swap to the first thread in the ready queue. 
    //           current thread can only be runned if and only if the joining thread finished running.
    //           We push the current thread into the back of the joining queue.
    raii_interrupt interrupt;
    if (pos_map[this->impl_ptr->thread_index] == true){
        // if the joining thread finishes running, we do nothing and continue
        return;
    }
    else if (ready_queue.empty()){
        // if the joining thread does not finish running, we check whether the ready_queue is empty.
        // if the ready queue is empty, no runable thread exists, the program should terminate
        assert_interrupts_disabled();
        cpu::interrupt_enable_suspend();
    }
    else {
        // if the ready queue is not empty, we run the first thread in ready queue and put current thread to join queue
        int temp = current_index;
        join_queue[this->impl_ptr->thread_index].push(temp);
        current_index = ready_queue.front();
        ready_queue.pop();
        swapcontext(index2context[temp], index2context[current_index]);
    }
}
