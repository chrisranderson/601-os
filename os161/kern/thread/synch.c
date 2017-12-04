/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count) {
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstring_copy(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem) {
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

// Try to get a semaphore
void
P(struct semaphore *sem) {
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(current_thread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

// Release a sempahore
void
V(struct semaphore *sem) {
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

/*

	lock - defined in synch.h
		- lk_name
		- hangman lockable
*/

struct lock *
lock_create(const char *name) {
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));

	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstring_copy(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	lock->wait_channel = wchan_create(lock->lk_name);
	if (lock->wait_channel == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->spinlock);
	lock->is_locked = false;

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

	return lock;
}

void
lock_destroy(struct lock *lock) {
	KASSERT(lock != NULL);

	if (lock->is_locked) {
		panic("#### Trying to destroy a lock that is still locked.");
	}

	spinlock_cleanup(&lock->spinlock);
	wchan_destroy(lock->wait_channel);

	kfree(lock->owner);

	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock) {

	/* Call this (atomically) before waiting for a lock */
	HANGMAN_WAIT(&current_thread->t_hangman, &lock->lk_hangman);

	spinlock_acquire(&lock->spinlock);
	
	if (lock->owner == current_thread) {
		panic("#### Trying to re-aquire lock.");
	}

	while (lock->is_locked) {
		wchan_sleep(lock->wait_channel, &lock->spinlock);
	}

	lock->is_locked = true;
	lock->owner = current_thread;

	spinlock_release(&lock->spinlock);

	// P(lock->semaphore);
	// lock->owner = current_thread;
	// (void)lock;  // suppress warning until code gets written

	/* Call this (atomically) once the lock is acquired */
	HANGMAN_ACQUIRE(&current_thread->t_hangman, &lock->lk_hangman);
}

void
lock_release(struct lock *lock) {
	spinlock_acquire(&lock->spinlock);

	if (lock->owner != current_thread) {
		panic("#### Owner is trying to release thread it doesn't own.");
	}
	
	lock->is_locked = false;
	wchan_wakeone(lock->wait_channel, &lock->spinlock);
	lock->owner = NULL;
	spinlock_release(&lock->spinlock);


	/* Call this (atomically) when the lock is released */
	HANGMAN_RELEASE(&current_thread->t_hangman, &lock->lk_hangman);
	// (void)lock;  // suppress warning until code gets written
}

void
fancy_lock_release(struct lock *lock) {
	spinlock_acquire(&lock->spinlock);

	lock->is_locked = false;
	wchan_wakeone(lock->wait_channel, &lock->spinlock);
	lock->owner = NULL;
	spinlock_release(&lock->spinlock);


	/* Call this (atomically) when the lock is released */
	HANGMAN_RELEASE(&current_thread->t_hangman, &lock->lk_hangman);
	// (void)lock;  // suppress warning until code gets written
}

bool
lock_do_i_hold(struct lock *lock) {
	// (void)lock;  // suppress warning until code gets written
	return lock->owner == current_thread;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name) {
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstring_copy(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->spinlock);

	cv->wait_channel = wchan_create(cv->cv_name);
	if (cv->wait_channel == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}
	// add stuff here as needed

	return cv;
}

void
cv_destroy(struct cv *cv) {
	KASSERT(cv != NULL);

	// add stuff here as needed

	wchan_destroy(cv->wait_channel);
	spinlock_cleanup(&cv->spinlock);
	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock) {
	if (!lock->is_locked) {
		panic("You think you can waltz in here without holding the lock?! Get outta here!"); // ZAPPALA: Why assert vs. panic?
	}

	spinlock_acquire(&cv->spinlock);
	lock_release(lock);

	// This can
	// while (lock->is_locked) {
	// 	wchan_sleep(lock->wait_channel, &lock->spinlock);
	// }


	// It isn't waking up!
	wchan_sleep(cv->wait_channel, &cv->spinlock);
	spinlock_release(&cv->spinlock); // ZAPPALA: How does the other thread progress?


	lock_acquire(lock);
	// Write this
	// (void)cv;    // suppress warning until code gets written
	// (void)lock;  // suppress warning until code gets written
}

void
cv_signal(struct cv *cv, struct lock *lock) {
	if (!lock->is_locked) {
		panic("You think you can waltz in here without holding the lock?! Get outta here!"); 
		// ZAPPALA: Why should this be locked?
	}
	spinlock_acquire(&cv->spinlock);
	
	// lock_acquire(lock);
	wchan_wakeone(cv->wait_channel, &cv->spinlock);

	spinlock_release(&cv->spinlock);
	// (void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_broadcast(struct cv *cv, struct lock *lock) {
	if (!lock->is_locked) {
		panic("You think you can waltz in here without holding the lock?! Get outta here!");
	}

	spinlock_acquire(&cv->spinlock);
	wchan_wakeall(cv->wait_channel, &cv->spinlock);
	spinlock_release(&cv->spinlock);

	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct rwlock * rwlock_create(const char * name) {
	struct rwlock* self;

	self = kmalloc(sizeof(*self));

	if (self == NULL) {
		return NULL;
	}

	self->name = kstring_copy(name);
	if (self->name == NULL) {
		kfree(self);
		return NULL;
	}

	self->count_lock = lock_create("count_lock");
	self->write_lock = lock_create("write_lock");

	self->counter = 0;

	return self;
}

void rwlock_destroy(struct rwlock * rwlock) {
	KASSERT(rwlock != NULL);

	lock_destroy(rwlock->count_lock);
	lock_destroy(rwlock->write_lock);
	
	kfree(rwlock->name);
	kfree(rwlock);
}

void rwlock_acquire_read(struct rwlock * rwlock) {
	lock_acquire(rwlock->count_lock);
	rwlock->counter += 1;

	if (rwlock->counter == 1) {
		lock_acquire(rwlock->write_lock);
	}

	fancy_lock_release(rwlock->count_lock);
}

void rwlock_release_read(struct rwlock * rwlock) {
	lock_acquire(rwlock->count_lock);
	rwlock->counter -= 1;

	if (rwlock->counter == 0) {
		fancy_lock_release(rwlock->write_lock);
	}

	fancy_lock_release(rwlock->count_lock);
}

void rwlock_acquire_write(struct rwlock * rwlock) {
	lock_acquire(rwlock->write_lock);
}

void rwlock_release_write(struct rwlock * rwlock) {
	fancy_lock_release(rwlock->write_lock);
}
