from setuptools import setup
from distutils.command.install import install
import shutil


class CustomInstall(install):
    def run(self):
        install.run(self)
        shutil.copyfile('./build/llm_sys.cpython-310-x86_64-linux-gnu.so', self.install_lib + '/llm_sys.so')


setup(
    name='example',
    version='1.0',
    cmdclass={'install': CustomInstall},
)
