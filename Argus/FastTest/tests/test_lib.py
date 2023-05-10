import sys
import os


#os.add_dll_directory("C:\\msys64\\mingw64\\bin")
sys.path.append(os.path.abspath('..'))
sys.path.append(os.path.abspath('../lib'))

import FastTest

if __name__ == "__main__":
    print("imported")