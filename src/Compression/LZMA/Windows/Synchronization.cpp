// Windows/Synchronization.cpp

#include "StdAfx.h"

#include "Synchronization.h"

#ifndef FREEARC_WIN

#include <sched.h>

// Per-object synchronization (see CBaseHandle in Synchronization.h): no global lock and no
// broadcast to unrelated waiters.

static bool TryAcquire (NWindows::NSynchronization::CBaseHandle *h)
{
  // Called with h->_mtx locked
  if (h->type == NWindows::NSynchronization::CBaseHandle::EVENT) {
    if (!h->u.event._state)  return false;
    if (!h->u.event._manual_reset)  h->u.event._state = false;
    return true;
  }
  if (h->u.sema.count > 0) { h->u.sema.count--; return true; }
  return false;
}

static bool WaitOne (NWindows::NSynchronization::CBaseHandle *h, DWORD timeout)
{
  ::pthread_mutex_lock(&h->_mtx);
  bool ok = TryAcquire(h);
  if (timeout == INFINITE)
    while (!ok) {
      ::pthread_cond_wait(&h->_cnd, &h->_mtx);
      ok = TryAcquire(h);
    }
  ::pthread_mutex_unlock(&h->_mtx);
  return ok;
}

DWORD WINAPI WaitForMultipleObjects( DWORD count, const HANDLE *handles, BOOL wait_all, DWORD timeout )
{
  if (timeout != 0 && timeout != INFINITE) {
    printf("\n\n INTERNAL ERROR - WaitForMultipleObjects(...) timeout(%u) != 0 or INFINITE\n\n",(unsigned)timeout);
    abort();
  }
  if (count == 1)
    return WaitOne((NWindows::NSynchronization::CBaseHandle*)handles[0], timeout) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
  if (wait_all) {
    printf("\n\n INTERNAL ERROR - WaitForMultipleObjects(...,true) with several handles is not supported\n\n");
    abort();
  }
  // Several handles (not used by FreeArc): poll them
  for (;;) {
    for (DWORD i=0; i<count; i++)
      if (WaitOne((NWindows::NSynchronization::CBaseHandle*)handles[i], 0))
        return WAIT_OBJECT_0+i;
    if (timeout == 0)  return WAIT_TIMEOUT;
    sched_yield();
  }
}


namespace NWindows {
namespace NSynchronization {

bool CBaseEvent::Set()
{
  ::pthread_mutex_lock(&_mtx);
  this->u.event._state = true;
  if (this->u.event._manual_reset)  ::pthread_cond_broadcast(&_cnd);
  else                              ::pthread_cond_signal(&_cnd);
  ::pthread_mutex_unlock(&_mtx);
  return true;
}

bool CBaseEvent::Reset()
{
  ::pthread_mutex_lock(&_mtx);
  this->u.event._state = false;
  ::pthread_mutex_unlock(&_mtx);
  return true;
}

bool CBaseEvent::Lock()
{
  return WaitOne(this, INFINITE);
}

CEvent::CEvent(bool manualReset, bool initiallyOwn)
{
  if (!Create(manualReset, initiallyOwn))
    CHECK(FALSE,(s,"CreateEvent error"));
}

bool CSemaphore::Release(LONG releaseCount)
{
  if (releaseCount < 1) return false;

  ::pthread_mutex_lock(&_mtx);
  LONG newCount = this->u.sema.count + releaseCount;
  if (newCount > this->u.sema.maxCount)
  {
    ::pthread_mutex_unlock(&_mtx);
    return false;
  }
  this->u.sema.count = newCount;
  if (releaseCount == 1)  ::pthread_cond_signal(&_cnd);
  else                    ::pthread_cond_broadcast(&_cnd);
  ::pthread_mutex_unlock(&_mtx);
  return true;
}

bool CSemaphore::Lock(DWORD timeoutInterval)
{
  return WaitOne(this, timeoutInterval);
}

}}

#endif  // ! FREEARC_WIN
