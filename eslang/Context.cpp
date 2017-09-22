#include "Context.h"
#include <algorithm>
#include <folly/Conv.h>

namespace s {

TimePoint Context::now() const { return std::chrono::steady_clock::now(); }

Context::RunningProcess::RunningProcess(Pid pid, std::unique_ptr<Process> proc,
                                        ProcessTask t, Context* parent)
  : pid(pid), process(std::move(proc)), task(std::move(t)), parent(parent) {
  if (task.waiting()) {
    ESLANGEXCEPT("Should not have a waiting ptr now");
  }
}

void Context::RunningProcess::resume() {
  try {
    if (dead) {
      return;
    }

    // cleanup old waiting things:
    cancelTimeout();

    try {
      ++resumes;
      task.resume();
    }
    catch (std::exception const& error) {
      parent->addtoDestroy(
        pid, folly::to<std::string>("Caught exception: ", error.what()));
      return;
    }
    if (task.done()) {
      parent->addtoDestroy(pid, {});
      return;
    }

    if (task.waiting()->isReadyForResume()) {
      parent->queueResume(pid, resumes);
    }
    else {
      if (task.waiting()->wakeOnTime()) {
        auto now = parent->now();
        if (*task.waiting()->wakeOnTime() > now) {
          auto const wait = std::chrono::duration_cast<std::chrono::milliseconds>(
            *task.waiting()->wakeOnTime() - now);
          parent->wheelTimer_->scheduleTimeout(this, wait);
        }
        else {
          parent->queueResume(pid, resumes);
        }
      }
      if (auto* future = task.waiting()->wakeOnFuture()) {
        future->via(parent->eventBase()).then([this, resumes = this->resumes]() {
          if (this->resumes == resumes) {
            resume();
          }
        });
      }
    }
  }
  catch (std::exception const& e) {
    LOG(FATAL) << "Uncaught " << e.what();
  }
}

bool Context::RunningProcess::waitingFor(SlotId s) const {
  return task.waiting() && task.waiting()->isWaiting(s);
}

void Context::RunningProcess::send(SendAddress s, MessageBase m) {
  bool waiting = waitingFor(s.slot());
  process->push(s.slot(), std::move(m));
  if (waiting) {
    resume();
  }
}

void Context::RunningProcess::timeoutExpired() noexcept { resume(); }

std::vector<std::unique_ptr<Context::RunningProcess>>::const_iterator
Context::findProc(Pid p) const {
  if (p.idx() >= processes_.size()) {
    return processes_.end();
  }
  auto ret = processes_.begin() + p.idx();
  if ((*ret)->pid.version() != p.version()) {
    return processes_.end();
  }
  return ret;
}

std::vector<std::unique_ptr<Context::RunningProcess>>::iterator
Context::findProc(Pid p) {
  if (p.idx() >= processes_.size()) {
    return processes_.end();
  }
  auto ret = processes_.begin() + p.idx();
  if (!*ret || (*ret)->pid.version() != p.version()) {
    return processes_.end();
  }
  return ret;
}

void Context::destroy(std::unique_ptr<RunningProcess> p, std::string reason) {
  if (reason.size()) {
    LOG(INFO) << "Kill " << p->pid << " for " << reason;
  }
  for (auto to_destroy : p->process->links()) {
    addtoDestroy(to_destroy, reason);
  }
}

Pid Context::nextPid() {
  if (tombstones_.size()) {
    auto ret = Pid(tombstones_.back().idx(), tombstones_.back().version() + 1);
    tombstones_.pop_back();
    return ret;
  }
  processes_.emplace_back();
  return Pid(processes_.size() - 1, 0);
}

void Context::addtoDestroy(Pid p, std::string s) {
  auto it = findProc(p);
  if (it != processes_.end()) {
    (*it)->dead = true;
    toDestroy_.emplace_back(std::move((*it)), std::move(s));
    tombstones_.push_back(p);
  }
}

void Context::queueResume(Pid p, uint64_t resumes) {
  queue_.emplace_back(p);
  queue_.back().resume = resumes;
}

void Context::queueSend(SendAddress a, MessageBase m) {
  queue_.emplace_back(a.pid());
  queue_.back().message = std::make_pair(std::move(a), std::move(m));
}


void Context::processQueueItem(ToProcessItem i) {
  if (i.message) {
    auto it = findProc(i.pid);
    if (it != processes_.end()) {
      (*it)->send(i.message->first, std::move(i.message->second));
    }
  }
  if (i.resume) {
    auto it = findProc(i.pid);
    if (it != processes_.end() && (*it)->resumes == *i.resume) {
      (*it)->resume();
    }
  }
}

void Context::run() {
  try {
    while (processes_.size() != tombstones_.size()) {
      if (queue_.size()) {
        auto i = std::move(queue_.front());
        queue_.pop_front();
        processQueueItem(std::move(i));
      }
      else {
        eventBase_.loopOnce();
      }
      while (toDestroy_.size()) {
        auto m = std::move(toDestroy_.front());
        toDestroy_.pop_front();
        VLOG(3) << "Destroy " << m.first->pid << " for reason " << m.second;
        destroy(std::move(m.first), std::move(m.second));
      }
    }
  }
  catch (std::exception const& e) {
    LOG(INFO) << "Uncaught exception " << e.what();
    throw;
  }
}
}