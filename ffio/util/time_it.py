# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com


import time


class TimeIt(object):
    """ measuring execution time

    Args:
        name(str): specify a name for the measuring, default `None`
        unit(str): specify the unit of time, either second `s` or millisecond `ms`
        verbose(bool): if to print the log.

    Examples:
        >>> import jionlp as jio
        >>> with jio.TimeIt('test func1') as ti:
        >>>     func1()
        >>>     cost_time = ti.break_point(restart=True)
        >>> print(cost_time)

    """
    def __init__(self, name=None, unit='s', verbose=True):
        self.start_time = None
        self.restart_time = None  # restart measuring from the last breakpoint
        self.cost_time = None
        self.verbose = verbose
        self.name = name if name is not None else 'None'
        self.unit = unit
        assert self.unit in ['s', 'ms'], 'the unit of time is either `s` or `ms`'
        
    def __enter__(self):
        self.start_time = time.time()
        self.restart_time = time.time()
        return self
        
    def __exit__(self, *args, **kwargs):
        self.cost_time = time.time() - self.start_time
        if self.verbose:
            if self.unit == 's':
                print('{0:s} totally costs {1:.3f} s.'.format(
                    self.name, self.cost_time))
            elif self.unit == 'ms':
                print('{0:s} totally costs {1:.1f} ms.'.format(
                    self.name, self.cost_time * 1000))

    def start(self):
        self.start_time = time.time()
        self.restart_time = time.time()

    def break_point(self, restart=True):
        """ measuring the execution time from the begining or the last breakpoint

        Args:
             restart(bool): if set as True, the measuring would be set to restart
                from the last breakpoint, otherwise continuing measuring

        Returns:
            float: the consumed time

        """
        if not restart:
            cost_time = time.time() - self.start_time
        else:
            cost_time = time.time() - self.restart_time
            
        if self.verbose:
            if self.unit == 's':
                print('{0:s} break point costs {1:.3f} s.'.format(
                    self.name, cost_time))
            elif self.unit == 'ms':
                print('{0:s} break point costs {1:.1f} ms.'.format(
                    self.name, cost_time * 1000))
        
        self.restart_time = time.time()
        
        return cost_time

