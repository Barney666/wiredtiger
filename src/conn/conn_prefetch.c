/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_prefetch_create --
 *     Start the pre-fetch server.
 *
 * FIXME-WT-11691 The pre-fetch server currently starts up when pre-fetch is enabled on the
 *     connection level but this needs to be modified when we add the session level configuration.
 *     Perhaps we could delay starting the utility threads until the first session enables
 *     pre-fetching.
 */
int
__wt_prefetch_create(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    uint32_t session_flags;

    conn = S2C(session);

    /*
     * This might have already been parsed and set during connection configuration, but do it here
     * as well, in preparation for the functionality being runtime configurable.
     */
    WT_RET(__wt_config_gets(session, cfg, "prefetch.available", &cval));
    conn->prefetch_available = cval.val != 0;

    /*
     * Pre-fetch functionality isn't runtime configurable, so don't bother starting utility threads
     * if it isn't available.
     */
    if (!conn->prefetch_available)
        return (0);

    F_SET(conn, WT_CONN_PREFETCH_RUN);

    session_flags = WT_THREAD_CAN_WAIT | WT_THREAD_PANIC_FAIL;
    WT_ERR(__wt_thread_group_create(session, &conn->prefetch_threads, "prefetch-server", 8, 8,
      session_flags, __wt_prefetch_thread_chk, __wt_prefetch_thread_run, NULL));

    return (0);

err:
    /* Quit the prefetch server. */
    WT_TRET(__wt_prefetch_destroy(session));
    return (ret);
}

/*
 * __wt_prefetch_thread_chk --
 *     Check to decide if the pre-fetch thread should continue running.
 */
bool
__wt_prefetch_thread_chk(WT_SESSION_IMPL *session)
{
    return (F_ISSET(S2C(session), WT_CONN_PREFETCH_RUN));
}

/*
 * __wt_prefetch_thread_run --
 *     Entry function for a prefetch thread. This is called repeatedly from the thread group code so
 *     it does not need to loop itself.
 */
int
__wt_prefetch_thread_run(WT_SESSION_IMPL *session, WT_THREAD *thread)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_ITEM(tmp);
    WT_DECL_RET;
    WT_PREFETCH_QUEUE_ENTRY *pe;
    bool locked;

    WT_UNUSED(thread);

    WT_ASSERT(session, session->id != 0);

    conn = S2C(session);
    locked = false;

    WT_RET(__wt_scr_alloc(session, 0, &tmp));

    while (F_ISSET(conn, WT_CONN_PREFETCH_RUN)) {
        /*
         * Wait and cycle if there aren't any pages on the queue. It would be nice if this was
         * interrupt driven, but for now just backoff and re-check.
         */
        if (conn->prefetch_queue_count == 0) {
            __wt_sleep(0, 5000);
            break;
        }
        __wt_spin_lock(session, &conn->prefetch_lock);
        locked = true;
        pe = TAILQ_FIRST(&conn->pfqh);

        /* If there is no work for the thread to do - return back to the thread pool */
        if (pe == NULL)
            break;

        TAILQ_REMOVE(&conn->pfqh, pe, q);
        --conn->prefetch_queue_count;

        WT_PREFETCH_ASSERT(
          session, F_ISSET(pe->ref, WT_REF_FLAG_PREFETCH), block_prefetch_skipped_no_flag_set);

        __wt_spin_unlock(session, &conn->prefetch_lock);
        locked = false;

        /*
         * It's a weird case, but if verify is utilizing prefetch and encounters a corrupted block,
         * stop using prefetch. Some of the guarantees about ref and page freeing are ignored in
         * that case, which can invalidate entries on the prefetch queue. Don't prefetch fast
         * deleted pages - they have special performance and visibility considerations associated
         * with them. Don't prefetch fast deleted pages to avoid wasted effort. We can skip reading
         * these deleted pages into the cache if the fast truncate information is visible in the
         * session transaction snapshot.
         */
        if (!F_ISSET(conn, WT_CONN_DATA_CORRUPTION) && pe->ref->page_del == NULL)
            WT_WITH_DHANDLE(session, pe->dhandle, ret = __wt_prefetch_page_in(session, pe));

        /*
         * It probably isn't strictly necessary to re-acquire the lock to reset the flag, but other
         * flag accesses do need to lock, so it's better to be consistent.
         */
        __wt_spin_lock(session, &conn->prefetch_lock);
        F_CLR(pe->ref, WT_REF_FLAG_PREFETCH);
        __wt_spin_unlock(session, &conn->prefetch_lock);
        WT_ERR(ret);

        __wt_free(session, pe);
    }

err:
    if (locked)
        __wt_spin_unlock(session, &conn->prefetch_lock);
    __wt_scr_free(session, &tmp);
    return (ret);
}

/*
 * __wt_conn_prefetch_queue_push --
 *     Push a ref onto the pre-fetch queue.
 */
int
__wt_conn_prefetch_queue_push(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_PREFETCH_QUEUE_ENTRY *pe;

    conn = S2C(session);

    /*
     * We want to avoid the scenario of requesting pre-fetch on one particular ref many times (e.g
     * when reading along a single page). We can identify this by checking if the previous pre-fetch
     * was performed using the same home ref.
     *
     * In the event that we find this to be true, we perform pre-fetch for approximately the number
     * of pages that were added to the queue (WT_PREFETCH_QUEUE_PER_TRIGGER). We then want to ensure
     * that we will not pre-fetch from this ref for a while, and this is done by checking a counter.
     *
     * The counter variable prefetch_skipped_with_parent tracks the number of skips we have
     * performed on a particular ref. If the number of skips surpasses the number of pages that have
     * been queued for pre-fetch, we are okay to pre-fetch from this ref again. This condition will
     * evaluate to false and the counter will be reset, effectively marking the ref as available to
     * pre-fetch from.
     */
    if (session->pf.prefetch_prev_ref->page == ref->home)
        if (session->pf.prefetch_skipped_with_parent < WT_PREFETCH_QUEUE_PER_TRIGGER) {
            ++session->pf.prefetch_skipped_with_parent;
            WT_STAT_CONN_INCR(session, block_prefetch_skipped_same_ref);
            WT_STAT_CONN_INCR(session, block_prefetch_skipped);
            return (0);
        }

    session->pf.prefetch_skipped_with_parent = 0;

    WT_RET(__wt_calloc_one(session, &pe));
    pe->ref = ref;
    pe->first_home = ref->home;
    pe->dhandle = session->dhandle;
    __wt_spin_lock(session, &conn->prefetch_lock);

    /*
     * Don't add refs from trees that have eviction disabled since they are probably being closed,
     * also never add the same ref twice. These checks need to be carried out while holding the
     * pre-fetch lock - which is why they are internal to the push function.
     */
    if (S2BT(session)->evict_disabled > 0 || F_ISSET(ref, WT_REF_FLAG_PREFETCH))
        ret = EBUSY;
    else {
        F_SET(ref, WT_REF_FLAG_PREFETCH);
        TAILQ_INSERT_TAIL(&conn->pfqh, pe, q);
        ++conn->prefetch_queue_count;
    }
    __wt_spin_unlock(session, &conn->prefetch_lock);

    if (ret != 0)
        __wt_free(session, pe);
    return (ret);
}

/*
 * __wt_conn_prefetch_clear_tree --
 *     Clear pages from the pre-fetch queue, either all pages on the queue or pages from the current
 *     btree - depending on input parameters.
 */
int
__wt_conn_prefetch_clear_tree(WT_SESSION_IMPL *session, bool all)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;
    WT_PREFETCH_QUEUE_ENTRY *pe, *pe_tmp;

    conn = S2C(session);
    dhandle = session->dhandle;

    WT_ASSERT_ALWAYS(session, all || dhandle != NULL,
      "Pre-fetch needs to save a valid dhandle when clearing the queue for a btree");

    __wt_spin_lock(session, &conn->prefetch_lock);
    TAILQ_FOREACH_SAFE(pe, &conn->pfqh, q, pe_tmp)
    {
        if (all || pe->dhandle == dhandle) {
            TAILQ_REMOVE(&conn->pfqh, pe, q);
            F_CLR(pe->ref, WT_REF_FLAG_PREFETCH);
            __wt_free(session, pe);
            --conn->prefetch_queue_count;
        }
    }
    if (all)
        WT_ASSERT(session, conn->prefetch_queue_count == 0);

    __wt_spin_unlock(session, &conn->prefetch_lock);

    return (0);
}

/*
 * __wt_prefetch_destroy --
 *     Destroy the pre-fetch threads.
 */
int
__wt_prefetch_destroy(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;

    conn = S2C(session);

    if (!F_ISSET(conn, WT_CONN_PREFETCH_RUN))
        return (0);

    F_CLR(conn, WT_CONN_PREFETCH_RUN);

    /* Ensure that the pre-fetch queue is drained. */
    WT_TRET(__wt_conn_prefetch_clear_tree(session, true));

    __wt_writelock(session, &conn->prefetch_threads.lock);

    WT_RET(__wt_thread_group_destroy(session, &conn->prefetch_threads));

    return (ret);
}
