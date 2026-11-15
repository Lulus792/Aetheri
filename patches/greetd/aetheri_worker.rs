// SPDX-License-Identifier: GPL-3.0-only
// Aetheri integration for the pinned greetd candidate.
//
// Every use holds Context::inner, INCLUDING the only backend waitpid reaper.
// The original direct child stays unreaped until observed here or recorded by
// that reaper. Never use this for a PID supplied by a client or after Start.
use std::future::Future;

use nix::{
    errno::Errno,
    sys::wait::{waitid, Id, WaitPidFlag, WaitStatus},
    unistd::Pid,
};
use tokio::time::{sleep, timeout_at, Duration, Instant};

use crate::error::Error;

pub(crate) struct Worker {
    task: Pid,
    ended: bool,
    sent: bool,
    deadline: Option<Instant>,
    failure: Option<Error>,
    limit: Duration,
}

impl Worker {
    pub(crate) fn new(task: Pid) -> Self {
        Self {
            task,
            ended: false,
            sent: false,
            deadline: None,
            failure: None,
            limit: Duration::from_secs(5),
        }
    }

    // Only the serialized Context reaper calls this with its actual waitpid
    // result, before releasing the same lock. Nonterminal/foreign results do
    // not supply original-child evidence. A prior failure is never cleared.
    pub(crate) fn reaped(&mut self, status: WaitStatus) {
        if matches!(status, WaitStatus::Exited(pid, _) | WaitStatus::Signaled(pid, _, _) if pid == self.task) {
            self.ended = true;
        }
    }

    fn observe(&mut self) -> Result<bool, Error> {
        if let Some(error) = &self.failure {
            return Err(error.clone());
        }
        if self.ended {
            return Ok(true);
        }
        match waitid(
            Id::Pid(self.task),
            WaitPidFlag::WEXITED | WaitPidFlag::WNOHANG | WaitPidFlag::WNOWAIT,
        ) {
            Ok(WaitStatus::StillAlive) | Err(Errno::EINTR) => Ok(false),
            Ok(status @ WaitStatus::Exited(pid, _))
            | Ok(status @ WaitStatus::Signaled(pid, _, _)) if pid == self.task => {
                self.reaped(status);
                Ok(true)
            }
            // ECHILD is lost evidence, never proof that the worker ended.
            _ => {
                let error = Error::Error("original PAM worker observation lost".into());
                self.failure = Some(error.clone());
                Err(error)
            }
        }
    }

    pub(crate) async fn cancel<F, T>(&mut self, send: F) -> Result<(), Error>
    where
        F: FnOnce() -> T,
        T: Future<Output = Result<(), Error>>,
    {
        if self.observe()? {
            return Ok(());
        }
        let limit = self.limit;
        let deadline = *self.deadline.get_or_insert_with(|| Instant::now() + limit);
        let send_once = !self.sent;
        // Retain this before the first await, including if the future is dropped.
        // A send error is neither completion nor permission to send again.
        self.sent = true;
        let result = timeout_at(deadline, async {
            if send_once {
                let _ = send().await;
            }
            loop {
                if self.observe()? {
                    return Ok(());
                }
                sleep(Duration::from_millis(10)).await;
            }
        }).await;
        match result {
            Ok(result) => result,
            Err(_) => {
                let error = Error::Error("original PAM worker cancellation timed out".into());
                self.failure = Some(error.clone());
                Err(error)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use nix::sys::wait::waitpid;
    use std::{cell::Cell, process::Command};

    fn child(seconds: &str) -> std::process::Child {
        Command::new("/usr/bin/sleep").arg(seconds).spawn().unwrap()
    }

    #[tokio::test(flavor = "current_thread")]
    async fn failed_send_waits_for_original_child_and_never_replays() {
        let mut child = child("0.15");
        let mut worker = Worker::new(Pid::from_raw(child.id() as i32));
        let sends = Cell::new(0);
        worker.cancel(|| async {
            sends.set(sends.get() + 1);
            Err(Error::Error("test send failure".into()))
        }).await.unwrap();
        assert!(child.try_wait().unwrap().is_some());
        worker.cancel(|| async { panic!("replayed cancellation") }).await.unwrap();
        assert_eq!(sends.get(), 1);
    }

    #[tokio::test(flavor = "current_thread")]
    async fn send_failure_and_timeout_retain_the_live_obligation() {
        let mut child = child("30");
        let pid = Pid::from_raw(child.id() as i32);
        let mut worker = Worker::new(pid);
        worker.limit = Duration::from_millis(25);
        let first = worker.cancel(|| async { Err(Error::Error("test send failure".into())) }).await;
        let alive = child.try_wait().unwrap().is_none();
        child.kill().unwrap();
        worker.reaped(waitpid(pid, None).unwrap());
        let second = worker.cancel(|| async { panic!("replayed unknown cancellation") }).await;
        assert!(alive && first.is_err() && second.is_err());
        assert_eq!(first.unwrap_err().to_string(), second.unwrap_err().to_string());
    }

    #[tokio::test(flavor = "current_thread")]
    async fn foreign_terminal_child_does_not_release_original() {
        let mut original = child("30");
        let pid = Pid::from_raw(original.id() as i32);
        let other = child("0");
        let mut worker = Worker::new(pid);
        worker.reaped(waitpid(Pid::from_raw(other.id() as i32), None).unwrap());
        let still_original = !worker.observe().unwrap();
        original.kill().unwrap();
        worker.reaped(waitpid(pid, None).unwrap());
        assert!(still_original);
        worker.cancel(|| async { panic!("sent to an already reaped original") }).await.unwrap();
    }

    #[tokio::test(flavor = "current_thread")]
    async fn unrecorded_reap_is_lost_evidence() {
        let mut child = child("0");
        let mut worker = Worker::new(Pid::from_raw(child.id() as i32));
        child.wait().unwrap();
        let result = worker.cancel(|| async { panic!("sent after losing original child") }).await;
        assert!(result.is_err());
        assert!(worker.cancel(|| async { panic!("rearmed lost child") }).await.is_err());
    }

    #[tokio::test(flavor = "current_thread")]
    async fn dropped_wait_keeps_the_original_send_and_deadline() {
        let mut child = child("0.15");
        let mut worker = Worker::new(Pid::from_raw(child.id() as i32));
        let first = tokio::time::timeout(Duration::from_millis(20), worker.cancel(|| async { Ok(()) })).await;
        assert!(first.is_err());
        worker.cancel(|| async { panic!("replayed after dropping the wait") }).await.unwrap();
        assert!(child.try_wait().unwrap().is_some());
    }

    #[tokio::test(flavor = "current_thread")]
    async fn blocked_send_is_bounded_and_never_replayed() {
        let mut child = child("30");
        let pid = Pid::from_raw(child.id() as i32);
        let mut worker = Worker::new(pid);
        worker.limit = Duration::from_millis(25);
        let first = worker.cancel(std::future::pending::<Result<(), Error>>).await;
        let alive = child.try_wait().unwrap().is_none();
        child.kill().unwrap();
        worker.reaped(waitpid(pid, None).unwrap());
        assert!(alive && first.is_err());
        assert!(worker.cancel(|| async { panic!("replayed a blocked send") }).await.is_err());
    }
}
