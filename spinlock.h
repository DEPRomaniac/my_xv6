// Mutual exclusion lock.
#define W_LOCK 1
#define R_LOCK 0

struct spinlock {
  uint locked;       // Is the lock held?
  uint rec_locked;   // number of times lock is recursively locked    
  // struct proc* p;	// The process holding the lock.
  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
  // uint lock_type;
};

