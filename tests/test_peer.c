#include "linux/peer.h"
#include "linux/exec_channel.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static unsigned int credential_source;
int __real_sd_bus_query_sender_creds(sd_bus_message *, uint64_t, sd_bus_creds **);
int __wrap_sd_bus_query_sender_creds(sd_bus_message *message, uint64_t mask, sd_bus_creds **output)
{
    if (credential_source == 0) return __real_sd_bus_query_sender_creds(message, mask, output);
    if (credential_source == 2) mask = SD_BUS_CREDS_EUID;
    /* Local /proc credentials must not pass as bus-authenticated observations. */
    int result = sd_bus_creds_new_from_pid(output, getpid(), mask);
    CHECK(result == 0);
    if (credential_source == 1) {
        CHECK((sd_bus_creds_get_mask(*output) & mask) == mask);
        CHECK((sd_bus_creds_get_augmented_mask(*output) & mask) != 0);
    } else CHECK((sd_bus_creds_get_mask(*output) & SD_BUS_CREDS_PIDFD) == 0);
    return result;
}

struct server {
    pid_t child;
    unsigned int calls;
    char label[512];
    struct ae_peer retained;
    struct ae_exec_channel *error_channel;
};

static int descriptor_count(void)
{
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory != NULL);
    int count = 0;
    while (readdir(directory) != NULL) ++count;
    CHECK(closedir(directory) == 0);
    return count;
}

static int check_call(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    (void)error;
    struct server *state = userdata;
    uint32_t mode = 0;
    CHECK(sd_bus_message_read(message, "u", &mode) > 0 && mode <= 2);
    CHECK(mode == state->calls);
    credential_source = mode;
    struct ae_peer peer = AE_PEER_INIT;
    struct ae_peer sentinel = peer;
    int result = ae_peer_capture(message, &peer);
    if (mode == 0) {
        if (result != 0) fprintf(stderr, "capture: %s\n", strerror(-result));
        CHECK(result == 0);
        CHECK(peer.connection_pid == state->child && peer.connection_uid == geteuid());
        CHECK(strcmp(peer.unique_name, sd_bus_message_get_sender(message)) == 0);
        CHECK(strcmp(peer.connection_label, state->label) == 0);
        sd_id128_t expected;
        CHECK(sd_bus_get_bus_id(sd_bus_message_get_bus(message), &expected) == 0);
        CHECK(memcmp(peer.bus_id, expected.bytes, sizeof peer.bus_id) == 0);
        CHECK((fcntl(peer.pidfd, F_GETFD) & FD_CLOEXEC) != 0);
        CHECK(ae_peer_alive(&peer) == 0);
        state->retained = peer;
        struct ae_peer occupied=peer;
        CHECK(ae_peer_capture(message,&occupied)==-EINVAL && memcmp(&occupied,&peer,sizeof peer)==0);
        /* Repeated real credential queries must release every transient FD. */
        int baseline = descriptor_count();
        for (unsigned int n = 0; n < 128; ++n) {
            struct ae_peer another = AE_PEER_INIT;
            CHECK(ae_peer_capture(message, &another) == 0);
            CHECK(another.connection_pid == state->child);
            ae_peer_clear(&another);
        }
        CHECK(descriptor_count() == baseline);
    } else {
        CHECK(result == -ENODATA);
        CHECK(memcmp(&peer, &sentinel, sizeof peer) == 0);
    }
    credential_source = 0;
    ++state->calls;
    return sd_bus_reply_method_return(message, "i", result);
}
static int issue_writer(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    (void)error;
    struct server *s=userdata; struct ae_peer current=AE_PEER_INIT;
    CHECK(s->calls==3 && s->error_channel==NULL && sd_bus_message_at_end(message,1)==1);
    CHECK(ae_peer_capture(message,&current)==0 && current.connection_pid==s->child && current.connection_uid==geteuid());
    CHECK(strcmp(current.unique_name,s->retained.unique_name)==0 && strcmp(current.connection_label,s->label)==0);
    CHECK(ae_peer_alive(&s->retained)==0); ae_peer_clear(&current);
    int writer=-1; CHECK(ae_exec_channel_new(&s->error_channel,&writer)==0);
    /* The issuer keeps only its reader. sd-bus duplicates the outbound writer;
     * the local copy must close even if sending the reply fails. */
    int r=sd_bus_reply_method_return(message,"h",writer); CHECK(close(writer)==0);
    ++s->calls; return r;
}

static const sd_bus_vtable methods[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Check", "u", "i", check_call, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("IssueErrorWriter", "", "h", issue_writer, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static void child_client(int input)
{
    char destination[256];
    CHECK(read(input, destination, sizeof destination) == (ssize_t)sizeof destination);
    CHECK(close(input) == 0);
    sd_bus *bus = NULL;
    CHECK(sd_bus_open_user(&bus) >= 0);
    sd_bus_message *first_request=NULL;
    for (uint32_t mode = 0; mode < 3; ++mode) {
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = NULL;
        sd_bus_message *request=NULL;
        CHECK(sd_bus_message_new_method_call(bus,&request,destination,"/org/aetheri/TestPeer","org.aetheri.TestPeer1","Check")>=0);
        CHECK(sd_bus_message_append(request,"u",mode)>=0 && sd_bus_call(bus,request,2000000,&error,&reply)>=0);
        struct ae_peer remote=AE_PEER_INIT,sentinel=remote;
        CHECK(ae_peer_capture(reply,&remote)==-EPROTO);
        if (mode>0) CHECK(ae_peer_capture_reply(reply,first_request,&remote)==-EPROTO && memcmp(&remote,&sentinel,sizeof remote)==0);
        credential_source=mode;
        int captured=ae_peer_capture_reply(reply,request,&remote);
        CHECK(captured==(mode==0 ? 0 : -ENODATA)); credential_source=0;
        if (mode==0) {
            CHECK(remote.connection_pid==getppid() && remote.connection_uid==geteuid() && strcmp(remote.unique_name,destination)==0);
            int baseline=descriptor_count();
            for (unsigned int n=0;n<128;++n) {
                struct ae_peer another=AE_PEER_INIT; CHECK(ae_peer_capture_reply(reply,request,&another)==0);
                CHECK(another.connection_pid==remote.connection_pid && strcmp(another.connection_label,remote.connection_label)==0);
                ae_peer_clear(&another);
            }
            CHECK(descriptor_count()==baseline);
            struct ae_peer occupied=remote;
            CHECK(ae_peer_capture_reply(reply,request,&occupied)==-EINVAL && memcmp(&occupied,&remote,sizeof remote)==0);
            first_request=sd_bus_message_ref(request); ae_peer_clear(&remote);
        } else CHECK(memcmp(&remote,&sentinel,sizeof remote)==0);
        int32_t status = 1;
        CHECK(sd_bus_message_read(reply, "i", &status) > 0);
        CHECK(status == (mode == 0 ? 0 : -ENODATA));
        sd_bus_message_unref(reply);
        sd_bus_message_unref(request);
        sd_bus_error_free(&error);
    }
    sd_bus_message_unref(first_request);
    sd_bus_message *reply=NULL;
    CHECK(sd_bus_call_method(bus,destination,"/org/aetheri/TestPeer","org.aetheri.TestPeer1","IssueErrorWriter",NULL,&reply,"")>=0);
    int borrowed=-1;
    CHECK(sd_bus_message_read(reply,"h",&borrowed)>0 && sd_bus_message_at_end(reply,1)==1);
    CHECK(ae_exec_writer_check(borrowed,geteuid(),getegid())==0);
    int writer=fcntl(borrowed,F_DUPFD_CLOEXEC,3); CHECK(writer>=3); sd_bus_message_unref(reply);
    uint8_t record[16]={0}; memcpy(record,"AETHXER1",8); record[11]=AE_EXEC_UID; record[15]=EPERM;
    CHECK(write(writer,record,7)==7 && write(writer,record+7,9)==9 && close(writer)==0);
    sd_bus_flush_close_unref(bus);
    _exit(0);
}

int main(void)
{
    alarm(20);
    struct server state = {.retained = AE_PEER_INIT};
    FILE *label = fopen("/proc/self/attr/current", "r");
    CHECK(label != NULL && fgets(state.label, sizeof state.label, label) != NULL);
    CHECK(fclose(label) == 0);
    state.label[strcspn(state.label, "\n")] = '\0';
    int channel[2];
    CHECK(pipe2(channel, O_CLOEXEC) == 0);
    state.child = fork();
    CHECK(state.child >= 0);
    if (state.child == 0) {
        CHECK(close(channel[1]) == 0);
        child_client(channel[0]);
    }
    CHECK(close(channel[0]) == 0);
    sd_bus *bus = NULL;
    sd_bus_slot *slot = NULL;
    CHECK(sd_bus_open_user(&bus) >= 0);
    CHECK(sd_bus_add_object_vtable(bus, &slot, "/org/aetheri/TestPeer", "org.aetheri.TestPeer1", methods, &state) >= 0);
    const char *unique = NULL;
    CHECK(sd_bus_get_unique_name(bus, &unique) >= 0);
    char destination[256] = {0};
    CHECK(strlen(unique) < sizeof destination);
    memcpy(destination, unique, strlen(unique) + 1U);

    struct ae_peer untouched = AE_PEER_INIT;
    struct ae_peer sentinel = untouched;
    CHECK(ae_peer_capture(NULL, &untouched) == -EINVAL);
    CHECK(ae_peer_alive(&untouched) == -EINVAL);
    sd_bus_message *signal_message = NULL;
    CHECK(sd_bus_message_new_signal(bus, &signal_message, "/org/aetheri/TestPeer", "org.aetheri.TestPeer1", "Signal") >= 0);
    CHECK(ae_peer_capture(signal_message, &untouched) == -EPROTO);
    CHECK(memcmp(&untouched, &sentinel, sizeof untouched) == 0);
    sd_bus_message_unref(signal_message);

    CHECK(write(channel[1], destination, sizeof destination) == (ssize_t)sizeof destination);
    CHECK(close(channel[1]) == 0);
    while (state.calls < 4) {
        int result = sd_bus_process(bus, NULL);
        CHECK(result >= 0);
        if (result == 0) CHECK(sd_bus_wait(bus, UINT64_MAX) >= 0);
    }
    CHECK(sd_bus_flush(bus) >= 0);
    int status = 0;
    CHECK(waitpid(state.child, &status, 0) == state.child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    struct ae_exec_report report={0};
    struct timespec started,now; CHECK(clock_gettime(CLOCK_MONOTONIC,&started)==0);
    for (;;) {
        CHECK(ae_exec_channel_read(state.error_channel,&report)==0);
        if (report.state!=AE_EXEC_CHANNEL_PENDING) break;
        /* The child's exit does not synchronously retire every bus transport
         * writer copy. Wait for actual stream progress/EOF within one deadline. */
        CHECK(clock_gettime(CLOCK_MONOTONIC,&now)==0);
        int64_t elapsed=(int64_t)(now.tv_sec-started.tv_sec)*1000+(now.tv_nsec-started.tv_nsec)/1000000;
        CHECK(elapsed>=0 && elapsed<5000);
        struct pollfd item={.fd=ae_exec_channel_fd(state.error_channel),.events=POLLIN|POLLHUP|POLLERR};
        int ready=poll(&item,1,(int)(5000-elapsed));
        if (ready<0 && errno==EINTR) continue;
        CHECK(ready>0);
    }
    CHECK(report.state==AE_EXEC_CHANNEL_FAILURE && report.phase==AE_EXEC_UID && report.error==EPERM);
    CHECK(ae_exec_channel_close(&state.error_channel)==0);
    CHECK(ae_peer_alive(&state.retained) == -ESRCH);
    int closed = state.retained.pidfd;
    ae_peer_clear(&state.retained);
    CHECK(fcntl(closed, F_GETFD) == -1 && errno == EBADF);
    ae_peer_clear(&state.retained);
    sd_bus_slot_unref(slot);
    sd_bus_flush_close_unref(bus);
    puts("peer: actual sender UID/label/ProcessFD/bus binding, issuer-only error-reader with sd-bus writer reply, exit/provenance/FD checks passed");
    return 0;
}
