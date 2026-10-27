#include "core/fixture.h"
#include "core/digest.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "fixture line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/* Test-only batch input/hex dump. Runtime code does not read this archive format. */
static uint32_t word(FILE *input)
{
    uint8_t bytes[4];
    CHECK(fread(bytes, 1, sizeof bytes, input) == sizeof bytes);
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
}

static void hex(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    for (size_t n = 0; n < size; ++n) printf("%02x", (unsigned int)bytes[n]);
}

static void text(const char *key, struct ae_fixture_text value)
{
    printf("%s ", key); hex(value.data, value.size); putchar('\n');
}

static void dump(const struct ae_fixture *fixture)
{
    text("component", fixture->component);
    fputs("publisher ", stdout); hex(fixture->publisher, sizeof fixture->publisher); putchar('\n');
    text("version", fixture->version); text("target", fixture->target);
    printf("files %" PRIu32 "\n", fixture->file_count);
    for (uint32_t n = 0; n < fixture->file_count; ++n) {
        const struct ae_fixture_file *file = &fixture->files[n];
        fputs("f ", stdout); hex(file->path.data, file->path.size);
        printf(" %" PRIu32 " %" PRIu64 " ", file->mode, file->size);
        hex(file->sha256, sizeof file->sha256); putchar('\n');
    }
    printf("actions %" PRIu32 "\n", fixture->action_count);
    for (uint32_t n = 0; n < fixture->action_count; ++n) {
        const struct ae_fixture_action *action = &fixture->actions[n];
        fputs("a ", stdout); hex(action->id.data, action->id.size);
        printf(" %" PRIu32 " %" PRIu32 "\n", action->file_index, action->argument_count);
        for (uint32_t argument = 0; argument < action->argument_count; ++argument) text("arg", action->arguments[argument]);
    }
    printf("dependencies %" PRIu32 "\n", fixture->dependency_count);
    for (uint32_t n = 0; n < fixture->dependency_count; ++n) {
        fputs("d ", stdout); hex(fixture->dependencies[n], 32); putchar('\n');
    }
    puts("END");
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    FILE *input = fopen(argv[1], "rb");
    CHECK(input != NULL);
    uint32_t cases = word(input);
    CHECK(cases > 0 && cases <= 4096);
    uint8_t *bytes = malloc(AE_FIXTURE_MAX_BYTES + 1U);
    CHECK(bytes != NULL);
    struct ae_fixture fixture, unchanged;
    memset(&unchanged, 0xa5, sizeof unchanged);
    CHECK(ae_fixture_decode(NULL, 0, &fixture) == -EINVAL);
    CHECK(ae_fixture_decode(bytes, 0, NULL) == -EINVAL);
    for (uint32_t n = 0; n < cases; ++n) {
        uint32_t accepted = word(input), size = word(input);
        CHECK(accepted <= 1 && size <= AE_FIXTURE_MAX_BYTES + 1U);
        CHECK(fread(bytes, 1, size, input) == size);
        memcpy(&fixture, &unchanged, sizeof fixture);
        int result = ae_fixture_decode(bytes, size, &fixture);
        if (accepted) {
            CHECK(result == 0);
            dump(&fixture);
            uint8_t hash[32]; CHECK(ae_sha256_bytes(bytes,size,hash)==0);
            struct ae_fixture_command command,marker; memset(&marker,0xa5,sizeof marker);
            for (uint32_t action=0;action<fixture.action_count;++action) {
                const struct ae_fixture_action *declared=&fixture.actions[action];
                char id[65]={0}; memcpy(id,declared->id.data,declared->id.size);
                CHECK(ae_fixture_resolve_action(bytes,size,hash,id,&command)==0);
                const struct ae_fixture_file *file=&fixture.files[declared->file_index];
                CHECK(command.argc==declared->argument_count+1 && command.file_size==file->size && memcmp(command.sha256,file->sha256,32)==0);
                CHECK(strlen(command.executable)==file->path.size && memcmp(command.executable,file->path.data,file->path.size)==0 && strcmp(command.arguments[0],command.executable)==0);
                for (uint32_t arg=0;arg<declared->argument_count;++arg) CHECK(strlen(command.arguments[arg+1])==declared->arguments[arg].size && memcmp(command.arguments[arg+1],declared->arguments[arg].data,declared->arguments[arg].size)==0);
            }
            command=marker;
            CHECK(ae_fixture_resolve_action(bytes,size,hash,"action-not-declared-0123456789",&command)==-ENOENT && memcmp(&command,&marker,sizeof command)==0);
            CHECK(ae_fixture_resolve_action(bytes,size,hash,"run;echo",&command)==-EINVAL && memcmp(&command,&marker,sizeof command)==0);
            CHECK(ae_fixture_resolve_action(bytes,size,hash,"",&command)==-EINVAL && memcmp(&command,&marker,sizeof command)==0);
            hash[0]^=1;
            CHECK(ae_fixture_resolve_action(bytes,size,hash,"run",&command)==-EBADMSG && memcmp(&command,&marker,sizeof command)==0);
            if (n == 0) {
                for (size_t prefix = 0; prefix < size; ++prefix) {
                    memcpy(&fixture, &unchanged, sizeof fixture);
                    CHECK(ae_fixture_decode(bytes, prefix, &fixture) == -EBADMSG);
                    CHECK(memcmp(&fixture, &unchanged, sizeof fixture) == 0);
                }
            }
        } else {
            CHECK(result == (size > AE_FIXTURE_MAX_BYTES ? -E2BIG : -EBADMSG));
            CHECK(memcmp(&fixture, &unchanged, sizeof fixture) == 0);
            puts("REJECT");
        }
    }
    CHECK(fgetc(input) == EOF && !ferror(input));
    CHECK(fclose(input) == 0);
    free(bytes);
    return 0;
}
