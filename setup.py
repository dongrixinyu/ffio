# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com


import os
import re
import sys
import setuptools


# get version tag
DIR_PATH = os.path.dirname(os.path.abspath(__file__))
__version__ = '2.0.0'

with open(os.path.join(DIR_PATH, 'requirements.txt'),
          'r', encoding='utf-8') as f:
    requirements = f.readlines()


def setup_package():

    # this line could be dismissed
    # extensions = None
    setuptools.setup(
        name='ffio',
        version=__version__,
        author='dongrixinyu',
        author_email='dongrixinyu.89@163.com',
        description='ffio: An easy-to-use Python wrapper for FFmpeg-C-API.',
        # long_description_content_type='text/markdown',
        url='https://github.com/dongrixinyu/ffio',
        packages=setuptools.find_packages(),
        package_data={
            '': ['*.txt', '*.pkl', '*.npz', '*.zip',
                 '*.json', '*.c', '*.h']
        },
        include_package_data=True,
        classifiers=[
            'Programming Language :: Python :: 3',
            'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
            'Operating System :: Ubuntu',
            'Topic :: Audio & Video Stream Processing',
            'Topic :: FFmpeg',
        ],
        install_requires=requirements,
        zip_safe=False,
        # cmdclass={'build_ext': build_ext} if sys.platform == 'linux' else {},
        # setup_requires=['numpy'],
    )


if __name__ == '__main__':
    setup_package()
