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
  i_pipe = ffio.FFIO( i_url, ffio.FFIOMode.DECODE, True,
                      your_shm.name, shm_size, shm_offset=some_data_bytes)
  params = ffio.CCodecParams()
  params.width    = 1920
  params.height   = 1080
  params.bitrate  = 2400*1024
  params.fps      = 24
  params.gop      = 48
  params.b_frames = 0
  params.profile  = b"baseline"
  params.preset   = b"fast"
  o_pipe = ffio.FFIO( o_url, ffio.FFIOMode.ENCODE, True,
                      your_shm.name, shm_size, shm_offset=some_data_bytes, codec_params=params)

  if i_pipe and o_pipe:
    time_total = 0
    idx        = 10
    in_index   = idx % 30
    # If your shm contains more than one frame,
    # you can specify an offset to place the RGB bytes in the desired location.
    # Just do it like this:
    rgbs = np.ndarray((30, 1080, 1920, 3), dtype=np.uint8, buffer=your_shm.buf, offset=some_data_bytes)
    while i_pipe.decode_one_frame_to_shm( offset=(in_index * 1080 * 1920 * 3) ) and idx < 500:
      time_last = time.time()
      # Access the rgb data:
      # Maybe you want to make push slower than pull, than you can do like this:
      out_idx    = (idx - 10) % 30
      out_offset = out_idx*1080*1920*3
      frame      = rgbs[out_idx]
      # Same as above:
      # frame = np.ndarray((1080, 1920, 3), dtype=np.uint8, buffer=your_shm.buf, offset=some_data_bytes+out_offset)
      _draw(frame, idx)        # The cv2.rectangle() operates in-place.
      o_pipe.encode_one_frame_from_shm( offset=out_offset )

      dt          = time.time() - time_last
      time_total += dt
      idx        += 1
      in_index    = idx % 30
      avg         = time_total * 1000 / (idx-10)
      fps         = 1000 / avg
      print(f"{idx}: dt:{dt * 1000:.2f}ms, avg:{avg:.2f}ms, {fps:.2f}fps, "
            f"total: {time_total:.3f}s, shape:{frame.shape}")
  # Attention !!
  # Force quitting this script will result in a memory leak.
  # Ensure the following process is executed.
    i_pipe.release_memory()
    o_pipe.release_memory()
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


def _draw(frame: np.ndarray, seq: int):
  diff    = seq % 100
  coord_x = 20 + diff*4
  coord_y = 10 + diff*2
  cv2.rectangle(frame, (coord_x, coord_y), (coord_x+200, coord_y+150), (0, 255, 0), 2)


test()
