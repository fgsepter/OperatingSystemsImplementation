#include "raiiinterrupt.h"
#include "cpu.h"

raii_interrupt::raii_interrupt(){
	assert_interrupts_enabled();
	cpu::interrupt_disable();
}

raii_interrupt::~raii_interrupt(){
	cpu::interrupt_enable();
}
