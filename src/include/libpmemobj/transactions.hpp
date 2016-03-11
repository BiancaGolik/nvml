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
 * transactions.hpp -- cpp pmemobj transactions implementation
 */
#ifndef PMEMOBJ_TX_HPP
#define PMEMOBJ_TX_HPP

#include "libpmemobj.h"
#include "libpmemobj/pool.hpp"
#include "libpmemobj/detail/pexceptions.hpp"
#include "libpmemobj/mutex.hpp"
#include "libpmemobj/shared_mutex.hpp"
using namespace std;
namespace nvml{
namespace obj {

	class transaction
	{
		int add_lock(void *lane)
		{
			return 0;
		}

		template <typename P, typename... L>
		int add_lock(PMEMobjpool *pop, P &lock, L&&... args)
		{
			int err = add_lock(pop, args...);
			if (err)
				return err;

			return pmemobj_tx_add_lock(pop, lock->get_type(),
							lock->native_handle());
		}
	public:
		template<typename T>
		transaction(pool<T> &p)
		{
			if (pmemobj_tx_begin(p.get_handle(), NULL,
							TX_LOCK_NONE) != 0)
				throw transaction_error(
					"failed to start transaction");
		}

		template<typename T, typename... L>
		transaction(pool<T> &p, L&&... locks)
		{
			if (pmemobj_tx_begin(p.get_handle(), NULL,
							TX_LOCK_NONE) != 0)
				throw transaction_error(
					"failed to start transaction");

			int err = add_lock(p.get_handle(), locks...);

			if (err)
				pmemobj_tx_abort(EINVAL);
		}

		~transaction()
		{
			/* can't throw from destructor */
			pmemobj_tx_process();
			pmemobj_tx_end();
		}

		void abort(int err)
		{
			pmemobj_tx_abort(err);
			throw transaction_error("explicit abort " +
						std::to_string(err));
		}
	};
	void transaction_abort_current(int err)
	{
		pmemobj_tx_abort(err);
		throw transaction_error("explicit abort " +
					std::to_string(err));
	}
}

}
#endif	/* PMEMOBJ_TX_HPP */
