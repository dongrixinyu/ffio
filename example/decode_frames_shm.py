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

stream_url = "rtmp://..."
stream_obj = ffio.InputStreamParser(stream_url, False,
                                    your_shm.name, shm_size, shm_offset=some_data_bytes)
time_last  = time.time()
time_begin = time_last
time_total = 0
idx        = 0
rgb_index  = idx % 30
# If your shm contains more than one frame,
# you can specify an offset to place the RGB bytes in the desired location.
# Just do it like this:
rgbs = np.ndarray((30, 1080, 1920, 3), dtype=np.uint8, buffer=your_shm.buf, offset=some_data_bytes)
while stream_obj.get_one_frame_to_shm( offset=(rgb_index * 1080 * 1920 * 3) ):
    # Access the rgb data:
    frame = rgbs[rgb_index]

    dt          = time.time() - time_last
    time_last  += dt
    time_total  = time_last - time_begin
    idx        += 1
    avg         = time_total * 1000 / idx
    fps         = 1000 / avg
    print(f"{idx}: dt:{dt * 1000:.2f}ms, avg:{avg:.2f}ms, {fps}fps, "
          f"total: {time_total:.3f}s, shape:{frame.shape}")

stream_obj.release_memory()
print('successfully release memory of input stream context.')
