/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/kernel/compiler.h>

bool PBL_WEAK timeline_prefs_get_show_all_day_events(void) {
  return false;
}

void PBL_WEAK timeline_prefs_set_show_all_day_events(bool show) {
}
