import time
import cv2
import numpy as np
from multiprocessing import shared_memory

import ffio

def test():
  # Here is an example that shows you may use a SharedMemory buff,
  # which contains some other data and not only one frame.
  some_data_bytes = 2 * 4096
  rgb_data_bytes  = 30 * 1080 * 1920 * 3  # 30 frames of 1080p rgb data.
  shm_size = some_data_bytes + rgb_data_bytes
  your_shm = shared_memory.SharedMemory(create=True, size=shm_size)

  i_url  = "rtmp://..."
  o_url  = "rtmp://..."
  i_pipe = ffio.InputStreamParser ( i_url, True,
                                    your_shm.name, shm_size, shm_offset=some_data_bytes)
  o_pipe = ffio.OutputStreamParser( o_url, use_cuda=True, preset="fast",
                                    framerate_num=24, framerate_den=1,
                                    image_width=1920, image_height=1080,
                                    shm_name=your_shm.name, shm_size=shm_size, shm_offset=some_data_bytes
                                    )

  time_last  = time.time()
  time_begin = time_last
  time_total = 0
  idx        = 10
  rgb_index  = idx % 30
  # If your shm contains more than one frame,
  # you can specify an offset to place the RGB bytes in the desired location.
  # Just do it like this:
  rgbs = np.ndarray((30, 1080, 1920, 3), dtype=np.uint8, buffer=your_shm.buf, offset=some_data_bytes)
  while i_pipe.decode_one_frame_to_shm( offset=(rgb_index * 1080 * 1920 * 3) ):
      # Access the rgb data:
      frame = rgbs[rgb_index]
      # Maybe you want to make push slower than pull, than you can do like this:
      output_idx = (idx - 10) % 30
      _draw(rgbs[output_idx], idx)  # The cv2.rectangle() operates in-place.
      o_pipe.encode_one_frame_from_shm( offset=output_idx*1080*1920*3 )

      dt          = time.time() - time_last
      time_last  += dt
      time_total  = time_last - time_begin
      idx        += 1
      rgb_index   = idx % 30
      avg         = time_total * 1000 / idx
      fps         = 1000 / avg
      print(f"{idx}: dt:{dt * 1000:.2f}ms, avg:{avg:.2f}ms, {fps}fps, "
            f"total: {time_total:.3f}s, shape:{frame.shape}")

  i_pipe.release_memory()
  o_pipe.release_memory()
  print('successfully release memory of input stream context.')

def _draw(frame: np.ndarray, seq: int):
  diff    = seq % 100
  coord_x = 20 + diff*4
  coord_y = 10 + diff*2
  cv2.rectangle(frame, (coord_x, coord_y), (coord_x+200, coord_y+150), (0, 255, 0), 2)


test()
