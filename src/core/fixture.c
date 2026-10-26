#include "core/fixture.h"
#include "core/digest.h"

#include <errno.h>
#include <string.h>

int ae_fixture_verify_manifest(const uint8_t *bytes, size_t size,
                               const uint8_t expected_artifact[32], struct ae_fixture *output)
{
    if (expected_artifact == NULL || output == NULL) return -EINVAL;
    struct ae_fixture fixture;
    int result = ae_fixture_decode(bytes, size, &fixture);
    if (result != 0) return result;
    uint8_t digest[AE_SHA256_BYTES];
    result = ae_sha256_bytes(bytes, size, digest);
    if (result != 0) return result;
    if (memcmp(digest, expected_artifact, sizeof digest) != 0) return -EBADMSG;
    *output = fixture;
    return 0;
}

int ae_fixture_verify_file(const struct ae_fixture_file *file, int fd)
{
    if (file == NULL) return -EINVAL;
    uint8_t digest[AE_SHA256_BYTES];
    int result = ae_sha256_fd(fd, file->size, digest);
    if (result != 0) return result;
    return memcmp(digest, file->sha256, sizeof digest) == 0 ? 0 : -EBADMSG;
}

struct reader { const uint8_t *bytes; size_t size, position; };

static const uint8_t *take(struct reader *reader, size_t count)
{
    if (count > reader->size - reader->position) return NULL;
    const uint8_t *value = reader->bytes + reader->position;
    reader->position += count;
    return value;
}

static int number(struct reader *reader, size_t count, uint64_t *output)
{
    const uint8_t *bytes = take(reader, count);
    if (bytes == NULL) return -EBADMSG;
    uint64_t value = 0;
    for (size_t n = 0; n < count; ++n) value = (value << 8) | bytes[n];
    *output = value;
    return 0;
}

static int count(struct reader *reader, uint32_t minimum, uint32_t maximum, uint32_t *output)
{
    uint64_t value;
    if (number(reader, 4, &value) != 0 || value < minimum || value > maximum) return -EBADMSG;
    *output = (uint32_t)value;
    return 0;
}

static int utf8(const uint8_t *bytes, size_t size)
{
    for (size_t n = 0; n < size;) {
        uint32_t value = bytes[n++], minimum = 0;
        size_t following = 0;
        if (value >= 0xc2 && value <= 0xdf) { minimum = 0x80; following = 1; value &= 0x1f; }
        else if (value >= 0xe0 && value <= 0xef) { minimum = 0x800; following = 2; value &= 0x0f; }
        else if (value >= 0xf0 && value <= 0xf4) { minimum = 0x10000; following = 3; value &= 7; }
        else if (value >= 0x80) return 0;
        if (following > size - n) return 0;
        for (size_t end = n + following; n < end; ++n) {
            if ((bytes[n] & 0xc0) != 0x80) return 0;
            value = (value << 6) | (bytes[n] & 0x3fU);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
            value < 32 || (value >= 127 && value <= 159)) return 0;
    }
    return 1;
}

static int text(struct reader *reader, uint32_t minimum, uint32_t maximum, struct ae_fixture_text *output)
{
    uint32_t size;
    if (count(reader, minimum, maximum, &size) != 0) return -EBADMSG;
    const uint8_t *bytes = take(reader, size);
    if (bytes == NULL || !utf8(bytes, size)) return -EBADMSG;
    output->data = (const char *)bytes;
    output->size = size;
    return 0;
}

static int token(struct reader *reader, uint32_t maximum, struct ae_fixture_text *output)
{
    if (text(reader, 1, maximum, output) != 0) return -EBADMSG;
    for (size_t n = 0; n < output->size; ++n) {
        unsigned char c = (unsigned char)output->data[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return -EBADMSG;
    }
    return 0;
}

static int path(struct reader *reader, struct ae_fixture_text *output)
{
    if (text(reader, 1, 256, output) != 0) return -EBADMSG;
    size_t start = 0;
    for (size_t n = 0; n <= output->size; ++n) {
        if (n == output->size || output->data[n] == '/') {
            size_t length = n - start;
            if (length == 0 || length > 64 ||
                (length == 1 && output->data[start] == '.') ||
                (length == 2 && memcmp(output->data + start, "..", 2) == 0)) return -EBADMSG;
            start = n + 1;
        } else {
            unsigned char c = (unsigned char)output->data[n];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-')) return -EBADMSG;
        }
    }
    return 0;
}

static int compare(struct ae_fixture_text left, struct ae_fixture_text right)
{
    size_t common = left.size < right.size ? left.size : right.size;
    int order = memcmp(left.data, right.data, common);
    return order != 0 ? order : (left.size > right.size) - (left.size < right.size);
}

static int files(struct reader *reader, struct ae_fixture *fixture)
{
    if (count(reader, 1, AE_FIXTURE_FILES, &fixture->file_count) != 0) return -EBADMSG;
    uint64_t total = 0;
    for (uint32_t n = 0; n < fixture->file_count; ++n) {
        struct ae_fixture_file *file = &fixture->files[n];
        uint64_t mode;
        if (path(reader, &file->path) != 0 || number(reader, 4, &mode) != 0 || (mode != 0444 && mode != 0555) ||
            number(reader, 8, &file->size) != 0 || file->size > 16U * 1024U * 1024U) return -EBADMSG;
        file->mode = (uint32_t)mode;
        total += file->size;
        if (total > 32U * 1024U * 1024U) return -EBADMSG;
        const uint8_t *digest = take(reader, sizeof file->sha256);
        if (digest == NULL) return -EBADMSG;
        memcpy(file->sha256, digest, sizeof file->sha256);
        if (n != 0 && compare(fixture->files[n - 1].path, file->path) >= 0) return -EBADMSG;
        for (uint32_t prior = 0; prior < n; ++prior) {
            struct ae_fixture_text ancestor = fixture->files[prior].path;
            if (ancestor.size < file->path.size && file->path.data[ancestor.size] == '/' &&
                memcmp(ancestor.data, file->path.data, ancestor.size) == 0) return -EBADMSG;
        }
    }
    return 0;
}

static int actions(struct reader *reader, struct ae_fixture *fixture)
{
    if (count(reader, 0, AE_FIXTURE_ACTIONS, &fixture->action_count) != 0) return -EBADMSG;
    for (uint32_t n = 0; n < fixture->action_count; ++n) {
        struct ae_fixture_action *action = &fixture->actions[n];
        struct ae_fixture_text executable;
        if (token(reader, 64, &action->id) != 0 || path(reader, &executable) != 0) return -EBADMSG;
        if (n != 0 && compare(fixture->actions[n - 1].id, action->id) >= 0) return -EBADMSG;
        uint32_t file = 0;
        while (file < fixture->file_count && compare(fixture->files[file].path, executable) != 0) ++file;
        if (file == fixture->file_count || fixture->files[file].mode != 0555) return -EBADMSG;
        action->file_index = file;
        if (count(reader, 0, AE_FIXTURE_ARGUMENTS, &action->argument_count) != 0) return -EBADMSG;
        for (uint32_t argument = 0; argument < action->argument_count; ++argument)
            if (text(reader, 0, 256, &action->arguments[argument]) != 0) return -EBADMSG;
    }
    return 0;
}

int ae_fixture_decode(const uint8_t *bytes, size_t size, struct ae_fixture *output)
{
    if (bytes == NULL || output == NULL) return -EINVAL;
    if (size > AE_FIXTURE_MAX_BYTES) return -E2BIG;
    if (size < 8 || memcmp(bytes, "AETHFIX1", 8) != 0) return -EBADMSG;
    struct reader reader = {bytes, size, 8};
    struct ae_fixture fixture = {0};
    if (token(&reader, 128, &fixture.component) != 0) return -EBADMSG;
    const uint8_t *publisher = take(&reader, sizeof fixture.publisher);
    if (publisher == NULL) return -EBADMSG;
    unsigned int nonzero = 0;
    for (size_t n = 0; n < sizeof fixture.publisher; ++n) nonzero |= publisher[n];
    if (nonzero == 0) return -EBADMSG;
    memcpy(fixture.publisher, publisher, sizeof fixture.publisher);
    if (text(&reader, 1, 128, &fixture.version) != 0 || text(&reader, 1, 128, &fixture.target) != 0 ||
        fixture.target.size != 16 || memcmp(fixture.target.data, "x86_64-linux-gnu", 16) != 0) return -EBADMSG;
    if (files(&reader, &fixture) != 0 || actions(&reader, &fixture) != 0 ||
        count(&reader, 0, AE_FIXTURE_DEPENDENCIES, &fixture.dependency_count) != 0) return -EBADMSG;
    for (uint32_t n = 0; n < fixture.dependency_count; ++n) {
        const uint8_t *dependency = take(&reader, 32);
        if (dependency == NULL || (n != 0 && memcmp(fixture.dependencies[n - 1], dependency, 32) >= 0)) return -EBADMSG;
        memcpy(fixture.dependencies[n], dependency, 32);
    }
    if (reader.position != size) return -EBADMSG;
    *output = fixture;
    return 0;
}
