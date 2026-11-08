"""Real own-editor PTYs, complete command effects and original job-mode handoff."""
import hashlib
import json
import os
from pathlib import Path
import signal
import sys
import termios
import time
import unittest

import test_shell_interactive as native
ALLOCATION=str(Path(sys.argv.pop(1)).resolve())

PROMPT=rb'\r\x1b\[Kaetheri> \r\x1b\[9C'


class OwnEditor(unittest.TestCase):
    def setUp(self):
        self.t=native.Terminal('editor-'+self._testMethodName,args=('-i','--edit'))
        self.t.expect(PROMPT)

    def tearDown(self):self.t.close()

    def command(self,text):
        self.t.send(text.encode() if isinstance(text,str) else text)
        self.t.send(b'\n');self.t.expect(PROMPT)

    def file(self,name,value):self.assertEqual((self.t.root/name).read_bytes(),value)

    def test_midline_insertion_home_end_delete_and_kill_yank(self):
        self.command(b"printf 'ac'\x02\x02b\x05 > edited")
        self.file('edited',b'abc')
        self.command(b"printf 'abxc'\x02\x02\x02\x1b[3~\x05 > deleted")
        self.file('deleted',b'abc')
        self.command(b"printf killed\x01\x0b\x19 > yanked")
        self.file('yanked',b'killed')

    def test_unterminated_file_output_has_a_boundary_before_editor_redraw(self):
        (self.t.root/'no-lf').write_bytes(b'VISIBLE_WITHOUT_NEWLINE')
        self.t.send(b'cat no-lf\n')
        self.t.expect(rb'VISIBLE_WITHOUT_NEWLINE\r\n'+PROMPT)
        self.t.send(b'p');self.t.expect(rb'\r\x1b\[Kaetheri> p')
        self.t.send(b'\x15');self.t.expect(PROMPT)
        self.command('cat no-lf > exact-copy');self.file('exact-copy',b'VISIBLE_WITHOUT_NEWLINE')

    def test_undo_redo_keeps_text_cursor_and_completed_token_separate(self):
        self.command(b"printf 'ac'\x02\x02b\x1f\x1e\x05 > undone")
        self.file('undone',b'abc')
        (self.t.root/'alpha-one').write_bytes(b'one');(self.t.root/'alpha-two').write_bytes(b'two')
        self.t.send(b'cat alpha\t');self.t.expect(rb"'alpha-'")
        self.t.send(b'\t');self.t.expect(rb"'alpha-one'\r\n'alpha-two'")
        self.command(b'\x1f\x1eone > prefix-proof');self.file('prefix-proof',b'one')

    def test_literal_completion_never_runs_a_candidate_or_splits_a_filename(self):
        helper=self.t.root/'marker-command';helper.write_text("#!/bin/sh\nprintf command\n: > candidate-ran\n");helper.chmod(0o700)
        self.t.send(b'./marker-c\t');self.t.expect(rb"'./marker-command' ")
        self.assertFalse((self.t.root/'candidate-ran').exists())
        self.command(b'> command-proof');self.file('command-proof',b'command');self.file('candidate-ran',b'')
        (self.t.root/"value $;'with space").write_bytes(b'literal')
        self.command(b'cat val\t > literal-proof');self.file('literal-proof',b'literal')
        (self.t.root/'space dir').mkdir();(self.t.root/'space dir/nested data').write_bytes(b'nested')
        self.command(b'cat spa\tnes\t > nested-proof');self.file('nested-proof',b'nested')

    def test_undo_or_completion_cannot_reopen_a_rejected_draft(self):
        self.t.send(b'touch rejected-undo\x00\x1f\x1e\t\n');self.t.expect(rb'input rejected; Ctrl\+U or Ctrl\+C')
        self.assertFalse((self.t.root/'rejected-undo').exists())
        self.t.send(b'\x15');self.t.expect(PROMPT)
        self.command(b'\x1f\x1eprintf fresh > clean-draft');self.file('clean-draft',b'fresh')
        self.assertFalse((self.t.root/'rejected-undo').exists())

    def test_failed_edit_allocation_blocks_every_raw_delete_or_cut(self):
        for name,move,key in (('backspace',b'',b'\x7f'),('delete',b'\x02',b'\x04'),('cut-start',b'',b'\x15'),
                              ('cut-end',b'\x01',b'\x0b'),('word',b'',b'\x17'),('csi-delete',b'\x02',b'\x1b[3~'),('completion',b'',b'\t')):
            t=native.Terminal('allocation-'+name,binary=ALLOCATION,args=('-i','--edit'))
            try:
                if name=='completion':(t.root/'failed-edit-target').touch()
                t.expect(PROMPT);t.send(b'touch failed-edit'+move+key+b'\n')
                t.expect(rb'input rejected; Ctrl\+U or Ctrl\+C')
                self.assertFalse((t.root/'failed-edit').exists())
                t.send(b'\x15');t.expect(PROMPT);t.send(b'printf fresh > after-failure\n');t.expect(PROMPT)
                self.assertEqual((t.root/'after-failure').read_bytes(),b'fresh')
            finally:t.close()

    def test_history_search_accepts_without_execution_and_navigates_both_ways(self):
        self.command('printf old > search-old');self.command('printf new > search-new')
        (self.t.root/'search-old').unlink();(self.t.root/'search-new').unlink()
        self.t.send(b'\x12search');self.t.expect(rb'find> search \| printf new > search-new')
        for key,text in ((b'\x12',b'old'),(b'\x13',b'new'),(b'\x1b[A',b'old'),(b'\x1b[B',b'new')):
            self.t.send(key);self.t.expect(rb'find> search \| printf '+text+b' > search-'+text)
        self.assertFalse((self.t.root/'search-new').exists())
        self.t.send(b'\n');self.t.expect(rb'\r\x1b\[Kaetheri> printf new > search-new')
        self.assertFalse((self.t.root/'search-new').exists())
        self.t.send(b'\n');self.t.expect(PROMPT);self.file('search-new',b'new')
        self.assertFalse((self.t.root/'search-old').exists())

    def test_history_search_cancel_preserves_draft_cursor_and_escape_sequences(self):
        self.command('printf older > search-history')
        self.t.send(b"printf 'ac'\x02\x02\x12history");self.t.expect(rb'find> history \| printf older')
        self.t.send(b'\x1b[D\x07');self.t.expect(rb"\r\x1b\[Kaetheri> printf 'ac'")
        self.command(b'b\x05 > cancelled-search');self.file('cancelled-search',b'abc')
        self.t.send(b'printf live\x12history');self.t.expect(rb'find> history \| printf older')
        self.t.send(b'\x1b');self.t.expect(rb'\r\x1b\[Kaetheri> printf live')
        self.command(b' > escaped-search');self.file('escaped-search',b'live')
        self.command("printf 'j́' > unicode-history")
        self.t.send(b'\x12'+'j́'.encode());self.t.expect(b'find> '+'j́'.encode()+rb" \| printf '")
        self.t.send(b'\x7f');self.t.expect(rb"find>  \| printf '")
        self.t.send(b'\x07');self.t.expect(PROMPT)

    def test_failed_or_oversize_search_never_accepts_an_old_candidate(self):
        self.command('touch stale-search');(self.t.root/'stale-search').unlink()
        self.t.send(b'printf untouched\x12absent-token');self.t.expect(rb'find> absent-token \| \(no match\)')
        for _ in range(2):
            self.t.send(b'\n');self.t.expect(rb'find> absent-token \| \(no match\)')
        self.assertFalse((self.t.root/'stale-search').exists())
        self.t.send(b'\x07');self.t.expect(rb'\r\x1b\[Kaetheri> printf untouched')
        self.command(b' > restored-query');self.file('restored-query',b'untouched')
        self.t.send(b'touch oversized-search\x12'+b'q'*1025+b'\n');self.t.expect(rb'input rejected; Ctrl\+U or Ctrl\+C')
        self.assertFalse((self.t.root/'oversized-search').exists())
        self.t.send(b'\x15');self.t.expect(PROMPT)
        self.command(b'\x1f\x1eprintf fresh > search-reset');self.file('search-reset',b'fresh')

    def test_grapheme_units_keep_actual_utf8_command_bytes(self):
        self.command("printf 'Aäj́👩‍💻🇩🇪Z'".encode()+b'\x02'+b'\x7f'*4+b'!\x05 > unicode-proof')
        self.file('unicode-proof','Aä!'.encode())
        self.command("printf 'äj́'".encode()+b'\x01'+b'\x06'*8+b'\x1b[3~'+b'\x05 > forward-proof')
        self.file('forward-proof','j́'.encode())

    def test_history_restores_live_draft_and_does_not_record_private_prefix(self):
        self.command("printf history")
        self.command(" printf private")
        self.command(b'\x1b[A\x05 > history-proof')
        self.file('history-proof',b'history')
        self.command(b'printf draft\x1b[A\x1b[B > draft-proof')
        self.file('draft-proof',b'draft')

    def test_signal_cancel_and_invalid_input_never_execute_a_prefix(self):
        self.t.send(b'touch cancelled');self.t.send(b'\x03');self.t.expect(PROMPT)
        self.command("printf '%s' $? > status-proof");self.file('status-proof',b'130')
        self.assertFalse((self.t.root/'cancelled').exists())
        for bad in (b'\xc0\x80',b'\xe2\x82\n',b'\x1b[999z'):
            self.t.send(b'touch invalid-prefix'+bad+b'\n')
            self.t.expect(rb'input rejected; Ctrl\+U or Ctrl\+C')
            self.assertFalse((self.t.root/'invalid-prefix').exists())
            self.t.send(b'\x15');self.t.expect(PROMPT)
        self.t.send(b'touch partial-escape\x1b[');time.sleep(.18);self.t.send(b'\n')
        self.t.expect(rb'input rejected; Ctrl\+U or Ctrl\+C')
        self.assertFalse((self.t.root/'partial-escape').exists())
        self.t.send(b'\x15');self.t.expect(PROMPT)
        self.command('printf fresh > fresh-proof');self.file('fresh-proof',b'fresh')

    def test_oversize_input_has_no_kernel_saturation_prefix_execution(self):
        self.t.send(b'touch saturated '+b'a'*16400+b'\n')
        self.t.expect(rb'input rejected; Ctrl\+U or Ctrl\+C',timeout=10)
        self.assertFalse((self.t.root/'saturated').exists())
        self.t.send(b'\x15');self.t.expect(PROMPT)
        self.command('printf recovered > after-limit');self.file('after-limit',b'recovered')

    def test_foreground_sees_original_modes_flags_and_pending_program_input(self):
        helper="""import os,termios,json,fcntl
from pathlib import Path
m=termios.tcgetattr(0)
Path('child-context.json').write_text(json.dumps({'canonical':bool(m[3]&termios.ICANON),'echo':bool(m[3]&termios.ECHO),'nonblock':bool(fcntl.fcntl(0,fcntl.F_GETFL)&os.O_NONBLOCK),'foreground':os.tcgetpgrp(0)==os.getpgrp()}))
Path('child-input').write_bytes(os.read(0,128))
"""
        (self.t.root/'helper.py').write_text(helper)
        self.t.send(b'python3 helper.py\nactual program input\n');self.t.expect(PROMPT)
        self.file('child-input',b'actual program input\n')
        context=json.loads((self.t.root/'child-context.json').read_text())
        self.assertEqual(context,dict(canonical=True,echo=True,nonblock=False,foreground=True))
        self.assertEqual(termios.tcgetattr(self.t.master)[3]&(termios.ICANON|termios.ECHO),0)

    def test_resize_escape_timeout_and_stopped_job_restore(self):
        termios.tcsetwinsize(self.t.master,(20,32));self.t.expect(PROMPT);self.t.send(b'printf resize')
        self.t.send(b'\x1b');time.sleep(.18);self.t.send(b' > resize-proof\n');self.t.expect(PROMPT)
        self.file('resize-proof',b'resize')
        (self.t.root/'stop.py').write_text("import os,signal,termios\nfrom pathlib import Path\nm=termios.tcgetattr(0);m[3]&=~termios.ECHO;termios.tcsetattr(0,termios.TCSANOW,m)\nos.kill(os.getpid(),signal.SIGTSTP)\nassert not termios.tcgetattr(0)[3]&termios.ECHO\nPath('resumed').write_text('yes')\n")
        self.command('python3 stop.py')
        self.assertEqual(termios.tcgetattr(self.t.master)[3]&(termios.ICANON|termios.ECHO),0)
        self.command('fg');self.file('resumed',b'yes')
        self.command('printf restored > restored-proof');self.file('restored-proof',b'restored')

    def test_exit_restores_canonical_terminal_without_a_history_file(self):
        self.t.send(b'exit 0\n');self.t.wait(0)
        self.assertEqual(termios.tcgetattr(self.t.master)[3]&(termios.ICANON|termios.ECHO),termios.ICANON|termios.ECHO)
        self.assertEqual(list(self.t.root.iterdir()),[])

    def test_long_cluster_viewport_never_places_cursor_outside_current_width(self):
        self.t.send(("printf '"+'ᄀ'*30+'a'*20).encode()+b'\x00\n')
        self.t.expect(rb'input rejected; Ctrl\+U or Ctrl\+C')
        position=int(self.t.expect(rb'\r\x1b\[([0-9]+)C').group(1))
        self.assertLessEqual(position,78,'own viewport cursor must remain within the actual 80-column terminal')
        self.t.send(b'\x15');self.t.expect(PROMPT)
        self.command('printf safe > viewport-proof');self.file('viewport-proof',b'safe')


if __name__=='__main__':
    print('Own-editor native evidence:',native.ROOT,flush=True)
    (native.ROOT/'editor-inputs.json').write_text(json.dumps({'shell':native.SHELL,'sha256':hashlib.sha256(Path(native.SHELL).read_bytes()).hexdigest(),
        'allocation_producer':ALLOCATION,'allocation_sha256':hashlib.sha256(Path(ALLOCATION).read_bytes()).hexdigest()},indent=2)+'\n')
    unittest.main()
