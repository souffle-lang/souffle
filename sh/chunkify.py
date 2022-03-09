#!/usr/bin/env python3

"""Helper for GitHub Actions."""

n_tests, n_chunks, chunk_id = map(int, input().split())
d, m = divmod(n_tests, n_chunks)
print(f"{chunk_id * d + min(m, chunk_id) + 1},{(chunk_id+1) * d + min(m, chunk_id+1)}")
