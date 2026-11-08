/* Pure parser checks; the included privileged fixture entry is never called. */
#define AETHERI_AUDIO_POLICY
#define main ae_audio_owner_fixture_main
#include "../src/boot/audio.c"
#undef main
#include <assert.h>

int main(void) {
    const char *good="aetheri.audio.check:output_FL\n  |-> alsa_output.test:playback_FL\naetheri.audio.check:output_FR\n  |-> alsa_output.test:playback_FR\n";
    assert(automatic_links(good,"alsa_output.test"));
    assert(!automatic_links(good,"alsa_output.foreign"));
    assert(!automatic_links("aetheri.audio.check:output_FL\naetheri.audio.check:output_FR\n","alsa_output.test"));
    assert(!automatic_links("aetheri.audio.check:output_FL\n  |-> alsa_output.test:playback_FR\naetheri.audio.check:output_FR\n  |-> alsa_output.test:playback_FL\n","alsa_output.test"));
    assert(!automatic_links("aetheri.audio.check:output_FL\n  |-> alsa_output.test:playback_FL\n","alsa_output.test"));
    assert(!automatic_links("aetheri.audio.check:output_FL\n  |-> alsa_output.test:playback_FL\n  |-> alsa_output.test:playback_FL\naetheri.audio.check:output_FR\n  |-> alsa_output.test:playback_FR\n","alsa_output.test"));
    char value[128];
    assert(property(" * node.name = \"alsa_output.pci-0000_00_02.0.analog-stereo\"\n","node.name = \"",value,sizeof(value)));
    assert(!strcmp(value,"alsa_output.pci-0000_00_02.0.analog-stereo"));
    assert(!property("node.name = \"unterminated","node.name = \"",value,sizeof(value)));
    assert(!property("node.name = \"escape/path\"\n","node.name = \"",value,sizeof(value)));
    assert(!property("node.name = \"first\"\nnode.name = \"second\"\n","node.name = \"",value,sizeof(value)));
    assert(!property("node.name = \"long-name\"\n","node.name = \"",value,3));
    puts("audio device parsers: 12 controls passed");return 0;
}
