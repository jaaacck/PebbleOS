/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/services/timeline/timeline_actions.h>

#include <clar.h>

// Test Data
///////////////////////////////////////////////////////////
#include "test_data.h"

static TimelineItemAction s_reply_action = {
  .id = 0,
  .type = TimelineItemActionTypeResponse,
  .attr_list = (AttributeList){
    .num_attributes = 1, .attributes = (Attribute[1]){{.id = AttributeIdTitle, .cstring = "Reply"}}
  }
};

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_common.h"

// Externs
///////////////////////////////////////////////////////////
extern const int TIMELINE_ACTION_ENDPOINT;

typedef struct ActionResultData ActionResultData;
extern ActionResultData *prv_invoke_action(ActionMenu *action_menu,
                                           const TimelineItemAction *action,
                                           const TimelineItem *pin, const char *label);

// Fakes / Helpers
///////////////////////////////////////////////////////////
static const uint8_t *s_expected_send_data = NULL;
static bool s_sent_action = false;
static size_t s_sent_length = 0;
static bool s_window_state_supported = false;
static int s_num_sends;

bool comm_session_has_capability(CommSession *session, CommSessionCapability capability) {
  return s_window_state_supported && capability == CommSessionNotificationWindowStateSupport;
}

bool comm_session_send_data(CommSession *session, uint16_t endpoint_id, const uint8_t *data,
                            size_t length, uint32_t timeout_ms) {
  s_num_sends++;
  if (s_expected_send_data == NULL) {
    return false;
  }

  if (endpoint_id != TIMELINE_ACTION_ENDPOINT) {
    return false;
  }

  cl_assert_equal_m(s_expected_send_data, data, length);
  s_sent_action = true;
  s_sent_length = length;
  return true;
}

// KernelMain callbacks run when the test drains them
#define MAX_CALLBACKS 8
static struct {
  void (*cb)(void *data);
  void *data;
} s_callbacks[MAX_CALLBACKS];
static int s_num_callbacks;

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  cl_assert(s_num_callbacks < MAX_CALLBACKS);
  s_callbacks[s_num_callbacks].cb = callback;
  s_callbacks[s_num_callbacks++].data = data;
}

static void prv_run_callbacks(void) {
  while (s_num_callbacks > 0) {
    void (*cb)(void *data) = s_callbacks[0].cb;
    void *data = s_callbacks[0].data;
    s_num_callbacks--;
    memmove(&s_callbacks[0], &s_callbacks[1], s_num_callbacks * sizeof(s_callbacks[0]));
    cb(data);
  }
}

static uint32_t s_ancs_dismissed_uids[4];
static int s_num_ancs_dismissed;

void ancs_perform_action(uint32_t notification_uid, uint8_t action_id) {
  cl_assert(s_num_ancs_dismissed < (int)ARRAY_LENGTH(s_ancs_dismissed_uids));
  s_ancs_dismissed_uids[s_num_ancs_dismissed++] = notification_uid;
}

// Notification storage holding the items in s_stored
static const TimelineItem *s_stored[4];
static uint8_t s_stored_status[4];
static int s_num_stored;
static bool s_storage_cleared;

static void prv_store(const TimelineItem *item, uint8_t status) {
  s_stored[s_num_stored] = item;
  s_stored_status[s_num_stored++] = status;
}

void notification_storage_iterate(bool (*iter_callback)(void *data,
                                                        SerializedTimelineItemHeader *header),
                                  void *data) {
  for (int i = 0; (i < s_num_stored) && !s_storage_cleared; i++) {
    SerializedTimelineItemHeader header = {.common = s_stored[i]->header};
    header.common.status = s_stored_status[i];
    if (!iter_callback(data, &header)) {
      return;
    }
  }
}

bool notification_storage_get(const Uuid *id, TimelineItem *item_out) {
  for (int i = 0; (i < s_num_stored) && !s_storage_cleared; i++) {
    if (uuid_equal(&s_stored[i]->header.id, id)) {
      *item_out = *s_stored[i];
      return true;
    }
  }
  return false;
}

void notification_storage_reset_and_init(void) {
  s_storage_cleared = true;
}

void notification_storage_set_status(const Uuid *id, uint8_t status) {
}

// Setup
/////////////////////////
void test_timeline_actions__initialize(void) {
  s_expected_send_data = NULL;
  s_sent_action = false;
  s_sent_length = 0;
  s_window_state_supported = false;
  s_num_sends = 0;
  s_num_callbacks = 0;
  s_num_ancs_dismissed = 0;
  s_num_stored = 0;
  s_storage_cleared = false;
}

void test_timeline_actions__cleanup(void) {
}

// Tests
///////////////////////////

// Tests a regular response to a notification
void test_timeline_actions__response(void) {
  const TimelineItem item = {
    .attr_list =
        (AttributeList){
          .num_attributes = 5,
          .attributes =
              (Attribute[5]){
                {.id = AttributeIdTitle, .cstring = "Ian Graham"},
                {.id = AttributeIdBody, .cstring = "this is a test notification"},
                {.id = AttributeIdIconTiny, .uint32 = TIMELINE_RESOURCE_GENERIC_SMS},
                {.id = AttributeIdBgColor, .uint8 = GColorIslamicGreenARGB8}
              }
        },
    .action_group = (TimelineItemActionGroup){.num_actions = 1, .actions = &s_reply_action}
  };

  s_expected_send_data = s_sms_reply_action_data;
  prv_invoke_action(NULL, &item.action_group.actions[0], &item, "Yo, what's up?");
  cl_assert(s_sent_action);
}

// Tests that we send the required data for the Send Text app and reply to call features
void test_timeline_actions__send_text(void) {
  const TimelineItem item = {
    .header = {.id = UUID_SEND_SMS},
    .attr_list =
        (AttributeList){
          .num_attributes = 2,
          .attributes =
              (Attribute[2]){
                {.id = AttributeIdSender, .cstring = "555-123-4567"},
                {.id = AttributeIdiOSAppIdentifier, .cstring = "com.pebble.android.phone"}
              }
        },
    .action_group = (TimelineItemActionGroup){.num_actions = 1, .actions = &s_reply_action}
  };

  s_expected_send_data = s_send_text_data;
  prv_invoke_action(NULL, &item.action_group.actions[0], &item, "Yo, what's up?");
  cl_assert(s_sent_action);
}

void test_timeline_actions__displayed_item(void) {
  const Uuid id = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                   0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
  const uint8_t expected[] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
  s_window_state_supported = true;
  s_expected_send_data = expected;
  timeline_action_endpoint_send_displayed_item(&id);
  cl_assert(s_sent_action);
  cl_assert_equal_i(s_sent_length, sizeof(expected));
}

void test_timeline_actions__nothing_displayed(void) {
  const uint8_t expected[] = {0x04, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  s_window_state_supported = true;
  s_expected_send_data = expected;
  timeline_action_endpoint_send_displayed_item(NULL);
  cl_assert(s_sent_action);
  cl_assert_equal_i(s_sent_length, sizeof(expected));
}

void test_timeline_actions__displayed_item_needs_phone_support(void) {
  const uint8_t expected[] = {0x04};
  s_expected_send_data = expected;
  timeline_action_endpoint_send_displayed_item(NULL);
  cl_assert(!s_sent_action);
}

// Clear All
///////////////////////////

static TimelineItemAction s_ancs_dismiss_action = {
  .type = TimelineItemActionTypeAncsNegative,
  .attr_list = {
    .num_attributes = 1, .attributes = (Attribute[1]){{.id = AttributeIdAncsAction, .uint8 = 1}}
  },
};

static TimelineItemAction s_app_dismiss_action = {
  .type = TimelineItemActionTypeDismiss,
};

static const TimelineItem s_ios_notification = {
  .header =
      {.id = {0x01}, .type = TimelineItemTypeNotification, .ancs_uid = 42, .ancs_notif = true},
  .action_group = {.num_actions = 1, .actions = &s_ancs_dismiss_action},
};

static const TimelineItem s_dismissed_ios_notification = {
  .header =
      {.id = {0x02}, .type = TimelineItemTypeNotification, .ancs_uid = 43, .ancs_notif = true},
  .action_group = {.num_actions = 1, .actions = &s_ancs_dismiss_action},
};

static const TimelineItem s_android_notification = {
  .header =
      {.id = {0x03},
       .type = TimelineItemTypeNotification,
       .parent_id = UUID_NOTIFICATIONS_DATA_SOURCE},
  .action_group = {.num_actions = 1, .actions = &s_app_dismiss_action},
};

void test_timeline_actions__clear_history_dismisses_on_the_phone(void) {
  prv_store(&s_ios_notification, 0);
  prv_store(&s_dismissed_ios_notification, TimelineItemStatusDismissed);
  prv_store(&s_android_notification, 0);

  timeline_actions_clear_history();
  prv_run_callbacks();

  // Only the notifications still active, then the history is cleared
  cl_assert_equal_i(s_num_ancs_dismissed, 1);
  cl_assert_equal_i(s_ancs_dismissed_uids[0], 42);
  cl_assert_equal_i(s_num_sends, 1);
  cl_assert(s_storage_cleared);
}

void test_timeline_actions__clear_empty_history(void) {
  timeline_actions_clear_history();
  prv_run_callbacks();
  cl_assert_equal_i(s_num_ancs_dismissed, 0);
  cl_assert(s_storage_cleared);
}
