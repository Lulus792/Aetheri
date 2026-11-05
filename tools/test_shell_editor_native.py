"""Real own-editor PTYs, complete command effects and original job-mode handoff."""
import hashlib
import json
import os
from pathlib import Path
import signal
import termios
import time
import unittest

import test_shell_interactive as native

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
    (native.ROOT/'editor-inputs.json').write_text(json.dumps({'shell':native.SHELL,'sha256':hashlib.sha256(Path(native.SHELL).read_bytes()).hexdigest()},indent=2)+'\n')
    unittest.main()
