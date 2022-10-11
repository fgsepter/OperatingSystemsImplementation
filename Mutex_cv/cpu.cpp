#include "thread.h"
#include "ucontext.h"
#include "raiiinterrupt.h"
#include <queue>
#include <map>

extern std::queue<int> ready_queue;
extern std::map<int, ucontext_t*> index2context;
extern int current_index;

void Handler_interrupt(){
    // EFFECTS: call yield() to generate interrupts.
    thread::yield();
}


void cpu::init(thread_startfunc_t func, void * para){
    // MODIFIES: ready_queue, current_index
    // EFFECTS:  initilize the cpu, create and run the first thread.
    cpu::interrupt_vector_table[0] = Handler_interrupt;
    thread original_thread ( (thread_startfunc_t) func, (void *) para);
    current_index = ready_queue.front();
    ready_queue.pop();
    setcontext(index2context[current_index]);
}
