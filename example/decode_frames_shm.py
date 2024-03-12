import time
import numpy as np
from multiprocessing import shared_memory

import ffio

# Here is an example that shows you may use a SharedMemory buff,
# which contains some other data and not only one frame.
some_data_bytes = 2 * 4096
rgb_data_bytes  = 30 * 1080 * 1920 * 3  # 30 frames of 1080p rgb data.
shm_size = some_data_bytes + rgb_data_bytes
your_shm = shared_memory.SharedMemory(create=True, size=shm_size)

target_url = "rtmp://..."
ffio       = ffio.FFIO(
  target_url, ffio.FFIOMode.DECODE, False,
  your_shm.name, shm_size, shm_offset=some_data_bytes
)
if ffio:
  time_last  = time.time()
  time_begin = time_last
  time_total = 0
  idx        = 0
  rgb_index  = idx % 30
  # If your shm contains more than one frame,
  # you can specify an offset to place the RGB bytes in the desired location.
  # Just do it like this:
  rgbs = np.ndarray((30, 1080, 1920, 3), dtype=np.uint8, buffer=your_shm.buf, offset=some_data_bytes)
  while ffio.decode_one_frame_to_shm( offset=(rgb_index * 1080 * 1920 * 3) ) and idx < 500:
    # Access the rgb data:
    frame = rgbs[rgb_index]

    dt          = time.time() - time_last
    time_last  += dt
    time_total  = time_last - time_begin
    idx        += 1
    avg         = time_total * 1000 / idx
    fps         = 1000 / avg
    print(f"{idx}: dt:{dt * 1000:.2f}ms, avg:{avg:.2f}ms, {fps:.2f}fps, "
          f"total: {time_total:.3f}s, shape:{frame.shape}")
    rgb_index   = idx % 30
  # Attention !!
  # Force quitting this script will result in a memory leak.
  # Ensure the following process is executed.
  ffio.release_memory()
your_shm.close()
your_shm.unlink()
print('successfully release memory of input stream context.')

# If you quit without cleanup the memory,
# you can find the shm in /dev/shm/, name like: psm_b699387a
# Than you can clean it manually:
# from multiprocessing import shared_memory
# _shm = shared_memory.SharedMemory(name="psm_b699387a")
# _shm.close()
# _shm.unlink()
