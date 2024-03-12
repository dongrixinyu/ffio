import platform
import time
import numpy as np

from typing  import Optional, Union
from PIL     import Image
from ctypes  import POINTER, byref

from ffio.ffio_c import c_lib, CFFIO, CCodecParams, CFFIOFrame, FFIOMode, FFIOPTSTrick


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

  # or try others listed by ` ffmpeg -hwaccels` on your device.
  hw_device = "cuda" if platform.system() != 'Darwin' else "videotoolbox"

  def __init__(self, target_url: str, mode: FFIOMode = FFIOMode.DECODE,
               hw_enabled: bool = False,
               shm_name: str = None, shm_size: int = 0, shm_offset: int = 0,
               codec_params: CCodecParams = None):
    start_time = time.time()

    self.target_url   = target_url
    self.mode         = mode
    self.frame_seq_py = 0
    self.codec_params = codec_params if codec_params is not None else CCodecParams()
    self._auto_set_pts_trick()
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

  def __bool__(self):
    # see FFIOState defined in ffio.h
    state = self._c_ffio_ptr.contents.ffio_state
    return True if state == 1 or state == 2 else False

  @property
  def ffio_state(self) -> bool:
    return self.__bool__()

  @property
  def frame_seq_c(self):
    return self._c_ffio_ptr.contents.frame_seq

  """
    Description:
      Get one decoded from target video.
      
    Params:
      sei_filter: If provided, return the first sei attached which contains str of `sei_filter`.
                  If None, simply return the first sei attached to the frame if exists.
    
    Return:
      CFFIOFrame:
        contains the decoded image bytes and sei data attached.
        `CFFIOFrame.as_numpy()` to get decoded image in numpy format.
        `CFFIOFrame.as_image()` to get decoded image in PIL.Image format.
        `CFFIOFrame.sei_msg` to get sei data bytes.
        CFFIOFrame had overridden the `__bool__()`, so you can conveniently use it as a condition.
    
    When failure:
      Please manually to call `release_memory()` to release the resources.
  """
  def decode_one_frame(self, sei_filter: Optional[str] = None) -> CFFIOFrame:
    sei_filter_bytes = sei_filter.encode() if sei_filter is not None else None
    ret : POINTER(CFFIOFrame) = c_lib.api_decodeOneFrame(self._c_ffio_ptr, sei_filter_bytes)
    if ret.contents:
      self.frame_seq_py += 1
    return ret.contents

  """
    Description:
      In common with `decode_one_frame()`, but additionally will fill decoded image bytes to shm.
  """
  def decode_one_frame_to_shm(self, offset: int = 0, sei_filter: Optional[str] = None) -> CFFIOFrame:
    sei_filter_bytes = sei_filter.encode() if sei_filter is not None else None
    ret: POINTER(CFFIOFrame) = c_lib.api_decodeOneFrameToShm(self._c_ffio_ptr, offset, sei_filter_bytes)
    if ret.contents:
      self.frame_seq_py += 1
    return ret.contents

  """
    Description:
      Only ensures that one frame is successfully sent to the encoder, then attempt to fetch
      the encoded packet. Returns true even if the attempt fails.
      This means the encoded packet might actually be written to the target file on next or later
      call to `encode_one_frame()`.
    
    Params:
      sei_msg: if provided, encode this message as an attached sei data to that frame.
      
    Return:
      True or False
      
    When failure:
      Please manually to call release_memory() to release the resources.
  """
  def encode_one_frame(self, rgb_image: Union[bytes, np.ndarray],
                       sei_msg: Optional[str] = None) -> bool:
    # len(sei_msg_bytes)+1: when calling c_lib, ctypes will add '\0' to the end of the bytes automatically.
    sei_msg_bytes  = sei_msg.encode()     if sei_msg is not None else None
    sei_msg_size   = len(sei_msg_bytes)+1 if sei_msg is not None else 0
    rgb_image_type = type(rgb_image)
    if rgb_image_type is bytes:
      ret = c_lib.api_encodeOneFrame(self._c_ffio_ptr, rgb_image,
                                     sei_msg_bytes, sei_msg_size)
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

  """
    Description:
      In common with encode_one_frame(), but get the raw image data to be encoded 
      from provided shm location.
  """
  def encode_one_frame_from_shm(self, offset: int = 0, sei_msg: Optional[str] = None) -> bool:
    sei_msg_bytes = sei_msg.encode()     if sei_msg is not None else None
    sei_msg_size  = len(sei_msg_bytes)+1 if sei_msg is not None else 0
    ret = c_lib.api_encodeOneFrameFromShm(self._c_ffio_ptr, offset,
                                          sei_msg_bytes, sei_msg_size)
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

  def _auto_set_pts_trick(self):
    # If pts_trick is -1, auto set the pts trick based on whether the target is a live stream or a local file.
    if self.codec_params.pts_trick == -1:
      if ( self.target_url.startswith("rtmp://") or self.target_url.startswith("rtsp://")
           or self.target_url.startswith("srt://") ):
        self.codec_params.pts_trick = FFIOPTSTrick.FFIO_PTS_TRICK_EVEN.value
      else:
        self.codec_params.pts_trick = FFIOPTSTrick.FFIO_PTS_TRICK_INCREASE.value
