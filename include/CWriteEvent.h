/*!
    \file
    \brief Общий тип callback-а для уведомления о начале/конце операции записи во flash.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 17.07.2026
*/

#pragma once

/// Callback function type for write event notification.
/// Used by CLittlefsSystem, CSpiffsSystem, COTASystem and COTATask to notify
/// subscribers (e.g. CRadioTask) about the start/end of a flash write operation,
/// so that time-critical work (radio IRQ handling) can be paused for its duration.
/// @param lock true - operation start (write/erase begins), false - operation end
typedef void onWriteEvent(bool lock);
