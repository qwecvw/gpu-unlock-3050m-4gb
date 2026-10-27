"""GEMM throughput before/after the GA107 unlock: median of several timed runs per dtype."""
import statistics
import sys
import time

import torch

N = 4096
ITERS = 100
RUNS = 5

if not torch.cuda.is_available():
    sys.exit(f"CUDA not available (torch {torch.__version__}); install a CUDA build of PyTorch")

torch.backends.cuda.matmul.allow_tf32 = False
print(f"{torch.cuda.get_device_name(0)} | torch {torch.__version__} | CUDA {torch.version.cuda}")

for name, dtype in (("FP16", torch.float16), ("BF16", torch.bfloat16), ("FP32", torch.float32)):
    a = torch.randn(N, N, dtype=dtype, device="cuda")
    b = torch.randn(N, N, dtype=dtype, device="cuda")
    for _ in range(10):  # warm-up: cuBLAS handle, kernel selection, clocks
        torch.mm(a, b)
    torch.cuda.synchronize()
    results = []
    for _ in range(RUNS):
        start = time.perf_counter()
        for _ in range(ITERS):
            torch.mm(a, b)
        torch.cuda.synchronize()
        results.append(2 * N**3 * ITERS / (time.perf_counter() - start) / 1e12)
    print(f"{name}: median {statistics.median(results):.2f} TFLOPS  (runs: {', '.join(f'{r:.2f}' for r in results)})")
