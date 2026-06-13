#include "ActivityManager.h"

#include <ArduinoJson.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "boot_sleep/BootActivity.h"
#include "boot_sleep/SleepActivity.h"
#include "books/BookReadingsActivity.h"
#include "books/LetterPickerActivity.h"
#include "books/PhysicalBookDetailActivity.h"
#include "books/ReadingStatsActivity.h"
#include "browser/FileBrowserActivity.h"
#include "home/HomeActivity.h"
#include "util/CrashActivity.h"
#ifndef SIMULATOR
#include "network/MyneWebServerActivity.h"
#endif
#include "settings/SettingsActivity.h"
#include "util/FullScreenMessageActivity.h"

void ActivityManager::begin() {
  xTaskCreate(&renderTaskTrampoline, "ActivityManagerRender",
              8192,              // Stack size
              this,              // Parameters
              1,                 // Priority
              &renderTaskHandle  // Task handle
  );
  assert(renderTaskHandle != nullptr && "Failed to create render task");
}

void ActivityManager::renderTaskTrampoline(void* param) {
  auto* self = static_cast<ActivityManager*>(param);
  self->renderTaskLoop();
}

void ActivityManager::renderTaskLoop() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Acquire the lock before reading currentActivity to avoid a TOCTOU race
    // where the main task deletes the activity between the null-check and render().
    RenderLock lock;
    if (currentActivity) {
      HalPowerManager::Lock powerLock;  // Ensure we don't go into low-power mode while rendering
      currentActivity->render(std::move(lock));
    }
    // Notify any task blocked in requestUpdateAndWait() that the render is done.
    TaskHandle_t waiter = nullptr;
    taskENTER_CRITICAL(nullptr);
    waiter = waitingTaskHandle;
    waitingTaskHandle = nullptr;
    taskEXIT_CRITICAL(nullptr);
    if (waiter) {
      xTaskNotify(waiter, 1, eIncrement);
    }
  }
}

void ActivityManager::loop() {
  if (currentActivity) {
    // Note: do not hold a lock here, the loop() method must be responsible for acquire one if needed
    currentActivity->loop();
  }

  while (pendingAction != PendingAction::None) {
    if (pendingAction == PendingAction::Pop) {
      RenderLock lock;

      if (!currentActivity) {
        // Should never happen in practice
        LOG_ERR("ACT", "Pop set but currentActivity is null; ignoring pop request");
        pendingAction = PendingAction::None;
        continue;
      }

      ActivityResult pendingResult = std::move(currentActivity->result);

      // Destroy the current activity
      exitActivity(lock);
      pendingAction = PendingAction::None;

      if (stackActivities.empty()) {
        LOG_DBG("ACT", "No more activities on stack, going home");
        lock.unlock();  // goHome may acquire its own lock
        goHome();
        continue;  // Will launch goHome immediately

      } else {
        currentActivity = std::move(stackActivities.back());
        stackActivities.pop_back();
        LOG_DBG("ACT", "Popped from activity stack, new size = %zu", stackActivities.size());
        // Handle result if necessary
        if (currentActivity->resultHandler) {
          LOG_DBG("ACT", "Handling result for popped activity");

          // Move it here to avoid the case where handler calling another startActivityForResult()
          auto handler = std::move(currentActivity->resultHandler);
          currentActivity->resultHandler = nullptr;
          lock.unlock();  // Handler may acquire its own lock
          handler(pendingResult);
        }

        // Request an update to ensure the popped activity gets re-rendered
        if (pendingAction == PendingAction::None) {
          requestUpdate();
        }

        // Handler may request another pending action, we will handle it in the next loop iteration
        continue;
      }

    } else if (pendingActivity) {
      // Current activity has requested a new activity to be launched
      RenderLock lock;

      if (pendingAction == PendingAction::Replace) {
        // Destroy the current activity
        exitActivity(lock);
        // Clear the stack
        while (!stackActivities.empty()) {
          stackActivities.back()->onExit();
          stackActivities.pop_back();
        }
      } else if (pendingAction == PendingAction::Push) {
        // Move current activity to stack
        stackActivities.push_back(std::move(currentActivity));
        LOG_DBG("ACT", "Pushed to activity stack, new size = %zu", stackActivities.size());
      }
      pendingAction = PendingAction::None;
      currentActivity = std::move(pendingActivity);

      lock.unlock();  // onEnter may acquire its own lock
      currentActivity->onEnter();

      // onEnter may request another pending action, we will handle it in the next loop iteration
      continue;
    }
  }

  if (requestedUpdate) {
    requestedUpdate = false;
    // Using direct notification to signal the render task to update
    // Increment counter so multiple rapid calls won't be lost
    if (renderTaskHandle) {
      xTaskNotify(renderTaskHandle, 1, eIncrement);
    }
  }
}

void ActivityManager::exitActivity(const RenderLock& lock) {
  // Note: lock must be held by the caller
  if (currentActivity) {
    currentActivity->onExit();
    currentActivity.reset();
  }
}

void ActivityManager::replaceActivity(std::unique_ptr<Activity>&& newActivity) {
  // Note: no lock here, this is usually called by loop() and we may run into deadlock
  if (currentActivity) {
    // Defer launch if we're currently in an activity, to avoid deleting the current activity
    // leading to the "delete this" problem
    pendingActivity = std::move(newActivity);
    pendingAction = PendingAction::Replace;
  } else {
    // No current activity, safe to launch immediately
    currentActivity = std::move(newActivity);
    currentActivity->onEnter();
  }
}

void ActivityManager::goToFileTransfer() {
#ifndef SIMULATOR
  replaceActivity(std::make_unique<MyneWebServerActivity>(renderer, mappedInput));
#endif
}

void ActivityManager::goToSettings() { replaceActivity(std::make_unique<SettingsActivity>(renderer, mappedInput)); }

void ActivityManager::goToFileBrowser(std::string path) {
  replaceActivity(std::make_unique<FileBrowserActivity>(renderer, mappedInput, std::move(path)));
}

void ActivityManager::goToSleep() {
  replaceActivity(std::make_unique<SleepActivity>(renderer, mappedInput));
  loop();  // Important: sleep screen must be rendered immediately, the caller will go to sleep right after this returns
}

void ActivityManager::goToBoot() { replaceActivity(std::make_unique<BootActivity>(renderer, mappedInput)); }

void ActivityManager::goToFullScreenMessage(std::string message, EpdFontFamily::Style style, bool showBack) {
  replaceActivity(std::make_unique<FullScreenMessageActivity>(renderer, mappedInput, std::move(message), style, HalDisplay::FAST_REFRESH, showBack));
}

void ActivityManager::goToCrashReport() { replaceActivity(std::make_unique<CrashActivity>(renderer, mappedInput)); }

void ActivityManager::goHome() { replaceActivity(std::make_unique<HomeActivity>(renderer, mappedInput)); }

void ActivityManager::goToPhysicalBooks() {
  replaceActivity(std::make_unique<LetterPickerActivity>(renderer, mappedInput));
}

void ActivityManager::goToPhysicalBookDetail(PhysicalBook book) {
  pushActivity(std::make_unique<PhysicalBookDetailActivity>(renderer, mappedInput, std::move(book)));
}

void ActivityManager::goToBookReadings(PhysicalBook book) {
  pushActivity(std::make_unique<BookReadingsActivity>(renderer, mappedInput, std::move(book)));
}

void ActivityManager::goToReadingStats() {
  replaceActivity(std::make_unique<ReadingStatsActivity>(renderer, mappedInput));
}

void ActivityManager::goToLastRead() {
  // Iterate /.myne/readings/ — one small file per book.
  // For each, scan sessions for the latest date. O(1) memory per file.
  std::string bestBookId;
  std::string bestDate;

  if (Storage.exists(ReadingLog::DIR_PATH)) {
    HalFile dir = Storage.open(ReadingLog::DIR_PATH);
    if (dir && dir.isDirectory()) {
      static constexpr size_t BUF_SIZE = 4096;
      auto* buf = static_cast<char*>(malloc(BUF_SIZE));
      if (buf) {
        while (true) {
          HalFile entry = dir.openNextFile();
          if (!entry) break;
          if (entry.isDirectory()) continue;

          char name[64];
          entry.getName(name, sizeof(name));
          const size_t nameLen = strlen(name);
          if (nameLen < 6 || strcmp(name + nameLen - 5, ".json") != 0) continue;

          // Derive bookId from filename (strip .json)
          std::string bookId(name, nameLen - 5);

          const size_t n = entry.read(buf, BUF_SIZE - 1);
          buf[n] = '\0';

          JsonDocument doc;
          if (deserializeJson(doc, buf) != DeserializationError::Ok) continue;

          JsonArray readings = doc.as<JsonArray>();
          if (readings.isNull()) continue;
          for (JsonObject r : readings) {
            JsonArray sessions = r["sessions"].as<JsonArray>();
            if (sessions.isNull()) continue;
            for (JsonObject s : sessions) {
              const char* d = s["d"] | "";
              if (d[0] != '\0' && (bestDate.empty() || strcmp(d, bestDate.c_str()) > 0)) {
                bestDate   = d;
                bestBookId = bookId;
              }
            }
          }
        }
        free(buf);
      }
    }
  }

  if (bestBookId.empty()) {
    goToFullScreenMessage(tr(STR_NO_LAST_READ), EpdFontFamily::REGULAR, true);
    return;
  }

  // Look up the book directly from its own file using short keys.
  PhysicalBook book;
  bool found = false;

  char bookPath[80];
  snprintf(bookPath, sizeof(bookPath), "%s/%s.json", BookStore::DIR_PATH, bestBookId.c_str());
  if (Storage.exists(bookPath)) {
    static constexpr size_t BOOK_BUF_SIZE = 512;
    auto* buf = static_cast<char*>(malloc(BOOK_BUF_SIZE));
    if (buf) {
      const size_t n = Storage.readFileToBuffer(bookPath, buf, BOOK_BUF_SIZE);
      buf[n] = '\0';
      JsonDocument doc;
      if (deserializeJson(doc, buf) == DeserializationError::Ok) {
        book.id         = doc["id"] | "";
        book.title      = doc["t"]  | "";
        book.author     = doc["a"]  | "";
        book.collection = doc["c"]  | "";
        book.volume     = doc["v"]  | "";
        book.location   = doc["l"]  | "";
        book.notes      = doc["n"]  | "";
        found = !book.id.empty() && !book.title.empty();
      }
      free(buf);
    }
  }

  if (!found) {
    goToFullScreenMessage(tr(STR_NO_LAST_READ), EpdFontFamily::REGULAR, true);
    return;
  }

  pushActivity(std::make_unique<PhysicalBookDetailActivity>(renderer, mappedInput, std::move(book)));
}

void ActivityManager::pushActivity(std::unique_ptr<Activity>&& activity) {
  if (pendingActivity) {
    // Should never happen in practice
    LOG_ERR("ACT", "pendingActivity while pushActivity is not expected");
    pendingActivity.reset();
  }
  pendingActivity = std::move(activity);
  pendingAction = PendingAction::Push;
}

void ActivityManager::popActivity() {
  if (pendingActivity) {
    // Should never happen in practice
    LOG_ERR("ACT", "pendingActivity while popActivity is not expected");
    pendingActivity.reset();
  }
  pendingAction = PendingAction::Pop;
}

bool ActivityManager::preventAutoSleep() const { return currentActivity && currentActivity->preventAutoSleep(); }

bool ActivityManager::skipLoopDelay() const { return currentActivity && currentActivity->skipLoopDelay(); }

ScreenshotInfo ActivityManager::getScreenshotInfo() const {
  if (currentActivity) {
    return currentActivity->getScreenshotInfo();
  }
  return {};
}

void ActivityManager::requestUpdate(bool immediate) {
  if (immediate) {
    if (renderTaskHandle) {
      xTaskNotify(renderTaskHandle, 1, eIncrement);
    }
  } else {
    // Deferring the update until current loop is finished
    // This is to avoid multiple updates being requested in the same loop
    requestedUpdate = true;
  }
}
void ActivityManager::requestUpdateAndWait() {
  if (!renderTaskHandle) {
    return;
  }

  // Atomic section to perform checks
  taskENTER_CRITICAL(nullptr);
  auto currTaskHandler = xTaskGetCurrentTaskHandle();
  auto mutexHolder = xSemaphoreGetMutexHolder(renderingMutex);
  bool isRenderTask = (currTaskHandler == renderTaskHandle);
  bool alreadyWaiting = (waitingTaskHandle != nullptr);
  bool holdingRenderLock = (mutexHolder == currTaskHandler);
  if (!alreadyWaiting && !isRenderTask && !holdingRenderLock) {
    waitingTaskHandle = currTaskHandler;
  }
  taskEXIT_CRITICAL(nullptr);

  // Render task cannot call requestUpdateAndWait() or it will cause a deadlock
  assert(!isRenderTask && "Render task cannot call requestUpdateAndWait()");

  // There should never be the case where 2 tasks are waiting for a render at the same time
  assert(!alreadyWaiting && "Already waiting for a render to complete");

  // Cannot call while holding RenderLock or it will cause a deadlock
  assert(!holdingRenderLock && "Cannot call requestUpdateAndWait() while holding RenderLock");

  xTaskNotify(renderTaskHandle, 1, eIncrement);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

// RenderLock

RenderLock::RenderLock() {
  xSemaphoreTake(activityManager.renderingMutex, portMAX_DELAY);
  isLocked = true;
}

RenderLock::RenderLock([[maybe_unused]] Activity&) {
  xSemaphoreTake(activityManager.renderingMutex, portMAX_DELAY);
  isLocked = true;
}

RenderLock::~RenderLock() {
  if (isLocked) {
    xSemaphoreGive(activityManager.renderingMutex);
    isLocked = false;
  }
}

void RenderLock::unlock() {
  if (isLocked) {
    xSemaphoreGive(activityManager.renderingMutex);
    isLocked = false;
  }
}

/**
 *
 * Checks if renderingMutex is busy.
 *
 * @return true if renderingMutex is busy, otherwise false.
 *
 */
bool RenderLock::peek() { return xQueuePeek(activityManager.renderingMutex, NULL, 0) != pdTRUE; };
