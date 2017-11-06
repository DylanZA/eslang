#include "Context.h"
#include <algorithm>
#include <folly/Conv.h>

namespace s {

TimePoint Context::now() const { return std::chrono::steady_clock::now(); }

Context::RunningProcess::RunningProcess(Pid pid, std::unique_ptr<Process> proc,
                                        ProcessTask t, Context* parent)
    : pid(pid), process(std::move(proc)), task(std::move(t)), parent(parent),
      timer(parent->ioService()) {
}

void Context::RunningProcess::resume() {
  try {
    if (dead) {
      return;
    }

    // cleanup old waiting things:
    timer.cancel();
    try {
      if (lastWaiting) {
        if (auto* p = lastWaiting->wakeOnFuture()) {
          p->process();
        }
      }
      ++resumes;
      lastWaiting = task.resume();
      if (task.done()) {
        parent->addtoDestroy(pid, {});
        return;
      }
    } catch (std::exception const& error) {
      parent->addtoDestroy(
          pid, folly::to<std::string>("Caught exception: ", error.what()));
      return;
    }

    if (lastWaiting->isReadyForResume()) {
      parent->queueResume(pid, resumes);
    } else {
      if (lastWaiting->sleepFor()) {
        timer.expires_from_now(boost::posix_time::milliseconds(
          lastWaiting->sleepFor()->count()));
        timer.async_wait([this](const boost::system::error_code& error) {
          if (error == boost::asio::error::operation_aborted) {
            return;
          }
          this->resume();
        });
      }
      if (auto* promise = lastWaiting->wakeOnFuture()) {
        promise->setContinuation(parent->ioService(),
                                 [ this, resumes = this->resumes ]() {
                                   if (this->resumes == resumes) {
                                     resume();
                                   }
                                 });
      }
    }
  } catch (std::exception const& e) {
    LOG(FATAL) << "Uncaught " << e.what();
  }
}

bool Context::RunningProcess::waitingFor(SlotId s) const {
  return lastWaiting && lastWaiting->isWaiting(s);
}

void Context::RunningProcess::send(SendAddress s, MessageBase m) {
  bool waiting = waitingFor(s.slot());
  process->push(s.slot(), std::move(m));
  if (waiting) {
    resume();
  }
}

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
  auto const pid = p->pid;
  if (reason.size()) {
    LOG(INFO) << "Kill " << pid << " for " << reason;
  }

  for (auto to_destroy : p->process->killOnDie()) {
    addtoDestroy(to_destroy, reason);
  }

  for (auto to_notify : p->process->notifyOnDie()) {
    queueSend(to_notify, Message<Pid>(pid));
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
      } else {
        ioService_.run_one();
        // make sure to flush the queue so that anything that we are about to
        // kill, if it has timers, they will not be already on the queue
        while (ioService_.poll())
          ;
      }
      while (toDestroy_.size()) {
        auto m = std::move(toDestroy_.front());
        toDestroy_.pop_front();
        VLOG(3) << "Destroy " << m.first->pid << " for reason " << m.second;
        destroy(std::move(m.first), std::move(m.second));
      }
    }
  } catch (std::exception const& e) {
    LOG(INFO) << "Uncaught exception " << e.what();
    throw;
  }
}
}