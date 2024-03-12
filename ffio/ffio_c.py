import os
import base64
import numpy as np

from enum    import IntEnum
from pathlib import Path
from ctypes  import Structure, PyDLL, POINTER, c_int, c_bool, c_char_p, py_object, c_char, c_ubyte
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
  sei_msg : bytes
  data    : POINTER(c_ubyte)
  _fields_ = [
    ("type",    c_int),
    ("err",     c_int),
    ("_width",  c_int),
    ("_height", c_int),
    ("sei_msg", c_char_p),
    ("data",    POINTER(c_ubyte))
  ]

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
    self.pts_trick           = -1
    self.use_h264_AnnexB_sei = True
    # You can modify this uuid what else you want.
    self.sei_uuid = (c_ubyte * 16).from_buffer_copy(b'\x0f\xf1\x0f\xf1'
                                                    b'\x00\x11\x22\x33'
                                                    b'\xa0\xb1\xc2\xd3'
                                                    b'\x00\x11\x22\x33')


DIR_PATH = os.path.dirname(os.path.abspath(__file__))

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

c_lib.api_decodeOneFrame.argtypes        = [POINTER(CFFIO), c_char_p]
c_lib.api_decodeOneFrame.restype         = POINTER(CFFIOFrame)
c_lib.api_decodeOneFrameToShm.argtypes   = [POINTER(CFFIO), c_int, c_char_p]
c_lib.api_decodeOneFrameToShm.restype    = POINTER(CFFIOFrame)

c_lib.api_encodeOneFrame.argtypes        = [POINTER(CFFIO), py_object, c_char_p, c_int]
c_lib.api_encodeOneFrame.restype         = c_int
c_lib.api_encodeOneFrameFromShm.argtypes = [POINTER(CFFIO), c_int, c_char_p, c_int]
c_lib.api_encodeOneFrameFromShm.restype  = c_bool
