"""Actual shell -> full-screen editor -> restored shell, effects and original pins."""
import hashlib,json,os,select,signal,sys,termios,time,unittest
from pathlib import Path
EDITOR=str(Path(sys.argv.pop(1)).resolve());FAULT=str(Path(sys.argv.pop(1)).resolve())
import test_shell_interactive as native
PROMPT=rb'\r\x1b\[Kaetheri> \r\x1b\[9C'
class Editor(unittest.TestCase):
    def setUp(self):
        self.t=native.Terminal('modal-'+self._testMethodName,args=('-i','--edit'))
        self.t.expect(PROMPT);self.modes=termios.tcgetattr(self.t.master);self.last_frame_end=-1
    def tearDown(self):self.t.close()
    def start(self,path='document',existing=False,fault=False):
        if existing:(self.t.root/path).write_bytes(existing)
        self.t.send((FAULT if fault else EDITOR)+((' ' if existing else ' --new ')+path if path else '')+'\n')
        self.t.expect(rb'\x1b\[\?1049h');self.wait(lambda frame:b'NORMAL' in frame)
        self.editor=self.t.foreground();self.pin=os.pidfd_open(self.editor)
        key=(self.editor,os.fstat(self.pin).st_ino)
        if key in self.t.members:os.close(self.pin);self.pin=self.t.members[key]
        else:self.t.members[key]=self.pin
    def frame(self):
        data=bytes(self.t.data);end=data.rfind(b'\x1b[?25h')
        if end<0:return b''
        begin=data.rfind(b'\x1b[?25l',0,end);return data[begin:end+6] if begin>=0 else b''
    def wait(self,predicate,timeout=8):
        end=time.monotonic()+timeout
        while not (bytes(self.t.data).rfind(b'\x1b[?25h')>self.last_frame_end and predicate(self.frame())):
            assert time.monotonic()<end,(self.t.root,bytes(self.t.data[-3000:]));self.t.pump(.01)
        self.last_frame_end=bytes(self.t.data).rfind(b'\x1b[?25h')
        return self.frame()
    def insert(self,text):
        self.t.send('i');self.wait(lambda f:b'INSERT' in f)
        self.t.send(text);self.wait(lambda f:b'modified' in f)
        self.t.send(b'\x1b');self.wait(lambda f:b'NORMAL' in f)
    def shell(self,check_modes=True):
        self.t.expect(rb'\x1b\[\?1049l');self.t.expect(PROMPT)
        if check_modes:self.assertEqual(termios.tcgetattr(self.t.master),self.modes)
    def test_create_edit_save_and_restore_following_shell_input(self):
        self.start();self.assertFalse((self.t.root/'document').exists());self.insert('hello\nworld')
        self.t.send(b'\x13');self.wait(lambda f:b'Saved' in f);self.assertEqual((self.t.root/'document').read_bytes(),b'hello\nworld')
        self.t.send(":q\nprintf 'after' > following\n");self.shell(False);self.t.expect(PROMPT);self.assertEqual(termios.tcgetattr(self.t.master),self.modes)
        self.assertEqual((self.t.root/'following').read_bytes(),b'after')
    def test_dirty_close_save_cancel_and_explicit_discard(self):
        self.start();self.insert('retained');self.t.send(b'\x11');self.wait(lambda f:b'[s] Save [d] Drop' in f)
        self.t.send(b'\x1b');self.wait(lambda f:b'NORMAL' in f and b'[s] Save' not in f)
        self.t.send(':q!\n');self.wait(lambda f:b'Type DISCARD' in f);self.t.send('discard\n');self.wait(lambda f:b'Type DISCARD' in f)
        self.assertFalse((self.t.root/'document').exists());self.t.send(b'\x1b');self.wait(lambda f:b'Type DISCARD' not in f)
        self.t.send(b'\x11');self.wait(lambda f:b'[s] Save' in f);self.t.send('s');self.shell();self.assertEqual((self.t.root/'document').read_bytes(),b'retained')
    def test_unicode_grapheme_backspace_and_endings(self):
        self.start(existing=b'one\r\nlast');self.t.send('A');self.wait(lambda f:b'INSERT' in f)
        self.t.send('j́'.encode()+b'\x7f'+'🇩🇪'.encode()+b'\x7f'+b'!\rnext');self.wait(lambda f:b'next' in f)
        self.t.send(b'\x1b');self.wait(lambda f:b'NORMAL' in f);self.t.send(':wq\n');self.shell()
        self.assertEqual((self.t.root/'document').read_bytes(),b'one!\r\nnext\r\nlast')
    def test_external_conflict_and_create_only_saveas(self):
        self.start(existing=b'original');self.insert('new-');(self.t.root/'document').write_bytes(b'external')
        self.t.send(b'\x13');self.wait(lambda f:b'File changed' in f);self.assertEqual((self.t.root/'document').read_bytes(),b'external')
        self.t.send(':saveas document\n');self.wait(lambda f:b'name exists' in f);self.t.send(':saveas recovered copy\n');self.wait(lambda f:b'Saved' in f)
        self.assertEqual((self.t.root/'recovered copy').read_bytes(),b'new-original');self.t.send(':q\n');self.shell()
    def test_unknown_save_can_export_without_rearming_and_needs_discard(self):
        self.t.send('export AETHERI_TEST_EDITOR_FAILURE directory-after\n');self.t.expect(PROMPT)
        self.start(fault=True);self.insert('first');self.t.send(b'\x13');self.wait(lambda f:b'SAVE UNKNOWN' in f)
        self.assertEqual((self.t.root/'document').read_bytes(),b'first');self.t.send('A');self.wait(lambda f:b'INSERT' in f);self.t.send('-next\x1b');self.wait(lambda f:b'NORMAL' in f)
        self.t.send(b'\x13');self.wait(lambda f:b'no retry' in f);self.assertEqual((self.t.root/'document').read_bytes(),b'first')
        self.t.send(':export recovery\n');self.wait(lambda f:b'Copy saved' in f);self.assertEqual((self.t.root/'recovery').read_bytes(),b'first-next')
        self.assertIn(b'SAVE UNKNOWN',self.frame());self.t.send(':q\n');self.wait(lambda f:b'[e] Copy' in f)
        self.t.send('d');self.wait(lambda f:b'Type DISCARD' in f);self.t.send('DISCARD\n');self.shell()
    def test_clean_document_unknown_close_is_not_silently_accepted(self):
        self.t.send('export AETHERI_TEST_EDITOR_FAILURE directory-after\n');self.t.expect(PROMPT)
        self.start(existing=b'clean',fault=True);self.t.send(b'\x13');self.wait(lambda f:b'SAVE UNKNOWN' in f)
        self.t.send(b'\x11');self.wait(lambda f:b'[e] Copy' in f);self.assertEqual((self.t.root/'document').read_bytes(),b'clean')
        self.t.send('d');self.wait(lambda f:b'Type DISCARD' in f);self.t.send('DISCARD\n');self.shell()
    def test_untitled_save_on_close_requests_path_then_quits(self):
        self.start(path=None);self.assertIn(b'[untitled]',self.frame());self.insert('named later');self.t.send(b'\x11');self.wait(lambda f:b'[s] Save' in f)
        self.t.send('s');self.wait(lambda f:b':saveas ' in f);self.t.send('chosen name\n');self.shell()
        self.assertEqual((self.t.root/'chosen name').read_bytes(),b'named later')
    def test_command_rejection_cannot_submit_a_prefix(self):
        self.start();self.insert('keep');self.t.send(b':q!\xff\n');self.wait(lambda f:b'REJECTED' in f)
        self.assertFalse((self.t.root/'document').exists());self.t.send(b'\x15export '+b'x'*4100+b'\n');self.wait(lambda f:b'REJECTED' in f)
        self.assertFalse((self.t.root/('x'*200)).exists());self.t.send(b'\x1b');self.wait(lambda f:b'REJECTED' not in f)
        self.t.send(':wq\n');self.shell();self.assertEqual((self.t.root/'document').read_bytes(),b'keep')
    def test_resize_compact_does_not_edit_and_resume_keeps_document(self):
        self.start();self.insert('preserved');termios.tcsetwinsize(self.t.master,(6,20));self.wait(lambda f:b'Enlarge' in f)
        self.t.send('iHIDDEN:saveas invisible\r');self.t.pump(.1);termios.tcsetwinsize(self.t.master,(8,30));self.wait(lambda f:b'NORMAL' in f)
        self.t.send(':wq\n');self.shell();self.assertEqual((self.t.root/'document').read_bytes(),b'preserved')
        self.assertFalse((self.t.root/'invisible').exists())
    def test_help_can_scroll_at_minimum_size_without_editing(self):
        self.start(existing=b'kept');termios.tcsetwinsize(self.t.master,(8,30));self.wait(lambda f:b'NORMAL' in f)
        self.t.send(':help\n');self.wait(lambda f:b'keyboard help' in f)
        saw_search=False
        for i in range(64):
            self.t.send('j');self.wait(lambda f:True)
            saw_search=saw_search or b'/ forward;' in self.frame()
            if b'Block selection/panes' in self.frame():break
        self.assertTrue(saw_search);self.assertIn(b'Block selection/panes',self.frame());self.t.send(b'\x1b');self.wait(lambda f:b'kept' in f and b'NORMAL' in f and b'Help j/k scroll' not in f)
        self.t.send(':q\n');self.shell();self.assertEqual((self.t.root/'document').read_bytes(),b'kept')
    def test_suspend_bg_refuses_terminal_and_fg_restores_same_buffer(self):
        self.start();self.insert('suspended');self.t.send(b'\x1a');self.shell()
        self.t.send('bg\n');self.t.expect(PROMPT);self.t.send('fg\n');self.wait(lambda f:b'NORMAL' in f and b'suspended' in f)
        self.t.send(':wq\n');self.shell();self.assertEqual((self.t.root/'document').read_bytes(),b'suspended')
    def test_signal_exit_keeps_emergency_copy(self):
        self.start();self.insert('emergency');signal.pidfd_send_signal(self.pin,signal.SIGTERM);self.shell()
        files=list(self.t.root.glob('.aetheri-editor-recovery-*'));self.assertEqual(len(files),1);self.assertEqual(files[0].read_bytes(),b'emergency')
        self.assertFalse((self.t.root/'document').exists())
    def test_queued_terminations_make_only_one_emergency_copy(self):
        self.start();self.insert('queued termination')
        signal.pidfd_send_signal(self.pin,signal.SIGSTOP)
        self.t.expect(PROMPT)
        for sig in (signal.SIGHUP,signal.SIGTERM,signal.SIGTSTP):signal.pidfd_send_signal(self.pin,sig)
        self.t.send('fg\n');self.shell()
        self.assertTrue(select.select([self.pin],[],[],2)[0], 'terminating original editor suspended again')
        files=list(self.t.root.glob('.aetheri-editor-recovery-*'));self.assertEqual(len(files),1)
        self.assertEqual(files[0].read_bytes(),b'queued termination')
        self.assertFalse((self.t.root/'document').exists())
    def test_stop_during_emergency_copy_does_not_restart_suspension(self):
        self.t.send('export AETHERI_TEST_EDITOR_FAILURE hold-first-sync\n');self.t.expect(PROMPT)
        self.start(fault=True);self.insert('late stop');signal.pidfd_send_signal(self.pin,signal.SIGTERM)
        deadline=time.monotonic()+5
        while not (self.t.root/'editor-sync-ready').exists():
            self.assertLess(time.monotonic(),deadline);self.t.pump(.01)
        signal.pidfd_send_signal(self.pin,signal.SIGTSTP)
        fd=os.open(self.t.root/'editor-sync-release',os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.close(fd)
        self.shell();self.assertTrue(select.select([self.pin],[],[],2)[0], 'termination restarted suspension')
        files=list(self.t.root.glob('.aetheri-editor-recovery-*'));self.assertEqual(len(files),1)
        self.assertEqual(files[0].read_bytes(),b'late stop')
    def test_undo_redo_groups_cursor_and_saved_state(self):
        self.start(existing=b'old');self.t.send('A');self.wait(lambda f:b'INSERT' in f);self.t.send('abc\x1b');self.wait(lambda f:b'NORMAL' in f)
        self.t.send('u');self.wait(lambda f:b'Undone' in f and b'clean' in f);self.assertEqual((self.t.root/'document').read_bytes(),b'old')
        self.t.send(b'\x12');self.wait(lambda f:b'Redone' in f and b'oldabc' in f and b'modified' in f)
        self.t.send(b'\x13');self.wait(lambda f:b'Saved' in f);self.assertEqual((self.t.root/'document').read_bytes(),b'oldabc')
        self.t.send('A');self.wait(lambda f:b'INSERT' in f);self.t.send('d\x1b');self.wait(lambda f:b'NORMAL' in f)
        self.t.send('u');self.wait(lambda f:b'Undone' in f and b'clean' in f);self.t.send(':q\n');self.shell()
    def test_undo_unknown_stays_fenced_and_new_edit_discards_redo(self):
        self.t.send('export AETHERI_TEST_EDITOR_FAILURE directory-after\n');self.t.expect(PROMPT)
        self.start(existing=b'base',fault=True);self.insert('first-');self.t.send(b'\x13');self.wait(lambda f:b'SAVE UNKNOWN' in f)
        self.t.send('u');self.wait(lambda f:b'Undone' in f and b'SAVE UNKNOWN' in f and b'clean' in f)
        self.assertEqual((self.t.root/'document').read_bytes(),b'first-base')
        self.t.send('i');self.wait(lambda f:b'INSERT' in f);self.t.send('new-\x1b');self.wait(lambda f:b'NORMAL' in f)
        self.t.send(b'\x12');self.wait(lambda f:b'No later edit' in f);self.t.send(':export recovered\n');self.wait(lambda f:b'Copy saved' in f)
        self.assertEqual((self.t.root/'recovered').read_bytes(),b'new-base');self.t.send(':q\n');self.wait(lambda f:b'[e] Copy' in f)
        self.t.send('d');self.wait(lambda f:b'Type DISCARD' in f);self.t.send('DISCARD\n');self.shell()
    def test_insert_history_aliases_exit_insert_without_stopping_job(self):
        self.start();self.t.send('i');self.wait(lambda f:b'INSERT' in f);self.t.send('j́'.encode());self.wait(lambda f:b'modified' in f)
        self.t.send(b'\x1f');self.wait(lambda f:b'Undone' in f and b'NORMAL' in f and b'clean' in f)
        self.t.send(b'\x1e');self.wait(lambda f:b'Redone' in f and b'NORMAL' in f);self.t.send(':wq\n');self.shell()
        self.assertEqual((self.t.root/'document').read_bytes(),'j́'.encode())
    def test_fast_escape_then_undo_or_save_does_not_drop_the_next_key(self):
        self.start();self.t.send('ialpha\x1bu');self.wait(lambda f:b'Undone' in f and b'clean' in f)
        self.assertFalse((self.t.root/'document').exists());self.t.send(b'\x12');self.wait(lambda f:b'Redone' in f and b'alpha' in f)
        self.t.send('A gamma\x1b\x13');self.wait(lambda f:b'Saved' in f);self.assertEqual((self.t.root/'document').read_bytes(),b'alpha gamma')
        self.t.send(':q\n');self.shell()
    def test_literal_search_preview_cancel_and_bidirectional_repeat(self):
        original=b'zero cat\nmiddle cat\nlast cat';self.start(existing=original)
        self.t.send('/cat');self.wait(lambda f:b'Match: Enter choose' in f and b'/cat' in f)
        termios.tcsetwinsize(self.t.master,(8,30));self.wait(lambda f:b'Esc back' in f and b'/cat' in f)
        termios.tcsetwinsize(self.t.master,(24,80));self.wait(lambda f:b'Esc back' in f)
        self.t.send(b'\x1b');self.wait(lambda f:b'1:1 | clean' in f)
        self.t.send('n');self.wait(lambda f:b'No accepted search' in f)
        self.t.send('/cat\n');self.wait(lambda f:b'1:6 | clean' in f and b'Match; n next' in f)
        for position in (b'2:8 | clean',b'3:6 | clean'):
            self.t.send('n');self.wait(lambda f:position in f)
        self.t.send('n');self.wait(lambda f:b'No further match' in f and b'3:6 | clean' in f)
        self.t.send('N');self.wait(lambda f:b'2:8 | clean' in f)
        self.t.send('?zero\n');self.wait(lambda f:b'1:1 | clean' in f)
        self.assertEqual((self.t.root/'document').read_bytes(),original);self.t.send(':q\n');self.shell()
    def test_rejected_search_cannot_accept_a_stale_prefix_or_replace_last_query(self):
        self.start(existing=b'cat one cat two cat');self.t.send('/cat\n');self.wait(lambda f:b'Match; n next' in f)
        self.t.send('/cat missing\n');self.wait(lambda f:b'No match' in f)
        self.t.send(b'\x15'+b'c'*257+b'\n');self.wait(lambda f:b'REJECTED' in f)
        self.t.send(b'\x1b');self.wait(lambda f:b'1:1 | clean' in f)
        self.t.send('n');self.wait(lambda f:b'1:9 | clean' in f)
        self.t.send(b'/cat\xff\n');self.wait(lambda f:b'REJECTED' in f)
        self.t.send(b'\x1b');self.wait(lambda f:b'1:9 | clean' in f)
        self.t.send('n');self.wait(lambda f:b'1:17 | clean' in f)
        self.t.send(b'/cat\xc3\x1b');self.wait(lambda f:b'1:17 | clean' in f and b'REJECTED' not in f)
        self.t.send(':q\n');self.shell()
    def test_search_unicode_boundaries_and_recompute_after_document_edit(self):
        self.start(existing='j́ j other\nj́'.encode());self.t.send('/j\n');self.wait(lambda f:b'1:3 | clean' in f)
        self.t.send('/j́\n');self.wait(lambda f:b'2:1 | clean' in f)
        self.t.send('x');self.wait(lambda f:b'modified' in f)
        self.t.send('n');self.wait(lambda f:b'No further match' in f)
        self.t.send(':wq\n');self.shell();self.assertEqual((self.t.root/'document').read_bytes(),'j́ j other\n'.encode())
    def test_search_does_not_settle_unknown_save_and_close_cancels_preview(self):
        self.t.send('export AETHERI_TEST_EDITOR_FAILURE directory-after\n');self.t.expect(PROMPT)
        self.start(existing=b'cat',fault=True);self.t.send(b'\x13');self.wait(lambda f:b'SAVE UNKNOWN' in f)
        self.t.send('/cat');self.wait(lambda f:b'Match: Enter choose' in f)
        self.t.send(b'\x11');self.wait(lambda f:b'[e] Copy' in f and b'SAVE UNKNOWN' in f)
        self.t.send('d');self.wait(lambda f:b'Type DISCARD' in f);self.t.send('DISCARD\n');self.shell()
    def test_search_repeat_resize_and_cleared_preview_keep_exact_document_rows(self):
        self.start(existing=b'zero cat\nmiddle cat\nlast cat')
        self.t.send('jj?cat\n');self.wait(lambda f:b'2:8 | clean' in f)
        self.t.send('nnN');self.wait(lambda f:b'2:8 | clean' in f and b'Match; n next' in f)
        self.t.send('/missing');self.wait(lambda f:b'No match' in f)
        self.t.send(b'\x15cat');self.wait(lambda f:b'Match: Enter choose' in f)
        self.t.send(b'\x1b');self.wait(lambda f:b'2:8 | clean' in f)
        termios.tcsetwinsize(self.t.master,(12,50));self.wait(lambda f:b'2:8 | clean' in f)
        self.t.send('?zero');frame=self.wait(lambda f:b'Match: Enter choose' in f and b'?zero' in f)
        self.assertIn(b'zero',frame);self.assertIn(b'middle cat',frame);self.assertIn(b'last cat',frame)
        self.t.send(b'\x1b');self.wait(lambda f:b'2:8 | clean' in f);self.t.send(':q\n');self.shell()
    def test_distinct_query_bytes_cannot_be_inferred_from_typing_intent(self):
        self.start();self.insert('zero cat\nmiddle cat\nlast cat')
        self.t.send('?zerr');self.wait(lambda f:b'No match' in f and b'?zerr' in f)
        self.t.send(b'\x15zero');self.wait(lambda f:b'Match: Enter choose' in f and b'?zero' in f)
        self.t.send('\n');self.wait(lambda f:b'1:1 | modified' in f)
        self.t.send(':wq\n');self.shell();self.assertEqual((self.t.root/'document').read_bytes(),b'zero cat\nmiddle cat\nlast cat')
    def test_visual_copy_cut_paste_and_undo_keep_saved_state_separate(self):
        self.start(existing=b'alpha beta');self.t.send('vllll');self.wait(lambda f:b'VISUAL' in f and b'Esc Back' in f)
        termios.tcsetwinsize(self.t.master,(8,30));self.wait(lambda f:b'VISUAL' in f and b'Esc Back' in f)
        self.t.send('y');self.wait(lambda f:b'Copied in editor' in f and b'clean' in f)
        self.assertEqual((self.t.root/'document').read_bytes(),b'alpha beta')
        termios.tcsetwinsize(self.t.master,(24,80));self.wait(lambda f:b'NORMAL' in f)
        self.t.send('0P');self.wait(lambda f:b'Pasted from editor' in f and b'modified' in f)
        self.t.send('u');self.wait(lambda f:b'Undone' in f and b'clean' in f)
        self.t.send(b'\x12');self.wait(lambda f:b'Redone' in f);self.t.send(':wq\n');self.shell()
        self.assertEqual((self.t.root/'document').read_bytes(),b'alphaalpha beta')
    def test_visual_line_cut_change_and_register_paste_preserve_crlf(self):
        self.start(existing=b'one\r\ntwo\r\nlast');self.t.send('jVd');self.wait(lambda f:b'Cut in editor' in f)
        self.t.send('k');self.wait(lambda f:b'1:' in f);self.t.send('p');self.wait(lambda f:b'Pasted from editor' in f)
        self.t.send('k0Vc');self.wait(lambda f:b'INSERT' in f and b'Replace selection' in f)
        self.t.send('new\x1b');self.wait(lambda f:b'NORMAL' in f);self.t.send(':wq\n');self.shell()
        self.assertEqual((self.t.root/'document').read_bytes(),b'one\r\nnew\r\nlast')
    def test_visual_selection_cancel_search_and_history_never_settle_unknown(self):
        self.t.send('export AETHERI_TEST_EDITOR_FAILURE directory-after\n');self.t.expect(PROMPT)
        self.start(existing=b'alpha beta',fault=True);self.t.send(b'\x13');self.wait(lambda f:b'SAVE UNKNOWN' in f)
        self.t.send('vll');self.wait(lambda f:b'VISUAL' in f);self.t.send('/beta\n');self.wait(lambda f:b'NORMAL' in f and b'Match; n next' in f)
        self.t.send('v');self.wait(lambda f:b'VISUAL' in f);self.t.send('d');self.wait(lambda f:b'Cut in editor' in f)
        self.t.send('u');self.wait(lambda f:b'Undone' in f and b'SAVE UNKNOWN' in f)
        self.t.send(':q\n');self.wait(lambda f:b'[e] Copy' in f);self.t.send('d');self.wait(lambda f:b'Type DISCARD' in f);self.t.send('DISCARD\n');self.shell()
if __name__=='__main__':
    print('Modal editor PTY evidence:',native.ROOT,flush=True)
    print('Binaries:',json.dumps({p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in (EDITOR,FAULT,native.SHELL)}),flush=True)
    unittest.main(verbosity=2)
