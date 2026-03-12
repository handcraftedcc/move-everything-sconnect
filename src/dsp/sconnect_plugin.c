#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "plugin_api_v1.h"

#define RING_SECONDS        3
#define RING_SAMPLES        (MOVE_SAMPLE_RATE * 2 * RING_SECONDS)
#define AUDIO_IDLE_MS       3000
#define DEVICE_NAME_MAX     128
#define LOG_PATH            "/data/UserData/move-anything/cache/sconnect-runtime.log"
#define CACHE_DIR_DEFAULT   "/data/UserData/move-anything/cache/sconnect"
#define NOWPLAYING_PATH     "/data/UserData/move-anything/cache/sconnect-nowplaying.env"
#define CONTROL_AUTH_PATH   "/data/UserData/move-anything/cache/sconnect-control-auth.env"
#define QUALITY_PREF_PATH   "/data/UserData/move-anything/cache/sconnect-quality.env"
#define LOG_POLL_INTERVAL_MS 250
#define STATE_POLL_INTERVAL_MS 250
#define CONTROL_MIN_INTERVAL_MS 900
#define CONTROL_429_BACKOFF_MS 10000
#define SESSION_RECOVERY_COOLDOWN_MS 15000
#define TOKEN_MAX           2048
#define TRACK_TEXT_MAX      256

static const host_api_v1_t *g_host = NULL;
static int g_instance_counter = 0;

typedef struct {
    char module_dir[512];
    char fifo_path[512];
    char device_name[DEVICE_NAME_MAX];
    char error_msg[256];
    char cache_dir[512];
    char supervisor_state[32];
    char access_token[TOKEN_MAX];
    char control_device_id[128];
    char track_name[TRACK_TEXT_MAX];
    char track_artist[TRACK_TEXT_MAX];
    char track_album[TRACK_TEXT_MAX];
    char track_uri[TRACK_TEXT_MAX];
    char playback_event[64];
    int slot;
    int bitrate_kbps;

    int fifo_fd;
    pid_t daemon_pid;
    bool daemon_running;
    bool controls_enabled;

    int16_t ring[RING_SAMPLES];
    size_t write_pos;
    uint64_t write_abs;
    uint64_t play_abs;
    uint8_t pending_bytes[4];
    uint8_t pending_len;

    float gain;
    bool receiving_audio;
    uint64_t last_audio_ms;
    uint64_t last_control_request_ms;
    uint64_t control_backoff_until_ms;
    uint64_t log_offset;
    uint64_t last_log_poll_ms;
    uint64_t last_state_poll_ms;
    uint64_t last_session_recover_ms;
    bool pending_session_recover;
    char log_line_buf[8192];
    size_t log_line_len;
} sconnect_instance_t;

static void clear_ring(sconnect_instance_t *inst);
static int supervisor_restart(sconnect_instance_t *inst);

static void append_log(const char *msg) {
    FILE *fp;
    if (!msg || msg[0] == '\0') return;
    fp = fopen(LOG_PATH, "a");
    if (!fp) return;
    fprintf(fp, "%s\n", msg);
    fclose(fp);
}

static void ap_log(const char *msg) {
    append_log(msg);
    if (g_host && g_host->log) {
        char buf[384];
        snprintf(buf, sizeof(buf), "[sconnect] %s", msg);
        g_host->log(buf);
    }
}

static uint64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

static void set_supervisor_state(sconnect_instance_t *inst, const char *state) {
    char log_msg[256];
    if (!inst) return;
    if (!state || state[0] == '\0') state = "unknown";
    if (strcmp(inst->supervisor_state, state) == 0) return;

    snprintf(log_msg, sizeof(log_msg), "state: %s -> %s",
             inst->supervisor_state[0] ? inst->supervisor_state : "unset",
             state);
    ap_log(log_msg);
    snprintf(inst->supervisor_state, sizeof(inst->supervisor_state), "%s",
             state);
}

static void set_error(sconnect_instance_t *inst, const char *msg) {
    if (!inst) return;
    snprintf(inst->error_msg, sizeof(inst->error_msg), "%s", msg ? msg : "unknown error");
    set_supervisor_state(inst, "error");
    ap_log(inst->error_msg);
}

static void clear_error(sconnect_instance_t *inst) {
    if (!inst) return;
    inst->error_msg[0] = '\0';
}

static void safe_copy(char *dst, size_t dst_len, const char *src) {
    if (!dst || dst_len == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_len, "%s", src);
}

static void set_controls_enabled(sconnect_instance_t *inst, bool enabled) {
    if (!inst) return;
    inst->controls_enabled = enabled;
}

static int normalize_quality(int q) {
    if (q == 96 || q == 160 || q == 320) return q;
    return 320;
}

static void save_quality_pref(const sconnect_instance_t *inst) {
    FILE *fp;
    if (!inst) return;
    fp = fopen(QUALITY_PREF_PATH, "w");
    if (!fp) return;
    fprintf(fp, "%d\n", normalize_quality(inst->bitrate_kbps));
    fclose(fp);
}

static void load_quality_pref(sconnect_instance_t *inst) {
    FILE *fp;
    int q = 0;

    if (!inst) return;
    fp = fopen(QUALITY_PREF_PATH, "r");
    if (!fp) return;

    if (fscanf(fp, "%d", &q) == 1) {
        inst->bitrate_kbps = normalize_quality(q);
    }
    fclose(fp);
}

static void save_control_auth(const sconnect_instance_t *inst) {
    FILE *fp;
    if (!inst) return;
    if (inst->access_token[0] == '\0') return;

    fp = fopen(CONTROL_AUTH_PATH, "w");
    if (!fp) return;
    fprintf(fp, "token=%s\n", inst->access_token);
    fprintf(fp, "device_id=%s\n", inst->control_device_id);
    fclose(fp);
}

static void load_control_auth(sconnect_instance_t *inst) {
    FILE *fp;
    char line[2300];

    if (!inst) return;
    fp = fopen(CONTROL_AUTH_PATH, "r");
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        char *value = NULL;
        size_t len;
        if (!eq) continue;
        *eq = '\0';
        value = eq + 1;

        len = strlen(value);
        while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
            value[--len] = '\0';
        }

        if (strcmp(line, "token") == 0) {
            safe_copy(inst->access_token, sizeof(inst->access_token), value);
        } else if (strcmp(line, "device_id") == 0) {
            safe_copy(inst->control_device_id, sizeof(inst->control_device_id), value);
        }
    }

    fclose(fp);
    if (inst->access_token[0] != '\0') {
        set_controls_enabled(inst, true);
    }
}

static int parse_quoted_value(const char *line, const char *prefix, char *out, size_t out_len) {
    const char *start;
    const char *end;
    size_t n;

    if (!line || !prefix || !out || out_len == 0) return -1;

    start = strstr(line, prefix);
    if (!start) return -1;
    start += strlen(prefix);
    end = strchr(start, '"');
    if (!end) return -1;

    n = (size_t)(end - start);
    if (n >= out_len) n = out_len - 1;
    memcpy(out, start, n);
    out[n] = '\0';
    return 0;
}

static bool credentials_exist(const sconnect_instance_t *inst) {
    char creds_path[1024];
    struct stat st;

    if (!inst) return false;
    snprintf(creds_path, sizeof(creds_path), "%s/credentials.json", inst->cache_dir);
    return stat(creds_path, &st) == 0;
}

static void supervisor_init_log_offset(sconnect_instance_t *inst) {
    struct stat st;
    if (!inst) return;

    inst->log_offset = 0;
    inst->log_line_len = 0;
    inst->log_line_buf[0] = '\0';
    inst->last_log_poll_ms = 0;

    if (stat(LOG_PATH, &st) == 0 && st.st_size > 0) {
        inst->log_offset = (uint64_t)st.st_size;
    }
}

static void supervisor_parse_token_line(sconnect_instance_t *inst, const char *line) {
    char token[TOKEN_MAX];
    if (!inst || !line) return;

    if (parse_quoted_value(line, "access_token: \"", token, sizeof(token)) == 0) {
        safe_copy(inst->access_token, sizeof(inst->access_token), token);
        set_controls_enabled(inst, true);
        save_control_auth(inst);
    }
}

static void supervisor_parse_device_id_line(sconnect_instance_t *inst, const char *line) {
    char device_id[128];
    if (!inst || !line) return;

    if (parse_quoted_value(line, "device_identifier\": String(\"", device_id, sizeof(device_id)) == 0 ||
        parse_quoted_value(line, "sent_by_device_id\": String(\"", device_id, sizeof(device_id)) == 0) {
        safe_copy(inst->control_device_id, sizeof(inst->control_device_id), device_id);
        if (inst->access_token[0] != '\0') {
            save_control_auth(inst);
        }
    }
}

static void supervisor_poll_nowplaying_state(sconnect_instance_t *inst) {
    uint64_t now;
    FILE *fp;
    char line[768];
    char prev_event[64];
    bool event_changed = false;

    if (!inst) return;

    now = now_ms();
    if (now > inst->last_state_poll_ms && (now - inst->last_state_poll_ms) < STATE_POLL_INTERVAL_MS) {
        return;
    }
    inst->last_state_poll_ms = now;

    fp = fopen(NOWPLAYING_PATH, "r");
    if (!fp) return;
    safe_copy(prev_event, sizeof(prev_event), inst->playback_event);

    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        char *value = NULL;
        if (!eq) continue;
        *eq = '\0';
        value = eq + 1;

        size_t len = strlen(value);
        while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
            value[--len] = '\0';
        }

        if (strcmp(line, "name") == 0) {
            safe_copy(inst->track_name, sizeof(inst->track_name), value);
        } else if (strcmp(line, "artists") == 0) {
            safe_copy(inst->track_artist, sizeof(inst->track_artist), value);
        } else if (strcmp(line, "album") == 0) {
            safe_copy(inst->track_album, sizeof(inst->track_album), value);
        } else if (strcmp(line, "uri") == 0) {
            safe_copy(inst->track_uri, sizeof(inst->track_uri), value);
        } else if (strcmp(line, "event") == 0) {
            safe_copy(inst->playback_event, sizeof(inst->playback_event), value);
            if (strcmp(prev_event, inst->playback_event) != 0) {
                event_changed = true;
            }
        }
    }

    fclose(fp);

    if (event_changed &&
        (strcmp(inst->playback_event, "paused") == 0 ||
         strcmp(inst->playback_event, "stopped") == 0)) {
        clear_ring(inst);
        inst->receiving_audio = false;
        if (inst->daemon_running && strcmp(inst->supervisor_state, "error") != 0) {
            set_supervisor_state(inst, "ready");
        }
    }
}

static void supervisor_process_log_line(sconnect_instance_t *inst, const char *line) {
    if (!inst || !line || line[0] == '\0') return;

    supervisor_parse_token_line(inst, line);
    supervisor_parse_device_id_line(inst, line);

    if (strstr(line, "SESSION_DELETED") || strstr(line, "DEVICES_DISAPPEARED")) {
        bool had_recent_playback =
            inst->receiving_audio ||
            strcmp(inst->supervisor_state, "playing") == 0 ||
            strcmp(inst->playback_event, "playing") == 0;
        if (inst->daemon_running && had_recent_playback) {
            inst->pending_session_recover = true;
            ap_log("spotify session invalidated; scheduling reconnect");
        }
    }

    if (strstr(line, "Authenticated") || strstr(line, "Login successful") ||
        strstr(line, "Using cached credentials")) {
        if (strcmp(inst->supervisor_state, "playing") != 0) {
            set_supervisor_state(inst, "ready");
        }
        return;
    }

    if (strstr(line, "authenticat") || strstr(line, "Authenticating")) {
        if (strcmp(inst->supervisor_state, "playing") != 0) {
            set_supervisor_state(inst, "authenticating");
        }
        return;
    }

    if (strstr(line, "PlayerEvent::Playing")) {
        set_supervisor_state(inst, "playing");
        return;
    }

    if (strstr(line, "PlayerEvent::Paused") || strstr(line, "PlayerEvent::Stopped")) {
        if (strcmp(inst->supervisor_state, "error") != 0) {
            set_supervisor_state(inst, "ready");
        }
        return;
    }

    if (strstr(line, "authentication failed") || strstr(line, "Login failed") ||
        strstr(line, "Invalid username/password")) {
        set_error(inst, "spotify authentication failed");
    }
}

static void maybe_recover_session(sconnect_instance_t *inst) {
    uint64_t now;
    if (!inst || !inst->pending_session_recover) return;
    if (!inst->daemon_running) {
        inst->pending_session_recover = false;
        return;
    }

    now = now_ms();
    if (inst->last_session_recover_ms > 0 &&
        now > inst->last_session_recover_ms &&
        (now - inst->last_session_recover_ms) < SESSION_RECOVERY_COOLDOWN_MS) {
        return;
    }

    inst->pending_session_recover = false;
    inst->last_session_recover_ms = now;
    ap_log("attempting reconnect after spotify session invalidation");
    (void)supervisor_restart(inst);
}

static void supervisor_poll_runtime_log(sconnect_instance_t *inst) {
    uint8_t buf[256];
    uint64_t now;
    int fd;

    if (!inst || !inst->daemon_running) return;

    now = now_ms();
    if (now > inst->last_log_poll_ms && (now - inst->last_log_poll_ms) < LOG_POLL_INTERVAL_MS) {
        return;
    }
    inst->last_log_poll_ms = now;

    fd = open(LOG_PATH, O_RDONLY);
    if (fd < 0) return;

    if (lseek(fd, (off_t)inst->log_offset, SEEK_SET) < 0) {
        close(fd);
        return;
    }

    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        inst->log_offset += (uint64_t)n;

        for (ssize_t i = 0; i < n; i++) {
            char ch = (char)buf[i];
            if (ch == '\n' || ch == '\r') {
                if (inst->log_line_len > 0) {
                    inst->log_line_buf[inst->log_line_len] = '\0';
                    supervisor_process_log_line(inst, inst->log_line_buf);
                    inst->log_line_len = 0;
                }
                continue;
            }

            if (inst->log_line_len + 1 < sizeof(inst->log_line_buf)) {
                inst->log_line_buf[inst->log_line_len++] = ch;
            }
        }
    }

    close(fd);
}

/* --- Ring buffer --- */

static size_t ring_available(const sconnect_instance_t *inst) {
    uint64_t avail;
    if (!inst) return 0;
    if (inst->write_abs <= inst->play_abs) return 0;
    avail = inst->write_abs - inst->play_abs;
    if (avail > (uint64_t)RING_SAMPLES) avail = (uint64_t)RING_SAMPLES;
    return (size_t)avail;
}

static void ring_push(sconnect_instance_t *inst, const int16_t *samples, size_t n) {
    size_t i;
    uint64_t oldest;
    for (i = 0; i < n; i++) {
        inst->ring[inst->write_pos] = samples[i];
        inst->write_pos = (inst->write_pos + 1) % RING_SAMPLES;
        inst->write_abs++;
    }

    oldest = 0;
    if (inst->write_abs > (uint64_t)RING_SAMPLES) {
        oldest = inst->write_abs - (uint64_t)RING_SAMPLES;
    }
    if (inst->play_abs < oldest) {
        inst->play_abs = oldest;
    }
}

static size_t ring_pop(sconnect_instance_t *inst, int16_t *out, size_t n) {
    size_t got;
    size_t i;
    uint64_t abs_pos;

    if (!inst || !out || n == 0) return 0;

    got = ring_available(inst);
    if (got > n) got = n;
    abs_pos = inst->play_abs;

    for (i = 0; i < got; i++) {
        out[i] = inst->ring[(size_t)(abs_pos % (uint64_t)RING_SAMPLES)];
        abs_pos++;
    }

    inst->play_abs = abs_pos;
    return got;
}

static void clear_ring(sconnect_instance_t *inst) {
    if (!inst) return;
    inst->write_pos = 0;
    inst->write_abs = 0;
    inst->play_abs = 0;
    inst->pending_len = 0;
    memset(inst->pending_bytes, 0, sizeof(inst->pending_bytes));
}

/* --- librespot supervisor --- */

static void supervisor_stop(sconnect_instance_t *inst) {
    int status;
    pid_t rc;

    if (!inst) return;

    if (inst->daemon_pid > 0) {
        set_supervisor_state(inst, "stopping");
        rc = waitpid(inst->daemon_pid, &status, WNOHANG);
        if (rc == 0) {
            (void)kill(inst->daemon_pid, SIGTERM);
            usleep(300000);
            rc = waitpid(inst->daemon_pid, &status, WNOHANG);
            if (rc == 0) {
                (void)kill(inst->daemon_pid, SIGKILL);
                (void)waitpid(inst->daemon_pid, &status, 0);
            }
        }
        inst->daemon_pid = -1;
    }

    inst->daemon_running = false;
    set_supervisor_state(inst, "stopped");
}

static int supervisor_start(sconnect_instance_t *inst) {
    pid_t pid;
    char log_msg[384];
    char librespot_path[1024];
    char event_hook_path[1024];
    char bitrate_str[16];
    bool has_event_hook = false;

    if (!inst) return -1;

    snprintf(librespot_path, sizeof(librespot_path),
             "%s/bin/librespot", inst->module_dir);
    snprintf(event_hook_path, sizeof(event_hook_path),
             "%s/bin/sconnect_event.sh", inst->module_dir);
    has_event_hook = access(event_hook_path, X_OK) == 0;
    snprintf(bitrate_str, sizeof(bitrate_str), "%d", inst->bitrate_kbps);

    if (access(librespot_path, X_OK) != 0) {
        set_error(inst, "librespot binary missing or not executable");
        return -1;
    }

    supervisor_stop(inst);
    set_supervisor_state(inst, "starting");

    pid = fork();
    if (pid < 0) {
        set_error(inst, "fork failed for librespot");
        return -1;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }

        int logfd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }

        unsetenv("LD_PRELOAD");

        if (has_event_hook) {
            execl(librespot_path, "librespot",
                  "--name", inst->device_name,
                  "--backend", "pipe",
                  "--device", inst->fifo_path,
                  "--cache", inst->cache_dir,
                  "--bitrate", bitrate_str,
                  "--onevent", event_hook_path,
                  "-v",
                  (char *)NULL);
        } else {
            execl(librespot_path, "librespot",
                  "--name", inst->device_name,
                  "--backend", "pipe",
                  "--device", inst->fifo_path,
                  "--cache", inst->cache_dir,
                  "--bitrate", bitrate_str,
                  "-v",
                  (char *)NULL);
        }
        _exit(127);
    }

    inst->daemon_pid = pid;
    inst->daemon_running = true;
    clear_error(inst);
    if (credentials_exist(inst)) {
        set_supervisor_state(inst, "authenticating");
    } else {
        set_supervisor_state(inst, "waiting_for_spotify");
    }

    snprintf(log_msg, sizeof(log_msg),
             "librespot started pid=%d name=%s fifo=%s bitrate=%d onevent=%s",
             (int)pid, inst->device_name, inst->fifo_path, inst->bitrate_kbps,
             has_event_hook ? "yes" : "no");
    ap_log(log_msg);

    return 0;
}

static int supervisor_restart(sconnect_instance_t *inst) {
    if (!inst) return -1;
    inst->pending_session_recover = false;
    clear_ring(inst);
    clear_error(inst);
    inst->receiving_audio = false;
    supervisor_init_log_offset(inst);
    return supervisor_start(inst);
}

static bool supervisor_is_running(const sconnect_instance_t *inst) {
    if (!inst) return false;
    return inst->daemon_running;
}

static pid_t supervisor_get_pid(const sconnect_instance_t *inst) {
    if (!inst) return -1;
    return inst->daemon_pid;
}

static const char* supervisor_get_state(const sconnect_instance_t *inst) {
    if (!inst) return "error";
    return inst->supervisor_state[0] ? inst->supervisor_state : "unknown";
}

static void supervisor_clear_credentials(sconnect_instance_t *inst) {
    char creds_path[1024];
    char cache_db_path[1024];

    if (!inst) return;

    snprintf(creds_path, sizeof(creds_path), "%s/credentials.json", inst->cache_dir);
    snprintf(cache_db_path, sizeof(cache_db_path), "%s/cache.db", inst->cache_dir);

    (void)unlink(creds_path);
    (void)unlink(cache_db_path);
    (void)unlink(NOWPLAYING_PATH);
    (void)unlink(CONTROL_AUTH_PATH);
    inst->access_token[0] = '\0';
    inst->control_device_id[0] = '\0';
    inst->track_name[0] = '\0';
    inst->track_artist[0] = '\0';
    inst->track_album[0] = '\0';
    inst->track_uri[0] = '\0';
    inst->playback_event[0] = '\0';
    inst->last_control_request_ms = 0;
    inst->control_backoff_until_ms = 0;
    inst->pending_session_recover = false;
    set_controls_enabled(inst, false);
    set_supervisor_state(inst, "waiting_for_spotify");

    ap_log("librespot credentials cleared (best effort)");
}

static void check_daemon_alive(sconnect_instance_t *inst) {
    int status;
    pid_t rc;

    if (!inst || inst->daemon_pid <= 0) return;

    rc = waitpid(inst->daemon_pid, &status, WNOHANG);
    if (rc == inst->daemon_pid) {
        inst->daemon_pid = -1;
        inst->daemon_running = false;
        set_error(inst, "librespot exited unexpectedly");
    }
}

static bool spotify_api_call(sconnect_instance_t *inst, const char *method, const char *path) {
    char url[512];
    char cmd[4096];
    char status_path[192];
    char err_path[192];
    char err_msg[160];
    uint64_t now;
    uint64_t backoff_ms = CONTROL_429_BACKOFF_MS;
    const char *post_flag;
    const char *override_flag;
    bool method_is_put;
    bool method_is_post;
    FILE *fp;
    int rc;
    int http_code = 0;
    int retry_after_sec = 0;

    if (!inst || !method || !path) return false;
    now = now_ms();

    if (inst->control_backoff_until_ms > now) {
        unsigned long long wait_ms =
            (unsigned long long)(inst->control_backoff_until_ms - now);
        snprintf(err_msg, sizeof(err_msg),
                 "spotify controls cooling down (%llums)", wait_ms);
        set_error(inst, err_msg);
        return false;
    }

    if (inst->last_control_request_ms > 0 &&
        now > inst->last_control_request_ms &&
        (now - inst->last_control_request_ms) < CONTROL_MIN_INTERVAL_MS) {
        set_error(inst, "spotify controls cooling down");
        return false;
    }
    inst->last_control_request_ms = now;

    if (inst->access_token[0] == '\0') {
        set_controls_enabled(inst, false);
        set_error(inst, "controls unavailable: no spotify token");
        return false;
    }

    if (inst->control_device_id[0] != '\0') {
        snprintf(url, sizeof(url), "https://api.spotify.com%s?device_id=%s",
                 path, inst->control_device_id);
    } else {
        snprintf(url, sizeof(url), "https://api.spotify.com%s", path);
    }

    method_is_put = strcmp(method, "PUT") == 0;
    method_is_post = strcmp(method, "POST") == 0 || method_is_put;
    post_flag = method_is_post ? "--post-data='' " : "";
    override_flag = method_is_put ? "--header=\"X-HTTP-Method-Override: PUT\" " : "";

    snprintf(status_path, sizeof(status_path), "/tmp/sconnect-http-%d.status", inst->slot);
    snprintf(err_path, sizeof(err_path), "/tmp/sconnect-http-%d.err", inst->slot);
    (void)unlink(status_path);
    (void)unlink(err_path);

    snprintf(
        cmd, sizeof(cmd),
        "if command -v curl >/dev/null 2>&1; then "
        "curl -sS -o /dev/null -w '%%{http_code}' -X %s --max-time 6 "
        "-H \"Authorization: Bearer %s\" -H \"Content-Type: application/json\" "
        "\"%s\" >\"%s\" 2>\"%s\"; "
        "else "
        "wget -S -O /dev/null -T 6 "
        "--header=\"Authorization: Bearer %s\" --header=\"Content-Type: application/json\" "
        "%s%s\"%s\" > /dev/null 2>\"%s\"; "
        "fi",
        method, inst->access_token, url, status_path, err_path,
        inst->access_token, post_flag, override_flag, url, err_path
    );

    rc = system(cmd);

    fp = fopen(status_path, "r");
    if (fp) {
        if (fscanf(fp, "%d", &http_code) != 1) {
            http_code = 0;
        }
        fclose(fp);
    }

    if (http_code <= 0) {
        char line[256];
        fp = fopen(err_path, "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                char *p = strstr(line, "HTTP/");
                char *colon = NULL;
                if (strstr(line, "Retry-After") != NULL ||
                    strstr(line, "retry-after") != NULL) {
                    colon = strchr(line, ':');
                    if (colon) {
                        int candidate = atoi(colon + 1);
                        if (candidate > 0) retry_after_sec = candidate;
                    }
                }
                if (!p) continue;
                while (*p && *p != ' ') p++;
                if (*p == ' ') {
                    int code = atoi(p + 1);
                    if (code > 0) {
                        http_code = code;
                    }
                }
            }
            fclose(fp);
        }
    }

    if (rc != 0 || http_code < 200 || http_code >= 300) {
        bool disable_controls = false;
        if (http_code == 429) {
            if (retry_after_sec > 0) {
                backoff_ms = (uint64_t)retry_after_sec * 1000ULL;
            }
            inst->control_backoff_until_ms = now_ms() + backoff_ms;
            snprintf(err_msg, sizeof(err_msg),
                     "spotify controls rate-limited (HTTP 429, backoff %llums)",
                     (unsigned long long)backoff_ms);
        } else if (http_code == 401 || http_code == 403) {
            disable_controls = true;
            snprintf(err_msg, sizeof(err_msg),
                     "spotify controls unauthorized (HTTP %d)", http_code);
        } else if (http_code > 0) {
            snprintf(err_msg, sizeof(err_msg), "spotify control request failed (HTTP %d)", http_code);
        } else {
            snprintf(err_msg, sizeof(err_msg), "spotify control request failed");
        }
        if (disable_controls) {
            set_controls_enabled(inst, false);
        }
        set_error(inst, err_msg);
        return false;
    }

    inst->control_backoff_until_ms = 0;
    clear_error(inst);
    set_controls_enabled(inst, true);
    save_control_auth(inst);
    return true;
}

static void sconnect_play_pause(sconnect_instance_t *inst) {
    if (!inst) return;
    if (strcmp(inst->playback_event, "playing") == 0) {
        (void)spotify_api_call(inst, "PUT", "/v1/me/player/pause");
    } else {
        (void)spotify_api_call(inst, "PUT", "/v1/me/player/play");
    }
}

/* --- FIFO management --- */

static int create_fifo(sconnect_instance_t *inst) {
    if (!inst) return -1;

    snprintf(inst->fifo_path, sizeof(inst->fifo_path),
             "/tmp/sconnect-audio-%d", inst->slot);

    (void)unlink(inst->fifo_path);

    if (mkfifo(inst->fifo_path, 0666) != 0) {
        set_error(inst, "mkfifo failed");
        return -1;
    }

    /* Keep one write reference open and read non-blocking. */
    inst->fifo_fd = open(inst->fifo_path, O_RDWR | O_NONBLOCK);
    if (inst->fifo_fd < 0) {
        set_error(inst, "failed to open audio FIFO");
        (void)unlink(inst->fifo_path);
        return -1;
    }

    return 0;
}

static void close_fifo(sconnect_instance_t *inst) {
    if (!inst) return;

    if (inst->fifo_fd >= 0) {
        close(inst->fifo_fd);
        inst->fifo_fd = -1;
    }

    if (inst->fifo_path[0] != '\0') {
        (void)unlink(inst->fifo_path);
        inst->fifo_path[0] = '\0';
    }
}

/* --- Pipe pump (reads FIFO into ring buffer) --- */

static void pump_pipe(sconnect_instance_t *inst) {
    uint8_t buf[4096];
    uint8_t merged[4100];
    int16_t samples[2048];

    if (!inst || inst->fifo_fd < 0) return;

    while (1) {
        if (ring_available(inst) + 2048 >= (size_t)RING_SAMPLES) {
            break;
        }

        ssize_t n = read(inst->fifo_fd, buf, sizeof(buf));
        if (n > 0) {
            size_t merged_bytes = inst->pending_len;
            size_t aligned_bytes;
            size_t remainder;
            size_t sample_count;

            if (inst->pending_len > 0) {
                memcpy(merged, inst->pending_bytes, inst->pending_len);
            }

            memcpy(merged + merged_bytes, buf, (size_t)n);
            merged_bytes += (size_t)n;

            aligned_bytes = merged_bytes & ~((size_t)3U);
            remainder = merged_bytes - aligned_bytes;
            if (remainder > 0) {
                memcpy(inst->pending_bytes, merged + aligned_bytes, remainder);
            }
            inst->pending_len = (uint8_t)remainder;

            sample_count = aligned_bytes / sizeof(int16_t);
            if (sample_count > 0) {
                memcpy(samples, merged, sample_count * sizeof(int16_t));
                ring_push(inst, samples, sample_count);
            }

            inst->last_audio_ms = now_ms();
            inst->receiving_audio = true;
            set_supervisor_state(inst, "playing");

            if ((size_t)n < sizeof(buf)) {
                break;
            }
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            break;
        }

        break;
    }

    if (inst->receiving_audio && inst->last_audio_ms > 0) {
        uint64_t now = now_ms();
        if (now > inst->last_audio_ms && (now - inst->last_audio_ms) > AUDIO_IDLE_MS) {
            inst->receiving_audio = false;
            if (inst->daemon_running && strcmp(inst->supervisor_state, "error") != 0) {
                set_supervisor_state(inst, "ready");
            }
        }
    }
}

/* --- Plugin API v2 --- */

static void* v2_create_instance(const char *module_dir, const char *json_defaults) {
    sconnect_instance_t *inst;

    inst = calloc(1, sizeof(*inst));
    if (!inst) return NULL;

    inst->slot = ++g_instance_counter;

    snprintf(inst->module_dir, sizeof(inst->module_dir), "%s",
             module_dir ? module_dir : ".");
    snprintf(inst->device_name, sizeof(inst->device_name),
             "Move Everything");
    snprintf(inst->cache_dir, sizeof(inst->cache_dir), "%s", CACHE_DIR_DEFAULT);

    inst->gain = 1.0f;
    inst->bitrate_kbps = 320;
    inst->fifo_fd = -1;
    inst->daemon_pid = -1;
    inst->controls_enabled = false;
    inst->access_token[0] = '\0';
    inst->control_device_id[0] = '\0';
    inst->track_name[0] = '\0';
    inst->track_artist[0] = '\0';
    inst->track_album[0] = '\0';
    inst->track_uri[0] = '\0';
    inst->playback_event[0] = '\0';
    inst->last_control_request_ms = 0;
    inst->control_backoff_until_ms = 0;
    inst->last_session_recover_ms = 0;
    inst->pending_session_recover = false;
    set_supervisor_state(inst, "uninitialized");

    (void)json_defaults;
    (void)mkdir(inst->cache_dir, 0755);
    supervisor_init_log_offset(inst);
    load_quality_pref(inst);
    load_control_auth(inst);

    if (create_fifo(inst) != 0) {
        free(inst);
        return NULL;
    }

    if (supervisor_start(inst) != 0) {
        close_fifo(inst);
        free(inst);
        return NULL;
    }

    ap_log("sconnect plugin instance created");
    return inst;
}

static void v2_destroy_instance(void *instance) {
    sconnect_instance_t *inst = (sconnect_instance_t *)instance;
    if (!inst) return;

    supervisor_stop(inst);
    close_fifo(inst);

    free(inst);
    if (g_instance_counter > 0) g_instance_counter--;
    ap_log("sconnect plugin instance destroyed");
}

static void v2_on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)instance;
    (void)msg;
    (void)len;
    (void)source;
}

static void v2_set_param(void *instance, const char *key, const char *val) {
    sconnect_instance_t *inst = (sconnect_instance_t *)instance;
    if (!inst || !key || !val) return;

    if (strcmp(key, "gain") == 0) {
        float g = (float)atof(val);
        if (g < 0.0f) g = 0.0f;
        if (g > 2.0f) g = 2.0f;
        inst->gain = g;
        return;
    }

    if (strcmp(key, "device_name") == 0) {
        char log_msg[256];
        if (val[0] == '\0') return;
        snprintf(inst->device_name, sizeof(inst->device_name), "%s", val);
        snprintf(log_msg, sizeof(log_msg), "device name changed to: %s", inst->device_name);
        ap_log(log_msg);
        (void)supervisor_restart(inst);
        return;
    }

    if (strcmp(key, "restart") == 0) {
        ap_log("manual restart requested");
        (void)supervisor_restart(inst);
        return;
    }

    if (strcmp(key, "enable_controls") == 0) {
        ap_log("enable controls requested");
        (void)supervisor_restart(inst);
        return;
    }

    if (strcmp(key, "play_pause") == 0) {
        ap_log("play/pause requested");
        sconnect_play_pause(inst);
        return;
    }

    if (strcmp(key, "next") == 0) {
        ap_log("next requested");
        (void)spotify_api_call(inst, "POST", "/v1/me/player/next");
        return;
    }

    if (strcmp(key, "previous") == 0) {
        ap_log("previous requested");
        (void)spotify_api_call(inst, "POST", "/v1/me/player/previous");
        return;
    }

    if (strcmp(key, "quality") == 0) {
        int q = normalize_quality(atoi(val));
        if (q != inst->bitrate_kbps) {
            char log_msg[128];
            inst->bitrate_kbps = q;
            save_quality_pref(inst);
            snprintf(log_msg, sizeof(log_msg), "quality changed to %d kbps", q);
            ap_log(log_msg);
            (void)supervisor_restart(inst);
        }
        return;
    }

    if (strcmp(key, "quality_low") == 0) {
        if (inst->bitrate_kbps != 96) {
            inst->bitrate_kbps = 96;
            save_quality_pref(inst);
            (void)supervisor_restart(inst);
        }
        return;
    }

    if (strcmp(key, "quality_medium") == 0) {
        if (inst->bitrate_kbps != 160) {
            inst->bitrate_kbps = 160;
            save_quality_pref(inst);
            (void)supervisor_restart(inst);
        }
        return;
    }

    if (strcmp(key, "quality_high") == 0) {
        if (inst->bitrate_kbps != 320) {
            inst->bitrate_kbps = 320;
            save_quality_pref(inst);
            (void)supervisor_restart(inst);
        }
        return;
    }

    if (strcmp(key, "reset_auth") == 0 ||
        strcmp(key, "reset_controls") == 0 ||
        strcmp(key, "reset_login") == 0) {
        ap_log("auth reset requested");
        supervisor_clear_credentials(inst);
        (void)supervisor_restart(inst);
        return;
    }
}

static int v2_get_param(void *instance, const char *key, char *buf, int buf_len) {
    sconnect_instance_t *inst = (sconnect_instance_t *)instance;
    const char *state;

    if (!key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "gain") == 0) {
        return snprintf(buf, (size_t)buf_len, "%.2f", inst ? inst->gain : 1.0f);
    }

    if (strcmp(key, "preset_name") == 0 || strcmp(key, "name") == 0) {
        return snprintf(buf, (size_t)buf_len, "SConnect");
    }

    if (strcmp(key, "device_name") == 0) {
        return snprintf(buf, (size_t)buf_len, "%s",
                        inst ? inst->device_name : "Move Everything");
    }

    if (strcmp(key, "backend_pid") == 0) {
        return snprintf(buf, (size_t)buf_len, "%d", (int)supervisor_get_pid(inst));
    }

    if (strcmp(key, "backend_state") == 0) {
        return snprintf(buf, (size_t)buf_len, "%s", supervisor_get_state(inst));
    }

    if (strcmp(key, "controls_enabled") == 0) {
        return snprintf(buf, (size_t)buf_len, "%s",
                        (inst && inst->controls_enabled) ? "1" : "0");
    }

    if (strcmp(key, "track_name") == 0) {
        return snprintf(buf, (size_t)buf_len, "%s",
                        inst ? inst->track_name : "");
    }

    if (strcmp(key, "track_artist") == 0) {
        return snprintf(buf, (size_t)buf_len, "%s",
                        inst ? inst->track_artist : "");
    }

    if (strcmp(key, "track_album") == 0) {
        return snprintf(buf, (size_t)buf_len, "%s",
                        inst ? inst->track_album : "");
    }

    if (strcmp(key, "playback_event") == 0) {
        return snprintf(buf, (size_t)buf_len, "%s",
                        inst ? inst->playback_event : "");
    }

    if (strcmp(key, "quality") == 0) {
        return snprintf(buf, (size_t)buf_len, "%d",
                        inst ? inst->bitrate_kbps : 320);
    }

    if (strcmp(key, "status") == 0) {
        if (!inst) return snprintf(buf, (size_t)buf_len, "error");
        if (inst->error_msg[0] != '\0') return snprintf(buf, (size_t)buf_len, "error");

        state = supervisor_get_state(inst);
        if (strcmp(state, "starting") == 0) return snprintf(buf, (size_t)buf_len, "starting");
        if (strcmp(state, "waiting_for_spotify") == 0) {
            return snprintf(buf, (size_t)buf_len, "waiting_for_spotify");
        }
        if (strcmp(state, "authenticating") == 0) {
            return snprintf(buf, (size_t)buf_len, "authenticating");
        }
        if (strcmp(state, "ready") == 0) return snprintf(buf, (size_t)buf_len, "ready");
        if (supervisor_is_running(inst) && inst->receiving_audio) {
            return snprintf(buf, (size_t)buf_len, "playing");
        }
        if (supervisor_is_running(inst)) return snprintf(buf, (size_t)buf_len, "ready");
        return snprintf(buf, (size_t)buf_len, "stopped");
    }

    return -1;
}

static int v2_get_error(void *instance, char *buf, int buf_len) {
    sconnect_instance_t *inst = (sconnect_instance_t *)instance;
    if (!inst || !inst->error_msg[0]) return 0;
    return snprintf(buf, (size_t)buf_len, "%s", inst->error_msg);
}

static void v2_render_block(void *instance, int16_t *out_interleaved_lr, int frames) {
    sconnect_instance_t *inst = (sconnect_instance_t *)instance;
    size_t needed;
    size_t got;
    size_t i;

    if (!out_interleaved_lr || frames <= 0) return;

    needed = (size_t)frames * 2;
    memset(out_interleaved_lr, 0, needed * sizeof(int16_t));

    if (!inst) return;

    check_daemon_alive(inst);
    supervisor_poll_runtime_log(inst);
    maybe_recover_session(inst);
    supervisor_poll_nowplaying_state(inst);
    pump_pipe(inst);

    got = ring_pop(inst, out_interleaved_lr, needed);

    if (inst->gain != 1.0f && got > 0) {
        for (i = 0; i < got; i++) {
            float s = out_interleaved_lr[i] * inst->gain;
            if (s > 32767.0f) s = 32767.0f;
            if (s < -32768.0f) s = -32768.0f;
            out_interleaved_lr[i] = (int16_t)s;
        }
    }

    /* Keep render loop alive while backend is active. */
    if (inst->daemon_running) {
        out_interleaved_lr[needed - 1] |= 5;
    }
}

static plugin_api_v2_t g_plugin_api_v2 = {
    .api_version       = MOVE_PLUGIN_API_VERSION_2,
    .create_instance   = v2_create_instance,
    .destroy_instance  = v2_destroy_instance,
    .on_midi           = v2_on_midi,
    .set_param         = v2_set_param,
    .get_param         = v2_get_param,
    .get_error         = v2_get_error,
    .render_block      = v2_render_block,
};

plugin_api_v2_t* move_plugin_init_v2(const host_api_v1_t *host) {
    g_host = host;
    ap_log("sconnect plugin v2 initialized");
    return &g_plugin_api_v2;
}
