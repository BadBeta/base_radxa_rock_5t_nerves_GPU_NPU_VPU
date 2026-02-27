/*
 * rknn_infer — General RKNN inference via librknnrt.so
 *
 * Protocol (stdin/stdout, length-prefixed binary):
 *   Request:  [4 bytes: cmd] [4 bytes: payload_len] [payload...]
 *   Response: [4 bytes: status] [4 bytes: payload_len] [payload...]
 *
 * Commands:
 *   CMD_LOAD  (1): payload = model file path (null-terminated string)
 *   CMD_INFER (2): payload = input tensor data
 *   CMD_INFO  (3): no payload — returns model info as JSON
 *   CMD_QUIT  (4): no payload — clean exit
 *
 * Status:
 *   0 = OK, 1 = ERROR (payload = error message)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "rknn_api.h"

#define CMD_LOAD  1
#define CMD_INFER 2
#define CMD_INFO  3
#define CMD_QUIT  4

#define STATUS_OK    0
#define STATUS_ERROR 1

static rknn_context ctx = 0;
static int model_loaded = 0;
static rknn_input_output_num io_num = {0};
static rknn_tensor_attr *input_attrs = NULL;
static rknn_tensor_attr *output_attrs = NULL;

/* get_type_string, get_format_string, get_qnt_type_string are
 * provided as inline functions in rknn_api.h — no need to redefine. */

/* Helper: bytes per element for a tensor type */
static int type_bytes(rknn_tensor_type type)
{
    switch (type) {
    case RKNN_TENSOR_FLOAT32:
    case RKNN_TENSOR_INT32:
    case RKNN_TENSOR_UINT32:   return 4;
    case RKNN_TENSOR_FLOAT16:
    case RKNN_TENSOR_INT16:
    case RKNN_TENSOR_UINT16:   return 2;
    case RKNN_TENSOR_INT64:    return 8;
    default:                   return 1;  /* INT8, UINT8, BOOL */
    }
}

/* Read exactly n bytes from fd */
static int read_exact(int fd, void *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, (char *)buf + got, n - got);
        if (r <= 0) return -1;
        got += r;
    }
    return 0;
}

/* Write exactly n bytes to fd */
static int write_exact(int fd, const void *buf, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = write(fd, (const char *)buf + sent, n - sent);
        if (w <= 0) return -1;
        sent += w;
    }
    return 0;
}

static void send_response(uint32_t status, const void *payload, uint32_t len)
{
    write_exact(STDOUT_FILENO, &status, 4);
    write_exact(STDOUT_FILENO, &len, 4);
    if (len > 0 && payload)
        write_exact(STDOUT_FILENO, payload, len);
}

static void send_ok(const void *payload, uint32_t len)
{
    send_response(STATUS_OK, payload, len);
}

static void send_error(const char *msg)
{
    send_response(STATUS_ERROR, msg, strlen(msg));
}

static void handle_load(const char *path)
{
    /* Unload previous model if any */
    if (model_loaded) {
        rknn_destroy(ctx);
        ctx = 0;
        model_loaded = 0;
        free(input_attrs);
        free(output_attrs);
        input_attrs = NULL;
        output_attrs = NULL;
    }

    /* Read model file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Cannot open model: %s", path);
        send_error(buf);
        return;
    }

    fseek(f, 0, SEEK_END);
    long model_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void *model_data = malloc(model_size);
    if (!model_data) {
        fclose(f);
        send_error("malloc failed for model data");
        return;
    }

    if (fread(model_data, 1, model_size, f) != (size_t)model_size) {
        fclose(f);
        free(model_data);
        send_error("Failed to read model file");
        return;
    }
    fclose(f);

    /* Initialize RKNN */
    int ret = rknn_init(&ctx, model_data, model_size, 0, NULL);
    free(model_data);

    if (ret != RKNN_SUCC) {
        char buf[128];
        snprintf(buf, sizeof(buf), "rknn_init failed: %d", ret);
        send_error(buf);
        return;
    }

    /* Query I/O */
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        rknn_destroy(ctx);
        send_error("rknn_query IN_OUT_NUM failed");
        return;
    }

    /* Query input attributes */
    input_attrs = calloc(io_num.n_input, sizeof(rknn_tensor_attr));
    for (uint32_t i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
    }

    /* Query output attributes */
    output_attrs = calloc(io_num.n_output, sizeof(rknn_tensor_attr));
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
    }

    model_loaded = 1;

    /* Send OK with basic info */
    char info[512];
    snprintf(info, sizeof(info),
             "{\"inputs\":%u,\"outputs\":%u,"
             "\"input_dims\":[%u,%u,%u,%u],"
             "\"input_type\":\"%s\",\"input_fmt\":\"%s\"}",
             io_num.n_input, io_num.n_output,
             input_attrs[0].dims[0], input_attrs[0].dims[1],
             input_attrs[0].dims[2], input_attrs[0].dims[3],
             get_type_string(input_attrs[0].type),
             get_format_string(input_attrs[0].fmt));

    send_ok(info, strlen(info));
}

static void handle_infer(const void *data, uint32_t data_len)
{
    if (!model_loaded) {
        send_error("Model not loaded");
        return;
    }

    /*
     * Multi-input protocol:
     *   [4 bytes: n_inputs]
     *   For each input: [4 bytes: size] [size bytes: data]
     *
     * Single-input backward compat:
     *   If the first 4 bytes interpreted as n_inputs doesn't match io_num.n_input
     *   AND io_num.n_input == 1, treat entire payload as single input data.
     */
    rknn_input *inputs = calloc(io_num.n_input, sizeof(rknn_input));
    if (!inputs) {
        send_error("malloc failed for inputs");
        return;
    }

    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t remaining = data_len;
    int multi_input = 0;

    if (io_num.n_input > 1 && remaining >= 4) {
        /* Multi-input model: payload MUST use multi-input protocol */
        multi_input = 1;
    } else if (io_num.n_input == 1 && remaining >= 4) {
        /* Single-input model: check if payload uses multi-input protocol
         * by reading first 4 bytes as n_inputs */
        uint32_t maybe_n = *(const uint32_t *)ptr;
        if (maybe_n == 1 && remaining > 8) {
            uint32_t maybe_size = *(const uint32_t *)(ptr + 4);
            if (maybe_size + 8 == remaining) {
                multi_input = 1;  /* Looks like multi-input protocol with 1 input */
            }
        }
    }

    if (multi_input) {
        if (remaining < 4) {
            send_error("Invalid multi-input: missing n_inputs");
            free(inputs);
            return;
        }
        uint32_t n_inputs_provided = *(const uint32_t *)ptr;
        ptr += 4; remaining -= 4;

        if (n_inputs_provided != io_num.n_input) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Expected %u inputs, got %u",
                     io_num.n_input, n_inputs_provided);
            send_error(buf);
            free(inputs);
            return;
        }

        for (uint32_t i = 0; i < io_num.n_input; i++) {
            if (remaining < 4) {
                send_error("Incomplete multi-input data");
                free(inputs);
                return;
            }
            uint32_t size = *(const uint32_t *)ptr;
            ptr += 4; remaining -= 4;

            if (remaining < size) {
                send_error("Input data truncated");
                free(inputs);
                return;
            }

            inputs[i].index = i;
            inputs[i].buf = (void *)ptr;
            inputs[i].size = size;
            inputs[i].pass_through = 0;

            /* Auto-detect type from data size vs expected elements */
            uint32_t n_elems = input_attrs[i].n_elems;
            if (size == n_elems) {
                inputs[i].type = RKNN_TENSOR_UINT8;
            } else if (size == n_elems * 2) {
                inputs[i].type = RKNN_TENSOR_FLOAT16;
            } else if (size == n_elems * 4) {
                inputs[i].type = RKNN_TENSOR_FLOAT32;
            } else if (size == n_elems * 8) {
                inputs[i].type = RKNN_TENSOR_INT64;
            } else {
                inputs[i].type = input_attrs[i].type;
            }
            inputs[i].fmt = input_attrs[i].fmt;

            ptr += size; remaining -= size;
        }
    } else {
        /* Legacy single-input: entire payload is one tensor */
        inputs[0].index = 0;
        inputs[0].buf = (void *)data;
        inputs[0].size = data_len;
        inputs[0].pass_through = 0;

        uint32_t n_elems = input_attrs[0].n_elems;
        if (data_len == n_elems) {
            inputs[0].type = RKNN_TENSOR_UINT8;
        } else if (data_len == n_elems * 2) {
            inputs[0].type = RKNN_TENSOR_FLOAT16;
        } else if (data_len == n_elems * 4) {
            inputs[0].type = RKNN_TENSOR_FLOAT32;
        } else {
            inputs[0].type = input_attrs[0].type;
        }
        inputs[0].fmt = RKNN_TENSOR_NHWC;
    }

    int ret = rknn_inputs_set(ctx, io_num.n_input, inputs);
    free(inputs);
    if (ret != RKNN_SUCC) {
        char buf[128];
        snprintf(buf, sizeof(buf), "rknn_inputs_set failed: %d", ret);
        send_error(buf);
        return;
    }

    /* Run inference */
    ret = rknn_run(ctx, NULL);
    if (ret != RKNN_SUCC) {
        char buf[128];
        snprintf(buf, sizeof(buf), "rknn_run failed: %d", ret);
        send_error(buf);
        return;
    }

    /* Get outputs — request float32 for easier postprocessing in Elixir */
    rknn_output *outputs = calloc(io_num.n_output, sizeof(rknn_output));
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        outputs[i].index = i;
        outputs[i].want_float = 1;
    }

    ret = rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);
    if (ret != RKNN_SUCC) {
        free(outputs);
        char buf[128];
        snprintf(buf, sizeof(buf), "rknn_outputs_get failed: %d", ret);
        send_error(buf);
        return;
    }

    /* Send all outputs: [n_outputs:4] [size0:4] [data0...] [size1:4] [data1...] */
    uint32_t total_size = 4;
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        total_size += 4 + outputs[i].size;
    }

    uint8_t *response = malloc(total_size);
    uint32_t offset = 0;

    memcpy(response + offset, &io_num.n_output, 4);
    offset += 4;

    for (uint32_t i = 0; i < io_num.n_output; i++) {
        uint32_t sz = outputs[i].size;
        memcpy(response + offset, &sz, 4);
        offset += 4;
        memcpy(response + offset, outputs[i].buf, sz);
        offset += sz;
    }

    send_ok(response, total_size);

    free(response);
    rknn_outputs_release(ctx, io_num.n_output, outputs);
    free(outputs);
}

static void handle_info(void)
{
    if (!model_loaded) {
        send_error("Model not loaded");
        return;
    }

    char buf[4096];
    int pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"inputs\":[");
    for (uint32_t i = 0; i < io_num.n_input; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "{\"index\":%u,\"name\":\"%s\","
                        "\"dims\":[%u,%u,%u,%u],"
                        "\"n_elems\":%u,\"size\":%u,"
                        "\"type\":\"%s\",\"fmt\":\"%s\","
                        "\"qnt\":\"%s\",\"zp\":%d,\"scale\":%.6f}",
                        input_attrs[i].index, input_attrs[i].name,
                        input_attrs[i].dims[0], input_attrs[i].dims[1],
                        input_attrs[i].dims[2], input_attrs[i].dims[3],
                        input_attrs[i].n_elems, input_attrs[i].size,
                        get_type_string(input_attrs[i].type),
                        get_format_string(input_attrs[i].fmt),
                        get_qnt_type_string(input_attrs[i].qnt_type),
                        input_attrs[i].zp, input_attrs[i].scale);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos, "],\"outputs\":[");
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "{\"index\":%u,\"name\":\"%s\","
                        "\"dims\":[%u,%u,%u,%u],"
                        "\"n_elems\":%u,\"size\":%u,"
                        "\"type\":\"%s\",\"fmt\":\"%s\","
                        "\"qnt\":\"%s\",\"zp\":%d,\"scale\":%.6f}",
                        output_attrs[i].index, output_attrs[i].name,
                        output_attrs[i].dims[0], output_attrs[i].dims[1],
                        output_attrs[i].dims[2], output_attrs[i].dims[3],
                        output_attrs[i].n_elems, output_attrs[i].size,
                        get_type_string(output_attrs[i].type),
                        get_format_string(output_attrs[i].fmt),
                        get_qnt_type_string(output_attrs[i].qnt_type),
                        output_attrs[i].zp, output_attrs[i].scale);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");

    send_ok(buf, pos);
}

int main(int argc, char **argv)
{
    /* Disable stdout buffering for binary protocol */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* If model path passed as argument, load immediately */
    if (argc > 1) {
        handle_load(argv[1]);
    }

    /* Main command loop */
    while (1) {
        uint32_t cmd, payload_len;

        if (read_exact(STDIN_FILENO, &cmd, 4) != 0) break;
        if (read_exact(STDIN_FILENO, &payload_len, 4) != 0) break;

        void *payload = NULL;
        if (payload_len > 0) {
            payload = malloc(payload_len);
            if (!payload) {
                send_error("malloc failed");
                continue;
            }
            if (read_exact(STDIN_FILENO, payload, payload_len) != 0) {
                free(payload);
                break;
            }
        }

        switch (cmd) {
        case CMD_LOAD:
            if (payload) {
                ((char *)payload)[payload_len - 1] = '\0';
                handle_load((const char *)payload);
            } else {
                send_error("No model path provided");
            }
            break;

        case CMD_INFER:
            handle_infer(payload, payload_len);
            break;

        case CMD_INFO:
            handle_info();
            break;

        case CMD_QUIT:
            free(payload);
            goto cleanup;

        default:
            send_error("Unknown command");
            break;
        }

        free(payload);
    }

cleanup:
    if (model_loaded) {
        rknn_destroy(ctx);
        free(input_attrs);
        free(output_attrs);
    }

    return 0;
}
