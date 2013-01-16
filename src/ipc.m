/*
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "ipc.h"
#include "fiber.h"
#include <stdlib.h>
#include <rlist.h>

const ev_tstamp IPC_TIMEOUT_INFINITY = 365*86400*100.0;

struct ipc_channel {
	struct rlist readers, writers;
	unsigned creaders;
	unsigned cwriters;
	unsigned size;
	unsigned beg;
	unsigned count;

	void *bcast_msg;

	void *item[0];
};

bool
ipc_channel_is_empty(struct ipc_channel *ch)
{
	return ch->count == 0;
}

bool
ipc_channel_is_full(struct ipc_channel *ch)
{
	return ch->count >= ch->size;
}


struct ipc_channel *
ipc_channel_alloc(unsigned size)
{
	if (!size)
		size = 1;
	struct ipc_channel *res =
		malloc(sizeof(struct ipc_channel) + sizeof(void *) * size);
	if (res)
		res->size = size;
	return res;
}

void
ipc_channel_init(struct ipc_channel *ch)
{
	ch->beg = ch->count = 0;
	ch->creaders = 0;
	ch->cwriters = 0;
	rlist_init(&ch->readers);
	rlist_init(&ch->writers);
}

void
ipc_channel_cleanup(struct ipc_channel *ch)
{
	while (!rlist_empty(&ch->writers)) {
		struct fiber *f =
			rlist_first_entry(&ch->writers, struct fiber, state);
		rlist_del_entry(f, state);
	}
	while (!rlist_empty(&ch->readers)) {
		struct fiber *f =
			rlist_first_entry(&ch->readers, struct fiber, state);
		rlist_del_entry(f, state);
	}
}

void *
ipc_channel_get_timeout(struct ipc_channel *ch, ev_tstamp timeout)
{
	/* channel is empty */
	if (ch->count == 0 || ch->creaders >= ch->count) {
		rlist_add_tail_entry(&ch->readers, fiber, state);
		ch->creaders++;
		bool cancellable = fiber_setcancellable(true);
		bool timed_out = fiber_yield_timeout(timeout);
		rlist_del_entry(fiber, state);
		ch->creaders--;

		fiber_testcancel();
		fiber_setcancellable(cancellable);

		if (timed_out)
			return NULL;

		if (fiber->waiter) {
			fiber_wakeup(fiber->waiter);
			return ch->bcast_msg;
		}
	}

	assert(ch->count > 0);
	void *res = ch->item[ch->beg];
	if (++ch->beg >= ch->size)
		ch->beg -= ch->size;
	ch->count--;

	if (!rlist_empty(&ch->writers)) {
		struct fiber *f =
			rlist_first_entry(&ch->writers, struct fiber, state);
		rlist_del_entry(f, state);
		fiber_wakeup(f);
	}


	return res;
}

void *
ipc_channel_get(struct ipc_channel *ch)
{
	return ipc_channel_get_timeout(ch, IPC_TIMEOUT_INFINITY);
}

int
ipc_channel_put_timeout(struct ipc_channel *ch, void *data,
			ev_tstamp timeout)
{
	/* channel is full */
	if (ch->count >= ch->size || ch->cwriters >= ch->size - ch->count) {

		rlist_add_tail_entry(&ch->writers, fiber, state);
		ch->cwriters++;

		bool cancellable = fiber_setcancellable(true);
		bool timed_out = fiber_yield_timeout(timeout);
		rlist_del_entry(fiber, state);
		ch->cwriters--;

		fiber_testcancel();
		fiber_setcancellable(cancellable);

		if (timed_out) {
			errno = ETIMEDOUT;
			return -1;
		}
	}

	assert(ch->count < ch->size);

	unsigned i = ch->beg;
	i += ch->count;
	ch->count++;
	if (i >= ch->size)
		i -= ch->size;

	ch->item[i] = data;
	if (!rlist_empty(&ch->readers)) {
		struct fiber *f =
			rlist_first_entry(&ch->readers, struct fiber, state);
		rlist_del_entry(f, state);
		fiber_wakeup(f);
	}
	return 0;
}

void
ipc_channel_put(struct ipc_channel *ch, void *data)
{
	ipc_channel_put_timeout(ch, data, IPC_TIMEOUT_INFINITY);
}

bool
ipc_channel_has_readers(struct ipc_channel *ch)
{
	return ch->creaders > 0;
}

bool
ipc_channel_has_writers(struct ipc_channel *ch)
{
	return ch->cwriters > 0;
}

int
ipc_channel_broadcast(struct ipc_channel *ch, void *data)
{
	if (rlist_empty(&ch->readers)) {
		ipc_channel_put(ch, data);
		return 1;
	}

	struct fiber *f;
	int count = 0;
	rlist_foreach_entry(f, &ch->readers, state) {
		count++;
	}

	for (int i = 0; i < count && !rlist_empty(&ch->readers); i++) {
		struct fiber *f =
			rlist_first_entry(&ch->readers, struct fiber, state);
		rlist_del_entry(f, state);
		assert(f->waiter == NULL);
		f->waiter = fiber;
		ch->bcast_msg = data;
		fiber_wakeup(f);
		fiber_yield();
		f->waiter = NULL;
		fiber_testcancel();
		if (rlist_empty(&ch->readers)) {
			count = i;
			break;
		}
	}

	return count;
}
