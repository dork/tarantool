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
#include "tarantool.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "stat.h"

#include <util.h>
#include <tarantool_ev.h>
#include <tbuf.h>
#include <say.h>
#include <fiber.h>
#include <salloc.h>
#include <palloc.h>

#include <assoc.h>
#include TARANTOOL_CONFIG

#define SECS 5
static ev_timer timer;

struct {
	const char *name;
	i64 value[SECS + 1];
} *stats = NULL;
static int stats_size = 0;
static int stats_max = 0;
static int base = 0;

struct fiber *stat_fiber = NULL;

int
stat_register(const char **name, size_t max_idx)
{
	int initial_base = base;

	for (int i = 0; i < max_idx; i++, name++, base++) {
		if (stats_size <= base) {
			stats_size += 1024;
			stats = realloc(stats, sizeof(*stats) * stats_size);
			if (stats == NULL)
				abort();
		}

		stats[base].name = *name;

		if (*name == NULL)
			continue;

		for (int i = 0; i < SECS + 1; i++)
			stats[base].value[i] = 0;

		stats_max = base;
	}

	return initial_base;
}

void
stat_collect(int base, int name, i64 value)
{
	stats[base + name].value[0] += value;
	stats[base + name].value[SECS] += value;
}

void
stat_print(struct tbuf *buf)
{
	int max_len = 0;
	tbuf_printf(buf, "statistics:" CRLF);

	for (int i = 0; i <= stats_max; i++) {
		if (stats[i].name == NULL)
			continue;
		max_len = MAX(max_len, strlen(stats[i].name));
	}

	for (int i = 0; i <= stats_max; i++) {
		if (stats[i].name == NULL)
			continue;

		int diff = 0;
		for (int j = 0; j < SECS; j++)
			diff += stats[i].value[j];

		diff /= SECS;

		tbuf_printf(buf, "  %s:%*s{ rps: %- 6i, total: %- 12" PRIi64 " }" CRLF,
			    stats[i].name, 1 + max_len - (int)strlen(stats[i].name), " ",
			    diff, stats[i].value[SECS]);
	}
}

int stat_iter(int *iter, const char **name, u64 *avg, u64 *total)
{
	int i = *iter;
	for (; i <= stats_max; i++)
		if (stats[i].name)
			break;
	if (i > stats_max)
		return 0;
	*name = stats[i].name;
	*avg = 0;
	for (int j = 0; j < SECS; j++)
		*avg += stats[i].value[j];
	*avg /= SECS;
	*total = stats[i].value[SECS];
	*iter = i + 1;
	return 1;
}

void
stat_age(ev_timer *timer, int events __attribute__((unused)))
{
	if (stats == NULL)
		return;

	for (int i = 0; i <= stats_max; i++) {
		if (stats[i].name == NULL)
			continue;

		for (int j = SECS - 2; j >= 0;  j--)
			stats[i].value[j + 1] = stats[i].value[j];
		stats[i].value[0] = 0;
	}

	ev_timer_again(timer);
}

void
stat_init(void)
{
	ev_init(&timer, stat_age);
	timer.repeat = 1.;
	ev_timer_again(&timer);
}

void
stat_free(void)
{
	ev_timer_stop(&timer);
	if (stats)
		free(stats);
}

void
stat_cleanup(int base, size_t max_idx)
{
	for (int i = base; i < max_idx; i++)
		for (int j = 0; j < SECS + 1; j++)
			stats[i].value[j] = 0;
}

static char stat_graphite_prefix[256];

static void
stat_graphite_init(void)
{
	int off = 0;
	off += snprintf(stat_graphite_prefix, sizeof(stat_graphite_prefix),
			"tarantool.");
	char hostname[128];
	int len = 0;
	if (gethostname(hostname, sizeof(hostname)) == 0)
		len = strlen(hostname);
	else
		len = snprintf(hostname, sizeof(hostname), "unknown");
	int i;
	for (i = 0; i < len; i++)
		if (hostname[i] == '.')
			hostname[i] = '-';
	off += snprintf(stat_graphite_prefix + off,
			sizeof(stat_graphite_prefix) - off, "%s", hostname);
	off += snprintf(stat_graphite_prefix + off,
			sizeof(stat_graphite_prefix) - off, ":%d",
			cfg.primary_port);
}

/*
 * Graphite server driver.
 *
 * Graphite server expects to receive a buffer which contains
 * metrics values formatted as follows:
 *
 * metric.path.name numeric_value timestamp\n
 */
static inline void
stat_graphite_add(struct tbuf *buf, time_t ts, double value, int subc, ...)
{
	va_list args;
	va_start(args, subc);
	tbuf_printf(buf, "%s", stat_graphite_prefix);
	while (subc-- > 0)
		tbuf_printf(buf, ".%s", va_arg(args, char*));
	va_end(args);
	tbuf_printf(buf, " ");
	tbuf_printf(buf, "%f %ju\n", value, (uintmax_t)ts);
}

static void stat_graphite_puller(struct tbuf *stat, time_t tm)
{
	stat_graphite_add(stat, tm, tarantool_uptime(), 1, "server.uptime");

	u64 salloc_bytes_used;
	u64 salloc_items;
	slab_stat2(&salloc_bytes_used, &salloc_items);
	stat_graphite_add(stat, tm, salloc_bytes_used, 1, "slab.bytes_used");
	stat_graphite_add(stat, tm, salloc_items, 1, "slab.items");

	u64 palloc_total;
	u64 palloc_used;
	palloc_stat2(&palloc_total, &palloc_used);
	stat_graphite_add(stat, tm, palloc_total, 1, "palloc.bytes_total");
	stat_graphite_add(stat, tm, palloc_used, 1, "palloc.bytes_used");

	const char *name;
	u64 avg = 0;
	u64 total = 0;
	int i = 0;
	while (stat_iter(&i, &name, &avg, &total)) {
		stat_graphite_add(stat, tm, avg, 3, "op", name, "rps");
		stat_graphite_add(stat, tm, avg, 3, "op", name, "total");
	}
}

static void (*stat_puller_init)(void) = stat_graphite_init;
static void (*stat_puller)(struct tbuf *buf, time_t time) = stat_graphite_puller;

static void
stat_pusher(void *arg)
{
	struct sockaddr_in server;
	memcpy((void*)&server, (struct sockaddr_in*)arg, sizeof(server));
	if (fiber_socket(SOCK_DGRAM) == -1) {
		say_syserror("failed to allocate socket");
		return;
	}

	struct tbuf *stat = tbuf_alloc(fiber->gc_pool);
	while (1) {
		fiber_sleep(cfg.stat_interval);

		time_t tm = time(NULL);
		stat_puller(stat, tm);

		if (fiber_sendto(&server, stat->data, stat->size) == -1)
			say_syserror("sendto()");
		tbuf_reset(stat);
	}
}

/* Statistics pusher.
 *
 * Pusher fiber is created at server start. Upon specified
 * intervals (cfg.stat_interval) it sends collected statistics
 * data to a remote statistics server (cfg.stat).
 *
 * Data is grabbed and formated by statistics puller driver
 * (eg. graphite).
 */
void stat_pusher_init(void)
{
	struct sockaddr_in server;
	char ip_addr[32];
	int port;
	int rc;
	if ((cfg.stat_addr == NULL || !strcmp(cfg.stat_addr, "")) &&
	     cfg.stat_interval > 0)
		return;

	stat_puller_init();

	say_crit("using statistics server %s", cfg.stat_addr);
	rc = sscanf(cfg.stat_addr, "%31[^:]:%i", ip_addr, &port);
	if (rc != 2) {
		say_error("incorrect 'stat_addr' configuration format");
		return;
	}
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if (inet_aton(ip_addr, &server.sin_addr) < 0) {
		say_syserror("inet_aton: %s", ip_addr);
		return;
	}

	stat_fiber = fiber_create("stat_pusher", -1, stat_pusher, &server);
	if (stat_fiber == NULL)
		return;
	fiber_call(stat_fiber);
}
