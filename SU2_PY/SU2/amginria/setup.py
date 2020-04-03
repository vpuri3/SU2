import sys,os
from distutils.core import setup, Extension
from distutils import sysconfig


def setup_amgio(argv=[]):
    
    sav_argv = sys.argv;    
    sys.argv = ['', 'build_ext', '--inplace'];
    
    working_dir = os.getcwd();
    file_dir    = os.path.dirname(os.path.abspath(__file__));
    
    os.chdir(file_dir);
    setup(name="amgio",
          ext_modules=[ \
          Extension("amgio",
          sources=[ "./amgio/amgio_py.c", \
                    "./amgio/mesh.c", \
                    "./amgio/GMFio.c", \
                    "./amgio/SU2io.c", \
                    "./amgio/option.c", \
                    "./amgio/libmesh6.c", \
                    "./amgio/amgio_py.i", \
                    "./amgio/convert.c"],
          extra_compile_args=["-std=c99","-Wno-unused-variable","-Wno-unused-result"]), 
          ],);
    
    os.rename("amgio"+sysconfig.get_config_var('EXT_SUFFIX'),"amgio.so");
    os.chdir(working_dir);
    sys.argv = sav_argv;

if __name__ == '__main__':
    setup_amgio(sys.argv)