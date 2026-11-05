//-----------------------------------------------------------------------------
// A thin progress bar along the top of the main window, for computations that
// keep the user interface busy for a noticeable time.
//-----------------------------------------------------------------------------
#include <thread>
#include "solvespace.h"

namespace SolveSpace {

namespace {

// How long the outermost scope runs before the bar appears, so that quick
// computations, as while dragging, do not make it flicker.
const int64_t PROGRESS_DELAY_MS = 250;
// How often the bar is painted at most, unless it grew by at least
// PROGRESS_MIN_STEP.
const int64_t PROGRESS_INTERVAL_MS = 30;
const double  PROGRESS_MIN_STEP = 0.01;

// The bar belongs to the thread that runs the user interface, which is the one
// that initializes this. Scopes on other threads, as in loops run in parallel,
// are kept apart and do not report.
const std::thread::id uiThread = std::this_thread::get_id();
thread_local ProgressScope *innermost = NULL;

int64_t startMillis;
int64_t paintedMillis;
double  paintedValue;
bool    shown;

}

ProgressScope::ProgressScope() {
    parent = innermost;
    if(parent == NULL) {
        base  = 0;
        scale = 1;
        if(std::this_thread::get_id() == uiThread) {
            startMillis  = GetMilliseconds();
            paintedValue = 0;
            shown        = false;
        }
    } else {
        base  = parent->stepFrom;
        scale = parent->stepTo - parent->stepFrom;
    }
    stepFrom = stepTo = base;
    innermost = this;
}

ProgressScope::~ProgressScope() {
    innermost = parent;
    if(parent == NULL && shown && std::this_thread::get_id() == uiThread) {
        shown = false;
        if(SS.GW.window) SS.GW.window->SetProgress(-1);
    }
}

void ProgressScope::Set(double fraction, double next) {
    // Calls from a loop that runs in parallel come in from several threads;
    // only those on our own thread count.
    if(std::this_thread::get_id() != uiThread || innermost != this) return;

    auto clamp = [](double f) { return std::max(0.0, std::min(1.0, f)); };
    stepFrom = base + scale * clamp(fraction);
    stepTo   = (next < 0) ? stepFrom : std::max(stepFrom, base + scale * clamp(next));

    if(!SS.GW.window) return;
    // A nested computation that starts over does not move the bar back.
    double value = std::max(paintedValue, stepFrom);

    int64_t now = GetMilliseconds();
    if(!shown && now - startMillis < PROGRESS_DELAY_MS) return;
    if(shown && now - paintedMillis < PROGRESS_INTERVAL_MS &&
       value - paintedValue < PROGRESS_MIN_STEP) return;

    shown = true;
    paintedMillis = now;
    paintedValue  = value;
    SS.GW.window->SetProgress(value);
}

}
