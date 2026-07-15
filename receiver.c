/* BASELINE RECEIVER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47002  <- media from your sender, via the hostile relay
 *   send 47020  -> harness player. MUST be: 4-byte big-endian seq +
 *                  160-byte payload. Frame i counts only if it arrives
 *                  BEFORE its deadline t0 + DELAY_MS + i*20ms.
 *   send 47003  -> feedback to your sender, via the relay (optional)
 *
 * This baseline forwards whatever arrives straight to the player: lost
 * frames stay lost, late frames stay late, duplicates are re-sent
 * harmlessly. All yours to fix — jitter buffer, reordering, recovery.
 *
 * Env vars available: T0, DURATION_S, DELAY_MS. Harness kills the process
 * at run end; a forever-loop is fine.
 */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#define PAYLOAD_SIZE 160
#define BUFFER_SIZE 30000

// Track which sequences we have already sent to the player to suppress duplicates
bool sent_to_player[BUFFER_SIZE];

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(sent_to_player, 0, sizeof(sent_to_player));
    uint8_t recv_buf[1024];

    for (;;) {
        ssize_t n = recvfrom(in_fd, recv_buf, sizeof(recv_buf), 0, NULL, NULL);
        if (n <= 0) continue;

        if (n >= 4) {
            uint32_t prim_seq_net;
            memcpy(&prim_seq_net, recv_buf, 4);
            uint32_t prim_seq = ntohl(prim_seq_net);

            if (n == 4 + PAYLOAD_SIZE) { // Even packet (Solo payload)
                if (prim_seq < BUFFER_SIZE && !sent_to_player[prim_seq]) {
                    // Forward directly to player immediately
                    uint8_t out_buf[164];
                    memcpy(out_buf, &prim_seq_net, 4);
                    memcpy(out_buf + 4, recv_buf + 4, PAYLOAD_SIZE);
                    sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&player, sizeof player);
                    sent_to_player[prim_seq] = true;
                }
            } 
            else if (n == 4 + 2 * PAYLOAD_SIZE) { // Odd packet (Primary + Backup payload)
                // 1. Deliver primary frame if not sent yet
                if (prim_seq < BUFFER_SIZE && !sent_to_player[prim_seq]) {
                    uint8_t out_buf[164];
                    memcpy(out_buf, &prim_seq_net, 4);
                    memcpy(out_buf + 4, recv_buf + 4, PAYLOAD_SIZE);
                    sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&player, sizeof player);
                    sent_to_player[prim_seq] = true;
                }
                // 2. Deliver backup frame if not sent yet
                if (prim_seq > 0) {
                    uint32_t back_seq = prim_seq - 1;
                    if (back_seq < BUFFER_SIZE && !sent_to_player[back_seq]) {
                        uint8_t out_buf[164];
                        uint32_t back_seq_net = htonl(back_seq);
                        memcpy(out_buf, &back_seq_net, 4);
                        memcpy(out_buf + 4, recv_buf + 4 + PAYLOAD_SIZE, PAYLOAD_SIZE);
                        sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&player, sizeof player);
                        sent_to_player[back_seq] = true;
                    }
                }
            }
        }
    }

    close(in_fd);
    close(out_fd);
    return 0;
}
