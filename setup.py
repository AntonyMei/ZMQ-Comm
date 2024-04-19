from setuptools import setup
from distutils.command.install import install
import shutil


class CustomInstall(install):
    def run(self):
        install.run(self)
        shutil.copyfile('./build/llm_worker.cpython-310-x86_64-linux-gnu.so', self.install_lib + '/llm_worker.so')
        shutil.copyfile('./build/llm_host.cpython-310-x86_64-linux-gnu.so', self.install_lib + '/llm_host.so')


setup(
    name='LLM system',
    version='1.0',
    cmdclass={'install': CustomInstall},
)
