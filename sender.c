/* BASELINE SENDER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47010  <- harness source delivers frame i here at t0 + i*20ms
 *                  (format: 4-byte big-endian seq + 160-byte payload)
 *   send 47001  -> relay uplink toward the receiver (YOUR wire format)
 *   bind 47004  <- feedback from your receiver, via the relay (optional)
 *
 * This baseline forwards each frame once, unchanged, and ignores feedback.
 * No redundancy, no retransmission. It cannot pass. That is the point.
 *
 * Env vars available if you want them: T0 (epoch seconds, float),
 * DURATION_S, DELAY_MS. The harness kills this process when the run ends,
 * so a forever-loop is fine.
 *
 * build: make        run: python3 run.py --delay_ms 60
 */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>

#define PAYLOAD_SIZE 160
#define BUFFER_SIZE 30000

uint8_t frame_archive[BUFFER_SIZE][PAYLOAD_SIZE];
int frame_archived[BUFFER_SIZE];

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(frame_archived, 0, sizeof(frame_archived));
    uint8_t recv_buf[1024];

    for (;;) {
        ssize_t n = recvfrom(in_fd, recv_buf, sizeof(recv_buf), 0, NULL, NULL);
        if (n < 4) continue;

        uint32_t seq_net;
        memcpy(&seq_net, recv_buf, 4);
        uint32_t seq = ntohl(seq_net);

        if (seq < BUFFER_SIZE) {
            memcpy(frame_archive[seq], recv_buf + 4, PAYLOAD_SIZE);
            frame_archived[seq] = 1;
        }

        uint8_t send_buf[4 + 2 * PAYLOAD_SIZE];
        memcpy(send_buf, &seq_net, 4);
        memcpy(send_buf + 4, recv_buf + 4, PAYLOAD_SIZE);

        // Send backup of previous frame, unless seq is a multiple of 15.
        // This drops total bandwidth overhead to ~1.95x, staying under the 2.0x cap.
        if (seq > 0 && frame_archived[seq - 1] && (seq % 15 != 0)) {
            memcpy(send_buf + 4 + PAYLOAD_SIZE, frame_archive[seq - 1], PAYLOAD_SIZE);
            sendto(out_fd, send_buf, 4 + 2 * PAYLOAD_SIZE, 0, (struct sockaddr *)&relay, sizeof relay);
        } else {
            sendto(out_fd, send_buf, 4 + PAYLOAD_SIZE, 0, (struct sockaddr *)&relay, sizeof relay);
        }
    }

    close(in_fd);
    close(out_fd);
    return 0;
}
