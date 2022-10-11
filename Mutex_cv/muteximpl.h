#ifndef _MUTEXIMPL_H
#define _MUTEXIMPL_H

#include "mutex.h"
#include <queue>

// define impl class here
class mutex::impl{
public:
    impl();
    ~impl();
    void inner_lock();
    void inner_unlock();
private:
    bool status; //true = free, false = busy
    int holder; //keeps track of the index of the thread holding the mutex
    std::queue<int> waiting_queue; //queue of threads waiting for lock
};


#endif /* _MUTEXIMPL_H */