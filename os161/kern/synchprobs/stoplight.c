/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */
static struct lock* q1;
static struct lock* q2;
static struct lock* q3;
static struct lock* q4;
static struct semaphore* intersection_semaphore;

void
stoplight_init() {
	q1 = lock_create("q1");
	q2 = lock_create("q2");
	q3 = lock_create("q3");
	q4 = lock_create("q4");

	intersection_semaphore = sem_create("intersection", 3);
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	return;
}

void acquire_quadrant_lock(uint32_t);
void acquire_quadrant_lock(uint32_t quadrant_number) {
	switch (quadrant_number) {
		case 0:
			lock_acquire(q1);
			break;
		case 1:
			lock_acquire(q2);
			break;
		case 2:
			lock_acquire(q3);
			break;
		case 3:
			lock_acquire(q4);
			break;
	}
}

void release_quadrant_lock(uint32_t);
void release_quadrant_lock(uint32_t quadrant_number) {
	switch (quadrant_number) {
		case 0:
			lock_release(q1);
			break;
		case 1:
			lock_release(q2);
			break;
		case 2:
			lock_release(q3);
			break;
		case 3:
			lock_release(q4);
			break;
	}
}

void move_to_quadrant(uint32_t, uint32_t, uint32_t);
void move_to_quadrant(uint32_t index, uint32_t previous_quadrant,  uint32_t next_quadrant) {
	acquire_quadrant_lock(next_quadrant);
	inQuadrant(next_quadrant, index);
	release_quadrant_lock(previous_quadrant);
}

void leave_intersection(uint32_t, uint32_t);
void leave_intersection(uint32_t index, uint32_t previous_quadrant) {
	leaveIntersection(index);
	release_quadrant_lock(previous_quadrant);

	V(intersection_semaphore);
}

void enter_intersection(uint32_t, uint32_t);
void enter_intersection(uint32_t index, uint32_t direction) {
	P(intersection_semaphore);

	acquire_quadrant_lock(direction);
	inQuadrant(direction, index);
}

void
turnright(uint32_t direction, uint32_t index)
{
	enter_intersection(index, direction);
	leave_intersection(index, direction);
	return;
}

void
gostraight(uint32_t direction, uint32_t index)
{
	enter_intersection(index, direction);
	move_to_quadrant(index, direction, (direction + 3) % 4);
	leave_intersection(index, (direction + 3) % 4);
	return;
}

void
turnleft(uint32_t direction, uint32_t index)
{
	enter_intersection(index, direction);
	move_to_quadrant(index, direction, (direction + 3) % 4);
	move_to_quadrant(index, (direction + 3) % 4, (direction + 2) % 4);
	leave_intersection(index, (direction + 2) % 4);
	return;
}
