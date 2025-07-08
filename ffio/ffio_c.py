# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu, koisi
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com

import os
import pdb
import base64
import numpy as np

from enum    import IntEnum
from pathlib import Path
from ctypes  import Structure, PyDLL, POINTER, c_int, c_bool, c_char_p, py_object, \
  c_char, c_ubyte, c_byte, string_at, c_double, c_int64
from PIL     import Image
from io      import BytesIO


class FFIOMode(IntEnum):
  DECODE = 0
  ENCODE = 1


class FFIOFrameType(IntEnum):
  FFIO_FRAME_TYPE_ERROR = -1
  FFIO_FRAME_TYPE_RGB   = 0
  FFIO_FRAME_TYPE_EOF   = 1


class FFIOPTSTrick(IntEnum):
  FFIO_PTS_TRICK_EVEN     = 0    # For     live-streaming scenarios.
  FFIO_PTS_TRICK_INCREASE = 1    # For non-live-streaming scenarios.
  FFIO_PTS_TRICK_RELATIVE = 2    # If you are calling encodeOneFrame() at a stable rate.
  FFIO_PTS_TRICK_DIRECT   = 3    # Manually set `ffio->pts_anchor` every time before encodeOneFrame().


class CFFIOFrame(Structure):
  type    : int    # see  c: enum FFIOFrameType
  err     : int    # see  c: enum FFIOError
  _width  : int
  _height : int
  sei_msg : POINTER(c_byte)
  sei_msg_size: int
  data    : POINTER(c_ubyte)
  _fields_ = [
    ("type",    c_int),
    ("err",     c_int),
    ("_width",  c_int),
    ("_height", c_int),
    ("sei_msg", POINTER(c_byte)),
    ("sei_msg_size", c_int),
    ("data",    POINTER(c_ubyte))
  ]

  def __repr__(self) -> str:
    # default: <ffio.ffio_c.CFFIOFrame object at 0x7f1719261a70>
    if self.__bool__():
      has_sei = True if self.sei_msg_size >0 else False
      return f"<ffio.ffio_c.CFFIOFrame, valid frame, {self._width}x{self._height}, " \
            f"has_sei: {has_sei} at {hex(id(self))}>"
    else:
      return f"<ffio.ffio_c.CFFIOFrame, invalid frame at {hex(id(self))}>"

  def __bool__(self):
    return self.type == FFIOFrameType.FFIO_FRAME_TYPE_RGB.value

  def as_numpy(self) -> np.ndarray:
    return np.ctypeslib.as_array(self.data, shape=(self._height, self._width, 3))

  def as_image(self) -> Image:
    return Image.fromarray(self.as_numpy(), 'RGB')

  def as_base64(self) -> str:
    image  = self.as_image()
    buffer = BytesIO()
    image.save(buffer, format="JPEG")
    base64_str = base64.b64encode(buffer.getvalue()).decode()

    return base64_str

  def get_sei(self) -> bytes:
    return string_at(self.sei_msg, self.sei_msg_size)


class CFFIO(Structure):
  _fields_ = [
    ("ffio_state",         c_int),
    ("ffio_mode",          c_int),
    ("frame_seq",          c_int),
    ("hw_enabled",         c_bool),
    ("pix_fmt_hw_enabled", c_bool),
    ("shm_enabled",        c_bool),
    ("shm_fd",             c_int),
    ("shm_size",           c_int),
    ("video_stream_index", c_int),
    ("image_width",        c_int),
    ("image_height",       c_int),
    ("image_byte_size",    c_int),
    ("framerate",          c_double),

    ("pts_anchor",         c_int64)
  ]


class CCodecParams(Structure):
  # All parameters are provided with default values properly, so ffio can run normally
  # even if you do not provide a CCodecParams.
  # If a parameter is required only by the encoder, you can simply ignore it when you are
  # working on decoding.(Currently all params are required by encoder only.)

  # Required by (E)ncoder or (D)ecoder  |   default value, see: ffio_init_check_and_set_codec_params()
  width               : int            # E   | 1920
  height              : int            # E   | 1080
  bitrate             : int            # E   | 2.4Mbps
  max_bitrate         : int            # E   | 2.4Mbps
  fps                 : int            # E   | 12
  gop                 : int            # E   | 12
  b_frames            : int            # E   | 0
  pts_trick           : int            # E   | see: FFIO._auto_set_pts_trick()
  flags               : bytes
  flags2              : bytes
  profile             : bytes          # E   | "high422"
  preset              : bytes          # E   | "veryslow"
  tune                : bytes          # E   | None
  pix_fmt             : bytes          # E   | see: find_avcodec_1st_sw_pix_fmt()
  format              : bytes          # E   | rtmp -> flv; srt -> mpegts
  codec               : bytes          # E   | hw_enabled ? h264_nvenc : libx264;
  sei_uuid            : c_ubyte * 16   # E   | see: CCodecParams.__init__()
  use_h264_AnnexB_sei : bool           # E   | True

  _fields_ = [
    ("width",               c_int),
    ("height",              c_int),
    ("bitrate",             c_int),
    ("max_bitrate",         c_int),
    ("fps",                 c_int),
    ("gop",                 c_int),
    ("b_frames",            c_int),
    ("pts_trick",           c_int),
    ("flags",               c_char * 24),
    ("flags2",              c_char * 24),
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
    self.pts_trick           = -1
    self.use_h264_AnnexB_sei = True
    # You can modify this uuid what else you want.
    self.sei_uuid = (c_ubyte * 16).from_buffer_copy(b'\x0f\xf1\x0f\xf1'
                                                    b'\x00\x11\x22\x33'
                                                    b'\xa0\xb1\xc2\xd3'
                                                    b'\x00\x11\x22\x33')


DIR_PATH = os.path.dirname(os.path.abspath(__file__))


cuda_c_lib_path = (os.path.join(DIR_PATH, 'build', 'libcudaAPI.dylib')
              if Path(os.path.join(DIR_PATH, 'build', 'libcudaAPI.dylib')).is_file()
              else os.path.join(DIR_PATH, 'build', 'libcudaAPI.so'))
try:
  cuda_c_lib = PyDLL(cuda_c_lib_path)

  cuda_c_lib.check_if_cuda_is_available.argtypes = []
  cuda_c_lib.check_if_cuda_is_available.restype  = c_int

  cuda_c_lib.available_gpu_memory.argtypes = []
  cuda_c_lib.available_gpu_memory.restype  = py_object
except:
  cuda_c_lib = None


c_lib_path = (os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.dylib')
              if Path(os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.dylib')).is_file()
              else os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.so'))
c_lib = PyDLL(c_lib_path)

c_lib.api_newFFIO.argtypes = []
c_lib.api_newFFIO.restype  = POINTER(CFFIO)

c_lib.api_initFFIO.argtypes = [
  POINTER(CFFIO), c_int, c_char_p,
  c_bool, c_bool, c_char_p,
  c_bool, c_char_p, c_int, c_int,
  POINTER(CCodecParams)
]
c_lib.api_initFFIO.restype = None

c_lib.api_finalizeFFIO.argtypes = [POINTER(CFFIO)]
c_lib.api_finalizeFFIO.restype  = None
c_lib.api_deleteFFIO.argtypes   = [POINTER(CFFIO)]
c_lib.api_deleteFFIO.restype    = None

c_lib.api_decodeOneFrame.argtypes        = [POINTER(CFFIO), c_char_p]
c_lib.api_decodeOneFrame.restype         = POINTER(CFFIOFrame)
c_lib.api_decodeOneFrameToShm.argtypes   = [POINTER(CFFIO), c_int, c_char_p]
c_lib.api_decodeOneFrameToShm.restype    = POINTER(CFFIOFrame)

c_lib.api_encodeOneFrame.argtypes        = [POINTER(CFFIO), py_object, c_char_p, c_int]
c_lib.api_encodeOneFrame.restype         = c_int
c_lib.api_encodeOneFrameFromShm.argtypes = [POINTER(CFFIO), c_int, c_char_p, c_int]
c_lib.api_encodeOneFrameFromShm.restype  = c_bool
