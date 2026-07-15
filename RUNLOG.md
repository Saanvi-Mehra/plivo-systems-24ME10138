# RUNLOG.md

This log tracks the systematic optimization of our real-time UDP streaming engine, grouped by the underlying code configuration.

## Experiment History

| Config ID | Profile | Delay (ms) | Miss Rate (%) | Overhead | Result | Code Setup & Key Lessons Learned |
| :---: | :--- | :---: | :---: | :---: | :--- | :--- |
| **1** | profiles/A.json | 60 ms | 100.00% | 1.52x | **INVALID** | **Baseline Code:** The receiver select loop was oversleeping and dropping late packets. |
| **2** | profiles/A.json | 60 ms | 1.27% | 1.52x | **INVALID** | **Zero-Lag Receiver:** Forwarded packets instantly. Fixed receiver lag but failed by 4 frames due to lack of redundancy. |
| **3** | profiles/A.json | 60 ms | 0.20% | 2.02x | **INVALID** | **Full 1-Frame Redundancy:** Perfect reliability, but exceeded the strict 2.00x bandwidth cap. |
| **4** | profiles/A.json | 60 ms | 0.33% | 1.96x | **VALID** | **Selective FEC (Skip every 15th backup):** First valid run! Safely under the 2.0x overhead threshold. |
| **4** | profiles/B.json | 60 ms | 28.40% | 1.96x | **INVALID** | Tested Config 4 on Profile B. Severe network jitter holding packets caused massive misses. |
| **4** | profiles/B.json | 80 ms | 2.87% | 1.96x | **INVALID** | Swept delay on Profile B. 80 ms is still too narrow to absorb Profile B's heavy network jitter. |
| **4** | profiles/B.json | 100 ms | 0.73% | 1.96x | **VALID (Optimal)** | **Optimal for B.** Playout delay of 100 ms is wide enough to fully absorb the relay's jitter buffer. |
| **4** | profiles/B.json | 150 ms | 0.67% | 1.96x | **VALID** | Confirmed jitter absorption levels off above 100 ms. |
| **4** | profiles/B.json | 200 ms | 0.67% | 1.96x | **VALID** | Maximum bounds tested; no further gain over 100 ms delay. |
| **5** | profiles/A.json | 60 ms | 0.93% | 1.96x | **VALID** | **4-Step Sliding FEC:** Attempted to span a wider frame history to protect against consecutive drops, but created "gaps" in coverage. |
| **5** | profiles/B.json | 60 ms | 31.33% | 2.02x | **INVALID** | Tested Config 5 on Profile B. Performed worse due to gaps and broke bandwidth limit. |
| **5** | profiles/B.json | 80 ms | 3.40% | 2.02x | **INVALID** | Swept delay on Config 5. Confirmed that a simpler, consistent 1-frame backup (Config 4) handles jitter much better. |
| **4** | profiles/A.json | 40 ms | 3.87% | 1.96x | **INVALID** | Reverted to Config 4. Tried to lower delay on Profile A to baseline. Jitter occasionally exceeded the 40 ms playout window. |
| **4** | profiles/A.json | 50 ms | 0.80% | 1.96x | **VALID (Optimal)** | **Optimal for A.** 50 ms delay hit the perfect balance of latency and compliance. |
