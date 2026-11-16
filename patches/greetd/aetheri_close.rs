// SPDX-License-Identifier: GPL-3.0-only
// Optional Aetheri integration. This proves normal completion of the original
// PAM worker, never descendant/cgroup drain or application lease settlement.
use std::time::Duration;
use nix::{
    errno::Errno,
    sys::{signal::{kill, pthread_sigmask, SigSet, SigmaskHow, Signal},
          wait::{waitid, waitpid, Id, WaitPidFlag, WaitStatus}},
    unistd::{getpid, getppid, Pid},
};
use crate::error::Error;

// One fresh external worker process. Block TERM before opening PAM/forking;
// only its child restores the inherited mask before exec. Synchronous signal
// waiting has no handler, polling timer or extra thread.
pub(crate) struct Signals { old_mask: SigSet, events: SigSet, parent: Pid, worker: Pid, parent_lost: bool }
impl Signals {
    pub(crate) fn prepare() -> Result<Self, Error> {
        let parent=getppid();
        if parent.as_raw()<=1 { return Err("original login parent absent".into()); }
        let mut blocked=SigSet::empty();blocked.add(Signal::SIGTERM);blocked.add(Signal::SIGUSR1);blocked.add(Signal::SIGCHLD);
        let mut old_mask=SigSet::empty();
        pthread_sigmask(SigmaskHow::SIG_BLOCK,Some(&blocked),Some(&mut old_mask))?;
        if old_mask.contains(Signal::SIGTERM) || old_mask.contains(Signal::SIGUSR1) || old_mask.contains(Signal::SIGCHLD) {
            return Err("PAM worker inherited blocked session signals".into());
        }
        Ok(Self {old_mask,events:blocked,parent,worker:getpid(),parent_lost:false})
    }
    pub(crate) fn child(&self) -> Result<(), Error> {
        // Sole fork child; SIGTERM is still blocked until the original worker
        // relationship and the child's already-armed PDEATHSIG are checked.
        if getppid()!=self.worker { return Err("original PAM parent absent".into()); }
        pthread_sigmask(SigmaskHow::SIG_SETMASK,Some(&self.old_mask),None)?;
        Ok(())
    }
    pub(crate) fn activate(&mut self) {
        // Bracket the parent's PDEATHSIG setup; never adopt a later parent.
        self.parent_lost=getppid()!=self.parent;
    }
    pub(crate) fn wait(&self, child: Pid) {
        let mut sent=false;let mut killed=false;let mut force=false;let mut requested=self.parent_lost;
        loop {
            if requested && !sent {
                sent=true;
                // This process is the sole reaper of its original fork child.
                // It has not reaped it yet, so this PID cannot be reused.
                let _=kill(child,Signal::SIGTERM);
            }
            if force && !killed { killed=true;let _=kill(child,Signal::SIGKILL); }
            match waitpid(child,Some(WaitPidFlag::WNOHANG)) {
                Ok(WaitStatus::Exited(pid,_)) | Ok(WaitStatus::Signaled(pid,_,_)) if pid==child => {
                    // Keep TERM blocked through PAM close/end. Restore the old
                    // CHLD mask so PAM can wait for its own teardown helpers.
                    let mut closing=self.old_mask;closing.add(Signal::SIGTERM);closing.add(Signal::SIGUSR1);
                    if pthread_sigmask(SigmaskHow::SIG_SETMASK,Some(&closing),None).is_err() {
                        retain("PAM teardown signal mask lost");
                    }
                    return;
                },
                Ok(WaitStatus::StillAlive) | Err(Errno::EINTR) => (),
                // Lost identity cannot authorize PAM teardown over a live child.
                _ => retain("original session child observation lost"),
            }
            match self.events.wait() {
                Ok(Signal::SIGTERM) => requested=true,
                Ok(Signal::SIGUSR1) => force=true,
                Ok(Signal::SIGCHLD) | Err(Errno::EINTR) => (),
                _ => retain("session signal observation lost"),
            }
        }
    }
}

pub(crate) fn retain(message: &str) -> ! {
    eprintln!("AETHERI_PAM_SHUTDOWN_PENDING {}",message);
    loop { std::thread::park(); }
}
pub(crate) fn completed(task: Pid, status: WaitStatus) -> bool {
    matches!(status, WaitStatus::Exited(pid,0) if pid==task)
}
fn normal_end(task: Pid) -> Result<bool,Error> {
    match waitid(Id::Pid(task),WaitPidFlag::WEXITED|WaitPidFlag::WNOHANG|WaitPidFlag::WNOWAIT) {
        Ok(WaitStatus::Exited(pid,0)) if pid==task => Ok(true),
        Ok(WaitStatus::StillAlive) | Err(Errno::EINTR) => Ok(false),
        _ => Err("original PAM worker did not complete normally".into()),
    }
}
// Context holds its exclusive lock across this whole call, excluding its sole
// reaper and new session work. `task` is the retained original direct child.
// Persist `sent` in the original current slot before signaling/awaiting; dropping
// a future cannot permit another signal. No timeout implies successful closure.
pub(crate) async fn finish(task: Pid,sent: &mut bool) -> Result<(),Error> {
    if normal_end(task)? { return Ok(()); }
    if !*sent { *sent=true;let _=kill(task,Signal::SIGTERM); }
    loop {
        if normal_end(task)? { return Ok(()); }
        tokio::time::sleep(Duration::from_millis(10)).await;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::process::Command;
    #[test]
    fn only_original_normal_worker_result_completes() {
        let original=Pid::from_raw(123);let other=Pid::from_raw(124);
        assert!(completed(original,WaitStatus::Exited(original,0)));
        for status in [WaitStatus::Exited(original,1),WaitStatus::Exited(other,0),
                       WaitStatus::Signaled(original,Signal::SIGKILL,false),WaitStatus::StillAlive] {
            assert!(!completed(original,status));
        }
    }
    #[tokio::test(flavor="current_thread")]
    async fn normal_original_end_without_signal() {
        let mut child=Command::new("/usr/bin/true").spawn().unwrap();
        let task=Pid::from_raw(child.id() as i32);
        while !normal_end(task).unwrap() {tokio::time::sleep(Duration::from_millis(2)).await;}
        let mut sent=false;finish(task,&mut sent).await.unwrap();assert!(!sent);
        assert!(child.wait().unwrap().success());
    }
    #[tokio::test(flavor="current_thread")]
    async fn abnormal_and_lost_original_never_complete() {
        let mut child=Command::new("/usr/bin/false").spawn().unwrap();let task=Pid::from_raw(child.id() as i32);
        loop {match normal_end(task) {Ok(false)=>tokio::time::sleep(Duration::from_millis(2)).await,_=>break}}
        let mut sent=false;assert!(finish(task,&mut sent).await.is_err() && !sent);
        child.wait().unwrap();assert!(finish(task,&mut sent).await.is_err() && !sent);
    }
    #[tokio::test(flavor="current_thread")]
    async fn dropped_wait_never_resignals_and_waits_for_normal_completion() {
        use std::io::{BufRead,BufReader};
        let script="import signal,time,sys,os\ncount=0\ndef close(*unused):\n global count\n count+=1\n time.sleep(.12)\n os.write(1,('CLOSED '+str(count)+'\\n').encode())\n sys.exit(0 if count==1 else 2)\nsignal.signal(signal.SIGTERM,close)\nos.write(1,b'READY\\n')\nwhile True:time.sleep(1)\n";
        let mut child=Command::new("/usr/bin/python3").args(["-I","-B","-c",script]).stdout(std::process::Stdio::piped()).spawn().unwrap();
        let mut output=BufReader::new(child.stdout.take().unwrap());let mut line=String::new();output.read_line(&mut line).unwrap();assert_eq!(line,"READY\n");
        let task=Pid::from_raw(child.id() as i32);let mut sent=false;
        assert!(tokio::time::timeout(Duration::from_millis(20),finish(task,&mut sent)).await.is_err());
        assert!(sent);finish(task,&mut sent).await.unwrap();assert!(child.wait().unwrap().success());
        line.clear();output.read_line(&mut line).unwrap();assert_eq!(line,"CLOSED 1\n");
    }
}
