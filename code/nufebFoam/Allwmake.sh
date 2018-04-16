#!/bin/bash
cd ${0%/*} || exit 1 # Run from this directory

# Read the information of current directory.
# And collect information of the installation of LAMMPS from user.
echo "Installing nufebFoam (for mac/linux).."
currentDir=$PWD
echo "Enter the directory of your LAMMPS and press [ENTER] "
echo -n "(default directory ../lammps5Nov16: "
read lammpsDir

# Determine if the directory of LAMMPS exists or not.
# If not, look for LAMMPS in the default directory.
if [ ! -d "$lammpsDir" ]
then
    echo "Directory NOT found! Use default directory instead."
    cd ..
    lammpsDir="$PWD/lammps5Nov16"
fi

cd $lammpsDir
lammpsDir=$PWD

echo "Directory of LAMMPS is: " $lammpsDir

# Copy/link all the extra implementations
cd $lammpsDir/src
lammpsSRC=$PWD
echo $lammpsSRC
ln -sf $currentDir/interfaceToLammps/*.* . 
cd $lammpsDir/src/MAKE
ln -sf $currentDir/interfaceToLammps/MAKE/*.* .

# Make packages
make yes-GRANULAR
make yes-USER-NUFEB

version=`uname`
# Use different options according to different versions
if [ $version == "Linux" ]
then
    echo "The version you choose is openmpi version"
    make -j4 shanghailinux mode=shlibmake
    cd $FOAM_USER_LIBBIN
    ln -sf $lammpsDir/src/liblammps_shanghailinux.so .
    cd $currentDir/lammpsFoam
    touch Make/options
    echo "LAMMPS_DIR ="$lammpsSRC > Make/options
    cat Make/options-ubuntu-openmpi >> Make/options
elif [ $version == "Darwin" ]
then
    echo "The version you choose is mac version"
    make -j4 make shanghailinux mode=shlib
    cd $FOAM_USER_LIBBIN
    ln -sf $lammpsDir/src/liblammps_shanghaimac.so .
    cd $currentDir/lammpsFoam
    touch Make/options
    echo "LAMMPS_DIR ="$lammpsSRC > Make/options
    cat Make/options-mac-openmpi >> Make/options
else
    echo "Sorry, we haven't got the required version."
    echo "Please contact the developer (sunrui@vt.edu) for help."
fi

wmake libso dragModels
wmake libso chPressureGrad
wmake libso lammpsFoamTurbulenceModels
wmake 

