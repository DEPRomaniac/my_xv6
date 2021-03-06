#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_cbps(void)
{
  // struct proc* curproc = myproc()
  int number = myproc()->tf->edx;
  int bps = 0;

  if (number < 0)
    return -1;

  for (int i = number/2; i>0; i--){
    if (i*i <= number){
      bps = i;
      break;
    }
  }
  cprintf("Biggest perfects square for %d is : %d\n", number, bps);
  
  return 1;
}

int
sys_get_date(void)
{ 
  struct rtcdate* date;
  argptr(0, (void*)(&date), sizeof(*date));
  cmostime(date);
  return 0;
}

int
sys_set_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  release(&tickslock);
  for(;;){
    acquire(&tickslock);
    if(ticks - ticks0 >= n){
      release(&tickslock);
      break;
    }
    release(&tickslock);
  }
  return 0;
}

int
sys_process_start_time(void)
{
  int my_time_per_second = myproc()->creation_time / 100;
  int my_time_fractional = myproc()->creation_time % 100;
  // double my_time = myproc()->creation_time / 100;
  cprintf("start process time: %d.%d s\n", my_time_per_second, my_time_fractional);
  return myproc()->creation_time;
  // return process_start_time();
}

int sys_ancestor(void)
{
  int process_id;

  if(argint(0, &process_id) < 0)
    return -1;

  get_ancestors(process_id);
  return 1;
}

int sys_descendant(void)
{
  int process_id;

  if(argint(0, &process_id) < 0)
    return -1;

  get_descendants(process_id);
  return 1; 
}

////////////////////////////////////////////////lab3
int 
sys_change_queue(void)
{
  int pid, new_queue;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &new_queue) < 0)
    return -1;

  change_sched_queue(pid, new_queue);

  return 1;
}

int 
sys_change_prio(void)
{
  int pid;
  int my_priority;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &my_priority) < 0)
    return -1;
  
  change_proc_prio(pid, my_priority);

  return 1;
}

int 
sys_pratio(void)
{
  int pid, prio_ratio, ctime_ratio, exec_cyc_ratio;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &prio_ratio) < 0)
    return -1;
  if(argint(2, &ctime_ratio) < 0)
    return -1;
  if(argint(3, &exec_cyc_ratio) < 0)
    return -1;

  pratio(pid, prio_ratio, ctime_ratio, exec_cyc_ratio);

  return 1;
}

int 
sys_plog(void)
{
  plog();
  return 1;
}

int
sys_acquire_rec(void)
{
  // struct spinlock* lk;
  // argptr(0, (void*)(&lk), sizeof(lk));
  acquire_rec(&test_lock);
  return 1;
}

int
sys_release_rec(void)
{
  // struct spinlock* lk;
  // argptr(0, (void*)(&lk), sizeof(lk));
  release_rec(&test_lock);
  return 1;
}

int
sys_init_lock(void){
  struct spinlock* lk;
  argptr(0, (void*)(&lk), sizeof(lk));
  initlock(&test_lock, "test_lk");
  return 1;
}


int
sys_rwinit(void){
  rwinit();
  return 1; 
}

int
sys_rwtest(void){
  int pattern;
  int priority;
  if(argint(0, &pattern) < 0){
    // cprintf("wat\n");
    return -1;
  }
  if(argint(1, &priority) < 0){
    // cprintf("wat\n");
    return -1;
  }
  // cprintf("rwtest started...\n");
  rwtest((uint)pattern, (uint)priority);
  // cprintf("rwtest finished...\n");
  return 1;
}

int
sys_add_reader(void){
  int rindex;
  if(argint(0, &rindex) < 0)
    return -1;
  add_reader(rindex);
  return 1;
}

int
sys_add_writer(void){
  int windex;
  if(argint(0, &windex) < 0)
    return -1;
  add_writer(windex);
  return 1;
}