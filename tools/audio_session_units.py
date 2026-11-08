"""Fixed opt-in user units for the isolated audio-session image.

No unit is enabled or installed on the build host. Session-target existence is
coordination, not authorization; production login/device mediation is separate.
"""

def units():
    environment='''Environment=PIPEWIRE_CONFIG_DIR=/etc/aetheri-audio
Environment=PIPEWIRE_MODULE_DIR=/usr/lib64/pipewire-0.3 SPA_PLUGIN_DIR=/usr/lib64/spa-0.2
Environment=PIPEWIRE_RUNTIME_DIR=%t/aetheri-audio PIPEWIRE_REMOTE=%t/aetheri-audio/pw-hda
Environment=XDG_RUNTIME_DIR=%t HOME=%t/aetheri-audio XDG_STATE_HOME=%t/aetheri-audio/state
Environment=ALSA_CONFIG_PATH=/usr/share/alsa/alsa.conf
Environment=WIREPLUMBER_CONFIG_DIR=/etc/aetheri-wireplumber WIREPLUMBER_DATA_DIR=/usr/share/wireplumber
Environment=WIREPLUMBER_MODULE_DIR=/usr/lib64/wireplumber-0.5 GIO_MODULE_DIR=/nonexistent GIO_USE_VFS=local
'''
    service='''[Service]
Type=exec
Restart=no
KillMode=control-group
TimeoutStopSec=10s
NoNewPrivileges=yes
AmbientCapabilities=
UMask=0077
LimitCORE=0
StandardOutput=journal
StandardError=journal
'''+environment
    return {
        'aetheri-audio.target':'''[Unit]
Description=Aetheri user audio
Requisite=aetheri-session.target
After=aetheri-session.target aetheri-pipewire.service aetheri-wireplumber.service
BindsTo=aetheri-pipewire.service aetheri-wireplumber.service
PartOf=aetheri-session.target
''',
        'aetheri-pipewire.service':'''[Unit]
Description=Aetheri audio graph
Requisite=aetheri-session.target
After=aetheri-session.target
PartOf=aetheri-audio.target
'''+service+'''RuntimeDirectory=aetheri-audio
RuntimeDirectoryMode=0700
ExecStart=/usr/bin/pipewire -c server.conf
''',
        'aetheri-wireplumber.service':'''[Unit]
Description=Aetheri audio device and routing policy
Requisite=aetheri-session.target
BindsTo=aetheri-pipewire.service
After=aetheri-session.target aetheri-pipewire.service
PartOf=aetheri-audio.target
'''+service+'''ExecStart=/usr/bin/wireplumber -p aetheri-hda-test
''',
    }
