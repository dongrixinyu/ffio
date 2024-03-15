# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu, koisi
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com


import os
import sys
import logging
from logging.handlers import TimedRotatingFileHandler


_LEVELS = {
    'CRITICAL': logging.CRITICAL,
    'ERROR': logging.ERROR,
    'WARN': logging.WARNING,
    'WARNING': logging.WARNING,
    'INFO': logging.INFO,
    'DEBUG': logging.DEBUG,
    'NOTSET': logging.NOTSET
}


def _logging_level_from_str(level):
    level = level.upper()
    if level in _LEVELS:
        return _LEVELS[level]
    return logging.INFO


def _refresh_logger(logger):
    # refresh handler
    if len(logger.handlers) != 0:
        for i in range(len(logger.handlers)):
            logger.removeHandler(logger.handlers[0])

    return logger


def set_logger(level='INFO', log_dir_name='.cache/ffio'):
    """ ffio print logger

    Args:
        level(str): level of logger, do not print if set to `None`
        log_dir_name(str): directory to store log, do not write to file if `None`

    """
    # set level
    if level is None:
        logger = logging.getLogger(__name__)
        _refresh_logger(logger)
        return logger

    level = _logging_level_from_str(level)
    logger = logging.getLogger(__name__)

    _refresh_logger(logger)
    logger.setLevel(level)

    # log format
    formatter = logging.Formatter(
        fmt="%(asctime)s %(levelname)s %(funcName)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S")

    stream_handler = logging.StreamHandler(sys.stdout)
    stream_handler.setLevel(level)
    stream_handler.setFormatter(formatter)

    if log_dir_name is not None:
        if log_dir_name.startswith("/"):
            filename_directory = log_dir_name
        else:
            filename_directory = os.path.join(os.path.expanduser('~'), log_dir_name)
        if not os.path.exists(filename_directory):
            os.makedirs(filename_directory)

        file_handler = TimedRotatingFileHandler(
            os.path.join(filename_directory, "log.txt"),
            when="midnight", backupCount=30)

        file_handler.setLevel(level)
        file_handler.suffix = "%Y%m%d"
        file_handler.setFormatter(formatter)

        logger.addHandler(file_handler)

    length = 20
    logger.log(level, "-" * length + " logging start " + "-" * length)
    logger.log(level, "LEVEL: {}".format(logging.getLevelName(level)))
    if log_dir_name is not None:
        logger.log(level, "PATH:  {}".format(filename_directory))
    logger.log(level, "-" * (length * 2 + 15))

    logger.addHandler(stream_handler)

    return logger
