/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/notifications/ancs/ancs_reconcile.h>
#include <pbl/services/timeline/item.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup services_notifications_pending_dismissals Pending dismissals
 * @ingroup services_notifications
 * @brief Dismissals made while the phone was disconnected.
 *
 * They are kept across reboots and sent once the phone is back: to the Pebble app when it
 * reconnects, or over ANCS once iOS has replayed Notification Center.
 * @{
 */

/**
 * @brief Queue the dismissal of a notification dismissed while the phone can't be reached.
 *
 * @param notification Notification dismissed.
 * @param dismiss Its dismiss action.
 * @return false if the notification isn't dismissed on the phone at all.
 */
bool pending_dismissals_add(const TimelineItem *notification, const TimelineItemAction *dismiss);

/** @brief Send the dismissals queued for the Pebble app. Call when it connects. */
void pending_dismissals_send_to_app(void);

/**
 * @brief Check whether any dismissal is queued for iOS.
 *
 * @return true if there is one.
 */
bool pending_dismissals_has_ancs(void);

/**
 * @brief Find the newest queued iOS dismissal whose UID iOS replayed.
 *
 * Used to check whether iOS kept its UIDs when the watch has no notification of its own to check
 * with.
 *
 * @param uids UIDs iOS replayed.
 * @param num_uids Number of UIDs.
 * @param[out] canary_out Entry of the dismissal found.
 * @return false if there is none.
 */
bool pending_dismissals_find_ancs_canary(const uint32_t *uids, size_t num_uids,
                                         ANCSReconcileEntry *canary_out);

/**
 * @brief Called to send a dismissal to iOS.
 *
 * @param ancs_uid ANCS notification UID.
 * @param action_id ANCS action id.
 */
typedef void (*PendingDismissalSendCallback)(uint32_t ancs_uid, uint8_t action_id);

/**
 * @brief Send the queued iOS dismissals whose UID iOS replayed, and drop the others, as iOS no
 * longer has them.
 *
 * For use when the UIDs are unchanged.
 *
 * @param uids UIDs iOS replayed.
 * @param num_uids Number of UIDs.
 * @param send Called for each dismissal to send.
 */
void pending_dismissals_resolve_ancs_by_uid(const uint32_t *uids, size_t num_uids,
                                            PendingDismissalSendCallback send);

/**
 * @brief Send each queued iOS dismissal to the replayed notification with the same app and date,
 * and drop the others, as iOS no longer has them or they can't be told apart.
 *
 * For use when the UIDs changed.
 *
 * @param entries Entries iOS replayed.
 * @param num_entries Number of entries.
 * @param send Called for each dismissal to send.
 */
void pending_dismissals_resolve_ancs_by_content(const ANCSReconcileEntry *entries,
                                                size_t num_entries,
                                                PendingDismissalSendCallback send);

/** @} */
