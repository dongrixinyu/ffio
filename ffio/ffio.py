import os
import time
import cv2
import base64
import numpy as np

from enum    import Enum
from pathlib import Path
from typing  import Optional
from PIL     import Image
from ctypes  import Structure, PyDLL, POINTER, c_int, c_bool, c_char_p, py_object, c_char, byref, c_ubyte

DIR_PATH = os.path.dirname(os.path.abspath(__file__))


class CFFIO(Structure):
  _fields_ = [
    ("ffio_state",         c_int),
    ("ffio_mode",          c_int),
    ("frame_seq",          c_int),
    ("hw_enabled",         c_bool),
    ("shm_enabled",        c_bool),
    ("shm_fd",             c_int),
    ("shm_size",           c_int),
    ("video_stream_index", c_int),
    ("image_width",        c_int),
    ("image_height",       c_int),
    ("image_byte_size",    c_int),
    ("pts_anchor",         c_int)
  ]


class CCodecParams(Structure):
  width      : int
  height     : int
  bitrate    : int
  fps        : int
  gop        : int
  b_frames   : int
  pts_trick  : int
  profile    : bytes
  preset     : bytes
  tune       : bytes
  pix_fmt    : bytes
  format     : bytes
  codec      : bytes
  sei_uuid   : c_ubyte * 16
  use_h264_AnnexB_sei : bool

  _fields_ = [
    ("width",               c_int),
    ("height",              c_int),
    ("bitrate",             c_int),
    ("fps",                 c_int),
    ("gop",                 c_int),
    ("b_frames",            c_int),
    ("pts_trick",           c_int),
    ("profile",             c_char * 24),
    ("preset",              c_char * 24),
    ("tune",                c_char * 24),
    ("pix_fmt",             c_char * 24),
    ("format",              c_char * 24),
    ("codec",               c_char * 24),
    ('sei_uuid',            c_ubyte * 16),
    ('use_h264_AnnexB_sei', c_bool)
  ]

  def __init__(self):
    super(CCodecParams, self).__init__()
    self.use_h264_AnnexB_sei = True
    self.sei_uuid = (c_ubyte * 16).from_buffer_copy(b'\x0f\xf1\x0f\xf1'
                                                    b'\x00\x11\x22\x33'
                                                    b'\xa0\xb1\xc2\xd3'
                                                    b'\x00\x11\x22\x33')


c_lib_path = ( os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.dylib')
               if Path(os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.dylib')).is_file()
               else os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.so'))
c_lib = PyDLL(c_lib_path)

c_lib.api_newFFIO.argtypes = []
c_lib.api_newFFIO.restype  = POINTER(CFFIO)

c_lib.api_initFFIO.argtypes = [
  POINTER(CFFIO), c_int, c_char_p,
  c_bool, c_char_p,
  c_bool, c_char_p, c_int, c_int,
  POINTER(CCodecParams)
]
c_lib.api_initFFIO.restype  = None

c_lib.api_finalizeFFIO.argtypes = [POINTER(CFFIO)]
c_lib.api_finalizeFFIO.restype  = None
c_lib.api_deleteFFIO.argtypes   = [POINTER(CFFIO)]
c_lib.api_deleteFFIO.restype    = None

c_lib.api_decodeOneFrame.argtypes      = [POINTER(CFFIO)]
c_lib.api_decodeOneFrame.restype       = py_object
c_lib.api_decodeOneFrameToShm.argtypes = [POINTER(CFFIO), c_int]
c_lib.api_decodeOneFrameToShm.restype  = c_bool

c_lib.api_encodeOneFrame.argtypes        = [POINTER(CFFIO), py_object]
c_lib.api_encodeOneFrame.restype         = c_int
c_lib.api_encodeOneFrameFromShm.argtypes = [POINTER(CFFIO), c_int]
c_lib.api_encodeOneFrameFromShm.restype  = c_bool


class FFIOMode(Enum):
  DECODE = 0
  ENCODE = 1


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

  def encode_one_frame(self, rgb_image) -> bool:
    rgb_image_type = type(rgb_image)
    if rgb_image_type is bytes:
      ret = c_lib.api_encodeOneFrame(self._c_ffio_ptr, rgb_image)
      if ret == 0:
        self.frame_seq_py += 1
        return True
      return False

    elif rgb_image_type == np.ndarray:
      rgb_image_bytes = rgb_image.tobytes()
      return self.encode_one_frame(rgb_image_bytes)
    elif rgb_image_type == Image:
      return False  # not implemented yet.

    return False

  def encode_one_frame_from_shm(self, offset=0) -> bool:
    # encode one rgb data frame from shm, and write it to target video.
    ret = c_lib.api_encodeOneFrameFromShm(self._c_ffio_ptr, offset)
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
