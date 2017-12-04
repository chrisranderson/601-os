/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>
// #include <test_data.c>

#define NTHREADS 4



/*
 * Use these stubs to test your reader-writer locks.
 */



static void reader_thread (void* unstructured_data, unsigned long thread_number) {
	test_struct* test_data = (test_struct*) unstructured_data;

	rwlock_acquire_read(test_data->lock);
	kprintf("%lu-%s\n", thread_number, test_data->data);
	rwlock_release_read(test_data->lock);
}

test_struct* make_struct (void);
test_struct* make_struct () {
	struct test_struct *test;

	test = kmalloc(sizeof(*test));
	test->lock = rwlock_create("blah");
	test->data = (char*) "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20";

	return test;
}


// multiple readers
int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

	int result;

	test_struct* test_data = make_struct();

	for (int i = 0; i < NTHREADS; i++) {
		result = thread_fork("rwtest", NULL, reader_thread, test_data, i);
		kprintf("%i", i);
	}

	kprintf("All done!");

	success(TEST161_SUCCESS, SECRET, "rwt1");

	(void) result;
	return 0;
}

// multiple writers
int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt2 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt2");

	return 0;
}

// readers with a writer
int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt3 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

// starve the writer
int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt4 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

// starve a particular reader with lots of writers
int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt5 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
