/*-
 * Copyright (c) 2014-2020 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_timestamp_to_string --
 *     Convert a timestamp to the MongoDB string representation.
 */
char *
__wt_timestamp_to_string(wt_timestamp_t ts, char *ts_string)
{
    WT_IGNORE_RET(__wt_snprintf(ts_string, WT_TS_INT_STRING_SIZE, "(%" PRIu32 ", %" PRIu32 ")",
      (uint32_t)((ts >> 32) & 0xffffffff), (uint32_t)(ts & 0xffffffff)));
    return (ts_string);
}

/*
 * __wt_time_pair_to_string --
 *     Converts a time pair to a standard string representation.
 */
char *
__wt_time_pair_to_string(wt_timestamp_t timestamp, uint64_t txn_id, char *tp_string)
{
    char ts_string[WT_TS_INT_STRING_SIZE];

    WT_IGNORE_RET(__wt_snprintf(tp_string, WT_TP_STRING_SIZE, "%s/%" PRIu64,
      __wt_timestamp_to_string(timestamp, ts_string), txn_id));
    return (tp_string);
}

/*
 * __wt_time_window_to_string --
 *     Converts a time window to a standard string representation.
 */
char *
__wt_time_window_to_string(WT_TIME_WINDOW *tw, char *tw_string)
{
    char ts_string[4][WT_TS_INT_STRING_SIZE];

    WT_IGNORE_RET(__wt_snprintf(tw_string, WT_TIME_STRING_SIZE,
      "start: %s/%s/%" PRIu64 " stop: %s/%s/%" PRIu64 "%s",
      __wt_timestamp_to_string(tw->durable_start_ts, ts_string[0]),
      __wt_timestamp_to_string(tw->start_ts, ts_string[1]), tw->start_txn,
      __wt_timestamp_to_string(tw->durable_stop_ts, ts_string[2]),
      __wt_timestamp_to_string(tw->stop_ts, ts_string[3]), tw->stop_txn,
      tw->prepare ? ", prepared" : ""));
    return (tw_string);
}

/*
 * __wt_time_aggregate_to_string --
 *     Converts a time aggregate to a standard string representation.
 */
char *
__wt_time_aggregate_to_string(WT_TIME_AGGREGATE *ta, char *ta_string)
{
    char ts_string[4][WT_TS_INT_STRING_SIZE];

    WT_IGNORE_RET(__wt_snprintf(ta_string, WT_TIME_STRING_SIZE,
      "newest durable: %s/%s oldest start: %s/%" PRIu64 " newest stop %s/%" PRIu64 "%s",
      __wt_timestamp_to_string(ta->newest_start_durable_ts, ts_string[0]),
      __wt_timestamp_to_string(ta->newest_stop_durable_ts, ts_string[1]),
      __wt_timestamp_to_string(ta->oldest_start_ts, ts_string[2]), ta->oldest_start_txn,
      __wt_timestamp_to_string(ta->newest_stop_ts, ts_string[3]), ta->newest_stop_txn,
      ta->prepare ? ", prepared" : ""));
    return (ta_string);
}

/*
 * __wt_timestamp_to_hex_string --
 *     Convert a timestamp to hex string representation.
 */
void
__wt_timestamp_to_hex_string(wt_timestamp_t ts, char *hex_timestamp)
{
    char *p, v;

    if (ts == 0) {
        hex_timestamp[0] = '0';
        hex_timestamp[1] = '\0';
        return;
    }
    if (ts == WT_TS_MAX) {
#define WT_TS_MAX_HEX_STRING "ffffffffffffffff"
        (void)memcpy(hex_timestamp, WT_TS_MAX_HEX_STRING, strlen(WT_TS_MAX_HEX_STRING) + 1);
        return;
    }

    for (p = hex_timestamp; ts != 0; ts >>= 4)
        *p++ = (char)__wt_hex((u_char)(ts & 0x0f));
    *p = '\0';

    /* Reverse the string. */
    for (--p; p > hex_timestamp;) {
        v = *p;
        *p-- = *hex_timestamp;
        *hex_timestamp++ = v;
    }
}

/*
 * __wt_verbose_timestamp --
 *     Output a verbose message along with the specified timestamp.
 */
void
__wt_verbose_timestamp(WT_SESSION_IMPL *session, wt_timestamp_t ts, const char *msg)
{
    char ts_string[WT_TS_INT_STRING_SIZE];

    __wt_verbose(
      session, WT_VERB_TIMESTAMP, "Timestamp %s: %s", __wt_timestamp_to_string(ts, ts_string), msg);
}

#define WT_TIME_VALIDATE_RET(session, ...)        \
    do {                                          \
        if (silent)                               \
            return (EINVAL);                      \
        WT_RET_MSG(session, EINVAL, __VA_ARGS__); \
    } while (0)

/*
 * __wt_time_aggregate_validate --
 *     Do a aggregated time window validation.
 */
int
__wt_time_aggregate_validate(
  WT_SESSION_IMPL *session, WT_TIME_AGGREGATE *ta, WT_TIME_AGGREGATE *parent, bool silent)
{
    char time_string[2][WT_TIME_STRING_SIZE];

    if (ta->oldest_start_ts > ta->newest_stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has an oldest start time after its newest stop time; time "
          "aggregate %s",
          __wt_time_aggregate_to_string(ta, time_string[0]));

    if (ta->oldest_start_txn > ta->newest_stop_txn)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has an oldest start transaction after its newest stop "
          "transaction; time aggregate %s",
          __wt_time_aggregate_to_string(ta, time_string[0]));

    if (ta->oldest_start_ts > ta->newest_start_durable_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has an oldest start time after its newest start durable time; "
          "time aggregate %s",
          __wt_time_aggregate_to_string(ta, time_string[0]));

    if (ta->newest_stop_ts != WT_TS_MAX && ta->newest_stop_ts > ta->newest_stop_durable_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has a newest stop time after its newest stop durable time; time "
          "aggregate %s",
          __wt_time_aggregate_to_string(ta, time_string[0]));

    if (ta->newest_start_durable_ts > ta->newest_stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has a newest stop durable time after its newest stop time; time "
          "aggregate %s",
          __wt_time_aggregate_to_string(ta, time_string[0]));

    if (ta->newest_stop_durable_ts != WT_TS_NONE &&
      ta->newest_stop_durable_ts < ta->oldest_start_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has a newest stop durable time before its oldest start time; time "
          "aggregate %s",
          __wt_time_aggregate_to_string(ta, time_string[0]));

    /*
     * Optionally validate the time window against a parent's time window.
     */
    if (parent == NULL)
        return (0);

    if (ta->newest_start_durable_ts > parent->newest_start_durable_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has a newest start durable time after its parent's; time "
          "aggregate %s, parent %s",
          __wt_time_aggregate_to_string(ta, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (ta->newest_stop_durable_ts > parent->newest_stop_durable_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has a newest stop durable time after its parent's; time aggregate "
          "%s, parent %s",
          __wt_time_aggregate_to_string(ta, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (ta->oldest_start_ts < parent->oldest_start_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has an oldest start time before its parent's; time aggregate %s, "
          "parent %s",
          __wt_time_aggregate_to_string(ta, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (ta->oldest_start_txn < parent->oldest_start_txn)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has an oldest start transaction before its parent's; time "
          "aggregate %s, parent %s",
          __wt_time_aggregate_to_string(ta, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (ta->newest_stop_ts > parent->newest_stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has a newest stop time after its parent's; time aggregate %s, "
          "parent %s",
          __wt_time_aggregate_to_string(ta, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (ta->newest_stop_txn > parent->newest_stop_txn)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window has a newest stop transaction after its parent's; time aggregate "
          "%s, parent %s",
          __wt_time_aggregate_to_string(ta, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (ta->prepare && !parent->prepare)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window is prepared but its parent is not; time aggregate %s, parent %s",
          __wt_time_aggregate_to_string(ta, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    return (0);
}

/*
 * __wt_time_value_validate --
 *     Do a value time window validation.
 */
int
__wt_time_value_validate(
  WT_SESSION_IMPL *session, WT_TIME_WINDOW *tw, WT_TIME_AGGREGATE *parent, bool silent)
{
    char time_string[2][WT_TIME_STRING_SIZE];

    if (tw->start_ts > tw->stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a start time after its stop time; time window %s",
          __wt_time_window_to_string(tw, time_string[0]));

    if (tw->start_txn > tw->stop_txn)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a start transaction after its stop transaction; time window %s",
          __wt_time_window_to_string(tw, time_string[0]));

    if (tw->start_ts > tw->durable_start_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a start time after its durable start time; time window %s",
          __wt_time_window_to_string(tw, time_string[0]));

    if (tw->stop_ts != WT_TS_MAX && tw->stop_ts > tw->durable_stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a stop time after its durable stop time; time window %s",
          __wt_time_window_to_string(tw, time_string[0]));

    if (tw->durable_start_ts > tw->stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a durable start time after its stop time; time window %s",
          __wt_time_window_to_string(tw, time_string[0]));

    if (tw->durable_stop_ts != WT_TS_NONE && tw->durable_start_ts > tw->durable_stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a durable start time after its durable stop time; time window %s",
          __wt_time_window_to_string(tw, time_string[0]));

    /*
     * Optionally validate the time window against a parent's time window.
     */
    if (parent == NULL)
        return (0);

    if (parent->newest_start_durable_ts != WT_TS_NONE &&
      tw->durable_start_ts > parent->newest_start_durable_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a durable start time after its parent's newest durable start "
          "time; time window %s, parent %s",
          __wt_time_window_to_string(tw, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (tw->start_ts < parent->oldest_start_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a start time before its parent's oldest start time; time window "
          "%s, parent %s",
          __wt_time_window_to_string(tw, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (tw->start_txn < parent->oldest_start_txn)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a start transaction before its parent's oldest start transaction; "
          "time window %s, parent %s",
          __wt_time_window_to_string(tw, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (parent->newest_stop_durable_ts != WT_TS_NONE &&
      tw->durable_stop_ts > parent->newest_stop_durable_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a durable stop time after its parent's newest durable stop time; "
          "time window %s, parent %s",
          __wt_time_window_to_string(tw, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (tw->stop_ts > parent->newest_stop_ts)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a stop time after its parent's newest stop time; time window %s, "
          "parent %s",
          __wt_time_window_to_string(tw, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (tw->stop_txn > parent->newest_stop_txn)
        WT_TIME_VALIDATE_RET(session,
          "value time window has a stop transaction after its parent's newest stop transaction; "
          "time window %s, parent %s",
          __wt_time_window_to_string(tw, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    if (tw->prepare && !parent->prepare)
        WT_TIME_VALIDATE_RET(session,
          "aggregate time window is prepared but its parent is not; time aggregate %s, parent %s",
          __wt_time_window_to_string(tw, time_string[0]),
          __wt_time_aggregate_to_string(parent, time_string[1]));

    return (0);
}
