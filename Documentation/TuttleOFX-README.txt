* On OSX

* Setting up MacPorts

    Download and install Xcode tools (latest versions are 3.1.4 for Leopard, 3.2.6 for Snow Leopard, and 4.3.2 for Lion). 3.1.4 and 3.2.6 are available from the Mac Dev Center at developer.apple.com, whereas version 4 is available from the App Store. Don't forget to also install the UNIX command-line utilities (this is an install option in Xcode 3, and must be installed from the "Downloads" pane of the Xcode Preferences in Xcode 4.3 and later)

   Download and install the MacPorts installer that corresponds to your Mac OS X version.

    Download the local portfiles archive

    Uncompress the portfiles in a directory of your choice. In the following, we suppose you uncompressed it in /Users/USER_NAME/Development/dports-dev, but it can be anywhere except in the Documents directory, which has special permissions.

    Give read/execute permissions to the local repository: "chmod 755 /Users/USER_NAME/Development/dports-dev"

    Check that the user "nobody" can read this directory by typing the following command in a Terminal: "sudo -u nobody ls /Users/USER_NAME/Development/dports-dev". If it fails, then try again after having given execution permissions on your home directory using the following command: "chmod o+x /Users/USER_NAME". If this still fails, then something is really wrong.

    Edit the sources.conf file for MacPorts, for example using the nano editor: "sudo nano /opt/local/etc/macports/sources.conf", insert at the beginning of the file the configuration for a local repository (read the comments in the file), by inserting the line "file:///Users/USER_NAME/Development/dports-dev" (without quotes, and yes there are 3 slashes). Save and exit (if you're using nano, this means typing ctrl-X, Y and return).
 
   Update MacPorts: "sudo port selfupdate"

    Recreate the index in the local repository: "cd /Users/USER_NAME/Development/dports-dev; portindex" (no need to be root for this)

   Add the following line to /opt/local/etc/macports/variants.conf:
-x11 +no_x11 +bash_completion +no_gnome +quartz
(add +universal on OSX 10.5 and 10.6)

* special portfiles:
- graphics/ctl
- graphics/openimageio
- graphics/opencolorio
- aqua/qt4-quick-controls (for ButtleOFX)

* external libraries (from https://sites.google.com/site/tuttleofx/development/build/libraries/macos )
sudo port -v install \
git \
scons \
boost +python27 \
jpeg \
openexr \
ffmpeg \
openjpeg15 \
libcaca \
freetype \
glew \
lcms \
swig \

REMOVED: freeglut

* missing ports (with respect to the wiki):
sudo port -v install \
ImageMagick +no_x11 \
lcms2 \
libraw \
nasm \
opencolorio \
openimageio \
swig-python \
py27-numpy \
flex \
bison \
ctl

* for the OpenFX plugins and Natron:
sudo port -v install \
openexr \
opencv \
qt4-mac


* if the command "pkg-config --cflags glew" gives the following error:
  $ pkg-config --libs glew
  Package glu was not found in the pkg-config search path.
  Perhaps you should add the directory containing `glu.pc'
  to the PKG_CONFIG_PATH environment variable
  Package 'glu', required by 'glew', not found
execute the following:
sudo -s
cat > /opt/local/lib/pkgconfig/glew.pc << EOF
prefix=/opt/local
exec_prefix=/opt/local/bin
libdir=/opt/local/lib
includedir=/opt/local/include/GL

Name: glew
Description: The OpenGL Extension Wrangler library
Version: 1.10.0
Cflags: -I\${includedir}
Libs: -L\${libdir} -lGLEW -framework OpenGL
Requires:
EOF

* on OSX 10.7 and below:
sudo port -v install clang-3.3
sudo port select clang mp-clang-3.3

* on OSX 10.8 and above
 sudo port select clang none
 sudo port select gcc none

* compiling/installing libjpeg-turbo (don't install it in MacPorts, because it can't coexist with libjpeg):
- download and install from the dmg file found at from http://sourceforge.net/projects/libjpeg-turbo/files/
- if you prefer to compile from sources :
./configure --prefix=/opt/libjpeg-turbo --host x86_64-apple-darwin NASM=/opt/local/bin/nasm && make && sudo make install

- checkout sources

git clone https://github.com/devernay/openfx.git
git clone https://github.com/devernay/openfx-yadif.git
git clone https://github.com/devernay/openfx-opencv.git
git clone https://github.com/MrKepzie/openfx-io.git
#git clone https://github.com/tuttleofx/CTL.git
git clone https://github.com/wdas/SeExpr
#git clone https://github.com/devernay/sconsProject.git
git clone https://github.com/tuttleofx/TuttleOFX.git
git clone https://github.com/MrKepzie/Natron.git
# grab the latest openfx source
for i in openfx* openfx-io/IOSupport/SequenceParsing  Natron Natron/libs/SequenceParsing; do (echo $i;cd $i; git submodule update -i -r; git submodule foreach git pull origin master); done

- compile OpenFX plugins

bits=64
if [ `uname -s` = "Darwin" ]; then
  macosx=`uname -r | sed 's|\([0-9][0-9]*\)\.[0-9][0-9]*\.[0-9][0-9]*|\1|'`
  case "$macosx" in
  [1-8])
    bits=32
    ;;
  9|10)
    bits=Universal
    ;;
   esac;
  echo "Building for architecture $bits"
fi
ofx=/Library/OFX

cd openfx
git pull
cd Examples
rm -rf */*-${bits}-release */*-${bits}-debug
rm */*.o
make BITS=$bits
rm -rf $ofx/Plugins/Examples.debug
mkdir -p $ofx/Plugins/Examples.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/Examples.debug
rm */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/Examples
mkdir -p $ofx/Plugins.disabled/Examples
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/Examples
rm -rf */*-${bits}-release */*-${bits}-debug

cd ../Support/Plugins
rm -rf */*-${bits}-release */*-${bits}-debug
rm */*.o
make BITS=$bits
rm -rf $ofx/Plugins/Support.debug
mkdir -p $ofx/Plugins/Support.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/Support.debug
rm */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/Support
mkdir -p $ofx/Plugins.disabled/Support
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/Support
rm -rf */*-${bits}-release */*-${bits}-debug

cd ../PropTester
rm -rf *-${bits}-release *-${bits}-debug
rm *.o
make BITS=$bits
rm -rf $ofx/Plugins/PropTester.debug
mkdir -p $ofx/Plugins/PropTester.debug
mv *-${bits}-debug/*.ofx.bundle $ofx/Plugins/PropTester.debug
rm *.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/PropTester
mkdir -p $ofx/Plugins.disabled/PropTester
mv *-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/PropTester
rm -rf *-${bits}-release *-${bits}-debug

cd ../../..



cd openfx-yadif
git pull
git submodule update -i;git submodule foreach git pull origin master
rm -rf *-${bits}-release *-${bits}-debug
rm *.o
make BITS=$bits
rm -rf $ofx/Plugins/yadif.debug
mkdir -p $ofx/Plugins/yadif.debug
mv *-${bits}-debug/*.ofx.bundle $ofx/Plugins/yadif.debug
rm *.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/yadif
mkdir -p $ofx/Plugins.disabled/yadif
mv *-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/yadif
rm -rf *-${bits}-release *-${bits}-debug
cd ..

cd openfx-opencv
git pull
git submodule update -i;git submodule foreach git pull origin master
cd opencv2fx
rm -rf */*-${bits}-release */*-${bits}-debug
rm */*.o
make BITS=$bits
rm -rf $ofx/Plugins/opencv2fx.debug
mkdir -p $ofx/Plugins/opencv2fx.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/opencv2fx.debug
rm */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/opencv2fx
mkdir -p $ofx/Plugins.disabled/opencv2fx
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/opencv2fx
rm -rf */*-${bits}-release */*-${bits}-debug
cd ../..

cd openfx-io
git pull
git submodule update -i;git submodule foreach git pull origin master
rm *.o */*.o
make BITS=$bits
rm -rf $ofx/Plugins/io.debug
mkdir -p $ofx/Plugins/io.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/io.debug
rm *.o */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/io
mkdir -p $ofx/Plugins.disabled/io
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/io
rm -rf */*-${bits}-release */*-${bits}-debug
cd ..


- compile TuttleOFX

#(cd TuttleOFX;git checkout develop;git remote add upstream https://github.com/tuttleofx/TuttleOFX.git;git fetch upstream;git submodule update -i)
(cd TuttleOFX;git checkout develop;git pull;git submodule update -i)

cd TuttleOFX;
#(cd 3rdParty; ln -s ../../CTL/CTL ctl)
#(cd 3rdParty/ctl; git pull; autoreconf -i; ./configure --prefix=/opt/local CC=clang  CXX=clang++ && make -j3)
# if there is an error about memset being not defined, add "#include <cstring>" in /opt/local/include/OpenEXR/ImathMatrix.h and relaunch the last line
(cd 3rdParty; ln -s ../../SeExpr seexpr)
(cd 3rdParty/seexpr; git pull; mkdir build; cd build && cmake .. && make)

rm host.sconf; cat > host.sconf << EOF
import os
macports_base = '/opt/local'
macports_include = os.path.join(macports_base, 'include')
fwkdir_corefoundation = True
dir_boost = macports_base
lib_boost_filesystem = 'boost_filesystem-mt'
lib_boost_program_options = 'boost_program_options-mt'
lib_boost_python = 'boost_python-mt'
lib_boost_regex = 'boost_regex-mt'
lib_boost_system = 'boost_system-mt'
lib_boost_serialization = 'boost_serialization-mt'
lib_boost_unit_test_framework = 'boost_unit_test_framework-mt'
lib_boost_thread = 'libboost_thread-mt'
lib_boost_date_time = 'libboost_date_time-mt'
lib_boost_chrono = 'libboost_chrono-mt'
lib_boost_log = 'libboost_log-mt'
lib_boost_atomic= 'libboost_atomic-mt'
lib_rt = ''
dir_caca = macports_base
dir_ffmpeg = macports_base
dir_avutil = macports_base
dir_avcodec = macports_base
dir_avformat = macports_base
dir_swscale = macports_base
dir_fontconfig = macports_base
dir_freetype = macports_base
incdir_freetype = [ macports_include,
                    os.path.join(macports_include, 'freetype2') ]
fwkdir_gl = True
fwkdir_glu = True
fwkdir_freeglut = True
dir_glew = macports_base
dir_half = macports_base
incdir_half = [ macports_include, os.path.join(macports_include, 'OpenExr') ]
dir_ilmbase = macports_base
lib_gomp = ''
dir_ltdl = macports_base
incdir_imagemagick = os.path.join(macports_include, "ImageMagick-6")
lib_imagemagick = "libMagickCore-6.Q16"
dir_jpeg = macports_base
dir_littlecms = macports_base
dir_lcms = macports_base
dir_lcms2 = macports_base
dir_opencolorio = macports_base
incdir_openexr = os.path.join(macports_include, 'OpenEXR/')
dir_openimageio = macports_base
incdir_openimageio = [ os.path.join(dir_openimageio, 'include'),
                       os.path.join(dir_openimageio, 'include', 'OpenImageIO') ]
dir_openjpeg = macports_base
dir_png = macports_base
dir_python= macports_base
incdir_python = os.path.join(macports_base,
                        'Library/Frameworks/Python.framework/Versions',
                        '2.7/Headers')
lib_python = "python2.7"
dir_python_numpy = os.path.join(macports_base,
                         'Library/Frameworks/Python.framework/Versions',
                         '2.7/lib/python2.7/site-packages/numpy/core')
dir_raw = macports_base
dir_tiff = macports_base
dir_turbojpeg = '/opt/libjpeg-turbo'
dir_xlibs = macports_base
dir_x264 = macports_base
dir_mp3lame = macports_base
dir_xvidcore = macports_base
extern = '#3rdParty'
dir_ctl = macports_base
incdir_ctl = join(macports_include, 'CTL')
dir_seexpr = join(extern, 'seexpr')
incdir_seexpr = join(dir_seexpr, 'src/SeExpr')
libdir_seexpr = join(dir_seexpr, 'build/src/SeExpr')
EOF

#dir_ctl = join(extern, 'ctl')
#modules_ctl = ['IlmCtlSimd', 'IlmCtl', 'IlmCtlMath']
#incdir_ctl = [incdir_openexr]+[join(dir_ctl,inc) for inc in modules_ctl]
#libdir_ctl = [join(dir_ctl,inc,'.libs') for inc in modules_ctl]

scons -k mode=debug CC=clang CXX=clang++ compiler=clang -j3
rm -rf $ofx/Plugins/TuttleOFX.debug
mkdir -p $ofx/Plugins/TuttleOFX.debug
mv dist/*/*/debug/plugin/*.ofx.bundle $ofx/Plugins/TuttleOFX.debug

scons -k CC=clang CXX=clang++ compiler=clang -j3
rm -rf $ofx/Plugins.disabled/TuttleOFX
mkdir -p $ofx/Plugins.disabled/TuttleOFX
mv dist/*/*/production/plugin/*.ofx.bundle $ofx/Plugins.disabled/TuttleOFX


cd ..


- Compile Natron

#You'll need to edit the file $HOME/.launchd.conf and add this line:
#setenv PATH /opt/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
#And restart your session so it takes it into account.
# see http://www.emacswiki.org/emacs/EmacsApp

launchctl setenv PATH /opt/local/bin:/opt/local/sbin:/usr/bin:/bin:/usr/sbin:/sbin

(cd Natron;git checkout workshop;git submodule update -i; git submodule foreach git pull origin master)
cd Natron
cat > config.pri << EOF
boost{
    INCLUDEPATH += /opt/local/include
    LIBS += -L/opt/local/lib -lboost_serialization-mt -lboost_thread-mt -lboost_system-mt
}
EOF


# using Xcode
(cd Natron; qmake ../Natron/Natron.pro -spec macx-xcode; rm -rf Natron.xcodeproj)

mkdir Natron-qt4-xcode
cd Natron-qt4-xcode
qmake ../Natron/Natron.pro -spec macx-xcode DEFINES+=NDEBUG
ln -s ../Natron/Info.plist
xcodebuild
cd ..

mkdir Natron-qt4-xcode-debug
cd Natron-qt4-xcode-debug
qmake ../Natron/Natron.pro -spec macx-xcode CONFIG+=debug
ln -s ../Natron/Info.plist
xcodebuild -configuration Debug
cd ..


#or using make
mkdir Natron-qt4
cd Natron-qt4
qmake ../Natron/Natron.pro -spec unsupported/macx-clang  DEFINES+=NDEBUG
make
cd ..

#or using make to build a Universal Natron (all the libraries have to be Universal too, and this is only meaningful on OSX<=10.6)
mkdir Natron-qt4-universal
cd Natron-qt4-universal
qmake ../Natron/Natron.pro -spec unsupported/macx-clang CONFIG+=universal DEFINES+=NDEBUG
make
cd ..

mkdir Natron-qt4-universal-debug
cd Natron-qt4-universal-debug
qmake ../Natron/Natron.pro -spec unsupported/macx-clang CONFIG+=universal CONFIG+=debug
make
cd ..


** Github cheat sheet

* Forking

1- click "Fork" on github

2- clone
git clone https://github.com/username/Spoon-Knife.git
# Clones your fork of the repository into the current directory in terminal

3- setup remotes
cd Spoon-Knife
# Changes the active directory in the prompt to the newly cloned "Spoon-Knife" directory
git remote add upstream https://github.com/octocat/Spoon-Knife.git
# Assigns the original repository to a remote called "upstream"
git fetch upstream
# Pulls in changes not present in your local repository, without modifying your files
* Push commits
git push origin master
# Pushes commits to your remote repository stored on GitHub

* Pull in upstream changes
git fetch upstream
# Fetches any new changes from the original repository
git merge upstream/master
# Merges any changes fetched into your working files

*  Create branches
git branch mybranch
# Creates a new branch called "mybranch"
git checkout mybranch
# Makes "mybranch" the active branch

* Switching branches
git checkout master
# Makes "master" the active branch
git checkout mybranch
# Makes "mybranch" the active branch

* Merge changes to master branch
git checkout master
# Makes "master" the active branch
git merge mybranch
# Merges the commits from "mybranch" into "master"
git branch -d mybranch
# Deletes the "mybranch" branch



***** on fedora:

- install boost >= 1.54.0 in /opt/boost, see http://www.boost.org/doc/html/bbv2/installation.html

- install dependencies

sudo yum install \
\
scons \
git \
gcc gcc-c++ \
swig \
boost-devel \
python-devel \
freetype-devel \
libXt-devel \
bzip2-devel \
lcms2-devel \
libtool-ltdl-devel \
libpng-devel \
libcaca-devel \
libjpeg-turbo-devel \
glew-devel \
libtiff-devel \
OpenEXR-devel \
OpenEXR_CTL-devel \
CTL-devel \
ImageMagick-devel \
LibRaw-devel \
openjpeg-devel \
graphviz-devel \
 LibRaw-devel nasm OpenColorIO-devel OpenImageIO-devel numpy ffmpeg-devel turbojpeg-devel \
opencv-devel ftgl-devel eigen2-devel

- checkout sources

git clone https://github.com/devernay/openfx.git
git clone https://github.com/devernay/openfx-yadif.git
git clone https://github.com/devernay/openfx-opencv.git
git clone https://github.com/tuttleofx/CTL.git
git clone https://github.com/wdas/SeExpr
#git clone https://github.com/devernay/sconsProject.git
#git clone https://github.com/devernay/TuttleOFX.git
git clone https://github.com/tuttleofx/TuttleOFX.git
git clone https://github.com/MrKepzie/Natron.git
# grab the latest openfx source
for i in openfx* Natron; do (echo $i;cd $i; git submodule foreach git pull origin master); done

- compile OpenFX plugins

bits=64
ofx=/usr/OFX

cd openfx
git pull
cd Examples
rm -rf */*-${bits}-release */*-${bits}-debug
rm */*.o
make BITS=$bits
rm -rf $ofx/Plugins/Examples.debug
mkdir -p $ofx/Plugins/Examples.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/Examples.debug
rm */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/Examples
mkdir -p $ofx/Plugins.disabled/Examples
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/Examples
rm -rf */*-${bits}-release */*-${bits}-debug

cd ../Support/Plugins
rm -rf */*-${bits}-release */*-${bits}-debug
rm */*.o
make BITS=$bits
rm -rf $ofx/Plugins/Support.debug
mkdir -p $ofx/Plugins/Support.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/Support.debug
rm */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/Support
mkdir -p $ofx/Plugins.disabled/Support
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/Support
rm -rf */*-${bits}-release */*-${bits}-debug

cd ../PropTester
rm -rf *-${bits}-release *-${bits}-debug
rm *.o
make BITS=$bits
rm -rf $ofx/Plugins/PropTester.debug
mkdir -p $ofx/Plugins/PropTester.debug
mv *-${bits}-debug/*.ofx.bundle $ofx/Plugins/PropTester.debug
rm *.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/PropTester
mkdir -p $ofx/Plugins.disabled/PropTester
mv *-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/PropTester
rm -rf *-${bits}-release *-${bits}-debug

cd ../../..

cd openfx-yadif
git pull
git submodule update -i;git submodule foreach git pull origin master
rm -rf *-${bits}-release *-${bits}-debug
rm *.o
make BITS=$bits
rm -rf $ofx/Plugins/yadif.debug
mkdir -p $ofx/Plugins/yadif.debug
mv *-${bits}-debug/*.ofx.bundle $ofx/Plugins/yadif.debug
rm *.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/yadif
mkdir -p $ofx/Plugins.disabled/yadif
mv *-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/yadif
rm -rf *-${bits}-release *-${bits}-debug
cd ..

cd openfx-opencv
git pull
git submodule update -i;git submodule foreach git pull origin master
cd opencv2fx
rm -rf */*-${bits}-release */*-${bits}-debug
rm */*.o
make BITS=$bits
rm -rf $ofx/Plugins/opencv2fx.debug
mkdir -p $ofx/Plugins/opencv2fx.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/opencv2fx.debug
rm */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/opencv2fx
mkdir -p $ofx/Plugins.disabled/opencv2fx
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/opencv2fx
rm -rf */*-${bits}-release */*-${bits}-debug
 cd ../..

cd openfx-io
git pull
git submodule update -i;git submodule foreach git pull origin master
make BITS=$bits
rm -rf $ofx/Plugins/io.debug
mkdir -p $ofx/Plugins/io.debug
mv */*-${bits}-debug/*.ofx.bundle $ofx/Plugins/io.debug
rm */*.o
make BITS=$bits CONFIG=release
rm -rf $ofx/Plugins.disabled/io
mkdir -p $ofx/Plugins.disabled/io
mv */*-${bits}-release/*.ofx.bundle $ofx/Plugins.disabled/io
rm -rf */*-${bits}-release */*-${bits}-debug
cd ..

- compile TuttleOFX

#(cd TuttleOFX;git checkout develop;git remote add upstream https://github.com/tuttleofx/TuttleOFX.git;git fetch upstream;git submodule update -i)
(cd TuttleOFX;git checkout develop;git pull;git submodule update -i)

cd TuttleOFX;
(cd 3rdParty; ln -s ../../CTL/CTL ctl)
(cd 3rdParty/ctl; git checkout afcff9748a81526ff9a85f6f85a4e9aabd8ae851; mkdir build; cd build && cmake .. && make)
(cd 3rdParty; ln -s ../../SeExpr seexpr)
(cd 3rdParty/seexpr; git pull; mkdir build; cd build && cmake .. && make)


rm host.sconf; cat > host.sconf << EOF
incdir_openexr = '/usr/include/OpenEXR'
incdir_freetype = '/usr/include/freetype2'
incdir_imagemagick = '/usr/include/ImageMagick'
incdir_openimageio = [incdir_openexr,'/usr/include/OpenImageIO']
lib_python = 'python2.7'
incdir_python = '/usr/include/python2.7'
extern = '#3rdParty'
dir_ctl = join(extern, 'ctl')
modules_ctl = ['IlmCtlSimd', 'IlmCtl', 'IlmCtlMath']
incdir_ctl = [incdir_openexr]+[join(dir_ctl,'lib',inc) for inc in modules_ctl]
libdir_ctl = [join(dir_ctl,'build/lib',inc) for inc in modules_ctl]
dir_boost = '/opt/boost'
incdir_ffmpeg = '/usr/include/ffmpeg'
incdir_avutil = incdir_ffmpeg
incdir_avcodec = incdir_ffmpeg
incdir_avformat = incdir_ffmpeg
incdir_swscale = incdir_ffmpeg
dir_seexpr = join(extern, 'seexpr')
incdir_seexpr = join(dir_seexpr, 'src/SeExpr')
libdir_seexpr = join(dir_seexpr, 'build/src/SeExpr')
EOF

scons -k mode=debug -j3
rm -rf $ofx/Plugins/TuttleOFX.debug
mkdir -p $ofx/Plugins/TuttleOFX.debug
mv dist/*/*/debug/plugin/*.ofx.bundle $ofx/Plugins/TuttleOFX.debug

scons -k -j3
rm -rf $ofx/Plugins.disabled/TuttleOFX
mkdir -p $ofx/Plugins.disabled/TuttleOFX
mv dist/*/*/production/plugin/*.ofx.bundle $ofx/Plugins.disabled/TuttleOFX


cd ..

(cd Natron;git checkout workshop;git submodule update -i; git submodule foreach git pull origin master)
cd Natron
qmake-qt4
make CXX="/opt/llvm/bin/clang++ -fsanitize=thread -fPIE -pie" LINK="/opt/llvm/bin/clang++ -fsanitize=thread -fPIE -pie" -j8

** on ubuntu:


- install dependencies

[[[[ Special note for Ubuntu 12.04 (Precise):
we alse require
- boost >= 1.49 for Natron
- install boost >= 1.54.0 in /opt/boost, see http://www.boost.org/doc/html/bbv2/installation.html
- ilmbase, openexr, opencolorio, openimageio for TuttleOFX
- opencv for the opencv plugins

#we don't want the following cutting-edge repositories
sudo add-apt-repository -r irie/ocio-static
sudo add-apt-repository -r irie/openimageio
sudo add-apt-repository -r irie/boost

# kubuntu-ppa backports provides opencv, (opencolorio and openimageio are broken)
sudo add-apt-repository -y ppa:kubuntu-ppa/backports
# mapnik/boost provides 1.49 backport
sudo add-apt-repository -y ppa:mapnik/boost

sudo apt-get update

# and then proceed with the following instructions
]]]]

sudo apt-get install \
\
automake \
cmake \
git \
scons \
gcc g++ \
swig \
swig2.0 \
libboost-chrono-dev \
libboost-date-time-dev \
libboost-dbg \
libboost-dev \
libboost-doc \
libboost-filesystem-dev \
libboost-graph-dev \
libboost-iostreams-dev \
libboost-locale-dev \
libboost-math-dev \
libboost-program-options-dev \
libboost-python-dev \
libboost-random-dev \
libboost-regex-dev \
libboost-serialization-dev \
libboost-signals-dev \
libboost-system-dev \
libboost-test-dev \
libboost-thread-dev \
libboost-timer-dev \
libboost-wave-dev \
python-dev \
python-numpy \
libfreetype6-dev \
libXt-dev \
libbz2-dev \
liblcms-dev \
libopenctl0.8 \
libltdl-dev \
libpng-dev \
libcaca-dev \
libjpeg-dev \
libglew-dev \
libtiff-dev \
libilmbase-dev \
libopenexr-dev \
libMagickCore-dev \
libraw-dev \
libavdevice-dev \
libswscale-dev \
libavformat-dev \
libavcodec-dev \
libavutil-dev \
libopenjpeg-dev \
libglew-dev \
graphviz graphviz-dev \
python-nose python-imaging \
libturbojpeg \
libopenimageio-dev liblcms2-dev freeglut3-dev libmagick++-dev libopenjpeg-dev libmagickcore-dev libopencv-dev qt-sdk doxygen bison flex \
yasm libmp3lame-dev libvorbis-dev libopus-dev libtheora-dev libschroedinger-dev libopenjpeg-dev libmodplug-dev libvpx-dev libspeex-dev libass-dev libbluray-dev libgnutls-dev libfreetype6-dev libfontconfig1-dev libx264-dev libxvidcore-dev libopencore-amrnb-dev libopencore-amrwb-dev

# [Ubuntu 13.10 saucy] fix a bug in libturbojpeg package
#sudo ln -s /usr/lib/x86_64-linux-gnu/libturbojpeg.so.0 /usr/lib/x86_64-linux-gnu/libturbojpeg.so

- checkout sources

see fedora install (above)

- compile OpenFX plugins

see fedora install (above)

- install recent ffmpeg


- compile TuttleOFX

#(cd TuttleOFX;git checkout develop;git remote add upstream https://github.com/tuttleofx/TuttleOFX.git;git fetch upstream;git submodule update -i)
(cd TuttleOFX;git checkout develop;git pull;git submodule update -i)

cd TuttleOFX;
(cd 3rdParty; ln -s ../../CTL ctl)
(cd 3rdParty/ctl; git checkout afcff9748a81526ff9a85f6f85a4e9aabd8ae851;  mkdir build; cd build && cmake .. && make)
(cd 3rdParty;python init.py opencolorio)
(cd 3rdParty; ln -s ../../SeExpr seexpr)
 (cd 3rdParty/seexpr; git pull; mkdir build; cd build && cmake .. && make)
 (cd 3rdParty/opencolorio;mkdir build;cd build && cmake .. -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF -DOCIO_BUILD_PYGLUE=OFF  && make)
(cd 3rdParty; rm -rf ffmpeg; wget http://ffmpeg.org/releases/ffmpeg-2.1.1.tar.bz2 && tar jxvf ffmpeg-2.1.1.tar.bz2 && mv ffmpeg-2.1.1 ffmpeg)
(cd 3rdParty/ffmpeg; ./configure --prefix=/opt/ffmpeg --enable-shared --enable-gpl \
                            --enable-swscale \
        --enable-avfilter \
        --enable-avresample \
        --enable-libmp3lame \
        --enable-libvorbis \
        --enable-libopus \
        --enable-libtheora \
        --enable-libschroedinger \
        --enable-libopenjpeg \
        --enable-libmodplug \
        --enable-libvpx \
        --enable-libspeex \
        --enable-libass \
        --enable-libbluray \
        --enable-gnutls \
        --enable-libfreetype \
        --enable-fontconfig  --enable-pthreads --enable-postproc \
                            --enable-libx264 \
                            --enable-libxvid  --enable-version3 \
                            --enable-libopencore-amrnb \
                            --enable-libopencore-amrwb && make)

rm host.sconf; cat > host.sconf << EOF
incdir_qt4 = '/usr/include/qt4'
incdir_ilmbase = '/usr/include/OpenEXR'
incdir_openexr = '/usr/include/OpenEXR'
incdir_freetype = '/usr/include/freetype2'
incdir_imagemagick = '/usr/include/ImageMagick'
incdir_openimageio = [incdir_openexr,'/usr/include/OpenImageIO']
lib_python = 'python2.7'
incdir_python = join('/usr/include',lib_python)
extern = '#3rdParty'
dir_ctl = join(extern, 'ctl')
modules_ctl = ['IlmCtlSimd', 'IlmCtl', 'IlmCtlMath']
incdir_ctl = [incdir_openexr]+[join(dir_ctl,'lib',inc) for inc in modules_ctl]
libdir_ctl = [join(dir_ctl,'build/lib',inc) for inc in modules_ctl]
dir_boost = '/opt/boost'
dir_opencolorio = join(extern, 'opencolorio')
incdir_opencolorio = [join(dir_opencolorio, 'export'),
                      join(dir_opencolorio, 'build/export')]
libdir_opencolorio = join(dir_opencolorio, 'build/src/core')
dir_seexpr = join(extern, 'seexpr')
incdir_seexpr = join(dir_seexpr, 'src/SeExpr')
libdir_seexpr = join(dir_seexpr, 'build/src/SeExpr')
ffmpeg = join(extern,'ffmpeg')
incdir_ffmpeg = ffmpeg
incdir_avutil = ffmpeg
libdir_avutil = join(ffmpeg,'libavutil')
incdir_avcodec = ffmpeg
libdir_avcodec = join(ffmpeg,'libavcodec')
incdir_avformat = ffmpeg
libdir_avformat = join(ffmpeg,'libavformat')
incdir_swscale =  ffmpeg
libdir_swscale = join(ffmpeg,'libswscale')
EOF

scons -k mode=debug -j3
mkdir -p $ofx/Plugins/TuttleOFX.debug
mv dist/*/*/debug/plugin/*.ofx.bundle $ofx/Plugins/TuttleOFX.debug

scons -k -j3
mkdir -p $ofx/Plugins.disabled/TuttleOFX
mv dist/*/*/production/plugin/*.ofx.bundle $ofx/Plugins.disabled/TuttleOFX
