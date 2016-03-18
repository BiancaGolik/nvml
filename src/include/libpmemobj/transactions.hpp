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

namespace nvml{

namespace obj {

	enum pobj_tx_end {
		TX_ABORT,
		TX_COMMIT,
		TX_END_UNKNOWN
	};

	/**
	 * PMEMobj transaction class
	 *
	 * This class is the pmemobj transaction handler. All operations
	 * between creating and destroing transaction object are treated as
	 * performed in transiction block and can be rolled back
	 */
	class transaction
	{
	public:

		/**
		 * Simple construcor.
		 *
		 * Start pmemobj transaction
		 *
		 * @param[in] pop: pool object.
		 * @param[in] end: enum describing how to end transaction. Only
		 *		valid options are TX_COMMIT or TX_ABORT.
		 *
		 * @throw nvml::transaction_error when pmemobj_tx_begin
		 * function failed.
		 */
		transaction(pool_base &p, enum pobj_tx_end end_op = TX_COMMIT)
		{
			if (pmemobj_tx_begin(p.get_handle(), NULL,
							TX_LOCK_NONE) != 0)
				throw transaction_error(
					"failed to start transaction");

			if (end_op >= TX_END_UNKNOWN)
				throw transaction_error("invalid ");
			end = end_op;
		}

		/**
		 * Locks construcor.
		 *
		 * Start pmemobj transaction and add list of locks to new
		 * transaction
		 *
		 * @param[in] pop: pool object.
		 * @param[in] lock, args: locks of mutex or shared_mutex type.
		 * @param[in] end_op: enum describing how to end transaction. Only
		 *		valid options are TX_COMMIT or TX_ABORT.
		 *
		 * @throw nvml::transaction_error when pmemobj_tx_begin
		 * function or locks adding failed or when invalid end_op
		 * argument is given..
		 */
		template<typename... L>
		transaction(pool_base &p, enum pobj_tx_end end_op,
								L&&... locks)

		{
			if (pmemobj_tx_begin(p.get_handle(), NULL,
							TX_LOCK_NONE) != 0)
				throw transaction_error(
					"failed to start transaction");

			enum pobj_tx_end end = end_op;
			if (end >= TX_END_UNKNOWN)
				throw transaction_error("invalid ");

			int err = add_lock(locks...);

			if (err) {
				pmemobj_tx_abort(EINVAL);
				throw transaction_error("failed to add lock");
			}

		}

		/**
		 * Default destrucor.
		 *
		 * End pmemobj transaction
		 */
		~transaction()
		{
			if (end == TX_ABORT) {
				abort(EINVAL);
			}

			pmemobj_tx_process();
			pmemobj_tx_end();
		}

		/**
		 * Abort current transaction.
		 *
		 * @throw nvml::transaction_error when pmemobj_tx_begin
		 * function or locks adding failed.
		 */
		static void abort(int err)
		{
			pmemobj_tx_abort(err);
			throw transaction_error("explicit abort " +
						std::to_string(err));
		}

		/**
		 * Commit current transaction.
		 *
		 * @throw nvml::transaction_error when trying to commit
		 * transaction in different stage than TX_STAGE_WORK.
		 */
		static void commit(int err)
		{
			if (pmemobj_tx_stage() != TX_STAGE_WORK)
				throw transaction_error("no open transaction");
			pmemobj_tx_process();
		}
	private:
		enum pobj_tx_end end;
		/**
		 * Default add_lock.
		 *
		 * If there are no locks specifiad in constructors arguments
		 * there is no operation performed.
		 *
		 */
		int add_lock()
		{
			return 0;
		}

		/**
		 * Add lock to current transaction.
		 *
		 * Add first lock from list of locks to current transaction
		 * and calls recursive function for the list of the rest locks
		 *
		 * @param[in] lock, args: locks of mutex or shared_mutex type.
		 *
		 * @return error number if adding lock operation fail.
		 *
		 */
		template <typename P, typename... L>
		int add_lock(P &lock, L&&... args)
		{
			int err = add_lock(args...);
			if (err)
				return err;
			return pmemobj_tx_lock(lock->get_type(),
							lock->native_handle());
		}
	};

}

}
#endif	/* PMEMOBJ_TX_HPP */
