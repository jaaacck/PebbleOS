/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <comm/ble/kernel_le_client/ancs/ancs_types.h>

/**
 * @defgroup services_notifications_ancs_ancs_reconcile ANCS reconciliation
 * @ingroup services_notifications_ancs
 * @brief Brings the iOS notifications on the watch in line with Notification Center.
 *
 * After a reconnection iOS replays the notifications it still has when the watch subscribes to
 * ANCS. Active iOS notifications on the watch that iOS no longer has are dismissed.
 * @{
 */

/** @brief A notification iOS still has, identified by its content rather than only its UID. */
typedef struct {
  /** ANCS notification UID. */
  uint32_t uid;
  /** Hash of the app identifier, see ancs_notifications_util_hash_app_id(). */
  uint32_t app_id_hash;
  /** UTC date of the notification, or 0 when iOS sent no valid date. */
  time_t timestamp;
} ANCSReconcileEntry;

/**
 * @brief Build an entry from the attributes iOS returned for a notification.
 *
 * @param uid ANCS notification UID.
 * @param app_id App identifier attribute.
 * @param date Date attribute.
 * @param[out] entry_out Entry to fill.
 */
void ancs_reconcile_entry_from_attributes(uint32_t uid, const ANCSAttribute *app_id,
                                          const ANCSAttribute *date, ANCSReconcileEntry *entry_out);

/**
 * @brief Check whether the watch has any active iOS notification to compare.
 *
 * @return true if there is one.
 */
bool ancs_reconcile_has_candidates(void);

/**
 * @brief Find the newest active iOS notification on the watch whose UID iOS replayed.
 *
 * @param uids UIDs iOS replayed.
 * @param num_uids Number of UIDs.
 * @param[out] canary_out Entry of the notification found.
 * @return false if there is none.
 */
bool ancs_reconcile_find_canary(const uint32_t *uids, size_t num_uids,
                                ANCSReconcileEntry *canary_out);

/**
 * @brief Check whether iOS still describes the canary the same way.
 *
 * @param expected Entry of the canary on the watch.
 * @param fetched Entry iOS returned for the canary's UID.
 * @return true if it is still the same notification.
 */
bool ancs_reconcile_canary_matches(const ANCSReconcileEntry *expected,
                                   const ANCSReconcileEntry *fetched);

/**
 * @brief Dismiss the active iOS notifications whose UID iOS no longer has.
 *
 * For use when the UIDs are unchanged.
 *
 * @param uids UIDs iOS replayed.
 * @param num_uids Number of UIDs.
 * @param live_uids UIDs of notifications received since reconnecting, which are always kept.
 * @param num_live_uids Number of live UIDs.
 */
void ancs_reconcile_by_uid(const uint32_t *uids, size_t num_uids, const uint32_t *live_uids,
                           size_t num_live_uids);

/**
 * @brief Match notifications on app and timestamp, dismiss the ones iOS no longer has and move
 * the others to their new UIDs.
 *
 * For use when the UIDs changed.
 *
 * @param entries Entries iOS replayed.
 * @param num_entries Number of entries.
 * @param live_uids UIDs of notifications received since reconnecting, which are always kept.
 * @param num_live_uids Number of live UIDs.
 */
void ancs_reconcile_by_content(const ANCSReconcileEntry *entries, size_t num_entries,
                               const uint32_t *live_uids, size_t num_live_uids);

/** @} */
