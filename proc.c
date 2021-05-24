#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();

  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  p->creation_time = ticks; // adding creation time field for question #2

  p->sched_queue = ROUND_ROBIN;
  p->my_priority = 1;
  p->ctime_ratio = 1;
  p->priority_ratio = 1;
  p->exec_cyc_ratio = 1;
  p->exec_cyc = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}


float calc_rank(struct proc *p){
  return (p->priority_ratio / p->my_priority) +
          (p->ctime_ratio * p->creation_time) +
          (p->exec_cyc_ratio * p->exec_cyc*0.1);
}


struct proc* rr_scheduler(int* index){
  struct proc *p;
  int selected_index = -1;
  int cur_index;
  for (int i = 0; i < NPROC; i++){
    cur_index = ((*index) + i)%NPROC;
    if(ptable.proc[cur_index].state != RUNNABLE || ptable.proc[cur_index].sched_queue != ROUND_ROBIN)
      continue;

    p = &ptable.proc[cur_index];
    selected_index = cur_index;
    *index = (cur_index+1) % NPROC;
    break;
  }
  if (selected_index < 0){
    return 0;
  }
  return p;
}


struct proc* priority_scheduler(void)
{
  struct proc *p;
  int best_prio;
  int cur_prio;
  int best_is_set = 0;
  struct proc* best_prio_process = 0;

  for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE || p->sched_queue != PRIORITY)
        continue;
    if (best_is_set == 0){
      best_prio_process = p;
      best_prio = p->my_priority;
      best_is_set = 1;
    }
    else{
      cur_prio = p->my_priority;
      if (cur_prio < best_prio){
        best_prio = cur_prio;
        best_prio_process = p;
      }
    }
  }
  if (best_is_set == 0)
    return 0;
  return best_prio_process;
}


struct proc* bjf_scheduler(void)
{
  struct proc* p;
  float best_rank, p_rank;
  int best_is_set = 0;
  struct proc* best_rank_process = 0;

  for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE || p->sched_queue != BJF)
        continue;

    p_rank = calc_rank(p);
    if (best_is_set == 1 && p_rank < best_rank){
        best_rank = p_rank;
        best_rank_process = p;
    }
    else{
      best_is_set = 1;
      best_rank_process = p;
      best_rank = p_rank;
    }
  }
  if (best_is_set == 0)
    return 0;
  return best_rank_process;
}


struct proc* fcfs_scheduler(void)
{
  struct proc* min_creation_time_process = 0;
  struct proc* p;
  for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state != RUNNABLE || p->sched_queue != FCFS)
        continue;
    if (min_creation_time_process != 0) {
    // here I find the process with the lowest creation time (the first one that was created)
      if(p->creation_time < min_creation_time_process->creation_time)
        min_creation_time_process = p;
    }
    else
      min_creation_time_process = p;
  }
  return min_creation_time_process;
}
//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct proc *waiting_p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  int rr_index = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();
    
    acquire(&ptable.lock);
    p = rr_scheduler(&rr_index);
    if (p == 0)
    {
      rr_index = 0;
      p = priority_scheduler();
    }
    if (p == 0)
      p = bjf_scheduler();

    if (p == 0)
      p = fcfs_scheduler();

    if(p != 0){
      p->exec_cyc++;

      for(waiting_p = ptable.proc ; waiting_p < &ptable.proc[NPROC]; waiting_p++){
        if(waiting_p->pid == 0)
          continue;

        if(waiting_p->state == RUNNABLE)
          waiting_p->wtime++;

        if (waiting_p->wtime > 8000 && waiting_p->sched_queue > ROUND_ROBIN){
            waiting_p->sched_queue--;
            waiting_p->wtime = 0;
        }
      }
      p->wtime = 0;

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      c->proc = 0;
    }

    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void
get_ancestors(int process_id)
{
  struct proc* curproc;

  int process_avail = 0;

  for(int i=0;i<NPROC;i++)
  {
    // cprintf("%d ", ptable.proc[i].pid);
    if(ptable.proc[i].pid == process_id)
    {
      curproc = &(ptable.proc[i]);
      process_avail = 1;
      break;
    }
  }
  // cprintf("\n");

  if (process_avail == 0)
  {
    cprintf("process with pid=%d does not exist\n", process_id);
    return;
  }

  struct proc* parproc = curproc->parent;
  while (curproc->pid > 1){
    cprintf("""my id"": %d, ""my parent id"": %d\n", curproc->pid, parproc->pid);
    curproc = parproc;
    parproc = parproc->parent;
  }

}

void
get_descendants(int process_id)
{
  struct proc* curproc;

  //find process
  int process_avail = 0;
  for(int i=0;i<NPROC;i++)
  {
    // cprintf("%d ", ptable.proc[i].pid);
    if(ptable.proc[i].pid == process_id)
    {
      curproc = &(ptable.proc[i]);
      process_avail = 1;
      break;
    }
  }
  // cprintf("\n");
  if (process_avail == 0)
  {
    cprintf("process with pid=%d does not exist\n", process_id);
    return;
  }

  struct proc* childproc;
  int min;
  int child_avail = 1;
  while (child_avail){
    //find earliest child
    min = -1;
    child_avail = 0;
    for(int i=0;i<NPROC;i++)
    {
      // cprintf("%d ", ptable.proc[i].pid);
      if(ptable.proc[i].parent == curproc && (ptable.proc[i].creation_time < min || min == -1))
      {
        childproc = &(ptable.proc[i]);
        min = ptable.proc[i].creation_time;
        child_avail = 1;
      }
    }
    // cprintf("\n");
    if (child_avail == 0)
    {
      cprintf("process with pid=%d does not have any descendants\n", curproc->pid);
      return;
    }
    else
    {
      cprintf("""my id"": %d, ""my child id"": %d\n", curproc->pid, childproc->pid);
      curproc = childproc;
    }
  }  
}

void change_sched_queue(int pid, int new_queue)
{
  struct proc* p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->sched_queue = new_queue;
      break;
    }
  }
}

void change_proc_prio(int pid, int prio){
  struct proc* p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->my_priority = prio;
      break;
    }
  } 
}

void pratio(int pid, int priority_ratio, int ctime_ratio, int exec_cyc_ratio)
{
  struct proc* p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid == pid)
    {
      p->priority_ratio = priority_ratio;
      p->ctime_ratio = ctime_ratio;
      p->exec_cyc_ratio = exec_cyc_ratio;
      break;
    }
  }
}

char* state_names[] = {"UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE" };
char* queue_name[] = {"NORMAL", "RR", "PRIORITY", "BJF", "FCFS"};
char temp[32] = "";

char* itos(int x, char* buf){
  // temp_res = "";
  int i = 0;
  int isNegative = 0;
  if (x < 0) {
    isNegative = 1;
    x *= -1;
  }
  if (x == 0) buf[i++]= '0';
  while (x > 0){
    buf[i++] = (x%10) + '0';
    x /= 10;
  }
  if (isNegative) buf[i++] = '-';
  buf[i] = '\0';

  // cprintf("i = %d\n", i);
  char t;
  for(int j = 0; j < (i)/2; j++){
    t = buf[j];
    buf[j] = buf[i-1-j];
    buf[i-1-j] = t;
  }
  return buf;
}

char* ftos(float x, char* buf){
  int nat = (int)x;
  float dec = (float) (x - nat);
  char my_temp[32];
  char* nat_str = itos(nat, my_temp);

  char dec_str[32];
  int i = 0;
  if (dec == 0) dec_str[i++] = '0';

  for(int j = 0; j < 3; j++){
  // while(dec > 0){
    dec *= 10;
    // cprintf("float part = %d\n", (int)dec );
    dec_str[i++] = (int)dec + '0';
    dec -= (int)dec;
  }
  dec_str[i] = '\0';
  // cprintf
  for(int j = 0; j < strlen(nat_str); j++){
    buf[j] = nat_str[j];
  }
  buf[strlen(nat_str)] = '.';
  int index = 1 + strlen(nat_str);
  for (int j = 0; j < i; j++){
    buf[index++] = dec_str[j];
  }
  buf[index] = '\0';
  return buf; 
}

void plog(void)
{
  struct proc *p;
  float p_rank;

  // cprintf("ftos example: %s, %d\n", ftos(4438.234, temp), strlen(ftos(4438.234, temp)));
  cprintf("------------------------------------------------------------------------------\n");
  cprintf("name    pid  state      queue    priority  rank      ratios      Ecycle  Wtime\n");
  cprintf("                                                (prio,ctime,Ecyc)             \n");
  cprintf("------------------------------------------------------------------------------\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == 0)
      continue;

    p_rank = calc_rank(p);
    cprintf("%s", p->name);
    for (int i = 0; i < 8 - strlen(p->name); i++) cprintf(" ");
    // cprintf("   ");
    cprintf("%d", p->pid);
    for (int i = 0; i < 5 - strlen(itos(p->pid, temp)); i++) cprintf(" ");
    // cprintf("   ");
    cprintf("%s", state_names[p->state]);
    for (int i = 0; i < 11 - strlen(state_names[p->state]); i++) cprintf(" ");
    // cprintf("   ");
    cprintf("%s", queue_name[p->sched_queue]);
    for (int i = 0; i < 9 - strlen(queue_name[p->sched_queue]); i++) cprintf(" ");
    // cprintf("   ");
    cprintf("%d", p->my_priority);
    for (int i = 0; i < 10 - strlen(itos(p->my_priority, temp)); i++) cprintf(" ");
    // cprintf("   ");
    cprintf("%s", ftos(p_rank, temp));
    for (int i = 0; i < 10 - strlen(ftos(p_rank, temp)); i++) cprintf(" ");
    // cprintf("   ");
    cprintf("%d,%d,%d", p->priority_ratio, p->ctime_ratio, p->exec_cyc_ratio);
    for (int i = 0; i < 12 - 2 - strlen(itos(p->priority_ratio, temp)) - strlen(itos(p->ctime_ratio, temp)) - strlen(itos(p->exec_cyc_ratio, temp)); i++) cprintf(" ");
    // cprintf("      ");
    cprintf("%d", p->exec_cyc);
    for (int i = 0; i < 8 - strlen(itos(p->exec_cyc, temp)); i++) cprintf(" ");
    // cprintf("   ");
    cprintf("%d\n", p->wtime);
  }
  cprintf("------------------------------------------------------------------------------\n");
}