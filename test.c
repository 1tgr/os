#include <stdio.h>
#include "array.h"
#include "cutest/CuTest.h"
#include "inbox.h"
#include "thread.h"

static void test_obj_alloc_release(CuTest *ct) {
    obj_t *o = obj_alloc(sizeof(*o));
	CuAssertIntEquals(ct, 1, o->refs);
    CuAssertIntEquals(ct, 0, obj_release(o));
}

static void test_obj_alloc_retain_release(CuTest *ct) {
    obj_t *o = obj_alloc(sizeof(*o));
	CuAssertIntEquals(ct, 1, o->refs);

    obj_t *o2 = obj_retain(o);
    CuAssertPtrEquals(ct, o, o2);
    CuAssertIntEquals(ct, 2, o2->refs);
    CuAssertIntEquals(ct, 1, obj_release(o2));
    CuAssertIntEquals(ct, 0, obj_release(o2));
}

static void test_obj_alloc_pool_autorelease(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *p = obj_alloc_pool();
    CuAssertIntEquals(ct, 0, p->objs->count);
    CuAssertPtrEquals(ct, p, obj_pool);

    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    CuAssertIntEquals(ct, 1, p->objs->count);
    CuAssertIntEquals(ct, 0, obj_release(&p->obj));
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_obj_alloc_pool_parent_autorelease(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *outer = obj_alloc_pool();
    CuAssertIntEquals(ct, 0, outer->objs->count);
    CuAssertPtrEquals(ct, outer, obj_pool);

    pool_t *inner = obj_alloc_pool();
    CuAssertIntEquals(ct, 0, outer->objs->count);
    CuAssertIntEquals(ct, 0, inner->objs->count);
    CuAssertPtrEquals(ct, inner, obj_pool);

    obj_t * o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    CuAssertIntEquals(ct, 1, inner->objs->count);
    CuAssertIntEquals(ct, 0, obj_release(&inner->obj));
    CuAssertIntEquals(ct, 0, obj_release(&outer->obj));
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_array_alloc(CuTest *ct) {
    array_t *a = array_alloc();
    CuAssertIntEquals(ct, 0, a->count);
    obj_release(&a->obj);
}

static void test_array_add_release(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    array_t *a = array_alloc();
    CuAssertIntEquals(ct, 0, a->count);

    obj_t *o = obj_alloc(sizeof(*o));
    o->dealloc = dealloc;
    array_add(a, o);
    CuAssertIntEquals(ct, 1, obj_release(o));
    CuAssertIntEquals(ct, 0, is_dealloc);
    CuAssertIntEquals(ct, 0, obj_release(&a->obj));
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_inbox_post(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *pool = obj_alloc_pool();
    inbox_t *i = obj_autorelease(&inbox_alloc()->obj);
    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;

    inbox_post(i, o);
    obj_release(&pool->obj);
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_inbox_post_read_sync(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *pool = obj_alloc_pool();
    inbox_t *i = obj_autorelease(&inbox_alloc()->obj);
    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    inbox_post(i, o);

    obj_t *o2 = inbox_read(i);
    CuAssertPtrEquals(ct, o, o2);
    obj_release(&pool->obj);
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void async_inbox_reader(void *arg) {
    void **args = arg;
    inbox_t *inbox = args[0];
    obj_t **o = args[1];
    *o = inbox_read(inbox);
}

static void test_inbox_post_read_async(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *pool = obj_alloc_pool();
    inbox_t *i = obj_autorelease(&inbox_alloc()->obj);
    volatile obj_t *result = NULL;
    void *args[] = { i, &result };
    thread_t *reader_thread = thread_start(async_inbox_reader, args);
    CuAssertIntEquals(ct, thread_waiting, reader_thread->state);
    CuAssertPtrEquals(ct, i, reader_thread->u.waiting.inbox);

    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    inbox_post(i, o);

    obj_t *o2 = (obj_t*) result;
    CuAssertPtrEquals(ct, o, o2);
    obj_release(&pool->obj);
    CuAssertIntEquals(ct, 1, is_dealloc);
}

void test_thread(void *arg) {
	CuSuite* suite = CuSuiteNew();

    {
        CuSuite* s = CuSuiteNew();
        SUITE_ADD_TEST(s, test_obj_alloc_release);
        SUITE_ADD_TEST(s, test_obj_alloc_retain_release);
        SUITE_ADD_TEST(s, test_obj_alloc_pool_autorelease);
        SUITE_ADD_TEST(s, test_obj_alloc_pool_parent_autorelease);
        CuSuiteAddSuite(suite, s);
    }

    {
        CuSuite* s = CuSuiteNew();
        SUITE_ADD_TEST(s, test_array_alloc);
        SUITE_ADD_TEST(s, test_array_add_release);
        CuSuiteAddSuite(suite, s);
    }

    {
        CuSuite* s = CuSuiteNew();
        SUITE_ADD_TEST(s, test_inbox_post);
        SUITE_ADD_TEST(s, test_inbox_post_read_sync);
        SUITE_ADD_TEST(s, test_inbox_post_read_async);
        CuSuiteAddSuite(suite, s);
    }

	CuSuiteRun(suite);

	CuString *output = CuStringNew();
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
}
