import time
import cv2
import base64
import numpy as np

from typing  import Optional
from PIL     import Image
from ctypes  import POINTER, byref

from ffio.ffio_c import CFFIO, CCodecParams, FFIOMode, c_lib


class FFIO(object):
  _c_ffio_ptr  : Optional[POINTER(CFFIO)]
  target_url   : Optional[str]
  mode         : FFIOMode                   # decode or encode
  frame_seq_py : int                        # from this py class:FFIO
  frame_seq_c  : int                        # from c struct:FFIO          @property
  shm_enabled  : bool
  width        : int
  height       : int
  ffio_state   : int                        # from c struct:FFIO          @property
  codec_params : Optional[CCodecParams]
  hw_device    : str

  hw_device = "cuda"   # or try others listed by ` ffmpeg -hwaccels` for your device.

  def __init__(self, target_url: str, mode: FFIOMode = FFIOMode.DECODE,
               hw_enabled: bool = False,
               shm_name: str = None, shm_size: int = 0, shm_offset: int = 0,
               codec_params: CCodecParams = None):
    start_time = time.time()

    self.target_url   = target_url
    self.mode         = mode
    self.frame_seq_py = 0
    self.codec_params = codec_params if codec_params is not None else CCodecParams()
    self._c_ffio_ptr  = c_lib.api_newFFIO()

    int_mode = 0 if mode == FFIOMode.DECODE else 1
    if shm_name is None:
      self.shm_enabled = False
      c_lib.api_initFFIO(
        self._c_ffio_ptr,
        int_mode, self.target_url.encode(),
        hw_enabled, self.hw_device.encode(),
        self.shm_enabled, "".encode(), 0, 0,
        byref(self.codec_params)
      )
    else:
      self.shm_enabled = True
      c_lib.api_initFFIO(
        self._c_ffio_ptr,
        int_mode, self.target_url.encode(),
        hw_enabled, self.hw_device.encode(),
        self.shm_enabled, shm_name.encode(), shm_size, shm_offset,
        byref(self.codec_params)
      )

    end_time = time.time()

    if self._c_ffio_ptr.contents.ffio_state == 1:  # succeeded
      print(f"[ffio_py][{self.mode.name}] inited ffio after: {(end_time-start_time):.4f} seconds.")
      print(f"[ffio_py][{self.mode.name}] open stream with: "
            f"{self._c_ffio_ptr.contents.image_width}x{self._c_ffio_ptr.contents.image_height}.")

      self.width  = self._c_ffio_ptr.contents.image_width
      self.height = self._c_ffio_ptr.contents.image_height
    else:
      print(f"[ffio_py][{self.mode.name}] failed to initialize ffio after: {(end_time-start_time):.4f} seconds.")
      c_lib.api_deleteFFIO(self._c_ffio_ptr)

  @property
  def ffio_state(self) -> bool:
    # see FFIOState defined in ffio.h
    state = self._c_ffio_ptr.contents.ffio_state
    return True if state == 1 or state == 2 else False

  @property
  def frame_seq_c(self):
    return self._c_ffio_ptr.contents.frame_seq

  def decode_one_frame(self, image_format: Optional[str] = "numpy"):
    # image_format: numpy, Image, base64, None
    ret = c_lib.api_decodeOneFrame(self._c_ffio_ptr)
    ret_type = type(ret)
    if ret_type is bytes:  # rgb data of image will return with type: bytes.
      self.frame_seq_py += 1
      if image_format is None:
        return ret
      elif image_format == 'numpy':
        np_buffer = np.frombuffer(ret, dtype=np.uint8)
        np_frame  = np.reshape(np_buffer, (self.height, self.width, 3))
        return np_frame
      elif image_format == 'Image':
        rgb_image = Image.frombytes("RGB", (self.width, self.height), ret)
        return rgb_image
      elif image_format == 'base64':
        np_buffer = np.frombuffer(ret, dtype=np.uint8)
        np_frame  = np.reshape(np_buffer, (self.height, self.width, 3))
        bgr_image = cv2.cvtColor(np_frame, cv2.COLOR_RGB2BGR)
        np_image  = cv2.imencode('.jpg', bgr_image)[1]
        base64_image_code = base64.b64encode(np_image).decode()
        return base64_image_code

    elif ret_type is int:
      return False

  def decode_one_frame_to_shm(self, offset=0) -> bool:
    # decode one frame from target video, and write raw rgb bytes of that frame to shm.
    ret = c_lib.api_decodeOneFrameToShm(self._c_ffio_ptr, offset)
    if ret:
      self.frame_seq_py += 1
    return c_lib.api_decodeOneFrameToShm(self._c_ffio_ptr, offset)

  def encode_one_frame(self, rgb_image, sei_msg: Optional[bytes, np.ndarray] = None) -> bool:
    rgb_image_type = type(rgb_image)
    if rgb_image_type is bytes:
      ret = c_lib.api_encodeOneFrame(self._c_ffio_ptr, rgb_image, sei_msg)
      if ret == 0:
        self.frame_seq_py += 1
        return True
      return False

    elif rgb_image_type == np.ndarray:
      rgb_image_bytes = rgb_image.tobytes()
      return self.encode_one_frame(rgb_image_bytes, sei_msg)
    elif rgb_image_type == Image:
      return False  # not implemented yet.

    return False

  def encode_one_frame_from_shm(self, offset: int = 0, sei_msg: Optional[bytes] = None) -> bool:
    # encode one rgb data frame from shm, and write it to target video.
    ret = c_lib.api_encodeOneFrameFromShm(self._c_ffio_ptr, offset, sei_msg)
    if ret:
      self.frame_seq_py += 1
    return ret

  def release_memory(self):
    c_lib.api_finalizeFFIO(self._c_ffio_ptr)
    c_lib.api_deleteFFIO(self._c_ffio_ptr)

    self.target_url   = None
    self._c_ffio_ptr  = None
    self.codec_params = None
    self.width        = 0
    self.height       = 0
    self.frame_seq_py = 0
