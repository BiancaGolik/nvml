/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * obj_cpp_tx.c -- cpp transactions test
 *
 */


 #include "unittest.h"
 #include <libpmemobj/transactions.hpp>
 #include <libpmemobj/p.hpp>
 #include <libpmemobj/persistent_ptr.hpp>
 #include <libpmemobj/pool.hpp>
 #include <libpmemobj/mutex.hpp>
 #include <libpmemobj/shared_mutex.hpp>

#define	LAYOUT "cpp"

using namespace std;
using namespace nvml;
using namespace nvml::obj;

#define LOCKS_NUM 10
#define TEST_VALUE 5
#define N_NESTINGS 100

/* pool root class */
class root {
public:
	p<int> test;
	p<int> data;
	nvml::obj::mutex *mtx[LOCKS_NUM];
	shared_mutex *rwlk[LOCKS_NUM];
};

class test
{
public:
	test(pool<root> &p)
	{
		pop = p;
		r = p.get_root();
	}
	template<typename... T>
	void cpp_tx(T&&... locks)
	{
		r->test = 0;
		transaction *tx;
		try {
			tx = new transaction(pop, TX_COMMIT, locks...);
			r->test = TEST_VALUE;
		} catch (transaction_error &e) {
			ASSERT(0);
		}
		ASSERTeq(r->test, TEST_VALUE);
		delete(tx);
	}
	void cpp_tx_nested_one_lock()
	{
		return;
	}

	template<typename L, typename... T>
	void cpp_tx_nested_one_lock(L lock, T... locks)
	{
		r->test = 0;
		transaction *tx;
		try {
			tx = new transaction(pop, TX_COMMIT, lock);
			r->test = TEST_VALUE;
			cpp_tx_nested_one_lock(locks...);
		} catch (transaction_error &e) {
			ASSERT(0);
		}
		ASSERTeq(r->test, TEST_VALUE);
		delete(tx);
	}

	void cpp_tx_nested_all_locks()
	{
		return;
	}

	template<typename L, typename... T>
	void cpp_tx_nested_all_locks(L lock, T... locks)
	{
		r->test = 0;
		transaction *tx;
		try {
			tx = new transaction(pop, TX_COMMIT, locks...);
			r->test = TEST_VALUE;
			if (level != N_NESTINGS)
				cpp_tx_nested_all_locks(locks...);
		} catch (transaction_error &e) {
			ASSERT(0);
		}
		ASSERTeq(r->test, TEST_VALUE);
		delete(tx);
	}

	template<typename... T>
	void cpp_tx_abort(T... locks)
	{
		r->test = 0;
		r->data = 0;
		transaction *tx;
		try {
			tx = new transaction(pop, TX_COMMIT, locks...);
			r->test = TEST_VALUE;
			tx->abort(-1);
			ASSERT(0);
		} catch (transaction_error &e) {
			r->data = TEST_VALUE;
		}
		ASSERTeq(r->data, TEST_VALUE);
		ASSERTeq(r->test, 0);
		delete(tx);
	}

	template<typename... T>
	void cpp_tx_default_abort(T... locks)
	{
		r->test = 0;
		r->data = 0;
		transaction *tx;
		try {
			tx = new transaction(pop, TX_ABORT, locks...);
			r->test = TEST_VALUE;
		} catch (transaction_error &e) {
			r->data = TEST_VALUE;
		}
		ASSERTeq(r->data, 0);
		ASSERTeq(r->test, TEST_VALUE);
		delete(tx);
	}




	template<typename... T>
	void cpp_do_tx(T... locks)
	{
		level = 0;
		cpp_tx(locks...);
		cpp_tx_nested_one_lock(locks...);
		cpp_tx_nested_all_locks(locks...);
		cpp_tx_abort(locks...);
		cpp_tx_default_abort(locks...);

	}

private:
	pool<root> pop;
	persistent_ptr<root> r;
	p<int> level;
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_tx");
	pool<root> p;
	persistent_ptr<root> r;
	try {
		p = pool<root>::create(argv[1], LAYOUT);
		r = p.get_root();
	} catch (pool_error &e) {
		FATAL("pool create failed: %s", e.what());
	}

	test *t = new test(p);

	for (int i = 0; i < LOCKS_NUM; i++) {
		r->mtx[i] = new nvml::obj::mutex();
		r->rwlk[i] = new nvml::obj::shared_mutex();
	}

	t->cpp_do_tx();
	t->cpp_do_tx(r->mtx[0]);
	t->cpp_do_tx(r->mtx[0], r->rwlk[0]);
	t->cpp_do_tx(r->mtx[0], r->rwlk[0], r->mtx[1], r->rwlk[1], r->mtx[2],
		r->rwlk[2], r->mtx[3], r->rwlk[3], r->mtx[4], r->rwlk[4],
		r->mtx[5], r->rwlk[5], r->mtx[6], r->rwlk[6], r->mtx[7],
		r->rwlk[7], r->mtx[8], r->rwlk[8], r->mtx[9], r->rwlk[9]);
	p.close();

	DONE(NULL);
}
